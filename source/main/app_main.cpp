//
// Copyright 2026 AUTOMATOUS.IO
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// app_main.cpp

#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <inttypes.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_system.h>
#if CONFIG_PM_ENABLE
#include <esp_pm.h>
#endif

#include <esp_matter.h>
#include <esp_matter_ota.h>

#include <app_priv.h>
#include "battery.h"
#include "button.h"
#include "soil_probe.h"
#include "status_led.h"

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#include <esp_openthread.h>
#include <esp_openthread_lock.h>
#include <openthread/platform/radio.h>
#endif

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <clusters/soil_measurement/integration.h>

static const char *TAG = "app_main";

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

uint16_t soil_endpoint_id = 0;

constexpr auto k_timeout_seconds = 300;

static TaskHandle_t s_sample_task;
static esp_timer_handle_t s_sample_timer;

// The code-driven SoilMeasurement server cluster requires the measurement
// limits before Server::Init runs, and aborts the boot otherwise. It keeps a
// shallow reference to the accuracy range list, so this storage must be
// static. ±5% accuracy (Percent100ths units) over the full 0-100% range.
static const Globals::Structs::MeasurementAccuracyRangeStruct::Type kSoilAccuracyRanges[] = {
    { .rangeMin = 0, .rangeMax = 100, .percentMax = chip::MakeOptional(static_cast<chip::Percent100ths>(500)) },
};

static const SoilMeasurement::Attributes::SoilMoistureMeasurementLimits::TypeInfo::Type kSoilMoistureLimits = {
    .measurementType = Globals::MeasurementTypeEnum::kSoilMoisture,
    .measured = true,
    .minMeasuredValue = 0,
    .maxMeasuredValue = 100,
    .accuracyRanges =
        chip::app::DataModel::List<const Globals::Structs::MeasurementAccuracyRangeStruct::Type>(kSoilAccuracyRanges),
};

// ---------------------------------------------------------------------------
// Reporting - attribute updates must run on the Matter thread; readings are
// passed through ScheduleWork's intptr_t argument.

static void report_moisture(uint8_t percent)
{
    static int s_last = -1;
    if (s_last >= 0 && abs((int)percent - s_last) < CONFIG_SOIL_REPORT_DELTA_PERCENT) {
        return;
    }
    s_last = percent;
    CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(
        [](intptr_t arg) {
            // The registered SoilMeasurement server cluster owns this
            // attribute; its setter also triggers the change report.
            chip::app::DataModel::Nullable<chip::Percent> value;
            value.SetNonNull(static_cast<chip::Percent>(arg));
            CHIP_ERROR set_err = SoilMeasurement::SetSoilMoistureMeasuredValue(soil_endpoint_id, value);
            if (set_err != CHIP_NO_ERROR) {
                ESP_LOGE(TAG, "Failed to set soil moisture value: %" CHIP_ERROR_FORMAT, set_err.Format());
            }
        },
        (intptr_t)percent);
    if (err != CHIP_NO_ERROR) {
        s_last = -1;  // retry on the next sample instead of dropping the change
    }
}

static void report_battery(const battery_reading_t &reading)
{
    // Charge level factors in health: high sag = high internal resistance,
    // even at a full resting voltage.
    auto level = PowerSource::BatChargeLevelEnum::kOk;
    if (reading.percent <= 10 || reading.sag_mv >= CONFIG_SOIL_BATTERY_SAG_CRIT_MV) {
        level = PowerSource::BatChargeLevelEnum::kCritical;
    } else if (reading.percent <= 20 || reading.sag_mv >= CONFIG_SOIL_BATTERY_SAG_WARN_MV) {
        level = PowerSource::BatChargeLevelEnum::kWarning;
    }
    const bool replace = (reading.sag_mv >= CONFIG_SOIL_BATTERY_SAG_WARN_MV) || (reading.percent <= 10);

    static int s_last_pct = -1;
    static int s_last_level = -1;
    static int s_last_replace = -1;
    if ((int)reading.percent == s_last_pct && (int)level == s_last_level && (int)replace == s_last_replace) {
        return;
    }

    const intptr_t packed = (intptr_t)reading.percent | ((intptr_t)level << 8) | ((intptr_t)replace << 16);
    CHIP_ERROR err = chip::DeviceLayer::PlatformMgr().ScheduleWork(
        [](intptr_t arg) {
            const uint8_t pct = arg & 0xFF;
            const uint8_t lvl = (arg >> 8) & 0xFF;
            const bool repl = ((arg >> 16) & 0x1) != 0;
            // BatPercentRemaining is in half-percent units (0-200).
            esp_matter_attr_val_t remaining = esp_matter_nullable_uint8(nullable<uint8_t>(pct * 2));
            attribute::update(0, PowerSource::Id, PowerSource::Attributes::BatPercentRemaining::Id,
                              &remaining);
            esp_matter_attr_val_t level_val = esp_matter_enum8(lvl);
            attribute::update(0, PowerSource::Id, PowerSource::Attributes::BatChargeLevel::Id, &level_val);
            esp_matter_attr_val_t repl_val = esp_matter_bool(repl);
            attribute::update(0, PowerSource::Id, PowerSource::Attributes::BatReplacementNeeded::Id,
                              &repl_val);
        },
        packed);
    if (err == CHIP_NO_ERROR) {
        s_last_pct = reading.percent;
        s_last_level = (int)level;
        s_last_replace = (int)replace;
    }
}

// ---------------------------------------------------------------------------
// Sample worker - the only place that touches the ADC and calibration flows.

static void sample_task(void *arg)
{
    for (;;) {
        uint32_t bits = 0;
        xTaskNotifyWait(0, UINT32_MAX, &bits, portMAX_DELAY);

        if (bits & APP_SAMPLE_CAL) {
            soil_probe_calibrate();
            // Fall through to a fresh sample so the new calibration reports.
            bits |= APP_SAMPLE_SHOW_LED;
        }

        if (bits & (APP_SAMPLE_SILENT | APP_SAMPLE_SHOW_LED)) {
            uint8_t moisture = 0;
            int raw_mv = 0;
            const bool have_moisture = (soil_probe_read(&moisture, &raw_mv) == ESP_OK);

            // Sag measurement lights the LEDs: never on button presses,
            // ~daily otherwise (internal resistance drifts over weeks).
            static int64_t s_last_sag_us = -1;
            const int64_t now_us = esp_timer_get_time();
            const bool measure_sag = !(bits & APP_SAMPLE_SHOW_LED) &&
                (s_last_sag_us < 0 || now_us - s_last_sag_us > (int64_t)24 * 60 * 60 * 1000000LL);

            battery_reading_t battery;
            const bool have_battery = (battery_read(&battery, measure_sag) == ESP_OK);
            if (have_battery && measure_sag) {
                s_last_sag_us = now_us;
            }

            if (have_moisture) {
                report_moisture(moisture);
                if (bits & APP_SAMPLE_SHOW_LED) {
                    status_led_moisture_blink(moisture);
                }
            }
            if (have_battery) {
                report_battery(battery);
            }
        }
    }
}

void app_sample_request(uint32_t bits)
{
    if (s_sample_task) {
        xTaskNotify(s_sample_task, bits, eSetBits);
    }
}

static void sample_timer_cb(void *arg)
{
    app_sample_request(APP_SAMPLE_SILENT);
}

static void sampling_start(void)
{
    xTaskCreate(sample_task, "sample", 4096, NULL, 5, &s_sample_task);

    const esp_timer_create_args_t args = {
        .callback = sample_timer_cb,
        .name = "sample_timer",
    };
    ABORT_APP_ON_FAILURE(esp_timer_create(&args, &s_sample_timer) == ESP_OK,
                         ESP_LOGE(TAG, "Failed to create sample timer"));
    esp_timer_start_periodic(s_sample_timer, (uint64_t)CONFIG_SOIL_SAMPLE_INTERVAL_SECONDS * 1000000);

    // First reading shortly after boot, once the stack has settled.
    app_sample_request(APP_SAMPLE_SILENT);
}

// ---------------------------------------------------------------------------
// Antenna - the XIAO ESP32-C6 routes RF through a switch: GPIO3 low enables
// the switch, GPIO14 high selects the external U.FL antenna (Seeed's design
// for this kit). Held so the selection survives light sleep.

static void antenna_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_RF_SWITCH_EN) | (1ULL << PIN_ANTENNA_SEL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(PIN_RF_SWITCH_EN, 0);
    gpio_set_level(PIN_ANTENNA_SEL, 1);
    gpio_hold_en(PIN_RF_SWITCH_EN);
    gpio_hold_en(PIN_ANTENNA_SEL);
    ESP_LOGI(TAG, "External antenna selected (GPIO%d low, GPIO%d high)", PIN_RF_SWITCH_EN, PIN_ANTENNA_SEL);
}

// ---------------------------------------------------------------------------
// Boot diagnostics: brownout resets blink red at boot and bump an NVS
// counter, observable without serial.

static esp_reset_reason_t s_reset_reason;
static uint32_t s_brownout_count;

static void boot_diag_log(void);

static void brownout_diag(esp_reset_reason_t reset_reason)
{
    s_reset_reason = reset_reason;
    nvs_handle_t nvs;
    if (nvs_open("diag", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, "brownouts", &s_brownout_count);
        if (reset_reason == ESP_RST_BROWNOUT) {
            s_brownout_count++;
            nvs_set_u32(nvs, "brownouts", s_brownout_count);
            nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    boot_diag_log();
    if (reset_reason == ESP_RST_BROWNOUT) {
        status_led_blink(LED_RED, 5, 100);
    }
}

// Logged at boot and again after Matter start: the USB-JTAG monitor
// reconnect drops the first ~1 s of boot output.
static void boot_diag_log(void)
{
    if (s_reset_reason == ESP_RST_BROWNOUT) {
        ESP_LOGW(TAG, "Reset reason: BROWNOUT - supply sagged (lifetime count: %" PRIu32 ")", s_brownout_count);
    } else {
        ESP_LOGI(TAG, "Reset reason: %d (lifetime brownouts: %" PRIu32 ")", s_reset_reason, s_brownout_count);
    }
}

// ---------------------------------------------------------------------------
// The 802.15.4 driver defaults to +20 dBm (~350 mA TX peaks); cap it once
// the Thread stack is up.

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
static void thread_txpower_set(void)
{
    esp_openthread_lock_acquire(portMAX_DELAY);
    otInstance *instance = esp_openthread_get_instance();
    if (instance != NULL) {
        otPlatRadioSetTransmitPower(instance, CONFIG_SOIL_THREAD_TX_POWER_DBM);
        ESP_LOGI(TAG, "Thread TX power capped at %d dBm", CONFIG_SOIL_THREAD_TX_POWER_DBM);
    }
    esp_openthread_lock_release();
}
#endif

// ---------------------------------------------------------------------------
// Matter callbacks

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        status_led_blink(LED_GREEN, 3, 200);
        app_sample_request(APP_SAMPLE_SILENT);
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        status_led_blink_forever(LED_YELLOW, 1000);
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        status_led_stop();
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved: {
        ESP_LOGI(TAG, "Fabric removed successfully");
        if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
            chip::CommissioningWindowManager &commissionMgr =
                chip::Server::GetInstance().GetCommissioningWindowManager();
            constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
            if (!commissionMgr.IsCommissioningWindowOpen()) {
                CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(
                    kTimeoutSeconds, chip::CommissioningWindowAdvertisement::kDnssdOnly);
                if (err != CHIP_NO_ERROR) {
                    ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
                }
            }
        }
        break;
    }

    default:
        break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id,
                                       uint8_t effect_id, uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    if (type == identification::START) {
        status_led_blink_forever(LED_YELLOW, 500);
    } else if (type == identification::STOP) {
        status_led_stop();
    } else {
        status_led_blink(LED_YELLOW, 3, 200);
    }
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id,
                                         uint32_t cluster_id, uint32_t attribute_id,
                                         esp_matter_attr_val_t *val, void *priv_data)
{
    // Sensor-only device: nothing to drive from controller-side writes.
    return ESP_OK;
}

// ---------------------------------------------------------------------------

extern "C" void app_main()
{
    const esp_reset_reason_t reset_reason = esp_reset_reason();

    antenna_init();

    nvs_flash_init();

    ABORT_APP_ON_FAILURE(status_led_init() == ESP_OK, ESP_LOGE(TAG, "Failed to init status LEDs"));
    brownout_diag(reset_reason);
    ABORT_APP_ON_FAILURE(soil_probe_init() == ESP_OK, ESP_LOGE(TAG, "Failed to init soil probe"));
    ABORT_APP_ON_FAILURE(battery_init() == ESP_OK, ESP_LOGE(TAG, "Failed to init battery ADC"));
    ABORT_APP_ON_FAILURE(button_init() == ESP_OK, ESP_LOGE(TAG, "Failed to init button"));

#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true,
#endif
    };
    esp_pm_configure(&pm_config);
#endif

    // Data model: root node (EP0) + battery power source, soil sensor (EP1).
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    endpoint_t *root = endpoint::get(node, 0);
    ABORT_APP_ON_FAILURE(root != nullptr, ESP_LOGE(TAG, "Failed to get root endpoint"));

    cluster::power_source::config_t ps_config;
    ps_config.status = 1;  // Active
    ps_config.order = 0;
    strncpy(ps_config.description, "Battery", sizeof(ps_config.description));
    ps_config.feature_flags = cluster::power_source::feature::battery::get_id();
    ps_config.features.battery.bat_charge_level = (uint8_t)PowerSource::BatChargeLevelEnum::kOk;
    ps_config.features.battery.bat_replacement_needed = false;
    ps_config.features.battery.bat_replaceability = (uint8_t)PowerSource::BatReplaceabilityEnum::kUserReplaceable;
    cluster_t *ps_cluster = cluster::power_source::create(root, &ps_config, CLUSTER_FLAG_SERVER);
    ABORT_APP_ON_FAILURE(ps_cluster != nullptr, ESP_LOGE(TAG, "Failed to create power source cluster"));
    // BatPercentRemaining is in half-percent units, spec range 0-200.
    cluster::power_source::attribute::create_bat_percent_remaining(ps_cluster, nullable<uint8_t>(),
                                                                   nullable<uint8_t>(0), nullable<uint8_t>(200));

    endpoint::soil_sensor::config_t soil_config;
    endpoint_t *soil_ep = endpoint::soil_sensor::create(node, &soil_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(soil_ep != nullptr, ESP_LOGE(TAG, "Failed to create soil sensor endpoint"));
    soil_endpoint_id = endpoint::get_id(soil_ep);
    ESP_LOGI(TAG, "Soil sensor created with endpoint_id %d", soil_endpoint_id);

    // Must happen before esp_matter::start(): the SoilMeasurement server
    // cluster aborts the boot if no limits are registered for the endpoint.
    SoilMeasurement::SetSoilMoistureLimits(soil_endpoint_id, kSoilMoistureLimits);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif

    esp_err_t err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    thread_txpower_set();
#endif
    boot_diag_log();
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

    sampling_start();
}

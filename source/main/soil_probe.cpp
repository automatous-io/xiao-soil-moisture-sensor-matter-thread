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

// soil_probe.cpp

#include "soil_probe.h"
#include "status_led.h"
#include "app_priv.h"

#include <esp_check.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <driver/ledc.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "soil_probe";

// 200 kHz excitation, 68% duty - same as the ESPHome firmware. At 200 kHz
// the LEDC timer resolution caps out around 8 bits (80 MHz / 200 kHz = 400).
#define EXCITATION_FREQ_HZ   200000
#define EXCITATION_DUTY_8BIT 174  // 68% of 256

#define SOIL_ADC_CHANNEL ADC_CHANNEL_1  // GPIO1
#define SAMPLE_COUNT     10
#define SAMPLE_GAP_MS    50

// A valid calibration needs the dry reference meaningfully above wet.
#define CAL_MIN_SPAN_MV 200

#define NVS_NAMESPACE "soilcal"
#define NVS_KEY_DRY   "dry_mv"
#define NVS_KEY_WET   "wet_mv"

static adc_oneshot_unit_handle_t s_adc_unit;
static adc_cali_handle_t s_cali;
static esp_pm_lock_handle_t s_pm_lock;
static int32_t s_dry_mv = CONFIG_SOIL_CAL_DEFAULT_DRY_MV;
static int32_t s_wet_mv = CONFIG_SOIL_CAL_DEFAULT_WET_MV;

adc_oneshot_unit_handle_t soil_probe_adc_unit(void)
{
    return s_adc_unit;
}

static void excitation_on(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, EXCITATION_DUTY_8BIT);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void excitation_off(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// Average SAMPLE_COUNT calibrated reads, SAMPLE_GAP_MS apart. Excitation and
// the PM lock must already be held by the caller.
static esp_err_t read_averaged_mv(int *out_mv)
{
    int64_t sum = 0;
    int valid = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc_unit, SOIL_ADC_CHANNEL, &raw) == ESP_OK) {
            int mv = 0;
            if (adc_cali_raw_to_voltage(s_cali, raw, &mv) == ESP_OK) {
                sum += mv;
                valid++;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_GAP_MS));
    }
    if (valid == 0) {
        return ESP_FAIL;
    }
    *out_mv = (int)(sum / valid);
    return ESP_OK;
}

// One full excited measurement: PM lock -> PWM on -> settle -> average -> off.
static esp_err_t measure_mv(int *out_mv)
{
    esp_pm_lock_acquire(s_pm_lock);
    excitation_on();
    vTaskDelay(pdMS_TO_TICKS(CONFIG_SOIL_EXCITATION_SETTLE_MS));
    esp_err_t err = read_averaged_mv(out_mv);
    excitation_off();
    esp_pm_lock_release(s_pm_lock);
    return err;
}

static uint8_t mv_to_percent(int mv)
{
    int32_t span = s_dry_mv - s_wet_mv;
    if (span <= 0) {
        return 0;
    }
    int32_t pct = (100 * (s_dry_mv - mv)) / span;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (uint8_t)pct;
}

static void load_calibration(void)
{
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        ESP_LOGI(TAG, "No stored calibration, using defaults dry=%ld mV wet=%ld mV", s_dry_mv, s_wet_mv);
        return;
    }
    nvs_get_i32(nvs, NVS_KEY_DRY, &s_dry_mv);
    nvs_get_i32(nvs, NVS_KEY_WET, &s_wet_mv);
    nvs_close(nvs);
    ESP_LOGI(TAG, "Loaded calibration dry=%ld mV wet=%ld mV", s_dry_mv, s_wet_mv);
}

static esp_err_t save_calibration(int32_t dry_mv, int32_t wet_mv)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs), TAG, "nvs_open failed");
    esp_err_t err = nvs_set_i32(nvs, NVS_KEY_DRY, dry_mv);
    if (err == ESP_OK) {
        err = nvs_set_i32(nvs, NVS_KEY_WET, wet_mv);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t soil_probe_init(void)
{
    // Excitation PWM, created idle (duty 0).
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = EXCITATION_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "ledc timer config failed");

    ledc_channel_config_t chan_cfg = {
        .gpio_num = PIN_SOIL_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&chan_cfg), TAG, "ledc channel config failed");

    // ADC1 oneshot unit, shared with battery.cpp.
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &s_adc_unit), TAG, "adc unit failed");

    adc_oneshot_chan_cfg_t adc_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc_unit, SOIL_ADC_CHANNEL, &adc_cfg), TAG,
                        "adc channel config failed");

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = SOIL_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali), TAG,
                        "adc calibration failed");

    ESP_RETURN_ON_ERROR(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "soil_probe", &s_pm_lock),
                        TAG, "pm lock create failed");

    load_calibration();
    ESP_LOGI(TAG, "Soil probe initialized (PWM GPIO%d, ADC GPIO%d)", PIN_SOIL_PWM, PIN_SOIL_ADC);
    return ESP_OK;
}

esp_err_t soil_probe_read(uint8_t *moisture_percent, int *raw_mv)
{
    int mv = 0;
    ESP_RETURN_ON_ERROR(measure_mv(&mv), TAG, "measurement failed");
    *raw_mv = mv;
    *moisture_percent = mv_to_percent(mv);
    ESP_LOGI(TAG, "Soil reading: %d mV -> %u%% (dry=%ld wet=%ld)", mv, *moisture_percent, s_dry_mv, s_wet_mv);
    return ESP_OK;
}

esp_err_t soil_probe_calibrate(void)
{
    ESP_LOGI(TAG, "Calibration: hold probe in DRY AIR (red blinking, 10 s)");
    status_led_blink_forever(LED_RED, 500);
    vTaskDelay(pdMS_TO_TICKS(10000));
    status_led_stop();

    int dry_new = 0;
    esp_err_t err = measure_mv(&dry_new);
    if (err != ESP_OK) {
        status_led_blink(LED_RED, 5, 200);
        return err;
    }
    ESP_LOGI(TAG, "Calibration: dry reference %d mV", dry_new);
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Calibration: place probe in WATER (green blinking, 10 s)");
    status_led_blink_forever(LED_GREEN, 500);
    vTaskDelay(pdMS_TO_TICKS(10000));
    status_led_stop();

    int wet_new = 0;
    err = measure_mv(&wet_new);
    if (err != ESP_OK) {
        status_led_blink(LED_RED, 5, 200);
        return err;
    }
    ESP_LOGI(TAG, "Calibration: wet reference %d mV", wet_new);

    if (dry_new <= wet_new + CAL_MIN_SPAN_MV) {
        ESP_LOGW(TAG, "Calibration rejected: dry %d mV not above wet %d mV by %d mV", dry_new, wet_new,
                 CAL_MIN_SPAN_MV);
        status_led_blink(LED_RED, 5, 200);
        return ESP_ERR_INVALID_STATE;
    }

    err = save_calibration(dry_new, wet_new);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist calibration: %s", esp_err_to_name(err));
        status_led_blink(LED_RED, 5, 200);
        return err;
    }

    s_dry_mv = dry_new;
    s_wet_mv = wet_new;
    ESP_LOGI(TAG, "Calibration saved: dry=%ld mV wet=%ld mV", s_dry_mv, s_wet_mv);
    status_led_blink(LED_GREEN, 5, 200);
    return ESP_OK;
}

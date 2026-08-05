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

// battery.cpp

#include "battery.h"
#include "soil_probe.h"
#include "status_led.h"
#include "app_priv.h"

#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "battery";

#define BATTERY_ADC_CHANNEL ADC_CHANNEL_0  // GPIO0
#define SAMPLE_COUNT        5
#define SAMPLE_GAP_MS       20
#define LOAD_SETTLE_MS      30

static adc_cali_handle_t s_cali;

esp_err_t battery_init(void)
{
    adc_oneshot_unit_handle_t unit = soil_probe_adc_unit();
    ESP_RETURN_ON_FALSE(unit != NULL, ESP_ERR_INVALID_STATE, TAG, "soil_probe_init must run first");

    adc_oneshot_chan_cfg_t adc_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(unit, BATTERY_ADC_CHANNEL, &adc_cfg), TAG,
                        "adc channel config failed");

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali), TAG,
                        "adc calibration failed");

    ESP_LOGI(TAG, "Battery ADC initialized (GPIO%d)", PIN_BATTERY_ADC);
    return ESP_OK;
}

static esp_err_t sample_avg_mv(adc_oneshot_unit_handle_t unit, int *avg_mv)
{
    int64_t sum = 0;
    int valid = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        int raw = 0;
        if (adc_oneshot_read(unit, BATTERY_ADC_CHANNEL, &raw) == ESP_OK) {
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
    *avg_mv = (int)(sum / valid);
    return ESP_OK;
}

esp_err_t battery_read(battery_reading_t *reading, bool measure_sag)
{
    adc_oneshot_unit_handle_t unit = soil_probe_adc_unit();
    ESP_RETURN_ON_FALSE(unit != NULL, ESP_ERR_INVALID_STATE, TAG, "battery not initialized");

    // Persists between loaded measurements; -1 = never measured. Only
    // meaningful on battery power (on USB the cell is unloaded).
    static int s_last_sag_mv = -1;

    ESP_RETURN_ON_ERROR(sample_avg_mv(unit, &reading->rest_mv), TAG, "rest measurement failed");

    if (measure_sag) {
        // Sag between resting and LED-loaded readings exposes internal
        // resistance, which resting voltage alone cannot.
        status_led_load(true);
        vTaskDelay(pdMS_TO_TICKS(LOAD_SETTLE_MS));
        esp_err_t err = sample_avg_mv(unit, &reading->loaded_mv);
        status_led_load(false);
        ESP_RETURN_ON_ERROR(err, TAG, "loaded measurement failed");

        s_last_sag_mv = reading->rest_mv - reading->loaded_mv;
        if (s_last_sag_mv < 0) {
            s_last_sag_mv = 0;
        }
    } else {
        reading->loaded_mv = reading->rest_mv;
    }
    reading->sag_mv = s_last_sag_mv;

    int32_t pct = (100 * (reading->rest_mv - CONFIG_SOIL_BATTERY_EMPTY_MV)) /
                  (CONFIG_SOIL_BATTERY_FULL_MV - CONFIG_SOIL_BATTERY_EMPTY_MV);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    reading->percent = (uint8_t)pct;

    if (measure_sag) {
        ESP_LOGI(TAG, "Battery: %d mV rest, %d mV loaded (sag %d mV) -> %u%%",
                 reading->rest_mv, reading->loaded_mv, reading->sag_mv, reading->percent);
    } else {
        ESP_LOGI(TAG, "Battery: %d mV -> %u%% (last sag %d mV)",
                 reading->rest_mv, reading->percent, reading->sag_mv);
    }
    return ESP_OK;
}

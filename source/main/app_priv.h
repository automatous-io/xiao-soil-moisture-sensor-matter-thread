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

// app_priv.h

#pragma once

#include <esp_err.h>
#include <esp_matter.h>

// XIAO ESP32-C6 soil moisture kit pin map.
#define PIN_BATTERY_ADC   GPIO_NUM_0   // D0 — battery voltage (divided)
#define PIN_SOIL_ADC      GPIO_NUM_1   // D1 — soil probe output
#define PIN_BUTTON        GPIO_NUM_2   // D2 — user button, active low
#define PIN_RF_SWITCH_EN  GPIO_NUM_3   // drive LOW to enable the antenna RF switch
#define PIN_ANTENNA_SEL   GPIO_NUM_14  // drive HIGH to select the external U.FL antenna
#define PIN_LED_YELLOW    GPIO_NUM_18  // D10
#define PIN_LED_GREEN     GPIO_NUM_19  // D8
#define PIN_LED_RED       GPIO_NUM_20  // D9
#define PIN_SOIL_PWM      GPIO_NUM_21  // D3 — 200 kHz probe excitation

// Soil sensor endpoint id, set during data model creation in app_main.cpp.
extern uint16_t soil_endpoint_id;

// Sample worker request bits. Requests are coalesced; CAL takes priority.
#define APP_SAMPLE_SILENT   (1u << 0)  // sample + report, no LEDs (timer path)
#define APP_SAMPLE_SHOW_LED (1u << 1)  // sample + report + moisture status blink
#define APP_SAMPLE_CAL      (1u << 2)  // run the dry/wet calibration flow

// Queue a request onto the sample worker task. Safe from any task.
void app_sample_request(uint32_t bits);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "esp_openthread_types.h"

#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()                                           \
    {                                                                                   \
        .radio_mode = RADIO_MODE_NATIVE,                                                \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()                                            \
    {                                                                                   \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,                              \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                            \
    {                                                                                   \
        .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10, \
    }
#endif

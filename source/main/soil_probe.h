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

// soil_probe.h

#pragma once

#include <esp_err.h>
#include <stdint.h>
#include "esp_adc/adc_oneshot.h"

// Initialize the soil probe: 200 kHz excitation PWM (idle), ADC channel,
// PM lock, and calibration values from NVS. Call once during boot.
esp_err_t soil_probe_init(void);

// Take a measurement: excite the probe, settle, average 10 ADC reads.
// Returns the calibrated moisture percent (0-100) and the raw averaged
// millivolts. Blocks for roughly settle + 10 * 50 ms; call from the sample
// worker task, not from timer or Matter context.
esp_err_t soil_probe_read(uint8_t *moisture_percent, int *raw_mv);

// Run the interactive dry/wet calibration flow (blocking, ~30 s):
// red blink 10 s with probe in dry air -> dry reference, then green blink
// 10 s with probe in water -> wet reference. Persists to NVS on success.
// Returns ESP_OK if the new calibration was accepted.
esp_err_t soil_probe_calibrate(void);

// The shared ADC unit handle (ADC1), also used by battery.cpp - the oneshot
// unit can only be claimed once per ADC peripheral.
adc_oneshot_unit_handle_t soil_probe_adc_unit(void);

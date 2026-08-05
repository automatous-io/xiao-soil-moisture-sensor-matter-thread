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

// battery.h

#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t percent;   // state of charge from the resting voltage
    int rest_mv;       // unloaded cell voltage
    int loaded_mv;     // cell voltage with the LEDs driven as a load
    int sag_mv;        // rest_mv - loaded_mv; internal-resistance proxy,
                       // -1 until first measured
} battery_reading_t;

// Initialize the battery ADC channel. Call after soil_probe_init() (shares
// the ADC1 oneshot unit).
esp_err_t battery_init(void);

// Measure the battery. measure_sag adds a second average with the LEDs on
// as a load (~150 ms, lights all three); otherwise the last sag is reused.
esp_err_t battery_read(battery_reading_t *reading, bool measure_sag);

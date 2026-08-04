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
#include <stdint.h>

// Initialize the battery ADC channel. Call after soil_probe_init() (shares
// the ADC1 oneshot unit).
esp_err_t battery_init(void);

// Read the battery level as a percent (0-100), averaging 5 ADC reads and
// applying the kit's linear mapping (1.0 V -> 0%, 1.5 V -> 100% at the pin).
esp_err_t battery_read(uint8_t *percent);

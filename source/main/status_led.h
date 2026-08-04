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

// status_led.h

#pragma once

#include <esp_err.h>
#include <stdint.h>

typedef enum {
    LED_RED,
    LED_YELLOW,
    LED_GREEN,
} led_color_t;

// Initialize the three status LEDs. Call once during boot.
esp_err_t status_led_init(void);

// Blink `count` on/off cycles of `color`, each phase lasting `period_ms`.
// Asynchronous (esp_timer driven); a new pattern replaces the current one.
// Holds a no-light-sleep PM lock for the duration of the pattern so blinks
// stay clean while power management is active.
void status_led_blink(led_color_t color, int count, int period_ms);

// Blink continuously until status_led_stop() is called.
void status_led_blink_forever(led_color_t color, int period_ms);

// Stop any running pattern and turn all LEDs off.
void status_led_stop(void);

// One long (1 s) blink of the color classifying `moisture_percent`:
// red = dry, yellow = almost dry, green = normal. Same thresholds as the
// original ESPHome firmware.
void status_led_moisture_blink(uint8_t moisture_percent);

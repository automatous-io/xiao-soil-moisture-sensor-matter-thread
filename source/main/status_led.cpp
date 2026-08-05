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

// status_led.cpp

#include "status_led.h"
#include "app_priv.h"

#include <esp_check.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_pm.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

static const char *TAG = "status_led";

// Moisture classification thresholds, carried over from the ESPHome
// firmware's ref_dry (0.23) and ref_wet (0.58) fractions.
#define MOISTURE_DRY_BELOW_PERCENT    23
#define MOISTURE_NORMAL_ABOVE_PERCENT 58

static esp_timer_handle_t s_timer;
static esp_pm_lock_handle_t s_pm_lock;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

// Active pattern state, guarded by s_lock. remaining < 0 means blink forever.
static gpio_num_t s_gpio = GPIO_NUM_NC;
static int s_remaining;
static bool s_on;
static bool s_pm_held;

static gpio_num_t color_to_gpio(led_color_t color)
{
    switch (color) {
    case LED_RED: return PIN_LED_RED;
    case LED_YELLOW: return PIN_LED_YELLOW;
    default: return PIN_LED_GREEN;
    }
}

static void all_off(void)
{
    gpio_set_level(PIN_LED_RED, 0);
    gpio_set_level(PIN_LED_YELLOW, 0);
    gpio_set_level(PIN_LED_GREEN, 0);
}

static void pm_release_if_held(void)
{
    if (s_pm_held) {
        esp_pm_lock_release(s_pm_lock);
        s_pm_held = false;
    }
}

static void timer_cb(void *arg)
{
    portENTER_CRITICAL(&s_lock);
    if (s_gpio == GPIO_NUM_NC) {
        portEXIT_CRITICAL(&s_lock);
        return;
    }
    s_on = !s_on;
    gpio_set_level(s_gpio, s_on);
    bool done = false;
    if (!s_on && s_remaining > 0 && --s_remaining == 0) {
        done = true;
        s_gpio = GPIO_NUM_NC;
    }
    portEXIT_CRITICAL(&s_lock);

    if (done) {
        esp_timer_stop(s_timer);
        all_off();
        pm_release_if_held();
    }
}

static void start_pattern(led_color_t color, int count, int period_ms)
{
    esp_timer_stop(s_timer);
    all_off();

    if (!s_pm_held && esp_pm_lock_acquire(s_pm_lock) == ESP_OK) {
        s_pm_held = true;
    }

    portENTER_CRITICAL(&s_lock);
    s_gpio = color_to_gpio(color);
    s_remaining = count;
    s_on = true;
    gpio_set_level(s_gpio, 1);
    portEXIT_CRITICAL(&s_lock);

    esp_timer_start_periodic(s_timer, (uint64_t)period_ms * 1000);
}

esp_err_t status_led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_LED_RED) | (1ULL << PIN_LED_YELLOW) | (1ULL << PIN_LED_GREEN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio_config failed");
    all_off();

    ESP_RETURN_ON_ERROR(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "status_led", &s_pm_lock),
                        TAG, "pm lock create failed");

    const esp_timer_create_args_t args = {
        .callback = timer_cb,
        .name = "status_led",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&args, &s_timer), TAG, "timer create failed");

    ESP_LOGI(TAG, "Status LEDs initialized (R:%d Y:%d G:%d)", PIN_LED_RED, PIN_LED_YELLOW, PIN_LED_GREEN);
    return ESP_OK;
}

void status_led_blink(led_color_t color, int count, int period_ms)
{
    start_pattern(color, count, period_ms);
}

void status_led_blink_forever(led_color_t color, int period_ms)
{
    start_pattern(color, -1, period_ms);
}

void status_led_stop(void)
{
    esp_timer_stop(s_timer);
    portENTER_CRITICAL(&s_lock);
    s_gpio = GPIO_NUM_NC;
    portEXIT_CRITICAL(&s_lock);
    all_off();
    pm_release_if_held();
}

void status_led_load(bool on)
{
    // Hold the PM lock: light sleep isolates GPIOs and would drop the load
    // mid-measurement.
    static bool s_load_pm_held;
    if (on) {
        if (!s_load_pm_held && esp_pm_lock_acquire(s_pm_lock) == ESP_OK) {
            s_load_pm_held = true;
        }
        gpio_set_level(PIN_LED_RED, 1);
        gpio_set_level(PIN_LED_YELLOW, 1);
        gpio_set_level(PIN_LED_GREEN, 1);
    } else {
        all_off();
        if (s_load_pm_held) {
            esp_pm_lock_release(s_pm_lock);
            s_load_pm_held = false;
        }
    }
}

void status_led_moisture_blink(uint8_t moisture_percent)
{
    led_color_t color;
    if (moisture_percent < MOISTURE_DRY_BELOW_PERCENT) {
        color = LED_RED;
    } else if (moisture_percent < MOISTURE_NORMAL_ABOVE_PERCENT) {
        color = LED_YELLOW;
    } else {
        color = LED_GREEN;
    }
    status_led_blink(color, 1, 1000);
}

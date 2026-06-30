// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Onboard status LED driver. A single task renders the current pattern from a
// phase counter, so status changes and the confirmation flash are responsive.

#include "led.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO 8
#define LED_LVL_ON 0    // active low: GPIO low = LED on
#define LED_LVL_OFF 1
#define TICK_MS 40

static volatile led_state_t s_state = LED_SEARCHING;
static volatile int s_fault = 0;       // 0 = none, else blink-code count
static volatile bool s_flash_req = false;

static inline void out(bool on)
{
    gpio_set_level(LED_GPIO, on ? LED_LVL_ON : LED_LVL_OFF);
}

static void led_task(void *arg)
{
    uint32_t phase = 0;        // ms since boot (mod patterns)
    uint32_t flash_until = 0;  // phase value the confirmation flash ends at
    for (;;) {
        if (s_flash_req) {
            s_flash_req = false;
            flash_until = phase + 360;  // 3 quick flashes at 120ms each
        }

        bool on;
        if (phase < flash_until) {
            on = ((phase / 120) % 2) == 0;
        } else if (s_fault > 0) {
            uint32_t period = (uint32_t)s_fault * 270 + 900;  // blinks + gap
            uint32_t t = phase % period;
            on = (t < (uint32_t)s_fault * 270) && ((t % 270) < 120);
        } else {
            switch (s_state) {
            case LED_CONNECTED: on = true; break;
            case LED_SEARCHING: on = (phase % 2000) < 150; break;
            case LED_OTA:       on = (phase % 140) < 70; break;
            case LED_CONFIG: {
                uint32_t t = phase % 1800;
                on = (t < 120) || (t >= 240 && t < 360);
                break;
            }
            case LED_OFF:
            default: on = false; break;
            }
        }
        out(on);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
        phase += TICK_MS;
    }
}

void led_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    out(false);
    xTaskCreate(led_task, "led", 1536, NULL, 3, NULL);
}

void led_set(led_state_t s)
{
    if (s_fault == 0) {
        s_state = s;
    }
}

void led_fault(int blinks)
{
    s_fault = blinks;
}

void led_flash(void)
{
    s_flash_req = true;
}

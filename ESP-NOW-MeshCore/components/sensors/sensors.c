//
// File Path: ESP-NOW-MeshCore/components/sensors/sensors.c
// Brief:     Source file for sensors component.
//            Handles reading hardware inputs (buttons/simulated sensors).
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.4.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-07
//

#include "sensors.h"
#include "shared_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>

#if DEVICE_ROLE == ROLE_SENSOR || DEVICE_ROLE == ROLE_BOTH

static const char *TAG = "sensors";

void sensors_init(void)
{
#if ACTIVE_APP_SAMPLE == 1
    ESP_LOGI(TAG, "Initializing hardware sensors... (MOCK MODE)");
#elif ACTIVE_APP_SAMPLE == 2
    ESP_LOGI(TAG, "Initializing hardware sensors... (MOMENTARY BUTTON MODE)");
    gpio_set_direction(BUTTON_SENSOR_GPIO, GPIO_MODE_INPUT);
#if USE_INTERNAL_PULLUPS
    gpio_pullup_en(BUTTON_SENSOR_GPIO);
#else
    gpio_pullup_dis(BUTTON_SENSOR_GPIO);
#endif
#elif ACTIVE_APP_SAMPLE == 3
    ESP_LOGI(TAG, "Initializing hardware sensors... (TOGGLE BUTTON MODE)");
    gpio_set_direction(BUTTON_SENSOR_GPIO, GPIO_MODE_INPUT);
#if USE_INTERNAL_PULLUPS
    gpio_pullup_en(BUTTON_SENSOR_GPIO);
#else
    gpio_pullup_dis(BUTTON_SENSOR_GPIO);
#endif
#elif ACTIVE_APP_SAMPLE == 4
    ESP_LOGI(TAG, "Initializing hardware sensors... (4-NODE TOGGLE MODE)");
    const int btn_pins[4] = {BTN1_GPIO, BTN2_GPIO, BTN3_GPIO, BTN4_GPIO};
    for(int i = 0; i < 4; i++) {
        gpio_set_direction(btn_pins[i], GPIO_MODE_INPUT);
#if USE_INTERNAL_PULLUPS
        gpio_pullup_en(btn_pins[i]);
#else
        gpio_pullup_dis(btn_pins[i]);
#endif
    }
#elif ACTIVE_APP_SAMPLE == 5
    ESP_LOGI(TAG, "Initializing hardware sensors... (MOD-BUS MODE)");
    // In this sample, the sensor component might act as a MOD-BUS master or simply pass data
#endif
}

#if ACTIVE_APP_SAMPLE == 3
static bool s_toggle_led_state = false;
static int s_last_btn_state = 1;
#elif ACTIVE_APP_SAMPLE == 4
static int s_last_btn_states[4] = {1, 1, 1, 1};
#endif

void sensors_force_initial_state(void)
{
#if ACTIVE_APP_SAMPLE == 3
    s_toggle_led_state = false;
#elif ACTIVE_APP_SAMPLE == 4
    for(int i=0; i<4; i++) {
        s_last_btn_states[i] = 1;
        // Optionally update local LEDs if we want them synced
        // gpio_set_level(led_pins[i], 0);
    }
#endif
    ESP_LOGW(TAG, "Sensor internal states forcefully reset to initial.");
}

void sensors_read(char *buffer, size_t max_len)
{
    if (!buffer || max_len == 0) return;

#if ACTIVE_APP_SAMPLE == 1
    // --- MOCK SENSOR DATA ---
    int64_t current_time_sec = esp_timer_get_time() / 1000000;
    float mock_temp = (current_time_sec / 15) % 2 == 0 ? 24.5f : 25.0f;
    int mock_humidity = 60; // Static humidity
    
    snprintf(buffer, max_len, "[%s]TEMP:%.1fC|HUM:%d%%", DATA_KEYWORDS, mock_temp, mock_humidity);

#elif ACTIVE_APP_SAMPLE == 2
    // --- REMOTE POWERING LED (MOMENTARY BUTTON) ---
    int btn_state = gpio_get_level(BUTTON_SENSOR_GPIO);
    // Assuming active low button (pressed = 0)
    if (btn_state == 0) {
        snprintf(buffer, max_len, "[%s]CMD:LED_ON", DATA_KEYWORDS);
    } else {
        snprintf(buffer, max_len, "[%s]CMD:LED_OFF", DATA_KEYWORDS);
    }

#elif ACTIVE_APP_SAMPLE == 3
    // --- REMOTE POWERING LED (TOGGLE BUTTON) ---
    int current_btn_state = gpio_get_level(BUTTON_SENSOR_GPIO);

    // Detect falling edge (Press)
    if (current_btn_state == 0 && s_last_btn_state == 1) {
        s_toggle_led_state = !s_toggle_led_state; // Toggle the state
    }
    s_last_btn_state = current_btn_state;

    if (s_toggle_led_state) {
        snprintf(buffer, max_len, "[%s]CMD:LED_ON", DATA_KEYWORDS);
    } else {
        snprintf(buffer, max_len, "[%s]CMD:LED_OFF", DATA_KEYWORDS);
    }

#elif ACTIVE_APP_SAMPLE == 4
    // --- 4-NODE REMOTE CONTROL (4 TOGGLE BUTTONS) ---
    const int btn_pins[4] = {BTN1_GPIO, BTN2_GPIO, BTN3_GPIO, BTN4_GPIO};
    const int led_pins[4] = {LED1_GPIO, LED2_GPIO, LED3_GPIO, LED4_GPIO};

    for(int i = 0; i < 4; i++) {
        int current_btn_state = gpio_get_level(btn_pins[i]);
        // Detect falling edge (Press)
        if (current_btn_state == 0 && s_last_btn_states[i] == 1) {
            // Toggle the local LED immediately to update state
            int new_state = !gpio_get_level(led_pins[i]);
            gpio_set_level(led_pins[i], new_state);
            ESP_LOGI(TAG, "Button %d pressed! Toggled LED %d to %d", i+1, i+1, new_state);
        }
        s_last_btn_states[i] = current_btn_state;
    }

    // Always output the current state of all 4 LEDs
    snprintf(buffer, max_len, "[%s]CMD:LEDS:%d,%d,%d,%d", DATA_KEYWORDS,
             gpio_get_level(LED1_GPIO), 
             gpio_get_level(LED2_GPIO), 
             gpio_get_level(LED3_GPIO), 
             gpio_get_level(LED4_GPIO));

#elif ACTIVE_APP_SAMPLE == 5
    // --- MOD-BUS SENSOR DATA ---
    // Here we would read data from a MOD-BUS slave device and format it.
    // For now, we just send a mock payload indicating MOD-BUS data.
    snprintf(buffer, max_len, "[%s]MODBUS_DATA:READY", DATA_KEYWORDS);
#endif
}

#endif // DEVICE_ROLE == ROLE_SENSOR || DEVICE_ROLE == ROLE_BOTH

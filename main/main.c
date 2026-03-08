#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BIO_BUBBLER";

// Status LED pins (RGB)
#define LED_RED GPIO_NUM_2
#define LED_GREEN GPIO_NUM_4
#define LED_BLUE GPIO_NUM_5

// Pump control pins (via relays)
#define PUMP1_PIN GPIO_NUM_12
#define PUMP2_PIN GPIO_NUM_13

// Button pins
#define BUTTON_MODE_PIN GPIO_NUM_32
#define BUTTON_CONFIRM_PIN GPIO_NUM_33

// Timing constants (in milliseconds)
#define BUTTON_DEBOUNCE_MS 20
#define BUTTON_CHECK_INTERVAL_MS 10
#define LED_FLASH_INTERVAL_MS 500
#define PULSE_DURATION_MS 30000

// Machine state enum
typedef enum {
    STATE_IDLE,       // Green - pumps off, safe to work
    STATE_CONTINUOUS, // Blue - both pumps running continuously
    STATE_PULSE       // Red - pumps running briefly, be careful
} machine_state_t;

// Pending state enum (for flashing LED before confirmation)
typedef enum {
    PENDING_NONE,
    PENDING_PULSE,
    PENDING_CONTINUOUS
} pending_state_t;

// Current machine state
static machine_state_t current_state = STATE_IDLE;
static pending_state_t pending_state = PENDING_NONE;

// LED flashing control
static int flash_led = 0;
static uint32_t flash_timer = 0;

// Pulse timer (for 30-second pump cycles)
static uint32_t pulse_timer = 0;

// Emergency stop toggle for continuous mode
static int continuous_pumps_enabled = 1;

void set_machine_state(machine_state_t state)
{
    // Turn off all status LEDs
    gpio_set_level(LED_RED, 0);
    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_BLUE, 0);

    // Clear pending state when actually transitioning
    pending_state = PENDING_NONE;
    flash_led = 0;
    flash_timer = 0;

    // Set appropriate LED and pump states based on new state
    switch (state) {
        case STATE_IDLE:
            gpio_set_level(LED_GREEN, 1);
            gpio_set_level(PUMP1_PIN, 0);
            gpio_set_level(PUMP2_PIN, 0);
            continuous_pumps_enabled = 1;  // Reset emergency stop for next continuous session
            break;

        case STATE_CONTINUOUS:
            gpio_set_level(LED_BLUE, 1);
            // Only enable pumps if emergency stop hasn't been triggered
            if (continuous_pumps_enabled) {
                gpio_set_level(PUMP1_PIN, 1);
                gpio_set_level(PUMP2_PIN, 1);
            }
            break;

        case STATE_PULSE:
            gpio_set_level(LED_RED, 1);
            // Don't start pumps automatically, wait for confirm button to trigger cycle
            gpio_set_level(PUMP1_PIN, 0);
            gpio_set_level(PUMP2_PIN, 0);
            pulse_timer = 0;
            break;
    }

    current_state = state;
}

void init_gpio(void)
{
    // Initialize status LEDs
    gpio_reset_pin(LED_RED);
    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_GREEN);
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_BLUE);
    gpio_set_direction(LED_BLUE, GPIO_MODE_OUTPUT);

    // Initialize pump control pins
    gpio_reset_pin(PUMP1_PIN);
    gpio_set_direction(PUMP1_PIN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(PUMP2_PIN);
    gpio_set_direction(PUMP2_PIN, GPIO_MODE_OUTPUT);

    // Initialize button pins as inputs with internal pullups
    gpio_reset_pin(BUTTON_MODE_PIN);
    gpio_set_direction(BUTTON_MODE_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_MODE_PIN, GPIO_PULLUP_ONLY);

    gpio_reset_pin(BUTTON_CONFIRM_PIN);
    gpio_set_direction(BUTTON_CONFIRM_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_CONFIRM_PIN, GPIO_PULLUP_ONLY);
}

// Debounce button and return 1 if pressed
static int is_button_pressed(gpio_num_t pin)
{
    // Check if pin is low (button pressed, assuming active-low)
    if (gpio_get_level(pin) == 0) {
        vTaskDelay(BUTTON_DEBOUNCE_MS / portTICK_PERIOD_MS);
        if (gpio_get_level(pin) == 0) {
            return 1;
        }
    }
    return 0;
}

void handle_mode_button(void)
{
    // Cycle through pending states
    if (pending_state == PENDING_NONE) {
        // If no pending state, start flashing PULSE
        pending_state = PENDING_PULSE;
        flash_led = 1;
        flash_timer = 0;
        ESP_LOGI(TAG, "Mode: PENDING_PULSE (flashing red)");
    } else if (pending_state == PENDING_PULSE) {
        // If PULSE pending, switch to CONTINUOUS pending
        pending_state = PENDING_CONTINUOUS;
        flash_timer = 0;
        ESP_LOGI(TAG, "Mode: PENDING_CONTINUOUS (flashing blue)");
    } else if (pending_state == PENDING_CONTINUOUS) {
        // If CONTINUOUS pending, go back to IDLE (confirmed)
        pending_state = PENDING_NONE;
        flash_led = 0;
        set_machine_state(STATE_IDLE);
        ESP_LOGI(TAG, "Mode: Back to IDLE");
    }
}

void handle_confirm_button(void)
{
    // Handle based on pending state or current state
    if (pending_state == PENDING_PULSE) {
        // Confirm PULSE state
        ESP_LOGI(TAG, "Confirmed: STATE_PULSE");
        set_machine_state(STATE_PULSE);
    } else if (pending_state == PENDING_CONTINUOUS) {
        // Confirm CONTINUOUS state
        ESP_LOGI(TAG, "Confirmed: STATE_CONTINUOUS");
        set_machine_state(STATE_CONTINUOUS);
    } else if (current_state == STATE_PULSE) {
        // In PULSE state, trigger 30-second pump cycle
        ESP_LOGI(TAG, "PULSE: Starting 30s pump cycle");
        gpio_set_level(PUMP1_PIN, 1);
        gpio_set_level(PUMP2_PIN, 1);
        pulse_timer = PULSE_DURATION_MS;
    } else if (current_state == STATE_CONTINUOUS) {
        // In CONTINUOUS state, emergency stop/start toggle
        continuous_pumps_enabled = !continuous_pumps_enabled;
        if (continuous_pumps_enabled) {
            ESP_LOGI(TAG, "CONTINUOUS: Pumps ON");
            gpio_set_level(PUMP1_PIN, 1);
            gpio_set_level(PUMP2_PIN, 1);
        } else {
            ESP_LOGI(TAG, "CONTINUOUS: Pumps OFF (emergency stop)");
            gpio_set_level(PUMP1_PIN, 0);
            gpio_set_level(PUMP2_PIN, 0);
        }
    }
    // In IDLE state, confirm button does nothing
}

void update_led_flash(void)
{
    // Handle flashing for pending states
    if (flash_led) {
        flash_timer += BUTTON_CHECK_INTERVAL_MS;
        
        if (flash_timer >= LED_FLASH_INTERVAL_MS) {
            flash_timer = 0;
        }
        
        // Turn off all LEDs during off phase of flash
        if (flash_timer < LED_FLASH_INTERVAL_MS / 2) {
            gpio_set_level(LED_RED, 0);
            gpio_set_level(LED_GREEN, 0);
            gpio_set_level(LED_BLUE, 0);
        } else {
            // On phase - flash the appropriate color
            if (pending_state == PENDING_PULSE) {
                gpio_set_level(LED_RED, 1);
            } else if (pending_state == PENDING_CONTINUOUS) {
                gpio_set_level(LED_BLUE, 1);
            }
        }
    }
}

void update_pulse_timer(void)
{
    // Handle pulse timer countdown
    if (current_state == STATE_PULSE && pulse_timer > 0) {
        pulse_timer -= BUTTON_CHECK_INTERVAL_MS;
        
        if (pulse_timer <= 0) {
            // Timer expired, turn off pumps
            pulse_timer = 0;
            gpio_set_level(PUMP1_PIN, 0);
            gpio_set_level(PUMP2_PIN, 0);
        }
    }
}

void app_main(void)
{
    init_gpio();

    // Start in idle state (green LED on, pumps off)
    set_machine_state(STATE_IDLE);
    ESP_LOGI(TAG, "Initialized - STATE_IDLE");

    // Main control loop
    while (1) {
        // Check mode button
        if (is_button_pressed(BUTTON_MODE_PIN)) {
            ESP_LOGI(TAG, "Mode button pressed");
            handle_mode_button();
            ESP_LOGI(TAG, "Pending state: %d", pending_state);
            // Wait for button release
            while (gpio_get_level(BUTTON_MODE_PIN) == 0) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            vTaskDelay(BUTTON_DEBOUNCE_MS / portTICK_PERIOD_MS);
        }

        // Check confirm button
        if (is_button_pressed(BUTTON_CONFIRM_PIN)) {
            ESP_LOGI(TAG, "Confirm button pressed");
            handle_confirm_button();
            ESP_LOGI(TAG, "Current state: %d", current_state);
            // Wait for button release
            while (gpio_get_level(BUTTON_CONFIRM_PIN) == 0) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            vTaskDelay(BUTTON_DEBOUNCE_MS / portTICK_PERIOD_MS);
        }

        // Update LED flashing
        update_led_flash();

        // Update pulse timer
        update_pulse_timer();

        // Small delay to avoid hogging CPU
        vTaskDelay(BUTTON_CHECK_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

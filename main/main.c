#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "esp_timer.h"

static const char *TAG = "BIO_BUBBLER";

// ESP32-C3 Super Mini pin map: Status LED pins (RGB)
#define LED_RED GPIO_NUM_0      // LED Red (R1)
#define LED_GREEN GPIO_NUM_1    // LED Green (R2)
#define LED_BLUE GPIO_NUM_2     // LED Blue (R3)

// Pump control pins (via relays)
#define PUMP1_PIN GPIO_NUM_20   // Relay IN1
#define PUMP2_PIN GPIO_NUM_21   // Relay IN2

// Button pins
#define BUTTON_MODE_PIN GPIO_NUM_3      // BTN_RED (SW1)
#define BUTTON_CONFIRM_PIN GPIO_NUM_10  // BTN_GREEN (SW2)

// OLED SPI pins (0.96 inch display with pins: GND VCC D0 D1 RES DC CS)
#define OLED_PIN_CLK GPIO_NUM_5    // D0/CLK
#define OLED_PIN_MOSI GPIO_NUM_4   // D1/MOSI
#define OLED_PIN_RST GPIO_NUM_8    // RES
#define OLED_PIN_DC GPIO_NUM_7     // DC
#define OLED_PIN_CS GPIO_NUM_6     // CS

#define OLED_SPI_HOST SPI2_HOST
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define OLED_PAGES (OLED_HEIGHT / 8)
#define OLED_BUF_SIZE (OLED_WIDTH * OLED_PAGES)

// Timing constants (in milliseconds)
#define BUTTON_DEBOUNCE_MS 20
#define BUTTON_CHECK_INTERVAL_MS 10
#define LED_FLASH_INTERVAL_MS 500
#define DEFAULT_PULSE_DURATION_MS 30000

// WiFi Configuration
#define WIFI_SSID "BioBubbler"
#define WIFI_PASS "bubbler123"
#define WIFI_AP_IP "192.168.4.1"
#define WIFI_CHANNEL 1
#define MAX_STA_CONN 4

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

// Pulse timers (separate for each pump)
static uint32_t pump1_timer = 0;
static uint32_t pump2_timer = 0;

// Pump durations (configurable, in milliseconds)
static uint32_t pump1_duration_ms = DEFAULT_PULSE_DURATION_MS;
static uint32_t pump2_duration_ms = DEFAULT_PULSE_DURATION_MS;

// Emergency stop toggle for continuous mode
static int continuous_pumps_enabled = 1;

// HTTP server handle
static httpd_handle_t server = NULL;

// OLED handle and framebuffer
static spi_device_handle_t oled_spi = NULL;
static uint8_t oled_buffer[OLED_BUF_SIZE];
static int oled_show_wifi_info = 0;

// Brewing runtime tracker for CONTINUOUS mode (microseconds).
static uint64_t brewing_elapsed_us = 0;
static int64_t brewing_last_tick_us = 0;
static int brewing_running = 0;

static void brewing_reset(void)
{
    brewing_elapsed_us = 0;
    brewing_last_tick_us = 0;
    brewing_running = 0;
}

static void brewing_update(void)
{
    if (current_state != STATE_CONTINUOUS) {
        // Leaving Brewing mode fully resets the counter.
        brewing_reset();
        return;
    }

    if (!continuous_pumps_enabled) {
        // Pause: keep elapsed time, stop accumulating.
        brewing_running = 0;
        brewing_last_tick_us = 0;
        return;
    }

    int64_t now_us = esp_timer_get_time();
    if (!brewing_running) {
        brewing_running = 1;
        brewing_last_tick_us = now_us;
        return;
    }

    if (brewing_last_tick_us > 0 && now_us > brewing_last_tick_us) {
        brewing_elapsed_us += (uint64_t)(now_us - brewing_last_tick_us);
    }
    brewing_last_tick_us = now_us;
}

static esp_err_t oled_spi_write(const uint8_t *buf, size_t len, int dc_level)
{
    if (!oled_spi) {
        return ESP_ERR_INVALID_STATE;
    }

    gpio_set_level(OLED_PIN_DC, dc_level);

    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = buf,
    };

    return spi_device_transmit(oled_spi, &t);
}

static esp_err_t oled_send_cmd(uint8_t cmd)
{
    return oled_spi_write(&cmd, 1, 0);
}

static esp_err_t oled_send_data(const uint8_t *buf, size_t len)
{
    return oled_spi_write(buf, len, 1);
}

static esp_err_t oled_update(void)
{
    esp_err_t err;

    for (int page = 0; page < OLED_PAGES; page++) {
        err = oled_send_cmd(0xB0 | page);  // page address
        if (err != ESP_OK) return err;
        err = oled_send_cmd(0x00);         // lower column start
        if (err != ESP_OK) return err;
        err = oled_send_cmd(0x10);         // higher column start
        if (err != ESP_OK) return err;

        err = oled_send_data(&oled_buffer[page * OLED_WIDTH], OLED_WIDTH);
        if (err != ESP_OK) return err;
    }

    return ESP_OK;
}

static void oled_clear_buffer(void)
{
    memset(oled_buffer, 0x00, sizeof(oled_buffer));
}

static void oled_set_pixel(int x, int y, int on)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }

    size_t idx = (size_t)(y / 8) * OLED_WIDTH + (size_t)x;
    uint8_t mask = (uint8_t)(1U << (y & 7));

    if (on) {
        oled_buffer[idx] |= mask;
    } else {
        oled_buffer[idx] &= (uint8_t)~mask;
    }
}

static const uint8_t *oled_get_glyph(char c)
{
    static const uint8_t G_SPACE[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t G_DOT[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06};

    static const uint8_t G_A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t G_B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    static const uint8_t G_C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    static const uint8_t G_D[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const uint8_t G_E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const uint8_t G_G[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
    static const uint8_t G_H[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t G_I[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    static const uint8_t G_L[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    static const uint8_t G_M[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    static const uint8_t G_N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const uint8_t G_O[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t G_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t G_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const uint8_t G_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t G_T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t G_U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t G_W[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};

    static const uint8_t G_0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    static const uint8_t G_1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t G_2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    static const uint8_t G_3[7] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t G_4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    static const uint8_t G_5[7] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    static const uint8_t G_6[7] = {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    static const uint8_t G_7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const uint8_t G_8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    static const uint8_t G_9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};

    char up = (char)toupper((unsigned char)c);

    switch (up) {
        case 'A': return G_A;
        case 'B': return G_B;
        case 'C': return G_C;
        case 'D': return G_D;
        case 'E': return G_E;
        case 'G': return G_G;
        case 'H': return G_H;
        case 'I': return G_I;
        case 'L': return G_L;
        case 'M': return G_M;
        case 'N': return G_N;
        case 'O': return G_O;
        case 'P': return G_P;
        case 'R': return G_R;
        case 'S': return G_S;
        case 'T': return G_T;
        case 'U': return G_U;
        case 'W': return G_W;
        case '0': return G_0;
        case '1': return G_1;
        case '2': return G_2;
        case '3': return G_3;
        case '4': return G_4;
        case '5': return G_5;
        case '6': return G_6;
        case '7': return G_7;
        case '8': return G_8;
        case '9': return G_9;
        case '.': return G_DOT;
        case ' ': return G_SPACE;
        default:  return G_SPACE;
    }
}

static void oled_draw_char_scaled(int x, int y, char c, int scale)
{
    const uint8_t *glyph = oled_get_glyph(c);

    for (int row = 0; row < 7; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 5; col++) {
            if (bits & (1U << (4 - col))) {
                for (int dy = 0; dy < scale; dy++) {
                    for (int dx = 0; dx < scale; dx++) {
                        oled_set_pixel(x + (col * scale) + dx, y + (row * scale) + dy, 1);
                    }
                }
            }
        }
    }
}

static void oled_draw_text_scaled(int x, int y, const char *text, int scale)
{
    while (*text) {
        oled_draw_char_scaled(x, y, *text, scale);
        x += 6 * scale;
        text++;
    }
}

static int oled_text_width(const char *text, int scale)
{
    int len = 0;
    while (text[len] != '\0') {
        len++;
    }

    if (len <= 0) {
        return 0;
    }

    return (len * 5 * scale) + ((len - 1) * scale);
}

static void oled_draw_centered_text(int y, const char *text, int scale)
{
    int w = oled_text_width(text, scale);
    int x = (OLED_WIDTH - w) / 2;
    if (x < 0) x = 0;
    oled_draw_text_scaled(x, y, text, scale);
}

static const char *oled_state_label(machine_state_t state)
{
    switch (state) {
        case STATE_CONTINUOUS: return "Brewing";
        case STATE_PULSE: return ((pump1_timer > 0 || pump2_timer > 0) ? "Pouring" : "Tap");
        case STATE_IDLE:
        default:
            return "Idle";
    }
}

static const char *oled_pending_label(pending_state_t state)
{
    switch (state) {
        case PENDING_PULSE: return "Tap";
        case PENDING_CONTINUOUS: return "Brew";
        case PENDING_NONE:
        default:
            return "";
    }
}

static void oled_draw_status(int force)
{
    static machine_state_t last_state = (machine_state_t)-1;
    static pending_state_t last_pending = (pending_state_t)-1;
    static int last_flash_on = -1;
    static int last_show_wifi_info = -1;
    static uint32_t last_brew_minutes = 0xFFFFFFFFU;
    static int last_brewing_enabled = -1;
    static int last_pouring = -1;

    if (!oled_spi) {
        return;
    }

    uint32_t brew_minutes = (uint32_t)(brewing_elapsed_us / (60ULL * 1000000ULL));
    int pouring = (pump1_timer > 0 || pump2_timer > 0);
    int flash_on_phase = (flash_timer >= (LED_FLASH_INTERVAL_MS / 2));
    int show_wifi_page = (current_state == STATE_IDLE && pending_state == PENDING_NONE && oled_show_wifi_info);

    if (!force) {
        if (current_state == STATE_CONTINUOUS) {
            if (current_state == last_state &&
                brew_minutes == last_brew_minutes &&
                continuous_pumps_enabled == last_brewing_enabled) {
                return;
            }
        } else if (current_state == STATE_PULSE) {
            if (current_state == last_state && pouring == last_pouring) {
                return;
            }
        } else {
            // In IDLE, refresh when pending mode or flash phase changes.
            if (current_state == last_state &&
                pending_state == last_pending &&
                flash_on_phase == last_flash_on &&
                show_wifi_page == last_show_wifi_info) {
                return;
            }
        }
    }

    last_state = current_state;
    last_pending = pending_state;
    last_flash_on = flash_on_phase;
    last_show_wifi_info = show_wifi_page;
    last_brew_minutes = brew_minutes;
    last_brewing_enabled = continuous_pumps_enabled;
    last_pouring = pouring;

    oled_clear_buffer();

    if (show_wifi_page) {
        oled_draw_centered_text(0, "SSID", 1);
        oled_draw_centered_text(9, WIFI_SSID, 1);
        oled_draw_centered_text(22, "PASS", 1);
        oled_draw_centered_text(31, WIFI_PASS, 1);
        oled_draw_centered_text(44, "IP", 1);
        oled_draw_centered_text(53, WIFI_AP_IP, 1);

        if (oled_update() != ESP_OK) {
            ESP_LOGW(TAG, "OLED status update failed");
        }
        return;
    }

    const int scale = 2;
    const char *label = oled_state_label(current_state);

    // In IDLE, show pending selections as flashing OLED text synchronized with LED flash.
    if (current_state == STATE_IDLE && pending_state != PENDING_NONE) {
        if (flash_on_phase) {
            label = oled_pending_label(pending_state);
        } else {
            label = "";
        }
    }

    int text_w = oled_text_width(label, scale);
    int text_h = 7 * scale;
    int x = (OLED_WIDTH - text_w) / 2;
    int y = (current_state == STATE_CONTINUOUS) ? 8 : (OLED_HEIGHT - text_h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    if (label[0] != '\0') {
        oled_draw_text_scaled(x, y, label, scale);
    }

    if (current_state == STATE_CONTINUOUS) {
        uint32_t total_minutes = brew_minutes;
        uint32_t days = total_minutes / (24U * 60U);
        total_minutes %= (24U * 60U);
        uint32_t hours = total_minutes / 60U;
        uint32_t mins = total_minutes % 60U;

        char timer_text[24];
        if (days > 0) {
            snprintf(timer_text, sizeof(timer_text), "%02lud%02luh%02lum",
                     (unsigned long)days,
                     (unsigned long)hours,
                     (unsigned long)mins);
        } else {
            snprintf(timer_text, sizeof(timer_text), "%02luh%02lum",
                     (unsigned long)hours,
                     (unsigned long)mins);
        }

        const int timer_scale = 2;
        int timer_w = oled_text_width(timer_text, timer_scale);
        int timer_x = (OLED_WIDTH - timer_w) / 2;
        if (timer_x < 0) timer_x = 0;
        oled_draw_text_scaled(timer_x, 38, timer_text, timer_scale);
    }

    if (oled_update() != ESP_OK) {
        ESP_LOGW(TAG, "OLED status update failed");
    }
}

static esp_err_t oled_init(void)
{
    esp_err_t err;

    gpio_reset_pin(OLED_PIN_DC);
    gpio_set_direction(OLED_PIN_DC, GPIO_MODE_OUTPUT);

    gpio_reset_pin(OLED_PIN_RST);
    gpio_set_direction(OLED_PIN_RST, GPIO_MODE_OUTPUT);

    spi_bus_config_t buscfg = {
        .mosi_io_num = OLED_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = OLED_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = OLED_BUF_SIZE,
    };

    err = spi_bus_initialize(OLED_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OLED spi_bus_initialize failed: %s", esp_err_to_name(err));
        return err;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = OLED_PIN_CS,
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };

    err = spi_bus_add_device(OLED_SPI_HOST, &devcfg, &oled_spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OLED spi_bus_add_device failed: %s", esp_err_to_name(err));
        return err;
    }

    // Hardware reset pulse.
    gpio_set_level(OLED_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(OLED_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // SSD1306-compatible init sequence (works on most 0.96in SPI OLED modules).
    const uint8_t init_cmds[] = {
        0xAE,       // display off
        0xD5, 0x80, // clock divide
        0xA8, 0x3F, // multiplex ratio 1/64
        0xD3, 0x00, // display offset
        0x40,       // start line
        0x8D, 0x14, // charge pump on
        0x20, 0x02, // page addressing mode
        0xA1,       // segment remap
        0xC8,       // COM scan direction remap
        0xDA, 0x12, // COM pins config
        0x81, 0x7F, // contrast
        0xD9, 0xF1, // pre-charge
        0xDB, 0x40, // VCOMH deselect level
        0xA4,       // display resume RAM
        0xA6,       // normal (not inverted)
        0x2E,       // deactivate scroll
        0xAF,       // display on
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        err = oled_send_cmd(init_cmds[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OLED init command failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    return ESP_OK;
}

void set_machine_state(machine_state_t state)
{
    // Turn off all status LEDs
    gpio_set_level(LED_RED, 0);
    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_BLUE, 0);

    if (state != STATE_IDLE) {
        oled_show_wifi_info = 0;
    }

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
            pump1_timer = 0;
            pump2_timer = 0;
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
    // Mode/select should always return OLED to state/pending display mode.
    oled_show_wifi_info = 0;

    // From CONTINUOUS, mode acts as quick exit to IDLE.
    if (current_state == STATE_CONTINUOUS) {
        ESP_LOGI(TAG, "Mode: CONTINUOUS -> IDLE");
        set_machine_state(STATE_IDLE);
        return;
    }

    // From PULSE, mode prepares a direct switch path to CONTINUOUS.
    if (current_state == STATE_PULSE) {
        set_machine_state(STATE_IDLE);
        pending_state = PENDING_CONTINUOUS;
        flash_led = 1;
        flash_timer = 0;
        ESP_LOGI(TAG, "Mode: PULSE -> PENDING_CONTINUOUS (flashing blue)");
        return;
    }
    
    // Cycle through pending states (only when in IDLE)
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
        // In PULSE state, trigger pump cycle with configured durations
        ESP_LOGI(TAG, "PULSE: Starting pump cycle (P1: %lums, P2: %lums)", pump1_duration_ms, pump2_duration_ms);
        gpio_set_level(PUMP1_PIN, 1);
        gpio_set_level(PUMP2_PIN, 1);
        pump1_timer = pump1_duration_ms;
        pump2_timer = pump2_duration_ms;
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
    } else if (current_state == STATE_IDLE && pending_state == PENDING_NONE) {
        // In IDLE, confirm toggles Wi-Fi info page on OLED.
        oled_show_wifi_info = !oled_show_wifi_info;
        ESP_LOGI(TAG, "IDLE: OLED %s", oled_show_wifi_info ? "WiFi info" : "Idle");
    }
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
    // Handle pulse timer countdown for each pump separately
    if (current_state == STATE_PULSE) {
        // Pump 1 timer
        if (pump1_timer > 0) {
            if (pump1_timer <= BUTTON_CHECK_INTERVAL_MS) {
                pump1_timer = 0;
                gpio_set_level(PUMP1_PIN, 0);
                ESP_LOGI(TAG, "Pump 1 stopped");
            } else {
                pump1_timer -= BUTTON_CHECK_INTERVAL_MS;
            }
        }
        
        // Pump 2 timer
        if (pump2_timer > 0) {
            if (pump2_timer <= BUTTON_CHECK_INTERVAL_MS) {
                pump2_timer = 0;
                gpio_set_level(PUMP2_PIN, 0);
                ESP_LOGI(TAG, "Pump 2 stopped");
            } else {
                pump2_timer -= BUTTON_CHECK_INTERVAL_MS;
            }
        }
    }
}

// NVS Functions for pump duration storage
void load_pump_durations(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        nvs_get_u32(nvs_handle, "pump1_dur", &pump1_duration_ms);
        nvs_get_u32(nvs_handle, "pump2_dur", &pump2_duration_ms);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Loaded pump durations: P1=%lums, P2=%lums", pump1_duration_ms, pump2_duration_ms);
    } else {
        ESP_LOGI(TAG, "Using default pump durations: %dms", DEFAULT_PULSE_DURATION_MS);
    }
}

void save_pump_durations(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_u32(nvs_handle, "pump1_dur", pump1_duration_ms);
        nvs_set_u32(nvs_handle, "pump2_dur", pump2_duration_ms);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Saved pump durations: P1=%lums, P2=%lums", pump1_duration_ms, pump2_duration_ms);
    }
}

// HTTP Server handlers
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char* html = 
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>"
        "body{font-family:Arial;margin:20px;background:#f0f0f0}"
        ".container{max-width:600px;margin:auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
        "h1{color:#333;text-align:center}"
        ".pump{background:#e8f4f8;padding:15px;margin:15px 0;border-radius:8px}"
        ".pump h2{color:#0066cc;margin-top:0}"
        ".preset-btn{background:#0066cc;color:white;border:none;padding:12px 20px;margin:5px;border-radius:5px;cursor:pointer;font-size:16px}"
        ".preset-btn:hover{background:#0052a3}"
        ".preset-btn:active{background:#004080}"
        ".custom{margin-top:20px;padding:15px;background:#fff5e6;border-radius:8px}"
        "input[type=number]{width:100px;padding:8px;font-size:16px;border:1px solid #ccc;border-radius:4px}"
        ".save-btn{background:#28a745;color:white;border:none;padding:12px 30px;margin-top:10px;border-radius:5px;cursor:pointer;font-size:16px}"
        ".save-btn:hover{background:#218838}"
        ".current{font-size:14px;color:#666;margin-top:5px}"
        ".status{padding:10px;margin-top:15px;border-radius:5px;text-align:center;font-weight:bold}"
        ".success{background:#d4edda;color:#155724}"
        ".info{background:#d1ecf1;color:#0c5460}"
        "</style>"
        "</head><body>"
        "<div class='container'>"
        "<h1>🫧 Bio Bubbler Config</h1>"
        "<div class='status info' id='status'>Ready</div>"
        
        "<div class='pump'>"
        "<h2>Pump 1</h2>"
        "<div class='current'>Current: <span id='p1curr'>Loading...</span>ms</div>"
        "<div><button class='preset-btn' onclick='setPreset(1,9000)'>1.5L (9s)</button>"
        "<button class='preset-btn' onclick='setPreset(1,12000)'>2L (12s)</button>"
        "<button class='preset-btn' onclick='setPreset(1,30000)'>5L (30s)</button></div>"
        "</div>"
        
        "<div class='pump'>"
        "<h2>Pump 2</h2>"
        "<div class='current'>Current: <span id='p2curr'>Loading...</span>ms</div>"
        "<div><button class='preset-btn' onclick='setPreset(2,9000)'>1.5L (9s)</button>"
        "<button class='preset-btn' onclick='setPreset(2,12000)'>2L (12s)</button>"
        "<button class='preset-btn' onclick='setPreset(2,30000)'>5L (30s)</button></div>"
        "</div>"
        
        "<div class='custom'>"
        "<h3>Custom Duration (ms)</h3>"
        "Pump 1: <input type='number' id='custom1' value='30000'> ms<br><br>"
        "Pump 2: <input type='number' id='custom2' value='30000'> ms<br>"
        "<button class='save-btn' onclick='saveCustom()'>Save Custom</button>"
        "</div>"
        
        "</div>"
        
        "<script>"
        "function updateDisplay(){fetch('/status').then(r=>r.json()).then(d=>{"
        "document.getElementById('p1curr').innerText=d.pump1;"
        "document.getElementById('p2curr').innerText=d.pump2;"
        "document.getElementById('custom1').value=d.pump1;"
        "document.getElementById('custom2').value=d.pump2;})}"
        
        "function setPreset(pump,dur){"
        "fetch('/set?pump='+pump+'&dur='+dur).then(r=>r.text()).then(msg=>{"
        "document.getElementById('status').className='status success';"
        "document.getElementById('status').innerText=msg;updateDisplay();"
        "setTimeout(()=>{document.getElementById('status').className='status info';"
        "document.getElementById('status').innerText='Ready'},3000)})}"
        
        "function saveCustom(){"
        "const p1=document.getElementById('custom1').value;"
        "const p2=document.getElementById('custom2').value;"
        "fetch('/set?pump=1&dur='+p1).then(()=>fetch('/set?pump=2&dur='+p2))"
        ".then(()=>{document.getElementById('status').className='status success';"
        "document.getElementById('status').innerText='Custom durations saved!';"
        "updateDisplay();setTimeout(()=>{"
        "document.getElementById('status').className='status info';"
        "document.getElementById('status').innerText='Ready'},3000)})}"
        
        "updateDisplay();"
        "</script>"
        "</body></html>";
    
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char json[128];
    snprintf(json, sizeof(json), "{\"pump1\":%lu,\"pump2\":%lu}", pump1_duration_ms, pump2_duration_ms);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t set_get_handler(httpd_req_t *req)
{
    char buf[100];
    int pump = 0;
    uint32_t dur = 0;
    
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[32];
        if (httpd_query_key_value(buf, "pump", param, sizeof(param)) == ESP_OK) {
            pump = atoi(param);
        }
        if (httpd_query_key_value(buf, "dur", param, sizeof(param)) == ESP_OK) {
            dur = atoi(param);
        }
    }
    
    if (pump == 1 && dur > 0 && dur <= 300000) {
        pump1_duration_ms = dur;
        save_pump_durations();
        httpd_resp_send(req, "Pump 1 duration updated", HTTPD_RESP_USE_STRLEN);
    } else if (pump == 2 && dur > 0 && dur <= 300000) {
        pump2_duration_ms = dur;
        save_pump_durations();
        httpd_resp_send(req, "Pump 2 duration updated", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameters");
    }
    
    return ESP_OK;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station joined, AID=%d", event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station left, AID=%d", event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started. SSID:%s password:%s channel:%d",
             WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_get_handler,
        };
        httpd_register_uri_handler(server, &root);
        
        httpd_uri_t status = {
            .uri       = "/status",
            .method    = HTTP_GET,
            .handler   = status_get_handler,
        };
        httpd_register_uri_handler(server, &status);
        
        httpd_uri_t set = {
            .uri       = "/set",
            .method    = HTTP_GET,
            .handler   = set_get_handler,
        };
        httpd_register_uri_handler(server, &set);
        
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Load pump durations from NVS
    load_pump_durations();
    
    // Initialize GPIO
    init_gpio();

    // Initialize OLED and draw a simple test pattern.
    if (oled_init() == ESP_OK) {
        ESP_LOGI(TAG, "OLED initialized");
    } else {
        ESP_LOGW(TAG, "OLED init failed (continuing without display)");
    }

    // Initialize WiFi and Web Server
    wifi_init_softap();
    start_webserver();
    
    ESP_LOGI(TAG, "===============================================");
    ESP_LOGI(TAG, "  Bio Bubbler Control System Started");
    ESP_LOGI(TAG, "  WiFi AP: %s", WIFI_SSID);
    ESP_LOGI(TAG, "  Password: %s", WIFI_PASS);
    ESP_LOGI(TAG, "  Connect and navigate to: http://192.168.4.1");
    ESP_LOGI(TAG, "===============================================");

    // Start in idle state (green LED on, pumps off)
    set_machine_state(STATE_IDLE);
    ESP_LOGI(TAG, "Initialized - STATE_IDLE");
    oled_draw_status(1);

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

        // Track brewing runtime when in CONTINUOUS mode.
        brewing_update();

        // Refresh OLED state display when state/pending changes
        oled_draw_status(0);

        // Small delay to avoid hogging CPU
        vTaskDelay(BUTTON_CHECK_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

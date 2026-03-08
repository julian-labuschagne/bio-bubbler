#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_mac.h"

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
#define DEFAULT_PULSE_DURATION_MS 30000

// WiFi Configuration
#define WIFI_SSID "BioBubbler"
#define WIFI_PASS "bubbler123"
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
    // Handle pulse timer countdown for each pump separately
    if (current_state == STATE_PULSE) {
        // Pump 1 timer
        if (pump1_timer > 0) {
            pump1_timer -= BUTTON_CHECK_INTERVAL_MS;
            if (pump1_timer <= 0) {
                pump1_timer = 0;
                gpio_set_level(PUMP1_PIN, 0);
                ESP_LOGI(TAG, "Pump 1 stopped");
            }
        }
        
        // Pump 2 timer
        if (pump2_timer > 0) {
            pump2_timer -= BUTTON_CHECK_INTERVAL_MS;
            if (pump2_timer <= 0) {
                pump2_timer = 0;
                gpio_set_level(PUMP2_PIN, 0);
                ESP_LOGI(TAG, "Pump 2 stopped");
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

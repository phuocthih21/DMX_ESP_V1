/**
 * @file led_driver.c
 * @brief WS2812B LED driver using RMT peripheral
 * 
 * Uses RMT Channel 7 to avoid conflicts with DMX outputs (Channels 0-3)
 */

#include "status_types.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include <string.h>

static const char* TAG = "LED_DRV";

/* RMT configuration */
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000  // 10MHz = 0.1us per tick
#define RMT_LED_STRIP_GPIO_NUM      28        // Default GPIO

/* Module state */
static rmt_channel_handle_t s_led_chan = NULL;
static rmt_encoder_handle_t s_led_encoder = NULL;
static int s_gpio_pin = -1;

/**
 * @brief Initialize RMT channel for WS2812B LED
 * 
 * Configures RMT with 10MHz clock for precise WS2812B timing:
 * - Bit 0: 0.4us HIGH, 0.85us LOW
 * - Bit 1: 0.8us HIGH, 0.45us LOW
 * 
 * @param gpio_pin GPIO pin number
 * @return ESP_OK on success
 */
esp_err_t led_driver_init(int gpio_pin) {
    esp_err_t ret;
    
    s_gpio_pin = gpio_pin;
    
    ESP_LOGI(TAG, "Initializing LED driver on GPIO %d", gpio_pin);
    
    // Configure RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_pin,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    
    ret = rmt_new_tx_channel(&tx_chan_config, &s_led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT TX channel: %d", ret);
        return ret;
    }
    
    // Create LED strip encoder
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    
    ret = rmt_new_led_strip_encoder(&encoder_config, &s_led_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED encoder: %d", ret);
        return ret;
    }
    
    // Enable RMT channel
    ret = rmt_enable(s_led_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "LED driver initialized successfully");
    return ESP_OK;
}

/**
 * @brief Send RGB color to WS2812B LED
 * 
 * Automatically converts RGB to GRB byte order required by WS2812B.
 * Uses non-blocking RMT transmission.
 * 
 * @param color RGB color to display
 * @return ESP_OK on success
 */
esp_err_t led_driver_set_color(rgb_color_t color) {
    if (!s_led_chan || !s_led_encoder) {
        ESP_LOGW(TAG, "LED driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // WS2812B expects GRB byte order, not RGB
    uint8_t led_data[3];
    led_data[0] = color.g;  // Green first
    led_data[1] = color.r;  // Red second
    led_data[2] = color.b;  // Blue last
    
    // Configure transmission
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,  // No loop, single transmission
    };
    
    // Transmit data (non-blocking)
    esp_err_t ret = rmt_transmit(s_led_chan, s_led_encoder, led_data, sizeof(led_data), &tx_config);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "RMT transmit failed: %d", ret);
    }
    
    return ret;
}

/**
 * @brief Deinitialize LED driver
 * 
 * @return ESP_OK on success
 */
esp_err_t led_driver_deinit(void) {
    if (s_led_chan) {
        rmt_disable(s_led_chan);
        rmt_del_channel(s_led_chan);
        s_led_chan = NULL;
    }
    
    if (s_led_encoder) {
        rmt_del_encoder(s_led_encoder);
        s_led_encoder = NULL;
    }
    
    ESP_LOGI(TAG, "LED driver deinitialized");
    return ESP_OK;
}

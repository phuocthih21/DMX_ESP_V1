/**
 * @file dmx_rmt.c
 * @brief RMT Backend for DMX Output (Ports A/B)
 * @details Implements stable DMX512 signal generation using ESP32 RMT.
 * @author Embedded Firmware Architect
 * @date 2025
 */

#include "sys_mod.h"
#include "mod_dmx.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

#define TAG "DMX_RMT"

/* DMX Timing Constants (us) */
#define DMX_RMT_CLK_DIV      80    /* 80MHz / 80 = 1MHz (1us tick) */
#define DMX_BREAK_US         176   /* Break duration */
#define DMX_MAB_US           12    /* Mark After Break */
#define DMX_BIT_US           4     /* 1 bit at 250kbps */


/* RMT Buffer sizing */
#define DMX_RMT_ITEM_COUNT   3200

/* Static Buffers for RMT items */
static rmt_item32_t *s_rmt_bufs[2] = {NULL, NULL};

/* Helper to map port to channel */
static rmt_channel_t s_get_channel(int port_idx)
{
    if (port_idx == DMX_PORT_A) return RMT_CHANNEL_0;
    if (port_idx == DMX_PORT_B) return RMT_CHANNEL_1;
    return RMT_CHANNEL_MAX;
}

/**
 * @brief Convert byte array to DMX RMT items
 */
static void IRAM_ATTR s_fill_rmt_items(rmt_item32_t *items, const uint8_t *data, uint16_t len)
{
    int idx = 0;

    // BREAK (Low) + MAB (High)
    items[idx++] = (rmt_item32_t){{{DMX_BREAK_US, 0, DMX_MAB_US, 1}}};

    const uint8_t sc = 0x00;

    // Encode Start Code and Data bytes
    for (int i = -1; i < (int)len; i++) {
        uint8_t b = (i == -1) ? sc : data[i];

        // Start Bit (Low 4us) + D0
        items[idx].duration0 = DMX_BIT_US;
        items[idx].level0 = 0;
        items[idx].duration1 = DMX_BIT_US;
        items[idx].level1 = (b & 0x01) ? 1 : 0;
        idx++;

        // D1 + D2
        items[idx].duration0 = DMX_BIT_US;
        items[idx].level0 = (b & 0x02) ? 1 : 0;
        items[idx].duration1 = DMX_BIT_US;
        items[idx].level1 = (b & 0x04) ? 1 : 0;
        idx++;

        // D3 + D4
        items[idx].duration0 = DMX_BIT_US;
        items[idx].level0 = (b & 0x08) ? 1 : 0;
        items[idx].duration1 = DMX_BIT_US;
        items[idx].level1 = (b & 0x10) ? 1 : 0;
        idx++;

        // D5 + D6
        items[idx].duration0 = DMX_BIT_US;
        items[idx].level0 = (b & 0x20) ? 1 : 0;
        items[idx].duration1 = DMX_BIT_US;
        items[idx].level1 = (b & 0x40) ? 1 : 0;
        idx++;

        // D7 + Stop1 (High)
        items[idx].duration0 = DMX_BIT_US;
        items[idx].level0 = (b & 0x80) ? 1 : 0;
        items[idx].duration1 = DMX_BIT_US;
        items[idx].level1 = 1;
        idx++;

        // Stop2 (High)
        items[idx].duration0 = DMX_BIT_US;
        items[idx].level0 = 1;
        items[idx].duration1 = 0;
        items[idx].level1 = 0;
        idx++;
    }

    // End Marker
    items[idx] = (rmt_item32_t){{{0, 0, 0, 0}}};
}

esp_err_t dmx_rmt_init(int port_idx, int gpio_num)
{
    rmt_channel_t ch = s_get_channel(port_idx);
    if (ch == RMT_CHANNEL_MAX) return ESP_ERR_INVALID_ARG;

    int buf_idx = (port_idx == DMX_PORT_A) ? 0 : 1;
    if (s_rmt_bufs[buf_idx] == NULL) {
        s_rmt_bufs[buf_idx] = (rmt_item32_t *)heap_caps_malloc(DMX_RMT_ITEM_COUNT * sizeof(rmt_item32_t), MALLOC_CAP_DMA);
        if (!s_rmt_bufs[buf_idx]) {
            ESP_LOGE(TAG, "Failed to allocate RMT buffer");
            return ESP_ERR_NO_MEM;
        }
    }

    rmt_config_t config = {
        .rmt_mode = RMT_MODE_TX,
        .channel = ch,
        .gpio_num = gpio_num,
        .clk_div = DMX_RMT_CLK_DIV,
        .mem_block_num = 1,
        .tx_config = {
            .loop_en = false,
            .carrier_en = false,
            .idle_output_en = true,
            .idle_level = 1,
        }
    };

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(ch, 0, 0));

    ESP_LOGI(TAG, "RMT Port %d (GPIO %d) Init OK", port_idx, gpio_num);
    return ESP_OK;
}

esp_err_t dmx_rmt_send_frame(int port_idx, const uint8_t *data, uint16_t len)
{
    int buf_idx = (port_idx == DMX_PORT_A) ? 0 : 1;
    if (!s_rmt_bufs[buf_idx]) return ESP_ERR_INVALID_STATE;
    
    rmt_channel_t ch = s_get_channel(port_idx);
    
    s_fill_rmt_items(s_rmt_bufs[buf_idx], data, len);

    int count = 1 + (len + 1) * 6 + 1;
    
    return rmt_write_items(ch, s_rmt_bufs[buf_idx], count, false);
}

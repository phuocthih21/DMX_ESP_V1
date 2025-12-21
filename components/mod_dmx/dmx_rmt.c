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
#include <stdlib.h>

#define TAG "DMX_RMT"

/* DMX Timing Constants (us) */
#define DMX_RMT_CLK_DIV      80    /* 80MHz / 80 = 1MHz (1us tick) */
#define DMX_BREAK_US         176   /* Break duration */
#define DMX_MAB_US           12    /* Mark After Break */
#define DMX_BIT_US           4     /* 1 bit at 250kbps */


/* Module state (RMT TX API) */
static rmt_channel_handle_t s_rmt_chan[2] = {NULL, NULL};
static rmt_encoder_handle_t s_dmx_bytes_encoder[2] = {NULL, NULL};
static rmt_encoder_handle_t s_dmx_copy_encoder[2] = {NULL, NULL};
static rmt_encoder_handle_t s_dmx_encoder[2] = {NULL, NULL};
static int s_gpio_pins[2] = {-1, -1};

/* Helper to map port to index */
static int s_port_index(int port_idx)
{
    if (port_idx == DMX_PORT_A) return 0;
    if (port_idx == DMX_PORT_B) return 1;
    return -1;
}

/**
 * @brief Convert byte array to DMX RMT items
 */
/* Composite DMX encoder implementation (use RMT TX encoders) */

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_handle_t bytes_enc;
    rmt_encoder_handle_t copy_enc;
    const uint8_t *data;
    size_t data_len;
    size_t byte_idx;
    int substate; /* 0=send break, 1=start, 2=data, 3=stop1, 4=stop2 */
} rmt_dmx_encoder_t;

static size_t rmt_encode_dmx(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                              const void *primary_data, size_t data_size,
                              rmt_encode_state_t *ret_state)
{
    rmt_dmx_encoder_t *enc = __containerof(encoder, rmt_dmx_encoder_t, base);
    rmt_encoder_handle_t bytes_enc = enc->bytes_enc;
    rmt_encoder_handle_t copy_enc = enc->copy_enc;
    rmt_encode_state_t session = RMT_ENCODING_RESET;
    rmt_encode_state_t out_state = RMT_ENCODING_RESET;
    size_t total_symbols = 0;

    if (primary_data && data_size > 0 && enc->data == NULL) {
        enc->data = primary_data;
        enc->data_len = data_size;
        enc->byte_idx = 0;
        enc->substate = 0;
    }

    /* Local symbol placeholders */
    rmt_symbol_word_t break_sym = { .level0 = 0, .duration0 = DMX_BREAK_US, .level1 = 1, .duration1 = DMX_MAB_US };
    rmt_symbol_word_t start_sym = { .level0 = 0, .duration0 = DMX_BIT_US, .level1 = 0, .duration1 = 0 };
    rmt_symbol_word_t stop_sym  = { .level0 = 1, .duration0 = DMX_BIT_US, .level1 = 1, .duration1 = 0 };

    /* State machine to stream: break -> for each byte: start + data byte + stop1 + stop2 */
    while (1) {
        if (enc->substate == 0) {
            /* send break+MAB as a single symbol */
            size_t added = copy_enc->encode(copy_enc, channel, &break_sym, sizeof(break_sym), &session);
            total_symbols += added;
            if (session & RMT_ENCODING_MEM_FULL) { out_state |= RMT_ENCODING_MEM_FULL; break; }
            if (session & RMT_ENCODING_COMPLETE) { enc->substate = 1; continue; }
            break;
        }

        if (enc->byte_idx >= enc->data_len) {
            out_state |= RMT_ENCODING_COMPLETE;
            break; /* done */
        }

        /* start bit */
        if (enc->substate == 1) {
            size_t added = copy_enc->encode(copy_enc, channel, &start_sym, sizeof(start_sym), &session);
            total_symbols += added;
            if (session & RMT_ENCODING_MEM_FULL) { out_state |= RMT_ENCODING_MEM_FULL; break; }
            if (session & RMT_ENCODING_COMPLETE) { enc->substate = 2; continue; }
            break;
        }

        /* data byte via bytes encoder */
        if (enc->substate == 2) {
            const void *p = &enc->data[enc->byte_idx];
            size_t added = bytes_enc->encode(bytes_enc, channel, p, 1, &session);
            total_symbols += added;
            if (session & RMT_ENCODING_MEM_FULL) { out_state |= RMT_ENCODING_MEM_FULL; break; }
            if (session & RMT_ENCODING_COMPLETE) { enc->substate = 3; continue; }
            break;
        }

        /* stop bits */
        if (enc->substate == 3) {
            size_t added = copy_enc->encode(copy_enc, channel, &stop_sym, sizeof(stop_sym), &session);
            total_symbols += added;
            if (session & RMT_ENCODING_MEM_FULL) { out_state |= RMT_ENCODING_MEM_FULL; break; }
            if (session & RMT_ENCODING_COMPLETE) { enc->substate = 4; continue; }
            break;
        }

        if (enc->substate == 4) {
            /* second stop */
            size_t added = copy_enc->encode(copy_enc, channel, &stop_sym, sizeof(stop_sym), &session);
            total_symbols += added;
            if (session & RMT_ENCODING_MEM_FULL) { out_state |= RMT_ENCODING_MEM_FULL; break; }
            if (session & RMT_ENCODING_COMPLETE) {
                enc->substate = 1;
                enc->byte_idx++;
                continue;
            }
            break;
        }

        break;
    }

    *ret_state = out_state;
    return total_symbols;
}

static esp_err_t rmt_del_dmx_encoder(rmt_encoder_t *encoder)
{
    rmt_dmx_encoder_t *enc = __containerof(encoder, rmt_dmx_encoder_t, base);
    if (enc->bytes_enc) rmt_del_encoder(enc->bytes_enc);
    if (enc->copy_enc) rmt_del_encoder(enc->copy_enc);
    free(enc);
    return ESP_OK;
}

static esp_err_t rmt_dmx_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_dmx_encoder_t *enc = __containerof(encoder, rmt_dmx_encoder_t, base);
    enc->byte_idx = 0;
    enc->substate = 0;
    enc->data = NULL;
    enc->data_len = 0;
    return ESP_OK;
}

static esp_err_t rmt_new_dmx_encoder(rmt_encoder_handle_t bytes_enc, rmt_encoder_handle_t copy_enc, rmt_encoder_handle_t *ret)
{
    if (!bytes_enc || !copy_enc || !ret) return ESP_ERR_INVALID_ARG;
    rmt_dmx_encoder_t *enc = calloc(1, sizeof(*enc));
    if (!enc) return ESP_ERR_NO_MEM;
    enc->base.encode = rmt_encode_dmx;
    enc->base.del = rmt_del_dmx_encoder;
    enc->base.reset = rmt_dmx_encoder_reset;
    enc->bytes_enc = bytes_enc;
    enc->copy_enc = copy_enc;
    *ret = &enc->base;
    return ESP_OK;
}

esp_err_t dmx_rmt_init(int port_idx, int gpio_num)
{
    int idx = s_port_index(port_idx);
    if (idx < 0) return ESP_ERR_INVALID_ARG;

    s_gpio_pins[idx] = gpio_num;

    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio_num,
        .mem_block_symbols = 64,
        .resolution_hz = 1000000, /* 1MHz -> 1us ticks */
        .trans_queue_depth = 4,
    };

    esp_err_t ret = rmt_new_tx_channel(&tx_chan_config, &s_rmt_chan[idx]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT TX channel: %d", ret);
        return ret;
    }

    /* Create bytes encoder for DMX bits (LSB first). Each bit is two halves of 4us */
    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = { .level0 = 0, .duration0 = DMX_BIT_US, .level1 = 0, .duration1 = DMX_BIT_US },
        .bit1 = { .level0 = 0, .duration0 = DMX_BIT_US, .level1 = 1, .duration1 = DMX_BIT_US },
    };
    bytes_cfg.flags.msb_first = 0; /* DMX is LSB-first */

    ret = rmt_new_bytes_encoder(&bytes_cfg, &s_dmx_bytes_encoder[idx]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DMX bytes encoder: %d", ret);
        rmt_del_channel(s_rmt_chan[idx]);
        s_rmt_chan[idx] = NULL;
        return ret;
    }

    /* Create copy encoder for break/start/stop symbols */
    rmt_copy_encoder_config_t copy_cfg = {0};
    ret = rmt_new_copy_encoder(&copy_cfg, &s_dmx_copy_encoder[idx]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create copy encoder: %d", ret);
        rmt_del_encoder(s_dmx_bytes_encoder[idx]);
        rmt_del_channel(s_rmt_chan[idx]);
        s_dmx_bytes_encoder[idx] = NULL;
        s_rmt_chan[idx] = NULL;
        return ret;
    }

    /* Create composite DMX encoder */
    ret = rmt_new_dmx_encoder(s_dmx_bytes_encoder[idx], s_dmx_copy_encoder[idx], &s_dmx_encoder[idx]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DMX composite encoder: %d", ret);
        rmt_del_encoder(s_dmx_copy_encoder[idx]);
        rmt_del_encoder(s_dmx_bytes_encoder[idx]);
        rmt_del_channel(s_rmt_chan[idx]);
        s_dmx_copy_encoder[idx] = NULL;
        s_dmx_bytes_encoder[idx] = NULL;
        s_rmt_chan[idx] = NULL;
        return ret;
    }

    ret = rmt_enable(s_rmt_chan[idx]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel: %d", ret);
        rmt_del_encoder(s_dmx_encoder[idx]);
        s_dmx_encoder[idx] = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "RMT Port %d (GPIO %d) Init OK", port_idx, gpio_num);
    return ESP_OK;
}

esp_err_t dmx_rmt_deinit(int port_idx)
{
    int idx = s_port_index(port_idx);
    if (idx < 0) return ESP_ERR_INVALID_ARG;

    if (s_dmx_encoder[idx]) {
        /* deleting composite encoder will also delete child encoders */
        rmt_del_encoder(s_dmx_encoder[idx]);
        s_dmx_encoder[idx] = NULL;
        s_dmx_copy_encoder[idx] = NULL;
        s_dmx_bytes_encoder[idx] = NULL;
    } else {
        if (s_dmx_copy_encoder[idx]) {
            rmt_del_encoder(s_dmx_copy_encoder[idx]);
            s_dmx_copy_encoder[idx] = NULL;
        }
        if (s_dmx_bytes_encoder[idx]) {
            rmt_del_encoder(s_dmx_bytes_encoder[idx]);
            s_dmx_bytes_encoder[idx] = NULL;
        }
    }

    if (s_rmt_chan[idx]) {
        rmt_disable(s_rmt_chan[idx]);
        rmt_del_channel(s_rmt_chan[idx]);
        s_rmt_chan[idx] = NULL;
    }

    s_gpio_pins[idx] = -1;
    return ESP_OK;
}

esp_err_t dmx_rmt_send_frame(int port_idx, const uint8_t *data, uint16_t len)
{
    int idx = s_port_index(port_idx);
    if (idx < 0) return ESP_ERR_INVALID_ARG;
    if (!s_rmt_chan[idx] || !s_dmx_encoder[idx]) return ESP_ERR_INVALID_STATE;

    /* Build buffer with start code (0x00) first */
    static uint8_t buf[DMX_UNIVERSE_SIZE + 1];
    buf[0] = 0x00; /* start code */
    if (len > DMX_UNIVERSE_SIZE) len = DMX_UNIVERSE_SIZE;
    memcpy(&buf[1], data, len);

    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    /* Reset encoder state for fresh frame */
    if (s_dmx_encoder[idx] && s_dmx_encoder[idx]->reset) s_dmx_encoder[idx]->reset(s_dmx_encoder[idx]);

    /* Non-blocking transmit; the RMT TX API will schedule streaming of the frame */
    esp_err_t ret = rmt_transmit(s_rmt_chan[idx], s_dmx_encoder[idx], buf, len + 1, &tx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "RMT transmit failed: %d", ret);
    }
    return ret;
}

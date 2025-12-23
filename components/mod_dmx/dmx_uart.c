#include "mod_dmx.h"
#include "dmx_types.h"
#include "sys_mod.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_err.h"
#include <string.h>

// Use public SYS_MOD accessors instead of direct globals
// (sys_get_config/sys_get_last_activity/sys_get_dmx_buffer)
;

static const char *TAG = "DMX_UART";

typedef struct {
    uart_port_t uart_num;
    int tx_pin;
    int de_pin;
} dmx_uart_ctx_t;

static dmx_uart_ctx_t s_uart[2]; // ports C (idx 0) and D (idx 1)

esp_err_t dmx_uart_init_port(int port_idx, uart_port_t uart_num, int tx_pin, int de_pin)
{
    int idx = -1;
    if (port_idx == DMX_PORT_C) idx = 0;
    else if (port_idx == DMX_PORT_D) idx = 1;
    else return ESP_ERR_INVALID_ARG;

    s_uart[idx].uart_num = uart_num;
    s_uart[idx].tx_pin = tx_pin;
    s_uart[idx].de_pin = de_pin;

    uart_config_t uart_cfg = {
        .baud_rate = 250000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_pin, -1, -1, -1));
    /* RX buffer must be non-zero; DMX only uses TX but some UART drivers require RX buffer > 0 */
    ESP_ERROR_CHECK(uart_driver_install(uart_num, 2048, 1024, 0, NULL, 0));

    // Configure DE pin for RS485
    gpio_config_t de_cfg = {
        .pin_bit_mask = (1ULL << de_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&de_cfg);
    gpio_set_level(de_pin, 0); // default to receive (DE low)

    ESP_LOGI(TAG, "UART port %d init: tx=%d de=%d", port_idx, tx_pin, de_pin);
    return ESP_OK;
}

// IRAM-safe send function
void IRAM_ATTR dmx_uart_send_frame(int port_idx, const uint8_t* data)
{
    int idx = -1;
    if (port_idx == DMX_PORT_C) idx = 0;
    else if (port_idx == DMX_PORT_D) idx = 1;
    else return;

    uart_port_t uart = s_uart[idx].uart_num;
    int de_pin = s_uart[idx].de_pin;

    // Set DE high to enable driver (TX)
    gpio_set_level(de_pin, 1);

    // Make sure UART TX FIFO is idle
    uart_wait_tx_done(uart, pdMS_TO_TICKS(10));

    // Generate BREAK: force TX low. Use line inverse to drive TX low for break
    uart_set_line_inverse(uart, UART_SIGNAL_TXD_INV);
    // The timing should come from system config (read via accessor)
    const sys_config_t *cfg = sys_get_config();
    uint32_t break_us = cfg->ports[port_idx].timing.break_us;
    uint32_t mab_us = cfg->ports[port_idx].timing.mab_us;

    esp_rom_delay_us(break_us);

    // Release line (MAB)
    uart_set_line_inverse(uart, 0);
    esp_rom_delay_us(mab_us);

    // Build frame (Start code + 512)
    uint8_t frame[DMX_FRAME_SIZE];
    frame[0] = DMX_START_CODE;
    memcpy(&frame[1], data, DMX_UNIVERSE_SIZE);

    // Write bytes (quasi-blocking: copies into TX FIFO)
    uart_write_bytes(uart, (const char*)frame, 513);

    // Wait for TX to finish
    uart_wait_tx_done(uart, pdMS_TO_TICKS(50));

    // Disable transmitter
    gpio_set_level(de_pin, 0);
}

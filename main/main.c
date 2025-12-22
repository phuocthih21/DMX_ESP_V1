/*
 * @file status_test.c
 * @brief Simple test application to exercise the status LED module
 *
 * Enable by building with environment variable STATUS_TEST=1:
 *   STATUS_TEST=1 idf.py build
 */

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "status_led.h"

static const char *TAG = "STATUS_TEST";


void app_main(void)
{
    ESP_LOGI(TAG, "Starting status LED test");

    esp_err_t r = status_init(48); /* GPIO 27 */
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "status_init failed: %s", esp_err_to_name(r));
        return;
    }

    status_set_brightness(40);

    const status_code_t seq[] = {
        STATUS_BOOTING,
        STATUS_NET_ETH,
        STATUS_NET_WIFI,
        STATUS_NET_AP,
        STATUS_NO_NET,
        STATUS_DMX_WARN,
        STATUS_OTA,
        STATUS_ERROR,
        STATUS_IDENTIFY,
        STATUS_OFF
    };

    for (;;) {
        for (size_t i = 0; i < sizeof(seq)/sizeof(seq[0]); ++i) {
            status_code_t c = seq[i];
            if (c == STATUS_IDENTIFY) {
                ESP_LOGI(TAG, "Trigger IDENTIFY for 2s");
                status_trigger_identify(2000);
            } else {
                ESP_LOGI(TAG, "Set status %d", (int)c);
                status_set_code(c);
                /* Let status run for 3s (longer for OTA) */
                if (c == STATUS_OTA) vTaskDelay(pdMS_TO_TICKS(5000));
                else vTaskDelay(pdMS_TO_TICKS(3000));
            }
        }

        /* Ramp brightness test */
        for (int b = 10; b <= 100; b += 10) {
            status_set_brightness(b);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        for (int b = 100; b >= 10; b -= 10) {
            status_set_brightness(b);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
    }
}


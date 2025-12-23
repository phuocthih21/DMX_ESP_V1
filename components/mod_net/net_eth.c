#include "esp_log.h"
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "sys_mod.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "nvs.h"
#include <string.h>
#include <stdbool.h>
#include <time.h>

static const char* TAG = "NET_ETH";

// Default pin definitions (override in board-specific headers or Kconfig later)
#ifndef ETH_MISO_GPIO
#ifdef CONFIG_MODNET_ETH_MISO_GPIO
#define ETH_MISO_GPIO CONFIG_MODNET_ETH_MISO_GPIO
#else
#define ETH_MISO_GPIO 12
#endif
#endif

#ifndef ETH_MOSI_GPIO
#ifdef CONFIG_MODNET_ETH_MOSI_GPIO
#define ETH_MOSI_GPIO CONFIG_MODNET_ETH_MOSI_GPIO
#else
#define ETH_MOSI_GPIO 13
#endif
#endif

#ifndef ETH_SCLK_GPIO
#ifdef CONFIG_MODNET_ETH_SCLK_GPIO
#define ETH_SCLK_GPIO CONFIG_MODNET_ETH_SCLK_GPIO
#else
#define ETH_SCLK_GPIO 14
#endif
#endif

#ifndef ETH_CS_GPIO
#ifdef CONFIG_MODNET_ETH_CS_GPIO
#define ETH_CS_GPIO CONFIG_MODNET_ETH_CS_GPIO
#else
#define ETH_CS_GPIO 15
#endif
#endif

#ifndef ETH_INT_GPIO
#ifdef CONFIG_MODNET_ETH_INT_GPIO
#define ETH_INT_GPIO CONFIG_MODNET_ETH_INT_GPIO
#else
#define ETH_INT_GPIO 16
#endif
#endif

#ifndef ETH_RST_GPIO
#ifdef CONFIG_MODNET_ETH_RST_GPIO
#define ETH_RST_GPIO CONFIG_MODNET_ETH_RST_GPIO
#else
#define ETH_RST_GPIO 17
#endif
#endif

static esp_eth_handle_t s_eth_handle = NULL;
static esp_netif_t* s_eth_netif = NULL;

/*
 * Record a small failure report (JSON string) in NVS so that the
 * device can expose the last failure to the web UI or upload it on next
 * successful boot. This is intentionally compact and non-blocking.
 */
static void net_write_failure_report(esp_err_t err, const char* reason, int attempts)
{
    nvs_handle_t nvs;
    esp_err_t r = nvs_open("err_log", NVS_READWRITE, &nvs);
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "Cannot open err_log namespace: %s", esp_err_to_name(r));
        return;
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf), "{\"t\":%u,\"module\":\"net_eth\",\"code\":\"%s\",\"num\":%d,\"attempts\":%d,\"heap\":%u,\"reason\":\"%s\"}",
                       (unsigned)time(NULL), esp_err_to_name(err), (int)err, attempts, (unsigned)esp_get_free_heap_size(), reason ? reason : "");
    if (len < 0) len = 0; /* snprintf error guard */
    // Use a short key name to respect NVS key length limits
    const char *nvs_key = "net_fail"; // <= 15 chars
    r = nvs_set_blob(nvs, nvs_key, buf, (size_t)len + 1);
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write net failure to NVS (%s): %s", nvs_key, esp_err_to_name(r));
        /* Try a second time with a very short key as fallback */
        const char *fallback_key = "nf";
        esp_err_t r2 = nvs_set_blob(nvs, fallback_key, buf, (size_t)len + 1);
        if (r2 != ESP_OK) {
            ESP_LOGW(TAG, "Fallback write also failed (%s): %s", fallback_key, esp_err_to_name(r2));
        } else {
            nvs_commit(nvs);
            ESP_LOGI(TAG, "Recorded net failure (fallback key): %s", buf);
        }
    } else {
        nvs_commit(nvs);
        ESP_LOGI(TAG, "Recorded net failure: %s", buf);
    }
    nvs_close(nvs);
}

#if defined(ETH_EVENT) && defined(ETHERNET_EVENT_CONNECTED)
static void net_tmp_eth_connected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    (void)event_base; (void)event_id; (void)event_data;
    SemaphoreHandle_t sem = (SemaphoreHandle_t)arg;
    xSemaphoreGive(sem);
}
#endif

esp_err_t net_eth_start(void) {
#ifdef CONFIG_ETH_SPI_ETHERNET_W5500
    ESP_LOGI(TAG, "Starting W5500 Ethernet (SPI)");

    /* Defensive heap check: avoid starting the Ethernet stack when free heap is very low.
     * gpio_install_isr_service and other drivers allocate from the TLSF heap and can assert
     * if memory is fragmented or exhausted. We prefer to defer Ethernet initialization and
     * fall back to WiFi than risk crashing the device.
     */
    size_t _free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Free heap before W5500 init: %u bytes", (unsigned)_free_heap);
    const size_t _min_heap_for_eth = 32 * 1024; /* 32KB */
    if (_free_heap < _min_heap_for_eth) {
        ESP_LOGW(TAG, "Low heap (%u bytes), deferring W5500 init", (unsigned)_free_heap);
        net_write_failure_report(ESP_ERR_NO_MEM, "low_heap_before_init", (int)_free_heap);
        return ESP_ERR_NO_MEM;
    }

    // Initialize SPI bus for W5500
    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_MISO_GPIO,
        .mosi_io_num = ETH_MOSI_GPIO,
        .sclk_io_num = ETH_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    bool spi_initialized = false;
    if (ret == ESP_OK) {
        spi_initialized = true;
        ESP_LOGD(TAG, "SPI bus initialized successfully");
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGD(TAG, "SPI bus already initialized (IGNORING)");
    } else {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s (%d)", esp_err_to_name(ret), ret);
        net_write_failure_report(ret, "spi_init_failed", 0);
        sys_send_event(SYS_EVT_ERROR, NULL, 0);
        return ret;
    }

    // Device config
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 10 * 1000 * 1000, // 10MHz (lowered for reliability)
        .spics_io_num = ETH_CS_GPIO,
        .queue_size = 20,
    };

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_config.int_gpio_num = ETH_INT_GPIO;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    /* Ensure GPIO ISR service is installed (some MAC drivers call gpio_isr_handler_add) */
    esp_err_t gpio_isr_ret = gpio_install_isr_service(0);
    if (gpio_isr_ret == ESP_OK || gpio_isr_ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGD(TAG, "gpio_isr_service installed/ready: %s", esp_err_to_name(gpio_isr_ret));
    } else {
        ESP_LOGW(TAG, "gpio_install_isr_service returned %s (%d)", esp_err_to_name(gpio_isr_ret), gpio_isr_ret);
        if (gpio_isr_ret == ESP_ERR_NO_MEM) {
            net_write_failure_report(gpio_isr_ret, "gpio_isr_no_mem", 0);
            /* Do not proceed with risky operations; abort ethernet init so we can fallback to WiFi */
            return gpio_isr_ret;
        } else {
            net_write_failure_report(gpio_isr_ret, "gpio_isr_failed", 0);
        }
    }

    /* Hardware reset W5500 to avoid stale state on the bus */
    gpio_config_t rst_cfg = {
        .pin_bit_mask = (1ULL << ETH_RST_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&rst_cfg);
    /* Assert reset low, then high with longer delays to account for slow power-up */
    gpio_set_level(ETH_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(ETH_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(500)); // allow extra time for W5500 to power up and stabilize

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (!mac) {
        ESP_LOGE(TAG, "Failed to create W5500 MAC object");
        /* record detailed failure and continue without panicking */
        net_write_failure_report(ESP_FAIL, "mac_create_failed", 0);
        sys_send_event(SYS_EVT_ERROR, NULL, 0);
        return ESP_FAIL;
    }

    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!phy) {
        ESP_LOGE(TAG, "Failed to create W5500 PHY object");
        net_write_failure_report(ESP_FAIL, "phy_create_failed", 0);
        sys_send_event(SYS_EVT_ERROR, NULL, 0);
        return ESP_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);

    /* Try to install driver with limited retries and cleanup on failure to avoid runaway tasks */
    const int max_tries = 3;
    int attempt = 0;
    for (; attempt < max_tries; ++attempt) {
        size_t _heap_before_attempt = esp_get_free_heap_size();
        ret = esp_eth_driver_install(&eth_config, &s_eth_handle);
        if (ret == ESP_OK) break;
        ESP_LOGW(TAG, "esp_eth_driver_install attempt %d/%d failed: %s (%d), free_heap=%u", attempt + 1, max_tries, esp_err_to_name(ret), ret, (unsigned)_heap_before_attempt);
        /* If the failure is an invalid version, record a specific, human-friendly reason */
        if (ret == ESP_ERR_INVALID_VERSION) {
            net_write_failure_report(ret, "invalid_chip_version_or_no_response", (int)_heap_before_attempt);
        }
        /* ensure no partial state is left behind */
        if (s_eth_handle) {
            esp_err_t unret = esp_eth_driver_uninstall(s_eth_handle);
            if (unret != ESP_OK) {
                ESP_LOGW(TAG, "esp_eth_driver_uninstall cleanup returned %d", unret);
            }
            s_eth_handle = NULL;
        }
        /* small exponential backoff before retrying */
        vTaskDelay(pdMS_TO_TICKS(500 * (attempt + 1))); // increased backoff to avoid tight retry loops when hardware absent
    }
    if (ret != ESP_OK) {
        /* Final cleanup: avoid calling spi_bus_free here (can lead to assert when devices still hold CS)
         * Instead, record the failure details and leave the SPI bus initialized so system can continue.
         */
        size_t _heap_final = esp_get_free_heap_size();
        ESP_LOGE(TAG, "esp_eth_driver_install failed after %d attempts: %s (%d); free_heap=%u", max_tries, esp_err_to_name(ret), ret, (unsigned)_heap_final);
        net_write_failure_report(ret, "driver_install_failed", (int)_heap_final);
        sys_send_event(SYS_EVT_ERROR, NULL, 0);
        /* do not call spi_bus_free() here - it can assert if devices are still attached */
        return ret;
    }

    // Derive a unique MAC for the W5500 based on the ESP32 base MAC
    // and apply it to the Ethernet driver to avoid duplicates on the LAN.
    {
        uint8_t base_mac[6];
        if (esp_efuse_mac_get_default(base_mac) == ESP_OK) {
            uint8_t w5500_mac[6];
            memcpy(w5500_mac, base_mac, sizeof(w5500_mac));

            // Mark as locally administered and tweak last byte to avoid collision
            w5500_mac[0] |= 0x02; /* locally administered bit */
            w5500_mac[5] = (uint8_t)(w5500_mac[5] + 1);

            esp_err_t ioctl_ret = esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, w5500_mac);
            if (ioctl_ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to set W5500 MAC via ioctl: %d", ioctl_ret);
            } else {
                ESP_LOGI(TAG, "W5500 MAC set to %02x:%02x:%02x:%02x:%02x:%02x",
                         w5500_mac[0], w5500_mac[1], w5500_mac[2], w5500_mac[3], w5500_mac[4], w5500_mac[5]);
            }
        } else {
            ESP_LOGW(TAG, "Failed to read base MAC; skipping W5500 MAC set");
        }
    }



    // Create and attach netif
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);
    if (!s_eth_netif) {
        ESP_LOGE(TAG, "Failed to create ETH netif");
        return ESP_FAIL;
    }

    ret = esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_attach failed: %s (%d)", esp_err_to_name(ret), ret);
        net_write_failure_report(ret, "netif_attach_failed", 0);
        sys_send_event(SYS_EVT_ERROR, NULL, 0);
        return ret;
    }

    // Apply static IP configuration if DHCP disabled
    const sys_config_t* cfg = sys_get_config();
    if (cfg && !cfg->net.dhcp_enabled) {
        esp_netif_ip_info_t ip_info = {0};
        if (esp_netif_str_to_ip4(cfg->net.ip, &ip_info.ip) == ESP_OK &&
            esp_netif_str_to_ip4(cfg->net.netmask, &ip_info.netmask) == ESP_OK &&
            esp_netif_str_to_ip4(cfg->net.gateway, &ip_info.gw) == ESP_OK) {
            ESP_LOGI(TAG, "Applying static IP %s to Ethernet", cfg->net.ip);
            esp_netif_set_ip_info(s_eth_netif, &ip_info);
        } else {
            ESP_LOGW(TAG, "Invalid static IP config in sys_config, skipping");
        }
    }

    ret = esp_eth_start(s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_start failed: %s (%d)", esp_err_to_name(ret), ret);
        net_write_failure_report(ret, "eth_start_failed", 0);
        return ret;
    }

    /* Wait briefly for a link up event; if none, cleanly tear down the driver to avoid leaving
       W5500 driver tasks running and causing potential stack issues when the hardware is absent */
#if defined(ETH_EVENT) && defined(ETHERNET_EVENT_CONNECTED)
    {
        SemaphoreHandle_t link_sem = xSemaphoreCreateBinary();
        if (link_sem) {
            esp_err_t _er = esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_CONNECTED, &net_tmp_eth_connected, link_sem);
            if (_er == ESP_OK) {
                if (xSemaphoreTake(link_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
                    ESP_LOGW(TAG, "No ETH link detected after 5s; uninstalling W5500 driver to avoid faults");
                    net_write_failure_report(ESP_ERR_TIMEOUT, "no_link_after_start", 0);
                    esp_err_t stopret = esp_eth_stop(s_eth_handle);
                    if (stopret != ESP_OK) ESP_LOGW(TAG, "esp_eth_stop returned %d", stopret);
                    esp_err_t unret = esp_eth_driver_uninstall(s_eth_handle);
                    if (unret != ESP_OK) ESP_LOGW(TAG, "esp_eth_driver_uninstall returned %d", unret);
                    s_eth_handle = NULL;
                    if (s_eth_netif) { esp_netif_destroy(s_eth_netif); s_eth_netif = NULL; }
                    esp_event_handler_unregister(ETH_EVENT, ETHERNET_EVENT_CONNECTED, &net_tmp_eth_connected);
                    vSemaphoreDelete(link_sem);
                    return ESP_ERR_TIMEOUT;
                }
                esp_event_handler_unregister(ETH_EVENT, ETHERNET_EVENT_CONNECTED, &net_tmp_eth_connected);
            }
            vSemaphoreDelete(link_sem);
        }
    }
#endif

    ESP_LOGI(TAG, "W5500 Ethernet started");
    return ESP_OK;
#else
    ESP_LOGW(TAG, "W5500 driver not enabled in sdkconfig");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t net_eth_stop(void) {
    ESP_LOGI(TAG, "Stopping Ethernet");
    if (s_eth_handle) {
        esp_err_t ret = esp_eth_stop(s_eth_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_eth_stop failed: %d", ret);
        }
        ret = esp_eth_driver_uninstall(s_eth_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_eth_driver_uninstall failed: %d", ret);
        }
        s_eth_handle = NULL;
    }

    if (s_eth_netif) {
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
    }

    // Note: We don't free the SPI bus here; if needed, caller must manage it
    return ESP_OK;
}

#include "esp_log.h"
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"
#include "sys_mod.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include <string.h>
#include <stdbool.h>

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

esp_err_t net_eth_start(void) {
#ifdef CONFIG_ETH_SPI_ETHERNET_W5500
    ESP_LOGI(TAG, "Starting W5500 Ethernet (SPI)");

    // Initialize SPI bus for W5500
    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_MISO_GPIO,
        .mosi_io_num = ETH_MOSI_GPIO,
        .sclk_io_num = ETH_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %d", ret);
        return ret;
    }

    // Device config
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 20 * 1000 * 1000, // 20MHz
        .spics_io_num = ETH_CS_GPIO,
        .queue_size = 20,
    };

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_config.int_gpio_num = ETH_INT_GPIO;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    if (!mac) {
        ESP_LOGE(TAG, "Failed to create W5500 MAC object");
        return ESP_FAIL;
    }

    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
    if (!phy) {
        ESP_LOGE(TAG, "Failed to create W5500 PHY object");
        return ESP_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);

    ret = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_eth_driver_install failed: %d", ret);
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
        ESP_LOGE(TAG, "esp_netif_attach failed: %d", ret);
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
        ESP_LOGE(TAG, "esp_eth_start failed: %d", ret);
        return ret;
    }

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

#include "esp_log.h"
#include "esp_err.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sys_mod.h"

static const char* TAG = "NET_ETH";

// Pin Defines
#ifndef ETH_MISO_GPIO
#define ETH_MISO_GPIO 12
#endif
#ifndef ETH_MOSI_GPIO
#define ETH_MOSI_GPIO 13
#endif
#ifndef ETH_SCLK_GPIO
#define ETH_SCLK_GPIO 14
#endif
#ifndef ETH_CS_GPIO
#define ETH_CS_GPIO 15
#endif
#ifndef ETH_INT_GPIO
#define ETH_INT_GPIO 16
#endif
#ifndef ETH_RST_GPIO
#define ETH_RST_GPIO 17
#endif

static esp_eth_handle_t s_eth_handle = NULL;
static esp_netif_t* s_eth_netif = NULL;
static bool s_spi_initialized = false;

// Hàm dọn dẹp phần cứng triệt để
static void net_eth_cleanup_hardware(void) {
    ESP_LOGW(TAG, "Cleaning up Ethernet hardware...");

    // 1. Stop & Uninstall Driver
    if (s_eth_handle) {
        esp_eth_stop(s_eth_handle);
        esp_eth_driver_uninstall(s_eth_handle);
        s_eth_handle = NULL;
    }

    // 2. Disable Interrupt GPIO (Quan trọng: Ngăn chặn ngắt ma)
    gpio_reset_pin(ETH_INT_GPIO);
    gpio_set_direction(ETH_INT_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(ETH_INT_GPIO, GPIO_PULLUP_ONLY);

    // 3. Uninstall ISR Service nếu đã cài
    gpio_uninstall_isr_service();

    // 4. Free SPI Bus
    if (s_spi_initialized) {
        spi_bus_free(SPI2_HOST);
        s_spi_initialized = false;
    }
}

// Hàm khởi tạo nội bộ (thử 1 lần)
static esp_err_t net_eth_try_init(void) {
    esp_err_t ret;

    // Init SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_MISO_GPIO,
        .mosi_io_num = ETH_MOSI_GPIO,
        .sclk_io_num = ETH_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    
    // Chỉ init SPI nếu chưa init
    if (!s_spi_initialized) {
        ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (ret == ESP_OK) {
            s_spi_initialized = true;
        } else {
            ESP_LOGE(TAG, "SPI Init failed: %d", ret);
            return ret;
        }
    }

    // Config W5500
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 10 * 1000 * 1000, // 10MHz safe speed
        .spics_io_num = ETH_CS_GPIO,
        .queue_size = 20,
    };

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_config.int_gpio_num = ETH_INT_GPIO;
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Install GPIO ISR
    gpio_install_isr_service(0); // Ignore error if already installed

    // Hardware Reset W5500
    gpio_reset_pin(ETH_RST_GPIO);
    gpio_set_direction(ETH_RST_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(ETH_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(ETH_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(200)); // Wait for W5500 boot

    // Create MAC & PHY
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    if (!mac || !phy) {
        ESP_LOGE(TAG, "Failed to create MAC/PHY");
        return ESP_FAIL;
    }

    // Install Driver
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ret = esp_eth_driver_install(&config, &s_eth_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Driver install failed");
        if(mac) mac->del(mac); // Cleanup if install fails
        if(phy) phy->del(phy);
        return ret;
    }

    return ESP_OK;
}

esp_err_t net_eth_start(void) {
    ESP_LOGI(TAG, "Starting Ethernet (3 attempts)...");
    
    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "Ethernet Init Attempt %d/3", i + 1);
        if (net_eth_try_init() == ESP_OK) {
            // Attach to Netif
            if (!s_eth_netif) {
                esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
                s_eth_netif = esp_netif_new(&cfg);
            }
            esp_netif_attach(s_eth_netif, esp_eth_new_netif_glue(s_eth_handle));
            esp_eth_start(s_eth_handle);
            ESP_LOGI(TAG, "Ethernet Started Successfully");
            return ESP_OK;
        }
        
        // Failed attempt: Cleanup immediately
        net_eth_cleanup_hardware();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGE(TAG, "Ethernet failed after 3 attempts. DISABLED.");
    return ESP_FAIL;
}

esp_err_t net_eth_stop(void) {
    net_eth_cleanup_hardware();
    return ESP_OK;
}
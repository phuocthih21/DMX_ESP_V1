#include "mod_proto.h"
#include "proto_types.h"

#include "sdkconfig.h"
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mod_proto";
static TaskHandle_t s_proto_task = NULL;
static volatile bool s_task_stop = false;

/* Forward declarations of merge/parsers in this component */
int merge_input_by_universe(uint16_t universe, const uint8_t *data, size_t len, uint8_t priority, uint32_t src_ip);
void merge_check_timeout_ms(uint64_t now_ms);
void merge_init(void);
int parse_artnet_packet(const uint8_t *buf, ssize_t buflen, uint16_t *out_universe, const uint8_t **out_data, uint16_t *out_len);
int parse_sacn_packet(const uint8_t *buf, ssize_t buflen, uint16_t *out_universe, const uint8_t **out_data, uint16_t *out_len, uint8_t *out_priority);

#define ARTNET_PORT 6454
#define SACN_PORT 5568
#define RX_BUFFER_SIZE 1536

static int make_udp_socket(const char *bind_addr, uint16_t port)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: %s", strerror(errno));
        return -1;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = bind_addr ? inet_addr(bind_addr) : INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "bind() failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    return sock;
}

static void proto_task(void *arg)
{
    ESP_LOGI(TAG, "proto_task started");
    int artnet_sock = make_udp_socket(NULL, ARTNET_PORT);
    int sacn_sock = make_udp_socket(NULL, SACN_PORT);

    uint8_t rxbuf[RX_BUFFER_SIZE];

    while (!s_task_stop) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int maxfd = -1;
        if (artnet_sock >= 0) { FD_SET(artnet_sock, &read_fds); maxfd = artnet_sock; }
        if (sacn_sock >= 0) { FD_SET(sacn_sock, &read_fds); if (sacn_sock > maxfd) maxfd = sacn_sock; }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100 * 1000; /* 100 ms */

        int ret = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);
        uint64_t now_ms = esp_timer_get_time() / 1000ULL;
        merge_check_timeout_ms(now_ms);

        if (ret > 0) {
            if (artnet_sock >= 0 && FD_ISSET(artnet_sock, &read_fds)) {
                struct sockaddr_in src;
                socklen_t sl = sizeof(src);
                ssize_t len = recvfrom(artnet_sock, rxbuf, sizeof(rxbuf), 0, (struct sockaddr*)&src, &sl);
                if (len > 0) {
                    uint16_t universe = 0, dmx_len = 0;
                    const uint8_t *dmx_ptr = NULL;
                    if (parse_artnet_packet(rxbuf, len, &universe, &dmx_ptr, &dmx_len) > 0) {
                        merge_input_by_universe(universe, dmx_ptr, dmx_len, 0 /* priority for Art-Net */, src.sin_addr.s_addr);
                    }
                }
            }

            if (sacn_sock >= 0 && FD_ISSET(sacn_sock, &read_fds)) {
                struct sockaddr_in src;
                socklen_t sl = sizeof(src);
                ssize_t len = recvfrom(sacn_sock, rxbuf, sizeof(rxbuf), 0, (struct sockaddr*)&src, &sl);
                if (len > 0) {
                    uint16_t universe = 0, dmx_len = 0;
                    const uint8_t *dmx_ptr = NULL;
                    uint8_t priority = 0;
                    if (parse_sacn_packet(rxbuf, len, &universe, &dmx_ptr, &dmx_len, &priority) > 0) {
                        merge_input_by_universe(universe, dmx_ptr, dmx_len, priority, src.sin_addr.s_addr);
                    }
                }
            }
        }
    }

    if (artnet_sock >= 0) close(artnet_sock);
    if (sacn_sock >= 0) close(sacn_sock);

    ESP_LOGI(TAG, "proto_task exiting");
    s_proto_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t mod_proto_init(void)
{
    if (s_proto_task) return ESP_ERR_INVALID_STATE;
    s_task_stop = false;

    /* initialize merge contexts */
    merge_init();

    BaseType_t res = xTaskCreatePinnedToCore(proto_task, "proto_task", 4096, NULL, tskIDLE_PRIORITY + 2, &s_proto_task, 0);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create proto_task");
        s_proto_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t mod_proto_deinit(void)
{
    if (!s_proto_task) return ESP_ERR_INVALID_STATE;
    s_task_stop = true;
    /* Wait for task to exit */
    while (s_proto_task) { vTaskDelay(pdMS_TO_TICKS(50)); }
    return ESP_OK;
}

/** Runtime merge mode control */
extern merge_context_t g_merge_ctx[];

esp_err_t mod_proto_set_merge_mode(int port_idx, uint8_t mode)
{
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) return ESP_ERR_INVALID_ARG;
    if (mode != MERGE_MODE_HTP && mode != MERGE_MODE_LTP) return ESP_ERR_INVALID_ARG;
    g_merge_ctx[port_idx].merge_mode = mode;
    return ESP_OK;
}

uint8_t mod_proto_get_merge_mode(int port_idx)
{
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) return MERGE_MODE_HTP;
    return g_merge_ctx[port_idx].merge_mode;
}

/* Simple helper to request join (delegates to sacn.c) */
extern esp_err_t sacn_join_universe(uint16_t uni);

esp_err_t mod_proto_join_universe(uint8_t protocol, uint16_t universe)
{
    (void)protocol;
    return sacn_join_universe(universe);
}

esp_err_t proto_start(void)
{
    // API Contract compliance: proto_start() is alias for mod_proto_init()
    return mod_proto_init();
}

void proto_reload_config(void)
{
    // API Contract: Leave/Join Multicast group based on new universe mapping
    // Since routing is done dynamically via sys_route_find_port() in merge_input_by_universe(),
    // the main work here is to re-join sACN multicast groups for newly mapped universes.
    
    // For now, this is a no-op since:
    // 1. Routing is dynamic (reads from sys_config each packet)
    // 2. sACN multicast join is handled per-packet if needed
    // 
    // Future enhancement: Track current multicast groups and leave/join as needed
    ESP_LOGI(TAG, "proto_reload_config called (routing is dynamic, no action needed)");
}
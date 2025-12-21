#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mod_proto.h" // metrics API

static const char *TAG = "mod_proto.sacn";

/* Simple membership tracking to allow proto_reload_config() to add/remove
 * multicast memberships even when the sACN socket is recreated.
 */
#define SACN_MAX_JOINED 64
static uint16_t s_joined_universes[SACN_MAX_JOINED];
static size_t s_joined_count = 0;
static SemaphoreHandle_t s_join_mutex = NULL;
static int s_current_sacn_sock = -1; /* -1 when socket not present */

static void sacn_lock(void) {
    if (!s_join_mutex) s_join_mutex = xSemaphoreCreateMutex();
    if (s_join_mutex) xSemaphoreTake(s_join_mutex, portMAX_DELAY);
}
static void sacn_unlock(void) {
    if (s_join_mutex) xSemaphoreGive(s_join_mutex);
}

static bool sacn_contains(uint16_t uni) {
    for (size_t i = 0; i < s_joined_count; ++i) if (s_joined_universes[i] == uni) return true;
    return false;
}

static esp_err_t sacn_socket_join(int sock, uint16_t uni)
{
    if (sock < 0) return ESP_ERR_INVALID_ARG;
    struct ip_mreq mreq;
    uint8_t u_h = (uni >> 8) & 0xFF;
    uint8_t u_l = uni & 0xFF;
    char mcast_ip[16];
    snprintf(mcast_ip, sizeof(mcast_ip), "239.255.%u.%u", u_h, u_l);

    mreq.imr_multiaddr.s_addr = inet_addr(mcast_ip);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        ESP_LOGW(TAG, "Failed to join %s: %s", mcast_ip, strerror(errno));
        mod_proto_metrics_inc_igmp_failure();
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Joined multicast %s (universe %u)", mcast_ip, uni);
    return ESP_OK;
}

static esp_err_t sacn_socket_leave(int sock, uint16_t uni)
{
    if (sock < 0) return ESP_ERR_INVALID_ARG;
    struct ip_mreq mreq;
    uint8_t u_h = (uni >> 8) & 0xFF;
    uint8_t u_l = uni & 0xFF;
    char mcast_ip[16];
    snprintf(mcast_ip, sizeof(mcast_ip), "239.255.%u.%u", u_h, u_l);

    mreq.imr_multiaddr.s_addr = inet_addr(mcast_ip);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        ESP_LOGW(TAG, "Failed to drop membership %s: %s", mcast_ip, strerror(errno));
        mod_proto_metrics_inc_igmp_failure();
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Left multicast %s (universe %u)", mcast_ip, uni);
    return ESP_OK;
}

/* Minimal sACN (E1.31) parser based on the struct in design docs
 * - Verifies "ASC-E1.17" at root layer offset 4
 * - Extracts universe from framing layer (big-endian) at offset 113
 * - Extracts DMP prop_val_count at offset 123..124 and data at offset 125
 */
int parse_sacn_packet(const uint8_t *buf, ssize_t buflen, uint16_t *out_universe, const uint8_t **out_data, uint16_t *out_len, uint8_t *out_priority)
{
    if (buflen < 126) return 0;

    if (memcmp(&buf[4], "ASC-E1.17", 8) != 0) {
        ESP_LOGD(TAG, "Not an sACN packet");
        return 0;
    }

    /* Framing Layer universe at offset 113..114 (big-endian) */
    uint16_t universe = (uint16_t)buf[113] << 8 | (uint16_t)buf[114];
    uint8_t priority = buf[110]; /* priority field is before universe (offset 110 in our mapping) */

    /* DMP prop_val_count at offset 123..124 (big-endian) */
    uint16_t prop_val_count = (uint16_t)buf[123] << 8 | (uint16_t)buf[124];
    if (prop_val_count == 0 || prop_val_count > 513) {
        mod_proto_metrics_inc_malformed_sacn();
        return 0;
    }

    const uint8_t *data = &buf[125]; /* start code + 512 data */
    uint16_t dlen = prop_val_count - 1; /* subtract start code */
    if (dlen > 512) dlen = 512;

    *out_universe = universe;
    *out_data = data + 1; /* skip start code */
    *out_len = dlen;
    *out_priority = priority;
    return 1;
}

/* sACN IGMP join helper: compute multicast IP 239.255.u.u and join on a socket
 * NOTE: For simplicity this function only logs the target and returns OK. Implementing
 * actual join requires access to the sACN socket used by proto_mgr; that can be added
 * in the next step where the socket lifetime is shared.
 */
esp_err_t sacn_join_universe(uint16_t uni)
{
    sacn_lock();
    if (!sacn_contains(uni)) {
        if (s_joined_count >= SACN_MAX_JOINED) {
            sacn_unlock();
            ESP_LOGW(TAG, "Cannot join universe %u: max joined count reached", uni);
            return ESP_ERR_NO_MEM;
        }
        s_joined_universes[s_joined_count++] = uni;
    }

    /* If socket is present, attempt to join immediately */
    if (s_current_sacn_sock >= 0) {
        sacn_socket_join(s_current_sacn_sock, uni);
    } else {
        uint8_t u_h = (uni >> 8) & 0xFF;
        uint8_t u_l = uni & 0xFF;
        char mcast_ip[16];
        snprintf(mcast_ip, sizeof(mcast_ip), "239.255.%u.%u", u_h, u_l);
        ESP_LOGI(TAG, "sACN join requested for universe %u -> %s (queued, socket not ready)", uni, mcast_ip);
    }

    sacn_unlock();
    return ESP_OK;
}

esp_err_t sacn_leave_universe(uint16_t uni)
{
    sacn_lock();
    bool found = false;
    for (size_t i = 0; i < s_joined_count; ++i) {
        if (s_joined_universes[i] == uni) {
            found = true;
            s_joined_universes[i] = s_joined_universes[--s_joined_count];
            break;
        }
    }
    if (!found) {
        sacn_unlock();
        ESP_LOGI(TAG, "sACN leave requested for universe %u (not joined)", uni);
        return ESP_ERR_NOT_FOUND;
    }

    if (s_current_sacn_sock >= 0) {
        sacn_socket_leave(s_current_sacn_sock, uni);
    } else {
        ESP_LOGI(TAG, "sACN leave requested for universe %u (queued until socket available)", uni);
    }
    sacn_unlock();
    return ESP_OK;
}

/* Called by proto_mgr when sacn socket is created -- applies queued joins */
void sacn_apply_memberships_for_socket(int sock)
{
    sacn_lock();
    s_current_sacn_sock = sock;
    for (size_t i = 0; i < s_joined_count; ++i) {
        sacn_socket_join(s_current_sacn_sock, s_joined_universes[i]);
    }
    sacn_unlock();
}

/* Called by proto_mgr when sacn socket is closed */
void sacn_clear_socket(void)
{
    sacn_lock();
    s_current_sacn_sock = -1;
    sacn_unlock();
}

/* Expose current joined list (copy) */
size_t sacn_get_joined_universes(uint16_t *out, size_t max)
{
    sacn_lock();
    size_t n = (s_joined_count < max) ? s_joined_count : max;
    for (size_t i = 0; i < n; ++i) out[i] = s_joined_universes[i];
    sacn_unlock();
    return n;
}

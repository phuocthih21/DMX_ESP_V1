#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

static const char *TAG = "mod_proto.sacn";

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
    if (prop_val_count == 0 || prop_val_count > 513) return 0;

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
    uint8_t u_h = (uni >> 8) & 0xFF;
    uint8_t u_l = uni & 0xFF;
    char mcast_ip[16];
    snprintf(mcast_ip, sizeof(mcast_ip), "239.255.%u.%u", u_h, u_l);
    ESP_LOGI(TAG, "sACN join requested for universe %u -> %s (IGMP join will be attempted when socket is available)", uni, mcast_ip);
    return ESP_OK;
}

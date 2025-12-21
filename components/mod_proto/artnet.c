#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "mod_proto.h" // metrics API

static const char *TAG = "mod_proto.artnet";

/*
 * Minimal Art-Net parser.
 * - Verifies ID "Art-Net" at the beginning
 * - Reads OpCode as little-endian at offset 8
 * - Reads subUni/net and constructs a 16-bit universe as (net<<8)|subUni
 * - Reads length (big-endian) at offset 16
 */
int parse_artnet_packet(const uint8_t *buf, _ssize_t buflen, uint16_t *out_universe, const uint8_t **out_data, uint16_t *out_len)
{
    if (buflen < 18) return 0;
    /* ID is 8 bytes but often nul-terminated; check prefix */
    if (memcmp(buf, "Art-Net", 7) != 0) return 0;

    uint16_t op_code = (uint16_t)buf[8] | ((uint16_t)buf[9] << 8); /* little-endian */
    if (op_code != 0x5000) {
        ESP_LOGD(TAG, "Ignoring opcode 0x%04x", op_code);
        return 0;
    }

    uint8_t sub_uni = buf[14];
    uint8_t net = buf[15];
    uint16_t universe = ((uint16_t)net << 8) | sub_uni;

    uint16_t length = ((uint16_t)buf[16] << 8) | (uint16_t)buf[17]; /* big-endian */
    if (length > 512) length = 512;

    if (length == 0 || buflen < 18 + length) {
        /* packet claims to be ArtDMX but length is inconsistent */
        mod_proto_metrics_inc_malformed_artnet();
        return 0;
    }

    *out_universe = universe;
    *out_data = &buf[18];
    *out_len = length;
    return 1;
}

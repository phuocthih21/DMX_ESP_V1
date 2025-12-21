#ifndef _MOD_PROTO_H_
#define _MOD_PROTO_H_

#include <stdint.h>
#include "esp_err.h"

/* Timeout for streams (ms) per ANSI E1.31 */
#define PROTO_STREAM_TIMEOUT_MS 2500u

#define MERGE_MODE_HTP 0
#define MERGE_MODE_LTP 1

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mod_proto_init(void);
esp_err_t mod_proto_deinit(void);

/**
 * @brief Start protocol stack (API Contract alias)
 * 
 * This is an alias for mod_proto_init() for API Contract compliance.
 * 
 * @return ESP_OK on success
 */
esp_err_t proto_start(void);

/**
 * @brief Reload configuration (universe mappings)
 * 
 * According to API Contract: Leave/Join Multicast group based on new universe mapping.
 * Since routing is done dynamically via sys_route_find_port(), this function
 * mainly handles sACN multicast group management.
 */
void proto_reload_config(void);

/**
 * Runtime-only merge mode change. Does NOT persist to NVS.
 * mode must be MERGE_MODE_HTP or MERGE_MODE_LTP.
 */
esp_err_t mod_proto_set_merge_mode(int port_idx, uint8_t mode);
uint8_t    mod_proto_get_merge_mode(int port_idx);

/**
 * Convenience helper to request sACN IGMP join for a universe (protocol-specific behavior)
 */
esp_err_t mod_proto_join_universe(uint8_t protocol, uint16_t universe);

/* ===== Metrics API ===== */

typedef struct {
    uint32_t malformed_artnet_packets;
    uint32_t malformed_sacn_packets;
    uint32_t socket_errors;
    uint32_t igmp_failures;
} mod_proto_metrics_t;

/** Populate metrics (atomic-safe snapshot). */
void mod_proto_get_metrics(mod_proto_metrics_t *out);

/** Increment counters (used internally by parsers/sockets) */
void mod_proto_metrics_inc_malformed_artnet(void);
void mod_proto_metrics_inc_malformed_sacn(void);
void mod_proto_metrics_inc_socket_error(void);
void mod_proto_metrics_inc_igmp_failure(void);

#ifdef __cplusplus
}
#endif

#endif /* _MOD_PROTO_H_ */

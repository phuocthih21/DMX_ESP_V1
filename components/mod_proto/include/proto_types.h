#ifndef _PROTO_TYPES_H_
#define _PROTO_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include "sys_mod.h" /* for SYS_MAX_PORTS, DMX_UNIVERSE_SIZE */

typedef struct {
    bool active;
    uint64_t last_pkt_ts_ms; /* esp_timer_get_time() / 1000 */
    uint8_t priority;     /* sACN priority or 0 for Art-Net */
    uint8_t data[DMX_UNIVERSE_SIZE];
    uint32_t src_ip;
} proto_source_t;

typedef struct {
    uint16_t universe;
    uint8_t  merge_mode;    /* runtime only: MERGE_MODE_* */
    proto_source_t source_a;
    proto_source_t source_b;
    uint8_t final_data[DMX_UNIVERSE_SIZE];
} merge_context_t;

#endif /* _PROTO_TYPES_H_ */

#include "proto_types.h"
#include "mod_proto.h"

#include "sdkconfig.h"
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "sys_mod.h"

// TAG not used in this file - logging done in proto_mgr.c
// static const char *TAG = "mod_proto.merge";

merge_context_t g_merge_ctx[SYS_MAX_PORTS];

void merge_init(void)
{
    for (int i = 0; i < SYS_MAX_PORTS; ++i) {
        g_merge_ctx[i].universe = 0xFFFF;
        g_merge_ctx[i].merge_mode = MERGE_MODE_HTP;
        memset(g_merge_ctx[i].final_data, 0, sizeof(g_merge_ctx[i].final_data));
        g_merge_ctx[i].source_a.active = false;
        g_merge_ctx[i].source_b.active = false;
    }
}

static void write_output_if_changed(int port_idx, const uint8_t *data)
{
    uint8_t *out = sys_get_dmx_buffer(port_idx);
    if (!out) return;

    if (memcmp(out, data, DMX_UNIVERSE_SIZE) != 0) {
        memcpy(out, data, DMX_UNIVERSE_SIZE);
        sys_notify_activity(port_idx);
    }
}

/* Helper to decide which source wins for LTP */
static proto_source_t* newer_source(proto_source_t *a, proto_source_t *b)
{
    if (!a->active) return b->active ? b : NULL;
    if (!b->active) return a->active ? a : NULL;
    return (a->last_pkt_ts_ms >= b->last_pkt_ts_ms) ? a : b;
}

int merge_input_by_universe(uint16_t universe, const uint8_t *data, size_t len, uint8_t priority, uint32_t src_ip)
{
    (void)len; // assume 512 or less

    /* Look up configured port for sACN then Art-Net */
    int8_t port = sys_route_find_port(PROTOCOL_SACN, universe);
    if (port < 0) port = sys_route_find_port(PROTOCOL_ARTNET, universe);
    if (port < 0) return -1; // no mapping

    merge_context_t *ctx = &g_merge_ctx[port];

    /* Update source B if active else A (simple: track by IP) */
    proto_source_t *target = NULL;
    if (ctx->source_a.src_ip == src_ip || !ctx->source_a.active) target = &ctx->source_a;
    else if (ctx->source_b.src_ip == src_ip || !ctx->source_b.active) target = &ctx->source_b;
    else {
        /* Overwrite oldest source */
        uint64_t a_ts = ctx->source_a.active ? ctx->source_a.last_pkt_ts_ms : 0;
        uint64_t b_ts = ctx->source_b.active ? ctx->source_b.last_pkt_ts_ms : 0;
        target = (a_ts <= b_ts) ? &ctx->source_a : &ctx->source_b;
    }

    target->active = true;
    target->last_pkt_ts_ms = esp_timer_get_time() / 1000ULL;
    target->priority = priority;
    target->src_ip = src_ip;
    /* Copy up to DMX_UNIVERSE_SIZE bytes */
    memcpy(target->data, data, DMX_UNIVERSE_SIZE);

    /* Recompute final_data based on merge_mode and sACN priority rules
       Priority rule: if both sources are active and have different sACN priority
       values, the source with the higher priority wins the entire universe. If
       priorities are equal (or one source inactive), fall back to existing HTP/LTP rules. */
    if (ctx->source_a.active && ctx->source_b.active && ctx->source_a.priority != ctx->source_b.priority) {
        proto_source_t *higher = (ctx->source_a.priority > ctx->source_b.priority) ? &ctx->source_a : &ctx->source_b;
        memcpy(ctx->final_data, higher->data, DMX_UNIVERSE_SIZE);
    } else if (ctx->merge_mode == MERGE_MODE_HTP) {
        for (int i = 0; i < DMX_UNIVERSE_SIZE; ++i) {
            uint8_t a = ctx->source_a.active ? ctx->source_a.data[i] : 0;
            uint8_t b = ctx->source_b.active ? ctx->source_b.data[i] : 0;
            ctx->final_data[i] = (a >= b) ? a : b;
        }
    } else {
        /* LTP: choose newest source entirely */
        proto_source_t *newer = newer_source(&ctx->source_a, &ctx->source_b);
        if (newer) {
            memcpy(ctx->final_data, newer->data, DMX_UNIVERSE_SIZE);
        }
    }

    /* Write to sys output if changed */
    write_output_if_changed(port, ctx->final_data);
    return 0;
}

void merge_check_timeout_ms(uint64_t now_ms)
{
    for (int port = 0; port < SYS_MAX_PORTS; ++port) {
        merge_context_t *ctx = &g_merge_ctx[port];
        bool changed = false;
        if (ctx->source_a.active && (now_ms - ctx->source_a.last_pkt_ts_ms) > PROTO_STREAM_TIMEOUT_MS) {
            ctx->source_a.active = false;
            memset(ctx->source_a.data, 0, sizeof(ctx->source_a.data));
            changed = true;
            /* Source A timed out on this port */
        }
        if (ctx->source_b.active && (now_ms - ctx->source_b.last_pkt_ts_ms) > PROTO_STREAM_TIMEOUT_MS) {
            ctx->source_b.active = false;
            memset(ctx->source_b.data, 0, sizeof(ctx->source_b.data));
            changed = true;
            /* Source B timed out on this port */
        }
        if (changed) {
            /* Recompute final and write, honoring sACN priority as above */
            if (ctx->source_a.active && ctx->source_b.active && ctx->source_a.priority != ctx->source_b.priority) {
                proto_source_t *higher = (ctx->source_a.priority > ctx->source_b.priority) ? &ctx->source_a : &ctx->source_b;
                memcpy(ctx->final_data, higher->data, DMX_UNIVERSE_SIZE);
            } else if (ctx->merge_mode == MERGE_MODE_HTP) {
                for (int i = 0; i < DMX_UNIVERSE_SIZE; ++i) {
                    uint8_t a = ctx->source_a.active ? ctx->source_a.data[i] : 0;
                    uint8_t b = ctx->source_b.active ? ctx->source_b.data[i] : 0;
                    ctx->final_data[i] = (a >= b) ? a : b;
                }
            } else {
                proto_source_t *newer = newer_source(&ctx->source_a, &ctx->source_b);
                if (newer) memcpy(ctx->final_data, newer->data, DMX_UNIVERSE_SIZE);
                else memset(ctx->final_data, 0, DMX_UNIVERSE_SIZE);
            }

            int8_t port_idx = sys_route_find_port(PROTOCOL_SACN, ctx->universe);
            if (port_idx < 0) port_idx = sys_route_find_port(PROTOCOL_ARTNET, ctx->universe);
            if (port_idx >= 0) write_output_if_changed(port_idx, ctx->final_data);
        }
    }
}

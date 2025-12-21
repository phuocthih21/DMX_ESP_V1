#include "sys_mod.h"
#include <string.h>

// This file provides a small, safe stub implementation of the public API declared
// in components/sys_mod/sys_mod.h. It does NOT modify the existing SYS_MOD
// implementation; it is intended as a compile-ready placeholder and for unit tests.

static sys_system_status_t s_status;
static sys_dmx_port_status_t s_ports[4];

/* sys_mod_init/sys_mod_deinit are implemented in sys_setup.c; these stubs were removed to avoid duplicate symbols. */

sys_status_t sys_get_system_status(sys_system_status_t *out)
{
    if (!out) return SYS_ERR_INVALID;
    *out = s_status;
    return SYS_OK;
}

sys_status_t sys_get_dmx_status(sys_dmx_port_status_t *out, size_t max_port)
{
    if (!out || max_port == 0) return SYS_ERR_INVALID;
    size_t to_copy = (max_port < 4) ? max_port : 4;
    for (size_t i = 0; i < to_copy; ++i) out[i] = s_ports[i];
    return SYS_OK;
}

/* removed duplicate stub implementation - real implementation with events is located later in this file */

sys_status_t sys_apply_network_config(const network_cfg_t *cfg)
{
    if (!cfg) return SYS_ERR_INVALID;
    (void)cfg; // stub: accept but do not apply
    return SYS_OK;
}

sys_status_t sys_system_control(sys_cmd_type_t cmd)
{
    switch (cmd) {
        case SYS_CMD_REBOOT:
            // in real firmware, schedule reboot
            return SYS_OK;
        case SYS_CMD_FACTORY_RESET:
            // in real firmware, erase config
            return SYS_OK;
        default:
            return SYS_ERR_UNSUPPORTED;
    }
}

// Helper used by unit tests: advance uptime counter
void sys_mod_tick(uint32_t seconds)
{
    s_status.uptime += seconds;
}

// ===== Event registration (local lightweight registry) =====
#define MAX_EVENT_CBS 4
static sys_event_cb_t s_cbs[MAX_EVENT_CBS];
static void *s_cb_ctx[MAX_EVENT_CBS];

int sys_event_register_cb(sys_event_cb_t cb, void *user_ctx)
{
    if (!cb) return -1;
    for (int i = 0; i < MAX_EVENT_CBS; ++i) {
        if (s_cbs[i] == NULL) {
            s_cbs[i] = cb;
            s_cb_ctx[i] = user_ctx;
            return 0;
        }
    }
    return -1;
}

int sys_event_unregister_cb(sys_event_cb_t cb, void *user_ctx)
{
    for (int i = 0; i < MAX_EVENT_CBS; ++i) {
        if (s_cbs[i] == cb && s_cb_ctx[i] == user_ctx) {
            s_cbs[i] = NULL;
            s_cb_ctx[i] = NULL;
            return 0;
        }
    }
    return -1;
}

static void sys_event_emit(const sys_evt_msg_t *evt)
{
    if (!evt) return;
    for (int i = 0; i < MAX_EVENT_CBS; ++i) {
        if (s_cbs[i]) s_cbs[i](evt, s_cb_ctx[i]);
    }
}

// Example: emit a config-applied event after applying DMX config
// (This is used by tests or mock callers)
static void emit_config_applied(uint8_t port)
{
    sys_evt_msg_t evt;
    evt.type = SYS_EVT_CONFIG_APPLIED;
    evt.timestamp = s_status.uptime;
    evt.payload.config_applied.port = port;
    sys_event_emit(&evt);
}

// Update sys_apply_dmx_config to emit events
sys_status_t sys_apply_dmx_config(const sys_dmx_port_status_t *cfg, size_t count)
{
    if (!cfg || count == 0) return SYS_ERR_INVALID;
    for (size_t i = 0; i < count && i < 4; ++i) {
        if (cfg[i].universe > 63999) return SYS_ERR_INVALID;
        if (cfg[i].fps == 0 || cfg[i].fps > 1000) return SYS_ERR_INVALID;
    }
    // Apply and emit
    for (size_t i = 0; i < count && i < 4; ++i) {
        s_ports[i] = cfg[i];
        emit_config_applied((uint8_t)i);
    }
    return SYS_OK;
}

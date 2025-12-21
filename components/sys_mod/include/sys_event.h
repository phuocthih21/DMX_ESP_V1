#ifndef SYS_EVENT_H
#define SYS_EVENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    SYS_EVT_CONFIG_APPLIED,
    SYS_EVT_LINK_UP,
    SYS_EVT_LINK_DOWN,
    SYS_EVT_ERROR
} sys_event_t;

typedef struct {
    sys_event_t type;
    uint32_t timestamp;
    union {
        uint32_t error_code;
        struct { uint8_t port; } config_applied;
    } payload;
} sys_evt_msg_t;

// Event callback signature: returns void, called in SYS_MOD context
typedef void (*sys_event_cb_t)(const sys_evt_msg_t *evt, void *user_ctx);

// Register a callback to receive events. Multiple callbacks allowed.
// Returns 0 on success, non-zero on error (e.g., max callbacks reached).
int sys_event_register_cb(sys_event_cb_t cb, void *user_ctx);

// Unregister a previously registered callback.
int sys_event_unregister_cb(sys_event_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif

#endif // SYS_EVENT_H


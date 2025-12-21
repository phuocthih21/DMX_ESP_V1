#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_MODE_ETHERNET = 0,   // Wired (highest priority)
    NET_MODE_WIFI_STA,       // Wireless client
    NET_MODE_WIFI_AP,        // Rescue/config AP
    NET_MODE_NONE
} net_mode_t;

typedef struct {
    net_mode_t current_mode;
    bool eth_connected;
    bool wifi_connected;
    bool has_ip;
    char current_ip[16];     // "192.168.1.x"
} net_status_t;

#ifdef __cplusplus
}
#endif

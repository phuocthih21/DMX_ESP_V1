/**
 * @file sys_route.c
 * @brief Universe-to-port routing logic
 */

#include "sys_mod.h"

/* ========== ROUTING LOGIC ========== */

int8_t sys_route_find_port(uint8_t protocol, uint16_t universe) {
    const sys_config_t* cfg = sys_get_config();
    
    // Linear search through ports
    for (int i = 0; i < SYS_MAX_PORTS; i++) {
        const dmx_port_cfg_t* port = &cfg->ports[i];
        
        // Check if port matches criteria
        if (port->enabled && 
            port->protocol == protocol && 
            port->universe == universe) {
            return (int8_t)i;
        }
    }
    
    // No match found
    return -1;
}

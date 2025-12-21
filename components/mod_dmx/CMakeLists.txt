idf_component_register(SRCS "dmx_rmt_stub.c" "dmx_core.c" "dmx_rmt.c" "dmx_uart.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver sys_mod mod_status)

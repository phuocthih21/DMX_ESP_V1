idf_component_register(
    SRCS "proto_mgr.c" "artnet.c" "sacn.c" "merge.c" "mod_proto_metrics.c"
    INCLUDE_DIRS "include"
    REQUIRES sys_mod esp_timer lwip
)

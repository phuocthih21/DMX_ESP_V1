idf_component_register(SRCS "mod_net.c" "net_eth.c" "net_wifi.c" "net_mdns.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_netif esp_eth esp_wifi mdns sys_mod)

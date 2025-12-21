idf_component_register(
    SRCS "main.c"
         "startup/crash_monitor.c"
         "startup/button_check.c"
         "startup/startup.c"
    INCLUDE_DIRS "." "startup/include"
    REQUIRES sys_mod mod_status mod_dmx mod_net mod_proto mod_web nvs_flash driver esp_timer
)
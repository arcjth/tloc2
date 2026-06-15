idf_component_register(
    SRCS
        "main.c"
        "jloc2.c"
        "lib/esp.c"
        "lib/tcp_debug.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi nvs_flash lwip esp_netif 
        driver esp_timer
)

target_compile_definitions(${COMPONENT_TARGET} PRIVATE DEBUG_WIFI=1)

idf_component_register(
    SRCS
        "main.c"
        "jloc2.c"
        "lib/tcp_debug.c"
        "lib/esp.c"
        "lib/jlinear.c"
        "lib/jthio.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi nvs_flash lwip esp_netif 
        driver esp_timer esp_driver_i2s esp_driver_gpio
)

target_compile_definitions(${COMPONENT_TARGET} PRIVATE DEBUG_WIFI=1)

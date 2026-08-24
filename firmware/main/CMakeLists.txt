idf_component_register(
    SRCS
        "main.c"
        "loc.c"
        "server.c"
        "lib/i2s_esp.c"
        "lib/math.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi nvs_flash lwip esp_netif 
        driver esp_timer esp_driver_i2s esp_driver_gpio
)

target_compile_definitions(${COMPONENT_TARGET} PRIVATE DEBUG_WIFI=1)

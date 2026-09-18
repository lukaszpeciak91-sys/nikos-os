idf_component_register(
    SRCS "radio.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_event
        esp_netif
        esp_timer
        esp_wifi
        freertos
        log
)

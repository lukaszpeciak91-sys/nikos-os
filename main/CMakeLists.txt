idf_component_register(
    SRCS
        "app_main.cpp"
        "hardware_sanity.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_app_format
        esp_hw_support
        freertos
        log
        m5unified
)

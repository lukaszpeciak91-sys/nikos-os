idf_component_register(
    SRCS
        "auto_power_off_store.cpp"
        "rtc_date_configured_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        nvs_flash
)

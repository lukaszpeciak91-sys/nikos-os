idf_component_register(
    SRCS
        "auto_power_off_store.cpp"
        "communicator_preferences_store.cpp"
        "signal_volume_store.cpp"
        "rtc_date_configured_store.cpp"
        "rtc_time_basis_store.cpp"
        "entertainment_cycle_store.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        nvs_flash
)

idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        battery_guard
        board
        clock
        communicator
        countdown
        launcher
        log
        messaging
        nvs_flash
        power
        power_diag
        power_diag_app
        radio
        radiolab
        settings
        signal_sound
)

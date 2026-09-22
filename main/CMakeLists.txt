idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        board
        communicator
        launcher
        log
        messaging
        nvs_flash
        power
        radio
        radiolab
        settings
        signal_sound
)

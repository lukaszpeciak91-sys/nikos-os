idf_component_register(
    SRCS "launcher.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        board
        font_core
        clock
        countdown
        esp_timer
        freertos
        settings
        signal_sound
)

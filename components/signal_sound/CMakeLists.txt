idf_component_register(
    SRCS "signal_sound_player.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        board
        esp_timer
        settings
)

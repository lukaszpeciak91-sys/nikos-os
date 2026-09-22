idf_component_register(
    SRCS "launcher.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        board
        clock
        countdown
        freertos
        settings
        signal_sound
)

idf_component_register(
    SRCS
        "communicator_app.cpp"
        "communicator_catalogue.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        board
        esp_timer
        messaging
        signal_sound
)

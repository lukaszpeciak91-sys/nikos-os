idf_component_register(
    SRCS "radiolab_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        board
        esp_app_format
        esp_timer
        log
        protocol
        radio
)

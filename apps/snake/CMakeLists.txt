idf_component_register(
    SRCS "snake_app.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        board
        esp_hw_support
        esp_timer
        power
)

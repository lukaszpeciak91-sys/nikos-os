idf_component_register(
    SRCS "messaging_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        communicator_protocol
        esp_hw_support
        esp_timer
        log
        radio
)

idf_component_register(
    SRCS "keypad.c"
    INCLUDE_DIRS "."
    REQUIRES driver log esp_timer
)

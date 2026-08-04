idf_component_register(
    SRCS "led_bar.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_driver_spi esp_driver_rmt led_strip
)

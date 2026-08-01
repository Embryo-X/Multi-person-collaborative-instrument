# OLED 组件 CMakeLists.txt
idf_component_register(
    SRCS "oled.c" "cn_font.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos log esp_lcd
)

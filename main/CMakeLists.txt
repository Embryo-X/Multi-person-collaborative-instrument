# main 组件 CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES oled button keypad touch audio ble_app led_bar nvs_flash
)

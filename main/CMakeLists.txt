# main 组件 CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES oled button keypad touch audio ble_app nvs_flash
    # 后续启用 LED 时取消注释：
    # REQUIRES oled button touch audio led
    # 后续启用时取消注释：
    # REQUIRES oled button wifi_app audio ble_app
)

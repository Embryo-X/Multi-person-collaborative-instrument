# Wi-Fi 组件 CMakeLists.txt
idf_component_register(
    SRCS "wifi_app.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event nvs_flash log
)

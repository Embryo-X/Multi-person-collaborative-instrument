# BLE 组件 CMakeLists.txt
set(BT_DIR "${IDF_PATH}/components/bt")
idf_component_register(
    SRCS "ble_app.c"
    INCLUDE_DIRS "."
                "${BT_DIR}/include/esp32c3/include"
                "${BT_DIR}/host/bluedroid/api/include/api"
    REQUIRES bt log nvs_flash song_guide
)

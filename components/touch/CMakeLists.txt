# Touch 组件 CMakeLists.txt
idf_component_register(
    SRCS "touch.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos log esp_driver_touch_sens
)

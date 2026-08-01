# LED 组件 CMakeLists.txt
idf_component_register(
    SRCS "led.c"
    INCLUDE_DIRS "."
    REQUIRES driver log
)

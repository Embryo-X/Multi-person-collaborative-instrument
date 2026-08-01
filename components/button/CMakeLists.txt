# Button 组件 CMakeLists.txt
idf_component_register(
    SRCS "button.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos log
)

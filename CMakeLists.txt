# ==============================================================
# ESP-IDF 项目顶层 CMakeLists.txt
# 作用：告诉 ESP-IDF 构建系统项目的名称和组件搜索路径
# ==============================================================

# 设置项目名称（会作为固件文件名的一部分）
set(PROJECT_NAME "btn_oled_switch")

# 告知 ESP-IDF 构建系统：
#   - 项目入口在 main/ 目录
#   - 自定义组件在 components/ 目录
#   - 自动查找并包含所有子目录下的组件
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${PROJECT_NAME})

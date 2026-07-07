# ESP-IDF 项目顶层 CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# 包含 ESP-IDF 项目构建脚本
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目名称和目标芯片
project(s3_link_device)

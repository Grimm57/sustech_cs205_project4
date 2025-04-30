# 设置最低 CMake 版本和项目名称
cmake_minimum_required(VERSION 3.10)
project(my_imglib)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 添加 include 目录
include_directories(include)

# 设置源文件列表
set(SOURCES
    src/mat.cpp
    src/image_decoder.cpp
)

# 生成静态库
add_library(imglib_static STATIC ${SOURCES})
set_target_properties(imglib_static PROPERTIES OUTPUT_NAME "imglib")

# 生成动态库
add_library(imglib_shared SHARED ${SOURCES})
set_target_properties(imglib_shared PROPERTIES OUTPUT_NAME "imglib")

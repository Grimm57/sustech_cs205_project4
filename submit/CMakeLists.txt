# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 定义项目名称
project(ImageLibraryProject VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) 

# 设置最终构建产物的输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 添加 include 目录，以便源文件可以找到头文件
# ${PROJECT_SOURCE_DIR} 指向包含此 CMakeLists.txt 的目录 (即扁平化的提交目录)
include_directories(${PROJECT_SOURCE_DIR})

# --- 创建图像处理库 (imglib) ---

# 定义库的源文件列表 
set(IMGLIB_SOURCES
    image.cpp
    image_io.cpp
    processor.cpp
)

# 添加静态库目标 imglib
add_library(imglib STATIC ${IMGLIB_SOURCES})

# 添加共享库目标 imglib_shared
add_library(imglib_shared SHARED ${IMGLIB_SOURCES})
# 设置共享库输出文件名与静态库基础名称一致 (例如 libimglib.so 或 imglib.dll)
set_target_properties(imglib_shared PROPERTIES OUTPUT_NAME imglib)

# --- 创建演示程序 (img_demo) ---

# 添加可执行文件目标 img_demo (假设 demo.cpp 与 CMakeLists.txt 在同一目录)
add_executable(img_demo demo.cpp)

# 将 img_demo 链接到 imglib 库 (默认链接静态库)
# 如果要链接动态库，可以改为 target_link_libraries(img_demo PRIVATE imglib_shared)
target_link_libraries(img_demo PRIVATE imglib)

# --- 打印构建信息 (可选) ---
message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "Project Source Directory: ${PROJECT_SOURCE_DIR}")
message(STATUS "Build Directory: ${PROJECT_BINARY_DIR}")
message(STATUS "Using C++ Standard: ${CMAKE_CXX_STANDARD}")

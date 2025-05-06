# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 定义项目名称
project(ImageLibraryProject VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准 (例如 C++11, C++14, C++17)
# 考虑到使用了 unique_ptr, unordered_map, call_once 等，建议至少 C++11，C++17 更佳
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # 通常建议关闭编译器特定扩展

# 添加 include 目录，以便源文件可以找到头文件
# ${PROJECT_SOURCE_DIR} 指向包含此 CMakeLists.txt 的目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# --- 创建图像处理库 (imglib) ---

# 定义库的源文件列表
set(IMGLIB_SOURCES
    src/image.cpp
    src/image_io.cpp
    src/processor.cpp
)

# 添加静态库目标 imglib
add_library(imglib STATIC ${IMGLIB_SOURCES})

# (可选) 添加共享库目标 imglib_shared
# add_library(imglib_shared SHARED ${IMGLIB_SOURCES})
# set_target_properties(imglib_shared PROPERTIES OUTPUT_NAME imglib) # 设置输出文件名

# --- 创建演示程序 (img_demo) ---

# 添加可执行文件目标 img_demo
add_executable(img_demo demo/demo.cpp)

# 将 img_demo 链接到 imglib 库
target_link_libraries(img_demo PRIVATE imglib)

# (可选) 如果创建了共享库并想链接它
# target_link_libraries(img_demo PRIVATE imglib_shared)

# --- 安装规则 (可选) ---
# 如果你想让库和头文件可以被安装到系统中

# 安装头文件到 <prefix>/include/imglib
# install(DIRECTORY include/ DESTINATION include/imglib FILES_MATCHING PATTERN "*.h")

# 安装静态库到 <prefix>/lib
# install(TARGETS imglib ARCHIVE DESTINATION lib)

# (可选) 安装共享库到 <prefix>/lib 和运行时目录
# install(TARGETS imglib_shared LIBRARY DESTINATION lib RUNTIME DESTINATION bin)

# --- 打印构建信息 (可选) ---
message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "Project Source Directory: ${PROJECT_SOURCE_DIR}")
message(STATUS "Build Directory: ${PROJECT_BINARY_DIR}")
message(STATUS "Using C++ Standard: ${CMAKE_CXX_STANDARD}")

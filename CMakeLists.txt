cmake_minimum_required(VERSION 3.20)

# =========================================================
# 1. 自动判断编译器并设置 VCPKG 架构
# =========================================================
if(MINGW)
    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "" FORCE)
    message(STATUS ">> Detected MinGW Compiler. Target: x64-mingw-dynamic")
else()
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "" FORCE)
    message(STATUS ">> Detected MSVC Compiler. Target: x64-windows")
endif()

set(CMAKE_TOOLCHAIN_FILE "F:/Software/IncludePackage/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(VisualMetric)

if(MSVC)
    add_compile_options("/utf-8")
    add_compile_options("/wd4267")
    add_compile_options("/wd4244")
endif()

set(CMAKE_CXX_STANDARD 17)

# ---------------------------------------------------------
# 2. 查找包
# ---------------------------------------------------------
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ---------------------------------------------------------
# 3. 包含目录
# ---------------------------------------------------------
include_directories(src)
include_directories(third_party/stb)

# ---------------------------------------------------------
# 4. 源文件管理
# ---------------------------------------------------------
# 自动查找 src 下所有 .cpp 和 .h (包括子目录)
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

# [修正] 这里只放 src 目录之外的第三方文件
set(STB_SOURCES
        "third_party/stb/stb_image.h"
        "third_party/stb/stb_image_write.h"
)

# ---------------------------------------------------------
# 5. 生成可执行文件
# ---------------------------------------------------------
add_executable(VisualMetrics
        ${SOURCES}      # 包含了 src 下的所有文件
        ${STB_SOURCES}  # 包含了 stb 头文件
)

# ---------------------------------------------------------
# 6. 链接库
# ---------------------------------------------------------
target_link_libraries(VisualMetrics PRIVATE
        glfw
        glad::glad
        glm::glm
        assimp::assimp
        nlohmann_json::nlohmann_json
)

# 全局启用 GLM 实验性扩展
target_compile_definitions(VisualMetrics PRIVATE GLM_ENABLE_EXPERIMENTAL)

# ---------------------------------------------------------
# 7. 配置预编译头
# ---------------------------------------------------------
target_precompile_headers(VisualMetrics PRIVATE src/pch.h)

# 编译选项
if(MSVC)
    target_compile_options(VisualMetrics PRIVATE /W4)
else()
    target_compile_options(VisualMetrics PRIVATE -Wall -Wextra)
endif()
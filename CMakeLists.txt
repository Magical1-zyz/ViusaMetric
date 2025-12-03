cmake_minimum_required(VERSION 3.20)

# =========================================================
# 1. 自动判断编译器并设置 VCPKG 架构
# =========================================================
if(MINGW)
    # 检测到 MinGW (CLion 默认编译器)
    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "" FORCE)
    message(STATUS ">> Detected MinGW Compiler. Target: x64-mingw-dynamic")
else()
    # 检测到 MSVC (Visual Studio)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "" FORCE)
    message(STATUS ">> Detected MSVC Compiler. Target: x64-windows")
endif()

# 设置 vcpkg 工具链路径 (保持你原有的路径)
set(CMAKE_TOOLCHAIN_FILE "F:/Software/IncludePackage/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(VisualMetric)

# --- 强制 MSVC 使用 UTF-8 编码，解决乱码警告 ---
if(MSVC)
    add_compile_options("/utf-8")
    add_compile_options("/W3")
    add_compile_options("/wd4267") # 屏蔽 size_t 转 int 警告
    add_compile_options("/wd4244") # 屏蔽 double 转 float 警告
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
# 3. 包含目录 (Include Directories)
# ---------------------------------------------------------
# 你的源代码目录
include_directories(src)

# [修改点1] 添加 STB 库的头文件搜索路径
# 这样你在代码里就可以直接写 #include <stb_image.h> 了
include_directories(third_party/stb)

# ---------------------------------------------------------
# 4. 源文件管理
# ---------------------------------------------------------
# 自动查找 src 下所有 .cpp 和 .h
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

# [修改点2] 手动添加 STB 头文件，以便在 CLion 项目视图中显示它们
# 假设你同时下载了 stb_image.h 和 stb_image_write.h
set(STB_SOURCES
        "third_party/stb/stb_image.h"
        "third_party/stb/stb_image_write.h"
        third_party/stb/stb_image_write.h
)

# ---------------------------------------------------------
# 5. 生成可执行文件
# ---------------------------------------------------------
# [修改点3] 这里不需要再手动列出 src 下的文件了，${SOURCES} 已经包含了
add_executable(VisualMetrics
        ${SOURCES}
        ${STB_SOURCES}
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

# 启用各类编译警告
if(MSVC)
    target_compile_options(VisualMetrics PRIVATE /W4)
else()
    target_compile_options(VisualMetrics PRIVATE -Wall -Wextra)
endif()

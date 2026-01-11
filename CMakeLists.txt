cmake_minimum_required(VERSION 3.10)
project(ShutdownAssistant C)

set(CMAKE_C_STANDARD 99)

if(MSVC)
    # 核心修复：同时包含 /utf-8 (防乱码) 和 UNICODE 定义 (防类型错误)
    add_compile_options("/utf-8")
    add_compile_definitions(UNICODE _UNICODE)
endif()

# 包含目录
include_directories(include)

# 源文件列表
set(SOURCES
    src/main.c
    src/globals.c
    src/config.c
    src/gui.c
    src/system_ops.c
)

# 资源文件
set(RESOURCES resources/resource.rc)

# 生成可执行文件
add_executable(ShutdownTray WIN32 ${SOURCES} ${RESOURCES})

# 链接库
target_link_libraries(ShutdownTray User32 Kernel32 Advapi32 Shell32 Gdi32)

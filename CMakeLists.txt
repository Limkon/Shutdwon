cmake_minimum_required(VERSION 3.10)
project(ShutdownAssistant C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUNICODE -D_UNICODE")

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

# 资源文件 (如果有 .rc 文件，可以在此添加)
# set(RESOURCES resources/resource.rc)

# 生成可执行文件 (Windows 窗口应用，不带控制台)
add_executable(ShutdownTray WIN32 ${SOURCES} ${RESOURCES})

# 链接库
target_link_libraries(ShutdownTray User32 Kernel32 Advapi32 Shell32 Gdi32)

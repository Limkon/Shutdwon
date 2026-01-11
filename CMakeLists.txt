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

# --- 修改开始: 启用资源文件 ---
# 设置资源文件路径
set(RESOURCES resources/resource.rc)

# 生成可执行文件 (将 ${RESOURCES} 添加到列表中)
add_executable(ShutdownTray WIN32 ${SOURCES} ${RESOURCES})
# --- 修改结束 ---

# 链接库
target_link_libraries(ShutdownTray User32 Kernel32 Advapi32 Shell32 Gdi32)

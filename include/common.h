#ifndef COMMON_H
#define COMMON_H

#define _WIN32_WINNT 0x0501 // For Windows XP compatibility
// 移除 UNICODE 和 _UNICODE 定义，因为 CMake 已经在编译器参数中定义了它们
// 避免 warning C4005: macro redefinition

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <time.h>
#include <io.h>

#include "resource.h"

// --- Constants ---
extern const WCHAR *MAIN_WINDOW_CLASS;
extern const WCHAR *HIDDEN_WINDOW_CLASS;
extern const WCHAR *MUTEX_NAME;
extern const WCHAR *CONFIG_FILE_BASE_NAME;
extern const WCHAR *CONFIG_SECTION_NAME;

// --- Types ---
typedef struct {
    BOOL enable_autorun;
    BOOL enable_timed_shutdown;
    int shutdown_hour;
    int shutdown_minute;
    BOOL enable_idle_shutdown;
    int idle_minutes;
    int countdown_seconds;
    BOOL hide_main_window;
} AppConfig;

// --- Global Variables (Extern) ---
extern WCHAR g_config_file_path[MAX_PATH];
extern HWND g_hMainWindow;
extern HWND g_hHiddenWindow;
extern HANDLE g_hMutex;
extern BOOL g_shutdown_executed_today;
extern BOOL g_is_shutdown_pending;
extern WORD g_last_handled_day_for_timed_shutdown;
extern AppConfig g_config;

#endif // COMMON_H

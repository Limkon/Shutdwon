#include "../include/common.h"

// --- Constants Definition ---
const WCHAR *MAIN_WINDOW_CLASS = L"ShutdownAssistantMainWindowClass";
const WCHAR *HIDDEN_WINDOW_CLASS = L"ShutdownAssistantHiddenWindowClass";
const WCHAR *MUTEX_NAME = L"Global\\ShutdownAssistantMutex";
const WCHAR *CONFIG_FILE_BASE_NAME = L"config.ini";
const WCHAR *CONFIG_SECTION_NAME = L"Settings";

// --- Global Variables Definition ---
WCHAR g_config_file_path[MAX_PATH];

HWND g_hMainWindow = NULL;
HWND g_hHiddenWindow = NULL;
HANDLE g_hMutex = NULL;
BOOL g_shutdown_executed_today = FALSE;
BOOL g_is_shutdown_pending = FALSE;
WORD g_last_handled_day_for_timed_shutdown = 0;

AppConfig g_config;

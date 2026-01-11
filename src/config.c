#include "../include/config.h"

// --- Helper Functions ---
BOOL GetPrivateProfileBoolW(LPCWSTR lpAppName, LPCWSTR lpKeyName, BOOL fDefault, LPCWSTR lpFileName) {
    WCHAR szRet[8];
    GetPrivateProfileStringW(lpAppName, lpKeyName, fDefault ? L"true" : L"false", szRet, ARRAYSIZE(szRet), lpFileName);
    return (wcscmp(szRet, L"true") == 0);
}

BOOL WritePrivateProfileBoolW(LPCWSTR lpAppName, LPCWSTR lpKeyName, BOOL fValue, LPCWSTR lpFileName) {
    return WritePrivateProfileStringW(lpAppName, lpKeyName, fValue ? L"true" : L"false", lpFileName);
}

BOOL WritePrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName, int iValue, LPCWSTR lpFileName) {
    WCHAR szValue[16];
    swprintf_s(szValue, ARRAYSIZE(szValue), L"%d", iValue);
    return WritePrivateProfileStringW(lpAppName, lpKeyName, szValue, lpFileName);
}

// --- Configuration Functions ---
BOOL LoadConfig(const WCHAR* configPath) {
    g_config.enable_autorun = FALSE;
    g_config.enable_timed_shutdown = FALSE;
    g_config.shutdown_hour = 0;
    g_config.shutdown_minute = 0;
    g_config.enable_idle_shutdown = FALSE;
    g_config.idle_minutes = 0;
    g_config.countdown_seconds = 30;
    g_config.hide_main_window = FALSE;

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(configPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        FILE* f_create = _wfopen(configPath, L"w,ccs=UTF-16LE");
        if (!f_create) {
            MessageBoxW(NULL, L"致命错误：无法创建配置文件！请检查程序权限。\n尝试将程序放在桌面或“文档”等有写入权限的目录。\n错误码: %lu", L"配置文件创建失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
            return FALSE;
        }
        fwprintf(f_create, L"[%ls]\n", CONFIG_SECTION_NAME);
        fwprintf(f_create, L"EnableAutorun=%ls\n", g_config.enable_autorun ? L"true" : L"false");
        fwprintf(f_create, L"EnableTimedShutdown=%ls\n", g_config.enable_timed_shutdown ? L"true" : L"false");
        fwprintf(f_create, L"ShutdownHour=%d\n", g_config.shutdown_hour);
        fwprintf(f_create, L"ShutdownMinute=%d\n", g_config.shutdown_minute);
        fwprintf(f_create, L"EnableIdleShutdown=%ls\n", g_config.enable_idle_shutdown ? L"true" : L"false");
        fwprintf(f_create, L"IdleMinutes=%d\n", g_config.idle_minutes);
        fwprintf(f_create, L"CountdownSeconds=%d\n", g_config.countdown_seconds);
        fwprintf(f_create, L"HideMainWindow=%ls\n", g_config.hide_main_window ? L"true" : L"false");
        fclose(f_create);
    }
    FindClose(hFind);

    g_config.enable_autorun = GetPrivateProfileBoolW(CONFIG_SECTION_NAME, L"EnableAutorun", g_config.enable_autorun, configPath);
    g_config.enable_timed_shutdown = GetPrivateProfileBoolW(CONFIG_SECTION_NAME, L"EnableTimedShutdown", g_config.enable_timed_shutdown, configPath);
    g_config.shutdown_hour = GetPrivateProfileIntW(CONFIG_SECTION_NAME, L"ShutdownHour", g_config.shutdown_hour, configPath);
    g_config.shutdown_minute = GetPrivateProfileIntW(CONFIG_SECTION_NAME, L"ShutdownMinute", g_config.shutdown_minute, configPath);
    g_config.enable_idle_shutdown = GetPrivateProfileBoolW(CONFIG_SECTION_NAME, L"EnableIdleShutdown", g_config.enable_idle_shutdown, configPath);
    g_config.idle_minutes = GetPrivateProfileIntW(CONFIG_SECTION_NAME, L"IdleMinutes", g_config.idle_minutes, configPath);
    g_config.countdown_seconds = GetPrivateProfileIntW(CONFIG_SECTION_NAME, L"CountdownSeconds", g_config.countdown_seconds, configPath);
    g_config.hide_main_window = GetPrivateProfileBoolW(CONFIG_SECTION_NAME, L"HideMainWindow", g_config.hide_main_window, configPath);

    return TRUE;
}

BOOL SaveConfig(const WCHAR* configPath) {
    WritePrivateProfileBoolW(CONFIG_SECTION_NAME, L"EnableAutorun", g_config.enable_autorun, configPath);
    WritePrivateProfileBoolW(CONFIG_SECTION_NAME, L"EnableTimedShutdown", g_config.enable_timed_shutdown, configPath);
    WritePrivateProfileIntW(CONFIG_SECTION_NAME, L"ShutdownHour", g_config.shutdown_hour, configPath);
    WritePrivateProfileIntW(CONFIG_SECTION_NAME, L"ShutdownMinute", g_config.shutdown_minute, configPath);
    WritePrivateProfileBoolW(CONFIG_SECTION_NAME, L"EnableIdleShutdown", g_config.enable_idle_shutdown, configPath);
    WritePrivateProfileIntW(CONFIG_SECTION_NAME, L"IdleMinutes", g_config.idle_minutes, configPath);
    WritePrivateProfileIntW(CONFIG_SECTION_NAME, L"CountdownSeconds", g_config.countdown_seconds, configPath);
    WritePrivateProfileBoolW(CONFIG_SECTION_NAME, L"HideMainWindow", g_config.hide_main_window, configPath);

    if (WritePrivateProfileStringW(NULL, NULL, NULL, configPath) == 0) {
        return FALSE;
    }
    return TRUE;
}

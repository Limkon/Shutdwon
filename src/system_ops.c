#include "../include/system_ops.h"
#include <stdio.h>

// GetLastInputInfo Function Pointer and Module Handle (Internal to this file)
typedef BOOL (WINAPI *PFN_GetLastInputInfo)(LPLASTINPUTINFO);
static PFN_GetLastInputInfo pfnGetLastInputInfo = NULL;
static HMODULE hUser32 = NULL;

// --- Autorun Functions ---
BOOL IsAutorunEnabled() {
    HKEY hKey;
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR value[MAX_PATH];
        DWORD size = sizeof(value);
        LONG res = RegQueryValueExW(hKey, L"ShutdownTray", NULL, NULL, (LPBYTE)value, &size);
        RegCloseKey(hKey);
        return (res == ERROR_SUCCESS && wcscmp(value, path) == 0);
    }
    return FALSE;
}

void SetAutorun(BOOL enable) {
    HKEY hKey;
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);

    LONG createRes = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                                     0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
    if (createRes == ERROR_SUCCESS) {
        if (enable) {
            RegSetValueExW(hKey, L"ShutdownTray", 0, REG_SZ, (BYTE*)path,
                           (lstrlenW(path) + 1) * sizeof(WCHAR));
        } else {
            RegDeleteValueW(hKey, L"ShutdownTray");
        }
        RegCloseKey(hKey);
    }
}

// --- Shutdown Functions ---
// 重命名为 StartShutdownProcess 以解决冲突
void StartShutdownProcess(UINT countdown) {
    if (g_is_shutdown_pending) {
        return;
    }
    g_is_shutdown_pending = TRUE;
    SetTimer(g_hHiddenWindow, IDT_TIMER_SHUTDOWN_COUNTDOWN, countdown * 1000, NULL);
    WCHAR szMessage[256];
    swprintf_s(szMessage, ARRAYSIZE(szMessage), L"系统将在 %d 秒后关机。请保存您的工作。", countdown);
    MessageBoxW(g_hMainWindow, szMessage, L"关机提示", MB_OK | MB_ICONWARNING | MB_TOPMOST);
}

void StopShutdownCountdown() {
    KillTimer(g_hHiddenWindow, IDT_TIMER_SHUTDOWN_COUNTDOWN);
    g_is_shutdown_pending = FALSE;
}

void InitializeGetLastInputInfo() {
    hUser32 = LoadLibraryW(L"User32.dll");
    if (hUser32) {
        pfnGetLastInputInfo = (PFN_GetLastInputInfo)GetProcAddress(hUser32, "GetLastInputInfo");
        if (!pfnGetLastInputInfo) {
            FreeLibrary(hUser32);
            hUser32 = NULL;
        }
    }
}

void CleanupGetLastInputInfo() {
    if (hUser32) {
        FreeLibrary(hUser32);
        hUser32 = NULL;
        pfnGetLastInputInfo = NULL;
    }
}

DWORD GetIdleTime() {
    if (pfnGetLastInputInfo) {
        LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
        if (pfnGetLastInputInfo(&lii)) {
            return (GetTickCount() - lii.dwTime);
        }
    }
    return 0;
}

// --- Timer Management ---
void SetShutdownTimers() {
    // 停止所有现有的定时器
    KillTimer(g_hHiddenWindow, IDT_TIMER_CHECK_IDLE);
    KillTimer(g_hHiddenWindow, IDT_TIMER_CHECK_TIMED_SHUTDOWN);
    StopShutdownCountdown(); // 这将 g_is_shutdown_pending 设置为 FALSE

    if (g_config.enable_idle_shutdown) {
        if (g_config.idle_minutes <= 0) {
            g_config.enable_idle_shutdown = FALSE;
            MessageBoxW(NULL, L"闲置时间无效，已禁用闲置关机！", L"错误", MB_OK | MB_ICONERROR | MB_TOPMOST);
        } else {
            SetTimer(g_hHiddenWindow, IDT_TIMER_CHECK_IDLE, 30 * 1000, NULL);
        }
    }

    if (g_config.enable_timed_shutdown) {
        if (g_config.shutdown_hour < 0 || g_config.shutdown_hour > 23 ||
            g_config.shutdown_minute < 0 || g_config.shutdown_minute > 59) {
            g_config.enable_timed_shutdown = FALSE;
            MessageBoxW(NULL, L"定时关机时间无效，已禁用定时关机！", L"错误", MB_OK | MB_ICONERROR | MB_TOPMOST);
        } else {
            // 在重新设置定时器之前，重新评估 g_shutdown_executed_today 的状态
            SYSTEMTIME st_current_for_eval;
            GetLocalTime(&st_current_for_eval);
            long current_total_seconds_eval = st_current_for_eval.wHour * 3600 + st_current_for_eval.wMinute * 60 + st_current_for_eval.wSecond;
            long new_scheduled_total_seconds_eval = g_config.shutdown_hour * 3600 + g_config.shutdown_minute * 60;

            // 如果新的计划关机时间在当前时间之后，则重置 g_shutdown_executed_today
            if (new_scheduled_total_seconds_eval > current_total_seconds_eval) {
                g_shutdown_executed_today = FALSE;
            } else {
                g_shutdown_executed_today = TRUE;
            }
            // 确保 g_last_handled_day_for_timed_shutdown 是当前日期
            g_last_handled_day_for_timed_shutdown = st_current_for_eval.wDay;

            // 定时检查的间隔数为60秒
            SetTimer(g_hHiddenWindow, IDT_TIMER_CHECK_TIMED_SHUTDOWN, 60 * 1000, NULL);
        }
    }
}

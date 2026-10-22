#include "../include/system_ops.h"
#include <stdio.h>

// GetLastInputInfo Function Pointer and Module Handle (Internal to this file)
typedef BOOL (WINAPI *PFN_GetLastInputInfo)(LPLASTINPUTINFO);
static PFN_GetLastInputInfo pfnGetLastInputInfo = NULL;
static HMODULE hUser32 = NULL;

// --- Internal helper: run shutdown.exe with given arguments ---
// 返回 TRUE 表示 shutdown.exe 执行且退出码为 0；FALSE 表示创建失败或退出码非 0
// showError: 是否在失败时弹出错误提示（发起关机弹，取消关机不弹）
static BOOL RunShutdownCommand(const WCHAR* args, BOOL showError) {
    WCHAR cmdLine[128];

    // 使用完整路径，避免当前目录同名 shutdown.exe 劫持
    WCHAR sysDir[MAX_PATH];
    UINT sysDirLen = GetSystemDirectoryW(sysDir, MAX_PATH);
    if (sysDirLen > 0 && sysDirLen < MAX_PATH) {
        swprintf_s(cmdLine, ARRAYSIZE(cmdLine), L"%ls\\shutdown.exe %ls", sysDir, args);
    } else {
        swprintf_s(cmdLine, ARRAYSIZE(cmdLine), L"shutdown.exe %ls", args);
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        if (showError) {
            DWORD err = GetLastError();
            WCHAR msg[256];
            swprintf_s(msg, ARRAYSIZE(msg), L"CreateProcess 失败，错误码：%lu\n命令：%ls", err, cmdLine);
            MessageBoxW(g_hMainWindow, msg, L"关机命令失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
        }
        return FALSE;
    }

    // 等待进程结束，获取退出码
    WaitForSingleObject(pi.hProcess, 5000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        if (showError) {
            WCHAR msg[256];
            swprintf_s(msg, ARRAYSIZE(msg), L"shutdown.exe 返回错误码：%lu\n命令：%ls", exitCode, cmdLine);
            MessageBoxW(g_hMainWindow, msg, L"关机命令失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
        }
        return FALSE;
    }
    return TRUE;
}

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
    if (countdown == 0) {
        countdown = 1;   // SetTimer 间隔不能为 0；系统侧 -t 0 仍立即关机
    }
    g_is_shutdown_pending = TRUE;

    WCHAR args[32];
    swprintf_s(args, ARRAYSIZE(args), L"-s -t %u", countdown);

    // 发起关机时，若失败需要弹窗
    if (!RunShutdownCommand(args, TRUE)) {
        g_is_shutdown_pending = FALSE;
        return;
    }

    // 设置本地倒计时定时器，到点后清理状态（系统侧 shutdown -t N 会自行执行关机）
    SetTimer(g_hHiddenWindow, IDT_TIMER_SHUTDOWN_COUNTDOWN, countdown * 1000, NULL);
}

void CancelShutdownProcess() {
    // 无论本地状态如何，都尝试中止系统侧倒计时（幂等）
    // 取消关机时，如果本来就没有关机任务（错误码 1116），静默忽略，不弹窗
    RunShutdownCommand(L"-a", FALSE);

    KillTimer(g_hHiddenWindow, IDT_TIMER_SHUTDOWN_COUNTDOWN);
    g_is_shutdown_pending = FALSE;
}

void StopShutdownCountdown() {
    // 保留原函数名，语义等同于 CancelShutdownProcess
    CancelShutdownProcess();
}

void InitializeGetLastInputInfo() {
    if (hUser32 != NULL) {
        return; // 幂等保护：防止重复 LoadLibrary 造成句柄泄漏
    }
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
    CancelShutdownProcess(); // 这将 g_is_shutdown_pending 设置为 FALSE 并中止系统侧倒计时

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

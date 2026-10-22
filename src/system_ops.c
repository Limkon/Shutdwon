#include "../include/system_ops.h"
#include <stdio.h>

// GetLastInputInfo Function Pointer and Module Handle (Internal to this file)
typedef BOOL (WINAPI *PFN_GetLastInputInfo)(LPLASTINPUTINFO);
static PFN_GetLastInputInfo pfnGetLastInputInfo = NULL;
static HMODULE hUser32 = NULL;

// --- 权限提升与原生关机 API ---
// 尝试启用 SE_SHUTDOWN_NAME 特权
static BOOL EnableShutdownPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return FALSE;
    }

    if (!LookupPrivilegeValueW(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return FALSE;
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
    // AdjustTokenPrivileges 即使返回 TRUE，也可能因为权限不足而未实际生效
    if (result && GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        result = FALSE;
    }

    CloseHandle(hToken);
    return result;
}

// 尝试使用原生 API 关机
static BOOL TryShutdownAPI(UINT countdown) {
    if (!EnableShutdownPrivilege()) {
        return FALSE; // 权限不足（未提权），放弃 API 方案
    }

    // 参数: 机器名(NULL为本机), 提示消息, 倒计时秒数, 强制关闭应用, 关机后重启(FALSE), 原因代码
    if (InitiateSystemShutdownExW(NULL, L"系统将在倒计时结束后关机，请保存工作。", countdown, TRUE, FALSE,
                                  SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED)) {
        return TRUE;
    }

    return FALSE;
}

// 尝试使用原生 API 取消关机
static BOOL TryAbortShutdownAPI() {
    if (!EnableShutdownPrivilege()) {
        return FALSE;
    }
    return AbortSystemShutdownW(NULL);
}

// --- 备用方案：依赖 shutdown.exe 命令行 ---
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
void StartShutdownProcess(UINT countdown) {
    if (g_is_shutdown_pending) {
        return;
    }
    if (countdown == 0) {
        countdown = 1;   // SetTimer 间隔不能为 0；系统侧 -t 0 仍立即关机
    }
    g_is_shutdown_pending = TRUE;

    BOOL success = FALSE;

    // 方案 1：尝试原生 API 方案（自带系统倒计时与取消功能）
    success = TryShutdownAPI(countdown);

    // 方案 2：若原生 API 失败（如无特权），回退到命令行方案
    if (!success) {
        WCHAR args[32];
        swprintf_s(args, ARRAYSIZE(args), L"-s -t %u", countdown);
        success = RunShutdownCommand(args, TRUE);
    }

    if (!success) {
        g_is_shutdown_pending = FALSE;
        return;
    }

    // 设置本地倒计时定时器，到点后清理状态
    SetTimer(g_hHiddenWindow, IDT_TIMER_SHUTDOWN_COUNTDOWN, countdown * 1000, NULL);
}

void CancelShutdownProcess() {
    // 无论当前走的是哪个方案，都尝试取消（幂等，失败静默忽略）
    
    // 尝试原生取消
    if (!TryAbortShutdownAPI()) {
        // 如果原生取消失败，尝试命令行取消
        RunShutdownCommand(L"-a", FALSE);
    }

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
            SYSTEMTIME st_current_for_eval;
            GetLocalTime(&st_current_for_eval);
            long current_total_seconds_eval = st_current_for_eval.wHour * 3600 + st_current_for_eval.wMinute * 60 + st_current_for_eval.wSecond;
            long new_scheduled_total_seconds_eval = g_config.shutdown_hour * 3600 + g_config.shutdown_minute * 60;

            if (new_scheduled_total_seconds_eval > current_total_seconds_eval) {
                g_shutdown_executed_today = FALSE;
            } else {
                g_shutdown_executed_today = TRUE;
            }
            g_last_handled_day_for_timed_shutdown = st_current_for_eval.wDay;

            SetTimer(g_hHiddenWindow, IDT_TIMER_CHECK_TIMED_SHUTDOWN, 60 * 1000, NULL);
        }
    }
}

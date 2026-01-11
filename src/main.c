#include "../include/common.h"
#include "../include/config.h"
#include "../include/system_ops.h"
#include "../include/gui.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPWSTR lpCmdLine, int nCmdShow) {
    // 设置工作目录
    WCHAR exeDir[MAX_PATH];
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    WCHAR* p = wcsrchr(exeDir, L'\\');
    if (p != NULL) {
        *p = L'\0';
        if (!SetCurrentDirectoryW(exeDir)) {
            MessageBoxW(NULL, L"致命错误：无法设置程序工作目录！请检查程序权限或放置位置。", L"程序启动失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
            return 1;
        }
    } else {
        MessageBoxW(NULL, L"致命错误：无法确定程序所在目录！请将程序放置在有效位置。", L"程序启动失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
        return 1;
    }

    // 确定配置文件路径
    swprintf_s(g_config_file_path, ARRAYSIZE(g_config_file_path), L"%s\\%s", exeDir, CONFIG_FILE_BASE_NAME);

    // 测试写入权限
    FILE* testConfigFile = _wfopen(g_config_file_path, L"a");
    if (!testConfigFile) {
        MessageBoxW(NULL,
                             L"致命错误：程序所在目录无写入权限！\n\n"
                             L"请将本程序 (ShutdownTray.exe) 移动到桌面、文档、下载等您拥有完全写入权限的目录，然后再次运行。\n"
                             L"错误码: %lu",
                             L"权限不足，无法启动", MB_OK | MB_ICONERROR | MB_TOPMOST);
        return 1;
    }
    fclose(testConfigFile);

    // 互斥量以确保单实例运行
    g_hMutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing_hwnd = FindWindowW(MAIN_WINDOW_CLASS, NULL);
        if (existing_hwnd) {
            ShowWindow(existing_hwnd, SW_RESTORE);
            SetWindowPos(existing_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetWindowPos(existing_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetForegroundWindow(existing_hwnd);
        }
        if (g_hMutex) CloseHandle(g_hMutex);
        return 0;
    }

    // 加载配置
    if (!LoadConfig(g_config_file_path)) {
        if (g_hMutex) CloseHandle(g_hMutex);
        return 1;
    }

    // 设置开机启动
    SetAutorun(g_config.enable_autorun);

    // 根据当前时间初始化 g_shutdown_executed_today
    SYSTEMTIME st_init;
    GetLocalTime(&st_init);
    long current_total_seconds_init = st_init.wHour * 3600 + st_init.wMinute * 60 + st_init.wSecond;
    long scheduled_total_seconds_init = g_config.shutdown_hour * 3600 + g_config.shutdown_minute * 60;

    if (g_config.enable_timed_shutdown) {
        if (current_total_seconds_init >= scheduled_total_seconds_init) {
            g_shutdown_executed_today = TRUE;
        } else {
            g_shutdown_executed_today = FALSE;
        }
    } else {
        g_shutdown_executed_today = FALSE;
    }
    g_last_handled_day_for_timed_shutdown = st_init.wDay;

    // 初始化 GetLastInputInfo
    InitializeGetLastInputInfo();

    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_WINDOW_CLASS;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"致命错误：无法注册主窗口类！程序将无法运行。", L"程序启动失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
        CleanupGetLastInputInfo();
        if (g_hMutex) CloseHandle(g_hMutex);
        return 1;
    }

    wc.lpfnWndProc = HiddenWindowProc;
    wc.lpszClassName = HIDDEN_WINDOW_CLASS;
    wc.hIcon = NULL;
    wc.hbrBackground = NULL;
    if (!RegisterClassW(&wc)) {
        MessageBoxW(NULL, L"致命错误：无法注册隐藏窗口类！程序将无法接收定时器消息。", L"程序启动失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
        CleanupGetLastInputInfo();
        if (g_hMutex) CloseHandle(g_hMutex);
        return 1;
    }

    // 创建隐藏窗口
    g_hHiddenWindow = CreateWindowExW(0, HIDDEN_WINDOW_CLASS, L"ShutdownAssistantHiddenWindow", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    if (!g_hHiddenWindow) {
        MessageBoxW(NULL, L"致命错误：无法创建隐藏窗口！程序将无法运行。", L"程序启动失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
        CleanupGetLastInputInfo();
        if (g_hMutex) CloseHandle(g_hMutex);
        return 1;
    }

    // 创建主窗口
    int initialCmdShow = g_config.hide_main_window ? SW_HIDE : SW_SHOW;
    g_hMainWindow = CreateWindowExW(
        0, MAIN_WINDOW_CLASS, L"定时闲置关机助手",
        WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 430, 320, // 已修改：高度从 250 增加到 320，确保按钮可见
        NULL, NULL, hInstance, NULL);

    if (!g_hMainWindow) {
        MessageBoxW(NULL, L"致命错误：无法创建主界面窗口！程序将无法运行。", L"程序启动失败", MB_OK | MB_ICONERROR | MB_TOPMOST);
        DestroyWindow(g_hHiddenWindow);
        CleanupGetLastInputInfo();
        if (g_hMutex) CloseHandle(g_hMutex);
        return 1;
    }

    ApplyConfigToGUI();
    ShowWindow(g_hMainWindow, initialCmdShow);
    UpdateWindow(g_hMainWindow);

    SetShutdownTimers(); 

    // 消息循环
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 清理资源
    CleanupGetLastInputInfo();
    if (g_hMutex) CloseHandle(g_hMutex);
    UnregisterClassW(MAIN_WINDOW_CLASS, hInstance);
    UnregisterClassW(HIDDEN_WINDOW_CLASS, hInstance);
    return (int)msg.wParam;
}

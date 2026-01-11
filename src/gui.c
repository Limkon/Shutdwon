#include "../include/gui.h"
#include "../include/config.h"
#include "../include/system_ops.h"
#include <commctrl.h> // 为了更好的控件样式（可选）

// 全局字体句柄，用于在程序退出时销毁
static HFONT g_hAppFont = NULL;

// --- 辅助函数：遍历并设置子控件字体 ---
static BOOL CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam) {
    SendMessage(hwndChild, WM_SETFONT, (WPARAM)lParam, MAKELPARAM(TRUE, 0));
    return TRUE;
}

// --- 核心修复：创建并应用系统默认字体 ---
void SetSystemFont(HWND hwndParent) {
    if (g_hAppFont == NULL) {
        // 获取系统非客户区度量信息（包含消息框字体，即系统默认UI字体）
        NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
        
        // 兼容性处理：如果编译时定义了 Vista 之后的结构体大小，但在 XP 运行，需要调整大小
        // 这里直接使用当前结构体大小，通常没问题
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0)) {
            g_hAppFont = CreateFontIndirectW(&ncm.lfMessageFont);
        } else {
            // 如果获取失败，回退到默认 GUI 字体（虽然较丑，但总比没有好）
            g_hAppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        }
    }

    // 将字体应用到主窗口（虽然主窗口通常没有文字直接显示在客户区，但作为父句柄是个好习惯）
    SendMessage(hwndParent, WM_SETFONT, (WPARAM)g_hAppFont, MAKELPARAM(TRUE, 0));

    // 关键：枚举所有子控件（按钮、输入框、标签）并设置字体
    EnumChildWindows(hwndParent, EnumChildProc, (LPARAM)g_hAppFont);
}

// --- GUI Functions ---
void ApplyConfigToGUI() {
    if (!g_hMainWindow) return;

    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_AUTORUN), BM_SETCHECK, g_config.enable_autorun ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_TIMED_SHUTDOWN), BM_SETCHECK, g_config.enable_timed_shutdown ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_IDLE_SHUTDOWN), BM_SETCHECK, g_config.enable_idle_shutdown ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_HIDE_MAIN_WINDOW), BM_SETCHECK, g_config.hide_main_window ? BST_CHECKED : BST_UNCHECKED, 0);

    WCHAR szTime[16];
    swprintf_s(szTime, ARRAYSIZE(szTime), L"%02d", g_config.shutdown_hour);
    SetDlgItemTextW(g_hMainWindow, IDC_EDIT_SHUTDOWN_HOUR, szTime);
    swprintf_s(szTime, ARRAYSIZE(szTime), L"%02d", g_config.shutdown_minute);
    SetDlgItemTextW(g_hMainWindow, IDC_EDIT_SHUTDOWN_MINUTE, szTime);
    swprintf_s(szTime, ARRAYSIZE(szTime), L"%d", g_config.idle_minutes);
    SetDlgItemTextW(g_hMainWindow, IDC_EDIT_IDLE_MINUTES, szTime);
    swprintf_s(szTime, ARRAYSIZE(szTime), L"%d", g_config.countdown_seconds);
    SetDlgItemTextW(g_hMainWindow, IDC_EDIT_COUNTDOWN_SECONDS, szTime);

    InvalidateRect(g_hMainWindow, NULL, TRUE);
    UpdateWindow(g_hMainWindow);
}

void GetConfigFromGUI() {
    g_config.enable_autorun = (SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_AUTORUN), BM_GETCHECK, 0, 0) == BST_CHECKED);
    g_config.enable_timed_shutdown = (SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_TIMED_SHUTDOWN), BM_GETCHECK, 0, 0) == BST_CHECKED);
    g_config.enable_idle_shutdown = (SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_IDLE_SHUTDOWN), BM_GETCHECK, 0, 0) == BST_CHECKED);
    g_config.hide_main_window = (SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_HIDE_MAIN_WINDOW), BM_GETCHECK, 0, 0) == BST_CHECKED);

    WCHAR szTime[16];
    GetDlgItemTextW(g_hMainWindow, IDC_EDIT_SHUTDOWN_HOUR, szTime, ARRAYSIZE(szTime));
    g_config.shutdown_hour = _wtoi(szTime);
    GetDlgItemTextW(g_hMainWindow, IDC_EDIT_SHUTDOWN_MINUTE, szTime, ARRAYSIZE(szTime));
    g_config.shutdown_minute = _wtoi(szTime);
    GetDlgItemTextW(g_hMainWindow, IDC_EDIT_IDLE_MINUTES, szTime, ARRAYSIZE(szTime));
    g_config.idle_minutes = _wtoi(szTime);
    GetDlgItemTextW(g_hMainWindow, IDC_EDIT_COUNTDOWN_SECONDS, szTime, ARRAYSIZE(szTime));
    g_config.countdown_seconds = _wtoi(szTime);
}

// --- Main Window Procedure ---
LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hMainWindow = hWnd;
            int yPos = 20;
            const int lineSpacing = 35; // 稍微增加行距以适应更清晰的字体
            const int labelWidth = 120;
            const int checkboxColX = 145;
            const int editWidthSmall = 35;
            const int editWidthLarge = 60;
            const int buttonWidth = 80; // 稍微加宽按钮
            const int buttonHeight = 28;
            const int buttonHorizontalSpacing = 8;

            // 1. 开机启动
            CreateWindowW(L"STATIC", L"开机启动:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 60, 20, hWnd, (HMENU)IDC_CHK_AUTORUN, NULL, NULL);
            yPos += lineSpacing;

            // 2. 定时关机
            CreateWindowW(L"STATIC", L"定时关机:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 60, 20, hWnd, (HMENU)IDC_CHK_TIMED_SHUTDOWN, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER, checkboxColX + 65, yPos, editWidthSmall, 22, hWnd, (HMENU)IDC_EDIT_SHUTDOWN_HOUR, NULL, NULL);
            CreateWindowW(L"STATIC", L":", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE | SS_CENTER, checkboxColX + 100, yPos, 10, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER, checkboxColX + 115, yPos, editWidthSmall, 22, hWnd, (HMENU)IDC_EDIT_SHUTDOWN_MINUTE, NULL, NULL);
            yPos += lineSpacing;

            // 3. 闲置关机
            CreateWindowW(L"STATIC", L"闲置关机:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 60, 20, hWnd, (HMENU)IDC_CHK_IDLE_SHUTDOWN, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER, checkboxColX + 65, yPos, editWidthLarge, 22, hWnd, (HMENU)IDC_EDIT_IDLE_MINUTES, NULL, NULL);
            CreateWindowW(L"STATIC", L"分钟", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, checkboxColX + 130, yPos, 40, 20, hWnd, NULL, NULL, NULL);
            yPos += lineSpacing;

            // 4. 倒计时
            CreateWindowW(L"STATIC", L"关机倒计时:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_CENTER, checkboxColX, yPos, editWidthLarge, 22, hWnd, (HMENU)IDC_EDIT_COUNTDOWN_SECONDS, NULL, NULL);
            CreateWindowW(L"STATIC", L"秒", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, checkboxColX + 65, yPos, 40, 20, hWnd, NULL, NULL, NULL);
            yPos += lineSpacing;

            // 5. 隐藏界面
            CreateWindowW(L"STATIC", L"启动时隐藏界面:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"是", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 50, 20, hWnd, (HMENU)IDC_CHK_HIDE_MAIN_WINDOW, NULL, NULL);
            yPos += lineSpacing + 15;

            // 6. 按钮区域
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            int windowWidth = clientRect.right - clientRect.left;
            int totalButtonsWidth = (buttonWidth * 4) + (buttonHorizontalSpacing * 3);
            int btnX = (windowWidth - totalButtonsWidth) / 2;

            CreateWindowW(L"BUTTON", L"保存设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, yPos, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_BTN_SAVE_SETTINGS, NULL, NULL);
            btnX += buttonWidth + buttonHorizontalSpacing;
            CreateWindowW(L"BUTTON", L"立即关机", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, yPos, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_BTN_SHUTDOWN_NOW, NULL, NULL);
            btnX += buttonWidth + buttonHorizontalSpacing;
            CreateWindowW(L"BUTTON", L"隐藏后台", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, yPos, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_BTN_HIDE_PROGRAM, NULL, NULL);
            btnX += buttonWidth + buttonHorizontalSpacing;
            CreateWindowW(L"BUTTON", L"退出程序", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, yPos, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_BTN_EXIT_APP, NULL, NULL);

            // --- 核心修复：应用系统字体 ---
            SetSystemFont(hWnd);
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDC_BTN_SAVE_SETTINGS:
                    GetConfigFromGUI();
                    SaveConfig(g_config_file_path);
                    SetAutorun(g_config.enable_autorun);

                    SetShutdownTimers(); 

                    BOOL immediate_shutdown_triggered_on_save = FALSE;
                    if (g_config.enable_timed_shutdown && !g_is_shutdown_pending && !g_shutdown_executed_today) {
                        SYSTEMTIME st_current_on_save_check;
                        GetLocalTime(&st_current_on_save_check);
                        long current_total_seconds_on_save = st_current_on_save_check.wHour * 3600 + st_current_on_save_check.wMinute * 60 + st_current_on_save_check.wSecond;
                        long scheduled_total_seconds_on_save = g_config.shutdown_hour * 3600 + g_config.shutdown_minute * 60;
                        long time_diff_seconds_on_save = current_total_seconds_on_save - scheduled_total_seconds_on_save;

                        if (time_diff_seconds_on_save >= 0 && time_diff_seconds_on_save <= 60) {
                            StartShutdownProcess(g_config.countdown_seconds);
                            g_shutdown_executed_today = TRUE; 
                            immediate_shutdown_triggered_on_save = TRUE;
                        }
                    }

                    if (!immediate_shutdown_triggered_on_save) {
                        MessageBoxW(hWnd, L"设置已保存并应用！", L"提示", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                    }
                    break;
                case IDC_BTN_SHUTDOWN_NOW:
                    StartShutdownProcess(g_config.countdown_seconds);
                    break;
                case IDC_BTN_HIDE_PROGRAM:
                    ShowWindow(hWnd, SW_HIDE);
                    break;
                case IDC_BTN_EXIT_APP:
                    DestroyWindow(g_hHiddenWindow); // 触发 HiddenWindow 的 WM_DESTROY，进而 PostQuitMessage
                    break;
            }
            break;
        }

        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE);
            break;

        case WM_DESTROY:
            // --- 核心修复：清理字体对象 ---
            if (g_hAppFont) {
                DeleteObject(g_hAppFont);
                g_hAppFont = NULL;
            }
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

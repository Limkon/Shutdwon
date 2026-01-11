#include "../include/gui.h"
#include "../include/config.h"
#include "../include/system_ops.h"

// --- GUI Functions ---
void ApplyConfigToGUI() {
    if (!g_hMainWindow) return;

    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_AUTORUN), BM_SETCHECK, g_config.enable_autorun ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_TIMED_SHUTDOWN), BM_SETCHECK, g_config.enable_timed_shutdown ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_IDLE_SHUTDOWN), BM_SETCHECK, g_config.enable_idle_shutdown ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_HIDE_MAIN_WINDOW), BM_SETCHECK, g_config.hide_main_window ? BST_CHECKED : BST_UNCHECKED, 0);

    WCHAR szTime[5];
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

    WCHAR szTime[5];
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
            const int lineSpacing = 28;
            const int labelWidth = 120;
            const int checkboxColX = 145;
            const int editWidthSmall = 35;
            const int editWidthLarge = 60;
            const int buttonWidth = 75;
            const int buttonHeight = 25;
            const int buttonHorizontalSpacing = 8;

            CreateWindowW(L"STATIC", L"开机启动:", WS_VISIBLE | WS_CHILD, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 20, 20, hWnd, (HMENU)IDC_CHK_AUTORUN, NULL, NULL);
            yPos += lineSpacing;

            CreateWindowW(L"STATIC", L"定时关机:", WS_VISIBLE | WS_CHILD, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 20, 20, hWnd, (HMENU)IDC_CHK_TIMED_SHUTDOWN, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, checkboxColX + 30, yPos, editWidthSmall, 20, hWnd, (HMENU)IDC_EDIT_SHUTDOWN_HOUR, NULL, NULL);
            CreateWindowW(L"STATIC", L":", WS_VISIBLE | WS_CHILD, checkboxColX + 70, yPos, 10, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, checkboxColX + 85, yPos, editWidthSmall, 20, hWnd, (HMENU)IDC_EDIT_SHUTDOWN_MINUTE, NULL, NULL);
            yPos += lineSpacing;

            CreateWindowW(L"STATIC", L"闲置关机:", WS_VISIBLE | WS_CHILD, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 20, 20, hWnd, (HMENU)IDC_CHK_IDLE_SHUTDOWN, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, checkboxColX + 30, yPos, editWidthLarge, 20, hWnd, (HMENU)IDC_EDIT_IDLE_MINUTES, NULL, NULL);
            CreateWindowW(L"STATIC", L"分钟", WS_VISIBLE | WS_CHILD, checkboxColX + 95, yPos, 40, 20, hWnd, NULL, NULL, NULL);
            yPos += lineSpacing;

            CreateWindowW(L"STATIC", L"关机倒计时:", WS_VISIBLE | WS_CHILD, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, checkboxColX, yPos, editWidthLarge, 20, hWnd, (HMENU)IDC_EDIT_COUNTDOWN_SECONDS, NULL, NULL);
            CreateWindowW(L"STATIC", L"秒", WS_VISIBLE | WS_CHILD, checkboxColX + 65, yPos, 40, 20, hWnd, NULL, NULL, NULL);
            yPos += lineSpacing;

            CreateWindowW(L"STATIC", L"下次启动隐藏界面:", WS_VISIBLE | WS_CHILD, 20, yPos, labelWidth + 30, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX + 30, yPos, 20, 20, hWnd, (HMENU)IDC_CHK_HIDE_MAIN_WINDOW, NULL, NULL);
            yPos += lineSpacing + 15;

            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            int windowWidth = clientRect.right - clientRect.left;
            int totalButtonsWidth = (buttonWidth * 4) + (buttonHorizontalSpacing * 3);
            int btnX = (windowWidth - totalButtonsWidth) / 2;

            CreateWindowW(L"BUTTON", L"保存设置", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, yPos, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_BTN_SAVE_SETTINGS, NULL, NULL);
            btnX += buttonWidth + buttonHorizontalSpacing;
            CreateWindowW(L"BUTTON", L"立即关机", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, yPos, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_BTN_SHUTDOWN_NOW, NULL, NULL);
            btnX += buttonWidth + buttonHorizontalSpacing;
            CreateWindowW(L"BUTTON", L"隐藏程序", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, yPos, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_BTN_HIDE_PROGRAM, NULL, NULL);
            btnX += buttonWidth + buttonHorizontalSpacing;
            CreateWindowW(L"BUTTON", L"退出程序", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, btnX, yPos, buttonWidth, buttonHeight, hWnd, (HMENU)IDC_BTN_EXIT_APP, NULL, NULL);
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
                            InitiateShutdown(g_config.countdown_seconds);
                            g_shutdown_executed_today = TRUE; 
                            immediate_shutdown_triggered_on_save = TRUE;
                        }
                    }

                    if (!immediate_shutdown_triggered_on_save) {
                        MessageBoxW(hWnd, L"设置已保存并应用！", L"提示", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                    }
                    break;
                case IDC_BTN_SHUTDOWN_NOW:
                    InitiateShutdown(g_config.countdown_seconds);
                    break;
                case IDC_BTN_HIDE_PROGRAM:
                    ShowWindow(hWnd, SW_HIDE);
                    break;
                case IDC_BTN_EXIT_APP:
                    DestroyWindow(g_hHiddenWindow);
                    break;
            }
            break;
        }

        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE);
            break;

        case WM_DESTROY:
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// --- Hidden Window Procedure ---
LRESULT CALLBACK HiddenWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            g_hHiddenWindow = hWnd;
            break;

        case WM_TIMER:
            if (LOWORD(wParam) == IDT_TIMER_CHECK_IDLE) {
                if (g_config.enable_idle_shutdown && !g_is_shutdown_pending) {
                    DWORD idle_time_ms = GetIdleTime();
                    DWORD configured_idle_time_ms = (DWORD)g_config.idle_minutes * 60 * 1000;
                    if (configured_idle_time_ms > 0 && idle_time_ms >= configured_idle_time_ms) {
                        InitiateShutdown(g_config.countdown_seconds);
                    }
                }
            } else if (LOWORD(wParam) == IDT_TIMER_CHECK_TIMED_SHUTDOWN) {
                SYSTEMTIME st;
                GetLocalTime(&st);

                // Check date change
                if (g_last_handled_day_for_timed_shutdown == 0) {
                    g_last_handled_day_for_timed_shutdown = st.wDay;
                }
                if (st.wDay != g_last_handled_day_for_timed_shutdown) {
                    g_shutdown_executed_today = FALSE;
                    g_last_handled_day_for_timed_shutdown = st.wDay;
                }

                // Check timed shutdown
                if (g_config.enable_timed_shutdown && !g_shutdown_executed_today && !g_is_shutdown_pending) {
                    long current_total_seconds = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
                    long scheduled_total_seconds = g_config.shutdown_hour * 3600 + g_config.shutdown_minute * 60;
                    long time_diff_seconds = current_total_seconds - scheduled_total_seconds;

                    if (time_diff_seconds >= 0 && time_diff_seconds <= 60) {
                        InitiateShutdown(g_config.countdown_seconds);
                        g_shutdown_executed_today = TRUE;
                    }
                }
            } else if (LOWORD(wParam) == IDT_TIMER_SHUTDOWN_COUNTDOWN) {
                KillTimer(hWnd, IDT_TIMER_SHUTDOWN_COUNTDOWN);
                g_is_shutdown_pending = FALSE;

                STARTUPINFOW si = { sizeof(si) };
                PROCESS_INFORMATION pi = {0};
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                WCHAR cmdLine[] = L"shutdown.exe -s -t 0";
                if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
                                   CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                } else {
                    MessageBoxW(NULL, L"执行关机命令失败！请检查权限。", L"关机错误", MB_OK | MB_ICONERROR | MB_TOPMOST);
                }
                PostQuitMessage(0);
            }
            break;

        case WM_ENDSESSION:
            if (wParam == TRUE) {
                KillTimer(hWnd, IDT_TIMER_CHECK_IDLE);
                KillTimer(hWnd, IDT_TIMER_CHECK_TIMED_SHUTDOWN);
                KillTimer(hWnd, IDT_TIMER_SHUTDOWN_COUNTDOWN);
                g_is_shutdown_pending = FALSE;
            }
            return 0;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

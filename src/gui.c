#include "../include/gui.h"
#include "../include/config.h"
#include "../include/system_ops.h"
#include <commctrl.h>

// 全局字体句柄
static HFONT g_hAppFont = NULL;

// --- 辅助函数：遍历并设置子控件字体 ---
static BOOL CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam) {
    SendMessage(hwndChild, WM_SETFONT, (WPARAM)lParam, MAKELPARAM(TRUE, 0));
    return TRUE;
}

// --- 核心优化：使用微软雅黑 UI + ClearType 抗锯齿字体 ---
void SetSystemFont(HWND hwndParent) {
    if (g_hAppFont == NULL) {
        LOGFONTW lf = {0};
        lf.lfHeight = -14;                    // 约 10.5pt，比默认 9pt 更清晰
        lf.lfWeight = FW_NORMAL;              // 普通字重，避免过粗发黑
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;     // 启用 ClearType 抗锯齿，消除毛刺
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");

        g_hAppFont = CreateFontIndirectW(&lf);

        // 如果系统没有微软雅黑，回退到系统消息字体（但保持 ClearType）
        if (!g_hAppFont) {
            NONCLIENTMETRICSW ncm = { sizeof(NONCLIENTMETRICSW) };
            if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0)) {
                ncm.lfMessageFont.lfQuality = CLEARTYPE_QUALITY;
                g_hAppFont = CreateFontIndirectW(&ncm.lfMessageFont);
            } else {
                g_hAppFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            }
        }
    }
    SendMessage(hwndParent, WM_SETFONT, (WPARAM)g_hAppFont, MAKELPARAM(TRUE, 0));
    EnumChildWindows(hwndParent, EnumChildProc, (LPARAM)g_hAppFont);
}

// --- Tray Icon Helpers ---
static void CreateTrayIcon() {
    if (g_tray_icon_added) return;

    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = g_hMainWindow;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCE(IDI_APPICON));
    wcscpy_s(g_nid.szTip, ARRAYSIZE(g_nid.szTip), L"定时闲置关机助手");

    if (Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        g_tray_icon_added = TRUE;
        g_show_tray_icon_active = TRUE;
    } else {
        g_tray_icon_added = FALSE;
        g_show_tray_icon_active = FALSE;
    }
}

static void RemoveTrayIcon() {
    if (g_tray_icon_added) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_tray_icon_added = FALSE;
    }
    g_show_tray_icon_active = FALSE;
}

static void ShowTrayContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW_MAIN, L"显示主界面");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHUTDOWN_NOW, L"立即关机");
    AppendMenuW(hMenu, MF_STRING | (g_is_shutdown_pending ? MF_ENABLED : MF_GRAYED), IDM_TRAY_CANCEL_SHUTDOWN, L"取消倒计时");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"退出程序");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g_hMainWindow);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hMainWindow, NULL);
    PostMessage(g_hMainWindow, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

// --- Status Text Helpers ---
static void FormatMinutesToHMS(long total_minutes, WCHAR* buf, size_t cch) {
    if (total_minutes < 0) total_minutes = 0;
    long hours = total_minutes / 60;
    long mins = total_minutes % 60;
    if (hours > 0) {
        swprintf_s(buf, cch, L"%ld 小时 %ld 分", hours, mins);
    } else {
        swprintf_s(buf, cch, L"%ld 分", mins);
    }
}

static void UpdateStatusText() {
    if (!g_hMainWindow) return;

    WCHAR timed_part[128];
    WCHAR idle_part[128];
    WCHAR countdown_part[128];
    WCHAR tmp[64];

    // 定时关机状态
    if (g_config.enable_timed_shutdown) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        long now_sec = st.wHour * 3600L + st.wMinute * 60L + st.wSecond;
        long sched_sec = g_config.shutdown_hour * 3600L + g_config.shutdown_minute * 60L;
        long remain_sec = sched_sec - now_sec;
        if (remain_sec < 0) remain_sec += 24L * 3600L;

        WCHAR remain_str[64];
        FormatMinutesToHMS(remain_sec / 60, remain_str, ARRAYSIZE(remain_str));
        swprintf_s(timed_part, ARRAYSIZE(timed_part),
                   L"定时关机：已启用，%02d:%02d 触发（剩余 %ls）",
                   g_config.shutdown_hour, g_config.shutdown_minute, remain_str);
    } else {
        swprintf_s(timed_part, ARRAYSIZE(timed_part), L"定时关机：未启用");
    }

    // 闲置关机状态
    if (g_config.enable_idle_shutdown && g_config.idle_minutes > 0) {
        DWORD idle_ms = GetIdleTime();
        long idle_min = (long)(idle_ms / 60000UL);
        swprintf_s(idle_part, ARRAYSIZE(idle_part),
                   L"闲置关机：已启用，阈值 %d 分钟，当前已闲置 %ld 分钟",
                   g_config.idle_minutes, idle_min);
    } else {
        swprintf_s(idle_part, ARRAYSIZE(idle_part), L"闲置关机：未启用");
    }

    // 倒计时状态
    if (g_is_shutdown_pending) {
        swprintf_s(countdown_part, ARRAYSIZE(countdown_part),
                   L"倒计时：进行中（%d 秒内关机）", g_config.countdown_seconds);
    } else {
        swprintf_s(countdown_part, ARRAYSIZE(countdown_part), L"倒计时：未进行");
    }

    swprintf_s(g_status_text, ARRAYSIZE(g_status_text),
               L"%ls ｜ %ls ｜ %ls", timed_part, idle_part, countdown_part);

    SetDlgItemTextW(g_hMainWindow, IDC_STATIC_STATUS, g_status_text);

    (void)tmp; // 保留以防未来扩展
}

// --- GUI Functions ---
void ApplyConfigToGUI() {
    if (!g_hMainWindow) return;

    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_AUTORUN), BM_SETCHECK, g_config.enable_autorun ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_TIMED_SHUTDOWN), BM_SETCHECK, g_config.enable_timed_shutdown ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_IDLE_SHUTDOWN), BM_SETCHECK, g_config.enable_idle_shutdown ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_HIDE_MAIN_WINDOW), BM_SETCHECK, g_config.hide_main_window ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_SHOW_TRAY_ICON), BM_SETCHECK, g_config.show_tray_icon ? BST_CHECKED : BST_UNCHECKED, 0);

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
    g_config.show_tray_icon = (SendMessage(GetDlgItem(g_hMainWindow, IDC_CHK_SHOW_TRAY_ICON), BM_GETCHECK, 0, 0) == BST_CHECKED);

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
            const int lineSpacing = 35; 
            const int labelWidth = 120;
            const int checkboxColX = 145;
            const int editWidthSmall = 35;
            const int editWidthLarge = 60;
            const int buttonWidth = 80; 
            const int buttonHeight = 28;
            const int buttonHorizontalSpacing = 8;

            // 1. 开机启动
            CreateWindowW(L"STATIC", L"开机启动:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 60, 20, hWnd, (HMENU)IDC_CHK_AUTORUN, NULL, NULL);
            yPos += lineSpacing;

            // 2. 定时关机
            CreateWindowW(L"STATIC", L"定时关机:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 60, 20, hWnd, (HMENU)IDC_CHK_TIMED_SHUTDOWN, NULL, NULL);
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER, checkboxColX + 65, yPos, editWidthSmall, 22, hWnd, (HMENU)IDC_EDIT_SHUTDOWN_HOUR, NULL, NULL);
            CreateWindowW(L"STATIC", L":", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE | SS_CENTER, checkboxColX + 100, yPos, 10, 20, hWnd, NULL, NULL, NULL);
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER, checkboxColX + 115, yPos, editWidthSmall, 22, hWnd, (HMENU)IDC_EDIT_SHUTDOWN_MINUTE, NULL, NULL);
            yPos += lineSpacing;

            // 3. 闲置关机
            CreateWindowW(L"STATIC", L"闲置关机:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"启用", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 60, 20, hWnd, (HMENU)IDC_CHK_IDLE_SHUTDOWN, NULL, NULL);
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER, checkboxColX + 65, yPos, editWidthLarge, 22, hWnd, (HMENU)IDC_EDIT_IDLE_MINUTES, NULL, NULL);
            CreateWindowW(L"STATIC", L"分钟", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, checkboxColX + 130, yPos, 40, 20, hWnd, NULL, NULL, NULL);
            yPos += lineSpacing;

            // 4. 倒计时
            CreateWindowW(L"STATIC", L"关机倒计时:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_NUMBER | ES_CENTER, checkboxColX, yPos, editWidthLarge, 22, hWnd, (HMENU)IDC_EDIT_COUNTDOWN_SECONDS, NULL, NULL);
            CreateWindowW(L"STATIC", L"秒", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, checkboxColX + 65, yPos, 40, 20, hWnd, NULL, NULL, NULL);
            yPos += lineSpacing;

            // 5. 隐藏界面
            CreateWindowW(L"STATIC", L"启动时隐藏界面:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"是", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 50, 20, hWnd, (HMENU)IDC_CHK_HIDE_MAIN_WINDOW, NULL, NULL);
            yPos += lineSpacing;

            // 6. 显示托盘图标
            CreateWindowW(L"STATIC", L"显示托盘图标:", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 20, yPos, labelWidth, 20, hWnd, NULL, NULL, NULL);
            CreateWindowW(L"BUTTON", L"是", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, checkboxColX, yPos, 50, 20, hWnd, (HMENU)IDC_CHK_SHOW_TRAY_ICON, NULL, NULL);
            yPos += lineSpacing + 15;

            // 7. 按钮区域
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
            yPos += buttonHeight + 15;

            // 8. 状态栏
            CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"状态：就绪", 
                WS_VISIBLE | WS_CHILD | SS_LEFT | SS_CENTERIMAGE,
                20, yPos, windowWidth - 40, 24, hWnd, (HMENU)IDC_STATIC_STATUS, NULL, NULL);

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

                    // 托盘图标显隐切换
                    if (g_config.show_tray_icon && !g_tray_icon_added) {
                        CreateTrayIcon();
                    } else if (!g_config.show_tray_icon && g_tray_icon_added) {
                        RemoveTrayIcon();
                    }

                    // 更新「隐藏后台」按钮文案
                    SetWindowTextW(GetDlgItem(hWnd, IDC_BTN_HIDE_PROGRAM),
                                   g_config.show_tray_icon ? L"隐藏后台" : L"最小化");

                    SetShutdownTimers(); 
                    UpdateStatusText();

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
                            UpdateStatusText();
                        }
                    }

                    if (!immediate_shutdown_triggered_on_save) {
                        MessageBoxW(hWnd, L"设置已保存并应用！", L"提示", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                    }
                    break;
                case IDC_BTN_SHUTDOWN_NOW:
                    StartShutdownProcess(g_config.countdown_seconds);
                    UpdateStatusText();
                    break;
                case IDC_BTN_HIDE_PROGRAM:
                    if (g_config.show_tray_icon) {
                        ShowWindow(hWnd, SW_HIDE);
                    } else {
                        ShowWindow(hWnd, SW_MINIMIZE);
                    }
                    break;
                case IDC_BTN_EXIT_APP:
                    RemoveTrayIcon();
                    DestroyWindow(g_hHiddenWindow); 
                    break;
                case IDM_TRAY_SHOW_MAIN:
                    ShowWindow(g_hMainWindow, SW_RESTORE);
                    SetForegroundWindow(g_hMainWindow);
                    break;
                case IDM_TRAY_SHUTDOWN_NOW:
                    StartShutdownProcess(g_config.countdown_seconds);
                    UpdateStatusText();
                    break;
                case IDM_TRAY_CANCEL_SHUTDOWN:
                    CancelShutdownProcess();
                    UpdateStatusText();
                    break;
                case IDM_TRAY_EXIT:
                    RemoveTrayIcon();
                    DestroyWindow(g_hHiddenWindow);
                    break;
            }
            break;
        }

        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(g_hMainWindow, SW_RESTORE);
                SetForegroundWindow(g_hMainWindow);
            } else if (lParam == WM_RBUTTONUP) {
                ShowTrayContextMenu();
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_TIMER_UPDATE_STATUS) {
                UpdateStatusText();
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(60, 60, 60));       // 深灰，比纯黑柔和
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }

        case WM_CLOSE:
            if (g_config.show_tray_icon) {
                ShowWindow(hWnd, SW_HIDE);
            } else {
                ShowWindow(hWnd, SW_MINIMIZE);
            }
            break;

        case WM_DESTROY:
            RemoveTrayIcon();
            KillTimer(hWnd, IDT_TIMER_UPDATE_STATUS);
            if (g_hAppFont) {
                DeleteObject(g_hAppFont);
                g_hAppFont = NULL;
            }
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// --- Hidden Window Procedure ---
LRESULT CALLBACK HiddenWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            g_hHiddenWindow = hWnd;
            // 状态栏每秒刷新
            SetTimer(hWnd, IDT_TIMER_UPDATE_STATUS, 1000, NULL);
            break;

        case WM_TIMER:
            if (LOWORD(wParam) == IDT_TIMER_CHECK_IDLE) {
                if (g_config.enable_idle_shutdown && !g_is_shutdown_pending) {
                    DWORD idle_time_ms = GetIdleTime();
                    DWORD configured_idle_time_ms = (DWORD)g_config.idle_minutes * 60 * 1000;
                    if (configured_idle_time_ms > 0 && idle_time_ms >= configured_idle_time_ms) {
                        StartShutdownProcess(g_config.countdown_seconds);
                        UpdateStatusText();
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
                        StartShutdownProcess(g_config.countdown_seconds);
                        g_shutdown_executed_today = TRUE;
                        UpdateStatusText();
                    }
                }
            } else if (LOWORD(wParam) == IDT_TIMER_SHUTDOWN_COUNTDOWN) {
                // 系统侧 shutdown -s -t N 已执行关机，这里只清理本地状态
                KillTimer(hWnd, IDT_TIMER_SHUTDOWN_COUNTDOWN);
                g_is_shutdown_pending = FALSE;
                UpdateStatusText();
                PostQuitMessage(0);
            }
            break;

        case WM_ENDSESSION:
            if (wParam == TRUE) {
                KillTimer(hWnd, IDT_TIMER_CHECK_IDLE);
                KillTimer(hWnd, IDT_TIMER_CHECK_TIMED_SHUTDOWN);
                KillTimer(hWnd, IDT_TIMER_SHUTDOWN_COUNTDOWN);
                KillTimer(hWnd, IDT_TIMER_UPDATE_STATUS);
                g_is_shutdown_pending = FALSE;
            }
            return 0;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_DESTROY:
            KillTimer(hWnd, IDT_TIMER_UPDATE_STATUS);
            PostQuitMessage(0);
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

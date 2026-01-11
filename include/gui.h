#ifndef GUI_H
#define GUI_H

#include "common.h"

void ApplyConfigToGUI();
void GetConfigFromGUI();
LRESULT CALLBACK MainWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HiddenWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // GUI_H

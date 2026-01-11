#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

BOOL LoadConfig(const WCHAR* configPath);
BOOL SaveConfig(const WCHAR* configPath);

// Helper functions (exposed in case needed, though mostly internal to config.c)
BOOL GetPrivateProfileBoolW(LPCWSTR lpAppName, LPCWSTR lpKeyName, BOOL fDefault, LPCWSTR lpFileName);
BOOL WritePrivateProfileBoolW(LPCWSTR lpAppName, LPCWSTR lpKeyName, BOOL fValue, LPCWSTR lpFileName);
BOOL WritePrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName, int iValue, LPCWSTR lpFileName);

#endif // CONFIG_H

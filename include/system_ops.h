#ifndef SYSTEM_OPS_H
#define SYSTEM_OPS_H

#include "common.h"

BOOL IsAutorunEnabled();
void SetAutorun(BOOL enable);
// 重命名以避免与 Windows API InitiateShutdown 冲突
void StartShutdownProcess(UINT countdown);
void StopShutdownCountdown();
void SetShutdownTimers();
DWORD GetIdleTime();
void InitializeGetLastInputInfo();
void CleanupGetLastInputInfo();

#endif // SYSTEM_OPS_H

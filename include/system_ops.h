#ifndef SYSTEM_OPS_H
#define SYSTEM_OPS_H

#include "common.h"

BOOL IsAutorunEnabled();
void SetAutorun(BOOL enable);
void InitiateShutdown(UINT countdown);
void StopShutdownCountdown();
void SetShutdownTimers();
DWORD GetIdleTime();
void InitializeGetLastInputInfo();
void CleanupGetLastInputInfo();

#endif // SYSTEM_OPS_H

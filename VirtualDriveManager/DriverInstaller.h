#pragma once
#include <bzscore/status.h>

bool IsDriverInstallationPending(bool *pOldVersionInstalled);
BazisLib::ActionStatus InstallDriver(bool oldVersionInstalled);
BazisLib::ActionStatus UninstallDriver();
BazisLib::ActionStatus StartDriverIfNeeded();
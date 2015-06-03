#pragma once

#include <vector>
#include <bzscore/file.h>

std::vector<int> QueryAvailableReadSpeeds(const BazisLib::String &devicePath, bool *pbDVD);
BazisLib::ActionStatus SetCDReadSpeed(const BazisLib::String &devicePath, unsigned short readSpeed);
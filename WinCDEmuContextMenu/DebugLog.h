#pragma once

#ifdef ENABLE_DEBUG_LOGGING
#include <bzscore/file.h>

void LogDebugLine(BazisLib::DynamicString line);
#define WINCDEMU_LOG_LINE(...) LogDebugLine(BazisLib::String::sFormat(__VA_ARGS__))

#else

#define WINCDEMU_LOG_LINE(...)

#endif

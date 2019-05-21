#pragma once

#include "core/Str.h"

namespace aiModInternal {
	const char* getModuleName();
	[[noreturn]] void reportFatalError0(const char* lineInfo, string error);
	void aiReportImpl(EDebug debug, string reportStr, bool isInternal);
}

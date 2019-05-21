#include <game/StdAfx.h>
#include "Analysis.h"
#include "Utils.h"
#include "Macros.h"

#include <windows.h>

namespace aiModInternal {
	EXTERN_C IMAGE_DOS_HEADER __ImageBase;
	static string calculateModuleName() {
		char DllPath[MAX_PATH] = { 0 };
		GetModuleFileNameA((HINSTANCE)&__ImageBase, DllPath, _countof(DllPath));
		return string(DllPath);
	}
	const char* getModuleName() {
		static const string name = calculateModuleName();
		return name.c_str();
	}

	[[noreturn]] void reportFatalError0(const char* lineInfo, string error) {
		auto msg = str_format("Fatal error in %s %s: %s", getModuleName(), lineInfo, error);
		fprintf(stderr, "%s\n", msg.c_str());
		MessageBoxA(NULL, msg.c_str(), "Reassembly", MB_OK | MB_ICONERROR);
		exit(1);
	}

	ENUM_TO_STR_FN(eDebugName, EDebug, DBG_TYPES);

	void aiReportImpl(EDebug debug, string reportStr, bool isInternal) {
		ASSERT_FATAL(debug || isInternal);
		ASSERT_FATAL(!debug || !isInternal);

		auto name = debug == 0 ? "ANALYSIS" : eDebugName(debug);
		auto moduleName = getModuleName();
		auto str = str_format("[%s] %s - %s", name, moduleName, reportStr.c_str());

		if (!isInternal) {
			auto globals = tryGetGlobals();
			if (globals != NULL && globals->debugRender & debug) {
				::Report(str);
				return;
			}
		}
		printf("%s\n", str.c_str());
	}
}
#include <game/StdAfx.h>
#include "Analysis.h"
#include "Utils.h"
#include "Macros.h"

#include <windows.h>
#include <algorithm>

namespace aiModInternal {
	EXTERN_C IMAGE_DOS_HEADER __ImageBase;
	string getModuleName(HMODULE mod) {
		char DllPath[MAX_PATH] = { 0 };
		GetModuleFileNameA(mod, DllPath, _countof(DllPath));
		auto filename = strrchr(DllPath, '\\');
		return string(filename == NULL ? DllPath : filename + 1);
	}
	const char* getSelfModuleName() {
		static const string name = getModuleName((HINSTANCE) &__ImageBase);
		return name.c_str();
	}

	[[noreturn]] void reportFatalError0(const char* lineInfo, string error) {
		auto msg = str_format("Fatal error in %s %s: %s", getSelfModuleName(), lineInfo, error);
		fprintf(stderr, "%s\n", msg.c_str());
		MessageBoxA(NULL, msg.c_str(), "Reassembly", MB_OK | MB_ICONERROR);

		// We kill ourselves to prevent any error handling from running.
		auto hnd = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, TRUE, GetCurrentProcessId());
		TerminateProcess(hnd, 0);
	}

	ENUM_TO_STR_FN(eDebugName, EDebug, DBG_TYPES);

	static std::once_flag printedGlobalsWarning;
	void aiReportImpl(EDebug debug, string reportStr, bool isInternal, bool alwaysReport) {
		ASSERT_FATAL((debug || isInternal) && !(debug && isInternal));

		auto name = debug == 0 ? "ANALYSIS" : eDebugName(debug);
		auto moduleName = getSelfModuleName();
		auto str = str_format("[%s] %s - %s", name, moduleName, reportStr.c_str());

		if (alwaysReport || isInternal) {
			::Report(str);
			return;
		}
		if (!isInternal) {
			auto globals = tryGetGlobals();
			if (!globals) std::call_once(printedGlobalsWarning, []() { 
				DPRINT_LOW_REPORT("Could not find globals offset. AI mod logging has been disabled.");
			});
			if (globals.has_value() && ((*globals)->debugRender & debug)) {
				::Report(str);
				return;
			}
		}
		printf("\n%s", str.c_str());
	}

	static std::once_flag printedCvarsWarning;
	CVarBase* findCvarImpl(string cvar) {
		if (!cvarIndexLoaded()) {
			std::call_once(printedCvarsWarning, []() {
				DPRINT_LOW_REPORT("Could not find CVarBase::index offset. Assuming all CVars are default.");
			});
			return NULL;
		}
		else {
			auto cvarIndex = CVarBase_index();
			auto result = cvarIndex.find(cvar);
			if (result == cvarIndex.end()) {
				DPRINT_LOW("Could not find cvar %s. Assuming default value.", cvar.c_str());
				return NULL;
			}
			else return result->second;
		}
	}
	void warnCvarType(CVarBase* base, const char* expected) {
		auto name = base->getName();
		auto cvarTypeName = str_demangle(typeid(*base).name());
		DPRINT_LOW("Cvar %s is of type %s, expected CVar<%s>. Assuming default value.", 
				   name.c_str(), cvarTypeName.c_str(), expected);
	}
}
#pragma once

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define LI_STR __FILE__ ":" STRINGIZE(__LINE__)

#define reportFatalError(...) ::aiModInternal::reportFatalError0(LI_STR, str_format(__VA_ARGS__))
#define ASSERT_FATAL(cond) if (!(cond)) ::aiModInternal::reportFatalError0(LI_STR, #cond)

#define DPRINT_LOW(...) ::aiModInternal::aiReportImpl(0, str_format(__VA_ARGS__), true, false)
#define DPRINT_LOW_REPORT(...) ::aiModInternal::aiReportImpl(0, str_format(__VA_ARGS__), true, true)

#define ANALYSIS_TRY(var) if (!(var).has_value()) { \
	DPRINT_LOW("  " #var " could not be determined."); \
	return std::nullopt; \
}
#define ANALYSIS_DBG(format, var) DPRINT_LOW("  " #var " = " format, var)
#define ANALYSIS_DBG_LIST(format, var) { \
	auto listAsStr = ::aiModInternal::formatList(format, var); \
	DPRINT_LOW("  " #var " = %s", listAsStr.c_str()); \
}

#define ENUM_TO_STR_SIMPLE_CASE(s, v) case v: return #s;
#define ENUM_TO_STR_FN(fnName, Type, T) static const char* fnName(Type t) { \
	switch (t.get()) { \
		T(ENUM_TO_STR_SIMPLE_CASE) \
		default: return "UNKNOWN"; \
	} \
}

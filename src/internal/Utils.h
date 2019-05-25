#pragma once

#include <core/Str.h>
#include <game/Save.h>

#include <Windows.h>

namespace aiModInternal {
	string getModuleName(HMODULE mod);
	const char* getSelfModuleName();
	[[noreturn]] void reportFatalError0(const char* lineInfo, string error);
	void aiReportImpl(EDebug debug, string reportStr, bool isInternal, bool alwaysReport);

	template <typename T> string formatList(const char* format, std::vector<T> vector) {
		string str = "[";
		bool hasLast = false;
		for (auto val : vector) {
			if (hasLast) str += ", ";
			str += str_format(format, val);
			hasLast = true;
		}
		str += "]";
		return str;
	}

	CVarBase* findCvarImpl(string cvar);
	void warnCvarType(CVarBase* base, const char* expected);
	template <typename T> T& makeReference(T t) {
		// TODO: Attempt to delete these on DLL unload?
		T* tBox = new T(std::move(t));
		return (T&) tBox;
	}
	template <typename T> T& findCvar(string cvarName, T defaultValue) {
		auto cvar = findCvarImpl(cvarName);
		if (!cvar) return makeReference(defaultValue);
		auto cast = dynamic_cast<CVar<T>*>(cvar);
		if (!cast) {
			auto typeName = TYPE_NAME_S(T);
			warnCvarType(cvar, typeName.c_str());
			return makeReference(defaultValue);
		}
		return (T&) cast->m_vptr;
	}
}

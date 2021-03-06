#include <game/StdAfx.h>
#include <game/AI.h>
#include <game/AI_modapi.h>

// Exported API
extern "C" {
	__declspec(dllexport) void GetApiVersion(int * major, int * minor);
	__declspec(dllexport) bool CreateAiActions(AI* ai);
}

void GetApiVersion(int * major, int * minor) {
	*major = 1;
	*minor = 0;

	printf("\nglobals = %p", &globals);
}

bool CreateAiActions(AI* ai) {
	return true;
}
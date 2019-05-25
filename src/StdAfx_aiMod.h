#pragma once

#include <internal/Utils.h>

#define DPRINT(TYPE, ARGS) aiModInternal::aiReportImpl(DBG_ ## TYPE, str_strip(str_format ARGS), false, false)

extern Globals& globals;

#define DEFINE_CVAR(TYPE, NAME, VALUE) \
    TYPE& NAME = aiModInternal::findCvar<TYPE>(#NAME, VALUE)
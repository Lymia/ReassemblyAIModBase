#pragma once

#include <internal/Utils.h>

#define DPRINT(TYPE, ARGS) aiModInternal::aiReportImpl(DBG_ ## TYPE, str_strip(str_format ARGS), false)

extern Globals& globals;
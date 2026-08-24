// F-DDrace gamecore extras — included from gamecore.h under CONF_FDDRACE_MOD.
// Flag-hook sentinels are reserved for Phase 5/6 (flag entities). Do not snap HookedPlayer >= MAX_CLIENTS.

#ifndef CONF_FDDRACE_MOD
// Mod build disabled.
#else

#include "collision_mod.h"

enum
{
	HOOK_FLAG_RED = MAX_CLIENTS,
	HOOK_FLAG_BLUE = MAX_CLIENTS + 1,
};

#endif

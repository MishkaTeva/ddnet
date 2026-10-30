// F-DDrace gamecore extras — included from gamecore.h under CONF_FDDRACE_MOD.
// Flag-hook sentinels are reserved for Phase 5/6 (flag entities). Do not snap HookedPlayer >= MAX_CLIENTS.

#ifndef CONF_FDDRACE_MOD
// Mod build disabled.
#else

#include "collision_mod.h"

#include <cstring>

class CLockedTune
{
public:
	char m_aParam[64];
	CTuneParam m_Value;

	CLockedTune(const char *pParam, float Value)
	{
		std::memset(m_aParam, 0, sizeof(m_aParam));
		if(pParam)
			std::strncpy(m_aParam, pParam, sizeof(m_aParam) - 1);
		m_Value = Value;
	}

	bool operator==(const CLockedTune &Tune) const
	{
		return std::strncmp(m_aParam, Tune.m_aParam, sizeof(m_aParam)) == 0 && m_Value.Get() == Tune.m_Value.Get();
	}
};
typedef std::vector<CLockedTune> LOCKED_TUNES;

enum
{
	HOOK_FLAG_RED = MAX_CLIENTS,
	HOOK_FLAG_BLUE = MAX_CLIENTS + 1,
};

#endif

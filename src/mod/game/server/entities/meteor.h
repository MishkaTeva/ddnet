#ifndef MOD_GAME_SERVER_ENTITIES_METEOR_H
#define MOD_GAME_SERVER_ENTITIES_METEOR_H

#include "stable_projectile.h"

class CMeteor : public CStableProjectile
{
	vec2 m_Vel;
	int m_Owner;
	bool m_Infinite;
	int m_TuneZone;

public:
	CMeteor(CGameWorld *pGameWorld, vec2 Pos, int Owner, bool Infinite);

	void Reset() override;
	void Tick() override;
};

#endif

#include "meteor.h"

#include <base/math.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CMeteor::CMeteor(CGameWorld *pGameWorld, vec2 Pos, int Owner, bool Infinite) :
	CStableProjectile(pGameWorld, WEAPON_SHOTGUN, Owner, Pos, true)
{
	m_Vel = vec2(0.1f, 0.1f);
	m_Owner = Owner;
	m_Infinite = Infinite;
	m_TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
}

void CMeteor::Reset()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
	if(pChr)
	{
		if(m_Infinite && pChr->GetPlayer() && pChr->GetPlayer()->m_InfMeteors > 0)
			pChr->GetPlayer()->m_InfMeteors--;
		else if(!m_Infinite && pChr->m_Meteors > 0)
			pChr->m_Meteors--;
	}
	CStableProjectile::Reset();
}

void CMeteor::Tick()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
	if(!GameServer()->m_apPlayers[m_Owner] ||
		(!pChr && !m_Infinite) ||
		(m_Infinite && pChr && pChr->GetPlayer() && !pChr->GetPlayer()->m_InfMeteors) ||
		(!m_Infinite && pChr && !pChr->m_Meteors))
	{
		// Avoid double-decrement: mark destroy without Reset's counter tweak when already zeroed
		m_MarkedForDestroy = true;
		return;
	}

	CTuningParams *pTuning = GameWorld()->GetTuning(m_TuneZone);
	const float Friction = pTuning->m_MeteorFriction / 1000000.f;
	const float MaxAccel = pTuning->m_MeteorMaxAccel / 1000.f;
	const float AccelPreserve = pTuning->m_MeteorAccelPreserve / 1000.f;

	if(pChr)
	{
		const vec2 CharPos = pChr->GetPos();
		const float Dist = distance(CharPos, m_Pos);
		if(Dist > 0.001f)
			m_Vel += normalize(CharPos - m_Pos) * (MaxAccel * AccelPreserve / (Dist + AccelPreserve));
	}
	m_Pos += m_Vel;
	m_Vel *= 1.f - Friction;
}

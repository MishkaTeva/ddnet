#include "advanced_entity.h"

#include <algorithm>

#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/teams.h>

CAdvancedEntity::CAdvancedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos, vec2 Size, int Owner, bool CheckDeath) :
	CEntity(pGameWorld, Objtype, true, Pos, (int)std::min(Size.x, Size.y))
{
	m_Pos = Pos;
	m_PrevPos = Pos;
	m_Size = Size;
	m_Owner = Owner;
	m_CheckDeath = CheckDeath;
	m_Gravity = true;
	m_Elasticity = 0.5f;
	m_CheckGameLayerClipped = true;

	if(CCharacter *pOwner = GetOwner())
	{
		m_DDTeam = pOwner->Team();
		m_TeamMask = pOwner->TeamMask();
	}
}

void CAdvancedEntity::Reset()
{
	Destroy();
}

CCharacter *CAdvancedEntity::GetOwner()
{
	return GameServer()->GetPlayerChar(m_Owner);
}

void CAdvancedEntity::Tick()
{
	if(m_Owner >= 0 && (!GameServer()->m_apPlayers[m_Owner] || !GetOwner()))
	{
		// Keep owner id for respawn logic in children; clear team if fully gone.
		if(!GameServer()->m_apPlayers[m_Owner])
		{
			m_Owner = -1;
			m_DDTeam = 0;
		}
	}

	if(CCharacter *pOwner = GetOwner())
	{
		m_DDTeam = pOwner->Team();
		m_TeamMask = pOwner->TeamMask();
	}

	if((m_CheckGameLayerClipped && GameLayerClipped(m_Pos)) ||
		(m_CheckDeath && (Collision()->GetCollisionAt(m_Pos.x, m_Pos.y) == TILE_DEATH ||
				   Collision()->GetFrontCollisionAt(m_Pos.x, m_Pos.y) == TILE_DEATH)))
	{
		Reset();
	}
}

bool CAdvancedEntity::IsGrounded()
{
	const float R = GetProximityRadius();
	return Collision()->CheckPoint(m_Pos.x + R, m_Pos.y + R + 5) ||
	       Collision()->CheckPoint(m_Pos.x - R, m_Pos.y + R + 5);
}

void CAdvancedEntity::HandleDropped()
{
	if(m_Gravity)
	{
	CTuningParams *pTuning = GetTuning(m_TuneZone);
	m_Vel.y += pTuning->m_Gravity;
	}

	Collision()->MoveBox(&m_Pos, &m_Vel, m_Size, vec2(m_Elasticity, m_Elasticity));
	if(IsGrounded())
		m_Vel.x *= 0.75f;
}

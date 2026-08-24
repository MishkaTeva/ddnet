#include "flyingpoint.h"

#include <algorithm>

#include <base/math.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CFlyingPoint::CFlyingPoint(CGameWorld *pGameWorld, vec2 Pos, int To, int Owner, vec2 InitialVel, vec2 ToPos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_FLYINGPOINT, true, Pos)
{
	m_InitialVel = InitialVel;
	m_Owner = Owner;
	m_To = To;
	m_ToPos = ToPos;
	m_TeamMask = CClientMask().set();
	GameWorld()->InsertEntity(this);
}

void CFlyingPoint::Reset()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_To);
	vec2 Pos = pChr ? pChr->GetPos() : m_Pos;
	int Id = pChr ? m_To : m_Owner;
	GameServer()->CreateDeath(Pos, Id);
	m_MarkedForDestroy = true;
}

void CFlyingPoint::Tick()
{
	vec2 ToPos = m_ToPos;
	if(m_To != -1)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_To);
		if(!pChr)
		{
			Reset();
			return;
		}
		ToPos = pChr->GetPos();
		m_TeamMask = pChr->TeamMask();
	}
	else if(m_Owner != -1)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
		if(pChr)
			m_TeamMask = pChr->TeamMask();
	}
	else
		m_TeamMask = CClientMask().set();

	float Dist = distance(m_Pos, ToPos);
	if(Dist < 24.0f)
	{
		Reset();
		return;
	}

	vec2 Dir = normalize(ToPos - m_Pos);
	m_Pos += Dir * std::clamp(Dist, 0.0f, 16.0f) * (1.0f - m_InitialAmount) + m_InitialVel * m_InitialAmount;
	m_InitialAmount *= 0.98f;
}

void CFlyingPoint::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !GetId().has_value())
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	CNetObj_Projectile Proj = {};
	Proj.m_X = round_to_int(m_Pos.x);
	Proj.m_Y = round_to_int(m_Pos.y);
	Proj.m_VelX = 0;
	Proj.m_VelY = 0;
	Proj.m_StartTick = Server()->Tick();
	Proj.m_Type = WEAPON_HAMMER;
	Server()->SnapNewItem(GetId().value(), Proj);
}

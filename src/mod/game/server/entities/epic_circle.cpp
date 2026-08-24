#include "epic_circle.h"

#include <base/math.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CEpicCircle::CEpicCircle(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_EPIC_CIRCLE, false, Pos)
{
	m_Owner = Owner;
	m_TeamMask = CClientMask().set();
	for(auto &Id : m_aId)
		Id = Server()->SnapNewId();
	GameWorld()->InsertEntity(this);
}

CEpicCircle::~CEpicCircle()
{
	for(auto &Id : m_aId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CEpicCircle::Reset()
{
	m_MarkedForDestroy = true;
}

void CEpicCircle::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner || !pOwner->m_EpicCircle)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
	m_TeamMask = pOwner->TeamMask();

	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		float Rad = 16.0f * powf(sinf(Server()->Tick() / 30.0f), 3) + 50.f;
		float TurnFac = 0.025f;
		m_aRotatePos[i].x = cosf(2 * pi * (i / (float)MAX_PARTICLES) + Server()->Tick() * TurnFac) * Rad;
		m_aRotatePos[i].y = sinf(2 * pi * (i / (float)MAX_PARTICLES) + Server()->Tick() * TurnFac) * Rad;
	}
}

void CEpicCircle::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;
	if(pOwner && pOwner->GetPlayer() && pOwner->GetPlayer()->IsPaused())
		return;

	for(int i = 0; i < MAX_PARTICLES; i++)
	{
		if(!m_aId[i].has_value())
			continue;
		CNetObj_Projectile Proj = {};
		Proj.m_X = round_to_int(m_Pos.x + m_aRotatePos[i].x);
		Proj.m_Y = round_to_int(m_Pos.y + m_aRotatePos[i].y);
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = 0;
		Proj.m_Type = WEAPON_HAMMER;
		Server()->SnapNewItem(m_aId[i].value(), Proj);
	}
}

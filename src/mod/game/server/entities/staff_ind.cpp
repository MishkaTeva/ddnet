#include "staff_ind.h"

#include <algorithm>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CStaffInd::CStaffInd(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_STAFF_IND, false, Pos)
{
	m_Owner = Owner;
	m_TeamMask = CClientMask().set();
	for(auto &Id : m_aId)
		Id = Server()->SnapNewId();
	GameWorld()->InsertEntity(this);
}

CStaffInd::~CStaffInd()
{
	for(auto &Id : m_aId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CStaffInd::Reset()
{
	m_MarkedForDestroy = true;
}

void CStaffInd::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner || !pOwner->m_StaffInd)
	{
		Reset();
		return;
	}

	m_TeamMask = pOwner->TeamMask();
	m_Pos = pOwner->GetPos();
	m_aPos[ARMOR] = vec2(m_Pos.x, m_Pos.y - 70.f);

	if(m_BallFirst)
	{
		m_Dist += 0.9f;
		if(m_Dist > 25.f)
			m_BallFirst = false;
	}
	else
	{
		m_Dist -= 0.9f;
		if(m_Dist < -25.f)
			m_BallFirst = true;
	}

	m_aPos[BALL] = vec2(m_Pos.x + m_Dist, m_aPos[ARMOR].y);
}

void CStaffInd::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;
	if(pOwner && pOwner->GetPlayer() && pOwner->GetPlayer()->IsPaused())
		return;

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	if(m_aId[ARMOR].has_value())
		GameServer()->SnapPickup(Context, m_aId[ARMOR].value(), m_aPos[ARMOR], POWERUP_ARMOR, 0, 0, 0);

	const int BallId = (m_BallFirst ? m_aId[BALL_FRONT] : m_aId[BALL]).value_or(-1);
	if(BallId < 0)
		return;
	GameServer()->SnapLaserObject(Context, BallId, m_aPos[BALL], m_aPos[BALL], Server()->Tick(), -1, LASERTYPE_RIFLE, 0, 0);
}

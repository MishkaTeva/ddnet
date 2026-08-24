#include "trail.h"

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CTrail::CTrail(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_TRAIL, false, Pos)
{
	m_Owner = Owner;
	for(auto &Id : m_aId)
		Id = Server()->SnapNewId();
	GameWorld()->InsertEntity(this);
}

CTrail::~CTrail()
{
	for(auto &Id : m_aId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CTrail::Reset()
{
	m_MarkedForDestroy = true;
}

void CTrail::Tick()
{
	if(m_Owner != -1)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
		CPlayer *pPlayer = GameServer()->m_apPlayers[m_Owner];
		if(!pPlayer || (pChr && !pChr->m_Trail))
		{
			Reset();
			return;
		}
		if(!pChr)
		{
			m_Initialized = false;
			m_TrailHistory.clear();
			m_TrailHistoryLength = 0.f;
			return;
		}
		m_Pos = pChr->GetPos();
	}

	if(!m_Initialized)
	{
		m_TrailHistory.clear();
		m_TrailHistory.emplace_front(m_Pos, 0.0f);
		m_TrailHistory.emplace_front(m_Pos, NUM_TRAILS * TRAIL_DIST);
		m_TrailHistoryLength = NUM_TRAILS * TRAIL_DIST;
		m_Initialized = true;
	}

	vec2 FrontPos = m_TrailHistory.front().m_Pos;
	if(FrontPos != m_Pos)
	{
		float FrontLength = distance(m_Pos, FrontPos);
		m_TrailHistory.emplace_front(m_Pos, FrontLength);
		m_TrailHistoryLength += FrontLength;
	}

	while(true)
	{
		float LastDist = m_TrailHistory.back().m_Dist;
		if(m_TrailHistoryLength - LastDist >= NUM_TRAILS * TRAIL_DIST)
		{
			m_TrailHistory.pop_back();
			m_TrailHistoryLength -= LastDist;
		}
		else
			break;
	}

	int HistoryPos = 0;
	float HistoryPosLength = 0.0f;
	float AdditionalLength = 0.0f;
	for(int i = 0; i < NUM_TRAILS; i++)
	{
		float Length = (i + 1) * TRAIL_DIST;
		float NextDist = 0.0f;
		while(true)
		{
			if((unsigned int)HistoryPos >= m_TrailHistory.size())
			{
				m_TrailHistoryLength = 0.0f;
				for(const auto &Point : m_TrailHistory)
					m_TrailHistoryLength += Point.m_Dist;
				return;
			}
			NextDist = m_TrailHistory[HistoryPos].m_Dist;
			if(Length <= HistoryPosLength + NextDist)
			{
				AdditionalLength = Length - HistoryPosLength;
				break;
			}
			HistoryPos += 1;
			HistoryPosLength += NextDist;
			AdditionalLength = 0.f;
		}
		vec2 Pos = m_TrailHistory[HistoryPos].m_Pos;
		if((unsigned int)HistoryPos + 1 < m_TrailHistory.size() && NextDist > 0.f)
			Pos += (m_TrailHistory[HistoryPos + 1].m_Pos - Pos) * (AdditionalLength / NextDist);
		m_aPos[i] = Pos;
	}
}

void CTrail::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !m_Initialized)
		return;
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(pOwner && pOwner->GetPlayer() && pOwner->GetPlayer()->IsPaused())
		return;

	for(int i = 0; i < NUM_TRAILS; i++)
	{
		if(!m_aId[i].has_value())
			continue;
		CNetObj_Projectile Proj = {};
		Proj.m_X = round_to_int(m_aPos[i].x);
		Proj.m_Y = round_to_int(m_aPos[i].y);
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = 0;
		Proj.m_Type = WEAPON_SHOTGUN;
		Server()->SnapNewItem(m_aId[i].value(), Proj);
	}
}

#include "lovely.h"

#include <cstdlib>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CLovely::CLovely(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LOVELY, false, Pos)
{
	m_Owner = Owner;
	m_SpawnDelay = 0;
	m_TeamMask = CClientMask().set();
	for(auto &Heart : m_aLovelyData)
		Heart.m_Id = Server()->SnapNewId();
	GameWorld()->InsertEntity(this);
}

CLovely::~CLovely()
{
	for(auto &Heart : m_aLovelyData)
	{
		if(Heart.m_Id.has_value())
			Server()->SnapFreeId(Heart.m_Id.value());
	}
}

void CLovely::Reset()
{
	m_MarkedForDestroy = true;
}

void CLovely::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner || !pOwner->m_Lovely)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
	m_TeamMask = pOwner->TeamMask();

	m_SpawnDelay--;
	if(m_SpawnDelay <= 0)
	{
		SpawnNewHeart();
		const int SpawnTime = 45;
		m_SpawnDelay = Server()->TickSpeed() - (rand() % (SpawnTime - (SpawnTime - 10) + 1) + (SpawnTime - 10));
	}

	for(auto &Heart : m_aLovelyData)
	{
		if(Heart.m_Lifespan == -1)
			continue;
		Heart.m_Lifespan--;
		Heart.m_Pos.y -= 5.f;
		if(Heart.m_Lifespan == 0 || GameServer()->Collision()->TestBox(Heart.m_Pos, vec2(14.f, 14.f)))
			Heart.m_Lifespan = -1;
	}
}

void CLovely::SpawnNewHeart()
{
	for(auto &Heart : m_aLovelyData)
	{
		if(Heart.m_Lifespan > 0)
			continue;
		CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
		if(!pOwner)
			return;
		Heart.m_Lifespan = Server()->TickSpeed() / 2;
		Heart.m_Pos = vec2(pOwner->GetPos().x + (rand() % 50 - 25), pOwner->GetPos().y - 30);
		pOwner->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
		break;
	}
}

void CLovely::Snap(int SnappingClient)
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

	for(auto &Heart : m_aLovelyData)
	{
		if(Heart.m_Lifespan == -1 || !Heart.m_Id.has_value())
			continue;
		GameServer()->SnapPickup(Context, Heart.m_Id.value(), Heart.m_Pos, POWERUP_HEALTH, 0, 0, 0);
	}
}

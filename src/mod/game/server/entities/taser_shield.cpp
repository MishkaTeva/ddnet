#include "taser_shield.h"

#include <cstdlib>

#include <base/math.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CTaserShield::CTaserShield(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_TASER_SHIELD, false, Pos)
{
	m_Owner = Owner;
	m_SpawnDelay = 0;
	m_TeamMask = CClientMask().set();
	for(auto &Shield : m_aShieldData)
	{
		Shield.m_Id = Server()->SnapNewId();
		Shield.m_Lifespan = -1;
		Shield.m_Used = false;
		Shield.m_Pos = Pos;
	}
	GameWorld()->InsertEntity(this);
}

CTaserShield::~CTaserShield()
{
	for(auto &Shield : m_aShieldData)
	{
		if(Shield.m_Id.has_value())
			Server()->SnapFreeId(Shield.m_Id.value());
	}
}

void CTaserShield::Reset()
{
	m_MarkedForDestroy = true;
}

void CTaserShield::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
	m_TeamMask = pOwner->TeamMask();

	m_SpawnDelay--;
	if(m_SpawnDelay <= 0)
	{
		bool AllUsed = true;
		for(auto &Shield : m_aShieldData)
		{
			if(!Shield.m_Used || Shield.m_Lifespan != -1)
				AllUsed = false;
		}
		if(AllUsed)
		{
			Reset();
			return;
		}

		SpawnNewShield();
		constexpr int SpawnTime = 45;
		m_SpawnDelay = (float)(Server()->TickSpeed() - (rand() % (SpawnTime - (SpawnTime - 5) + 1) + (SpawnTime - 5)));
	}

	for(auto &Shield : m_aShieldData)
	{
		if(Shield.m_Lifespan == -1)
			continue;

		Shield.m_Lifespan--;
		Shield.m_Pos.y -= 5.f;

		if(Shield.m_Lifespan == 0 || GameServer()->Collision()->TestBox(Shield.m_Pos, vec2(14.f, 14.f)))
			Shield.m_Lifespan = -1;
	}
}

void CTaserShield::SpawnNewShield()
{
	for(auto &Shield : m_aShieldData)
	{
		if(Shield.m_Lifespan > 0 || Shield.m_Used)
			continue;

		CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
		if(!pOwner)
			return;
		Shield.m_Lifespan = (float)(Server()->TickSpeed() / 2);
		Shield.m_Pos = vec2(pOwner->GetPos().x + (rand() % 50 - 25), pOwner->GetPos().y - 30);
		Shield.m_Used = true;
		pOwner->SetEmote(EMOTE_PAIN, Server()->Tick() + Server()->TickSpeed());
		GameServer()->CreateSound(Shield.m_Pos, SOUND_PICKUP_ARMOR, m_TeamMask);
		break;
	}
}

void CTaserShield::Snap(int SnappingClient)
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

	for(auto &Shield : m_aShieldData)
	{
		if(Shield.m_Lifespan == -1 || !Shield.m_Id.has_value())
			continue;
		GameServer()->SnapPickup(Context, Shield.m_Id.value(), Shield.m_Pos, POWERUP_ARMOR, 0, 0, 0);
	}
}

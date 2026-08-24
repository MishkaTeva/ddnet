#include "pickup_drop.h"

#include <algorithm>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CPickupDrop::CPickupDrop(CGameWorld *pGameWorld, vec2 Pos, int Type, int Owner, float Direction, int Lifetime, int Weapon) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP_DROP, true, Pos, PHYS_SIZE)
{
	m_Type = Type;
	m_Weapon = Weapon;
	m_Owner = Owner;
	m_Lifetime = Server()->TickSpeed() * Lifetime;
	m_Vel = vec2(5.f * Direction, Direction == 0.f ? 0.f : -5.f);
	m_PickupDelay = Server()->TickSpeed() * 2;
	m_TeamMask = CClientMask().set();

	CCharacter *pOwner = GameServer()->GetPlayerChar(Owner);
	if(pOwner)
		m_TeamMask = pOwner->TeamMask();

	GameWorld()->InsertEntity(this);
}

void CPickupDrop::Reset()
{
	GameServer()->CreateDeath(m_Pos, m_Owner, m_TeamMask);
	m_MarkedForDestroy = true;
}

void CPickupDrop::Tick()
{
	if(m_MarkedForDestroy)
		return;

	if(m_Owner >= 0 && !GameServer()->m_apPlayers[m_Owner] && g_Config.m_SvDestroyDropsOnLeave)
	{
		Reset();
		return;
	}

	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);
	if(pOwnerChr)
		m_TeamMask = pOwnerChr->TeamMask();

	m_Lifetime--;
	if(m_Lifetime <= 0)
	{
		Reset();
		return;
	}

	if(m_PickupDelay > 0)
		m_PickupDelay--;

	m_Vel.y += GameWorld()->GetTuning(0)->m_Gravity;
	Collision()->MoveBox(&m_Pos, &m_Vel, vec2(PHYS_SIZE * 2.f, PHYS_SIZE * 2.f), vec2(0.5f, 0.5f));

	Pickup();
}

int CPickupDrop::IsCharacterNear()
{
	CEntity *apEnts[MAX_CLIENTS];
	const int Num = GameWorld()->FindEntities(m_Pos, 20.0f, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for(int i = 0; i < Num; i++)
	{
		auto *pChr = static_cast<CCharacter *>(apEnts[i]);
		if(!pChr || !pChr->IsAlive() || !pChr->GetPlayer())
			continue;

		if(m_PickupDelay > 0 && pChr->GetPlayer()->GetCid() == m_Owner)
			continue;

		if(m_Type == POWERUP_WEAPON)
		{
			if(m_Weapon < 0 || m_Weapon >= NUM_WEAPONS)
				continue;
			if(pChr->GetWeaponGot(m_Weapon))
				continue;
		}
		else if(m_Type == POWERUP_HEALTH)
		{
			if(!pChr->IncreaseHealth(1))
				continue;
		}
		else if(m_Type == POWERUP_ARMOR)
		{
			if(!pChr->IncreaseArmor(1))
				continue;
		}
		else
			continue;

		return pChr->GetPlayer()->GetCid();
	}

	return -1;
}

void CPickupDrop::Pickup()
{
	const int Id = IsCharacterNear();
	if(Id < 0)
		return;

	CCharacter *pChr = GameServer()->GetPlayerChar(Id);
	if(!pChr)
		return;

	if(m_Type == POWERUP_WEAPON)
	{
		pChr->GiveWeapon(m_Weapon);
		GameServer()->SendWeaponPickup(Id, m_Weapon);
		if(m_Weapon == WEAPON_SHOTGUN || m_Weapon == WEAPON_LASER)
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, pChr->TeamMask());
		else if(m_Weapon == WEAPON_GRENADE)
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, pChr->TeamMask());
		else
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());
	}
	else if(m_Type == POWERUP_HEALTH)
		GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, pChr->TeamMask());
	else if(m_Type == POWERUP_ARMOR)
		GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, pChr->TeamMask());

	m_MarkedForDestroy = true;
}

void CPickupDrop::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !GetId().has_value())
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	const int SubType = (m_Type == POWERUP_WEAPON) ? m_Weapon : 0;
	GameServer()->SnapPickup(Context, GetId().value(), m_Pos, m_Type, SubType, 0, 0);
}

#include "custom_projectile.h"

#include <engine/shared/config.h>

#include <generated/protocol.h>
#include <generated/server_data.h>

#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CCustomProjectile::CCustomProjectile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir, bool Freeze,
	bool Explosive, bool Unfreeze, bool Bloody, bool Ghost, bool Spooky, int Type, float Lifetime, float Accel, float Speed) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_CUSTOM_PROJECTILE, true, Pos)
{
	m_Owner = Owner;
	m_Pos = Pos;
	const float Len = length(Dir);
	m_Direction = Len > 0.001f ? Dir / Len : vec2(1.f, 0.f);
	m_Core = m_Direction * Speed;
	m_Freeze = Freeze;
	m_Explosive = Explosive;
	m_Unfreeze = Unfreeze;
	m_Bloody = Bloody;
	m_Ghost = Ghost;
	m_Spooky = Spooky;
	m_EvalTick = Server()->Tick();
	m_LifeTime = Server()->TickSpeed() * Lifetime;
	m_Type = Type;
	m_Accel = Accel;
	m_PrevPos = m_Pos;
	m_pOwner = nullptr;
	m_TeamMask = CClientMask().set();
	m_CollisionState = CUSTOM_PROJ_NOT_COLLIDED;

	GameWorld()->InsertEntity(this);
}

void CCustomProjectile::Reset()
{
	m_MarkedForDestroy = true;
}

int CCustomProjectile::DamageForType() const
{
	const int Weapon = (m_Type >= 0 && m_Type < NUM_WEAPONS) ? m_Type : WEAPON_GUN;
	return g_pData->m_Weapons.m_aId[Weapon].m_Damage;
}

void CCustomProjectile::Tick()
{
	if(m_MarkedForDestroy)
		return;

	m_pOwner = GameServer()->GetPlayerChar(m_Owner);

	if(m_Owner >= 0 && !m_pOwner && g_Config.m_SvDestroyBulletsOnDeath)
	{
		Reset();
		return;
	}

	m_TeamMask = m_pOwner ? m_pOwner->TeamMask() : CClientMask().set();

	m_LifeTime--;
	if(m_LifeTime <= 0)
	{
		Reset();
		return;
	}

	Move();
	HitCharacter();
	if(m_MarkedForDestroy)
		return;

	if(GameServer()->Collision()->IsSolid(round_to_int(m_Pos.x), round_to_int(m_Pos.y)))
	{
		if(m_Explosive)
		{
			const int Team = m_pOwner ? m_pOwner->Team() : -1;
			GameServer()->CreateExplosion(m_Pos, m_Owner, (m_Type >= 0 && m_Type < NUM_WEAPONS) ? m_Type : WEAPON_GUN, m_Owner == -1, Team, m_TeamMask);
			GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
		}

		if(m_Bloody)
		{
			if(m_Ghost && m_CollisionState == CUSTOM_PROJ_COLLIDED_ONCE)
			{
				if(Server()->Tick() % 10 == 0)
					GameServer()->CreateDeath(m_PrevPos, m_Owner, m_TeamMask);
			}
			else
				GameServer()->CreateDeath(m_PrevPos, m_Owner, m_TeamMask);
		}

		if(m_CollisionState == CUSTOM_PROJ_NOT_COLLIDED)
			m_CollisionState = CUSTOM_PROJ_COLLIDED_ONCE;

		if(m_CollisionState == CUSTOM_PROJ_COLLIDED_TWICE || !m_Ghost)
		{
			Reset();
			return;
		}
	}
	else if(m_CollisionState == CUSTOM_PROJ_COLLIDED_ONCE)
		m_CollisionState = CUSTOM_PROJ_COLLIDED_TWICE;

	const int MapIndex = GameServer()->Collision()->GetIndex(m_PrevPos, m_Pos);
	int Tele = 0;
	if(g_Config.m_SvOldTeleportWeapons)
		Tele = GameServer()->Collision()->IsTeleport(MapIndex);
	else
		Tele = GameServer()->Collision()->IsTeleportWeapon(MapIndex);
	if(Tele && !GameServer()->Collision()->TeleOuts(Tele - 1).empty())
	{
		const int Out = GameWorld()->m_Core.RandomOr0((int)GameServer()->Collision()->TeleOuts(Tele - 1).size());
		m_Pos = GameServer()->Collision()->TeleOuts(Tele - 1)[Out];
		m_EvalTick = Server()->Tick();
	}

	m_PrevPos = m_Pos;
}

void CCustomProjectile::Move()
{
	m_Pos += m_Core;
	m_Core *= m_Accel;
}

void CCustomProjectile::HitCharacter()
{
	vec2 NewPos = m_Pos + m_Core;
	CCharacter *pHit = GameWorld()->IntersectCharacter(m_PrevPos, NewPos, 6.0f, NewPos, m_pOwner, m_Owner);
	if(!pHit || !pHit->GetPlayer())
		return;

	if(m_Owner >= 0 && !pHit->CanCollide(m_Owner))
		return;

	if(m_Bloody)
		GameServer()->CreateDeath(pHit->GetPos(), pHit->GetPlayer()->GetCid(), m_TeamMask);

	if(m_Freeze)
		pHit->Freeze();
	else if(m_Unfreeze)
		pHit->Unfreeze();

	const int Weapon = (m_Type >= 0 && m_Type < NUM_WEAPONS) ? m_Type : WEAPON_GUN;
	if(m_Explosive)
	{
		GameServer()->CreateExplosion(m_Pos, m_Owner, Weapon, m_Owner == -1, pHit->Team(), m_TeamMask);
		GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
	}
	else
		pHit->TakeDamage(vec2(0, 0), DamageForType(), m_Owner, Weapon);

	if(m_Type == CUSTOM_PROJ_TYPE_HEART)
	{
		pHit->SetEmote(EMOTE_HAPPY, Server()->Tick() + 2 * Server()->TickSpeed());
		GameServer()->SendEmoticon(pHit->GetPlayer()->GetCid(), EMOTICON_HEARTS, -1);
	}
	else if(m_Spooky)
	{
		pHit->SetEmote(EMOTE_SURPRISE, Server()->Tick() + 2 * Server()->TickSpeed());
		GameServer()->SendEmoticon(pHit->GetPlayer()->GetCid(), EMOTICON_GHOST, -1);
	}

	Reset();
}

void CCustomProjectile::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !GetId().has_value())
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	if(m_Type == CUSTOM_PROJ_TYPE_HEART)
	{
		GameServer()->SnapPickup(Context, GetId().value(), m_Pos, POWERUP_HEALTH, 0, 0, 0);
		return;
	}

	if(m_Type == CUSTOM_PROJ_TYPE_PLASMA || m_Type == WEAPON_GUN || m_Type == WEAPON_LASER)
	{
		GameServer()->SnapLaserObject(Context, GetId().value(), m_Pos, m_Pos, m_EvalTick, m_Owner, LASERTYPE_RIFLE, 0, 0);
		return;
	}

	CNetObj_Projectile Proj = {};
	Proj.m_X = (int)m_Pos.x;
	Proj.m_Y = (int)m_Pos.y;
	Proj.m_VelX = 0;
	Proj.m_VelY = 0;
	Proj.m_StartTick = m_EvalTick;
	Proj.m_Type = (m_Type >= 0 && m_Type < NUM_WEAPONS) ? m_Type : WEAPON_GUN;
	Server()->SnapNewItem(GetId().value(), Proj);
}

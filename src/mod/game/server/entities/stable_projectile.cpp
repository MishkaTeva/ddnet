#include "stable_projectile.h"

#include <base/math.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CStableProjectile::CStableProjectile(CGameWorld *pGameWorld, int Type, int Owner, vec2 Pos, bool HideOnSpec, bool OnlyShowOwner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_STABLE_PROJECTILE, true, Pos)
{
	// Map custom weapon subtypes to vanilla projectile types when needed later.
	m_Type = Type;
	if(m_Type < 0 || m_Type > WEAPON_NINJA)
		m_Type = WEAPON_GUN;

	m_Pos = Pos;
	m_LastResetPos = Pos;
	m_Owner = Owner;
	m_HideOnSpec = HideOnSpec;
	m_LastResetTick = Server()->Tick();
	m_CalculatedVel = false;
	m_TeamMask = CClientMask();
	m_OnlyShowOwner = OnlyShowOwner;
	m_VelX = 0;
	m_VelY = 0;

	GameWorld()->InsertEntity(this);
}

void CStableProjectile::Reset()
{
	m_MarkedForDestroy = true;
}

void CStableProjectile::TickDeferred()
{
	if(Server()->Tick() % 4 == 1)
	{
		m_LastResetPos = m_Pos;
		m_LastResetTick = Server()->Tick();
	}
	m_CalculatedVel = false;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	m_TeamMask = pOwner ? pOwner->TeamMask() : CClientMask().set();
}

void CStableProjectile::CalculateVel()
{
	const float Time = (Server()->Tick() - m_LastResetTick) / (float)Server()->TickSpeed();
	if(Time <= 0.0001f)
	{
		m_VelX = 0;
		m_VelY = 0;
		m_CalculatedVel = true;
		return;
	}

	float Curvature = 0;
	float Speed = 0;

	CTuningParams *pTuning = GameServer()->GlobalTuning();
	switch(m_Type)
	{
	case WEAPON_GRENADE:
		Curvature = pTuning->m_GrenadeCurvature;
		Speed = pTuning->m_GrenadeSpeed;
		break;
	case WEAPON_SHOTGUN:
		Curvature = pTuning->m_ShotgunCurvature;
		Speed = pTuning->m_ShotgunSpeed;
		break;
	case WEAPON_GUN:
	default:
		Curvature = pTuning->m_GunCurvature;
		Speed = pTuning->m_GunSpeed;
		break;
	}

	if(Speed <= 0.0001f)
	{
		m_VelX = 0;
		m_VelY = 0;
	}
	else
	{
		m_VelX = (int)(((m_Pos.x - m_LastResetPos.x) / Time / Speed) * 100);
		m_VelY = (int)(((m_Pos.y - m_LastResetPos.y) / Time / Speed - Time * Speed * Curvature / 10000) * 100);
	}

	m_CalculatedVel = true;
}

void CStableProjectile::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if(m_Owner != -1 && !GameServer()->m_apPlayers[m_Owner])
	{
		Reset();
		return;
	}

	if(m_OnlyShowOwner && SnappingClient != m_Owner)
		return;

	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(m_HideOnSpec && pOwner && pOwner->GetPlayer() && pOwner->GetPlayer()->IsPaused())
		return;

	if(!GetId().has_value())
		return;

	if(!m_CalculatedVel)
		CalculateVel();

	CNetObj_Projectile Proj = {};
	Proj.m_X = round_to_int(m_LastResetPos.x);
	Proj.m_Y = round_to_int(m_LastResetPos.y);
	Proj.m_VelX = m_VelX;
	Proj.m_VelY = m_VelY;
	Proj.m_StartTick = m_LastResetTick;
	Proj.m_Type = m_Type;
	Server()->SnapNewItem(GetId().value(), Proj);
}

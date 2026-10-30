#include "missile.h"

#include <base/math.h>
#include <base/vmath.h>

#include <cstdlib>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CMissile::CMissile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Direction, int SpanTicks) :
	CMissile(pGameWorld, Owner, Pos, Direction * 12.f, Direction, SpanTicks)
{
}

CMissile::CMissile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Vel, vec2 Direction, int SpanTicks) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_MISSILE, true, Pos, 14)
{
	m_Owner = Owner;
	m_Direction = Direction;
	if(length(m_Direction) < 0.001f)
		m_Direction = vec2(1, 0);
	else
		m_Direction = normalize(m_Direction);
	m_Vel = Vel;
	if(length(m_Vel) < 0.001f)
		m_Vel = m_Direction * 12.f;
	m_LifeSpan = SpanTicks;
	m_IgnitionTime = Server()->TickSpeed() / 4;
	for(int i = 0; i < NUM_SPARKS; i++)
	{
		m_aSparkId[i] = Server()->SnapNewId();
		m_aSparkLife[i] = 0;
	}
	GameWorld()->InsertEntity(this);
}

CMissile::~CMissile()
{
	for(int i = 0; i < NUM_SPARKS; i++)
	{
		if(m_aSparkId[i].has_value())
			Server()->SnapFreeId(*m_aSparkId[i]);
	}
}

void CMissile::TriggerExplosions()
{
	if(m_ExplosionsLeft >= 0)
		return;
	m_ExplosionsLeft = 3;
	GameServer()->CreateExplosion(m_Pos, m_Owner, WEAPON_GRENADE, true, -1, CClientMask().set());
	GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE);
}

void CMissile::Tick()
{
	if(m_ExplosionsLeft >= 0)
	{
		if(m_ExplosionsLeft-- <= 0)
			m_MarkedForDestroy = true;
		return;
	}

	if(m_IgnitionTime > 0)
		m_IgnitionTime--;
	else
		m_Vel = m_Direction * 14.f;

	m_LifeSpan--;
	vec2 NewPos = m_Pos + m_Vel;
	if(m_LifeSpan <= 0 || Collision()->IntersectLine(m_Pos, NewPos, &NewPos, nullptr))
	{
		m_Pos = NewPos;
		TriggerExplosions();
		return;
	}

	CCharacter *apHit[MAX_CLIENTS];
	const int Num = GameWorld()->FindEntities(NewPos, 20.f, (CEntity **)apHit, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		if(!apHit[i] || apHit[i]->GetPlayer()->GetCid() == m_Owner)
			continue;
		m_Pos = NewPos;
		TriggerExplosions();
		return;
	}

	m_Pos = NewPos;

	if(m_IgnitionTime <= 0)
	{
		for(int i = 0; i < NUM_SPARKS; i++)
		{
			if(m_aSparkLife[i] <= 0)
			{
				m_aSparkPos[i] = m_Pos + vec2((float)(rand() % 21 - 10), (float)(rand() % 21 - 10));
				m_aSparkLife[i] = 4 + rand() % 6;
			}
			else
			{
				m_aSparkPos[i] -= m_Direction * 2.f;
				m_aSparkLife[i]--;
			}
		}
	}
}

void CMissile::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !GetId().has_value())
		return;

	CNetObj_Projectile Proj = {};
	Proj.m_X = round_to_int(m_Pos.x);
	Proj.m_Y = round_to_int(m_Pos.y);
	Proj.m_VelX = round_to_int(m_Direction.x * 100.f);
	Proj.m_VelY = round_to_int(m_Direction.y * 100.f);
	Proj.m_Type = WEAPON_GRENADE;
	Proj.m_StartTick = Server()->Tick();
	Server()->SnapNewItem(*GetId(), Proj);

	for(int i = 0; i < NUM_SPARKS; i++)
	{
		if(m_aSparkLife[i] <= 0 || !m_aSparkId[i].has_value())
			continue;
		CNetObj_Projectile Spark = {};
		Spark.m_X = round_to_int(m_aSparkPos[i].x);
		Spark.m_Y = round_to_int(m_aSparkPos[i].y);
		Spark.m_VelX = 0;
		Spark.m_VelY = 0;
		Spark.m_Type = WEAPON_HAMMER;
		Spark.m_StartTick = 0;
		Server()->SnapNewItem(*m_aSparkId[i], Spark);
	}
}

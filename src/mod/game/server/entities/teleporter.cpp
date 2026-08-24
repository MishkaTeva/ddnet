#include "teleporter.h"

#include <algorithm>

#include <base/math.h>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/mapitems.h>
#include <game/server/gamecontext.h>

static float s_CurrentDist = 0.f;
static bool s_Vertical = false;
static int64_t s_LastProcessTick = 0;

CTeleporter::CTeleporter(CGameWorld *pGameWorld, vec2 Pos, int Type, int Number, bool Collision) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_TELEPORTER, false, Pos, 14)
{
	m_Type = Type;
	m_Number = Number;
	m_Collision = Collision;
	m_Snap.m_Pos = m_Pos;
	m_Snap.m_Time = 0.f;
	m_Snap.m_LastTime = (float)Server()->Tick();

	for(auto &Id : m_aId)
		Id = Server()->SnapNewId();

	ResetCollision(false);
	GameWorld()->InsertEntity(this);
}

CTeleporter::~CTeleporter()
{
	ResetCollision(true);
	for(auto &Id : m_aId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CTeleporter::Reset()
{
	m_MarkedForDestroy = true;
}

void CTeleporter::ResetCollision(bool Remove)
{
	if(!m_Collision && !Remove)
		return;

	if(m_Type == TILE_TELEOUT)
	{
		auto &Outs = GameServer()->Collision()->TeleOutsMutable(m_Number - 1);
		if(Remove)
		{
			for(size_t i = 0; i < Outs.size(); i++)
			{
				if(Outs[i] == m_Pos)
				{
					Outs.erase(Outs.begin() + i);
					break;
				}
			}
		}
		else
			Outs.push_back(m_Pos);
	}

	int Type = m_Type;
	int Number = m_Number;
	if(Remove)
	{
		Type = 0;
		Number = 0;
		m_Collision = false;
	}
	GameServer()->Collision()->SetTeleporter(m_Pos, Type, Number);
}

void CTeleporter::Tick()
{
	if(s_LastProcessTick != Server()->Tick())
	{
		if(g_Config.m_SvLightTeleporters && Server()->Tick() % 2 == 0)
		{
			if(s_CurrentDist < 32.f)
				s_CurrentDist = std::clamp(s_CurrentDist + 3.75f, 0.f, 32.f);
			else
			{
				s_CurrentDist = 0.f;
				s_Vertical = !s_Vertical;
			}
		}
		s_LastProcessTick = Server()->Tick();
	}
}

void CTeleporter::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	if(g_Config.m_SvLightTeleporters)
	{
		const vec2 aCorners[4] = {
			vec2(-12, -12),
			vec2(12, -12),
			vec2(12, 12),
			vec2(-12, 12),
		};

		for(int i = 0; i < 2; i++)
		{
			if(!m_aId[i].has_value())
				continue;
			const int From = (i * 2) + (int)s_Vertical;
			const int To = From == 3 ? 0 : From + 1;
			const float Diff = s_CurrentDist / 32.f;
			vec2 Pos;
			Pos.x = mix(aCorners[From].x, aCorners[To].x, Diff);
			Pos.y = mix(aCorners[From].y, aCorners[To].y, Diff);

			CNetObj_Projectile Proj = {};
			Proj.m_X = round_to_int(m_Pos.x + Pos.x);
			Proj.m_Y = round_to_int(m_Pos.y + Pos.y);
			Proj.m_VelX = 0;
			Proj.m_VelY = 0;
			Proj.m_StartTick = 0;
			Proj.m_Type = WEAPON_HAMMER;
			Server()->SnapNewItem(m_aId[i].value(), Proj);
		}
	}
	else
	{
		const float AngleStep = 2.0f * pi / NUM_CIRCLE;
		m_Snap.m_Time += (Server()->Tick() - (int)m_Snap.m_LastTime) / (float)Server()->TickSpeed();
		m_Snap.m_LastTime = (float)Server()->Tick();

		for(int i = 0; i < NUM_CIRCLE; i++)
		{
			if(!m_aId[i].has_value())
				continue;
			vec2 Pos = m_Pos;
			Pos.x += TELE_RADIUS * cosf(m_Snap.m_Time * 2.5f + AngleStep * i);
			Pos.y += TELE_RADIUS * sinf(m_Snap.m_Time * 2.5f + AngleStep * i);

			CNetObj_Projectile Proj = {};
			Proj.m_X = (int)Pos.x;
			Proj.m_Y = (int)Pos.y;
			Proj.m_VelX = 0;
			Proj.m_VelY = 0;
			Proj.m_StartTick = 0;
			Proj.m_Type = WEAPON_HAMMER;
			Server()->SnapNewItem(m_aId[i].value(), Proj);
		}
	}

	if(!m_aId[NUM_CIRCLE].has_value())
		return;

	if(m_Type == TILE_TELEINWEAPON)
	{
		CNetObj_Projectile Proj = {};
		Proj.m_X = (int)m_Pos.x;
		Proj.m_Y = (int)m_Pos.y;
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = Server()->Tick() - 2;
		Proj.m_Type = WEAPON_GUN;
		Server()->SnapNewItem(m_aId[NUM_CIRCLE].value(), Proj);
	}
	else if(m_Type == TILE_TELEINHOOK)
	{
		CNetObj_Projectile Proj = {};
		Proj.m_X = (int)m_Pos.x;
		Proj.m_Y = (int)m_Pos.y;
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = Server()->Tick() - 2;
		Proj.m_Type = WEAPON_SHOTGUN;
		Server()->SnapNewItem(m_aId[NUM_CIRCLE].value(), Proj);
	}
	else if(m_Type != TILE_TELEOUT && m_Collision)
	{
		const float RandomRadius = (random_float() * (TELE_RADIUS - 4.0f));
		const float RandomAngle = 2.0f * pi * random_float();
		const vec2 ParticlePos = m_Pos + vec2(RandomRadius * cosf(RandomAngle), RandomRadius * sinf(RandomAngle));

		CNetObj_Projectile Proj = {};
		Proj.m_X = (int)ParticlePos.x;
		Proj.m_Y = (int)ParticlePos.y;
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = Server()->Tick();
		Proj.m_Type = WEAPON_HAMMER;
		Server()->SnapNewItem(m_aId[NUM_CIRCLE].value(), Proj);
	}
}

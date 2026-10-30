#include "speedup.h"

#include <algorithm>

#include <base/math.h>
#include <base/vmath.h>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/server/gamecontext.h>

#include <game/server/entities/character.h>

CSpeedup::CSpeedup(CGameWorld *pGameWorld, vec2 Pos, float Angle, int Force, int MaxSpeed, bool Collision) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_SPEEDUP, true, Pos, 14)
{
	m_Force = Force;
	m_MaxSpeed = MaxSpeed;
	m_Collision = Collision;

	const vec2 aOffsets[NUM_DOTS] = {
		vec2(5, 0),
		vec2(2, 3),
		vec2(2, -3),
		vec2(-1, 6),
		vec2(-1, -6),
		vec2(-4, 9),
		vec2(-4, -9)};

	for(int i = 0; i < NUM_DOTS; i++)
	{
		m_aDots[i].m_Pos = aOffsets[i];
		m_aDots[i].m_Id = Server()->SnapNewId();
	}

	m_CurrentDist = 0.f;
	m_Distance = length(m_aDots[DOT_END_TOP].m_Pos - m_aDots[DOT_CENTER].m_Pos);
	SetAngle((int)Angle);
	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CSpeedup::~CSpeedup()
{
	ResetCollision(true);
	for(int i = 0; i < NUM_DOTS; i++)
	{
		if(m_aDots[i].m_Id.has_value())
			Server()->SnapFreeId(*m_aDots[i].m_Id);
	}
}

void CSpeedup::ResetCollision(bool Remove)
{
	if(!m_Collision)
		return;

	int Angle = m_Angle;
	int Force = m_Force;
	int MaxSpeed = m_MaxSpeed;
	if(Remove)
	{
		Angle = 0;
		Force = 0;
		MaxSpeed = 0;
		m_Collision = false;
	}
	GameServer()->Collision()->SetSpeedup(m_Pos, Angle, Force, MaxSpeed);
}

void CSpeedup::Rotate(int Angle)
{
	m_Angle += Angle;
	for(int i = 0; i < NUM_DOTS; i++)
		m_aDots[i].Rotate(Angle);
}

void CSpeedup::SetAngle(int Angle)
{
	Rotate(-m_Angle);
	Rotate(Angle);
}

void CSpeedup::Tick()
{
	if(Server()->Tick() % 2 == 0)
	{
		if(m_CurrentDist < m_Distance)
			m_CurrentDist = std::clamp(m_CurrentDist + 3.25f, 0.f, m_Distance);
		else
			m_CurrentDist = 0.f;
	}
}

void CSpeedup::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if(pChr && pChr->m_DrawEditor.OnSnapPreview(this))
		return;

	for(int i = 0; i < NUM_DOTS; i++)
	{
		if(!m_aDots[i].m_Id.has_value())
			continue;

		vec2 Pos = m_aDots[i].m_Pos;
		if(g_Config.m_SvLightSpeedups)
		{
			if(i >= 2)
				break;
			const int To = NUM_DOTS - 1 - i;
			const float Diff = m_Distance > 0.f ? m_CurrentDist / m_Distance : 0.f;
			Pos.x = mix(m_aDots[To].m_Pos.x, m_aDots[DOT_CENTER].m_Pos.x, Diff);
			Pos.y = mix(m_aDots[To].m_Pos.y, m_aDots[DOT_CENTER].m_Pos.y, Diff);
		}

		CNetObj_Projectile Proj = {};
		Proj.m_X = round_to_int(m_Pos.x + Pos.x);
		Proj.m_Y = round_to_int(m_Pos.y + Pos.y);
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_Type = WEAPON_HAMMER;
		Proj.m_StartTick = 0;
		Server()->SnapNewItem(*m_aDots[i].m_Id, Proj);
	}
}

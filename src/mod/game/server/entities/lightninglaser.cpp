#include "lightninglaser.h"

#include <algorithm>
#include <cstdlib>

#include <base/math.h>
#include <base/vmath.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>

CLightningLaser::CLightningLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LIGHTNING_LASER, false, Pos)
{
	m_Pos = Pos;
	m_Owner = Owner;
	const float Len = length(Direction);
	m_Dir = Len > 0.001f ? Direction / Len : vec2(1.f, 0.f);
	m_StartTick = -2.5f;

	m_Lifespan = m_StartLifespan = Server()->TickSpeed() / 5;

	const int TuneZone = GameServer()->Collision()->IsTune(GameServer()->Collision()->GetMapIndex(m_Pos));
	CTuningParams *pTuning = GameWorld()->GetTuning(TuneZone);
	m_Count = std::max(1, (int)pTuning->m_LightningLaserCount);
	m_Length = std::max(1, (int)pTuning->m_LightningLaserLength);

	m_aIds.resize(m_Count);
	m_aPositions.resize(m_Count);
	for(int i = 0; i < m_Count; i++)
	{
		m_aIds[i] = Server()->SnapNewId();
		m_aPositions[i][POS_START] = Pos;
		m_aPositions[i][POS_END] = Pos;
	}

	m_Target.Reset();
	InitTarget();
	GenerateLights();
	GameWorld()->InsertEntity(this);
}

CLightningLaser::~CLightningLaser()
{
	for(auto &Id : m_aIds)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CLightningLaser::Reset()
{
	m_MarkedForDestroy = true;
}

bool CLightningLaser::TargetAlive()
{
	return m_Target.m_Id != -1 && GameServer()->GetPlayerChar(m_Target.m_Id);
}

void CLightningLaser::InitTarget()
{
	m_Target.Reset();

	constexpr float DetectAngle = 90.f;
	const float OwnAngle = angle(m_Dir) * 180.f / pi + 75.f;
	float ClosestDist = 0.f;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == m_Owner)
			continue;
		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if(!pChr)
			continue;

		const float Dist = distance(m_Pos, pChr->GetPos());
		if(Dist > m_Count * m_Length)
			continue;

		if(m_Target.m_Id == -1 || Dist < ClosestDist)
		{
			const float TargetAngle = angle(pChr->GetPos() - m_Pos) * 180.f / pi + 90.f;
			if((OwnAngle - TargetAngle < DetectAngle && OwnAngle - TargetAngle > -DetectAngle) ||
				(TargetAngle - OwnAngle < DetectAngle && TargetAngle - OwnAngle > -DetectAngle))
			{
				m_Target.m_IsAlive = true;
				m_Target.m_Id = i;
				m_Target.m_Pos = pChr->GetPos();
				ClosestDist = Dist;
			}
		}
	}
}

void CLightningLaser::UpdateDirection(vec2 From)
{
	if(TargetAlive())
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_Target.m_Id);
		if(pChr)
			m_Dir = normalize(pChr->GetPos() - From);
	}
	else
		m_Target.m_Id = -1;
}

bool CLightningLaser::TargetBehindWall(vec2 From)
{
	if(!TargetAlive())
		return false;
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Target.m_Id);
	return pChr && GameServer()->Collision()->IntersectLine(From, pChr->GetPos(), nullptr, nullptr);
}

void CLightningLaser::GenerateLights()
{
	for(int i = 0; i < m_Count; i++)
	{
		m_aPositions[i][POS_START] = i != 0 ? m_aPositions[i - 1][POS_END] : m_Pos;

		if(!TargetBehindWall(m_aPositions[i][POS_START]))
			UpdateDirection(m_aPositions[i][POS_START]);

		const int RandDir = m_Target.m_Id != -1 ? (rand() % 80 - 40) : (rand() % 140 - 70);
		const float RandShot = angle(m_Dir) + RandDir * pi / 180.f;
		m_aPositions[i][POS_END] = m_aPositions[i][POS_START] + direction(RandShot) * (float)m_Length;

		vec2 CollisionPos;
		if(GameServer()->Collision()->IntersectLine(m_aPositions[i][POS_START], m_aPositions[i][POS_END], nullptr, &CollisionPos))
			m_aPositions[i][POS_END] = CollisionPos;
	}
}

void CLightningLaser::HitCharacter()
{
	for(int i = 1; i < m_Count; i++)
	{
		for(int j = 0; j < MAX_CLIENTS; j++)
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(j);
			if(!pChr || j == m_Owner)
				continue;

			vec2 Point;
			if(closest_point_on_line(m_aPositions[i][POS_END], m_aPositions[i][POS_START], pChr->GetPos(), Point))
			{
				const bool IntersectLine = GameServer()->Collision()->IntersectLine(m_aPositions[i][POS_END], pChr->GetPos(), nullptr, nullptr);
				if(distance(Point, pChr->GetPos()) <= m_Length / 2 && !IntersectLine)
					m_aPositions[i][POS_END] = pChr->GetPos();

				if(m_aPositions[i - 1][POS_END] == pChr->GetPos())
					m_aPositions[i][POS_END] = m_aPositions[i][POS_START] = m_aPositions[i - 1][POS_END];

				if(distance(Point, pChr->GetPos()) < 10.f)
				{
					pChr->Freeze(3);
					pChr->Core()->m_Vel = vec2(0.f, 0.f);
				}
			}
		}
	}
}

void CLightningLaser::Tick()
{
	m_Lifespan--;
	if(m_Lifespan <= 0)
	{
		Reset();
		return;
	}
	if(m_Lifespan <= 3)
		m_StartTick--;

	HitCharacter();
}

void CLightningLaser::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, m_aPositions[0][POS_START]))
		return;

	const float Percentage = 100.f - (m_Lifespan * 100.f / (float)m_StartLifespan);
	const int Start = (int)std::max((float)std::ceil(Percentage * m_Count / 100.f), 1.f);

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	for(int i = Start - 1; i >= 0; i--)
	{
		if(!m_aIds[i].has_value())
			continue;
		GameServer()->SnapLaserObject(Context, m_aIds[i].value(),
			m_aPositions[i][POS_START], m_aPositions[i][POS_END],
			Server()->Tick() + (int)m_StartTick, m_Owner, LASERTYPE_FREEZE, 0, 0);
	}
}

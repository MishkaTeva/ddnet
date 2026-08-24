#include "rotating_ball.h"

#include <cstdlib>

#include <base/math.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CRotatingBall::CRotatingBall(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_ROTATING_BALL, true, Pos)
{
	m_Owner = Owner;
	m_TeamMask = CClientMask().set();
	m_RotateDelay = Server()->TickSpeed() + 10;
	m_TableDirV[0][0] = 5;
	m_TableDirV[0][1] = 12;
	m_TableDirV[1][0] = -12;
	m_TableDirV[1][1] = -5;
	m_Id2 = Server()->SnapNewId();
	GameWorld()->InsertEntity(this);
}

CRotatingBall::~CRotatingBall()
{
	if(m_Id2.has_value())
		Server()->SnapFreeId(m_Id2.value());
}

void CRotatingBall::Reset()
{
	m_MarkedForDestroy = true;
}

void CRotatingBall::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner || !pOwner->m_RotatingBall)
	{
		Reset();
		return;
	}

	m_Pos = pOwner->GetPos();
	m_TeamMask = pOwner->TeamMask();

	m_RotateDelay--;
	if(m_RotateDelay <= 0)
	{
		m_IsRotating ^= true;
		int DirSelect = rand() % 2;
		m_LaserInputDir = rand() % (m_TableDirV[DirSelect][1] - m_TableDirV[DirSelect][0] + 1) + m_TableDirV[DirSelect][0];
		m_RotateDelay = m_IsRotating ? Server()->TickSpeed() + (rand() % (7 - 3 + 1) + 3) : Server()->TickSpeed() + (rand() % (20 - 5 + 1) + 5);
	}

	if(m_IsRotating)
		m_LaserDirAngle += m_LaserInputDir;

	m_LaserPos.x = pOwner->GetPos().x + 65 * sinf(m_LaserDirAngle * pi / 180.0f);
	m_LaserPos.y = pOwner->GetPos().y + 65 * cosf(m_LaserDirAngle * pi / 180.0f);
	m_ProjPos.x = m_LaserPos.x + 20 * sinf(Server()->Tick() * 13 * pi / 180.0f);
	m_ProjPos.y = m_LaserPos.y + 20 * cosf(Server()->Tick() * 13 * pi / 180.0f);
}

void CRotatingBall::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !GetId().has_value())
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;
	if(pOwner && pOwner->GetPlayer() && pOwner->GetPlayer()->IsPaused())
		return;

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);
	GameServer()->SnapLaserObject(Context, GetId().value(), m_LaserPos, m_LaserPos, Server()->Tick(), -1, LASERTYPE_RIFLE, 0, 0);

	if(!m_Id2.has_value())
		return;
	CNetObj_Projectile Proj = {};
	Proj.m_X = round_to_int(m_ProjPos.x);
	Proj.m_Y = round_to_int(m_ProjPos.y);
	Proj.m_VelX = 0;
	Proj.m_VelY = 0;
	Proj.m_StartTick = 0;
	Proj.m_Type = WEAPON_HAMMER;
	Server()->SnapNewItem(m_Id2.value(), Proj);
}

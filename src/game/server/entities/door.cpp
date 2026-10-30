/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#include "door.h"

#include "character.h"

#include <generated/protocol.h>

#include <game/mapitems.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/teamscore.h>

CDoor::CDoor(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
	int Number, bool Collision) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_DOOR, true)
{
	m_Number = Number;
	m_Pos = Pos;
	m_Length = Length;
	m_Rotation = Rotation;
	m_Collision = Collision;
	m_Direction = vec2(std::sin(Rotation), std::cos(Rotation));
	UpdateEndpoint();
	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CDoor::~CDoor()
{
	ClearCollision();
}

void CDoor::UpdateEndpoint()
{
	vec2 To = m_Pos + normalize(m_Direction) * m_Length;
	GameServer()->Collision()->IntersectNoLaser(m_Pos, To, &this->m_To, nullptr);
}

void CDoor::SetDirection(float Rotation)
{
	m_Rotation = Rotation;
	m_Direction = vec2(std::sin(Rotation), std::cos(Rotation));
	UpdateEndpoint();
}

void CDoor::SetLength(int Length)
{
	m_Length = Length;
	UpdateEndpoint();
}

void CDoor::ResetCollision()
{
	if(!m_Collision)
		return;
	if(GameServer()->Collision()->GetTile(m_Pos.x, m_Pos.y) || GameServer()->Collision()->GetFrontTile(m_Pos.x, m_Pos.y))
		return;

	for(int i = 0; i < m_Length - 1; i++)
	{
		vec2 CurrentPos = m_Pos + m_Direction * i;
		if(GameServer()->Collision()->CheckPoint(CurrentPos))
			break;
		else
			GameServer()->Collision()->SetDoorCollisionAt(CurrentPos.x, CurrentPos.y, TILE_STOPA, 0, m_Number);
	}
}

void CDoor::ClearCollision()
{
	if(!m_Collision)
		return;
	for(int i = 0; i < m_Length - 1; i++)
	{
		vec2 CurrentPos = m_Pos + m_Direction * i;
		if(GameServer()->Collision()->CheckPoint(CurrentPos))
			break;
		GameServer()->Collision()->SetDoorCollisionAt(CurrentPos.x, CurrentPos.y, TILE_AIR, 0, 0);
	}
	m_Collision = false;
}

void CDoor::Reset()
{
	m_MarkedForDestroy = true;
}

void CDoor::Snap(int SnappingClient)
{
	if((NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_To)) || !GetId().has_value())
		return;

#ifdef CONF_FDDRACE_MOD
	CCharacter *pSnapChr = GameServer()->GetPlayerChar(SnappingClient);
	if(pSnapChr && pSnapChr->m_DrawEditor.OnSnapPreview(this))
		return;
#endif

	int SnappingClientVersion = GameServer()->GetClientVersion(SnappingClient);

	vec2 From;
	int StartTick;

	if(SnappingClientVersion >= VERSION_DDNET_ENTITY_NETOBJS)
	{
		From = m_To;
		StartTick = -1;
	}
	else
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);

		if(SnappingClient != SERVER_DEMO_CLIENT && (GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS || GameServer()->m_apPlayers[SnappingClient]->IsPaused()) && GameServer()->m_apPlayers[SnappingClient]->SpectatorId() != SPEC_FREEVIEW)
			pChr = GameServer()->GetPlayerChar(GameServer()->m_apPlayers[SnappingClient]->SpectatorId());

		if(pChr && pChr->Team() != TEAM_SUPER && pChr->IsAlive() && !Switchers().empty() && Switchers()[m_Number].m_aStatus[pChr->Team()])
		{
			From = m_To;
		}
		else
		{
			From = m_Pos;
		}
		StartTick = Server()->Tick();
	}

	GameServer()->SnapLaserObject(CSnapContext(SnappingClientVersion, Server()->IsSixup(SnappingClient), SnappingClient), GetId().value(),
		m_Pos, From, StartTick, -1, LASERTYPE_DOOR, 0, m_Number);
}

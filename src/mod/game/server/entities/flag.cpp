#include "flag.h"

#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CFlag::CFlag(CGameWorld *pGameWorld, int Team, vec2 Pos) :
	CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG, Pos, vec2(ms_PhysSize, ms_PhysSize), -1, false)
{
	m_Pos = m_PrevPos = Pos;
	m_StandPos = Pos;
	m_Team = Team;
	m_CheckDeath = false;
	m_Gravity = true;
	Reset(true);
	if(m_Team == TEAM_RED || m_Team == TEAM_BLUE)
		GameServer()->m_apFlags[m_Team] = this;
	GameWorld()->InsertEntity(this);
}

CFlag::~CFlag()
{
	if(m_Team == TEAM_RED || m_Team == TEAM_BLUE)
	{
		if(GameServer()->m_apFlags[m_Team] == this)
			GameServer()->m_apFlags[m_Team] = nullptr;
	}
}

void CFlag::Reset(bool Init)
{
	if(!Init)
		GameServer()->CreateDeath(m_Pos, GetCarrier() ? m_Carrier : (GetLastCarrier() ? m_LastCarrier : -1));

	m_Carrier = -1;
	m_LastCarrier = -1;
	m_AtStand = true;
	m_Pos = m_PrevPos = m_StandPos;
	m_Vel = vec2(0, 0);
	m_DropTick = 0;
}

CCharacter *CFlag::GetCarrier()
{
	return GameServer()->GetPlayerChar(m_Carrier);
}

CCharacter *CFlag::GetLastCarrier()
{
	return GameServer()->GetPlayerChar(m_LastCarrier);
}

void CFlag::Drop(int Dir)
{
	if(m_Carrier == -1)
		return;
	m_DropTick = Server()->Tick();
	m_LastCarrier = m_Carrier;
	m_Carrier = -1;
	m_Vel = vec2(5.f * Dir, Dir == 0 ? 0.f : -5.f);
}

void CFlag::Grab(int NewCarrier)
{
	m_AtStand = false;
	m_Carrier = NewCarrier;
}

void CFlag::Tick()
{
	m_Owner = GetCarrier() ? m_Carrier : (GetLastCarrier() ? m_LastCarrier : -1);
	CAdvancedEntity::Tick();

	if(GetCarrier())
	{
		// Drop if carrier crosses FLAG_STOP / VIP+ tiles between prev and current.
		const int End = round_to_int(distance(m_Pos, m_PrevPos)) + 1;
		const float Inv = End > 0 ? 1.0f / End : 1.f;
		for(int i = 0; i <= End; i++)
		{
			const vec2 Pos = mix(m_PrevPos, m_Pos, i * Inv);
			const int MapIndex = GameServer()->Collision()->GetMapIndex(Pos);
			const int TileIndex = GameServer()->Collision()->GetTileIndex(MapIndex);
			const int TileFIndex = GameServer()->Collision()->GetFrontTileIndex(MapIndex);
			if(TileIndex == TILE_FLAG_STOP || TileFIndex == TILE_FLAG_STOP ||
				TileIndex == TILE_VIP_PLUS_ONLY || TileFIndex == TILE_VIP_PLUS_ONLY)
			{
				const int Dir = GetCarrier() ? GetCarrier()->GetAimDir() : 0;
				Drop(Dir);
				m_Pos = m_PrevPos;
				break;
			}
		}
		m_PrevPos = m_Pos;
		return;
	}

	CCharacter *apClose[MAX_CLIENTS];
	const int Num = GameWorld()->FindEntities(m_Pos, GetProximityRadius(), (CEntity **)apClose, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		CCharacter *pChr = apClose[i];
		if(!pChr || !pChr->GetPlayer() || pChr->GetPlayer()->GetTeam() == TEAM_SPECTATORS)
			continue;
		if(GameServer()->HasFlag(pChr) != -1)
			continue;
		Grab(pChr->GetPlayer()->GetCid());
		break;
	}

	if(!GetCarrier())
	{
		HandleDropped();
		if(m_DropTick && Server()->Tick() > m_DropTick + Server()->TickSpeed() * 30)
			Reset(false);
	}
	m_PrevPos = m_Pos;
}

void CFlag::TickDeferred()
{
	if(GetCarrier())
		m_Pos = GetCarrier()->GetPos();
}

void CFlag::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;

	CNetObj_Flag Flag;
	Flag.m_X = round_to_int(m_Pos.x);
	Flag.m_Y = round_to_int(m_Pos.y);
	Flag.m_Team = m_Team;
	Server()->SnapNewItem(m_Team, Flag);
}

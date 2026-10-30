#include "button.h"

#include <engine/server.h>

#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/teamscore.h>

CButton::CButton(CGameWorld *pGameWorld, vec2 Pos, int Number, bool Collision) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_BUTTON, true, Pos, 14)
{
	m_Number = Number;
	m_Collision = Collision;

	const vec2 aOffsets[NUM_SIDES] = {
		vec2(0, -16),
		vec2(16, 0),
		vec2(0, 16),
		vec2(-16, 0)};

	for(int i = 0; i < NUM_SIDES; i++)
	{
		m_aSides[i].m_Pos = aOffsets[i];
		m_aSides[i].m_Id = Server()->SnapNewId();
	}

	ResetCollision();
	GameWorld()->InsertEntity(this);
}

CButton::~CButton()
{
	ResetCollision(true);
	for(int i = 0; i < NUM_SIDES; i++)
	{
		if(m_aSides[i].m_Id.has_value())
			Server()->SnapFreeId(*m_aSides[i].m_Id);
	}
}

void CButton::ResetCollision(bool Remove)
{
	if(!m_Collision)
		return;

	const int Index = GameServer()->Collision()->GetPureMapIndex(m_Pos);
	// DDNet TILE_SPEED_BOOST_OLD=28 conflicts with F-DDrace TILE_SWITCHTOGGLE;
	// use TILE_STOPA until a dedicated switch-toggle door index is reserved.
	if(Remove)
	{
		GameServer()->Collision()->RemoveDoorTile(Index, TILE_STOPA, m_Number);
		m_Collision = false;
	}
	else
	{
		GameServer()->Collision()->AddDoorTile(Index, TILE_STOPA, m_Number);
	}
}

void CButton::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !GetId().has_value())
		return;

	CCharacter *pChr = GameServer()->GetPlayerChar(SnappingClient);
	if(pChr && pChr->m_DrawEditor.OnSnapPreview(this))
		return;

	bool Status = false;
	if(pChr && pChr->Team() != TEAM_SUPER && m_Number > 0 && m_Number < (int)Switchers().size())
		Status = Switchers()[m_Number].m_aStatus[pChr->Team()];

	if(!Status && (Server()->Tick() % Server()->TickSpeed()) % 11 == 0)
		return;

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	GameServer()->SnapLaserObject(Context, *GetId(), m_Pos, m_Pos, 0, -1, LASERTYPE_DOOR, 0, 0);

	if(!Status)
		return;

	for(int i = 0; i < NUM_SIDES; i++)
	{
		if(!m_aSides[i].m_Id.has_value())
			continue;
		const int To = i == POINT_LEFT ? POINT_TOP : i + 1;
		const vec2 From = m_Pos + m_aSides[To].m_Pos;
		const vec2 ToPos = m_Pos + m_aSides[i].m_Pos;
		GameServer()->SnapLaserObject(Context, *m_aSides[i].m_Id, ToPos, From, Server()->Tick(), -1, LASERTYPE_DOOR, 0, 0);
	}
}

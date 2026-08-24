#include "playercounter.h"

#include "lasertext.h"

#include <base/str.h>

#include <game/server/gamecontext.h>

CPlayerCounter::CPlayerCounter(CGameWorld *pGameWorld, vec2 Pos, int Port) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PLAYER_COUNTER, false, Pos)
{
	m_Port = Port;
	m_pLaserText = nullptr;
	m_LastUpdate = 0;
	Update(-1);
	GameWorld()->InsertEntity(this);
}

CPlayerCounter::~CPlayerCounter()
{
	if(m_pLaserText)
	{
		m_pLaserText->Reset();
		m_pLaserText = nullptr;
	}
}

void CPlayerCounter::OnUpdate(int Port, int PlayerCount)
{
	if(Port != m_Port)
		return;
	Update(PlayerCount);
}

void CPlayerCounter::Update(int PlayerCount)
{
	m_PlayerCount = PlayerCount;

	if(m_pLaserText)
	{
		m_pLaserText->Reset();
		m_pLaserText = nullptr;
	}

	char aBuf[8];
	if(m_PlayerCount < 0)
	{
		str_copy(aBuf, "OFF");
		m_LastUpdate = 0;
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "%d", m_PlayerCount);
		m_LastUpdate = Server()->Tick();
	}

	const int Len = str_length(aBuf);
	vec2 Pos = m_Pos;
	if(Len >= 2)
		Pos.x -= 26.f;
	if(Len >= 3)
		Pos.x -= 26.f;
	// Permanent text (Seconds = -1); AboveTee=false so position is used as-is after CreateLaserText offsets.
	m_pLaserText = GameServer()->CreateLaserText(Pos + vec2(16.f, 32.f), -1, aBuf, -1, false);
}

void CPlayerCounter::Tick()
{
	if(m_LastUpdate && m_LastUpdate + Server()->TickSpeed() * 65 < Server()->Tick())
		Update(-1);
}

void CPlayerCounter::Snap(int SnappingClient)
{
}

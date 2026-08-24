#include "clock.h"

#include <ctime>

#include <base/math.h>

#include <generated/protocol.h>

#include <game/server/gamecontext.h>

CClock::CClock(CGameWorld *pGameWorld, vec2 Pos) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_CLOCK, true, Pos)
{
	m_Hand[CLOCK_SECOND].m_Length = 72;
	m_Hand[CLOCK_MINUTE].m_Length = 96;
	m_Hand[CLOCK_HOUR].m_Length = 60;
	m_Id2 = Server()->SnapNewId();
	m_Id3 = Server()->SnapNewId();
	SetHandRotations();
	for(auto &Hand : m_Hand)
		Hand.m_To = m_Pos + vec2(0, (float)Hand.m_Length);
	GameWorld()->InsertEntity(this);
}

CClock::~CClock()
{
	if(m_Id2.has_value())
		Server()->SnapFreeId(m_Id2.value());
	if(m_Id3.has_value())
		Server()->SnapFreeId(m_Id3.value());
}

void CClock::Tick()
{
	if(Server()->Tick() % Server()->TickSpeed() != 0)
		return;

	SetHandRotations();
	for(int i = 0; i < 3; i++)
	{
		vec2 Dir(sinf(m_Hand[i].m_Rotation), cosf(m_Hand[i].m_Rotation));
		vec2 To2 = m_Pos + normalize(Dir) * m_Hand[i].m_Length;
		GameServer()->Collision()->IntersectLine(m_Pos, To2, &m_Hand[i].m_To, nullptr);
	}
}

void CClock::SetHandRotations()
{
	time_t RawTime;
	time(&RawTime);
	struct tm *pTimeInfo = localtime(&RawTime);
	m_Hand[CLOCK_SECOND].m_Rotation = -(pTimeInfo->tm_sec + 30) * pi / 30;
	m_Hand[CLOCK_MINUTE].m_Rotation = -(pTimeInfo->tm_min + 30) * pi / 30;
	m_Hand[CLOCK_HOUR].m_Rotation = -(pTimeInfo->tm_hour + 6) * pi / 6;
}

int CClock::GetSnapId(int Hand) const
{
	switch(Hand)
	{
	case CLOCK_SECOND:
		return GetId().value_or(-1);
	case CLOCK_MINUTE:
		return m_Id2.value_or(-1);
	case CLOCK_HOUR:
		return m_Id3.value_or(-1);
	}
	return -1;
}

void CClock::Snap(int SnappingClient)
{
	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	for(int i = 2; i > -1; i--)
	{
		if(NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_Hand[i].m_To))
			return;
		const int SnapId = GetSnapId(i);
		if(SnapId < 0)
			continue;
		GameServer()->SnapLaserObject(Context, SnapId, m_Pos, m_Hand[i].m_To, Server()->Tick() - 4 + i, -1, LASERTYPE_RIFLE, 0, 0);
	}
}

#include "jail_release.h"

#include "lasertext.h"

#include <algorithm>

#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <base/math.h>

const int CJailRelease::s_aPhaseTicks[NUM_PHASES] = {
	20,
	25,
	35,
	20,
};

CJailRelease::CJailRelease(CGameWorld *pGameWorld, vec2 PortalPos, int Victim) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_JAIL_RELEASE, false, PortalPos)
{
	m_Victim = Victim;
	m_Phase = PHASE_APPEAR;
	m_PhaseStartTick = Server()->Tick();
	m_VictimStartPos = PortalPos;
	m_TeamMask = CClientMask().set();

	for(auto &Id : m_aId)
		Id = Server()->SnapNewId();

	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(pChr)
	{
		m_VictimStartPos = pChr->GetPos();
		m_TeamMask = pChr->TeamMask();
		pChr->Freeze(4);
		pChr->Core()->m_Vel = vec2(0.f, 0.f);
	}

	if(GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_JailReleasing = true;

	GameServer()->CreateLaserText(m_Pos + vec2(12.f, -40.f), m_Victim, "FREE", 3, false);
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, m_TeamMask);
	GameServer()->CreatePlayerSpawn(m_Pos, m_TeamMask);
	GameWorld()->InsertEntity(this);
}

CJailRelease::~CJailRelease()
{
	for(auto &Id : m_aId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CJailRelease::Reset()
{
	if(m_Victim >= 0 && m_Victim < MAX_CLIENTS && GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_JailReleasing = false;
	m_MarkedForDestroy = true;
}

float CJailRelease::PhaseProgress()
{
	int Duration = s_aPhaseTicks[m_Phase];
	if(Duration <= 0)
		return 1.f;
	return std::clamp((Server()->Tick() - m_PhaseStartTick) / (float)Duration, 0.f, 1.f);
}

void CJailRelease::AdvancePhase()
{
	m_Phase++;
	m_PhaseStartTick = Server()->Tick();

	if(m_Phase == PHASE_BREAK)
		GameServer()->CreateSound(m_Pos, SOUND_HOOK_ATTACH_GROUND, m_TeamMask);
	if(m_Phase == PHASE_LIFT)
		GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, m_TeamMask);
	if(m_Phase == PHASE_EXIT)
		GameServer()->CreatePlayerSpawn(m_Pos, m_TeamMask);

	if(m_Phase >= NUM_PHASES)
		Finish();
}

void CJailRelease::DoReleaseCleanup()
{
	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : nullptr;
	if(!pPlayer)
		return;

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' was released from jail", Server()->ClientName(m_Victim));
	GameServer()->SendChat(-1, TEAM_ALL, aBuf);

	pPlayer->m_JailReleasing = false;
	pPlayer->m_JailTime = 1;
}

void CJailRelease::Finish()
{
	DoReleaseCleanup();

	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : nullptr;
	vec2 Pos = m_Pos;
	if(pPlayer && pPlayer->GetCharacter())
	{
		Pos = pPlayer->GetCharacter()->GetPos();
		pPlayer->KillCharacter(WEAPON_GAME);
	}

	GameServer()->CreatePlayerSpawn(Pos, m_TeamMask);
	GameServer()->CreateSound(Pos, SOUND_WEAPON_SPAWN, m_TeamMask);
	m_MarkedForDestroy = true;
}

void CJailRelease::UpdateVictimControl()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(!pChr)
		return;

	m_TeamMask = pChr->TeamMask();
	pChr->Freeze(2);
	pChr->Core()->m_Vel = vec2(0.f, 0.f);

	float Progress = PhaseProgress();
	vec2 NewPos = m_VictimStartPos;

	if(m_Phase == PHASE_APPEAR || m_Phase == PHASE_BREAK)
		NewPos = m_VictimStartPos;
	else if(m_Phase == PHASE_LIFT)
	{
		float Smooth = Progress * Progress * (3.f - 2.f * Progress);
		NewPos = mix(m_VictimStartPos, m_Pos, Smooth);
	}
	else if(m_Phase == PHASE_EXIT)
		NewPos = m_Pos;

	pChr->SetPosition(NewPos);
	pChr->Core()->m_Vel = vec2(0.f, 0.f);
}

void CJailRelease::SnapLaser(int SnappingClient, int Id, vec2 From, vec2 To, int Type)
{
	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);
	GameServer()->SnapLaserObject(Context, Id, To, From, Server()->Tick(), -1, Type, 0, 0);
}

bool CJailRelease::FindPortalPos(CGameContext *pGameServer, vec2 From, vec2 *pOut)
{
	const float aUp[] = {72.f, 64.f, 80.f, 56.f, 96.f, 48.f, 112.f, 128.f};
	const float aSide[] = {0.f, -28.f, 28.f, -48.f, 48.f};
	const vec2 Size(32.f, 32.f);

	for(int u = 0; u < (int)(sizeof(aUp) / sizeof(aUp[0])); u++)
	{
		for(int s = 0; s < (int)(sizeof(aSide) / sizeof(aSide[0])); s++)
		{
			vec2 Candidate = From + vec2(aSide[s], -aUp[u]);
			if(pGameServer->Collision()->TestBox(Candidate, Size))
				continue;
			if(pGameServer->Collision()->IntersectLine(From, Candidate, nullptr, nullptr))
				continue;
			*pOut = Candidate;
			return true;
		}
	}

	*pOut = From + vec2(0.f, -64.f);
	if(pGameServer->Collision()->TestBox(*pOut, Size))
		*pOut = From;
	return true;
}

void CJailRelease::Tick()
{
	if(m_MarkedForDestroy)
		return;

	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : nullptr;
	if(!pPlayer)
	{
		m_MarkedForDestroy = true;
		return;
	}

	if(!pPlayer->GetCharacter())
	{
		DoReleaseCleanup();
		m_MarkedForDestroy = true;
		return;
	}

	UpdateVictimControl();

	if(Server()->Tick() - m_PhaseStartTick >= s_aPhaseTicks[m_Phase])
		AdvancePhase();
}

void CJailRelease::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, m_Pos))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	float Appear = 1.f;
	if(m_Phase == PHASE_APPEAR)
		Appear = PhaseProgress();
	else if(m_Phase == PHASE_EXIT)
		Appear = 1.f - PhaseProgress() * 0.5f;

	float Radius = 36.f * Appear;
	float Spin = Server()->Tick() * 0.22f;
	float AngleStep = 2.f * pi / NUM_RING;
	int Idx = 0;
	int RingType = LASERTYPE_RIFLE;

	for(int i = 0; i < NUM_RING; i++)
	{
		if(Idx >= NUM_IDS || !m_aId[Idx].has_value())
			return;
		float A0 = AngleStep * i + Spin;
		float A1 = AngleStep * (i + 1) + Spin;
		vec2 A = m_Pos + vec2(Radius * cosf(A0), Radius * sinf(A0));
		vec2 B = m_Pos + vec2(Radius * cosf(A1), Radius * sinf(A1));
		SnapLaser(SnappingClient, m_aId[Idx++].value(), A, B, RingType);
	}

	float RayLen = 0.f;
	if(m_Phase == PHASE_BREAK)
		RayLen = 20.f + PhaseProgress() * 50.f;
	else if(m_Phase == PHASE_LIFT)
		RayLen = 70.f * (1.f - PhaseProgress() * 0.4f);
	else if(m_Phase == PHASE_EXIT)
		RayLen = 40.f * (1.f - PhaseProgress());
	else if(m_Phase == PHASE_APPEAR)
		RayLen = Appear * 18.f;

	for(int i = 0; i < NUM_RAYS; i++)
	{
		if(Idx >= NUM_IDS || !m_aId[Idx].has_value())
			return;
		float A = (2.f * pi * i) / NUM_RAYS + Spin * 0.5f;
		vec2 Dir = vec2(cosf(A), sinf(A));
		SnapLaser(SnappingClient, m_aId[Idx++].value(), m_Pos, m_Pos + Dir * RayLen, LASERTYPE_FREEZE);
	}

	for(int i = 0; i < NUM_PARTICLES; i++)
	{
		if(Idx >= NUM_IDS || !m_aId[Idx].has_value())
			return;
		float t = i / (float)NUM_PARTICLES + Server()->Tick() * 0.05f;
		float r = (6.f + fmodf(t * 21.f, std::max(1.f, Radius - 6.f))) * Appear;
		float a = t * 2.f * pi * 2.5f + Spin;
		vec2 P = m_Pos + vec2(r * cosf(a), r * sinf(a));

		CNetObj_Projectile Proj = {};
		Proj.m_X = (int)P.x;
		Proj.m_Y = (int)P.y;
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = Server()->Tick();
		Proj.m_Type = WEAPON_HAMMER;
		Server()->SnapNewItem(m_aId[Idx++].value(), Proj);
	}

	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(pChr && (m_Phase == PHASE_LIFT || m_Phase == PHASE_EXIT))
	{
		if(Idx < NUM_IDS && m_aId[Idx].has_value())
			SnapLaser(SnappingClient, m_aId[Idx++].value(), pChr->GetPos(), m_Pos, LASERTYPE_RIFLE);
		if(Idx < NUM_IDS && m_aId[Idx].has_value())
			SnapLaser(SnappingClient, m_aId[Idx++].value(), pChr->GetPos() + vec2(-4.f, 0.f), m_Pos + vec2(-4.f, 0.f), LASERTYPE_RIFLE);
		if(Idx < NUM_IDS && m_aId[Idx].has_value())
			SnapLaser(SnappingClient, m_aId[Idx++].value(), pChr->GetPos() + vec2(4.f, 0.f), m_Pos + vec2(4.f, 0.f), LASERTYPE_RIFLE);
	}
}

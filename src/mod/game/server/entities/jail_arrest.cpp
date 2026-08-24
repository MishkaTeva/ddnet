#include "jail_arrest.h"

#include "lasertext.h"

#include <algorithm>

#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <base/math.h>

const int CJailArrest::s_aPhaseTicks[NUM_PHASES] = {
	15,
	30,
	40,
	15,
};

CJailArrest::CJailArrest(CGameWorld *pGameWorld, vec2 Pos, int Victim) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_JAIL_ARREST, false, Pos)
{
	m_Victim = Victim;
	m_StartTick = Server()->Tick();
	m_Phase = PHASE_APPEAR;
	m_PhaseStartTick = m_StartTick;
	m_GrabStartPos = Pos;
	m_TeamMask = CClientMask().set();

	for(auto &Id : m_aId)
		Id = Server()->SnapNewId();

	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(pChr)
	{
		m_GrabStartPos = pChr->GetPos();
		m_TeamMask = pChr->TeamMask();
		pChr->Freeze(3);
		pChr->Core()->m_Vel = vec2(0.f, 0.f);
	}

	if(GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_JailArresting = true;

	GameServer()->CreateLaserText(m_Pos + vec2(10.f, -80.f), m_Victim, "JAIL", 4, false);
	GameServer()->CreateSound(m_Pos, SOUND_NINJA_HIT, m_TeamMask);
	GameWorld()->InsertEntity(this);
}

CJailArrest::~CJailArrest()
{
	for(auto &Id : m_aId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CJailArrest::Reset()
{
	if(m_Victim >= 0 && m_Victim < MAX_CLIENTS && GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_JailArresting = false;
	m_MarkedForDestroy = true;
}

float CJailArrest::PhaseProgress()
{
	int Duration = s_aPhaseTicks[m_Phase];
	if(Duration <= 0)
		return 1.f;
	float Progress = (Server()->Tick() - m_PhaseStartTick) / (float)Duration;
	return std::clamp(Progress, 0.f, 1.f);
}

void CJailArrest::AdvancePhase()
{
	m_Phase++;
	m_PhaseStartTick = Server()->Tick();

	if(m_Phase == PHASE_DRAG)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
		if(pChr)
			m_GrabStartPos = pChr->GetPos();
	}

	if(m_Phase == PHASE_SWALLOW)
		GameServer()->CreateSound(m_Pos, SOUND_NINJA_HIT, m_TeamMask);

	if(m_Phase >= NUM_PHASES)
		Finish();
}

void CJailArrest::Finish()
{
	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : nullptr;
	if(pPlayer)
	{
		pPlayer->m_JailArresting = false;
		if(pPlayer->GetCharacter())
			pPlayer->KillCharacter(WEAPON_GAME);
	}

	GameServer()->CreateDeath(m_Pos, m_Victim, m_TeamMask);
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, m_TeamMask);
	m_MarkedForDestroy = true;
}

void CJailArrest::UpdateVictimControl()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(!pChr)
		return;

	m_TeamMask = pChr->TeamMask();
	pChr->Freeze(2);
	pChr->Core()->m_Vel = vec2(0.f, 0.f);

	float Progress = PhaseProgress();
	if(m_Phase == PHASE_DRAG)
	{
		float Smooth = Progress * Progress * (3.f - 2.f * Progress);
		vec2 NewPos = mix(m_GrabStartPos, m_Pos, Smooth);
		pChr->SetPosition(NewPos);
	}
	else if(m_Phase == PHASE_SWALLOW)
	{
		pChr->SetPosition(m_Pos);
	}
}

vec2 CJailArrest::TentaclePoint(int Tentacle, float t, float Reach)
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	vec2 Target = pChr ? pChr->GetPos() : m_GrabStartPos;
	vec2 Dir = Target - m_Pos;
	float Len = length(Dir);
	if(Len < 0.001f)
		Dir = vec2(1.f, 0.f);
	else
		Dir /= Len;

	vec2 Perp = vec2(-Dir.y, Dir.x);
	float Side = (Tentacle == 0) ? 1.f : -1.f;
	float Attach = 26.f;
	vec2 Start = m_Pos + Perp * Side * Attach;
	vec2 End = Target + Perp * Side * (8.f * (1.f - t));

	vec2 Base = mix(Start, End, t * Reach);
	float Wave = sinf(t * pi * 3.5f + Server()->Tick() * 0.35f + Tentacle * pi) * 22.f * Reach;
	Wave += sinf(t * pi * 7.f - Server()->Tick() * 0.55f + Tentacle) * 8.f * Reach;
	float Envelope = sinf(std::clamp(t, 0.f, 1.f) * pi);
	return Base + Perp * Side * Wave * Envelope;
}

bool CJailArrest::FindHolePos(CGameContext *pGameServer, vec2 From, vec2 *pOut)
{
	const float aDistances[] = {160.f, 144.f, 176.f, 128.f, 192.f, 112.f, 208.f, 96.f, 224.f};
	const int NumAngles = 20;
	const vec2 Size(40.f, 40.f);
	const float AngleOffset = (pGameServer->Server()->Tick() % NumAngles) * (2.f * pi / NumAngles);

	for(int d = 0; d < (int)(sizeof(aDistances) / sizeof(aDistances[0])); d++)
	{
		for(int i = 0; i < NumAngles; i++)
		{
			float Angle = AngleOffset + 2.f * pi * i / NumAngles;
			vec2 Candidate = From + vec2(cosf(Angle), sinf(Angle)) * aDistances[d];
			if(pGameServer->Collision()->TestBox(Candidate, Size))
				continue;
			if(pGameServer->Collision()->IntersectLine(From, Candidate, nullptr, nullptr))
				continue;
			*pOut = Candidate;
			return true;
		}
	}
	return false;
}

void CJailArrest::Tick()
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
		pPlayer->m_JailArresting = false;
		m_MarkedForDestroy = true;
		return;
	}

	UpdateVictimControl();

	if(Server()->Tick() - m_PhaseStartTick >= s_aPhaseTicks[m_Phase])
		AdvancePhase();
}

void CJailArrest::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	float Appear = 1.f;
	if(m_Phase == PHASE_APPEAR)
		Appear = PhaseProgress();
	else if(m_Phase == PHASE_SWALLOW)
		Appear = 1.f - PhaseProgress() * 0.35f;

	float Radius = 44.f * Appear;
	float AngleStep = 2.f * pi / NUM_HOLE_SIDE;
	float Spin = Server()->Tick() * 0.18f;

	for(int i = 0; i < NUM_HOLE_SIDE; i++)
	{
		if(!m_aId[i].has_value())
			continue;
		float A0 = AngleStep * i + Spin;
		float A1 = AngleStep * (i + 1) + Spin;
		vec2 PartPosStart = m_Pos + vec2(Radius * cosf(A0), Radius * sinf(A0));
		vec2 PartPosEnd = m_Pos + vec2(Radius * cosf(A1), Radius * sinf(A1));
		GameServer()->SnapLaserObject(Context, m_aId[i].value(), PartPosStart, PartPosEnd, Server()->Tick(), -1, LASERTYPE_DOOR, 0, 0);
	}

	for(int i = 0; i < NUM_HOLE_PARTICLES; i++)
	{
		const int Idx = NUM_HOLE_SIDE + i;
		if(!m_aId[Idx].has_value())
			continue;
		float t = (i / (float)NUM_HOLE_PARTICLES) + Server()->Tick() * 0.04f;
		float r = (4.f + fmodf(t * 17.f, std::max(1.f, Radius - 4.f))) * Appear;
		float a = t * 2.f * pi * 3.f - Spin * 2.f;
		vec2 ParticlePos = m_Pos + vec2(r * cosf(a), r * sinf(a));

		CNetObj_Projectile Proj = {};
		Proj.m_X = (int)ParticlePos.x;
		Proj.m_Y = (int)ParticlePos.y;
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = Server()->Tick();
		Proj.m_Type = WEAPON_HAMMER;
		Server()->SnapNewItem(m_aId[Idx].value(), Proj);
	}

	if(m_Phase < PHASE_REACH)
		return;

	float Reach = 1.f;
	if(m_Phase == PHASE_REACH)
		Reach = PhaseProgress();
	else if(m_Phase == PHASE_SWALLOW)
		Reach = 1.f - PhaseProgress();

	for(int tent = 0; tent < NUM_TENTACLES; tent++)
	{
		vec2 Prev = TentaclePoint(tent, 0.f, Reach);
		for(int seg = 0; seg < NUM_TENTACLE_SEGS; seg++)
		{
			float t = (seg + 1) / (float)NUM_TENTACLE_SEGS;
			vec2 Cur = TentaclePoint(tent, t, Reach);
			const int Idx = NUM_HOLE_SIDE + NUM_HOLE_PARTICLES + tent * NUM_TENTACLE_SEGS + seg;
			if(!m_aId[Idx].has_value())
				continue;
			GameServer()->SnapLaserObject(Context, m_aId[Idx].value(), Prev, Cur, Server()->Tick() - 2, -1, LASERTYPE_SHOTGUN, 0, 0);
			Prev = Cur;
		}
	}
}

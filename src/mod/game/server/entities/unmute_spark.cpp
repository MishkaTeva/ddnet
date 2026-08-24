#include <algorithm>

#include <base/math.h>

#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "lasertext.h"
#include <game/server/player.h>
#include <game/server/entities/character.h>
#include "unmute_spark.h"

const int CUnmuteSpark::s_aPhaseTicks[NUM_PHASES] = {
	16, // charge ~0.32s
	12, // rise   ~0.24s
	24, // flash  ~0.48s
};

CUnmuteSpark::CUnmuteSpark(CGameWorld *pGameWorld, vec2 Pos, int Victim)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_UNMUTE_SPARK, false, Pos)
{
	m_Victim = Victim;
	m_Phase = PHASE_CHARGE;
	m_PhaseStartTick = Server()->Tick();
	m_TeamMask = CClientMask().set();
	m_Seed = Server()->Tick() + Victim * 73;

	for(int i = 0; i < NUM_IDS; i++)
		m_aId[i] = Server()->SnapNewId();

	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(pChr)
	{
		m_Pos = pChr->GetPos();
		m_TeamMask = pChr->TeamMask();
	}

	if(GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_UnmuteSpark = true;

	GameServer()->CreateLaserText(m_Pos + vec2(0.f, -70.f), m_Victim, "UNMUTE", 4, false);
	GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, m_TeamMask);
	GameWorld()->InsertEntity(this);
}

CUnmuteSpark::~CUnmuteSpark()
{
	for(int i = 0; i < NUM_IDS; i++)
		if(m_aId[i].has_value()) Server()->SnapFreeId(m_aId[i].value());
}

void CUnmuteSpark::Reset()
{
	if(m_Victim >= 0 && m_Victim < MAX_CLIENTS && GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_UnmuteSpark = false;
	m_MarkedForDestroy = true;
}

float CUnmuteSpark::PhaseProgress()
{
	int Duration = s_aPhaseTicks[m_Phase];
	if(Duration <= 0)
		return 1.f;
	return std::clamp((Server()->Tick() - m_PhaseStartTick) / (float)Duration, 0.f, 1.f);
}

void CUnmuteSpark::AdvancePhase()
{
	m_Phase++;
	m_PhaseStartTick = Server()->Tick();
	m_Seed += 29;

	if(m_Phase == PHASE_RISE)
	{
		GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, m_TeamMask);
		GameServer()->CreateHammerHit(m_Pos + vec2(0.f, -12.f), m_TeamMask);
	}
	if(m_Phase == PHASE_FLASH)
	{
		GameServer()->CreateSound(m_Pos + vec2(0.f, -180.f), SOUND_WEAPON_SPAWN, m_TeamMask);
		GameServer()->CreateExplosion(m_Pos + vec2(0.f, -200.f), -1, WEAPON_LASER, true, -1, m_TeamMask);
	}
	if(m_Phase >= NUM_PHASES)
		Finish();
}

void CUnmuteSpark::Finish()
{
	if(m_Victim >= 0 && m_Victim < MAX_CLIENTS && GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_UnmuteSpark = false;
	m_MarkedForDestroy = true;
}

void CUnmuteSpark::FollowVictim()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(!pChr)
		return;
	m_Pos = pChr->GetPos();
	m_TeamMask = pChr->TeamMask();
}

vec2 CUnmuteSpark::BoltPoint(int Seg, int NumSegs, float Jag)
{
	// Tee → sky (reverse of mute gag)
	float t = Seg / (float)std::max(1, NumSegs - 1);
	vec2 Hit = m_Pos + vec2(0.f, -10.f);
	vec2 Sky = m_Pos + vec2(0.f, -220.f);
	vec2 Base = mix(Hit, Sky, t);
	int H = m_Seed + Seg * 127;
	float Ox = ((H % 17) - 8) * Jag;
	float Oy = ((H % 11) - 5) * Jag * 0.35f;
	if(Seg == 0 || Seg == NumSegs - 1)
		Ox = Oy = 0.f;
	return Base + vec2(Ox, Oy);
}

void CUnmuteSpark::SnapLaser(int SnappingClient, int Id, vec2 From, vec2 To, int Type)
{
	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);
	GameServer()->SnapLaserObject(Context, Id, To, From, Server()->Tick(), -1, Type, 0, 0);
}

bool CUnmuteSpark::SnapLaserIdx(int SnappingClient, int &Idx, vec2 From, vec2 To, int Type)
{
	if(Idx < 0 || Idx >= NUM_IDS || !m_aId[Idx].has_value())
		return false;
	SnapLaser(SnappingClient, m_aId[Idx++].value(), From, To, Type);
	return true;
}

void CUnmuteSpark::Tick()
{
	if(m_MarkedForDestroy)
		return;

	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : 0;
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		if(pPlayer)
			pPlayer->m_UnmuteSpark = false;
		m_MarkedForDestroy = true;
		return;
	}

	FollowVictim();

	if(Server()->Tick() - m_PhaseStartTick >= s_aPhaseTicks[m_Phase])
		AdvancePhase();
}

void CUnmuteSpark::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	const bool DDNetLaser = false; // unused; SnapLaserObject selects format
	int Idx = 0;
	float P = PhaseProgress();

	if(m_Phase == PHASE_CHARGE)
	{
		// Sparks rising off the tee
		float Spread = 28.f * P;
		float Lift = 40.f * P;
		for(int i = 0; i < NUM_FLASH; i++)
		{
			float A = i * (2.f * pi / NUM_FLASH) + Server()->Tick() * 0.3f;
			vec2 APos = m_Pos + vec2(cosf(A) * Spread, -Lift + sinf(A) * Spread * 0.3f);
			vec2 BPos = APos + vec2(cosf(A + 0.5f) * 10.f, -12.f - i * 2.f);
			if(!SnapLaserIdx(SnappingClient, Idx, APos, BPos, LASERTYPE_SHOTGUN))
				return;
		}
		return;
	}

	float Jag = (m_Phase == PHASE_RISE) ? 6.f : 3.5f;
	float Reveal = (m_Phase == PHASE_RISE) ? P : 1.f;
	int VisSegs = std::max(2, (int)(NUM_BOLT_SEGS * Reveal));
	for(int i = 0; i < VisSegs - 1; i++)
	{
		vec2 A = BoltPoint(i, NUM_BOLT_SEGS, Jag);
		vec2 B = BoltPoint(i + 1, NUM_BOLT_SEGS, Jag);
		if(!SnapLaserIdx(SnappingClient, Idx, A, B, LASERTYPE_RIFLE))
			return;
	}

	if(m_Phase >= PHASE_RISE)
	{
		for(int b = 0; b < NUM_BRANCHES; b++)
		{
			int Seg = 2 + (b * 2) % (NUM_BOLT_SEGS - 3);
			if(Seg >= VisSegs - 1)
				continue;
			vec2 Root = BoltPoint(Seg, NUM_BOLT_SEGS, Jag);
			float Dir = (b % 2) ? 1.f : -1.f;
			vec2 Tip = Root + vec2(Dir * (16.f + b * 5.f), -12.f - b * 4.f);
			if(!SnapLaserIdx(SnappingClient, Idx, Root, Tip, LASERTYPE_FREEZE))
				return;
			vec2 Tip2 = Tip + vec2(Dir * 8.f, -10.f);
			if(!SnapLaserIdx(SnappingClient, Idx, Tip, Tip2, LASERTYPE_SHOTGUN))
				return;
		}
	}

	// Sky burst when bolt arrives
	if(m_Phase == PHASE_FLASH || (m_Phase == PHASE_RISE && P > 0.75f))
	{
		vec2 Cloud = m_Pos + vec2(0.f, -210.f);
		float R = 24.f * ((m_Phase == PHASE_FLASH) ? (1.f - P * 0.45f) : P);
		float Step = 2.f * pi / NUM_FLASH;
		for(int i = 0; i < NUM_FLASH; i++)
		{
			float A0 = Step * i;
			float A1 = Step * (i + 1);
			vec2 A = Cloud + vec2(cosf(A0) * R, sinf(A0) * R * 0.4f);
			vec2 B = Cloud + vec2(cosf(A1) * R, sinf(A1) * R * 0.4f);
			if(!SnapLaserIdx(SnappingClient, Idx, A, B, LASERTYPE_SHOTGUN))
				return;
		}
	}
}

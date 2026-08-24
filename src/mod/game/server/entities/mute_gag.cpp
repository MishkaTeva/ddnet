#include <algorithm>

#include <base/math.h>

#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "lasertext.h"
#include <game/server/player.h>
#include <game/server/entities/character.h>
#include "mute_gag.h"

const int CMuteGag::s_aPhaseTicks[NUM_PHASES] = {
	18, // charge ~0.36s
	12, // strike ~0.24s
	28, // flash  ~0.56s
};

CMuteGag::CMuteGag(CGameWorld *pGameWorld, vec2 Pos, int Victim)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_MUTE_GAG, false, Pos)
{
	m_Victim = Victim;
	m_Phase = PHASE_CHARGE;
	m_PhaseStartTick = Server()->Tick();
	m_TeamMask = CClientMask().set();
	m_Seed = Server()->Tick() + Victim * 97;

	for(int i = 0; i < NUM_IDS; i++)
		m_aId[i] = Server()->SnapNewId();

	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(pChr)
	{
		m_Pos = pChr->GetPos();
		m_TeamMask = pChr->TeamMask();
	}

	if(GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_MuteGag = true;

	GameServer()->CreateLaserText(m_Pos + vec2(8.f, -70.f), m_Victim, "MUTE", 3, false);
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, m_TeamMask);
	GameWorld()->InsertEntity(this);
}

CMuteGag::~CMuteGag()
{
	for(int i = 0; i < NUM_IDS; i++)
		if(m_aId[i].has_value()) Server()->SnapFreeId(m_aId[i].value());
}

void CMuteGag::Reset()
{
	if(m_Victim >= 0 && m_Victim < MAX_CLIENTS && GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_MuteGag = false;
	m_MarkedForDestroy = true;
}

float CMuteGag::PhaseProgress()
{
	int Duration = s_aPhaseTicks[m_Phase];
	if(Duration <= 0)
		return 1.f;
	return std::clamp((Server()->Tick() - m_PhaseStartTick) / (float)Duration, 0.f, 1.f);
}

void CMuteGag::AdvancePhase()
{
	m_Phase++;
	m_PhaseStartTick = Server()->Tick();
	m_Seed += 31;

	if(m_Phase == PHASE_STRIKE)
	{
		GameServer()->CreateSound(m_Pos, SOUND_GRENADE_EXPLODE, m_TeamMask);
		GameServer()->CreateExplosion(m_Pos + vec2(0.f, -8.f), -1, WEAPON_LASER, true, -1, m_TeamMask);
		GameServer()->CreateHammerHit(m_Pos, m_TeamMask);
	}
	if(m_Phase == PHASE_FLASH)
		GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, m_TeamMask);
	if(m_Phase >= NUM_PHASES)
		Finish();
}

void CMuteGag::Finish()
{
	if(m_Victim >= 0 && m_Victim < MAX_CLIENTS && GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_MuteGag = false;
	m_MarkedForDestroy = true;
}

void CMuteGag::FollowVictim()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(!pChr)
		return;
	m_Pos = pChr->GetPos();
	m_TeamMask = pChr->TeamMask();
}

vec2 CMuteGag::BoltPoint(int Seg, int NumSegs, float Jag)
{
	float t = Seg / (float)std::max(1, NumSegs - 1);
	vec2 Sky = m_Pos + vec2(0.f, -220.f);
	vec2 Hit = m_Pos + vec2(0.f, -10.f);
	vec2 Base = mix(Sky, Hit, t);
	// Deterministic jagged offset
	int H = m_Seed + Seg * 131;
	float Ox = ((H % 17) - 8) * Jag;
	float Oy = ((H % 11) - 5) * Jag * 0.35f;
	if(Seg == 0 || Seg == NumSegs - 1)
		Ox = Oy = 0.f;
	return Base + vec2(Ox, Oy);
}

void CMuteGag::SnapLaser(int SnappingClient, int Id, vec2 From, vec2 To, int Type)
{
	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);
	GameServer()->SnapLaserObject(Context, Id, To, From, Server()->Tick(), -1, Type, 0, 0);
}

bool CMuteGag::SnapLaserIdx(int SnappingClient, int &Idx, vec2 From, vec2 To, int Type)
{
	if(Idx < 0 || Idx >= NUM_IDS || !m_aId[Idx].has_value())
		return false;
	SnapLaser(SnappingClient, m_aId[Idx++].value(), From, To, Type);
	return true;
}

void CMuteGag::Tick()
{
	if(m_MarkedForDestroy)
		return;

	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : 0;
	if(!pPlayer || !pPlayer->GetCharacter())
	{
		if(pPlayer)
			pPlayer->m_MuteGag = false;
		m_MarkedForDestroy = true;
		return;
	}

	FollowVictim();

	if(Server()->Tick() - m_PhaseStartTick >= s_aPhaseTicks[m_Phase])
		AdvancePhase();
}

void CMuteGag::Snap(int SnappingClient)
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
		// Sparks gathering above the tee
		vec2 Cloud = m_Pos + vec2(0.f, -180.f);
		float Spread = 40.f * P;
		for(int i = 0; i < NUM_FLASH; i++)
		{
			float A = i * (2.f * pi / NUM_FLASH) + Server()->Tick() * 0.25f;
			vec2 APos = Cloud + vec2(cosf(A) * Spread, sinf(A) * Spread * 0.4f);
			vec2 BPos = Cloud + vec2(cosf(A + 0.4f) * Spread * 0.5f, sinf(A + 0.4f) * Spread * 0.2f);
			if(!SnapLaserIdx(SnappingClient, Idx, APos, BPos, LASERTYPE_SHOTGUN))
				return;
		}
		return;
	}

	// Main jagged bolt sky → tee
	float Jag = (m_Phase == PHASE_STRIKE) ? 7.f : 4.f;
	float Reveal = (m_Phase == PHASE_STRIKE) ? P : 1.f;
	int VisSegs = std::max(2, (int)(NUM_BOLT_SEGS * Reveal));
	for(int i = 0; i < VisSegs - 1; i++)
	{
		vec2 A = BoltPoint(i, NUM_BOLT_SEGS, Jag);
		vec2 B = BoltPoint(i + 1, NUM_BOLT_SEGS, Jag);
		if(!SnapLaserIdx(SnappingClient, Idx, A, B, LASERTYPE_RIFLE))
			return;
	}

	// Side branches during strike/flash
	if(m_Phase >= PHASE_STRIKE)
	{
		for(int b = 0; b < NUM_BRANCHES; b++)
		{
			int Seg = 2 + (b * 2) % (NUM_BOLT_SEGS - 3);
			if(Seg >= VisSegs - 1)
				continue;
			vec2 Root = BoltPoint(Seg, NUM_BOLT_SEGS, Jag);
			float Dir = (b % 2) ? 1.f : -1.f;
			vec2 Tip = Root + vec2(Dir * (18.f + b * 6.f), 14.f + b * 4.f);
			if(!SnapLaserIdx(SnappingClient, Idx, Root, Tip, LASERTYPE_FREEZE))
				return;
			vec2 Tip2 = Tip + vec2(Dir * 10.f, 8.f);
			if(!SnapLaserIdx(SnappingClient, Idx, Tip, Tip2, LASERTYPE_SHOTGUN))
				return;
		}
	}

	// Ground flash ring on impact
	if(m_Phase == PHASE_FLASH || (m_Phase == PHASE_STRIKE && P > 0.7f))
	{
		float R = 28.f * ((m_Phase == PHASE_FLASH) ? (1.f - P * 0.5f) : P);
		float Step = 2.f * pi / NUM_FLASH;
		for(int i = 0; i < NUM_FLASH; i++)
		{
			float A0 = Step * i;
			float A1 = Step * (i + 1);
			vec2 A = m_Pos + vec2(cosf(A0) * R, sinf(A0) * R * 0.35f - 4.f);
			vec2 B = m_Pos + vec2(cosf(A1) * R, sinf(A1) * R * 0.35f - 4.f);
			if(!SnapLaserIdx(SnappingClient, Idx, A, B, LASERTYPE_SHOTGUN))
				return;
		}
	}
}

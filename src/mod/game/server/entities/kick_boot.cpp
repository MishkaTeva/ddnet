#include <algorithm>

#include <base/math.h>

#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include "lasertext.h"
#include <game/server/player.h>
#include <game/server/entities/character.h>
#include "kick_boot.h"

const int CKickBoot::s_aPhaseTicks[NUM_PHASES] = {
	22, // windup ~0.44s
	14, // swing  ~0.28s
	28, // fly    ~0.56s
};

CKickBoot::CKickBoot(CGameWorld *pGameWorld, vec2 Pos, int Victim, const char *pReason)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_KICK_BOOT, false, Pos)
{
	m_Victim = Victim;
	str_copy(m_aReason, pReason ? pReason : "No reason given", sizeof(m_aReason));
	m_StartPos = Pos;
	m_Phase = PHASE_WINDUP;
	m_PhaseStartTick = Server()->Tick();
	m_TeamMask = CClientMask().set();

	for(int i = 0; i < NUM_IDS; i++)
		m_aId[i] = Server()->SnapNewId();

	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(pChr)
	{
		m_StartPos = pChr->GetPos();
		m_Pos = m_StartPos;
		m_TeamMask = pChr->TeamMask();
		pChr->Freeze(3);
		pChr->Core()->m_Vel = vec2(0.f, 0.f);
	}

	if(GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_KickBoot = true;

	GameServer()->CreateLaserText(m_StartPos + vec2(10.f, -70.f), m_Victim, "KICK", 3, false);
	GameServer()->CreateSound(m_StartPos, SOUND_WEAPON_SPAWN, m_TeamMask);
	GameWorld()->InsertEntity(this);
}

CKickBoot::~CKickBoot()
{
	for(int i = 0; i < NUM_IDS; i++)
		if(m_aId[i].has_value()) Server()->SnapFreeId(m_aId[i].value());
}

void CKickBoot::Reset()
{
	if(m_Victim >= 0 && m_Victim < MAX_CLIENTS && GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_KickBoot = false;
	m_MarkedForDestroy = true;
}

float CKickBoot::PhaseProgress()
{
	int Duration = s_aPhaseTicks[m_Phase];
	if(Duration <= 0)
		return 1.f;
	return std::clamp((Server()->Tick() - m_PhaseStartTick) / (float)Duration, 0.f, 1.f);
}

void CKickBoot::AdvancePhase()
{
	m_Phase++;
	m_PhaseStartTick = Server()->Tick();

	if(m_Phase == PHASE_SWING)
		GameServer()->CreateSound(m_StartPos, SOUND_HAMMER_FIRE, m_TeamMask);
	if(m_Phase == PHASE_FLY)
	{
		GameServer()->CreateHammerHit(m_StartPos, m_TeamMask);
		GameServer()->CreateSound(m_StartPos, SOUND_PLAYER_PAIN_SHORT, m_TeamMask);
		GameServer()->CreateExplosion(m_StartPos, -1, WEAPON_HAMMER, true, -1, m_TeamMask);
	}
	if(m_Phase >= NUM_PHASES)
		Finish();
}

void CKickBoot::Finish()
{
	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : 0;
	if(pPlayer)
	{
		pPlayer->m_KickBoot = false;
		if(Server()->ClientIngame(m_Victim))
			Server()->Kick(m_Victim, m_aReason[0] ? m_aReason : "Kicked");
	}

	GameServer()->CreateDeath(m_Pos, m_Victim, m_TeamMask);
	GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, m_TeamMask);
	m_MarkedForDestroy = true;
}

vec2 CKickBoot::BootPos()
{
	float P = PhaseProgress();
	vec2 Back = m_StartPos + vec2(-70.f, -10.f);
	vec2 Contact = m_StartPos + vec2(-8.f, 4.f);
	vec2 Follow = m_StartPos + vec2(80.f, -40.f);

	if(m_Phase == PHASE_WINDUP)
		return mix(Back + vec2(-20.f, 10.f), Back, P * P);
	if(m_Phase == PHASE_SWING)
		return mix(Back, Contact, P * P);
	return mix(Contact, Follow, P);
}

void CKickBoot::UpdateVictimControl()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(!pChr)
		return;

	m_TeamMask = pChr->TeamMask();
	pChr->Freeze(2);
	float P = PhaseProgress();

	if(m_Phase == PHASE_WINDUP || m_Phase == PHASE_SWING)
	{
		pChr->SetPosition(m_StartPos);
		pChr->Core()->m_Vel = vec2(0.f, 0.f);
		m_Pos = m_StartPos;
	}
	else // FLY
	{
		vec2 Fly = mix(m_StartPos, m_StartPos + vec2(110.f, -90.f), P * P);
		pChr->SetPosition(Fly);
		pChr->Core()->m_Vel = vec2(18.f, -12.f);
		m_Pos = Fly;
		if((Server()->Tick() % 3) == 0)
			GameServer()->CreateDeath(Fly, m_Victim, m_TeamMask);
	}
}

void CKickBoot::SnapLaser(int SnappingClient, int Id, vec2 From, vec2 To, int Type)
{
	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);
	GameServer()->SnapLaserObject(Context, Id, To, From, Server()->Tick(), -1, Type, 0, 0);
}

bool CKickBoot::SnapLaserIdx(int SnappingClient, int &Idx, vec2 From, vec2 To, int Type)
{
	if(Idx < 0 || Idx >= NUM_IDS || !m_aId[Idx].has_value())
		return false;
	SnapLaser(SnappingClient, m_aId[Idx++].value(), From, To, Type);
	return true;
}

void CKickBoot::Tick()
{
	if(m_MarkedForDestroy)
		return;

	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : 0;
	if(!pPlayer)
	{
		m_MarkedForDestroy = true;
		return;
	}

	if(!pPlayer->GetCharacter())
	{
		pPlayer->m_KickBoot = false;
		if(Server()->ClientIngame(m_Victim))
			Server()->Kick(m_Victim, m_aReason[0] ? m_aReason : "Kicked");
		m_MarkedForDestroy = true;
		return;
	}

	UpdateVictimControl();

	if(Server()->Tick() - m_PhaseStartTick >= s_aPhaseTicks[m_Phase])
		AdvancePhase();
}

void CKickBoot::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, BootPos()))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	const bool DDNetLaser = false; // unused; SnapLaserObject selects format
	int Idx = 0;
	vec2 Boot = BootPos();

	// Side-profile boot outline (sole + upper)
	vec2 Heel = Boot + vec2(-10.f, 8.f);
	vec2 Toe = Boot + vec2(22.f, 10.f);
	vec2 Ankle = Boot + vec2(-4.f, -6.f);
	vec2 Top = Boot + vec2(8.f, -14.f);
	vec2 CuffL = Boot + vec2(-2.f, -22.f);
	vec2 CuffR = Boot + vec2(10.f, -20.f);

	if(!SnapLaserIdx(SnappingClient, Idx, Heel, Toe, LASERTYPE_DOOR)) return;       // sole
	if(!SnapLaserIdx(SnappingClient, Idx, Toe, Toe + vec2(-2.f, -8.f), LASERTYPE_DOOR)) return;
	if(!SnapLaserIdx(SnappingClient, Idx, Toe + vec2(-2.f, -8.f), Top, LASERTYPE_DOOR)) return;
	if(!SnapLaserIdx(SnappingClient, Idx, Top, Ankle, LASERTYPE_DOOR)) return;
	if(!SnapLaserIdx(SnappingClient, Idx, Ankle, Heel, LASERTYPE_DOOR)) return;
	if(!SnapLaserIdx(SnappingClient, Idx, Ankle, CuffL, LASERTYPE_FREEZE)) return;  // leg cuff
	if(!SnapLaserIdx(SnappingClient, Idx, Top, CuffR, LASERTYPE_FREEZE)) return;
	if(!SnapLaserIdx(SnappingClient, Idx, CuffL, CuffR, LASERTYPE_FREEZE)) return;

	// Heel block
	if(!SnapLaserIdx(SnappingClient, Idx, Heel, Heel + vec2(-6.f, 0.f), LASERTYPE_RIFLE)) return;
	if(!SnapLaserIdx(SnappingClient, Idx, Heel + vec2(-6.f, 0.f), Heel + vec2(-6.f, -10.f), LASERTYPE_RIFLE)) return;

	// Motion trail during swing/fly
	if(m_Phase >= PHASE_SWING)
	{
		for(int i = 0; i < NUM_TRAIL; i++)
		{
			float t = (i + 1) / (float)(NUM_TRAIL + 1);
			vec2 Trail = mix(m_StartPos + vec2(-70.f, -10.f), Boot, 1.f - t * 0.65f);
			vec2 Trail2 = Trail + vec2(-8.f, 4.f);
			if(!SnapLaserIdx(SnappingClient, Idx, Trail, Trail2, LASERTYPE_SHOTGUN))
				return;
		}
	}
}

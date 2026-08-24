#include <algorithm>

#include <base/math.h>

#include <generated/protocol.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/entities/character.h>
#include "lasertext.h"
#include "ban_plunger_toilet.h"

const int CBanPlungerToilet::s_aPhaseTicks[NUM_PHASES] = {
	50, // lift   ~1.0s
	30, // appear ~0.6s
	45, // press  ~0.9s
	40, // flush  ~0.8s
};

CBanPlungerToilet::CBanPlungerToilet(CGameWorld *pGameWorld, vec2 AirPos, int Victim, int BanSeconds, const char *pReason)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_BAN_PLUNGER_TOILET, false, AirPos)
{
	m_Victim = Victim;
	m_BanSeconds = BanSeconds;
	str_copy(m_aReason, pReason ? pReason : "No reason given", sizeof(m_aReason));
	m_AirPos = AirPos;
	m_VictimStartPos = AirPos;

	m_Phase = PHASE_LIFT;
	m_PhaseStartTick = Server()->Tick();
	m_TeamMask = CClientMask().set();

	for(int i = 0; i < NUM_IDS; i++)
		m_aId[i] = Server()->SnapNewId();

	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(pChr)
	{
		m_VictimStartPos = pChr->GetPos();
		m_TeamMask = pChr->TeamMask();
		pChr->Freeze(5);
		pChr->Core()->m_Vel = vec2(0.f, 0.f);
	}

	if(GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_BanPlungerToilet = true;

	GameServer()->CreateLaserText(m_AirPos + vec2(10.f, -100.f), m_Victim, "BAN", 5, false);
	GameServer()->CreateSound(m_VictimStartPos, SOUND_WEAPON_SPAWN, m_TeamMask);
	GameWorld()->InsertEntity(this);
}

CBanPlungerToilet::~CBanPlungerToilet()
{
	for(int i = 0; i < NUM_IDS; i++)
		if(m_aId[i].has_value()) Server()->SnapFreeId(m_aId[i].value());
}

void CBanPlungerToilet::Reset()
{
	if(m_Victim >= 0 && m_Victim < MAX_CLIENTS && GameServer()->m_apPlayers[m_Victim])
		GameServer()->m_apPlayers[m_Victim]->m_BanPlungerToilet = false;
	m_MarkedForDestroy = true;
}

float CBanPlungerToilet::PhaseProgress()
{
	int Duration = s_aPhaseTicks[m_Phase];
	if(Duration <= 0)
		return 1.f;
	return std::clamp((Server()->Tick() - m_PhaseStartTick) / (float)Duration, 0.f, 1.f);
}

float CBanPlungerToilet::LiftAmount()
{
	if(m_Phase == PHASE_LIFT)
	{
		float P = PhaseProgress();
		return P * P * (3.f - 2.f * P); // smoothstep
	}
	return 1.f;
}

vec2 CBanPlungerToilet::SceneAnchor()
{
	return mix(m_VictimStartPos, m_AirPos, LiftAmount());
}

vec2 CBanPlungerToilet::ToiletOrigin()
{
	// Pedestal floor of the side-profile toilet, under/slightly behind the tee
	return SceneAnchor() + vec2(-6.f, 46.f);
}

void CBanPlungerToilet::AdvancePhase()
{
	m_Phase++;
	m_PhaseStartTick = Server()->Tick();

	if(m_Phase == PHASE_PRESS)
		GameServer()->CreateSound(SceneAnchor(), SOUND_HAMMER_FIRE, m_TeamMask);

	if(m_Phase == PHASE_FLUSH)
	{
		GameServer()->CreateHammerHit(SceneAnchor(), m_TeamMask);
		GameServer()->CreateSound(SceneAnchor(), SOUND_HOOK_LOOP, m_TeamMask);
	}

	if(m_Phase >= NUM_PHASES)
		Finish();
}

void CBanPlungerToilet::Finish()
{
	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : 0;
	if(pPlayer)
	{
		pPlayer->m_BanPlungerToilet = false;
		// Only queue ban if still connected — otherwise animation already ended without a ban target
		if(Server()->ClientIngame(m_Victim))
			Server()->Ban(m_Victim, m_BanSeconds, m_aReason, true);
	}

	vec2 DeathPos = SceneAnchor() + vec2(18.f, 20.f);
	GameServer()->CreateDeath(DeathPos, m_Victim, m_TeamMask);
	GameServer()->CreateHammerHit(DeathPos, m_TeamMask);
	GameServer()->CreateSound(DeathPos, SOUND_PLAYER_DIE, m_TeamMask);

	m_MarkedForDestroy = true;
}

vec2 CBanPlungerToilet::PlungerCupPos()
{
	float Progress = PhaseProgress();
	vec2 Above = SceneAnchor() + vec2(18.f, -70.f);
	vec2 OnHead = SceneAnchor() + vec2(18.f, -26.f);
	vec2 InBowl = SceneAnchor() + vec2(22.f, 16.f);

	if(m_Phase == PHASE_LIFT)
		return Above + vec2(0.f, -20.f * (1.f - LiftAmount()));
	if(m_Phase == PHASE_APPEAR)
		return mix(Above + vec2(0.f, -18.f), Above, Progress);
	if(m_Phase == PHASE_PRESS)
		return mix(Above, OnHead, Progress * Progress);
	return mix(OnHead, InBowl, Progress);
}

void CBanPlungerToilet::UpdateVictimControl()
{
	CCharacter *pChr = GameServer()->GetPlayerChar(m_Victim);
	if(!pChr)
		return;

	m_TeamMask = pChr->TeamMask();
	pChr->Freeze(2);
	pChr->Core()->m_Vel = vec2(0.f, 0.f);

	float Progress = PhaseProgress();
	vec2 NewPos = SceneAnchor();

	if(m_Phase == PHASE_LIFT || m_Phase == PHASE_APPEAR)
	{
		NewPos = SceneAnchor();
	}
	else if(m_Phase == PHASE_PRESS)
	{
		NewPos = mix(SceneAnchor(), SceneAnchor() + vec2(18.f, 22.f), Progress * Progress);
		if((Server()->Tick() % 3) == 0)
			GameServer()->CreateHammerHit(NewPos + vec2(0.f, -14.f), m_TeamMask);
	}
	else if(m_Phase == PHASE_FLUSH)
	{
		NewPos = mix(SceneAnchor() + vec2(18.f, 22.f), SceneAnchor() + vec2(22.f, 40.f), Progress);
		if((Server()->Tick() % 4) == 0)
			GameServer()->CreateDeath(NewPos, m_Victim, m_TeamMask);
	}

	pChr->SetPosition(NewPos);
	pChr->Core()->m_Vel = vec2(0.f, 0.f);
	m_Pos = NewPos;
}

void CBanPlungerToilet::SnapLaser(int SnappingClient, int Id, vec2 From, vec2 To, int Type)
{
	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);
	GameServer()->SnapLaserObject(Context, Id, To, From, Server()->Tick(), -1, Type, 0, 0);
}

bool CBanPlungerToilet::SnapLaserIdx(int SnappingClient, int &Idx, vec2 From, vec2 To, int Type)
{
	if(Idx < 0 || Idx >= NUM_IDS || !m_aId[Idx].has_value())
		return false;
	SnapLaser(SnappingClient, m_aId[Idx++].value(), From, To, Type);
	return true;
}

bool CBanPlungerToilet::FindPos(CGameContext *pGameServer, vec2 From, vec2 *pOut)
{
	// Prefer a modest lift into open air; keep checks light so the effect actually starts
	const float aUp[] = {56.f, 48.f, 64.f, 72.f, 40.f, 80.f, 96.f, 112.f, 128.f};
	const float aSide[] = {0.f, -24.f, 24.f, -48.f, 48.f};
	const vec2 PlayerSize(28.f, 28.f);

	for(int u = 0; u < (int)(sizeof(aUp) / sizeof(aUp[0])); u++)
	{
		for(int s = 0; s < (int)(sizeof(aSide) / sizeof(aSide[0])); s++)
		{
			vec2 AirPlayer = From + vec2(aSide[s], -aUp[u]);
			if(pGameServer->Collision()->TestBox(AirPlayer, PlayerSize))
				continue;
			if(pGameServer->Collision()->IntersectLine(From, AirPlayer, 0, 0))
				continue;

			*pOut = AirPlayer;
			return true;
		}
	}

	// Always allow the effect — even if space is tight
	*pOut = From + vec2(0.f, -56.f);
	if(pGameServer->Collision()->TestBox(*pOut, PlayerSize))
		*pOut = From;
	return true;
}

void CBanPlungerToilet::Tick()
{
	if(m_MarkedForDestroy)
		return;

	CPlayer *pPlayer = (m_Victim >= 0 && m_Victim < MAX_CLIENTS) ? GameServer()->m_apPlayers[m_Victim] : 0;
	if(!pPlayer)
	{
		m_MarkedForDestroy = true;
		return;
	}

	// Character gone mid-FX (disconnect/kill) — still apply deferred ban if player object lives
	if(!pPlayer->GetCharacter())
	{
		pPlayer->m_BanPlungerToilet = false;
		if(Server()->ClientIngame(m_Victim))
			Server()->Ban(m_Victim, m_BanSeconds, m_aReason, true);
		m_MarkedForDestroy = true;
		return;
	}

	UpdateVictimControl();

	if(Server()->Tick() - m_PhaseStartTick >= s_aPhaseTicks[m_Phase])
		AdvancePhase();
}

void CBanPlungerToilet::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, SceneAnchor()))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	/* SnapLaserObject picks format */

	// Fade toilet in while lifting
	float Appear = LiftAmount();
	if(Appear < 0.05f)
		return;

	float S = Appear; // scale
	vec2 O = ToiletOrigin(); // pedestal floor point
	int Type = LASERTYPE_DOOR;
	int Idx = 0;

	/*
	 * Side-profile toilet facing RIGHT (like the reference photo):
	 *
	 *   [ tank ]
	 *   |     |__/ lid / seat ~~~~~~~~|
	 *   |_____|    )___________/
	 *          \  pedestal  /
	 *           |__________|
	 */

	// --- Tank (cistern) on the left ---
	vec2 TankTL = O + vec2(-38.f, -98.f) * S;
	vec2 TankTR = O + vec2(-8.f, -98.f) * S;
	vec2 TankBL = O + vec2(-38.f, -58.f) * S;
	vec2 TankBR = O + vec2(-8.f, -58.f) * S;
	SnapLaserIdx(SnappingClient, Idx, TankTL, TankTR, Type); // lid top
	SnapLaserIdx(SnappingClient, Idx, TankTL, TankBL, Type); // back
	SnapLaserIdx(SnappingClient, Idx, TankTR, TankBR, Type); // front
	SnapLaserIdx(SnappingClient, Idx, TankBL, TankBR, Type); // bottom

	// --- Closed seat + lid on the bowl (flat, pointing right) ---
	vec2 SeatBack = O + vec2(-8.f, -58.f) * S;
	vec2 SeatFront = O + vec2(48.f, -56.f) * S;
	vec2 LidBack = O + vec2(-8.f, -64.f) * S;
	vec2 LidFront = O + vec2(46.f, -62.f) * S;
	SnapLaserIdx(SnappingClient, Idx, LidBack, LidFront, Type);   // lid
	SnapLaserIdx(SnappingClient, Idx, SeatBack, SeatFront, Type); // seat

	// --- Bowl outer silhouette (rounded underside, extends right) ---
	vec2 Lip = O + vec2(52.f, -50.f) * S;          // front lip
	vec2 BowlFront = O + vec2(56.f, -28.f) * S;    // front curve
	vec2 BowlBottom = O + vec2(28.f, -14.f) * S;   // lowest bowl point
	vec2 BowlBack = O + vec2(2.f, -22.f) * S;      // back of bowl shell
	SnapLaserIdx(SnappingClient, Idx, SeatFront, Lip, Type);
	SnapLaserIdx(SnappingClient, Idx, Lip, BowlFront, Type);
	SnapLaserIdx(SnappingClient, Idx, BowlFront, BowlBottom, Type);
	SnapLaserIdx(SnappingClient, Idx, BowlBottom, BowlBack, Type);
	SnapLaserIdx(SnappingClient, Idx, BowlBack, TankBR, Type);

	// Inner bowl line (water / hollow look)
	vec2 InnerFront = O + vec2(42.f, -48.f) * S;
	vec2 InnerBottom = O + vec2(26.f, -28.f) * S;
	SnapLaserIdx(SnappingClient, Idx, SeatFront + vec2(-6.f, 4.f) * S, InnerFront, LASERTYPE_FREEZE);
	SnapLaserIdx(SnappingClient, Idx, InnerFront, InnerBottom, LASERTYPE_FREEZE);

	// --- Pedestal / base ---
	vec2 NeckL = O + vec2(4.f, -18.f) * S;
	vec2 NeckR = O + vec2(30.f, -16.f) * S;
	vec2 BaseL = O + vec2(-4.f, 0.f) * S;
	vec2 BaseR = O + vec2(36.f, 0.f) * S;
	SnapLaserIdx(SnappingClient, Idx, BowlBack, NeckL, Type);
	SnapLaserIdx(SnappingClient, Idx, BowlBottom, NeckR, Type);
	SnapLaserIdx(SnappingClient, Idx, NeckL, BaseL, Type);
	SnapLaserIdx(SnappingClient, Idx, NeckR, BaseR, Type);
	SnapLaserIdx(SnappingClient, Idx, BaseL, BaseR, Type);

	// ---- Classic plunger (long wood handle + black bell cup) ----
	float PlungerAppear = 1.f;
	if(m_Phase == PHASE_LIFT)
		PlungerAppear = LiftAmount();
	else if(m_Phase == PHASE_APPEAR)
		PlungerAppear = 0.35f + 0.65f * PhaseProgress();

	vec2 Cup = PlungerCupPos();
	// Cup: black bell with flat wide rim (photo style)
	const float RimW = 30.f * PlungerAppear;   // flat bottom rim half-width
	const float CupH = 26.f * PlungerAppear;   // dome height
	const float NeckW = 7.f * PlungerAppear;   // collar where stick enters
	const float StickLen = CupH * 4.6f;        // handle ~4.5x cup height
	const float StickHalf = 3.5f * PlungerAppear;

	vec2 RimL = Cup + vec2(-RimW, CupH * 0.55f);
	vec2 RimR = Cup + vec2(RimW, CupH * 0.55f);
	vec2 CollarL = Cup + vec2(-NeckW, -CupH * 0.15f);
	vec2 CollarR = Cup + vec2(NeckW, -CupH * 0.15f);
	vec2 CollarTopL = Cup + vec2(-NeckW, -CupH * 0.35f);
	vec2 CollarTopR = Cup + vec2(NeckW, -CupH * 0.35f);
	// Dome shoulders (flare from collar down to rim)
	vec2 ShoulderL = Cup + vec2(-RimW * 0.85f, CupH * 0.15f);
	vec2 ShoulderR = Cup + vec2(RimW * 0.85f, CupH * 0.15f);

	int CupType = LASERTYPE_DOOR; // dark = rubber
	SnapLaserIdx(SnappingClient, Idx, RimL, RimR, CupType);           // flat rim
	SnapLaserIdx(SnappingClient, Idx, CollarL, ShoulderL, CupType);   // left dome
	SnapLaserIdx(SnappingClient, Idx, ShoulderL, RimL, CupType);
	SnapLaserIdx(SnappingClient, Idx, CollarR, ShoulderR, CupType);   // right dome
	SnapLaserIdx(SnappingClient, Idx, ShoulderR, RimR, CupType);
	SnapLaserIdx(SnappingClient, Idx, CollarL, CollarR, CupType);     // collar bottom
	SnapLaserIdx(SnappingClient, Idx, CollarTopL, CollarTopR, CupType); // collar top
	SnapLaserIdx(SnappingClient, Idx, CollarTopL, CollarL, CupType);
	SnapLaserIdx(SnappingClient, Idx, CollarTopR, CollarR, CupType);

	// Wooden handle (rifle laser = warm/wood look), long & straight
	int WoodType = LASERTYPE_RIFLE;
	vec2 StickBot = Cup + vec2(0.f, -CupH * 0.35f);
	vec2 StickTop = StickBot + vec2(0.f, -StickLen);
	SnapLaserIdx(SnappingClient, Idx, StickBot + vec2(-StickHalf, 0.f), StickTop + vec2(-StickHalf, 0.f), WoodType);
	SnapLaserIdx(SnappingClient, Idx, StickBot + vec2(StickHalf, 0.f), StickTop + vec2(StickHalf, 0.f), WoodType);
	SnapLaserIdx(SnappingClient, Idx, StickBot, StickTop, WoodType);
	// Rounded tip of the handle
	SnapLaserIdx(SnappingClient, Idx, StickTop + vec2(-StickHalf, 0.f), StickTop + vec2(StickHalf, 0.f), WoodType);
	SnapLaserIdx(SnappingClient, Idx, StickTop + vec2(-StickHalf * 0.6f, -StickHalf), StickTop + vec2(StickHalf * 0.6f, -StickHalf), WoodType);
}

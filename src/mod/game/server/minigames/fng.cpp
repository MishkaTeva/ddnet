// FNG / BoomFNG — solo FFA or Team (red vs blue)
#include "fng.h"
#include <game/server/gamecontext.h>
#include <game/server/teams.h>
#include <game/server/gamecontroller.h>
#include <game/server/entities/character.h>
#include <game/server/player.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <generated/server_data.h>
#include <engine/shared/localization.h>
#include <game/mapitems.h>
#include <cstdarg>
#include <cstdlib>

#undef  FNG_ROUND_DURATION
#define FNG_ROUND_DURATION      0     // 0 = no time limit

#define FNG_WARMUP_DURATION     10    // practice seconds before round (no votes)
#define FNG_WARMUP_EXTEND       8

#define FNG_POSTROUND_DURATION  10
#define FNG_START_FREEZE        3

#define FNG_SCORELIMIT_DEFAULT  300
#define FNG_SCORELIMIT_MIN      50
#define FNG_SCORELIMIT_MAX      1500
#define FNG_SCORELIMIT_STEP     50

#undef  FNG_FREEZE_SECS
#define FNG_FREEZE_SECS         10

#undef  FNG_HAMMER_SCALE_X
#undef  FNG_HAMMER_SCALE_Y
#define FNG_HAMMER_SCALE_X      320
#define FNG_HAMMER_SCALE_Y      120

#undef  FNG_SCORE_SPIKE_NORMAL
#undef  FNG_SCORE_SPIKE_GOLD
#undef  FNG_SCORE_SPIKE_GREEN
#undef  FNG_SCORE_SPIKE_PURPLE
#undef  FNG_SCORE_SPIKE_RED
#undef  FNG_SCORE_SPIKE_BLUE
#define FNG_SCORE_SPIKE_NORMAL  3
#define FNG_SCORE_SPIKE_GOLD    8
#define FNG_SCORE_SPIKE_GREEN   6
#define FNG_SCORE_SPIKE_PURPLE  10
#define FNG_SCORE_SPIKE_TEAM    5

#define FNG_TEAM_SCORE_SPIKE_NORMAL  5
#define FNG_TEAM_SCORE_SPIKE_GOLD    12
#define FNG_TEAM_SCORE_SPIKE_GREEN   15
#define FNG_TEAM_SCORE_SPIKE_PURPLE  18
#define FNG_TEAM_SCORE_SPIKE_TEAM    10

#define FNG_MELT_HAMMER_SCALE_X 50
#define FNG_MELT_HAMMER_SCALE_Y 50
#define FNG_WRONG_SPIKE_FREEZE  5

#define FNG_MIN_PLAYERS         2

#define TEAMS (GameServer()->m_pController->Teams())

CFng::CFng(CGameContext *pGameServer, int Type) : CMinigame(pGameServer, Type)
{
	for(int i = 0; i < MAX_FNG_ROOMS; i++)
	{
		m_aRooms[i].Reset();
		m_aPending[i].Reset();
	}
	for(int i = 0; i < MAX_CLIENTS; i++) m_aClientRoom[i] = -1;
}

int  CFng::FindFreeRoom()        const { for(int i=0;i<MAX_FNG_ROOMS;i++) if(!m_aRooms[i].m_Active) return i; return -1; }
int  CFng::FindRoomByTeam(int T) const { for(int i=0;i<MAX_FNG_ROOMS;i++) if(m_aRooms[i].m_Active&&m_aRooms[i].m_Team==T) return i; return -1; }
int  CFng::GetClientRoom(int C)  const { return (C>=0&&C<MAX_CLIENTS)?m_aClientRoom[C]:-1; }
bool CFng::IsInRound(int C)      const { int r=GetClientRoom(C); return r>=0&&m_aRooms[r].m_State==FNG_STATE_ROUND; }
bool CFng::IsWaiting(int C)      const
{
	int R = GetClientRoom(C);
	return R >= 0 && m_aRooms[R].m_State == FNG_STATE_WAITING;
}
int  CFng::GetRoomTeam(int C)    const { int r=GetClientRoom(C); return r>=0?m_aRooms[r].m_Team:-1; }
int  CFng::CountPlayers(int R)   const { int n=0; for(int i=0;i<MAX_CLIENTS;i++) if(m_aClientRoom[i]==R) n++; return n; }
int  CFng::CountActivePlayers(int R) const
{
	int n = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(IsPlayingInRoom(i) && m_aClientRoom[i] == R)
			n++;
	return n;
}
bool CFng::IsPlayingInRoom(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClientRoom[ClientID] < 0)
		return false;
	CPlayer *p = GameServer()->m_apPlayers[ClientID];
	return p && p->GetTeam() != TEAM_SPECTATORS;
}
int  CFng::CountSide(int R, int Side) const { int n=0; for(int i=0;i<MAX_CLIENTS;i++) if(m_aClientRoom[i]==R && m_aRooms[R].m_aSide[i]==Side) n++; return n; }

void CFng::RestoreIsolationTeam(int ClientID, int R)
{
	if(R < 0 || !m_aRooms[R].m_Active)
		return;
	TEAMS.SetForceCharacterTeam(ClientID, m_aRooms[R].m_Team);
}

void CFng::AbortToWaiting(int R)
{
	CFngRoom &r = m_aRooms[R];
	if(!r.m_Active || r.m_State == FNG_STATE_WAITING)
		return;
	r.m_State = FNG_STATE_WAITING;
	r.m_TimerTick = 0;
	r.m_RoundStartTick = 0;
	KillAllInRoom(R);
	SyncGameInfoRoom(R);
}

void CFng::OnActivePlayersChanged(int R)
{
	if(R < 0 || !m_aRooms[R].m_Active)
		return;
	CFngRoom &r = m_aRooms[R];
	int n = CountActivePlayers(R);
	if((r.m_State == FNG_STATE_ROUND || r.m_State == FNG_STATE_WARMUP || r.m_State == FNG_STATE_POSTROUND) && n < FNG_MIN_PLAYERS)
		AbortToWaiting(R);
	else if(r.m_State == FNG_STATE_WAITING && n >= FNG_MIN_PLAYERS)
		StartWarmup(R);
}

void CFng::RefreshRoomClient(int ClientID, int R)
{
	SendScoreToAll(R);
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->m_apPlayers[i])
			TEAMS.SendTeamsState(i);
	GameServer()->m_pController->UpdateGameInfo(ClientID);
	GameServer()->UpdateHidePlayers(ClientID);
}

int CFng::GetSide(int ClientID) const
{
	int R = GetClientRoom(ClientID);
	if(R < 0) return FNG_SIDE_NONE;
	return m_aRooms[R].m_aSide[ClientID];
}

bool CFng::IsTeamMode(int ClientID) const
{
	int R = GetClientRoom(ClientID);
	return R >= 0 && m_aRooms[R].m_TeamMode;
}

int CFng::GetScoreLimit(int ClientID) const
{
	int R = GetClientRoom(ClientID);
	return R >= 0 ? m_aRooms[R].m_ScoreLimit : FNG_SCORELIMIT_DEFAULT;
}

int CFng::GetTeamPoints(int ClientID, int Side) const
{
	int R = GetClientRoom(ClientID);
	if(R < 0 || Side < 0 || Side > 1) return 0;
	return m_aRooms[R].m_aTeamPoints[Side];
}

bool CFng::IsInRoundOrWarmup(int ClientID) const
{
	int R = GetClientRoom(ClientID);
	if(R < 0) return false;
	return m_aRooms[R].m_State == FNG_STATE_ROUND || m_aRooms[R].m_State == FNG_STATE_WARMUP;
}

bool CFng::GetTuneReset(int ClientID) const
{
	int R = GetClientRoom(ClientID);
	return R >= 0 && m_aRooms[R].m_TuneReset;
}

bool CFng::GetTeamModeSetting(int ClientID) const
{
	return IsTeamMode(ClientID);
}

int CFng::GetScoreLimitSetting(int ClientID) const
{
	return GetScoreLimit(ClientID);
}

int64_t CFng::GetRoundStartTick(int ClientID) const
{
	int R = GetClientRoom(ClientID);
	if(R < 0 || m_aRooms[R].m_State != FNG_STATE_ROUND)
		return 0;
	return m_aRooms[R].m_RoundStartTick;
}

int CFng::GetSpawnEntity(int ClientID) const
{
	int R = GetClientRoom(ClientID);
	if(R < 0)
		return ENTITY_SPAWN_BLUE;
	if(m_aRooms[R].m_TeamMode)
	{
		int Side = m_aRooms[R].m_aSide[ClientID];
		if(Side == FNG_SIDE_RED)
			return ENTITY_SPAWN_RED;
		if(Side == FNG_SIDE_BLUE)
			return ENTITY_SPAWN_BLUE;
	}
	return (m_aRooms[R].m_MinigameType == MINIGAME_INSTAGIB_BOOMFNG) ? ENTITY_SPAWN_RED : ENTITY_SPAWN_BLUE;
}

bool CFng::SameSide(int A, int B, int R) const
{
	if(!m_aRooms[R].m_TeamMode) return false;
	int Sa = m_aRooms[R].m_aSide[A];
	int Sb = m_aRooms[R].m_aSide[B];
	return Sa != FNG_SIDE_NONE && Sa == Sb;
}

bool CFng::IsClientFrozen(int C) const
{
	CCharacter *p = GameServer()->GetPlayerChar(C);
	return p && p->m_FreezeTime > 0;
}

bool CFng::CanSelfkill(int ClientID) const
{
	CCharacter *p = GameServer()->GetPlayerChar(ClientID);
	bool Frozen = p && (p->m_FreezeTime > 0 || p->Core()->m_DeepFrozen);
	return !Frozen;
}

int CFng::GetLeaderPoints(int R) const
{
	int Best=-1, BestID=-1;
	for(int i=0;i<MAX_CLIENTS;i++)
	{
		if(m_aClientRoom[i]!=R) continue;
		int pts = m_aRooms[R].m_aPoints[i];
		if(pts > Best) { Best=pts; BestID=i; }
	}
	return BestID;
}

void CFng::SendScoreToAll(int R)
{
	// Keep m_Score as race/time score for the global board. FNG points are Snap-only (same room).
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClientRoom[i] != R)
			continue;
		CPlayer *p = GameServer()->m_apPlayers[i];
		if(!p)
			continue;
		Server()->SetClientScore(i, std::nullopt); /* TODO: restore race score */
		(void)p;
	}
}

void CFng::BroadcastRoom(int R, const char *pMsg, bool Imp)
{
	for(int i=0;i<MAX_CLIENTS;i++)
		if(m_aClientRoom[i]==R && GameServer()->m_apPlayers[i])
			GameServer()->SendBroadcast(pMsg, i, Imp);
}

void CFng::BroadcastRoomFormat(int R, bool Imp, const char *pFmt, ...)
{
	va_list Args;
	for(int i=0;i<MAX_CLIENTS;i++)
	{
		if(m_aClientRoom[i]!=R || !GameServer()->m_apPlayers[i]) continue;
		const char *pTranslated = GameServer()->m_apPlayers[i]->Localize(pFmt);
		char aBuf[256];
		va_start(Args, pFmt);
		vsnprintf(aBuf, sizeof(aBuf), pTranslated, Args);
		va_end(Args);
		GameServer()->SendBroadcast(aBuf, i, Imp);
	}
}

void CFng::ResetSessionStats(int R)
{
	CFngRoom &r = m_aRooms[R];
	r.m_aTeamPoints[0] = r.m_aTeamPoints[1] = 0;
	for(int i=0;i<MAX_CLIENTS;i++)
	{
		if(m_aClientRoom[i]!=R) continue;
		r.m_aPoints[i] = 0;
		r.m_aKills[i] = 0;
		r.m_aDeaths[i] = 0;
		r.m_aSide[i] = FNG_SIDE_NONE;
	}
	SendScoreToAll(R);
}

void CFng::GiveWeapons(int ClientID)
{
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if(!pChr) return;
	int R = m_aClientRoom[ClientID];
	if(R < 0) return;

	for(int w = 0; w < NUM_WEAPONS; w++)
		pChr->GiveWeapon(w, true);

	pChr->GiveWeapon(WEAPON_HAMMER, false);

	if(m_aRooms[R].m_State == FNG_STATE_WAITING)
	{
		// Lobby loadout like 1vs1: hammer + gun only
		pChr->GiveWeapon(WEAPON_GUN, false);
		pChr->SetActiveWeapon(WEAPON_GUN);
		return;
	}

	if(m_aRooms[R].m_MinigameType == MINIGAME_INSTAGIB_FNG)
	{
		pChr->GiveWeapon(WEAPON_LASER, false);
		pChr->SetActiveWeapon(WEAPON_LASER);
	}
	else
	{
		pChr->GiveWeapon(WEAPON_GRENADE, false);
		pChr->SetActiveWeapon(WEAPON_GRENADE);
	}
}

void CFng::ApplyFngTunings(int ClientID)
{
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if(!pChr) return;
	int R = m_aClientRoom[ClientID];
	if(R < 0) return;

	pChr->m_LockedTunings.clear();

	if(m_aRooms[R].m_TuneReset)
	{
		CTuningParams Def;
		for(int p=0; p<Def.Num(); p++)
		{
			float d, s;
			Def.Get(p, &d);
			GameServer()->GlobalTuning()->Get(p, &s);
			if(d != s)
				pChr->m_LockedTunings.push_back(CLockedTune(CTuningParams::Name(p), d));
		}
	}

	pChr->m_LockedTunings.push_back(CLockedTune("gun_curvature", 1.25f));
	pChr->m_LockedTunings.push_back(CLockedTune("gun_speed", 2200.0f));
	pChr->m_LockedTunings.push_back(CLockedTune("shotgun_curvature", 1.25f));
	pChr->m_LockedTunings.push_back(CLockedTune("shotgun_speed", 2750.0f));
	pChr->m_LockedTunings.push_back(CLockedTune("shotgun_speeddiff", 0.8f));

	pChr->ApplyLockedTunings();
}

void CFng::RemoveFngTunings(int ClientID)
{
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if(!pChr) return;
	pChr->m_LockedTunings.clear();
	pChr->ApplyLockedTunings(false);
}

void CFng::KillAllInRoom(int R)
{
	for(int i=0;i<MAX_CLIENTS;i++)
	{
		if(m_aClientRoom[i]!=R) continue;
		CPlayer *p = GameServer()->m_apPlayers[i];
		if(p) p->KillCharacter(WEAPON_GAME);
	}
}

void CFng::AssignSides(int R)
{
	CFngRoom &r = m_aRooms[R];
	for(int i=0;i<MAX_CLIENTS;i++)
	{
		if(m_aClientRoom[i]!=R) continue;
		if(r.m_aSide[i] != FNG_SIDE_NONE) continue;

		int Red = CountSide(R, FNG_SIDE_RED);
		int Blue = CountSide(R, FNG_SIDE_BLUE);
		r.m_aSide[i] = (Red <= Blue) ? FNG_SIDE_RED : FNG_SIDE_BLUE;

		const char *pSide = r.m_aSide[i] == FNG_SIDE_RED ? "Red" : "Blue";
		GameServer()->SendChatTargetFormat(i, Localizable("You are on team %s"), pSide);
	}
}

void CFng::SyncGameInfoRoom(int R)
{
	for(int i=0;i<MAX_CLIENTS;i++)
		if(m_aClientRoom[i]==R && GameServer()->m_apPlayers[i])
			GameServer()->m_pController->UpdateGameInfo(i);
}

bool CFng::ApplySettingDirect(int R, CPendingSetting::EType Type, int IntValue, bool BoolValue)
{
	CFngRoom &r = m_aRooms[R];
	switch(Type)
	{
	case CPendingSetting::TUNE_RESET:
		r.m_TuneReset = BoolValue;
		if(r.m_State == FNG_STATE_ROUND || r.m_State == FNG_STATE_WARMUP)
		{
			for(int i=0;i<MAX_CLIENTS;i++)
			{
				if(m_aClientRoom[i]!=R) continue;
				if(r.m_TuneReset) ApplyFngTunings(i);
				else RemoveFngTunings(i);
			}
		}
		BroadcastRoomFormat(R, true, r.m_TuneReset
			? Localizable("Physics: original (tune reset)")
			: Localizable("Physics: server default"));
		break;
	case CPendingSetting::TEAM_MODE:
		r.m_TeamMode = BoolValue;
		if(r.m_TeamMode)
			AssignSides(R);
		else
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
				r.m_aSide[i] = FNG_SIDE_NONE;
		}
		BroadcastRoomFormat(R, true, r.m_TeamMode
			? Localizable("Mode: Team FNG")
			: Localizable("Mode: Solo FNG"));
		SyncGameInfoRoom(R);
		SendScoreToAll(R); // repair any packed solo scores after mode switch
		break;
	case CPendingSetting::SCORE_LIMIT:
		r.m_ScoreLimit = std::clamp(IntValue, FNG_SCORELIMIT_MIN, FNG_SCORELIMIT_MAX);
		BroadcastRoomFormat(R, true, Localizable("Score limit: %d"), r.m_ScoreLimit);
		SyncGameInfoRoom(R);
		break;
	default:
		return false;
	}
	return true;
}

void CFng::StartSettingsConfirmVote(int R, CPendingSetting::EType Type, int IntValue, bool BoolValue)
{
	CPendingSetting &P = m_aPending[R];
	P.Reset();
	P.m_Type = Type;
	P.m_IntValue = IntValue;
	P.m_BoolValue = BoolValue;
	P.m_EndTick = Server()->Tick() + Server()->TickSpeed() * 20;

	const char *pDesc = Localizable("Confirm FNG settings change?");
	for(int i=0;i<MAX_CLIENTS;i++)
	{
		if(m_aClientRoom[i]!=R) continue;
		CPlayer *p = GameServer()->m_apPlayers[i];
		if(p) p->StartVoteQuestion(CPlayer::VOTE_QUESTION_FNG_SETTINGS, 20);
	}
	BroadcastRoom(R, pDesc, true);
}

void CFng::ApplyPendingSetting(int R)
{
	CPendingSetting &P = m_aPending[R];
	if(P.m_Type == CPendingSetting::NONE) return;
	CPendingSetting Copy = P;
	P.Reset(); // clear first so vote-end callbacks ignore
	if(Copy.m_VoteYes > Copy.m_VoteNo)
		ApplySettingDirect(R, Copy.m_Type, Copy.m_IntValue, Copy.m_BoolValue);
	else
		BroadcastRoom(R, Localizable("Settings change rejected."), true);
	for(int i=0;i<MAX_CLIENTS;i++)
	{
		if(m_aClientRoom[i]!=R) continue;
		CPlayer *p = GameServer()->m_apPlayers[i];
		if(p && p->m_VoteQuestionRunning && p->m_VoteQuestionType == CPlayer::VOTE_QUESTION_FNG_SETTINGS)
			p->OnEndVoteQuestion(0);
	}
}

void CFng::TickSettingsVote(int R)
{
	CPendingSetting &P = m_aPending[R];
	if(P.m_Type == CPendingSetting::NONE) return;
	int N = CountActivePlayers(R);
	if(N > 0 && (P.m_VoteYes + P.m_VoteNo) >= N)
	{
		ApplyPendingSetting(R);
		return;
	}
	if(Server()->Tick() >= P.m_EndTick)
		ApplyPendingSetting(R);
}

bool CFng::TrySetTuneReset(int ClientID, bool Value)
{
	int R = GetClientRoom(ClientID);
	if(R < 0) return false;
	if(m_aRooms[R].m_TuneReset == Value) return true;
	if(m_aRooms[R].m_State == FNG_STATE_ROUND)
	{
		StartSettingsConfirmVote(R, CPendingSetting::TUNE_RESET, 0, Value);
		return true;
	}
	return ApplySettingDirect(R, CPendingSetting::TUNE_RESET, 0, Value);
}

bool CFng::TrySetTeamMode(int ClientID, bool Value)
{
	int R = GetClientRoom(ClientID);
	if(R < 0) return false;
	if(m_aRooms[R].m_TeamMode == Value) return true;
	if(m_aRooms[R].m_State == FNG_STATE_ROUND)
	{
		StartSettingsConfirmVote(R, CPendingSetting::TEAM_MODE, 0, Value);
		return true;
	}
	return ApplySettingDirect(R, CPendingSetting::TEAM_MODE, 0, Value);
}

bool CFng::TrySetScoreLimit(int ClientID, int Value)
{
	int R = GetClientRoom(ClientID);
	if(R < 0) return false;
	Value = std::clamp(Value, FNG_SCORELIMIT_MIN, FNG_SCORELIMIT_MAX);
	// snap to step (50, 100, 150, ...)
	Value = FNG_SCORELIMIT_MIN + ((Value - FNG_SCORELIMIT_MIN + FNG_SCORELIMIT_STEP / 2) / FNG_SCORELIMIT_STEP) * FNG_SCORELIMIT_STEP;
	Value = std::clamp(Value, FNG_SCORELIMIT_MIN, FNG_SCORELIMIT_MAX);
	if(m_aRooms[R].m_ScoreLimit == Value) return true;
	if(m_aRooms[R].m_State == FNG_STATE_ROUND)
	{
		StartSettingsConfirmVote(R, CPendingSetting::SCORE_LIMIT, Value, false);
		return true;
	}
	return ApplySettingDirect(R, CPendingSetting::SCORE_LIMIT, Value, false);
}

void CFng::StartWarmup(int R)
{
	CFngRoom &r = m_aRooms[R];
	r.m_State = FNG_STATE_WARMUP;
	r.m_WarmupPhase = FNG_WARMUP_PHYSICS;
	r.m_TimerTick = Server()->Tick() + Server()->TickSpeed() * FNG_WARMUP_DURATION;
	r.m_RoundStartTick = 0;
	ResetSessionStats(R);
	if(r.m_TeamMode)
		AssignSides(R);
	KillAllInRoom(R);
	SyncGameInfoRoom(R);
	BroadcastRoomFormat(R, true, Localizable("Warmup! Round starts in %ds"), FNG_WARMUP_DURATION);
}

void CFng::StartRound(int R)
{
	CFngRoom &r = m_aRooms[R];
	r.m_State = FNG_STATE_ROUND;
	r.m_RoundStartTick = Server()->Tick();
	r.m_TimerTick = Server()->Tick() + Server()->TickSpeed() * FNG_START_FREEZE;

	if(r.m_TeamMode)
		AssignSides(R);

	KillAllInRoom(R);
	SyncGameInfoRoom(R);

	// Fresh round = fresh First Blood, and Prepare belongs here (not warmup).
	GameServer()->ResetKillAnnouncer();
	{
		int aAudience[MAX_CLIENTS];
		int NumAudience = 0;
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			if(m_aClientRoom[ClientID] == R)
				aAudience[NumAudience++] = ClientID;
		GameServer()->PlayMapSoundClients(15 + rand() % 3, aAudience, NumAudience, SOUND_CTF_RETURN);
	}

	if(r.m_TeamMode)
		BroadcastRoomFormat(R, true, Localizable("Team FNG — Red vs Blue! Score limit: %d"), r.m_ScoreLimit);
	else
		BroadcastRoomFormat(R, true, Localizable("Solo FNG — free for all! Score limit: %d"), r.m_ScoreLimit);
}

void CFng::EndRound(int R)
{
	CFngRoom &r = m_aRooms[R];
	const char *pType = (r.m_MinigameType == MINIGAME_INSTAGIB_BOOMFNG)
		? (r.m_TeamMode ? "Team BoomFNG" : "BoomFNG")
		: (r.m_TeamMode ? "Team FNG" : "FNG");

	if(r.m_TeamMode)
	{
		int Red = r.m_aTeamPoints[FNG_SIDE_RED];
		int Blue = r.m_aTeamPoints[FNG_SIDE_BLUE];
		if(Red == Blue)
		{
			BroadcastRoomFormat(R, true, Localizable("Draw! Red %d — Blue %d"), Red, Blue);
			char aChat[256];
			str_format(aChat, sizeof(aChat), Localizable("%s round ended in a draw! Red %d — Blue %d"), pType, Red, Blue);
			GameServer()->SendChat(-1, TEAM_ALL, aChat);
		}
		else
		{
			const char *pWin = Red > Blue ? "Red" : "Blue";
			int WinPts = std::max(Red, Blue);
			int LosePts = std::min(Red, Blue);
			BroadcastRoomFormat(R, true, Localizable("Team %s wins! %d — %d"), pWin, WinPts, LosePts);
			char aChat[256];
			str_format(aChat, sizeof(aChat), Localizable("Team %s won a %s round! %d — %d"), pWin, pType, WinPts, LosePts);
			GameServer()->SendChat(-1, TEAM_ALL, aChat);

			if(GameServer()->Config()->m_SvWebhookMinigamesURL[0] && CountPlayers(R) >= FNG_MIN_PLAYERS)
			{
				char aWebhook[512];
				str_format(aWebhook, sizeof(aWebhook),
					"🏆 Team **%s** won **%s**!\n🔴 Red %d — 🔵 Blue %d",
					pWin, pType, Red, Blue);
				Server()->SendWebhookMessage(GameServer()->Config()->m_SvWebhookMinigamesURL, aWebhook,
					GameServer()->Config()->m_SvWebhookMinigamesName,
					GameServer()->Config()->m_SvWebhookMinigamesAvatarURL);
			}
		}

		// Top scorers
		int WinnerID = GetLeaderPoints(R);
		if(WinnerID >= 0)
		{
			char aLine[128];
			str_format(aLine, sizeof(aLine), Localizable("Top scorer: %s — %d pts"),
				Server()->ClientName(WinnerID), r.m_aPoints[WinnerID]);
			for(int j=0;j<MAX_CLIENTS;j++)
				if(m_aClientRoom[j]==R && GameServer()->m_apPlayers[j])
					GameServer()->SendChatTarget(j, aLine);
		}
	}
	else
	{
		int WinnerID = GetLeaderPoints(R);
		if(WinnerID >= 0 && GameServer()->m_apPlayers[WinnerID])
		{
			int Pts    = r.m_aPoints[WinnerID];
			int Kills  = r.m_aKills[WinnerID];
			int Deaths = r.m_aDeaths[WinnerID];

			BroadcastRoomFormat(R, true, Localizable("%s wins! %d pts | %d spikes | %d deaths"),
				Server()->ClientName(WinnerID), Pts, Kills, Deaths);

			char aChat[256];
			str_format(aChat, sizeof(aChat),
				Localizable("'%s' won a %s round! %d pts, %d spikes, %d deaths"),
				Server()->ClientName(WinnerID), pType, Pts, Kills, Deaths);
			GameServer()->SendChat(-1, TEAM_ALL, aChat);

			if(GameServer()->Config()->m_SvWebhookMinigamesURL[0] && CountPlayers(R) >= FNG_MIN_PLAYERS)
			{
				char aWebhook[512];
				str_format(aWebhook, sizeof(aWebhook),
					"🏆 **%s** won a **%s** round!\n📊 %d pts | ⚡ %d spikes | 💀 %d deaths",
					Server()->ClientName(WinnerID), pType, Pts, Kills, Deaths);

				int aTop[3] = {-1, -1, -1};
				int aTopPts[3] = {-1, -1, -1};
				for(int i=0;i<MAX_CLIENTS;i++)
				{
					if(m_aClientRoom[i]!=R) continue;
					int p = r.m_aPoints[i];
					if(p > aTopPts[0]) { aTop[2]=aTop[1]; aTopPts[2]=aTopPts[1]; aTop[1]=aTop[0]; aTopPts[1]=aTopPts[0]; aTop[0]=i; aTopPts[0]=p; }
					else if(p > aTopPts[1]) { aTop[2]=aTop[1]; aTopPts[2]=aTopPts[1]; aTop[1]=i; aTopPts[1]=p; }
					else if(p > aTopPts[2]) { aTop[2]=i; aTopPts[2]=p; }
				}
				const char *aMedals[] = {"🥇", "🥈", "🥉"};
				for(int i=0;i<3;i++)
				{
					if(aTop[i] < 0 || aTopPts[i] <= 0) break;
					char tmp[128];
					str_format(tmp, sizeof(tmp), "\n%s #%d **%s** — %d pts, %d spikes, %d deaths",
						aMedals[i], i+1, Server()->ClientName(aTop[i]),
						aTopPts[i], r.m_aKills[aTop[i]], r.m_aDeaths[aTop[i]]);
					str_append(aWebhook, tmp, sizeof(aWebhook));
				}

				Server()->SendWebhookMessage(GameServer()->Config()->m_SvWebhookMinigamesURL, aWebhook,
					GameServer()->Config()->m_SvWebhookMinigamesName,
					GameServer()->Config()->m_SvWebhookMinigamesAvatarURL);
			}

			int aTop[3] = {-1, -1, -1};
			int aTopPts[3] = {-1, -1, -1};
			for(int i=0;i<MAX_CLIENTS;i++)
			{
				if(m_aClientRoom[i]!=R) continue;
				int p = r.m_aPoints[i];
				if(p > aTopPts[0]) { aTop[2]=aTop[1]; aTopPts[2]=aTopPts[1]; aTop[1]=aTop[0]; aTopPts[1]=aTopPts[0]; aTop[0]=i; aTopPts[0]=p; }
				else if(p > aTopPts[1]) { aTop[2]=aTop[1]; aTopPts[2]=aTopPts[1]; aTop[1]=i; aTopPts[1]=p; }
				else if(p > aTopPts[2]) { aTop[2]=i; aTopPts[2]=p; }
			}
			for(int i=0;i<3;i++)
			{
				if(aTop[i] < 0) break;
				char aLine[128];
				const char *pMedal = i==0?"#1":i==1?"#2":"#3";
				str_format(aLine, sizeof(aLine), "%s %s — %d pts, %d spikes, %d deaths",
					pMedal, Server()->ClientName(aTop[i]), aTopPts[i],
					r.m_aKills[aTop[i]], r.m_aDeaths[aTop[i]]);
				for(int j=0;j<MAX_CLIENTS;j++)
					if(m_aClientRoom[j]==R && GameServer()->m_apPlayers[j])
						GameServer()->SendChatTarget(j, aLine);
			}
		}
		else
		{
			BroadcastRoom(R, Localizable("Draw! No winner this round."), true);
			GameServer()->SendChat(-1, TEAM_ALL, Localizable("FNG round ended in a draw!"));
		}
	}

	for(int i=0;i<MAX_CLIENTS;i++)
	{
		if(m_aClientRoom[i]!=R) continue;
		RemoveFngTunings(i);
	}

	{
		int aAudience[MAX_CLIENTS];
		int NumAudience = 0;
		for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
			if(m_aClientRoom[ClientID] == R)
				aAudience[NumAudience++] = ClientID;
		// Win feel for the room only — never the whole server.
		GameServer()->PlayMapSoundClients(14, aAudience, NumAudience, SOUND_CTF_CAPTURE);
	}
	r.m_State = FNG_STATE_POSTROUND;
	r.m_RoundStartTick = 0;
	r.m_TimerTick = Server()->Tick() + Server()->TickSpeed() * FNG_POSTROUND_DURATION;
}

void CFng::TickWaiting(int R)
{
	if(Server()->Tick() % Server()->TickSpeed() != 0) return;
	int n = CountActivePlayers(R);
	BroadcastRoomFormat(R, false, Localizable("Waiting for players... (%d/2)"), n);
	if(n >= FNG_MIN_PLAYERS) StartWarmup(R);
}

void CFng::TickWarmup(int R)
{
	CFngRoom &r = m_aRooms[R];
	TickSettingsVote(R);
	if(CountActivePlayers(R) < FNG_MIN_PLAYERS)
	{
		AbortToWaiting(R);
		return;
	}

	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		int s = (int)((r.m_TimerTick - Server()->Tick()) / Server()->TickSpeed()) + 1;
		if(s > 0)
		{
			BroadcastRoomFormat(R, false, Localizable("Warmup: %ds"), s);
			if(s <= 3) GameServer()->CreateSound(vec2(0,0), SOUND_HOOK_NOATTACH, CGameContext::CmaskNone());
		}
	}

	if(Server()->Tick() >= r.m_TimerTick)
		StartRound(R);
}

bool CFng::CheckWin(int R)
{
	CFngRoom &r = m_aRooms[R];
	if(r.m_ScoreLimit <= 0) return false;

	if(r.m_TeamMode)
	{
		int Red = r.m_aTeamPoints[FNG_SIDE_RED];
		int Blue = r.m_aTeamPoints[FNG_SIDE_BLUE];
		if(Red >= r.m_ScoreLimit || Blue >= r.m_ScoreLimit)
		{
			if(Red != Blue)
				return true;
			// tied at limit — keep playing (sudden death style)
		}
	}
	else
	{
		int Best = -1, Second = -1;
		for(int i=0;i<MAX_CLIENTS;i++)
		{
			if(m_aClientRoom[i]!=R) continue;
			int p = r.m_aPoints[i];
			if(p > Best) { Second = Best; Best = p; }
			else if(p > Second) Second = p;
		}
		if(Best >= r.m_ScoreLimit && Best > Second)
			return true;
	}
	return false;
}

void CFng::TickRound(int R)
{
	CFngRoom &r = m_aRooms[R];
	TickSettingsVote(R);
	bool FreezePhase = Server()->Tick() < r.m_TimerTick;

	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		if(FreezePhase)
		{
			int fl = (int)((r.m_TimerTick - Server()->Tick()) / Server()->TickSpeed()) + 1;
			BroadcastRoomFormat(R, false, Localizable("Starting in %d..."), fl);
		}
	}

	if(CountActivePlayers(R) < FNG_MIN_PLAYERS)
	{
		AbortToWaiting(R);
		return;
	}

	if(!FreezePhase && CheckWin(R))
		EndRound(R);
}

void CFng::TickPostRound(int R)
{
	CFngRoom &r = m_aRooms[R];
	if(Server()->Tick() % Server()->TickSpeed() == 0)
	{
		int s = (int)((r.m_TimerTick - Server()->Tick()) / Server()->TickSpeed()) + 1;
		BroadcastRoomFormat(R, true, Localizable("Next round in %d..."), s);
	}
	if(Server()->Tick() >= r.m_TimerTick)
		CountActivePlayers(R) >= FNG_MIN_PLAYERS ? StartWarmup(R) : AbortToWaiting(R);
}

void CFng::TickRoom(int R)
{
	switch(m_aRooms[R].m_State)
	{
	case FNG_STATE_WAITING:   TickWaiting(R);   break;
	case FNG_STATE_WARMUP:    TickWarmup(R);    break;
	case FNG_STATE_ROUND:     TickRound(R);     break;
	case FNG_STATE_POSTROUND: TickPostRound(R); break;
	}
}

void CFng::Tick()
{
	for(int i=0;i<MAX_FNG_ROOMS;i++)
		if(m_aRooms[i].m_Active) TickRoom(i);

	static const int aSpikeTiles[] = {
		TILE_FNG_SPIKE_RED, TILE_FNG_SPIKE_BLUE, TILE_FNG_SPIKE_NORMAL,
		TILE_FNG_SPIKE_GOLD, TILE_FNG_SPIKE_GREEN, TILE_FNG_SPIKE_PURPLE
	};

	for(int i=0;i<MAX_CLIENTS;i++)
	{
		int R = m_aClientRoom[i];
		if(R < 0 || m_aRooms[R].m_State != FNG_STATE_ROUND) continue;

		CCharacter *pChr = GameServer()->GetPlayerChar(i);
		if(!pChr) continue;

		vec2 Pos = pChr->GetPos();
		float Rad = pChr->GetProximityRadius() / 3.f;

		int aIndices[5];
		aIndices[0] = GameServer()->Collision()->GetMapIndex(Pos);
		aIndices[1] = GameServer()->Collision()->GetPureMapIndex(vec2(Pos.x + Rad, Pos.y - Rad));
		aIndices[2] = GameServer()->Collision()->GetPureMapIndex(vec2(Pos.x + Rad, Pos.y + Rad));
		aIndices[3] = GameServer()->Collision()->GetPureMapIndex(vec2(Pos.x - Rad, Pos.y - Rad));
		aIndices[4] = GameServer()->Collision()->GetPureMapIndex(vec2(Pos.x - Rad, Pos.y + Rad));

		int HitSpike = 0;
		for(int Idx : aIndices)
		{
			if(Idx < 0) continue;
			int T  = GameServer()->Collision()->GetTileIndex(Idx);
			int FT = GameServer()->Collision()->GetFrontTileIndex(Idx);
			for(int Spike : aSpikeTiles)
			{
				if(T == Spike || FT == Spike)
				{
					HitSpike = Spike;
					break;
				}
			}
			if(HitSpike) break;
		}

		if(HitSpike)
			OnSpikeTile(i, HitSpike);
	}
}

int CFng::OnPlayerJoin(int ClientID, int MinigameType)
{
	char aBuf[256];
	bool HasAny = false;
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	const char *pHeader = pPlayer ? pPlayer->Localize(Localizable("Active rooms (/fng <N> to join, /fng to create new): ")) : Localizable("Active rooms (/fng <N> to join, /fng to create new): ");
	str_copy(aBuf, pHeader, sizeof(aBuf));
	for(int i=0;i<MAX_FNG_ROOMS;i++)
	{
		if(!m_aRooms[i].m_Active || m_aRooms[i].m_MinigameType != MinigameType) continue;
		const char *pS = m_aRooms[i].m_State==FNG_STATE_ROUND?"round":
			m_aRooms[i].m_State==FNG_STATE_WARMUP?"warmup":
			m_aRooms[i].m_State==FNG_STATE_WAITING?"waiting":"post";
		const char *pMode = m_aRooms[i].m_TeamMode ? "team" : "solo";
		char tmp[32]; str_format(tmp, sizeof(tmp), "team%d(%s/%s) ", m_aRooms[i].m_Team, pS, pMode);
		str_append(aBuf, tmp, sizeof(aBuf));
		HasAny = true;
	}
	GameServer()->SendChatTarget(ClientID, HasAny ? aBuf : Localizable("No active rooms. Use '/fng' again to create one."));
	return -1;
}

void CFng::MoveClientToRoom(int ClientID, int RoomIdx)
{
	if(m_aClientRoom[ClientID] >= 0) OnPlayerLeave(ClientID);
	CFngRoom &r = m_aRooms[RoomIdx];
	m_aClientRoom[ClientID] = RoomIdx;
	r.m_aPoints[ClientID] = 0;
	r.m_aKills[ClientID] = 0;
	r.m_aDeaths[ClientID] = 0;
	r.m_aSide[ClientID] = FNG_SIDE_NONE;
	TEAMS.SetForceCharacterTeam(ClientID, r.m_Team);
	CPlayer *p = GameServer()->m_apPlayers[ClientID];
	if(p)
	{
		// Keep race/time score intact; FNG display is Snap-only.
		p->KillCharacter(WEAPON_GAME);
		p->SetPlaying();
		p->Respawn();
		p->TryRespawn();
	}
	for(int i=0;i<MAX_CLIENTS;i++)
		if(GameServer()->m_apPlayers[i]) TEAMS.SendTeamsState(i);
	GameServer()->m_pController->UpdateGameInfo(ClientID);
	if(r.m_State == FNG_STATE_WARMUP)
	{
		OnClientJoinedRoom(RoomIdx);
		if(r.m_TeamMode)
			AssignSides(RoomIdx);
	}
	else if(r.m_State == FNG_STATE_WAITING && CountActivePlayers(RoomIdx) >= FNG_MIN_PLAYERS)
		StartWarmup(RoomIdx);
	else if(r.m_TeamMode && (r.m_State == FNG_STATE_WAITING || r.m_State == FNG_STATE_ROUND))
		AssignSides(RoomIdx);
}

void CFng::CreateNewRoom(int ClientID)
{
	CGameTeams *pTeams = &TEAMS;
	int OldR = m_aClientRoom[ClientID];
	int MinigameType = OldR >= 0 ? m_aRooms[OldR].m_MinigameType : m_Type;

	// Leave current room first so Count/room occupancy is accurate before picking a team.
	if(OldR >= 0)
		OnPlayerLeave(ClientID);

	auto TeamTakenByFng = [&](int Team) -> bool {
		for(int Type : {MINIGAME_INSTAGIB_FNG, MINIGAME_INSTAGIB_BOOMFNG})
		{
			CFng *p = (CFng *)GameServer()->m_pMinigames[Type];
			if(!p) continue;
			for(int i = 0; i < MAX_FNG_ROOMS; i++)
				if(p->m_aRooms[i].m_Active && p->m_aRooms[i].m_Team == Team)
					return true;
		}
		return false;
	};

	int FreeTeam = -1;
	for(int t = 1; t < MAX_CLIENTS; t++)
	{
		// Prefer empty DDRace teams, but never reuse a team number of an active FNG room
		// (MembersCount can be 0 if players were reset while the room stayed active).
		if(TeamTakenByFng(t))
			continue;
		if(pTeams->TeamSize(t) == 0)
		{
			FreeTeam = t;
			break;
		}
	}
	int RoomIdx = FindFreeRoom();
	if(FreeTeam == -1 || RoomIdx == -1) { GameServer()->SendChatTarget(ClientID, Localizable("No free FNG rooms")); return; }
	m_aRooms[RoomIdx].Reset();
	m_aRooms[RoomIdx].m_Active = true;
	m_aRooms[RoomIdx].m_MinigameType = MinigameType;
	m_aRooms[RoomIdx].m_Team = FreeTeam;
	m_aRooms[RoomIdx].m_State = FNG_STATE_WAITING;
	MoveClientToRoom(ClientID, RoomIdx);
	GameServer()->SendChatTargetFormat(ClientID, Localizable("Created new room (team %d)"), FreeTeam);
}

void CFng::LeaveRoomToLobby(int ClientID)
{
	if(GetClientRoom(ClientID) < 0)
		return;

	OnPlayerLeave(ClientID);
	CPlayer *p = GameServer()->m_apPlayers[ClientID];
	if(p)
	{
		p->KillCharacter(WEAPON_GAME);
		p->SetPlaying();
		p->Respawn();
		p->TryRespawn();
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(GameServer()->m_apPlayers[i])
			TEAMS.SendTeamsState(i);
	GameServer()->m_pController->UpdateGameInfo(ClientID);
	GameServer()->UpdateHidePlayers(ClientID);
	GameServer()->SendChatTarget(ClientID, Localizable("Left the FNG room. Use the vote menu or /fng to join/create one."));
}

void CFng::OnClientJoinedRoom(int R)
{
	CFngRoom &r = m_aRooms[R];
	if(r.m_State != FNG_STATE_WARMUP) return;

	r.m_TimerTick = std::min(r.m_TimerTick + (int64_t)Server()->TickSpeed()*FNG_WARMUP_EXTEND,
		Server()->Tick() + (int64_t)Server()->TickSpeed()*FNG_WARMUP_DURATION);
}

void CFng::OnPlayerLeave(int ClientID, bool Disconnect)
{
	int R = m_aClientRoom[ClientID];
	if(R < 0) return;
	CFngRoom &r = m_aRooms[R];
	r.m_aPoints[ClientID] = 0;
	r.m_aKills[ClientID] = 0;
	r.m_aDeaths[ClientID] = 0;
	r.m_aSide[ClientID] = FNG_SIDE_NONE;
	m_aClientRoom[ClientID] = -1;
	RemoveFngTunings(ClientID);
	if(GameServer()->m_apPlayers[ClientID])
	{
		TEAMS.SetForceCharacterTeam(ClientID, 0);
		// Repair scores corrupted by older solo FNG packing (points*3600).
		CPlayer *p = GameServer()->m_apPlayers[ClientID];
		Server()->SetClientScore(ClientID, std::nullopt); /* TODO: restore race score */
		(void)p;
	}
	if(CountPlayers(R) == 0)
	{
		r.Reset();
		m_aPending[R].Reset();
		return;
	}
	OnActivePlayersChanged(R);
}

void CFng::OnCharacterSpawn(int ClientID)
{
	int R = m_aClientRoom[ClientID];
	if(R < 0) return;
	RestoreIsolationTeam(ClientID, R);
	CFngRoom &r = m_aRooms[R];
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if(!pChr) return;

	// Waiting lobby: hammer+gun only. Round/warmup: FNG weapons.
	GiveWeapons(ClientID);

	if(r.m_State == FNG_STATE_WAITING)
		return;

	if(r.m_State == FNG_STATE_ROUND || r.m_State == FNG_STATE_WARMUP)
		ApplyFngTunings(ClientID);

	if(r.m_State == FNG_STATE_ROUND)
	{
		if(Server()->Tick() < r.m_TimerTick)
			pChr->Freeze(3.0f);
	}
}

void CFng::OnPlayerDie(int ClientID, int KillerID, int Weapon)
{
	int R = m_aClientRoom[ClientID];
	if(R < 0) return;
	CFngRoom &r = m_aRooms[R];

	if(r.m_State == FNG_STATE_ROUND && Weapon != WEAPON_GAME)
	{
		r.m_aDeaths[ClientID]++;
		SendScoreToAll(R);
	}
}

bool CFng::OnWeaponHit(int VictimID, int AttackerID, int Weapon)
{
	if (AttackerID < 0 || AttackerID >= MAX_CLIENTS || m_aClientRoom[AttackerID] != m_aClientRoom[VictimID])
		return false;
	int R = m_aClientRoom[VictimID];
	if(R < 0) return false;
	CFngRoom &r = m_aRooms[R];
	if(r.m_State != FNG_STATE_ROUND) return false;

	bool IsFng  = (r.m_MinigameType == MINIGAME_INSTAGIB_FNG  && Weapon == WEAPON_LASER);
	bool IsBoom = (r.m_MinigameType == MINIGAME_INSTAGIB_BOOMFNG && Weapon == WEAPON_GRENADE);
	if(!IsFng && !IsBoom) return false;
	if(Weapon == WEAPON_HAMMER) return false;

	CCharacter *pVictim = GameServer()->GetPlayerChar(VictimID);
	if(!pVictim) return false;

	if(pVictim->m_FreezeTime > 0) return false;
	if(AttackerID == VictimID) return false;
	if(SameSide(AttackerID, VictimID, R)) return false;

	pVictim->Core()->m_Killer.m_ClientID = AttackerID;
	pVictim->Core()->m_Killer.m_Weapon = Weapon;
	pVictim->Freeze((float)FNG_FREEZE_SECS);

	CPlayer *pAttacker = GameServer()->m_apPlayers[AttackerID];
	if(pAttacker)
	{
		r.m_aPoints[AttackerID]++;
		if(r.m_TeamMode)
		{
			int Side = r.m_aSide[AttackerID];
			if(Side == FNG_SIDE_RED || Side == FNG_SIDE_BLUE)
				r.m_aTeamPoints[Side]++;
		}
		SendScoreToAll(R);
	}

	CNetMsg_Sv_KillMsg Msg;
	Msg.m_Killer = AttackerID;
	Msg.m_Victim = VictimID;
	Msg.m_Weapon = Weapon;
	Msg.m_ModeSpecial = 0;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);

	GameServer()->CreateDeath(pVictim->GetPos(), VictimID, pVictim->TeamMask());
	return true;
}

bool CFng::OnHammerHit(int AttackerID, int VictimID, vec2 &Force)
{
	if (AttackerID < 0 || AttackerID >= MAX_CLIENTS || m_aClientRoom[AttackerID] != m_aClientRoom[VictimID])
		return false;
	int R = m_aClientRoom[VictimID];
	if(R < 0) return false;

	CCharacter *pVictim = GameServer()->GetPlayerChar(VictimID);
	CCharacter *pAttacker = GameServer()->GetPlayerChar(AttackerID);
	if(!pVictim || !pAttacker) return false;

	vec2 Dir;
	if(length(pVictim->GetPos() - pAttacker->GetPos()) > 0.0f)
		Dir = normalize(pVictim->GetPos() - pAttacker->GetPos());
	else
		Dir = vec2(0.f, -1.f);

	vec2 Push = vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f;

	// Melt hammer: frozen teammate (team mode only)
	if(m_aRooms[R].m_TeamMode && SameSide(AttackerID, VictimID, R) && pVictim->m_FreezeTime > 0)
	{
		Push.x *= FNG_MELT_HAMMER_SCALE_X * 0.01f;
		Push.y *= FNG_MELT_HAMMER_SCALE_Y * 0.01f;
		Force = Push;
		pVictim->m_FreezeTime -= Server()->TickSpeed() * 3;
		if(pVictim->m_FreezeTime < 0)
			pVictim->m_FreezeTime = 0;
		return true;
	}

	Push.x *= FNG_HAMMER_SCALE_X * 0.01f;
	Push.y *= FNG_HAMMER_SCALE_Y * 0.01f;

	Force = Push;
	return true;
}

void CFng::OnSpikeTile(int ClientID, int SpikeTile)
{
	int R = m_aClientRoom[ClientID];
	if(R < 0) return;
	CFngRoom &r = m_aRooms[R];
	if(r.m_State != FNG_STATE_ROUND && r.m_State != FNG_STATE_WARMUP) return;

	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if(!pChr) return;

	if(pChr->m_FreezeTime <= 0)
	{
		// Unfrozen on spike / death tile — not a scored spike kill, no announcer.
		pChr->Core()->m_Killer.m_ClientID = -1;
		pChr->Core()->m_Killer.m_Weapon = -1;
		pChr->Die(ClientID, WEAPON_WORLD);
		return;
	}

	int KillerID = pChr->Core()->m_Killer.m_ClientID;

	if(KillerID >= 0 && KillerID < MAX_CLIENTS && KillerID != ClientID && m_aClientRoom[KillerID] == R
		&& !SameSide(KillerID, ClientID, R))
	{
		DoSpike(ClientID, SpikeTile, KillerID);
		pChr->Core()->m_Killer.m_Weapon = WEAPON_NINJA;
		pChr->Die(KillerID, WEAPON_NINJA);
	}
	else
	{
		pChr->Core()->m_Killer.m_ClientID = -1;
		pChr->Core()->m_Killer.m_Weapon = -1;
		pChr->Die(ClientID, WEAPON_WORLD);
	}
}

void CFng::DoSpike(int ClientID, int SpikeTile, int KillerID)
{
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientID);
	if(!pChr) return;
	int R = m_aClientRoom[ClientID];
	if(R < 0) return;
	CFngRoom &r = m_aRooms[R];

	CPlayer *pKiller = (KillerID >= 0 && KillerID < MAX_CLIENTS) ? GameServer()->m_apPlayers[KillerID] : nullptr;
	if(!pKiller) return;

	int PlayerPoints = FNG_SCORE_SPIKE_NORMAL;
	int TeamPoints = FNG_TEAM_SCORE_SPIKE_NORMAL;
	bool WrongSpike = false;

	switch(SpikeTile)
	{
	case TILE_FNG_SPIKE_GOLD:
		PlayerPoints = FNG_SCORE_SPIKE_GOLD;
		TeamPoints = FNG_TEAM_SCORE_SPIKE_GOLD;
		break;
	case TILE_FNG_SPIKE_GREEN:
		PlayerPoints = FNG_SCORE_SPIKE_GREEN;
		TeamPoints = FNG_TEAM_SCORE_SPIKE_GREEN;
		break;
	case TILE_FNG_SPIKE_PURPLE:
		PlayerPoints = FNG_SCORE_SPIKE_PURPLE;
		TeamPoints = FNG_TEAM_SCORE_SPIKE_PURPLE;
		break;
	case TILE_FNG_SPIKE_RED:
		PlayerPoints = FNG_SCORE_SPIKE_TEAM;
		TeamPoints = FNG_TEAM_SCORE_SPIKE_TEAM;
		if(r.m_TeamMode && r.m_aSide[KillerID] != FNG_SIDE_RED)
			WrongSpike = true;
		break;
	case TILE_FNG_SPIKE_BLUE:
		PlayerPoints = FNG_SCORE_SPIKE_TEAM;
		TeamPoints = FNG_TEAM_SCORE_SPIKE_TEAM;
		if(r.m_TeamMode && r.m_aSide[KillerID] != FNG_SIDE_BLUE)
			WrongSpike = true;
		break;
	default: break;
	}

	if(WrongSpike)
	{
		r.m_aPoints[KillerID] -= PlayerPoints;
		if(r.m_TeamMode)
		{
			int Side = r.m_aSide[KillerID];
			if(Side == FNG_SIDE_RED || Side == FNG_SIDE_BLUE)
				r.m_aTeamPoints[Side] -= TeamPoints;
		}
		SendScoreToAll(R);
		CCharacter *pKillerChr = GameServer()->GetPlayerChar(KillerID);
		if(pKillerChr)
			pKillerChr->Freeze((float)FNG_WRONG_SPIKE_FREEZE);
		GameServer()->CreateSound(pChr->GetPos(), SOUND_CTF_CAPTURE, pChr->TeamMask());
		return;
	}

	r.m_aPoints[KillerID] += PlayerPoints;
	r.m_aKills[KillerID]++;
	if(r.m_TeamMode)
	{
		int Side = r.m_aSide[KillerID];
		if(Side == FNG_SIDE_RED || Side == FNG_SIDE_BLUE)
			r.m_aTeamPoints[Side] += TeamPoints;
	}
	SendScoreToAll(R);

	GameServer()->CreateSound(pChr->GetPos(), SOUND_CTF_CAPTURE, pChr->TeamMask());

	// Announcer only on real spike kills — not freeze, not TILE_DEATH (2) / WEAPON_WORLD.
	CCharacter *pKillerChr = GameServer()->GetPlayerChar(KillerID);
	if(pKillerChr)
	{
		/* TODO: m_KillStreak++ */ (void)0;
		/* TODO: PlayKillAnnouncer */
		if(false /* TODO: killstreak */)
		{
			GameServer()->SendChatFormat(-1, TEAM_ALL, -1, CGameContext::CHATFLAG_ALL,
				Localizable("%s is on a killing spree with %d kills"),
				Server()->ClientName(KillerID), 0);
			GameServer()->CreateFinishConfetti(pKillerChr->GetPos(), pKillerChr->TeamMask());
		}
	}

	const char *pKillerName = Server()->ClientName(KillerID);
	BroadcastRoomFormat(R, true, Localizable("'%s' spike! (+%d)"), pKillerName, PlayerPoints);
}

void CFng::OnTeamCommand(int ClientID, int TeamNum)
{
	int R = FindRoomByTeam(TeamNum);
	if(R < 0) { GameServer()->SendChatTarget(ClientID, Localizable("No FNG room on that team")); return; }
	if(m_aClientRoom[ClientID] == R)
		return;

	CGameTeams *pTeams = &TEAMS;
	if(pTeams->TeamLocked(TeamNum) && !pTeams->IsInvited(TeamNum, ClientID))
	{
		GameServer()->SendChatTarget(ClientID, Localizable("This FNG room is locked. Ask a member to /invite or /lock."));
		return;
	}

	int MinigameType = GameServer()->m_apPlayers[ClientID] ? GameServer()->m_apPlayers[ClientID]->m_Minigame : -1;
	if(m_aRooms[R].m_MinigameType != MinigameType)
	{
		CPlayer *pP = GameServer()->m_apPlayers[ClientID];
		const char *pWrongType = pP ? pP->Localize(Localizable("Wrong type. Active rooms: ")) : Localizable("Wrong type. Active rooms: ");
		char aBuf[256]; str_copy(aBuf, pWrongType, sizeof(aBuf));
		bool First = true;
		for(int i=0;i<MAX_FNG_ROOMS;i++)
			if(m_aRooms[i].m_Active && m_aRooms[i].m_MinigameType == MinigameType)
			{
				char tmp[8]; str_format(tmp, sizeof(tmp), First?"%d":", %d", m_aRooms[i].m_Team);
				str_append(aBuf, tmp, sizeof(aBuf)); First = false;
			}
		GameServer()->SendChatTarget(ClientID, aBuf); return;
	}
	MoveClientToRoom(ClientID, R);
}

bool CFng::OnClientSetTeam(int ClientID, int Team)
{
	int R = GetClientRoom(ClientID);
	if(R < 0 || (Team != TEAM_SPECTATORS && Team != TEAM_RED && Team != TEAM_BLUE))
		return false;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientID];
	if(!pPlayer)
		return false;

	if(Team == TEAM_SPECTATORS)
	{
		if(pPlayer->GetTeam() == TEAM_SPECTATORS)
			return true;
		pPlayer->SetTeam(TEAM_SPECTATORS, false);
		OnActivePlayersChanged(R);
		RefreshRoomClient(ClientID, R);
		return true;
	}

	const bool InSpec = pPlayer->GetTeam() == TEAM_SPECTATORS || pPlayer->IsPaused();
	if(m_aRooms[R].m_TeamMode)
	{
		const int Side = Team == TEAM_BLUE ? FNG_SIDE_BLUE : FNG_SIDE_RED;
		const bool SameSide = m_aRooms[R].m_aSide[ClientID] == Side;
		if(SameSide && !InSpec && pPlayer->GetCharacter())
			return true;
		m_aRooms[R].m_aSide[ClientID] = Side;
	}
	else if(!InSpec && pPlayer->GetCharacter())
		return true;

	RestoreIsolationTeam(ClientID, R);
	if(InSpec)
		pPlayer->SetPlaying(false);
	else
		pPlayer->KillCharacter(WEAPON_GAME);
	RestoreIsolationTeam(ClientID, R);

	// Warmup/waiting transition first, so TryRespawn uses the new room state.
	OnActivePlayersChanged(R);

	pPlayer->Respawn();
	pPlayer->TryRespawn();
	RestoreIsolationTeam(ClientID, R);
	RefreshRoomClient(ClientID, R);
	return true;
}

void CFng::OnVote(int ClientID, bool Yes)
{
	OnSettingsVote(ClientID, Yes);
}

void CFng::OnModeVote(int ClientID, bool Yes)
{
	OnSettingsVote(ClientID, Yes);
}

void CFng::OnSettingsVote(int ClientID, bool Yes)
{
	int R = m_aClientRoom[ClientID];
	if(R < 0) return;
	CPendingSetting &P = m_aPending[R];
	if(P.m_Type == CPendingSetting::NONE) return;
	if(Yes) P.m_VoteYes++; else P.m_VoteNo++;
	if(CountActivePlayers(R) > 0 && (P.m_VoteYes + P.m_VoteNo) >= CountActivePlayers(R))
		ApplyPendingSetting(R);
}

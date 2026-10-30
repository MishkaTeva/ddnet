// FNG / BoomFNG minigame
// Solo (FFA) or Team (red vs blue). Settings via vote menu.
#ifndef GAME_SERVER_MINIGAMES_FNG_H
#define GAME_SERVER_MINIGAMES_FNG_H

#include "minigame.h"
#include <engine/shared/protocol.h>
#include <base/types.h>

#define MAX_FNG_ROOMS (VANILLA_MAX_CLIENTS - 1)

// Fallback defaults (overridden in fng.cpp)
#define FNG_ROUND_DURATION 0 // 0 = no time limit

#ifndef TILE_FNG_SPIKE_GOLD
#define TILE_FNG_SPIKE_GOLD   7
#endif
#ifndef TILE_FNG_SPIKE_NORMAL
#define TILE_FNG_SPIKE_NORMAL 8
#endif
#ifndef TILE_FNG_SPIKE_RED
#define TILE_FNG_SPIKE_RED    9
#endif
#ifndef TILE_FNG_SPIKE_BLUE
#define TILE_FNG_SPIKE_BLUE   10
#endif
#ifndef TILE_FNG_SPIKE_GREEN
#define TILE_FNG_SPIKE_GREEN  14
#endif
#ifndef TILE_FNG_SPIKE_PURPLE
#define TILE_FNG_SPIKE_PURPLE 15
#endif

#define FNG_SCORE_SPIKE_NORMAL 3
#define FNG_SCORE_SPIKE_GOLD   8
#define FNG_SCORE_SPIKE_GREEN  6
#define FNG_SCORE_SPIKE_PURPLE 10
#define FNG_SCORE_SPIKE_TEAM   5

#define FNG_FREEZE_SECS 8

#define FNG_HAMMER_SCALE_X 320
#define FNG_HAMMER_SCALE_Y 120

enum EFngRoomState
{
	FNG_STATE_WAITING   = 0,
	FNG_STATE_WARMUP    = 1,
	FNG_STATE_ROUND     = 2,
	FNG_STATE_POSTROUND = 3,
};

enum EFngWarmupPhase
{
	FNG_WARMUP_PHYSICS = 0, // vote: reset tunes?
	FNG_WARMUP_MODE    = 1, // vote: Team FNG?
};

enum EFngSide
{
	FNG_SIDE_NONE = -1,
	FNG_SIDE_RED  = 0,
	FNG_SIDE_BLUE = 1,
};

struct CFngRoom
{
	bool m_Active;
	int  m_MinigameType;
	int  m_Team; // DDRace isolation team

	EFngRoomState m_State;
	EFngWarmupPhase m_WarmupPhase;
	int64_t m_TimerTick;
	int64_t m_RoundStartTick; // for race-time HUD (elapsed)

	// physics vote
	int  m_VoteYes;
	int  m_VoteNo;
	bool m_TuneReset;

	// mode vote (Team FNG?)
	int  m_ModeVoteYes;
	int  m_ModeVoteNo;
	bool m_TeamMode;

	int m_aSide[MAX_CLIENTS]; // EFngSide
	int m_aTeamPoints[2];
	int m_ScoreLimit;

	int m_aPoints[MAX_CLIENTS];
	int m_aKills[MAX_CLIENTS];
	int m_aDeaths[MAX_CLIENTS];

	void Reset()
	{
		m_Active = false;
		m_MinigameType = -1;
		m_Team = -1;
		m_State = FNG_STATE_WAITING;
		m_WarmupPhase = FNG_WARMUP_PHYSICS;
		m_TimerTick = 0;
		m_RoundStartTick = 0;
		m_VoteYes = 0;
		m_VoteNo = 0;
		m_TuneReset = true; // original physics by default
		m_ModeVoteYes = 0;
		m_ModeVoteNo = 0;
		m_TeamMode = true; // team mode by default
		m_ScoreLimit = 300;
		m_aTeamPoints[0] = m_aTeamPoints[1] = 0;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			m_aSide[i] = FNG_SIDE_NONE;
			m_aPoints[i] = 0;
			m_aKills[i] = 0;
			m_aDeaths[i] = 0;
		}
	}
};

class CFng : public CMinigame
{
public:
	CFng(CGameContext *pGameServer, int Type);

	virtual void Tick() override;

	int OnPlayerJoin(int ClientID, int MinigameType);
	void OnClientJoinedRoom(int RoomIdx);
	void OnPlayerLeave(int ClientID, bool Disconnect = false);

	void OnCharacterSpawn(int ClientID);
	void OnPlayerDie(int ClientID, int KillerID, int Weapon);
	bool OnWeaponHit(int VictimID, int AttackerID, int Weapon);
	void OnSpikeTile(int ClientID, int SpikeTile);
	bool OnHammerHit(int AttackerID, int VictimID, vec2 &Force);
	bool CanSelfkill(int ClientID) const;

	void OnTeamCommand(int ClientID, int TeamNum);
	bool OnClientSetTeam(int ClientID, int Team);
	void CreateNewRoom(int ClientID);
	void LeaveRoomToLobby(int ClientID);
	void OnVote(int ClientID, bool Yes);
	void OnModeVote(int ClientID, bool Yes);
	void OnSettingsVote(int ClientID, bool Yes);

	int  GetClientRoom(int ClientID) const;
	bool IsInRound(int ClientID) const;
	bool IsWaiting(int ClientID) const;
	int  GetRoomTeam(int ClientID) const;
	int  GetSide(int ClientID) const;
	bool IsTeamMode(int ClientID) const;
	int  GetScoreLimit(int ClientID) const;
	int  GetTeamPoints(int ClientID, int Side) const;
	bool IsInRoundOrWarmup(int ClientID) const;
	int64_t GetRoundStartTick(int ClientID) const;
	int  GetSpawnEntity(int ClientID) const; // ENTITY_SPAWN_RED / BLUE

	// Settings (vote menu). Mid-round changes may require room confirmation vote.
	bool TrySetTuneReset(int ClientID, bool Value);
	bool TrySetTeamMode(int ClientID, bool Value);
	bool TrySetScoreLimit(int ClientID, int Value);
	bool GetTuneReset(int ClientID) const;
	bool GetTeamModeSetting(int ClientID) const;
	int  GetScoreLimitSetting(int ClientID) const;

	CFngRoom m_aRooms[MAX_FNG_ROOMS];

private:
	int m_aClientRoom[MAX_CLIENTS];

	void TickRoom(int R);
	void TickWaiting(int R);
	void TickWarmup(int R);
	void TickRound(int R);
	void TickPostRound(int R);

	void StartWarmup(int R);
	void StartRound(int R);
	void EndRound(int R);
	void AssignSides(int R);
	bool CheckWin(int R);

	int  CountPlayers(int R) const;
	int  CountActivePlayers(int R) const;
	int  CountSide(int R, int Side) const;
	bool IsPlayingInRoom(int ClientID) const;
	void RestoreIsolationTeam(int ClientID, int R);
	void AbortToWaiting(int R);
	void OnActivePlayersChanged(int R);
	void RefreshRoomClient(int ClientID, int R);
	int  GetLeaderPoints(int R) const;
	bool IsClientFrozen(int ClientID) const;
	bool SameSide(int A, int B, int R) const;

	void BroadcastRoom(int R, const char *pMsg, bool Important = true);
	void BroadcastRoomFormat(int R, bool Important, const char *pFmt, ...);
	void SendScoreToAll(int R);
	void ResetSessionStats(int R);
	void SyncGameInfoRoom(int R);

	void KillAllInRoom(int R);
	void GiveWeapons(int ClientID);
	void ApplyFngTunings(int ClientID);
	void RemoveFngTunings(int ClientID);

	void MoveClientToRoom(int ClientID, int RoomIdx);
	int  FindFreeRoom() const;
	int  FindRoomByTeam(int Team) const;

	void DoSpike(int ClientID, int SpikeTile, int KillerID);

	// Pending settings change confirmation (room vote)
	struct CPendingSetting
	{
		enum EType { NONE = 0, TUNE_RESET, TEAM_MODE, SCORE_LIMIT };
		EType m_Type;
		int m_IntValue;
		bool m_BoolValue;
		int m_VoteYes;
		int m_VoteNo;
		int64_t m_EndTick;
		void Reset() { m_Type = NONE; m_IntValue = 0; m_BoolValue = false; m_VoteYes = m_VoteNo = 0; m_EndTick = 0; }
	};
	CPendingSetting m_aPending[MAX_FNG_ROOMS];
	void StartSettingsConfirmVote(int R, CPendingSetting::EType Type, int IntValue, bool BoolValue);
	void ApplyPendingSetting(int R);
	void TickSettingsVote(int R);
	bool ApplySettingDirect(int R, CPendingSetting::EType Type, int IntValue, bool BoolValue);
};

#endif

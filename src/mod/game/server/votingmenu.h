#ifndef MOD_GAME_SERVER_VOTINGMENU_H
#define MOD_GAME_SERVER_VOTINGMENU_H

#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/voting.h>

#include <vector>

class CGameContext;
class IServer;
struct CVoteOptionServer;

// Faithful F-DDrace voting menu (page chrome + account + minigame settings).
class CVotingMenu
{
	enum AccFlags
	{
		VOTEFLAG_PAGE_VOTES = 1 << 0,
		VOTEFLAG_PAGE_ACCOUNT = 1 << 1,
		VOTEFLAG_PAGE_MISCELLANEOUS = 1 << 2,
		VOTEFLAG_SHOW_ACC_INFO = 1 << 3,
		VOTEFLAG_SHOW_ACC_STATS = 1 << 4,
		VOTEFLAG_SHOW_PLOT_INFO = 1 << 5,
		VOTEFLAG_SHOW_WANTED_PLAYERS = 1 << 6,
		VOTEFLAG_PAGE_LANGUAGES = 1 << 7,
	};

	enum EVotingPage
	{
		PAGE_VOTES = 0,
		PAGE_ACCOUNT,
		PAGE_MISCELLANEOUS,
		PAGE_LANGUAGES,
		NUM_PAGES,

		NUM_PAGE_SEPERATORS = 2,
		NUM_OPTION_START_OFFSET = NUM_PAGES + NUM_PAGE_SEPERATORS,
		NUM_PAGE_MAX_VOTES = 128 - NUM_OPTION_START_OFFSET,
	};

	enum
	{
		PREVFLAG_ACC_VIP_RAINBOW = 1 << 6,
		PREVFLAG_ACC_VIP_BLOODY = 1 << 7,
		PREVFLAG_ACC_VIP_ATOM = 1 << 8,
		PREVFLAG_ACC_VIP_TRAIL = 1 << 9,
		PREVFLAG_ACC_VIP_PLUS_ROTATINGBALL = 1 << 12,
		PREVFLAG_ACC_VIP_PLUS_EPICCIRCLE = 1 << 13,
		PREVFLAG_ACC_VIP_PLUS_LOVELY = 1 << 14,
		PREVFLAG_ACC_NINJAJETPACK = 1 << 18,
	};

	CGameContext *m_pGameServer = nullptr;
	CGameContext *GameServer() const { return m_pGameServer; }
	IServer *Server() const;
	bool m_Initialized = false;

	struct SClientVoteInfo
	{
		int m_Page = PAGE_MISCELLANEOUS;
		int64_t m_LastVoteMsg = 0;
		int64_t m_NextVoteSendTick = 0;
		bool m_ShowAccountInfo = false;
		bool m_ShowAccountStats = false;
		bool m_ShowPlotInfo = false;
		bool m_ShowMinigameSettings = false;

		struct SPrevStats
		{
			int m_Flags = 0;
			int m_Minigame = 0;
			int64_t m_JailTime = 0;
			int m_FngScoreLimit = 0;
			int m_FngFlags = 0; // bit0 tune, bit1 team
			struct
			{
				int64_t m_XP = 0;
				int64_t m_Money = 0;
				int64_t m_WalletMoney = 0;
				int m_PoliceLevel = 0;
				int m_TaserBattery = 0;
				int m_PortalBattery = 0;
				int m_Points = 0;
				int m_Kills = 0;
				int m_Deaths = 0;
				char m_aContact[128] = {};
			} m_Acc;
		};
		SPrevStats m_PrevStats;
	} m_aClients[MAX_CLIENTS];

	bool FillStats(int ClientId, SClientVoteInfo::SPrevStats *pStats);
	int GetPage(int ClientId) const { return m_aClients[ClientId].m_Page; }

	struct SPageInfo
	{
		char m_aName[64];
	} m_aPages[NUM_PAGES];
	char m_aaTempDesc[NUM_PAGE_MAX_VOTES][VOTE_DESC_LENGTH];

	bool SetPage(int ClientId, int Page);
	const char *GetPageDescription(int ClientId, int Page);

	bool IsOptionWithSuffix(const char *pDesc, const char *pWantedOption);
	bool IsOptionPrefix(const char *pDesc, const char *pWantedOption);
	bool IsOption(const char *pDesc, const char *pWantedOption);
	bool OnMessageSuccess(int ClientId, const char *pDesc, const char *pReason);

	int PrepareTempDescriptions(int ClientId);
	void DoPageAccount(int ClientId, int *pNumOptions);
	void DoPageMiscellaneous(int ClientId, int *pNumOptions);
	void DoPageLanguages(int ClientId, int *pNumOptions);
	void DoLineToggleOption(int *pNumOptions, const char *pDescription, bool Value);
	void DoLineValueOption(int *pNumOptions, const char *pDescription, int Value, int Max = -1, int BulletPoint = 0);
	void DoLineSeperator(int *pNumOptions);
	enum
	{
		BULLET_NONE,
		BULLET_POINT,
		BULLET_ARROW,
		BULLET_ARROW_LEFT,
		BULLET_PLUS,
	};
	void DoLineText(int *pNumOptions, const char *pDescription, int BulletPoint = BULLET_NONE);
	void DoLineTextSubheader(int *pNumOptions, const char *pDescription);
	bool DoLineCollapse(int *pNumOptions, const char *pDescription, bool ShowContent, int NumEntries);
	void AddWrappedLine(int *pNumOptions, const char *pDescription, const char *pPrefix, bool CollapseHeader);
	int m_NumCollapseEntries = 0;

	void FillVoteOptionMsg(CNetMsg_Sv_VoteOptionListAdd *pMsg, int Index, const char *pDesc);

public:
	enum
	{
		MAX_VOTES_PER_PACKET = 15,
	};

	void Init(CGameContext *pGameServer);
	void Tick();

	bool OnMessage(int ClientId, const CNetMsg_Cl_CallVote *pMsg);
	void ProgressVoteOptions(int ClientId);
	void SendPageVotes(int ClientId, bool ResendVotesPage = true);

	void Reset(int ClientId);
	void InitPlayer(int ClientId);

	void ApplyFlags(int ClientId, int Flags);
	int GetFlags(int ClientId);

	int OptionStartOffset() const { return NUM_OPTION_START_OFFSET; }
	bool IsVotesPage(int ClientId) const { return GetPage(ClientId) == PAGE_VOTES; }
};

#endif

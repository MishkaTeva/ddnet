// F-DDrace-compatible account layer for DDNet-base migration.
// On-disk format matches F-DDrace: one value per line, NUM_ACCOUNT_VARIABLES lines.

#ifndef MOD_GAME_SERVER_ACCOUNT_H
#define MOD_GAME_SERVER_ACCOUNT_H

#include <base/hash.h>
#include <base/types.h>

#include <engine/shared/protocol.h>

#include <ctime>
#include <fstream>
#include <string>
#include <vector>

class CGameContext;
class CPlayer;

enum
{
	DIFFERENCE_XP_END = 100,
	OVER_LVL_100_XP = 2000000,

	ACC_START = 1,
	VIP_NONE = 0,
	VIP_CLASSIC = 1,
	VIP_PLUS = 2,
	VIP_ADMIN = 5,
	ACC_USERNAME_LENGTH = 32,
	ACC_CONTACT_LENGTH = 128,
	ACC_CURRENT_VERSION = 15,
};

// Must match F-DDrace CGameContext::AccountVariables order exactly.
enum
{
	ACC_PORT = 0,
	ACC_LOGGED_IN,
	ACC_DISABLED,
	ACC_PASSWORD,
	ACC_USERNAME,
	ACC_CLIENT_ID,
	ACC_LEVEL,
	ACC_XP,
	ACC_MONEY,
	ACC_KILLS,
	ACC_DEATHS,
	ACC_POLICE_LEVEL,
	ACC_SURVIVAL_KILLS,
	ACC_SURVIVAL_WINS,
	ACC_SPOOKY_GHOST,
	ACC_LAST_MONEY_TRANSACTION_0,
	ACC_LAST_MONEY_TRANSACTION_1,
	ACC_LAST_MONEY_TRANSACTION_2,
	ACC_LAST_MONEY_TRANSACTION_3,
	ACC_LAST_MONEY_TRANSACTION_4,
	ACC_VIP,
	ACC_BLOCK_POINTS,
	ACC_INSTAGIB_KILLS,
	ACC_INSTAGIB_WINS,
	ACC_SPAWN_WEAPON_0,
	ACC_SPAWN_WEAPON_1,
	ACC_SPAWN_WEAPON_2,
	ACC_NINJAJETPACK,
	ACC_LAST_PLAYER_NAME,
	ACC_SURVIVAL_DEATHS,
	ACC_INSTAGIB_DEATHS,
	ACC_TASER_LEVEL,
	ACC_KILLING_SPREE_RECORD,
	ACC_EUROS,
	ACC_EXPIRE_DATE_VIP,
	ACC_PORTAL_RIFLE,
	ACC_EXPIRE_DATE_PORTAL_RIFLE,
	ACC_VERSION,
	ACC_ADDR,
	ACC_LAST_ADDR,
	ACC_TASER_BATTERY,
	ACC_CONTACT,
	ACC_TIMEOUT_CODE,
	ACC_SECURITY_PIN,
	ACC_REGISTER_DATE,
	ACC_LAST_LOGIN_DATE,
	ACC_FLAGS,
	ACC_EMAIL,
	ACC_DESIGN,
	ACC_PORTAL_BATTERY,
	ACC_PORTAL_BLOCKER,
	ACC_VOTE_MENU_FLAGS,
	ACC_DURAK_WINS,
	ACC_DURAK_PROFIT,
	ACC_LANGUAGE,
	ACC_QUEST_PROGRESS_0,
	ACC_QUEST_PROGRESS_1,
	ACC_QUEST_PROGRESS_2,
	ACC_QUEST_PROGRESS_3,
	ACC_QUEST_PROGRESS_4,
	ACC_QUEST_COMPLETED_0,
	ACC_QUEST_COMPLETED_1,
	ACC_QUEST_COMPLETED_2,
	ACC_QUEST_COMPLETED_3,
	ACC_QUEST_COMPLETED_4,
	ACC_QUEST_DAY,
	ACC_TOTAL_FARM_MONEY,
	ACC_REGISTER_PLAYER_NAME,
	NUM_ACCOUNT_VARIABLES
};

struct SAccountInfo
{
	int m_Port = 0;
	bool m_LoggedIn = false;
	bool m_Disabled = false;
	SHA256_DIGEST m_Password{};
	char m_aUsername[ACC_USERNAME_LENGTH]{};
	int m_ClientId = -1;
	int m_Level = 0;
	int64_t m_XP = 0;
	int64_t m_Money = 0;
	int m_Kills = 0;
	int m_Deaths = 0;
	int m_PoliceLevel = 0;
	int m_SurvivalKills = 0;
	int m_SurvivalWins = 0;
	bool m_SpookyGhost = false;
	char m_aLastMoneyTransaction[5][128]{};
	int m_VIP = VIP_NONE;
	int m_BlockPoints = 0;
	int m_InstagibKills = 0;
	int m_InstagibWins = 0;
	int m_aSpawnWeapon[3]{};
	bool m_Ninjajetpack = false;
	char m_aLastPlayerName[MAX_NAME_LENGTH]{};
	int m_SurvivalDeaths = 0;
	int m_InstagibDeaths = 0;
	int m_TaserLevel = 0;
	int m_KillingSpreeRecord = 0;
	float m_Euros = 0.f;
	int64_t m_ExpireDateVIP = 0;
	int m_PortalRifle = 0;
	int64_t m_ExpireDatePortalRifle = 0;
	int m_Version = ACC_CURRENT_VERSION;
	NETADDR m_Addr{};
	NETADDR m_LastAddr{};
	int m_TaserBattery = 0;
	char m_aContact[ACC_CONTACT_LENGTH]{};
	char m_aTimeoutCode[64]{};
	char m_aSecurityPin[5]{};
	time_t m_RegisterDate = 0;
	time_t m_LastLoginDate = 0;
	int m_Flags = 0;
	char m_aEmail[128]{};
	char m_aDesign[256]{};
	int m_PortalBattery = 0;
	int m_PortalBlocker = 0;
	int m_VoteMenuFlags = 0;
	int m_DurakWins = 0;
	int64_t m_DurakProfit = 0;
	char m_aLanguage[32]{};
	int m_aQuestProgress[5]{};
	bool m_aQuestCompleted[5]{};
	int m_QuestDay = 0;
	int64_t m_TotalFarmMoney = 0;
	char m_aRegisterPlayerName[MAX_NAME_LENGTH]{};
};

class CAccountSystem
{
	CGameContext *m_pGameServer = nullptr;
	std::vector<SAccountInfo> m_vAccounts;

	void EnsureDir() const;
	void AccountPath(char *pBuf, int BufSize, const char *pUsername) const;
	bool ReadFile(int AccId);
	bool WriteFile(int AccId) const;
	bool ReadNativeLines(int AccId, std::ifstream &File);
	bool ReadSlimKeyValue(int AccId, std::ifstream &File, const std::string &FirstLine);
	void SetVar(int AccId, int VariableId, const char *pData);
	const char *GetVarValue(int AccId, int VariableId) const;
	static SHA256_DIGEST HashPassword(const char *pPassword);
	int AllocSlot();

public:
	void Init(CGameContext *pGameServer);
	void Shutdown();

	SAccountInfo *Get(int AccId);
	const SAccountInfo *Get(int AccId) const;
	int Num() const { return (int)m_vAccounts.size(); }

	int FindByUsername(const char *pUsername) const;
	int FindByClientId(int ClientId) const;

	bool Register(int ClientId, const char *pUsername, const char *pPassword);
	bool Login(int ClientId, const char *pUsername, const char *pPassword);
	void Logout(int AccId, bool Silent = false);
	void LogoutClient(int ClientId, bool Silent = false);

	void SetVip(int AccId, int VipLevel, int64_t ExpireDate = 0);
	void SyncMoveRestrictions(CPlayer *pPlayer) const;
	void SyncWalletFromPlayer(int AccId);
	bool Persist(int AccId);

	bool BuyRoomKey(int ClientId);
	void ApplyAccountLoadouts(CPlayer *pPlayer) const;
	void ExpireTimedItems(int ClientId);

	void JailPlayer(int ClientId, int Seconds);
	void ReleaseJail(int ClientId);
	bool IsJailed(int ClientId) const;
	int64_t GetNeededXP(int Level) const;
};

#endif

#include "account.h"

#include <base/fs.h>
#include <base/hash.h>
#include <base/log.h>
#include <base/net.h>
#include <base/str.h>

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/weapons_ex.h>

#include <mod/game/server/entities/jail_arrest.h>
#include <mod/game/server/entities/jail_release.h>

#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>

void CAccountSystem::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_vAccounts.clear();
	m_vAccounts.emplace_back(); // index 0 = not logged in
	EnsureDir();
	log_info("account", "F-DDrace-compatible accounts ready (path=%s vars=%d)",
		g_Config.m_SvAccFilePath, NUM_ACCOUNT_VARIABLES);
}

void CAccountSystem::Shutdown()
{
	for(int i = ACC_START; i < Num(); i++)
	{
		if(m_vAccounts[i].m_LoggedIn)
			Logout(i, true);
	}
	m_vAccounts.clear();
	m_pGameServer = nullptr;
}

void CAccountSystem::EnsureDir() const
{
	(void)fs_makedir_rec_for(g_Config.m_SvAccFilePath);
	(void)fs_makedir(g_Config.m_SvAccFilePath);
}

void CAccountSystem::AccountPath(char *pBuf, int BufSize, const char *pUsername) const
{
	str_format(pBuf, BufSize, "%s/%s.acc", g_Config.m_SvAccFilePath, pUsername);
}

SAccountInfo *CAccountSystem::Get(int AccId)
{
	if(AccId < 0 || AccId >= Num())
		return nullptr;
	return &m_vAccounts[AccId];
}

const SAccountInfo *CAccountSystem::Get(int AccId) const
{
	if(AccId < 0 || AccId >= Num())
		return nullptr;
	return &m_vAccounts[AccId];
}

int CAccountSystem::FindByUsername(const char *pUsername) const
{
	if(!pUsername || !pUsername[0])
		return -1;
	for(int i = ACC_START; i < Num(); i++)
	{
		if(str_comp_nocase(m_vAccounts[i].m_aUsername, pUsername) == 0)
			return i;
	}
	return -1;
}

int CAccountSystem::FindByClientId(int ClientId) const
{
	for(int i = ACC_START; i < Num(); i++)
	{
		if(m_vAccounts[i].m_LoggedIn && m_vAccounts[i].m_ClientId == ClientId)
			return i;
	}
	return -1;
}

int CAccountSystem::AllocSlot()
{
	for(int i = ACC_START; i < Num(); i++)
	{
		if(!m_vAccounts[i].m_LoggedIn && m_vAccounts[i].m_aUsername[0] == '\0')
			return i;
	}
	m_vAccounts.emplace_back();
	return Num() - 1;
}

SHA256_DIGEST CAccountSystem::HashPassword(const char *pPassword)
{
	return sha256(pPassword, str_length(pPassword));
}

void CAccountSystem::SetVar(int AccId, int VariableId, const char *pData)
{
	SAccountInfo *pAcc = Get(AccId);
	if(!pAcc || !pData)
		return;

	switch(VariableId)
	{
	case ACC_PORT: pAcc->m_Port = atoi(pData); break;
	case ACC_LOGGED_IN: pAcc->m_LoggedIn = atoi(pData) != 0; break;
	case ACC_DISABLED: pAcc->m_Disabled = atoi(pData) != 0; break;
	case ACC_PASSWORD: sha256_from_str(&pAcc->m_Password, pData); break;
	case ACC_USERNAME: str_copy(pAcc->m_aUsername, pData, sizeof(pAcc->m_aUsername)); break;
	case ACC_CLIENT_ID: pAcc->m_ClientId = atoi(pData); break;
	case ACC_LEVEL: pAcc->m_Level = atoi(pData); break;
	case ACC_XP: pAcc->m_XP = atoll(pData); break;
	case ACC_MONEY: pAcc->m_Money = atoll(pData); break;
	case ACC_KILLS: pAcc->m_Kills = atoi(pData); break;
	case ACC_DEATHS: pAcc->m_Deaths = atoi(pData); break;
	case ACC_POLICE_LEVEL: pAcc->m_PoliceLevel = atoi(pData); break;
	case ACC_SURVIVAL_KILLS: pAcc->m_SurvivalKills = atoi(pData); break;
	case ACC_SURVIVAL_WINS: pAcc->m_SurvivalWins = atoi(pData); break;
	case ACC_SPOOKY_GHOST: pAcc->m_SpookyGhost = atoi(pData) != 0; break;
	case ACC_LAST_MONEY_TRANSACTION_0: str_copy(pAcc->m_aLastMoneyTransaction[0], pData, sizeof(pAcc->m_aLastMoneyTransaction[0])); break;
	case ACC_LAST_MONEY_TRANSACTION_1: str_copy(pAcc->m_aLastMoneyTransaction[1], pData, sizeof(pAcc->m_aLastMoneyTransaction[1])); break;
	case ACC_LAST_MONEY_TRANSACTION_2: str_copy(pAcc->m_aLastMoneyTransaction[2], pData, sizeof(pAcc->m_aLastMoneyTransaction[2])); break;
	case ACC_LAST_MONEY_TRANSACTION_3: str_copy(pAcc->m_aLastMoneyTransaction[3], pData, sizeof(pAcc->m_aLastMoneyTransaction[3])); break;
	case ACC_LAST_MONEY_TRANSACTION_4: str_copy(pAcc->m_aLastMoneyTransaction[4], pData, sizeof(pAcc->m_aLastMoneyTransaction[4])); break;
	case ACC_VIP: pAcc->m_VIP = atoi(pData); break;
	case ACC_BLOCK_POINTS: pAcc->m_BlockPoints = atoi(pData); break;
	case ACC_INSTAGIB_KILLS: pAcc->m_InstagibKills = atoi(pData); break;
	case ACC_INSTAGIB_WINS: pAcc->m_InstagibWins = atoi(pData); break;
	case ACC_SPAWN_WEAPON_0: pAcc->m_aSpawnWeapon[0] = atoi(pData); break;
	case ACC_SPAWN_WEAPON_1: pAcc->m_aSpawnWeapon[1] = atoi(pData); break;
	case ACC_SPAWN_WEAPON_2: pAcc->m_aSpawnWeapon[2] = atoi(pData); break;
	case ACC_NINJAJETPACK: pAcc->m_Ninjajetpack = atoi(pData) != 0; break;
	case ACC_LAST_PLAYER_NAME: str_copy(pAcc->m_aLastPlayerName, pData, sizeof(pAcc->m_aLastPlayerName)); break;
	case ACC_SURVIVAL_DEATHS: pAcc->m_SurvivalDeaths = atoi(pData); break;
	case ACC_INSTAGIB_DEATHS: pAcc->m_InstagibDeaths = atoi(pData); break;
	case ACC_TASER_LEVEL: pAcc->m_TaserLevel = atoi(pData); break;
	case ACC_KILLING_SPREE_RECORD: pAcc->m_KillingSpreeRecord = atoi(pData); break;
	case ACC_EUROS: pAcc->m_Euros = (float)atof(pData); break;
	case ACC_EXPIRE_DATE_VIP: pAcc->m_ExpireDateVIP = atoll(pData); break;
	case ACC_PORTAL_RIFLE: pAcc->m_PortalRifle = atoi(pData); break;
	case ACC_EXPIRE_DATE_PORTAL_RIFLE: pAcc->m_ExpireDatePortalRifle = atoll(pData); break;
	case ACC_VERSION: pAcc->m_Version = atoi(pData); break;
	case ACC_ADDR: net_addr_from_str(&pAcc->m_Addr, pData); break;
	case ACC_LAST_ADDR: net_addr_from_str(&pAcc->m_LastAddr, pData); break;
	case ACC_TASER_BATTERY: pAcc->m_TaserBattery = atoi(pData); break;
	case ACC_CONTACT: str_copy(pAcc->m_aContact, pData, sizeof(pAcc->m_aContact)); break;
	case ACC_TIMEOUT_CODE: str_copy(pAcc->m_aTimeoutCode, pData, sizeof(pAcc->m_aTimeoutCode)); break;
	case ACC_SECURITY_PIN: str_copy(pAcc->m_aSecurityPin, pData, sizeof(pAcc->m_aSecurityPin)); break;
	case ACC_REGISTER_DATE: pAcc->m_RegisterDate = (time_t)atoll(pData); break;
	case ACC_LAST_LOGIN_DATE: pAcc->m_LastLoginDate = (time_t)atoll(pData); break;
	case ACC_FLAGS: pAcc->m_Flags = atoi(pData); break;
	case ACC_EMAIL: str_copy(pAcc->m_aEmail, pData, sizeof(pAcc->m_aEmail)); break;
	case ACC_DESIGN: str_copy(pAcc->m_aDesign, pData, sizeof(pAcc->m_aDesign)); break;
	case ACC_PORTAL_BATTERY: pAcc->m_PortalBattery = atoi(pData); break;
	case ACC_PORTAL_BLOCKER: pAcc->m_PortalBlocker = atoi(pData); break;
	case ACC_VOTE_MENU_FLAGS: pAcc->m_VoteMenuFlags = atoi(pData); break;
	case ACC_DURAK_WINS: pAcc->m_DurakWins = atoi(pData); break;
	case ACC_DURAK_PROFIT: pAcc->m_DurakProfit = atoll(pData); break;
	case ACC_LANGUAGE: str_copy(pAcc->m_aLanguage, pData, sizeof(pAcc->m_aLanguage)); break;
	case ACC_QUEST_PROGRESS_0: pAcc->m_aQuestProgress[0] = atoi(pData); break;
	case ACC_QUEST_PROGRESS_1: pAcc->m_aQuestProgress[1] = atoi(pData); break;
	case ACC_QUEST_PROGRESS_2: pAcc->m_aQuestProgress[2] = atoi(pData); break;
	case ACC_QUEST_PROGRESS_3: pAcc->m_aQuestProgress[3] = atoi(pData); break;
	case ACC_QUEST_PROGRESS_4: pAcc->m_aQuestProgress[4] = atoi(pData); break;
	case ACC_QUEST_COMPLETED_0: pAcc->m_aQuestCompleted[0] = atoi(pData) != 0; break;
	case ACC_QUEST_COMPLETED_1: pAcc->m_aQuestCompleted[1] = atoi(pData) != 0; break;
	case ACC_QUEST_COMPLETED_2: pAcc->m_aQuestCompleted[2] = atoi(pData) != 0; break;
	case ACC_QUEST_COMPLETED_3: pAcc->m_aQuestCompleted[3] = atoi(pData) != 0; break;
	case ACC_QUEST_COMPLETED_4: pAcc->m_aQuestCompleted[4] = atoi(pData) != 0; break;
	case ACC_QUEST_DAY: pAcc->m_QuestDay = atoi(pData); break;
	case ACC_TOTAL_FARM_MONEY: pAcc->m_TotalFarmMoney = atoll(pData); break;
	case ACC_REGISTER_PLAYER_NAME: str_copy(pAcc->m_aRegisterPlayerName, pData, sizeof(pAcc->m_aRegisterPlayerName)); break;
	}
}

const char *CAccountSystem::GetVarValue(int AccId, int VariableId) const
{
	static char aBuf[256];
	str_copy(aBuf, "Unknown", sizeof(aBuf));
	const SAccountInfo *pAcc = Get(AccId);
	if(!pAcc)
		return aBuf;

	switch(VariableId)
	{
	case ACC_PORT: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_Port); break;
	case ACC_LOGGED_IN: str_format(aBuf, sizeof(aBuf), "%d", (int)pAcc->m_LoggedIn); break;
	case ACC_DISABLED: str_format(aBuf, sizeof(aBuf), "%d", (int)pAcc->m_Disabled); break;
	case ACC_PASSWORD: sha256_str(pAcc->m_Password, aBuf, sizeof(aBuf)); break;
	case ACC_USERNAME: str_copy(aBuf, pAcc->m_aUsername, sizeof(aBuf)); break;
	case ACC_CLIENT_ID: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_ClientId); break;
	case ACC_LEVEL: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_Level); break;
	case ACC_XP: str_format(aBuf, sizeof(aBuf), "%lld", (long long)pAcc->m_XP); break;
	case ACC_MONEY: str_format(aBuf, sizeof(aBuf), "%lld", (long long)pAcc->m_Money); break;
	case ACC_KILLS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_Kills); break;
	case ACC_DEATHS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_Deaths); break;
	case ACC_POLICE_LEVEL: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_PoliceLevel); break;
	case ACC_SURVIVAL_KILLS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_SurvivalKills); break;
	case ACC_SURVIVAL_WINS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_SurvivalWins); break;
	case ACC_SPOOKY_GHOST: str_format(aBuf, sizeof(aBuf), "%d", (int)pAcc->m_SpookyGhost); break;
	case ACC_LAST_MONEY_TRANSACTION_0: str_copy(aBuf, pAcc->m_aLastMoneyTransaction[0], sizeof(aBuf)); break;
	case ACC_LAST_MONEY_TRANSACTION_1: str_copy(aBuf, pAcc->m_aLastMoneyTransaction[1], sizeof(aBuf)); break;
	case ACC_LAST_MONEY_TRANSACTION_2: str_copy(aBuf, pAcc->m_aLastMoneyTransaction[2], sizeof(aBuf)); break;
	case ACC_LAST_MONEY_TRANSACTION_3: str_copy(aBuf, pAcc->m_aLastMoneyTransaction[3], sizeof(aBuf)); break;
	case ACC_LAST_MONEY_TRANSACTION_4: str_copy(aBuf, pAcc->m_aLastMoneyTransaction[4], sizeof(aBuf)); break;
	case ACC_VIP: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_VIP); break;
	case ACC_BLOCK_POINTS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_BlockPoints); break;
	case ACC_INSTAGIB_KILLS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_InstagibKills); break;
	case ACC_INSTAGIB_WINS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_InstagibWins); break;
	case ACC_SPAWN_WEAPON_0: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aSpawnWeapon[0]); break;
	case ACC_SPAWN_WEAPON_1: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aSpawnWeapon[1]); break;
	case ACC_SPAWN_WEAPON_2: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aSpawnWeapon[2]); break;
	case ACC_NINJAJETPACK: str_format(aBuf, sizeof(aBuf), "%d", (int)pAcc->m_Ninjajetpack); break;
	case ACC_LAST_PLAYER_NAME: str_copy(aBuf, pAcc->m_aLastPlayerName, sizeof(aBuf)); break;
	case ACC_SURVIVAL_DEATHS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_SurvivalDeaths); break;
	case ACC_INSTAGIB_DEATHS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_InstagibDeaths); break;
	case ACC_TASER_LEVEL: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_TaserLevel); break;
	case ACC_KILLING_SPREE_RECORD: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_KillingSpreeRecord); break;
	case ACC_EUROS: str_format(aBuf, sizeof(aBuf), "%.2f", pAcc->m_Euros); break;
	case ACC_EXPIRE_DATE_VIP: str_format(aBuf, sizeof(aBuf), "%lld", (long long)pAcc->m_ExpireDateVIP); break;
	case ACC_PORTAL_RIFLE: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_PortalRifle); break;
	case ACC_EXPIRE_DATE_PORTAL_RIFLE: str_format(aBuf, sizeof(aBuf), "%lld", (long long)pAcc->m_ExpireDatePortalRifle); break;
	case ACC_VERSION: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_Version); break;
	case ACC_ADDR: net_addr_str(&pAcc->m_Addr, aBuf, sizeof(aBuf), true); break;
	case ACC_LAST_ADDR: net_addr_str(&pAcc->m_LastAddr, aBuf, sizeof(aBuf), true); break;
	case ACC_TASER_BATTERY: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_TaserBattery); break;
	case ACC_CONTACT: str_copy(aBuf, pAcc->m_aContact, sizeof(aBuf)); break;
	case ACC_TIMEOUT_CODE: str_copy(aBuf, pAcc->m_aTimeoutCode, sizeof(aBuf)); break;
	case ACC_SECURITY_PIN: str_copy(aBuf, pAcc->m_aSecurityPin, sizeof(aBuf)); break;
	case ACC_REGISTER_DATE: str_format(aBuf, sizeof(aBuf), "%lld", (long long)pAcc->m_RegisterDate); break;
	case ACC_LAST_LOGIN_DATE: str_format(aBuf, sizeof(aBuf), "%lld", (long long)pAcc->m_LastLoginDate); break;
	case ACC_FLAGS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_Flags); break;
	case ACC_EMAIL: str_copy(aBuf, pAcc->m_aEmail, sizeof(aBuf)); break;
	case ACC_DESIGN: str_copy(aBuf, pAcc->m_aDesign, sizeof(aBuf)); break;
	case ACC_PORTAL_BATTERY: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_PortalBattery); break;
	case ACC_PORTAL_BLOCKER: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_PortalBlocker); break;
	case ACC_VOTE_MENU_FLAGS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_VoteMenuFlags); break;
	case ACC_DURAK_WINS: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_DurakWins); break;
	case ACC_DURAK_PROFIT: str_format(aBuf, sizeof(aBuf), "%lld", (long long)pAcc->m_DurakProfit); break;
	case ACC_LANGUAGE: str_copy(aBuf, pAcc->m_aLanguage, sizeof(aBuf)); break;
	case ACC_QUEST_PROGRESS_0: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestProgress[0]); break;
	case ACC_QUEST_PROGRESS_1: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestProgress[1]); break;
	case ACC_QUEST_PROGRESS_2: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestProgress[2]); break;
	case ACC_QUEST_PROGRESS_3: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestProgress[3]); break;
	case ACC_QUEST_PROGRESS_4: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestProgress[4]); break;
	case ACC_QUEST_COMPLETED_0: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestCompleted[0] ? 1 : 0); break;
	case ACC_QUEST_COMPLETED_1: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestCompleted[1] ? 1 : 0); break;
	case ACC_QUEST_COMPLETED_2: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestCompleted[2] ? 1 : 0); break;
	case ACC_QUEST_COMPLETED_3: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestCompleted[3] ? 1 : 0); break;
	case ACC_QUEST_COMPLETED_4: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_aQuestCompleted[4] ? 1 : 0); break;
	case ACC_QUEST_DAY: str_format(aBuf, sizeof(aBuf), "%d", pAcc->m_QuestDay); break;
	case ACC_TOTAL_FARM_MONEY: str_format(aBuf, sizeof(aBuf), "%lld", (long long)pAcc->m_TotalFarmMoney); break;
	case ACC_REGISTER_PLAYER_NAME: str_copy(aBuf, pAcc->m_aRegisterPlayerName, sizeof(aBuf)); break;
	}
	return aBuf;
}

bool CAccountSystem::WriteFile(int AccId) const
{
	const SAccountInfo *pAcc = Get(AccId);
	if(!pAcc || !pAcc->m_aUsername[0])
		return false;

	char aPath[IO_MAX_PATH_LENGTH];
	AccountPath(aPath, sizeof(aPath), pAcc->m_aUsername);

	std::ofstream File(aPath, std::ios::trunc);
	if(!File.is_open())
	{
		log_error("account", "failed to write %s", aPath);
		return false;
	}

	for(int i = 0; i < NUM_ACCOUNT_VARIABLES; i++)
		File << GetVarValue(AccId, i) << "\n";
	return true;
}

bool CAccountSystem::ReadNativeLines(int AccId, std::ifstream &File)
{
	SAccountInfo *pAcc = Get(AccId);
	if(!pAcc)
		return false;

	char aUsernameKeep[ACC_USERNAME_LENGTH];
	str_copy(aUsernameKeep, pAcc->m_aUsername, sizeof(aUsernameKeep));

	std::string Line;
	for(int i = 0; i < NUM_ACCOUNT_VARIABLES; i++)
	{
		if(!std::getline(File, Line))
		{
			log_error("account", "account file truncated at var %d", i);
			str_copy(pAcc->m_aUsername, aUsernameKeep, sizeof(pAcc->m_aUsername));
			return false;
		}
		if(!Line.empty() && Line.back() == '\r')
			Line.pop_back();
		SetVar(AccId, i, Line.c_str());
	}

	if(!pAcc->m_aUsername[0])
		str_copy(pAcc->m_aUsername, aUsernameKeep, sizeof(pAcc->m_aUsername));
	if(pAcc->m_Level < 1)
		pAcc->m_Level = 1;
	return true;
}

bool CAccountSystem::ReadSlimKeyValue(int AccId, std::ifstream &File, const std::string &FirstLine)
{
	(void)FirstLine;
	SAccountInfo *pAcc = Get(AccId);
	if(!pAcc)
		return false;

	std::string Line;
	while(std::getline(File, Line))
	{
		if(!Line.empty() && Line.back() == '\r')
			Line.pop_back();
		if(Line.rfind("password=", 0) == 0)
			sha256_from_str(&pAcc->m_Password, Line.c_str() + 9);
		else if(Line.rfind("vip=", 0) == 0)
			pAcc->m_VIP = atoi(Line.c_str() + 4);
		else if(Line.rfind("expire_vip=", 0) == 0)
			pAcc->m_ExpireDateVIP = atoll(Line.c_str() + 11);
		else if(Line.rfind("disabled=", 0) == 0)
			pAcc->m_Disabled = atoi(Line.c_str() + 9) != 0;
		else if(Line.rfind("money=", 0) == 0)
			pAcc->m_Money = atoll(Line.c_str() + 6);
		else if(Line.rfind("level=", 0) == 0)
			pAcc->m_Level = atoi(Line.c_str() + 6);
		else if(Line.rfind("xp=", 0) == 0)
			pAcc->m_XP = atoll(Line.c_str() + 3);
		else if(Line.rfind("police=", 0) == 0)
			pAcc->m_PoliceLevel = atoi(Line.c_str() + 7);
		else if(Line.rfind("taser_level=", 0) == 0)
			pAcc->m_TaserLevel = atoi(Line.c_str() + 12);
		else if(Line.rfind("taser_battery=", 0) == 0)
			pAcc->m_TaserBattery = atoi(Line.c_str() + 14);
		else if(Line.rfind("portal_rifle=", 0) == 0)
			pAcc->m_PortalRifle = atoi(Line.c_str() + 13);
		else if(Line.rfind("expire_portal_rifle=", 0) == 0)
			pAcc->m_ExpireDatePortalRifle = atoll(Line.c_str() + 20);
		else if(Line.rfind("portal_battery=", 0) == 0)
			pAcc->m_PortalBattery = atoi(Line.c_str() + 15);
		else if(Line.rfind("portal_blocker=", 0) == 0)
			pAcc->m_PortalBlocker = atoi(Line.c_str() + 15);
		else if(Line.rfind("ninjajetpack=", 0) == 0)
			pAcc->m_Ninjajetpack = atoi(Line.c_str() + 13) != 0;
		else if(Line.rfind("spooky_ghost=", 0) == 0)
			pAcc->m_SpookyGhost = atoi(Line.c_str() + 13) != 0;
		else if(Line.rfind("spawn_weapon0=", 0) == 0)
			pAcc->m_aSpawnWeapon[0] = atoi(Line.c_str() + 14);
		else if(Line.rfind("spawn_weapon1=", 0) == 0)
			pAcc->m_aSpawnWeapon[1] = atoi(Line.c_str() + 14);
		else if(Line.rfind("spawn_weapon2=", 0) == 0)
			pAcc->m_aSpawnWeapon[2] = atoi(Line.c_str() + 14);
		else if(Line.rfind("contact=", 0) == 0)
		{
			const char *pVal = Line.c_str() + 8;
			if(str_comp(pVal, "-") != 0)
				str_copy(pAcc->m_aContact, pVal, sizeof(pAcc->m_aContact));
		}
		else if(Line.rfind("last_name=", 0) == 0)
		{
			const char *pVal = Line.c_str() + 10;
			if(str_comp(pVal, "-") != 0)
				str_copy(pAcc->m_aLastPlayerName, pVal, sizeof(pAcc->m_aLastPlayerName));
		}
		else if(Line.rfind("register_date=", 0) == 0)
			pAcc->m_RegisterDate = (time_t)atoll(Line.c_str() + 14);
		else if(Line.rfind("last_login=", 0) == 0)
			pAcc->m_LastLoginDate = (time_t)atoll(Line.c_str() + 11);
	}
	if(pAcc->m_Level < 1)
		pAcc->m_Level = 1;
	pAcc->m_Version = ACC_CURRENT_VERSION;
	pAcc->m_LoggedIn = false;
	pAcc->m_ClientId = -1;
	return true;
}

bool CAccountSystem::ReadFile(int AccId)
{
	SAccountInfo *pAcc = Get(AccId);
	if(!pAcc || !pAcc->m_aUsername[0])
		return false;

	char aPath[IO_MAX_PATH_LENGTH];
	AccountPath(aPath, sizeof(aPath), pAcc->m_aUsername);

	std::ifstream File(aPath);
	if(!File.is_open())
		return false;

	std::string FirstLine;
	if(!std::getline(File, FirstLine))
		return false;
	if(!FirstLine.empty() && FirstLine.back() == '\r')
		FirstLine.pop_back();

	if(FirstLine.rfind("FDDRACE_SLIM_ACC", 0) == 0)
	{
		log_info("account", "migrating slim account %s -> native F-DDrace format", aPath);
		if(!ReadSlimKeyValue(AccId, File, FirstLine))
			return false;
		return WriteFile(AccId);
	}

	// Native: first line is ACC_PORT. Rewind and read all NUM_ACCOUNT_VARIABLES lines.
	File.clear();
	File.seekg(0);
	return ReadNativeLines(AccId, File);
}

bool CAccountSystem::Register(int ClientId, const char *pUsername, const char *pPassword)
{
	if(!m_pGameServer || !g_Config.m_SvAccounts)
	{
		m_pGameServer->SendChatTarget(ClientId, "Accounts are disabled");
		return false;
	}
	if(!pUsername || !pUsername[0] || !pPassword || !pPassword[0])
	{
		m_pGameServer->SendChatTarget(ClientId, "Usage: /register <name> <password> <password>");
		return false;
	}
	if(str_length(pUsername) >= ACC_USERNAME_LENGTH)
	{
		m_pGameServer->SendChatTarget(ClientId, "Username too long");
		return false;
	}
	if(FindByClientId(ClientId) >= ACC_START)
	{
		m_pGameServer->SendChatTarget(ClientId, "Already logged in");
		return false;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	AccountPath(aPath, sizeof(aPath), pUsername);
	if(fs_is_file(aPath))
	{
		m_pGameServer->SendChatTarget(ClientId, "Username already exists");
		return false;
	}

	const int AccId = AllocSlot();
	SAccountInfo *pAcc = Get(AccId);
	*pAcc = SAccountInfo{};
	str_copy(pAcc->m_aUsername, pUsername, sizeof(pAcc->m_aUsername));
	pAcc->m_Password = HashPassword(pPassword);
	pAcc->m_VIP = VIP_NONE;
	pAcc->m_Disabled = false;
	pAcc->m_LoggedIn = false;
	pAcc->m_ClientId = -1;
	pAcc->m_Level = 1;
	pAcc->m_Version = ACC_CURRENT_VERSION;
	pAcc->m_RegisterDate = time(nullptr);
	pAcc->m_LastLoginDate = 0;
	str_copy(pAcc->m_aLastPlayerName, m_pGameServer->Server()->ClientName(ClientId), sizeof(pAcc->m_aLastPlayerName));
	str_copy(pAcc->m_aRegisterPlayerName, m_pGameServer->Server()->ClientName(ClientId), sizeof(pAcc->m_aRegisterPlayerName));
	EnsureDir();
	if(!WriteFile(AccId))
	{
		pAcc->m_aUsername[0] = '\0';
		m_pGameServer->SendChatTarget(ClientId, "Failed to create account");
		return false;
	}

	// Keep slot free for later login (file is written, not session-active).
	pAcc->m_aUsername[0] = '\0';
	m_pGameServer->SendChatTarget(ClientId, "Account created. Use /login <name> <password>");
	log_info("account", "registered '%s'", pUsername);
	return true;
}

bool CAccountSystem::Login(int ClientId, const char *pUsername, const char *pPassword)
{
	if(!m_pGameServer || !g_Config.m_SvAccounts)
	{
		m_pGameServer->SendChatTarget(ClientId, "Accounts are disabled");
		return false;
	}
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;
	if(FindByClientId(ClientId) >= ACC_START)
	{
		m_pGameServer->SendChatTarget(ClientId, "Already logged in");
		return false;
	}
	if(!pUsername || !pUsername[0] || !pPassword || !pPassword[0])
	{
		m_pGameServer->SendChatTarget(ClientId, "Usage: /login <name> <password>");
		return false;
	}

	char aPath[IO_MAX_PATH_LENGTH];
	AccountPath(aPath, sizeof(aPath), pUsername);
	if(!fs_is_file(aPath))
	{
		m_pGameServer->SendChatTarget(ClientId, "Account not found");
		return false;
	}

	int AccId = FindByUsername(pUsername);
	if(AccId >= ACC_START && m_vAccounts[AccId].m_LoggedIn)
	{
		m_pGameServer->SendChatTarget(ClientId, "Account already logged in");
		return false;
	}
	if(AccId < ACC_START)
	{
		AccId = AllocSlot();
		str_copy(m_vAccounts[AccId].m_aUsername, pUsername, sizeof(m_vAccounts[AccId].m_aUsername));
	}

	if(!ReadFile(AccId))
	{
		m_pGameServer->SendChatTarget(ClientId, "Failed to read account");
		m_vAccounts[AccId].m_aUsername[0] = '\0';
		return false;
	}

	SAccountInfo *pAcc = Get(AccId);
	if(pAcc->m_LoggedIn)
	{
		// Stale flag after crash: allow login if this process does not hold the session.
		bool HeldInMemory = false;
		for(int i = ACC_START; i < Num(); i++)
		{
			if(i == AccId)
				continue;
			if(m_vAccounts[i].m_LoggedIn && str_comp_nocase(m_vAccounts[i].m_aUsername, pUsername) == 0)
			{
				HeldInMemory = true;
				break;
			}
		}
		if(HeldInMemory)
		{
			m_pGameServer->SendChatTarget(ClientId, "Account already logged in");
			m_vAccounts[AccId].m_aUsername[0] = '\0';
			return false;
		}
		pAcc->m_LoggedIn = false;
		pAcc->m_ClientId = -1;
	}
	if(pAcc->m_Disabled)
	{
		m_pGameServer->SendChatTarget(ClientId, "Account is disabled");
		m_vAccounts[AccId].m_aUsername[0] = '\0';
		return false;
	}
	if(sha256_comp(pAcc->m_Password, HashPassword(pPassword)) != 0)
	{
		m_pGameServer->SendChatTarget(ClientId, "Wrong password");
		m_vAccounts[AccId].m_aUsername[0] = '\0';
		return false;
	}

	pAcc->m_Port = g_Config.m_SvPort;
	pAcc->m_LoggedIn = true;
	pAcc->m_ClientId = ClientId;
	pAcc->m_Version = ACC_CURRENT_VERSION;
	pAcc->m_LastLoginDate = time(nullptr);
	str_copy(pAcc->m_aLastPlayerName, m_pGameServer->Server()->ClientName(ClientId), sizeof(pAcc->m_aLastPlayerName));
	if(pPlayer->m_aTimeoutCode[0] != '\0')
		str_copy(pAcc->m_aTimeoutCode, pPlayer->m_aTimeoutCode, sizeof(pAcc->m_aTimeoutCode));

	const NETADDR *pAddr = m_pGameServer->Server()->ClientAddr(ClientId);
	if(pAddr)
	{
		if(net_addr_comp_noport(pAddr, &pAcc->m_Addr) != 0)
			pAcc->m_LastAddr = pAcc->m_Addr;
		pAcc->m_Addr = *pAddr;
	}

	pPlayer->SetWalletMoney(pAcc->m_Money);
	WriteFile(AccId);

	SyncMoveRestrictions(pPlayer);
	ExpireTimedItems(ClientId);
	ApplyAccountLoadouts(pPlayer);
	m_pGameServer->m_VotingMenu.ApplyFlags(ClientId, pAcc->m_VoteMenuFlags);
	m_pGameServer->m_VotingMenu.SendPageVotes(ClientId, false);
	m_pGameServer->SendChatTarget(ClientId, "Successfully logged in");
	log_info("account", "login '%s' -> client %d (vip=%d money=%lld)", pUsername, ClientId, pAcc->m_VIP, (long long)pAcc->m_Money);
	return true;
}

void CAccountSystem::Logout(int AccId, bool Silent)
{
	SAccountInfo *pAcc = Get(AccId);
	if(!pAcc || AccId < ACC_START || !pAcc->m_LoggedIn)
		return;

	const int ClientId = pAcc->m_ClientId;
	if(ClientId >= 0 && ClientId < MAX_CLIENTS && m_pGameServer->m_apPlayers[ClientId])
	{
		pAcc->m_Money = m_pGameServer->m_apPlayers[ClientId]->GetWalletMoney();
		str_copy(pAcc->m_aLastPlayerName, m_pGameServer->Server()->ClientName(ClientId), sizeof(pAcc->m_aLastPlayerName));
		pAcc->m_VoteMenuFlags = m_pGameServer->m_VotingMenu.GetFlags(ClientId);
	}

	pAcc->m_LoggedIn = false;
	pAcc->m_ClientId = -1;
	WriteFile(AccId);

	if(ClientId >= 0 && ClientId < MAX_CLIENTS && m_pGameServer->m_apPlayers[ClientId])
	{
		CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
		if(CCharacter *pChr = pPlayer->GetCharacter())
			pChr->Core()->m_MoveRestrictionExtra.m_VipPlus = false;
		m_pGameServer->m_VotingMenu.SendPageVotes(ClientId, false);
		if(!Silent)
			m_pGameServer->SendChatTarget(ClientId, "Logged out");
	}
}

void CAccountSystem::LogoutClient(int ClientId, bool Silent)
{
	const int AccId = FindByClientId(ClientId);
	if(AccId >= ACC_START)
		Logout(AccId, Silent);
}

void CAccountSystem::SetVip(int AccId, int VipLevel, int64_t ExpireDate)
{
	SAccountInfo *pAcc = Get(AccId);
	if(!pAcc || AccId < ACC_START)
		return;
	pAcc->m_VIP = VipLevel;
	pAcc->m_ExpireDateVIP = ExpireDate;
	WriteFile(AccId);
	if(pAcc->m_LoggedIn && pAcc->m_ClientId >= 0 && m_pGameServer->m_apPlayers[pAcc->m_ClientId])
		SyncMoveRestrictions(m_pGameServer->m_apPlayers[pAcc->m_ClientId]);
}

void CAccountSystem::SyncMoveRestrictions(CPlayer *pPlayer) const
{
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	const int AccId = FindByClientId(pPlayer->GetCid());
	const SAccountInfo *pAcc = Get(AccId);
	const bool VipPlus = pAcc && (pAcc->m_VIP == VIP_PLUS || pAcc->m_VIP == VIP_ADMIN);
	pChr->Core()->m_MoveRestrictionExtra.m_VipPlus = VipPlus;
	pChr->Core()->m_MoveRestrictionExtra.m_RoomKey = pPlayer->m_HasRoomKey;
}

void CAccountSystem::SyncWalletFromPlayer(int AccId)
{
	SAccountInfo *pAcc = Get(AccId);
	if(!pAcc || AccId < ACC_START || !pAcc->m_LoggedIn)
		return;
	const int ClientId = pAcc->m_ClientId;
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !m_pGameServer->m_apPlayers[ClientId])
		return;
	pAcc->m_Money = m_pGameServer->m_apPlayers[ClientId]->GetWalletMoney();
}

bool CAccountSystem::Persist(int AccId)
{
	SyncWalletFromPlayer(AccId);
	return WriteFile(AccId);
}

bool CAccountSystem::BuyRoomKey(int ClientId)
{
	if(!m_pGameServer || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	if(!pPlayer)
		return false;

	if(pPlayer->m_HasRoomKey)
	{
		m_pGameServer->SendChatTarget(ClientId, "You already have a room key");
		return false;
	}

	const int Price = g_Config.m_SvRoomKeyPrice;
	if(Price > 0 && pPlayer->GetWalletMoney() < Price)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Need %d money for a room key (you have %lld)", Price, (long long)pPlayer->GetWalletMoney());
		m_pGameServer->SendChatTarget(ClientId, aBuf);
		return false;
	}

	if(Price > 0)
		pPlayer->WalletTransaction(-Price);

	pPlayer->m_HasRoomKey = true;
	SyncMoveRestrictions(pPlayer);

	const int AccId = FindByClientId(ClientId);
	if(AccId >= ACC_START)
		Persist(AccId);

	char aBuf[128];
	if(Price > 0)
		str_format(aBuf, sizeof(aBuf), "Bought room key for %d money", Price);
	else
		str_copy(aBuf, "Room key granted", sizeof(aBuf));
	m_pGameServer->SendChatTarget(ClientId, aBuf);
	return true;
}

void CAccountSystem::ApplyAccountLoadouts(CPlayer *pPlayer) const
{
	if(!pPlayer)
		return;
	CCharacter *pChr = pPlayer->GetCharacter();
	if(!pChr)
		return;

	const int AccId = FindByClientId(pPlayer->GetCid());
	const SAccountInfo *pAcc = Get(AccId);
	if(!pAcc)
		return;

	if(pAcc->m_Ninjajetpack)
		pChr->Core()->m_Jetpack = true;

	// Spawn weapons: 0=shotgun, 1=grenade, 2=laser — give if account unlock set.
	static const int s_aWeapons[3] = {WEAPON_SHOTGUN, WEAPON_GRENADE, WEAPON_LASER};
	for(int i = 0; i < 3; i++)
	{
		if(pAcc->m_aSpawnWeapon[i] <= 0)
			continue;
		pChr->GiveWeapon(s_aWeapons[i], false);
	}

	if(pAcc->m_TaserLevel > 0 || pAcc->m_TaserBattery > 0)
		pChr->GiveWeapon(WEAPON_TASER, false);
	if(pAcc->m_PortalRifle > 0 || pAcc->m_PortalBattery > 0)
		pChr->GiveWeapon(WEAPON_PORTAL_RIFLE, false);
}

void CAccountSystem::ExpireTimedItems(int ClientId)
{
	if(!m_pGameServer || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	const int AccId = FindByClientId(ClientId);
	SAccountInfo *pAcc = Get(AccId);
	if(!pAcc || AccId < ACC_START)
		return;

	const time_t Now = time(nullptr);
	bool Changed = false;

	if(pAcc->m_VIP > VIP_NONE && pAcc->m_VIP != VIP_ADMIN && pAcc->m_ExpireDateVIP > 0 && Now >= (time_t)pAcc->m_ExpireDateVIP)
	{
		pAcc->m_VIP = VIP_NONE;
		pAcc->m_ExpireDateVIP = 0;
		Changed = true;
		m_pGameServer->SendChatTarget(ClientId, "[WARNING] Your VIP expired");
		if(CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId])
			SyncMoveRestrictions(pPlayer);
	}

	if(pAcc->m_PortalRifle > 0 && pAcc->m_ExpireDatePortalRifle > 0 && Now >= (time_t)pAcc->m_ExpireDatePortalRifle)
	{
		pAcc->m_PortalRifle = 0;
		pAcc->m_ExpireDatePortalRifle = 0;
		Changed = true;
		m_pGameServer->SendChatTarget(ClientId, "[WARNING] Your portal rifle expired");
		if(CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientId))
			pChr->GiveWeapon(WEAPON_PORTAL_RIFLE, true);
	}

	if(Changed)
		Persist(AccId);
}

void CAccountSystem::JailPlayer(int ClientId, int Seconds)
{
	if(!m_pGameServer || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	if(!pPlayer)
		return;
	if(Seconds < 0)
		Seconds = 0;
	if(pPlayer->m_JailArresting || pPlayer->m_JailReleasing)
		return;

	pPlayer->m_JailTime = (int64_t)m_pGameServer->Server()->TickSpeed() * Seconds;

	CCharacter *pChr = pPlayer->GetCharacter();
	if(pChr)
	{
		vec2 HolePos = pChr->GetPos();
		if(!CJailArrest::FindHolePos(m_pGameServer, pChr->GetPos(), &HolePos))
			HolePos = pChr->GetPos() + vec2(0.f, -128.f);
		new CJailArrest(&m_pGameServer->m_World, HolePos, ClientId);
	}
	else
		pPlayer->KillCharacter(WEAPON_GAME);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' arrested for %d seconds", m_pGameServer->Server()->ClientName(ClientId), Seconds);
	m_pGameServer->SendChat(-1, TEAM_ALL, aBuf);
}

void CAccountSystem::ReleaseJail(int ClientId)
{
	if(!m_pGameServer || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	if(!pPlayer || pPlayer->m_JailTime <= 0)
		return;
	if(pPlayer->m_JailArresting || pPlayer->m_JailReleasing)
		return;

	CCharacter *pChr = pPlayer->GetCharacter();
	if(pChr)
	{
		vec2 PortalPos = pChr->GetPos();
		CJailRelease::FindPortalPos(m_pGameServer, pChr->GetPos(), &PortalPos);
		new CJailRelease(&m_pGameServer->m_World, PortalPos, ClientId);
	}
	else
	{
		pPlayer->m_JailTime = 1;
		pPlayer->KillCharacter(WEAPON_GAME);
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "'%s' released from jail", m_pGameServer->Server()->ClientName(ClientId));
		m_pGameServer->SendChat(-1, TEAM_ALL, aBuf);
	}

	m_pGameServer->m_FddraceSave.RemoveJailSave(ClientId);
}

bool CAccountSystem::IsJailed(int ClientId) const
{
	if(!m_pGameServer || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	return pPlayer && pPlayer->m_JailTime > 0;
}

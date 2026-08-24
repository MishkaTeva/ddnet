#include "account.h"

#include <base/fs.h>
#include <base/hash.h>
#include <base/log.h>

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <cstdio>
#include <fstream>
#include <string>

static constexpr int SLIM_ACC_VERSION = 1;

void CAccountSystem::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_vAccounts.clear();
	m_vAccounts.emplace_back(); // index 0 = not logged in
	EnsureDir();
	log_info("account", "slim account system ready (path=%s)", g_Config.m_SvAccFilePath);
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
	fs_makedir_rec_for(g_Config.m_SvAccFilePath);
	fs_makedir(g_Config.m_SvAccFilePath);
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

bool CAccountSystem::WriteFile(int AccId) const
{
	const SAccountInfo *pAcc = Get(AccId);
	if(!pAcc || !pAcc->m_aUsername[0])
		return false;

	char aPath[IO_MAX_PATH_LENGTH];
	AccountPath(aPath, sizeof(aPath), pAcc->m_aUsername);

	char aHash[SHA256_MAXSTRSIZE];
	sha256_str(pAcc->m_Password, aHash, sizeof(aHash));

	std::ofstream File(aPath, std::ios::trunc);
	if(!File.is_open())
	{
		log_error("account", "failed to write %s", aPath);
		return false;
	}

	File << "FDDRACE_SLIM_ACC " << SLIM_ACC_VERSION << "\n";
	File << "password=" << aHash << "\n";
	File << "vip=" << pAcc->m_VIP << "\n";
	File << "expire_vip=" << pAcc->m_ExpireDateVIP << "\n";
	File << "disabled=" << (pAcc->m_Disabled ? 1 : 0) << "\n";
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

	std::string Line;
	if(!std::getline(File, Line) || Line.rfind("FDDRACE_SLIM_ACC", 0) != 0)
	{
		log_error("account", "unsupported account file %s (need slim format)", aPath);
		return false;
	}

	while(std::getline(File, Line))
	{
		if(Line.rfind("password=", 0) == 0)
			sha256_from_str(&pAcc->m_Password, Line.c_str() + 9);
		else if(Line.rfind("vip=", 0) == 0)
			pAcc->m_VIP = atoi(Line.c_str() + 4);
		else if(Line.rfind("expire_vip=", 0) == 0)
			pAcc->m_ExpireDateVIP = atoll(Line.c_str() + 11);
		else if(Line.rfind("disabled=", 0) == 0)
			pAcc->m_Disabled = atoi(Line.c_str() + 9) != 0;
	}
	return true;
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
	str_copy(pAcc->m_aUsername, pUsername);
	pAcc->m_Password = HashPassword(pPassword);
	pAcc->m_VIP = VIP_NONE;
	pAcc->m_Disabled = false;
	EnsureDir();
	if(!WriteFile(AccId))
	{
		pAcc->m_aUsername[0] = '\0';
		m_pGameServer->SendChatTarget(ClientId, "Failed to create account");
		return false;
	}

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

	// Already loaded in memory and logged in elsewhere?
	int AccId = FindByUsername(pUsername);
	if(AccId >= ACC_START && m_vAccounts[AccId].m_LoggedIn)
	{
		m_pGameServer->SendChatTarget(ClientId, "Account already logged in");
		return false;
	}
	if(AccId < ACC_START)
	{
		AccId = AllocSlot();
		str_copy(m_vAccounts[AccId].m_aUsername, pUsername);
	}

	if(!ReadFile(AccId))
	{
		m_pGameServer->SendChatTarget(ClientId, "Failed to read account");
		m_vAccounts[AccId].m_aUsername[0] = '\0';
		return false;
	}

	SAccountInfo *pAcc = Get(AccId);
	if(pAcc->m_Disabled)
	{
		m_pGameServer->SendChatTarget(ClientId, "Account is disabled");
		return false;
	}
	if(sha256_comp(pAcc->m_Password, HashPassword(pPassword)) != 0)
	{
		m_pGameServer->SendChatTarget(ClientId, "Wrong password");
		return false;
	}

	pAcc->m_LoggedIn = true;
	pAcc->m_ClientId = ClientId;
	WriteFile(AccId);

	SyncMoveRestrictions(pPlayer);
	m_pGameServer->SendChatTarget(ClientId, "Successfully logged in");
	log_info("account", "login '%s' -> client %d (vip=%d)", pUsername, ClientId, pAcc->m_VIP);
	return true;
}

void CAccountSystem::Logout(int AccId, bool Silent)
{
	SAccountInfo *pAcc = Get(AccId);
	if(!pAcc || AccId < ACC_START || !pAcc->m_LoggedIn)
		return;

	const int ClientId = pAcc->m_ClientId;
	pAcc->m_LoggedIn = false;
	pAcc->m_ClientId = -1;
	WriteFile(AccId);

	if(ClientId >= 0 && ClientId < MAX_CLIENTS && m_pGameServer->m_apPlayers[ClientId])
	{
		CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
		if(CCharacter *pChr = pPlayer->GetCharacter())
			pChr->Core()->m_MoveRestrictionExtra.m_VipPlus = false;
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

void CAccountSystem::JailPlayer(int ClientId, int Seconds)
{
	if(!m_pGameServer || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	if(!pPlayer)
		return;
	if(Seconds < 0)
		Seconds = 0;

	pPlayer->m_JailTime = (int64_t)m_pGameServer->Server()->TickSpeed() * Seconds;
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

	pPlayer->m_JailTime = 0;
	pPlayer->KillCharacter(WEAPON_GAME);
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' released from jail", m_pGameServer->Server()->ClientName(ClientId));
	m_pGameServer->SendChat(-1, TEAM_ALL, aBuf);
}

bool CAccountSystem::IsJailed(int ClientId) const
{
	if(!m_pGameServer || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	return pPlayer && pPlayer->m_JailTime > 0;
}

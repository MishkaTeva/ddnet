#include "fddrace_save.h"

#include "account.h"
#include "entities/flag.h"

#include <base/fs.h>
#include <base/hash.h>
#include <base/io.h>
#include <base/log.h>
#include <base/net.h>

#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/server/save.h>

#include <cstdio>

static bool SavedAddrExtractV4(const NETADDR *pAddr, unsigned char aOut[4])
{
	if(!pAddr)
		return false;
	if(pAddr->type == NETTYPE_IPV4)
	{
		mem_copy(aOut, pAddr->ip, 4);
		return true;
	}
	if(pAddr->type == NETTYPE_IPV6
		&& pAddr->ip[0] == 0 && pAddr->ip[1] == 0 && pAddr->ip[2] == 0 && pAddr->ip[3] == 0
		&& pAddr->ip[4] == 0 && pAddr->ip[5] == 0 && pAddr->ip[6] == 0 && pAddr->ip[7] == 0
		&& pAddr->ip[8] == 0 && pAddr->ip[9] == 0 && pAddr->ip[10] == 0xff && pAddr->ip[11] == 0xff)
	{
		mem_copy(aOut, pAddr->ip + 12, 4);
		return true;
	}
	return false;
}

static void SavedAddrNormalize(NETADDR *pAddr)
{
	if(!pAddr)
		return;
	unsigned char a4[4];
	if(!SavedAddrExtractV4(pAddr, a4))
		return;
	const unsigned short Port = pAddr->port;
	mem_zero(pAddr, sizeof(*pAddr));
	pAddr->type = NETTYPE_IPV4;
	mem_copy(pAddr->ip, a4, 4);
	pAddr->port = Port;
}

static bool SavedAddrIsLoopback(const NETADDR *pAddr)
{
	if(!pAddr)
		return false;
	unsigned char a4[4];
	if(!SavedAddrExtractV4(pAddr, a4))
		return false;
	return a4[0] == 127 && a4[1] == 0 && a4[2] == 0 && a4[3] == 1;
}

static bool SavedAddrMatch(const NETADDR *pA, const NETADDR *pB, bool CheckPort)
{
	if(!pA || !pB)
		return false;

	NETADDR A = *pA;
	NETADDR B = *pB;
	SavedAddrNormalize(&A);
	SavedAddrNormalize(&B);

	if(CheckPort)
	{
		if(net_addr_comp(&A, &B) == 0)
			return true;
	}
	else if(net_addr_comp_noport(&A, &B) == 0)
	{
		return true;
	}
	if(SavedAddrIsLoopback(&A) && SavedAddrIsLoopback(&B))
		return !CheckPort || A.port == B.port;
	return false;
}

static bool NamesMatch(const char *pSaved, const char *pIncoming)
{
	if(!pSaved || !pIncoming || pSaved[0] == '\0' || pIncoming[0] == '\0')
		return false;
	if(str_comp(pSaved, pIncoming) == 0)
		return true;

	const int IncomingLen = str_length(pIncoming);
	const int SavedLen = str_length(pSaved);
	const int MinMatchLen = 3;
	if(IncomingLen < MinMatchLen || SavedLen < MinMatchLen)
		return false;

	const int CompareLen = IncomingLen < SavedLen ? IncomingLen : SavedLen;
	return str_comp_num(pSaved, pIncoming, CompareLen) == 0;
}

static void SetExpireDate(time_t *pDate, float Hours)
{
	time_t Now;
	time(&Now);
	struct tm ExpireDate = *localtime(&Now);

	if(*pDate != 0)
	{
		struct tm AccDate = *localtime(pDate);
		ExpireDate.tm_year = AccDate.tm_year;
		ExpireDate.tm_mon = AccDate.tm_mon;
		ExpireDate.tm_mday = AccDate.tm_mday;
		ExpireDate.tm_hour = AccDate.tm_hour;
	}

	const time_t OneHour = 60 * 60;
	time_t DateSeconds = mktime(&ExpireDate) + (time_t)(Hours * OneHour);
	ExpireDate = *localtime(&DateSeconds);
	ExpireDate.tm_min = 0;
	ExpireDate.tm_sec = 0;
	*pDate = mktime(&ExpireDate);
}

static bool IsExpired(time_t Date)
{
	if(!Date)
		return false;

	struct tm AccDate = *localtime(&Date);
	time_t Now;
	time(&Now);
	struct tm ExpireDate = *localtime(&Now);
	ExpireDate.tm_year = AccDate.tm_year;
	ExpireDate.tm_mon = AccDate.tm_mon;
	ExpireDate.tm_mday = AccDate.tm_mday;
	ExpireDate.tm_hour = AccDate.tm_hour;
	ExpireDate.tm_min = AccDate.tm_min;
	ExpireDate.tm_sec = AccDate.tm_sec;

	const double Seconds = difftime(Now, mktime(&ExpireDate));
	return Seconds / 60.0 >= 0.0;
}

void CFddraceSave::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_vSavedIdentities.clear();
	m_vSavedIdentitiesFiles.clear();
	m_LastPeriodicSaveTick = m_pGameServer->Server()->Tick();
}

static const char *CurrentMapName(CGameContext *pGameServer)
{
	(void)pGameServer;
	return g_Config.m_SvMap;
}

static bool GetClientAddr(CGameContext *pGameServer, int ClientId, NETADDR *pOut)
{
	const NETADDR *pAddr = pGameServer->Server()->ClientAddr(ClientId);
	if(!pAddr)
		return false;
	*pOut = *pAddr;
	return true;
}

void CFddraceSave::EnsureSaveDirs() const
{
	// TYPE_SAVE resolves to %APPDATA%/DDNet — create nested dirs there, not in CWD.
	char aRel[IO_MAX_PATH_LENGTH];
	char aFull[IO_MAX_PATH_LENGTH];
	str_format(aRel, sizeof(aRel), "dumps/%s/%s/.keep",
		g_Config.m_SvSavedTeesFilePath, CurrentMapName(m_pGameServer));
	m_pGameServer->Storage()->GetCompletePath(IStorage::TYPE_SAVE, aRel, aFull, sizeof(aFull));
	(void)fs_makedir_rec_for(aFull);

	str_format(aRel, sizeof(aRel), "dumps/%s/x_redirect_tile/.keep", g_Config.m_SvSavedTeesFilePath);
	m_pGameServer->Storage()->GetCompletePath(IStorage::TYPE_SAVE, aRel, aFull, sizeof(aFull));
	(void)fs_makedir_rec_for(aFull);
}

void CFddraceSave::FormatIdentitySavePath(char *pBuf, int BufSize, const SSavedIdentity &Identity) const
{
	const char *pStem = SavedIdentityFileStem(Identity, this);
	if(Identity.m_IsRedirectSave)
	{
		str_format(pBuf, BufSize, "dumps/%s/x_redirect_tile/%s.save",
			g_Config.m_SvSavedTeesFilePath, pStem);
	}
	else
	{
		str_format(pBuf, BufSize, "dumps/%s/%s/%s.save",
			g_Config.m_SvSavedTeesFilePath,
			CurrentMapName(m_pGameServer),
			pStem);
	}
}

const char *CFddraceSave::GetSavedIdentityHash(const SSavedIdentity &Info) const
{
	char aSavedAddress[NETADDR_MAXSTRSIZE];
	NETADDR Addr = Info.m_Addr;
	SavedAddrNormalize(&Addr);
	net_addr_str(&Addr, aSavedAddress, sizeof(aSavedAddress), true);

	char aInput[512];
	str_format(aInput, sizeof(aInput), "%s|%s|%s|%s|%c|%d",
		Info.m_aAccUsername, aSavedAddress, Info.m_aTimeoutCode, Info.m_aName,
		Info.m_IsMain ? 'M' : 'D',
		Info.m_RedirectTilePort);

	static char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(sha256(aInput, str_length(aInput)), aSha256, sizeof(aSha256));
	return aSha256;
}

const char *CFddraceSave::SavedIdentityFileStem(const SSavedIdentity &Info, const CFddraceSave *pSave)
{
	if(Info.m_aFileHash[0])
		return Info.m_aFileHash;
	return pSave->GetSavedIdentityHash(Info);
}

bool CFddraceSave::IsLoggedIn(int ClientId) const
{
	const int AccId = m_pGameServer->Accounts()->FindByClientId(ClientId);
	if(AccId < ACC_START)
		return false;
	const SAccountInfo *pAcc = m_pGameServer->Accounts()->Get(AccId);
	return pAcc && pAcc->m_LoggedIn;
}

const char *CFddraceSave::AccUsername(int ClientId) const
{
	const int AccId = m_pGameServer->Accounts()->FindByClientId(ClientId);
	if(AccId < ACC_START)
		return "";
	const SAccountInfo *pAcc = m_pGameServer->Accounts()->Get(AccId);
	return (pAcc && pAcc->m_LoggedIn) ? pAcc->m_aUsername : "";
}

bool CFddraceSave::WriteSaveFile(const char *pPath, int Flags, const SSavedIdentity &Identity, CCharacter *pChr)
{
	EnsureSaveDirs();

	CSaveTee SaveTee;
	SaveTee.Save(pChr, false);

	IOHANDLE File = m_pGameServer->Storage()->OpenFile(pPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		char aFull[IO_MAX_PATH_LENGTH];
		m_pGameServer->Storage()->GetCompletePath(IStorage::TYPE_SAVE, pPath, aFull, sizeof(aFull));
		dbg_msg("fddrace_save", "failed to open for write: %s", aFull);
		return false;
	}

	char aAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&Identity.m_Addr, aAddr, sizeof(aAddr), true);

	char aHeader[512];
	str_format(aHeader, sizeof(aHeader), "fddrace_v1\t%d\t%s\t%s\t%s\t%s\t%lld\t%d\t%d\t%lld\t%lld\t%d\t%d",
		Flags,
		Identity.m_aAccUsername[0] ? Identity.m_aAccUsername : "$",
		aAddr,
		Identity.m_aTimeoutCode[0] ? Identity.m_aTimeoutCode : "$",
		Identity.m_aName,
		(long long)Identity.m_ExpireDate,
		Identity.m_IsPeriodic ? 1 : 0,
		Identity.m_IsJailSave ? 1 : 0,
		(long long)Identity.m_JailSeconds,
		(long long)Identity.m_WalletMoney,
		Identity.m_RedirectTilePort,
		Identity.m_CarriedFlag);

	const char *pTeeString = SaveTee.GetString(nullptr);
	io_write(File, aHeader, str_length(aHeader));
	io_write_newline(File);
	io_write(File, pTeeString, str_length(pTeeString));
	io_write_newline(File);
	io_close(File);
	return true;
}

bool CFddraceSave::ReadSaveFile(const char *pPath, SSavedIdentity &Identity, int *pFlags, CSaveTee *pSaveTee) const
{
	CLineReader LineReader;
	if(!LineReader.OpenFile(m_pGameServer->Storage()->OpenFile(pPath, IOFLAG_READ, IStorage::TYPE_SAVE)))
		return false;

	const char *pHeader = LineReader.Get();
	const char *pTeeLine = LineReader.Get();
	if(!pHeader || !pTeeLine || strncmp(pHeader, "fddrace_v1\t", 11) != 0)
		return false;

	Identity = {};
	char aAddr[NETADDR_MAXSTRSIZE];
	char aAcc[32];
	char aTimeout[64];
	int Periodic = 0;
	int Jail = 0;
	long long ExpireDate = 0;
	long long JailSeconds = 0;
	long long WalletMoney = 0;
	int RedirectPort = 0;
	int CarriedFlag = -1;

	const int Parsed = sscanf(pHeader + 11, "%d\t%31[^\t]\t%63[^\t]\t%63[^\t]\t%31[^\t]\t%lld\t%d\t%d\t%lld\t%lld\t%d\t%d",
		pFlags, aAcc, aAddr, aTimeout, Identity.m_aName, &ExpireDate, &Periodic, &Jail, &JailSeconds, &WalletMoney, &RedirectPort, &CarriedFlag);
	if(Parsed < 8)
		return false;

	if(str_comp(aAcc, "$") != 0)
		str_copy(Identity.m_aAccUsername, aAcc, sizeof(Identity.m_aAccUsername));
	if(str_comp(aTimeout, "$") != 0)
		str_copy(Identity.m_aTimeoutCode, aTimeout, sizeof(Identity.m_aTimeoutCode));
	net_addr_from_str(&Identity.m_Addr, aAddr);
	SavedAddrNormalize(&Identity.m_Addr);
	Identity.m_ExpireDate = (time_t)ExpireDate;
	Identity.m_IsPeriodic = Periodic != 0;
	Identity.m_IsJailSave = Jail != 0;
	Identity.m_IsMain = ((*pFlags) & SAVE_DDNET_DUMMY) == 0;
	Identity.m_JailSeconds = Parsed >= 9 ? JailSeconds : 0;
	Identity.m_WalletMoney = Parsed >= 10 ? WalletMoney : 0;
	Identity.m_RedirectTilePort = Parsed >= 11 ? RedirectPort : 0;
	Identity.m_CarriedFlag = Parsed >= 12 ? CarriedFlag : -1;

	return pSaveTee->FromString(pTeeLine, 1);
}

void CFddraceSave::RemoveSavedIdentityFile(const SSavedIdentity &Identity)
{
	char aPath[IO_MAX_PATH_LENGTH];
	FormatIdentitySavePath(aPath, sizeof(aPath), Identity);
	dbg_msg("fddrace_save", "%s: removing saved identity", Identity.m_aName);
	m_pGameServer->Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
}

void CFddraceSave::RemoveDuplicateSaves(const SSavedIdentity &Info, int Flags)
{
	const bool NewIsJail = (Flags & SAVE_JAIL) != 0;
	const bool NewIsRedirect = (Flags & SAVE_REDIRECT) != 0;

	for(size_t i = 0; i < m_vSavedIdentities.size();)
	{
		// Keep existing jail saves unless we are writing a new jail save for the same player.
		if(m_vSavedIdentities[i].m_IsJailSave && !NewIsJail)
		{
			i++;
			continue;
		}
		if(NewIsJail && !m_vSavedIdentities[i].m_IsJailSave)
		{
			i++;
			continue;
		}
		// Main and DDNet-dummy keep separate slots.
		if(m_vSavedIdentities[i].m_IsMain != Info.m_IsMain)
		{
			i++;
			continue;
		}
		// Map saves and redirect-folder saves stay independent.
		if(m_vSavedIdentities[i].m_IsRedirectSave != NewIsRedirect)
		{
			i++;
			continue;
		}

		bool SamePlayer = false;
		if(Info.m_aTimeoutCode[0] != '\0' && m_vSavedIdentities[i].m_aTimeoutCode[0] != '\0')
			SamePlayer = str_comp(m_vSavedIdentities[i].m_aTimeoutCode, Info.m_aTimeoutCode) == 0;
		if(!SamePlayer && Info.m_aAccUsername[0] != '\0' && m_vSavedIdentities[i].m_aAccUsername[0] != '\0')
			SamePlayer = str_comp(m_vSavedIdentities[i].m_aAccUsername, Info.m_aAccUsername) == 0;
		if(!SamePlayer)
		{
			const bool SameAddr = SavedAddrMatch(&m_vSavedIdentities[i].m_Addr, &Info.m_Addr, false);
			const bool SameName = NamesMatch(m_vSavedIdentities[i].m_aName, Info.m_aName);
			SamePlayer = SameAddr && SameName;
		}

		if(SamePlayer && m_vSavedIdentities[i].m_RedirectTilePort == Info.m_RedirectTilePort)
		{
			RemoveSavedIdentityFile(m_vSavedIdentities[i]);
			m_vSavedIdentities.erase(m_vSavedIdentities.begin() + i);
		}
		else
		{
			i++;
		}
	}
}

int CFddraceSave::SaveCharacter(int ClientId, int Flags, float Hours)
{
	CCharacter *pChr = m_pGameServer->GetPlayerChar(ClientId);
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	if(!pChr || !pPlayer)
		return -1;

	SSavedIdentity Info{};
	if(!GetClientAddr(m_pGameServer, ClientId, &Info.m_Addr))
		return -1;
	SavedAddrNormalize(&Info.m_Addr);
	str_copy(Info.m_aName, m_pGameServer->Server()->ClientName(ClientId), sizeof(Info.m_aName));
	str_copy(Info.m_aAccUsername, AccUsername(ClientId), sizeof(Info.m_aAccUsername));
	str_copy(Info.m_aTimeoutCode, pPlayer->m_aTimeoutCode, sizeof(Info.m_aTimeoutCode));
	Info.m_IsPeriodic = (Flags & SAVE_PERIODIC) != 0;
	Info.m_IsJailSave = (Flags & SAVE_JAIL) != 0;
	Info.m_IsMain = m_pGameServer->Server()->IsMain(ClientId);
	if(!Info.m_IsMain)
		Flags |= SAVE_DDNET_DUMMY;
	Info.m_IsRedirectSave = (Flags & SAVE_REDIRECT) != 0;
	Info.m_RedirectTilePort = pChr->m_RedirectTilePort;
	Info.m_CarriedFlag = -1;
	if(Flags & SAVE_FLAG)
		Info.m_CarriedFlag = m_pGameServer->HasFlag(pChr);
	if(Info.m_IsJailSave && pPlayer->m_JailTime > 0)
		Info.m_JailSeconds = pPlayer->m_JailTime / m_pGameServer->Server()->TickSpeed();
	if(Flags & SAVE_WALLET)
		Info.m_WalletMoney = pPlayer->GetWalletMoney();
	if(Hours >= 0.f)
		SetExpireDate(&Info.m_ExpireDate, Hours);
	str_copy(Info.m_aFileHash, GetSavedIdentityHash(Info), sizeof(Info.m_aFileHash));

	RemoveDuplicateSaves(Info, Flags);
	m_vSavedIdentities.push_back(Info);

	char aFilename[IO_MAX_PATH_LENGTH];
	FormatIdentitySavePath(aFilename, sizeof(aFilename), Info);

	if(!WriteSaveFile(aFilename, Flags, Info, pChr))
	{
		m_vSavedIdentities.pop_back();
		return -1;
	}

	return (int)m_vSavedIdentities.size() - 1;
}

int CFddraceSave::FindSavedPlayer(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !m_pGameServer->m_apPlayers[ClientId])
		return -1;

	const bool WantMain = m_pGameServer->Server()->IsMain(ClientId);

	NETADDR Addr;
	mem_zero(&Addr, sizeof(Addr));
	if(!GetClientAddr(m_pGameServer, ClientId, &Addr))
		return -1;
	SavedAddrNormalize(&Addr);

	const char *pCurrentName = m_pGameServer->Server()->ClientName(ClientId);
	const char *pMyTimeoutCode = m_pGameServer->m_apPlayers[ClientId]->m_aTimeoutCode;
	const bool LoggedIn = IsLoggedIn(ClientId);
	const char *pMyAccUsername = AccUsername(ClientId);

	int BestIndex = -1;
	int BestScore = -1;

	for(size_t i = 0; i < m_vSavedIdentities.size(); i++)
	{
		const SSavedIdentity &Info = m_vSavedIdentities[i];
		if(Info.m_IsMain != WantMain)
			continue;

		const bool SameAddr = SavedAddrMatch(&Addr, &Info.m_Addr, false);
		const bool SameAddrAndPort = SavedAddrMatch(&Addr, &Info.m_Addr, true);
		const bool TimeoutMatch = pMyTimeoutCode[0] != '\0' && Info.m_aTimeoutCode[0] != '\0'
			&& str_comp(Info.m_aTimeoutCode, pMyTimeoutCode) == 0;
		const bool AccMatch = LoggedIn && pMyAccUsername[0] != '\0' && Info.m_aAccUsername[0] != '\0'
			&& str_comp(Info.m_aAccUsername, pMyAccUsername) == 0;
		const bool NameMatch = NamesMatch(Info.m_aName, pCurrentName);
		const bool WeakMatch = SameAddr && NameMatch;

		int Score = -1;
		if(SameAddrAndPort)
			Score = 100;
		else if(TimeoutMatch)
			Score = 80;
		else if(AccMatch)
			Score = 70;
		else if(WeakMatch)
			Score = 40;

		if(Score < 0)
			continue;
		if(Info.m_IsJailSave)
			Score += 15;

		if(Score > BestScore)
		{
			BestScore = Score;
			BestIndex = (int)i;
		}
	}

	return BestIndex;
}

bool CFddraceSave::TryLoadPlayer(int ClientId, int Index)
{
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	if(!pPlayer || !pPlayer->GetCharacter())
		return false;

	const char *pHash = SavedIdentityFileStem(m_vSavedIdentities[Index], this);
	(void)pHash;
	char aPath[IO_MAX_PATH_LENGTH];
	FormatIdentitySavePath(aPath, sizeof(aPath), m_vSavedIdentities[Index]);

	SSavedIdentity Identity;
	int Flags = 0;
	CSaveTee SaveTee;
	if(!ReadSaveFile(aPath, Identity, &Flags, &SaveTee))
	{
		dbg_msg("fddrace_save", "TryLoadPlayer failed cID=%d path='%s'", ClientId, aPath);
		return false;
	}

	if(!SaveTee.Load(pPlayer->GetCharacter()))
	{
		dbg_msg("fddrace_save", "TryLoadPlayer load failed cID=%d path='%s'", ClientId, aPath);
		return false;
	}

	const int64_t JailSeconds = Identity.m_JailSeconds > 0 ? Identity.m_JailSeconds : m_vSavedIdentities[Index].m_JailSeconds;
	if(JailSeconds > 0 || (Flags & SAVE_JAIL) || Identity.m_IsJailSave)
	{
		const int64_t Seconds = JailSeconds > 0 ? JailSeconds : 1;
		pPlayer->m_JailTime = Seconds * m_pGameServer->Server()->TickSpeed();
	}

	if((Flags & SAVE_WALLET) || Identity.m_WalletMoney != 0)
		pPlayer->SetWalletMoney(Identity.m_WalletMoney);

	if(Identity.m_RedirectTilePort > 0)
		pPlayer->GetCharacter()->LoadRedirectTile(Identity.m_RedirectTilePort);

	if((Flags & SAVE_FLAG) && Identity.m_CarriedFlag >= 0)
	{
		CFlag *pFlag = m_pGameServer->GetFlag(Identity.m_CarriedFlag);
		if(pFlag && !pFlag->GetCarrier())
			m_pGameServer->ForceFlagOwner(ClientId, Identity.m_CarriedFlag);
	}

	dbg_msg("fddrace_save", "%d:%s used save, removing file", ClientId, m_pGameServer->Server()->ClientName(ClientId));
	m_pGameServer->Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
	m_vSavedIdentities.erase(m_vSavedIdentities.begin() + Index);
	pPlayer->m_LoadedSavedPlayer = true;
	pPlayer->m_ViewPos = pPlayer->GetCharacter()->GetPos();
	return true;
}

bool CFddraceSave::CheckLoadPlayer(int ClientId, bool FromSpawn)
{
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	if(!pPlayer || pPlayer->m_LoadedSavedPlayer)
		return false;

	const int Index = FindSavedPlayer(ClientId);
	if(Index < 0 || Index >= (int)m_vSavedIdentities.size())
		return false;

	if(m_vSavedIdentities[Index].m_IsPeriodic && !FromSpawn)
		return false;

	return TryLoadPlayer(ClientId, Index);
}

int CFddraceSave::LoadSavedPlayersCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CFddraceSave *pSelf = static_cast<CFddraceSave *>(pUser);
	if(!IsDir && str_endswith(pName, ".save"))
	{
		std::string Name = pName;
		pSelf->m_vSavedIdentitiesFiles.push_back(Name.erase(Name.size() - 5));
	}
	return 0;
}

void CFddraceSave::ReadSavedPlayersFile()
{
	m_vSavedIdentities.clear();
	m_vSavedIdentitiesFiles.clear();

	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "dumps/%s/%s",
		g_Config.m_SvSavedTeesFilePath,
		CurrentMapName(m_pGameServer));
	m_pGameServer->Storage()->ListDirectory(IStorage::TYPE_SAVE, aPath, LoadSavedPlayersCallback, this);

	for(const std::string &FileStem : m_vSavedIdentitiesFiles)
	{
		str_format(aPath, sizeof(aPath), "dumps/%s/%s/%s.save",
			g_Config.m_SvSavedTeesFilePath,
			CurrentMapName(m_pGameServer),
			FileStem.c_str());

		SSavedIdentity Identity;
		int Flags = 0;
		CSaveTee SaveTee;
		if(ReadSaveFile(aPath, Identity, &Flags, &SaveTee))
		{
			Identity.m_IsPeriodic = (Flags & SAVE_PERIODIC) != 0;
			Identity.m_IsJailSave = Identity.m_IsJailSave || (Flags & SAVE_JAIL) != 0;
			Identity.m_IsMain = (Flags & SAVE_DDNET_DUMMY) == 0;
			Identity.m_IsRedirectSave = false;
			str_copy(Identity.m_aFileHash, FileStem.c_str(), sizeof(Identity.m_aFileHash));
			if(IsExpired(Identity.m_ExpireDate))
			{
				RemoveSavedIdentityFile(Identity);
				continue;
			}
			m_vSavedIdentities.push_back(Identity);
		}
		else
		{
			dbg_msg("fddrace_save", "failed to parse saved tee '%s'", aPath);
		}
	}

	m_vSavedIdentitiesFiles.clear();

	// Cross-server redirect saves (destination port must match this server).
	str_format(aPath, sizeof(aPath), "dumps/%s/x_redirect_tile", g_Config.m_SvSavedTeesFilePath);
	m_pGameServer->Storage()->ListDirectory(IStorage::TYPE_SAVE, aPath, LoadSavedPlayersCallback, this);

	for(const std::string &FileStem : m_vSavedIdentitiesFiles)
	{
		str_format(aPath, sizeof(aPath), "dumps/%s/x_redirect_tile/%s.save",
			g_Config.m_SvSavedTeesFilePath, FileStem.c_str());

		SSavedIdentity Identity;
		int Flags = 0;
		CSaveTee SaveTee;
		if(ReadSaveFile(aPath, Identity, &Flags, &SaveTee))
		{
			Identity.m_IsPeriodic = (Flags & SAVE_PERIODIC) != 0;
			Identity.m_IsJailSave = Identity.m_IsJailSave || (Flags & SAVE_JAIL) != 0;
			Identity.m_IsMain = (Flags & SAVE_DDNET_DUMMY) == 0;
			Identity.m_IsRedirectSave = true;
			str_copy(Identity.m_aFileHash, FileStem.c_str(), sizeof(Identity.m_aFileHash));
			if(IsExpired(Identity.m_ExpireDate))
			{
				RemoveSavedIdentityFile(Identity);
				continue;
			}
			if(Identity.m_RedirectTilePort == g_Config.m_SvPort)
				m_vSavedIdentities.push_back(Identity);
		}
		else
		{
			dbg_msg("fddrace_save", "failed to parse redirect saved tee '%s'", aPath);
		}
	}

	m_vSavedIdentitiesFiles.clear();
}

void CFddraceSave::ExpireSavedIdentities()
{
	for(int i = (int)m_vSavedIdentities.size() - 1; i >= 0; i--)
	{
		if(IsExpired(m_vSavedIdentities[i].m_ExpireDate))
		{
			RemoveSavedIdentityFile(m_vSavedIdentities[i]);
			m_vSavedIdentities.erase(m_vSavedIdentities.begin() + i);
		}
	}
}

void CFddraceSave::OnMapLoaded()
{
	m_KeepSavedTeesOnDisconnect = false;
	m_LastPeriodicSaveTick = m_pGameServer->Server()->Tick();
	EnsureSaveDirs();
	ReadSavedPlayersFile();
	ExpireSavedIdentities();

	char aFull[IO_MAX_PATH_LENGTH];
	char aRel[IO_MAX_PATH_LENGTH];
	str_format(aRel, sizeof(aRel), "dumps/%s/%s", g_Config.m_SvSavedTeesFilePath, CurrentMapName(m_pGameServer));
	m_pGameServer->Storage()->GetCompletePath(IStorage::TYPE_SAVE, aRel, aFull, sizeof(aFull));
	dbg_msg("fddrace_save", "loaded %d saved identities from %s", (int)m_vSavedIdentities.size(), aFull);
}

void CFddraceSave::OnTick()
{
	if(g_Config.m_SvPeriodicSave <= 0)
		return;

	const int SaveInterval = g_Config.m_SvPeriodicSave * 60 * m_pGameServer->Server()->TickSpeed();
	if(m_pGameServer->Server()->Tick() - m_LastPeriodicSaveTick < SaveInterval)
		return;

	m_LastPeriodicSaveTick = m_pGameServer->Server()->Tick();

	int SavedCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pGameServer->m_apPlayers[i] || !m_pGameServer->GetPlayerChar(i))
			continue;

		if(SaveCharacter(i, SAVE_WALLET | SAVE_FLAG | SAVE_PERIODIC, g_Config.m_SvShutdownSaveTeeExpire) != -1)
			SavedCount++;
	}

	if(SavedCount > 0)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Periodic autosave: %d players saved", SavedCount);
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "autosave", aBuf);
	}
}

void CFddraceSave::OnShutdown()
{
	if(!g_Config.m_SvShutdownSaveTees)
		return;

	m_KeepSavedTeesOnDisconnect = true;

	int SavedCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pGameServer->m_apPlayers[i] || !m_pGameServer->GetPlayerChar(i))
			continue;

		if(SaveCharacter(i, SAVE_WALLET | SAVE_FLAG, g_Config.m_SvShutdownSaveTeeExpire) != -1)
			SavedCount++;
	}

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "Shutdown save: %d players saved", SavedCount);
	m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "save", aBuf);
}

void CFddraceSave::SaveDrop(int ClientId, float Hours, const char *pReason)
{
	if(!m_pGameServer->GetPlayerChar(ClientId) || !m_pGameServer->m_apPlayers[ClientId])
		return;

	SaveCharacter(ClientId, SAVE_WALLET, Hours);
	m_pGameServer->Server()->Kick(ClientId, pReason ? pReason : "save drop");
}

bool CFddraceSave::RemoveJailSave(int ClientId)
{
	const int SaveIndex = FindSavedPlayer(ClientId);
	if(SaveIndex < 0 || SaveIndex >= (int)m_vSavedIdentities.size())
		return false;
	if(!m_vSavedIdentities[SaveIndex].m_IsJailSave)
		return false;

	RemoveSavedIdentityFile(m_vSavedIdentities[SaveIndex]);
	m_vSavedIdentities.erase(m_vSavedIdentities.begin() + SaveIndex);
	return true;
}

void CFddraceSave::RemoveNonJailSaves(int ClientId)
{
	// Build identity matching the disconnecting client, then drop non-jail saves.
	CPlayer *pPlayer = m_pGameServer->m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	SSavedIdentity Info{};
	if(!GetClientAddr(m_pGameServer, ClientId, &Info.m_Addr))
		return;
	SavedAddrNormalize(&Info.m_Addr);
	str_copy(Info.m_aName, m_pGameServer->Server()->ClientName(ClientId), sizeof(Info.m_aName));
	str_copy(Info.m_aAccUsername, AccUsername(ClientId), sizeof(Info.m_aAccUsername));
	str_copy(Info.m_aTimeoutCode, pPlayer->m_aTimeoutCode, sizeof(Info.m_aTimeoutCode));
	Info.m_IsMain = m_pGameServer->Server()->IsMain(ClientId);

	for(size_t i = 0; i < m_vSavedIdentities.size();)
	{
		if(m_vSavedIdentities[i].m_IsJailSave)
		{
			i++;
			continue;
		}
		if(m_vSavedIdentities[i].m_IsMain != Info.m_IsMain)
		{
			i++;
			continue;
		}

		bool SamePlayer = false;
		if(Info.m_aTimeoutCode[0] != '\0' && m_vSavedIdentities[i].m_aTimeoutCode[0] != '\0')
			SamePlayer = str_comp(m_vSavedIdentities[i].m_aTimeoutCode, Info.m_aTimeoutCode) == 0;
		if(!SamePlayer && Info.m_aAccUsername[0] != '\0' && m_vSavedIdentities[i].m_aAccUsername[0] != '\0')
			SamePlayer = str_comp(m_vSavedIdentities[i].m_aAccUsername, Info.m_aAccUsername) == 0;
		if(!SamePlayer)
		{
			SamePlayer = SavedAddrMatch(&m_vSavedIdentities[i].m_Addr, &Info.m_Addr, false)
				&& NamesMatch(m_vSavedIdentities[i].m_aName, Info.m_aName);
		}

		if(SamePlayer)
		{
			RemoveSavedIdentityFile(m_vSavedIdentities[i]);
			m_vSavedIdentities.erase(m_vSavedIdentities.begin() + i);
		}
		else
		{
			i++;
		}
	}
}

void CFddraceSave::ListSavedTees() const
{
	m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "save", "Listing saved identities:");
	m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "save", "----------------------------------");
	for(const SSavedIdentity &Info : m_vSavedIdentities)
	{
		char aAddr[NETADDR_MAXSTRSIZE];
		net_addr_str(&Info.m_Addr, aAddr, sizeof(aAddr), true);
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "| %s | expire=%lld | '%s' | %s | %s | periodic=%d | jail=%lld |",
			Info.m_aFileHash[0] ? Info.m_aFileHash : "?",
			(long long)Info.m_ExpireDate,
			Info.m_aName,
			Info.m_aAccUsername[0] ? Info.m_aAccUsername : "<no_acc>",
			aAddr,
			Info.m_IsPeriodic ? 1 : 0,
			(long long)Info.m_JailSeconds);
		m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "save", aBuf);
	}
	char aCount[64];
	str_format(aCount, sizeof(aCount), "Total: %d", (int)m_vSavedIdentities.size());
	m_pGameServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "save", aCount);
}

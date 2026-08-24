#include "anticheat.h"

#include <base/dbg.h>
#include <base/io.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>

#include <algorithm>

CAntiCheat::CAntiCheat()
{
	m_pServer = nullptr;
	m_pStorage = nullptr;
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aClients[i].Reset();
}

void CAntiCheat::CClientData::Reset()
{
	m_InfoMessages = m_ReadyMessages = m_EnterGameMessages = m_StartInfoMessages = 0;
	m_SentUuidMessage = m_HasConnectionID = m_SentClientVer = m_Checked = false;
	m_PendingJail = false;
	m_aPendingJailReason[0] = '\0';
	m_ConnectTime = m_FirstMessageTime = m_LastCheckTime = 0;
	m_SuspiciousScore = 0;
}

void CAntiCheat::Init(IServer *pServer, IStorage *pStorage)
{
	m_pServer = pServer;
	m_pStorage = pStorage;
	LoadConfig();
}

bool CAntiCheat::WildcardMatch(const char *pStr, const char *pPattern)
{
	if(!pPattern || !*pPattern || str_comp(pPattern, "*") == 0) return true;
	const char *pS = pStr ? pStr : "", *pP = pPattern, *pStar = nullptr, *pMatch = pS;
	while(*pS)
	{
		if(*pP == '*') { pStar = pP++; pMatch = pS; }
		else if(*pP == *pS) { pP++; pS++; }
		else if(pStar) { pP = pStar + 1; pS = ++pMatch; }
		else return false;
	}
	while(*pP == '*') pP++;
	return !*pP;
}

bool CAntiCheat::MatchFingerprint(const CClientFingerprint &FP, const CClientListEntry &E) const
{
	if(E.m_DDNetVersion && E.m_DDNetVersion != FP.m_DDNetVersion) return false;
	if(E.m_aVersionStr[0] && !WildcardMatch(FP.m_aVersionStr, E.m_aVersionStr)) return false;
	if(E.m_aIamTaterStr[0] && !WildcardMatch(FP.m_aIamTaterStr, E.m_aIamTaterStr)) return false;
	if(E.m_Sevendown != -1 && E.m_Sevendown != (int)FP.m_Sevendown) return false;
	if(E.m_TClientVerified != -1 && E.m_TClientVerified != (int)FP.m_TClientVerified) return false;
	if(E.m_HasHardcodedHash != -1 && E.m_HasHardcodedHash != (int)FP.m_HasHardcodedHash) return false;
	return true;
}

bool CAntiCheat::ParseEntry(const char *pLine, CClientListEntry &Entry)
{
	if(!pLine || pLine[0] == '#' || !pLine[0]) return false;
	char aLine[512];
	str_copy(aLine, pLine, sizeof(aLine));

	char *pComment = (char *)str_find(aLine, " # ");
	if(pComment) { str_copy(Entry.m_aComment, pComment + 3, sizeof(Entry.m_aComment)); *pComment = 0; }

	if(str_startswith(aLine, "whitelist ")) Entry.m_IsWhitelist = true;
	else if(str_startswith(aLine, "blacklist ")) Entry.m_IsWhitelist = false;
	else return false;

	const char *pFields = aLine + 10;
	if(!str_find(pFields, "="))
	{
		char aHash[64];
		if(sscanf(pFields, "%63s", aHash) == 1 && aHash[0])
			{ Entry.m_IsOldFormat = true; str_copy(Entry.m_aHashOnly, aHash, sizeof(Entry.m_aHashOnly)); return true; }
		return false;
	}

	Entry.m_IsOldFormat = false;
	char aBuf[512]; str_copy(aBuf, pFields, sizeof(aBuf));
	char *p = aBuf;
	while(*p)
	{
		while(*p == ' ') p++;
		if(!*p) break;
		char *pKey = p;
		char *pEq = (char *)str_find(p, "=");
		if(!pEq) break;
		*pEq = 0; p = pEq + 1;
		char aVal[256];
		if(*p == '"')
		{
			p++;
			char *pEnd = (char *)str_find(p, "\"");
			if(!pEnd) break;
			int len = (int)(pEnd - p);
			str_copy(aVal, p, std::min(len + 1, (int)sizeof(aVal)));
			p = pEnd + 1;
		}
		else
		{
			char *pEnd = p;
			while(*pEnd && *pEnd != ' ') pEnd++;
			int len = (int)(pEnd - p);
			str_copy(aVal, p, std::min(len + 1, (int)sizeof(aVal)));
			p = pEnd;
		}
		if(!str_comp(pKey, "ddnet_version"))    Entry.m_DDNetVersion = str_toint(aVal);
		else if(!str_comp(pKey, "version_str")) str_copy(Entry.m_aVersionStr, aVal, sizeof(Entry.m_aVersionStr));
		else if(!str_comp(pKey, "iamtater"))    str_copy(Entry.m_aIamTaterStr, aVal, sizeof(Entry.m_aIamTaterStr));
		else if(!str_comp(pKey, "sevendown"))   Entry.m_Sevendown = str_toint(aVal);
		else if(!str_comp(pKey, "verified"))    Entry.m_TClientVerified = str_toint(aVal);
		else if(!str_comp(pKey, "hardcoded"))   Entry.m_HasHardcodedHash = str_toint(aVal);
	}
	return true;
}

void CAntiCheat::LoadConfig()
{
	m_vWhitelist.clear();
	m_vBlacklist.clear();
	if(!m_pStorage) return;

	char aFullPath[512];
	m_pStorage->GetBinaryPath("../anticheat_hashes.cfg", aFullPath, sizeof(aFullPath));
	dbg_msg("anticheat", "Attempting to load hash config from: '%s'", aFullPath);

	IOHANDLE File = io_open(aFullPath, IOFLAG_READ);
	if(!File) { dbg_msg("anticheat", "No anticheat_hashes.cfg found"); return; }

	CLineReader LR;
	if(!LR.OpenFile(File)) return;

	int nW = 0, nB = 0;
	const char *pLine;
	while((pLine = LR.Get()))
	{
		if(!str_length(pLine) || pLine[0] == '#') continue;
		CClientListEntry E;
		if(ParseEntry(pLine, E))
		{
			if(E.m_IsWhitelist) { m_vWhitelist.push_back(E); nW++; }
			else { m_vBlacklist.push_back(E); nB++; }
		}
	}
	dbg_msg("anticheat", "Loaded %d whitelist and %d blacklist entries from '%s'", nW, nB, aFullPath);
}

CClientFingerprint CAntiCheat::GetFingerprint(int ClientID) const
{
	CClientFingerprint FP;
	if(!m_pServer || ClientID < 0 || ClientID >= MAX_CLIENTS) return FP;
	IServer::CClientInfo Info;
	if(!m_pServer->GetClientInfo(ClientID, &Info)) return FP;
	FP.m_DDNetVersion = Info.m_DDNetVersion;
	if(Info.m_pDDNetVersionStr) str_copy(FP.m_aVersionStr, Info.m_pDDNetVersionStr, sizeof(FP.m_aVersionStr));
	FP.m_Sevendown = m_pServer->IsSixup(ClientID);
	FP.m_TClientVerified = m_pServer->IsClientTClientVerified(ClientID);
	FP.m_HasHardcodedHash = m_pServer->IsClientHardcodedHash(ClientID);
	const char *pIT = m_pServer->GetClientIamTaterStr(ClientID);
	if(pIT)
	{
		str_copy(FP.m_aIamTaterStr, pIT, sizeof(FP.m_aIamTaterStr));
		FP.m_SentIamTater = pIT[0] != 0;
	}
	return FP;
}

void CAntiCheat::FingerprintToString(const CClientFingerprint &FP, char *pBuf, int BufSize)
{
	str_format(pBuf, BufSize, "ddnet_version=%d version_str=\"%s\" iamtater=\"%s\" sevendown=%d verified=%d hardcoded=%d",
		FP.m_DDNetVersion, FP.m_aVersionStr, FP.m_aIamTaterStr,
		(int)FP.m_Sevendown, (int)FP.m_TClientVerified, (int)FP.m_HasHardcodedHash);
}

void CAntiCheat::SaveConfig()
{
	if(!m_pStorage) return;
	char aFullPath[512];
	m_pStorage->GetBinaryPath("../anticheat_hashes.cfg", aFullPath, sizeof(aFullPath));
	IOHANDLE File = io_open(aFullPath, IOFLAG_WRITE);
	if(!File) { dbg_msg("anticheat", "Failed to save anticheat_hashes.cfg"); return; }

	const char *pHdr = "# anticheat_hashes.cfg\n# Old format: whitelist/blacklist <hash>\n"
		"# New format: whitelist/blacklist ddnet_version=N version_str=\"pat\" iamtater=\"pat\" sevendown=0/1 verified=0/1 hardcoded=0/1 # comment\n"
		"# Whitelist has priority over blacklist. Supports * wildcard.\n\n";
	io_write(File, pHdr, str_length(pHdr));

	auto Write = [&](const CClientListEntry &E, const char *pType)
	{
		char aLine[1024];
		if(E.m_IsOldFormat)
		{
			str_format(aLine, sizeof(aLine), "%s %s%s%s\n", pType, E.m_aHashOnly,
				E.m_aComment[0] ? " # " : "", E.m_aComment);
		}
		else
		{
			char aP[512] = "";
			if(E.m_DDNetVersion) { char t[32]; str_format(t, sizeof(t), " ddnet_version=%d", E.m_DDNetVersion); str_append(aP, t, sizeof(aP)); }
			if(E.m_aVersionStr[0]) { char t[128]; str_format(t, sizeof(t), " version_str=\"%s\"", E.m_aVersionStr); str_append(aP, t, sizeof(aP)); }
			if(E.m_aIamTaterStr[0]) { char t[192]; str_format(t, sizeof(t), " iamtater=\"%s\"", E.m_aIamTaterStr); str_append(aP, t, sizeof(aP)); }
			if(E.m_Sevendown != -1) { char t[16]; str_format(t, sizeof(t), " sevendown=%d", E.m_Sevendown); str_append(aP, t, sizeof(aP)); }
			if(E.m_TClientVerified != -1) { char t[16]; str_format(t, sizeof(t), " verified=%d", E.m_TClientVerified); str_append(aP, t, sizeof(aP)); }
			if(E.m_HasHardcodedHash != -1) { char t[16]; str_format(t, sizeof(t), " hardcoded=%d", E.m_HasHardcodedHash); str_append(aP, t, sizeof(aP)); }
			str_format(aLine, sizeof(aLine), "%s%s%s%s\n", pType, aP,
				E.m_aComment[0] ? " # " : "", E.m_aComment);
		}
		io_write(File, aLine, str_length(aLine));
	};
	for(const auto &E : m_vWhitelist) Write(E, "whitelist");
	for(const auto &E : m_vBlacklist) Write(E, "blacklist");
	io_close(File);
	dbg_msg("anticheat", "Saved anticheat_hashes.cfg (%d whitelist, %d blacklist)", (int)m_vWhitelist.size(), (int)m_vBlacklist.size());
}

void CAntiCheat::AddHashEntry(const CClientListEntry &Entry)
{
	if(Entry.m_IsWhitelist) m_vWhitelist.push_back(Entry);
	else m_vBlacklist.push_back(Entry);
	SaveConfig();
	dbg_msg("anticheat", "Added %s entry: %s", Entry.m_IsWhitelist ? "whitelist" : "blacklist",
		Entry.m_IsOldFormat ? Entry.m_aHashOnly : Entry.m_aVersionStr);
}

int CAntiCheat::RemoveHashEntries(const char *pHashes, bool bWhitelist)
{
	char aBuf[512];
	str_copy(aBuf, pHashes, sizeof(aBuf));
	auto &vList = bWhitelist ? m_vWhitelist : m_vBlacklist;
	int Removed = 0;
	char *pHash = strtok(aBuf, ",");
	while(pHash)
	{
		while(*pHash == ' ') pHash++;
		char *pEnd = pHash + str_length(pHash) - 1;
		while(pEnd > pHash && *pEnd == ' ') { *pEnd = 0; pEnd--; }
		if(str_length(pHash) > 0)
		{
			auto it = vList.begin();
			while(it != vList.end())
			{
				bool Match = (it->m_IsOldFormat && str_comp(it->m_aHashOnly, pHash) == 0) ||
				             (!it->m_IsOldFormat && str_find(it->m_aVersionStr, pHash) != nullptr);
				if(Match) { it = vList.erase(it); Removed++; }
				else ++it;
			}
		}
		pHash = strtok(nullptr, ",");
	}
	if(Removed) SaveConfig();
	return Removed;
}

bool CAntiCheat::AddToWhitelist(int ClientID, const char *pComment)
{
	if(!m_pServer || ClientID < 0 || ClientID >= MAX_CLIENTS) return false;
	CClientFingerprint FP = GetFingerprint(ClientID);
	CClientListEntry E;
	E.m_IsWhitelist = true; E.m_IsOldFormat = false;
	E.m_DDNetVersion = FP.m_DDNetVersion;
	str_copy(E.m_aVersionStr, FP.m_aVersionStr, sizeof(E.m_aVersionStr));
	str_copy(E.m_aIamTaterStr, FP.m_aIamTaterStr, sizeof(E.m_aIamTaterStr));
	E.m_Sevendown = FP.m_Sevendown ? 1 : 0;
	E.m_TClientVerified = E.m_HasHardcodedHash = -1;
	str_copy(E.m_aComment, pComment ? pComment : m_pServer->ClientName(ClientID), sizeof(E.m_aComment));
	m_vWhitelist.push_back(E); SaveConfig();
	char aFP[512]; FingerprintToString(FP, aFP, sizeof(aFP));
	dbg_msg("anticheat", "ClientID=%d added to whitelist: %s # %s", ClientID, aFP, E.m_aComment);
	return true;
}

bool CAntiCheat::AddToBlacklist(int ClientID, const char *pComment)
{
	if(!m_pServer || ClientID < 0 || ClientID >= MAX_CLIENTS) return false;
	CClientFingerprint FP = GetFingerprint(ClientID);
	CClientListEntry E;
	E.m_IsWhitelist = false; E.m_IsOldFormat = false;
	E.m_DDNetVersion = FP.m_DDNetVersion;
	str_copy(E.m_aVersionStr, FP.m_aVersionStr, sizeof(E.m_aVersionStr));
	str_copy(E.m_aIamTaterStr, FP.m_aIamTaterStr, sizeof(E.m_aIamTaterStr));
	E.m_Sevendown = FP.m_Sevendown ? 1 : 0;
	E.m_TClientVerified = -1;
	E.m_HasHardcodedHash = FP.m_HasHardcodedHash ? 1 : -1;
	str_copy(E.m_aComment, pComment ? pComment : m_pServer->ClientName(ClientID), sizeof(E.m_aComment));
	m_vBlacklist.push_back(E); SaveConfig();
	char aFP[512]; FingerprintToString(FP, aFP, sizeof(aFP));
	dbg_msg("anticheat", "ClientID=%d added to blacklist: %s # %s", ClientID, aFP, E.m_aComment);
	return true;
}

static void PrintEntry(IConsole *pConsole, int i, const CClientListEntry &E)
{
	char aBuf[512];
	if(E.m_IsOldFormat)
	{
		str_format(aBuf, sizeof(aBuf), "[%d] hash: %s%s%s", i, E.m_aHashOnly,
			E.m_aComment[0] ? " # " : "", E.m_aComment);
	}
	else
	{
		char aP[384] = "";
		if(E.m_DDNetVersion) { char t[32]; str_format(t, sizeof(t), " ver=%d", E.m_DDNetVersion); str_append(aP, t, sizeof(aP)); }
		if(E.m_aVersionStr[0]) { char t[128]; str_format(t, sizeof(t), " str=\"%s\"", E.m_aVersionStr); str_append(aP, t, sizeof(aP)); }
		if(E.m_aIamTaterStr[0]) { char t[160]; str_format(t, sizeof(t), " iamtater=\"%s\"", E.m_aIamTaterStr); str_append(aP, t, sizeof(aP)); }
		if(E.m_Sevendown != -1) { char t[16]; str_format(t, sizeof(t), " 7down=%d", E.m_Sevendown); str_append(aP, t, sizeof(aP)); }
		if(E.m_TClientVerified != -1) { char t[16]; str_format(t, sizeof(t), " verified=%d", E.m_TClientVerified); str_append(aP, t, sizeof(aP)); }
		if(E.m_HasHardcodedHash != -1) { char t[16]; str_format(t, sizeof(t), " hardcoded=%d", E.m_HasHardcodedHash); str_append(aP, t, sizeof(aP)); }
		str_format(aBuf, sizeof(aBuf), "[%d] fp:%s%s%s", i, aP, E.m_aComment[0] ? " # " : "", E.m_aComment);
	}
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "anticheat", aBuf);
}

void CAntiCheat::PrintWhitelist(IConsole *pConsole) const
{
	char aBuf[64]; str_format(aBuf, sizeof(aBuf), "Whitelist (%d entries):", (int)m_vWhitelist.size());
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "anticheat", aBuf);
	for(int i = 0; i < (int)m_vWhitelist.size(); i++) PrintEntry(pConsole, i, m_vWhitelist[i]);
}

void CAntiCheat::PrintBlacklist(IConsole *pConsole) const
{
	char aBuf[64]; str_format(aBuf, sizeof(aBuf), "Blacklist (%d entries):", (int)m_vBlacklist.size());
	pConsole->Print(IConsole::OUTPUT_LEVEL_STANDARD, "anticheat", aBuf);
	for(int i = 0; i < (int)m_vBlacklist.size(); i++) PrintEntry(pConsole, i, m_vBlacklist[i]);
}

static void ExtractHash(const char *pVersionStr, char *pHash, int HashSize)
{
	pHash[0] = 0;
	if(!pVersionStr || !pVersionStr[0]) return;
	const char *pS = str_find(pVersionStr, "(");
	if(!pS) return;
	pS++;
	const char *pE = str_find(pS, ")");
	if(!pE || pE <= pS) return;
	int len = (int)(pE - pS);
	if(len > 0 && len < HashSize) str_copy(pHash, pS, len + 1);
}

bool CAntiCheat::CheckClientAllowed(int ClientID) const
{
	if(!m_pServer) return true;
	const int Mode = GetMode();
	if(Mode == 2)
		return true;

	CClientFingerprint FP = GetFingerprint(ClientID);

	char aHash[64];
	ExtractHash(FP.m_aVersionStr, aHash, sizeof(aHash));

	// В режиме blacklist проверяем только совпадения с blacklist.
	if(Mode == 1)
	{
		for(const auto &E : m_vBlacklist)
		{
			if(E.m_IsOldFormat) { if(aHash[0] && !str_comp(aHash, E.m_aHashOnly)) return false; }
			else if(MatchFingerprint(FP, E)) return false;
		}
		return true;
	}

	// Whitelist — приоритет
	for(const auto &E : m_vWhitelist)
	{
		if(E.m_IsOldFormat) { if(aHash[0] && !str_comp(aHash, E.m_aHashOnly)) { dbg_msg("anticheat", "ClientID=%d: ALLOWED whitelist hash '%s'", ClientID, aHash); return true; } }
		else if(MatchFingerprint(FP, E)) { dbg_msg("anticheat", "ClientID=%d: ALLOWED whitelist fp # %s", ClientID, E.m_aComment); return true; }
	}
	// Blacklist
	for(const auto &E : m_vBlacklist)
	{
		if(E.m_IsOldFormat) { if(aHash[0] && !str_comp(aHash, E.m_aHashOnly)) { dbg_msg("anticheat", "ClientID=%d: BLOCKED blacklist hash '%s'", ClientID, aHash); return false; } }
		else if(MatchFingerprint(FP, E)) { dbg_msg("anticheat", "ClientID=%d: BLOCKED blacklist fp # %s", ClientID, E.m_aComment); return false; }
	}

	// Не в списках
	if(aHash[0])
	{
		if(str_length(aHash) >= 40) { dbg_msg("anticheat", "ClientID=%d: ALLOWED hash len>=40", ClientID); return true; }
		dbg_msg("anticheat", "ClientID=%d: BLOCKED short hash '%s' not in whitelist", ClientID, aHash);
		return false;
	}
	// Нет хеша в строке версии
	if(!FP.m_aVersionStr[0]) { dbg_msg("anticheat", "ClientID=%d: ALLOWED no version string (old client)", ClientID); return true; }
	dbg_msg("anticheat", "ClientID=%d: BLOCKED no hash in version string '%s'", ClientID, FP.m_aVersionStr);
	return false;
}

int CAntiCheat::GetMode() const
{
	if(!m_pServer)
		return 2;
	return g_Config.m_SvAnticheatMode;
}

bool CAntiCheat::HandleViolation(int ClientID, const char *pReason)
{
	if(!m_pServer || ClientID < 0 || ClientID >= MAX_CLIENTS)
		return false;

	CClientFingerprint FP = GetFingerprint(ClientID);
	char aAddr[NETADDR_MAXSTRSIZE];
	m_pServer->GetClientAddrStr(ClientID, aAddr, sizeof(aAddr), true);

	char aReport[2048];
	str_format(aReport, sizeof(aReport),
		"**Anticheat jail**\nReason: `%s`\nCID: `%d`\nName: `%s`\nClan: `%s`\nAddress: `%s`\nAuth ident: `%s`\nCountry: `%d`\nClient version: `%d`\nRaw net version: `%s`\nDDNet version: `%d`\nDDNet version string: `%s`\nIAMTATER: `%s`\nSevendown: `%d`\nTClient verified: `%d`\nHardcoded hash: `%d`\nSent IAMTATER: `%d`\nJail time: `3600 seconds`",
		pReason ? pReason : "unknown", ClientID, m_pServer->ClientName(ClientID),
		m_pServer->ClientClan(ClientID), aAddr, m_pServer->GetAuthIdent(ClientID),
		m_pServer->ClientCountry(ClientID), m_pServer->GetClientVersion(ClientID),
		m_pServer->GetClientNetVersion(ClientID), FP.m_DDNetVersion,
		FP.m_aVersionStr, FP.m_aIamTaterStr, FP.m_Sevendown, FP.m_TClientVerified,
		FP.m_HasHardcodedHash, FP.m_SentIamTater);

	if(g_Config.m_SvWebhookModLogURL[0])
	{
		m_pServer->SendWebhookMessage(g_Config.m_SvWebhookModLogURL, aReport,
			"Anticheat", "");
	}

	// Jail system is ported later; kick until then.
	m_pServer->Kick(ClientID, pReason ? pReason : GetKickMessage());
	return true;
}

bool CAntiCheat::ValidateClientHash(const char *pHash, int)
{
	if(!pHash || !str_length(pHash)) return false;
	for(const auto &E : m_vWhitelist)
	{
		if(E.m_IsOldFormat && !str_comp(pHash, E.m_aHashOnly)) return true;
		if(!E.m_IsOldFormat && E.m_aVersionStr[0]) { char t[128]; str_format(t, sizeof(t), "*(%s)*", pHash); if(WildcardMatch(t, E.m_aVersionStr)) return true; }
	}
	for(const auto &E : m_vBlacklist)
	{
		if(E.m_IsOldFormat && !str_comp(pHash, E.m_aHashOnly)) return false;
		if(!E.m_IsOldFormat && E.m_aVersionStr[0]) { char t[128]; str_format(t, sizeof(t), "*(%s)*", pHash); if(WildcardMatch(t, E.m_aVersionStr)) return false; }
	}
	return str_length(pHash) >= 40;
}

bool CAntiCheat::AnalyzeClientBehavior(int ClientID)
{
	if(!m_pServer) return false;
	dbg_msg("anticheat", "ClientID=%d: Version=%d String='%s'", ClientID,
		m_pServer->GetClientVersion(ClientID), m_pServer->GetClientVersionStr(ClientID));
	return CheckClientAllowed(ClientID);
}

bool CAntiCheat::ValidateClient(int ClientID, char *pKickReason, int KickReasonSize)
{
	if(!AnalyzeClientBehavior(ClientID)) { str_format(pKickReason, KickReasonSize, "%s", GetKickMessage()); return false; }
	return true;
}

bool CAntiCheat::ValidateClientWithData(int ClientID, int DDNetVersion, const char *pDDNetVersionStr, char *pKickReason, int KickReasonSize)
{
	dbg_msg("anticheat", "=== Analyzing ClientID=%d (with data) === DDNetVersion=%d VersionStr='%s'",
		ClientID, DDNetVersion, pDDNetVersionStr ? pDDNetVersionStr : "NULL");
	dbg_msg("anticheat", "ClientID=%d: Skipping check on connect, will check after spawn", ClientID);
	return true;
}

void CAntiCheat::OnClientMessage(int ClientID, int MsgID, bool System)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS) return;
	CClientData *pC = &m_aClients[ClientID];
	if(!pC->m_FirstMessageTime) pC->m_FirstMessageTime = time_get();
	if(System)
	{
		if(MsgID == NETMSG_INFO && ++pC->m_InfoMessages > 1) pC->m_SuspiciousScore += 10;
		else if(MsgID == NETMSG_READY && ++pC->m_ReadyMessages > 1) pC->m_SuspiciousScore += 10;
		else if(MsgID == NETMSG_ENTERGAME && ++pC->m_EnterGameMessages > 1) pC->m_SuspiciousScore += 10;
	}
}

void CAntiCheat::OnClientDisconnect(int ClientID)
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS) m_aClients[ClientID].Reset();
}

const char *CAntiCheat::GetKickMessage()
{
	return "Invalid client detected. Please use official DDNet client from ddnet.org";
}

void CAntiCheat::Tick()
{
	if(!m_pServer)
		return;
	if(GetMode() == 2)
		return;

	int64_t Now = time_get();
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!m_pServer->ClientIngame(i))
			continue;
		if(m_pServer->IsOldClient(i))
			continue;

		CClientData *pC = &m_aClients[i];
		if(!pC->m_Checked)
		{
			if(!pC->m_ConnectTime)
				pC->m_ConnectTime = Now;
			if(Now - pC->m_ConnectTime < time_freq() * 2)
				continue;
			pC->m_Checked = true;
			pC->m_LastCheckTime = Now;
		}
		else
		{
			if(Now - pC->m_LastCheckTime < time_freq() * 30)
				continue;
			pC->m_LastCheckTime = Now;
		}

		if(!CheckClientAllowed(i))
		{
			dbg_msg("anticheat", "ClientID=%d: FAILED periodic check", i);
			HandleViolation(i, "client fingerprint is not allowed");
		}
		else
			dbg_msg("anticheat", "ClientID=%d: Check PASSED", i);
	}
}

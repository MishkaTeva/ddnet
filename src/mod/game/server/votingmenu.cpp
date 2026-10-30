#include "votingmenu.h"

#include <algorithm>
#include <cstring>
#include <ctime>

#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/server.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <mod/game/server/account.h>
#include <mod/game/server/houses/shop.h>
#include <mod/game/server/minigames/arenas.h>
#include <mod/game/server/minigames/fng.h>
#include <mod/game/server/minigames/minigame.h>

static const char *PLACEHOLDER_DESC = " ";
static const char *COLLAPSE_HEADER_ACC_INFO = "Aᴄᴄᴏᴜɴᴛ Iɴғᴏ";
static const char *COLLAPSE_HEADER_ACC_STATS = "Aᴄᴄᴏᴜɴᴛ Sᴛᴀᴛs";
static const char *COLLAPSE_HEADER_PLOT_INFO = "Pʟᴏᴛ";
static const char *ACC_PLOT_EDIT = "Edit Plot";
static const char *ACC_PLOT_CLEAR = "Clear Plot";
static const char *ACC_VIP_RAINBOW = "Rainbow";
static const char *ACC_VIP_BLOODY = "Bloody";
static const char *ACC_VIP_ATOM = "Atom";
static const char *ACC_VIP_TRAIL = "Trail";
static const char *ACC_VIP_PLUS_ROTATINGBALL = "Rotating Ball";
static const char *ACC_VIP_PLUS_EPICCIRCLE = "Epic Circle";
static const char *ACC_VIP_PLUS_LOVELY = "Lovely";
static const char *ACC_MISC_NINJAJETPACK = "Ninjajetpack";

static const char *COLLAPSE_HEADER_MINIGAME_SETTINGS = "Sᴇᴛᴛɪɴɢs";
static const char *MG_FNG_TUNE_RESET = "Original physics";
static const char *MG_FNG_TEAM_MODE = "Team mode";
static const char *MG_FNG_SCORE_LIMIT = "Score limit";
static const char *MG_FNG_SCORE_LIMIT_HINT = "Set score limit in the reason field (50-1500, step 50)";
static const char *MG_FNG_CREATE_ROOM = "Create FNG room or /fng";
static const char *MG_FNG_LEAVE_ROOM = "Leave FNG room";
static const char *MG_FNG_JOIN_ROOM = "Join room";
static const char *MG_1VS1_SCORE_LIMIT = "1vs1 score limit";
static const char *MG_1VS1_SCORE_LIMIT_HINT = "Set score limit in the reason field (1-100)";
static const char *MG_1VS1_KILL_BORDER = "Kill border";
static const char *MG_1VS1_TUNE_RESET = "1vs1 original physics";
static const char *MG_1VS1_HAMMER = "Hammer";
static const char *MG_1VS1_SHOTGUN = "Shotgun";
static const char *MG_1VS1_GRENADE = "Grenade";
static const char *MG_1VS1_LASER = "Laser";

static const char *str_skip_voting_menu_prefixes(const char *pStr)
{
	if(!pStr || !pStr[0])
		return nullptr;

	const char *pPrefixes[] = {"•", "☒", "☐", "│", "╭", "─", ">", "⇨", "⇦", "✚"};
	const char *pTemp = pStr;
	while(true)
	{
		bool Break = true;
		for(const char *pPrefix : pPrefixes)
		{
			const char *pFound = str_utf8_find_nocase(pTemp, pPrefix);
			if(pFound)
			{
				int NewCursor = str_utf8_forward(pFound, 0);
				if(NewCursor != 0)
				{
					pTemp = pFound + NewCursor;
					Break = false;
					break;
				}
			}
		}
		if(Break)
			break;
	}
	return str_utf8_skip_whitespaces(pTemp);
}

static void FormatDate(char *pBuf, int BufSize, int64_t Timestamp)
{
	if(!Timestamp)
	{
		str_copy(pBuf, "n/a", BufSize);
		return;
	}
	time_t T = (time_t)Timestamp;
	struct tm *pTm = localtime(&T);
	if(!pTm)
	{
		str_copy(pBuf, "n/a", BufSize);
		return;
	}
	str_format(pBuf, BufSize, "%04d-%02d-%02d", pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday);
}

IServer *CVotingMenu::Server() const { return GameServer()->Server(); }

void CVotingMenu::Init(CGameContext *pGameServer)
{
	m_pGameServer = pGameServer;
	m_NumCollapseEntries = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
		Reset(i);

	str_copy(m_aPages[PAGE_VOTES].m_aName, "Vᴏᴛᴇs", sizeof(m_aPages[PAGE_VOTES].m_aName));
	str_copy(m_aPages[PAGE_ACCOUNT].m_aName, "Aᴄᴄᴏᴜɴᴛ", sizeof(m_aPages[PAGE_ACCOUNT].m_aName));
	str_copy(m_aPages[PAGE_MISCELLANEOUS].m_aName, "Mɪsᴄᴇʟʟᴀɴᴇᴏᴜs", sizeof(m_aPages[PAGE_MISCELLANEOUS].m_aName));
	str_copy(m_aPages[PAGE_LANGUAGES].m_aName, "Lᴀɴɢᴜᴀɢᴇs", sizeof(m_aPages[PAGE_LANGUAGES].m_aName));

	for(auto &Desc : m_aaTempDesc)
		Desc[0] = 0;

	m_Initialized = true;
}

void CVotingMenu::Reset(int ClientId)
{
	m_aClients[ClientId].m_Page = PAGE_MISCELLANEOUS;
	m_aClients[ClientId].m_LastVoteMsg = 0;
	m_aClients[ClientId].m_NextVoteSendTick = 0;
	m_aClients[ClientId].m_ShowAccountInfo = false;
	m_aClients[ClientId].m_ShowAccountStats = false;
	m_aClients[ClientId].m_ShowPlotInfo = false;
	m_aClients[ClientId].m_ShowMinigameSettings = false;
	m_aClients[ClientId].m_PrevStats = {};
}

void CVotingMenu::InitPlayer(int ClientId)
{
	(void)ClientId;
}

const char *CVotingMenu::GetPageDescription(int ClientId, int Page)
{
	static char aBuf[VOTE_DESC_LENGTH];
	str_format(aBuf, sizeof(aBuf), "%s %s", Page == GetPage(ClientId) ? "☒" : "☐", m_aPages[Page].m_aName);
	return aBuf;
}

bool CVotingMenu::SetPage(int ClientId, int Page)
{
	if(Page < 0 || Page >= NUM_PAGES || Page == GetPage(ClientId))
		return false;

	m_aClients[ClientId].m_Page = Page;
	if(Page != PAGE_VOTES)
	{
		if(GameServer()->m_apPlayers[ClientId] && GameServer()->m_apPlayers[ClientId]->m_SendVoteIndex != -1)
		{
			CNetMsg_Sv_VoteOptionGroupEnd EndMsg;
			Server()->SendPackMsg(&EndMsg, MSGFLAG_VITAL, ClientId);
			GameServer()->m_apPlayers[ClientId]->m_SendVoteIndex = -1;
		}
	}
	SendPageVotes(ClientId);
	return true;
}

void CVotingMenu::FillVoteOptionMsg(CNetMsg_Sv_VoteOptionListAdd *pMsg, int Index, const char *pDesc)
{
	switch(Index)
	{
	case 0: pMsg->m_pDescription0 = pDesc; break;
	case 1: pMsg->m_pDescription1 = pDesc; break;
	case 2: pMsg->m_pDescription2 = pDesc; break;
	case 3: pMsg->m_pDescription3 = pDesc; break;
	case 4: pMsg->m_pDescription4 = pDesc; break;
	case 5: pMsg->m_pDescription5 = pDesc; break;
	case 6: pMsg->m_pDescription6 = pDesc; break;
	case 7: pMsg->m_pDescription7 = pDesc; break;
	case 8: pMsg->m_pDescription8 = pDesc; break;
	case 9: pMsg->m_pDescription9 = pDesc; break;
	case 10: pMsg->m_pDescription10 = pDesc; break;
	case 11: pMsg->m_pDescription11 = pDesc; break;
	case 12: pMsg->m_pDescription12 = pDesc; break;
	case 13: pMsg->m_pDescription13 = pDesc; break;
	case 14: pMsg->m_pDescription14 = pDesc; break;
	}
}

void CVotingMenu::ProgressVoteOptions(int ClientId)
{
	CPlayer *pPl = GameServer()->m_apPlayers[ClientId];
	if(!pPl || GetPage(ClientId) != PAGE_VOTES)
		return;
	if(pPl->m_SendVoteIndex == -1)
		return;

	const int TotalOptions = NUM_OPTION_START_OFFSET + GameServer()->m_NumVoteOptions;
	if(pPl->m_SendVoteIndex >= TotalOptions)
		return;

	int VotesLeft = TotalOptions - pPl->m_SendVoteIndex;
	int NumVotesToSend = std::min(g_Config.m_SvSendVotesPerTick, VotesLeft);
	if(!VotesLeft)
		return;

	CNetMsg_Sv_VoteOptionListAdd OptionMsg;
	OptionMsg.m_pDescription0 = "";
	OptionMsg.m_pDescription1 = "";
	OptionMsg.m_pDescription2 = "";
	OptionMsg.m_pDescription3 = "";
	OptionMsg.m_pDescription4 = "";
	OptionMsg.m_pDescription5 = "";
	OptionMsg.m_pDescription6 = "";
	OptionMsg.m_pDescription7 = "";
	OptionMsg.m_pDescription8 = "";
	OptionMsg.m_pDescription9 = "";
	OptionMsg.m_pDescription10 = "";
	OptionMsg.m_pDescription11 = "";
	OptionMsg.m_pDescription12 = "";
	OptionMsg.m_pDescription13 = "";
	OptionMsg.m_pDescription14 = "";

	static char aaPageDesc[NUM_OPTION_START_OFFSET][VOTE_DESC_LENGTH];
	int CurIndex = 0;
	int Index = pPl->m_SendVoteIndex;

	const CVoteOptionServer *pCurrent = nullptr;
	if(Index >= NUM_OPTION_START_OFFSET)
	{
		if(Index == NUM_OPTION_START_OFFSET)
			pCurrent = GameServer()->m_pVoteOptionFirst;
		else
			pCurrent = pPl->m_pLastSentVoteOption == nullptr ? nullptr : pPl->m_pLastSentVoteOption->m_pNext;
	}

	while(CurIndex < NumVotesToSend)
	{
		const char *pDesc = PLACEHOLDER_DESC;
		if(Index < NUM_PAGES)
		{
			str_copy(aaPageDesc[Index], GetPageDescription(ClientId, Index), sizeof(aaPageDesc[Index]));
			pDesc = aaPageDesc[Index];
		}
		else if(Index < NUM_OPTION_START_OFFSET)
		{
			pDesc = PLACEHOLDER_DESC;
		}
		else if(pCurrent)
		{
			pDesc = pCurrent->m_aDescription;
			pPl->m_pLastSentVoteOption = pCurrent;
			pCurrent = pCurrent->m_pNext;
		}

		FillVoteOptionMsg(&OptionMsg, CurIndex, pDesc);
		CurIndex++;
		Index++;
	}

	if(pPl->m_SendVoteIndex == 0)
	{
		CNetMsg_Sv_VoteOptionGroupStart StartMsg;
		Server()->SendPackMsg(&StartMsg, MSGFLAG_VITAL, ClientId);
	}

	OptionMsg.m_NumOptions = NumVotesToSend;
	Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientId);
	pPl->m_SendVoteIndex += NumVotesToSend;

	if(pPl->m_SendVoteIndex >= TotalOptions)
	{
		CNetMsg_Sv_VoteOptionGroupEnd EndMsg;
		Server()->SendPackMsg(&EndMsg, MSGFLAG_VITAL, ClientId);
	}
}

bool CVotingMenu::OnMessage(int ClientId, const CNetMsg_Cl_CallVote *pMsg)
{
	if(m_aClients[ClientId].m_LastVoteMsg + Server()->TickSpeed() / 2 > Server()->Tick())
		return true;
	m_aClients[ClientId].m_LastVoteMsg = Server()->Tick();

	if(!str_utf8_check(pMsg->m_pType) || !str_utf8_check(pMsg->m_pValue) || !str_utf8_check(pMsg->m_pReason))
		return true;

	if(str_comp_nocase(pMsg->m_pType, "option") != 0)
		return false;

	int WantedPage = -1;
	for(int i = 0; i < NUM_PAGES; i++)
	{
		if(str_comp(pMsg->m_pValue, GetPageDescription(ClientId, i)) == 0)
		{
			WantedPage = i;
			break;
		}
	}

	if(WantedPage != -1)
	{
		SetPage(ClientId, WantedPage);
		return true;
	}
	if(GetPage(ClientId) == PAGE_VOTES)
	{
		if(!pMsg->m_pValue[0])
			return true;
		return false;
	}

	PrepareTempDescriptions(ClientId);

	const char *pDesc = nullptr;
	for(int i = 0; i < NUM_PAGE_MAX_VOTES; i++)
	{
		const char *pFullDesc = str_skip_voting_menu_prefixes(m_aaTempDesc[i]);
		if(!pFullDesc || !pFullDesc[0])
			continue;
		const char *pValue = str_skip_voting_menu_prefixes(pMsg->m_pValue);
		if(pValue && pValue[0] && str_comp(pFullDesc, pValue) == 0)
		{
			pDesc = pFullDesc;
			break;
		}
	}

	if(!pDesc)
		return true;

	if(OnMessageSuccess(ClientId, pDesc, pMsg->m_pReason))
		SendPageVotes(ClientId);
	return true;
}

bool CVotingMenu::IsOptionWithSuffix(const char *pDesc, const char *pWantedOption)
{
	return str_startswith(pDesc, pWantedOption) != nullptr;
}

bool CVotingMenu::IsOptionPrefix(const char *pDesc, const char *pWantedOption)
{
	return str_startswith(pDesc, pWantedOption) != nullptr;
}

bool CVotingMenu::IsOption(const char *pDesc, const char *pWantedOption)
{
	if(str_comp(pDesc, pWantedOption) == 0)
		return true;
	// First segment of a wrapped vote line is a prefix of the full string
	return str_startswith(pWantedOption, pDesc) != nullptr && str_length(pDesc) > 0 && str_length(pDesc) < str_length(pWantedOption);
}

bool CVotingMenu::OnMessageSuccess(int ClientId, const char *pDesc, const char *pReason)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	CCharacter *pChr = pPlayer ? pPlayer->GetCharacter() : nullptr;
	if(!pPlayer)
		return false;

	int Reason = (pReason && pReason[0] && str_isallnum(pReason)) ? str_toint(pReason) : -1;

	if(GetPage(ClientId) == PAGE_ACCOUNT)
	{
		if(IsOptionWithSuffix(pDesc, COLLAPSE_HEADER_ACC_INFO))
		{
			m_aClients[ClientId].m_ShowAccountInfo = !m_aClients[ClientId].m_ShowAccountInfo;
			return true;
		}
		if(IsOptionWithSuffix(pDesc, COLLAPSE_HEADER_ACC_STATS))
		{
			m_aClients[ClientId].m_ShowAccountStats = !m_aClients[ClientId].m_ShowAccountStats;
			return true;
		}
		if(IsOptionWithSuffix(pDesc, COLLAPSE_HEADER_PLOT_INFO))
		{
			m_aClients[ClientId].m_ShowPlotInfo = !m_aClients[ClientId].m_ShowPlotInfo;
			return true;
		}
		if(IsOption(pDesc, ACC_PLOT_EDIT))
		{
			pPlayer->StartPlotEdit();
			return true;
		}
		if(IsOption(pDesc, ACC_PLOT_CLEAR))
		{
			pPlayer->ClearPlot();
			return true;
		}
		if(IsOption(pDesc, ACC_MISC_NINJAJETPACK))
		{
			pPlayer->m_NinjaJetpack = !pPlayer->m_NinjaJetpack;
			return true;
		}
		if(pChr)
		{
			if(IsOption(pDesc, ACC_VIP_RAINBOW))
			{
				pChr->m_Rainbow = !pChr->m_Rainbow;
				return true;
			}
			if(IsOption(pDesc, ACC_VIP_BLOODY))
			{
				pChr->m_Bloody = !pChr->m_Bloody;
				return true;
			}
			if(IsOption(pDesc, ACC_VIP_ATOM))
			{
				pChr->m_Atom = !pChr->m_Atom;
				return true;
			}
			if(IsOption(pDesc, ACC_VIP_TRAIL))
			{
				pChr->m_Trail = !pChr->m_Trail;
				return true;
			}
			if(IsOption(pDesc, ACC_VIP_PLUS_ROTATINGBALL))
			{
				pChr->m_RotatingBall = !pChr->m_RotatingBall;
				return true;
			}
			if(IsOption(pDesc, ACC_VIP_PLUS_EPICCIRCLE))
			{
				pChr->m_EpicCircle = !pChr->m_EpicCircle;
				return true;
			}
			if(IsOption(pDesc, ACC_VIP_PLUS_LOVELY))
			{
				pChr->m_Lovely = !pChr->m_Lovely;
				return true;
			}
		}
	}
	else if(GetPage(ClientId) == PAGE_MISCELLANEOUS)
	{
		for(int i = -1; i < NUM_MINIGAMES; i++)
		{
			const char *pMinigame = i == -1 ? "No minigame" : GameServer()->GetMinigameName(i);
			if(!IsOption(pDesc, pMinigame))
				continue;
			GameServer()->SetMinigame(ClientId, i);
			return true;
		}

		if(IsOptionWithSuffix(pDesc, COLLAPSE_HEADER_MINIGAME_SETTINGS))
		{
			m_aClients[ClientId].m_ShowMinigameSettings = !m_aClients[ClientId].m_ShowMinigameSettings;
			return true;
		}
		if(IsOption(pDesc, MG_FNG_CREATE_ROOM))
		{
			if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
			{
				CFng *pFng = (CFng *)GameServer()->m_pMinigames[pPlayer->m_Minigame];
				pFng->CreateNewRoom(ClientId);
			}
			return true;
		}
		if(IsOption(pDesc, MG_FNG_LEAVE_ROOM))
		{
			if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
			{
				CFng *pFng = (CFng *)GameServer()->m_pMinigames[pPlayer->m_Minigame];
				pFng->LeaveRoomToLobby(ClientId);
			}
			return true;
		}
		if(IsOptionPrefix(pDesc, MG_FNG_JOIN_ROOM))
		{
			if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
			{
				const char *pColon = str_find(pDesc, ":");
				int TeamNum = -1;
				if(pColon && pColon[1])
					TeamNum = str_toint(pColon + 1);
				else
				{
					const char *pNum = pDesc;
					while(*pNum && !(*pNum >= '0' && *pNum <= '9'))
						pNum++;
					if(*pNum)
						TeamNum = str_toint(pNum);
				}
				if(TeamNum > 0)
				{
					CFng *pFng = (CFng *)GameServer()->m_pMinigames[pPlayer->m_Minigame];
					pFng->OnTeamCommand(ClientId, TeamNum);
				}
			}
			return true;
		}
		if(IsOption(pDesc, MG_FNG_TUNE_RESET))
		{
			if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
			{
				CFng *pFng = (CFng *)GameServer()->m_pMinigames[pPlayer->m_Minigame];
				pFng->TrySetTuneReset(ClientId, !pFng->GetTuneReset(ClientId));
			}
			return true;
		}
		if(IsOption(pDesc, MG_FNG_TEAM_MODE))
		{
			if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
			{
				CFng *pFng = (CFng *)GameServer()->m_pMinigames[pPlayer->m_Minigame];
				pFng->TrySetTeamMode(ClientId, !pFng->GetTeamModeSetting(ClientId));
			}
			return true;
		}
		// 1vs1 before FNG: avoid score-limit prefix clash
		if(IsOptionPrefix(pDesc, MG_1VS1_SCORE_LIMIT) || IsOption(pDesc, MG_1VS1_SCORE_LIMIT))
		{
			if(pPlayer->m_Minigame == MINIGAME_1VS1)
			{
				if(Reason >= 1)
					GameServer()->Arenas()->SetSettingScoreLimit(ClientId, Reason);
				else
					GameServer()->SendChatTarget(ClientId, pPlayer->Localize(MG_1VS1_SCORE_LIMIT_HINT));
			}
			return true;
		}
		if(IsOptionPrefix(pDesc, MG_FNG_SCORE_LIMIT) || IsOption(pDesc, MG_FNG_SCORE_LIMIT))
		{
			if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
			{
				CFng *pFng = (CFng *)GameServer()->m_pMinigames[pPlayer->m_Minigame];
				if(Reason >= 50)
					pFng->TrySetScoreLimit(ClientId, Reason);
				else
					GameServer()->SendChatTarget(ClientId, pPlayer->Localize(MG_FNG_SCORE_LIMIT_HINT));
			}
			return true;
		}
		if(IsOption(pDesc, MG_1VS1_KILL_BORDER))
		{
			if(pPlayer->m_Minigame == MINIGAME_1VS1)
				GameServer()->Arenas()->SetSettingKillBorder(ClientId, !GameServer()->Arenas()->GetSettingKillBorder(ClientId));
			return true;
		}
		if(IsOption(pDesc, MG_1VS1_TUNE_RESET))
		{
			if(pPlayer->m_Minigame == MINIGAME_1VS1)
				GameServer()->Arenas()->SetSettingTuneReset(ClientId, !GameServer()->Arenas()->GetSettingTuneReset(ClientId));
			return true;
		}
		if(IsOption(pDesc, MG_1VS1_HAMMER))
		{
			if(pPlayer->m_Minigame == MINIGAME_1VS1)
				GameServer()->Arenas()->ToggleSettingWeapon(ClientId, WEAPON_HAMMER);
			return true;
		}
		if(IsOption(pDesc, MG_1VS1_SHOTGUN))
		{
			if(pPlayer->m_Minigame == MINIGAME_1VS1)
				GameServer()->Arenas()->ToggleSettingWeapon(ClientId, WEAPON_SHOTGUN);
			return true;
		}
		if(IsOption(pDesc, MG_1VS1_GRENADE))
		{
			if(pPlayer->m_Minigame == MINIGAME_1VS1)
				GameServer()->Arenas()->ToggleSettingWeapon(ClientId, WEAPON_GRENADE);
			return true;
		}
		if(IsOption(pDesc, MG_1VS1_LASER))
		{
			if(pPlayer->m_Minigame == MINIGAME_1VS1)
				GameServer()->Arenas()->ToggleSettingWeapon(ClientId, WEAPON_LASER);
			return true;
		}
	}

	return false;
}

int CVotingMenu::PrepareTempDescriptions(int ClientId)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return 0;

	m_NumCollapseEntries = 0;
	int NumOptions = 0;
	for(auto &Desc : m_aaTempDesc)
		Desc[0] = 0;

	if(pPlayer->m_JailTime)
	{
		char aBuf[VOTE_DESC_LENGTH];
		str_format(aBuf, sizeof(aBuf), "You are arrested for %lld seconds", (long long)(pPlayer->m_JailTime / Server()->TickSpeed()));
		DoLineText(&NumOptions, aBuf, BULLET_POINT);
		DoLineSeperator(&NumOptions);
	}

	if(GetPage(ClientId) == PAGE_ACCOUNT)
		DoPageAccount(ClientId, &NumOptions);
	else if(GetPage(ClientId) == PAGE_MISCELLANEOUS)
		DoPageMiscellaneous(ClientId, &NumOptions);
	else if(GetPage(ClientId) == PAGE_LANGUAGES)
		DoPageLanguages(ClientId, &NumOptions);

	return NumOptions;
}

void CVotingMenu::DoPageAccount(int ClientId, int *pNumOptions)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	CCharacter *pChr = pPlayer->GetCharacter();
	const int AccId = pPlayer->GetAccID();
	SAccountInfo *pAccount = GameServer()->Accounts()->Get(AccId);

	if(AccId < ACC_START || !pAccount)
	{
		DoLineText(pNumOptions, "You're not logged in.");
		DoLineText(pNumOptions, "Use '/login' to see more information about your account.");
		return;
	}

	const bool ShowPortalDate = g_Config.m_SvPortalRifleShop || pAccount->m_PortalRifle;
	if(DoLineCollapse(pNumOptions, COLLAPSE_HEADER_ACC_INFO, m_aClients[ClientId].m_ShowAccountInfo, 5 + (int)ShowPortalDate))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Account Name: %s", pAccount->m_aUsername);
		DoLineText(pNumOptions, aBuf);

		char aDate[64];
		if(pAccount->m_RegisterDate != 0)
		{
			FormatDate(aDate, sizeof(aDate), pAccount->m_RegisterDate);
			str_format(aBuf, sizeof(aBuf), "Registered: %s", aDate);
		}
		else
			str_copy(aBuf, "Registered: before April 9th 2021", sizeof(aBuf));
		DoLineText(pNumOptions, aBuf);

		if(pAccount->m_VIP)
		{
			FormatDate(aDate, sizeof(aDate), pAccount->m_ExpireDateVIP);
			const char *pVipType = pAccount->m_VIP == VIP_ADMIN ? " ADMIN" : pAccount->m_VIP == VIP_PLUS ? "+" : "";
			if(pAccount->m_VIP == VIP_ADMIN)
				str_format(aBuf, sizeof(aBuf), "VIP%s: Forever", pVipType);
			else
				str_format(aBuf, sizeof(aBuf), "VIP%s: until %s", pVipType, aDate);
			DoLineText(pNumOptions, aBuf);
		}
		else
			DoLineText(pNumOptions, "VIP: not bought");

		if(pAccount->m_PortalRifle)
		{
			FormatDate(aDate, sizeof(aDate), pAccount->m_ExpireDatePortalRifle);
			str_format(aBuf, sizeof(aBuf), "Portal Rifle: until %s", aDate);
			DoLineText(pNumOptions, aBuf);
		}
		else if(g_Config.m_SvPortalRifleShop)
			DoLineText(pNumOptions, "Portal Rifle: not bought");

		str_format(aBuf, sizeof(aBuf), "Contact: %s", pAccount->m_aContact);
		DoLineText(pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "E-Mail: %s", pAccount->m_aEmail);
		DoLineText(pNumOptions, aBuf);
		DoLineSeperator(pNumOptions);
	}

	if(DoLineCollapse(pNumOptions, COLLAPSE_HEADER_ACC_STATS, m_aClients[ClientId].m_ShowAccountStats, 10))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Level [%d]", pAccount->m_Level);
		DoLineText(pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "XP [%lld]", (long long)pAccount->m_XP);
		DoLineText(pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "Money [%lld]", (long long)pAccount->m_Money);
		DoLineText(pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "Wallet [%lld]", (long long)pPlayer->GetWalletMoney());
		DoLineText(pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "Police Level [%d]", pAccount->m_PoliceLevel);
		DoLineText(pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "Taser Battery [%d/%d]", pAccount->m_TaserBattery, MAX_TASER_BATTERY);
		DoLineText(pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "Portal Battery [%d]", pAccount->m_PortalBattery);
		DoLineText(pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "Block Points [%d]", pAccount->m_BlockPoints);
		DoLineText(pNumOptions, aBuf);
		str_format(aBuf, sizeof(aBuf), "Kills [%d] / Deaths [%d]", pAccount->m_Kills, pAccount->m_Deaths);
		DoLineText(pNumOptions, aBuf);
		DoLineSeperator(pNumOptions);
	}

	if(pAccount->m_VIP)
	{
		DoLineText(pNumOptions, "─── VIP ───");
		DoLineToggleOption(pNumOptions, ACC_VIP_RAINBOW, pChr && pChr->m_Rainbow);
		DoLineToggleOption(pNumOptions, ACC_VIP_BLOODY, pChr && pChr->m_Bloody);
		DoLineToggleOption(pNumOptions, ACC_VIP_ATOM, pChr && pChr->m_Atom);
		DoLineToggleOption(pNumOptions, ACC_VIP_TRAIL, pChr && pChr->m_Trail);
		if(pAccount->m_VIP == VIP_PLUS || pAccount->m_VIP == VIP_ADMIN)
		{
			DoLineToggleOption(pNumOptions, ACC_VIP_PLUS_ROTATINGBALL, pChr && pChr->m_RotatingBall);
			DoLineToggleOption(pNumOptions, ACC_VIP_PLUS_EPICCIRCLE, pChr && pChr->m_EpicCircle);
			DoLineToggleOption(pNumOptions, ACC_VIP_PLUS_LOVELY, pChr && pChr->m_Lovely);
		}
	}

	if(pAccount->m_Ninjajetpack)
		DoLineToggleOption(pNumOptions, ACC_MISC_NINJAJETPACK, pPlayer->m_NinjaJetpack);

	const int PlotId = GameServer()->GetPlotID(AccId);
	if(PlotId >= CGameContext::PLOT_START)
	{
		char aPlotHeader[32];
		str_format(aPlotHeader, sizeof(aPlotHeader), "%s %d", COLLAPSE_HEADER_PLOT_INFO, PlotId);
		if(DoLineCollapse(pNumOptions, aPlotHeader, m_aClients[ClientId].m_ShowPlotInfo, 3))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Rented until: %s", GameServer()->GetDate(GameServer()->m_aPlots[PlotId].m_ExpireDate));
			DoLineText(pNumOptions, aBuf);
			DoLineText(pNumOptions, ACC_PLOT_EDIT, BULLET_POINT);
			DoLineText(pNumOptions, ACC_PLOT_CLEAR, BULLET_POINT);
		}
	}
}

void CVotingMenu::DoPageMiscellaneous(int ClientId, int *pNumOptions)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	DoLineTextSubheader(pNumOptions, "Mɪɴɪɢᴀᴍᴇs");
	for(int i = -1; i < NUM_MINIGAMES; i++)
	{
		if(i != -1 && GameServer()->m_aMinigameDisabled[i])
			continue;
		bool CurMinigame = i == pPlayer->m_Minigame;
		const char *pMinigame = i == -1 ? "No minigame" : GameServer()->GetMinigameName(i);
		DoLineToggleOption(pNumOptions, pMinigame, CurMinigame);
	}

	if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG || pPlayer->m_Minigame == MINIGAME_1VS1)
	{
		int NumSettings = 0;
		bool FngInRoom = false;
		if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
		{
			CFng *pFng = (CFng *)GameServer()->m_pMinigames[pPlayer->m_Minigame];
			FngInRoom = pFng->GetClientRoom(ClientId) >= 0;
			int NumRooms = 0;
			for(int i = 0; i < MAX_FNG_ROOMS; i++)
				if(pFng->m_aRooms[i].m_Active && pFng->m_aRooms[i].m_MinigameType == pPlayer->m_Minigame)
					NumRooms++;
			NumSettings = FngInRoom ? (1 + 3) : (1 + NumRooms);
		}
		else if(pPlayer->m_Minigame == MINIGAME_1VS1)
		{
			NumSettings = 7;
		}

		if(DoLineCollapse(pNumOptions, COLLAPSE_HEADER_MINIGAME_SETTINGS, m_aClients[ClientId].m_ShowMinigameSettings, NumSettings))
		{
			if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
			{
				CFng *pFng = (CFng *)GameServer()->m_pMinigames[pPlayer->m_Minigame];
				if(FngInRoom)
				{
					DoLineText(pNumOptions, MG_FNG_LEAVE_ROOM, BULLET_ARROW_LEFT);
					DoLineToggleOption(pNumOptions, MG_FNG_TUNE_RESET, pFng->GetTuneReset(ClientId));
					DoLineToggleOption(pNumOptions, MG_FNG_TEAM_MODE, pFng->GetTeamModeSetting(ClientId));
					DoLineValueOption(pNumOptions, MG_FNG_SCORE_LIMIT, pFng->GetScoreLimitSetting(ClientId), -1, BULLET_ARROW);
				}
				else
				{
					DoLineText(pNumOptions, MG_FNG_CREATE_ROOM, BULLET_PLUS);
					for(int i = 0; i < MAX_FNG_ROOMS; i++)
					{
						if(!pFng->m_aRooms[i].m_Active || pFng->m_aRooms[i].m_MinigameType != pPlayer->m_Minigame)
							continue;
						DoLineValueOption(pNumOptions, MG_FNG_JOIN_ROOM, pFng->m_aRooms[i].m_Team, -1, BULLET_ARROW);
					}
				}
			}
			else if(pPlayer->m_Minigame == MINIGAME_1VS1)
			{
				CArenas *pArenas = GameServer()->Arenas();
				DoLineValueOption(pNumOptions, MG_1VS1_SCORE_LIMIT, pArenas->GetSettingScoreLimit(ClientId), -1, BULLET_ARROW);
				DoLineToggleOption(pNumOptions, MG_1VS1_KILL_BORDER, pArenas->GetSettingKillBorder(ClientId));
				DoLineToggleOption(pNumOptions, MG_1VS1_TUNE_RESET, pArenas->GetSettingTuneReset(ClientId));
				DoLineToggleOption(pNumOptions, MG_1VS1_HAMMER, pArenas->GetSettingWeapon(ClientId, WEAPON_HAMMER));
				DoLineToggleOption(pNumOptions, MG_1VS1_SHOTGUN, pArenas->GetSettingWeapon(ClientId, WEAPON_SHOTGUN));
				DoLineToggleOption(pNumOptions, MG_1VS1_GRENADE, pArenas->GetSettingWeapon(ClientId, WEAPON_GRENADE));
				DoLineToggleOption(pNumOptions, MG_1VS1_LASER, pArenas->GetSettingWeapon(ClientId, WEAPON_LASER));
			}
		}
	}
}

void CVotingMenu::DoPageLanguages(int ClientId, int *pNumOptions)
{
	(void)ClientId;
	DoLineText(pNumOptions, "Languages will be ported later.");
}

void CVotingMenu::AddWrappedLine(int *pNumOptions, const char *pDescription, const char *pPrefix, bool CollapseHeader)
{
	const int PrefixLength = str_length(pPrefix);
	const int CollapsePrefixLength = m_NumCollapseEntries > 0 && !CollapseHeader ? 2 : 0;
	const int PrefixSeparatorLength = pPrefix[0] ? 1 : 0;
	const int MaxTextLength = VOTE_DESC_LENGTH - 1 - PrefixLength - PrefixSeparatorLength - CollapsePrefixLength;
	if(MaxTextLength <= 0 || NUM_OPTION_START_OFFSET + *pNumOptions >= NUM_PAGE_MAX_VOTES)
		return;

	char aaSegments[VOTE_DESC_LENGTH][VOTE_DESC_LENGTH];
	int NumSegments = 0;
	const char *pStart = pDescription;
	while(*pStart && NumSegments < VOTE_DESC_LENGTH)
	{
		const int RemainingLength = str_length(pStart);
		int SegmentLength = std::min(RemainingLength, MaxTextLength);

		if(RemainingLength > MaxTextLength)
		{
			int LastSpace = -1;
			for(int i = 0; i < SegmentLength; i++)
			{
				if(pStart[i] == ' ')
					LastSpace = i;
			}
			if(LastSpace > 0)
				SegmentLength = LastSpace;
			else
			{
				while(SegmentLength > 0 && (pStart[SegmentLength] & 0xc0) == 0x80)
					SegmentLength--;
				if(SegmentLength == 0)
					SegmentLength = MaxTextLength;
			}
		}

		mem_copy(aaSegments[NumSegments], pStart, SegmentLength);
		aaSegments[NumSegments][SegmentLength] = '\0';
		NumSegments++;
		pStart += SegmentLength;
		while(*pStart == ' ')
			pStart++;
	}

	if(NumSegments == 0)
	{
		aaSegments[0][0] = '\0';
		NumSegments = 1;
	}

	if(m_NumCollapseEntries > 0 && !CollapseHeader)
		m_NumCollapseEntries += NumSegments - 1;

	for(int Segment = 0; Segment < NumSegments; Segment++)
	{
		if(NUM_OPTION_START_OFFSET + *pNumOptions >= NUM_PAGE_MAX_VOTES)
			return;

		char aLine[VOTE_DESC_LENGTH];
		if(pPrefix[0] && Segment == 0)
			str_format(aLine, sizeof(aLine), "%s %s", pPrefix, aaSegments[Segment]);
		else if(pPrefix[0])
		{
			// Continuation: keep wrap indent, but do not repeat ☐/⇨ so it is not a second vote option
			char aIndent[VOTE_DESC_LENGTH];
			int IndentLen = std::min(PrefixLength + PrefixSeparatorLength, (int)sizeof(aIndent) - 1);
			std::memset(aIndent, ' ', IndentLen);
			aIndent[IndentLen] = '\0';
			str_format(aLine, sizeof(aLine), "%s%s", aIndent, aaSegments[Segment]);
		}
		else
			str_copy(aLine, aaSegments[Segment], sizeof(aLine));

		bool AddCollapseFooter = false;
		if(m_NumCollapseEntries > 0 && !CollapseHeader)
		{
			m_NumCollapseEntries--;
			AddCollapseFooter = m_NumCollapseEntries == 0;
			char aTemp[VOTE_DESC_LENGTH];
			str_format(aTemp, sizeof(aTemp), "│ %s", aLine);
			str_copy(aLine, aTemp, sizeof(aLine));
		}

		str_copy(m_aaTempDesc[NUM_OPTION_START_OFFSET + *pNumOptions], aLine, VOTE_DESC_LENGTH);
		(*pNumOptions)++;
		if(AddCollapseFooter && NUM_OPTION_START_OFFSET + *pNumOptions < NUM_PAGE_MAX_VOTES)
		{
			str_copy(m_aaTempDesc[NUM_OPTION_START_OFFSET + *pNumOptions], "╰───────────────────────", VOTE_DESC_LENGTH);
			(*pNumOptions)++;
		}
	}
}

void CVotingMenu::DoLineToggleOption(int *pNumOptions, const char *pDescription, bool Value)
{
	AddWrappedLine(pNumOptions, pDescription, Value ? "☒" : "☐", false);
}

void CVotingMenu::DoLineSeperator(int *pNumOptions)
{
	AddWrappedLine(pNumOptions, PLACEHOLDER_DESC, "", false);
}

void CVotingMenu::DoLineValueOption(int *pNumOptions, const char *pDescription, int Value, int Max, int BulletPoint)
{
	char aBuf[VOTE_DESC_LENGTH];
	if(Max == -1)
		str_format(aBuf, sizeof(aBuf), "%s: %d", pDescription, Value);
	else
		str_format(aBuf, sizeof(aBuf), "%s: %d/%d", pDescription, Value, Max);

	const char *pPrefix = "";
	if(BulletPoint == BULLET_POINT)
		pPrefix = "• ";
	else if(BulletPoint == BULLET_ARROW)
		pPrefix = "⇨ ";
	else if(BulletPoint == BULLET_ARROW_LEFT)
		pPrefix = "⇦ ";
	else if(BulletPoint == BULLET_PLUS)
		pPrefix = "✚ ";
	AddWrappedLine(pNumOptions, aBuf, pPrefix, false);
}

void CVotingMenu::DoLineText(int *pNumOptions, const char *pDescription, int BulletPoint)
{
	const char *pPrefix = "";
	if(BulletPoint == BULLET_POINT)
		pPrefix = "• ";
	else if(BulletPoint == BULLET_ARROW)
		pPrefix = "⇨ ";
	else if(BulletPoint == BULLET_ARROW_LEFT)
		pPrefix = "⇦ ";
	else if(BulletPoint == BULLET_PLUS)
		pPrefix = "✚ ";
	AddWrappedLine(pNumOptions, pDescription, pPrefix, false);
}

void CVotingMenu::DoLineTextSubheader(int *pNumOptions, const char *pDescription)
{
	char aBuf[VOTE_DESC_LENGTH];
	str_format(aBuf, sizeof(aBuf), "─── %s ───", pDescription);
	AddWrappedLine(pNumOptions, aBuf, "", false);
}

bool CVotingMenu::DoLineCollapse(int *pNumOptions, const char *pDescription, bool ShowContent, int NumEntries)
{
	char aBuf[256];
	const char *pPrefix = ShowContent ? "╭─" : ">";
	const char *pSuffix = ShowContent ? "[‒]" : "[+]";
	int PrefixLength = str_length(pPrefix);
	int DescLength = str_length(pDescription);
	int SuffixLength = str_length(pSuffix);
	int TotalWidth = VOTE_DESC_LENGTH - 1;
	int TotalSpaces = std::clamp(TotalWidth - PrefixLength - DescLength - SuffixLength, 0, 20);
	int SpacesBefore = std::min(13, (int)(TotalSpaces * 0.7f));
	int SpacesAfter = TotalSpaces - SpacesBefore;

	char aSpacesBefore[VOTE_DESC_LENGTH] = {};
	if(SpacesBefore)
		std::memset(aSpacesBefore, ' ', SpacesBefore);
	char aSpacesAfter[VOTE_DESC_LENGTH] = {};
	if(SpacesAfter)
		std::memset(aSpacesAfter, ' ', SpacesAfter);

	if(ShowContent)
		m_NumCollapseEntries = NumEntries;

	str_format(aBuf, sizeof(aBuf), "%s%s%s%s%s", pPrefix, aSpacesBefore, pDescription, aSpacesAfter, pSuffix);
	AddWrappedLine(pNumOptions, aBuf, "", true);
	return ShowContent;
}

void CVotingMenu::Tick()
{
	if(Server()->Tick() % Server()->TickSpeed() != 0)
		return;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];
		if(!pPlayer)
			continue;
		if(m_aClients[i].m_NextVoteSendTick > Server()->Tick())
			continue;
		if(m_aClients[i].m_Page == PAGE_VOTES)
			continue;

		SClientVoteInfo::SPrevStats Stats;
		if(!FillStats(i, &Stats))
			continue;
		if(mem_comp(&Stats, &m_aClients[i].m_PrevStats, sizeof(Stats)) != 0)
			SendPageVotes(i);
	}
}

bool CVotingMenu::FillStats(int ClientId, SClientVoteInfo::SPrevStats *pStats)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientId);
	if(!pPlayer || !pStats)
		return false;

	*pStats = {};
	const int AccId = pPlayer->GetAccID();
	SAccountInfo *pAccount = GameServer()->Accounts()->Get(AccId);
	int Flags = 0;

	if(GetPage(ClientId) == PAGE_MISCELLANEOUS)
	{
		pStats->m_Minigame = pPlayer->m_Minigame;
		if(pPlayer->m_Minigame == MINIGAME_INSTAGIB_FNG || pPlayer->m_Minigame == MINIGAME_INSTAGIB_BOOMFNG)
		{
			CFng *pFng = (CFng *)GameServer()->m_pMinigames[pPlayer->m_Minigame];
			pStats->m_FngScoreLimit = pFng->GetScoreLimitSetting(ClientId);
			if(pFng->GetTuneReset(ClientId))
				pStats->m_FngFlags |= 1;
			if(pFng->GetTeamModeSetting(ClientId))
				pStats->m_FngFlags |= 2;
		}
	}
	else if(GetPage(ClientId) == PAGE_ACCOUNT && pAccount && AccId >= ACC_START)
	{
		if(pChr && pChr->m_Rainbow)
			Flags |= PREVFLAG_ACC_VIP_RAINBOW;
		if(pChr && pChr->m_Bloody)
			Flags |= PREVFLAG_ACC_VIP_BLOODY;
		if(pChr && pChr->m_Atom)
			Flags |= PREVFLAG_ACC_VIP_ATOM;
		if(pChr && pChr->m_Trail)
			Flags |= PREVFLAG_ACC_VIP_TRAIL;
		if(pChr && pChr->m_RotatingBall)
			Flags |= PREVFLAG_ACC_VIP_PLUS_ROTATINGBALL;
		if(pChr && pChr->m_EpicCircle)
			Flags |= PREVFLAG_ACC_VIP_PLUS_EPICCIRCLE;
		if(pChr && pChr->m_Lovely)
			Flags |= PREVFLAG_ACC_VIP_PLUS_LOVELY;
		if(pAccount->m_Ninjajetpack && pPlayer->m_NinjaJetpack)
			Flags |= PREVFLAG_ACC_NINJAJETPACK;

		if(m_aClients[ClientId].m_ShowAccountStats)
		{
			pStats->m_Acc.m_XP = pAccount->m_XP;
			pStats->m_Acc.m_Money = pAccount->m_Money;
			pStats->m_Acc.m_WalletMoney = pPlayer->GetWalletMoney();
			pStats->m_Acc.m_PoliceLevel = pAccount->m_PoliceLevel;
			pStats->m_Acc.m_TaserBattery = pAccount->m_TaserBattery;
			pStats->m_Acc.m_PortalBattery = pAccount->m_PortalBattery;
			pStats->m_Acc.m_Points = pAccount->m_BlockPoints;
			pStats->m_Acc.m_Kills = pAccount->m_Kills;
			pStats->m_Acc.m_Deaths = pAccount->m_Deaths;
			str_copy(pStats->m_Acc.m_aContact, pAccount->m_aContact, sizeof(pStats->m_Acc.m_aContact));
		}
	}

	pStats->m_JailTime = pPlayer->m_JailTime;
	pStats->m_Flags = Flags;
	return true;
}

void CVotingMenu::SendPageVotes(int ClientId, bool ResendVotesPage)
{
	if(!GameServer()->m_apPlayers[ClientId])
		return;

	m_aClients[ClientId].m_NextVoteSendTick = Server()->Tick() + Server()->TickSpeed() * 3;
	if(GetPage(ClientId) == PAGE_VOTES)
	{
		if(ResendVotesPage)
		{
			CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
			Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, ClientId);
			GameServer()->m_apPlayers[ClientId]->m_SendVoteIndex = 0;
			GameServer()->m_apPlayers[ClientId]->m_pLastSentVoteOption = nullptr;
		}
		return;
	}

	SClientVoteInfo::SPrevStats Stats;
	if(FillStats(ClientId, &Stats))
		m_aClients[ClientId].m_PrevStats = Stats;

	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, ClientId);

	const int NumVotesPage = PrepareTempDescriptions(ClientId);
	const int NumVotesToSend = NUM_OPTION_START_OFFSET + NumVotesPage;
	int TotalVotesSent = 0;

	{
		CNetMsg_Sv_VoteOptionGroupStart StartMsg;
		Server()->SendPackMsg(&StartMsg, MSGFLAG_VITAL, ClientId);
	}

	static char aaPageDesc[NUM_OPTION_START_OFFSET][VOTE_DESC_LENGTH];
	while(TotalVotesSent < NumVotesToSend)
	{
		const int VotesLeft = std::min(NumVotesToSend - TotalVotesSent, (int)MAX_VOTES_PER_PACKET);
		CNetMsg_Sv_VoteOptionListAdd OptionMsg;
		OptionMsg.m_pDescription0 = "";
		OptionMsg.m_pDescription1 = "";
		OptionMsg.m_pDescription2 = "";
		OptionMsg.m_pDescription3 = "";
		OptionMsg.m_pDescription4 = "";
		OptionMsg.m_pDescription5 = "";
		OptionMsg.m_pDescription6 = "";
		OptionMsg.m_pDescription7 = "";
		OptionMsg.m_pDescription8 = "";
		OptionMsg.m_pDescription9 = "";
		OptionMsg.m_pDescription10 = "";
		OptionMsg.m_pDescription11 = "";
		OptionMsg.m_pDescription12 = "";
		OptionMsg.m_pDescription13 = "";
		OptionMsg.m_pDescription14 = "";

		for(int CurIndex = 0; CurIndex < VotesLeft; CurIndex++)
		{
			const char *pDesc = PLACEHOLDER_DESC;
			if(TotalVotesSent < NUM_PAGES)
			{
				str_copy(aaPageDesc[TotalVotesSent], GetPageDescription(ClientId, TotalVotesSent), sizeof(aaPageDesc[TotalVotesSent]));
				pDesc = aaPageDesc[TotalVotesSent];
			}
			else if(TotalVotesSent >= NUM_OPTION_START_OFFSET)
				pDesc = m_aaTempDesc[TotalVotesSent];

			FillVoteOptionMsg(&OptionMsg, CurIndex, pDesc);
			TotalVotesSent++;
		}

		OptionMsg.m_NumOptions = VotesLeft;
		Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientId);
	}

	{
		CNetMsg_Sv_VoteOptionGroupEnd EndMsg;
		Server()->SendPackMsg(&EndMsg, MSGFLAG_VITAL, ClientId);
	}
	GameServer()->m_apPlayers[ClientId]->m_SendVoteIndex = -1;
}

void CVotingMenu::ApplyFlags(int ClientId, int Flags)
{
	if(Flags & VOTEFLAG_PAGE_VOTES)
		m_aClients[ClientId].m_Page = PAGE_VOTES;
	if(Flags & VOTEFLAG_PAGE_ACCOUNT)
		m_aClients[ClientId].m_Page = PAGE_ACCOUNT;
	if(Flags & VOTEFLAG_PAGE_MISCELLANEOUS)
		m_aClients[ClientId].m_Page = PAGE_MISCELLANEOUS;
	if(Flags & VOTEFLAG_PAGE_LANGUAGES)
		m_aClients[ClientId].m_Page = PAGE_LANGUAGES;
	if(Flags & VOTEFLAG_SHOW_ACC_INFO)
		m_aClients[ClientId].m_ShowAccountInfo = true;
	if(Flags & VOTEFLAG_SHOW_ACC_STATS)
		m_aClients[ClientId].m_ShowAccountStats = true;
	if(Flags & VOTEFLAG_SHOW_PLOT_INFO)
		m_aClients[ClientId].m_ShowPlotInfo = true;
}

int CVotingMenu::GetFlags(int ClientId)
{
	int Flags = 0;
	int Page = GetPage(ClientId);
	if(Page == PAGE_VOTES)
		Flags |= VOTEFLAG_PAGE_VOTES;
	else if(Page == PAGE_ACCOUNT)
		Flags |= VOTEFLAG_PAGE_ACCOUNT;
	else if(Page == PAGE_MISCELLANEOUS)
		Flags |= VOTEFLAG_PAGE_MISCELLANEOUS;
	else if(Page == PAGE_LANGUAGES)
		Flags |= VOTEFLAG_PAGE_LANGUAGES;
	if(m_aClients[ClientId].m_ShowAccountInfo)
		Flags |= VOTEFLAG_SHOW_ACC_INFO;
	if(m_aClients[ClientId].m_ShowAccountStats)
		Flags |= VOTEFLAG_SHOW_ACC_STATS;
	if(m_aClients[ClientId].m_ShowPlotInfo)
		Flags |= VOTEFLAG_SHOW_PLOT_INFO;
	return Flags;
}

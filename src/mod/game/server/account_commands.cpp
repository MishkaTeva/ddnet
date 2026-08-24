#include "account.h"

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#ifdef CONF_FDDRACE_MOD

void CGameContext::ConRegister(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(ClientId < 0 || !pSelf->m_apPlayers[ClientId])
		return;
	if(pResult->NumArguments() < 3)
	{
		pSelf->SendChatTarget(ClientId, "Usage: /register <name> <password> <password>");
		return;
	}
	if(str_comp(pResult->GetString(1), pResult->GetString(2)) != 0)
	{
		pSelf->SendChatTarget(ClientId, "Passwords do not match");
		return;
	}
	pSelf->Accounts()->Register(ClientId, pResult->GetString(0), pResult->GetString(1));
}

void CGameContext::ConLogin(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(ClientId < 0 || !pSelf->m_apPlayers[ClientId])
		return;
	if(pResult->NumArguments() < 2)
	{
		pSelf->SendChatTarget(ClientId, "Usage: /login <name> <password>");
		return;
	}
	pSelf->Accounts()->Login(ClientId, pResult->GetString(0), pResult->GetString(1));
}

void CGameContext::ConLogout(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(ClientId < 0)
		return;
	pSelf->Accounts()->LogoutClient(ClientId, false);
}

void CGameContext::ConGiveRoomKey(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !pSelf->m_apPlayers[ClientId])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Invalid player");
		return;
	}
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	pPlayer->m_HasRoomKey = true;
	pSelf->Accounts()->SyncMoveRestrictions(pPlayer);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Room key granted");
}

void CGameContext::ConGiveVipPlus(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !pSelf->m_apPlayers[ClientId])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Invalid player");
		return;
	}
	int AccId = pSelf->Accounts()->FindByClientId(ClientId);
	if(AccId < ACC_START)
	{
		// Session-only VIP+ until they register/login.
		if(CCharacter *pChr = pSelf->m_apPlayers[ClientId]->GetCharacter())
			pChr->Core()->m_MoveRestrictionExtra.m_VipPlus = true;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Session VIP+ granted (not logged in)");
		return;
	}
	pSelf->Accounts()->SetVip(AccId, VIP_PLUS);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "VIP+ saved on account");
}

void CGameContext::ConJailArrest(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Accounts()->JailPlayer(pResult->GetVictim(), pResult->GetInteger(1));
}

void CGameContext::ConJailRelease(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Accounts()->ReleaseJail(pResult->GetVictim());
}

void CGameContext::RegisterFddraceAccountCommands()
{
	Console()->Register("register", "s[name] s[password] s[password]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConRegister, this, "Register an account");
	Console()->Register("login", "s[name] s[password]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConLogin, this, "Log into an account");
	Console()->Register("logout", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConLogout, this, "Log out of an account");

	Console()->Register("give_roomkey", "v[id]", CFGFLAG_SERVER, ConGiveRoomKey, this, "Grant room key to player (session)");
	Console()->Register("give_vipplus", "v[id]", CFGFLAG_SERVER, ConGiveVipPlus, this, "Grant VIP+ (account if logged in, else session)");
	Console()->Register("jail_arrest", "v[id] i[seconds]", CFGFLAG_SERVER, ConJailArrest, this, "Arrest player for i seconds");
	Console()->Register("jail_release", "v[id]", CFGFLAG_SERVER, ConJailRelease, this, "Release player from jail");
}

#endif

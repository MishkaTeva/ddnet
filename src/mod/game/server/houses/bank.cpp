#include "bank.h"

#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <mod/game/server/account.h>

CBank::CBank(CGameContext *pGameServer) :
	CHouse(pGameServer, HOUSE_BANK)
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		m_aAssignmentMode[i] = ASSIGNMENT_NONE;
}

const char *CBank::GetWelcomeMessage(int ClientId)
{
	(void)ClientId;
	return "Welcome to the bank, %s! Press F4 to manage your bank account.";
}

const char *CBank::GetConfirmMessage(int ClientId)
{
	static char aBuf[128];
	const int64_t Amount = GetAmount(m_aClients[ClientId].m_Page, ClientId);
	if(m_aAssignmentMode[ClientId] == ASSIGNMENT_DEPOSIT)
		str_format(aBuf, sizeof(aBuf), "Are you sure that you want to deposit %lld money from your wallet to your bank account?", (long long)Amount);
	else if(m_aAssignmentMode[ClientId] == ASSIGNMENT_WITHDRAW)
		str_format(aBuf, sizeof(aBuf), "Are you sure that you want to withdraw %lld money from your bank account to your wallet?", (long long)Amount);
	else
		str_copy(aBuf, "Are you sure?", sizeof(aBuf));
	return aBuf;
}

const char *CBank::GetEndMessage(int ClientId)
{
	if(m_aClients[ClientId].m_State == STATE_CHOSE_ASSIGNMENT)
		return "You can't use the bank without an account. Check '/account'.";
	return "You cancelled the assignment.";
}

bool CBank::NotLoggedIn(int ClientId)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer || pPlayer->GetAccID() < ACC_START)
	{
		EndSession(ClientId, true);
		m_aAssignmentMode[ClientId] = ASSIGNMENT_NONE;
		return true;
	}
	return false;
}

void CBank::OnSuccess(int ClientId)
{
	if(NotLoggedIn(ClientId))
		return;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	SAccountInfo *pAccount = GameServer()->Accounts()->Get(pPlayer->GetAccID());
	const int64_t Amount = GetAmount(m_aClients[ClientId].m_Page, ClientId);
	if(!pAccount || Amount <= 0)
	{
		GameServer()->SendChatTarget(ClientId, "You need to select an amount to deposit or withdraw.");
		return;
	}

	char aMsg[128];
	if(m_aAssignmentMode[ClientId] == ASSIGNMENT_DEPOSIT)
	{
		if(pPlayer->GetWalletMoney() < Amount)
		{
			GameServer()->SendChatTarget(ClientId, "You don't have enough money in your wallet to deposit this amount.");
			return;
		}
		pPlayer->BankTransaction(Amount);
		pPlayer->WalletTransaction(-Amount);
		str_format(aMsg, sizeof(aMsg), "You deposited %lld money from your wallet to your bank account.", (long long)Amount);
		GameServer()->SendChatTarget(ClientId, aMsg);
	}
	else if(m_aAssignmentMode[ClientId] == ASSIGNMENT_WITHDRAW)
	{
		if(pAccount->m_Money < Amount)
		{
			GameServer()->SendChatTarget(ClientId, "You don't have enough money on your bank account to withdraw this amount.");
			return;
		}
		pPlayer->BankTransaction(-Amount);
		pPlayer->WalletTransaction(Amount);
		str_format(aMsg, sizeof(aMsg), "You withdrew %lld money from your bank account to your wallet.", (long long)Amount);
		GameServer()->SendChatTarget(ClientId, aMsg);
	}
}

void CBank::SetAssignment(int ClientId, int Dir)
{
	if(NotLoggedIn(ClientId))
		return;

	if(Dir == -1)
	{
		m_aAssignmentMode[ClientId] = ASSIGNMENT_WITHDRAW;
		SetPage(ClientId, AMOUNT_100);
	}
	else if(Dir == 1)
	{
		m_aAssignmentMode[ClientId] = ASSIGNMENT_DEPOSIT;
		SetPage(ClientId, AMOUNT_EVERYTHING);
	}
}

void CBank::OnPageChange(int ClientId)
{
	char aMsg[512];
	const char *pFooter = "";
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	if(m_aClients[ClientId].m_Page <= PAGE_MAIN)
	{
		m_aAssignmentMode[ClientId] = ASSIGNMENT_NONE;
		if(g_Config.m_SvMoneyBankMode == 0)
			str_copy(aMsg, "Welcome to the bank!\n\nThis feature is currently disabled and your money is instantly saved.", sizeof(aMsg));
		else
			str_copy(aMsg, "Welcome to the bank!\n\nPlease select your option:\nF3: Deposit (+)\nF4: Withdraw (-).\n\nOnce you selected an option, shoot to the right to go one step forward, and shoot left to go one step back.", sizeof(aMsg));
	}
	else
	{
		pFooter = "Press F3 to confirm your assignment.";
		SAccountInfo *pAccount = GameServer()->Accounts()->Get(pPlayer->GetAccID());
		const int64_t BankMoney = pAccount ? pAccount->m_Money : 0;
		const char *pAssignment = m_aAssignmentMode[ClientId] == ASSIGNMENT_DEPOSIT ? "D E P O S I T" :
			m_aAssignmentMode[ClientId] == ASSIGNMENT_WITHDRAW ? "W I T H D R A W" : "";
		str_format(aMsg, sizeof(aMsg), "Bank: %lld\nWallet: %lld\n\n%s\n\n",
			(long long)BankMoney, (long long)pPlayer->GetWalletMoney(), pAssignment);

		char aAmount[64];
		const int Type = m_aClients[ClientId].m_Page;
		if(Type == AMOUNT_EVERYTHING)
			str_format(aAmount, sizeof(aAmount), "Everything (%lld)", (long long)GetAmount(Type, ClientId));
		else
			str_format(aAmount, sizeof(aAmount), "%lld", (long long)GetAmount(Type));

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "- > %s < +", aAmount);
		str_append(aMsg, aBuf, sizeof(aMsg));
	}

	SendWindow(ClientId, aMsg, pFooter);
	m_aClients[ClientId].m_LastMotd = Server()->Tick();
}

int64_t CBank::GetAmount(int Type, int ClientId)
{
	if(Type == AMOUNT_EVERYTHING)
	{
		if(ClientId < 0)
			return 0;
		CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
		if(!pPlayer)
			return 0;
		if(m_aAssignmentMode[ClientId] == ASSIGNMENT_DEPOSIT)
			return pPlayer->GetWalletMoney();
		if(m_aAssignmentMode[ClientId] == ASSIGNMENT_WITHDRAW)
		{
			SAccountInfo *pAccount = GameServer()->Accounts()->Get(pPlayer->GetAccID());
			return pAccount ? pAccount->m_Money : 0;
		}
		return 0;
	}

	switch(Type)
	{
	case AMOUNT_100: return 100;
	case AMOUNT_1K: return 1000;
	case AMOUNT_5K: return 5000;
	case AMOUNT_10K: return 10000;
	case AMOUNT_50K: return 50000;
	case AMOUNT_100K: return 100000;
	case AMOUNT_500K: return 500000;
	case AMOUNT_1MIL: return 1000000;
	case AMOUNT_5MIL: return 5000000;
	case AMOUNT_10MIL: return 10000000;
	case AMOUNT_50MIL: return 50000000;
	case AMOUNT_100MIL: return 100000000;
	default: return 0;
	}
}

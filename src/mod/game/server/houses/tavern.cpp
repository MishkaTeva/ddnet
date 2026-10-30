#include "tavern.h"

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CTavern::CTavern(CGameContext *pGameServer) :
	CHouse(pGameServer, HOUSE_TAVERN)
{
}

const char *CTavern::GetWelcomeMessage(int ClientId)
{
	(void)ClientId;
	return "Welcome to the tavern, %s! Press F4 to buy a grog.";
}

const char *CTavern::GetConfirmMessage(int ClientId)
{
	(void)ClientId;
	return "Are you sure you want to buy a grog?";
}

const char *CTavern::GetEndMessage(int ClientId)
{
	(void)ClientId;
	return "You canceled the purchase.";
}

void CTavern::OnSuccess(int ClientId)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	const int GrogPrice = g_Config.m_SvGrogPrice;
	if(pPlayer->GetUsableMoney() < GrogPrice)
	{
		GameServer()->SendChatTarget(ClientId, "You don't have enough money in your wallet to buy a grog.");
		return;
	}

	CCharacter *pChr = pPlayer->GetCharacter();
	if(pChr && pChr->AddGrog())
	{
		pPlayer->WalletTransaction(-GrogPrice);
		GameServer()->SendChatTarget(ClientId, "You bought one grog");
	}
}

void CTavern::OnPageChange(int ClientId)
{
	const char *pFooter = "Press F3 to confirm your assignment.";
	char aMsg[256];
	str_format(aMsg, sizeof(aMsg), "Welcome to the tavern!\n\nIf you would like to buy a grog for %d money, please press F3.", g_Config.m_SvGrogPrice);
	SendWindow(ClientId, aMsg, pFooter);
	m_aClients[ClientId].m_LastMotd = Server()->Tick();
}

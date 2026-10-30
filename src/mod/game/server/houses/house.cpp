#include "house.h"

#include <engine/server.h>
#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

IServer *CHouse::Server() { return GameServer()->Server(); }

CHouse::CHouse(CGameContext *pGameServer, int Type)
{
	m_pGameServer = pGameServer;
	m_Type = Type;

	switch(m_Type)
	{
	case HOUSE_SHOP: m_pHeadline = "~ S H O P ~"; break;
	case HOUSE_PLOT_SHOP: m_pHeadline = "~ P L O T - S H O P ~"; break;
	case HOUSE_BANK: m_pHeadline = "~ B A N K ~"; break;
	case HOUSE_TAVERN: m_pHeadline = "~ T A V E R N ~"; break;
	default: m_pHeadline = "~ I N V A L I D ~";
	}

	for(int i = 0; i < MAX_CLIENTS; i++)
		Reset(i);
}

void CHouse::Reset(int ClientId)
{
	m_aClients[ClientId].m_Inside = false;
	m_aClients[ClientId].m_LastMotd = 0;
	m_aClients[ClientId].m_NextMsg = Server()->Tick();
	m_aClients[ClientId].m_Page = PAGE_NONE;
	m_aClients[ClientId].m_State = STATE_NONE;
}

void CHouse::SendWindow(int ClientId, const char *pMsg, const char *pFooterMsg, int Page)
{
	char aMsg[900];
	const char *pCut = "*************************************\n";

	char aPage[8] = "";
	if(m_aClients[ClientId].m_Page > PAGE_MAIN && m_Type != HOUSE_BANK && m_Type != HOUSE_TAVERN)
	{
		if(Page == -1)
			Page = m_aClients[ClientId].m_Page;
		str_format(aPage, sizeof(aPage), "~ %d ~", Page);
	}

	char aFooter[128];
	str_format(aFooter, sizeof(aFooter), "%s%s\n                       %s", pCut, pFooterMsg, aPage);

	str_format(aMsg, sizeof(aMsg),
		"%s"
		"                  %s\n"
		"%s\n"
		"%s\n"
		"%s",
		pCut, m_pHeadline, pCut, pMsg, aFooter);

	GameServer()->SendMotd(aMsg, ClientId);
	m_aClients[ClientId].m_LastMotd = Server()->Tick();
}

void CHouse::Tick(int ClientId)
{
	if(m_aClients[ClientId].m_LastMotd && m_aClients[ClientId].m_LastMotd < Server()->Tick() - Server()->TickSpeed() * 10)
	{
		m_aClients[ClientId].m_Page = PAGE_NONE;
		m_aClients[ClientId].m_State = STATE_NONE;
	}

	if(m_aClients[ClientId].m_Inside)
	{
		if(Server()->Tick() % 50 == 0)
			GameServer()->SendBroadcast(m_pHeadline, ClientId, false);
	}
}

void CHouse::OnEnter(int ClientId)
{
	if(m_aClients[ClientId].m_Inside)
		return;

	m_aClients[ClientId].m_Inside = true;

	if(m_aClients[ClientId].m_NextMsg < Server()->Tick())
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), GetWelcomeMessage(ClientId), Server()->ClientName(ClientId));
		GameServer()->SendChatTarget(ClientId, aBuf);
	}
}

void CHouse::OnLeave(int ClientId)
{
	if(!m_aClients[ClientId].m_Inside)
		return;

	m_aClients[ClientId].m_Inside = false;

	if(m_aClients[ClientId].m_NextMsg < Server()->Tick())
	{
		GameServer()->SendChatTarget(ClientId, "Bye! Come back if you need something.");
		m_aClients[ClientId].m_NextMsg = Server()->Tick() + Server()->TickSpeed() * 5;
	}

	if(m_aClients[ClientId].m_Page != PAGE_NONE)
		GameServer()->SendMotd("", ClientId);
	GameServer()->SendBroadcast("", ClientId, false);

	m_aClients[ClientId].m_State = STATE_NONE;
	m_aClients[ClientId].m_Page = PAGE_NONE;
}

void CHouse::OnKeyPress(int ClientId, int Dir)
{
	if(m_Type == HOUSE_BANK && m_aClients[ClientId].m_State == STATE_OPENED_WINDOW)
	{
		if(g_Config.m_SvMoneyBankMode != 0)
		{
			m_aClients[ClientId].m_State = STATE_CHOSE_ASSIGNMENT;
			SetAssignment(ClientId, Dir);
		}
		return;
	}

	if(Dir == 1)
	{
		const int NeedConfirmState = m_Type == HOUSE_BANK ? STATE_CHOSE_ASSIGNMENT : STATE_OPENED_WINDOW;
		if(m_aClients[ClientId].m_State == NeedConfirmState)
		{
			if(m_Type == HOUSE_TAVERN || m_aClients[ClientId].m_Page > PAGE_MAIN)
				ConfirmAssignment(ClientId);
		}
		else if(m_aClients[ClientId].m_State == STATE_CONFIRM)
		{
			EndSession(ClientId, false);
		}
	}
	else if(Dir == -1)
	{
		if(m_aClients[ClientId].m_Page == PAGE_NONE)
		{
			SetPage(ClientId, PAGE_MAIN);
			m_aClients[ClientId].m_State = STATE_OPENED_WINDOW;
		}
		else if(m_aClients[ClientId].m_State == STATE_CONFIRM)
		{
			EndSession(ClientId, true);
		}
	}
}

void CHouse::ConfirmAssignment(int ClientId)
{
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s\n\nF3: yes\nF4: no", GetConfirmMessage(ClientId));
	SendWindow(ClientId, aBuf);
	m_aClients[ClientId].m_State = STATE_CONFIRM;
}

void CHouse::EndSession(int ClientId, bool Cancelled)
{
	if(Cancelled)
		SendWindow(ClientId, GetEndMessage(ClientId));
	else
	{
		OnSuccess(ClientId);
		SetPage(ClientId, PAGE_MAIN);
	}

	m_aClients[ClientId].m_State = STATE_OPENED_WINDOW;
}

bool CHouse::CanChangePage(int ClientId)
{
	return m_aClients[ClientId].m_Page != PAGE_NONE && m_aClients[ClientId].m_State >= STATE_OPENED_WINDOW && m_aClients[ClientId].m_State != STATE_CONFIRM;
}

void CHouse::SetPage(int ClientId, int Page)
{
	m_aClients[ClientId].m_Page = Page;
	OnPageChange(ClientId);
}

void CHouse::DoPageChange(int ClientId, int Dir)
{
	m_aClients[ClientId].m_LastMotd = Server()->Tick();

	if(m_Type != HOUSE_TAVERN && (m_Type != HOUSE_BANK || m_aClients[ClientId].m_Page > PAGE_MAIN))
	{
		do
		{
			if(Dir == PAGE_MAIN)
			{
				m_aClients[ClientId].m_Page = PAGE_MAIN;
			}
			else if(Dir == 1)
			{
				m_aClients[ClientId].m_Page++;
				if(m_aClients[ClientId].m_Page >= NumPages())
					m_aClients[ClientId].m_Page = FirstPage();
			}
			else if(Dir == -1)
			{
				m_aClients[ClientId].m_Page--;
				if(m_aClients[ClientId].m_Page < FirstPage())
					m_aClients[ClientId].m_Page = NumPages() - 1;
			}
		} while(!PageValid(m_aClients[ClientId].m_Page));
	}

	OnPageChange(ClientId);
}

const char *CHouse::GetWelcomeMessage(int ClientId)
{
	(void)ClientId;
	return "Welcome, %s!";
}

const char *CHouse::GetConfirmMessage(int ClientId)
{
	(void)ClientId;
	return "Are you sure?";
}

const char *CHouse::GetEndMessage(int ClientId)
{
	(void)ClientId;
	return "Cancelled.";
}

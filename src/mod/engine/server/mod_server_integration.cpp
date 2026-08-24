#include "mod_server_integration.h"

#include "discord_bridge.h"

#include <base/str.h>

#include <engine/console.h>
#include <engine/server/server.h>
#include <engine/shared/config.h>

void ModServerInit(CServer *pServer)
{
	if(g_Config.m_SvDiscordBridgePort > 0)
	{
		CDiscordBridge *pBridge = new CDiscordBridge();
		if(pBridge->Start(g_Config.m_SvDiscordBridgePort, pServer->GameServer()))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "Discord bridge started on port %d", g_Config.m_SvDiscordBridgePort);
			pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			pServer->SetDiscordBridge(pBridge);
		}
		else
		{
			delete pBridge;
			pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Failed to start Discord bridge");
		}
	}
}

void ModServerShutdown(CServer *pServer)
{
	CDiscordBridge *pBridge = pServer->DiscordBridge();
	if(pBridge)
	{
		pBridge->Stop();
		delete pBridge;
		pServer->SetDiscordBridge(nullptr);
	}
}

void ModServerTick(CServer *pServer)
{
	(void)pServer;
}

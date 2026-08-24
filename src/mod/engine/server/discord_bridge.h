#ifndef ENGINE_SERVER_DISCORD_BRIDGE_H
#define ENGINE_SERVER_DISCORD_BRIDGE_H

#include <base/types.h>

class IGameServer;

class CDiscordBridge
{
	class IGameServer *m_pGameServer;
	NETSOCKET m_Socket;
	int m_Port;
	bool m_Running;
	void *m_pThread;
	
	static void ThreadFunc(void *pUser);
	void Run();
	void HandleRequest(NETSOCKET Socket);
	
public:
	CDiscordBridge();
	~CDiscordBridge();
	
	bool Start(int Port, class IGameServer *pGameServer);
	void Stop();
	bool IsRunning() const { return m_Running; }
};

#endif

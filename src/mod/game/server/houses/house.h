#ifndef MOD_GAME_SERVER_HOUSES_HOUSE_H
#define MOD_GAME_SERVER_HOUSES_HOUSE_H

#include <engine/shared/protocol.h>

enum HouseTypes
{
	HOUSE_SHOP,
	HOUSE_PLOT_SHOP,
	HOUSE_BANK,
	HOUSE_TAVERN,
	NUM_HOUSES
};

enum HouseStates
{
	STATE_NONE = 0,
	STATE_OPENED_WINDOW,
	STATE_CHOSE_ASSIGNMENT,
	STATE_CONFIRM,
};

enum HousePages
{
	PAGE_NONE = -1,
	PAGE_MAIN,
};

class CGameContext;
class IServer;

// Faithful port of F-DDrace CHouse (shop F3/F4 + MOTD pages).
class CHouse
{
protected:
	CGameContext *m_pGameServer;
	CGameContext *GameServer() { return m_pGameServer; }
	IServer *Server();

	int m_Type;
	const char *m_pHeadline;

	struct
	{
		bool m_Inside;
		int m_Page;
		int m_State;
		int64_t m_NextMsg;
		int64_t m_LastMotd;
	} m_aClients[MAX_CLIENTS];

	void SetPage(int ClientId, int Page);
	virtual void SendWindow(int ClientId, const char *pMsg, const char *pFooterMsg = "", int Page = -1);

	virtual int FirstPage() { return PAGE_MAIN; }
	virtual int NumPages() { return 1; }
	virtual bool PageValid(int Page) { return true; }

public:
	CHouse(CGameContext *pGameServer, int Type);
	virtual ~CHouse() {}

	void Tick(int ClientId);
	void Reset(int ClientId);

	bool IsInside(int ClientId) { return m_aClients[ClientId].m_Inside; }
	int GetPage(int ClientId) const { return m_aClients[ClientId].m_Page; }

	void ResetLastMotd(int ClientId) { m_aClients[ClientId].m_LastMotd = 0; }
	void OnEnter(int ClientId);
	void OnLeave(int ClientId);
	bool CanChangePage(int ClientId);
	void DoPageChange(int ClientId, int Dir);
	void OnKeyPress(int ClientId, int Dir);
	void ConfirmAssignment(int ClientId);
	void EndSession(int ClientId, bool Cancelled);

	virtual void OnPageChange(int ClientId) {}
	virtual void OnSuccess(int ClientId) {}
	virtual void SetAssignment(int ClientId, int Dir) { (void)ClientId; (void)Dir; }
	virtual const char *GetWelcomeMessage(int ClientId);
	virtual const char *GetConfirmMessage(int ClientId);
	virtual const char *GetEndMessage(int ClientId);

	bool IsType(int Type) const { return m_Type == Type; }
};

#endif

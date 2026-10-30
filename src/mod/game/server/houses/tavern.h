#ifndef MOD_GAME_SERVER_HOUSES_TAVERN_H
#define MOD_GAME_SERVER_HOUSES_TAVERN_H

#include "house.h"

class CTavern : public CHouse
{
public:
	CTavern(CGameContext *pGameServer);

	void OnPageChange(int ClientId) override;
	void OnSuccess(int ClientId) override;
	const char *GetWelcomeMessage(int ClientId) override;
	const char *GetConfirmMessage(int ClientId) override;
	const char *GetEndMessage(int ClientId) override;
};

#endif

#ifndef MOD_GAME_SERVER_HOUSES_BANK_H
#define MOD_GAME_SERVER_HOUSES_BANK_H

#include "house.h"

enum BankPages
{
	AMOUNT_EVERYTHING = PAGE_MAIN + 1,
	AMOUNT_100,
	AMOUNT_1K,
	AMOUNT_5K,
	AMOUNT_10K,
	AMOUNT_50K,
	AMOUNT_100K,
	AMOUNT_500K,
	AMOUNT_1MIL,
	AMOUNT_5MIL,
	AMOUNT_10MIL,
	AMOUNT_50MIL,
	AMOUNT_100MIL,
	NUM_PAGES_BANK
};

enum BankAssignment
{
	ASSIGNMENT_NONE,
	ASSIGNMENT_DEPOSIT,
	ASSIGNMENT_WITHDRAW,
};

class CBank : public CHouse
{
	int64_t GetAmount(int Type, int ClientId = -1);
	int m_aAssignmentMode[MAX_CLIENTS];
	bool NotLoggedIn(int ClientId);

	int FirstPage() override { return AMOUNT_EVERYTHING; }
	int NumPages() override { return NUM_PAGES_BANK; }

public:
	CBank(CGameContext *pGameServer);

	void OnPageChange(int ClientId) override;
	void OnSuccess(int ClientId) override;
	const char *GetWelcomeMessage(int ClientId) override;
	const char *GetConfirmMessage(int ClientId) override;
	const char *GetEndMessage(int ClientId) override;
	void SetAssignment(int ClientId, int Dir) override;
};

#endif

#ifndef MOD_GAME_SERVER_HOUSES_SHOP_H
#define MOD_GAME_SERVER_HOUSES_SHOP_H

#include "house.h"

#include <game/mapitems_mod.h>

enum
{
	NUM_TASER_LEVELS = 10,
	NUM_POLICE_LEVELS = 5,
	MAX_TASER_BATTERY = 100,
};

enum ShopItems
{
	ITEM_RAINBOW = PAGE_MAIN + 1,
	ITEM_BLOODY,
	ITEM_POLICE,
	ITEM_SPOOKY_GHOST,
	ITEM_ROOM_KEY,
	ITEM_VIP,
	ITEM_VIP_PLUS,
	ITEM_SPAWN_SHOTGUN,
	ITEM_SPAWN_GRENADE,
	ITEM_SPAWN_RIFLE,
	ITEM_NINJAJETPACK,
	ITEM_TASER,
	ITEM_TASER_BATTERY,
	ITEM_PORTAL_RIFLE,
	ITEM_PORTAL_BLOCKER,
	NUM_ITEMS_SHOP_LIST,

	POLICE_RANK_1 = NUM_ITEMS_SHOP_LIST,
	POLICE_RANK_2,
	POLICE_RANK_3,
	POLICE_RANK_4,
	POLICE_RANK_5,

	TASER_LEVEL_1,
	TASER_LEVEL_2,
	TASER_LEVEL_3,
	TASER_LEVEL_4,
	TASER_LEVEL_5,
	TASER_LEVEL_6,
	TASER_LEVEL_7,
	TASER_LEVEL_8,
	TASER_LEVEL_9,
	TASER_LEVEL_10,
	NUM_ITEMS_SHOP
};

enum ShopTime
{
	TIME_DEATH,
	TIME_DISCONNECT,
	TIME_FOREVER,
	TIME_30_DAYS,
	TIME_20_DAYS,
	TIME_7_DAYS,
	TIME_5_DAYS,
};

enum ShopExpire
{
	ITEM_EXPIRE_VIP = 30,
	ITEM_EXPIRE_VIP_PLUS = 20,
	ITEM_EXPIRE_PORTAL_RIFLE = 20,
	ITEM_EXPIRE_PLOT_SMALL = 7,
	ITEM_EXPIRE_PLOT_BIG = 5,
};

class CShop : public CHouse
{
	struct ItemInfo
	{
		bool m_Used;
		const char *m_pName;
		int m_Level;
		float m_Price;
		int m_Time;
		const char *m_pDescription;
		int m_Amount;
		bool m_IsEuro;
	} m_aItems[MAX_PLOTS];

	void AddItem(const char *pName, int Level, float Price, int Time, const char *pDescription, bool IsEuro = false, int Amount = 1);
	void BuyItem(int ClientId, int Item);

	const char *GetHeadline(int Item);
	const char *GetTimeMessage(int Time);

	int NumPages() override { return m_NumItemsList; }
	bool PageValid(int Page) override;
	int m_aBackgroundItem[MAX_CLIENTS];

	int m_NumItems;
	int m_NumItemsList;

public:
	CShop(CGameContext *pGameServer, int Type);

	const char *GetItemName(int Item) { return m_aItems[Item].m_pName; }

	void OnPageChange(int ClientId) override;
	void OnSuccess(int ClientId) override;
	const char *GetWelcomeMessage(int ClientId) override;
	const char *GetConfirmMessage(int ClientId) override;
	const char *GetEndMessage(int ClientId) override;
};

#endif

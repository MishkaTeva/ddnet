#include "shop.h"

#include <algorithm>
#include <ctime>

#include <base/str.h>

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/weapons_ex.h>

#include <mod/game/server/account.h>

static void SetExpireDateDays(int64_t *pDate, float Days)
{
	time_t Now = time(nullptr);
	struct tm ExpireDate = *localtime(&Now);
	time_t Date = (time_t)*pDate;
	if(Date != 0)
	{
		struct tm AccDate = *localtime(&Date);
		ExpireDate.tm_year = AccDate.tm_year;
		ExpireDate.tm_mon = AccDate.tm_mon;
		ExpireDate.tm_mday = AccDate.tm_mday;
		ExpireDate.tm_hour = AccDate.tm_hour;
	}
	const time_t OneHour = 60 * 60;
	time_t DateSeconds = mktime(&ExpireDate) + (time_t)(Days * 24.f * OneHour);
	ExpireDate = *localtime(&DateSeconds);
	ExpireDate.tm_min = 0;
	ExpireDate.tm_sec = 0;
	*pDate = (int64_t)mktime(&ExpireDate);
}

CShop::CShop(CGameContext *pGameServer, int Type) :
	CHouse(pGameServer, Type)
{
	for(auto &Item : m_aItems)
		Item.m_Used = false;
	m_aItems[PAGE_MAIN].m_Used = true;

	if(IsType(HOUSE_SHOP))
	{
		m_NumItems = NUM_ITEMS_SHOP;
		m_NumItemsList = NUM_ITEMS_SHOP_LIST;

		AddItem("Rainbow", 5, 1500, TIME_DEATH, "Rainbow will make your tee change the color very fast.");
		AddItem("Bloody", 15, 3500, TIME_DEATH, "Bloody will give your tee a permanent kill effect.");
		AddItem("Police", -1, 100000, TIME_FOREVER, "Police officers get help from the police bot. For more information about the specific police ranks, please say '/police'.");
		AddItem("Spooky Ghost", 1, 1000000, TIME_FOREVER, "Using this item you can hide from other players behind bushes.");
		AddItem("Room Key", 16, 5000, TIME_DISCONNECT, "If you have the room key you can enter the room.");
		AddItem("VIP Classic", 1, 500000, TIME_30_DAYS, "VIP Classic gives you some benefits, check '/vip'.");
		AddItem("VIP+", 1, 1000000, TIME_20_DAYS, "VIP+ gives you even more benefits than VIP Classic, check '/vip'.");
		AddItem("Spawn Shotgun", 33, 600000, TIME_FOREVER, "You will have shotgun if you respawn.");
		AddItem("Spawn Grenade", 33, 600000, TIME_FOREVER, "You will have grenade if you respawn.");
		AddItem("Spawn Rifle", 33, 600000, TIME_FOREVER, "You will have rifle if you respawn.");
		AddItem("Ninjajetpack", 21, 10000, TIME_FOREVER, "It will make your jetpack gun be a ninja. Toggle it using '/ninjajetpack'.");
		AddItem("Taser", 30, -1, TIME_FOREVER, "Taser is a rifle that freezes a player. For more information visit '/taser'.");
		AddItem("Taser battery", 30, 100000, TIME_FOREVER, "Taser battery is required to use the taser. Maximum amount of ammo is 100.", false, 10);
		AddItem("Portal Rifle", 45, 500000, TIME_20_DAYS, "With Portal Rifle you can create two portals where your cursor is, then teleport between them.");
		AddItem("Portal Blocker", 20, 10000, TIME_FOREVER, "Create portal blockers with hammer and this ammo.", false, 10);

		static char aaPolice[NUM_POLICE_LEVELS][32];
		for(int i = 0; i < NUM_POLICE_LEVELS; i++)
		{
			str_format(aaPolice[i], sizeof(aaPolice[i]), "Police Rank %d", i + 1);
			AddItem(aaPolice[i], GameServer()->m_aPoliceLevel[i], m_aItems[ITEM_POLICE].m_Price, m_aItems[ITEM_POLICE].m_Time, m_aItems[ITEM_POLICE].m_pDescription);
		}

		static char aaTaser[NUM_TASER_LEVELS][32];
		for(int i = 0; i < NUM_TASER_LEVELS; i++)
		{
			str_format(aaTaser[i], sizeof(aaTaser[i]), "Taser Level %d", i + 1);
			AddItem(aaTaser[i], m_aItems[ITEM_TASER].m_Level, GameServer()->m_aTaserPrice[i], m_aItems[ITEM_TASER].m_Time, m_aItems[ITEM_TASER].m_pDescription);
		}
	}
	else if(IsType(HOUSE_PLOT_SHOP))
	{
		const int NumPlots = GameServer()->Collision()->NumPlots();
		m_NumItems = m_NumItemsList = NumPlots + 1;
		static char aaName[MAX_PLOTS][32];
		for(int i = CGameContext::PLOT_START; i < m_NumItems && i < MAX_PLOTS; i++)
		{
			const int Size = GameServer()->m_aPlots[i].m_Size;
			str_format(aaName[i], sizeof(aaName[i]), "Plot %d", i);
			const int Level = (Size + 1) * 20;
			const int Price = (Size + 1) * 50000;
			const int Time = Size == PLOT_SMALL ? TIME_7_DAYS : Size == PLOT_BIG ? TIME_5_DAYS : -1;
			AddItem(aaName[i], Level, Price, Time, "");
		}
	}
}

void CShop::AddItem(const char *pName, int Level, float Price, int Time, const char *pDescription, bool IsEuro, int Amount)
{
	for(int i = 0; i < m_NumItems; i++)
	{
		if(m_aItems[i].m_Used)
			continue;
		m_aItems[i].m_pName = pName;
		m_aItems[i].m_Level = Level;
		m_aItems[i].m_Price = Price;
		m_aItems[i].m_Time = Time;
		m_aItems[i].m_pDescription = pDescription;
		m_aItems[i].m_IsEuro = IsEuro;
		m_aItems[i].m_Amount = Amount;
		m_aItems[i].m_Used = true;
		break;
	}
}

bool CShop::PageValid(int Page)
{
	return !IsType(HOUSE_SHOP) || (Page != ITEM_PORTAL_RIFLE || g_Config.m_SvPortalRifleShop);
}

const char *CShop::GetWelcomeMessage(int ClientId)
{
	(void)ClientId;
	return "Welcome to the shop, %s! Press F4 to start shopping.";
}

const char *CShop::GetConfirmMessage(int ClientId)
{
	(void)ClientId;
	return "Are you sure you want to buy this item?";
}

const char *CShop::GetEndMessage(int ClientId)
{
	(void)ClientId;
	return "You canceled the purchase.";
}

const char *CShop::GetHeadline(int Item)
{
	if(Item <= 0 || Item >= m_NumItems)
		return "Unknown";

	static char aRet[64];
	aRet[0] = 0;
	int s = 0;
	int NameLength = str_length(m_aItems[Item].m_pName) + 1;
	for(int i = 0; i < NameLength; i++)
	{
		aRet[s] = str_uppercase(m_aItems[Item].m_pName[i]);
		if(i == NameLength - 2)
			break;
		str_append(aRet, " ", sizeof(aRet));
		s += 2;
	}
	return aRet;
}

const char *CShop::GetTimeMessage(int Time)
{
	switch(Time)
	{
	case TIME_DEATH: return "You own this item until you die.";
	case TIME_DISCONNECT: return "You own this item until\nyou disconnect.";
	case TIME_FOREVER: return "You own this item forever.";
	case TIME_30_DAYS: return "You own this item for 30 days.";
	case TIME_20_DAYS: return "You own this item for 20 days.";
	case TIME_7_DAYS: return "You own this item for 7 days.";
	case TIME_5_DAYS: return "You own this item for 5 days.";
	}
	return "Unknown";
}

void CShop::OnSuccess(int ClientId)
{
	BuyItem(ClientId, m_aClients[ClientId].m_Page);
}

void CShop::OnPageChange(int ClientId)
{
	m_aBackgroundItem[ClientId] = m_aClients[ClientId].m_Page;

	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	if(IsType(HOUSE_SHOP))
	{
		SAccountInfo *pAccount = GameServer()->Accounts()->Get(pPlayer->GetAccID());
		if(pAccount)
		{
			if(m_aClients[ClientId].m_Page == ITEM_POLICE)
				m_aBackgroundItem[ClientId] = std::clamp(POLICE_RANK_1 + pAccount->m_PoliceLevel, (int)POLICE_RANK_1, (int)POLICE_RANK_5);
			else if(m_aClients[ClientId].m_Page == ITEM_TASER)
				m_aBackgroundItem[ClientId] = std::clamp(TASER_LEVEL_1 + pAccount->m_TaserLevel, (int)TASER_LEVEL_1, (int)TASER_LEVEL_10);
		}
	}

	int Item = m_aBackgroundItem[ClientId];
	char aMsg[512];
	if(m_aClients[ClientId].m_Page <= PAGE_MAIN)
	{
		if(IsType(HOUSE_PLOT_SHOP))
			str_copy(aMsg, "Welcome to the plot shop!\n\nBy shooting to the right you go one site forward, and by shooting left you go one site back.", sizeof(aMsg));
		else
			str_copy(aMsg, "Welcome to the shop!\n\nBy shooting to the right you go one site forward, and by shooting left you go one site back.", sizeof(aMsg));
	}
	else
	{
		char aDescription[256];
		if(IsType(HOUSE_PLOT_SHOP))
		{
			char aOwner[32];
			char aRented[64];
			const bool Owned = GameServer()->m_aPlots[Item].m_aOwner[0] != 0;
			if(Owned)
			{
				str_format(aOwner, sizeof(aOwner), "'%s'", GameServer()->m_aPlots[Item].m_aDisplayName);
				str_format(aRented, sizeof(aRented), "Rented until: %s", GameServer()->GetDate(GameServer()->m_aPlots[Item].m_ExpireDate));
			}
			else
			{
				str_copy(aOwner, "for sale", sizeof(aOwner));
				aRented[0] = 0;
			}
			str_format(aDescription, sizeof(aDescription),
				"Size: %s\n"
				"Max. objects: %u\n"
				"Owner: %s\n"
				"%s",
				GameServer()->GetPlotSizeString(Item),
				GameServer()->GetMaxPlotObjects(Item),
				aOwner, aRented);
		}
		else
			str_copy(aDescription, m_aItems[Item].m_pDescription, sizeof(aDescription));

		char aAmount[64] = "\n";
		if(IsType(HOUSE_SHOP) && m_aItems[Item].m_Amount > 1)
			str_format(aAmount, sizeof(aAmount), "Amount: %d\n\n", m_aItems[Item].m_Amount);

		char aPrice[16];
		str_format(aPrice, sizeof(aPrice), "%d", (int)m_aItems[Item].m_Price);

		str_format(aMsg, sizeof(aMsg),
			"%s\n\n"
			"Level: %d\n"
			"Price: %s money\n"
			"Time: %s\n"
			"%s"
			"%s\n\n"
			"F3: buy  F4: back",
			GetHeadline(Item),
			m_aItems[Item].m_Level,
			aPrice,
			GetTimeMessage(m_aItems[Item].m_Time),
			aAmount,
			aDescription);
	}

	char aFooter[64];
	str_format(aFooter, sizeof(aFooter), "F3: %s   F4: %s", m_aClients[ClientId].m_Page > PAGE_MAIN ? "buy" : "open", m_aClients[ClientId].m_Page > PAGE_MAIN ? "back" : "close");
	SendWindow(ClientId, aMsg, aFooter);
}

void CShop::BuyItem(int ClientId, int Item)
{
	CPlayer *pPlayer = GameServer()->m_apPlayers[ClientId];
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientId);
	if(!pPlayer || !pChr)
		return;

	const int AccId = pPlayer->GetAccID();
	SAccountInfo *pAccount = GameServer()->Accounts()->Get(AccId);
	if(!pAccount || AccId < ACC_START)
	{
		GameServer()->SendChatTarget(ClientId, "You are not logged in");
		return;
	}

	int ItemID = Item;
	int Amount = m_aItems[ItemID].m_Amount;
	char aMsg[256];
	char aDescription[64];
	str_copy(aDescription, m_aItems[ItemID].m_pName, sizeof(aDescription));

	if(IsType(HOUSE_PLOT_SHOP))
	{
		const int OwnPlotId = GameServer()->GetPlotID(AccId);
		if(GameServer()->m_aPlots[Item].m_aOwner[0] != 0)
		{
			GameServer()->SendChatTarget(ClientId, "This plot is already sold to someone else");
			return;
		}
		if(OwnPlotId == Item)
		{
			GameServer()->SendChatTarget(ClientId, "You already own that plot");
			return;
		}
		if(OwnPlotId != 0)
		{
			GameServer()->SendChatTarget(ClientId, "You already own another plot");
			return;
		}
		if(GameServer()->HasPlotByIP(ClientId))
		{
			GameServer()->SendChatTarget(ClientId, "Your IP address already owns one plot");
			return;
		}
	}
	else
	{
		if(Item == ITEM_POLICE || Item == ITEM_TASER)
			ItemID = m_aBackgroundItem[ClientId];

		if((Item == ITEM_RAINBOW && pChr->m_Rainbow)
			|| (Item == ITEM_BLOODY && pChr->m_Bloody)
			|| (Item == ITEM_POLICE && pAccount->m_PoliceLevel >= NUM_POLICE_LEVELS)
			|| (Item == ITEM_SPOOKY_GHOST && pAccount->m_SpookyGhost)
			|| (Item == ITEM_ROOM_KEY && pPlayer->m_HasRoomKey)
			|| (Item == ITEM_SPAWN_SHOTGUN && pAccount->m_aSpawnWeapon[0] >= 5)
			|| (Item == ITEM_SPAWN_GRENADE && pAccount->m_aSpawnWeapon[1] >= 5)
			|| (Item == ITEM_SPAWN_RIFLE && pAccount->m_aSpawnWeapon[2] >= 5)
			|| (Item == ITEM_NINJAJETPACK && pAccount->m_Ninjajetpack)
			|| (Item == ITEM_TASER && pAccount->m_TaserLevel >= NUM_TASER_LEVELS)
			|| (Item == ITEM_TASER_BATTERY && pAccount->m_TaserBattery >= MAX_TASER_BATTERY))
		{
			switch(Item)
			{
			case ITEM_POLICE: GameServer()->SendChatTarget(ClientId, "You already have the highest police rank"); break;
			case ITEM_SPAWN_SHOTGUN:
			case ITEM_SPAWN_GRENADE:
			case ITEM_SPAWN_RIFLE: GameServer()->SendChatTarget(ClientId, "You already have the maximum amount of bullets"); break;
			case ITEM_TASER: GameServer()->SendChatTarget(ClientId, "You already have the maximum taser level"); break;
			case ITEM_TASER_BATTERY: GameServer()->SendChatTarget(ClientId, "You already have a fully filled taser battery"); break;
			default:
				str_format(aMsg, sizeof(aMsg), "You already have %s", m_aItems[ItemID].m_pName);
				GameServer()->SendChatTarget(ClientId, aMsg);
			}
			return;
		}

		if((Item == ITEM_TASER || Item == ITEM_TASER_BATTERY) && pAccount->m_PoliceLevel < 3)
		{
			GameServer()->SendChatTarget(ClientId, "You need to be police level 3 or higher to get a taser license");
			return;
		}

		if(pAccount->m_VIP && ((Item == ITEM_VIP && pAccount->m_VIP != VIP_CLASSIC) || (Item == ITEM_VIP_PLUS && pAccount->m_VIP != VIP_PLUS)))
		{
			GameServer()->SendChatTarget(ClientId, "You can not buy this VIP level while owning another one");
			return;
		}

		if(Item == ITEM_TASER_BATTERY)
			Amount = std::clamp(MAX_TASER_BATTERY - pAccount->m_TaserBattery, 0, Amount);

		if(Item == ITEM_TASER_BATTERY || Item == ITEM_PORTAL_BLOCKER)
			str_format(aDescription, sizeof(aDescription), "%d %s", Amount, m_aItems[ItemID].m_pName);
	}

	if(Amount <= 0)
		return;

	float Price = Amount * (m_aItems[ItemID].m_Price / (float)std::max(1, m_aItems[ItemID].m_Amount));
	int FinalPrice = (int)Price;

	if(pPlayer->GetUsableMoney() < FinalPrice)
	{
		GameServer()->SendChatTarget(ClientId, "You don't have enough money");
		return;
	}

	if(m_aItems[ItemID].m_Level > 0 && pAccount->m_Level < m_aItems[ItemID].m_Level)
	{
		str_format(aMsg, sizeof(aMsg), "Your level is too low, you need to be level %d to buy %s", m_aItems[ItemID].m_Level, m_aItems[ItemID].m_pName);
		GameServer()->SendChatTarget(ClientId, aMsg);
		return;
	}

	if(IsType(HOUSE_SHOP) && Item == ITEM_TASER_BATTERY)
	{
		if(!pPlayer->GiveTaserBattery(Amount))
		{
			GameServer()->SendChatTarget(ClientId, "Taser battery purchase failed");
			return;
		}
	}

	str_format(aMsg, sizeof(aMsg), "You bought %s %s", aDescription,
		m_aItems[ItemID].m_Time == TIME_DEATH ? "until death" : m_aItems[ItemID].m_Time == TIME_DISCONNECT ? "until disconnect" : "");
	GameServer()->SendChatTarget(ClientId, aMsg);
	if(IsType(HOUSE_SHOP) && (Item == ITEM_VIP || Item == ITEM_VIP_PLUS || Item == ITEM_PORTAL_RIFLE))
		GameServer()->SendChatTarget(ClientId, "Check '/account' for more information about the expiration date");

	pPlayer->WalletTransaction(-FinalPrice);

	if(IsType(HOUSE_PLOT_SHOP))
	{
		GameServer()->SetPlotExpire(Item);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "The plot will expire on %s", GameServer()->GetDate(GameServer()->m_aPlots[Item].m_ExpireDate));
		GameServer()->SendChatTarget(ClientId, aBuf);
		GameServer()->SetPlotInfo(Item, AccId);
		return;
	}

	int Weapon = -1;
	switch(Item)
	{
	case ITEM_RAINBOW: pChr->m_Rainbow = true; break;
	case ITEM_BLOODY: pChr->m_Bloody = true; break;
	case ITEM_POLICE: pAccount->m_PoliceLevel++; break;
	case ITEM_SPOOKY_GHOST: pAccount->m_SpookyGhost = true; break;
	case ITEM_ROOM_KEY:
		pPlayer->m_HasRoomKey = true;
		pChr->Core()->m_MoveRestrictionExtra.m_RoomKey = true;
		break;
	case ITEM_VIP_PLUS:
		pChr->Core()->m_MoveRestrictionExtra.m_VipPlus = true;
		[[fallthrough]];
	case ITEM_VIP:
		pAccount->m_VIP = Item == ITEM_VIP ? VIP_CLASSIC : VIP_PLUS;
		SetExpireDateDays(&pAccount->m_ExpireDateVIP, Item == ITEM_VIP_PLUS ? ITEM_EXPIRE_VIP_PLUS : ITEM_EXPIRE_VIP);
		break;
	case ITEM_SPAWN_SHOTGUN:
		if(Weapon == -1)
			Weapon = 0;
		[[fallthrough]];
	case ITEM_SPAWN_GRENADE:
		if(Weapon == -1)
			Weapon = 1;
		[[fallthrough]];
	case ITEM_SPAWN_RIFLE:
		if(Weapon == -1)
			Weapon = 2;
		pAccount->m_aSpawnWeapon[Weapon]++;
		break;
	case ITEM_NINJAJETPACK: pAccount->m_Ninjajetpack = true; break;
	case ITEM_TASER:
		pAccount->m_TaserLevel++;
		pChr->GiveWeapon(WEAPON_TASER, false);
		break;
	case ITEM_TASER_BATTERY: break;
	case ITEM_PORTAL_RIFLE:
		pAccount->m_PortalRifle = 1;
		SetExpireDateDays(&pAccount->m_ExpireDatePortalRifle, ITEM_EXPIRE_PORTAL_RIFLE);
		pChr->GiveWeapon(WEAPON_PORTAL_RIFLE, false);
		break;
	case ITEM_PORTAL_BLOCKER: pAccount->m_PortalBlocker += Amount; break;
	}

	GameServer()->Accounts()->Persist(AccId);
	GameServer()->Accounts()->ApplyAccountLoadouts(pPlayer);
}

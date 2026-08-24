// F-DDrace map tile / switch indices — appended on top of DDNet mapitems.h.
// DDNet entity enum stays relative (+ ENTITY_OFFSET). Do not redefine DDNet
// TILE_LFREEZE / TILE_LUNFREEZE (144–145) or armor entity slots (abs 226–228).

#ifndef CONF_FDDRACE_MOD
// Mod build disabled — no extra map item constants.
#else

enum
{
	// Game / front (economy, access, minigames) — absolute indices from F-DDrace
	TILE_MONEY_XP_BOMB = 114,
	TILE_BANK = 119,
	TILE_JAIL = 121,
	TILE_JAIL_RELEASE = 122,
	TILE_MONEY = 160,
	TILE_SHOP = 161,
	TILE_ROOM = 162,
	TILE_SPECIAL_FINISH = 163,
	TILE_MONEY_POLICE = 164,
	TILE_MONEY_EXTRA = 165,
	TILE_PLOT_SHOP = 168,
	TILE_TAVERN = 169,
	TILE_NO_BONUS_AREA = 170,
	TILE_NO_BONUS_AREA_LEAVE = 171,
	TILE_VIP_PLUS_ONLY = 172,
	TILE_HELPERS_ONLY = 173,
	TILE_MODERATORS_ONLY = 174,
	TILE_ADMINS_ONLY = 175,
	TILE_MINIGAME_BLOCK = 176,
	TILE_SURVIVAL_LOBBY = 177,
	TILE_SURVIVAL_SPAWN = 178,
	TILE_SURVIVAL_DEATHMATCH = 179,
	TILE_DURAK_TABLE = 180,
	TILE_DURAK_SEAT = 181,
	TILE_DURAK_LOBBY = 182,

	// Switch layer — plot system (Phase 3 POC)
	TILE_SWITCH_PLOT = 192,
	TILE_SWITCH_PLOT_DOOR = 193,
	TILE_SWITCH_PLOT_TOTELE = 194,
	TILE_SWITCH_REDIRECT_SERVER_FROM = 195,
	TILE_SWITCH_REDIRECT_SERVER_TO = 196,

	NUM_FDDRACE_INDICES = 256,
	MAX_PLOTS = NUM_FDDRACE_INDICES - 1,

	PLOT_SMALL = 0,
	PLOT_BIG = 1,
	NUM_PLOT_SIZES = 2,
};

inline bool IsFddraceModGameTile(int Index)
{
	return Index == TILE_MONEY_XP_BOMB ||
	       Index == TILE_BANK ||
	       Index == TILE_JAIL ||
	       Index == TILE_JAIL_RELEASE ||
	       (Index >= TILE_MONEY && Index <= TILE_MONEY_EXTRA) ||
	       Index == TILE_PLOT_SHOP ||
	       Index == TILE_TAVERN ||
	       Index == TILE_NO_BONUS_AREA ||
	       Index == TILE_NO_BONUS_AREA_LEAVE ||
	       (Index >= TILE_VIP_PLUS_ONLY && Index <= TILE_ADMINS_ONLY) ||
	       (Index >= TILE_MINIGAME_BLOCK && Index <= TILE_SURVIVAL_DEATHMATCH) ||
	       (Index >= TILE_DURAK_TABLE && Index <= TILE_DURAK_LOBBY);
}

#endif

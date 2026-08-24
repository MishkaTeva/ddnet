// F-DDrace move-restriction extras — included from collision.h under CONF_FDDRACE_MOD.

#ifndef GAME_COLLISION_MOD_H
#define GAME_COLLISION_MOD_H

#ifndef CONF_FDDRACE_MOD
// Mod build disabled.
#else

enum
{
	CANTMOVE_ROOM = 1 << 12,
	CANTMOVE_VIP_PLUS_ONLY = 1 << 13,
	CANTMOVE_PLOT_DOOR = 1 << 14,
	CANTMOVE_DOWN_LASERDOOR = 1 << 15,
};

struct MoveRestrictionExtra
{
	bool m_RoomKey = false;
	bool m_VipPlus = false;
};

#endif

#endif // GAME_COLLISION_MOD_H

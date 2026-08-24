#include "account.h"

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/mapitems.h>

#include <mod/game/server/entities/atom.h>
#include <mod/game/server/entities/ban_plunger_toilet.h>
#include <mod/game/server/entities/clock.h>
#include <mod/game/server/entities/custom_projectile.h>
#include <mod/game/server/entities/epic_circle.h>
#include <mod/game/server/entities/kick_boot.h>
#include <mod/game/server/entities/lightsaber.h>
#include <mod/game/server/entities/lovely.h>
#include <mod/game/server/entities/meteor.h>
#include <mod/game/server/entities/mute_gag.h>
#include <mod/game/server/entities/pickup_drop.h>
#include <mod/game/server/entities/playercounter.h>
#include <mod/game/server/entities/portal.h>
#include <mod/game/server/entities/rotating_ball.h>
#include <mod/game/server/entities/staff_ind.h>
#include <mod/game/server/entities/stable_projectile.h>
#include <mod/game/server/entities/teleporter.h>
#include <mod/game/server/entities/trail.h>
#include <mod/game/server/entities/unmute_spark.h>

#ifdef CONF_FDDRACE_MOD

void CGameContext::ConRegister(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(ClientId < 0 || !pSelf->m_apPlayers[ClientId])
		return;
	if(pResult->NumArguments() < 3)
	{
		pSelf->SendChatTarget(ClientId, "Usage: /register <name> <password> <password>");
		return;
	}
	if(str_comp(pResult->GetString(1), pResult->GetString(2)) != 0)
	{
		pSelf->SendChatTarget(ClientId, "Passwords do not match");
		return;
	}
	pSelf->Accounts()->Register(ClientId, pResult->GetString(0), pResult->GetString(1));
}

void CGameContext::ConLogin(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(ClientId < 0 || !pSelf->m_apPlayers[ClientId])
		return;
	if(pResult->NumArguments() < 2)
	{
		pSelf->SendChatTarget(ClientId, "Usage: /login <name> <password>");
		return;
	}
	pSelf->Accounts()->Login(ClientId, pResult->GetString(0), pResult->GetString(1));
}

void CGameContext::ConLogout(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(ClientId < 0)
		return;
	pSelf->Accounts()->LogoutClient(ClientId, false);
}

void CGameContext::ConGiveRoomKey(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !pSelf->m_apPlayers[ClientId])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Invalid player");
		return;
	}
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	pPlayer->m_HasRoomKey = true;
	pSelf->Accounts()->SyncMoveRestrictions(pPlayer);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Room key granted");
}

void CGameContext::ConGiveVipPlus(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !pSelf->m_apPlayers[ClientId])
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Invalid player");
		return;
	}
	int AccId = pSelf->Accounts()->FindByClientId(ClientId);
	if(AccId < ACC_START)
	{
		// Session-only VIP+ until they register/login.
		if(CCharacter *pChr = pSelf->m_apPlayers[ClientId]->GetCharacter())
			pChr->Core()->m_MoveRestrictionExtra.m_VipPlus = true;
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Session VIP+ granted (not logged in)");
		return;
	}
	pSelf->Accounts()->SetVip(AccId, VIP_PLUS);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "VIP+ saved on account");
}

void CGameContext::ConJailArrest(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Accounts()->JailPlayer(pResult->GetVictim(), pResult->GetInteger(1));
}

void CGameContext::ConJailRelease(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Accounts()->ReleaseJail(pResult->GetVictim());
}

void CGameContext::ConLaserText(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer || !pPlayer->GetCharacter())
		return;
	pSelf->CreateLaserText(pPlayer->GetCharacter()->GetPos(), ClientId, pResult->GetString(1), 3);
}

void CGameContext::ConDropMoney(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer || !pPlayer->GetCharacter())
		return;
	const int64_t Amount = pResult->GetInteger(1);
	pSelf->CreateMoney(pPlayer->GetCharacter()->GetPos(), Amount, ClientId, 0.f, true);
}

void CGameContext::ConToggleLovely(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	pChr->m_Lovely = !pChr->m_Lovely;
	if(pChr->m_Lovely)
		new CLovely(&pSelf->m_World, pChr->GetPos(), ClientId);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "cosmetics", pChr->m_Lovely ? "lovely on" : "lovely off");
}

void CGameContext::ConToggleStaffInd(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	pChr->m_StaffInd = !pChr->m_StaffInd;
	if(pChr->m_StaffInd)
		new CStaffInd(&pSelf->m_World, pChr->GetPos(), ClientId);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "cosmetics", pChr->m_StaffInd ? "staff_ind on" : "staff_ind off");
}

void CGameContext::ConSpawnFlyingPoint(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int FromId = pResult->GetVictim();
	const int ToId = pResult->GetInteger(1);
	CCharacter *pFrom = pSelf->GetPlayerChar(FromId);
	if(!pFrom)
		return;
	pSelf->CreateFlyingPoint(pFrom->GetPos(), ToId, FromId, vec2(0, -5));
}

void CGameContext::ConToggleRotatingBall(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	pChr->m_RotatingBall = !pChr->m_RotatingBall;
	if(pChr->m_RotatingBall)
		new CRotatingBall(&pSelf->m_World, pChr->GetPos(), ClientId);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "cosmetics", pChr->m_RotatingBall ? "rotating_ball on" : "rotating_ball off");
}

void CGameContext::ConToggleAtom(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	pChr->m_Atom = !pChr->m_Atom;
	if(pChr->m_Atom)
		new CAtom(&pSelf->m_World, pChr->GetPos(), ClientId);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "cosmetics", pChr->m_Atom ? "atom on" : "atom off");
}

void CGameContext::ConToggleTrail(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	pChr->m_Trail = !pChr->m_Trail;
	if(pChr->m_Trail)
		new CTrail(&pSelf->m_World, pChr->GetPos(), ClientId);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "cosmetics", pChr->m_Trail ? "trail on" : "trail off");
}

void CGameContext::ConToggleEpicCircle(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	pChr->m_EpicCircle = !pChr->m_EpicCircle;
	if(pChr->m_EpicCircle)
		new CEpicCircle(&pSelf->m_World, pChr->GetPos(), ClientId);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "cosmetics", pChr->m_EpicCircle ? "epic_circle on" : "epic_circle off");
}

void CGameContext::ConMuteGagFx(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	new CMuteGag(&pSelf->m_World, pChr->GetPos(), ClientId);
}

void CGameContext::ConKickBootFx(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	const char *pReason = pResult->NumArguments() > 1 ? pResult->GetString(1) : "Kicked";
	new CKickBoot(&pSelf->m_World, pChr->GetPos(), ClientId, pReason);
}

void CGameContext::ConUnmuteSparkFx(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	new CUnmuteSpark(&pSelf->m_World, pChr->GetPos(), ClientId);
}

void CGameContext::ConSpawnPortal(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pPlayer || !pChr)
		return;

	vec2 PortalPos = pChr->GetPos();
	if(pResult->NumArguments() >= 3)
		PortalPos = vec2((float)pResult->GetInteger(1), (float)pResult->GetInteger(2));

	if(pPlayer->m_apPortal[PORTAL_FIRST] && pPlayer->m_apPortal[PORTAL_SECOND])
	{
		pPlayer->m_apPortal[PORTAL_FIRST]->Reset();
		if(pPlayer->m_apPortal[PORTAL_SECOND])
			pPlayer->m_apPortal[PORTAL_SECOND]->Reset();
		pPlayer->m_apPortal[PORTAL_FIRST] = nullptr;
		pPlayer->m_apPortal[PORTAL_SECOND] = nullptr;
	}

	for(int i = 0; i < NUM_PORTALS; i++)
	{
		if(pPlayer->m_apPortal[i])
			continue;
		pPlayer->m_apPortal[i] = new CPortal(&pSelf->m_World, PortalPos, ClientId);
		if(i == PORTAL_SECOND && pPlayer->m_apPortal[PORTAL_FIRST])
		{
			pPlayer->m_apPortal[PORTAL_FIRST]->SetLinkedPortal(pPlayer->m_apPortal[PORTAL_SECOND]);
			pPlayer->m_apPortal[PORTAL_SECOND]->SetLinkedPortal(pPlayer->m_apPortal[PORTAL_FIRST]);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "portal", "Portals linked");
		}
		else
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "portal", "First portal placed (call again to link)");
		return;
	}
}

void CGameContext::ConClearPortals(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer)
		return;
	for(int i = 0; i < NUM_PORTALS; i++)
	{
		if(pPlayer->m_apPortal[i])
			pPlayer->m_apPortal[i]->Reset();
		pPlayer->m_apPortal[i] = nullptr;
	}
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "portal", "Portals cleared");
}

void CGameContext::ConSpawnClock(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	vec2 Pos = pChr->GetPos() + vec2(0.f, -80.f);
	if(pResult->NumArguments() >= 3)
		Pos = vec2((float)pResult->GetInteger(1), (float)pResult->GetInteger(2));
	new CClock(&pSelf->m_World, Pos);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "clock", "Clock spawned");
}

void CGameContext::ConLightsaber(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;

	const char *pAction = pResult->NumArguments() > 1 ? pResult->GetString(1) : "toggle";
	if(!str_comp_nocase(pAction, "retract"))
	{
		if(pChr->m_pLightsaber)
			pChr->m_pLightsaber->Retract();
		return;
	}

	if(!pChr->m_pLightsaber)
	{
		pChr->m_pLightsaber = new CLightsaber(&pSelf->m_World, pChr->GetPos(), ClientId);
		pChr->m_pLightsaber->Extend();
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lightsaber", "extended");
	}
	else if(!str_comp_nocase(pAction, "extend"))
	{
		pChr->m_pLightsaber->Extend();
	}
	else
	{
		pChr->m_pLightsaber->Retract();
	}
}

void CGameContext::ConBanPlungerFx(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	const int Seconds = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : 60;
	const char *pReason = pResult->NumArguments() > 2 ? pResult->GetString(2) : "Banned";
	vec2 AirPos = pChr->GetPos();
	CBanPlungerToilet::FindPos(pSelf, pChr->GetPos(), &AirPos);
	new CBanPlungerToilet(&pSelf->m_World, AirPos, ClientId, Seconds, pReason);
}

void CGameContext::ConSpawnStableProjectile(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	int Type = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : WEAPON_GRENADE;
	if(Type < WEAPON_GUN || Type > WEAPON_NINJA)
		Type = WEAPON_GRENADE;
	vec2 Pos = pChr->GetPos() + vec2(0.f, -40.f);
	if(pResult->NumArguments() >= 4)
		Pos = vec2((float)pResult->GetInteger(2), (float)pResult->GetInteger(3));
	new CStableProjectile(&pSelf->m_World, Type, ClientId, Pos);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "stable_projectile", "Spawned");
}

void CGameContext::ConDropPickup(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;

	const char *pKind = pResult->NumArguments() > 1 ? pResult->GetString(1) : "health";
	int Type = POWERUP_HEALTH;
	int Weapon = WEAPON_GUN;
	if(!str_comp_nocase(pKind, "armor") || !str_comp_nocase(pKind, "shield"))
		Type = POWERUP_ARMOR;
	else if(!str_comp_nocase(pKind, "weapon") || !str_comp_nocase(pKind, "gun") || !str_comp_nocase(pKind, "shotgun") ||
		!str_comp_nocase(pKind, "grenade") || !str_comp_nocase(pKind, "laser") || !str_comp_nocase(pKind, "ninja"))
	{
		Type = POWERUP_WEAPON;
		if(!str_comp_nocase(pKind, "shotgun"))
			Weapon = WEAPON_SHOTGUN;
		else if(!str_comp_nocase(pKind, "grenade"))
			Weapon = WEAPON_GRENADE;
		else if(!str_comp_nocase(pKind, "laser"))
			Weapon = WEAPON_LASER;
		else if(!str_comp_nocase(pKind, "ninja"))
			Weapon = WEAPON_NINJA;
		else if(!str_comp_nocase(pKind, "gun") || !str_comp_nocase(pKind, "weapon"))
			Weapon = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : WEAPON_GUN;
		if(Weapon < WEAPON_GUN || Weapon >= NUM_WEAPONS)
			Weapon = WEAPON_GUN;
	}
	else if(str_comp_nocase(pKind, "health") != 0 && str_comp_nocase(pKind, "heart") != 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pickup_drop", "kind: health|armor|gun|shotgun|grenade|laser|ninja|weapon");
		return;
	}

	const float Dir = pChr->Core()->m_Input.m_Direction ? (float)pChr->Core()->m_Input.m_Direction : (pChr->Core()->m_Angle < 0 ? -1.f : 1.f);
	new CPickupDrop(&pSelf->m_World, pChr->GetPos(), Type, ClientId, Dir, 300, Weapon);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "pickup_drop", "Dropped");
}

void CGameContext::ConSpawnCustomProjectile(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;

	const char *pKind = pResult->NumArguments() > 1 ? pResult->GetString(1) : "spooky";
	bool Freeze = false, Explosive = false, Unfreeze = false, Bloody = false, Ghost = false, Spooky = false;
	int Type = WEAPON_GUN;
	if(!str_comp_nocase(pKind, "plasma"))
	{
		Explosive = true;
		Unfreeze = true;
		Type = CUSTOM_PROJ_TYPE_PLASMA;
	}
	else if(!str_comp_nocase(pKind, "heart"))
	{
		Type = CUSTOM_PROJ_TYPE_HEART;
	}
	else // spooky ghost gun
	{
		Bloody = true;
		Ghost = true;
		Spooky = true;
		Type = WEAPON_GUN;
	}

	float Angle = pChr->Core()->m_Angle / 256.0f;
	vec2 Dir = vec2(cosf(Angle), sinf(Angle));
	if(length(Dir) < 0.001f)
		Dir = vec2(1.f, 0.f);
	new CCustomProjectile(&pSelf->m_World, ClientId, pChr->GetPos(), Dir, Freeze, Explosive, Unfreeze, Bloody, Ghost, Spooky, Type);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "custom_projectile", "Spawned");
}

void CGameContext::ConSpawnTeleporter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;

	const char *pKind = pResult->NumArguments() > 1 ? pResult->GetString(1) : "out";
	int Type = TILE_TELEOUT;
	if(!str_comp_nocase(pKind, "weapon") || !str_comp_nocase(pKind, "inweapon"))
		Type = TILE_TELEINWEAPON;
	else if(!str_comp_nocase(pKind, "hook") || !str_comp_nocase(pKind, "inhook"))
		Type = TILE_TELEINHOOK;
	else if(!str_comp_nocase(pKind, "in"))
		Type = TILE_TELEIN;
	else if(str_comp_nocase(pKind, "out") != 0)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "teleporter", "kind: out|in|weapon|hook");
		return;
	}

	int Number = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 1;
	if(Number < 1)
		Number = 1;
	if(Number > 255)
		Number = 255;

	vec2 Pos = pChr->GetPos();
	Pos.x = (float)(round_to_int(Pos.x) / 32 * 32 + 16);
	Pos.y = (float)(round_to_int(Pos.y) / 32 * 32 + 16);
	new CTeleporter(&pSelf->m_World, Pos, Type, Number);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "teleporter", "Spawned");
}

void CGameContext::ConSpawnMeteor(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr || !pSelf->m_apPlayers[ClientId])
		return;
	const bool Infinite = pResult->NumArguments() > 1 && pResult->GetInteger(1) != 0;
	if(Infinite)
		pSelf->m_apPlayers[ClientId]->m_InfMeteors++;
	else
		pChr->m_Meteors++;
	new CMeteor(&pSelf->m_World, pChr->GetPos(), ClientId, Infinite);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "meteor", "Spawned");
}

void CGameContext::ConClearMeteors(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(pSelf->m_apPlayers[ClientId])
		pSelf->m_apPlayers[ClientId]->m_InfMeteors = 0;
	if(pChr)
		pChr->m_Meteors = 0;
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "meteor", "Cleared counters (entities despawn next tick)");
}

void CGameContext::ConSpawnPlayerCounter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim();
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	const int Port = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : 8303;
	new CPlayerCounter(&pSelf->m_World, pChr->GetPos(), Port);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "playercounter", "Spawned");
}

void CGameContext::ConUpdatePlayerCounter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	if(pResult->NumArguments() < 2)
		return;
	const int Port = pResult->GetInteger(0);
	const int Count = pResult->GetInteger(1);
	for(CPlayerCounter *pEnt = (CPlayerCounter *)pSelf->m_World.FindFirst(CGameWorld::ENTTYPE_PLAYER_COUNTER); pEnt; pEnt = (CPlayerCounter *)pEnt->TypeNext())
		pEnt->OnUpdate(Port, Count);
}

void CGameContext::RegisterFddraceAccountCommands()
{
	Console()->Register("register", "s[name] s[password] s[password]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConRegister, this, "Register an account");
	Console()->Register("login", "s[name] s[password]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConLogin, this, "Log into an account");
	Console()->Register("logout", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConLogout, this, "Log out of an account");

	Console()->Register("give_roomkey", "v[id]", CFGFLAG_SERVER, ConGiveRoomKey, this, "Grant room key to player (session)");
	Console()->Register("give_vipplus", "v[id]", CFGFLAG_SERVER, ConGiveVipPlus, this, "Grant VIP+ (account if logged in, else session)");
	Console()->Register("jail_arrest", "v[id] i[seconds]", CFGFLAG_SERVER, ConJailArrest, this, "Arrest player for i seconds");
	Console()->Register("jail_release", "v[id]", CFGFLAG_SERVER, ConJailRelease, this, "Release player from jail");
	Console()->Register("laser_text", "v[id] r[text]", CFGFLAG_SERVER, ConLaserText, this, "Spawn laser text above player");
	Console()->Register("drop_money", "v[id] i[amount]", CFGFLAG_SERVER, ConDropMoney, this, "Spawn money drop at player");
	Console()->Register("toggle_lovely", "v[id]", CFGFLAG_SERVER, ConToggleLovely, this, "Toggle lovely hearts cosmetic");
	Console()->Register("toggle_staff_ind", "v[id]", CFGFLAG_SERVER, ConToggleStaffInd, this, "Toggle staff indicator cosmetic");
	Console()->Register("spawn_flyingpoint", "v[from] i[to]", CFGFLAG_SERVER, ConSpawnFlyingPoint, this, "Spawn flying point from player to target id");
	Console()->Register("toggle_rotating_ball", "v[id]", CFGFLAG_SERVER, ConToggleRotatingBall, this, "Toggle rotating ball cosmetic");
	Console()->Register("toggle_atom", "v[id]", CFGFLAG_SERVER, ConToggleAtom, this, "Toggle atom cosmetic");
	Console()->Register("toggle_trail", "v[id]", CFGFLAG_SERVER, ConToggleTrail, this, "Toggle trail cosmetic");
	Console()->Register("toggle_epic_circle", "v[id]", CFGFLAG_SERVER, ConToggleEpicCircle, this, "Toggle epic circle cosmetic");
	Console()->Register("mute_gag_fx", "v[id]", CFGFLAG_SERVER, ConMuteGagFx, this, "Spawn mute lightning VFX");
	Console()->Register("kick_boot_fx", "v[id] ?r[reason]", CFGFLAG_SERVER, ConKickBootFx, this, "Spawn kick boot VFX (kicks after animation)");
	Console()->Register("unmute_spark_fx", "v[id]", CFGFLAG_SERVER, ConUnmuteSparkFx, this, "Spawn unmute spark VFX");
	Console()->Register("spawn_portal", "v[id] ?i[x] ?i[y]", CFGFLAG_SERVER, ConSpawnPortal, this, "Place/link portal for player (optional x y)");
	Console()->Register("clear_portals", "v[id]", CFGFLAG_SERVER, ConClearPortals, this, "Clear player portals");
	Console()->Register("spawn_clock", "v[id] ?i[x] ?i[y]", CFGFLAG_SERVER, ConSpawnClock, this, "Spawn analog clock above player");
	Console()->Register("lightsaber", "v[id] ?s[extend|retract|toggle]", CFGFLAG_SERVER, ConLightsaber, this, "Extend/retract lightsaber for player");
	Console()->Register("ban_plunger_fx", "v[id] ?i[seconds] ?r[reason]", CFGFLAG_SERVER, ConBanPlungerFx, this, "Spawn ban plunger VFX (bans after animation)");
	Console()->Register("spawn_stable_projectile", "v[id] ?i[type] ?i[x] ?i[y]", CFGFLAG_SERVER, ConSpawnStableProjectile, this, "Spawn stable projectile (type: gun/shotgun/grenade ids)");
	Console()->Register("drop_pickup", "v[id] ?s[kind] ?i[weapon]", CFGFLAG_SERVER, ConDropPickup, this, "Drop health/armor/weapon pickup (kind: health|armor|gun|shotgun|grenade|laser|ninja)");
	Console()->Register("spawn_custom_projectile", "v[id] ?s[spooky|plasma|heart]", CFGFLAG_SERVER, ConSpawnCustomProjectile, this, "Fire custom projectile from player");
	Console()->Register("spawn_teleporter", "v[id] ?s[out|in|weapon|hook] ?i[number]", CFGFLAG_SERVER, ConSpawnTeleporter, this, "Place runtime map teleporter at player tile");
	Console()->Register("spawn_meteor", "v[id] ?i[infinite]", CFGFLAG_SERVER, ConSpawnMeteor, this, "Spawn meteor following player (1=infinite)");
	Console()->Register("clear_meteors", "v[id]", CFGFLAG_SERVER, ConClearMeteors, this, "Clear meteor counters for player");
	Console()->Register("spawn_playercounter", "v[id] ?i[port]", CFGFLAG_SERVER, ConSpawnPlayerCounter, this, "Spawn laser player-count display");
	Console()->Register("update_playercounter", "i[port] i[count]", CFGFLAG_SERVER, ConUpdatePlayerCounter, this, "Update player counters for a port");
}

#endif

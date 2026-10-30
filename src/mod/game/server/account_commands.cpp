#include "account.h"

#include <algorithm>

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/mapitems.h>

#include <mod/game/server/entities/atom.h>
#include <mod/game/server/entities/ban_plunger_toilet.h>
#include <mod/game/server/entities/button.h>
#include <mod/game/server/entities/clock.h>
#include <mod/game/server/entities/custom_projectile.h>
#include <mod/game/server/entities/epic_circle.h>
#include <mod/game/server/entities/grog.h>
#include <mod/game/server/entities/helicopter.h>
#include <mod/game/server/entities/kick_boot.h>
#include <mod/game/server/entities/speedup.h>
#include <mod/game/server/entities/lightsaber.h>
#include <mod/game/server/entities/lightninglaser.h>
#include <mod/game/server/entities/lovely.h>
#include <mod/game/server/entities/meteor.h>
#include <mod/game/server/entities/mute_gag.h>
#include <mod/game/server/entities/pickup_drop.h>
#include <mod/game/server/entities/playercounter.h>
#include <mod/game/server/entities/portal.h>
#include <mod/game/server/entities/portalblocker.h>
#include <mod/game/server/entities/rotating_ball.h>
#include <mod/game/server/entities/staff_ind.h>
#include <mod/game/server/entities/stable_projectile.h>
#include <mod/game/server/entities/teleporter.h>
#include <mod/game/server/entities/taser_shield.h>
#include <mod/game/server/entities/trail.h>
#include <mod/game/server/entities/unmute_spark.h>
#include <mod/game/server/draweditor.h>
#include <game/weapons_ex.h>

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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	pSelf->Accounts()->JailPlayer(pResult->GetVictim(0), pResult->GetInteger(1));
}

void CGameContext::ConJailRelease(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Accounts()->ReleaseJail(pResult->GetVictim(0));
}

void CGameContext::ConLaserText(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer || !pPlayer->GetCharacter())
		return;
	pSelf->CreateLaserText(pPlayer->GetCharacter()->GetPos(), ClientId, pResult->GetString(1), 3);
}

void CGameContext::ConDropMoney(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer || !pPlayer->GetCharacter())
		return;
	const int64_t Amount = pResult->GetInteger(1);
	pSelf->CreateMoney(pPlayer->GetCharacter()->GetPos(), Amount, ClientId, 0.f, true);
}

void CGameContext::ConToggleLovely(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int FromId = pResult->GetVictim(0);
	const int ToId = pResult->GetInteger(1);
	CCharacter *pFrom = pSelf->GetPlayerChar(FromId);
	if(!pFrom)
		return;
	pSelf->CreateFlyingPoint(pFrom->GetPos(), ToId, FromId, vec2(0, -5));
}

void CGameContext::ConToggleRotatingBall(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	new CMuteGag(&pSelf->m_World, pChr->GetPos(), ClientId);
}

void CGameContext::ConKickBootFx(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	const char *pReason = pResult->NumArguments() > 1 ? pResult->GetString(1) : "Kicked";
	new CKickBoot(&pSelf->m_World, pChr->GetPos(), ClientId, pReason);
}

void CGameContext::ConUnmuteSparkFx(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	new CUnmuteSpark(&pSelf->m_World, pChr->GetPos(), ClientId);
}

void CGameContext::ConSpawnPortal(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pPlayer || !pChr)
		return;

	vec2 PortalPos = pChr->GetPos();
	if(pResult->NumArguments() >= 3)
		PortalPos = vec2((float)pResult->GetInteger(1), (float)pResult->GetInteger(2));

	if(g_Config.m_SvPortalRifleAmmo)
	{
		const int AccId = pSelf->Accounts()->FindByClientId(ClientId);
		const SAccountInfo *pAcc = pSelf->Accounts()->Get(AccId);
		if(!pAcc || AccId < ACC_START || !pAcc->m_LoggedIn)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "portal", "Login required for portal ammo");
			return;
		}
		if(pAcc->m_PortalBattery <= 0)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "portal", "No portal battery");
			return;
		}
	}

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
		if(g_Config.m_SvPortalRifleAmmo && !pPlayer->GivePortalBattery(-1))
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "portal", "No portal battery");
			return;
		}
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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
	const int ClientId = pResult->GetVictim(0);
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

void CGameContext::ConTaserShieldFx(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	new CTaserShield(&pSelf->m_World, pChr->GetPos(), ClientId);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "taser_shield", "Spawned");
}

void CGameContext::ConPortalBlocker(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;

	const char *pAction = pResult->NumArguments() > 1 ? pResult->GetString(1) : "start";
	if(!str_comp_nocase(pAction, "place"))
	{
		if(pChr->m_pPortalBlocker)
			pChr->m_pPortalBlocker->OnPlace();
		return;
	}

	if(pChr->m_pPortalBlocker)
	{
		pChr->m_pPortalBlocker->Cancel();
		pChr->m_pPortalBlocker = nullptr;
	}
	pChr->m_IsPortalBlocker = true;
	pChr->m_pPortalBlocker = new CPortalBlocker(&pSelf->m_World, pChr->GetPos(), ClientId);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "portalblocker", "Preview started; use portal_blocker <id> place twice");
}

void CGameContext::ConLightningLaser(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;

	float Angle = pChr->Core()->m_Angle / 256.0f;
	vec2 Dir = vec2(cosf(Angle), sinf(Angle));
	if(length(Dir) < 0.001f)
		Dir = vec2(1.f, 0.f);
	new CLightningLaser(&pSelf->m_World, pChr->GetPos(), Dir, ClientId);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "lightning_laser", "Spawned");
}

void CGameContext::ConGiveGrog(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	if(!pChr->AddGrog())
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "grog", "Failed (hold limit?)");
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "grog", "Given");
}

void CGameContext::ConGrogSip(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr || !pChr->m_pGrog)
		return;
	pChr->m_pGrog->OnSip();
}

void CGameContext::ConGrogDrop(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr || !pChr->m_pGrog)
		return;
	pChr->m_pGrog->Drop();
}

void CGameContext::ConSaveDrop(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int Victim = pResult->GetVictim(0);
	if(!CheckClientId(Victim) || !pSelf->m_apPlayers[Victim])
		return;
	const float Hours = pResult->NumArguments() >= 2 ? pResult->GetFloat(1) : 6.f;
	const char *pReason = pResult->NumArguments() >= 3 ? pResult->GetString(2) : "save drop";
	pSelf->m_FddraceSave.SaveDrop(Victim, Hours, pReason);
}

void CGameContext::ConListSavedTees(IConsole::IResult *pResult, void *pUserData)
{
	(void)pResult;
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_FddraceSave.ListSavedTees();
}

void CGameContext::ConWalletAdd(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer)
		return;
	pPlayer->WalletTransaction(pResult->GetInteger(1));
}

void CGameContext::ConSpawnButton(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	int Number = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : 1;
	if(Number < 1)
		Number = 1;
	if(Number > 255)
		Number = 255;
	vec2 Pos = pChr->GetPos();
	Pos.x = (float)(round_to_int(Pos.x) / 32 * 32 + 16);
	Pos.y = (float)(round_to_int(Pos.y) / 32 * 32 + 16);
	new CButton(&pSelf->m_World, Pos, Number);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "button", "Spawned");
}

void CGameContext::ConSpawnHelicopter(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	new CHelicopter(&pSelf->m_World, pChr->GetPos() + vec2(0, -48), ClientId, TURRETTYPE_LAUNCHER, true);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "helicopter", "Spawned");
}

void CGameContext::ConForceFlagOwner(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int Team = pResult->GetInteger(0);
	const int Victim = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : -1;
	pSelf->ForceFlagOwner(Victim, Team);
}

void CGameContext::ConSpawnSpeedup(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	const int Angle = pResult->NumArguments() > 1 ? pResult->GetInteger(1) : 0;
	const int Force = pResult->NumArguments() > 2 ? pResult->GetInteger(2) : 10;
	const int MaxSpeed = pResult->NumArguments() > 3 ? pResult->GetInteger(3) : 0;
	vec2 Pos = pChr->GetPos();
	Pos.x = (float)(round_to_int(Pos.x) / 32 * 32 + 16);
	Pos.y = (float)(round_to_int(Pos.y) / 32 * 32 + 16);
	new CSpeedup(&pSelf->m_World, Pos, (float)Angle, Force, MaxSpeed);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "speedup", "Spawned");
}

void CGameContext::ConRedirectClientSafe(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	const int Port = pResult->GetInteger(1);
	pChr->TrySafelyRedirectClient(Port, true);
}

void CGameContext::ConDrawTool(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pPlayer)
		return;
	const char *pMode = pResult->NumArguments() > 1 ? pResult->GetString(1) : "off";

	int Category = -1;
	if(str_comp_nocase(pMode, "speedup") == 0)
		Category = CDrawEditor::CAT_SPEEDUPS;
	else if(str_comp_nocase(pMode, "button") == 0)
		Category = CDrawEditor::CAT_LASERDOORS;
	else if(str_comp_nocase(pMode, "heli") == 0 || str_comp_nocase(pMode, "helicopter") == 0)
		Category = CDrawEditor::CAT_HELICOPTER;
	else if(str_comp_nocase(pMode, "tele") == 0 || str_comp_nocase(pMode, "teleporter") == 0)
		Category = CDrawEditor::CAT_TELEPORTER;

	// Keep slim m_DrawTool as fallback when character missing.
	if(Category < 0)
	{
		pPlayer->m_DrawTool = -1;
		if(pChr)
			pChr->GiveWeapon(WEAPON_DRAW_EDITOR, true);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "draw", "draw_tool=off");
		return;
	}

	pPlayer->m_DrawTool = -1;
	if(pChr)
	{
		pChr->GiveWeapon(WEAPON_DRAW_EDITOR, false);
		pChr->SetWeapon(WEAPON_DRAW_EDITOR);
		pChr->m_DrawEditor.SetCategory(Category);
	}
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "draw_tool=%s (WEAPON_DRAW_EDITOR)", pMode);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "draw", aBuf);
}

void CGameContext::ConRoomInvite(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer)
		return;
	if(!pPlayer->m_HasRoomKey)
	{
		pSelf->SendChatTarget(ClientId, "You don't have a room key");
		return;
	}

	const int TargetId = pSelf->FindClientIdByName(pResult->GetString(0)).value_or(-1);
	CCharacter *pChr = pSelf->GetPlayerChar(TargetId);
	if(TargetId < 0 || !pChr)
	{
		pSelf->SendChatTarget(ClientId, "That player doesn't exist");
		return;
	}
	if(pChr->GetPlayer()->m_HasRoomKey)
	{
		pSelf->SendChatTarget(ClientId, "This player has a key already");
		return;
	}
	if(pChr->Core()->m_MoveRestrictionExtra.m_RoomKey)
	{
		pSelf->SendChatTarget(ClientId, "This player got invited already");
		return;
	}

	pChr->Core()->m_MoveRestrictionExtra.m_RoomKey = true;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' invited you to the room", pSelf->Server()->ClientName(ClientId));
	pSelf->SendChatTarget(TargetId, aBuf);
	str_format(aBuf, sizeof(aBuf), "You invited '%s' to the room", pSelf->Server()->ClientName(TargetId));
	pSelf->SendChatTarget(ClientId, aBuf);
}

void CGameContext::ConRoomKick(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer)
		return;
	if(!pPlayer->m_HasRoomKey)
	{
		pSelf->SendChatTarget(ClientId, "You don't have a room key");
		return;
	}

	const int TargetId = pSelf->FindClientIdByName(pResult->GetString(0)).value_or(-1);
	CCharacter *pChr = pSelf->GetPlayerChar(TargetId);
	if(TargetId < 0 || !pChr)
	{
		pSelf->SendChatTarget(ClientId, "That player doesn't exist");
		return;
	}
	if(pChr->GetPlayer()->m_HasRoomKey)
	{
		pSelf->SendChatTarget(ClientId, "You can't kick a key holder");
		return;
	}
	if(!pChr->Core()->m_MoveRestrictionExtra.m_RoomKey)
	{
		pSelf->SendChatTarget(ClientId, "This player is not in the room");
		return;
	}

	pChr->Core()->m_MoveRestrictionExtra.m_RoomKey = false;
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "'%s' kicked you from the room", pSelf->Server()->ClientName(ClientId));
	pSelf->SendChatTarget(TargetId, aBuf);
	str_format(aBuf, sizeof(aBuf), "You kicked '%s' from the room", pSelf->Server()->ClientName(TargetId));
	pSelf->SendChatTarget(ClientId, aBuf);
}

static bool PlayerHasVipPlus(CGameContext *pSelf, int ClientId)
{
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(pChr && pChr->Core()->m_MoveRestrictionExtra.m_VipPlus)
		return true;
	const int AccId = pSelf->Accounts()->FindByClientId(ClientId);
	if(AccId < ACC_START)
		return false;
	const SAccountInfo *pAcc = pSelf->Accounts()->Get(AccId);
	return pAcc && (pAcc->m_VIP == VIP_PLUS || pAcc->m_VIP == VIP_ADMIN);
}

void CGameContext::ConVipCosmetic(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	CCharacter *pChr = pSelf->GetPlayerChar(ClientId);
	if(!pChr)
		return;
	if(!PlayerHasVipPlus(pSelf, ClientId))
	{
		pSelf->SendChatTarget(ClientId, "VIP+ required for cosmetics");
		return;
	}

	const char *pWhich = pResult->GetString(0);
	if(str_comp_nocase(pWhich, "lovely") == 0)
	{
		pChr->m_Lovely = !pChr->m_Lovely;
		if(pChr->m_Lovely)
			new CLovely(&pSelf->m_World, pChr->GetPos(), ClientId);
		pSelf->SendChatTarget(ClientId, pChr->m_Lovely ? "lovely on" : "lovely off");
	}
	else if(str_comp_nocase(pWhich, "trail") == 0)
	{
		pChr->m_Trail = !pChr->m_Trail;
		if(pChr->m_Trail)
			new CTrail(&pSelf->m_World, pChr->GetPos(), ClientId);
		pSelf->SendChatTarget(ClientId, pChr->m_Trail ? "trail on" : "trail off");
	}
	else if(str_comp_nocase(pWhich, "atom") == 0)
	{
		pChr->m_Atom = !pChr->m_Atom;
		if(pChr->m_Atom)
			new CAtom(&pSelf->m_World, pChr->GetPos(), ClientId);
		pSelf->SendChatTarget(ClientId, pChr->m_Atom ? "atom on" : "atom off");
	}
	else if(str_comp_nocase(pWhich, "ball") == 0 || str_comp_nocase(pWhich, "rotating_ball") == 0)
	{
		pChr->m_RotatingBall = !pChr->m_RotatingBall;
		if(pChr->m_RotatingBall)
			new CRotatingBall(&pSelf->m_World, pChr->GetPos(), ClientId);
		pSelf->SendChatTarget(ClientId, pChr->m_RotatingBall ? "rotating_ball on" : "rotating_ball off");
	}
	else if(str_comp_nocase(pWhich, "epic") == 0 || str_comp_nocase(pWhich, "epic_circle") == 0)
	{
		pChr->m_EpicCircle = !pChr->m_EpicCircle;
		if(pChr->m_EpicCircle)
			new CEpicCircle(&pSelf->m_World, pChr->GetPos(), ClientId);
		pSelf->SendChatTarget(ClientId, pChr->m_EpicCircle ? "epic_circle on" : "epic_circle off");
	}
	else
	{
		pSelf->SendChatTarget(ClientId, "Usage: /vipcosmetic <lovely|trail|atom|ball|epic>");
	}
}

void CGameContext::ConBuyRoomKey(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->Accounts()->BuyRoomKey(pResult->m_ClientId);
}

void CGameContext::ConMoney(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pPlayer)
		return;

	char aBuf[160];
	const int AccId = pSelf->Accounts()->FindByClientId(ClientId);
	if(AccId >= ACC_START)
	{
		const SAccountInfo *pAcc = pSelf->Accounts()->Get(AccId);
		str_format(aBuf, sizeof(aBuf), "Money: %lld (account '%s')",
			(long long)pPlayer->GetWalletMoney(), pAcc ? pAcc->m_aUsername : "?");
	}
	else
		str_format(aBuf, sizeof(aBuf), "Money: %lld (not logged in)", (long long)pPlayer->GetWalletMoney());
	pSelf->SendChatTarget(ClientId, aBuf);
}

void CGameContext::ConDiscord(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	if(!pSelf->m_apPlayers[ClientId])
		return;
	if(g_Config.m_SvDiscordURL[0])
	{
		char aBuf[192];
		str_format(aBuf, sizeof(aBuf), "Discord server: %s", g_Config.m_SvDiscordURL);
		pSelf->SendChatTarget(ClientId, aBuf);
	}
	else
		pSelf->SendChatTarget(ClientId, "This server does not have a linked Discord server");
}

void CGameContext::ConAccountInfo(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->m_ClientId;
	const int AccId = pSelf->Accounts()->FindByClientId(ClientId);
	if(AccId < ACC_START)
	{
		pSelf->SendChatTarget(ClientId, "Not logged in. Use /login");
		return;
	}
	const SAccountInfo *pAcc = pSelf->Accounts()->Get(AccId);
	CPlayer *pPlayer = pSelf->m_apPlayers[ClientId];
	if(!pAcc || !pPlayer)
		return;

	char aBuf[256];
	const char *pVip = pAcc->m_VIP == VIP_ADMIN ? "ADMIN" : pAcc->m_VIP == VIP_PLUS ? "VIP+" : pAcc->m_VIP == VIP_CLASSIC ? "VIP" : "none";
	str_format(aBuf, sizeof(aBuf),
		"Account '%s' | lvl %d | vip %s | money %lld | police %d | taser %d | portal bat %d | roomkey %s",
		pAcc->m_aUsername, pAcc->m_Level, pVip,
		(long long)pPlayer->GetWalletMoney(),
		pAcc->m_PoliceLevel, pAcc->m_TaserLevel, pAcc->m_PortalBattery,
		pPlayer->m_HasRoomKey ? "yes" : "no");
	pSelf->SendChatTarget(ClientId, aBuf);
}

void CGameContext::ConAccSet(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const int ClientId = pResult->GetVictim(0);
	const int AccId = pSelf->Accounts()->FindByClientId(ClientId);
	SAccountInfo *pAcc = pSelf->Accounts()->Get(AccId);
	if(!pAcc || AccId < ACC_START)
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Player not logged in");
		return;
	}

	const char *pField = pResult->GetString(1);
	const int Value = pResult->GetInteger(2);
	if(str_comp_nocase(pField, "portal_battery") == 0)
		pAcc->m_PortalBattery = std::max(0, Value);
	else if(str_comp_nocase(pField, "portal_blocker") == 0)
		pAcc->m_PortalBlocker = std::max(0, Value);
	else if(str_comp_nocase(pField, "portal_rifle") == 0)
		pAcc->m_PortalRifle = Value != 0 ? 1 : 0;
	else if(str_comp_nocase(pField, "taser_level") == 0)
		pAcc->m_TaserLevel = std::clamp(Value, 0, 10);
	else if(str_comp_nocase(pField, "taser_battery") == 0)
		pAcc->m_TaserBattery = std::max(0, Value);
	else if(str_comp_nocase(pField, "police") == 0)
		pAcc->m_PoliceLevel = std::max(0, Value);
	else if(str_comp_nocase(pField, "level") == 0)
		pAcc->m_Level = std::max(1, Value);
	else if(str_comp_nocase(pField, "spawn_shotgun") == 0)
		pAcc->m_aSpawnWeapon[0] = Value != 0 ? 1 : 0;
	else if(str_comp_nocase(pField, "spawn_grenade") == 0)
		pAcc->m_aSpawnWeapon[1] = Value != 0 ? 1 : 0;
	else if(str_comp_nocase(pField, "spawn_laser") == 0)
		pAcc->m_aSpawnWeapon[2] = Value != 0 ? 1 : 0;
	else if(str_comp_nocase(pField, "ninjajetpack") == 0)
		pAcc->m_Ninjajetpack = Value != 0;
	else
	{
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account",
			"fields: portal_battery|portal_blocker|portal_rifle|taser_level|taser_battery|police|level|spawn_shotgun|spawn_grenade|spawn_laser|ninjajetpack");
		return;
	}

	pSelf->Accounts()->Persist(AccId);
	if(CPlayer *pPlayer = pSelf->m_apPlayers[ClientId])
		pSelf->Accounts()->ApplyAccountLoadouts(pPlayer);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "account", "Updated");
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
	Console()->Register("taser_shield_fx", "v[id]", CFGFLAG_SERVER, ConTaserShieldFx, this, "Spawn taser shield armor VFX on player");
	Console()->Register("portal_blocker", "v[id] ?s[start|place]", CFGFLAG_SERVER, ConPortalBlocker, this, "Start portal blocker preview or place endpoint");
	Console()->Register("lightning_laser", "v[id]", CFGFLAG_SERVER, ConLightningLaser, this, "Fire lightning laser from player aim");
	Console()->Register("give_grog", "v[id]", CFGFLAG_SERVER, ConGiveGrog, this, "Give grog to player");
	Console()->Register("grog_sip", "v[id]", CFGFLAG_SERVER, ConGrogSip, this, "Take a sip from held grog");
	Console()->Register("grog_drop", "v[id]", CFGFLAG_SERVER, ConGrogDrop, this, "Drop held grog");
	Console()->Register("save_drop", "v[id] ?f[hours] ?r[reason]", CFGFLAG_SERVER, ConSaveDrop, this, "Save player tee then kick");
	Console()->Register("list_saved_tees", "", CFGFLAG_SERVER, ConListSavedTees, this, "List saved tee identities for current map");
	Console()->Register("wallet_add", "v[id] i[amount]", CFGFLAG_SERVER, ConWalletAdd, this, "Add/subtract money from player wallet");
	Console()->Register("spawn_button", "v[id] ?i[number]", CFGFLAG_SERVER, ConSpawnButton, this, "Spawn plot-draw button at player");
	Console()->Register("spawn_helicopter", "v[id]", CFGFLAG_SERVER, ConSpawnHelicopter, this, "Spawn slim helicopter near player");
	Console()->Register("forceflagowner", "i[flag] ?i[id]", CFGFLAG_SERVER, ConForceFlagOwner, this, "Give flag to player (0=red,1=blue; id=-1 to return)");
	Console()->Register("spawn_speedup", "v[id] ?i[angle] ?i[force] ?i[maxspeed]", CFGFLAG_SERVER, ConSpawnSpeedup, this, "Spawn speedup tile entity at player");
	Console()->Register("redirect_client_safe", "v[id] i[port]", CFGFLAG_SERVER, ConRedirectClientSafe, this, "Force safe redirect (save tee + redirect)");
	Console()->Register("draw_tool", "v[id] ?s[off|speedup|button|heli|tele]", CFGFLAG_SERVER, ConDrawTool, this, "Slim draw tool: fire places speedup/button/heli/tele");
	Console()->Register("roominvite", "s[player]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConRoomInvite, this, "Invite player into room (requires room key)");
	Console()->Register("roomkick", "s[player]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConRoomKick, this, "Kick invited player from room (requires room key)");
	Console()->Register("vipcosmetic", "s[which]", CFGFLAG_CHAT | CFGFLAG_SERVER, ConVipCosmetic, this, "Toggle VIP+ cosmetic: lovely/trail/atom/ball/epic");
	Console()->Register("buyroomkey", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConBuyRoomKey, this, "Buy a session room key for sv_room_key_price");
	Console()->Register("money", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConMoney, this, "Show wallet money");
	Console()->Register("discord", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConDiscord, this, "Show Discord server URL");
	Console()->Register("account", "", CFGFLAG_CHAT | CFGFLAG_SERVER, ConAccountInfo, this, "Show account summary");
	Console()->Register("acc_set", "v[id] s[field] i[value]", CFGFLAG_SERVER, ConAccSet, this, "Set account field (portal_battery, taser_level, spawn_*, …)");
}

#endif

#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <mod/game/server/entities/grog.h>
#include <mod/game/server/entities/epic_circle.h>

#ifdef CONF_FDDRACE_MOD

bool CCharacter::AddGrog()
{
	if(!m_pPlayer)
		return false;

	if(m_NumGrogsHolding >= g_Config.m_SvGrogHoldLimit)
	{
		if(m_LastGrogHoldMsg + Server()->TickSpeed() * 3 < Server()->Tick())
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "You can not hold more than %d grogs at once", g_Config.m_SvGrogHoldLimit);
			GameServer()->SendChatTarget(m_pPlayer->GetCid(), aBuf);
			m_LastGrogHoldMsg = Server()->Tick();
		}
		return false;
	}

	m_NumGrogsHolding++;
	if(!m_pGrog)
		m_pGrog = new CGrog(GameWorld(), m_Pos, m_pPlayer->GetCid());
	return true;
}

void CCharacter::IncreasePermille(int Permille)
{
	if(Permille <= 0 || !m_pPlayer)
		return;

	m_pPlayer->m_Permille += Permille;
	GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCid(), TeamMask());

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Permille: %d.%d", m_pPlayer->m_Permille / 10, m_pPlayer->m_Permille % 10);
	GameServer()->SendChatTarget(m_pPlayer->GetCid(), aBuf);
}

bool CCharacter::LoadRedirectTile(int Port)
{
	if(!Port)
		return false;

	const char *pList = g_Config.m_SvRedirectServerTilePorts;
	char aBuf[16];
	while((pList = str_next_token(pList, ",", aBuf, sizeof(aBuf))))
	{
		int EntryNumber = 0;
		int EntryPort = 0;
		if(sscanf(aBuf, "%d:%d", &EntryNumber, &EntryPort) != 2)
			continue;
		if(EntryNumber <= 0 || EntryPort != Port)
			continue;

		const vec2 Pos = GameServer()->Collision()->GetRandomRedirectTile(EntryNumber);
		if(Pos != vec2(-1, -1))
		{
			SetPosition(Pos);
			m_Core.m_Vel = vec2(0, 0);
			return true;
		}
	}

	return false;
}

bool CCharacter::TrySafelyRedirectClientImpl(int Port)
{
	if(Port == 0 || Port == g_Config.m_SvPort || !m_pPlayer)
		return false;

	m_RedirectTilePort = Port;
	const int IdentityIndex = GameServer()->m_FddraceSave.SaveCharacter(
		m_pPlayer->GetCid(), SAVE_REDIRECT | SAVE_WALLET | SAVE_FLAG, (float)g_Config.m_SvShutdownSaveTeeExpire);
	if(IdentityIndex != -1)
	{
		Server()->RedirectClient(m_pPlayer->GetCid(), Port);
		return true;
	}
	m_RedirectTilePort = 0;
	return false;
}

bool CCharacter::TrySafelyRedirectClient(int Port, bool Force)
{
	if(Port == 0 || Port == g_Config.m_SvPort || !m_pPlayer)
		return false;

	const int DummyId = Server()->GetDummy(m_pPlayer->GetCid());
	CPlayer *pDummy = DummyId != -1 ? GameServer()->m_apPlayers[DummyId] : nullptr;
	if(!Force && pDummy && (!pDummy->m_LastRedirectTryTick || pDummy->m_LastRedirectTryTick + Server()->TickSpeed() * 30 < Server()->Tick()))
	{
		m_pPlayer->m_LastRedirectTryTick = Server()->Tick();
		GameServer()->SendChatTarget(m_pPlayer->GetCid(),
			"[WARNING] Enter this teleporter with your dummy within 30 seconds to move safely");
		return false;
	}

	CCharacter *pDummyChar = GameServer()->GetPlayerChar(DummyId);
	if(!Force && pDummy && !pDummyChar)
	{
		GameServer()->SendChatTarget(m_pPlayer->GetCid(),
			"[WARNING] Your dummy has to be alive in order to get moved safely");
		return false;
	}

	if(TrySafelyRedirectClientImpl(Port))
	{
		if(IsAlive())
			m_pPlayer->KillCharacter(WEAPON_GAME);
		if(pDummyChar && pDummyChar->TrySafelyRedirectClientImpl(Port) && pDummyChar->IsAlive())
			pDummy->KillCharacter(WEAPON_GAME);
		return true;
	}
	return false;
}

void CCharacter::EpicCircle(bool Set, int FromId, bool Silent)
{
	(void)FromId;
	(void)Silent;
	if(m_EpicCircle == Set)
		return;
	m_EpicCircle = Set;
	if(m_EpicCircle)
		new CEpicCircle(GameWorld(), m_Pos, m_pPlayer->GetCid());
}

void CCharacter::Passive(bool Set, int FromId, bool Silent)
{
	(void)FromId;
	(void)Silent;
	m_Passive = Set;
	if(!Set)
		m_PassiveEndTick = 0;
}

bool CCharacter::UpdatePassiveEndTick(int64_t NewEndTick)
{
	if(m_Passive && (!m_PassiveEndTick || m_PassiveEndTick >= NewEndTick))
		return false;
	m_PassiveEndTick = NewEndTick;
	Passive(true, -1, true);
	return true;
}

#endif

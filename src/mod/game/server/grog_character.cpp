#include <engine/shared/config.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <mod/game/server/entities/grog.h>

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

#endif

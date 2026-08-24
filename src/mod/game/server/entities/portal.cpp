#include "portal.h"

#include "money.h"

#include <algorithm>

#include <base/math.h>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CPortal::CPortal(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTAL, false, Pos, g_Config.m_SvPortalRadius)
{
	m_Owner = Owner;
	m_StartTick = Server()->Tick();
	m_TeamMask = CClientMask().set();

	for(auto &Id : m_aId)
		Id = Server()->SnapNewId();

	GameWorld()->InsertEntity(this);

	CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
	if(pChr)
	{
		m_TeamMask = pChr->TeamMask();
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, m_TeamMask);
	}
}

CPortal::~CPortal()
{
	for(auto &Id : m_aId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CPortal::ClearOwnerSlot()
{
	if(m_Owner < 0 || m_Owner >= MAX_CLIENTS)
		return;
	CPlayer *pPlayer = GameServer()->m_apPlayers[m_Owner];
	if(!pPlayer)
		return;
	for(int i = 0; i < NUM_PORTALS; i++)
	{
		if(pPlayer->m_apPortal[i] == this)
			pPlayer->m_apPortal[i] = nullptr;
	}
}

void CPortal::Reset()
{
	ClearOwnerSlot();
	GameServer()->CreateDeath(m_Pos, m_Owner, m_TeamMask);
	m_MarkedForDestroy = true;
}

void CPortal::SetLinkedPortal(CPortal *pPortal)
{
	m_pLinkedPortal = pPortal;
	m_LinkedTick = Server()->Tick();
}

void CPortal::Tick()
{
	if(m_MarkedForDestroy)
		return;

	if(m_Owner != -1 && !GameServer()->m_apPlayers[m_Owner])
		m_Owner = -1;

	if(m_Owner != -1 && !GameServer()->GetPlayerChar(m_Owner))
	{
		Reset();
		return;
	}

	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);
	m_TeamMask = pOwnerChr ? pOwnerChr->TeamMask() : CClientMask().set();

	const int UnlinkedLife = Server()->TickSpeed() * g_Config.m_SvPortalDetonation;
	const int LinkedLife = Server()->TickSpeed() * g_Config.m_SvPortalDetonationLinked;
	if((m_LinkedTick == 0 && m_StartTick < Server()->Tick() - UnlinkedLife) ||
		(m_LinkedTick != 0 && m_LinkedTick < Server()->Tick() - LinkedLife))
	{
		Reset();
		return;
	}

	CharactersEnter();
}

void CPortal::CharactersEnter()
{
	if(!m_pLinkedPortal)
		return;

	const float Radius = (float)g_Config.m_SvPortalRadius;

	for(size_t i = 0; i < m_vTeleported.size();)
	{
		CEntity *pEnt = m_vTeleported[i];
		if(!pEnt || distance(pEnt->GetPos(), m_Pos) > Radius + pEnt->GetProximityRadius())
			m_vTeleported.erase(m_vTeleported.begin() + i);
		else
			++i;
	}

	CEntity *apEnts[MAX_CLIENTS];
	const int Num = GameWorld()->FindEntities(m_Pos, Radius, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	for(int i = 0; i < Num; i++)
	{
		CCharacter *pChr = (CCharacter *)apEnts[i];
		if(!pChr || !pChr->IsAlive())
			continue;
		if(std::find(m_vTeleported.begin(), m_vTeleported.end(), pChr) != m_vTeleported.end())
			continue;
		if(GameServer()->Collision()->IntersectLine(m_Pos, pChr->GetPos(), nullptr, nullptr))
			continue;

		pChr->ReleaseHook();
		pChr->SetPosition(m_pLinkedPortal->m_Pos);
		pChr->ResetVelocity();

		CClientMask TeamMask = pChr->TeamMask();
		const int ClientId = pChr->GetPlayer()->GetCid();
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, TeamMask);
		GameServer()->CreateDeath(m_Pos, ClientId, TeamMask);
		GameServer()->CreatePlayerSpawn(m_pLinkedPortal->m_Pos, TeamMask);

		m_pLinkedPortal->m_vTeleported.push_back(pChr);
		m_vTeleported.push_back(pChr);
	}

	// Also pull nearby money drops through linked portals (Phase 6 money entity).
	CEntity *apMoney[32];
	const int MoneyNum = GameWorld()->FindEntities(m_Pos, Radius, apMoney, 32, CGameWorld::ENTTYPE_MONEY);
	for(int i = 0; i < MoneyNum; i++)
	{
		CMoney *pMoney = (CMoney *)apMoney[i];
		if(!pMoney)
			continue;
		if(std::find(m_vTeleported.begin(), m_vTeleported.end(), pMoney) != m_vTeleported.end())
			continue;
		if(GameServer()->Collision()->IntersectLine(m_Pos, pMoney->GetPos(), nullptr, nullptr))
			continue;

		pMoney->m_Pos = m_pLinkedPortal->m_Pos;
		GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, m_TeamMask);
		m_pLinkedPortal->m_vTeleported.push_back(pMoney);
		m_vTeleported.push_back(pMoney);
	}
}

void CPortal::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	const float Radius = (float)g_Config.m_SvPortalRadius;
	const float AngleStep = 2.0f * pi / NUM_SIDE;

	for(int i = 0; i < NUM_SIDE; i++)
	{
		if(!m_aId[i].has_value())
			continue;
		vec2 PartPosStart = m_Pos + vec2(Radius * cosf(AngleStep * i), Radius * sinf(AngleStep * i));
		vec2 PartPosEnd = m_Pos + vec2(Radius * cosf(AngleStep * (i + 1)), Radius * sinf(AngleStep * (i + 1)));
		GameServer()->SnapLaserObject(Context, m_aId[i].value(), PartPosStart, PartPosEnd, Server()->Tick(), -1, LASERTYPE_RIFLE, 0, 0);
	}

	if(!m_pLinkedPortal)
		return;

	for(int i = 0; i < NUM_PARTICLES; i++)
	{
		const int Idx = NUM_SIDE + i;
		if(!m_aId[Idx].has_value())
			continue;
		const float RandomRadius = random_float() * (Radius - 4.0f);
		const float RandomAngle = 2.0f * pi * random_float();
		vec2 ParticlePos = m_Pos + vec2(RandomRadius * cosf(RandomAngle), RandomRadius * sinf(RandomAngle));

		CNetObj_Projectile Proj = {};
		Proj.m_X = (int)ParticlePos.x;
		Proj.m_Y = (int)ParticlePos.y;
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = Server()->Tick();
		Proj.m_Type = WEAPON_HAMMER;
		Server()->SnapNewItem(m_aId[Idx].value(), Proj);
	}
}

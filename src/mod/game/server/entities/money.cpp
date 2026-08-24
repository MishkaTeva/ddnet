#include "money.h"

#include "lasertext.h"

#include <algorithm>

#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CMoney::CMoney(CGameWorld *pGameWorld, vec2 Pos, int64_t Amount, int Owner, float Direction, bool GlobalPickupDelay) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_MONEY, true, Pos, Amount < SMALL_MONEY_AMOUNT ? MONEY_RADIUS_SMALL : MONEY_RADIUS_BIG)
{
	m_Amount = Amount;
	m_Owner = Owner;
	m_GlobalPickupDelay = GlobalPickupDelay;
	m_Vel = vec2(5.f * Direction, Direction == 0 ? 0.f : -5.f);
	m_StartTick = Server()->Tick();
	m_Snap.m_Time = 0.f;
	m_Snap.m_LastTick = Server()->Tick();
	m_TeamMask = CClientMask().set();

	CCharacter *pOwner = GameServer()->GetPlayerChar(Owner);
	if(pOwner)
		m_TeamMask = pOwner->TeamMask();

	for(auto &Id : m_aDotId)
		Id = Server()->SnapNewId();

	GameWorld()->InsertEntity(this);
}

CMoney::~CMoney()
{
	for(auto &Id : m_aDotId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

bool CMoney::SecondsPassed(float Seconds)
{
	return m_StartTick < (Server()->Tick() - Server()->TickSpeed() * Seconds);
}

void CMoney::Tick()
{
	if(m_MarkedForDestroy)
		return;

	CCharacter *pOwnerChr = GameServer()->GetPlayerChar(m_Owner);
	if(pOwnerChr)
		m_TeamMask = pOwnerChr->TeamMask();

	if(m_Amount < SMALL_MONEY_AMOUNT && SecondsPassed(60 * 10))
	{
		m_MarkedForDestroy = true;
		return;
	}

	m_Vel.y += GameWorld()->GetTuning(0)->m_Gravity;
	Collision()->MoveBox(&m_Pos, &m_Vel, vec2(GetRadius() * 2, GetRadius() * 2), vec2(0.5f, 0.5f));

	CCharacter *pClosest = nullptr;
	const bool TwoSecondsPassed = SecondsPassed(2);
	if(!m_GlobalPickupDelay || TwoSecondsPassed)
	{
		pClosest = GameWorld()->ClosestCharacter(m_Pos, RADIUS_FIND_PLAYERS, TwoSecondsPassed ? nullptr : pOwnerChr);
		if(pClosest && pClosest->GetPlayer())
		{
			if(distance(m_Pos, pClosest->GetPos()) < GetRadius() + pClosest->GetProximityRadius())
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "Collected %lld money", (long long)m_Amount);
				GameServer()->SendChatTarget(pClosest->GetPlayer()->GetCid(), aBuf);
				str_format(aBuf, sizeof(aBuf), "+%lld", (long long)m_Amount);
				GameServer()->CreateLaserText(m_Pos, pClosest->GetPlayer()->GetCid(), aBuf, 3);
				GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP, pClosest->TeamMask());
				m_MarkedForDestroy = true;
				return;
			}
			MoveTo(pClosest->GetPos(), RADIUS_FIND_PLAYERS);
		}
	}

	CEntity *apEnts[8];
	const int Num = GameWorld()->FindEntities(m_Pos, RADIUS_FIND_MONEY, apEnts, 8, CGameWorld::ENTTYPE_MONEY);
	for(int i = 0; i < Num; i++)
	{
		CMoney *pMoney = (CMoney *)apEnts[i];
		if(pMoney == this || pMoney->m_MarkedForDestroy)
			continue;
		if(distance(m_Pos, pMoney->GetPos()) < GetRadius() + pMoney->GetRadius())
		{
			m_Amount += pMoney->m_Amount;
			pMoney->m_MarkedForDestroy = true;
			GameServer()->CreateDeath(m_Pos, m_Owner, m_TeamMask);
			break;
		}
		else if(!pClosest)
			MoveTo(pMoney->GetPos(), RADIUS_FIND_MONEY);
	}
}

void CMoney::MoveTo(vec2 Pos, int Radius)
{
	constexpr float MaxFlySpeed = 12.f;
	vec2 Diff = Pos - m_Pos;
	float AddVelX = (Diff.x / Radius * 5);
	m_Vel.x = std::clamp(m_Vel.x + AddVelX, std::min(-MaxFlySpeed, m_Vel.x - AddVelX), std::max(MaxFlySpeed, m_Vel.x + AddVelX));
	float AddVelY = (Diff.y / Radius * 5);
	m_Vel.y = std::clamp(m_Vel.y + AddVelY, std::min(-MaxFlySpeed, m_Vel.y - AddVelY), std::max(MaxFlySpeed, m_Vel.y + AddVelY));
}

void CMoney::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || !GetId().has_value())
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	CNetObj_Projectile Center = {};
	Center.m_X = (int)m_Pos.x;
	Center.m_Y = (int)m_Pos.y;
	Center.m_VelX = 0;
	Center.m_VelY = 0;
	Center.m_StartTick = 0;
	Center.m_Type = WEAPON_SHOTGUN;
	Server()->SnapNewItem(GetId().value(), Center);

	const float AngleStep = 2.0f * pi / GetNumDots();
	m_Snap.m_Time += (Server()->Tick() - m_Snap.m_LastTick) / (float)Server()->TickSpeed();
	m_Snap.m_LastTick = Server()->Tick();

	for(int i = 0; i < GetNumDots(); i++)
	{
		if(!m_aDotId[i].has_value())
			continue;
		vec2 Pos = m_Pos;
		Pos.x += GetRadius() * cosf(m_Snap.m_Time * 10.f + AngleStep * i);
		Pos.y += GetRadius() * sinf(m_Snap.m_Time * 10.f + AngleStep * i);

		CNetObj_Projectile Proj = {};
		Proj.m_X = (int)Pos.x;
		Proj.m_Y = (int)Pos.y;
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = 0;
		Proj.m_Type = WEAPON_HAMMER;
		Server()->SnapNewItem(m_aDotId[i].value(), Proj);
	}
}

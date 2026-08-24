#include "atom.h"

#include <base/math.h>
#include <base/vmath.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CAtom::CAtom(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_ATOM, false, Pos)
{
	m_Owner = Owner;
	for(int i = 0; i < NUM_ATOMS; i++)
	{
		m_aId[i] = Server()->SnapNewId();
		m_aType[i] = (i % 2) ? WEAPON_GRENADE : WEAPON_SHOTGUN;
	}
	GameWorld()->InsertEntity(this);
}

CAtom::~CAtom()
{
	for(auto &Id : m_aId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CAtom::Reset()
{
	m_MarkedForDestroy = true;
}

void CAtom::Tick()
{
	if(m_Owner != -1)
	{
		CCharacter *pChr = GameServer()->GetPlayerChar(m_Owner);
		CPlayer *pPlayer = GameServer()->m_apPlayers[m_Owner];
		if(!pPlayer || (pChr && !pChr->m_Atom))
		{
			Reset();
			return;
		}
		if(!pChr)
			return;
		m_Pos = pChr->GetPos();
	}

	if(++m_AtomPosition >= 60)
		m_AtomPosition = 0;
}

void CAtom::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient))
		return;
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(pOwner && pOwner->GetPlayer() && pOwner->GetPlayer()->IsPaused())
		return;

	vec2 AtomPos;
	AtomPos.x = m_Pos.x + 200 * cosf(m_AtomPosition * pi * 2 / 60);
	AtomPos.y = m_Pos.y + 80 * sinf(m_AtomPosition * pi * 2 / 60);

	for(int i = 0; i < NUM_ATOMS; i++)
	{
		if(!m_aId[i].has_value())
			continue;
		const float Angle = i * 2.f * pi / NUM_ATOMS;
		const vec2 Diff = AtomPos - m_Pos;
		const vec2 Pos = m_Pos + vec2(Diff.x * cosf(Angle) - Diff.y * sinf(Angle), Diff.x * sinf(Angle) + Diff.y * cosf(Angle));
		CNetObj_Projectile Proj = {};
		Proj.m_X = round_to_int(Pos.x);
		Proj.m_Y = round_to_int(Pos.y);
		Proj.m_VelX = 0;
		Proj.m_VelY = 0;
		Proj.m_StartTick = 0;
		Proj.m_Type = m_aType[i];
		Server()->SnapNewItem(m_aId[i].value(), Proj);
	}
}

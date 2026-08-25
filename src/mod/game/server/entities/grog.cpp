#include "grog.h"

#include <algorithm>
#include <cmath>

#include <base/math.h>
#include <base/vmath.h>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CGrog::CGrog(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_GROG, false, Pos, 8)
{
	m_Owner = Owner;
	m_Direction = -1;
	m_LastDirChange = 0;
	m_NumSips = 0;
	m_Lifetime = -1;
	m_ProcessedNudge = false;
	m_LastNudgePos = vec2(-1, -1);
	m_PickupDelay = 0;
	m_Vel = vec2(0, 0);
	m_Dropped = false;
	m_TeamMask = CClientMask().set();

	const vec2 aOffsets[NUM_GROG_LINES][2] = {
		{vec2(0, 0), vec2(0, -40)},
		{vec2(8, -20), vec2(16, -20)},
		{vec2(8, 0), vec2(-8, 0)},
		{vec2(-8, -40), vec2(-8, 0)},
		{vec2(8, -40), vec2(8, 0)},
	};

	std::optional<int> aId[NUM_GROG_LINES];
	for(int i = 0; i < NUM_GROG_LINES; i++)
		aId[i] = Server()->SnapNewId();
	std::sort(std::begin(aId), std::end(aId), [](const std::optional<int> &A, const std::optional<int> &B) {
		return A.value_or(-1) < B.value_or(-1);
	});

	for(int i = 0; i < NUM_GROG_LINES; i++)
	{
		m_aLines[i].m_Id = aId[i];
		m_aLines[i].m_From = aOffsets[i][0];
		m_aLines[i].m_To = aOffsets[i][1];
	}

	CCharacter *pOwner = GameServer()->GetPlayerChar(Owner);
	if(pOwner)
		m_TeamMask = pOwner->TeamMask();

	GameWorld()->InsertEntity(this);
}

CGrog::~CGrog()
{
	for(auto &Line : m_aLines)
	{
		if(Line.m_Id.has_value())
			Server()->SnapFreeId(Line.m_Id.value());
	}
}

void CGrog::ResetInternal(bool CreateDeath)
{
	if(CreateDeath)
		GameServer()->CreateDeath(m_Pos, m_Owner, m_TeamMask);
	m_MarkedForDestroy = true;
}

void CGrog::Reset()
{
	ResetInternal(true);
}

void CGrog::DecreaseNumGrogsHolding()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;

	pOwner->m_NumGrogsHolding--;
	if(pOwner->m_NumGrogsHolding > 0)
		pOwner->m_pGrog = new CGrog(GameWorld(), pOwner->GetPos(), m_Owner);
	else
		pOwner->m_pGrog = nullptr;
}

void CGrog::OnSip()
{
	m_NumSips++;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, m_TeamMask);

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return;

	pOwner->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed() * 2);

	if(m_NumSips >= NUM_GROG_SIPS)
	{
		pOwner->IncreasePermille(3);
		DecreaseNumGrogsHolding();
		ResetInternal(false);
	}
}

bool CGrog::Drop(float Dir, bool OnDeath)
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return false;

	if(m_NumSips)
	{
		if(OnDeath)
		{
			DecreaseNumGrogsHolding();
			ResetInternal(true);
		}
		return false;
	}

	if(GameServer()->Collision()->IsSolid(round_to_int(m_Pos.x), round_to_int(m_Pos.y)))
		m_Pos = pOwner->GetPos();

	m_Lifetime = Server()->TickSpeed() * 300;
	m_PickupDelay = Server()->TickSpeed() * 2;
	m_Dropped = true;
	if(Dir == -3.f)
		Dir = 2.5f * (float)pOwner->GetAimDir();
	m_Vel = vec2(Dir, -4.f);
	DecreaseNumGrogsHolding();
	GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, m_TeamMask);
	return true;
}

void CGrog::Tick()
{
	if(m_MarkedForDestroy)
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(pOwner)
		m_TeamMask = pOwner->TeamMask();

	if(m_Lifetime == -1)
	{
		if(!pOwner)
		{
			ResetInternal(true);
			return;
		}

		const int Dir = pOwner->GetAimDir();
		m_Pos = pOwner->GetPos();
		m_Pos.x += 32.f * Dir;
		if(Dir != m_Direction)
		{
			for(int i = 0; i < NUM_GROG_LINES; i++)
			{
				m_aLines[i].m_From.x *= -1;
				m_aLines[i].m_To.x *= -1;
			}
			m_Direction = Dir;
			m_LastDirChange = Server()->Tick();
			m_ProcessedNudge = false;

			CEntity *apEnts[4];
			const int Num = GameWorld()->FindEntities(m_Pos, 40.f, apEnts, 4, CGameWorld::ENTTYPE_GROG);
			for(int i = 0; i < Num; i++)
			{
				auto *pGrog = static_cast<CGrog *>(apEnts[i]);
				if(pGrog == this || pGrog->m_ProcessedNudge || pGrog->m_Lifetime != -1)
					continue;
				if(pGrog->m_LastDirChange + Server()->TickSpeed() / 3 < Server()->Tick())
					continue;
				if(m_Direction == pGrog->m_Direction || (m_Direction == -1 && m_Pos.x < pGrog->GetPos().x) || (m_Direction == 1 && m_Pos.x > pGrog->GetPos().x))
					continue;
				const vec2 Diff = m_Pos - pGrog->GetPos();
				if(std::abs(Diff.x) > 30.f || std::abs(Diff.x) < 14.f || std::abs(Diff.y) > 37.f)
					continue;

				vec2 CenterPos = (m_Pos + pGrog->GetPos()) / 2;
				CenterPos = vec2((float)round_to_int(CenterPos.x), (float)round_to_int(CenterPos.y));
				m_LastNudgePos = pGrog->m_LastNudgePos = CenterPos;
				GameServer()->CreateHammerHit(CenterPos, m_TeamMask);
				m_ProcessedNudge = true;
			}
		}
	}
	else
	{
		m_Lifetime--;
		if(m_Lifetime <= 0)
		{
			ResetInternal(true);
			return;
		}

		m_Vel.y += GameWorld()->GetTuning(0)->m_Gravity;
		Collision()->MoveBox(&m_Pos, &m_Vel, vec2(16.f, 20.f), vec2(0.5f, 0.5f));

		if(GameLayerClipped(m_Pos) || GameServer()->Collision()->GetCollisionAt(m_Pos.x, m_Pos.y) == TILE_DEATH)
		{
			ResetInternal(true);
			return;
		}

		if(m_PickupDelay > 0)
			m_PickupDelay--;
		Pickup();
	}
}

void CGrog::Pickup()
{
	CEntity *apEnts[MAX_CLIENTS];
	const int Num = GameWorld()->FindEntities(m_Pos, 20.f, apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);

	for(int i = 0; i < Num; i++)
	{
		auto *pChr = static_cast<CCharacter *>(apEnts[i]);
		if(m_PickupDelay > 0 && pChr->GetPlayer() && pChr->GetPlayer()->GetCid() == m_Owner)
			continue;

		if(pChr->AddGrog())
		{
			GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, m_TeamMask);
			ResetInternal(false);
			break;
		}
	}
}

void CGrog::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient, m_Pos))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(pOwner && pOwner->GetPlayer() && pOwner->GetPlayer()->IsPaused())
		return;

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	int Owner = m_Owner;
	if(!Server()->Translate(Owner, SnappingClient))
		Owner = -1;

	for(int i = 0; i < NUM_GROG_LINES; i++)
	{
		if(!m_aLines[i].m_Id.has_value())
			continue;
		const bool IsLiquid = i == GROG_LINE_LIQUID;
		const int StartTick = IsLiquid ? Server()->Tick() : Server()->Tick() - 2;
		vec2 PosTo = m_Pos + m_aLines[i].m_To;
		const vec2 PosFrom = m_Pos + m_aLines[i].m_From;
		if(IsLiquid)
			PosTo.y += m_NumSips * 8.f;

		GameServer()->SnapLaserObject(Context, m_aLines[i].m_Id.value(), PosTo, PosFrom, StartTick, Owner,
			IsLiquid ? LASERTYPE_SHOTGUN : LASERTYPE_FREEZE, 0, 0);
	}
}

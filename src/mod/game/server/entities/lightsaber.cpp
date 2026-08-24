#include "lightsaber.h"

#include <algorithm>

#include <base/math.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CLightsaber::CLightsaber(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_LIGHTSABER, true, Pos)
{
	m_Owner = Owner;
	CCharacter *pChr = GameServer()->GetPlayerChar(Owner);
	if(pChr)
		m_Pos = pChr->GetPos();
	m_EvalTick = Server()->Tick();
	m_TeamMask = CClientMask().set();
	GameWorld()->InsertEntity(this);
}

void CLightsaber::Reset()
{
	if(m_pOwner)
		m_pOwner->m_pLightsaber = nullptr;
	m_MarkedForDestroy = true;
}

void CLightsaber::Extend()
{
	m_Extending = true;
	m_Retracting = false;
}

void CLightsaber::Retract()
{
	m_Retracting = true;
	m_Extending = false;
}

void CLightsaber::PlaySound()
{
	if(m_pOwner && m_SoundTick < Server()->Tick())
	{
		GameServer()->CreateSound(m_Pos, SOUND_LASER_BOUNCE, m_pOwner->TeamMask());
		m_SoundTick = Server()->Tick() + Server()->TickSpeed() / 10;
	}
}

void CLightsaber::HitCharacter()
{
	if(!m_pOwner || m_Length <= 0)
		return;

	std::vector<CCharacter *> HitCharacters = GameWorld()->IntersectedCharacters(m_Pos, m_To, 0.0f, m_pOwner);
	for(CCharacter *pChr : HitCharacters)
	{
		if(!pChr || !pChr->GetPlayer())
			continue;
		const int Cid = pChr->GetPlayer()->GetCid();
		if(Cid < 0 || Cid >= MAX_CLIENTS)
			continue;
		if(m_aLastHit[Cid] >= Server()->Tick())
			continue;
		m_aLastHit[Cid] = Server()->Tick() + Server()->TickSpeed() / 4;
		pChr->TakeDamage(vec2(0.f, 0.f), 1, m_Owner, WEAPON_LASER);
	}
}

void CLightsaber::Tick()
{
	m_pOwner = (m_Owner != -1) ? GameServer()->GetPlayerChar(m_Owner) : nullptr;
	if(!m_pOwner)
	{
		Reset();
		return;
	}

	m_Pos = m_pOwner->GetPos();
	m_TeamMask = m_pOwner->TeamMask();

	if(Server()->Tick() % std::max(1, (int)(Server()->TickSpeed() * 0.15f)) == 0)
		m_EvalTick = Server()->Tick();
	Step();
	HitCharacter();

	if(m_Extending && m_Length < LIGHTSABER_EXTENDED)
	{
		PlaySound();
		m_Length += LIGHTSABER_SPEED;
		if(m_Length >= LIGHTSABER_EXTENDED)
			m_Extending = false;
	}

	if(m_Retracting && m_Length > LIGHTSABER_RETRACTED)
	{
		PlaySound();
		m_Length -= LIGHTSABER_SPEED;
		if(m_Length <= LIGHTSABER_RETRACTED)
			Reset();
	}
}

void CLightsaber::Step()
{
	if(!m_pOwner)
		return;

	const CNetObj_PlayerInput &Input = m_pOwner->Core()->m_Input;
	vec2 Target((float)Input.m_TargetX, (float)Input.m_TargetY);
	if(length(Target) < 0.001f)
		Target = vec2(0.f, -1.f);
	vec2 To2 = m_Pos + normalize(Target) * m_Length;
	GameServer()->Collision()->IntersectLine(m_Pos, To2, &m_To, nullptr);
}

void CLightsaber::Snap(int SnappingClient)
{
	if((NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_To)) || !GetId().has_value())
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;
	if(m_pOwner && m_pOwner->GetPlayer() && m_pOwner->GetPlayer()->IsPaused())
		return;

	int StartTick = m_EvalTick;
	if(StartTick < Server()->Tick() - 2)
		StartTick = Server()->Tick() - 2;
	else if(StartTick > Server()->Tick())
		StartTick = Server()->Tick();

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);
	GameServer()->SnapLaserObject(Context, GetId().value(), m_Pos, m_To, StartTick, m_Owner, LASERTYPE_RIFLE, 0, 0);
}

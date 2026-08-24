#include "portalblocker.h"

#include <algorithm>

#include <base/math.h>
#include <base/vmath.h>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

static bool SegmentsIntersect(vec2 A0, vec2 A1, vec2 B0, vec2 B1)
{
	auto Cross = [](vec2 U, vec2 V) { return U.x * V.y - U.y * V.x; };
	const vec2 R = A1 - A0;
	const vec2 S = B1 - B0;
	const float Denom = Cross(R, S);
	if(absolute(Denom) < 0.0001f)
		return false;
	const float T = Cross(B0 - A0, S) / Denom;
	const float U = Cross(B0 - A0, R) / Denom;
	return T >= 0.f && T <= 1.f && U >= 0.f && U <= 1.f;
}

CPortalBlocker::CPortalBlocker(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CEntity(pGameWorld, CGameWorld::ENTTYPE_PORTAL_BLOCKER, false, Pos)
{
	m_Owner = Owner;
	m_Lifetime = g_Config.m_SvPortalBlockerDetonation * Server()->TickSpeed();
	m_HasStartPos = false;
	m_HasEndPos = false;
	m_StartPos = Pos;
	m_TeamMask = CClientMask().set();

	for(auto &Id : m_aId)
		Id = Server()->SnapNewId();
	std::sort(std::begin(m_aId), std::end(m_aId), [](const std::optional<int> &A, const std::optional<int> &B) {
		return A.value_or(-1) < B.value_or(-1);
	});

	GameWorld()->InsertEntity(this);
}

CPortalBlocker::~CPortalBlocker()
{
	for(auto &Id : m_aId)
	{
		if(Id.has_value())
			Server()->SnapFreeId(Id.value());
	}
}

void CPortalBlocker::Tick()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	m_TeamMask = pOwner ? pOwner->TeamMask() : CClientMask().set();

	if(!m_HasEndPos)
	{
		if(!pOwner || !pOwner->m_IsPortalBlocker)
		{
			if(pOwner)
				pOwner->m_pPortalBlocker = nullptr;
			m_MarkedForDestroy = true;
			return;
		}

		float Angle = pOwner->Core()->m_Angle / 256.0f;
		vec2 CursorPos = pOwner->GetPos() + vec2(cosf(Angle), sinf(Angle)) * 200.f;
		if(m_HasStartPos)
		{
			GameServer()->Collision()->IntersectLine(m_StartPos, CursorPos, nullptr, &CursorPos);
			float Amount = 1.f;
			if(g_Config.m_SvPortalBlockerMaxLength)
			{
				const float Multiples = distance(m_StartPos, CursorPos) / (g_Config.m_SvPortalBlockerMaxLength * 32.f);
				Amount = std::min(1.0f, 1.f / Multiples);
			}
			m_Pos = mix(m_StartPos, CursorPos, Amount);
		}
		else
			m_Pos = CursorPos;
		return;
	}

	if(--m_Lifetime <= 0 || (m_Owner != -1 && !GameServer()->m_apPlayers[m_Owner]))
	{
		GameServer()->CreateDeath(m_StartPos, m_Owner, m_TeamMask);
		GameServer()->CreateDeath(m_Pos, m_Owner, m_TeamMask);
		m_MarkedForDestroy = true;
	}
}

bool CPortalBlocker::CanPlace()
{
	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return false;
	if(GameLayerClipped(m_Pos) || distance(pOwner->GetPos(), m_Pos) > g_Config.m_SvPortalMaxDistance)
		return false;
	if(!m_HasStartPos && GameServer()->Collision()->IntersectLine(pOwner->GetPos(), m_Pos, nullptr, nullptr))
		return false;
	return true;
}

bool CPortalBlocker::OnPlace()
{
	if(!CanPlace())
		return false;

	CCharacter *pOwner = GameServer()->GetPlayerChar(m_Owner);
	if(!pOwner)
		return false;

	if(!m_HasStartPos)
	{
		m_StartPos = m_Pos;
		m_HasStartPos = true;
	}
	else
	{
		m_HasEndPos = true;
		const vec2 CenterPos = (m_Pos + m_StartPos) / 2;
		GameServer()->CreateSound(CenterPos, SOUND_WEAPON_SPAWN, m_TeamMask);
		pOwner->m_IsPortalBlocker = false;
		pOwner->m_pPortalBlocker = nullptr;
	}
	return true;
}

bool CPortalBlocker::BlocksSegment(vec2 From, vec2 To) const
{
	if(!m_HasEndPos)
		return false;
	return SegmentsIntersect(From, To, m_StartPos, m_Pos);
}

void CPortalBlocker::Snap(int SnappingClient)
{
	if(!m_HasEndPos && (SnappingClient != m_Owner || !CanPlace()))
		return;
	if(NetworkClipped(SnappingClient, m_Pos) && NetworkClipped(SnappingClient, m_StartPos))
		return;
	if(SnappingClient != SERVER_DEMO_CLIENT && !m_TeamMask.test(SnappingClient))
		return;

	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);

	int Owner = m_Owner;
	if(!Server()->Translate(Owner, SnappingClient))
		Owner = -1;

	for(int i = 0; i < 2; i++)
	{
		if(!m_aId[i].has_value())
			continue;
		vec2 To = m_Pos;
		vec2 From = m_Pos;
		if(i == 0)
		{
			if(!m_HasStartPos)
				continue;
			To = m_StartPos;
		}
		GameServer()->SnapLaserObject(Context, m_aId[i].value(), To, From, Server()->Tick() - 3, Owner, LASERTYPE_SHOTGUN, 0, 0);
	}
}

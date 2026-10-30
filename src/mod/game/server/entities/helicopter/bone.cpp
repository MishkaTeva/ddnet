#include "bone.h"

#include <engine/server.h>
#include <engine/shared/protocol.h>

#include <game/server/gamecontext.h>

void SBone::Snap(int SnappingClient)
{
	if(!m_Enabled || !m_pEntity || m_Id < 0)
		return;

	const vec2 ParentPos = m_pEntity->GetPos();
	const int Version = GameServer()->GetClientVersion(SnappingClient);
	const bool Sixup = Server()->IsSixup(SnappingClient);
	CSnapContext Context(Version, Sixup, SnappingClient);
	GameServer()->SnapLaserObject(Context, m_Id,
		ParentPos + m_To, ParentPos + m_From,
		Server()->Tick() - 4 + m_Thickness, -1, m_Color, 0, 0);
}

void STrail::Snap(int SnappingClient)
{
	if(!m_Enabled || !m_pEntity || m_Id < 0 || !m_pPos)
		return;

	const vec2 ParentPos = m_pEntity->GetPos();
	CNetObj_Projectile Proj = {};
	Proj.m_X = round_to_int(ParentPos.x + m_pPos->x);
	Proj.m_Y = round_to_int(ParentPos.y + m_pPos->y);
	Proj.m_VelX = 0;
	Proj.m_VelY = 0;
	Proj.m_StartTick = Server()->Tick();
	Proj.m_Type = WEAPON_HAMMER;
	Server()->SnapNewItem(m_Id, Proj);
}

void SHeart::Snap(int SnappingClient)
{
	if(!m_Enabled || !m_pEntity || m_Id < 0)
		return;

	const vec2 ParentPos = m_pEntity->GetPos();
	CNetObj_Pickup Pickup = {};
	Pickup.m_X = round_to_int(ParentPos.x + m_Pos.x);
	Pickup.m_Y = round_to_int(ParentPos.y + m_Pos.y);
	Pickup.m_Type = POWERUP_HEALTH;
	Server()->SnapNewItem(m_Id, Pickup);
}

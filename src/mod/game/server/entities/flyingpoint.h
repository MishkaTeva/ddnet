#ifndef MOD_GAME_SERVER_ENTITIES_FLYINGPOINT_H
#define MOD_GAME_SERVER_ENTITIES_FLYINGPOINT_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

class CFlyingPoint : public CEntity
{
	vec2 m_InitialVel;
	float m_InitialAmount = 1.f;
	int m_Owner = -1;
	CClientMask m_TeamMask;
	int m_To = -1;
	vec2 m_ToPos = vec2(-1, -1);

public:
	CFlyingPoint(CGameWorld *pGameWorld, vec2 Pos, int To, int Owner, vec2 InitialVel, vec2 ToPos = vec2(-1, -1));

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif

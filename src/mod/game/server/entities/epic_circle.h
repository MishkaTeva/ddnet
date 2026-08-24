#ifndef MOD_GAME_SERVER_ENTITIES_EPIC_CIRCLE_H
#define MOD_GAME_SERVER_ENTITIES_EPIC_CIRCLE_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>

class CEpicCircle : public CEntity
{
	enum
	{
		MAX_PARTICLES = 9
	};

	CClientMask m_TeamMask;
	int m_Owner;
	std::optional<int> m_aId[MAX_PARTICLES];
	vec2 m_aRotatePos[MAX_PARTICLES]{};

public:
	CEpicCircle(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CEpicCircle() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif

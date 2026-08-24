#ifndef MOD_GAME_SERVER_ENTITIES_STAFF_IND_H
#define MOD_GAME_SERVER_ENTITIES_STAFF_IND_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>

class CStaffInd : public CEntity
{
	enum
	{
		BALL,
		ARMOR,
		BALL_FRONT,
		NUM_IDS
	};

	std::optional<int> m_aId[NUM_IDS];
	vec2 m_aPos[2];
	CClientMask m_TeamMask;
	int m_Owner;
	float m_Dist = 0.f;
	bool m_BallFirst = true;

public:
	CStaffInd(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CStaffInd() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif

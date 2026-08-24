#ifndef MOD_GAME_SERVER_ENTITIES_TRAIL_H
#define MOD_GAME_SERVER_ENTITIES_TRAIL_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <deque>
#include <optional>

enum
{
	NUM_TRAILS = 20,
	TRAIL_DIST = 20,
};

class CTrail : public CEntity
{
	struct HistoryPoint
	{
		vec2 m_Pos;
		float m_Dist;
		HistoryPoint(vec2 Pos, float Dist) :
			m_Pos(Pos), m_Dist(Dist) {}
	};

	std::optional<int> m_aId[NUM_TRAILS];
	vec2 m_aPos[NUM_TRAILS]{};
	std::deque<HistoryPoint> m_TrailHistory;
	float m_TrailHistoryLength = 0.f;
	int m_Owner;
	bool m_Initialized = false;

public:
	CTrail(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CTrail() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif

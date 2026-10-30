#ifndef MOD_GAME_SERVER_ENTITIES_MISSILE_H
#define MOD_GAME_SERVER_ENTITIES_MISSILE_H

#include <game/server/entity.h>

#include <optional>

// Slim helicopter missile: flies, sparks, explodes on hit/timeout.
class CMissile : public CEntity
{
	enum
	{
		NUM_SPARKS = 8,
	};

	int m_Owner = -1;
	int m_LifeSpan = 0;
	int m_IgnitionTime = 0;
	int m_ExplosionsLeft = -1;
	vec2 m_Vel{};
	vec2 m_Direction{};
	std::optional<int> m_aSparkId[NUM_SPARKS];
	vec2 m_aSparkPos[NUM_SPARKS]{};
	int m_aSparkLife[NUM_SPARKS]{};

	void TriggerExplosions();

public:
	CMissile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Direction, int SpanTicks = 150);
	CMissile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Vel, vec2 Direction, int SpanTicks);
	~CMissile() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif

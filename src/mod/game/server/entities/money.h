#ifndef MOD_GAME_SERVER_ENTITIES_MONEY_H
#define MOD_GAME_SERVER_ENTITIES_MONEY_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>

enum
{
	NUM_MONEY_DOTS_SMALL = 4,
	NUM_MONEY_DOTS_BIG = 6,
	SMALL_MONEY_AMOUNT = 10000,
	MONEY_RADIUS_BIG = 14,
	MONEY_RADIUS_SMALL = 10,
	RADIUS_FIND_MONEY = 32 * 24,
	RADIUS_FIND_PLAYERS = 32 * 12,
};

class CMoney : public CEntity
{
	struct
	{
		float m_Time = 0.f;
		int m_LastTick = 0;
	} m_Snap;

	int64_t m_Amount = 0;
	int m_Owner = -1;
	int m_StartTick = 0;
	bool m_GlobalPickupDelay = false;
	vec2 m_Vel = vec2(0, 0);
	CClientMask m_TeamMask;
	std::optional<int> m_aDotId[NUM_MONEY_DOTS_BIG];

	bool SecondsPassed(float Seconds);
	void MoveTo(vec2 Pos, int Radius);
	int GetRadius() const { return m_Amount < SMALL_MONEY_AMOUNT ? MONEY_RADIUS_SMALL : MONEY_RADIUS_BIG; }
	int GetNumDots() const { return m_Amount < SMALL_MONEY_AMOUNT ? NUM_MONEY_DOTS_SMALL : NUM_MONEY_DOTS_BIG; }

public:
	CMoney(CGameWorld *pGameWorld, vec2 Pos, int64_t Amount, int Owner = -1, float Direction = 0, bool GlobalPickupDelay = false);
	~CMoney() override;

	int64_t GetAmount() const { return m_Amount; }
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif

#ifndef MOD_GAME_SERVER_ENTITIES_LIGHTNINGLASER_H
#define MOD_GAME_SERVER_ENTITIES_LIGHTNINGLASER_H

#include <optional>
#include <vector>

#include <game/server/entity.h>

class CLightningLaser : public CEntity
{
	enum
	{
		POS_START = 0,
		POS_END,
		POS_COUNT,
	};

	vec2 m_Dir;
	int m_Owner;

	int m_Lifespan;
	int m_StartLifespan;

	float m_StartTick;

	struct STarget
	{
		bool m_IsAlive;
		int m_Id;
		vec2 m_Pos;

		void Reset()
		{
			m_IsAlive = false;
			m_Id = -1;
			m_Pos = vec2(0, 0);
		}
	};

	STarget m_Target;

	std::vector<std::optional<int>> m_aIds;
	std::vector<std::array<vec2, POS_COUNT>> m_aPositions;

	int m_Count;
	int m_Length;

public:
	CLightningLaser(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, int Owner);
	~CLightningLaser() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

	void InitTarget();
	void UpdateDirection(vec2 From);
	bool TargetBehindWall(vec2 From);
	void HitCharacter();
	void GenerateLights();

	bool TargetAlive();
};

#endif

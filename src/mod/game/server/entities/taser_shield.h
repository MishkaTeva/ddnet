#ifndef MOD_GAME_SERVER_ENTITIES_TASER_SHIELD_H
#define MOD_GAME_SERVER_ENTITIES_TASER_SHIELD_H

#include <optional>

#include <game/server/entity.h>

class CTaserShield : public CEntity
{
	enum
	{
		MAX_SHIELDS = 3
	};

	CClientMask m_TeamMask;
	int m_Owner;
	float m_SpawnDelay;

	struct SShieldData
	{
		std::optional<int> m_Id;
		vec2 m_Pos;
		float m_Lifespan;
		bool m_Used;
	};
	SShieldData m_aShieldData[MAX_SHIELDS];
	void SpawnNewShield();

public:
	CTaserShield(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CTaserShield() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif

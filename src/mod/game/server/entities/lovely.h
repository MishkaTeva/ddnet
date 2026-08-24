#ifndef MOD_GAME_SERVER_ENTITIES_LOVELY_H
#define MOD_GAME_SERVER_ENTITIES_LOVELY_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>

class CLovely : public CEntity
{
	enum
	{
		MAX_HEARTS = 4
	};

	CClientMask m_TeamMask;
	int m_Owner;
	float m_SpawnDelay;

	struct SLovelyData
	{
		std::optional<int> m_Id;
		vec2 m_Pos = vec2(0, 0);
		float m_Lifespan = -1;
	};
	SLovelyData m_aLovelyData[MAX_HEARTS];
	void SpawnNewHeart();

public:
	CLovely(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CLovely() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif

#ifndef MOD_GAME_SERVER_ENTITIES_CLOCK_H
#define MOD_GAME_SERVER_ENTITIES_CLOCK_H

#include <game/server/entity.h>

#include <optional>

enum
{
	CLOCK_SECOND = 0,
	CLOCK_MINUTE,
	CLOCK_HOUR,
};

class CClock : public CEntity
{
	struct HandInfo
	{
		int m_Length = 0;
		float m_Rotation = 0.f;
		vec2 m_To = vec2(0, 0);
	} m_Hand[3];

	std::optional<int> m_Id2;
	std::optional<int> m_Id3;

	void SetHandRotations();
	int GetSnapId(int Hand) const;

public:
	CClock(CGameWorld *pGameWorld, vec2 Pos);
	~CClock() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
};

#endif

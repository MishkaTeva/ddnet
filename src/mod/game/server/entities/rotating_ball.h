#ifndef MOD_GAME_SERVER_ENTITIES_ROTATING_BALL_H
#define MOD_GAME_SERVER_ENTITIES_ROTATING_BALL_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>

class CRotatingBall : public CEntity
{
	CClientMask m_TeamMask;
	int m_Owner;
	std::optional<int> m_Id2;

	int m_RotateDelay;
	int m_LaserDirAngle = 0;
	int m_LaserInputDir = 0;
	bool m_IsRotating = true;

	vec2 m_LaserPos = vec2(0, 0);
	vec2 m_ProjPos = vec2(0, 0);
	int m_TableDirV[2][2];

public:
	CRotatingBall(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CRotatingBall() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif

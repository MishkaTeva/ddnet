#ifndef MOD_GAME_SERVER_ENTITIES_SPEEDUP_H
#define MOD_GAME_SERVER_ENTITIES_SPEEDUP_H

#include <game/server/entity.h>

#include <optional>

class CSpeedup : public CEntity
{
	enum
	{
		DOT_CENTER = 0,
		DOT_END_BOTTOM = 5,
		DOT_END_TOP = 6,
		NUM_DOTS = 7
	};

	struct SDot
	{
		std::optional<int> m_Id;
		vec2 m_Pos{};
		void Rotate(int Angle) { m_Pos = rotate(m_Pos, Angle * (pi / 180.0f)); }
	} m_aDots[NUM_DOTS];

	int m_Angle = 0;
	int m_Force = 0;
	int m_MaxSpeed = 0;
	bool m_Collision = true;
	float m_Distance = 0.f;
	float m_CurrentDist = 0.f;

	void Rotate(int Angle);

public:
	CSpeedup(CGameWorld *pGameWorld, vec2 Pos, float Angle, int Force, int MaxSpeed, bool Collision = true);
	~CSpeedup() override;
	void ResetCollision(bool Remove = false);
	void Tick() override;
	void Snap(int SnappingClient) override;
	void SetAngle(int Angle);
	int GetAngle() const { return m_Angle; }
	int GetForce() const { return m_Force; }
	int GetMaxSpeed() const { return m_MaxSpeed; }
};

#endif

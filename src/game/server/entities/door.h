/* (c) Shereef Marzouk. See "licence DDRace.txt" and the readme.txt in the root of the distribution for more information. */
#ifndef GAME_SERVER_ENTITIES_DOOR_H
#define GAME_SERVER_ENTITIES_DOOR_H

#include <game/server/entity.h>

class CGameWorld;

class CDoor : public CEntity
{
	vec2 m_To;
	void ResetCollision();
	void ClearCollision();
	int m_Length;
	float m_Rotation;
	vec2 m_Direction;
	bool m_Collision;

public:
	CDoor(CGameWorld *pGameWorld, vec2 Pos, float Rotation, int Length,
		int Number, bool Collision = true);
	~CDoor() override;

	void Reset() override;
	void Snap(int SnappingClient) override;

	float GetRotation() const { return m_Rotation; }
	int GetLength() const { return m_Length; }
	void SetDirection(float Rotation);
	void SetLength(int Length);
	void UpdateEndpoint();
};

#endif // GAME_SERVER_ENTITIES_DOOR_H

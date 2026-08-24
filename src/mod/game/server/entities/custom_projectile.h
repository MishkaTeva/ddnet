#ifndef MOD_GAME_SERVER_ENTITIES_CUSTOM_PROJECTILE_H
#define MOD_GAME_SERVER_ENTITIES_CUSTOM_PROJECTILE_H

#include <game/server/entity.h>

enum
{
	CUSTOM_PROJ_NOT_COLLIDED = 0,
	CUSTOM_PROJ_COLLIDED_ONCE,
	CUSTOM_PROJ_COLLIDED_TWICE,
};

// Visual subtypes beyond vanilla weapon ids (F-DDrace plasma / heart gun).
enum
{
	CUSTOM_PROJ_TYPE_PLASMA = 100,
	CUSTOM_PROJ_TYPE_HEART = 101,
};

class CCustomProjectile : public CEntity
{
public:
	CCustomProjectile(CGameWorld *pGameWorld, int Owner, vec2 Pos, vec2 Dir, bool Freeze,
		bool Explosive, bool Unfreeze, bool Bloody, bool Ghost, bool Spooky, int Type,
		float Lifetime = 6.0f, float Accel = 1.0f, float Speed = 10.0f);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	vec2 m_Core;
	vec2 m_PrevPos;
	vec2 m_Direction;

	int m_EvalTick;
	int m_LifeTime;

	CClientMask m_TeamMask;
	CCharacter *m_pOwner;
	int m_Owner;

	bool m_Freeze;
	bool m_Unfreeze;
	bool m_Bloody;
	bool m_Ghost;
	bool m_Spooky;
	bool m_Explosive;
	int m_Type;

	float m_Accel;

	int m_CollisionState;

	void HitCharacter();
	void Move();
	int DamageForType() const;
};

#endif

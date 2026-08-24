#ifndef MOD_GAME_SERVER_ENTITIES_PICKUP_DROP_H
#define MOD_GAME_SERVER_ENTITIES_PICKUP_DROP_H

#include <generated/protocol.h>

#include <game/server/entity.h>

// Slim F-DDrace pickup drop: health / armor / vanilla weapons.
// Full specials, batteries, weapon limits come later with advanced_entity.
class CPickupDrop : public CEntity
{
	static constexpr float PHYS_SIZE = 16.f;

	int m_Type;
	int m_Weapon;
	int m_Owner;
	int m_Lifetime;
	int m_PickupDelay;
	vec2 m_Vel;
	CClientMask m_TeamMask;

public:
	CPickupDrop(CGameWorld *pGameWorld, vec2 Pos, int Type, int Owner, float Direction, int Lifetime = 300, int Weapon = WEAPON_GUN);

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

private:
	int IsCharacterNear();
	void Pickup();
};

#endif

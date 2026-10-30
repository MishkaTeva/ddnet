#ifndef MOD_GAME_SERVER_ENTITIES_ADVANCED_ENTITY_H
#define MOD_GAME_SERVER_ENTITIES_ADVANCED_ENTITY_H

#include <game/server/entity.h>

// Slim base for vehicles / dropped advanced props (helicopter, flags later).
class CAdvancedEntity : public CEntity
{
public:
	CAdvancedEntity(CGameWorld *pGameWorld, int Objtype, vec2 Pos, vec2 Size, int Owner = -1, bool CheckDeath = true);
	~CAdvancedEntity() override = default;

	void Reset() override;
	void Tick() override;

	CCharacter *GetOwner();
	vec2 GetSize() const { return m_Size; }
	vec2 GetVel() const { return m_Vel; }
	void SetVel(vec2 Vel) { m_Vel = Vel; }
	void SetPrevPos(vec2 Pos) { m_PrevPos = Pos; }
	int GetOwnerId() const { return m_Owner; }
	void SetOwner(int Owner) { m_Owner = Owner; }

protected:
	bool IsGrounded();
	void HandleDropped();

	vec2 m_Vel{};
	vec2 m_PrevPos{};
	vec2 m_Size{};
	int m_Owner = -1;
	int m_TuneZone = 0;
	bool m_CheckDeath = true;
	bool m_CheckGameLayerClipped = true;
	bool m_Gravity = true;
	float m_Elasticity = 0.5f;
	CClientMask m_TeamMask;
	int m_DDTeam = 0;
};

#endif

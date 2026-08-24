#ifndef MOD_GAME_SERVER_ENTITIES_PORTAL_H
#define MOD_GAME_SERVER_ENTITIES_PORTAL_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>
#include <vector>

enum
{
	PORTAL_FIRST = 0,
	PORTAL_SECOND = 1,
	NUM_PORTALS = 2,
};

class CPortal : public CEntity
{
	enum
	{
		NUM_SIDE = 12,
		NUM_PARTICLES = 12,
		NUM_PORTAL_IDS = NUM_SIDE + NUM_PARTICLES,
	};

	int m_StartTick = 0;
	int m_LinkedTick = 0;
	CPortal *m_pLinkedPortal = nullptr;
	CClientMask m_TeamMask;
	int m_Owner = -1;
	std::optional<int> m_aId[NUM_PORTAL_IDS];
	std::vector<CEntity *> m_vTeleported;

	void CharactersEnter();
	void ClearOwnerSlot();

public:
	CPortal(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CPortal() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }

	void SetLinkedPortal(CPortal *pPortal);
	CPortal *LinkedPortal() const { return m_pLinkedPortal; }
	int GetOwner() const { return m_Owner; }
};

#endif

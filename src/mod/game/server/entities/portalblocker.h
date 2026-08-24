#ifndef MOD_GAME_SERVER_ENTITIES_PORTAL_BLOCKER_H
#define MOD_GAME_SERVER_ENTITIES_PORTAL_BLOCKER_H

#include <optional>

#include <game/server/entity.h>

class CPortalBlocker : public CEntity
{
	CClientMask m_TeamMask;
	int m_Owner;
	int m_Lifetime;
	std::optional<int> m_aId[2];

	vec2 m_StartPos;
	bool m_HasStartPos;
	bool m_HasEndPos;

	bool CanPlace();

public:
	CPortalBlocker(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CPortalBlocker() override;

	void Tick() override;
	void Snap(int SnappingClient) override;

	bool OnPlace();
	bool IsPlaced() const { return m_HasEndPos; }
	vec2 GetStartPos() const { return m_StartPos; }
	void Cancel() { m_MarkedForDestroy = true; }

	// True if segment From->To crosses this placed blocker.
	bool BlocksSegment(vec2 From, vec2 To) const;
};

#endif

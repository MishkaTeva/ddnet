#ifndef MOD_GAME_SERVER_ENTITIES_JAIL_RELEASE_H
#define MOD_GAME_SERVER_ENTITIES_JAIL_RELEASE_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>

class CGameContext;

class CJailRelease : public CEntity
{
	enum
	{
		NUM_RING = 12,
		NUM_RAYS = 6,
		NUM_PARTICLES = 10,
		NUM_IDS = NUM_RING + NUM_RAYS + NUM_PARTICLES + 4,

		PHASE_APPEAR = 0,
		PHASE_BREAK,
		PHASE_LIFT,
		PHASE_EXIT,
		NUM_PHASES
	};

	static const int s_aPhaseTicks[NUM_PHASES];

	int m_Victim;
	int m_Phase;
	int m_PhaseStartTick;
	vec2 m_VictimStartPos;
	CClientMask m_TeamMask;
	std::optional<int> m_aId[NUM_IDS];

	float PhaseProgress();
	void AdvancePhase();
	void Finish();
	void DoReleaseCleanup();
	void UpdateVictimControl();
	void SnapLaser(int SnappingClient, int Id, vec2 From, vec2 To, int Type);

public:
	CJailRelease(CGameWorld *pGameWorld, vec2 PortalPos, int Victim);
	~CJailRelease() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Victim; }

	static bool FindPortalPos(CGameContext *pGameServer, vec2 From, vec2 *pOut);
};

#endif

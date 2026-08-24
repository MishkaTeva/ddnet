#ifndef MOD_GAME_SERVER_ENTITIES_JAIL_ARREST_H
#define MOD_GAME_SERVER_ENTITIES_JAIL_ARREST_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>

class CGameContext;

class CJailArrest : public CEntity
{
	enum
	{
		NUM_HOLE_SIDE = 12,
		NUM_HOLE_PARTICLES = 10,
		NUM_TENTACLES = 2,
		NUM_TENTACLE_SEGS = 8,
		NUM_IDS = NUM_HOLE_SIDE + NUM_HOLE_PARTICLES + NUM_TENTACLES * NUM_TENTACLE_SEGS,

		PHASE_APPEAR = 0,
		PHASE_REACH,
		PHASE_DRAG,
		PHASE_SWALLOW,
		NUM_PHASES
	};

	static const int s_aPhaseTicks[NUM_PHASES];

	int m_Victim;
	int m_StartTick;
	int m_Phase;
	int m_PhaseStartTick;
	vec2 m_GrabStartPos;
	CClientMask m_TeamMask;
	std::optional<int> m_aId[NUM_IDS];

	float PhaseProgress();
	void AdvancePhase();
	void Finish();
	void UpdateVictimControl();
	vec2 TentaclePoint(int Tentacle, float t, float Reach);

public:
	CJailArrest(CGameWorld *pGameWorld, vec2 Pos, int Victim);
	~CJailArrest() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Victim; }

	static bool FindHolePos(CGameContext *pGameServer, vec2 From, vec2 *pOut);
};

#endif

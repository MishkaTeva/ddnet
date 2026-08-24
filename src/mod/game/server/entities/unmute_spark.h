#ifndef MOD_GAME_SERVER_ENTITIES_UNMUTE_SPARK_H
#define MOD_GAME_SERVER_ENTITIES_UNMUTE_SPARK_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>


class CGameContext;

// Reverse lightning rising from the tee when unmuted — cosmetic only, no freeze.
class CUnmuteSpark : public CEntity
{
	enum
	{
		NUM_BOLT_SEGS = 8,
		NUM_BRANCHES = 4,
		NUM_FLASH = 6,
		NUM_IDS = NUM_BOLT_SEGS + NUM_BRANCHES * 2 + NUM_FLASH,

		PHASE_CHARGE = 0, // sparks gather around tee
		PHASE_RISE,       // bolt climbs skyward
		PHASE_FLASH,      // residual glow above
		NUM_PHASES
	};

	static const int s_aPhaseTicks[NUM_PHASES];

	int m_Victim;
	int m_Phase;
	int m_PhaseStartTick;
	CClientMask m_TeamMask;
	std::optional<int> m_aId[NUM_IDS];
	int m_Seed;

	float PhaseProgress();
	void AdvancePhase();
	void Finish();
	void FollowVictim();
	vec2 BoltPoint(int Seg, int NumSegs, float Jag);
	void SnapLaser(int SnappingClient, int Id, vec2 From, vec2 To, int Type);
	bool SnapLaserIdx(int SnappingClient, int &Idx, vec2 From, vec2 To, int Type);

public:
	CUnmuteSpark(CGameWorld *pGameWorld, vec2 Pos, int Victim);
	~CUnmuteSpark();

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Victim; }
};

#endif

#ifndef MOD_GAME_SERVER_ENTITIES_MUTE_GAG_H
#define MOD_GAME_SERVER_ENTITIES_MUTE_GAG_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>


class CGameContext;

// Lightning strike over the tee when muted — cosmetic only (mute already applied), no freeze.
class CMuteGag : public CEntity
{
	enum
	{
		NUM_BOLT_SEGS = 8,
		NUM_BRANCHES = 4,
		NUM_FLASH = 6,
		NUM_IDS = NUM_BOLT_SEGS + NUM_BRANCHES * 2 + NUM_FLASH,

		PHASE_CHARGE = 0, // cloud / spark gather above
		PHASE_STRIKE,     // main bolt hits
		PHASE_FLASH,      // residual arcs
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
	CMuteGag(CGameWorld *pGameWorld, vec2 Pos, int Victim);
	~CMuteGag();

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Victim; }
};

#endif

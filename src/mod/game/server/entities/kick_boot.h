#ifndef MOD_GAME_SERVER_ENTITIES_KICK_BOOT_H
#define MOD_GAME_SERVER_ENTITIES_KICK_BOOT_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>


class CGameContext;

// Boot swings in, kicks the tee, then queues a deferred Drop (like ban plunger).
class CKickBoot : public CEntity
{
	enum
	{
		NUM_BOOT = 12,
		NUM_TRAIL = 6,
		NUM_IDS = NUM_BOOT + NUM_TRAIL + 2,

		PHASE_WINDUP = 0, // boot pulls back
		PHASE_SWING,      // kick contact
		PHASE_FLY,        // tee flies off
		NUM_PHASES
	};

	static const int s_aPhaseTicks[NUM_PHASES];

	int m_Victim;
	char m_aReason[128];
	vec2 m_StartPos;
	int m_Phase;
	int m_PhaseStartTick;
	CClientMask m_TeamMask;
	std::optional<int> m_aId[NUM_IDS];

	float PhaseProgress();
	void AdvancePhase();
	void Finish();
	void UpdateVictimControl();
	vec2 BootPos();
	void SnapLaser(int SnappingClient, int Id, vec2 From, vec2 To, int Type);
	bool SnapLaserIdx(int SnappingClient, int &Idx, vec2 From, vec2 To, int Type);

public:
	CKickBoot(CGameWorld *pGameWorld, vec2 Pos, int Victim, const char *pReason);
	~CKickBoot();

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Victim; }
};

#endif

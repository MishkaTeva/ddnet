#ifndef MOD_GAME_SERVER_ENTITIES_BAN_PLUNGER_TOILET_H
#define MOD_GAME_SERVER_ENTITIES_BAN_PLUNGER_TOILET_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>

class CGameContext;

class CBanPlungerToilet : public CEntity
{
	enum
	{
		// Side-profile toilet outline + classic plunger
		NUM_TOILET_LASERS = 18,
		NUM_PLUNGER_LASERS = 14,
		NUM_IDS = NUM_TOILET_LASERS + NUM_PLUNGER_LASERS + 4, // spare IDs, never overrun

		PHASE_LIFT = 0,   // raise tee + toilet into open air
		PHASE_APPEAR,     // plunger + BAN settle in
		PHASE_PRESS,      // plunger shoves tee into bowl
		PHASE_FLUSH,      // sink / flush
		NUM_PHASES
	};

	static const int s_aPhaseTicks[NUM_PHASES];

	int m_Victim;
	int m_BanSeconds;
	char m_aReason[128];
	vec2 m_VictimStartPos; // ground start
	vec2 m_AirPos;         // clear air scene anchor (player seat)

	int m_Phase;
	int m_PhaseStartTick;
	CClientMask m_TeamMask;
	std::optional<int> m_aId[NUM_IDS];

	float PhaseProgress();
	float LiftAmount();
	void AdvancePhase();
	void Finish();
	void UpdateVictimControl();
	vec2 SceneAnchor();
	vec2 ToiletOrigin();
	vec2 PlungerCupPos();
	void SnapLaser(int SnappingClient, int Id, vec2 From, vec2 To, int Type);
	bool SnapLaserIdx(int SnappingClient, int &Idx, vec2 From, vec2 To, int Type);

public:
	CBanPlungerToilet(CGameWorld *pGameWorld, vec2 AirPos, int Victim, int BanSeconds, const char *pReason);
	~CBanPlungerToilet() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Victim; }

	static bool FindPos(CGameContext *pGameServer, vec2 From, vec2 *pOut);
};

#endif

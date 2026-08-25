#ifndef MOD_GAME_SERVER_ENTITIES_GROG_H
#define MOD_GAME_SERVER_ENTITIES_GROG_H

#include <optional>

#include <game/server/entity.h>

// Slim grog: held/dropped drink without full CAdvancedEntity.
class CGrog : public CEntity
{
	enum
	{
		GROG_LINE_LIQUID,
		GROG_LINE_HANDLE,
		GROG_LINE_BOTTOM,
		GROG_LINE_LEFT,
		GROG_LINE_RIGHT,
		NUM_GROG_LINES,

		NUM_GROG_SIPS = 5,
	};

	int m_Direction;
	int64_t m_LastDirChange;
	bool m_ProcessedNudge;
	int m_NumSips;
	int m_Lifetime;

	struct SGrogLine
	{
		std::optional<int> m_Id;
		vec2 m_From;
		vec2 m_To;
	} m_aLines[NUM_GROG_LINES];

	int m_PickupDelay;
	vec2 m_Vel;
	CClientMask m_TeamMask;
	bool m_Dropped;
	int m_Owner;

	void Pickup();
	void ResetInternal(bool CreateDeath);
	void DecreaseNumGrogsHolding();

public:
	CGrog(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CGrog() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;

	void OnSip();
	bool Drop(float Dir = -3.f, bool OnDeath = false);

	vec2 m_LastNudgePos;
};

#endif

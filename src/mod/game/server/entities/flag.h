#ifndef MOD_GAME_SERVER_ENTITIES_FLAG_H
#define MOD_GAME_SERVER_ENTITIES_FLAG_H

#include "advanced_entity.h"

class CFlag : public CAdvancedEntity
{
	static constexpr int ms_PhysSize = 16;

	int m_Team = 0;
	bool m_AtStand = true;
	vec2 m_StandPos{};
	int m_Carrier = -1;
	int m_LastCarrier = -1;
	int m_DropTick = 0;

public:
	CFlag(CGameWorld *pGameWorld, int Team, vec2 Pos);
	~CFlag() override;

	CCharacter *GetCarrier();
	CCharacter *GetLastCarrier();
	void SetLastCarrier(int LastCarrier) { m_LastCarrier = LastCarrier; }
	bool IsAtStand() const { return m_AtStand; }
	void SetAtStand(bool AtStand) { m_AtStand = AtStand; }
	int GetTeam() const { return m_Team; }

	void Grab(int NewCarrier);
	void Drop(int Dir = 0);
	void Reset(bool Init);
	void Reset() override { Reset(false); }

	void Tick() override;
	void TickDeferred() override;
	void Snap(int SnappingClient) override;
};

#endif

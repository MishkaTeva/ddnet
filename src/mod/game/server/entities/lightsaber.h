#ifndef MOD_GAME_SERVER_ENTITIES_LIGHTSABER_H
#define MOD_GAME_SERVER_ENTITIES_LIGHTSABER_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

class CCharacter;

enum
{
	LIGHTSABER_SPEED = 15,
	LIGHTSABER_RETRACTED = 0,
	LIGHTSABER_EXTENDED = 200,
};

class CLightsaber : public CEntity
{
	int m_Length = LIGHTSABER_RETRACTED;
	vec2 m_To = vec2(0, 0);
	bool m_Extending = false;
	bool m_Retracting = false;
	int m_Owner = -1;
	CCharacter *m_pOwner = nullptr;
	int m_EvalTick = 0;
	int m_SoundTick = 0;
	CClientMask m_TeamMask;
	int m_aLastHit[MAX_CLIENTS]{};

	void PlaySound();
	void HitCharacter();
	void Step();

public:
	CLightsaber(CGameWorld *pGameWorld, vec2 Pos, int Owner);

	void Extend();
	void Retract();
	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif

#ifndef MOD_GAME_SERVER_ENTITIES_PLAYERCOUNTER_H
#define MOD_GAME_SERVER_ENTITIES_PLAYERCOUNTER_H

#include <game/server/entity.h>

class CLaserText;

class CPlayerCounter : public CEntity
{
	int m_Port;
	int m_PlayerCount;
	int64_t m_LastUpdate;
	CLaserText *m_pLaserText;

	void Update(int PlayerCount);

public:
	CPlayerCounter(CGameWorld *pGameWorld, vec2 Pos, int Port);
	~CPlayerCounter() override;

	void Tick() override;
	void Snap(int SnappingClient) override;

	void OnUpdate(int Port, int PlayerCount);
};

#endif

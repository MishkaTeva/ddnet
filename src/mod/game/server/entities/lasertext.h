#ifndef MOD_GAME_SERVER_ENTITIES_LASERTEXT_H
#define MOD_GAME_SERVER_ENTITIES_LASERTEXT_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

class CLaserChar : public CEntity
{
public:
	CLaserChar(CGameWorld *pGameWorld) :
		CEntity(pGameWorld, CGameWorld::ENTTYPE_LASERTEXT, true)
	{
	}

	vec2 m_FromPos;
};

class CLaserText : public CEntity
{
public:
	CLaserText(CGameWorld *pGameWorld, vec2 Pos, int Owner, int AliveTicks, const char *pText, int TextLen, float CharPointOffset = 15.f, float CharOffsetFactor = 3.5f);
	~CLaserText() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }

private:
	void MakeLaser(char Char, int CharOffset, int &CharCount);

	float m_PosOffsetCharPoints = 15.f;
	float m_PosOffsetChars = 0.f;
	int m_Owner = -1;
	CClientMask m_TeamMask;
	int m_AliveTicks = 0;
	int m_CurTicks = 0;
	int m_StartTick = 0;
	char *m_pText = nullptr;
	int m_TextLen = 0;
	CLaserChar **m_ppChars = nullptr;
	int m_CharNum = 0;
};

#endif

#ifndef MOD_GAME_SERVER_ENTITIES_ATOM_H
#define MOD_GAME_SERVER_ENTITIES_ATOM_H

#include <engine/shared/protocol.h>

#include <game/server/entity.h>

#include <optional>

enum
{
	NUM_ATOMS = 6
};

class CAtom : public CEntity
{
	std::optional<int> m_aId[NUM_ATOMS];
	int m_aType[NUM_ATOMS];
	int m_AtomPosition = 0;
	int m_Owner;

public:
	CAtom(CGameWorld *pGameWorld, vec2 Pos, int Owner);
	~CAtom() override;

	void Reset() override;
	void Tick() override;
	void Snap(int SnappingClient) override;
	int GetOwnerId() const override { return m_Owner; }
};

#endif

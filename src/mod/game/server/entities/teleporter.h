#ifndef MOD_GAME_SERVER_ENTITIES_TELEPORTER_H
#define MOD_GAME_SERVER_ENTITIES_TELEPORTER_H

#include <optional>

#include <game/server/entity.h>

// Runtime map teleporter (plot editor / RCON). Updates tele layer via SetTeleporter.
class CTeleporter : public CEntity
{
	enum
	{
		TELE_RADIUS = 16,
		NUM_CIRCLE = 5,
		NUM_PARTICLES = 1,
		NUM_TELEPORTER_IDS = NUM_CIRCLE + NUM_PARTICLES,
	};

	struct
	{
		vec2 m_Pos;
		float m_Time;
		float m_LastTime;
	} m_Snap;

	std::optional<int> m_aId[NUM_TELEPORTER_IDS];
	int m_Type;
	bool m_Collision;

public:
	CTeleporter(CGameWorld *pGameWorld, vec2 Pos, int Type, int Number, bool Collision = true);
	~CTeleporter() override;

	void Reset() override;
	void ResetCollision(bool Remove = false);
	void Tick() override;
	void Snap(int SnappingClient) override;

	int GetType() const { return m_Type; }
	int GetNumber() const { return m_Number; }
};

#endif

#ifndef MOD_GAME_SERVER_ENTITIES_BUTTON_H
#define MOD_GAME_SERVER_ENTITIES_BUTTON_H

#include <game/server/entity.h>

#include <optional>

// Slim plot-draw button: laser snap + single-slot door tile via AddDoorTile.
class CButton : public CEntity
{
	enum
	{
		POINT_TOP,
		POINT_RIGHT,
		POINT_BOTTOM,
		POINT_LEFT,
		NUM_SIDES
	};

	struct
	{
		std::optional<int> m_Id;
		vec2 m_Pos{};
	} m_aSides[NUM_SIDES];

	int m_Number = 0;
	bool m_Collision = true;

public:
	CButton(CGameWorld *pGameWorld, vec2 Pos, int Number, bool Collision = true);
	~CButton() override;
	void ResetCollision(bool Remove = false);
	void Snap(int SnappingClient) override;
	int GetNumber() const { return m_Number; }
};

#endif

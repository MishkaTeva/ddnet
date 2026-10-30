#ifndef MOD_GAME_SERVER_ENTITIES_HELICOPTER_BONE_H
#define MOD_GAME_SERVER_ENTITIES_HELICOPTER_BONE_H

#include <base/vmath.h>

#include <game/server/entity.h>

#include <generated/protocol.h>

// F-DDrace helicopter bone / trail / heart snap helpers.
struct SBone
{
	CEntity *m_pEntity = nullptr;
	int m_Id = -1;
	vec2 m_InitFrom{};
	vec2 m_InitTo{};
	int m_InitColor = LASERTYPE_RIFLE;
	int m_InitThickness = 5;
	bool m_Enabled = true;

	vec2 m_From{};
	vec2 m_To{};
	int m_Color = LASERTYPE_RIFLE;
	int m_Thickness = 5;

	SBone(CEntity *pEntity = nullptr, int SnapId = -1,
		float FromX = 0, float FromY = 0,
		float ToX = 0, float ToY = 0,
		int Thickness = 5) :
		SBone(pEntity, SnapId, vec2(FromX, FromY), vec2(ToX, ToY), Thickness)
	{
	}

	SBone(CEntity *pEntity, int SnapId, vec2 From, vec2 To, int Thickness = 5) :
		m_pEntity(pEntity), m_Id(SnapId), m_InitFrom(From), m_InitTo(To), m_InitColor(LASERTYPE_RIFLE), m_InitThickness(Thickness), m_Enabled(true), m_From(From), m_To(To), m_Color(LASERTYPE_RIFLE), m_Thickness(Thickness)
	{
	}

	IServer *Server() { return m_pEntity->Server(); }
	CGameContext *GameServer() { return m_pEntity->GameServer(); }
	float GetLength() { return distance(m_To, m_From); }

	void AssignEntityAndId(CEntity *pEntity, int SnapId)
	{
		m_pEntity = pEntity;
		m_Id = SnapId;
	}

	void UpdateLine(vec2 From, vec2 To)
	{
		m_From = From;
		m_To = To;
	}

	void Flip()
	{
		m_From.x *= -1;
		m_To.x *= -1;
	}

	void Rotate(float Angle)
	{
		m_From = rotate(m_From, Angle);
		m_To = rotate(m_To, Angle);
	}

	void Scale(float Factor)
	{
		m_InitFrom *= Factor;
		m_InitTo *= Factor;
		m_From *= Factor;
		m_To *= Factor;
	}

	void Snap(int SnappingClient);
};

struct STrail
{
	CEntity *m_pEntity = nullptr;
	int m_Id = -1;
	vec2 *m_pPos = nullptr;
	bool m_Enabled = true;

	STrail() = default;
	STrail(CEntity *pEntity, int SnapId, vec2 *pPos) :
		m_pEntity(pEntity), m_Id(SnapId), m_pPos(pPos), m_Enabled(true)
	{
	}

	IServer *Server() { return m_pEntity->Server(); }
	void Snap(int SnappingClient);
};

struct SHeart
{
	CEntity *m_pEntity = nullptr;
	vec2 m_Pos{};
	int m_Id = -1;
	bool m_Enabled = true;

	SHeart() = default;
	SHeart(CEntity *pEntity, int SnapId, vec2 Pos) :
		m_pEntity(pEntity), m_Pos(Pos), m_Id(SnapId), m_Enabled(true)
	{
	}

	IServer *Server() { return m_pEntity->Server(); }
	void Snap(int SnappingClient);
};

#endif

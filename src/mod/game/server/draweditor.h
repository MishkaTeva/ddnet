// CDrawEditor subset for F-DDrace-on-DDNet.
// Categories: pickups, laser walls, doors/buttons, speedups, teleporters, transform, helicopter.
// Plot object caps/registry wired; no presets / TilePlace yet.

#ifndef MOD_GAME_SERVER_DRAWEDITOR_H
#define MOD_GAME_SERVER_DRAWEDITOR_H

#include <base/vmath.h>
#include <generated/protocol.h>

#include <algorithm>
#include <optional>
#include <vector>

class CCharacter;
class CEntity;
class CGameContext;
class IServer;

class CDrawEditor
{
public:
	enum
	{
		// Pickup (order matters for SetPickup)
		DRAW_PICKUP_HEART = 0,
		DRAW_PICKUP_SHIELD,
		DRAW_PICKUP_HAMMER,
		DRAW_PICKUP_GUN,
		DRAW_PICKUP_SHOTGUN,
		DRAW_PICKUP_GRENADE,
		DRAW_PICKUP_LASER,
		NUM_DRAW_PICKUPS,

		// Laser walls (length is a setting; thickness/color deferred)
		LASERWALL_LENGTH = 0,
		NUM_LASERWALL_SETTINGS,

		// Door / button (CAT_LASERDOORS — buttons only in this port)
		LASERDOOR_NUMBER = 0,
		LASERDOOR_MODE,
		NUM_LASERDOOR_SETTINGS,

		// Speedup
		SPEEDUP_FORCE = 0,
		SPEEDUP_MAXSPEED,
		NUM_SPEEDUP_SETTINGS,

		// Teleporters
		TELEPORTER_NUMBER = 0,
		TELEPORTER_MODE,
		TELEPORTER_EVIL,
		NUM_TELEPORTERS_SETTINGS,

		TELE_MODE_TOGGLE = 0,
		TELE_MODE_IN,
		TELE_MODE_OUT,
		TELE_MODE_WEAPON,
		TELE_MODE_HOOK,
		NUM_TELE_MODES,

		// Transform (move/copy/erase only — no presets)
		TRANSFORM_MOVE = 0,
		TRANSFORM_COPY,
		TRANSFORM_ERASE,
		NUM_TRANSFORM_SETTINGS,

		TRANSFORM_STATE_SETTING_FIRST = 0,
		TRANSFORM_STATE_SETTING_SECOND,
		TRANSFORM_STATE_CONFIRM,
		TRANSFORM_STATE_RUNNING,

		CAT_UNINITIALIZED = -1,
		CAT_PICKUPS,
		CAT_LASERWALLS,
		CAT_LASERDOORS,
		CAT_SPEEDUPS,
		CAT_TELEPORTER,
		CAT_TRANSFORM,
		CAT_HELICOPTER,
		NUM_DRAW_CATEGORIES,

		MAX_DRAW_NUMBERS = 255,
		MAX_LASERWALL_LENGTH = 32,
	};

private:
	CGameContext *GameServer() const;
	IServer *Server() const;

	CCharacter *m_pCharacter = nullptr;
	int GetCid() const;

	CEntity *CreateEntity(bool Preview = false);
	CEntity *CreateTransformEntity(CEntity *pTemplate);

	bool IsCategoryLaser() const { return m_Category == CAT_LASERWALLS || m_Category == CAT_LASERDOORS; }

	void SetAngle(float Angle);
	void AddAngle(float Add);
	void AddLength(int Add);
	float ClampRotation(float Rotation);
	int ClampAngle(int Angle);

	void HandleInput();
	struct DrawInput
	{
		int m_Jump = 0;
		int m_Hook = 0;
		int m_Direction = 0;
	};
	DrawInput m_Input;
	DrawInput m_PrevInput;

	bool CanPlace(CEntity *pAt = nullptr);
	bool CanRemove(CEntity *pEntity);
	CEntity *ClosestDrawable(float Radius);
	bool RemoveEntity(CEntity *pEntity);
	void CollectTransformablesInArea();

	int GetOwnPlotID() const;
	int GetCursorPlotID() const;
	int CurrentPlotID() const;

	bool IsCategoryAllowed(int Category);
	vec2 RoundToTile(vec2 Pos) const;
	vec2 CalcPlacePos() const;

	vec2 m_Pos{};
	int m_Entity = -1;
	bool m_RoundPos = true;

	bool m_Erasing = false;
	bool m_Selecting = false;
	int64_t m_EditStartTick = 0;

	void SendWindow();
	const char *GetCategory(int Category);
	const char *GetPickupName(int Pickup);
	void SetPickup(int Pickup);
	int m_Category = CAT_UNINITIALIZED;

	int GetNumSettings();
	void SetSetting(int Setting);
	const char *FormatSetting(const char *pSetting, int Setting);
	int m_Setting = 0;

	const char *GetTeleporterMode();
	int GetTeleporterType();

	void StopTransform(bool Silent = false);
	void SnapTransformArea();

	struct
	{
		int m_Type = POWERUP_HEALTH;
		int m_SubType = 0;
	} m_Pickup;

	struct
	{
		int m_Number = 0;
		bool m_ButtonMode = true;
		float m_Angle = pi / 2.f; // radians for CDoor
		int m_Length = 3; // tiles
	} m_Laser;

	struct
	{
		int m_Force = 2;
		int m_MaxSpeed = 0;
		int m_Angle = 0;
	} m_Speedup;

	struct
	{
		int m_Number = 0;
		bool m_Evil = true;
		int m_Mode = TELE_MODE_OUT;
	} m_Teleporter;

	struct SSelectedEnt
	{
		CEntity *m_pEnt = nullptr;
		vec2 m_Offset{};
	};
	struct
	{
		int m_Angle = 0;
		int m_State = TRANSFORM_STATE_SETTING_FIRST;
		vec2 m_aPos[2]{};
		std::vector<SSelectedEnt> m_vSelected;
		std::vector<SSelectedEnt> m_vPreview; // templates + offsets (no ghost entities)
		std::optional<int> m_aSnapId[4];
		vec2 TopLeft() const
		{
			return vec2(std::min(m_aPos[0].x, m_aPos[1].x), std::min(m_aPos[0].y, m_aPos[1].y));
		}
		vec2 BottomRight() const
		{
			return vec2(std::max(m_aPos[0].x, m_aPos[1].x), std::max(m_aPos[0].y, m_aPos[1].y));
		}
		bool Includes(vec2 Pos) const
		{
			return Pos.x >= TopLeft().x - 1 && Pos.x <= BottomRight().x + 1 &&
			       Pos.y >= TopLeft().y - 1 && Pos.y <= BottomRight().y + 1;
		}
	} m_Transform;

	void SetPreview();
	void RemovePreview();
	void UpdatePreview();
	CEntity *m_pPreview = nullptr;

public:
	void Init(CCharacter *pChr);
	void Tick();
	void Snap();

	bool Active();
	bool Selecting() { return Active() && m_Selecting; }
	void SetCategory(int Category);

	void OnPlayerFire();
	void OnWeaponSwitch();
	void OnPlayerDeath();
	void OnPlayerKill();
	void OnInput(CNetObj_PlayerInput *pNewInput);

	bool OnSnapPreview(CEntity *pEntity);
};

#endif // MOD_GAME_SERVER_DRAWEDITOR_H

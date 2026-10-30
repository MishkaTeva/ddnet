#ifndef MOD_GAME_SERVER_ENTITIES_HELICOPTER_H
#define MOD_GAME_SERVER_ENTITIES_HELICOPTER_H

#include "helicopter/bone.h"
#include "helicopter/helicopter_turret.h"

#include "advanced_entity.h"

#define HELICOPTER_DEFAULT_SCALE 0.8f
#define HELICOPTER_PHYSSIZE (vec2(80, 128) * HELICOPTER_DEFAULT_SCALE)

// Bone helicopter from F-DDrace (silhouette, propellers, turret, hearts/damage, build anim).
class CHelicopter : public CAdvancedEntity
{
	enum
	{
		NUM_BONES_BODY = 14,
		NUM_BONES_PROPELLERS_TOP = 2,
		NUM_BONES_PROPELLERS_BACK = 2,
		NUM_BONES_PROPELLERS = NUM_BONES_PROPELLERS_TOP + NUM_BONES_PROPELLERS_BACK,
		NUM_BONES = NUM_BONES_BODY + NUM_BONES_PROPELLERS,
		NUM_TRAILS = 2,
		NUM_HEARTS = 4,
		NUM_BUILD_IDS = 3,
	};

	float m_MaxHealth = 100.f;
	float m_Health = 100.f;
	bool m_EngineOn = false;
	bool m_Flipped = false;
	float m_Angle = 0.f;
	vec2 m_Accel{};
	int m_LastFire = 0;

	float m_Scale = HELICOPTER_DEFAULT_SCALE;
	float m_BackPropellerRadius = 16.f;
	float m_TopPropellerRadius = 70.f;
	SBone m_aBones[NUM_BONES];
	STrail m_aTrails[NUM_TRAILS];
	SHeart m_aHearts[NUM_HEARTS];
	CVehicleTurret *m_pTurret = nullptr;

	int64_t m_ShowHeartsUntil = 0;
	int64_t m_LastDamage = 0;
	int64_t m_LastEnvironmentalDamage = 0;
	int m_LastKnownOwner = -1;
	int m_ExplosionsLeft = -1;

	// Tile spawn / build animation
	int m_DelayTurretType = -1;
	vec2 m_InitialPosition{};
	int m_SpawnTick = -1;
	bool m_Build = false;
	float m_BuildHeight = 0.f;
	int m_aBuildIDs[NUM_BUILD_IDS] = {-1, -1, -1};
	float m_BuildBottom = 0.f;
	float m_BuildTop = -70.f;
	float m_BuildLeft = 0.f;
	float m_BuildRight = 0.f;
	float m_BuildTotalHeight = 0.f;
	float m_BuildTotalWidth = 0.f;

	SBone *Body() { return &m_aBones[0]; }
	SBone *TopPropeller() { return &m_aBones[NUM_BONES_BODY]; }
	SBone *BackPropeller() { return &m_aBones[NUM_BONES_BODY + NUM_BONES_PROPELLERS_TOP]; }

	int AllocSnapId();
	void FreeBones();
	void InitBody();
	void InitPropellers();
	void InitHearts();
	void InitBuild();
	void InitUnbuilt();
	void BuildHelicopter();
	void SpinPropellers();
	void ResetAndTurnOff();
	void FlipBones();
	void ApplyInput();
	void UpdateHearts();
	void RegenerateHelicopter();
	void UpdateVisualDamage();
	void DamageInFreeze();
	void DamageInWall();
	void HandleExplosions();
	void Explode();
	bool TryRespawnNewHelicopter();

public:
	CHelicopter(CGameWorld *pGameWorld, vec2 Pos, int Owner = -1);
	CHelicopter(CGameWorld *pGameWorld, vec2 Pos, int Owner, int TurretType, bool Build = false, int Number = -1);
	~CHelicopter() override;

	void Tick() override;
	void Snap(int SnappingClient) override;
	void Reset() override;

	bool TryEnter(int ClientId);
	void Eject();
	bool Mount(int ClientId) { return TryEnter(ClientId); }
	void Dismount() { Eject(); }

	float GetScale() const { return m_Scale; }
	bool IsFlipped() const { return m_Flipped; }
	float Angle() const { return m_Angle; }
	bool IsExploding() const { return m_ExplosionsLeft > -1; }
	bool IsBuilding() const { return m_Build; }
	bool IsSpawning() const { return m_SpawnTick > -1; }
	bool PlacedByTile() const { return m_Number >= 0 && m_DelayTurretType >= TURRETTYPE_NONE; }
	bool IsRegenerating();

	bool AttachTurret(CVehicleTurret *pTurret);
	void DestroyTurret();
	void TakeDamage(float Damage, vec2 HitPos, int FromId);
};

#endif

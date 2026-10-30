//
// Created by ModTeam on 11/04/2025.
// Ported from F-DDrace helicopter turret.
//

#include "helicopter_turret.h"

#include "../helicopter.h"
#include "../missile.h"

#include <base/math.h>
#include <base/vmath.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <generated/protocol.h>

#include <game/server/entities/character.h>
#include <game/server/entities/projectile.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

void CVehicleTurret::SetFlipped(bool Flipped)
{ // SetFlipped instead of Flip because if turret is attached later to match the helicopter flip, flip |ovo|
	if(Flipped == m_Flipped)
		return;

	m_Flipped = !m_Flipped;
	m_Angle *= -1.f;
	m_PivotAngle *= -1.f;
	m_Pivot.x *= -1.f;
	m_TurretBone.Flip();
	for(int i = 0; i < m_NumBones; i++)
		m_apBones[i].Flip();
}

void CVehicleTurret::Rotate(float Angle)
{
	m_Angle += Angle;
	m_PivotAngle += Angle;
	m_Pivot = rotate(m_Pivot, Angle);
	m_TurretBone.Rotate(Angle);
	for(int i = 0; i < m_NumBones; i++)
		m_apBones[i].Rotate(Angle);
}

void CVehicleTurret::AimTurret()
{
	if(!m_pHelicopter->GetOwner())
		return;

	CCharacter *pOwner = m_pHelicopter->GetOwner();
	const vec2 CursorPos = pOwner->GetPos() + vec2(pOwner->GetLatestInput().m_TargetX, pOwner->GetLatestInput().m_TargetY);
	vec2 AimFromTurret = (CursorPos - m_pHelicopter->GetPos() - m_TurretBone.m_To) * (m_Flipped ? -1.f : 1.f);
	float TargetAngle = (atan2f(AimFromTurret.y, AimFromTurret.x) / pi * 180.f);

	float TargetAngleClamped = std::clamp(TargetAngle, m_Angle - m_AimingRange, m_Angle + m_AimingRange);
	float CorrectionAngle = (TargetAngleClamped - m_PivotAngle) * 0.15f; // Slow rotation

	RotateTurret(CorrectionAngle);
}

void CVehicleTurret::RotateTurret(float ByAngle)
{
	m_PivotAngle += ByAngle;

	// Rotate turret
	m_TurretBone.m_To -= m_Pivot;
	m_TurretBone.m_From -= m_Pivot;
	m_TurretBone.Rotate(ByAngle);
	m_TurretBone.m_To += m_Pivot;
	m_TurretBone.m_From += m_Pivot;

	for(int i = 0; i < m_NumBones; i++)
	{
		m_apBones[i].m_To -= m_Pivot;
		m_apBones[i].m_From -= m_Pivot;
		m_apBones[i].Rotate(ByAngle);
		m_apBones[i].m_To += m_Pivot;
		m_apBones[i].m_From += m_Pivot;
	}
}

void CVehicleTurret::FireTurret()
{
	if(!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime || !m_Shooting ||
		Server()->Tick() - m_LastShot <= m_ShootingCooldown)
		return;

	m_LastShot = Server()->Tick();

	vec2 StartingPos = m_pHelicopter->GetPos() + m_TurretBone.m_From;
	vec2 Direction = GetTurretDirection();

	vec2 ProjectileDirection = normalize(Direction + vec2(-Direction.y, Direction.x));
	const int OwnerId = m_pHelicopter->GetOwner()->GetPlayer()->GetCid();
	new CProjectile(
		GameWorld(),
		WEAPON_GUN,
		OwnerId,
		StartingPos,
		ProjectileDirection,
		Server()->TickSpeed() * 2,
		false,
		false,
		-1,
		ProjectileDirection);
}

vec2 CVehicleTurret::GetTurretDirection()
{
	return normalize(m_TurretBone.m_From - m_TurretBone.m_To);
}

CVehicleTurret::CVehicleTurret(int TurretType, int NumBones,
	const SBone &TurretBone, const vec2 &Pivot,
	float AimingRange, int ShootingCooldown)
{
	m_TurretType = TurretType;
	m_pHelicopter = nullptr;
	m_apBones = new SBone[NumBones];
	m_NumBones = NumBones;

	// Unsure
	m_TurretBone = TurretBone;
	m_Pivot = Pivot;
	m_Length = m_TurretBone.GetLength();

	m_Flipped = false;
	m_Angle = 0.f;

	m_AimPosition = vec2(0.f, 0.f);
	m_PivotAngle = 0.f;
	m_AimingRange = AimingRange;

	m_Shooting = false;
	m_LastShot = 0;
	m_ShootingCooldown = ShootingCooldown;

	m_Scale = 1.f;
}

CVehicleTurret::~CVehicleTurret()
{
	if(m_pHelicopter) // if helicopter was never set, ids were never set either
		for(int i = 0; i < m_NumBones; i++)
			if(m_apBones[i].m_Id != -1)
				Server()->SnapFreeId(m_apBones[i].m_Id);
	delete[] m_apBones;
}

IServer *CVehicleTurret::Server()
{
	return m_pHelicopter->Server();
}

CGameWorld *CVehicleTurret::GameWorld()
{
	return m_pHelicopter->GameWorld();
}

CGameContext *CVehicleTurret::GameServer()
{
	return m_pHelicopter->GameServer();
}

bool CVehicleTurret::TryBindHelicopter(CHelicopter *pHelicopter)
{ // returns true on passed ownership, on failure clean up yourself
	if(pHelicopter == nullptr)
		return false;

	if(m_pHelicopter == nullptr) // really only assign owner once
	{
		m_pHelicopter = pHelicopter;
		for(int i = 0; i < m_NumBones; i++)
			m_apBones[i].AssignEntityAndId(m_pHelicopter, Server()->SnapNewId().value_or(-1));

		// Match helicopter posture initially
		ApplyScale(m_pHelicopter->GetScale());
		SetFlipped(m_pHelicopter->IsFlipped());
		Rotate(m_pHelicopter->Angle());
		return true;
	}
	return false;
}

void CVehicleTurret::ApplyScale(float TurretScale)
{
	// Experimental
	m_Scale *= TurretScale;
	m_Length *= TurretScale;
	m_Pivot *= TurretScale;
	m_TurretBone.Scale(TurretScale);
	for(int i = 0; i < m_NumBones; i++)
		m_apBones[i].Scale(TurretScale);
	//
}

void CVehicleTurret::Tick()
{
	//    if (!m_pHelicopter) don't even try to run this function without a heli
	//        return;

	AimTurret();
	FireTurret();
}

void CVehicleTurret::Snap(int SnappingClient)
{
	if(!m_pHelicopter)
		return;

	for(int i = 0; i < m_NumBones; i++)
		m_apBones[i].Snap(SnappingClient);
}

void CVehicleTurret::OnInput(CNetObj_PlayerInput *pNewInput)
{
	if(!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime)
		return;

	m_AimPosition = vec2((float)pNewInput->m_TargetX, (float)pNewInput->m_TargetY);
	m_Shooting = pNewInput->m_Fire % 2 == 1;
}

//

void CMinigunTurret::InitCluster()
{
	UpdateClusterBones();

	for(int i = 0; i < NUM_BONES_CLUSTER; i++)
	{
		Cluster()[i].m_Color = LASERTYPE_FREEZE;
		Cluster()[i].m_Thickness = 3;
	}
}

void CMinigunTurret::UpdateClusterBones()
{
	vec2 Direction = GetTurretDirection(); // gun direction, length: 1
	vec2 Perpendicular = vec2(-Direction.y, Direction.x); // direction perpendicular of the gun, length: 1

	for(int i = 0; i < NUM_BONES_CLUSTER; i++)
	{
		float CurrentBoneRotation = m_ClusterRotation + AngleDiff() * (float)i * (m_Flipped ? -1.f : 1.f);
		float VerticalOffset = sinf(CurrentBoneRotation) * m_ClusterVerticalRange;
		float HorizontalOffset = cosf(CurrentBoneRotation) * m_ClusterHorizontalRange;

		// This will move each bone in an elliptical motion, even if the gun is rotated it will look as intended
		vec2 VerticalDirection = Perpendicular * VerticalOffset;
		vec2 HorizontalDirection = Direction * HorizontalOffset;

		vec2 Combined = VerticalDirection + HorizontalDirection;
		vec2 BarrelStartPos = m_TurretBone.m_To + Combined;
		vec2 BarrelEndPos = m_TurretBone.m_From + Combined;

		// Start and end are flipped to have laserball on our side of the gun
		Cluster()[i].m_To = BarrelStartPos;
		Cluster()[i].m_From = BarrelEndPos;
	}
}

void CMinigunTurret::SpinCluster()
{
	if(m_pHelicopter->GetOwner() && !m_pHelicopter->GetOwner()->m_FreezeTime && m_Shooting)
		m_ClusterSpeed += (float)Server()->TickSpeed() / 5000.f;
	m_ClusterSpeed *= 0.98f;
	m_ClusterRotation += m_ClusterSpeed;

	UpdateClusterBones();
}

void CMinigunTurret::InitRetainer()
{
	float Radius = m_RetainerRadius;
	vec2 InitialPosition = m_TurretBone.m_From + vec2(m_RetainerPosition, 0);
	for(int i = 0; i < NUM_BONES_RETAINER; i++)
	{
		vec2 FromPosition = InitialPosition;
		vec2 ToPosition = InitialPosition + vec2(0.f, Radius);

		Retainer()[i].m_To = ToPosition;
		Retainer()[i].m_From = FromPosition;
		Retainer()[i].m_Color = LASERTYPE_FREEZE;
		Retainer()[i].m_Thickness = 2;
		Radius *= -1.f;
	}
}

void CMinigunTurret::SetFlipped(bool Flipped)
{ // SetFlipped instead of Flip because if turret is attached later to match the helicopter flip, flip |ovo|
	if(Flipped == m_Flipped)
		return;

	m_Flipped = !m_Flipped;
	m_Angle *= -1.f;

	m_PivotAngle *= -1.f;

	m_Pivot.x *= -1.f;
	m_TurretBone.Flip(); // Cluster bones update dynamically
	for(int i = 0; i < NUM_BONES_RETAINER; i++)
		Retainer()[i].Flip();
}

void CMinigunTurret::Rotate(float Angle)
{
	m_Angle += Angle;
	m_PivotAngle += Angle;
	m_Pivot = rotate(m_Pivot, Angle);
	m_TurretBone.Rotate(Angle); // Cluster bones update dynamically
	for(int i = 0; i < NUM_BONES_RETAINER; i++)
		Retainer()[i].Rotate(Angle);
}

void CMinigunTurret::RotateTurret(float Angle)
{
	m_PivotAngle += Angle;

	// Rotate turret
	m_TurretBone.m_To -= m_Pivot;
	m_TurretBone.m_From -= m_Pivot;
	m_TurretBone.Rotate(Angle);
	m_TurretBone.m_To += m_Pivot;
	m_TurretBone.m_From += m_Pivot;

	// Rotate retainer
	for(int i = 0; i < NUM_BONES_RETAINER; i++)
	{
		Retainer()[i].m_To -= m_Pivot;
		Retainer()[i].m_From -= m_Pivot;
		Retainer()[i].Rotate(Angle);
		Retainer()[i].m_To += m_Pivot;
		Retainer()[i].m_From += m_Pivot;
	}
}

void CMinigunTurret::FireTurret()
{
	if(!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime || !m_Shooting ||
		Server()->Tick() - m_LastShot <= m_ShootingCooldown || m_ClusterSpeed < 0.3f)
		return;

	m_LastShot = Server()->Tick();

	vec2 StartingPos = m_pHelicopter->GetPos() + Cluster()[m_ShootingBarrelIndex].m_From;
	vec2 Direction = GetTurretDirection();

	float SpreadRandomness = (float)(rand() % 21 - 10) / 400.f;
	vec2 ProjectileDirection = normalize(Direction + vec2(-Direction.y, Direction.x) * SpreadRandomness);
	const int OwnerId = m_pHelicopter->GetOwner()->GetPlayer()->GetCid();
	new CProjectile(
		GameWorld(),
		WEAPON_GUN,
		OwnerId,
		StartingPos,
		ProjectileDirection,
		Server()->TickSpeed() * 2,
		false,
		true,
		SOUND_GRENADE_EXPLODE,
		ProjectileDirection);
	GameServer()->CreateSound(StartingPos, SOUND_GRENADE_FIRE, m_pHelicopter->GetOwner()->TeamMask());

	m_ShootingBarrelIndex = (m_ShootingBarrelIndex + 1) % NUM_BONES_CLUSTER;
	m_pHelicopter->SetVel(m_pHelicopter->GetVel() + -Direction * 0.5f);
}

CMinigunTurret::CMinigunTurret() :
	CVehicleTurret(TURRETTYPE_MINIGUN, NUM_BONES,
		SBone(nullptr, -1, vec2(70.f, 50.f), vec2(-34.f, 50.f)),
		vec2(4.f, 50.f), 35.f, 7)
{
	m_pHelicopter = nullptr;

	m_ClusterRotation = 0.f;
	m_ClusterSpeed = 0.f;
	m_ClusterVerticalRange = 8.f;
	m_ClusterHorizontalRange = 5.f;

	m_RetainerPosition = -14.f; // from the tip of the turret
	m_RetainerRadius = 9.f;

	m_ShootingBarrelIndex = 0;

	InitCluster();
	InitRetainer();
}

CMinigunTurret::~CMinigunTurret()
{
}

void CMinigunTurret::ApplyScale(float TurretScale)
{
	// Experimental
	m_Length *= TurretScale;
	m_Pivot *= TurretScale;
	m_RetainerPosition *= TurretScale;
	m_RetainerRadius *= TurretScale;
	m_ClusterHorizontalRange *= TurretScale;
	m_ClusterVerticalRange *= TurretScale;
	m_TurretBone.Scale(TurretScale);
	for(int i = 0; i < m_NumBones; i++) // Cluster will scale from m_TurretBone automatically
		m_apBones[i].Scale(TurretScale);
	//
}

void CMinigunTurret::Tick()
{
	//    if (!m_pHelicopter) don't even try to run this function without a heli
	//        return;

	AimTurret();
	FireTurret();

	SpinCluster();
}

void CMinigunTurret::Snap(int SnappingClient)
{
	if(!m_pHelicopter)
		return;

	for(int i = 0; i < m_NumBones; i++)
		m_apBones[i].Snap(SnappingClient);
}

void CMinigunTurret::OnInput(CNetObj_PlayerInput *pNewInput)
{
	if(!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime)
		return;

	m_AimPosition = vec2((float)pNewInput->m_TargetX, (float)pNewInput->m_TargetY);
	m_Shooting = pNewInput->m_Fire % 2 == 1;
}

//

void CLauncherTurret::InitBones()
{
	UpdateBones();

	Shaft()[0].m_Thickness = 2;
	Shaft()[1].m_Thickness = 2;
	Shaft()[2].m_Thickness = 2;

	Retainer()[0].m_Thickness = 2;
	Retainer()[1].m_Thickness = 2;

	for(int i = 0; i < NUM_BONES; i++)
		m_apBones[i].m_Color = LASERTYPE_FREEZE;
	Ejector()[0].m_Color = LASERTYPE_SHOTGUN;
}

void CLauncherTurret::UpdateBones()
{
	vec2 Direction = GetTurretDirection(); // gun direction, length: 1
	vec2 Perpendicular = vec2(-Direction.y, Direction.x); // direction perpendicular of the gun, length: 1

	vec2 PerpendicularShaft = Perpendicular * m_ShaftRadius;
	vec2 PerpendicularRetainer = Perpendicular * m_RetainerRadius;
	vec2 RetainerPosDiff = Direction * m_RetainerPosition;
	vec2 RecoilPosDiff = Direction * m_CurrentRecoilFactor * m_RecoilAmount;

	// Retraction animation
	m_TurretBone.m_From = m_TurretBone.m_To + Direction * (m_Length - m_CurrentRecoilFactor * m_RecoilAmount);

	// Middle
	Ejector()[0].m_From = m_TurretBone.m_From - RetainerPosDiff;
	Ejector()[0].m_To = m_TurretBone.m_To - Direction * 12.f + Direction * (m_Length - m_RetainerPosition) * m_CurrentRecoilFactor - RecoilPosDiff;

	// Base
	Shaft()[0].m_To = m_TurretBone.m_To + PerpendicularShaft - RecoilPosDiff;
	Shaft()[0].m_From = m_TurretBone.m_To - PerpendicularShaft - RecoilPosDiff;
	// Sides (startpoint): same as base
	Shaft()[1].m_To = Shaft()[0].m_To;
	Shaft()[2].m_To = Shaft()[0].m_From;
	// Sides (endpoint)
	Shaft()[1].m_From = m_TurretBone.m_From + PerpendicularShaft;
	Shaft()[2].m_From = m_TurretBone.m_From - PerpendicularShaft;

	// Retainer (midpoint)
	Retainer()[0].m_From = m_TurretBone.m_From - RetainerPosDiff;
	Retainer()[1].m_From = Retainer()[0].m_From;
	// Retainer (sides)
	Retainer()[0].m_To = Retainer()[0].m_From + PerpendicularRetainer;
	Retainer()[1].m_To = Retainer()[1].m_From - PerpendicularRetainer;
}

void CLauncherTurret::HandleRecoil()
{
	m_CurrentRecoilFactor = 1.f - std::clamp((float)(Server()->Tick() - m_LastShot) / m_RecoilSpan, 0.f, 1.f);
}

void CLauncherTurret::FireTurret()
{
	if(!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime || !m_Shooting ||
		Server()->Tick() - m_LastShot <= m_ShootingCooldown)
		return;

	m_LastShot = Server()->Tick();

	vec2 StartingPos = m_pHelicopter->GetPos() + m_TurretBone.m_From;
	vec2 Direction = GetTurretDirection();
	const int OwnerId = m_pHelicopter->GetOwner()->GetPlayer()->GetCid();
	// CMissile currently has no Vel arg; Direction only for now (F used heli Vel*0.5 + Direction*8).
	new CMissile(GameWorld(), OwnerId, StartingPos,
		m_pHelicopter->GetVel() * 0.5f + Direction * 8.f,
		Direction, Server()->TickSpeed() * 3);
	GameServer()->CreateSound(StartingPos, SOUND_GRENADE_FIRE, m_pHelicopter->GetOwner()->TeamMask());
	GameServer()->CreateSound(StartingPos, SOUND_GRENADE_EXPLODE, m_pHelicopter->GetOwner()->TeamMask());

	m_pHelicopter->SetVel(m_pHelicopter->GetVel() + -Direction * 3.f);
}

CLauncherTurret::CLauncherTurret() :
	CVehicleTurret(TURRETTYPE_LAUNCHER, NUM_BONES,
		SBone(nullptr, -1, vec2(70.f, 50.f), vec2(-44.f, 50.f)),
		vec2(-10.f, 50.f), 35.f, 50)
{
	m_pHelicopter = nullptr;

	m_ShaftRadius = 7.f;
	m_RetainerPosition = 16.f; // From the tip of the launcher
	m_RetainerRadius = 11.f;

	m_RecoilAmount = 20.f;
	m_RecoilSpan = 30.f; // Ticks until normal position
	m_CurrentRecoilFactor = 0.f;

	InitBones();
}

CLauncherTurret::~CLauncherTurret()
{
}

void CLauncherTurret::ApplyScale(float TurretScale)
{
	// Experimental
	m_Length *= TurretScale;
	m_Pivot *= TurretScale;
	m_ShaftRadius *= TurretScale;
	m_RetainerPosition *= TurretScale;
	m_RetainerRadius *= TurretScale;
	m_RecoilAmount *= TurretScale;
	m_TurretBone.Scale(TurretScale);
	for(int i = 0; i < m_NumBones; i++)
		m_apBones[i].Scale(TurretScale);
	//
}

void CLauncherTurret::Tick()
{
	//    if (!m_pHelicopter) don't even try to run this function without a heli
	//        return;
	AimTurret();
	FireTurret();

	HandleRecoil();
	UpdateBones();
}

void CLauncherTurret::Snap(int SnappingClient)
{
	if(!m_pHelicopter)
		return;

	for(int i = 0; i < m_NumBones; i++)
		m_apBones[i].Snap(SnappingClient);
}

void CLauncherTurret::OnInput(CNetObj_PlayerInput *pNewInput)
{
	if(!m_pHelicopter->GetOwner() || m_pHelicopter->GetOwner()->m_FreezeTime)
		return;

	m_AimPosition = vec2((float)pNewInput->m_TargetX, (float)pNewInput->m_TargetY);
	m_Shooting = pNewInput->m_Fire % 2 == 1;
}

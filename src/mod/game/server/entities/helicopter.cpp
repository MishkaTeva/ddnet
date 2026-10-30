#include "helicopter.h"
#include "missile.h"

#include <base/math.h>
#include <base/mem.h>
#include <base/vmath.h>

#include <cmath>
#include <cstdlib>

#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

CHelicopter::CHelicopter(CGameWorld *pGameWorld, vec2 Pos, int Owner) :
	CHelicopter(pGameWorld, Pos, Owner, TURRETTYPE_LAUNCHER, false, -1)
{
}

CHelicopter::CHelicopter(CGameWorld *pGameWorld, vec2 Pos, int Owner, int TurretType, bool Build, int Number) :
	CAdvancedEntity(pGameWorld, CGameWorld::ENTTYPE_HELICOPTER, Pos, HELICOPTER_PHYSSIZE, Owner, false)
{
	m_Gravity = false;
	m_CheckDeath = false;
	m_Number = Number;
	m_DelayTurretType = Number >= 0 ? TurretType : -1;
	m_InitialPosition = Pos;
	m_SpawnTick = -1;
	if(PlacedByTile())
	{
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvHeliRespawnTime;
		m_Layer = LAYER_SWITCH;
	}

	m_TopPropellerRadius *= m_Scale;
	m_BackPropellerRadius *= m_Scale;
	InitBody();
	InitPropellers();
	InitBuild();
	InitHearts();

	m_Build = Build;
	if(Build)
		InitUnbuilt();

	if(TurretType == TURRETTYPE_MINIGUN)
		AttachTurret(new CMinigunTurret());
	else if(TurretType == TURRETTYPE_LAUNCHER)
		AttachTurret(new CLauncherTurret());

	GameWorld()->InsertEntity(this);
}

CHelicopter::~CHelicopter()
{
	Eject();
	DestroyTurret();
	FreeBones();
	for(int i = 0; i < NUM_BUILD_IDS; i++)
	{
		if(m_aBuildIDs[i] >= 0)
		{
			Server()->SnapFreeId(m_aBuildIDs[i]);
			m_aBuildIDs[i] = -1;
		}
	}
}

void CHelicopter::Reset()
{
	Eject();
	DestroyTurret();
	FreeBones();
	TryRespawnNewHelicopter();
	CAdvancedEntity::Reset();
}

bool CHelicopter::TryRespawnNewHelicopter()
{
	if(!PlacedByTile())
		return false;
	return GameServer()->SpawnHelicopter(m_InitialPosition, m_DelayTurretType, true, m_Number);
}

int CHelicopter::AllocSnapId()
{
	const std::optional<int> Id = Server()->SnapNewId();
	return Id.has_value() ? *Id : -1;
}

void CHelicopter::FreeBones()
{
	for(auto &Bone : m_aBones)
	{
		if(Bone.m_Id >= 0)
		{
			Server()->SnapFreeId(Bone.m_Id);
			Bone.m_Id = -1;
		}
	}
	for(auto &Trail : m_aTrails)
	{
		if(Trail.m_Id >= 0)
		{
			Server()->SnapFreeId(Trail.m_Id);
			Trail.m_Id = -1;
		}
	}
	for(auto &Heart : m_aHearts)
	{
		if(Heart.m_Id >= 0)
		{
			Server()->SnapFreeId(Heart.m_Id);
			Heart.m_Id = -1;
		}
	}
}

void CHelicopter::InitBody()
{
	SBone aBones[NUM_BONES_BODY] = {
		SBone(this, AllocSnapId(), 70, 45, 50, 60, 4),
		SBone(this, AllocSnapId(), -55, 60, 50, 60, 4),
		SBone(this, AllocSnapId(), -25, 40, -30, 60, 3),
		SBone(this, AllocSnapId(), 25, 40, 30, 60, 3),
		SBone(this, AllocSnapId(), 0, -40, 0, -60, 4),
		SBone(this, AllocSnapId(), 35, 40, -35, 40, 3),
		SBone(this, AllocSnapId(), 60, 10, 35, 40, 4),
		SBone(this, AllocSnapId(), 25, -40, 60, 10, 4),
		SBone(this, AllocSnapId(), -35, -40, 25, -40, 4),
		SBone(this, AllocSnapId(), -45, 0, -35, -40, 4),
		SBone(this, AllocSnapId(), -100, 0, -45, 0, 4),
		SBone(this, AllocSnapId(), -120, -30, -100, 0, 4),
		SBone(this, AllocSnapId(), -105, 20, -120, -30, 4),
		SBone(this, AllocSnapId(), -35, 40, -105, 20, 4),
	};
	mem_copy(Body(), aBones, sizeof(SBone) * NUM_BONES_BODY);
	for(int i = 0; i < NUM_BONES_BODY; i++)
		Body()[i].Scale(m_Scale);
}

void CHelicopter::InitPropellers()
{
	float Radius = m_TopPropellerRadius;
	for(int i = 0; i < NUM_BONES_PROPELLERS_TOP; i++)
	{
		TopPropeller()[i] = SBone(this, AllocSnapId(), vec2(Radius, -60.f * m_Scale), vec2(0, -60.f * m_Scale), 3);
		TopPropeller()[i].m_InitColor = LASERTYPE_DOOR;
		TopPropeller()[i].m_Color = LASERTYPE_DOOR;
		Radius *= -1;
	}

	Radius = m_BackPropellerRadius;
	for(int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
	{
		BackPropeller()[i] = SBone(this, AllocSnapId(), vec2(-110.f * m_Scale + Radius, -10.f * m_Scale), vec2(-110.f * m_Scale, -10.f * m_Scale), 3);
		Radius *= -1;
		m_aTrails[i] = STrail(this, AllocSnapId(), &BackPropeller()[i].m_From);
	}
}

void CHelicopter::InitHearts()
{
	for(int i = 0; i < NUM_HEARTS; i++)
		m_aHearts[i] = SHeart(this, AllocSnapId(), vec2(0.f, 0.f));
	UpdateHearts();
}

bool CHelicopter::AttachTurret(CVehicleTurret *pTurret)
{
	DestroyTurret();
	if(!pTurret)
		return true;
	if(pTurret->TryBindHelicopter(this))
	{
		m_pTurret = pTurret;
		return true;
	}
	delete pTurret;
	return false;
}

void CHelicopter::DestroyTurret()
{
	delete m_pTurret;
	m_pTurret = nullptr;
}

void CHelicopter::ResetAndTurnOff()
{
	m_EngineOn = false;
	float Radius = m_TopPropellerRadius;
	vec2 Direction = normalize(TopPropeller()[0].m_From - TopPropeller()[0].m_To);
	if(length(Direction) < 0.001f)
		Direction = vec2(1.f, 0.f);
	for(int i = 0; i < NUM_BONES_PROPELLERS_TOP; i++)
	{
		TopPropeller()[i].m_From = TopPropeller()[i].m_To + Direction * Radius;
		Radius *= -1;
	}
}

void CHelicopter::SpinPropellers()
{
	if(!m_EngineOn || Server()->Tick() % 2 != 0)
		return;

	if(!GetOwner() && IsGrounded())
	{
		ResetAndTurnOff();
		return;
	}

	for(int i = 0; i < NUM_BONES_PROPELLERS_BACK; i++)
		BackPropeller()[i].m_From = rotate_around_point(BackPropeller()[i].m_From, BackPropeller()[i].m_To, 50.f / pi);

	float Len = m_TopPropellerRadius;
	float CurLen = std::clamp((float)std::sin(Server()->Tick() / 5.0) * Len, -Len, Len);
	vec2 Diff = TopPropeller()[0].m_From - TopPropeller()[0].m_To;
	if(length(Diff) < 0.001f)
		Diff = vec2(Len, 0.f);
	else
		Diff = normalize(Diff) * CurLen;
	for(int i = 0; i < NUM_BONES_PROPELLERS_TOP; i++)
	{
		TopPropeller()[i].m_From = TopPropeller()[i].m_To + Diff;
		Diff *= -1;
	}
}

void CHelicopter::FlipBones()
{
	for(auto &Bone : m_aBones)
		Bone.Flip();
	m_Flipped = !m_Flipped;
	m_Angle *= -1.f;
	if(m_pTurret)
		m_pTurret->SetFlipped(m_Flipped);
}

bool CHelicopter::TryEnter(int ClientId)
{
	if(m_Owner >= 0 || IsExploding() || IsBuilding() || IsSpawning())
		return false;
	CCharacter *pChr = GameServer()->GetPlayerChar(ClientId);
	if(!pChr)
		return false;
	if(distance(pChr->GetPos(), m_Pos) > pChr->GetProximityRadius() + GetProximityRadius())
		return false;
	m_Owner = ClientId;
	m_LastKnownOwner = ClientId;
	m_EngineOn = true;
	m_Gravity = false;
	m_ShowHeartsUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	pChr->m_pHelicopter = this;
	pChr->SetPosition(m_Pos);
	pChr->Core()->m_Vel = vec2(0, 0);
	return true;
}

void CHelicopter::Eject()
{
	if(m_Owner < 0)
		return;
	if(CCharacter *pChr = GetOwner())
	{
		if(pChr->m_pHelicopter == this)
			pChr->m_pHelicopter = nullptr;
		pChr->SetPosition(m_Pos + vec2(0, 48));
		pChr->Core()->m_Vel = m_Vel;
	}
	m_Owner = -1;
	ResetAndTurnOff();
	m_Vel = vec2(0, 0);
	m_Accel = vec2(0, 0);
}

void CHelicopter::TakeDamage(float Damage, vec2 HitPos, int FromId)
{
	if(IsExploding() || IsBuilding() || IsSpawning())
		return;

	m_Health -= Damage;
	m_ShowHeartsUntil = Server()->Tick() + Server()->TickSpeed() * 3;
	m_LastDamage = Server()->Tick();
	GameServer()->CreateSound(HitPos, SOUND_PLAYER_DIE, m_TeamMask);
	GameServer()->CreateDeath(HitPos, FromId, m_TeamMask);
}

bool CHelicopter::IsRegenerating()
{
	return m_Health != m_MaxHealth && m_LastDamage && m_LastDamage + Server()->TickSpeed() * 10 < Server()->Tick();
}

void CHelicopter::UpdateHearts()
{
	const float HealthPercentage = m_Health / m_MaxHealth;
	const float HeartsPrecise = NUM_HEARTS * HealthPercentage;
	const int HeartsFull = (int)std::floor(HeartsPrecise);
	const int ShowNumHearts = (int)std::ceil(HeartsPrecise);
	const float CurrentHeart = HeartsPrecise - (float)HeartsFull;
	const bool FlashingHalfHeart = CurrentHeart > 0.0f && CurrentHeart <= 0.5f;

	for(int i = 0; i < NUM_HEARTS; i++)
	{
		if(i < ShowNumHearts - (FlashingHalfHeart ? 1 : 0))
			m_aHearts[i].m_Enabled = true;
		else if(i == HeartsFull && FlashingHalfHeart)
			m_aHearts[i].m_Enabled = (Server()->Tick() / 4) % 2 == 0;
		else
			m_aHearts[i].m_Enabled = false;
	}

	const float Gap = 40.f;
	const float HeartsY = m_BuildTop - 30.f;
	float HeartsX = -(float)(std::max(ShowNumHearts, 1) - 1) / 2.f * Gap;
	for(int i = 0; i < NUM_HEARTS; i++)
	{
		m_aHearts[i].m_Pos.x = HeartsX;
		m_aHearts[i].m_Pos.y = HeartsY;
		if(i < ShowNumHearts)
			HeartsX += Gap;
	}
}

void CHelicopter::RegenerateHelicopter()
{
	if(!IsRegenerating())
		return;
	m_Health += 0.02f;
	if(m_Health > m_MaxHealth)
		m_Health = m_MaxHealth;
}

void CHelicopter::UpdateVisualDamage()
{
	const float HealthPercentage = 1.f - m_Health / m_MaxHealth;
	for(int i = 0; i < NUM_BONES; i++)
		m_aBones[i].m_Thickness = m_aBones[i].m_InitThickness - (int)(std::sin(Server()->Tick() / 4.0 + i) * HealthPercentage * (float)m_aBones[i].m_InitThickness);
}

void CHelicopter::DamageInFreeze()
{
	if(m_LastEnvironmentalDamage && m_LastEnvironmentalDamage + Server()->TickSpeed() > Server()->Tick())
		return;

	const int Index = Collision()->GetPureMapIndex(m_Pos);
	const int Tile = Collision()->GetTileIndex(Index);
	const int Front = Collision()->GetFrontTileIndex(Index);
	if((Tile == TILE_FREEZE || Front == TILE_FREEZE) && (!GetOwner() || !GetOwner()->IsSuper()))
	{
		TakeDamage(2, m_Pos, m_LastKnownOwner);
		m_LastEnvironmentalDamage = Server()->Tick();
	}
}

void CHelicopter::DamageInWall()
{
	if(m_LastEnvironmentalDamage && m_LastEnvironmentalDamage + Server()->TickSpeed() > Server()->Tick())
		return;
	if(Collision()->CheckPoint(m_Pos.x, m_Pos.y))
	{
		TakeDamage(20, m_Pos, m_LastKnownOwner);
		m_LastEnvironmentalDamage = Server()->Tick();
	}
}

void CHelicopter::Explode()
{
	if(IsExploding())
		return;
	Eject();
	m_ExplosionsLeft = 8;
	m_EngineOn = false;
	m_Vel = vec2(0, 0);
}

void CHelicopter::HandleExplosions()
{
	if(m_ExplosionsLeft < 0 || Server()->Tick() % 5 != 0)
		return;

	const int Diameter = (int)std::min(GetSize().x, GetSize().y) + 150;
	const int Radius = Diameter / 2;
	for(int i = 0; i < 3; i++)
	{
		const vec2 NearbyPos = m_Pos + vec2((float)(rand() % Diameter - Radius), (float)(rand() % Diameter - Radius));
		GameServer()->CreateExplosion(NearbyPos, m_LastKnownOwner, WEAPON_GRENADE, m_LastKnownOwner == -1, m_DDTeam, m_TeamMask);
		GameServer()->CreateSound(NearbyPos, SOUND_GRENADE_EXPLODE, m_TeamMask);
	}

	if(m_ExplosionsLeft == 0)
		Reset();
	m_ExplosionsLeft--;
}

void CHelicopter::ApplyInput()
{
	CCharacter *pChr = GetOwner();
	if(!pChr || pChr->m_FreezeTime)
	{
		m_Accel = vec2(0, 0);
		return;
	}

	const CNetObj_PlayerInput *pInput = &pChr->GetLatestInput();
	m_Accel.x = (float)pInput->m_Direction * 0.35f;
	m_Accel.y = 0.f;
	if(pInput->m_Jump)
		m_Accel.y -= 0.45f;
	if(pInput->m_Hook)
		m_Accel.y += 0.25f;

	if(pInput->m_Direction != 0)
	{
		const bool WantFlip = pInput->m_Direction < 0;
		if(WantFlip != m_Flipped)
			FlipBones();
	}

	const bool FirePressed = (pInput->m_Fire & 1) != 0;
	const bool FireEdge = FirePressed && !(m_LastFire & 1);
	m_LastFire = pInput->m_Fire;

	// Hook+fire edge ejects; turret handles shooting via OnInput.
	if(FireEdge && pInput->m_Hook)
		Eject();

	if(m_pTurret)
	{
		CNetObj_PlayerInput TurretInput = *pInput;
		m_pTurret->OnInput(&TurretInput);
	}
}

void CHelicopter::Tick()
{
	CAdvancedEntity::Tick();

	if(m_LastKnownOwner >= 0 && !GameServer()->m_apPlayers[m_LastKnownOwner])
		m_LastKnownOwner = -1;

	if(PlacedByTile() && IsSpawning())
	{
		if(m_Number > 0 && m_Number < (int)Switchers().size())
		{
			if(!Switchers()[m_Number].m_aStatus[0])
				m_SpawnTick++;
		}
		if(Server()->Tick() > m_SpawnTick)
		{
			m_SpawnTick = -1;
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, m_TeamMask);
		}
		return;
	}

	BuildHelicopter();

	if(IsBuilding())
	{
		m_PrevPos = m_Pos;
		return;
	}

	if(!IsExploding())
	{
		ApplyInput();
		SpinPropellers();

		if(GetOwner())
		{
			GetOwner()->SetPosition(m_Pos);
			GetOwner()->Core()->m_Vel = vec2(0, 0);
			m_Gravity = GetOwner()->m_FreezeTime;
		}
		else if(m_Owner >= 0)
		{
			m_Owner = -1;
			ResetAndTurnOff();
		}

		if(m_EngineOn && !m_Gravity)
		{
			m_Vel += m_Accel;
			m_Vel.x *= 0.96f;
			m_Vel.y *= 0.96f;
		}
		else
		{
			m_Vel.y += GetTuning(0)->m_Gravity * 0.35f;
			m_Vel.x *= 0.98f;
		}

		Collision()->MoveBox(&m_Pos, &m_Vel, m_Size, vec2(0.4f, 0.4f));

		DamageInWall();
		DamageInFreeze();
		UpdateHearts();
		RegenerateHelicopter();
		UpdateVisualDamage();

		if(m_pTurret)
			m_pTurret->Tick();

		if(m_Health <= 0.f)
			Explode();
	}

	HandleExplosions();

	if(m_Owner < 0 && !IsExploding())
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CCharacter *pChr = GameServer()->GetPlayerChar(i);
			if(!pChr)
				continue;
			if(distance(pChr->GetPos(), m_Pos) > 48.f)
				continue;
			if((pChr->GetLatestInput().m_Fire & 1) && TryEnter(i))
				break;
		}
	}
	else if(m_Owner >= 0 && !GetOwner())
	{
		m_Owner = -1;
		ResetAndTurnOff();
	}

	m_PrevPos = m_Pos;
}

void CHelicopter::Snap(int SnappingClient)
{
	if(NetworkClipped(SnappingClient) || IsExploding() || IsSpawning())
		return;

	if(IsBuilding())
	{
		const int Width = std::max(1, (int)m_BuildTotalWidth);
		for(int i = 0; i < NUM_BUILD_IDS; i++)
		{
			if(m_aBuildIDs[i] < 0)
				continue;
			CNetObj_Projectile Proj = {};
			Proj.m_X = round_to_int(m_Pos.x + m_BuildLeft + (float)(rand() % Width));
			Proj.m_Y = round_to_int(m_Pos.y + m_BuildHeight);
			Proj.m_VelX = 0;
			Proj.m_VelY = 0;
			Proj.m_StartTick = Server()->Tick();
			Proj.m_Type = WEAPON_HAMMER;
			Server()->SnapNewItem(m_aBuildIDs[i], Proj);
		}
	}
	else if(Server()->Tick() < m_ShowHeartsUntil || (GetOwner() && GetOwner()->GetPlayer()->GetCid() == SnappingClient))
	{
		for(auto &Heart : m_aHearts)
			Heart.Snap(SnappingClient);
	}

	for(auto &Bone : m_aBones)
		Bone.Snap(SnappingClient);
	for(auto &Trail : m_aTrails)
		Trail.Snap(SnappingClient);
	if(m_pTurret && !IsBuilding())
		m_pTurret->Snap(SnappingClient);
}

void CHelicopter::InitBuild()
{
	bool NotFirst = false;
	float Lowest = 0.f;
	float Highest = 0.f;
	float Leftest = 0.f;
	float Rightest = 0.f;
	for(int i = 0; i < NUM_BONES; i++)
	{
		if(!NotFirst || m_aBones[i].m_From.y > Lowest)
		{
			NotFirst = true;
			Lowest = m_aBones[i].m_From.y;
		}
		if(!NotFirst || m_aBones[i].m_To.y > Lowest)
		{
			NotFirst = true;
			Lowest = m_aBones[i].m_To.y;
		}
		if(!NotFirst || m_aBones[i].m_From.y < Highest)
		{
			NotFirst = true;
			Highest = m_aBones[i].m_From.y;
		}
		if(!NotFirst || m_aBones[i].m_To.y < Highest)
		{
			NotFirst = true;
			Highest = m_aBones[i].m_To.y;
		}
		if(!NotFirst || m_aBones[i].m_From.x < Leftest)
		{
			NotFirst = true;
			Leftest = m_aBones[i].m_From.x;
		}
		if(!NotFirst || m_aBones[i].m_To.x < Leftest)
		{
			NotFirst = true;
			Leftest = m_aBones[i].m_To.x;
		}
		if(!NotFirst || m_aBones[i].m_From.x > Rightest)
		{
			NotFirst = true;
			Rightest = m_aBones[i].m_From.x;
		}
		if(!NotFirst || m_aBones[i].m_To.x > Rightest)
		{
			NotFirst = true;
			Rightest = m_aBones[i].m_To.x;
		}
	}

	m_BuildTop = Highest;
	m_BuildBottom = Lowest;
	m_BuildLeft = Leftest;
	m_BuildRight = Rightest;
	m_BuildTotalHeight = Lowest - Highest;
	m_BuildTotalWidth = Rightest - Leftest;
	m_BuildHeight = Lowest;
}

void CHelicopter::InitUnbuilt()
{
	for(int i = 0; i < NUM_BUILD_IDS; i++)
		m_aBuildIDs[i] = AllocSnapId();

	for(int i = 0; i < NUM_BONES; i++)
	{
		m_aBones[i].m_Enabled = false;
		m_aBones[i].m_Thickness = 0;
		m_aBones[i].m_Color = LASERTYPE_FREEZE;
	}
	for(int i = 0; i < NUM_TRAILS; i++)
		m_aTrails[i].m_Enabled = false;
}

void CHelicopter::BuildHelicopter()
{
	if(!m_Build)
		return;

	m_BuildHeight -= 0.5f;
	for(int i = 0; i < NUM_BONES; i++)
	{
		const SBone &Bone = m_aBones[i];
		const vec2 From = Bone.m_InitFrom;
		const vec2 To = Bone.m_InitTo;
		const bool FromUnder = From.y > m_BuildHeight;
		const bool ToUnder = To.y > m_BuildHeight;
		m_aBones[i].m_Enabled = FromUnder || ToUnder;

		if(m_aBones[i].m_Enabled)
		{
			const float FullLength = distance(Bone.m_InitFrom, Bone.m_InitTo);
			const float VisibleLength = distance(m_aBones[i].m_From, m_aBones[i].m_To);
			const float Fraction = FullLength > 0.0f ? std::clamp(VisibleLength / FullLength, 0.0f, 1.0f) : 0.0f;
			m_aBones[i].m_Thickness = round_to_int(-3 + Fraction * ((float)Bone.m_InitThickness + 3.f));

			if(!FromUnder && !ToUnder)
				m_aBones[i].m_Enabled = false;
			else if(FromUnder && ToUnder)
			{
				m_aBones[i].m_From = From;
				m_aBones[i].m_To = To;
			}
			else if(FromUnder && !ToUnder)
			{
				const float t = (m_BuildHeight - From.y) / (To.y - From.y);
				m_aBones[i].m_From = From;
				m_aBones[i].m_To = vec2(From.x + (To.x - From.x) * t, m_BuildHeight);
			}
			else if(!FromUnder && ToUnder)
			{
				const float t = (m_BuildHeight - To.y) / (From.y - To.y);
				m_aBones[i].m_From = vec2(To.x + (From.x - To.x) * t, m_BuildHeight);
				m_aBones[i].m_To = To;
			}
		}
	}

	for(int i = 0; i < NUM_TRAILS; i++)
		m_aTrails[i].m_Enabled = m_aTrails[i].m_pPos && m_aTrails[i].m_pPos->y > m_BuildHeight;

	if(m_BuildHeight < m_BuildTop)
	{
		m_Build = false;
		for(int i = 0; i < NUM_BUILD_IDS; i++)
		{
			if(m_aBuildIDs[i] >= 0)
			{
				Server()->SnapFreeId(m_aBuildIDs[i]);
				m_aBuildIDs[i] = -1;
			}
		}
		for(int i = 0; i < NUM_BONES; i++)
			m_aBones[i].m_Color = m_aBones[i].m_InitColor;
	}
}


#include "draweditor.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/mapitems.h>
#include <game/server/entities/character.h>
#include <game/server/entities/door.h>
#include <game/server/entities/pickup.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include <game/weapons_ex.h>

#include <mod/game/server/entities/button.h>
#include <mod/game/server/entities/helicopter.h>
#include <mod/game/server/entities/speedup.h>
#include <mod/game/server/entities/teleporter.h>

#include <cmath>

CGameContext *CDrawEditor::GameServer() const { return m_pCharacter->GameServer(); }
IServer *CDrawEditor::Server() const { return GameServer()->Server(); }

void CDrawEditor::Init(CCharacter *pChr)
{
	m_pCharacter = pChr;

	m_Laser.m_ButtonMode = true;
	m_Laser.m_Number = 0;
	m_Laser.m_Angle = pi / 2.f;
	m_Laser.m_Length = 3;
	m_Speedup.m_Angle = 0;
	m_Speedup.m_Force = 2;
	m_Speedup.m_MaxSpeed = 0;
	m_Teleporter.m_Number = 0;
	m_Teleporter.m_Evil = true;
	m_Teleporter.m_Mode = TELE_MODE_OUT;
	m_Pickup.m_Type = POWERUP_HEALTH;
	m_Pickup.m_SubType = 0;

	m_Setting = -1;
	m_RoundPos = true;
	m_Erasing = false;
	m_Selecting = false;
	m_EditStartTick = 0;
	m_pPreview = nullptr;
	m_Entity = -1;

	m_Transform.m_State = TRANSFORM_STATE_SETTING_FIRST;
	m_Transform.m_Angle = 0;
	m_Transform.m_vSelected.clear();
	m_Transform.m_vPreview.clear();
	for(int i = 0; i < 4; i++)
	{
		if(!m_Transform.m_aSnapId[i].has_value())
			m_Transform.m_aSnapId[i] = Server()->SnapNewId();
	}

	m_Category = CAT_UNINITIALIZED;
	for(int i = 0; i < NUM_DRAW_CATEGORIES; i++)
	{
		if(IsCategoryAllowed(i))
		{
			SetCategory(i);
			break;
		}
	}
}

bool CDrawEditor::Active()
{
	return m_pCharacter && m_pCharacter->GetActiveWeapon() == WEAPON_DRAW_EDITOR;
}

vec2 CDrawEditor::RoundToTile(vec2 Pos) const
{
	Pos.x = (float)(round_to_int(Pos.x) / 32 * 32 + 16);
	Pos.y = (float)(round_to_int(Pos.y) / 32 * 32 + 16);
	return Pos;
}

vec2 CDrawEditor::CalcPlacePos() const
{
	const CNetObj_PlayerInput &Input = m_pCharacter->GetLatestInput();
	vec2 Target((float)Input.m_TargetX, (float)Input.m_TargetY);
	if(Target.x == 0.f && Target.y == 0.f)
		Target = vec2(1.f, 0.f);
	vec2 Direction = normalize(Target);
	vec2 PlacePos = m_pCharacter->m_Pos + Direction * 64.f;
	if(m_RoundPos && !m_Erasing)
		PlacePos = RoundToTile(PlacePos);
	return PlacePos;
}

bool CDrawEditor::CanPlace(CEntity *pAt)
{
	vec2 Pos = pAt ? pAt->GetPos() : m_Pos;
	int rx = round_to_int(Pos.x) / 32;
	int ry = round_to_int(Pos.y) / 32;
	if(rx <= 0 || rx >= GameServer()->Collision()->GetWidth() - 1 || ry <= 0 || ry >= GameServer()->Collision()->GetHeight() - 1)
		return false;

	if(GameServer()->Collision()->CheckPoint(Pos))
		return false;

	const int Index = GameServer()->Collision()->GetPureMapIndex(Pos);
	const int EntityType = pAt ? pAt->GetObjType() : m_Entity;
	if(EntityType == CGameWorld::ENTTYPE_SPEEDUP)
	{
		if(GameServer()->Collision()->IsSpeedup(Index))
			return false;
	}
	else if(EntityType == CGameWorld::ENTTYPE_TELEPORTER)
	{
		const CTeleTile *pTele = GameServer()->Collision()->TeleLayer();
		if(pTele && Index >= 0 && pTele[Index].m_Type != 0)
			return false;
	}

	const int Auth = Server()->GetAuthedState(GetCid());
	const bool InRange = distance(Pos, m_pCharacter->GetPos()) < g_Config.m_SvEditorMaxDistance || Auth >= AUTHED_ADMIN;
	if(!InRange)
		return false;

	const int OwnPlotId = GetOwnPlotID();
	const int CursorPlotId = pAt ? GameServer()->GetTilePlotID(Pos) : GetCursorPlotID();
	const bool FreeDraw = OwnPlotId < CGameContext::PLOT_START || CurrentPlotID() != OwnPlotId;
	if(FreeDraw && Auth < AUTHED_ADMIN)
		return false;

	return (CursorPlotId >= CGameContext::PLOT_START && CursorPlotId == OwnPlotId) || FreeDraw;
}

bool CDrawEditor::CanRemove(CEntity *pEntity)
{
	// Map entities (m_PlotID == -1) are not removable.
	return CanPlace() && pEntity && pEntity->m_PlotID >= 0 && pEntity->m_BrushCID == -1;
}

CEntity *CDrawEditor::ClosestDrawable(float Radius)
{
	static const int s_aTypes[] = {
		CGameWorld::ENTTYPE_PICKUP,
		CGameWorld::ENTTYPE_DOOR,
		CGameWorld::ENTTYPE_SPEEDUP,
		CGameWorld::ENTTYPE_BUTTON,
		CGameWorld::ENTTYPE_TELEPORTER,
		CGameWorld::ENTTYPE_HELICOPTER,
	};

	CEntity *pClosest = nullptr;
	float ClosestDist = Radius;
	CEntity *apEnts[64];

	for(int Type : s_aTypes)
	{
		const int Num = GameServer()->m_World.FindEntities(m_Pos, Radius, apEnts, 64, Type);
		for(int i = 0; i < Num; i++)
		{
			if(apEnts[i] == m_pPreview)
				continue;
			const float Dist = distance(m_Pos, apEnts[i]->GetPos());
			if(Dist < ClosestDist)
			{
				ClosestDist = Dist;
				pClosest = apEnts[i];
			}
		}
	}
	return pClosest;
}

bool CDrawEditor::RemoveEntity(CEntity *pEntity)
{
	if(!pEntity)
		return false;

	GameServer()->ErasePlotDrawEntity(pEntity);
	// Destructors call ResetCollision(true) for speedup/button/teleporter/door.
	pEntity->Destroy();
	return true;
}

int CDrawEditor::GetOwnPlotID() const
{
	return GameServer()->GetPlotID(m_pCharacter->GetPlayer()->GetAccID());
}

int CDrawEditor::GetCursorPlotID() const
{
	return GameServer()->GetTilePlotID(m_Pos);
}

int CDrawEditor::CurrentPlotID() const
{
	return GameServer()->GetTilePlotID(m_pCharacter->GetPos());
}

bool CDrawEditor::IsCategoryAllowed(int Category)
{
	switch(Category)
	{
	case CAT_PICKUPS:
	case CAT_LASERWALLS:
	case CAT_LASERDOORS:
	case CAT_SPEEDUPS:
	case CAT_TELEPORTER:
	case CAT_TRANSFORM:
	case CAT_HELICOPTER:
		return true;
	default:
		return false;
	}
}

void CDrawEditor::Tick()
{
	if(!Active())
		return;

	HandleInput();
	m_Pos = CalcPlacePos();

	if(m_pPreview)
	{
		m_pPreview->m_Pos = m_Pos;
		if(m_pPreview->GetObjType() == CGameWorld::ENTTYPE_DOOR)
			((CDoor *)m_pPreview)->UpdateEndpoint();
		else if(m_pPreview->GetObjType() == CGameWorld::ENTTYPE_TELEPORTER)
		{
			// teleporter snap uses m_Pos directly
		}
	}

	if(m_Category == CAT_TRANSFORM)
	{
		if(m_Transform.m_State == TRANSFORM_STATE_SETTING_FIRST || m_Transform.m_State == TRANSFORM_STATE_SETTING_SECOND)
		{
			if(m_Transform.m_State == TRANSFORM_STATE_SETTING_FIRST)
				m_Transform.m_aPos[0] = m_Pos;
			m_Transform.m_aPos[1] = m_Pos;
		}
	}

	m_PrevInput = m_Input;
}

void CDrawEditor::Snap()
{
	if(!Active() || m_Category != CAT_TRANSFORM)
		return;
	if(m_Transform.m_State == TRANSFORM_STATE_RUNNING)
		return;
	SnapTransformArea();
}

void CDrawEditor::SnapTransformArea()
{
	const int ClientId = GetCid();
	CSnapContext Context(GameServer()->GetClientVersion(ClientId), Server()->IsSixup(ClientId), ClientId);
	const vec2 TopLeft = m_Transform.TopLeft();
	const vec2 BottomRight = m_Transform.BottomRight();
	const vec2 aPoints[4] = {
		TopLeft,
		vec2(BottomRight.x, TopLeft.y),
		BottomRight,
		vec2(TopLeft.x, BottomRight.y),
	};
	for(int i = 0; i < 4; i++)
	{
		if(!m_Transform.m_aSnapId[i].has_value())
			continue;
		const int To = i == 3 ? 0 : i + 1;
		GameServer()->SnapLaserObject(Context, m_Transform.m_aSnapId[i].value(), aPoints[i], aPoints[To],
			Server()->Tick() - 2, ClientId, LASERTYPE_RIFLE, -1, -1);
	}
}

void CDrawEditor::CollectTransformablesInArea()
{
	m_Transform.m_vSelected.clear();
	static const int s_aTypes[] = {
		CGameWorld::ENTTYPE_PICKUP,
		CGameWorld::ENTTYPE_DOOR,
		CGameWorld::ENTTYPE_SPEEDUP,
		CGameWorld::ENTTYPE_BUTTON,
		CGameWorld::ENTTYPE_TELEPORTER,
		CGameWorld::ENTTYPE_HELICOPTER,
	};

	for(int Type : s_aTypes)
	{
		for(CEntity *pEnt = GameServer()->m_World.FindFirst(Type); pEnt; pEnt = pEnt->TypeNext())
		{
			if(pEnt == m_pPreview)
				continue;
			if(pEnt->m_PlotID < 0)
				continue;
			if(!m_Transform.Includes(pEnt->GetPos()))
				continue;
			SSelectedEnt Selected;
			Selected.m_pEnt = pEnt;
			Selected.m_Offset = pEnt->GetPos() - m_Transform.BottomRight();
			m_Transform.m_vSelected.push_back(Selected);
		}
	}
}

void CDrawEditor::OnPlayerFire()
{
	if(!Active())
		return;

	if(m_Selecting)
	{
		int Dir = m_pCharacter->GetAimDir();
		int Setting = m_Setting;
		int NumSettings = GetNumSettings();
		if(NumSettings <= 0)
			return;

		if(Dir == 1)
		{
			Setting++;
			if(Setting >= NumSettings)
				Setting = 0;
		}
		else if(Dir == -1)
		{
			Setting--;
			if(Setting < 0)
				Setting = NumSettings - 1;
		}
		SetSetting(Setting);
		SendWindow();
		return;
	}

	if(m_Category == CAT_TRANSFORM)
	{
		if(m_Transform.m_State == TRANSFORM_STATE_CONFIRM)
		{
			CollectTransformablesInArea();

			if(m_Setting == TRANSFORM_MOVE || m_Setting == TRANSFORM_COPY)
			{
				m_Transform.m_vPreview.clear();
				for(const SSelectedEnt &Selected : m_Transform.m_vSelected)
				{
					SSelectedEnt Preview = Selected;
					m_Transform.m_vPreview.push_back(Preview);
				}
				if(m_Setting == TRANSFORM_COPY)
					m_Transform.m_vSelected.clear();
			}
			else if(m_Setting == TRANSFORM_ERASE)
			{
				for(const SSelectedEnt &Selected : m_Transform.m_vSelected)
					RemoveEntity(Selected.m_pEnt);
				StopTransform();
				GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
				return;
			}
		}
		else if(m_Transform.m_State == TRANSFORM_STATE_RUNNING)
		{
			const int PlotId = GetCursorPlotID();
			std::vector<CEntity *> apToRemove;
			if(m_Setting == TRANSFORM_MOVE)
			{
				for(const SSelectedEnt &Selected : m_Transform.m_vSelected)
					apToRemove.push_back(Selected.m_pEnt);
			}

			for(const SSelectedEnt &Preview : m_Transform.m_vPreview)
			{
				if(!Preview.m_pEnt)
					continue;

				vec2 PlacePos = m_Pos + Preview.m_Offset;
				if(m_RoundPos)
					PlacePos = RoundToTile(PlacePos);

				const vec2 OldPos = Preview.m_pEnt->m_Pos;
				Preview.m_pEnt->m_Pos = PlacePos;
				const bool Ok = CanPlace(Preview.m_pEnt);
				if(Ok)
				{
					if(m_Setting == TRANSFORM_COPY && !GameServer()->CanInsertNewEntity(PlotId))
					{
						Preview.m_pEnt->m_Pos = OldPos;
						break;
					}
					CEntity *pEntity = CreateTransformEntity(Preview.m_pEnt);
					if(pEntity)
					{
						pEntity->m_PlotID = PlotId;
						GameServer()->InsertPlotDrawEntity(pEntity);
					}
				}
				Preview.m_pEnt->m_Pos = OldPos;
			}

			for(CEntity *pEnt : apToRemove)
				RemoveEntity(pEnt);

			StopTransform();
			GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
			return;
		}

		m_Transform.m_State++;
		return;
	}

	if(m_Erasing || m_pCharacter->m_FreezeTime || !CanPlace())
		return;

	const int PlotId = GetCursorPlotID();
	if(!GameServer()->CanInsertNewEntity(PlotId))
		return;

	CEntity *pEntity = CreateEntity(false);
	if(!pEntity)
		return;
	pEntity->m_PlotID = PlotId;
	GameServer()->InsertPlotDrawEntity(pEntity);
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
}

void CDrawEditor::OnInput(CNetObj_PlayerInput *pNewInput)
{
	m_Input.m_Jump = pNewInput->m_Jump;
	m_Input.m_Hook = pNewInput->m_Hook;
	m_Input.m_Direction = pNewInput->m_Direction;
}

void CDrawEditor::HandleInput()
{
	if(m_Input.m_Jump)
	{
		if(!m_Selecting || Server()->Tick() % 100 == 0)
			SendWindow();
		m_Selecting = true;
	}
	else
	{
		if(m_Selecting)
			GameServer()->SendMotd("", GetCid());
		m_Selecting = false;
	}

	if(m_Input.m_Hook)
	{
		if(m_Selecting && !m_PrevInput.m_Hook)
		{
			int Dir = m_pCharacter->GetAimDir();
			int Category = m_Category;
			do
			{
				if(Dir == 1)
				{
					Category++;
					if(Category >= NUM_DRAW_CATEGORIES)
						Category = 0;
				}
				else if(Dir == -1)
				{
					Category--;
					if(Category < 0)
						Category = NUM_DRAW_CATEGORIES - 1;
				}
			} while(!IsCategoryAllowed(Category) && Category != m_Category);
			SetCategory(Category);
			SendWindow();
		}
		else if(!m_Selecting)
		{
			if(m_Category == CAT_TRANSFORM && m_Transform.m_State != TRANSFORM_STATE_SETTING_FIRST)
			{
				if(!m_PrevInput.m_Hook)
					StopTransform();
			}
			else if(!m_pCharacter->m_FreezeTime)
			{
				m_Erasing = true;
				CEntity *pEntity = ClosestDrawable(16.f);
				if(CanRemove(pEntity) && RemoveEntity(pEntity))
					GameServer()->CreateSound(m_Pos, SOUND_HOOK_LOOP);
			}
		}
	}
	else
		m_Erasing = false;

	if(m_Input.m_Direction != 0)
	{
		if(m_EditStartTick == 0)
			m_EditStartTick = Server()->Tick();

		if(m_Selecting && (m_PrevInput.m_Direction == 0 || m_EditStartTick < Server()->Tick() - Server()->TickSpeed() / 2))
		{
			if(m_Category == CAT_LASERWALLS)
			{
				if(m_Setting == LASERWALL_LENGTH)
				{
					m_Laser.m_Length += m_Input.m_Direction;
					if(m_Laser.m_Length > MAX_LASERWALL_LENGTH)
						m_Laser.m_Length = 1;
					else if(m_Laser.m_Length < 1)
						m_Laser.m_Length = MAX_LASERWALL_LENGTH;
					if(m_pPreview && m_pPreview->GetObjType() == CGameWorld::ENTTYPE_DOOR)
						((CDoor *)m_pPreview)->SetLength(32 * m_Laser.m_Length);
				}
			}
			else if(m_Category == CAT_LASERDOORS)
			{
				if(m_Setting == LASERDOOR_NUMBER)
				{
					m_Laser.m_Number += m_Input.m_Direction;
					if(m_Laser.m_Number >= MAX_DRAW_NUMBERS)
						m_Laser.m_Number = 0;
					else if(m_Laser.m_Number < 0)
						m_Laser.m_Number = MAX_DRAW_NUMBERS - 1;
				}
				else if(m_Setting == LASERDOOR_MODE)
				{
					m_Laser.m_ButtonMode = !m_Laser.m_ButtonMode;
					m_Entity = m_Laser.m_ButtonMode ? CGameWorld::ENTTYPE_BUTTON : CGameWorld::ENTTYPE_DOOR;
					UpdatePreview();
				}
			}
			else if(m_Category == CAT_SPEEDUPS)
			{
				const int Max = 255;
				if(m_Setting == SPEEDUP_FORCE)
				{
					m_Speedup.m_Force += m_Input.m_Direction;
					if(m_Speedup.m_Force > Max)
						m_Speedup.m_Force = 1;
					else if(m_Speedup.m_Force < 1)
						m_Speedup.m_Force = Max;
				}
				else if(m_Setting == SPEEDUP_MAXSPEED)
				{
					m_Speedup.m_MaxSpeed += m_Input.m_Direction;
					if(m_Speedup.m_MaxSpeed > Max)
						m_Speedup.m_MaxSpeed = 0;
					else if(m_Speedup.m_MaxSpeed < 0)
						m_Speedup.m_MaxSpeed = Max;
				}
			}
			else if(m_Category == CAT_TELEPORTER)
			{
				if(m_Setting == TELEPORTER_NUMBER)
				{
					m_Teleporter.m_Number += m_Input.m_Direction;
					if(m_Teleporter.m_Number >= MAX_DRAW_NUMBERS)
						m_Teleporter.m_Number = 0;
					else if(m_Teleporter.m_Number < 0)
						m_Teleporter.m_Number = MAX_DRAW_NUMBERS - 1;
				}
				else if(m_Setting == TELEPORTER_MODE)
				{
					m_Teleporter.m_Mode += m_Input.m_Direction;
					if(m_Teleporter.m_Mode >= NUM_TELE_MODES)
						m_Teleporter.m_Mode = 0;
					else if(m_Teleporter.m_Mode < 0)
						m_Teleporter.m_Mode = NUM_TELE_MODES - 1;
				}
				else if(m_Setting == TELEPORTER_EVIL)
				{
					m_Teleporter.m_Evil = !m_Teleporter.m_Evil;
				}
			}
			SendWindow();
		}
		else if(!m_Selecting)
		{
			bool Faster = m_EditStartTick < Server()->Tick() - Server()->TickSpeed();
			float Add = m_Input.m_Direction * (1 + 2 * (int)Faster);
			if(IsCategoryLaser() && !m_Laser.m_ButtonMode)
				Add -= m_Input.m_Direction * 0.5f;

			if((IsCategoryLaser() && !m_Laser.m_ButtonMode) || m_Category == CAT_SPEEDUPS || m_Category == CAT_TRANSFORM)
			{
				if((m_pCharacter->GetPlayer()->m_PlayerFlags & PLAYERFLAG_SCOREBOARD) && IsCategoryLaser() && !m_Laser.m_ButtonMode)
					AddLength(m_Input.m_Direction * (1 + (int)Faster));
				else
					AddAngle(Add);
			}
		}
	}
	else
		m_EditStartTick = 0;
}

void CDrawEditor::SetPickup(int Pickup)
{
	if(m_Setting == Pickup && m_Entity == CGameWorld::ENTTYPE_PICKUP)
		return;

	m_Entity = CGameWorld::ENTTYPE_PICKUP;
	if(Pickup == DRAW_PICKUP_HEART || Pickup == DRAW_PICKUP_SHIELD)
	{
		m_Pickup.m_Type = Pickup;
		m_Pickup.m_SubType = 0;
	}
	else if(Pickup >= DRAW_PICKUP_HAMMER && Pickup <= DRAW_PICKUP_LASER)
	{
		m_Pickup.m_Type = POWERUP_WEAPON;
		m_Pickup.m_SubType = Pickup - 2;
	}

	UpdatePreview();
	m_Setting = Pickup;
}

void CDrawEditor::SetCategory(int Category)
{
	if(m_Category == Category)
		return;

	StopTransform(true);
	m_Setting = -1;
	if(Category == CAT_PICKUPS)
	{
		SetPickup(DRAW_PICKUP_HEART);
	}
	else if(Category == CAT_LASERWALLS)
	{
		m_Entity = CGameWorld::ENTTYPE_DOOR;
		m_RoundPos = true;
		m_Laser.m_ButtonMode = false;
		if(m_Laser.m_Length < 1)
			m_Laser.m_Length = 3;
	}
	else if(Category == CAT_LASERDOORS)
	{
		m_RoundPos = true;
		m_Entity = m_Laser.m_ButtonMode ? CGameWorld::ENTTYPE_BUTTON : CGameWorld::ENTTYPE_DOOR;
	}
	else if(Category == CAT_SPEEDUPS)
	{
		m_Entity = CGameWorld::ENTTYPE_SPEEDUP;
		m_RoundPos = true;
	}
	else if(Category == CAT_TELEPORTER)
	{
		m_Entity = CGameWorld::ENTTYPE_TELEPORTER;
		m_RoundPos = true;
	}
	else if(Category == CAT_TRANSFORM)
	{
		m_Entity = -1;
		m_RoundPos = true;
	}
	else if(Category == CAT_HELICOPTER)
	{
		m_Entity = CGameWorld::ENTTYPE_HELICOPTER;
		m_RoundPos = true;
	}
	else
	{
		m_Entity = -1;
	}

	if(Category != CAT_PICKUPS)
	{
		UpdatePreview();
		m_Setting = 0;
	}

	m_Category = Category;
}

void CDrawEditor::SetSetting(int Setting)
{
	if(m_Category == CAT_PICKUPS)
		SetPickup(Setting);
	else
		m_Setting = Setting;
}

CEntity *CDrawEditor::CreateEntity(bool Preview)
{
	const bool Collision = !Preview;
	switch(m_Entity)
	{
	case CGameWorld::ENTTYPE_PICKUP:
	{
		CPickup *pPickup = new CPickup(m_pCharacter->GameWorld(), m_Pickup.m_Type, m_Pickup.m_SubType, 0, 0, 0);
		pPickup->m_Pos = m_Pos;
		return pPickup;
	}
	case CGameWorld::ENTTYPE_DOOR:
		return new CDoor(m_pCharacter->GameWorld(), m_Pos, m_Laser.m_Angle, 32 * m_Laser.m_Length, 0, Collision);
	case CGameWorld::ENTTYPE_BUTTON:
		return new CButton(m_pCharacter->GameWorld(), m_Pos, m_Laser.m_Number + 1, Collision);
	case CGameWorld::ENTTYPE_SPEEDUP:
		return new CSpeedup(m_pCharacter->GameWorld(), m_Pos, (float)m_Speedup.m_Angle, m_Speedup.m_Force, m_Speedup.m_MaxSpeed, Collision);
	case CGameWorld::ENTTYPE_TELEPORTER:
	{
		const int Number = m_Teleporter.m_Number + 1;
		return new CTeleporter(m_pCharacter->GameWorld(), m_Pos, GetTeleporterType(), Number, Collision);
	}
	case CGameWorld::ENTTYPE_HELICOPTER:
		if(Preview)
			return nullptr;
		return new CHelicopter(m_pCharacter->GameWorld(), m_Pos, -1);
	}
	return nullptr;
}

CEntity *CDrawEditor::CreateTransformEntity(CEntity *pTemplate)
{
	if(!pTemplate)
		return nullptr;

	const vec2 Pos = pTemplate->GetPos();
	switch(pTemplate->GetObjType())
	{
	case CGameWorld::ENTTYPE_PICKUP:
	{
		CPickup *pSrc = (CPickup *)pTemplate;
		CPickup *pPickup = new CPickup(pTemplate->GameWorld(), pSrc->Type(), pSrc->Subtype(), 0, 0, 0);
		pPickup->m_Pos = Pos;
		return pPickup;
	}
	case CGameWorld::ENTTYPE_DOOR:
	{
		CDoor *pSrc = (CDoor *)pTemplate;
		return new CDoor(pTemplate->GameWorld(), Pos, pSrc->GetRotation(), pSrc->GetLength(), pTemplate->m_Number);
	}
	case CGameWorld::ENTTYPE_BUTTON:
		return new CButton(pTemplate->GameWorld(), Pos, ((CButton *)pTemplate)->GetNumber(), true);
	case CGameWorld::ENTTYPE_SPEEDUP:
	{
		CSpeedup *pSrc = (CSpeedup *)pTemplate;
		return new CSpeedup(pTemplate->GameWorld(), Pos, (float)pSrc->GetAngle(), pSrc->GetForce(), pSrc->GetMaxSpeed(), true);
	}
	case CGameWorld::ENTTYPE_TELEPORTER:
	{
		CTeleporter *pSrc = (CTeleporter *)pTemplate;
		return new CTeleporter(pTemplate->GameWorld(), Pos, pSrc->GetType(), pSrc->GetNumber(), true);
	}
	case CGameWorld::ENTTYPE_HELICOPTER:
		return new CHelicopter(pTemplate->GameWorld(), Pos, -1);
	}
	return nullptr;
}

void CDrawEditor::StopTransform(bool Silent)
{
	(void)Silent;
	m_Transform.m_State = TRANSFORM_STATE_SETTING_FIRST;
	m_Transform.m_Angle = 0;
	m_Transform.m_vPreview.clear();
	m_Transform.m_vSelected.clear();
}

void CDrawEditor::SendWindow()
{
	char aMsg[900];
	str_format(aMsg, sizeof(aMsg), "     > %s <\n\n", GetCategory(m_Category));

	{
		const int PlotId = GetCursorPlotID();
		char aObjects[64];
		str_format(aObjects, sizeof(aObjects), "Objects [%u/%u]\n\n",
			GameServer()->NumPlotObjects(PlotId), GameServer()->GetMaxPlotObjects(PlotId));
		str_append(aMsg, aObjects, sizeof(aMsg));
	}

	str_append(aMsg,
		"     Menu controls:\n\n"
		"Change category: hook left/right\n"
		"Move up/down: shoot left/right\n",
		sizeof(aMsg));
	if(m_Category == CAT_PICKUPS || m_Category == CAT_LASERWALLS || m_Category == CAT_SPEEDUPS ||
		m_Category == CAT_TELEPORTER || m_Category == CAT_LASERDOORS || m_Category == CAT_TRANSFORM)
		str_append(aMsg, "Change setting: A/D\n", sizeof(aMsg));

	str_append(aMsg, "\n", sizeof(aMsg));
	str_append(aMsg,
		"     Controls:\n\n"
		"Stop editing: Switch weapon\n"
		"Eraser: Right mouse\n"
		"Place object: Left mouse\n",
		sizeof(aMsg));

	if(m_Category == CAT_LASERWALLS)
	{
		str_append(aMsg,
			"Change angle: A/D\n"
			"Change length: TAB + A/D\n"
			"Add 45 degree steps: TAB + kill\n",
			sizeof(aMsg));
	}
	else if(m_Category == CAT_SPEEDUPS)
	{
		str_append(aMsg,
			"Change angle: A/D\n"
			"Add 45 degree steps: TAB + kill\n",
			sizeof(aMsg));
	}
	else if(m_Category == CAT_TRANSFORM)
	{
		str_append(aMsg,
			"Set area corners with left mouse\n"
			"Confirm / place with left mouse\n"
			"Abort: Right mouse\n",
			sizeof(aMsg));
	}

	str_append(aMsg, "\n", sizeof(aMsg));
	char aBuf[256];
	if(m_Category == CAT_PICKUPS)
	{
		str_append(aMsg, "     Objects:\n\n", sizeof(aMsg));
		for(int i = 0; i < NUM_DRAW_PICKUPS; i++)
			str_append(aMsg, FormatSetting(GetPickupName(i), i), sizeof(aMsg));
	}
	else if(m_Category == CAT_LASERWALLS)
	{
		str_append(aMsg, "     Settings:\n\n", sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Length: %d/%d tiles", m_Laser.m_Length, MAX_LASERWALL_LENGTH);
		str_append(aMsg, FormatSetting(aBuf, LASERWALL_LENGTH), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Angle: %d deg", ClampAngle(round_to_int(m_Laser.m_Angle * 180.f / pi)));
		str_append(aMsg, aBuf, sizeof(aMsg));
		str_append(aMsg, "\n", sizeof(aMsg));
	}
	else if(m_Category == CAT_LASERDOORS)
	{
		str_append(aMsg, "     Settings:\n\n", sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Number: %d/%d", m_Laser.m_Number + 1, MAX_DRAW_NUMBERS);
		str_append(aMsg, FormatSetting(aBuf, LASERDOOR_NUMBER), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Mode: %s", m_Laser.m_ButtonMode ? "Button" : "Door");
		str_append(aMsg, FormatSetting(aBuf, LASERDOOR_MODE), sizeof(aMsg));
	}
	else if(m_Category == CAT_SPEEDUPS)
	{
		str_append(aMsg, "     Settings:\n\n", sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Force speed: %d", m_Speedup.m_Force);
		str_append(aMsg, FormatSetting(aBuf, SPEEDUP_FORCE), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Max speed: %d", m_Speedup.m_MaxSpeed);
		str_append(aMsg, FormatSetting(aBuf, SPEEDUP_MAXSPEED), sizeof(aMsg));
	}
	else if(m_Category == CAT_TELEPORTER)
	{
		str_append(aMsg, "     Settings:\n\n", sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Number: %d/%d", m_Teleporter.m_Number + 1, MAX_DRAW_NUMBERS);
		str_append(aMsg, FormatSetting(aBuf, TELEPORTER_NUMBER), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Mode: %s", GetTeleporterMode());
		str_append(aMsg, FormatSetting(aBuf, TELEPORTER_MODE), sizeof(aMsg));
		str_format(aBuf, sizeof(aBuf), "Evil: %s", m_Teleporter.m_Evil ? "Yes" : "No");
		str_append(aMsg, FormatSetting(aBuf, TELEPORTER_EVIL), sizeof(aMsg));
	}
	else if(m_Category == CAT_TRANSFORM)
	{
		str_append(aMsg, "     Mode:\n\n", sizeof(aMsg));
		str_append(aMsg, FormatSetting("Move", TRANSFORM_MOVE), sizeof(aMsg));
		str_append(aMsg, FormatSetting("Copy", TRANSFORM_COPY), sizeof(aMsg));
		str_append(aMsg, FormatSetting("Erase", TRANSFORM_ERASE), sizeof(aMsg));
		const char *pState = "set first corner";
		if(m_Transform.m_State == TRANSFORM_STATE_SETTING_SECOND)
			pState = "set second corner";
		else if(m_Transform.m_State == TRANSFORM_STATE_CONFIRM)
			pState = "confirm selection";
		else if(m_Transform.m_State == TRANSFORM_STATE_RUNNING)
			pState = "place with left mouse";
		str_format(aBuf, sizeof(aBuf), "\nState: %s\n", pState);
		str_append(aMsg, aBuf, sizeof(aMsg));
	}
	else if(m_Category == CAT_HELICOPTER)
	{
		str_append(aMsg, "     Helicopter\n\nPlace with left mouse.\n", sizeof(aMsg));
	}

	GameServer()->SendMotd(aMsg, GetCid());
}

const char *CDrawEditor::FormatSetting(const char *pSetting, int Setting)
{
	static char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s%s%s\n", m_Setting == Setting ? "> " : "", pSetting, m_Setting == Setting ? " <" : "");
	return aBuf;
}

const char *CDrawEditor::GetCategory(int Category)
{
	switch(Category)
	{
	case CAT_PICKUPS: return "Pickups";
	case CAT_LASERWALLS: return "Laser Walls";
	case CAT_LASERDOORS: return "Doors / Buttons";
	case CAT_SPEEDUPS: return "Speedups";
	case CAT_TELEPORTER: return "Teleporters";
	case CAT_TRANSFORM: return "Transformation";
	case CAT_HELICOPTER: return "Helicopter";
	default: return "Unknown";
	}
}

const char *CDrawEditor::GetPickupName(int Pickup)
{
	switch(Pickup)
	{
	case DRAW_PICKUP_HEART: return "Heart";
	case DRAW_PICKUP_SHIELD: return "Shield";
	case DRAW_PICKUP_HAMMER: return "Hammer";
	case DRAW_PICKUP_GUN: return "Gun";
	case DRAW_PICKUP_SHOTGUN: return "Shotgun";
	case DRAW_PICKUP_GRENADE: return "Grenade";
	case DRAW_PICKUP_LASER: return "Laser";
	default: return "Unknown";
	}
}

int CDrawEditor::GetNumSettings()
{
	switch(m_Category)
	{
	case CAT_PICKUPS: return NUM_DRAW_PICKUPS;
	case CAT_LASERWALLS: return NUM_LASERWALL_SETTINGS;
	case CAT_LASERDOORS: return NUM_LASERDOOR_SETTINGS;
	case CAT_SPEEDUPS: return NUM_SPEEDUP_SETTINGS;
	case CAT_TELEPORTER: return NUM_TELEPORTERS_SETTINGS;
	case CAT_TRANSFORM: return NUM_TRANSFORM_SETTINGS;
	case CAT_HELICOPTER: return 0;
	default: return 0;
	}
}

const char *CDrawEditor::GetTeleporterMode()
{
	switch(m_Teleporter.m_Mode)
	{
	case TELE_MODE_TOGGLE: return "In (no In/Out tile)";
	case TELE_MODE_IN: return "From";
	case TELE_MODE_OUT: return "To";
	case TELE_MODE_WEAPON: return "Weapon From";
	case TELE_MODE_HOOK: return "Hook From";
	default: return "Unknown";
	}
}

int CDrawEditor::GetTeleporterType()
{
	switch(m_Teleporter.m_Mode)
	{
	case TELE_MODE_TOGGLE:
		return m_Teleporter.m_Evil ? TILE_TELEINEVIL : TILE_TELEIN;
	case TELE_MODE_IN:
		return m_Teleporter.m_Evil ? TILE_TELEINEVIL : TILE_TELEIN;
	case TELE_MODE_OUT:
		return TILE_TELEOUT;
	case TELE_MODE_WEAPON:
		return TILE_TELEINWEAPON;
	case TELE_MODE_HOOK:
		return TILE_TELEINHOOK;
	default:
		return TILE_TELEOUT;
	}
}

int CDrawEditor::GetCid() const
{
	return m_pCharacter->GetPlayer()->GetCid();
}

void CDrawEditor::OnPlayerKill()
{
	if(m_pCharacter->GetPlayer()->m_PlayerFlags & PLAYERFLAG_SCOREBOARD)
	{
		if(m_Category == CAT_SPEEDUPS || (IsCategoryLaser() && !m_Laser.m_ButtonMode))
		{
			if(m_Category == CAT_SPEEDUPS)
			{
				int Angle = m_Speedup.m_Angle;
				if(Angle % 45 != 0 || Angle >= 360)
					SetAngle(0);
				else
					AddAngle(45);
			}
			else
			{
				float Deg = m_Laser.m_Angle * 180.f / pi;
				int Angle = ClampAngle(round_to_int(Deg));
				if(Angle % 45 != 0)
					SetAngle(0);
				else
					AddAngle(45);
			}
		}
	}
	else if(m_Category == CAT_LASERDOORS || m_Category == CAT_TELEPORTER)
	{
		if(m_Selecting)
		{
			if(m_Category == CAT_LASERDOORS)
			{
				m_Laser.m_Number = (m_Laser.m_Number + 1) % MAX_DRAW_NUMBERS;
				SendWindow();
			}
			else if(m_Category == CAT_TELEPORTER)
			{
				m_Teleporter.m_Number = (m_Teleporter.m_Number + 1) % MAX_DRAW_NUMBERS;
				SendWindow();
			}
		}
	}
	else if(m_Category != CAT_SPEEDUPS)
	{
		m_RoundPos = !m_RoundPos;
	}
}

void CDrawEditor::SetAngle(float Angle)
{
	if(IsCategoryLaser() && !m_Laser.m_ButtonMode)
	{
		m_Laser.m_Angle = Angle;
		if(m_pPreview && m_pPreview->GetObjType() == CGameWorld::ENTTYPE_DOOR)
			((CDoor *)m_pPreview)->SetDirection(m_Laser.m_Angle);
	}
	else if(m_Category == CAT_SPEEDUPS)
	{
		m_Speedup.m_Angle = ClampAngle((int)Angle);
		if(m_pPreview && m_pPreview->GetObjType() == CGameWorld::ENTTYPE_SPEEDUP)
			((CSpeedup *)m_pPreview)->SetAngle(m_Speedup.m_Angle);
	}
	else if(m_Category == CAT_TRANSFORM && m_Transform.m_State == TRANSFORM_STATE_RUNNING)
		m_Transform.m_Angle = ClampAngle((int)Angle);
}

void CDrawEditor::AddAngle(float Add)
{
	if(IsCategoryLaser() && !m_Laser.m_ButtonMode)
	{
		float NewAngle = ((m_Laser.m_Angle * 180.f / pi) + Add) * pi / 180.f;
		SetAngle(ClampRotation(NewAngle));
	}
	else
	{
		int Angle = 0;
		bool Set = false;
		if(m_Category == CAT_SPEEDUPS)
		{
			Angle = m_Speedup.m_Angle;
			Set = true;
		}
		else if(m_Category == CAT_TRANSFORM)
		{
			Angle = m_Transform.m_Angle;
			Set = true;
		}
		if(Set)
			SetAngle((float)ClampAngle(Angle - (int)Add));
	}
}

void CDrawEditor::AddLength(int Add)
{
	if(!IsCategoryLaser() || m_Laser.m_ButtonMode)
		return;
	m_Laser.m_Length = std::clamp(m_Laser.m_Length + Add, 1, (int)MAX_LASERWALL_LENGTH);
	if(m_pPreview && m_pPreview->GetObjType() == CGameWorld::ENTTYPE_DOOR)
		((CDoor *)m_pPreview)->SetLength(32 * m_Laser.m_Length);
	if(m_Selecting)
		SendWindow();
}

float CDrawEditor::ClampRotation(float Rotation)
{
	float Angle = Rotation * 180.f / pi;
	if(Angle >= 360.f)
		Angle -= 360.f;
	else if(Angle < 0.f)
		Angle += 360.f;
	return Angle * pi / 180.f;
}

int CDrawEditor::ClampAngle(int Angle)
{
	if(Angle >= 360)
		Angle -= 360;
	else if(Angle < 0)
		Angle += 360;
	return Angle;
}

void CDrawEditor::OnWeaponSwitch()
{
	if(Active())
	{
		SetPreview();
	}
	else if(m_pCharacter->GetLastWeapon() == WEAPON_DRAW_EDITOR)
	{
		StopTransform(true);
		RemovePreview();
	}
	else
		return;

	GameServer()->SendTuningParams(GetCid(), m_pCharacter->m_TuneZone);
}

void CDrawEditor::OnPlayerDeath()
{
	StopTransform(true);
	RemovePreview();
}

void CDrawEditor::SetPreview()
{
	if(m_pPreview || m_Entity == -1)
		return;

	m_pPreview = CreateEntity(true);
	if(m_pPreview)
		m_pPreview->m_BrushCID = GetCid();
}

void CDrawEditor::RemovePreview()
{
	if(!m_pPreview)
		return;
	m_pPreview->Destroy();
	m_pPreview = nullptr;
}

void CDrawEditor::UpdatePreview()
{
	if(m_Category == CAT_UNINITIALIZED)
		return;
	RemovePreview();
	SetPreview();
}

bool CDrawEditor::OnSnapPreview(CEntity *pEntity)
{
	if(!pEntity)
		return false;
	// Hide brush ghosts from other clients; hide from owner while erasing or when placement is invalid
	if(pEntity->m_BrushCID != -1)
	{
		if(pEntity->m_BrushCID != GetCid() || m_Erasing || !CanPlace(pEntity))
			return true;
	}
	return false;
}

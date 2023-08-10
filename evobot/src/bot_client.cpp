//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_client.cpp
// 
// Contains all network message handling by the bot
//

#ifndef _WIN32
#include <string.h>
#endif

#include "bot_client.h"

#include <extdll.h>
#include <dllapi.h>
#include <h_export.h>
#include <meta_api.h>


#include "game_state.h"
#include "bot_tactical.h"
#include "player_util.h"
#include "bot_util.h"
#include "bot_commander.h"

// instant damage (from cbase.h)
#define DMG_CRUSH			(1 << 0)	// crushed by falling or moving object
#define DMG_BURN			(1 << 3)	// heat burned
#define DMG_FREEZE			(1 << 4)	// frozen
#define DMG_FALL			(1 << 5)	// fell too far
#define DMG_SHOCK			(1 << 8)	// electric shock
#define DMG_DROWN			(1 << 14)	// Drowning
#define DMG_NERVEGAS		(1 << 16)	// nerve toxins, very bad
#define DMG_RADIATION		(1 << 18)	// radiation exposure
#define DMG_DROWNRECOVER	(1 << 19)	// drowning recovery
#define DMG_ACID			(1 << 20)	// toxic chemicals or acid burns
#define DMG_SLOWBURN		(1 << 21)	// in an oven
#define DMG_SLOWFREEZE		(1 << 22)	// in a subzero freezer

// types of damage to ignore...
#define IGNORE_DAMAGE (DMG_CRUSH | DMG_BURN | DMG_FREEZE | DMG_FALL | \
                       DMG_SHOCK | DMG_DROWN | DMG_NERVEGAS | DMG_RADIATION | \
                       DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN | \
                       DMG_SLOWFREEZE | 0xFF000000)


extern bot_weapon_t weapon_defs[MAX_WEAPONS]; // array of weapon definitions
extern bot_t bots[MAX_CLIENTS];
extern edict_t* clients[MAX_CLIENTS];
extern bool bGameIsActive;
extern int GameStatus;

hive_info_msg HiveInfo;
alert_msg AlertInfo;
setup_map_msg MapInfo;
receive_order_msg OrderInfo;
game_status_msg GameInfo;
selection_msg SelectionInfo;
damage_msg DamageInfo;
death_msg DeathInfo;
current_weapon_msg CurrentWeaponInfo;
ammox_msg AmmoXInfo;

// This message is sent when a client joins the game.  All of the weapons
// are sent with the weapon ID and information about what ammo is used.
void BotClient_Valve_WeaponList(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static bot_weapon_t bot_weapon;

	if (state == 0)
	{
		state++;
		strcpy(bot_weapon.szClassname, (char*)p);

		bot_weapon.MinRefireTime = 0.0f;

		if (!strcmp(bot_weapon.szClassname, "weapon_pistol"))
		{
			bot_weapon.iClipSize = 10;
			bot_weapon.MinRefireTime = 0.15f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_machinegun"))
		{
			bot_weapon.iClipSize = 50;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_shotgun"))
		{
			bot_weapon.iClipSize = 8;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_heavymachinegun"))
		{
			bot_weapon.iClipSize = 125;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_grenadegun"))
		{
			bot_weapon.iClipSize = 4;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_grenade"))
		{
			bot_weapon.iClipSize = 2;
			bot_weapon.MinRefireTime = 2.0f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_divinewind"))
		{
			bot_weapon.MinRefireTime = 0.5f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_umbra"))
		{
			bot_weapon.MinRefireTime = 0.5f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_stomp"))
		{
			bot_weapon.MinRefireTime = 1.0f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_spore"))
		{
			bot_weapon.MinRefireTime = 1.0f;
		}


	}
	else if (state == 1)
	{
		state++;
		bot_weapon.iAmmo1 = *(int*)p;  // ammo index 1
	}
	else if (state == 2)
	{
		state++;
		bot_weapon.iAmmo1Max = *(int*)p;  // max ammo1
	}
	else if (state == 3)
	{
		state++;
		bot_weapon.iAmmo2 = *(int*)p;  // ammo index 2
	}
	else if (state == 4)
	{
		state++;
		bot_weapon.iAmmo2Max = *(int*)p;  // max ammo2
	}
	else if (state == 5)
	{
		state++;
		bot_weapon.iSlot = *(int*)p;  // slot for this weapon
	}
	else if (state == 6)
	{
		state++;
		bot_weapon.iPosition = *(int*)p;  // position in slot
	}
	else if (state == 7)
	{
		state++;
		bot_weapon.iId = *(int*)p;  // weapon ID
	}
	else if (state == 8)
	{
		state = 0;

		bot_weapon.iFlags = *(int*)p;  // flags for weapon (WTF???)

		weapon_defs[bot_weapon.iId] = bot_weapon;
		bots[bot_index].m_clipAmmo[bot_weapon.iId] = bot_weapon.iClipSize;
	}
}

void BotClient_NS_Alert_Reset()
{
	memset(&AlertInfo, 0, sizeof(alert_msg));
}

void BotClient_NS_Alert_32(void* p, int bot_index)
{

	if (AlertInfo.state == 0)
	{
		AlertInfo.flag = *(int*)p;
		AlertInfo.state++;
	}
	else if (AlertInfo.state == 1)
	{
		AlertInfo.AlertType = *(int*)p;
		AlertInfo.state++;
	}
	else if (AlertInfo.state == 2)
	{
		AlertInfo.LocationX = *(float*)p;
		AlertInfo.state++;
	}
	else if (AlertInfo.state == 3)
	{
		AlertInfo.LocationY = *(float*)p;

		bot_t* pBot = &bots[bot_index];

		if (pBot->is_used && !FNullEnt(pBot->pEdict))
		{

			if (IsPlayerCommander(pBot->pEdict))
			{
				CommanderReceiveAlert(pBot, Vector(AlertInfo.LocationX, AlertInfo.LocationY, 0.0f), (PlayerAlertType)AlertInfo.AlertType);
			}
			else if (IsPlayerOnAlienTeam(pBot->pEdict))
			{
				if (AlertInfo.flag == 0)
				{
					//AlienReceiveAlert(pBot, Vector(LocationX, LocationY, 0.0f), (PlayerAlertType)AlertType);
				}
			}
		}

		AlertInfo.state = 0;
	}
}

void BotClient_NS_Alert_33(void* p, int bot_index)
{


	if (AlertInfo.state == 0)
	{
		AlertInfo.flag = *(int*)p;
		AlertInfo.bIsRequest = (AlertInfo.flag == 0 || AlertInfo.flag == 1);
		AlertInfo.state++;
	}
	else if (AlertInfo.state == 1)
	{
		if (AlertInfo.bIsRequest)
		{
			AlertInfo.AlertType = *(int*)p;
		}
		else
		{
			AlertInfo.NumResearch = *(int*)p;
			AlertInfo.ResearchCounter = 0;
		}
		AlertInfo.state++;
	}
	else if (AlertInfo.state == 2)
	{
		if (AlertInfo.bIsRequest)
		{
			AlertInfo.LocationX = *(float*)p;
		}
		else
		{
			AlertInfo.ResearchId = *(int*)p;
		}
		AlertInfo.state++;
	}
	else if (AlertInfo.state == 3)
	{
		bot_t* pBot = &bots[bot_index];

		if (AlertInfo.bIsRequest)
		{
			AlertInfo.LocationY = *(float*)p;

			if (pBot->is_used && !FNullEnt(pBot->pEdict))
			{

				if (IsPlayerCommander(pBot->pEdict))
				{
					CommanderReceiveAlert(pBot, Vector(AlertInfo.LocationX, AlertInfo.LocationY, 0.0f), (PlayerAlertType)AlertInfo.AlertType);
				}
				else if (IsPlayerOnAlienTeam(pBot->pEdict))
				{
					if (AlertInfo.flag == 0)
					{
						//AlienReceiveAlert(pBot, Vector(LocationX, LocationY, 0.0f), (PlayerAlertType)AlertType);
					}
				}
			}

			AlertInfo.state = 0;


		}
		else
		{
			AlertInfo.ResearchProgress = *(int*)p;
			AlertInfo.ResearchCounter++;

			if (AlertInfo.ResearchCounter >= AlertInfo.NumResearch)
			{
				AlertInfo.state = 0;
			}
			else
			{
				AlertInfo.state = 2;
			}
		}

	}
}

void BotClient_NS_SetSelect_Reset()
{
	memset(&SelectionInfo, 0, sizeof(selection_msg));
}

void BotClient_NS_SetSelect(void* p, int bot_index)
{
	if (SelectionInfo.state == 0)
	{

		memset(SelectionInfo.SelectedEntities, 0, sizeof(SelectionInfo.SelectedEntities));
		SelectionInfo.group_number = *(int*)p;


		SelectionInfo.state++;
	}
	else if (SelectionInfo.state == 1)
	{
		SelectionInfo.selected_entity_count = *(int*)p;

		if (SelectionInfo.selected_entity_count == 0)
		{
			bots[bot_index].CommanderCurrentlySelectedBuilding = nullptr;
			SelectionInfo.state++;
		}

		SelectionInfo.counted_entities = 0;
		SelectionInfo.state++;
	}
	else if (SelectionInfo.state == 2)
	{
		SelectionInfo.SelectedEntities[SelectionInfo.counted_entities++] = *(int*)p;

		edict_t* SelectedEntity = INDEXENT(*(int*)p);

		if (!FNullEnt(SelectedEntity) && UTIL_IsMarineStructure(SelectedEntity))
		{
			bots[bot_index].CommanderCurrentlySelectedBuilding = SelectedEntity;
		}
		else
		{
			bots[bot_index].CommanderCurrentlySelectedBuilding = nullptr;
		}

		if (SelectionInfo.counted_entities >= SelectionInfo.selected_entity_count)
		{
			SelectionInfo.state++;
		}
	}
	else if (SelectionInfo.state == 3)
	{
		switch (SelectionInfo.group_number)
		{
		case 0:
		{
			SelectionInfo.TrackingEntity = *(int*)p;

			SelectionInfo.state = 0;
		}
		break;
		case kSelectAllHotGroup:
		{
			SelectionInfo.state = 0;
		}
		break;
		default:
		{
			SelectionInfo.group_type = *(int*)p;
			SelectionInfo.TrackingEntity = *(int*)p;

			SelectionInfo.state++;
		}
		break;
		}
	}
	else if (SelectionInfo.state == 4)
	{
		SelectionInfo.group_alert = *(int*)p;

		SelectionInfo.state = 0;
	}

}

void BotClient_Valve_CurrentWeapon_Reset()
{
	memset(&CurrentWeaponInfo, 0, sizeof(current_weapon_msg));
}

void BotClient_Valve_CurrentWeapon(void* p, int bot_index)
{
	if (CurrentWeaponInfo.state == 0)
	{
		CurrentWeaponInfo.state++;
		CurrentWeaponInfo.iState = *(int*)p;  // state of the current weapon
	}
	else if (CurrentWeaponInfo.state == 1)
	{
		CurrentWeaponInfo.state++;
		CurrentWeaponInfo.iId = *(int*)p;  // weapon ID of current weapon
	}
	else if (CurrentWeaponInfo.state == 2)
	{
		CurrentWeaponInfo.state = 0;

		CurrentWeaponInfo.iClip = *(int*)p;  // ammo currently in the clip for this weapon

		if (CurrentWeaponInfo.iId <= 31)
		{
			if ((CurrentWeaponInfo.iState & WEAPON_IS_CURRENT))
			{
				bots[bot_index].current_weapon.iId = CurrentWeaponInfo.iId;
				bots[bot_index].current_weapon.iClip = CurrentWeaponInfo.iClip;
				bots[bot_index].current_weapon.MinRefireTime = weapon_defs[CurrentWeaponInfo.iId].MinRefireTime;

				bots[bot_index].m_clipAmmo[CurrentWeaponInfo.iId] = CurrentWeaponInfo.iClip;

				// update the ammo counts for this weapon...
				bots[bot_index].current_weapon.iAmmo1 =
					bots[bot_index].m_rgAmmo[weapon_defs[CurrentWeaponInfo.iId].iAmmo1];
				bots[bot_index].current_weapon.iAmmo1Max =
					weapon_defs[CurrentWeaponInfo.iId].iAmmo1Max;
				bots[bot_index].current_weapon.iAmmo2 =
					bots[bot_index].m_rgAmmo[weapon_defs[CurrentWeaponInfo.iId].iAmmo2];
				bots[bot_index].current_weapon.iAmmo2Max =
					weapon_defs[CurrentWeaponInfo.iId].iAmmo2Max;
				bots[bot_index].current_weapon.iClipMax =
					weapon_defs[CurrentWeaponInfo.iId].iClipSize;
			}
		}

	}
}

void BotClient_Valve_AmmoX_Reset()
{
	memset(&AmmoXInfo, 0, sizeof(ammox_msg));
}

// This message is sent whenever ammo ammounts are adjusted (up or down).
void BotClient_Valve_AmmoX(void* p, int bot_index)
{

	if (AmmoXInfo.state == 0)
	{
		AmmoXInfo.state++;
		AmmoXInfo.index = *(int*)p;  // ammo index (for type of ammo)
	}
	else if (AmmoXInfo.state == 1)
	{
		AmmoXInfo.state = 0;

		AmmoXInfo.amount = *(int*)p;  // the ammount of ammo currently available

		bots[bot_index].m_rgAmmo[AmmoXInfo.index] = AmmoXInfo.amount;  // store it away

		AmmoXInfo.ammo_index = bots[bot_index].current_weapon.iId;

		// update the ammo counts for this weapon...
		bots[bot_index].current_weapon.iAmmo1 =
			bots[bot_index].m_rgAmmo[weapon_defs[AmmoXInfo.ammo_index].iAmmo1];
		bots[bot_index].current_weapon.iAmmo2 =
			bots[bot_index].m_rgAmmo[weapon_defs[AmmoXInfo.ammo_index].iAmmo2];
		bots[bot_index].current_weapon.iAmmo1Max =
			weapon_defs[AmmoXInfo.ammo_index].iAmmo1Max;
		bots[bot_index].current_weapon.iAmmo2Max =
			weapon_defs[AmmoXInfo.ammo_index].iAmmo2Max;
		bots[bot_index].current_weapon.iClipMax =
			weapon_defs[AmmoXInfo.ammo_index].iClipSize;

	}
}

void BotClient_NS_Damage_Reset()
{
	memset(&DamageInfo, 0, sizeof(damage_msg));
}

void BotClient_NS_Damage(void* p, int bot_index)
{

	if (DamageInfo.state == 0)
	{
		DamageInfo.state++;
		DamageInfo.damage_armor = *(int*)p;
	}
	else if (DamageInfo.state == 1)
	{
		DamageInfo.state++;
		DamageInfo.damage_taken = *(int*)p;
	}
	else if (DamageInfo.state == 2)
	{
		DamageInfo.state++;
		DamageInfo.damage_bits = *(int*)p;
	}
	else if (DamageInfo.state == 3)
	{
		DamageInfo.state++;
		DamageInfo.damage_origin.x = *(float*)p;
	}
	else if (DamageInfo.state == 4)
	{
		DamageInfo.state++;
		DamageInfo.damage_origin.y = *(float*)p;
	}
	else if (DamageInfo.state == 5)
	{
		DamageInfo.damage_origin.z = *(float*)p;

		if (DamageInfo.damage_taken > 0)
		{

			edict_t* aggressor = nullptr;
			float MinDistSq = 0.0f;
			float MaxDistSq = sqrf(50.0f);

			for (int i = 0; i < 32; i++)
			{
				if (clients[i] != NULL && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && !IsPlayerCommander(clients[i]))
				{
					float Dist = vDist3DSq(clients[i]->v.origin, DamageInfo.damage_origin);

					if (Dist <= MaxDistSq)
					{
						if (!aggressor || Dist < MinDistSq)
						{
							aggressor = clients[i];
							MinDistSq = Dist;
						}
					}
				}
			}

			if (!FNullEnt(aggressor))
			{
				BotTakeDamage(&bots[bot_index], DamageInfo.damage_taken, aggressor);
			}
		}

		DamageInfo.state = 0;
	}
}

void BotClient_NS_GameStatus_Reset()
{
	memset(&GameInfo, 0, sizeof(game_status_msg));
}

// This message gets sent when the bots money ammount changes (for CS)
void BotClient_NS_GameStatus(void* p, int bot_index)
{
	if (GameInfo.state == 0)
	{
		GameInfo.StatusCode = *(int*)p;

		GameStatus = *(int*)p;

		if (GameInfo.StatusCode == kGameStatusGameTime)
		{
			bGameIsActive = true;
		}
		else
		{
			bGameIsActive = false;
		}

		GameInfo.state++;
	}
	else if (GameInfo.state == 1)
	{
		if (GameInfo.StatusCode == kGameStatusReset || GameInfo.StatusCode == kGameStatusResetNewMap || GameInfo.StatusCode == kGameStatusEnded)
		{
			GameInfo.state = 0;
		}
		else
		{
			GameInfo.state++;
		}
	}
	else if (GameInfo.state == 2)
	{
		if (GameInfo.StatusCode == kGameStatusUnspentLevels)
		{
			GameInfo.state = 0;
		}
		else
		{
			GameInfo.state++;
		}
	}
	else
	{
		GameInfo.state++;

		if (GameInfo.state > 4)
		{
			GameInfo.state = 0;
		}
	}
}

void BotClient_NS_SetupMap_Reset()
{
	memset(&MapInfo, 0, sizeof(setup_map_msg));
}

void BotClient_NS_SetupMap(void* p, int bot_index)
{

	if (MapInfo.state == 0)
	{
		MapInfo.IsLocation = *(bool*)p;
		MapInfo.state++;
	}
	else if (MapInfo.state == 1)
	{
		if (MapInfo.IsLocation)
		{
			sprintf(MapInfo.LocationName, "%s", (char*)p);
		}

		MapInfo.state++;
	}
	else if (MapInfo.state == 2)
	{
		if (MapInfo.IsLocation)
		{
			MapInfo.LocationMaxX = *(float*)p;
		}

		MapInfo.state++;
	}
	else if (MapInfo.state == 3)
	{
		if (!MapInfo.IsLocation)
		{
			SetCommanderViewZHeight(*(float*)p);
		}
		else
		{
			MapInfo.LocationMaxY = *(float*)p;
		}

		MapInfo.state++;
	}
	else if (MapInfo.state == 4)
	{
		if (MapInfo.IsLocation)
		{
			MapInfo.LocationMinX = *(float*)p;
		}

		MapInfo.state++;
	}
	else if (MapInfo.state == 5)
	{
		if (MapInfo.IsLocation)
		{
			MapInfo.LocationMinY = *(float*)p;

			AddMapLocation(MapInfo.LocationName, Vector(MapInfo.LocationMinX, MapInfo.LocationMinY, 0.0f), Vector(MapInfo.LocationMaxX, MapInfo.LocationMaxY, 0.0f));

			MapInfo.state = 0;
		}
		else
		{
			MapInfo.state++;
		}
	}
	else if (MapInfo.state == 8)
	{
		MapInfo.state = 0;
	}
	else
	{
		MapInfo.state++;
	}

}

void BotClient_NS_DeathMessage_Reset()
{
	memset(&DeathInfo, 0, sizeof(death_msg));
}

// This message gets sent when the bots get killed
void BotClient_NS_DeathMsg(void* p, int bot_index)
{
	if (DeathInfo.state == 0)
	{
		DeathInfo.state++;
		DeathInfo.killer_index = *(int*)p;  // ENTINDEX() of killer
	}
	else if (DeathInfo.state == 1)
	{
		DeathInfo.state++;
		DeathInfo.victim_index = *(int*)p;  // ENTINDEX() of victim
	}
	else if (DeathInfo.state == 2)
	{
		DeathInfo.state = 0;

		DeathInfo.killer_edict = INDEXENT(DeathInfo.killer_index);
		DeathInfo.victim_edict = INDEXENT(DeathInfo.victim_index);

		// get the bot index of the killer...
		DeathInfo.index = GetBotIndex(DeathInfo.killer_edict);

		// get the bot index of the victim...
		DeathInfo.index = GetBotIndex(DeathInfo.victim_edict);

		// is this message about a bot being killed?
		if (DeathInfo.index != -1)
		{

			bot_t* botVictim = GetBotPointer(DeathInfo.victim_edict);
			bot_t* botKiller = GetBotPointer(DeathInfo.killer_edict);

			if (botVictim)
			{
				BotDied(botVictim, DeathInfo.killer_edict);
			}

			if (botKiller)
			{
				BotKilledPlayer(botKiller, DeathInfo.victim_edict);
			}

		}
	}
}

void BotClient_NS_ReceiveOrder_Reset()
{
	memset(&OrderInfo, 0, sizeof(receive_order_msg));
}

void BotClient_NS_ReceiveOrder(void* p, int bot_index)
{
	if (OrderInfo.state == 0)
	{
		int recipientIndex = *(int*)p;
		OrderInfo.recipient = INDEXENT(recipientIndex);
		OrderInfo.state++;
	}
	else if (OrderInfo.state == 1)
	{
		OrderInfo.orderType = *((AvHOrderType*)p);
		OrderInfo.isMovementOrder = true;
		OrderInfo.state++;
	}
	else if (OrderInfo.state == 2)
	{
		if (OrderInfo.isMovementOrder)
		{
			OrderInfo.moveDestination.x = *(float*)p;
		}
		else
		{
			int entityIndex = *(unsigned short*)p;
			OrderInfo.orderTarget = INDEXENT(entityIndex);
		}
		OrderInfo.state++;
	}
	else if (OrderInfo.state == 3)
	{
		if (OrderInfo.isMovementOrder)
		{
			OrderInfo.moveDestination.y = *(float*)p;
		}
		else
		{
			OrderInfo.targetType = *(byte*)p;
		}
		OrderInfo.state++;
	}
	else if (OrderInfo.state == 4)
	{
		if (OrderInfo.isMovementOrder)
		{
			OrderInfo.moveDestination.z = *(float*)p;

		}
		else
		{
			OrderInfo.OrderCompleted = *(byte*)p;
		}

		OrderInfo.state++;
	}
	else if (OrderInfo.state == 5)
	{

		if (OrderInfo.isMovementOrder)
		{
			OrderInfo.targetType = *(byte*)p;
			OrderInfo.state++;
		}
		else
		{
			OrderInfo.OrderNotificationType = *(byte*)p;

			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && bots[i].pEdict == OrderInfo.recipient)
				{
					if (OrderInfo.OrderNotificationType == kOrderStatusActive)
					{

					}
				}
			}

			OrderInfo.state = 0;
		}
	}
	else if (OrderInfo.state == 6)
	{
		OrderInfo.OrderCompleted = *(byte*)p;
		OrderInfo.state++;
	}
	else if (OrderInfo.state == 7)
	{
		OrderInfo.OrderNotificationType = *(byte*)p;
		OrderInfo.state = 0;

		for (int i = 0; i < 32; i++)
		{
			if (bots[i].is_used && bots[i].pEdict == OrderInfo.recipient)
			{
				if (OrderInfo.OrderNotificationType == kOrderStatusActive)
				{
					BotReceiveCommanderOrder(&bots[i], OrderInfo.orderType, (AvHUser3)OrderInfo.targetType, OrderInfo.moveDestination);

				}
			}
		}

	}
}

void BotClient_NS_AlienInfo_Reset()
{
	memset(&HiveInfo, 0, sizeof(hive_info_msg));
}

void BotClient_NS_AlienInfo_32(void* p, int bot_index)
{

	if (HiveInfo.state == 0)
	{
		HiveInfo.Header = *(int*)p;
		HiveInfo.HiveCounter = 0;
		HiveInfo.CoordsRead = 0;

		HiveInfo.bHiveInfo = !(HiveInfo.Header & 0x80);

		HiveInfo.state++;
	}
	else if (HiveInfo.state == 1)
	{
		if (!HiveInfo.bHiveInfo)
		{

			HiveInfo.NumUpgrades = *(int*)p;
			HiveInfo.currUpgrade = 0;
			HiveInfo.state++;
		}
		else
		{

			HiveInfo.CoordsRead = 0;
			HiveInfo.NumHives = HiveInfo.Header;
			SetNumberofHives(HiveInfo.NumHives);
			HiveInfo.Changes = *(AlienInfo_ChangeFlags*)p;

			if (HiveInfo.NumHives == 0)
			{
				HiveInfo.state = 0;
			}
			else
			{
				if (HiveInfo.Changes & COORDS_CHANGED)
				{
					HiveInfo.state = 2;
				}
				else if (HiveInfo.Changes & STATUS_CHANGED)
				{
					HiveInfo.state = 3;
				}
				else if (HiveInfo.Changes & HEALTH_CHANGED)
				{
					HiveInfo.state = 4;
				}
				else
				{
					HiveInfo.HiveCounter++;

					if (HiveInfo.HiveCounter >= HiveInfo.NumHives)
					{
						HiveInfo.state = 0;
					}
					else
					{
						HiveInfo.state = 1;
					}
				}
			}
		}


	}
	else if (HiveInfo.state == 2)
	{
		if (!HiveInfo.bHiveInfo)
		{
			HiveInfo.Upgrades[HiveInfo.currUpgrade++] = *(int*)p;

			if (HiveInfo.currUpgrade >= HiveInfo.NumUpgrades)
			{
				HiveInfo.state = 0;
			}
		}
		else
		{
			if (HiveInfo.CoordsRead == 0)
			{
				HiveInfo.HiveLocation.x = *(float*)p;
				HiveInfo.CoordsRead++;
			}
			else if (HiveInfo.CoordsRead == 1)
			{
				HiveInfo.HiveLocation.y = *(float*)p;
				HiveInfo.CoordsRead++;
			}
			else if (HiveInfo.CoordsRead == 2)
			{
				HiveInfo.HiveLocation.z = *(float*)p;

				SetHiveLocation(HiveInfo.HiveCounter, HiveInfo.HiveLocation);

				HiveInfo.CoordsRead = 0;
				if (HiveInfo.Changes & STATUS_CHANGED)
				{
					HiveInfo.state = 3;
				}
				else if (HiveInfo.Changes & HEALTH_CHANGED)
				{
					HiveInfo.state = 4;
				}
				else
				{
					HiveInfo.HiveCounter++;

					if (HiveInfo.HiveCounter >= HiveInfo.NumHives)
					{
						HiveInfo.state = 0;
					}
					else
					{
						HiveInfo.state = 1;
					}
				}
			}
		}
	}
	else if (HiveInfo.state == 3)
	{
		HiveInfo.HiveStatus = *(int*)p;

		int StatusType = (HiveInfo.HiveStatus >> 3) & 0x03;

		bool bUnderAttack = (HiveInfo.HiveStatus & 0x80) != 0;
		int HivemStatus = HiveInfo.HiveStatus & 0x07;

		SetHiveStatus(HiveInfo.HiveCounter, HivemStatus);
		SetHiveTechStatus(HiveInfo.HiveCounter, StatusType);
		SetHiveUnderAttack(HiveInfo.HiveCounter, bUnderAttack);

		if (HiveInfo.Changes & HEALTH_CHANGED)
		{
			HiveInfo.state = 4;
		}
		else
		{
			HiveInfo.HiveCounter++;

			if (HiveInfo.HiveCounter >= HiveInfo.NumHives)
			{
				HiveInfo.state = 0;
			}
			else
			{
				HiveInfo.state = 1;
			}
		}

	}
	else if (HiveInfo.state == 4)
	{
		HiveInfo.HiveHealthPercent = *(int*)p;
		SetHiveHealthPercent(HiveInfo.HiveCounter, HiveInfo.HiveHealthPercent);
		HiveInfo.HiveCounter++;

		if (HiveInfo.HiveCounter >= HiveInfo.NumHives)
		{
			HiveInfo.state = 0;
		}
		else
		{
			HiveInfo.state = 1;
		}
	}
}

void BotClient_NS_AlienInfo_33(void* p, int bot_index)
{

	if (HiveInfo.state == 0)
	{
		HiveInfo.Header = *(int*)p;
		HiveInfo.HiveCounter = 0;
		HiveInfo.CoordsRead = 0;

		HiveInfo.bHiveInfo = !(HiveInfo.Header & 0x80);

		HiveInfo.state++;
	}
	else if (HiveInfo.state == 1)
	{
		if (!HiveInfo.bHiveInfo)
		{
			HiveInfo.NumUpgrades = *(int*)p;
			HiveInfo.currUpgrade = 0;
			HiveInfo.state++;
		}
		else
		{

			HiveInfo.CoordsRead = 0;
			HiveInfo.NumHives = HiveInfo.Header;
			SetNumberofHives(HiveInfo.NumHives);
			HiveInfo.Changes = *(AlienInfo_ChangeFlags*)p;

			if (HiveInfo.NumHives == 0)
			{
				HiveInfo.state = 0;
			}
			else
			{
				if (HiveInfo.Changes & COORDS_CHANGED)
				{
					HiveInfo.state = 2;
				}
				else if (HiveInfo.Changes & STATUS_CHANGED)
				{
					HiveInfo.state = 3;
				}
				else if (HiveInfo.Changes & HEALTH_CHANGED)
				{
					HiveInfo.state = 4;
				}
				else
				{
					HiveInfo.HiveCounter++;

					if (HiveInfo.HiveCounter >= HiveInfo.NumHives)
					{
						HiveInfo.state = 0;
					}
					else
					{
						HiveInfo.state = 1;
					}
				}
			}
		}


	}
	else if (HiveInfo.state == 2)
	{
		if (!HiveInfo.bHiveInfo)
		{
			HiveInfo.Upgrades[HiveInfo.currUpgrade++] = *(int*)p;

			if (HiveInfo.currUpgrade >= HiveInfo.NumUpgrades)
			{
				HiveInfo.state = 0;
			}
		}
		else
		{
			if (HiveInfo.CoordsRead == 0)
			{
				HiveInfo.HiveLocation.x = *(float*)p;
				HiveInfo.CoordsRead++;
			}
			else if (HiveInfo.CoordsRead == 1)
			{
				HiveInfo.HiveLocation.y = *(float*)p;
				HiveInfo.CoordsRead++;
			}
			else if (HiveInfo.CoordsRead == 2)
			{
				HiveInfo.HiveLocation.z = *(float*)p;

				SetHiveLocation(HiveInfo.HiveCounter, HiveInfo.HiveLocation);

				HiveInfo.CoordsRead = 0;
				if (HiveInfo.Changes & STATUS_CHANGED)
				{
					HiveInfo.state = 3;
				}
				else if (HiveInfo.Changes & HEALTH_CHANGED)
				{
					HiveInfo.state = 4;
				}
				else
				{
					HiveInfo.HiveCounter++;

					if (HiveInfo.HiveCounter >= HiveInfo.NumHives)
					{
						HiveInfo.state = 0;
					}
					else
					{
						HiveInfo.state = 1;
					}
				}
			}
		}
	}
	else if (HiveInfo.state == 3)
	{
		HiveInfo.HiveStatus = *(int*)p;

		int StatusType = (HiveInfo.HiveStatus >> 3) & 0x03;

		bool bUnderAttack = (HiveInfo.HiveStatus & 0x80) != 0;
		int HivemStatus = HiveInfo.HiveStatus & 0x07;

		SetHiveStatus(HiveInfo.HiveCounter, HivemStatus);
		SetHiveTechStatus(HiveInfo.HiveCounter, StatusType);
		SetHiveUnderAttack(HiveInfo.HiveCounter, bUnderAttack);

		if (HiveInfo.Changes & HEALTH_CHANGED)
		{
			HiveInfo.state = 4;
		}
		else
		{
			HiveInfo.HiveCounter++;

			if (HiveInfo.HiveCounter >= HiveInfo.NumHives)
			{
				HiveInfo.state = 0;
			}
			else
			{
				HiveInfo.state = 1;
			}
		}

	}
	else if (HiveInfo.state == 4)
	{
		HiveInfo.HiveHealthPercent = *(int*)p;
		SetHiveHealthPercent(HiveInfo.HiveCounter, HiveInfo.HiveHealthPercent);
		HiveInfo.state = 5;
	}
	else if (HiveInfo.state == 5)
	{
		HiveInfo.HiveBuildTime = *(int*)p;

		HiveInfo.HiveCounter++;

		if (HiveInfo.HiveCounter >= HiveInfo.NumHives)
		{
			HiveInfo.state = 0;
		}
		else
		{
			HiveInfo.state = 1;
		}
	}
}
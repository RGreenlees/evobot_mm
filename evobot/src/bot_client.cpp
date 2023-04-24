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
		else if (!strcmp(bot_weapon.szClassname, "weapon_stomp"))
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

void BotClient_NS_Alert_32(void* p, int bot_index)
{
	static int state = 0;
	static int flag = 0;
	static int AlertType = 0;
	static float LocationX = 0.0f;
	static float LocationY = 0.0f;

	if (state == 0)
	{
		flag = *(int*)p;
		state++;
	}
	else if (state == 1)
	{
		AlertType = *(int*)p;
		state++;
	}
	else if (state == 2)
	{
		LocationX = *(float*)p;
		state++;
	}
	else if (state == 3)
	{
		LocationY = *(float*)p;

		bot_t* pBot = &bots[bot_index];

		if (pBot->is_used && !FNullEnt(pBot->pEdict))
		{

			if (IsPlayerCommander(pBot->pEdict))
			{
				CommanderReceiveAlert(pBot, Vector(LocationX, LocationY, 0.0f), (PlayerAlertType)AlertType);
			}
			else if (IsPlayerOnAlienTeam(pBot->pEdict))
			{
				if (flag == 0)
				{
					//AlienReceiveAlert(pBot, Vector(LocationX, LocationY, 0.0f), (PlayerAlertType)AlertType);
				}
			}
		}

		state = 0;
	}
}

void BotClient_NS_Alert_33(void* p, int bot_index)
{
	static int state = 0;
	static int flag = 0;
	static int AlertType = 0;
	static float LocationX = 0.0f;
	static float LocationY = 0.0f;
	static bool bIsRequest;
	static int NumResearch = 0;
	static int ResearchId = 0;
	static int ResearchProgress = 0;
	static int ResearchCounter = 0;

	if (state == 0)
	{
		flag = *(int*)p;
		bIsRequest = (flag == 0 || flag == 1);
		state++;
	}
	else if (state == 1)
	{
		if (bIsRequest)
		{
			AlertType = *(int*)p;
		}
		else
		{
			NumResearch = *(int*)p;
			ResearchCounter = 0;
		}
		state++;
	}
	else if (state == 2)
	{
		if (bIsRequest)
		{
			LocationX = *(float*)p;
		}
		else
		{
			ResearchId = *(int*)p;
		}
		state++;
	}
	else if (state == 3)
	{
		bot_t* pBot = &bots[bot_index];

		if (bIsRequest)
		{
			LocationY = *(float*)p;

			if (pBot->is_used && !FNullEnt(pBot->pEdict))
			{

				if (IsPlayerCommander(pBot->pEdict))
				{
					CommanderReceiveAlert(pBot, Vector(LocationX, LocationY, 0.0f), (PlayerAlertType)AlertType);
				}
				else if (IsPlayerOnAlienTeam(pBot->pEdict))
				{
					if (flag == 0)
					{
						//AlienReceiveAlert(pBot, Vector(LocationX, LocationY, 0.0f), (PlayerAlertType)AlertType);
					}
				}
			}

			state = 0;


		}
		else
		{
			ResearchProgress = *(int*)p;
			ResearchCounter++;

			if (ResearchCounter >= NumResearch)
			{
				state = 0;
			}
			else
			{
				state = 2;
			}
		}

	}
}

void BotClient_NS_SetSelect(void* p, int bot_index)
{
	static int state = 0;
	static int group_number = 0;
	static int selected_entity_count = 0;
	static int counted_entities = 0;
	static int SelectedEntities[32];
	static int TrackingEntity = 0;
	static int group_type = 0;
	static int group_alert = 0;

	if (state == 0)
	{

		memset(SelectedEntities, 0, sizeof(SelectedEntities));
		group_number = *(int*)p;


		state++;
	}
	else if (state == 1)
	{
		selected_entity_count = *(int*)p;

		if (selected_entity_count == 0)
		{
			bots[bot_index].CommanderCurrentlySelectedBuilding = nullptr;
			state++;
		}

		counted_entities = 0;
		state++;
	}
	else if (state == 2)
	{
		SelectedEntities[counted_entities++] = *(int*)p;

		edict_t* SelectedEntity = INDEXENT(*(int*)p);

		if (!FNullEnt(SelectedEntity) && UTIL_IsMarineStructure(SelectedEntity))
		{
			bots[bot_index].CommanderCurrentlySelectedBuilding = SelectedEntity;
		}
		else
		{
			bots[bot_index].CommanderCurrentlySelectedBuilding = nullptr;
		}

		if (counted_entities >= selected_entity_count)
		{
			state++;
		}
	}
	else if (state == 3)
	{
		switch (group_number)
		{
		case 0:
		{
			TrackingEntity = *(int*)p;

			state = 0;
		}
		break;
		case kSelectAllHotGroup:
		{
			state = 0;
		}
		break;
		default:
		{
			group_type = *(int*)p;
			TrackingEntity = *(int*)p;

			state++;
		}
		break;
		}
	}
	else if (state == 4)
	{
		group_alert = *(int*)p;

		state = 0;
	}

}

void BotClient_Valve_CurrentWeapon(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int iState;
	static int iId;
	static int iClip;

	if (state == 0)
	{
		state++;
		iState = *(int*)p;  // state of the current weapon
	}
	else if (state == 1)
	{
		state++;
		iId = *(int*)p;  // weapon ID of current weapon
	}
	else if (state == 2)
	{
		state = 0;

		iClip = *(int*)p;  // ammo currently in the clip for this weapon

		if (iId <= 31)
		{
			if ((iState & WEAPON_IS_CURRENT))
			{
				bots[bot_index].current_weapon.iId = iId;
				bots[bot_index].current_weapon.iClip = iClip;
				bots[bot_index].current_weapon.MinRefireTime = weapon_defs[iId].MinRefireTime;

				bots[bot_index].m_clipAmmo[iId] = iClip;

				// update the ammo counts for this weapon...
				bots[bot_index].current_weapon.iAmmo1 =
					bots[bot_index].m_rgAmmo[weapon_defs[iId].iAmmo1];
				bots[bot_index].current_weapon.iAmmo1Max =
					weapon_defs[iId].iAmmo1Max;
				bots[bot_index].current_weapon.iAmmo2 =
					bots[bot_index].m_rgAmmo[weapon_defs[iId].iAmmo2];
				bots[bot_index].current_weapon.iAmmo2Max =
					weapon_defs[iId].iAmmo2Max;
				bots[bot_index].current_weapon.iClipMax =
					weapon_defs[iId].iClipSize;
			}
		}

	}
}


// This message is sent whenever ammo ammounts are adjusted (up or down).
void BotClient_Valve_AmmoX(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int index;
	static int amount;
	int ammo_index;

	if (state == 0)
	{
		state++;
		index = *(int*)p;  // ammo index (for type of ammo)
	}
	else if (state == 1)
	{
		state = 0;

		amount = *(int*)p;  // the ammount of ammo currently available

		bots[bot_index].m_rgAmmo[index] = amount;  // store it away

		ammo_index = bots[bot_index].current_weapon.iId;

		// update the ammo counts for this weapon...
		bots[bot_index].current_weapon.iAmmo1 =
			bots[bot_index].m_rgAmmo[weapon_defs[ammo_index].iAmmo1];
		bots[bot_index].current_weapon.iAmmo2 =
			bots[bot_index].m_rgAmmo[weapon_defs[ammo_index].iAmmo2];
		bots[bot_index].current_weapon.iAmmo1Max =
			weapon_defs[ammo_index].iAmmo1Max;
		bots[bot_index].current_weapon.iAmmo2Max =
			weapon_defs[ammo_index].iAmmo2Max;
		bots[bot_index].current_weapon.iClipMax =
			weapon_defs[ammo_index].iClipSize;

	}
}

// This message is sent when the bot picks up some ammo (AmmoX messages are
// also sent so this message is probably not really necessary except it
// allows the HUD to draw pictures of ammo that have been picked up.  The
// bots don't really need pictures since they don't have any eyes anyway.
void BotClient_Valve_AmmoPickup(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int index;
	static int ammount;
	int ammo_index;

	if (state == 0)
	{
		state++;
		index = *(int*)p;
	}
	else if (state == 1)
	{
		state = 0;

		ammount = *(int*)p;

		bots[bot_index].m_rgAmmo[index] = ammount;

		ammo_index = bots[bot_index].current_weapon.iId;

		// update the ammo counts for this weapon...
		bots[bot_index].current_weapon.iAmmo1 =
			bots[bot_index].m_rgAmmo[weapon_defs[ammo_index].iAmmo1];
		bots[bot_index].current_weapon.iAmmo1Max =
			weapon_defs[ammo_index].iAmmo1Max;
		bots[bot_index].current_weapon.iAmmo2 =
			bots[bot_index].m_rgAmmo[weapon_defs[ammo_index].iAmmo2];
		bots[bot_index].current_weapon.iAmmo2Max =
			weapon_defs[ammo_index].iAmmo2Max;
		bots[bot_index].current_weapon.iClipMax =
			weapon_defs[ammo_index].iClipSize;


	}
}


void BotClient_NS_Damage(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int damage_armor;
	static int damage_taken;
	static int damage_bits;  // type of damage being done
	static Vector damage_origin;

	if (state == 0)
	{
		state++;
		damage_armor = *(int*)p;
	}
	else if (state == 1)
	{
		state++;
		damage_taken = *(int*)p;
	}
	else if (state == 2)
	{
		state++;
		damage_bits = *(int*)p;
	}
	else if (state == 3)
	{
		state++;
		damage_origin.x = *(float*)p;
	}
	else if (state == 4)
	{
		state++;
		damage_origin.y = *(float*)p;
	}
	else if (state == 5)
	{
		state = 0;

		damage_origin.z = *(float*)p;

		if (damage_taken > 0)
		{

			edict_t* aggressor = nullptr;
			float MinDistSq = 0.0f;
			float MaxDistSq = sqrf(50.0f);

			for (int i = 0; i < 32; i++)
			{
				if (clients[i] != NULL && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && !IsPlayerCommander(clients[i]))
				{
					float Dist = vDist3DSq(clients[i]->v.origin, damage_origin);

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

			if (aggressor)
			{
				BotTakeDamage(&bots[bot_index], damage_taken, aggressor);
			}
		}
	}
}


// This message gets sent when the bots money ammount changes (for CS)
void BotClient_NS_GameStatus(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int StatusCode = 0;

	if (state == 0)
	{
		StatusCode = *(int*)p;

		if (StatusCode == kGameStatusGameTime)
		{
			bGameIsActive = true;
		}
		else
		{
			bGameIsActive = false;
		}

		state++;
	}
	else if (state == 1)
	{
		if (StatusCode == kGameStatusReset || StatusCode == kGameStatusResetNewMap || StatusCode == kGameStatusEnded)
		{
			state = 0;
		}
		else
		{
			state++;
		}
	}
	else if (state == 2)
	{
		if (StatusCode == kGameStatusUnspentLevels)
		{
			state = 0;
		}
		else
		{
			state++;
		}
	}
	else
	{
		state++;

		if (state > 4)
		{
			state = 0;
		}
	}
}

void BotClient_NS_SetupMap(void* p, int bot_index)
{
	static int state = 0;
	static bool IsLocation;
	static char LocationName[64];
	static float LocationMinX;
	static float LocationMaxX;
	static float LocationMinY;
	static float LocationMaxY;

	if (state == 0)
	{
		IsLocation = *(bool*)p;
		state++;
	}
	else if (state == 1)
	{
		if (IsLocation)
		{
			sprintf(LocationName, "%s", (char*)p);
		}

		state++;
	}
	else if (state == 2)
	{
		if (IsLocation)
		{
			LocationMaxX = *(float*)p;
		}

		state++;
	}
	else if (state == 3)
	{
		if (!IsLocation)
		{
			SetCommanderViewZHeight(*(float*)p);
		}
		else
		{
			LocationMaxY = *(float*)p;
		}

		state++;
	}
	else if (state == 4)
	{
		if (IsLocation)
		{
			LocationMinX = *(float*)p;
		}

		state++;
	}
	else if (state == 5)
	{
		if (IsLocation)
		{
			LocationMinY = *(float*)p;

			AddMapLocation(LocationName, Vector(LocationMinX, LocationMinY, 0.0f), Vector(LocationMaxX, LocationMaxY, 0.0f));

			state = 0;
		}
		else
		{
			state++;
		}
	}
	else if (state == 8)
	{
		state = 0;
	}
	else
	{
		state++;
	}

}


// This message gets sent when the bots get killed
void BotClient_NS_DeathMsg(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static int killer_index;
	static int victim_index;
	static edict_t* killer_edict;
	static edict_t* victim_edict;
	static int index;

	if (state == 0)
	{
		state++;
		killer_index = *(int*)p;  // ENTINDEX() of killer
	}
	else if (state == 1)
	{
		state++;
		victim_index = *(int*)p;  // ENTINDEX() of victim
	}
	else if (state == 2)
	{
		state = 0;

		killer_edict = INDEXENT(killer_index);
		victim_edict = INDEXENT(victim_index);

		// get the bot index of the killer...
		index = GetBotIndex(killer_edict);

		// get the bot index of the victim...
		index = GetBotIndex(victim_edict);

		// is this message about a bot being killed?
		if (index != -1)
		{

			bot_t* botVictim = GetBotPointer(victim_edict);
			bot_t* botKiller = GetBotPointer(killer_edict);

			if (botVictim)
			{
				BotDied(botVictim, killer_edict);
			}

			if (botKiller)
			{
				BotKilledPlayer(botKiller, victim_edict);
			}

		}
	}
}

void BotClient_NS_ReceiveOrder(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static bool isMovementOrder = true;
	static AvHOrderType orderType;
	static Vector moveDestination;
	static int players;
	static edict_t* recipient;
	static edict_t* orderTarget;
	static byte OrderCompleted;
	static byte OrderNotificationType;
	static byte targetType;

	if (state == 0)
	{
		int recipientIndex = *(int*)p;
		recipient = INDEXENT(recipientIndex);
		state++;
	}
	else if (state == 1)
	{
		orderType = *((AvHOrderType*)p);
		isMovementOrder = true;
		state++;
	}
	else if (state == 2)
	{
		if (isMovementOrder)
		{
			moveDestination.x = *(float*)p;
		}
		else
		{
			int entityIndex = *(unsigned short*)p;
			orderTarget = INDEXENT(entityIndex);
		}
		state++;
	}
	else if (state == 3)
	{
		if (isMovementOrder)
		{
			moveDestination.y = *(float*)p;
		}
		else
		{
			targetType = *(byte*)p;
		}
		state++;
	}
	else if (state == 4)
	{
		if (isMovementOrder)
		{
			moveDestination.z = *(float*)p;

		}
		else
		{
			OrderCompleted = *(byte*)p;
		}

		state++;
	}
	else if (state == 5)
	{

		if (isMovementOrder)
		{
			targetType = *(byte*)p;
			state++;
		}
		else
		{
			OrderNotificationType = *(byte*)p;

			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && bots[i].pEdict == recipient)
				{
					if (OrderNotificationType == kOrderStatusActive)
					{

					}
				}
			}

			state = 0;
		}
	}
	else if (state == 6)
	{
		OrderCompleted = *(byte*)p;
		state++;
	}
	else if (state == 7)
	{
		OrderNotificationType = *(byte*)p;
		state = 0;

		for (int i = 0; i < 32; i++)
		{
			if (bots[i].is_used && bots[i].pEdict == recipient)
			{
				if (OrderNotificationType == kOrderStatusActive)
				{
					BotReceiveCommanderOrder(&bots[i], orderType, (AvHUser3)targetType, moveDestination);

				}
			}
		}

	}
}

void BotClient_NS_AlienInfo_32(void* p, int bot_index)
{
	static int state = 0;
	static int Header;
	static bool bHiveInfo;
	static int NumUpgrades;
	static int currUpgrade;
	static int Upgrades[32];
	static int NumHives;
	static int HiveCounter;
	static int HiveStatus;
	static int HiveHealthPercent;
	static int HiveBuildTime;
	static AlienInfo_ChangeFlags Changes;
	static Vector HiveLocation;
	static int CoordsRead;
	static bool bReadHeader;

	if (state == 0)
	{
		bReadHeader = false;
		Header = *(int*)p;
		HiveCounter = 0;
		CoordsRead = 0;

		bHiveInfo = !(Header & 0x80);

		state++;
	}
	else if (state == 1)
	{
		if (!bHiveInfo)
		{

			NumUpgrades = *(int*)p;
			currUpgrade = 0;
			state++;
		}
		else
		{

			CoordsRead = 0;
			NumHives = Header;
			SetNumberofHives(NumHives);
			Changes = *(AlienInfo_ChangeFlags*)p;

			if (NumHives == 0)
			{
				state = 0;
			}
			else
			{
				if (Changes & COORDS_CHANGED)
				{
					state = 2;
				}
				else if (Changes & STATUS_CHANGED)
				{
					state = 3;
				}
				else if (Changes & HEALTH_CHANGED)
				{
					state = 4;
				}
				else
				{
					HiveCounter++;

					if (HiveCounter >= NumHives)
					{
						state = 0;
					}
					else
					{
						state = 1;
					}
				}
			}
		}


	}
	else if (state == 2)
	{
		if (!bHiveInfo)
		{
			Upgrades[currUpgrade++] = *(int*)p;

			if (currUpgrade >= NumUpgrades)
			{
				state = 0;
			}
		}
		else
		{
			if (CoordsRead == 0)
			{
				HiveLocation.x = *(float*)p;
				CoordsRead++;
			}
			else if (CoordsRead == 1)
			{
				HiveLocation.y = *(float*)p;
				CoordsRead++;
			}
			else if (CoordsRead == 2)
			{
				HiveLocation.z = *(float*)p;

				SetHiveLocation(HiveCounter, HiveLocation);

				CoordsRead = 0;
				if (Changes & STATUS_CHANGED)
				{
					state = 3;
				}
				else if (Changes & HEALTH_CHANGED)
				{
					state = 4;
				}
				else
				{
					HiveCounter++;

					if (HiveCounter >= NumHives)
					{
						state = 0;
					}
					else
					{
						state = 1;
					}
				}
			}
		}
	}
	else if (state == 3)
	{
		HiveStatus = *(int*)p;

		int StatusType = (HiveStatus >> 3) & 0x03;

		bool bUnderAttack = (HiveStatus & 0x80) != 0;
		int HivemStatus = HiveStatus & 0x07;

		SetHiveStatus(HiveCounter, HivemStatus);
		SetHiveTechStatus(HiveCounter, StatusType);
		SetHiveUnderAttack(HiveCounter, bUnderAttack);

		if (Changes & HEALTH_CHANGED)
		{
			state = 4;
		}
		else
		{
			HiveCounter++;

			if (HiveCounter >= NumHives)
			{
				state = 0;
			}
			else
			{
				state = 1;
			}
		}

	}
	else if (state == 4)
	{
		HiveHealthPercent = *(int*)p;
		SetHiveHealthPercent(HiveCounter, HiveHealthPercent);
		HiveCounter++;

		if (HiveCounter >= NumHives)
		{
			state = 0;
		}
		else
		{
			state = 1;
		}
	}
}

void BotClient_NS_AlienInfo_33(void* p, int bot_index)
{
	static int state = 0;
	static int Header;
	static bool bHiveInfo;
	static int NumUpgrades;
	static int currUpgrade;
	static int Upgrades[32];
	static int NumHives;
	static int HiveCounter;
	static int HiveStatus;
	static int HiveHealthPercent;
	static int HiveBuildTime;
	static AlienInfo_ChangeFlags Changes;
	static Vector HiveLocation;
	static int CoordsRead;
	static bool bReadHeader;

	if (state == 0)
	{
		bReadHeader = false;
		Header = *(int*)p;
		HiveCounter = 0;
		CoordsRead = 0;

		bHiveInfo = !(Header & 0x80);

		state++;
	}
	else if (state == 1)
	{
		if (!bHiveInfo)
		{

			NumUpgrades = *(int*)p;
			currUpgrade = 0;
			state++;
		}
		else
		{

			CoordsRead = 0;
			NumHives = Header;
			SetNumberofHives(NumHives);
			Changes = *(AlienInfo_ChangeFlags*)p;

			if (NumHives == 0)
			{
				state = 0;
			}
			else
			{
				if (Changes & COORDS_CHANGED)
				{
					state = 2;
				}
				else if (Changes & STATUS_CHANGED)
				{
					state = 3;
				}
				else if (Changes & HEALTH_CHANGED)
				{
					state = 4;
				}
				else
				{
					HiveCounter++;

					if (HiveCounter >= NumHives)
					{
						state = 0;
					}
					else
					{
						state = 1;
					}
				}
			}
		}


	}
	else if (state == 2)
	{
		if (!bHiveInfo)
		{
			Upgrades[currUpgrade++] = *(int*)p;

			if (currUpgrade >= NumUpgrades)
			{
				state = 0;
			}
		}
		else
		{
			if (CoordsRead == 0)
			{
				HiveLocation.x = *(float*)p;
				CoordsRead++;
			}
			else if (CoordsRead == 1)
			{
				HiveLocation.y = *(float*)p;
				CoordsRead++;
			}
			else if (CoordsRead == 2)
			{
				HiveLocation.z = *(float*)p;

				SetHiveLocation(HiveCounter, HiveLocation);

				CoordsRead = 0;
				if (Changes & STATUS_CHANGED)
				{
					state = 3;
				}
				else if (Changes & HEALTH_CHANGED)
				{
					state = 4;
				}
				else
				{
					HiveCounter++;

					if (HiveCounter >= NumHives)
					{
						state = 0;
					}
					else
					{
						state = 1;
					}
				}
			}
		}
	}
	else if (state == 3)
	{
		HiveStatus = *(int*)p;

		int StatusType = (HiveStatus >> 3) & 0x03;

		bool bUnderAttack = (HiveStatus & 0x80) != 0;
		int HivemStatus = HiveStatus & 0x07;

		SetHiveStatus(HiveCounter, HivemStatus);
		SetHiveTechStatus(HiveCounter, StatusType);
		SetHiveUnderAttack(HiveCounter, bUnderAttack);

		if (Changes & HEALTH_CHANGED)
		{
			state = 4;
		}
		else
		{
			HiveCounter++;

			if (HiveCounter >= NumHives)
			{
				state = 0;
			}
			else
			{
				state = 1;
			}
		}

	}
	else if (state == 4)
	{
		HiveHealthPercent = *(int*)p;
		SetHiveHealthPercent(HiveCounter, HiveHealthPercent);
		state = 5;
	}
	else if (state == 5)
	{
		HiveBuildTime = *(int*)p;

		HiveCounter++;

		if (HiveCounter >= NumHives)
		{
			state = 0;
		}
		else
		{
			state = 1;
		}
	}
}
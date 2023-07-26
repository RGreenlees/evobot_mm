//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_client.h
// 
// Contains all network message handling by the bot
//

#pragma once

#ifndef BOT_CLIENT_H
#define BOT_CLIENT_H

#include "NS_Constants.h"
#include "extdll.h"

typedef struct _HIVE_INFO_MSG
{
	int state = 0;
	int Header;
	bool bHiveInfo;
	int NumUpgrades;
	int currUpgrade;
	int Upgrades[32];
	int NumHives;
	int HiveCounter;
	int HiveStatus;
	int HiveHealthPercent;
	int HiveBuildTime;
	AlienInfo_ChangeFlags Changes;
	Vector HiveLocation;
	int CoordsRead;
} hive_info_msg;

typedef struct _ALERT_MSG
{
	int state = 0;
	int flag = 0;
	int AlertType = 0;
	float LocationX = 0.0f;
	float LocationY = 0.0f;
	bool bIsRequest;
	int NumResearch = 0;
	int ResearchId = 0;
	int ResearchProgress = 0;
	int ResearchCounter = 0;
} alert_msg;

typedef struct _SETUP_MAP_MSG
{
	int state = 0;
	bool IsLocation;
	char LocationName[64];
	float LocationMinX;
	float LocationMaxX;
	float LocationMinY;
	float LocationMaxY;
} setup_map_msg;

typedef struct _RECEIVE_ORDER_MSG
{
	int state = 0;   // current state machine state
	bool isMovementOrder = true;
	AvHOrderType orderType;
	Vector moveDestination;
	int players;
	edict_t* recipient;
	edict_t* orderTarget;
	byte OrderCompleted;
	byte OrderNotificationType;
	byte targetType;
} receive_order_msg;

typedef struct _SELECTION_MSG
{
	int state = 0;
	int group_number = 0;
	int selected_entity_count = 0;
	int counted_entities = 0;
	int SelectedEntities[32];
	int TrackingEntity = 0;
	int group_type = 0;
	int group_alert = 0;
} selection_msg;

typedef struct _GAME_STATUS_MSG
{
	int state = 0;   // current state machine state
	int StatusCode = 0;
} game_status_msg;

typedef struct _DAMAGE_MSG
{
	int state = 0;   // current state machine state
	int damage_armor;
	int damage_taken;
	int damage_bits;  // type of damage being done
	Vector damage_origin;
} damage_msg;

typedef struct _DEATH_MSG
{
	int state = 0;   // current state machine state
	int killer_index;
	int victim_index;
	edict_t* killer_edict;
	edict_t* victim_edict;
	int index;
} death_msg;

typedef struct _CURRENT_WEAPON_MSG
{
	int state = 0;   // current state machine state
	int iState;
	int iId;
	int iClip;
} current_weapon_msg;

typedef struct _AMMOX_MSG
{
	int state = 0;   // current state machine state
	int index;
	int amount;
	int ammo_index;
} ammox_msg;

// Resets all the message info for ReceiveOrder network messages
void BotClient_NS_ReceiveOrder_Reset();
// Called when a client receives an order from the commander
void BotClient_NS_ReceiveOrder(void* p, int bot_index);

// Resets all the message info for SetupMap network messages
void BotClient_NS_GameStatus_Reset();
// Called when the game notifies a player of the match beginning (countdown finished) or ending
void BotClient_NS_GameStatus(void* p, int bot_index);

// Resets all the message info for SetupMap network messages
void BotClient_NS_SetupMap_Reset();
// Provides information about the commander's view, and the names of locations on the map
void BotClient_NS_SetupMap(void* p, int bot_index);

// Resets all the message info for SetSelect network messages
void BotClient_NS_SetSelect_Reset();
// Called when the game acknowledges a commander selecting/deselecting a player or structure
void BotClient_NS_SetSelect(void* p, int bot_index);

// Resets all the message info for Alert network messages
void BotClient_NS_Alert_Reset();
// Used to play HUD notifications (see PlayerAlertType enum in NS_Constants.h). NS 3.2 version
void BotClient_NS_Alert_32(void* p, int bot_index);
// Used to play HUD notifications (see PlayerAlertType enum in NS_Constants.h). NS 3.3 version
void BotClient_NS_Alert_33(void* p, int bot_index);

// Resets all the message info for AlienInfo network messages
void BotClient_NS_AlienInfo_Reset();
// Used to communicate the status of hives (built, under construction, what tech is assigned, is it under attack etc). NS 3.2 version.
void BotClient_NS_AlienInfo_32(void* p, int bot_index);
// Used to communicate the status of hives (built, under construction, what tech is assigned, is it under attack etc). NS 3.3 version.
void BotClient_NS_AlienInfo_33(void* p, int bot_index);

// Resets all the message info for Damage network messages
void BotClient_NS_Damage_Reset();
// Notifies player of damage taken so they can play hit reactions on their side (e.g. punch angles). Used so bot can react to being hurt by unseen player
void BotClient_NS_Damage(void* p, int bot_index);

// Resets all the message info for DeathMessage network messages
void BotClient_NS_DeathMessage_Reset();
// Notifies player of kills so it can play a kill feed in the top right. Used by the bot to determine when they've died and what killed them.
void BotClient_NS_DeathMsg(void* p, int bot_index);

// Resets all the message info for CurrentWeapon network messages
void BotClient_Valve_CurrentWeapon_Reset();
// This message is sent when a weapon is selected (either by the bot chosing a weapon or by the server auto assigning the bot a weapon).
void BotClient_Valve_CurrentWeapon(void* p, int bot_index);

// Sent upon connection to the server, lists all the possible weapons in the game
void BotClient_Valve_WeaponList(void* p, int bot_index);

// Resets all the message info for AmmoX network messages
void BotClient_Valve_AmmoX_Reset();
// Updates a player's ammo counts. Used by the bot to track what ammo it has left
void BotClient_Valve_AmmoX(void* p, int bot_index);


#endif
//
// evobot - Neoptolemus' Recast/Detour base GoldSrc bot, based on Botman's HPB bot template
//
// AIClient.h
// 
// Contains all network message handling by the bot
//

#pragma once

#ifndef AIClient_H
#define AIClient_H

#include "extdll.h"

#include <vector>
#include <string>

typedef struct _GAME_STATUS_MSG
{
	int state = 0;   // current state machine state
	int StatusCode = 0;
} game_status_msg;

typedef struct _DAMAGE_MSG
{
	int state = 0;   // current state machine state
	int damage_armor = 0;
	int damage_taken = 0;
	int damage_bits = 0;  // type of damage being done
	Vector damage_origin = Vector(0.0f, 0.0f, 0.0f);
} damage_msg;

typedef struct _DEATH_MSG
{
	int state = 0;   // current state machine state
	int killer_index = 0;
	int victim_index = 0;
	edict_t* killer_edict = nullptr;
	edict_t* victim_edict = nullptr;
	int index = 0;
} death_msg;

typedef struct _CURRENT_WEAPON_MSG
{
	int state = 0;   // current state machine state
	int iState = 0;
	int iId = 0;
	int iClip = 0;
} current_weapon_msg;

typedef struct _AMMOX_MSG
{
	int state = 0;   // current state machine state
	int index = 0;
	int amount = 0;
	int ammo_index = 0;
} ammox_msg;

typedef struct _TEAM_NAMES_MSG
{
	int state = 0;   // current state machine state
	int ListSize = 0;
	int CurrentIndex = 0;
	std::vector<std::string> TeamNames;
} teamnames_msg;

// Resets all the message info for Damage network messages
void BotClient_Valve_Damage_Reset();
// Notifies player of damage taken so they can play hit reactions on their side (e.g. punch angles). Used by bot so they can react to being hurt by a player
void BotClient_Valve_Damage(void* p, int bot_index);

// Resets all the message info for DeathMessage network messages
void BotClient_Valve_DeathMessage_Reset();
// Notifies player of kills so it can play a kill feed in the top right. Used by the bot to determine when they've died and what killed them.
void BotClient_Valve_DeathMsg(void* p, int bot_index);

// Resets all the message info for CurrentWeapon network messages
void BotClient_Valve_CurrentWeapon_Reset();
// This message is sent when a weapon is selected (either by the bot chosing a weapon or by the server auto assigning the bot a weapon).
void BotClient_Valve_CurrentWeapon(void* p, int player_index);

// Sent upon connection to the server, lists all the possible weapons in the game
void BotClient_Valve_WeaponList(void* p, int bot_index);
// This message is sent when a player picks up a weapon (displays the new weapon icon on the HUD). Can be used by the bot to detect when they have picked up a new weapon
void BotClient_Valve_WeapPickup(void* p, int Player_index);

// Resets all the message info for AmmoX network messages
void BotClient_Valve_AmmoX_Reset();
// Updates a player's ammo counts. Used by the bot to track what ammo it has left
void BotClient_Valve_AmmoX(void* p, int player_index);

#endif
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


// Don't think this is used here, it's a hangover from HPB bot but it's not hurting anyone
void BotClient_CS_HLTV(void *p, int bot_index);

// Called when a client receives an order from the commander
void BotClient_NS_ReceiveOrder(void *p, int bot_index);
// Called when the game notifies a player of the match beginning (countdown finished) or ending
void BotClient_NS_GameStatus(void* p, int bot_index);
// Provides information about the commander's view, and the names of locations on the map
void BotClient_NS_SetupMap(void* p, int bot_index);
// Called when the game acknowledges a commander selecting/deselecting a player or structure
void BotClient_NS_SetSelect(void* p, int bot_index);
// Used to play HUD notifications (see PlayerAlertType enum in NS_Constants.h). NS 3.2 version
void BotClient_NS_Alert_32(void* p, int bot_index);
// Used to play HUD notifications (see PlayerAlertType enum in NS_Constants.h). NS 3.3 version
void BotClient_NS_Alert_33(void* p, int bot_index);
// Used to communicate the status of hives (built, under construction, what tech is assigned, is it under attack etc). NS 3.2 version.
void BotClient_NS_AlienInfo_32(void* p, int bot_index);
// Used to communicate the status of hives (built, under construction, what tech is assigned, is it under attack etc). NS 3.3 version.
void BotClient_NS_AlienInfo_33(void* p, int bot_index);

// Notifies player of damage taken so they can play hit reactions on their side (e.g. punch angles). Used so bot can react to being hurt by unseen player
void BotClient_NS_Damage(void* p, int bot_index);
// Notifies player of kills so it can play a kill feed in the top right. Used by the bot to determine when they've died and what killed them.
void BotClient_NS_DeathMsg(void* p, int bot_index);

// This message is sent when a weapon is selected (either by the bot chosing a weapon or by the server auto assigning the bot a weapon).
void BotClient_Valve_CurrentWeapon(void* p, int bot_index);
// Sent upon connection to the server, lists all the possible weapons in the game
void BotClient_Valve_WeaponList(void *p, int bot_index);
// Updates a player's ammo counts. Used by the bot to track what ammo it has left
void BotClient_Valve_AmmoX(void *p, int bot_index);
// Updates a player's ammo counts. Used by the bot to track what ammo it has left
void BotClient_Valve_AmmoPickup(void *p, int bot_index);


#endif
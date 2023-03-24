//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_gorge.h
// 
// Contains gorge-related functions. Needs refactoring into helper function file
//

#pragma once

#ifndef BOT_MARINE_H
#define BOT_MARINE_H

#include "bot_structs.h"

void MarineThink(bot_t* pBot);

void MarineCombatThink(bot_t* pBot);
void MarineHuntEnemy(bot_t* pBot, enemy_status* TrackedEnemy);

// Helper function to pick the best weapon for any given situation and target type.
NSWeapon BotMarineChooseBestWeapon(bot_t* pBot, edict_t* target);
// Sub function for BotMarineChooseBestWeapon if target is a structure to pick the best weapon for attacking it
NSWeapon BotMarineChooseBestWeaponForStructure(bot_t* pBot, edict_t* target);

void MarineGuardLocation(bot_t* pBot, const Vector Location, const float GuardTime);

void MarineCheckWantsAndNeeds(bot_t* pBot);

void MarineProgressCapResNodeTask(bot_t* pBot, bot_task* Task);
void MarineProgressWeldTask(bot_t* pBot, bot_task* Task);

// Checks if the marine's current task is valid
bool UTIL_IsMarineTaskStillValid(bot_t* pBot, bot_task* Task);

bool UTIL_IsMarineCapResNodeTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsWeldTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsAmmoPickupTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsHealthPickupTaskStillValid(bot_t* pBot, bot_task* Task);

#endif
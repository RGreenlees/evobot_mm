//
// evobot - Neoptolemus' Recast/Detour base GoldSrc bot, based on Botman's HPB bot template
//
// bot_tactical.h
// 
// Contains all helper functions for making tactical decisions
//

#pragma once

#ifndef AI_TACTICAL_H
#define AI_TACTICAL_H

#include "AIPlayer.h"
#include "AIConstants.h"

#include <string>

// How frequently to update the global list of built structures (in seconds). 0 = every frame
static const float structure_inventory_refresh_rate = 0.2f;

// How frequently to update the global list of dropped marine items (in seconds). 0 = every frame
static const float item_inventory_refresh_rate = 0.2f;

// Defines a named location on the map. Used by the bot to communicate with humans (e.g. "I want to place hive at X")
typedef struct _MAP_LOCATION
{
	char LocationName[64] = "\0";
	Vector MinLocation = ZERO_VECTOR;
	Vector MaxLocation = ZERO_VECTOR;
} map_location;

void						AITAC_UpdateMapAIData();

// Checks if the nav mesh has been modified and will call AITAC_OnNavMeshModified() if so. Called every frame
void						AITAC_CheckNavMeshModified();

// Called when the nav mesh has been modified in some way (temp obstacle or off-mesh connection added/removed/modified), and is fully refreshed
void AITAC_OnNavMeshModified();

Vector						AITAC_GetRandomHintInLocation(unsigned int NavMeshIndex, const unsigned int HintFlags, const Vector SearchLocation, const float SearchRadius);


edict_t* AITAC_GetClosestPlayerOnTeamWithLOS(const int Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
bool AITAC_AnyPlayerOnTeamHasLOSToLocation(int Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
int AITAC_GetNumPlayersOnTeamWithLOS(int Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);
std::vector<edict_t*> AITAC_GetAllPlayersOnTeamWithLOS(int Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer);

// Clears out the marine and alien buildable structure maps, resource node and hive lists, and the marine item list
void AITAC_ClearMapAIData(bool bInitialMapLoad = false);
void AITAC_InitialiseMapAIData();

int AITAC_GetNumActivePlayersOnTeam(const int Team);
int AITAC_GetNumPlayersOfTeamInArea(const int Team, const Vector SearchLocation, const float SearchRadius, const edict_t* IgnorePlayer);
bool AITAC_AnyPlayersOfTeamInArea(const int Team, const Vector SearchLocation, const float SearchRadius, const edict_t* IgnorePlayer);
std::vector<edict_t*> AITAC_GetAllPlayersOfTeamInArea(const int Team, const Vector SearchLocation, const float SearchRadius, const edict_t* IgnorePlayer);

edict_t* AITAC_GetNearestHumanAtLocation(const int Team, const Vector Location, const float MaxSearchRadius);


int AITAC_GetNumDeadPlayersOnTeam(const int Team);


bool AITAC_AnyPlayerOnTeamWithLOS(int Team, const Vector& Location, float SearchRadius);



#endif
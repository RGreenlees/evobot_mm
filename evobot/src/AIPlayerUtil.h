#pragma once

#ifndef AI_PLAYER_HELPER_H
#define AI_PLAYER_HELPER_H

#include "AIConstants.h"

// How far a bot can be from a useable object when trying to interact with it. Used also for melee attacks
static const float max_player_use_reach = 60.0f;

// Minimum time a bot can wait between attempts to use something in seconds (when not holding the use key down)
static const float min_player_use_interval = 0.5f;

// Max height reachable by crouch-jumping
static const float max_player_normaljump_height = 44.0f;

// Max height reachable by crouch-jumping
static const float max_player_crouchjump_height = 62.0f;

/****************

Player Status Checks

*****************/

// Returns true if the player is actively in the game and not spectating, dead or some other status which means they're not on the field on play
bool IsPlayerActiveInGame(const edict_t* Player);
// Is the player a human?
bool IsPlayerHuman(const edict_t* Player);
// Is the player a bot (includes other bot fake clients)?
bool IsPlayerBot(const edict_t* Player);
// Is the player dead and waiting to respawn?
bool IsPlayerDead(const edict_t* Player);
// Is the player currently spectating?
bool IsPlayerSpectator(const edict_t* Player);
// Is the player currently on a ladder? Always false for Skulks and Lerks as they can't climb ladders
bool IsPlayerOnLadder(const edict_t* Player);
// Is the player currently in the ready room?
bool IsPlayerInReadyRoom(const edict_t* Player);

// Returns the player's max armour, based on armour research levels (marines) or class and carapace level (aliens)
int GetPlayerMaxArmour(const edict_t* Player);

// Can the player duck? Skulks, gorges and lerks cannot
bool CanPlayerCrouch(const edict_t* Player);

float GetPlayerRadius(const edict_t* Player);

// Returns the hull index that should be used for this player when performing hull traces. Depends on if player is crouching right now or not
int GetPlayerHullIndex(const edict_t* Player);

// Returns the hull index that should be used for this player when performing hull traces, can manually specify if it's their crouching hull index or not
int GetPlayerHullIndex(const edict_t* Player, const bool bIsCrouching);

// Expresses the combined health and armour of a player vs max
float GetPlayerOverallHealthPercent(const edict_t* Player);

// Gets the world position of the player's viewpoint (origin + view_ofs)
Vector GetPlayerEyePosition(const edict_t* Player);

// Player's current height based on their player class, can manually specify if you want the crouched height or not
float GetPlayerHeight(const edict_t* Player, const bool bIsCrouching);

// The origin offset from the bottom of the player's collision box, depending on crouching or not
Vector GetPlayerOriginOffsetFromFloor(const edict_t* pEdict, const bool bIsCrouching);
// The bottom-centre of the player's collision box
Vector GetPlayerBottomOfCollisionHull(const edict_t* pEdict);
// The top-centre of the player's collision box based on their player class and if they're currently crouching
Vector GetPlayerTopOfCollisionHull(const edict_t* pEdict);
// The top-centre of the player's collision box based on their player class, manually specifying if they're crouching or not
Vector GetPlayerTopOfCollisionHull(const edict_t* pEdict, const bool bIsCrouching);

// Based on current movement inputs from the player, what direction are they trying to move? Ignores actual velocity.
Vector GetPlayerAttemptedMoveDirection(const edict_t* Player);

// Returns true if the supplied edict is a player (bot or human)
bool IsEdictPlayer(const edict_t* edict);

bool IsPlayerTouchingEntity(const edict_t* Player, const edict_t* TargetEntity);

bool IsPlayerInUseRange(const edict_t* Player, const edict_t* Target);

bool UTIL_PlayerHasLOSToEntity(const edict_t* Player, const edict_t* Target, const float MaxRange, const bool bUseHullSweep);
bool UTIL_PlayerHasLOSToLocation(const edict_t* Player, const Vector Target, const float MaxRange);

bool IsPlayerReloading(const edict_t* Player);

bool IsPlayerStandingOnPlayer(const edict_t* Player);

edict_t* UTIL_GetNearestLadderAtPoint(const Vector SearchLocation);
Vector UTIL_GetNearestLadderNormal(edict_t* pEdict);
Vector UTIL_GetNearestLadderNormal(Vector SearchLocation);
Vector UTIL_GetNearestLadderCentrePoint(edict_t* pEdict);
Vector UTIL_GetNearestLadderCentrePoint(const Vector SearchLocation);
Vector UTIL_GetNearestLadderTopPoint(edict_t* pEdict);
Vector UTIL_GetNearestLadderTopPoint(const Vector SearchLocation);
Vector UTIL_GetNearestLadderBottomPoint(edict_t* pEdict);

Vector UTIL_GetNearestSurfaceNormal(Vector SearchLocation);

float GetPlayerMaxJumpHeight(const edict_t* Player);

#endif
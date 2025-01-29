#pragma once

#ifndef AI_WEAPON_HELPER_H
#define AI_WEAPON_HELPER_H

#include "AIPlayer.h"

typedef enum
{
	WEAPON_ON_TARGET = 0x01,
	WEAPON_IS_CURRENT = 0x02,
	WEAPON_IS_ENABLED = 0x04
} CurWeaponStateFlags;

typedef struct _AI_WEAPON_DEF
{
	char szClassname[64];
	AIWeaponType WeaponType = WEAPON_INVALID;
	int AmmoIndex = 0;
	int MaxClipSize = 0;
	int CurrentClip = 0;
	int MaxAmmoReserve = 0;
	int CurrentAmmoReserve = 0;
	int Slot = 0;
	int SlotPosition = 0;
	float MinRefireTime = 0.0f;
	float LastFireTime = 0.0f;

} AIWeaponTypeDefinition;

typedef struct _AI_PLAYER_INVENTORY
{
	int AmmoCounts[MAX_AMMO_SLOTS]; // How much reserve ammo the player has
	int ClipAmount[MAX_AMMO_SLOTS]; // How much the player has in their current clip
	float RefireTimes[MAX_AMMO_SLOTS]; // Times when each weapon can be refired
	AIWeaponType CurrentWeapon = WEAPON_INVALID; // Index of the currently-held weapon;

} AIPlayerInventory;

AIWeaponType WEAP_GetPlayerCurrentWeapon(const edict_t* Player);
int WEAP_GetPlayerCurrentWeaponClipAmmo(const edict_t* Player);
int WEAP_GetPlayerCurrentWeaponMaxClipAmmo(const edict_t* Player);
int WEAP_GetPlayerCurrentWeaponReserveAmmo(const edict_t* Player);
int WEAP_GetPlayerCurrentWeaponMaxReserveAmmo(const edict_t* Player);
AIWeaponType WEAP_GetPlayerCurrentWeapon(const edict_t* Player);

// Returns the projectile velocity of the weapon (returns 0 if hitscan)
float WEAP_GetProjectileVelocityForWeapon(const AIWeaponType Weapon);

// Returns the max ideal weapon range that the bot should engage an enemy at with the requested weapon
float WEAP_GetMaxIdealWeaponRange(const AIWeaponType Weapon, edict_t* Target = nullptr);
// Returns the minimum ideal weapon range (e.g. for explosive weapons)
float WEAP_GetMinIdealWeaponRange(const AIWeaponType Weapon, edict_t* Target = nullptr);

// Returns true if the requested weapon can be reloaded
bool WEAP_WeaponCanBeReloaded(const AIWeaponType CheckWeapon);
// Returns whether the requested weapon is a melee weapon or not
bool WEAP_IsMeleeWeapon(const AIWeaponType Weapon);

// Will attempt to return a location that, when aimed at by the player, will result in the grenade landing within the explosion radius of the target location
Vector UTIL_GetGrenadeThrowTarget(edict_t* Player, const Vector TargetLocation, const float ExplosionRadius, bool bPrecise);

// Orders the bot to begin reloading their current weapon
void WEAP_BotReloadCurrentWeapon(AIPlayer* pBot);

// Gets the end-to-end reload time for the requested weapon
float WEAP_GetReloadTimeForWeapon(AIWeaponType Weapon);

bool WEAP_CanInterruptWeaponReload(AIWeaponType Weapon);

// Makes the bot interrupt their reload if they are currently reloading
void InterruptReload(AIPlayer* pBot);

bool WEAP_IsHitScanWeapon(AIWeaponType Weapon);

// If projectiles are affected by gravity, e.g. grenades
bool WEAP_IsWeaponAffectedByGravity(AIWeaponType Weapon);

// Returns the time (in seconds) until the player can refire their weapon
float WEAP_GetTimeUntilPlayerNextRefire(const edict_t* Player);

BotAttackResult PerformAttackLOSCheck(AIPlayer* pBot, const AIWeaponType Weapon, const edict_t* Target);
BotAttackResult PerformAttackLOSCheck(AIPlayer* pBot, const AIWeaponType Weapon, const Vector TargetLocation, const edict_t* Target);
BotAttackResult PerformAttackLOSCheck(const Vector Location, const AIWeaponType Weapon, const edict_t* Target);

float WEAP_GetProjectileVelocityForWeapon(const AIWeaponType Weapon);

void UTIL_RegisterNewWeaponType(int WeaponIndex, AIWeaponTypeDefinition* WeaponSettings);
void UTIL_RegisterPlayerNewWeapon(int player_index, int WeaponIndex);
void UTIL_RegisterPlayerCurrentWeapon(int player_index, int WeaponIndex, int ClipAmmo);
void UTIL_UpdatePlayerAmmo(int player_index, int AmmoIndex, int NewAmmo);

const char* UTIL_WeaponTypeToClassname(const AIWeaponType WeaponType);

#endif
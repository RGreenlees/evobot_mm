#pragma once

#ifndef BOT_WEAPONS_H
#define BOT_WEAPONS_H

#include "bot_structs.h"

int BotGetCurrentWeaponClipAmmo(const bot_t* pBot);
int BotGetCurrentWeaponMaxClipAmmo(const bot_t* pBot);
int BotGetCurrentWeaponReserveAmmo(const bot_t* pBot);
NSWeapon GetBotCurrentWeapon(const bot_t* pBot);

NSWeapon GetBotPrimaryWeapon(const bot_t* pBot);

NSWeapon GetBotMarinePrimaryWeapon(const bot_t* pBot);
int BotGetPrimaryWeaponClipAmmo(const bot_t* pBot);
int BotGetPrimaryWeaponMaxClipSize(const bot_t* pBot);
int BotGetPrimaryWeaponAmmoReserve(bot_t* pBot);
int BotGetPrimaryWeaponMaxAmmoReserve(bot_t* pBot);

NSWeapon GetBotMarineSecondaryWeapon(const bot_t* pBot);
int BotGetSecondaryWeaponClipAmmo(const bot_t* pBot);
int BotGetSecondaryWeaponMaxClipSize(const bot_t* pBot);
int BotGetSecondaryWeaponAmmoReserve(bot_t* pBot);
int BotGetSecondaryWeaponMaxAmmoReserve(bot_t* pBot);

NSWeapon GetBotAlienPrimaryWeapon(const bot_t* pBot);

float GetEnergyCostForWeapon(const NSWeapon Weapon);
float UTIL_GetProjectileVelocityForWeapon(const NSWeapon Weapon);

float GetMaxIdealWeaponRange(const NSWeapon Weapon);
float GetMinIdealWeaponRange(const NSWeapon Weapon);

bool WeaponCanBeReloaded(const NSWeapon CheckWeapon);
bool IsMeleeWeapon(const NSWeapon Weapon);

Vector UTIL_GetGrenadeThrowTarget(bot_t* pBot, const Vector TargetLocation, const float ExplosionRadius);

NSWeapon BotMarineChooseBestWeaponForStructure(bot_t* pBot, edict_t* target);
NSWeapon BotAlienChooseBestWeaponForStructure(bot_t* pBot, edict_t* target);

// Helper function to pick the best weapon for any given situation and target type.
NSWeapon BotMarineChooseBestWeapon(bot_t* pBot, edict_t* target);

NSWeapon FadeGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target);
NSWeapon OnosGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target);
NSWeapon SkulkGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target);
NSWeapon GorgeGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target);
NSWeapon LerkGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target);

float GetReloadTimeForWeapon(NSWeapon Weapon);

bool CanInterruptWeaponReload(NSWeapon Weapon);

void InterruptReload(bot_t* pBot);

bool IsHitscanWeapon(NSWeapon Weapon);

#endif
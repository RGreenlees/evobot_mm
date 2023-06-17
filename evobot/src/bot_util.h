//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_util.h
// 
// Contains all generic helper functions for bot-specific behaviour
//

#pragma once

#ifndef BOT_UTIL_H
#define BOT_UTIL_H

#include "bot_structs.h"
#include "NS_Constants.h"

// These define the bot's view frustum sides
#define FRUSTUM_PLANE_TOP 0
#define FRUSTUM_PLANE_BOTTOM 1
#define FRUSTUM_PLANE_LEFT 2
#define FRUSTUM_PLANE_RIGHT 3
#define FRUSTUM_PLANE_NEAR 4
#define FRUSTUM_PLANE_FAR 5

static const float BOT_FOV = 90.0f;  // Bot's field of view;
static const float BOT_MAX_VIEW = 9999.0f; // Bot's maximum view distance;
static const float BOT_MIN_VIEW = 5.0f; // Bot's minimum view distance;
static const float BOT_ASPECT_RATIO = 1.77778f; // Bot's view aspect ratio, 1.333333 for 4:3, 1.777778 for 16:9, 1.6 for 16:10;

static const float f_fnheight = 2.0f * tan((BOT_FOV * 0.0174532925f) * 0.5f) * BOT_MIN_VIEW;
static const float f_fnwidth = f_fnheight * BOT_ASPECT_RATIO;

static const float f_ffheight = 2.0f * tan((BOT_FOV * 0.0174532925f) * 0.5f) * BOT_MAX_VIEW;
static const float f_ffwidth = f_ffheight * BOT_ASPECT_RATIO;

// After attempting to place a structure, how long should the AI wait before retrying if the building hasn't materialised within that time?
static const float build_attempt_retry_time = 0.5f;

// How long a bot will wait before sending another request if the commander isn't listening
static const float min_request_spam_time = 10.0f;

// Max rate bot can run its logic, default is 1/60th second. WARNING: Increasing the rate past 100hz causes bots to move and turn slowly due to GoldSrc limits!
static const double BOT_MIN_FRAME_TIME = (1.0 / 60.0);


void BotLookAt(bot_t* pBot, edict_t* target);
void BotLookAt(bot_t* pBot, const Vector target);
void BotMoveLookAt(bot_t* pBot, const Vector target);
// No view interpolation, but view instantly snaps to target
void BotDirectLookAt(bot_t* pBot, Vector target);

void UTIL_DisplayBotInfo(bot_t* pBot);

enemy_status* UTIL_GetTrackedEnemyRefForTarget(bot_t* pBot, edict_t* Target);

void BotUseObject(bot_t* pBot, edict_t* Target, bool bContinuous);
void BotLeap(bot_t* pBot, const Vector TargetLocation);
bool CanBotLeap(bot_t* pBot);
void BotJump(bot_t* pBot);

// Bot will perform LOS checks and return true if it successfully attacked the target
void BotShootTarget(bot_t* pBot, NSWeapon AttackWeapon, edict_t* Target);
void BotShootLocation(bot_t* pBot, NSWeapon AttackWeapon, const Vector TargetLocation);

void BotAttackTarget(bot_t* pBot, edict_t* Target);

BotAttackResult PerformAttackLOSCheck(bot_t* pBot, const NSWeapon Weapon, const edict_t* Target);
BotAttackResult PerformAttackLOSCheck(const Vector Location, const NSWeapon Weapon, const edict_t* Target);

float GetLeapCost(bot_t* pBot);

void BotSuicide(bot_t* pBot);

// Bot took damage from another bot or player. If no aggressor could be determined then the aggressor is the bot taking the damage
void BotTakeDamage(bot_t* pBot, int damageTaken, edict_t* aggressor);

void BotDied(bot_t* pBot, edict_t* killer);
void BotKilledPlayer(bot_t* pBot, edict_t* victim);

bot_t* GetBotPointer(const edict_t* pEdict);
int GetBotIndex(edict_t* pEdict);

bot_msg* UTIL_GetAvailableBotMsgSlot(bot_t* pBot);

void BotSay(bot_t* pBot, char* textToSay);
void BotSay(bot_t* pBot, float Delay, char* textToSay);
void BotTeamSay(bot_t* pBot, char* textToSay);
void BotTeamSay(bot_t* pBot, float Delay, char* textToSay);

bool UTIL_ShouldBotBeCautious(bot_t* pBot);

void BotReceiveCommanderOrder(bot_t* pBot, AvHOrderType orderType, AvHUser3 TargetType, Vector destination);

void BotDropWeapon(bot_t* pBot);
void BotThrowGrenadeAtTarget(bot_t* pBot, const Vector TargetPoint);
void BotReloadWeapons(bot_t* pBot);

bool IsBotReloading(bot_t* pBot);

void BotEvolveLifeform(bot_t* pBot, NSPlayerClass TargetLifeform);

void UTIL_ClearAllBotData(bot_t* pBot);
void BotUpdateViewFrustum(bot_t* pBot);
void BotUpdateDesiredViewRotation(bot_t* pBot);
void BotUpdateViewRotation(bot_t* pBot, float DeltaTime);
void BotUpdateView(bot_t* pBot);
void BotClearEnemyTrackingInfo(enemy_status* TrackingInfo);

byte BotThrottledMsec(bot_t* pBot);

bool IsPlayerInBotFOV(bot_t* Observer, edict_t* TargetPlayer);
bool DoesBotHaveLOSToPlayer(bot_t* Observer, edict_t* TargetPlayer);
bool IsPlayerVisibleToBot(bot_t* Observer, edict_t* TargetPlayer);

void OnBotBeginGestation(bot_t* pBot);

void StartNewBotFrame(bot_t* pBot);
int BotGetNextEnemyTarget(bot_t* pBot);

void WaitGameStartThink(bot_t* pBot);
void ReadyRoomThink(bot_t* pBot);

void BotThink(bot_t* pBot);

// Called during the regular NS game mode
void RegularModeThink(bot_t* pBot);
// Called during the combat game mode
void CombatModeThink(bot_t* pBot);
// Called if there isn't a valid game mode in play (e.g. user has loaded non-NS map). Bots will randomly roam and attack enemies but nothing else
void InvalidModeThink(bot_t* pBot);

void TestNavThink(bot_t* pBot);
void TestGuardThink(bot_t* pBot);
void TestAimThink(bot_t* pBot);
void DroneThink(bot_t* pBot);
void CustomThink(bot_t* pBot);

// Returns false if the bot is spectating, dead, gestating or being digested
bool ShouldBotThink(const bot_t* bot);
// Called whenever the bot respawns, finishes gestation or is rescued from digestion
void BotRestartPlay(bot_t* pBot);

// Simulates a fake client inputting a console command
void FakeClientCommand(edict_t* pBot, const char* arg1, const char* arg2, const char* arg3);
// Simulates the bot inputting the command to switch to a given weapon
void BotSwitchToWeapon(bot_t* pBot, NSWeapon NewWeaponSlot);

// Test the bot melee system
void DEBUG_BotMeleeTarget(bot_t* pBot, edict_t* Target);

// How many points has the bot spent on stuff in combat mode?
int GetBotSpentCombatPoints(bot_t* pBot);
// How many points does the bot have right now to spend?
int GetBotAvailableCombatPoints(bot_t* pBot);

// Returns the cost in points for the chosen marine combat mode upgrade
int GetMarineCombatUpgradeCost(const CombatModeMarineUpgrade Upgrade);
// Returns the cost in points for the chosen alien combat mode upgrade
int GetAlienCombatUpgradeCost(const CombatModeAlienUpgrade Upgrade);
// What impulse the bot needs to input to "buy" the chosen marine upgrade in combat mode
int GetImpulseForMarineCombatUpgrade(const CombatModeMarineUpgrade Upgrade);
// What impulse the bot needs to input to "buy" the chosen alien upgrade in combat mode
int GetImpulseForAlienCombatUpgrade(const CombatModeAlienUpgrade Upgrade);

// If the observer is spectating a bot, it will return the bot reference (or nullptr if not spectating a bot)
bot_t* UTIL_GetSpectatedBot(const edict_t* Observer);

bool BotHasTaskOfType(bot_t* pBot, BotTaskType TaskType);

#endif
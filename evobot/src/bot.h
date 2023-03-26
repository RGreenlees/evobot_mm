//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot.h
// 
// Contains core bot functionality. Needs refactoring as it's too big right now
//

#pragma once

#ifndef BOT_H
#define BOT_H

#include "bot_math.h"
#include "bot_goals.h"
#include "NS_Constants.h"
#include "bot_structs.h"

// stuff for Win32 vs. Linux builds

#ifdef __linux__
#define Sleep sleep
typedef int BOOL;
#endif

// define a new bit flag for bot identification

#define FL_THIRDPARTYBOT (1 << 27)


// Allows the bot to input a console command
void FakeClientCommand(edict_t *pBot, const char *arg1, const char *arg2, const char *arg3);
// Adds a bot to the specified team number (1 = marines, 2 = aliens)
void AddBotToTeam(const int Team);
// Removes a bot from the specified team number (1 = marines, 2 = aliens)
void RemoveBotFromTeam(const int Team);
// Removes all bots from the game
void RemoveAllBots();



#define RESPAWN_IDLE             1
#define RESPAWN_NEED_TO_RESPAWN  2
#define RESPAWN_IS_RESPAWNING    3


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

// These define the bot's view frustum sides
#define FRUSTUM_PLANE_TOP 0
#define FRUSTUM_PLANE_BOTTOM 1
#define FRUSTUM_PLANE_LEFT 2
#define FRUSTUM_PLANE_RIGHT 3
#define FRUSTUM_PLANE_NEAR 4
#define FRUSTUM_PLANE_FAR 5

#define BOT_FOV 90.0f  // Bot's field of view
#define BOT_MAX_VIEW 9999.0f // Bot's maximum view distance
#define BOT_MIN_VIEW 5.0f // Bot's minimum view distance
#define BOT_ASPECT_RATIO 1.77778f // Bot's view aspect ratio, 1.333333 for 4:3, 1.777778 for 16:9, 1.6 for 16:10

#define MAX_SECONDARY_BOT_TASKS 8

#define MAX_TEAMS 32
#define MAX_TEAMNAME_LENGTH 16

// Bot uses this to detect if it missed a jump and can no longer make it. Prompts a recalculation of the path if the jump it's trying to make is now too high
static const float MAX_JUMP_HEIGHT = 60.0f;

// Max rate bot can run its logic, default is 1/60th second. WARNING: Increasing the rate past 100hz causes bots to move and turn slowly due to GoldSrc limits!
static const float MIN_FRAME_TIME = (1.0f / 60.0f);

// The last time the BotThink function was run for all bots. Depends on MIN_FRAME_TIME
static float last_think_time = 0.0f;

// Every time the commander takes an action (moves view, selects building, places structure etc.), how long to wait before doing next action
static const float commander_action_cooldown = 1.0f;

// How long to give humans a chance to go commander before letting the AI command. Modified by CONFIG_GetCommanderWaitTime()
static const float max_no_commander_wait_time = 10.0f;

// How often should the commander nag you to do as you're told if you're not listening, in seconds
static const float min_order_reminder_time = 20.0f;

// How close in metres (see UTIL_MetresToGoldSrcUnits function) a marine should be before confirming move order complete
static const float move_order_success_dist_metres = 2.0f;

// After attempting to place a structure, how long should the AI wait before retrying if the building hasn't materialised within that time?
static const float build_attempt_retry_time = 0.5f;

// How many infantry portals should the AI make? TODO: This should be dynamic based on number of players on marine team
static const int min_infantry_portals = 2;
static const int min_armoury_in_base = 1;
static const int min_desired_arms_labs = 1;
// What is the minimum acceptable resource towers for a team? Bots will prioritise building them if below this number
static const int min_desired_resource_towers = 3;

// How far a bot can be from a useable object when trying to interact with it. Used also for melee attacks
static const float max_player_use_reach = 50.0f;

// Minimum time a bot can wait between attempts to use something in seconds (when not holding the use key down)
static const float min_use_gap = 0.5f;

// How long a bot will wait before sending another request if the commander isn't listening
static const float min_request_spam_time = 10.0f;

// new UTIL.CPP functions...
edict_t* UTIL_FindEntityInSphere( edict_t *pentStart, const Vector &vecCenter, float flRadius );
edict_t* UTIL_FindEntityByString( edict_t *pentStart, const char *szKeyword, const char *szValue );
edict_t* UTIL_FindEntityByClassname( edict_t *pentStart, const char *szName );
edict_t* UTIL_FindEntityByTarget( edict_t *pentStart, const char *szName );
edict_t* UTIL_FindEntityByTargetname( edict_t *pentStart, const char *szName );

// Returns the actual centre of the entity's collision box, which isn't necessarily its origin
Vector UTIL_GetCentreOfEntity(const edict_t* Entity);

// Is the player close enough to use an entity, and does it have LOS to the target?
bool UTIL_PlayerInUseRange(const edict_t* Player, const edict_t* Target);

// How high off the ground the edict's origin is
Vector UTIL_OriginOffsetFromFloor(const edict_t* pEdict, const bool bIsCrouching);
// The bottom of the entity's collision
Vector UTIL_GetBottomOfCollisionHull(const edict_t* pEdict);
// The top of the entity's collision, depending on whether it's crouching or not
Vector UTIL_GetTopOfCollisionHull(const edict_t* pEdict);
// The bottom of the entity's collision, ignores edict's actual crouch status in favour of bIsCrouching
Vector UTIL_GetTopOfCollisionHull(const edict_t* pEdict, const bool bIsCrouching);
// Returns true if the trace completed WITHOUT hitting anything
bool UTIL_QuickTrace(const edict_t* pEdict, const Vector &start, const Vector &end);
// Returns true if the hull trace completed WITHOUT hitting anything, hull number automatically determined based on pEdict
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end);
// Returns true if the hull trace completed WITHOUT hitting anything, using custom hull number
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum);

// Does a trace to see if the player has direct LOS to the target (will be blocked by monsters)
bool UTIL_PlayerHasLOSToEntity(const edict_t* Player, const edict_t* Target, const float MaxRange, const bool bUseHullSweep);
// Does a trace to see if the player has direct LOS to the location (will be blocked by monsters)
bool UTIL_PlayerHasLOSToLocation(const edict_t* Player, const Vector Target, const float MaxRange);

// Returns the player's eye position (origin + view_ofs)
Vector UTIL_GetPlayerEyePosition(const edict_t* Player);

// Draws a white line between start and end for the given player (pEntity) for 0.1s
void UTIL_DrawLine(const edict_t * pEntity, Vector start, Vector end);
// Draws a white line between start and end for the given player (pEntity) for given number of seconds
void UTIL_DrawLine(const edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds);
// Draws a coloured line using RGB input, between start and end for the given player (pEntity) for 0.1s
void UTIL_DrawLine(const edict_t * pEntity, Vector start, Vector end, int r, int g, int b);
// Draws a coloured line using RGB input, between start and end for the given player (pEntity) for given number of seconds
void UTIL_DrawLine(const edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds, int r, int g, int b);

// Makes the bot kill itself (issue "kill" console command)
void UTIL_BotSuicide(bot_t* pBot);

// Event fired when the match starts after the count down (if cheats are enabled, this will still only fire after the countdown even though players can move before then)
void UTIL_OnGameStart();

// Helper function to cycle through a bot's tracked enemy list (see enemy_status struct) and pick the most appropriate target, or -1 if no target exists
int UTIL_GetNextEnemyTarget(bot_t* pBot);

// Converts a weapon type to the droppable item it corresponds to (e.g. WEAPON_MARINE_HMG to ITEM_MARINE_HMG)
NSDeployableItem UTIL_WeaponTypeToDeployableItem(const NSWeapon WeaponType);

// Finds the next empty message slot (see bot_msg struct) for the bot to queue a new message
bot_msg* UTIL_GetAvailableBotMsgSlot(bot_t* pBot);

// Total number of humans and bots on given team
int UTIL_GetNumPlayersOnTeam(const int Team);

// Does this alien player have an upgrade of the given type? (Defence, Sensory, Movement)
bool UTIL_PlayerHasAlienUpgradeOfType(const edict_t* Player, const HiveTechStatus TechType);

void ClientPrint( edict_t *pEdict, int msg_dest, const char *msg_name);
void UTIL_SayText( const char *pText, edict_t *pEdict );
void UTIL_HostSay( edict_t *pEntity, int teamonly, char *message );
int UTIL_GetClass(edict_t *pEntity);
int UTIL_GetBotIndex(edict_t *pEdict);
bot_t* UTIL_GetBotPointer(edict_t *pEdict);

// Does the bot have this weapon in their inventory?
bool BotHasWeapon(const bot_t* pBot, const NSWeapon DesiredWeapon);
// Does this player (bot or human) have this weapon in their inventory?
bool PlayerHasWeapon(const edict_t* Player, const NSWeapon DesiredWeapon);

// Will make the bot look at the target, and if it is close enough and has LOS it will use it. If bContinuous is false, it will use every 0.5s
void BotUseObject(bot_t* pBot, edict_t* Target, bool bContinuous);
// Will make the bot jump, ensuring a 0.5s gap between jumps to avoid loss of momentum
void BotJump(bot_t* pBot);
// If the bot can leap or blink, will handle the inputs required to make it happen. If it can't leap, it will jump instead.
void BotLeap(bot_t* pBot, const Vector TargetLocation);

// Returns true if the bot is a skulk and has leap available, or is a fade and has blink available
bool CanBotLeap(bot_t* pBot);

// Returns the energy cost of leap or blink depending on whether the bot is a skulk or fade
float GetLeapCost(bot_t* pBot);

// How fast does the player (if alien) regenerate energy. Takes the adrenaline upgrade level into account it it has it
float GetPlayerEnergyRegenPerSecond(edict_t* Player);

int GetPlayerIndex(const edict_t* Edict);

NSPlayerClass UTIL_GetPlayerClass(const edict_t* Player);

/*	Runs logic to determine if the input task is still valid.
	For example, a build task is no longer valid if the structure is already fully built.
	An attack task is no longer valid if the target is dead, and so on.
*/
bool UTIL_IsTaskStillValid(bot_t* pBot, bot_task* Task);


// Checks if the aliens's current task is valid
bool UTIL_IsAlienTaskStillValid(bot_t* pBot, bot_task* Task);


bool UTIL_IsTaskCompleted(bot_t* pBot, bot_task* Task);
bool UTIL_IsMoveTaskStillValid(bot_t* pBot, bot_task* Task);

bool UTIL_IsAlienGetHealthTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsAlienHealTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsEquipmentPickupTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsWeaponPickupTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsGuardTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsAttackTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsResupplyTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsBuildTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsDefendTaskStillValid(bot_t* pBot, bot_task* Task);

bool UTIL_IsAlienEvolveTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsAlienBuildTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsAlienCapResNodeTaskStillValid(bot_t* pBot, bot_task* Task);

bool UTIL_IsTaskUrgent(bot_t* pBot, bot_task* Task);
bool UTIL_IsMoveTaskUrgent(bot_t* pBot, bot_task* Task);
bool UTIL_IsBuildTaskUrgent(bot_t* pBot, bot_task* Task);
bool UTIL_IsGuardTaskUrgent(bot_t* pBot, bot_task* Task);

void BotProgressTakeCommandTask(bot_t* pBot);
void BotProgressTask(bot_t* pBot, bot_task* Task);
void BotProgressMoveTask(bot_t* pBot, bot_task* Task);
void BotProgressPickupTask(bot_t* pBot, bot_task* Task);
void BotProgressResupplyTask(bot_t* pBot, bot_task* Task);
void BotProgressGuardTask(bot_t* pBot, bot_task* Task);
void BotProgressAttackTask(bot_t* pBot, bot_task* Task);
void BotProgressBuildTask(bot_t* pBot, bot_task* Task);
void BotProgressDefendTask(bot_t* pBot, bot_task* Task);

void BotProgressGrenadeTask(bot_t* pBot, bot_task* Task);

void BotThrowGrenadeAtTarget(bot_t* pBot, const Vector TargetPoint);

void MarineProgressTask(bot_t* pBot, bot_task* Task);
void AlienProgressTask(bot_t* pBot, bot_task* Task);

void AlienProgressGetHealthTask(bot_t* pBot, bot_task* Task);
void AlienProgressHealTask(bot_t* pBot, bot_task* Task);
void AlienProgressBuildTask(bot_t* pBot, bot_task* Task);
void AlienProgressEvolveTask(bot_t* pBot, bot_task* Task);

void UpdateAndClearTasks(bot_t* pBot);

bool BotHasWantsAndNeeds(bot_t* pBot);
void BotMarineSetPrimaryTask(bot_t* pBot, bot_task* Task);

void BotAlienSetPrimaryTask(bot_t* pBot, bot_task* Task);
void BotAlienSetSecondaryTask(bot_t* pBot, bot_task* Task);
void BotAlienCheckPriorityTargets(bot_t* pBot, bot_task* Task);
void BotAlienCheckDefendTargets(bot_t* pBot, bot_task* Task);

void BotMarineSetSecondaryTask(bot_t* pBot, bot_task* Task);

void AlienHarasserSetPrimaryTask(bot_t* pBot, bot_task* Task);
void AlienCapperSetPrimaryTask(bot_t* pBot, bot_task* Task);
void AlienBuilderSetPrimaryTask(bot_t* pBot, bot_task* Task);

NSStructureType UTIL_GetChamberTypeForHiveTech(const HiveTechStatus HiveTech);

void AlienHarasserSetSecondaryTask(bot_t* pBot, bot_task* Task);
void AlienCapperSetSecondaryTask(bot_t* pBot, bot_task* Task);
void AlienBuilderSetSecondaryTask(bot_t* pBot, bot_task* Task);

bool UTIL_BotWithBuildTaskExists(NSStructureType StructureType);



void BotNotifyStructureDestroyed(bot_t* pBot, const NSStructureType Structure, const Vector Location);

bot_task* BotGetNextTask(bot_t* pBot);

void BotOnCompletePrimaryTask(bot_t* pBot, bot_task* Task);



NSWeapon UTIL_GetBotMarinePrimaryWeapon(const bot_t* pBot);
NSWeapon UTIL_GetBotMarineSecondaryWeapon(const bot_t* pBot);
NSWeapon UTIL_GetBotCurrentWeapon(const bot_t* pBot);

NSWeapon UTIL_GetBotAlienPrimaryWeapon(const bot_t* pBot);

Vector UTIL_GetTraceHitLocation(const Vector Start, const Vector End);

int BotGetPrimaryWeaponAmmoReserve(bot_t* pBot);
int BotGetPrimaryWeaponMaxAmmoReserve(bot_t* pBot);
int BotGetPrimaryWeaponMaxClipSize(const bot_t* pBot);
int BotGetPrimaryWeaponClipAmmo(const bot_t* pBot);
int BotGetGrenadeCount(const bot_t* pBot);
int BotHasGrenades(const bot_t* pBot);





int BotGetSecondaryWeaponAmmoReserve(bot_t* pBot);
int BotGetSecondaryWeaponMaxAmmoReserve(bot_t* pBot);
int BotGetSecondaryWeaponMaxClipSize(const bot_t* pBot);
int BotGetSecondaryWeaponClipAmmo(const bot_t* pBot);

float UTIL_GetPlayerHealth(const edict_t* Player);
float UTIL_GetPlayerHeight(const edict_t* Player, const bool bIsCrouching);

bool UTIL_PlayerHasHeavyArmour(const edict_t* Player);
bool UTIL_PlayerHasJetpack(edict_t* Player);
bool UTIL_PlayerHasEquipment(edict_t* Player);
bool UTIL_PlayerHasWeapon(edict_t* Player, NSWeapon WeaponType);

// Not currently used, left-over from HPB bot
bool UpdateSounds(edict_t *pEdict, edict_t *pPlayer);
void UTIL_ShowMenu( edict_t *pEdict, int slots, int displaytime, bool needmore, char *pText );

// Retrieves the current game directory (i.e. mod directory) and appends the args, separated with '/'. Returned string does NOT end with '/'
void UTIL_BuildFileName(char *filename, const char *arg1, const char *arg2, const char *arg3, const char *arg4);

int UTIL_GetClientIndexByEdict(const edict_t* PlayerEdict);

void UTIL_ClearGuardInfo(bot_t* pBot);

// Returns how much ammo is left in the clip for the bot's currently-held weapon
int BotGetCurrentWeaponClipAmmo(const bot_t* pBot);
// Returns the max ammo in a clip for the bot's currently-held weapon
int BotGetCurrentWeaponMaxClipAmmo(const bot_t* pBot);
// Returns the ammo reserve for the bot's currently-held weapon
int BotGetCurrentWeaponReserveAmmo(const bot_t* pBot);

// Returns true if the input edict is a valid structure, and is in the process of recycling
bool UTIL_StructureIsRecycling(const edict_t* Structure);
// Returns true if the input edict is a valid structure, and is in the process of being upgraded (not yet completed)
bool UTIL_StructureIsUpgrading(const edict_t* Structure);

// Event called when an alien bot receives an alert (e.g. "Hive is under attack", "Life forms under attack" etc.)
void AlienReceiveAlert(bot_t* pBot, const Vector Location, const PlayerAlertType AlertType);

// Returns true if the input edict is a valid structure, and is in the process of any kind of research (not yet completed)
bool UTIL_StructureIsResearching(const edict_t* Structure);
// Returns true if the input edict is a valid structure, and is in the process of researching the specified research (not yet completed)
bool UTIL_StructureIsResearching(const edict_t* Structure, const NSResearch Research);

// First free action slot for the commander, for the given priority. -1 if none found
int UTIL_CommanderFirstFreeActionIndex(bot_t* CommanderBot, int Priority);

// Finds the nearest allied player to the given location for the specified team
float UTIL_DistToNearestFriendlyPlayer(const Vector& Location, int DesiredTeam);

// Is the input edict a valid structure, and is it electrified?
bool UTIL_IsStructureElectrified(const edict_t* Structure);

// Takes into account types like ANYARMOURY to match regular and advanced armouries etc
bool UTIL_StructureTypesMatch(const NSStructureType TypeOne, const NSStructureType TypeTwo);

char* UTIL_WeaponTypeToClassname(const NSWeapon WeaponType);
char* UTIL_TaskTypeToChar(const BotTaskType TaskType);
char* UTIL_BotRoleToChar(const BotRole Role);
char* UTIL_CommanderActionToChar(const CommanderActionType ActionType);

// Returns the cost in resources of the supplied type of structure
int UTIL_GetCostOfStructureType(NSStructureType StructureType);

float UTIL_GetDesiredDistanceToUseEntity(const bot_t* pBot, const edict_t* Entity);

// The bot is in the process of rotating its view to an intended angle (see BotUpdateDesiredViewRotation()), continue the interpolation until completion
void BotUpdateViewRotation(bot_t* pBot, float DeltaTime);
// If the bot has completed its current view interpolation (see BotUpdateViewRotation()) then decide where it wants to angle its view next
void BotUpdateDesiredViewRotation(bot_t* pBot);

// Resets all the bot inputs and updates the status ready for a new iteration of BotThink
void StartNewBotFrame(bot_t* pBot);

void UTIL_ClearBotTask(bot_t* pBot, bot_task* Task);

// If a marine, bot will play "I need a medpack" voiceline. Will be pinged to the commander as "soldier needs health". Limited by min_request_spam_time.
void BotRequestHealth(bot_t* pBot);
// If a marine, bot will play "I need ammo" voiceline. Will be pinged to the commander as "soldier needs ammo". Limited by min_request_spam_time.
void BotRequestAmmo(bot_t* pBot);
// If a marine, bot will play "where are those orders?" voiceline. Will be pinged to the commander as "soldier needs order". Limited by min_request_spam_time.
void BotRequestOrder(bot_t* pBot);

/****************

Player Status Checks

*****************/

// Is the player currently a Skulk?
bool IsPlayerSkulk(const edict_t* Player);
// Is the player currently a Gorge?
bool IsPlayerGorge(const edict_t* Player);
// Is the player currently a Lerk?
bool IsPlayerLerk(const edict_t* Player);
// Is the player currently a Fade?
bool IsPlayerFade(const edict_t* Player);
// Is the player currently an Onos?
bool IsPlayerOnos(const edict_t* Player);
// Is the player currently a Marine (not commander)? Includes both light and heavy marines
bool IsPlayerMarine(const edict_t* Player);
// Is the player the commander?
bool IsPlayerCommander(const edict_t* Player);
// Is the player currently climbing a wall?
bool IsPlayerClimbingWall(const edict_t* Player);
// Is the player in the ready room (i.e. not in the map proper)?
bool IsPlayerInReadyRoom(const edict_t* Player);
// Is the player a human?
bool IsPlayerHuman(const edict_t* Player);
// Is the player a bot (includes non-EvoBot fake clients)?
bool IsPlayerBot(const edict_t* Player);
// Is the player dead and waiting to respawn?
bool IsPlayerDead(const edict_t* Player);
// Is player stunned by Onos stomp?
bool IsPlayerStunned(const edict_t* Player);
// Is the player currently spectating?
bool IsPlayerSpectator(const edict_t* Player);
// Is the player currently being digested by an Onos?
bool IsPlayerBeingDigested(const edict_t* Player);
// Is the player an Onos and currently digesting someone?
bool IsPlayerDigesting(const edict_t* Player);
// Is the player currently gestating?
bool IsPlayerGestating(const edict_t* Player);
// Is the player on the marine team (i.e. not spectating or in the ready room)
bool IsPlayerOnMarineTeam(const edict_t* Player);
// Is the player on the alien team (i.e. not spectating or in the ready room)
bool IsPlayerOnAlienTeam(const edict_t* Player);
// Is the player affected by parasite?
bool IsPlayerParasited(const edict_t* Player);
// Is the player being marked through walls to enemies through being sighted by an ally or affected by motion tracking?
bool IsPlayerMotionTracked(const edict_t* Player);
// Is the player currently on a ladder? Always false for Skulks and Lerks as they can't climb ladders
bool IsPlayerOnLadder(const edict_t* Player);
// Is the player an onos under the effect of charge?
bool IsPlayerCharging(const edict_t* Player);



// Returns the player's max armour, based on armour research levels (marines) or class and carapace level (aliens)
int GetPlayerMaxArmour(const edict_t* Player);

float GetPlayerEnergyPercentage(const edict_t* Player);

float GetEnergyCostForWeapon(const NSWeapon Weapon);

// Called when a bot resumes play after respawning or being rescued from digestion
void OnBotRestartPlay(bot_t* pBot);

// Returns the number of resources a player has. For marines, will return the team resources.
int GetPlayerResources(const edict_t* Player);



// Bot should not think if its dead, spectating, gestating or being digested
bool ShouldBotThink(const bot_t* bot);

enemy_status* UTIL_GetTrackedEnemyRefForTarget(bot_t* pBot, edict_t* Target);

void BotEvolveLifeform(bot_t* pBot, NSPlayerClass TargetLifeform);
bool DoesBotHaveTraitCategory(bot_t* pBot, AlienTraitCategory TraitCategory);


void AlienThink(bot_t* pBot);



void SkulkCombatThink(bot_t* pBot);
void FadeCombatThink(bot_t* pBot);

NSWeapon SkulkGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target);
NSWeapon FadeGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target);
NSWeapon OnosGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target);

void OnosCombatThink(bot_t* pBot);

int UTIL_GetDesiredAlienUpgrade(const bot_t* pBot, const HiveTechStatus TechType);

float UTIL_GetMaxIdealWeaponRange(const NSWeapon Weapon);
float UTIL_GetMinIdealWeaponRange(const NSWeapon Weapon);

// Returns true if the given weapon is a melee one
bool UTIL_IsMeleeWeapon(const NSWeapon Weapon);

Vector UTIL_GetGrenadeThrowTarget(bot_t* pBot, const Vector TargetLocation, const float ExplosionRadius);

void AlienGuardLocation(bot_t* pBot, const Vector Location);

void AlienCheckWantsAndNeeds(bot_t* pBot);

void OnBotFinishGuardingLocation(bot_t* pBot);

void WaitGameStartThink(bot_t* pBot);
void ReadyRoomThink(bot_t* pBot);


/*	This function fixes the erratic behaviour caused by the use of the GET_GAME_DIR engine
	macro, which returns either an absolute directory path, or a relative one, depending on
	whether the game server is run standalone or not. This one always return a RELATIVE path.
*/
void GetGameDir (char *game_dir);

void MoveLookAt(bot_t* pBot, Vector target);
void LookAt(bot_t* pBot, Vector target);
void LookAt(bot_t* pBot, edict_t* target);

// Will aim at the target and attack if target is in range and the bot is aiming at it correctly. Does not handle movement
void BotAttackTarget(bot_t* pBot, edict_t* Target);

// Returns true if the weapon uses clips, NOT if the weapon actually needs to be reloaded right now
bool UTIL_WeaponNeedsReloading(const NSWeapon CheckWeapon);

void DEBUG_TestBackwardsPathFind(edict_t* pEdict, const Vector Destination);

// Updates what entities can be seen and not seen by the bot
void UpdateView(bot_t* pBot);
// Recalculates the bot's view frustum. Used by UpdateView() to start checking what is in and out of the bot's field of view
void UpdateViewFrustum(bot_t* pBot);

NSStructureType UTIL_IUSER3ToStructureType(const int inIUSER3);

// Confirms that the target is within the observer's view frustum, and has LOS
bool IsPlayerVisibleToBot(bot_t* Observer, edict_t* TargetPlayer);

// Called when a bot dies so it clears all its movement data and knows who was responsible
void BotDied(bot_t* pBot, edict_t* killer);
// Called when a bot scores a kill. Doesn't do anything yet.
void BotKilledPlayer(bot_t* pBot, edict_t* victim);
// Used so the bot can react to being hurt by an enemy that's out of sight. Without this, they'll let you kill them unless they directly see you
void BotTakeDamage(bot_t* pBot, int damageTaken, edict_t* aggressor);
void BotSeePlayer(bot_t* pBot, edict_t* seen);
void BotSay(bot_t* pBot, char* textToSay);
void BotSay(bot_t* pBot, float Delay, char* textToSay);
void BotTeamSay(bot_t* pBot, char* textToSay);
void BotTeamSay(bot_t* pBot, float Delay, char* textToSay);

void BotDropWeapon(bot_t* pBot);

void UTIL_ClearAllBotData(bot_t* pBot);

void BotReceiveCommanderOrder(bot_t* pBot, AvHOrderType orderType, AvHUser3 TargetType, Vector destination);
void BotReceiveMoveToOrder(bot_t* pBot, Vector destination);
void BotReceiveBuildOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination);
void BotReceiveAttackOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination);
void BotReceiveGuardOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination);
void BotReceiveWeldOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination);

// Returns the collision hull index used by the player, based on their class and whether they're crouching or not
int GetPlayerHullIndex(const edict_t* pEdict);

// Returns the player's radius in units, based on their current collision hull
float UTIL_GetPlayerRadius(const edict_t* pEdict);

bool UTIL_StructureIsHive(const edict_t* StructureEdict);

int UTIL_StructureTypeToImpulseCommand(const NSStructureType StructureType);

bool UTIL_StructureExistsOfType(const NSStructureType StructureType);

bool UTIL_IsBotCommanderAssigned();
bool UTIL_IsThereACommander();

bool UTIL_IsAnyHumanOnMarineTeam();
bool UTIL_IsAnyHumanOnAlienTeam();

void UTIL_GenerateGuardWatchPoints(bot_t* pBot, const Vector& GuardLocation);

void AssignCommander();
void AssignGuardBot();

bool UTIL_BotCanReload(bot_t* pBot);

NSStructureType UTIL_GetStructureTypeFromEdict(const edict_t* StructureEdict);

int UTIL_GetBotsWithRoleType(BotRole RoleType, bool bMarines);

void BotSwitchToWeapon(bot_t* pBot, NSWeapon NewWeaponSlot);

const char* UTIL_PlayerClassToChar(const NSPlayerClass PlayerClass);
const char* UTIL_StructTypeToChar(const NSStructureType StructureType);
const char* UTIL_ResearchTypeToChar(const NSResearch ResearchType);
const char* UTIL_DroppableItemTypeToChar(const NSDeployableItem ItemType);
const char* UTIL_HiveTechToChar(const HiveTechStatus HiveTech);

void OnBotChangeClass(bot_t* pBot);

bool UTIL_IsEdictPlayer(const edict_t* edict);

HiveTechStatus UTIL_GetTechForChamber(NSStructureType ChamberToConstruct);

void ReceiveStructureAttackAlert(bot_t* pBot, const Vector& AttackLocation);
void ReceiveHiveAttackAlert(bot_t* pBot, edict_t* HiveEdict);

AvHUpgradeMask UTIL_GetResearchMask(const NSResearch Research);

void AlienProgressCapResNodeTask(bot_t* pBot, bot_task* Task);


#endif // BOT_H


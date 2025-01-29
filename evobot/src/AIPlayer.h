#ifndef AI_PLAYER_H
#define AI_PLAYER_H

#include "AIConstants.h"

// These define the bot's view frustum sides
#define FRUSTUM_PLANE_TOP 0
#define FRUSTUM_PLANE_BOTTOM 1
#define FRUSTUM_PLANE_LEFT 2
#define FRUSTUM_PLANE_RIGHT 3
#define FRUSTUM_PLANE_NEAR 4
#define FRUSTUM_PLANE_FAR 5

static const float BOT_FOV = 100.0f;  // Bot's field of view;
static const float BOT_MAX_VIEW = 9999.0f; // Bot's maximum view distance;
static const float BOT_MIN_VIEW = 5.0f; // Bot's minimum view distance;
static const float BOT_ASPECT_RATIO = 1.77778f; // Bot's view aspect ratio, 1.333333 for 4:3, 1.777778 for 16:9, 1.6 for 16:10;

static const float f_fnheight = 2.0f * tan((BOT_FOV * 0.0174532925f) * 0.5f) * BOT_MIN_VIEW;
static const float f_fnwidth = f_fnheight * BOT_ASPECT_RATIO;

static const float f_ffheight = 2.0f * tan((BOT_FOV * 0.0174532925f) * 0.5f) * BOT_MAX_VIEW;
static const float f_ffwidth = f_ffheight * BOT_ASPECT_RATIO;

// Bot will jump. Will duck-jump if bDuckJump is set to true (true by default)
void BotJump(AIPlayer* pBot, bool bDuckJump = true);
// Bot will suicide (equivalent of "kill" in the console)
void BotSuicide(AIPlayer* pBot);
// Tells the bot to look at the target location. Overridden by BotMoveLookAt if set
void BotLookAt(AIPlayer* pBot, Vector NewLocation);
// Tells the bot to look at the centre of the target entity. Is overridden by BotMoveLookAt if set
void BotLookAt(AIPlayer* pBot, edict_t* target);
// Informs the bot that it must look at this spot to complete its current move (e.g. look at top of ladder to climb). Overrides BotLookAt
void BotMoveLookAt(AIPlayer* pBot, const Vector target);
// Instantly snaps the bot's view to the target position
void BotDirectLookAt(AIPlayer* pBot, Vector target);

// Bot will attempt to look at the object and use it. If bContinuous is set, the bot will hold the use key, otherwise they will spam every 0.5s as requested
bool BotUseObject(AIPlayer* pBot, edict_t* Target, bool bContinuous);

// Make the bot type something in either global or team chat
void BotSay(AIPlayer* pBot, bool bTeamSay, float Delay, char* textToSay);

// Helper function so the bot can find an empty message slot in its list of pending messages to print out
AIChatMessage* GetAvailableBotMsgSlot(AIPlayer* pBot);

// Bot will attempt to drop the currently-held weapon
void BotDropWeapon(AIPlayer* pBot);

void BotShootTarget(AIPlayer* pBot, AIWeaponType AttackWeapon, edict_t* Target);
void BotShootLocation(AIPlayer* pBot, AIWeaponType AttackWeapon, const Vector TargetLocation);

void BotUpdateDesiredViewRotation(AIPlayer* pBot);
void BotUpdateViewRotation(AIPlayer* pBot, float DeltaTime);
void BotUpdateView(AIPlayer* pBot);
bool IsEntityInBotFOV(AIPlayer* Observer, const edict_t* Entity);
bool IsEntityVisibleToBot(AIPlayer* pBot, const edict_t* Entity);
void UpdateAIPlayerViewFrustum(AIPlayer* pBot);

// Find a point on the target player that is visible from the view of the observer. Checks head, centre and feet for visibility
Vector GetVisiblePointOnPlayerFromObserver(edict_t* Observer, edict_t* TargetPlayer);

void UpdateBotChat(AIPlayer* pBot);

void ClearBotInputs(AIPlayer* pBot);
void StartNewBotFrame(AIPlayer* pBot);
void EndBotFrame(AIPlayer* pBot);

void OnBotTeleport(AIPlayer* pBot);

// Sets up the bot's think routine
void RunAIPlayerFrame(AIPlayer* pBot);
// The bot's actual think routine. All the logic to make the bot do something sits here
void AIPlayerThink(AIPlayer* pBot);
// The bot's think routine if they need to do something to join a team and start playing (e.g. pick a class)
void AIPlayerStartGameThink(AIPlayer* pBot);

// Called when the bot is killed by someone
void AIPlayerKilled(AIPlayer* pBot, edict_t* Killer);

void BotSwitchToWeapon(AIPlayer* pBot, AIWeaponType NewWeaponSlot);

bool ShouldBotThink(AIPlayer* pBot);

void BotResumePlay(AIPlayer* pBot);

void AIPlayerTakeDamage(AIPlayer* pBot, int damageTaken, edict_t* aggressor);

// Simulates a fake client inputting a console command
void FakeClientCommand(edict_t* pBot, const char* arg1, const char* arg2, const char* arg3);

#endif
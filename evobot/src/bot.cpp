//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot.cpp
// 
// Contains core bot functionality. Needs refactoring as it's too big right now
//

#ifndef _WIN32
#include <string.h>
#endif

#include <extdll.h>
#include <dllapi.h>
#include <h_export.h>
#include <meta_api.h>

#include <unordered_map>

#include "bot.h"
#include "bot_structs.h"
#include "bot_func.h"
#include "bot_navigation.h"
#include "bot_math.h"
#include "bot_commander.h"
#include "bot_gorge.h"
#include "bot_config.h"
#include "bot_tactical.h"
#include "bot_marine.h"

extern edict_t *clients[32];
extern int IsDedicatedServer;

extern bool bGameIsActive;
extern float GameStartTime;

static FILE *fp;

bot_t bots[32];   // max of 32 bots in a game

extern bot_weapon_t weapon_defs[MAX_WEAPONS];

BOTTIMER bottimers[32];

float f_ffheight; // Height of the far frustrum
float f_ffwidth; // Width of the far frustrum
float f_fnheight; // Height of the near frustrum
float f_fnwidth; // Width of the near frustrum


void BotSpawnInit(bot_t *pBot)
{

	memset(&(pBot->current_weapon), 0, sizeof(pBot->current_weapon));
	memset(&(pBot->m_rgAmmo), 0, sizeof(pBot->m_rgAmmo));


	// Force bot to choose a new destination
	ClearBotPath(pBot);

	UpdateViewFrustum(pBot);

	pBot->f_previous_command_time = gpGlobals->time;
}

int UTIL_GetNumPlayersOnTeam(const int Team)
{
	int Result = 0;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && clients[i]->v.team == Team) { Result++; }
	}

	return Result;
}

void BotCreate(edict_t *pPlayer, int Team)
{

	int index = 0;
	while ((bots[index].is_used) && (index < 32))
		index++;

	if (index == 32)
	{
		LOG_CONSOLE(PLID, "Max number of bots (32) added.\n");
		return;
	}

	if (!NavmeshLoaded())
	{
		if (!loadNavigationData(STRING(gpGlobals->mapname)))
		{
			LOG_CONSOLE(PLID, "Could not find %s.nav in addons/evobot/navmeshes, or file corrupted. Please generate a nav file and try again.\n", STRING(gpGlobals->mapname));
			UnloadNavigationData();
			return;
		}
		else
		{
			LOG_CONSOLE(PLID, "Navigation data for %s loaded successfully\n", STRING(gpGlobals->mapname));
			UTIL_ClearMapAIData();
			UTIL_ClearMapLocations();
			UTIL_PopulateResourceNodeLocations();
			PopulateEmptyHiveList();
		}
	}

	edict_t *BotEnt = nullptr;
	bot_t *pBot = nullptr;

	char c_name[BOT_NAME_LEN + 1];

	f_fnheight = 2.0f * tan((BOT_FOV * 0.0174532925f) / 2.0f) * BOT_MIN_VIEW;
	f_fnwidth = f_fnheight * BOT_ASPECT_RATIO;

	f_ffheight = 2.0f * tan((BOT_FOV * 0.0174532925f) / 2.0f) * BOT_MAX_VIEW;
	f_ffwidth = f_ffheight * BOT_ASPECT_RATIO;

	CONFIG_GetBotName(c_name);

	BotEnt = (*g_engfuncs.pfnCreateFakeClient)(c_name);
	
	if (!BotEnt)
	{
		LOG_CONSOLE(PLID, "Max players reached (Server Max: %d)\n", gpGlobals->maxClients);

		return;
	}
	
	char ptr[128];  // allocate space for message from ClientConnect
	char *infobuffer;
	int clientIndex;

	LOG_CONSOLE(PLID, "Creating EvoBot...\n");

	// create the player entity by calling MOD's player function
	// (from LINK_ENTITY_TO_CLASS for player object)

	CALL_GAME_ENTITY(PLID, "player", VARS(BotEnt));

	infobuffer = GET_INFOKEYBUFFER(BotEnt);
	clientIndex = ENTINDEX(BotEnt);

	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "model", "");
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "rate", "3500.000000");
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "cl_updaterate", "20");

	// Thanks Immortal_BLG for finding that cl_lw and cl_lc need to be 0 to fix bots getting stuck inside each other
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "cl_lw", "0");
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "cl_lc", "0");

	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "tracker", "0");
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "cl_dlmax", "128");
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "lefthand", "1");
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "friends", "0");
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "dm", "0");
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "ah", "1");
	SET_CLIENT_KEYVALUE(clientIndex, infobuffer, "_vgui_menus", "0");

	MDLL_ClientConnect(BotEnt, c_name, "127.0.0.1", ptr);

	// HPB_bot metamod fix - START

	// we have to do the ClientPutInServer() hook's job ourselves since calling the MDLL_
	// function calls directly the gamedll one, and not ours. You are allowed to call this
	// an "awful hack".

	int i = 0;

	while ((i < 32) && (clients[i] != NULL))
		i++;

	if (i < 32)
		clients[i] = BotEnt;  // store this clients edict in the clients array

	// HPB_bot metamod fix - END

	// Pieter van Dijk - use instead of DispatchSpawn() - Hip Hip Hurray!
	MDLL_ClientPutInServer(BotEnt);

	BotEnt->v.flags |= FL_THIRDPARTYBOT;

	BotEnt->v.idealpitch = BotEnt->v.v_angle.x;
	BotEnt->v.ideal_yaw = BotEnt->v.v_angle.y;

	// these should REALLY be MOD dependant...
	BotEnt->v.pitch_speed = 270;  // slightly faster than HLDM of 225
	BotEnt->v.yaw_speed = 250; // slightly faster than HLDM of 210

	// initialize all the variables for this bot...

	pBot = &bots[index];
	
	pBot->is_used = true;

	pBot->respawn_state = RESPAWN_IDLE;
	pBot->name[0] = 0;  // name not set by server yet
	pBot->BotNavInfo.PathSize = 0;

	pBot->pEdict = BotEnt;

	pBot->not_started = 1;  // hasn't joined game yet

	UTIL_ClearAllBotData(pBot);
	BotSpawnInit(pBot);

	pBot->bot_team = Team;

	//char logName[64];

	//sprintf(logName, "%s_log.txt", c_name);

	//pBot->logFile = fopen(logName, "w+");

	//if (pBot->logFile)
	//{
	//	fprintf(pBot->logFile, "Log for %s\n\n", c_name);
	//}
}

void UTIL_ClearAllBotData(bot_t* pBot)
{
	UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);

	UTIL_ClearGuardInfo(pBot);

	pBot->CurrentTask = nullptr;

	memset(pBot->CurrentCommanderActions, 0, sizeof(pBot->CurrentCommanderActions));

	memset(pBot->TrackedEnemies, 0, sizeof(pBot->TrackedEnemies));
	pBot->CurrentEnemyRef = nullptr;
	pBot->CurrentEnemy = -1;

	memset(pBot->ChatMessages, 0, sizeof(pBot->ChatMessages));

	pBot->DesiredCombatWeapon = WEAPON_NONE;
	ClearBotPath(pBot);

	pBot->BotNavInfo.bIsJumping = false;
	pBot->BotNavInfo.LandedTime = 0.0f;
	pBot->BotNavInfo.IsOnGround = false;

	memset(&(pBot->current_weapon), 0, sizeof(pBot->current_weapon));
	memset(&(pBot->m_rgAmmo), 0, sizeof(pBot->m_rgAmmo));

	pBot->LastCommanderRequestTime = 0.0f;
	pBot->LastCombatTime = 0.0f;
	pBot->next_commander_action_time = 0.0f;
	pBot->LastTargetTrackUpdate = 0.0f;

	pBot->DesiredLookDirection = ZERO_VECTOR;
	pBot->desiredMovementDir = ZERO_VECTOR;
	pBot->InterpolatedLookDirection = ZERO_VECTOR;
	pBot->LookTarget = nullptr;
	pBot->LookTargetLocation = ZERO_VECTOR;
	pBot->MoveLookLocation = ZERO_VECTOR;	

	pBot->CurrentRole = BOT_ROLE_NONE;

	pBot->bot_ns_class = CLASS_NONE;

	pBot->LastUseTime = 0.0f;

	pBot->CommanderLastScanTime = 0.0f;

	if (pBot->logFile)
	{
		fflush(pBot->logFile);
		fclose(pBot->logFile);
	}
}

float GetLeapCost(bot_t* pBot)
{
	switch (pBot->bot_ns_class)
	{
		case CLASS_SKULK:
			return ((BotHasWeapon(pBot, WEAPON_SKULK_LEAP)) ? kLeapEnergyCost : 0.0f);
		case CLASS_FADE:
			return ((BotHasWeapon(pBot, WEAPON_FADE_BLINK)) ? kBlinkEnergyCost : 0.0f);
		case CLASS_ONOS:
			return ((BotHasWeapon(pBot, WEAPON_ONOS_CHARGE)) ? kChargeEnergyCost : 0.0f);
		default:
			return 0.0f;
	}
}

char* UTIL_CommanderActionToChar(const CommanderActionType ActionType)
{
	switch (ActionType)
	{
		case ACTION_BUILD:
			return "Build";
		case ACTION_DROPITEM:
			return "Drop Item";
		case ACTION_GIVEORDER:
			return "Give Order";
		case ACTION_RECYCLE:
			return "Recycle";
		case ACTION_RESEARCH:
			return "Research";
		case ACTION_UPGRADE:
			return "Upgrade";
		default:
			return "None";
	}
}

char* UTIL_BotRoleToChar(const BotRole Role)
{
	switch (Role)
	{
		case BOT_ROLE_ATTACK_HIVE:
			return "Attack Hive";
		case BOT_ROLE_BUILDER:
			return "Builder";
		case BOT_ROLE_COMMAND:
			return "Command";
		case BOT_ROLE_FADE:
			return "Fade";
		case BOT_ROLE_FIND_RESOURCES:
			return "Find Resources";
		case BOT_ROLE_GUARD_BASE:
			return "Guard Base";
		case BOT_ROLE_HARASS:
			return "Harass";
		case BOT_ROLE_LERK:
			return "Lerk";
		case BOT_ROLE_NONE:
			return "None";
		case BOT_ROLE_ONOS:
			return "Onos";
		case BOT_ROLE_RES_CAPPER:
			return "Resource Capper";
		default:
			return "INVALID";
	}

	return "INVALID";
}

char* UTIL_TaskTypeToChar(const BotTaskType TaskType)
{
	switch (TaskType)
	{
	case TASK_NONE:
		return "None";
	case TASK_BUILD:
		return "Build";
	case TASK_GET_AMMO:
		return "Get Ammo";
	case TASK_ATTACK:
		return "Attack";
	case TASK_GET_EQUIPMENT:
		return "Get Equipment";
	case TASK_GET_HEALTH:
		return "Get Health";
	case TASK_GET_WEAPON:
		return "Get Weapon";
	case TASK_GUARD:
		return "Guard";
	case TASK_HEAL:
		return "Heal";
	case TASK_MOVE:
		return "Move";
	case TASK_RESUPPLY:
		return "Resupply";
	case TASK_CAP_RESNODE:
		return "Cap Resource Node";
	case TASK_WELD:
		return "Weld";
	case TASK_DEFEND:
		return "Defend";
	case TASK_EVOLVE:
		return "Evolve";
	default:
		return "INVALID";
	}
}

NSPlayerClass UTIL_GetPlayerClass(const edict_t* Player)
{
	if (FNullEnt(Player)) { return CLASS_NONE; }
	
	if (IsPlayerGestating(Player)) { return CLASS_EGG; }
	
	int iuser3 = Player->v.iuser3;

	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return CLASS_MARINE;
	case AVH_USER3_COMMANDER_PLAYER:
		return CLASS_MARINE_COMMANDER;
	case AVH_USER3_ALIEN_EMBRYO:
		return CLASS_EGG;
	case AVH_USER3_ALIEN_PLAYER1:
		return CLASS_SKULK;
	case AVH_USER3_ALIEN_PLAYER2:
		return CLASS_GORGE;
	case AVH_USER3_ALIEN_PLAYER3:
		return CLASS_LERK;
	case AVH_USER3_ALIEN_PLAYER4:
		return CLASS_FADE;
	case AVH_USER3_ALIEN_PLAYER5:
		return CLASS_ONOS;
	default:
		return CLASS_NONE;
	}

	return CLASS_NONE;
}

bool IsPlayerBot(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return Player && ((Player->v.flags & FL_FAKECLIENT) || (Player->v.flags & FL_THIRDPARTYBOT));
}

bool IsPlayerHuman(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (!(Player->v.flags & FL_FAKECLIENT) && !(Player->v.flags & FL_THIRDPARTYBOT));
}

bool IsPlayerCommander(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_COMMANDER_PLAYER);
}

bool IsPlayerSkulk(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER1);
}

bool IsPlayerMarine(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_MARINE_PLAYER);
}

bool IsPlayerClimbingWall(const edict_t* Player) {
	if (FNullEnt(Player)) { return false; }
	return (IsPlayerSkulk(Player) && (Player->v.iuser4 & MASK_WALLSTICKING));
}

bool IsPlayerGorge(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER2);
}

bool IsPlayerLerk(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER3);
}

bool IsPlayerFade(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER4);
}

bool IsPlayerOnos(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER5);
}

bool IsPlayerParasited(const edict_t* Player)
{
	if (FNullEnt(Player) || !IsPlayerOnMarineTeam(Player)) { return false; }
	return (Player->v.iuser4 & MASK_PARASITED);
}

bool IsPlayerMotionTracked(const edict_t* Player)
{
	if (FNullEnt(Player) || !IsPlayerOnAlienTeam(Player)) { return false; }
	return UTIL_StructureExistsOfType(STRUCTURE_MARINE_OBSERVATORY) && (Player->v.iuser4 & MASK_VIS_DETECTED);
}

float GetPlayerEnergyPercentage(const edict_t* Player)
{
	return (Player->v.fuser3 * 0.001f);
}

float GetEnergyCostForWeapon(const NSWeapon Weapon)
{
	switch (Weapon)
	{
		case WEAPON_SKULK_BITE:
			return kBiteEnergyCost;
		case WEAPON_SKULK_PARASITE:
			return kParasiteEnergyCost;
		case WEAPON_SKULK_LEAP:
			return kLeapEnergyCost;
		case WEAPON_SKULK_XENOCIDE:
			return kDivineWindEnergyCost;

		case WEAPON_GORGE_SPIT:
			return kSpitEnergyCost;
		case WEAPON_GORGE_HEALINGSPRAY:
			return kHealingSprayEnergyCost;
		case WEAPON_GORGE_BILEBOMB:
			return kBileBombEnergyCost;
		case WEAPON_GORGE_WEB:
			return kWebEnergyCost;

		case WEAPON_LERK_BITE:
			return kBite2EnergyCost;
		case WEAPON_LERK_SPORES:
			return kSporesEnergyCost;
		case WEAPON_LERK_UMBRA:
			return kUmbraEnergyCost;
		case WEAPON_LERK_PRIMALSCREAM:
			return kPrimalScreamEnergyCost;

		case WEAPON_FADE_SWIPE:
			return kSwipeEnergyCost;
		case WEAPON_FADE_BLINK:
			return kBlinkEnergyCost;
		case WEAPON_FADE_METABOLIZE:
			return kMetabolizeEnergyCost;
		case WEAPON_FADE_ACIDROCKET:
			return kAcidRocketEnergyCost;
		
		case WEAPON_ONOS_GORE:
			return kClawsEnergyCost;
		case WEAPON_ONOS_DEVOUR:
			return kDevourEnergyCost;
		case WEAPON_ONOS_STOMP:
			return kStompEnergyCost;
		case WEAPON_ONOS_CHARGE:
			return kChargeEnergyCost;

		default:
			return 0.0f;
	}
}


void UTIL_BotSuicide(bot_t* pBot)
{
	if (pBot && !IsPlayerDead(pBot->pEdict) && !pBot->bIsPendingKill)
	{
		pBot->bIsPendingKill = true;
		MDLL_ClientKill(pBot->pEdict);
	}
	
}

void StartNewBotFrame(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	pEdict->v.flags |= FL_THIRDPARTYBOT;
	pEdict->v.button = 0;
	pBot->ForwardMove = 0.0f;
	pBot->SideMove = 0.0f;
	pBot->UpMove = 0.0f;
	pBot->impulse = 0;
	pBot->resources = GetPlayerResources(pBot->pEdict);
	pBot->CurrentEyePosition = UTIL_GetPlayerEyePosition(pEdict);
	pBot->CurrentFloorPosition = UTIL_GetFloorUnderEntity(pEdict);
	pBot->LookTargetLocation = ZERO_VECTOR;
	pBot->MoveLookLocation = ZERO_VECTOR;
	pBot->LookTarget = nullptr;

	pBot->DesiredCombatWeapon = WEAPON_NONE;
	pBot->DesiredMoveWeapon = WEAPON_NONE;

	pBot->bot_ns_class = UTIL_GetPlayerClass(pBot->pEdict);

	if (IsPlayerOnAlienTeam(pEdict))
	{
		pBot->Adrenaline = pEdict->v.fuser3 * 0.001f;
	}

	if (IsPlayerSkulk(pEdict))
	{
		pEdict->v.button |= IN_DUCK;
	}

	if (pEdict->v.flags & FL_ONGROUND || IsPlayerOnLadder(pEdict))
	{
		if (!pBot->BotNavInfo.IsOnGround)
		{
			pBot->BotNavInfo.LandedTime = gpGlobals->time;
		}

		pBot->BotNavInfo.IsOnGround = true;
		pBot->BotNavInfo.bIsJumping = false;
	}
	else
	{
		pBot->BotNavInfo.IsOnGround = false;
	}
}

void BotUseObject(bot_t* pBot, edict_t* Target, bool bContinuous)
{
	LookAt(pBot, UTIL_GetCentreOfEntity(Target));

	if (!bContinuous && ((gpGlobals->time - pBot->LastUseTime) < min_use_gap)) { return; }

	Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);
	Vector TargetAimDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - pBot->CurrentEyePosition);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	if (AimDot >= 0.95f)
	{
		pBot->pEdict->v.button |= IN_USE;
		pBot->LastUseTime = gpGlobals->time;
	}
}

void BotJump(bot_t* pBot)
{
	if (pBot->BotNavInfo.IsOnGround)
	{
		if (gpGlobals->time - pBot->BotNavInfo.LandedTime >= 0.5f)
		{
			pBot->pEdict->v.button |= IN_JUMP;
			pBot->BotNavInfo.bIsJumping = true;
		}
	}
	else
	{
		if (pBot->BotNavInfo.bIsJumping)
		{
			// Skulks, gorges and lerks can't duck jump...
			if (!IsPlayerSkulk(pBot->pEdict) && !IsPlayerGorge(pBot->pEdict) && !IsPlayerLerk(pBot->pEdict))
			{
				pBot->pEdict->v.button |= IN_DUCK;
			}
		}
	}
}

bool CanBotLeap(bot_t* pBot)
{
	return (IsPlayerSkulk(pBot->pEdict) && BotHasWeapon(pBot, WEAPON_SKULK_LEAP)) || (IsPlayerFade(pBot->pEdict) && BotHasWeapon(pBot, WEAPON_FADE_BLINK));
}

void BotLeap(bot_t* pBot, const Vector TargetLocation)
{

	if (!CanBotLeap(pBot))
	{
		BotJump(pBot);
		return;
	}

	NSWeapon LeapWeapon = (IsPlayerSkulk(pBot->pEdict)) ? WEAPON_SKULK_LEAP : WEAPON_FADE_BLINK;

	if (UTIL_GetBotCurrentWeapon(pBot) != LeapWeapon)
	{
		pBot->DesiredCombatWeapon = LeapWeapon;
		return;
	}

	Vector LookLocation = TargetLocation; 

	int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	unsigned char NavArea = UTIL_GetNavAreaAtLocation(NavProfileIndex, pBot->pEdict->v.origin);

	if (NavArea == SAMPLE_POLYAREA_CROUCH)
	{
		Vector MoveDir = UTIL_GetVectorNormal2D(TargetLocation - pBot->pEdict->v.origin);
		LookLocation = (pBot->CurrentEyePosition + (MoveDir * 50.0f) + Vector(0.0f, 0.0f, 10.0f));
	}
	else
	{
		LookLocation = LookLocation + Vector(0.0f, 0.0f, 200.0f);

		if (LeapWeapon == WEAPON_FADE_BLINK)
		{
			float PlayerCurrentSpeed = vSize3D(pBot->pEdict->v.velocity);
			float LaunchVelocity = PlayerCurrentSpeed + 255.0f;

			Vector LaunchAngle = GetPitchForProjectile(pBot->CurrentEyePosition, TargetLocation, LaunchVelocity, GOLDSRC_GRAVITY);

			if (LaunchAngle != ZERO_VECTOR)
			{
				LaunchAngle = UTIL_GetVectorNormal(LaunchAngle);
				LookLocation = pBot->CurrentEyePosition + (LaunchAngle * 200.0f);
			}
		}

		if (LeapWeapon == WEAPON_SKULK_LEAP)
		{
			float PlayerCurrentSpeed = vSize3D(pBot->pEdict->v.velocity);
			float LaunchVelocity = PlayerCurrentSpeed + 500.0f;

			Vector LaunchAngle = GetPitchForProjectile(pBot->CurrentEyePosition, TargetLocation, LaunchVelocity, GOLDSRC_GRAVITY);

			if (LaunchAngle != ZERO_VECTOR)
			{
				LaunchAngle = UTIL_GetVectorNormal(LaunchAngle);
				LookLocation = pBot->CurrentEyePosition + (LaunchAngle * 200.0f);
			}
		}
	}

	MoveLookAt(pBot, LookLocation);

	if (pBot->BotNavInfo.IsOnGround)
	{
		if (gpGlobals->time - pBot->BotNavInfo.LandedTime >= 0.2f && gpGlobals->time - pBot->BotNavInfo.LeapAttemptedTime >= 0.5f)
		{
			Vector FaceAngle = UTIL_GetForwardVector2D(pBot->pEdict->v.v_angle);
			Vector MoveDir = UTIL_GetVectorNormal2D(TargetLocation - pBot->pEdict->v.origin);

			float Dot = UTIL_GetDotProduct2D(FaceAngle, MoveDir);

			if (Dot >= 0.98f)
			{
				pBot->pEdict->v.button = IN_ATTACK2;
				pBot->BotNavInfo.bIsJumping = true;
				pBot->BotNavInfo.LeapAttemptedTime = gpGlobals->time;
			}
		}
	}
	else
	{
		if (pBot->BotNavInfo.bIsJumping)
		{
			// Skulks, gorges and lerks can't duck jump...
			if (!IsPlayerSkulk(pBot->pEdict) && !IsPlayerGorge(pBot->pEdict) && !IsPlayerLerk(pBot->pEdict))
			{
				pBot->pEdict->v.button |= IN_DUCK;
			}
		}
	}

}

bool ShouldBotThink(const bot_t* bot)
{
	edict_t* pEdict = bot->pEdict;
	return (!IsPlayerSpectator(pEdict) && !IsPlayerDead(pEdict) && !IsPlayerBeingDigested(pEdict) && !IsPlayerGestating(pEdict));
}

void BotThink(bot_t *pBot)
{
	if (IsPlayerGestating(pBot->pEdict)) { return; }

	if (IsPlayerInReadyRoom(pBot->pEdict))
	{
		ReadyRoomThink(pBot);
	}
	else
	{
		if (!bGameIsActive)
		{
			WaitGameStartThink(pBot);
		}
		else
		{

			if (IsPlayerCommander(pBot->pEdict))
			{
				CommanderThink(pBot);
			}
			else
			{
				pBot->CurrentEnemy = UTIL_GetNextEnemyTarget(pBot);

				if (pBot->CurrentEnemy > -1)
				{
					pBot->LastCombatTime = gpGlobals->time;
				}

				if (IsPlayerMarine(pBot->pEdict))
				{
					MarineThink(pBot);
				}
				else
				{
					AlienThink(pBot);
				}
			}				
		}		
	}
}

void DroneThink(bot_t* pBot)
{
	UpdateAndClearTasks(pBot);

	if (pBot->PrimaryBotTask.TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, &pBot->PrimaryBotTask);
	}

	if (pBot->BotNavInfo.PathSize > 0)
	{
		DEBUG_DrawPath(pBot->BotNavInfo.CurrentPath, pBot->BotNavInfo.PathSize, 0.0f);
	}
}

void TestNavThink(bot_t* pBot)
{
	UpdateAndClearTasks(pBot);

	pBot->CurrentTask = &pBot->PrimaryBotTask;

	if (pBot->PrimaryBotTask.TaskType == TASK_MOVE)
	{
		BotProgressTask(pBot, &pBot->PrimaryBotTask);
	}
	else
	{

		int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

		Vector RandomPoint = UTIL_GetRandomPointOfInterest();

		if (RandomPoint != ZERO_VECTOR && UTIL_PointIsReachable(MoveProfile, pBot->pEdict->v.origin, RandomPoint, max_player_use_reach))
		{
			pBot->PrimaryBotTask.TaskType = TASK_MOVE;
			pBot->PrimaryBotTask.TaskLocation = RandomPoint;
			pBot->PrimaryBotTask.bOrderIsUrgent = true;
		}
		else
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
		}
	}
}

int GetPlayerResources(const edict_t* Player)
{
	if (FNullEnt(Player)) { return 0; }

	return (int)ceil(Player->v.vuser4.z / kNumericNetworkConstant);
}

void WaitGameStartThink(bot_t* pBot)
{
	if (gpGlobals->time - pBot->GuardStartLookTime > pBot->ThisGuardLookTime)
	{
		pBot->GuardLookLocation = UTIL_RandomPointOnCircle(pBot->pEdict->v.origin, 200.0f);
		pBot->GuardStartLookTime = gpGlobals->time;
		pBot->ThisGuardLookTime = frandrange(3.0f, 10.0f);
	}
	else
	{
		LookAt(pBot, pBot->GuardLookLocation);
	}
}

void BotUpdateDesiredViewRotation(bot_t* pBot)
{
	// We always prioritise MoveLookLocation if it is set so the bot doesn't screw up wall climbing or ladder movement
	Vector NewLookLocation = (pBot->MoveLookLocation != ZERO_VECTOR) ? pBot->MoveLookLocation : pBot->LookTargetLocation;

	// We're already interpolating to an existing desired look direction (see BotUpdateViewRotation()) or we don't have a desired look target
	if (pBot->DesiredLookDirection != ZERO_VECTOR || !NewLookLocation) { return; }

	edict_t* pEdict = pBot->pEdict;

	Vector dir = UTIL_GetVectorNormal(NewLookLocation - pBot->CurrentEyePosition);

	// Obtain the desired view angles the bot needs to look directly at the target position
	pBot->DesiredLookDirection = UTIL_VecToAngles(dir);

	// Sanity check to make sure we don't end up with NaN values. This causes the bot to start slowly rotating like they're adrift in space
	if (isnan(pBot->DesiredLookDirection.x))
	{
		pBot->DesiredLookDirection = ZERO_VECTOR;
	}

	// Clamp the pitch and yaw to valid ranges

	if (pBot->DesiredLookDirection.y > 180)
		pBot->DesiredLookDirection.y -= 360;

	// Paulo-La-Frite - START bot aiming bug fix
	if (pBot->DesiredLookDirection.y < -180)
		pBot->DesiredLookDirection.y += 360;

	if (pBot->DesiredLookDirection.x > 180)
		pBot->DesiredLookDirection.x -= 360;

	// Now figure out how far we have to turn to reach our desired target
	float yDelta = pBot->DesiredLookDirection.y - pBot->InterpolatedLookDirection.y;
	float xDelta = pBot->DesiredLookDirection.x - pBot->InterpolatedLookDirection.x;

	// This prevents them turning the long way around

	if (yDelta > 180.0f)
		yDelta -= 360.0f;
	if (yDelta < -180.0f)
		yDelta += 360.0f;

	float maxDelta = fmaxf(fabsf(yDelta), fabsf(xDelta));

	// We add a random offset to the view angles based on how far the bot has to move its view
	// This simulates the fact that humans can't spin and lock their cross-hair exactly on the target, the further you have the spin, the more off your view will be first attempt
	if (fabsf(maxDelta) >= 45.0f)
	{
		pBot->ViewInterpolationSpeed = 300.0f;//1.25f;
		pBot->DesiredLookDirection.x += frandrange(-20.0f, 20.0f);
		pBot->DesiredLookDirection.y += frandrange(-20.0f, 20.0f);
	}
	else if (fabsf(maxDelta) >= 25.0f)
	{
		pBot->ViewInterpolationSpeed = 150.0f;//0.3f;
		pBot->DesiredLookDirection.x += frandrange(-10.0f, 10.0f);
		pBot->DesiredLookDirection.y += frandrange(-10.0f, 10.0f);
	}
	else if (fabsf(maxDelta) >= 5.0f)
	{
		pBot->ViewInterpolationSpeed = 50.0f;//0.1f;
		pBot->DesiredLookDirection.x += frandrange(-5.0f, 5.0f);
		pBot->DesiredLookDirection.y += frandrange(-5.0f, 5.0f);
	}
	else
	{
		pBot->ViewInterpolationSpeed = 50.0f;
	}

	// We once again clamp everything to valid values in case the offsets we applied above took us above that

	if (pBot->DesiredLookDirection.y > 180)
		pBot->DesiredLookDirection.y -= 360;

	// Paulo-La-Frite - START bot aiming bug fix
	if (pBot->DesiredLookDirection.y < -180)
		pBot->DesiredLookDirection.y += 360;

	if (pBot->DesiredLookDirection.x > 180)
		pBot->DesiredLookDirection.x -= 360;

	// We finally have our desired turn movement, ready for BotUpdateViewRotation() to pick up and make happen
	pBot->ViewInterpStartedTime = gpGlobals->time;
}

byte ThrottledMsec(bot_t *pBot)
{

	// Thanks to The Storm (ePODBot) for this one, finally fixed the bot running speed!
	int newmsec = (int)((gpGlobals->time - pBot->f_previous_command_time) * 1000);
	if (newmsec > 255)  // Doh, bots are going to be slower than they should if this happens.
		newmsec = 255;		 // Upgrade that CPU or use fewer bots!

	return (byte)newmsec;
}

void UpdateView(bot_t* pBot) {
	int visibleCount = 0;

	// Updates the view frustum based on the bot's position and v_angle
	UpdateViewFrustum(pBot);

	memset(pBot->visiblePlayers, 0, sizeof(float) * 32);

	// Update list of currently visible players
	for (int i = 0; i < 32; i++) {
		if (!FNullEnt(clients[i])) {
			if (!IsPlayerInReadyRoom(clients[i]) && clients[i]->v.team != pBot->pEdict->v.team && !IsPlayerSpectator(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && !IsPlayerCommander(clients[i]))
			{
				pBot->TrackedEnemies[i].EnemyEdict = clients[i];

				if (IsPlayerVisibleToBot(pBot, clients[i]))
				{
					pBot->visiblePlayers[visibleCount++] = clients[i];

					pBot->TrackedEnemies[i].EnemyEdict = clients[i];
					pBot->TrackedEnemies[i].bIsValidTarget = true;
					pBot->TrackedEnemies[i].bCurrentlyVisible = true;
					pBot->TrackedEnemies[i].LastSeenLocation = UTIL_GetCentreOfEntity(clients[i]);
					pBot->TrackedEnemies[i].LastSeenVelocity = clients[i]->v.velocity;
					pBot->TrackedEnemies[i].bIsTracked = false;
					pBot->TrackedEnemies[i].LastSeenTime = gpGlobals->time;
					
				}
				else
				{
					pBot->TrackedEnemies[i].bCurrentlyVisible = false;

					if (IsPlayerParasited(pBot->TrackedEnemies[i].EnemyEdict) || IsPlayerMotionTracked(pBot->TrackedEnemies[i].EnemyEdict))
					{
						pBot->TrackedEnemies[i].TrackedLocation = clients[i]->v.origin;
						pBot->TrackedEnemies[i].LastSeenVelocity = clients[i]->v.velocity;
						pBot->TrackedEnemies[i].LastTrackedTime = gpGlobals->time;
						pBot->TrackedEnemies[i].bIsTracked = true;
						
					}
					else
					{
						pBot->TrackedEnemies[i].bIsTracked = false;
					}

					if (pBot->TrackedEnemies[i].bIsTracked)
					{
						bool IsCloseEnoughToBeRelevant = vDist2DSq(pBot->pEdict->v.origin, pBot->TrackedEnemies[i].TrackedLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f));
						bool RecentlyTracked = (gpGlobals->time - pBot->TrackedEnemies[i].LastTrackedTime) < 5.0f;
						pBot->TrackedEnemies[i].bIsValidTarget = IsCloseEnoughToBeRelevant && RecentlyTracked;
					}
					else
					{
						if (vDist2DSq(pBot->pEdict->v.origin, pBot->TrackedEnemies[i].LastSeenLocation) < sqrf(UTIL_GetPlayerRadius(pBot->pEdict)))
						{
							if (!UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, pBot->TrackedEnemies[i].EnemyEdict->v.origin))
							{
								pBot->TrackedEnemies[i].LastSeenTime = 0.0f;
							}
						}
						pBot->TrackedEnemies[i].bIsValidTarget = (gpGlobals->time - pBot->TrackedEnemies[i].LastSeenTime) < 10.0f;
					}
				}				
			}
			else
			{
				pBot->TrackedEnemies[i].bCurrentlyVisible = false;
				pBot->TrackedEnemies[i].TrackedLocation = ZERO_VECTOR;
				pBot->TrackedEnemies[i].LastSeenLocation = ZERO_VECTOR;
				pBot->TrackedEnemies[i].bIsValidTarget = false;
			}
		}
		else
		{
			pBot->TrackedEnemies[i].bCurrentlyVisible = false;
			pBot->TrackedEnemies[i].TrackedLocation = ZERO_VECTOR;
			pBot->TrackedEnemies[i].LastSeenLocation = ZERO_VECTOR;
			pBot->TrackedEnemies[i].bIsValidTarget = false;
		}
	}

	// Call BotSeePlayer for every visible player each frame
	for (int i = 0; i < visibleCount; i++) {
		BotSeePlayer(pBot, pBot->visiblePlayers[i]);
	}

}

Vector UTIL_GetGrenadeThrowTarget(bot_t* pBot, const Vector TargetLocation, const float ExplosionRadius)
{
	if (UTIL_PlayerHasLOSToLocation(pBot->pEdict, TargetLocation, UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		return TargetLocation;
	}

	if (UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, TargetLocation))
	{
		Vector Orientation = UTIL_GetVectorNormal(pBot->pEdict->v.origin - TargetLocation);

		Vector NewSpot = TargetLocation + (Orientation * UTIL_MetresToGoldSrcUnits(1.5f));

		NewSpot = UTIL_ProjectPointToNavmesh(NewSpot);

		if (NewSpot != ZERO_VECTOR)
		{
			NewSpot.z += 10.0f;
		}

		return NewSpot;
	}

	bot_path_node CheckPath[MAX_PATH_SIZE];
	int PathSize = 0;

	dtStatus Status = FindPathClosestToPoint(ALL_NAV_PROFILE, pBot->pEdict->v.origin, TargetLocation, CheckPath, &PathSize, ExplosionRadius);

	if (dtStatusSucceed(Status))
	{
		Vector FurthestPointVisible = UTIL_GetFurthestVisiblePointOnPath(pBot->CurrentEyePosition, CheckPath, PathSize);

		if (vDist3DSq(FurthestPointVisible, TargetLocation) <= sqrf(ExplosionRadius))
		{
			return FurthestPointVisible;
		}

		Vector ThrowDir = UTIL_GetVectorNormal(FurthestPointVisible - pBot->pEdict->v.origin);

		Vector LineEnd = FurthestPointVisible + (ThrowDir * UTIL_MetresToGoldSrcUnits(5.0f));

		Vector ClosestPointInTrajectory = vClosestPointOnLine(FurthestPointVisible, LineEnd, TargetLocation);

		ClosestPointInTrajectory = UTIL_ProjectPointToNavmesh(ClosestPointInTrajectory);
		ClosestPointInTrajectory.z += 10.0f;

		if (vDist2DSq(ClosestPointInTrajectory, TargetLocation) < sqrf(ExplosionRadius) && UTIL_PlayerHasLOSToLocation(pBot->pEdict, ClosestPointInTrajectory, UTIL_MetresToGoldSrcUnits(10.0f)) && UTIL_PointIsDirectlyReachable(ClosestPointInTrajectory, TargetLocation))
		{
			return ClosestPointInTrajectory;
		}
		else
		{
			return ZERO_VECTOR;
		}
	}
	else
	{
		return ZERO_VECTOR;
	}
}










// Called once per frame per player currently visible
void BotSeePlayer(bot_t* pBot, edict_t* seen)
{

}

bool UTIL_PlayerHasAlienUpgradeOfType(const edict_t* Player, const HiveTechStatus TechType)
{
	if (!IsPlayerOnAlienTeam(Player)) { return false; }

	switch (TechType)
	{
		case HIVE_TECH_DEFENCE:
			return ((Player->v.iuser4 & MASK_UPGRADE_1) || (Player->v.iuser4 & MASK_UPGRADE_2) || (Player->v.iuser4 & MASK_UPGRADE_3));
		case HIVE_TECH_MOVEMENT:
			return ((Player->v.iuser4 & MASK_UPGRADE_4) || (Player->v.iuser4 & MASK_UPGRADE_5) || (Player->v.iuser4 & MASK_UPGRADE_6));
		case HIVE_TECH_SENSORY:
			return ((Player->v.iuser4 & MASK_UPGRADE_7) || (Player->v.iuser4 & MASK_UPGRADE_8) || (Player->v.iuser4 & MASK_UPGRADE_9));
		default:
			return false;
	}
}

// The bot was killed by another bot or human player (or blew themselves up)
void BotDied(bot_t* pBot, edict_t* killer)
{
	UTIL_ClearGuardInfo(pBot);
	ClearBotMovement(pBot);

	pBot->LastCombatTime = 0.0f;

	for (int i = 0; i < 32; i++)
	{
		pBot->TrackedEnemies[i].LastSeenTime = 0.0f;
	}

	UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);

	pBot->bIsPendingKill = false;

	pBot->BotNavInfo.LastNavMeshPosition = ZERO_VECTOR;
}

// Bot killed another player or bot
void BotKilledPlayer(bot_t* pBot, edict_t* victim) {

}

int UTIL_GetNextEnemyTarget(bot_t* pBot)
{
	int ClosestVisibleEnemy = -1;
	float MinVisibleDist = 0.0f;

	int ClosestNonVisibleEnemy = -1;
	float MinNonVisibleDist = 0.0f;

	edict_t* ClosestVisibleEnemyEdict = nullptr;
	edict_t* ClosestNonVisibleEnemyEdict = nullptr;

	for (int i = 0; i < 32; i++)
	{
		if (pBot->TrackedEnemies[i].bIsValidTarget)
		{
			float thisDist = vDist2DSq(pBot->pEdict->v.origin, pBot->TrackedEnemies[i].EnemyEdict->v.origin);

			if (pBot->TrackedEnemies[i].bCurrentlyVisible)
			{
				if (ClosestVisibleEnemy < 0 || thisDist < MinVisibleDist)
				{
					ClosestVisibleEnemy = i;
					MinVisibleDist = thisDist;
				}
			}
			else
			{
				if (ClosestNonVisibleEnemy < 0 || thisDist < MinNonVisibleDist)
				{
					ClosestNonVisibleEnemy = i;
					MinNonVisibleDist = thisDist;
				}
			}
		}
	}

	if (ClosestNonVisibleEnemy == -1 || ClosestVisibleEnemy == -1)
	{
		return std::max(ClosestNonVisibleEnemy, ClosestVisibleEnemy);
	}
	else
	{
		ClosestVisibleEnemyEdict = pBot->TrackedEnemies[ClosestVisibleEnemy].EnemyEdict;
		ClosestNonVisibleEnemyEdict = pBot->TrackedEnemies[ClosestNonVisibleEnemy].EnemyEdict;

		if (pBot->TrackedEnemies[ClosestNonVisibleEnemy].LastSeenTime > 5.0f || IsPlayerGorge(ClosestNonVisibleEnemyEdict))
		{
			return ClosestVisibleEnemy;
		}
		else
		{
			bool bNonVisibleEnemyHasLOS = UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, ClosestNonVisibleEnemyEdict->v.origin);

			if (bNonVisibleEnemyHasLOS)
			{
				return (MinVisibleDist < MinNonVisibleDist) ? ClosestVisibleEnemy : ClosestNonVisibleEnemy;
			}
			else
			{
				return ClosestVisibleEnemy;
			}
		}
	}

	return -1;
}

// Bot took damage from another bot or player. If no aggressor could be determined then the aggressor is the bot taking the damage
void BotTakeDamage(bot_t* pBot, int damageTaken, edict_t* aggressor)
{
	int aggressorIndex = GetPlayerIndex(aggressor);

	if (aggressorIndex > -1 && !IsPlayerDead(aggressor) && aggressor->v.team != pBot->pEdict->v.team && !IsPlayerBeingDigested(aggressor))
	{
		pBot->TrackedEnemies[aggressorIndex].LastSeenTime = gpGlobals->time;

		// If the bot can't see the enemy (bCurrentlyVisible is false) then set the last seen location to a random point in the vicinity so the bot doesn't immediately know where they are
		if (pBot->TrackedEnemies[aggressorIndex].bCurrentlyVisible)
		{
			pBot->TrackedEnemies[aggressorIndex].LastSeenLocation = aggressor->v.origin;
		}
		else
		{
			// The further the enemy is, the more inaccurate the bot's guess will be where they are
			pBot->TrackedEnemies[aggressorIndex].LastSeenLocation = UTIL_GetRandomPointOnNavmeshInRadius(ALL_NAV_PROFILE, aggressor->v.origin, vDist2D(pBot->pEdict->v.origin, aggressor->v.origin));
		}

		//UTIL_DrawLine(clients[0], pBot->pEdict->v.origin, pBot->TrackedEnemies[aggressorIndex].LastSeenLocation, 5.0f);

		pBot->TrackedEnemies[aggressorIndex].LastSeenVelocity = aggressor->v.velocity;
		pBot->TrackedEnemies[aggressorIndex].bIsValidTarget = true;
	}
}

int GetPlayerIndex(const edict_t* Edict)
{
	for (int i = 0; i < 32; i++)
	{
		if (clients[i] == Edict) { return i; }
	}

	return -1;
}

// Updates the bot's viewing frustum. Done once per frame per bot
void UpdateViewFrustum(bot_t* pBot) {

	MAKE_VECTORS(pBot->pEdict->v.v_angle);
	Vector up = gpGlobals->v_up;
	Vector forward = gpGlobals->v_forward;
	Vector right = gpGlobals->v_right;

	Vector fc = (pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs) + (forward * BOT_MAX_VIEW);

	Vector fbl = fc + (up * f_ffheight / 2.0f) - (right * f_ffwidth / 2.0f);
	Vector fbr = fc + (up * f_ffheight / 2.0f) + (right * f_ffwidth / 2.0f);
	Vector ftl = fc - (up * f_ffheight / 2.0f) - (right * f_ffwidth / 2.0f);
	Vector ftr = fc - (up * f_ffheight / 2.0f) + (right * f_ffwidth / 2.0f);

	Vector nc = (pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs) + (forward * BOT_MIN_VIEW);

	Vector nbl = nc + (up * f_fnheight / 2.0f) - (right * f_fnwidth / 2.0f);
	Vector nbr = nc + (up * f_fnheight / 2.0f) + (right * f_fnwidth / 2.0f);
	Vector ntl = nc - (up * f_fnheight / 2.0f) - (right * f_fnwidth / 2.0f);
	Vector ntr = nc - (up * f_fnheight / 2.0f) + (right * f_fnwidth / 2.0f);

	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_TOP], ftl, ntl, ntr);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_BOTTOM], fbr, nbr, nbl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_LEFT], fbl, nbl, ntl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_RIGHT], ftr, ntr, nbr);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_NEAR], nbr, ntr, ntl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_FAR], fbl, ftl, ftr);

}

// Checks to see if pBot can see player. Returns true if player is visible
bool IsPlayerVisibleToBot(bot_t* Observer, edict_t* TargetPlayer)
{
	if (!TargetPlayer || IsPlayerBeingDigested(TargetPlayer) || IsPlayerCommander(TargetPlayer) || IsPlayerDead(TargetPlayer)) { return false; }
	// To make things a little more accurate, we're going to treat players as cylinders rather than boxes
	for (int i = 0; i < 6; i++) {
		// Our cylinder must be inside all planes to be visible, otherwise return false
		if (!UTIL_CylinderInsidePlane(&Observer->viewFrustum[i], TargetPlayer->v.origin - Vector(0, 0, 5), 60.0f, 16.0f)) {
			return false;
		}
	}

	// TODO: Think of a better way than simply checking to see if bot has line of sight with origin.
	//       Probably should just check the head, middle and feet.

	TraceResult hit;
	UTIL_TraceLine((Observer->pEdict->v.origin + Observer->pEdict->v.view_ofs), TargetPlayer->v.origin, ignore_monsters, ignore_glass, Observer->pEdict->v.pContainingEntity, &hit);

	return hit.flFraction >= 1.0f;
}

/* Makes the bot look at the specified entity */
void LookAt(bot_t* pBot, edict_t* target)
{
	if (!target) { return; }

	pBot->LookTarget = target;

	if (!UTIL_IsEdictPlayer(target))
	{
		pBot->LookTargetLocation = UTIL_GetCentreOfEntity(pBot->LookTarget);
		pBot->LastTargetTrackUpdate = gpGlobals->time;
		return;
	}

	if ((gpGlobals->time - pBot->LastTargetTrackUpdate) >= pBot->AimingDelay)
	{
		enemy_status* TrackedEnemyRef = UTIL_GetTrackedEnemyRefForTarget(pBot, target);

		Vector TargetVelocity = (TrackedEnemyRef) ? TrackedEnemyRef->LastSeenVelocity : pBot->LookTarget->v.velocity;
		Vector TargetLocation = (TrackedEnemyRef) ? TrackedEnemyRef->LastSeenLocation : UTIL_GetCentreOfEntity(pBot->LookTarget);

		Vector NewLoc = TargetLocation + (TargetVelocity * 0.2f);
		NewLoc = NewLoc - (pBot->pEdict->v.velocity * 0.2f);

		pBot->LookTargetLocation = NewLoc;
		pBot->LastTargetTrackUpdate = gpGlobals->time;
	}
}

/* Makes the bot look at the specified position */
void LookAt(bot_t* pBot, Vector target) {
	
	pBot->LookTargetLocation.x = target.x;
	pBot->LookTargetLocation.y = target.y;
	pBot->LookTargetLocation.z = target.z;

}

void MoveLookAt(bot_t* pBot, Vector target)
{
	pBot->MoveLookLocation.x = target.x;
	pBot->MoveLookLocation.y = target.y;
	pBot->MoveLookLocation.z = target.z;
}

void BotUpdateViewRotation(bot_t* pBot, float DeltaTime)
{
	if (pBot->DesiredLookDirection != ZERO_VECTOR)
	{
		edict_t* pEdict = pBot->pEdict;

		float Delta = pBot->DesiredLookDirection.y - pBot->InterpolatedLookDirection.y;

		if (Delta > 180.0f)
			Delta -= 360.0f;
		if (Delta < -180.0f)
			Delta += 360.0f;

		pBot->InterpolatedLookDirection.x = fInterpConstantTo(pBot->InterpolatedLookDirection.x, pBot->DesiredLookDirection.x, DeltaTime, (IsPlayerClimbingWall(pEdict) ? 400.0f : pBot->ViewInterpolationSpeed));

		float DeltaInterp = fInterpConstantTo(0.0f, Delta, DeltaTime, pBot->ViewInterpolationSpeed);

		pBot->InterpolatedLookDirection.y += DeltaInterp;

		if (pBot->InterpolatedLookDirection.y > 180.0f)
			pBot->InterpolatedLookDirection.y -= 360.0f;
		if (pBot->InterpolatedLookDirection.y < -180.0f)
			pBot->InterpolatedLookDirection.y += 360.0f;

		if (pBot->InterpolatedLookDirection.x == pBot->DesiredLookDirection.x && pBot->InterpolatedLookDirection.y == pBot->DesiredLookDirection.y)
		{
			pBot->DesiredLookDirection = ZERO_VECTOR;
		}
		else
		{
			// If the interp gets stuck for some reason then abandon it after 2 seconds. It should have completed way before then anyway
			if (gpGlobals->time - pBot->ViewInterpStartedTime > 2.0f)
			{
				pBot->DesiredLookDirection = ZERO_VECTOR;
			}
		}

		pEdict->v.v_angle.x = pBot->InterpolatedLookDirection.x;
		pEdict->v.v_angle.y = pBot->InterpolatedLookDirection.y;

		// set the body angles to point the gun correctly
		pEdict->v.angles.x = pEdict->v.v_angle.x / 3;
		pEdict->v.angles.y = pEdict->v.v_angle.y;
		pEdict->v.angles.z = 0;

		// adjust the view angle pitch to aim correctly (MUST be after body v.angles stuff)
		pEdict->v.v_angle.x = -pEdict->v.v_angle.x;
		// Paulo-La-Frite - END

		pEdict->v.ideal_yaw = pEdict->v.v_angle.y;

		if (pEdict->v.ideal_yaw > 180)
			pEdict->v.ideal_yaw -= 360;

		if (pEdict->v.ideal_yaw < -180)
			pEdict->v.ideal_yaw += 360;
	}

	if (pBot->bot_ns_class != CLASS_MARINE_COMMANDER && (gpGlobals->time - pBot->LastViewUpdateTime) > pBot->ViewUpdateRate)
	{
		UpdateView(pBot);
	}
}

void BotSay(bot_t* pBot, char* textToSay)
{
	UTIL_HostSay(pBot->pEdict, 0, textToSay);
}

void BotTeamSay(bot_t* pBot, char* textToSay)
{
	UTIL_HostSay(pBot->pEdict, 1, textToSay);
}

void BotReceiveCommanderOrder(bot_t* pBot, AvHOrderType orderType, AvHUser3 TargetType, Vector destination)
{
	UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);

	switch (orderType)
	{
		case ORDERTYPEL_MOVE:
			BotReceiveMoveToOrder(pBot, destination);
			break;
		case ORDERTYPET_BUILD:
			BotReceiveBuildOrder(pBot, TargetType, destination);
			break;
		case ORDERTYPET_ATTACK:
			BotReceiveAttackOrder(pBot, TargetType, destination);
			break;
		case ORDERTYPET_GUARD:
			BotReceiveGuardOrder(pBot, TargetType, destination);
			break;
		case ORDERTYPET_WELD:
			BotReceiveWeldOrder(pBot, TargetType, destination);
			break;
	}

	pBot->PrimaryBotTask.bOrderIsUrgent = UTIL_IsTaskUrgent(pBot, &pBot->PrimaryBotTask);
	pBot->PrimaryBotTask.bIssuedByCommander = true;
	pBot->PrimaryBotTask.TaskStartedTime = gpGlobals->time;
}

void BotReceiveAttackOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination)
{
	NSStructureType StructType = UTIL_IUSER3ToStructureType(TargetType);

	if (StructType != STRUCTURE_NONE)
	{
		edict_t* NearestStructure = UTIL_GetNearestStructureIndexOfType(destination, StructType, UTIL_MetresToGoldSrcUnits(2.0f), false);

		if (NearestStructure)
		{
			pBot->PrimaryBotTask.TaskType = TASK_ATTACK;
			pBot->PrimaryBotTask.TaskLocation = UTIL_GetFloorUnderEntity(NearestStructure);
			pBot->PrimaryBotTask.TaskTarget = NearestStructure;
			pBot->PrimaryBotTask.bTargetIsPlayer = false;

		}
		else
		{
			return;
		}
	}
	else
	{
		edict_t* NearestEnemy = NULL;
		float MinDist = 0.0f;

		for (int i = 0; i < 32; i++)
		{
			if (clients[i] && IsPlayerOnAlienTeam(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
			{
				float Dist = vDist2DSq(clients[i]->v.origin, destination);

				if (!NearestEnemy || Dist < MinDist)
				{
					NearestEnemy = clients[i];
					MinDist = Dist;
				}
			}
		}

		if (NearestEnemy)
		{
			pBot->PrimaryBotTask.TaskType = TASK_ATTACK;
			pBot->PrimaryBotTask.TaskTarget = NearestEnemy;
			pBot->PrimaryBotTask.bTargetIsPlayer = true;
		}
		else
		{
			return;
		}

	}

}

void BotReceiveBuildOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination)
{
	NSStructureType StructType = UTIL_IUSER3ToStructureType(TargetType);

	edict_t* NearestStructure = UTIL_GetNearestStructureIndexOfType(destination, StructType, UTIL_MetresToGoldSrcUnits(2.0f), false);

	if (NearestStructure)
	{
		pBot->PrimaryBotTask.TaskType = TASK_BUILD;
		pBot->PrimaryBotTask.TaskLocation = UTIL_GetFloorUnderEntity(NearestStructure);
		pBot->PrimaryBotTask.TaskTarget = NearestStructure;
		pBot->PrimaryBotTask.bIssuedByCommander = true;
		pBot->PrimaryBotTask.TaskStartedTime = gpGlobals->time;
		pBot->PrimaryBotTask.bTargetIsPlayer = false;
	}
}

void BotReceiveMoveToOrder(bot_t* pBot, Vector destination)
{
	const resource_node* ResNodeRef = UTIL_FindEligibleResNodeClosestToLocation(destination, pBot->bot_team, true);

	if (ResNodeRef && vDist2DSq(ResNodeRef->origin, destination) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		pBot->PrimaryBotTask.TaskType = TASK_CAP_RESNODE;
		pBot->PrimaryBotTask.StructureType = STRUCTURE_MARINE_RESTOWER;
	}
	else
	{
		pBot->PrimaryBotTask.TaskType = TASK_MOVE;
	}

	
	pBot->PrimaryBotTask.TaskLocation = destination;
	pBot->PrimaryBotTask.bIssuedByCommander = true;
	pBot->PrimaryBotTask.TaskStartedTime = gpGlobals->time;
}

void BotReceiveGuardOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination)
{
	return;

	NSStructureType StructType = UTIL_IUSER3ToStructureType(TargetType);

	if (StructType != STRUCTURE_NONE)
	{
		edict_t* NearestStructure = UTIL_GetNearestStructureIndexOfType(destination, StructType, UTIL_MetresToGoldSrcUnits(2.0f), false);

		if (NearestStructure)
		{
			pBot->PrimaryBotTask.TaskType = TASK_GUARD;
			pBot->PrimaryBotTask.TaskLocation = NearestStructure->v.origin;
			pBot->PrimaryBotTask.TaskTarget = NearestStructure;
			pBot->PrimaryBotTask.bTargetIsPlayer = false;

		}
		else
		{
			return;
		}
	}
	else
	{
		edict_t* NearestEnemy = NULL;
		float MinDist = 0.0f;

		for (int i = 0; i < 32; i++)
		{
			if (clients[i] && IsPlayerOnAlienTeam(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
			{
				float Dist = vDist2DSq(clients[i]->v.origin, destination);

				if (!NearestEnemy || Dist < MinDist)
				{
					NearestEnemy = clients[i];
					MinDist = Dist;
				}
			}
		}

		if (NearestEnemy)
		{
			pBot->PrimaryBotTask.TaskType = TASK_GUARD;
			pBot->PrimaryBotTask.TaskTarget = NearestEnemy;
			pBot->PrimaryBotTask.bTargetIsPlayer = true;
		}
		else
		{
			return;
		}

	}
}

void BotReceiveWeldOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination)
{
	NSStructureType StructType = UTIL_IUSER3ToStructureType(TargetType);

	if (StructType != STRUCTURE_NONE)
	{
		edict_t* NearestStructure = UTIL_GetNearestStructureIndexOfType(destination, StructType, UTIL_MetresToGoldSrcUnits(2.0f), false);

		if (NearestStructure)
		{
			pBot->PrimaryBotTask.TaskType = TASK_WELD;
			pBot->PrimaryBotTask.TaskLocation = UTIL_GetFloorUnderEntity(NearestStructure);
			pBot->PrimaryBotTask.TaskTarget = NearestStructure;
			pBot->PrimaryBotTask.bTargetIsPlayer = false;

		}
		else
		{
			return;
		}
	}
	else
	{
		edict_t* NearestEnemy = NULL;
		float MinDist = 0.0f;

		for (int i = 0; i < 32; i++)
		{
			if (clients[i] && IsPlayerOnAlienTeam(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
			{
				float Dist = vDist2DSq(clients[i]->v.origin, destination);

				if (!NearestEnemy || Dist < MinDist)
				{
					NearestEnemy = clients[i];
					MinDist = Dist;
				}
			}
		}

		if (!FNullEnt(NearestEnemy))
		{
			pBot->PrimaryBotTask.TaskType = TASK_WELD;
			pBot->PrimaryBotTask.TaskTarget = NearestEnemy;
			pBot->PrimaryBotTask.bTargetIsPlayer = true;
		}
		else
		{
			return;
		}

	}
}

float UTIL_GetPlayerHeight(const edict_t* Player, const bool bIsCrouching)
{
	if (FNullEnt(Player)) { return 0.0f; }

	return UTIL_OriginOffsetFromFloor(Player, bIsCrouching).z * 2.0f;
}

Vector UTIL_OriginOffsetFromFloor(const edict_t* pEdict, const bool bIsCrouching)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	int iuser3 = pEdict->v.iuser3;

	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? Vector(0.0f, 0.0f, 18.0f) : Vector(0.0f, 0.0f, 36.0f);
		break;
	case AVH_USER3_COMMANDER_PLAYER:
		return Vector(0.0f, 0.0f, 36.0f);
		break;
	case AVH_USER3_ALIEN_EMBRYO:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER1:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER2:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER3:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? Vector(0.0f, 0.0f, 18.0f) : Vector(0.0f, 0.0f, 36.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? Vector(0.0f, 0.0f, 36.0f) : Vector(0.0f, 0.0f, 54.0f);
		break;
	default:
		return Vector(0.0f, 0.0f, 36.0f);
		break;
	}
}

Vector UTIL_GetBottomOfCollisionHull(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	int iuser3 = pEdict->v.iuser3;
	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);
	Vector origin = pEdict->v.origin;


	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 18.0f)) : (origin - Vector(0.0f, 0.0f, 36.0f));
		break;
	case AVH_USER3_COMMANDER_PLAYER:
		return origin;
		break;
	case AVH_USER3_ALIEN_EMBRYO:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER1:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER2:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER3:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 18.0f)) : (origin - Vector(0.0f, 0.0f, 36.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 36.0f)) : (origin - Vector(0.0f, 0.0f, 54.0f));
		break;
	default:
		return origin;
		break;
	}
}

Vector UTIL_GetTopOfCollisionHull(const edict_t* pEdict, const bool bIsCrouching)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	int iuser3 = pEdict->v.iuser3;
	Vector origin = pEdict->v.origin;


	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_COMMANDER_PLAYER:
		return origin;
	case AVH_USER3_ALIEN_EMBRYO:
	case AVH_USER3_ALIEN_PLAYER1:
	case AVH_USER3_ALIEN_PLAYER2:
	case AVH_USER3_ALIEN_PLAYER3:
		return (origin + Vector(0.0f, 0.0f, 19.0f));
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 37.0f)) : (origin + Vector(0.0f, 0.0f, 55.0f));
	default:
		return origin;
	}
}

Vector UTIL_GetTopOfCollisionHull(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	int iuser3 = pEdict->v.iuser3;
	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);
	Vector origin = pEdict->v.origin;


	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_COMMANDER_PLAYER:
		return origin;
	case AVH_USER3_ALIEN_EMBRYO:
	case AVH_USER3_ALIEN_PLAYER1:
	case AVH_USER3_ALIEN_PLAYER2:
	case AVH_USER3_ALIEN_PLAYER3:
		return (origin + Vector(0.0f, 0.0f, 19.0f));
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 37.0f)) : (origin + Vector(0.0f, 0.0f, 55.0f));
	default:
		return origin;
	}
}

float UTIL_GetPlayerRadius(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return 0.0f; }

	int hullnum = GetPlayerHullIndex(pEdict);

	switch (hullnum)
	{
		case human_hull:
		case head_hull:
			return 16.0f;
			break;
		case large_hull:
			return 32.0f;
			break;
		default:
			return 16.0f;
			break;

	}
}

int GetPlayerHullIndex(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return 0; }

	NSPlayerClass PlayerClass = UTIL_GetPlayerClass(pEdict);

	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);

	switch (PlayerClass)
	{
	case CLASS_MARINE:
		return (bIsCrouching) ? head_hull : human_hull;
	case CLASS_MARINE_COMMANDER:
		return head_hull;
	case CLASS_EGG:
		return head_hull;
	case CLASS_SKULK:
		return head_hull;
	case CLASS_GORGE:
		return head_hull;
	case CLASS_LERK:
		return head_hull;
	case CLASS_FADE:
		return (bIsCrouching) ? head_hull : human_hull;
	case CLASS_ONOS:
		return (bIsCrouching) ? human_hull : large_hull;
	default:
		return head_hull;
	}

	return head_hull;
}

bool IsPlayerOnLadder(const edict_t* Player)
{
	return (Player->v.movetype == MOVETYPE_FLY);
}

bool IsPlayerDead(const edict_t* Player)
{
	if (FNullEnt(Player)) { return true; }
	return (Player->v.deadflag != DEAD_NO);
}

bool IsPlayerBeingDigested(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_DIGESTING && Player->v.effects & EF_NODRAW);
}

bool IsPlayerDigesting(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_DIGESTING && !(Player->v.effects & EF_NODRAW));
}

bool IsPlayerGestating(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_ALIEN_EMBRYO);
}

bool IsPlayerCharging(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_ALIEN_MOVEMENT);
}

bool IsPlayerOnMarineTeam(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player && (Player->v.team == MARINE_TEAM) && !IsPlayerInReadyRoom(Player) && !IsPlayerSpectator(Player));
}

bool IsPlayerOnAlienTeam(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.team == ALIEN_TEAM && !IsPlayerInReadyRoom(Player) && !IsPlayerSpectator(Player));
}

void BotAlienSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	switch (pBot->CurrentRole)
	{
	case BOT_ROLE_HARASS:
		AlienHarasserSetPrimaryTask(pBot, Task);
		break;
	case BOT_ROLE_FADE:
	{
		if (UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER) < 3 && !IsPlayerFade(pBot->pEdict))
		{
			AlienCapperSetPrimaryTask(pBot, Task);
		}
		else
		{
			AlienHarasserSetPrimaryTask(pBot, Task);
		}
		
	}
	break;
	case BOT_ROLE_ONOS:
	{
		if (UTIL_GetNumActiveHives() < 2 && !IsPlayerOnos(pBot->pEdict))
		{
			AlienCapperSetPrimaryTask(pBot, Task);
		}
		else
		{
			AlienHarasserSetPrimaryTask(pBot, Task);
		}
		
	}
	break;
	case BOT_ROLE_RES_CAPPER:
		AlienCapperSetPrimaryTask(pBot, Task);
		break;
	case BOT_ROLE_BUILDER:
		AlienBuilderSetPrimaryTask(pBot, Task);
		break;
	default:
		break;
	}
}

void BotAlienSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	switch (pBot->CurrentRole)
	{
	case BOT_ROLE_HARASS:
	case BOT_ROLE_FADE:
	case BOT_ROLE_ONOS:
	{
		if (UTIL_GetNumActiveHives() < 2 && (IsPlayerSkulk(pBot->pEdict) || IsPlayerGorge(pBot->pEdict)))
		{
			AlienCapperSetSecondaryTask(pBot, &pBot->PrimaryBotTask);
		}
		else
		{
			AlienHarasserSetSecondaryTask(pBot, &pBot->PrimaryBotTask);
		}
		break;
	}
	case BOT_ROLE_RES_CAPPER:
		AlienCapperSetSecondaryTask(pBot, &pBot->PrimaryBotTask);
		break;
	case BOT_ROLE_BUILDER:
		AlienBuilderSetSecondaryTask(pBot, &pBot->PrimaryBotTask);
		break;
	default:
		break;
	}
}

enemy_status* UTIL_GetTrackedEnemyRefForTarget(bot_t* pBot, edict_t* Target)
{
	for (int i = 0; i < 32; i++)
	{
		if (pBot->TrackedEnemies[i].EnemyEdict == Target)
		{
			return &pBot->TrackedEnemies[i];
		}
	}

	return nullptr;
}

void AlienThink(bot_t* pBot)
{
	if (pBot->CurrentEnemy > -1 && pBot->CurrentEnemy < 32)
	{
		edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

		if (!FNullEnt(CurrentEnemy) && !IsPlayerDead(CurrentEnemy))
		{
			switch (pBot->bot_ns_class)
			{
			case CLASS_SKULK:
				SkulkCombatThink(pBot);
				return;
			case CLASS_GORGE:
				GorgeCombatThink(pBot);
				return;
			case CLASS_LERK:
				return;
			case CLASS_FADE:
				FadeCombatThink(pBot);
				return;
			case CLASS_ONOS:
				OnosCombatThink(pBot);
				return;
			default:
				return;
			}
		}
	}

	edict_t* DangerTurret = BotGetNearestDangerTurret(pBot, UTIL_MetresToGoldSrcUnits(15.0f));

	if (!FNullEnt(DangerTurret) && UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, DangerTurret->v.origin))
	{
		if (pBot->SecondaryBotTask.TaskTarget != DangerTurret)
		{
			pBot->SecondaryBotTask.TaskType = TASK_ATTACK;
			pBot->SecondaryBotTask.TaskTarget = DangerTurret;
			pBot->SecondaryBotTask.TaskLocation = DangerTurret->v.origin;
			pBot->SecondaryBotTask.bOrderIsUrgent = true;
		}
	}

	// This should ensure a decent spread of roles regardless of bot count. Bots will do the less desireable roles first.
	if (pBot->CurrentRole == BOT_ROLE_NONE)
	{
		int NumCappers = UTIL_GetBotsWithRoleType(BOT_ROLE_RES_CAPPER, false);
		int NumBuilders = UTIL_GetBotsWithRoleType(BOT_ROLE_BUILDER, false);
		int NumFades = UTIL_GetBotsWithRoleType(BOT_ROLE_FADE, false);
		int NumOnos = UTIL_GetBotsWithRoleType(BOT_ROLE_ONOS, false);

		if (NumCappers < 1)
		{
			pBot->CurrentRole = BOT_ROLE_RES_CAPPER;
		}
		else if (NumBuilders < 1)
		{
			pBot->CurrentRole = BOT_ROLE_BUILDER;
		}
		else if (NumFades < 1)
		{
			pBot->CurrentRole = BOT_ROLE_FADE;
		}
		else if (NumBuilders < 2)
		{
			pBot->CurrentRole = BOT_ROLE_BUILDER;
		}
		else if (NumOnos < 1)
		{
			pBot->CurrentRole = BOT_ROLE_ONOS;
		}
		else
		{
			pBot->CurrentRole = BOT_ROLE_FADE;
		}		
	}

	if (!pBot->CurrentTask) { pBot->CurrentTask = &pBot->PrimaryBotTask; }

	UpdateAndClearTasks(pBot);

	AlienCheckWantsAndNeeds(pBot);

	if (pBot->PrimaryBotTask.TaskType == TASK_NONE)
	{
		BotAlienSetPrimaryTask(pBot, &pBot->PrimaryBotTask);
	}

	if (!pBot->PrimaryBotTask.bOrderIsUrgent && (pBot->SecondaryBotTask.TaskType == TASK_NONE || !pBot->SecondaryBotTask.bOrderIsUrgent))
	{
		BotAlienCheckPriorityTargets(pBot, &pBot->PendingTask);

		if (pBot->PendingTask.TaskType == TASK_NONE)
		{
			BotAlienCheckDefendTargets(pBot, &pBot->PendingTask);
		}

		if (pBot->PendingTask.TaskType != TASK_NONE)
		{
			memcpy(&pBot->SecondaryBotTask, &pBot->PendingTask, sizeof(bot_task));
		}
		else
		{
			BotAlienSetSecondaryTask(pBot, &pBot->SecondaryBotTask);
		}
	}

	pBot->CurrentTask = BotGetNextTask(pBot);

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}
}

void BotAlienCheckDefendTargets(bot_t* pBot, bot_task* Task)
{
	UTIL_ClearBotTask(pBot, Task);

	if (IsPlayerGorge(pBot->pEdict)) { return; }

	edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_RESTOWER);

	if (!FNullEnt(ResourceTower))
	{
		Task->TaskType = TASK_DEFEND;
		Task->TaskTarget = ResourceTower;
		Task->TaskLocation = ResourceTower->v.origin;
		Task->bOrderIsUrgent = true;

		return;
	}
}

void BotAlienCheckPriorityTargets(bot_t* pBot, bot_task* Task)
{
	UTIL_ClearBotTask(pBot, Task);

	if (IsPlayerGorge(pBot->pEdict) && !BotHasWeapon(pBot, WEAPON_GORGE_BILEBOMB))
	{
		return;
	}

	bool bAllowElectrified = !IsPlayerSkulk(pBot->pEdict);

	edict_t* PhaseGate = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), bAllowElectrified);

	if (!FNullEnt(PhaseGate))
	{
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = PhaseGate;
		Task->TaskLocation = PhaseGate->v.origin;
		Task->bOrderIsUrgent = true;

		return;
	}

	edict_t* TurretFactory = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), bAllowElectrified);

	if (!FNullEnt(TurretFactory))
	{
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = TurretFactory;
		Task->TaskLocation = TurretFactory->v.origin;
		Task->bOrderIsUrgent = true;

		return;
	}

	edict_t* SiegeTurret = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_SIEGETURRET, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), bAllowElectrified);

	if (!FNullEnt(SiegeTurret))
	{
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = SiegeTurret;
		Task->TaskLocation = SiegeTurret->v.origin;
		Task->bOrderIsUrgent = true;

		return;
	}
}

bool UTIL_IsAlienEvolveTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task || !Task->Evolution) { return false; }

	switch (Task->Evolution)
	{
	case IMPULSE_ALIEN_EVOLVE_FADE:
		return !IsPlayerFade(pBot->pEdict) && pBot->resources >= kFadeEvolutionCost;
	case IMPULSE_ALIEN_EVOLVE_ONOS:
		return !IsPlayerOnos(pBot->pEdict) && pBot->resources >= kOnosEvolutionCost;
	case IMPULSE_ALIEN_EVOLVE_LERK:
		return !IsPlayerLerk(pBot->pEdict) && pBot->resources >= kLerkEvolutionCost;
	case IMPULSE_ALIEN_EVOLVE_GORGE:
		return !IsPlayerGorge(pBot->pEdict) && pBot->resources >= kGorgeEvolutionCost;
	case IMPULSE_ALIEN_EVOLVE_SKULK:
		return !IsPlayerSkulk(pBot->pEdict);
	default:
		return false;
	}

	return false;
}

void AlienProgressEvolveTask(bot_t* pBot, bot_task* Task)
{
	if (!Task || !Task->Evolution) { return; }

	switch (Task->Evolution)
	{
		case IMPULSE_ALIEN_EVOLVE_LERK:
		case IMPULSE_ALIEN_EVOLVE_FADE:
		case IMPULSE_ALIEN_EVOLVE_ONOS:
		{
			const hive_definition* NearestHive = UTIL_GetNearestHiveOfStatus(pBot->pEdict->v.origin, HIVE_STATUS_BUILT);
			
			if (NearestHive)
			{
				if (vDist2DSq(pBot->pEdict->v.origin, NearestHive->Location) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
				{
					MoveTo(pBot, NearestHive->FloorLocation, MOVESTYLE_NORMAL);
					return;
				}
				else
				{
					pBot->pEdict->v.impulse = Task->Evolution;
				}
			}
			else
			{
				pBot->pEdict->v.impulse = Task->Evolution;
			}
		}
		break;
		default:
			pBot->pEdict->v.impulse = Task->Evolution;
	}
}

void SkulkCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0 || pBot->CurrentEnemy > 31) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	if (FNullEnt(CurrentEnemy) || !TrackedEnemyRef || IsPlayerDead(CurrentEnemy)) { return; }

	if (!TrackedEnemyRef->bCurrentlyVisible)
	{
		Vector EnemyLoc = (TrackedEnemyRef->bIsTracked) ? TrackedEnemyRef->TrackedLocation : TrackedEnemyRef->LastSeenLocation;

		MoveTo(pBot, EnemyLoc, MOVESTYLE_AMBUSH);

		LookAt(pBot, EnemyLoc);

		return;
	}

	NSWeapon DesiredCombatWeapon = SkulkGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	float CurrentDistance = vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin);
	float WeaponMaxDistance = sqrf(UTIL_GetMaxIdealWeaponRange(DesiredCombatWeapon));

	if (CurrentDistance > WeaponMaxDistance)
	{
		LookAt(pBot, CurrentEnemy);
		MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_AMBUSH);

		// We check that by leaping, we won't leave ourselves without enough energy to perform our main attack.

		float CombatWeaponEnergyCost = GetEnergyCostForWeapon(pBot->DesiredCombatWeapon);
		float RequiredEnergy = (CombatWeaponEnergyCost + GetLeapCost(pBot)) - (GetPlayerEnergyRegenPerSecond(pEdict) * 0.5f); // We allow for around .5s of regen time as well

		if (GetPlayerEnergyPercentage(pEdict) >= RequiredEnergy)
		{
			if (UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, CurrentEnemy->v.origin))
			{
				BotLeap(pBot, CurrentEnemy->v.origin);
			}
		}

		return;
	}

	pBot->DesiredCombatWeapon = DesiredCombatWeapon;

	if (UTIL_GetBotCurrentWeapon(pBot) != DesiredCombatWeapon)
	{
		return;
	}

	if (UTIL_GetBotCurrentWeapon(pBot) == WEAPON_SKULK_BITE)
	{
		Vector TargetLocation = UTIL_GetFloorUnderEntity(CurrentEnemy);
		Vector BehindPlayer = TargetLocation - (UTIL_GetForwardVector2D(CurrentEnemy->v.v_angle) * 50.0f);

		int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

		if (UTIL_PointIsReachable(NavProfileIndex, pBot->CurrentFloorPosition, BehindPlayer, 0.0f))
		{
			MoveTo(pBot, BehindPlayer, MOVESTYLE_NORMAL);
		}
		else
		{
			if (UTIL_PointIsReachable(NavProfileIndex, pBot->CurrentFloorPosition, TargetLocation, 50.0f))
			{
				MoveTo(pBot, TargetLocation, MOVESTYLE_NORMAL);
			}
		}

		BotAttackTarget(pBot, CurrentEnemy);

		return;
	}

	MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);
	BotAttackTarget(pBot, CurrentEnemy);
	return;
}

void FadeCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0 || pBot->CurrentEnemy > 31) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy)) { return; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	bool bLowOnHealth = ((pEdict->v.health / pEdict->v.max_health) < 0.5f);

	// Run away if low on health
	if (bLowOnHealth)
	{
		edict_t* NearestHealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);

		if (!FNullEnt(NearestHealingSource))
		{
			float DesiredDist = (UTIL_IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(10.0f);

			if (vDist2DSq(pBot->pEdict->v.origin, NearestHealingSource->v.origin) > sqrf(DesiredDist))
			{
				MoveTo(pBot, NearestHealingSource->v.origin, MOVESTYLE_HIDE);

				if ((pEdict->v.health / pEdict->v.max_health) < 0.3f)
				{

					if (BotHasWeapon(pBot, WEAPON_FADE_METABOLIZE))
					{
						if (UTIL_GetBotCurrentWeapon(pBot) != WEAPON_FADE_METABOLIZE)
						{
							pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;
						}
						else
						{
							pEdict->v.button |= IN_ATTACK;
						}
					}
				}
				return;
			}
			else
			{
				if (TrackedEnemyRef->bCurrentlyVisible)
				{
					if (BotHasWeapon(pBot, WEAPON_FADE_ACIDROCKET))
					{
						pBot->DesiredCombatWeapon = WEAPON_FADE_ACIDROCKET;
					}
					else
					{
						pBot->DesiredCombatWeapon = WEAPON_FADE_SWIPE;
					}

					if (UTIL_GetBotCurrentWeapon(pBot) != pBot->DesiredCombatWeapon)
					{
						return;
					}


					if (UTIL_GetBotCurrentWeapon(pBot) == WEAPON_FADE_SWIPE)
					{
						if (vDist2DSq(CurrentEnemy->v.origin, NearestHealingSource->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
						{
							MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_HIDE);
						}
					}

					BotAttackTarget(pBot, CurrentEnemy);
				}
			}

			return;
		}
		
		if (!UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, CurrentEnemy->v.origin))
		{
			if (BotHasWeapon(pBot, WEAPON_FADE_METABOLIZE))
			{
				if (UTIL_GetBotCurrentWeapon(pBot) != WEAPON_FADE_METABOLIZE)
				{
					pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;
				}
				else
				{
					pEdict->v.button |= IN_ATTACK;
				}
			}

			return;
		}

		
	}

	// If the enemy is not visible
	if (!TrackedEnemyRef->bCurrentlyVisible && !UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, CurrentEnemy->v.origin))
	{
		Vector EnemyLoc = (TrackedEnemyRef->bIsTracked) ? TrackedEnemyRef->TrackedLocation : TrackedEnemyRef->LastSeenLocation;

		MoveTo(pBot, EnemyLoc, MOVESTYLE_NORMAL);

		LookAt(pBot, EnemyLoc);

		return;
	}

	NSWeapon DesiredCombatWeapon = FadeGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	float CurrentDistance = vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin);
	float WeaponMaxDistance = sqrf(UTIL_GetMaxIdealWeaponRange(DesiredCombatWeapon));

	if (CurrentDistance > WeaponMaxDistance)
	{
		MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);
		if (UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, CurrentEnemy->v.origin))
		{
			BotLeap(pBot, CurrentEnemy->v.origin);
		}
		
		return;
	}

	pBot->DesiredCombatWeapon = DesiredCombatWeapon;

	if (UTIL_GetBotCurrentWeapon(pBot) != DesiredCombatWeapon)
	{
		return;
	}

	if (UTIL_GetBotCurrentWeapon(pBot) == WEAPON_FADE_SWIPE)
	{
		Vector TargetLocation = UTIL_GetFloorUnderEntity(CurrentEnemy);
		Vector BehindPlayer = TargetLocation - (UTIL_GetForwardVector2D(CurrentEnemy->v.v_angle) * 50.0f);

		int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

		if (UTIL_PointIsReachable(NavProfileIndex, pBot->CurrentFloorPosition, BehindPlayer, 0.0f))
		{
			MoveTo(pBot, BehindPlayer, MOVESTYLE_NORMAL);
		}
		else
		{
			if (UTIL_PointIsReachable(NavProfileIndex, pBot->CurrentFloorPosition, TargetLocation, 50.0f))
			{
				MoveTo(pBot, TargetLocation, MOVESTYLE_NORMAL);
			}
		}

		BotAttackTarget(pBot, CurrentEnemy);

		return;
	}
	else
	{

		float MinWeaponDistance = UTIL_GetMinIdealWeaponRange(DesiredCombatWeapon);

		Vector EngagementLocation = pBot->BotNavInfo.TargetDestination;

		float EngagementLocationDist = vDist2DSq(EngagementLocation, CurrentEnemy->v.origin);

		if (!EngagementLocation || EngagementLocationDist > WeaponMaxDistance || EngagementLocationDist < MinWeaponDistance || !UTIL_QuickTrace(pBot->pEdict, EngagementLocation, CurrentEnemy->v.origin))
		{
			int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
			EngagementLocation = UTIL_GetRandomPointOnNavmeshInRadius(MoveProfile, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(2.0f));

			if (EngagementLocation != ZERO_VECTOR && EngagementLocationDist < WeaponMaxDistance || EngagementLocationDist > MinWeaponDistance && UTIL_QuickTrace(pBot->pEdict, EngagementLocation, CurrentEnemy->v.origin))
			{
				MoveTo(pBot, EngagementLocation, MOVESTYLE_NORMAL);
			}
		}
		else
		{
			MoveTo(pBot, EngagementLocation, MOVESTYLE_NORMAL);
		}

		BotAttackTarget(pBot, CurrentEnemy);
	}

}

NSWeapon SkulkGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || IsPlayerDead(Target))
	{
		return WEAPON_SKULK_BITE;
	}
	
	if (BotHasWeapon(pBot, WEAPON_SKULK_XENOCIDE))
	{
		int NumTargetsInArea = UTIL_GetNumPlayersOfTeamInArea(Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), MARINE_TEAM, NULL, CLASS_NONE);

		NumTargetsInArea += UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_ANYTURRET, Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		NumTargetsInArea += UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_PHASEGATE, Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumTargetsInArea > 2)
		{
			return WEAPON_SKULK_XENOCIDE;
		}
	}

	float DistFromTarget = vDist2DSq(pBot->pEdict->v.origin, Target->v.origin);

	if (!IsPlayerParasited(Target) && DistFromTarget >= sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		return WEAPON_SKULK_PARASITE;
	}

	return WEAPON_SKULK_BITE;

}

NSWeapon OnosGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || IsPlayerDead(Target))
	{
		return WEAPON_ONOS_GORE;
	}

	float DistFromTarget = vDist2DSq(pBot->pEdict->v.origin, Target->v.origin);

	if (DistFromTarget > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		if (BotHasWeapon(pBot, WEAPON_ONOS_CHARGE) && UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, Target->v.origin))
		{
			return WEAPON_ONOS_CHARGE;
		}

		return WEAPON_ONOS_GORE;
	}

	if (BotHasWeapon(pBot, WEAPON_ONOS_STOMP) && !IsPlayerStunned(Target) && DistFromTarget > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)) && DistFromTarget < sqrf(UTIL_MetresToGoldSrcUnits(8.0f)))
	{
		return WEAPON_ONOS_STOMP;
	}

	if (!IsPlayerDigesting(pBot->pEdict) && DistFromTarget < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
	{
		return WEAPON_ONOS_DEVOUR;
	}

	return WEAPON_ONOS_GORE;
}

NSWeapon FadeGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || IsPlayerDead(Target))
	{
		return WEAPON_FADE_SWIPE;
	}

	if (!BotHasWeapon(pBot, WEAPON_FADE_ACIDROCKET))
	{
		return WEAPON_FADE_SWIPE;
	}

	float DistFromTarget = vDist2DSq(pBot->pEdict->v.origin, Target->v.origin);

	int NumEnemyAllies = UTIL_GetNumPlayersOfTeamInArea(Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), MARINE_TEAM, NULL, CLASS_NONE);

	if (NumEnemyAllies > 2)
	{
		return WEAPON_FADE_ACIDROCKET;
	}

	if (PlayerHasWeapon(Target, WEAPON_MARINE_SHOTGUN))
	{
		if (DistFromTarget > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			return WEAPON_FADE_ACIDROCKET;
		}
	}

	return WEAPON_FADE_SWIPE;

}

void OnosCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy)) { return; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	bool bLowOnHealth = ((pEdict->v.health / pEdict->v.max_health) < 0.33f);

	if (bLowOnHealth)
	{
		const hive_definition* NearestHive = UTIL_GetNearestHiveOfStatus(pEdict->v.origin, HIVE_STATUS_BUILT);

		if (NearestHive)
		{
			if (vDist2DSq(pBot->pEdict->v.origin, NearestHive->FloorLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				MoveTo(pBot, NearestHive->FloorLocation, MOVESTYLE_NORMAL);
			}
		}

		if (BotHasWeapon(pBot, WEAPON_ONOS_CHARGE))
		{
			if (UTIL_GetBotCurrentWeapon(pBot) != WEAPON_ONOS_CHARGE)
			{
				pBot->DesiredCombatWeapon = WEAPON_ONOS_CHARGE;
			}
			else
			{
				// Only charge if it will leave us with enough energy to stomp after, otherwise Onos charges in and then can't do anything

				

				float RequiredEnergy = (kStompEnergyCost + GetLeapCost(pBot)) - (GetPlayerEnergyRegenPerSecond(pEdict) * 0.5f); // We allow for around .5s of regen time as well

				if (GetPlayerEnergyPercentage(pEdict) >= RequiredEnergy)
				{
					if (UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, CurrentEnemy->v.origin))
					{
						pEdict->v.button |= IN_ATTACK2;
					}
				}
				
			}
		}

		return;
	}

	if (!TrackedEnemyRef->bCurrentlyVisible)
	{
		Vector EnemyLoc = (TrackedEnemyRef->bIsTracked) ? TrackedEnemyRef->TrackedLocation : TrackedEnemyRef->LastSeenLocation;

		MoveTo(pBot, EnemyLoc, MOVESTYLE_NORMAL);

		LookAt(pBot, EnemyLoc);

		return;
	}

	LookAt(pBot, CurrentEnemy);

	NSWeapon DesiredCombatWeapon = OnosGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	float CurrentDistance = vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin);
	float WeaponMaxDistance = sqrf(UTIL_GetMaxIdealWeaponRange(DesiredCombatWeapon));

	if (CurrentDistance > WeaponMaxDistance)
	{
		MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);

		return;
	}

	pBot->DesiredCombatWeapon = DesiredCombatWeapon;

	if (UTIL_GetBotCurrentWeapon(pBot) != DesiredCombatWeapon)
	{
		return;
	}

	MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);
	BotAttackTarget(pBot, CurrentEnemy);
	return;
}

float GetPlayerEnergyRegenPerSecond(edict_t* Player)
{
	int AdrenalineLevel = 0;

	if (Player->v.iuser4 & MASK_UPGRADE_5)
	{
		AdrenalineLevel = 1;

		if (Player->v.iuser4 & MASK_UPGRADE_13)
		{
			AdrenalineLevel = 3;
		}
		else if (Player->v.iuser4 & MASK_UPGRADE_12)
		{
			AdrenalineLevel = 2;
		}
	}

	return kAlienEnergyRate * (1.0f + (AdrenalineLevel * kAdrenalineEnergyPercentPerLevel));
}



void AlienHarasserSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerGorge(pBot->pEdict))
	{
		BotEvolveLifeform(pBot, CLASS_SKULK);
	}

	int NumMarineTowers = UTIL_GetStructureCountOfType(STRUCTURE_MARINE_RESTOWER);
	int NumAlienTowers = UTIL_GetStructureCountOfType(STRUCTURE_ALIEN_RESTOWER);

	if (NumMarineTowers > 2)
	{
		const resource_node* ResNode = UTIL_GetNearestCappedResNodeToLocation(pBot->pEdict->v.origin, MARINE_TEAM, !IsPlayerSkulk(pBot->pEdict));

		if (ResNode)
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskLocation = ResNode->origin;
			Task->TaskTarget = ResNode->TowerEdict;
			Task->bOrderIsUrgent = false;
			return;
		}
	}

	if (NumAlienTowers >= 3)
	{
		int NumInfantryPortals = UTIL_GetStructureCountOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

		if (NumInfantryPortals > 0)
		{
			edict_t* InfPortal = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

			if (InfPortal)
			{
				Task->TaskType = TASK_ATTACK;
				Task->TaskTarget = InfPortal;
				Task->TaskLocation = InfPortal->v.origin;
				Task->bOrderIsUrgent = false;
				return;
			}
		}
			
		edict_t* CommChair = UTIL_GetCommChair();

		if (CommChair)
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = CommChair;
			Task->TaskLocation = CommChair->v.origin;
			Task->bOrderIsUrgent = false;
			return;
		}

		edict_t* EnemyPlayer = UTIL_GetNearestPlayerOfTeamInArea(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(100.0f), MARINE_TEAM, nullptr, CLASS_NONE);

		if (!FNullEnt(EnemyPlayer))
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = EnemyPlayer;
			Task->TaskLocation = EnemyPlayer->v.origin;
			Task->bTargetIsPlayer = true;
			return;
		}
			
	}

	const resource_node* ResNode = UTIL_FindEligibleResNodeClosestToLocation(pBot->pEdict->v.origin, ALIEN_TEAM, !IsPlayerSkulk(pBot->pEdict));

	if (ResNode)
	{
		if (ResNode->bIsOccupied && !FNullEnt(ResNode->TowerEdict))
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskLocation = ResNode->origin;
			Task->TaskTarget = ResNode->TowerEdict;
			Task->bOrderIsUrgent = false;
			return;
		}
		else
		{
			Task->TaskType = TASK_MOVE;
			Task->TaskLocation = ResNode->origin;
			Task->bOrderIsUrgent = false;
			return;
		}
	}
	else
	{
		Task->TaskType = TASK_MOVE;
		int NavProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(NavProfile, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(50.0f));
		Task->bOrderIsUrgent = false;
		return;
	}
		
}

void AlienCapperSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	int RequiredRes = kResourceTowerCost;

	if (!IsPlayerGorge(pBot->pEdict))
	{
		RequiredRes += kGorgeEvolutionCost;
	}

	if (pBot->resources < RequiredRes)
	{
		if (IsPlayerGorge(pBot->pEdict))
		{
			BotEvolveLifeform(pBot, CLASS_SKULK);
			return;
		}

		Vector RandomPoint = UTIL_GetRandomPointOnNavmesh(pBot);

		const resource_node* RandomResNode = UTIL_FindEligibleResNodeClosestToLocation(RandomPoint, ALIEN_TEAM, !IsPlayerSkulk(pBot->pEdict));

		if (RandomResNode)
		{
			if (RandomResNode->bIsOccupied && RandomResNode->bIsOwnedByMarines)
			{
				Task->TaskType = TASK_ATTACK;
				Task->TaskLocation = RandomResNode->origin;
				Task->TaskTarget = RandomResNode->TowerEdict;
				Task->bOrderIsUrgent = false;
				return;
			}
			else
			{
				Task->TaskType = TASK_MOVE;
				Task->TaskLocation = RandomResNode->origin;
				Task->bOrderIsUrgent = false;
				return;
			}
		}

		AlienHarasserSetPrimaryTask(pBot, Task);
		return;
	}

	const resource_node* RandomResNode = nullptr;

	if (!IsPlayerGorge(pBot->pEdict) || BotHasWeapon(pBot, WEAPON_GORGE_BILEBOMB))
	{
		RandomResNode = UTIL_FindEligibleResNodeFurthestFromLocation(UTIL_GetCommChairLocation(), ALIEN_TEAM, !IsPlayerSkulk(pBot->pEdict));
	}
	else
	{
		RandomResNode = UTIL_AlienFindUnclaimedResNodeFurthestFromLocation(pBot, UTIL_GetCommChairLocation(), !IsPlayerSkulk(pBot->pEdict));
	}

	if (RandomResNode)
	{
		Task->TaskType = TASK_CAP_RESNODE;
		Task->TaskLocation = RandomResNode->origin;
		Task->bOrderIsUrgent = true;
		Task->StructureType = STRUCTURE_ALIEN_RESTOWER;
		return;
	}

	AlienHarasserSetPrimaryTask(pBot, Task);
	return;
}

NSDeployableItem UTIL_WeaponTypeToDeployableItem(const NSWeapon WeaponType)
{
	switch (WeaponType)
	{
		case WEAPON_MARINE_SHOTGUN:
			return ITEM_MARINE_SHOTGUN;
		case WEAPON_MARINE_GL:
			return ITEM_MARINE_GRENADELAUNCHER;
		case WEAPON_MARINE_HMG:
			return ITEM_MARINE_HMG;
		case WEAPON_MARINE_WELDER:
			return ITEM_MARINE_WELDER;
		default:
			return ITEM_NONE;
	}

	return ITEM_NONE;
}

void OnBotRestartPlay(bot_t* pBot)
{
	ClearBotPath(pBot);
	pBot->bBotThinkPaused = false;
}

NSStructureType UTIL_GetChamberTypeForHiveTech(const HiveTechStatus HiveTech)
{
	switch (HiveTech)
	{
		case HIVE_TECH_MOVEMENT:
			return STRUCTURE_ALIEN_MOVEMENTCHAMBER;
		case HIVE_TECH_DEFENCE:
			return STRUCTURE_ALIEN_DEFENCECHAMBER;
		case HIVE_TECH_SENSORY:
			return STRUCTURE_ALIEN_SENSORYCHAMBER;
		default:
			return STRUCTURE_NONE;
	}
}

void AlienBuilderSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* pEdict = pBot->pEdict;

	// Next check if we have a minimum number of resource nodes in place

	int NumResourceNodes = UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER);

	if (NumResourceNodes < min_desired_resource_towers)
	{
		const resource_node* FreeResNode = nullptr;

		if (!IsPlayerGorge(pEdict) || BotHasWeapon(pBot, WEAPON_GORGE_BILEBOMB))
		{
			FreeResNode = UTIL_FindEligibleResNodeFurthestFromLocation(UTIL_GetCommChairLocation(), ALIEN_TEAM, !IsPlayerSkulk(pBot->pEdict));
		}
		else
		{
			FreeResNode = UTIL_AlienFindUnclaimedResNodeFurthestFromLocation(pBot, UTIL_GetCommChairLocation(), true);
		}

		if (FreeResNode)
		{
			if (FreeResNode->bIsOccupied)
			{
				Task->TaskType = TASK_CAP_RESNODE;
				Task->TaskLocation = FreeResNode->origin;
				Task->StructureType = STRUCTURE_ALIEN_RESTOWER;
			}
			else
			{
				Task->TaskType = TASK_BUILD;
				Task->TaskLocation = FreeResNode->origin;
				Task->StructureType = STRUCTURE_ALIEN_RESTOWER;
			}

			return;
		}
	}

	NSStructureType TechChamberToBuild = STRUCTURE_NONE;
	const hive_definition* HiveIndex = nullptr;

	HiveTechStatus HiveTechOne = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(0);
	HiveTechStatus HiveTechTwo = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(1);
	HiveTechStatus HiveTechThree = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(2);

	if (UTIL_ActiveHiveWithTechExists(HiveTechOne) && UTIL_GetStructureCountOfType(UTIL_GetChamberTypeForHiveTech(HiveTechOne)) < 3)
	{
		TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechOne);
		HiveIndex = UTIL_GetHiveWithTech(HiveTechOne);
	}
	else if (UTIL_ActiveHiveWithTechExists(HiveTechTwo) && UTIL_GetStructureCountOfType(UTIL_GetChamberTypeForHiveTech(HiveTechTwo)) < 3)
	{
		TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechTwo);
		HiveIndex = UTIL_GetHiveWithTech(HiveTechTwo);
	}
	else if (UTIL_ActiveHiveWithTechExists(HiveTechThree) && UTIL_GetStructureCountOfType(UTIL_GetChamberTypeForHiveTech(HiveTechThree)) < 3)
	{
		TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechThree);
		HiveIndex = UTIL_GetHiveWithTech(HiveTechThree);
	}

	if (HiveIndex && TechChamberToBuild != STRUCTURE_NONE)
	{
		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, HiveIndex->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		Task->TaskType = TASK_BUILD;
		Task->TaskLocation = BuildLocation;
		Task->StructureType = TechChamberToBuild;

		return;
	}

	HiveIndex = UTIL_GetFirstHiveWithoutTech();

	if (HiveIndex)
	{

		if (!UTIL_ActiveHiveWithTechExists(HiveTechOne))
		{
			TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechOne);
		}
		else if (!UTIL_ActiveHiveWithTechExists(HiveTechTwo))
		{
			TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechTwo);
		}
		else
		{
			TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechThree);
		}

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, HiveIndex->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		if (BuildLocation != ZERO_VECTOR)
		{
			Task->TaskType = TASK_BUILD;
			Task->TaskLocation = BuildLocation;
			Task->StructureType = TechChamberToBuild;

			return;
		}
	}

	if (UTIL_GetNumUnbuiltHives() > 0 && !UTIL_BotWithBuildTaskExists(STRUCTURE_ALIEN_HIVE))
	{
		const hive_definition* UnbuiltHiveIndex = UTIL_GetClosestViableUnbuiltHive(pEdict->v.origin);

		if (UnbuiltHiveIndex)
		{
			edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(UnbuiltHiveIndex->Location, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(10.0f), pEdict);

			if (!OtherGorge || GetPlayerResources(OtherGorge) < pBot->resources)
			{
				Task->TaskType = TASK_BUILD;
				Task->TaskLocation = UnbuiltHiveIndex->FloorLocation;
				Task->StructureType = STRUCTURE_ALIEN_HIVE;

				char buf[64];
				sprintf(buf, "I'll drop hive at %s", UTIL_GetClosestMapLocationToPoint(Task->TaskLocation));

				BotTeamSay(pBot, 1.0f, buf);

				return;
			}
		}
	}

	const resource_node* NearestUnprotectedResNode = UTIL_GetNearestUnprotectedResNode(pBot->pEdict->v.origin);

	if (NearestUnprotectedResNode)
	{
		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(3.0f));

		if (BuildLocation == ZERO_VECTOR)
		{
			return;
		}
		
		int NumOffenceChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_OFFENCECHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumOffenceChambers < 2) 
		{
			Task->TaskType = TASK_BUILD;
			Task->TaskLocation = BuildLocation;
			Task->StructureType = STRUCTURE_ALIEN_OFFENCECHAMBER;
			return;
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_DEFENCE))
		{
			int NumDefenceChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_DEFENCECHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (NumDefenceChambers < 2)
			{
				Task->TaskType = TASK_BUILD;
				Task->TaskLocation = BuildLocation;
				Task->StructureType = STRUCTURE_ALIEN_DEFENCECHAMBER;
				return;
			}
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_MOVEMENT))
		{
			int NumMovementChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_MOVEMENTCHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (NumMovementChambers < 1)
			{
				Task->TaskType = TASK_BUILD;
				Task->TaskLocation = BuildLocation;
				Task->StructureType = STRUCTURE_ALIEN_MOVEMENTCHAMBER;
				return;
			}
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_SENSORY))
		{
			int NumSensoryChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_SENSORYCHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (NumSensoryChambers < 1) 
			{
				Task->TaskType = TASK_BUILD;
				Task->TaskLocation = BuildLocation;
				Task->StructureType = STRUCTURE_ALIEN_SENSORYCHAMBER;
				return;
			}
		}		
		
	}

	AlienHarasserSetPrimaryTask(pBot, Task);
}

bool UTIL_BotWithBuildTaskExists(NSStructureType StructureType)
{
	for (int i = 0; i < 32; i++)
	{
		if (!bots[i].is_used || FNullEnt(bots[i].pEdict)) { continue; }

		if ((bots[i].PrimaryBotTask.TaskType == TASK_BUILD && bots[i].PrimaryBotTask.StructureType == StructureType) || (bots[i].SecondaryBotTask.TaskType == TASK_BUILD && bots[i].SecondaryBotTask.StructureType == StructureType))
		{
			return true;
		}
	}

	return false;
}

void AlienHarasserSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* PhaseGate = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), !IsPlayerSkulk(pBot->pEdict));

	if (PhaseGate)
	{
		int NumExistingPlayers = UTIL_GetNumPlayersOfTeamInArea(PhaseGate->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), pBot->bot_team, pBot->pEdict, CLASS_GORGE);

		if (NumExistingPlayers < 2)
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = PhaseGate;
			Task->TaskLocation = PhaseGate->v.origin;
			Task->bOrderIsUrgent = true;
			return;
		}
	}

	edict_t* TurretFactory = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), !IsPlayerSkulk(pBot->pEdict));

	if (TurretFactory)
	{
		int NumExistingPlayers = UTIL_GetNumPlayersOfTeamInArea(TurretFactory->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), pBot->bot_team, pBot->pEdict, CLASS_GORGE);

		if (NumExistingPlayers < 2)
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = TurretFactory;
			Task->TaskLocation = TurretFactory->v.origin;
			Task->bOrderIsUrgent = true;
			return;
		}
	}

	edict_t* AnyMarineStructure = UTIL_FindClosestMarineStructureToLocation(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), !IsPlayerSkulk(pBot->pEdict));

	if (AnyMarineStructure)
	{
		int NumExistingPlayers = UTIL_GetNumPlayersOfTeamInArea(AnyMarineStructure->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), pBot->bot_team, pBot->pEdict, CLASS_GORGE);

		if (NumExistingPlayers < 2)
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = AnyMarineStructure;
			Task->TaskLocation = AnyMarineStructure->v.origin;
			Task->bOrderIsUrgent = false;
			return;
		}
	}
}

void AlienCapperSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerGorge(pBot->pEdict))
	{
		edict_t* HurtNearbyPlayer = UTIL_GetClosestPlayerNeedsHealing(pBot->pEdict->v.origin, pBot->bot_team, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict, true);

		if (HurtNearbyPlayer)
		{
			if (pBot->SecondaryBotTask.TaskType != TASK_HEAL)
			{
				pBot->SecondaryBotTask.TaskType = TASK_HEAL;
				pBot->SecondaryBotTask.TaskTarget = HurtNearbyPlayer;
				pBot->SecondaryBotTask.TaskLocation = HurtNearbyPlayer->v.origin;
				pBot->SecondaryBotTask.bOrderIsUrgent = (HurtNearbyPlayer->v.health < (HurtNearbyPlayer->v.max_health * 0.5f));
			}
			return;
		}
	}
}

void AlienBuilderSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerGorge(pBot->pEdict))
	{
		edict_t* HurtNearbyPlayer = UTIL_GetClosestPlayerNeedsHealing(pBot->pEdict->v.origin, pBot->bot_team, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict, true);

		if (HurtNearbyPlayer)
		{
			if (pBot->SecondaryBotTask.TaskType != TASK_HEAL)
			{
				pBot->SecondaryBotTask.TaskType = TASK_HEAL;
				pBot->SecondaryBotTask.TaskTarget = HurtNearbyPlayer;
				pBot->SecondaryBotTask.TaskLocation = HurtNearbyPlayer->v.origin;
				pBot->SecondaryBotTask.bOrderIsUrgent = (HurtNearbyPlayer->v.health < (HurtNearbyPlayer->v.max_health * 0.5f));
			}
			return;
		}
	}
}


void UpdateAndClearTasks(bot_t* pBot)
{
	if (pBot->PrimaryBotTask.TaskType != TASK_NONE)
	{
		if (!UTIL_IsTaskStillValid(pBot, &pBot->PrimaryBotTask))
		{		
			if (UTIL_IsTaskCompleted(pBot, &pBot->PrimaryBotTask))
			{
				BotOnCompletePrimaryTask(pBot, &pBot->PrimaryBotTask);
			}
			else
			{
				UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			}
		}
	}

	if (pBot->SecondaryBotTask.TaskType != TASK_NONE)
	{
		if (!UTIL_IsTaskStillValid(pBot, &pBot->SecondaryBotTask))
		{
			UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);
		}
	}

	if (pBot->WantsAndNeedsTask.TaskType != TASK_NONE)
	{
		if (!UTIL_IsTaskStillValid(pBot, &pBot->WantsAndNeedsTask))
		{
			UTIL_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);
		}
	}

}

bot_task* BotGetNextTask(bot_t* pBot)
{
	if (UTIL_IsTaskUrgent(pBot, &pBot->WantsAndNeedsTask))
	{
		return &pBot->WantsAndNeedsTask;
	}

	if (UTIL_IsTaskUrgent(pBot, &pBot->SecondaryBotTask))
	{
		return &pBot->SecondaryBotTask;
	}
	
	if (UTIL_IsTaskUrgent(pBot, &pBot->PrimaryBotTask))
	{
		return &pBot->PrimaryBotTask;
	}

	if (pBot->WantsAndNeedsTask.TaskType != TASK_NONE)
	{
		return &pBot->WantsAndNeedsTask;
	}

	if (pBot->SecondaryBotTask.TaskType != TASK_NONE)
	{
		return &pBot->SecondaryBotTask;
	}

	return &pBot->PrimaryBotTask;
}

bool BotHasWantsAndNeeds(bot_t* pBot)
{
	return pBot->WantsAndNeedsTask.TaskType != TASK_NONE;
}

void BotMarineSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* UnbuiltStructure = nullptr;

	if (pBot->CurrentRole == BOT_ROLE_GUARD_BASE)
	{
		UnbuiltStructure = UTIL_FindClosestMarineStructureUnbuilt(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f));
	}
	else
	{
		UnbuiltStructure = UTIL_GetNearestUnbuiltStructureWithLOS(pBot, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), MARINE_TEAM);

		if (UnbuiltStructure)
		{
			int NumBuilders = UTIL_GetNumPlayersOfTeamInArea(UnbuiltStructure->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), MARINE_TEAM, pBot->pEdict, CLASS_MARINE_COMMANDER);

			if (NumBuilders >= 2 && vDist2DSq(pBot->pEdict->v.origin, UnbuiltStructure->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
			{
				UnbuiltStructure = nullptr;
			}
		}

	}

	if (UnbuiltStructure)
	{
		pBot->SecondaryBotTask.TaskType = TASK_BUILD;
		pBot->SecondaryBotTask.TaskTarget = UnbuiltStructure;
		pBot->SecondaryBotTask.TaskLocation = UnbuiltStructure->v.origin;
		pBot->SecondaryBotTask.bOrderIsUrgent = false;
		return;
	}
	else
	{
		if (BotHasWeapon(pBot, WEAPON_MARINE_WELDER))
		{
			edict_t* DamagedEdict = UTIL_FindMarineWithDamagedArmour(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

			if (FNullEnt(DamagedEdict))
			{
				DamagedEdict = UTIL_FindClosestDamagedStructure(pBot->pEdict->v.origin, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(20.0f));
			}

			if (DamagedEdict)
			{
				pBot->SecondaryBotTask.TaskType = TASK_WELD;
				pBot->SecondaryBotTask.TaskTarget = DamagedEdict;
				pBot->SecondaryBotTask.TaskLocation = DamagedEdict->v.origin;
				pBot->SecondaryBotTask.bOrderIsUrgent = false;
				return;
			}
		}
	}

	if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 && BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
	{
		const hive_definition* NearestHiveIndex = UTIL_GetNearestHiveOfStatus(pBot->pEdict->v.origin, HIVE_STATUS_BUILT);

		if (NearestHiveIndex)
		{
			edict_t* Hive = NearestHiveIndex->edict;

			if (Hive && UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(Hive)))
			{
				pBot->SecondaryBotTask.TaskType = TASK_ATTACK;
				pBot->SecondaryBotTask.TaskTarget = Hive;
				pBot->SecondaryBotTask.TaskLocation = UTIL_GetFloorUnderEntity(Hive);
				pBot->SecondaryBotTask.bOrderIsUrgent = false;
				return;
			}
		}

		edict_t* NearestAlienStructure = UTIL_GetClosestStructureAtLocation(pBot->pEdict->v.origin, false);

		if (!FNullEnt(NearestAlienStructure) && vDist2DSq(NearestAlienStructure->v.origin, pBot->pEdict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)) && UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(NearestAlienStructure)))
		{
			pBot->SecondaryBotTask.TaskType = TASK_ATTACK;
			pBot->SecondaryBotTask.TaskTarget = NearestAlienStructure;
			pBot->SecondaryBotTask.TaskLocation = NearestAlienStructure->v.origin;
			pBot->SecondaryBotTask.bOrderIsUrgent = false;
			return;
		}
	}

}







void BotMarineSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	Task->TaskType = TASK_NONE;

	if (pBot->CurrentRole == BOT_ROLE_GUARD_BASE)
	{
		Task->TaskType = TASK_GUARD;
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(20.0f));
		Task->bOrderIsUrgent = false;
		return;
	}
	else
	{
		const hive_definition* SiegedHive = UTIL_GetNearestHiveUnderSiege(pBot->pEdict->v.origin);

		if (SiegedHive)
		{
			int NumAttackers = UTIL_GetNumPlayersOfTeamInArea(SiegedHive->Location, UTIL_MetresToGoldSrcUnits(15.0f), MARINE_TEAM, pBot->pEdict, CLASS_NONE);

			if (BotHasWeapon(pBot, WEAPON_MARINE_GL) || NumAttackers < 3)
			{
				edict_t* UnbuiltStructure = UTIL_FindClosestMarineStructureUnbuilt(SiegedHive->Location, UTIL_MetresToGoldSrcUnits(20.0f));

				if (!BotHasWeapon(pBot, WEAPON_MARINE_GL) && !FNullEnt(UnbuiltStructure))
				{
					int NumBuilders = UTIL_GetNumPlayersOfTeamInArea(UnbuiltStructure->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), MARINE_TEAM, pBot->pEdict, CLASS_NONE);

					if (NumBuilders < 2)
					{
						Task->TaskType = TASK_BUILD;
						Task->TaskTarget = UnbuiltStructure;
						Task->TaskLocation = UnbuiltStructure->v.origin;
						Task->bOrderIsUrgent = true;
						Task->StructureType = UTIL_GetStructureTypeFromEdict(UnbuiltStructure);

						return;
					}
				}

				Task->TaskType = TASK_ATTACK;
				Task->TaskTarget = SiegedHive->edict;
				Task->TaskLocation = SiegedHive->FloorLocation;
				Task->bOrderIsUrgent = true;
				Task->StructureType = STRUCTURE_ALIEN_HIVE;

				return;
			}
			
		}


		Vector RandomPoint = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f));
		const resource_node* RandomResNode = UTIL_FindEligibleResNodeClosestToLocation(RandomPoint, MARINE_TEAM, true);

		if (RandomResNode)
		{
			Task->TaskType = TASK_CAP_RESNODE;
			Task->TaskLocation = RandomResNode->origin;
			Task->bOrderIsUrgent = false;
			Task->StructureType = STRUCTURE_MARINE_RESTOWER;
			
			return;
		}
	}
}



void BotProgressTakeCommandTask(bot_t* pBot)
{
	edict_t* CommChair = UTIL_GetCommChair();

	if (!CommChair) { return; }

	float DistFromChair = vDist2DSq(pBot->pEdict->v.origin, CommChair->v.origin);

	if (!UTIL_PlayerInUseRange(pBot->pEdict, CommChair))
	{
		MoveTo(pBot, CommChair->v.origin, MOVESTYLE_NORMAL);

		if (DistFromChair < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			LookAt(pBot, CommChair);
		}
	}
	else
	{
		float CommanderWaitTime = CONFIG_GetCommanderWaitTime();

		if ((gpGlobals->time - GameStartTime) > CommanderWaitTime)
		{
			BotUseObject(pBot, CommChair, false);
		}
		else
		{
			edict_t* NearestHuman = UTIL_GetNearestHumanAtLocation(CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

			if (!NearestHuman)
			{
				BotUseObject(pBot, CommChair, false);
			}
			else
			{
				LookAt(pBot, NearestHuman);
			}
		}

	}
}

void UTIL_ClearBotTask(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return; }

	if (Task->TaskType == TASK_GUARD)
	{
		UTIL_ClearGuardInfo(pBot);
	}

	Task->TaskType = TASK_NONE;
	Task->TaskLocation = ZERO_VECTOR;
	Task->TaskTarget = NULL;
	Task->TaskStartedTime = 0.0f;
	Task->bIssuedByCommander = false;
	Task->bTargetIsPlayer = false;
	Task->bOrderIsUrgent = false;
	Task->bIsWaitingForBuildLink = false;
	Task->LastBuildAttemptTime = 0.0f;
	Task->BuildAttempts = 0;
	Task->StructureType = STRUCTURE_NONE;
}



bool UTIL_IsAlienTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }

	switch (Task->TaskType)
	{
	case TASK_MOVE:
		return UTIL_IsMoveTaskStillValid(pBot, Task);
	case TASK_GET_HEALTH:
		return UTIL_IsAlienGetHealthTaskStillValid(pBot, Task);
	case TASK_HEAL:
		return UTIL_IsAlienHealTaskStillValid(pBot, Task);
	case TASK_ATTACK:
		return UTIL_IsAttackTaskStillValid(pBot, Task);
	case TASK_GUARD:
		return UTIL_IsGuardTaskStillValid(pBot, Task);
	case TASK_DEFEND:
		return UTIL_IsDefendTaskStillValid(pBot, Task);
	case TASK_BUILD:
		return UTIL_IsAlienBuildTaskStillValid(pBot, Task);
	case TASK_CAP_RESNODE:
		return UTIL_IsAlienCapResNodeTaskStillValid(pBot, Task);
	case TASK_EVOLVE:
		return UTIL_IsAlienEvolveTaskStillValid(pBot, Task);
	default:
		return false;
	}
}

bool UTIL_IsTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task || FNullEnt(pBot->pEdict)) { return false; }

	if (IsPlayerMarine(pBot->pEdict))
	{
		return UTIL_IsMarineTaskStillValid(pBot, Task);
	}
	else
	{
		return UTIL_IsAlienTaskStillValid(pBot, Task);
	}

	return false;
}

bool UTIL_IsMoveTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task->TaskLocation) { return false; }

	//if (!UTIL_PointIsReachable(pBot->BotNavInfo.MoveProfile, pBot->pEdict->v.origin, Task->TaskLocation, max_player_use_reach)) { return false; }

	return (vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) > sqrf(max_player_use_reach) || !UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, Task->TaskLocation));
}

int BotGetPrimaryWeaponMaxAmmoReserve(bot_t* pBot)
{
	NSWeapon PrimaryWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);

	if (PrimaryWeapon == WEAPON_NONE) { return 0; }

	return weapon_defs[PrimaryWeapon].iAmmo1Max;
}

bool UTIL_IsAlienGetHealthTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.deadflag == DEAD_DEAD)) { return false; }

	if (UTIL_IsEdictPlayer(Task->TaskTarget))
	{
		if (IsPlayerDead(Task->TaskTarget) || !IsPlayerGorge(Task->TaskTarget)) { return false; }
	}
	return (pBot->pEdict->v.health < pBot->pEdict->v.max_health) || (!IsPlayerSkulk(pBot->pEdict) && pBot->pEdict->v.armorvalue < (GetPlayerMaxArmour(pBot->pEdict) * 0.7f));
}

bool UTIL_IsAlienHealTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (!IsPlayerGorge(pBot->pEdict)) { return false; }

	if (Task->TaskTarget->v.armorvalue >= GetPlayerMaxArmour(Task->TaskTarget)) { return false; }

	if (Task->TaskTarget->v.health >= Task->TaskTarget->v.max_health) { return false; }

	// If our target is a player, give up if they are too far away. I'm not going to waste time chasing you around the map!

	float MaxHealRelevant = sqrf(UTIL_MetresToGoldSrcUnits(5.0f));

	return (vDist2DSq(pBot->CurrentFloorPosition, Task->TaskTarget->v.origin) <= MaxHealRelevant);
}

bool UTIL_IsEquipmentPickupTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerDead(pBot->pEdict) || IsPlayerBeingDigested(pBot->pEdict) || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, Task->TaskTarget->v.origin, 32.0f)) { return false; }

	return !UTIL_PlayerHasEquipment(pBot->pEdict);
}

bool UTIL_IsWeaponPickupTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerDead(pBot->pEdict) || IsPlayerBeingDigested(pBot->pEdict) || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, Task->TaskTarget->v.origin, 32.0f)) { return false; }

	NSWeapon WeaponType = UTIL_GetWeaponTypeFromEdict(Task->TaskTarget);

	if (WeaponType == WEAPON_NONE) { return false; }

	return !UTIL_PlayerHasWeapon(pBot->pEdict, WeaponType);
}

bool UTIL_IsAttackTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget)) { return false; }

	if ((Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, pBot->BotNavInfo.MoveStyle);

	if (!UTIL_PointIsOnNavmesh(NavProfileIndex, UTIL_GetFloorUnderEntity(Task->TaskTarget), Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach))) { return false; }

	if (IsPlayerSkulk(pBot->pEdict))
	{
		if (UTIL_IsStructureElectrified(Task->TaskTarget)) { return false; }
	}

	if (IsPlayerGorge(pBot->pEdict) && !BotHasWeapon(pBot, WEAPON_GORGE_BILEBOMB)) { return false; }

	NSStructureType StructureType = UTIL_GetStructureTypeFromEdict(Task->TaskTarget);

	if (IsPlayerMarine(pBot->pEdict))
	{
		if (StructureType == STRUCTURE_ALIEN_HIVE || StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
		{
			if (BotGetPrimaryWeaponClipAmmo(pBot) <= 0 && BotGetPrimaryWeaponAmmoReserve(pBot) <= 0)
			{
				return false;
			}
		}
	}

	float SearchRadius = (StructureType == STRUCTURE_ALIEN_HIVE) ? UTIL_MetresToGoldSrcUnits(15.0f) : UTIL_MetresToGoldSrcUnits(2.0f);
	int MaxAttackers = (StructureType == STRUCTURE_ALIEN_HIVE) ? 3 : 1;

	int NumAttackingPlayers = UTIL_GetNumPlayersOfTeamInArea(Task->TaskTarget->v.origin, SearchRadius, pBot->bot_team, pBot->pEdict, CLASS_GORGE);

	if (NumAttackingPlayers >= MaxAttackers && vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(SearchRadius))
	{
		return false;
	}


	return Task->TaskTarget->v.team != pBot->pEdict->v.team;

}

bool UTIL_IsStructureElectrified(const edict_t* Structure)
{
	if (FNullEnt(Structure)) { return false; }

	return (UTIL_StructureIsFullyBuilt(Structure) && !UTIL_StructureIsRecycling(Structure) && (Structure->v.deadflag == DEAD_NO) && (Structure->v.iuser4 & MASK_UPGRADE_11));
}

bool UTIL_IsBuildTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (!FNullEnt(Task->TaskTarget) && !UTIL_PointIsOnNavmesh(BUILDING_REGULAR_NAV_PROFILE, UTIL_GetFloorUnderEntity(Task->TaskTarget), Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach))) { return false; }

	NSStructureType StructureType = UTIL_GetStructureTypeFromEdict(Task->TaskTarget);

	if (StructureType == STRUCTURE_NONE) { return false; }

	if (!Task->bIssuedByCommander)
	{

		if (pBot->CurrentRole != BOT_ROLE_GUARD_BASE && vDist2DSq(Task->TaskTarget->v.origin, UTIL_GetCommChairLocation()) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			edict_t* NearestPlayer = UTIL_GetNearestPlayerOfTeamInArea(Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), MARINE_TEAM, pBot->pEdict, CLASS_MARINE_COMMANDER);

			if (NearestPlayer && vDist2DSq(NearestPlayer->v.origin, Task->TaskTarget->v.origin) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin))
			{
				return false;
			}
		}
		else
		{
			int NumBuilders = UTIL_GetNumPlayersOfTeamInArea(Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), MARINE_TEAM, pBot->pEdict, CLASS_MARINE_COMMANDER);

			if (NumBuilders >= 2 && vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
			{
				return false;
			}
		}
	}

	return !UTIL_StructureIsFullyBuilt(Task->TaskTarget);
}

bool UTIL_IsAlienBuildTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }

	if (!Task->TaskLocation) { return false; }

	if (Task->StructureType == STRUCTURE_NONE) { return false; }

	int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, pBot->BotNavInfo.MoveStyle);

	if (!FNullEnt(Task->TaskTarget) && !UTIL_PointIsOnNavmesh(NavProfileIndex, UTIL_GetFloorUnderEntity(Task->TaskTarget), Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach))) { return false; }

	if (Task->StructureType == STRUCTURE_ALIEN_HIVE)
	{
		const hive_definition* HiveIndex = UTIL_GetNearestHiveAtLocation(Task->TaskLocation);

		if (!HiveIndex) { return false; }

		if (HiveIndex->Status != HIVE_STATUS_UNBUILT) { return false; }

		if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_PHASEGATE, HiveIndex->Location, UTIL_MetresToGoldSrcUnits(15.0f)))
		{
			char buf[64];
			sprintf(buf, "We need to clear %s before I can build the hive", UTIL_GetClosestMapLocationToPoint(HiveIndex->Location));
			BotTeamSay(pBot, 1.0f, buf);
			return false; 
		}

		if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_TURRETFACTORY, HiveIndex->Location, UTIL_MetresToGoldSrcUnits(15.0f)))
		{
			char buf[64];
			sprintf(buf, "We need to clear %s before I can build the hive", UTIL_GetClosestMapLocationToPoint(HiveIndex->Location));
			BotTeamSay(pBot, 1.0f, buf);
			return false; 
		}


		edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(HiveIndex->Location, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(10.0f), pBot->pEdict);

		if (!FNullEnt(OtherGorge) && GetPlayerResources(OtherGorge) > pBot->resources)
		{
			char buf[64];
			sprintf(buf, "I won't drop hive, %s can do it", STRING(OtherGorge->v.netname));
			BotTeamSay(pBot, 1.0f, buf);
			return false;
		}
	}

	if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER)
	{
		const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

		if (!ResNodeIndex) { return false; }

		if (ResNodeIndex->bIsOccupied)
		{
			if (ResNodeIndex->bIsOwnedByMarines) { return false; }

			if (!IsPlayerGorge(pBot->pEdict)) { return false; }

			if (UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict)) { return false; }

			if (vDist2DSq(pBot->pEdict->v.origin, ResNodeIndex->origin) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { return false; }
		}

		if (FNullEnt(Task->TaskTarget))
		{
			edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

			if (!FNullEnt(OtherGorge) && (GetPlayerResources(OtherGorge) >= kResourceTowerCost && vDist2DSq(OtherGorge->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation)))
			{
				return false;
			}

			edict_t* Egg = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_EGG, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

			if (!FNullEnt(Egg) && (GetPlayerResources(Egg) >= kResourceTowerCost && vDist2DSq(Egg->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation)))
			{
				return false;
			}
		}
		else
		{
			edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

			if (!FNullEnt(OtherGorge) && vDist2DSq(OtherGorge->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation))
			{
				return false;
			}

			edict_t* OtherEgg = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_EGG, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

			if (!FNullEnt(OtherEgg) && vDist2DSq(OtherEgg->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation))
			{
				return false;
			}
		}
	}

	if (Task->StructureType == STRUCTURE_ALIEN_DEFENCECHAMBER || Task->StructureType == STRUCTURE_ALIEN_MOVEMENTCHAMBER || Task->StructureType == STRUCTURE_ALIEN_SENSORYCHAMBER || Task->StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
	{
		if (UTIL_GetNumPlacedStructuresOfTypeInRadius(Task->StructureType, Task->TaskLocation, UTIL_MetresToGoldSrcUnits(10.0f)) >= 3)
		{
			return false;
		}
	}

	if (Task->bIsWaitingForBuildLink && (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f)) { return true; }

	if (Task->BuildAttempts > 3) { return false; }

	if (!FNullEnt(Task->TaskTarget))
	{
		if ((Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag == DEAD_DEAD)) { return false; }
		return !UTIL_StructureIsFullyBuilt(Task->TaskTarget);
	}
	else
	{
		if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER) { return true; }

		return UTIL_GetNavAreaAtLocation(Task->TaskLocation) == SAMPLE_POLYAREA_GROUND;
	}
}

bool UTIL_IsResupplyTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }

	if (IsPlayerDead(pBot->pEdict) || IsPlayerBeingDigested(pBot->pEdict) || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, pBot->BotNavInfo.MoveStyle);

	if (!FNullEnt(Task->TaskTarget) && !UTIL_PointIsOnNavmesh(NavProfileIndex, UTIL_GetFloorUnderEntity(Task->TaskTarget), Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach))) { return false; }

	if (!UTIL_IsMarineStructure(Task->TaskTarget) || !UTIL_StructureTypesMatch(UTIL_GetStructureTypeFromEdict(Task->TaskTarget), STRUCTURE_MARINE_ANYARMOURY) || !UTIL_StructureIsFullyBuilt(Task->TaskTarget) || UTIL_StructureIsRecycling(Task->TaskTarget)) { return false; }

	return ( (pBot->pEdict->v.health < pBot->pEdict->v.max_health)
		  || (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxAmmoReserve(pBot))
		  || (BotGetSecondaryWeaponAmmoReserve(pBot) < BotGetSecondaryWeaponMaxAmmoReserve(pBot))
		   );
}

bool UTIL_IsGuardTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }

	if (!Task->TaskLocation || (pBot->GuardLengthTime > 0.0f && ((gpGlobals->time - pBot->GuardStartedTime) > pBot->GuardLengthTime)))
	{
		return false;
	}

	return true;
}

void BotOnCompletePrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return; }

	BotTaskType OldTaskType = Task->TaskType;
	bool bOldTaskWasOrder = Task->bIssuedByCommander;
	UTIL_ClearBotTask(pBot, Task);
	
	if (OldTaskType == TASK_GUARD)
	{
		UTIL_ClearGuardInfo(pBot);
	}

	if (IsPlayerMarine(pBot->pEdict))
	{
		if (OldTaskType == TASK_MOVE)
		{
			edict_t* NearbyAlienTower = UTIL_GetNearestStructureIndexOfType(pBot->pEdict->v.origin, STRUCTURE_ALIEN_RESTOWER, UTIL_MetresToGoldSrcUnits(15.0f), false);

			if (NearbyAlienTower)
			{
				Task->TaskType = TASK_ATTACK;
				Task->TaskTarget = NearbyAlienTower;
				Task->TaskLocation = Task->TaskTarget->v.origin;
			}
			else
			{
				if (bOldTaskWasOrder)
				{
					Task->TaskType = TASK_GUARD;
					Task->TaskLocation = pBot->pEdict->v.origin;
				}
			}


			
		}
		return;
	}

}

bool UTIL_IsTaskCompleted(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }

	switch (Task->TaskType)
	{
		case TASK_MOVE:
			return vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(1.0f));
		case TASK_BUILD:
			return !FNullEnt(Task->TaskTarget) && UTIL_StructureIsFullyBuilt(Task->TaskTarget);
		case TASK_ATTACK:
			return FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO);
		case TASK_GUARD:
			return (pBot->GuardLengthTime > 0.0f && ((gpGlobals->time - pBot->GuardStartedTime) > pBot->GuardLengthTime));
		default:
			return !UTIL_IsTaskStillValid(pBot, Task);
	}

	return false;
}

void BotThrowGrenadeAtTarget(bot_t* pBot, const Vector TargetPoint)
{
	float ProjectileSpeed = 800.0f;
	float ProjectileGravity = 640.0f;

	if (BotHasWeapon(pBot, WEAPON_MARINE_GL) && (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0))
	{
		pBot->DesiredCombatWeapon = WEAPON_MARINE_GL;
		
	}
	else
	{
		pBot->DesiredCombatWeapon = WEAPON_MARINE_GRENADE;
	}

	if (UTIL_GetBotCurrentWeapon(pBot) != pBot->DesiredCombatWeapon)
	{
		return;
	}

	if (pBot->DesiredCombatWeapon == WEAPON_MARINE_GL)
	{
		// I *think* the grenade launcher projectiles have lower gravity than a thrown grenade, but the same velocity.
		// Lower gravity means the bot has to aim lower as it has a flatter arc. Seems to work in practice anyway...
		ProjectileGravity = 400.0f;
	}

	Vector ThrowAngle = GetPitchForProjectile(pBot->CurrentEyePosition, TargetPoint, ProjectileSpeed, ProjectileGravity);

	ThrowAngle = UTIL_GetVectorNormal(ThrowAngle);

	Vector ThrowTargetLocation = pBot->CurrentEyePosition + (ThrowAngle * 200.0f);

	LookAt(pBot, ThrowTargetLocation);

	if (UTIL_GetBotCurrentWeapon(pBot) == WEAPON_MARINE_GL && BotGetPrimaryWeaponClipAmmo(pBot) == 0)
	{
		pBot->pEdict->v.button |= IN_RELOAD;
		return;
	}
	
	if ((gpGlobals->time - pBot->current_weapon.LastFireTime) < pBot->current_weapon.MinRefireTime)
	{
		return;
	}

	Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);
	Vector TargetAimDir = UTIL_GetVectorNormal(ThrowTargetLocation - pBot->CurrentEyePosition);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	if (AimDot >= 0.95f)
	{
		pBot->pEdict->v.button |= IN_ATTACK;
		pBot->current_weapon.LastFireTime = gpGlobals->time;
	}
}

void BotProgressGrenadeTask(bot_t* pBot, bot_task* Task)
{
	BotThrowGrenadeAtTarget(pBot, Task->TaskLocation);
}

void BotProgressDefendTask(bot_t* pBot, bot_task* Task)
{
	BotProgressGuardTask(pBot, Task);
}







bool UTIL_IsDefendTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget)) { return false; }

	if (Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (Task->TaskTarget->v.team != pBot->pEdict->v.team) { return false; }

	if (gpGlobals->time - pBot->LastCombatTime < 5.0f) { return true; }

	int NumExistingDefenders = UTIL_GetNumPlayersOfTeamInArea(Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), pBot->pEdict->v.team, pBot->pEdict, CLASS_GORGE);

	if (NumExistingDefenders >= 2) { return false; }

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && gpGlobals->time - pBot->LastCombatTime > 10.0f) { return false; }

	return true;
}

void AlienProgressGetHealthTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerGorge(pBot->pEdict))
	{
		if (UTIL_GetBotCurrentWeapon(pBot) != WEAPON_GORGE_HEALINGSPRAY)
		{
			BotSwitchToWeapon(pBot, WEAPON_GORGE_HEALINGSPRAY);
		}
		else
		{
			pBot->pEdict->v.button |= IN_ATTACK;
		}
		return;
	}

	if (IsPlayerFade(pBot->pEdict) && BotHasWeapon(pBot, WEAPON_FADE_METABOLIZE))
	{
		if (UTIL_GetBotCurrentWeapon(pBot) != WEAPON_FADE_METABOLIZE)
		{
			BotSwitchToWeapon(pBot, WEAPON_FADE_METABOLIZE);
		}
		else
		{
			pBot->pEdict->v.button |= IN_ATTACK;
		}
	}


	if (Task->TaskTarget)
	{
		float DesiredDist = sqrf(UTIL_MetresToGoldSrcUnits(2.0f));
		if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > DesiredDist || !UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, UTIL_GetFloorUnderEntity(Task->TaskTarget)))
		{
			Vector HealLocation = pBot->BotNavInfo.TargetDestination;

			if (!HealLocation || vDist2DSq(HealLocation, Task->TaskTarget->v.origin) > DesiredDist)
			{
				int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_HIDE);
				HealLocation = UTIL_GetRandomPointOnNavmeshInRadius(MoveProfile, UTIL_GetFloorUnderEntity(Task->TaskTarget), UTIL_MetresToGoldSrcUnits(2.0f));
			}

			MoveTo(pBot, HealLocation, MOVESTYLE_HIDE);

		}
		else
		{
			if (IsPlayerGorge(Task->TaskTarget))
			{
				LookAt(pBot, Task->TaskTarget);

				if (gpGlobals->time - pBot->LastCommanderRequestTime > min_request_spam_time)
				{
					pBot->pEdict->v.impulse = IMPULSE_ALIEN_REQUEST_HEALTH;
					pBot->LastCommanderRequestTime = gpGlobals->time;
				}
			}

			AlienGuardLocation(pBot, pBot->pEdict->v.origin);
		}
	}
}

void AlienProgressHealTask(bot_t* pBot, bot_task* Task)
{
	if (!IsPlayerGorge(pBot->pEdict) || FNullEnt(Task->TaskTarget)) { return; }

	if (UTIL_PlayerInUseRange(pBot->pEdict, Task->TaskTarget))
	{
		LookAt(pBot, Task->TaskTarget);
		if (UTIL_GetBotCurrentWeapon(pBot) != WEAPON_GORGE_HEALINGSPRAY)
		{
			BotSwitchToWeapon(pBot, WEAPON_GORGE_HEALINGSPRAY);
			return;
		}

		pBot->pEdict->v.button |= IN_ATTACK;
		
	}
	else
	{
		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
	}
}

bool UTIL_PlayerHasLOSToEntity(const edict_t* Player, const edict_t* Target, const float MaxRange, const bool bUseHullSweep)
{
	if (FNullEnt(Player) || FNullEnt(Target)) { return false; }
	Vector StartTrace = UTIL_GetPlayerEyePosition(Player);	

	Vector LookDirection = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - StartTrace);
	StartTrace = StartTrace - (LookDirection * 5.0f);
	Vector EndTrace = StartTrace + (LookDirection * (MaxRange + 5.0f));

	TraceResult hit;

	if (bUseHullSweep)
	{
		UTIL_TraceHull(StartTrace, EndTrace, dont_ignore_monsters, head_hull, Player->v.pContainingEntity, &hit);
	}
	else
	{
		UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, Player->v.pContainingEntity, &hit);
	}

	if (hit.flFraction < 1.0f)
	{
		return hit.pHit == Target;
	}
	else
	{
		return false;
	}
}

bool UTIL_PlayerHasLOSToLocation(const edict_t* Player, const Vector Target, const float MaxRange)
{
	if (FNullEnt(Player)) { return false; }
	Vector StartTrace = UTIL_GetPlayerEyePosition(Player);

	Vector LookDirection = UTIL_GetVectorNormal(Target - StartTrace);
	float dist = vDist3DSq(StartTrace, Target);
	dist = fminf(dist, sqrf(MaxRange));
	Vector EndTrace = StartTrace + (LookDirection * sqrtf(dist));

	TraceResult hit;

	UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, Player->v.pContainingEntity, &hit);

	return (hit.flFraction >= 1.0f);

}

bool UTIL_PlayerInUseRange(const edict_t* Player, const edict_t* Target)
{
	if (FNullEnt(Player) || FNullEnt(Target)) { return false; }
	Vector StartTrace = UTIL_GetPlayerEyePosition(Player);
	Vector UseDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - StartTrace);
	// Sometimes if the bot is REALLY close to the target, the trace fails. Give it 5 units extra of room to avoid this and compensate during the trace.
	StartTrace = StartTrace - (UseDir * 5.0f);
	Vector EndTrace = StartTrace + (UseDir * (max_player_use_reach + 5.0f)); // Add back the 5 units we took away

	TraceResult hit;

	UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, Player->v.pContainingEntity, &hit);

	if (hit.flFraction < 1.0f)
	{
		return hit.pHit == Target;
	}
	else
	{
		return false;
	}
}

Vector UTIL_GetPlayerEyePosition(const edict_t* Player)
{
	if (FNullEnt(Player)) { return ZERO_VECTOR; }

	return (Player->v.origin + Player->v.view_ofs);
}

void AlienProgressBuildTask(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskTarget)
	{
		float DistFromBuildLocation = vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin);


		if (UTIL_PlayerInUseRange(pBot->pEdict, Task->TaskTarget))
		{
			BotUseObject(pBot, Task->TaskTarget, true);

			return;
		}

		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

		return;
	}

	if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER)
	{
		const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

		if (ResNodeIndex)
		{
			if (ResNodeIndex->bIsOccupied && !ResNodeIndex->bIsOwnedByMarines)
			{
				Task->TaskTarget = ResNodeIndex->TowerEdict;
				return;
			}
		}
	}

	float DesiredDist = (Task->StructureType == STRUCTURE_ALIEN_RESTOWER) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(1.1f);

	float DistFromBuildLocation = vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation);

	if (DistFromBuildLocation > sqrf(DesiredDist) || !UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, (Task->TaskLocation + Vector(0.0f, 0.0f, 50.0f))))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	int ResRequired = UTIL_GetCostOfStructureType(Task->StructureType);

	if (!IsPlayerGorge(pBot->pEdict))
	{
		ResRequired += kGorgeEvolutionCost;
	}

	if (pBot->resources >= ResRequired)
	{
		if (!IsPlayerGorge(pBot->pEdict))
		{
			BotEvolveLifeform(pBot, CLASS_GORGE);
			return;
		}

		if (DistFromBuildLocation < sqrf(UTIL_MetresToGoldSrcUnits(1.0f)))
		{
			Vector MoveDir = UTIL_GetVectorNormal2D(pBot->pEdict->v.origin - Task->TaskLocation);
			Vector NewLocation = Task->TaskLocation + (MoveDir * UTIL_MetresToGoldSrcUnits(1.1f));
			LookAt(pBot, Task->TaskLocation);
			MoveDirectlyTo(pBot, NewLocation);
			return;
		}

		Vector LookLocation = Task->TaskLocation;
		LookLocation.z = Task->TaskLocation.z + UTIL_GetPlayerHeight(pBot->pEdict, false);
		LookAt(pBot, LookLocation);

		if (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f) { return; }


		float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->pEdict->v.v_angle), UTIL_GetVectorNormal2D(Task->TaskLocation - pBot->pEdict->v.origin));

		if (LookDot > 0.9f)
		{
			pBot->pEdict->v.impulse = UTIL_StructureTypeToImpulseCommand(Task->StructureType);
			Task->LastBuildAttemptTime = gpGlobals->time;
			Task->BuildAttempts++;
			Task->bIsWaitingForBuildLink = true;
		}
		
	}
	else
	{
		AlienGuardLocation(pBot, Task->TaskLocation);
	}


}

void BotProgressTask(bot_t* pBot, bot_task* Task)
{
	if (!Task || Task->TaskType == TASK_NONE) { return; }

	if (IsPlayerMarine(pBot->pEdict))
	{
		return MarineProgressTask(pBot, Task);
	}
	else
	{
		return AlienProgressTask(pBot, Task);
	}

	
}

void AlienProgressTask(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return; }

	switch (Task->TaskType)
	{
	case TASK_MOVE:
		BotProgressMoveTask(pBot, Task);
		break;
	case TASK_GET_HEALTH:
		AlienProgressGetHealthTask(pBot, Task);
		break;
	case TASK_HEAL:
		AlienProgressHealTask(pBot, Task);
		break;
	case TASK_GUARD:
		BotProgressGuardTask(pBot, Task);
		break;
	case TASK_DEFEND:
		BotProgressDefendTask(pBot, Task);
		break;
	case TASK_ATTACK:
		BotProgressAttackTask(pBot, Task);
		break;
	case TASK_BUILD:
		AlienProgressBuildTask(pBot, Task);
		break;
	case TASK_CAP_RESNODE:
		AlienProgressCapResNodeTask(pBot, Task);
		break;
	case TASK_EVOLVE:
		AlienProgressEvolveTask(pBot, Task);
		break;
	default:
		break;

	}
}

void MarineProgressTask(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return; }

	switch (Task->TaskType)
	{
	case TASK_MOVE:
		BotProgressMoveTask(pBot, Task);
		break;
	case TASK_GET_AMMO:
	case TASK_GET_HEALTH:
	case TASK_GET_EQUIPMENT:
	case TASK_GET_WEAPON:
		BotProgressPickupTask(pBot, Task);
		break;
	case TASK_RESUPPLY:
		BotProgressResupplyTask(pBot, Task);
		break;
	case TASK_BUILD:
		BotProgressBuildTask(pBot, Task);
		break;
	case TASK_GUARD:
		BotProgressGuardTask(pBot, Task);
		break;
	case TASK_ATTACK:
		BotProgressAttackTask(pBot, Task);
		break;
	case TASK_CAP_RESNODE:
		MarineProgressCapResNodeTask(pBot, Task);
		break;
	case TASK_WELD:
		MarineProgressWeldTask(pBot, Task);
		break;
	case TASK_DEFEND:
		BotProgressDefendTask(pBot, Task);
		break;
	case TASK_GRENADE:
		BotProgressGrenadeTask(pBot, Task);
		break;
	default:
		break;

	}
}



bool UTIL_IsAlienCapResNodeTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task->TaskLocation)
	{
		return false;
	}

	if (!IsPlayerSkulk(pBot->pEdict) && !IsPlayerGorge(pBot->pEdict)) { return false; }

	if (!IsPlayerGorge(pBot->pEdict))
	{
		if (pBot->resources < (kGorgeEvolutionCost + kResourceTowerCost))
		{
			return false;
		}
	}

	edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

	if (OtherGorge)
	{
		if (vDist2DSq(OtherGorge->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation))
		{
			return false;
		}
	}

	edict_t* Egg = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_EGG, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

	if (Egg && (GetPlayerResources(Egg) >= kResourceTowerCost && vDist2DSq(Egg->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation)))
	{
		return false;
	}

	const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex)
	{
		return false;
	}

	if (ResNodeIndex->bIsOccupied)
	{
		if (IsPlayerGorge(pBot->pEdict))
		{
			if (!ResNodeIndex->bIsOwnedByMarines)
			{
				return !UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict);
			}
			else
			{
				if (BotHasWeapon(pBot, WEAPON_GORGE_BILEBOMB))
				{
					return true;
				}
				else
				{
					return UTIL_IsAlienPlayerInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);
				}
			}

		}
		else
		{
			return false;
		}
	}
	else
	{
		if ((pBot->resources < kResourceTowerCost && !Task->bIsWaitingForBuildLink) || Task->BuildAttempts > 3)
		{
			return false;
		}
	}

	return true;
}

void AlienProgressCapResNodeTask(bot_t* pBot, bot_task* Task)
{

	float DistFromNode = vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation);

	if (DistFromNode > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)) || !UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, (Task->TaskLocation + Vector(0.0f, 0.0f, 50.0f))))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return; }

	if (ResNodeIndex->bIsOccupied)
	{
		if (ResNodeIndex->bIsOwnedByMarines)
		{
			if (IsPlayerGorge(pBot->pEdict) && !BotHasWeapon(pBot, WEAPON_GORGE_BILEBOMB))
			{
				if (UTIL_IsAlienPlayerInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict))
				{
					AlienGuardLocation(pBot, Task->TaskLocation);
				}
				else
				{
					BotEvolveLifeform(pBot, CLASS_SKULK);
				}
			}
			else
			{
				if (pBot->SecondaryBotTask.TaskType != TASK_ATTACK || pBot->SecondaryBotTask.TaskTarget != ResNodeIndex->TowerEdict)
				{
					pBot->SecondaryBotTask.TaskType = TASK_ATTACK;
					pBot->SecondaryBotTask.TaskTarget = ResNodeIndex->TowerEdict;
					pBot->SecondaryBotTask.TaskLocation = ResNodeIndex->origin;
				}

				BotProgressAttackTask(pBot, &pBot->SecondaryBotTask);
			}
			return;
		}
		else
		{
			if (!UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict))
			{

				if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, ResNodeIndex->TowerEdict, max_player_use_reach, true))
				{
					BotUseObject(pBot, ResNodeIndex->TowerEdict, true);

					return;
				}

				MoveTo(pBot, ResNodeIndex->TowerEdict->v.origin, MOVESTYLE_NORMAL);

				if (vDist2DSq(pBot->pEdict->v.origin, ResNodeIndex->TowerEdict->v.origin) < UTIL_MetresToGoldSrcUnits(5.0f))
				{
					LookAt(pBot, UTIL_GetCentreOfEntity(ResNodeIndex->TowerEdict));
				}

				return;
			}
		}

		return;
	}

	if (!IsPlayerGorge(pBot->pEdict))
	{
		BotEvolveLifeform(pBot, CLASS_GORGE);
		return;
	}

	LookAt(pBot, Task->TaskLocation);

	if (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f) { return; }

	float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->pEdict->v.v_angle), UTIL_GetVectorNormal2D(Task->TaskLocation - pBot->pEdict->v.origin));

	if (LookDot > 0.9f)
	{

		pBot->pEdict->v.impulse = IMPULSE_ALIEN_BUILD_RESTOWER;
		Task->LastBuildAttemptTime = gpGlobals->time + 1.0f;
		Task->bIsWaitingForBuildLink = true;
		Task->BuildAttempts++;
	}
}


void BotProgressMoveTask(bot_t* pBot, bot_task* Task)
{
	MoveTo(pBot, Task->TaskLocation, MOVESTYLE_HIDE);
}

void BotProgressPickupTask(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskType == TASK_GET_AMMO)
	{
		pBot->DesiredCombatWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);
	}

	MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

	float DistFromItem = vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin);

	if (DistFromItem < sqrf(UTIL_MetresToGoldSrcUnits(1.0f)))
	{
		LookAt(pBot, Task->TaskTarget);

		if (Task->TaskType == TASK_GET_WEAPON)
		{
			NSDeployableItem ItemType = UTIL_GetItemTypeFromEdict(Task->TaskTarget);

			if (UTIL_DroppedItemIsPrimaryWeapon(ItemType))
			{
				NSWeapon CurrentPrimaryWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);

				if (CurrentPrimaryWeapon != WEAPON_NONE && CurrentPrimaryWeapon != WEAPON_MARINE_MG)
				{
					if (UTIL_GetBotCurrentWeapon(pBot) != UTIL_GetBotMarinePrimaryWeapon(pBot))
					{
						BotSwitchToWeapon(pBot, UTIL_GetBotMarinePrimaryWeapon(pBot));
					}
					else
					{
						BotDropWeapon(pBot);
					}
				}
			}
		}
	}
}

void BotDropWeapon(bot_t* pBot)
{
	pBot->pEdict->v.impulse = IMPULSE_MARINE_DROP_WEAPON;
}

void BotProgressResupplyTask(bot_t* pBot, bot_task* Task)
{
	if (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxAmmoReserve(pBot))
	{
		pBot->DesiredCombatWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);
	}
	else
	{
		pBot->DesiredCombatWeapon = UTIL_GetBotMarineSecondaryWeapon(pBot);
	}


	if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Task->TaskTarget, max_player_use_reach, true))
	{
		BotUseObject(pBot, Task->TaskTarget, true);

		return;
	}

	Vector UseLocation = pBot->BotNavInfo.TargetDestination;

	if (!UseLocation || vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(1.5f)))
	{
		int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
		UseLocation = UTIL_GetRandomPointOnNavmeshInDonut(MoveProfile, Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(1.0f), UTIL_MetresToGoldSrcUnits(1.5f));

		if (!UseLocation)
		{
			UseLocation = Task->TaskTarget->v.origin;
		}
	}

	MoveTo(pBot, UseLocation, MOVESTYLE_NORMAL);

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < UTIL_MetresToGoldSrcUnits(5.0f))
	{
		LookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
	}

}

void BotProgressBuildTask(bot_t* pBot, bot_task* Task)
{
	edict_t* pEdict = pBot->pEdict;

	if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
	{
		pBot->DesiredCombatWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);
	}
	else
	{
		if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			pBot->DesiredCombatWeapon = UTIL_GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			pBot->DesiredCombatWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);
		}
	}

	if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Task->TaskTarget, max_player_use_reach, true))
	{
		BotUseObject(pBot, Task->TaskTarget, true);

		return;
	}

	Vector UseLocation = pBot->BotNavInfo.TargetDestination;

	if (!UseLocation || vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(1.5f)))
	{
		int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
		UseLocation = UTIL_GetRandomPointOnNavmeshInDonut(MoveProfile, Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(1.0f), UTIL_MetresToGoldSrcUnits(1.5f));

		if (!UseLocation)
		{
			UseLocation = Task->TaskTarget->v.origin;
		}
	}

	MoveTo(pBot, UseLocation, MOVESTYLE_NORMAL);

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < UTIL_MetresToGoldSrcUnits(5.0f))
	{
		LookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
	}

}

void BotProgressGuardTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerMarine(pBot->pEdict))
	{
		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			pBot->DesiredCombatWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);
		}
		else
		{
			if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
			{
				pBot->DesiredCombatWeapon = UTIL_GetBotMarineSecondaryWeapon(pBot);
			}
			else
			{
				pBot->DesiredCombatWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);
			}
		}
	}

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		pBot->GuardLengthTime = 0.0f;
		return;
	}

	if (pBot->GuardLengthTime == 0.0f)
	{
		UTIL_GenerateGuardWatchPoints(pBot, Task->TaskLocation);
		pBot->GuardLengthTime = frandrange(30.0f, 40.0f);
		pBot->GuardStartedTime = gpGlobals->time;
	}

	edict_t* pEdict = pBot->pEdict;
	if (pBot->NumGuardPoints == 0)
	{
		UTIL_GenerateGuardWatchPoints(pBot, Task->TaskLocation);
	}

	if (!pBot->GuardLookLocation)
	{
		int NewGuardLookIndex = irandrange(0, (pBot->NumGuardPoints - 1));

		if (NewGuardLookIndex > -1 && NewGuardLookIndex < pBot->NumGuardPoints)
		{
			pBot->GuardLookLocation = pBot->GuardPoints[NewGuardLookIndex];
			LookAt(pBot, pBot->GuardLookLocation);
		}		
	}

	if (!pBot->CurrentGuardLocation || (gpGlobals->time - pBot->GuardStartLookTime) > pBot->ThisGuardLookTime)
	{
		if (pBot->NumGuardPoints == 0) { return; }
		int NewGuardLookIndex = irandrange(0, (pBot->NumGuardPoints - 1));

		pBot->GuardLookLocation = pBot->GuardPoints[NewGuardLookIndex];
		pBot->LookTargetLocation = pBot->GuardLookLocation;

		pBot->GuardStartLookTime = gpGlobals->time;
		pBot->ThisGuardLookTime = frandrange(2.0f, 5.0f);

		Vector LookDir = UTIL_GetVectorNormal2D(pBot->GuardLookLocation - Task->TaskLocation);
		Vector NewMoveCentre = Task->TaskLocation - (LookDir * UTIL_MetresToGoldSrcUnits(2.0f));

		Vector NewMoveLoc = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, NewMoveCentre, UTIL_MetresToGoldSrcUnits(2.0f));

		pBot->CurrentGuardLocation = NewMoveLoc;
		MoveTo(pBot, pBot->CurrentGuardLocation, MOVESTYLE_NORMAL);

	}
	else
	{
		MoveTo(pBot, pBot->CurrentGuardLocation, MOVESTYLE_NORMAL);
		LookAt(pBot, pBot->GuardLookLocation);
	}

}

float UTIL_GetMaxIdealWeaponRange(const NSWeapon Weapon)
{
	switch (Weapon)
	{
		case WEAPON_MARINE_GL:
		case WEAPON_MARINE_MG:
		case WEAPON_MARINE_PISTOL:
		case WEAPON_FADE_ACIDROCKET:
		case WEAPON_SKULK_PARASITE:
		case WEAPON_SKULK_LEAP:
		case WEAPON_LERK_SPORES:
		case WEAPON_LERK_UMBRA:
		case WEAPON_ONOS_CHARGE:
			return UTIL_MetresToGoldSrcUnits(20.0f);
		case WEAPON_MARINE_HMG:
		case WEAPON_MARINE_GRENADE:
			return UTIL_MetresToGoldSrcUnits(10.0f);
		case WEAPON_MARINE_SHOTGUN:
		case WEAPON_GORGE_BILEBOMB:
		case WEAPON_ONOS_STOMP:
			return UTIL_MetresToGoldSrcUnits(8.0f);
		case WEAPON_SKULK_XENOCIDE:
			return UTIL_MetresToGoldSrcUnits(5.0f);
		case WEAPON_ONOS_GORE:
		case WEAPON_ONOS_DEVOUR:
			return UTIL_MetresToGoldSrcUnits(1.5f);
		default:
			return 48.0f;
	}
}

float UTIL_GetMinIdealWeaponRange(const NSWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_MARINE_GL:
	case WEAPON_MARINE_GRENADE:
	case WEAPON_FADE_ACIDROCKET:
		return UTIL_MetresToGoldSrcUnits(5.0f);
	case WEAPON_SKULK_LEAP:
		return UTIL_MetresToGoldSrcUnits(3.0f);
	case WEAPON_MARINE_MG:
	case WEAPON_MARINE_PISTOL:
	case WEAPON_MARINE_HMG:
	case WEAPON_SKULK_PARASITE:
		return UTIL_MetresToGoldSrcUnits(5.0f);
	case WEAPON_MARINE_SHOTGUN:
		return UTIL_MetresToGoldSrcUnits(2.0f);
	case WEAPON_GORGE_BILEBOMB:
	case WEAPON_ONOS_STOMP:
		return UTIL_MetresToGoldSrcUnits(2.0f);
	default:
		return max_player_use_reach * 0.5f;
	}
}

void BotProgressAttackTask(bot_t* pBot, bot_task* Task)
{
	if (!Task || FNullEnt(Task->TaskTarget)) { return; }

	if (Task->bTargetIsPlayer)
	{
		// For now just move to the target, the combat code will take over once the enemy is sighted
		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_AMBUSH);
		return;
	}

	NSWeapon AttackWeapon = WEAPON_NONE;

	if (IsPlayerMarine(pBot->pEdict))
	{
		AttackWeapon = BotMarineChooseBestWeaponForStructure(pBot, Task->TaskTarget);
	}
	else
	{
		if (BotHasWeapon(pBot, WEAPON_GORGE_BILEBOMB))
		{
			AttackWeapon = WEAPON_GORGE_BILEBOMB;
		}
		else
		{
			if (BotHasWeapon(pBot, WEAPON_SKULK_XENOCIDE))
			{
				int NumTargetsInArea = UTIL_GetNumPlayersOfTeamInArea(Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), MARINE_TEAM, NULL, CLASS_NONE);

				NumTargetsInArea += UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_ANYTURRET, Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

				NumTargetsInArea += UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_PHASEGATE, Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

				if (NumTargetsInArea > 2)
				{
					AttackWeapon = WEAPON_SKULK_XENOCIDE;
				}
				else
				{
					AttackWeapon = UTIL_GetBotAlienPrimaryWeapon(pBot);
				}
			}
			else
			{
				AttackWeapon = UTIL_GetBotAlienPrimaryWeapon(pBot);
			}

			
		}
	}


	float MaxRange = UTIL_GetMaxIdealWeaponRange(AttackWeapon);
	bool bHullSweep = UTIL_IsMeleeWeapon(AttackWeapon);

	if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Task->TaskTarget, MaxRange, bHullSweep))
	{
		pBot->DesiredCombatWeapon = AttackWeapon;

		if (UTIL_GetBotCurrentWeapon(pBot) == AttackWeapon)
		{
			BotAttackTarget(pBot, Task->TaskTarget);
		}
	}
	else
	{
		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
	}

}

bool UTIL_PlayerHasHeavyArmour(const edict_t* Player)
{
	return (Player->v.iuser4 & MASK_UPGRADE_13);
}

bool UTIL_PlayerHasJetpack(edict_t* Player)
{
	return (Player->v.iuser4 & MASK_UPGRADE_7);
}

bool UTIL_PlayerHasEquipment(edict_t* Player)
{
	return UTIL_PlayerHasHeavyArmour(Player) || UTIL_PlayerHasJetpack(Player);
}

bool UTIL_PlayerHasWeapon(edict_t* Player, NSWeapon WeaponType)
{
	return (Player->v.weapons & (1 << WeaponType));
}

bool UTIL_StructureTypesMatch(const NSStructureType TypeOne, const NSStructureType TypeTwo)
{
	return (	TypeOne == TypeTwo
			 || (TypeOne == STRUCTURE_MARINE_ANYARMOURY && (TypeTwo == STRUCTURE_MARINE_ARMOURY || TypeTwo == STRUCTURE_MARINE_ADVARMOURY))
			 || (TypeOne == STRUCTURE_MARINE_ANYTURRETFACTORY && (TypeTwo == STRUCTURE_MARINE_TURRETFACTORY || TypeTwo == STRUCTURE_MARINE_ADVTURRETFACTORY))
			 || (TypeOne == STRUCTURE_MARINE_ANYTURRET && (TypeTwo == STRUCTURE_MARINE_TURRET || TypeTwo == STRUCTURE_MARINE_SIEGETURRET))
			 || (TypeTwo == STRUCTURE_MARINE_ANYARMOURY && (TypeOne == STRUCTURE_MARINE_ARMOURY || TypeOne == STRUCTURE_MARINE_ADVARMOURY))
			 || (TypeTwo == STRUCTURE_MARINE_ANYTURRETFACTORY && (TypeOne == STRUCTURE_MARINE_TURRETFACTORY || TypeOne == STRUCTURE_MARINE_ADVTURRETFACTORY))
			 || (TypeTwo == STRUCTURE_MARINE_ANYTURRET && (TypeOne == STRUCTURE_MARINE_TURRET || TypeOne == STRUCTURE_MARINE_SIEGETURRET))
		);
}

bot_msg* UTIL_GetAvailableBotMsgSlot(bot_t* pBot)
{
	for (int i = 0; i < 5; i++)
	{
		if (!pBot->ChatMessages[i].bIsPending) { return &pBot->ChatMessages[i]; }
	}

	return nullptr;
}

void BotSay(bot_t* pBot, float Delay, char* textToSay)
{
	bot_msg* msgSlot = UTIL_GetAvailableBotMsgSlot(pBot);

	if (msgSlot)
	{
		msgSlot->bIsPending = true;
		msgSlot->bIsTeamSay = false;
		msgSlot->SendTime = gpGlobals->time + Delay;
		sprintf(msgSlot->msg, textToSay);
	}
}

void BotTeamSay(bot_t* pBot, float Delay, char* textToSay)
{
	bot_msg* msgSlot = UTIL_GetAvailableBotMsgSlot(pBot);

	if (msgSlot)
	{
		msgSlot->bIsPending = true;
		msgSlot->bIsTeamSay = true;
		msgSlot->SendTime = gpGlobals->time + Delay;
		sprintf(msgSlot->msg, textToSay);
	}
}



void BotRequestHealth(bot_t* pBot)
{
	if (gpGlobals->time - pBot->LastCommanderRequestTime > min_request_spam_time)
	{
		pBot->pEdict->v.impulse = IMPULSE_MARINE_REQUEST_HEALTH;
		pBot->LastCommanderRequestTime = gpGlobals->time;
	}
}

void BotRequestAmmo(bot_t* pBot)
{
	if (gpGlobals->time - pBot->LastCommanderRequestTime > min_request_spam_time)
	{
		pBot->pEdict->v.impulse = IMPULSE_MARINE_REQUEST_AMMO;
		pBot->LastCommanderRequestTime = gpGlobals->time;
	}
}

void BotRequestOrder(bot_t* pBot)
{
	if (gpGlobals->time - pBot->LastCommanderRequestTime > min_request_spam_time)
	{
		pBot->pEdict->v.impulse = IMPULSE_MARINE_REQUEST_ORDER;
		pBot->LastCommanderRequestTime = gpGlobals->time;
	}
}

bool UTIL_BotCanReload(bot_t* pBot)
{
	return (pBot->current_weapon.iClip < pBot->current_weapon.iClipMax && pBot->current_weapon.iAmmo1 > 0);
}





void AlienCheckWantsAndNeeds(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (IsPlayerDead(pEdict)) { return; }

	bool bNeedsHealth = (pEdict->v.health < pEdict->v.max_health);

	if (bNeedsHealth)
	{
		if (pBot->WantsAndNeedsTask.TaskType != TASK_GET_HEALTH)
		{
			edict_t* HealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);

			if (!FNullEnt(HealingSource))
			{
				pBot->WantsAndNeedsTask.TaskType = TASK_GET_HEALTH;
				pBot->WantsAndNeedsTask.TaskTarget = HealingSource;
				pBot->WantsAndNeedsTask.TaskLocation = UTIL_GetFloorUnderEntity(HealingSource);
				pBot->WantsAndNeedsTask.bOrderIsUrgent = (pEdict->v.health < (pEdict->v.max_health * 0.5f));
				return;
			}

		}
	}


	if (pBot->CurrentRole != BOT_ROLE_BUILDER && (pBot->PrimaryBotTask.TaskType == TASK_CAP_RESNODE || pBot->PrimaryBotTask.TaskType == TASK_BUILD))
	{
		return;
	}

	if (gpGlobals->time - pBot->LastCombatTime > 15.0f)
	{
		if (pBot->CurrentRole == BOT_ROLE_FADE && pBot->PrimaryBotTask.TaskType != TASK_CAP_RESNODE)
		{
			if (!IsPlayerFade(pEdict) && pBot->resources >= kFadeEvolutionCost)
			{
				pBot->WantsAndNeedsTask.TaskType = TASK_EVOLVE;
				pBot->WantsAndNeedsTask.Evolution = IMPULSE_ALIEN_EVOLVE_FADE;
				pBot->WantsAndNeedsTask.bOrderIsUrgent = true;
				return;
			}
		}

		if (pBot->CurrentRole == BOT_ROLE_ONOS && pBot->PrimaryBotTask.TaskType != TASK_CAP_RESNODE)
		{
			if (!IsPlayerOnos(pEdict) && pBot->resources >= kOnosEvolutionCost)
			{
				pBot->WantsAndNeedsTask.TaskType = TASK_EVOLVE;
				pBot->WantsAndNeedsTask.Evolution = IMPULSE_ALIEN_EVOLVE_ONOS;
				pBot->WantsAndNeedsTask.bOrderIsUrgent = true;
				return;
			}
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_MOVEMENT) && !UTIL_PlayerHasAlienUpgradeOfType(pEdict, HIVE_TECH_MOVEMENT) && UTIL_StructureExistsOfType(STRUCTURE_ALIEN_MOVEMENTCHAMBER))
		{
			pEdict->v.impulse = UTIL_GetDesiredAlienUpgrade(pBot, HIVE_TECH_MOVEMENT);

			return;
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_DEFENCE) && !UTIL_PlayerHasAlienUpgradeOfType(pEdict, HIVE_TECH_DEFENCE) && UTIL_StructureExistsOfType(STRUCTURE_ALIEN_DEFENCECHAMBER))
		{
			pEdict->v.impulse = UTIL_GetDesiredAlienUpgrade(pBot, HIVE_TECH_DEFENCE);

			return;
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_SENSORY) && !UTIL_PlayerHasAlienUpgradeOfType(pEdict, HIVE_TECH_SENSORY) && UTIL_StructureExistsOfType(STRUCTURE_ALIEN_SENSORYCHAMBER))
		{
			pEdict->v.impulse = UTIL_GetDesiredAlienUpgrade(pBot, HIVE_TECH_SENSORY);

			return;
		}
	}
}

int UTIL_GetDesiredAlienUpgrade(const bot_t* pBot, const HiveTechStatus TechType)
{
	edict_t* pEdict = pBot->pEdict;

	if (TechType == HIVE_TECH_DEFENCE)
	{
		switch (pBot->bot_ns_class)
		{
		case CLASS_SKULK:
		{
			return IMPULSE_ALIEN_UPGRADE_CARAPACE;
		}
		case CLASS_GORGE:
		case CLASS_FADE:
		{
			if (randbool())
			{
				return IMPULSE_ALIEN_UPGRADE_CARAPACE;
			}
			else
			{
				if (BotHasWeapon(pBot, WEAPON_FADE_METABOLIZE))
				{
					return IMPULSE_ALIEN_UPGRADE_REDEMPTION;
				}
				else
				{
					if (randbool())
					{
						return IMPULSE_ALIEN_UPGRADE_REGENERATION;
					}
					else
					{
						return IMPULSE_ALIEN_UPGRADE_REDEMPTION;
					}
				}
			}
		}
		case CLASS_ONOS:
		{
			if (randbool())
			{
				return IMPULSE_ALIEN_UPGRADE_CARAPACE;
			}
			else
			{
				if (randbool())
				{
					return IMPULSE_ALIEN_UPGRADE_REGENERATION;
				}
				else
				{
					return IMPULSE_ALIEN_UPGRADE_REDEMPTION;
				}

				
			}
		}
		default:
			return 0;
		}
	}

	if (TechType == HIVE_TECH_MOVEMENT)
	{
		switch (pBot->bot_ns_class)
		{
			case CLASS_SKULK:
			{
				if (randbool())
				{
					return IMPULSE_ALIEN_UPGRADE_CELERITY;
				}
				else
				{
					return IMPULSE_ALIEN_UPGRADE_SILENCE;
				}
			}
			case CLASS_GORGE:
			case CLASS_FADE:
			case CLASS_ONOS:
			{
				if (randbool())
				{
					return IMPULSE_ALIEN_UPGRADE_CELERITY;
				}
				else
				{
					return IMPULSE_ALIEN_UPGRADE_ADRENALINE;
				}
			}
			default:
				return 0;
		}

	}

	if (TechType == HIVE_TECH_SENSORY)
	{
		switch (pBot->bot_ns_class)
		{
			case CLASS_GORGE:
				return IMPULSE_ALIEN_UPGRADE_CLOAK;		
			case CLASS_SKULK:
			case CLASS_FADE:
			case CLASS_ONOS:
			{
				if (randbool())
				{
					return IMPULSE_ALIEN_UPGRADE_CLOAK;
				}
				else
				{
					return IMPULSE_ALIEN_UPGRADE_FOCUS;
				}
			}
			default:
				return 0;
		}
	}

	return 0;
}

bool UTIL_IsTaskUrgent(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskType == TASK_NONE) { return false; }

	if (Task->bOrderIsUrgent) { return true; }

	switch (Task->TaskType)
	{
		case TASK_GET_AMMO:
			return (BotGetPrimaryWeaponAmmoReserve(pBot) == 0);
		case TASK_GET_HEALTH:
			return (pBot->pEdict->v.health < 50.0f);
		case TASK_ATTACK:
		case TASK_GET_WEAPON:
		case TASK_GET_EQUIPMENT:
		case TASK_WELD:
			return false;
		case TASK_RESUPPLY:
			return (pBot->pEdict->v.health < 50.0f) || (BotGetPrimaryWeaponAmmoReserve(pBot) == 0);
		case TASK_MOVE:
			return UTIL_IsMoveTaskUrgent(pBot, Task);
		case TASK_BUILD:
			return UTIL_IsBuildTaskUrgent(pBot, Task);
		case TASK_GUARD:
			return UTIL_IsGuardTaskUrgent(pBot, Task);
		default:
			return false;
	}

	return false;
}

bool UTIL_IsGuardTaskUrgent(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskTarget)
	{
		NSStructureType StructType = UTIL_GetStructureTypeFromEdict(Task->TaskTarget);

		if (StructType == STRUCTURE_MARINE_PHASEGATE || StructType == STRUCTURE_MARINE_TURRETFACTORY)
		{
			return true;
		}
	}

	return UTIL_IsAlienPlayerInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(20.0f));
}

bool UTIL_IsBuildTaskUrgent(bot_t* pBot, bot_task* Task)
{
	if (!Task->TaskTarget) { return false; }

	NSStructureType StructType = UTIL_GetStructureTypeFromEdict(Task->TaskTarget);

	if (StructType == STRUCTURE_MARINE_PHASEGATE || StructType == STRUCTURE_MARINE_TURRETFACTORY) { return true; }

	return false;
}

bool UTIL_IsMoveTaskUrgent(bot_t* pBot, bot_task* Task)
{
	return UTIL_IsNearActiveHive(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(30.0f)) || UTIL_IsAlienPlayerInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(20.0f));
}







float UTIL_GetPlayerHealth(const edict_t* Player)
{
	return Player->v.health;
}

int BotGetPrimaryWeaponAmmoReserve(bot_t* pBot)
{
	NSWeapon PrimaryWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);

	if (PrimaryWeapon == WEAPON_NONE) { return 0; }

	int PrimaryAmmoIndex = weapon_defs[PrimaryWeapon].iAmmo1;

	return pBot->m_rgAmmo[PrimaryAmmoIndex];
}

int BotGetSecondaryWeaponAmmoReserve(bot_t* pBot)
{
	NSWeapon SecondaryWeapon = UTIL_GetBotMarineSecondaryWeapon(pBot);

	if (SecondaryWeapon == WEAPON_NONE) { return 0; }

	int SecondaryAmmoIndex = weapon_defs[SecondaryWeapon].iAmmo1;

	return pBot->m_rgAmmo[SecondaryAmmoIndex];
}

int BotGetSecondaryWeaponMaxAmmoReserve(bot_t* pBot)
{
	NSWeapon SecondaryWeapon = UTIL_GetBotMarineSecondaryWeapon(pBot);

	if (SecondaryWeapon == WEAPON_NONE) { return 0; }

	return weapon_defs[SecondaryWeapon].iAmmo1Max;
}

NSWeapon UTIL_GetBotCurrentWeapon(const bot_t* pBot)
{
	return (NSWeapon)pBot->current_weapon.iId;
}

NSWeapon UTIL_GetBotAlienPrimaryWeapon(const bot_t* pBot)
{
	switch (pBot->bot_ns_class)
	{
		case CLASS_SKULK:
			return WEAPON_SKULK_BITE;
		case CLASS_GORGE:
			return WEAPON_GORGE_SPIT;
		case CLASS_LERK:
			return WEAPON_LERK_BITE;
		case CLASS_FADE:
			return WEAPON_FADE_SWIPE;
		case CLASS_ONOS:
			return WEAPON_ONOS_GORE;
		default:
			return WEAPON_NONE;
	}

	return WEAPON_NONE;
}

NSWeapon UTIL_GetBotMarinePrimaryWeapon(const bot_t* pBot)
{
	if (BotHasWeapon(pBot, WEAPON_MARINE_MG))
	{
		return WEAPON_MARINE_MG;
	}

	if (BotHasWeapon(pBot, WEAPON_MARINE_HMG))
	{
		return WEAPON_MARINE_HMG;
	}

	if (BotHasWeapon(pBot, WEAPON_MARINE_GL))
	{
		return WEAPON_MARINE_GL;
	}

	if (BotHasWeapon(pBot, WEAPON_MARINE_SHOTGUN))
	{
		return WEAPON_MARINE_SHOTGUN;
	}

	return WEAPON_NONE;
}

NSWeapon UTIL_GetBotMarineSecondaryWeapon(const bot_t* pBot)
{
	if (BotHasWeapon(pBot, WEAPON_MARINE_PISTOL))
	{
		return WEAPON_MARINE_PISTOL;
	}

	return WEAPON_NONE;
}

void UTIL_ClearGuardInfo(bot_t* pBot)
{
	memset(pBot->GuardPoints, 0, sizeof(pBot->GuardPoints));
	pBot->CurrentGuardLocation = ZERO_VECTOR;
	pBot->GuardLengthTime = 0.0f;
	pBot->NumGuardPoints = 0;
	pBot->GuardLookLocation = ZERO_VECTOR;
	pBot->GuardStartLookTime = 0.0f;
	pBot->GuardStartedTime = 0.0f;
	pBot->LookTargetLocation = ZERO_VECTOR;
}

void UTIL_GenerateGuardWatchPoints(bot_t* pBot, const Vector& GuardLocation)
{
	const edict_t* pEdict = pBot->pEdict;

	memset(&pBot->GuardPoints, 0, sizeof(pBot->GuardPoints));
	pBot->NumGuardPoints = 0;

	int MoveProfileIndex = (IsPlayerOnMarineTeam(pEdict)) ? SKULK_REGULAR_NAV_PROFILE : MARINE_REGULAR_NAV_PROFILE;

	bot_path_node path[MAX_PATH_SIZE];
	int pathSize = 0;

	if (UTIL_GetNumTotalHives() == 0)
	{
		PopulateEmptyHiveList();
	}

	for (int i = 0; i < UTIL_GetNumTotalHives(); i++)
	{

		dtStatus SearchResult = FindPathToPoint(MoveProfileIndex, UTIL_GetFloorUnderEntity(UTIL_GetHiveAtIndex(i)->edict), GuardLocation, path, &pathSize, true);

		if (dtStatusSucceed(SearchResult))
		{
			//Vector FurthestPointVisible = UTIL_GetFurthestVisiblePointOnPath(GuardLocation + Vector(0.0f, 0.0f, 32.0f), path, pathSize, true);
			Vector FinalApproachDir = UTIL_GetVectorNormal2D(path[pathSize - 1].Location - path[pathSize - 2].Location);
			Vector ProspectiveNewGuardLoc = GuardLocation - (FinalApproachDir * 300.0f);

			ProspectiveNewGuardLoc.z = path[pathSize - 2].Location.z;

			pBot->GuardPoints[pBot->NumGuardPoints++] = ProspectiveNewGuardLoc;
		}
	}

	if (vDist2DSq(GuardLocation, UTIL_GetCommChairLocation()) > sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
	{
		dtStatus SearchResult = FindPathToPoint(MoveProfileIndex, UTIL_GetCommChairLocation(), GuardLocation, path, &pathSize, true);

		if (dtStatusSucceed(SearchResult))
		{
			Vector FinalApproachDir = UTIL_GetVectorNormal2D(path[pathSize - 1].Location - path[pathSize - 2].Location);
			Vector ProspectiveNewGuardLoc = GuardLocation - (FinalApproachDir * 300.0f);

			ProspectiveNewGuardLoc.z = path[pathSize - 2].Location.z;
			
			pBot->GuardPoints[pBot->NumGuardPoints++] = ProspectiveNewGuardLoc;
		}
	}

}



void OnBotFinishGuardingLocation(bot_t* pBot)
{
	UTIL_ClearGuardInfo(pBot);

	if (pBot->CurrentRole == BOT_ROLE_GUARD_BASE)
	{
		Vector NewGuardLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f));
		float NewGuardTime = frandrange(20.0f, 30.0f);

		MarineGuardLocation(pBot, NewGuardLocation, NewGuardTime);

	}
}





bool UTIL_IsMeleeWeapon(const NSWeapon Weapon)
{
	switch (Weapon)
	{
		case WEAPON_MARINE_KNIFE:
		case WEAPON_SKULK_BITE:
		case WEAPON_FADE_SWIPE:
		case WEAPON_ONOS_GORE:
		case WEAPON_ONOS_DEVOUR:
		case WEAPON_LERK_BITE:
			return true;
		default:
			return false;
	}
}

bool UTIL_WeaponNeedsReloading(const NSWeapon CheckWeapon)
{
	switch (CheckWeapon)
	{
		case WEAPON_MARINE_GL:
		case WEAPON_MARINE_HMG:
		case WEAPON_MARINE_MG:
		case WEAPON_MARINE_PISTOL:
		case WEAPON_MARINE_SHOTGUN:
			return true;
		default:
			return false;

	}
}

void BotAttackTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	Vector AimLocation = UTIL_GetCentreOfEntity(Target);

	NSWeapon CurrentWeapon = UTIL_GetBotCurrentWeapon(pBot);

	if (CurrentWeapon == WEAPON_MARINE_GL || CurrentWeapon == WEAPON_MARINE_GRENADE)
	{
		Vector NewAimAngle = GetPitchForProjectile(pBot->CurrentEyePosition, AimLocation, 800.0f, 640.0f);

		NewAimAngle = UTIL_GetVectorNormal(NewAimAngle);

		Vector AimLocation = pBot->CurrentEyePosition + (NewAimAngle * 200.0f);
	}

	LookAt(pBot, AimLocation);

	// Don't need aiming or LOS checks for Xenocide as it's an AOE attack, just make sure we're close enough
	if (CurrentWeapon == WEAPON_SKULK_XENOCIDE)
	{
		float MaxXenoDist = UTIL_GetMaxIdealWeaponRange(CurrentWeapon);

		if (vDist3DSq(pBot->pEdict->v.origin, Target->v.origin) <= sqrf(MaxXenoDist))
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
			return;
		}
	}

	if (UTIL_IsMeleeWeapon(CurrentWeapon))
	{
		if (UTIL_PlayerInUseRange(pBot->pEdict, Target))
		{
			Vector DirToTarget = UTIL_GetVectorNormal2D(Target->v.origin - pBot->pEdict->v.origin);
			float DotProduct = UTIL_GetDotProduct2D(UTIL_GetForwardVector(pBot->pEdict->v.v_angle), DirToTarget);

			if (DotProduct >= 0.45f)
			{
				pBot->pEdict->v.button |= IN_ATTACK;
				pBot->current_weapon.LastFireTime = gpGlobals->time;
			}
		}

		return;
	}

	// For charge and stomp, we don't need to be precise about aiming: only facing the correct direction
	if (CurrentWeapon == WEAPON_ONOS_CHARGE || CurrentWeapon == WEAPON_ONOS_STOMP)
	{
		Vector DirToTarget = UTIL_GetVectorNormal2D(Target->v.origin - pBot->pEdict->v.origin);
		float DotProduct = UTIL_GetDotProduct2D(UTIL_GetForwardVector(pBot->pEdict->v.v_angle), DirToTarget);

		float MinDotProduct = (CurrentWeapon == WEAPON_ONOS_STOMP) ? 0.95f : 0.75f;

		if (DotProduct >= MinDotProduct)
		{
			if (CurrentWeapon == WEAPON_ONOS_CHARGE)
			{
				pBot->pEdict->v.button |= IN_ATTACK2;
			}
			else
			{
				pBot->pEdict->v.button |= IN_ATTACK;
			}
			
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}

		return;
	}

	if (UTIL_WeaponNeedsReloading(CurrentWeapon) && BotGetCurrentWeaponClipAmmo(pBot) == 0)
	{
		if (BotGetCurrentWeaponReserveAmmo(pBot) > 0)
		{
			pBot->pEdict->v.button |= IN_RELOAD;
		}
		return;
	}

	if ((gpGlobals->time - pBot->current_weapon.LastFireTime) < pBot->current_weapon.MinRefireTime)
	{
		return;
	}

	float MaxWeaponRange = UTIL_GetMaxIdealWeaponRange(CurrentWeapon);
	bool bHullSweep = UTIL_IsMeleeWeapon(CurrentWeapon);

	if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Target, MaxWeaponRange, bHullSweep))
	{
		Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);
		Vector TargetAimDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - pBot->CurrentEyePosition);

		float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

		if (AimDot >= 0.95f)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}
	}
}

int BotGetCurrentWeaponClipAmmo(const bot_t* pBot)
{
	return pBot->current_weapon.iClip;
}

int BotGetCurrentWeaponMaxClipAmmo(const bot_t* pBot)
{
	return pBot->current_weapon.iClipMax;
}

int BotGetCurrentWeaponReserveAmmo(const bot_t* pBot)
{
	return pBot->current_weapon.iAmmo1;
}

int BotHasGrenades(const bot_t* pBot)
{
	return BotHasWeapon(pBot, WEAPON_MARINE_GRENADE);//&& BotGetGrenadeCount(pBot) > 0;
}

int BotGetGrenadeCount(const bot_t* pBot)
{
	return pBot->m_clipAmmo[WEAPON_MARINE_GRENADE];
}

int BotGetPrimaryWeaponClipAmmo(const bot_t* pBot)
{
	NSWeapon PrimaryWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);

	if (PrimaryWeapon == WEAPON_NONE) { return 0; }

	return pBot->m_clipAmmo[PrimaryWeapon];
}

int BotGetSecondaryWeaponClipAmmo(const bot_t* pBot)
{
	NSWeapon SecondaryWeapon = UTIL_GetBotMarineSecondaryWeapon(pBot);

	if (SecondaryWeapon == WEAPON_NONE) { return 0; }

	return pBot->m_clipAmmo[SecondaryWeapon];
}

int BotGetPrimaryWeaponMaxClipSize(const bot_t* pBot)
{
	NSWeapon PrimaryWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);

	if (PrimaryWeapon == WEAPON_NONE) { return 0; }

	return weapon_defs[PrimaryWeapon].iClipSize;
}

int BotGetSecondaryWeaponMaxClipSize(const bot_t* pBot)
{
	NSWeapon SecondaryWeapon = UTIL_GetBotMarineSecondaryWeapon(pBot);

	if (SecondaryWeapon == WEAPON_NONE) { return 0; }

	return weapon_defs[SecondaryWeapon].iClipSize;
}

void BotEvolveLifeform(bot_t* pBot, NSPlayerClass TargetLifeform)
{
	if (TargetLifeform == pBot->bot_ns_class) { return; }

	switch (TargetLifeform)
	{
		case CLASS_SKULK:
			pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_SKULK;
			return;
		case CLASS_GORGE:
			pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_GORGE;
			return;
		case CLASS_LERK:
			pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_LERK;
			return;
		case CLASS_FADE:
			pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_FADE;
			return;
		case CLASS_ONOS:
			pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_ONOS;
			return;
		default:
			return;
	}
}

bool UTIL_StructureIsHive(const edict_t* StructureEdict)
{
	return (StructureEdict->v.iuser3 == AVH_USER3_HIVE);
}

bool DoesBotHaveTraitCategory(bot_t* pBot, AlienTraitCategory TraitCategory)
{
	if (!IsPlayerOnAlienTeam(pBot->pEdict) || IsPlayerDead(pBot->pEdict)) { return false; }

	int iUser4 = pBot->pEdict->v.iuser4;

	switch (TraitCategory)
	{
		case TRAIT_DEFENSIVE:
			return ((iUser4 & MASK_UPGRADE_1) || (iUser4 & MASK_UPGRADE_2) || (iUser4 & MASK_UPGRADE_3));
		case TRAIT_MOVEMENT:
			return ((iUser4 & MASK_UPGRADE_4) || (iUser4 & MASK_UPGRADE_5) || (iUser4 & MASK_UPGRADE_6));
		case TRAIT_SENSORY:
			return ((iUser4 & MASK_UPGRADE_7) || (iUser4 & MASK_UPGRADE_8) || (iUser4 & MASK_UPGRADE_9));
		default:
			return false;
	}

	return false;
}

Vector UTIL_GetCentreOfEntity(const edict_t* Entity)
{
	if (!Entity) { return ZERO_VECTOR; }

	return (Entity->v.absmin + (Entity->v.size * 0.5f));
}

void UTIL_OnGameStart()
{
	if (!NavmeshLoaded()) { return; }
	UTIL_ClearMapAIData();
	UTIL_PopulateResourceNodeLocations();
	PopulateEmptyHiveList();
}


void BotNotifyStructureDestroyed(bot_t* pBot, const NSStructureType Structure, const Vector Location)
{

}

NSStructureType UTIL_IUSER3ToStructureType(const int inIUSER3)
{
	if (inIUSER3 == AVH_USER3_COMMANDER_STATION)		{ return STRUCTURE_MARINE_COMMCHAIR; }
	if (inIUSER3 == AVH_USER3_RESTOWER)					{ return STRUCTURE_MARINE_RESTOWER; }
	if (inIUSER3 == AVH_USER3_INFANTRYPORTAL)			{ return STRUCTURE_MARINE_INFANTRYPORTAL; }
	if (inIUSER3 == AVH_USER3_ARMORY)					{ return STRUCTURE_MARINE_ARMOURY; }
	if (inIUSER3 == AVH_USER3_ADVANCED_ARMORY)			{ return STRUCTURE_MARINE_ADVARMOURY; }
	if (inIUSER3 == AVH_USER3_TURRET_FACTORY)			{ return STRUCTURE_MARINE_TURRETFACTORY; }
	if (inIUSER3 == AVH_USER3_ADVANCED_TURRET_FACTORY)	{ return STRUCTURE_MARINE_ADVTURRETFACTORY; }
	if (inIUSER3 == AVH_USER3_TURRET)					{ return STRUCTURE_MARINE_TURRET; }
	if (inIUSER3 == AVH_USER3_SIEGETURRET)				{ return STRUCTURE_MARINE_SIEGETURRET; }
	if (inIUSER3 == AVH_USER3_ARMSLAB)					{ return STRUCTURE_MARINE_ARMSLAB; }
	if (inIUSER3 == AVH_USER3_PROTOTYPE_LAB)			{ return STRUCTURE_MARINE_PROTOTYPELAB; }
	if (inIUSER3 == AVH_USER3_OBSERVATORY)				{ return STRUCTURE_MARINE_OBSERVATORY; }
	if (inIUSER3 == AVH_USER3_PHASEGATE)				{ return STRUCTURE_MARINE_PHASEGATE; }

	if (inIUSER3 == AVH_USER3_HIVE)						{ return STRUCTURE_ALIEN_HIVE; }
	if (inIUSER3 == AVH_USER3_ALIENRESTOWER)			{ return STRUCTURE_ALIEN_RESTOWER; }
	if (inIUSER3 == AVH_USER3_DEFENSE_CHAMBER)			{ return STRUCTURE_ALIEN_DEFENCECHAMBER; }
	if (inIUSER3 == AVH_USER3_SENSORY_CHAMBER)			{ return STRUCTURE_ALIEN_SENSORYCHAMBER; }
	if (inIUSER3 == AVH_USER3_MOVEMENT_CHAMBER)			{ return STRUCTURE_ALIEN_MOVEMENTCHAMBER; }
	if (inIUSER3 == AVH_USER3_OFFENSE_CHAMBER)			{ return STRUCTURE_ALIEN_OFFENCECHAMBER; }

	return STRUCTURE_NONE;

}



int UTIL_StructureTypeToImpulseCommand(const NSStructureType StructureType)
{
	switch (StructureType)
	{
		case STRUCTURE_MARINE_ARMOURY:
			return IMPULSE_COMMANDER_BUILD_ARMOURY;
		case STRUCTURE_MARINE_ARMSLAB:
			return IMPULSE_COMMANDER_BUILD_ARMSLAB;
		case STRUCTURE_MARINE_COMMCHAIR:
			return IMPULSE_COMMANDER_BUILD_COMMCHAIR;
		case STRUCTURE_MARINE_INFANTRYPORTAL:
			return IMPULSE_COMMANDER_BUILD_INFANTRYPORTAL;
		case STRUCTURE_MARINE_OBSERVATORY:
			return IMPULSE_COMMANDER_BUILD_OBSERVATORY;
		case STRUCTURE_MARINE_PHASEGATE:
			return IMPULSE_COMMANDER_BUILD_PHASEGATE;
		case STRUCTURE_MARINE_PROTOTYPELAB:
			return IMPULSE_COMMANDER_BUILD_PROTOTYPELAB;
		case STRUCTURE_MARINE_RESTOWER:
			return IMPULSE_COMMANDER_BUILD_RESTOWER;
		case STRUCTURE_MARINE_SIEGETURRET:
			return IMPULSE_COMMANDER_BUILD_SIEGETURRET;
		case STRUCTURE_MARINE_TURRET:
			return IMPULSE_COMMANDER_BUILD_TURRET;
		case STRUCTURE_MARINE_TURRETFACTORY:
			return IMPULSE_COMMANDER_BUILD_TURRETFACTORY;

		case STRUCTURE_ALIEN_DEFENCECHAMBER:
			return IMPULSE_ALIEN_BUILD_DEFENCECHAMBER;
		case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
			return IMPULSE_ALIEN_BUILD_MOVEMENTCHAMBER;
		case STRUCTURE_ALIEN_SENSORYCHAMBER:
			return IMPULSE_ALIEN_BUILD_SENSORYCHAMBER;
		case STRUCTURE_ALIEN_OFFENCECHAMBER:
			return IMPULSE_ALIEN_BUILD_OFFENCECHAMBER;
		case STRUCTURE_ALIEN_RESTOWER:
			return IMPULSE_ALIEN_BUILD_RESTOWER;
		case STRUCTURE_ALIEN_HIVE:
			return IMPULSE_ALIEN_BUILD_HIVE;
		default:
			return 0;
			
	}

	return 0;
}




bool IsPlayerInReadyRoom(const edict_t* Player)
{
	return Player->v.playerclass == PLAYMODE_READYROOM;
}

void ReadyRoomThink(bot_t* pBot)
{

}



bool IsPlayerStunned(const edict_t* Player)
{
	return !FNullEnt(Player) && !IsPlayerDead(Player) && !IsPlayerDigesting(Player) && (Player->v.iuser4 & MASK_PLAYER_STUNNED);
}

bool IsPlayerSpectator(const edict_t* Player)
{
	return !FNullEnt(Player) && (Player->v.playerclass == PLAYMODE_OBSERVER);
}





void OnBotChangeClass(bot_t* pBot)
{
	if (pBot->bot_ns_class == CLASS_MARINE_COMMANDER)
	{
		for (int i = 0; i < 32; i++)
		{
			pBot->LastPlayerOrders[i].bIsActive = false;
			pBot->LastPlayerOrders[i].OrderType = ORDERTYPE_UNDEFINED;
			pBot->LastPlayerOrders[i].LastReminderTime = 0.0f;
		}
		
	}
}

int UTIL_CommanderFirstFreeActionIndex(bot_t* CommanderBot, int Priority)
{
	for (int i = 0; i < MAX_PRIORITY_ACTIONS; i++)
	{
		if (!CommanderBot->CurrentCommanderActions[Priority][i].bIsActive)
		{
			return i;
		}
	}

	return -1;
}



float UTIL_DistToNearestFriendlyPlayer(const Vector& Location, int DesiredTeam)
{
	float smallestDist = 0.0f;

	for (int i = 0; i < 32; i++)
	{

		if (!FNullEnt(clients[i]) && clients[i]->v.team == DesiredTeam && !IsPlayerCommander(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
		{
			float newDist = vDist2DSq(clients[i]->v.origin, Location);
			if (smallestDist == 0.0f || newDist < smallestDist)
			{
				smallestDist = newDist;
			}
		}
	}

	return sqrtf(smallestDist);
}









const char* UTIL_ResearchTypeToChar(const NSResearch ResearchType)
{
	switch (ResearchType)
	{
	case RESEARCH_NONE:
		return "None";
		break;
	case RESEARCH_OBSERVATORY_DISTRESSBEACON:
		return "Distress Beacon";
		break;
	case RESEARCH_OBSERVATORY_MOTIONTRACKING:
		return "Motion Tracking";
		break;
	case RESEARCH_OBSERVATORY_PHASETECH:
		return "Phase Tech";
		break;
	case RESEARCH_ARMSLAB_ARMOUR1:
		return "Armour Level 1";
		break;
	case RESEARCH_ARMSLAB_ARMOUR2:
		return "Armour Level 2";
		break;
	case RESEARCH_ARMSLAB_ARMOUR3:
		return "Armour Level 3";
		break;
	case RESEARCH_ARMSLAB_WEAPONS1:
		return "Weapons Level 1";
		break;
	case RESEARCH_ARMSLAB_WEAPONS2:
		return "Weapons Level 2";
		break;
	case RESEARCH_ARMSLAB_WEAPONS3:
		return "Weapons Level 3";
		break;
	case RESEARCH_PROTOTYPELAB_HEAVYARMOUR:
		return "Heavy Armour";
		break;
	case RESEARCH_PROTOTYPELAB_JETPACKS:
		return "Jetpacks";
		break;
	case RESEARCH_ARMOURY_GRENADES:
		return "Grenades";
		break;
	default:
		return "INVALID";

	}

	return "INVALID";
}

const char* UTIL_HiveTechToChar(const HiveTechStatus HiveTech)
{
	switch (HiveTech)
	{
		case HIVE_TECH_MOVEMENT:
			return "Movement";
		case HIVE_TECH_DEFENCE:
			return "Defence";
		case HIVE_TECH_SENSORY:
			return "Sensory";
		default:
			return "None";
	}
}

const char* UTIL_DroppableItemTypeToChar(const NSDeployableItem ItemType)
{
	switch (ItemType)
	{
		case ITEM_MARINE_AMMO:
			return "Ammo";
			break;
		case ITEM_MARINE_HEALTHPACK:
			return "Healthpack";
			break;
		case ITEM_MARINE_CATALYSTS:
			return "Catalysts";
			break;
		case ITEM_MARINE_GRENADELAUNCHER:
			return "Grenade Launcher";
			break;
		case ITEM_MARINE_HEAVYARMOUR:
			return "Heavy Armour";
			break;
		case ITEM_MARINE_HMG:
			return "HMG";
			break;
		case ITEM_MARINE_JETPACK:
			return "Jetpack";
			break;
		case ITEM_MARINE_MINES:
			return "Mines";
			break;
		case ITEM_MARINE_SCAN:
			return "Scan";
			break;
		case ITEM_MARINE_SHOTGUN:
			return "Shotgun";
			break;
		case ITEM_MARINE_WELDER:
			return "Welder";
			break;
		default:
			return "Invalid";
			break;
	}

	return "Invalid";
}

char* UTIL_WeaponTypeToClassname(const NSWeapon WeaponType)
{
	switch (WeaponType)
	{
		case WEAPON_MARINE_MG:
			return "weapon_machinegun";
		case WEAPON_MARINE_PISTOL:
			return "weapon_pistol";
		case WEAPON_MARINE_KNIFE:
			return "weapon_knife";
		case WEAPON_MARINE_SHOTGUN:
			return "weapon_shotgun";
		case WEAPON_MARINE_HMG:
			return "weapon_heavymachinegun";
		case WEAPON_MARINE_WELDER:
			return "weapon_welder";
		case WEAPON_MARINE_MINES:
			return "weapon_mine";
		case WEAPON_MARINE_GRENADE:
			return "weapon_grenade";
		case WEAPON_MARINE_GL:
			return "weapon_grenadegun";

		case WEAPON_SKULK_BITE:
			return "weapon_bitegun";
		case WEAPON_SKULK_PARASITE:
			return "weapon_parasite";
		case WEAPON_SKULK_LEAP:
			return "weapon_leap";
		case WEAPON_SKULK_XENOCIDE:
			return "weapon_divinewind";

		case WEAPON_GORGE_SPIT:
			return "weapon_spit";
		case WEAPON_GORGE_HEALINGSPRAY:
			return "weapon_healingspray";
		case WEAPON_GORGE_BILEBOMB:
			return "weapon_bilebombgun";
		case WEAPON_GORGE_WEB:
			return "weapon_webspinner";

		case WEAPON_LERK_BITE:
			return "weapon_bite2gun";
		case WEAPON_LERK_SPORES:
			return "weapon_spore";
		case WEAPON_LERK_UMBRA:
			return "weapon_umbra";
		case WEAPON_LERK_PRIMALSCREAM:
			return "weapon_primalscream";

		case WEAPON_FADE_SWIPE:
			return "weapon_swipe";
		case WEAPON_FADE_BLINK:
			return "weapon_blink";
		case WEAPON_FADE_METABOLIZE:
			return "weapon_metabolize";
		case WEAPON_FADE_ACIDROCKET:
			return "weapon_acidrocketgun";

		case WEAPON_ONOS_GORE:
			return "weapon_claws";
		case WEAPON_ONOS_DEVOUR:
			return "weapon_devour";
		case WEAPON_ONOS_STOMP:
			return "weapon_stomp";
		case WEAPON_ONOS_CHARGE:
			return "weapon_charge";
		default:
			return "";
	}

	return "";
}

const char* UTIL_StructTypeToChar(const NSStructureType StructureType)
{
	switch (StructureType)
	{
		case STRUCTURE_MARINE_COMMCHAIR:
			return "Comm Chair";
		case STRUCTURE_MARINE_RESTOWER:
			return "Marine Resource Tower";
		case STRUCTURE_MARINE_INFANTRYPORTAL:
			return "Infantry Portal";
		case STRUCTURE_MARINE_ARMOURY:
			return "Armoury";
		case STRUCTURE_MARINE_ADVARMOURY:
			return "Advanced Armoury";
		case STRUCTURE_MARINE_TURRETFACTORY:
			return "Turret Factory";
		case STRUCTURE_MARINE_ADVTURRETFACTORY:
			return "Advanced Turret Factory";
		case STRUCTURE_MARINE_TURRET:
			return "Turret";
		case STRUCTURE_MARINE_SIEGETURRET:
			return "Siege Turret";
		case STRUCTURE_MARINE_ARMSLAB:
			return "Arms Lab";
		case STRUCTURE_MARINE_PROTOTYPELAB:
			return "Prototype Lab";
		case STRUCTURE_MARINE_OBSERVATORY:
			return "Observatory";
		case STRUCTURE_MARINE_PHASEGATE:
			return "Phase Gate";

		case STRUCTURE_ALIEN_HIVE:
			return "Hive";
		case STRUCTURE_ALIEN_RESTOWER:
			return "Alien Resource Tower";
		case STRUCTURE_ALIEN_DEFENCECHAMBER:
			return "Defence Chamber";
		case STRUCTURE_ALIEN_SENSORYCHAMBER:
			return "Sensory Chamber";
		case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
			return "Movement Chamber";
		case STRUCTURE_ALIEN_OFFENCECHAMBER:
			return "Offence Chamber";

		default:
			return "INVALID";

	}
}

const char* UTIL_PlayerClassToChar(const NSPlayerClass PlayerClass)
{
	switch (PlayerClass)
	{
	case CLASS_MARINE:
		return "Marine";
	case CLASS_MARINE_COMMANDER:
		return "Marine Commander";
	case CLASS_SKULK:
		return "Skulk";
	case CLASS_GORGE:
		return "Gorge";
	case CLASS_LERK:
		return "Lerk";
	case CLASS_FADE:
		return "Fade";
	case CLASS_ONOS:
		return "Onos";
	case CLASS_EGG:
		return "Egg";
	case CLASS_NONE:
		return "None";
	default:
		return "INVALID";

	}

	return "INVALID";

}

bool UTIL_StructureIsResearching(const edict_t* Structure)
{
	if (!Structure) { return false; }

	float NormalisedProgress = ((Structure->v.fuser1 / kNormalizationNetworkFactor) - kResearchFuser1Base);
	float ClampedNormalizedProgress = clampf(NormalisedProgress, 0.0f, 1.0f);

	return ClampedNormalizedProgress > 0.0f && ClampedNormalizedProgress < 1.0f;
}

bool UTIL_StructureIsResearching(const edict_t* Structure, const NSResearch Research)
{
	if (!Structure) { return false; }

	return (UTIL_StructureIsResearching(Structure) && Structure->v.iuser2 == (int)Research);
}

















bool PlayerHasWeapon(const edict_t* Player, const NSWeapon DesiredCombatWeapon)
{
	switch (DesiredCombatWeapon)
	{
	case WEAPON_MARINE_MG:
	case WEAPON_MARINE_HMG:
	case WEAPON_MARINE_SHOTGUN:
	case WEAPON_MARINE_WELDER:
	case WEAPON_MARINE_GL:
	case WEAPON_MARINE_GRENADE:
	case WEAPON_MARINE_KNIFE:
	case WEAPON_MARINE_MINES:
	case WEAPON_MARINE_PISTOL:
		return (IsPlayerMarine(Player) && (Player->v.weapons & (1 << DesiredCombatWeapon)));

	case WEAPON_SKULK_BITE:
	case WEAPON_SKULK_PARASITE:
		return IsPlayerSkulk(Player);
	case WEAPON_SKULK_LEAP:
		return (IsPlayerSkulk(Player) && UTIL_GetNumActiveHives() >= 2);
	case WEAPON_SKULK_XENOCIDE:
		return (IsPlayerSkulk(Player) && UTIL_GetNumActiveHives() >= 3);

	case WEAPON_GORGE_SPIT:
	case WEAPON_GORGE_HEALINGSPRAY:
		return IsPlayerGorge(Player);
	case WEAPON_GORGE_BILEBOMB:
		return (IsPlayerGorge(Player) && UTIL_GetNumActiveHives() >= 2);
	case WEAPON_GORGE_WEB:
		return (IsPlayerGorge(Player) && UTIL_GetNumActiveHives() >= 3);

	case WEAPON_LERK_BITE:
	case WEAPON_LERK_SPORES:
		return IsPlayerLerk(Player);
	case WEAPON_LERK_UMBRA:
		return (IsPlayerLerk(Player) && UTIL_GetNumActiveHives() >= 2);
	case WEAPON_LERK_PRIMALSCREAM:
		return (IsPlayerLerk(Player) && UTIL_GetNumActiveHives() >= 3);

	case WEAPON_FADE_SWIPE:
	case WEAPON_FADE_BLINK:
		return IsPlayerFade(Player);
	case WEAPON_FADE_METABOLIZE:
		return (IsPlayerFade(Player) && UTIL_GetNumActiveHives() >= 2);
	case WEAPON_FADE_ACIDROCKET:
		return (IsPlayerFade(Player) && UTIL_GetNumActiveHives() >= 3);

	case WEAPON_ONOS_GORE:
	case WEAPON_ONOS_DEVOUR:
		return IsPlayerOnos(Player);
	case WEAPON_ONOS_STOMP:
		return (IsPlayerOnos(Player) && UTIL_GetNumActiveHives() >= 2);
	case WEAPON_ONOS_CHARGE:
		return (IsPlayerOnos(Player) && UTIL_GetNumActiveHives() >= 3);

	}

	return false;
}

bool BotHasWeapon(const bot_t* pBot, const NSWeapon DesiredCombatWeapon)
{
	switch (DesiredCombatWeapon)
	{
		case WEAPON_MARINE_MG:
		case WEAPON_MARINE_HMG:
		case WEAPON_MARINE_SHOTGUN:
		case WEAPON_MARINE_WELDER:
		case WEAPON_MARINE_GL:
		case WEAPON_MARINE_GRENADE:
		case WEAPON_MARINE_KNIFE:
		case WEAPON_MARINE_MINES:
		case WEAPON_MARINE_PISTOL:
			return (pBot->bot_ns_class == CLASS_MARINE && (pBot->pEdict->v.weapons & (1 << DesiredCombatWeapon)));

		case WEAPON_SKULK_BITE:
		case WEAPON_SKULK_PARASITE:
			return (pBot->bot_ns_class == CLASS_SKULK);
		case WEAPON_SKULK_LEAP:
			return (pBot->bot_ns_class == CLASS_SKULK && UTIL_GetNumActiveHives() >= 2);
		case WEAPON_SKULK_XENOCIDE:
			return (pBot->bot_ns_class == CLASS_SKULK && UTIL_GetNumActiveHives() >= 3);

		case WEAPON_GORGE_SPIT:
		case WEAPON_GORGE_HEALINGSPRAY:
			return (pBot->bot_ns_class == CLASS_GORGE);
		case WEAPON_GORGE_BILEBOMB:
			return (pBot->bot_ns_class == CLASS_GORGE && UTIL_GetNumActiveHives() >= 2);
		case WEAPON_GORGE_WEB:
			return (pBot->bot_ns_class == CLASS_GORGE && UTIL_GetNumActiveHives() >= 3);

		case WEAPON_LERK_BITE:
		case WEAPON_LERK_SPORES:
			return (pBot->bot_ns_class == CLASS_LERK);
		case WEAPON_LERK_UMBRA:
			return (pBot->bot_ns_class == CLASS_LERK && UTIL_GetNumActiveHives() >= 2);
		case WEAPON_LERK_PRIMALSCREAM:
			return (pBot->bot_ns_class == CLASS_LERK && UTIL_GetNumActiveHives() >= 3);

		case WEAPON_FADE_SWIPE:
		case WEAPON_FADE_BLINK:
			return (pBot->bot_ns_class == CLASS_FADE);
		case WEAPON_FADE_METABOLIZE:
			return (pBot->bot_ns_class == CLASS_FADE && UTIL_GetNumActiveHives() >= 2);
		case WEAPON_FADE_ACIDROCKET:
			return (pBot->bot_ns_class == CLASS_FADE && UTIL_GetNumActiveHives() >= 3);

		case WEAPON_ONOS_GORE:
		case WEAPON_ONOS_DEVOUR:
			return (pBot->bot_ns_class == CLASS_ONOS);
		case WEAPON_ONOS_STOMP:
			return (pBot->bot_ns_class == CLASS_ONOS && UTIL_GetNumActiveHives() >= 2);
		case WEAPON_ONOS_CHARGE:
			return (pBot->bot_ns_class == CLASS_ONOS && UTIL_GetNumActiveHives() >= 3);

	}
	
	return false;
}

int UTIL_GetCostOfStructureType(NSStructureType StructureType)
{
	switch (StructureType)
	{
	case STRUCTURE_MARINE_ARMOURY:
		return kArmoryCost;
		break;
	case STRUCTURE_MARINE_ARMSLAB:
		return kArmsLabCost;
		break;
	case STRUCTURE_MARINE_COMMCHAIR:
		return kCommandStationCost;
		break;
	case STRUCTURE_MARINE_INFANTRYPORTAL:
		return kInfantryPortalCost;
		break;
	case STRUCTURE_MARINE_OBSERVATORY:
		return kObservatoryCost;
		break;
	case STRUCTURE_MARINE_PHASEGATE:
		return kPhaseGateCost;
		break;
	case STRUCTURE_MARINE_PROTOTYPELAB:
		return kPrototypeLabCost;
		break;
	case STRUCTURE_MARINE_RESTOWER:
	case STRUCTURE_ALIEN_RESTOWER:
		return kResourceTowerCost;
		break;
	case STRUCTURE_MARINE_SIEGETURRET:
		return kSiegeCost;
		break;
	case STRUCTURE_MARINE_TURRET:
		return kSentryCost;
		break;
	case STRUCTURE_MARINE_TURRETFACTORY:
		return kTurretFactoryCost;
		break;
	case STRUCTURE_ALIEN_HIVE:
		return kHiveCost;
		break;
	case STRUCTURE_ALIEN_OFFENCECHAMBER:
		return kOffenseChamberCost;
		break;
	case STRUCTURE_ALIEN_DEFENCECHAMBER:
		return kDefenseChamberCost;
		break;
	case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
		return kMovementChamberCost;
		break;
	case STRUCTURE_ALIEN_SENSORYCHAMBER:
		return kSensoryChamberCost;
		break;
	default:
		return 0;

	}

	return 0;
}

bool UTIL_IsBotCommanderAssigned()
{
	if (!UTIL_CommChairExists()) { return true; }

	for (int i = 0; i < 32; i++)
	{
		if (!bots[i].is_used || FNullEnt(bots[i].pEdict)) { continue; }

		if (bots[i].CurrentRole == BOT_ROLE_COMMAND) { return true; }
	}

	return false;
}

void AssignCommander()
{
	Vector CommChairLocation = UTIL_GetCommChairLocation();

	if (vEquals(CommChairLocation, ZERO_VECTOR)) { return; }

	bot_t* closestBot = nullptr;
	float nearestDist;

	for (int i = 0; i < 32; i++)
	{
		if (bots[i].is_used && !FNullEnt(bots[i].pEdict) && IsPlayerOnMarineTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict) && !IsPlayerBeingDigested(bots[i].pEdict))
		{
			float dist = vDist2DSq(bots[i].pEdict->v.origin, CommChairLocation);
			if (!closestBot || dist < nearestDist)
			{
				closestBot = &bots[i];
				nearestDist = dist;
			}
		}
	}

	if (closestBot)
	{
		closestBot->CurrentRole = BOT_ROLE_COMMAND;
	}
}

void AssignGuardBot()
{
	Vector CommChairLocation = UTIL_GetCommChairLocation();

	if (vEquals(CommChairLocation, ZERO_VECTOR)) { return; }

	bot_t* closestBot = nullptr;
	float nearestDist;

	for (int i = 0; i < 32; i++)
	{
		if (bots[i].is_used && !FNullEnt(bots[i].pEdict) && (bots[i].CurrentRole != BOT_ROLE_COMMAND) && IsPlayerOnMarineTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict) && !IsPlayerBeingDigested(bots[i].pEdict))
		{
			float dist = vDist2DSq(bots[i].pEdict->v.origin, CommChairLocation);
			if (!closestBot || dist < nearestDist)
			{
				closestBot = &bots[i];
				nearestDist = dist;
			}
		}
	}

	if (closestBot)
	{
		closestBot->CurrentRole = BOT_ROLE_GUARD_BASE;
		UTIL_ClearGuardInfo(closestBot);
	}
}

bool UTIL_IsThereACommander()
{
	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && IsPlayerCommander(clients[i]))
		{
			return true;
		}
	}

	return false;
}

int UTIL_GetBotsWithRoleType(BotRole RoleType, bool bMarines)
{
	int Result = 0;

	for (int i = 0; i < 32; i++)
	{
		if (!bots[i].is_used || FNullEnt(bots[i].pEdict)) { continue; }

		if (bMarines)
		{
			if (IsPlayerOnMarineTeam(bots[i].pEdict) && bots[i].CurrentRole == RoleType)
			{
				Result++;
			}
		}
		else
		{
			if (IsPlayerOnAlienTeam(bots[i].pEdict) && bots[i].CurrentRole == RoleType)
			{
				Result++;
			}
		}

	}

	return Result;
}

float UTIL_GetDesiredDistanceToUseEntity(const bot_t* pBot, const edict_t* Entity)
{
	if (FNullEnt(Entity)) { return 0.0f; }

	TraceResult hit;

	UTIL_TraceLine(pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(Entity), dont_ignore_monsters, pBot->pEdict->v.pContainingEntity, &hit);

	if (hit.flFraction < 1.0f && hit.pHit == Entity)
	{
		float EntityRadius = vDist2D(hit.vecEndPos, Entity->v.origin) - 5.0f;
		float BotRadius = UTIL_GetPlayerRadius(pBot->pEdict) - 1.0f;

		return (EntityRadius + BotRadius + max_player_use_reach);
	}
	else
	{
		float EntityRadius = vDist2D(Entity->v.absmin, UTIL_GetCentreOfEntity(Entity));
		float BotRadius = UTIL_GetPlayerRadius(pBot->pEdict);

		return (EntityRadius + BotRadius + max_player_use_reach);
	}
}

bool UTIL_IsAnyHumanOnMarineTeam()
{
	for (int i = 0; i < 32; i++)
	{
		if (!clients[i]) { continue; }

		if (!IsPlayerBot(clients[i]) && IsPlayerOnMarineTeam(clients[i]))
		{
			return true;
		}
	}

	return false;
}

bool UTIL_IsAnyHumanOnAlienTeam()
{
	for (int i = 0; i < 32; i++)
	{
		if (!clients[i]) { continue; }

		if (!IsPlayerBot(clients[i]) && IsPlayerOnAlienTeam(clients[i]))
		{
			return true;
		}
	}

	return false;
}

bool UTIL_StructureIsRecycling(const edict_t* Structure)
{
	return (Structure && (Structure->v.iuser4 & MASK_RECYCLING));
}

bool UTIL_StructureExistsOfType(const NSStructureType StructureType)
{
	return (UTIL_GetStructureCountOfType(StructureType) > 0);
}

bool CommanderProgressResearchAction(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	if (UTIL_ResearchInProgress(action->ResearchId) || !UTIL_MarineResearchIsAvailable(action->ResearchId))
	{
		UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
		return false;
	}

	return BotCommanderResearchTech(CommanderBot, ActionIndex, Priority);

}

bool CommanderProgressItemDropAction(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	if (!FNullEnt(action->StructureOrItem))
	{
		UTIL_SayText("Item already dropped!\n", clients[0]);
		UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
		return false;
	}

	return BotCommanderDropItem(CommanderBot, ActionIndex, Priority);
}

bool UTIL_StructureIsUpgrading(const edict_t* Structure)
{
	NSStructureType StructureType = UTIL_GetStructureTypeFromEdict(Structure);

	if (StructureType == STRUCTURE_MARINE_ARMOURY)
	{
		return UTIL_IsArmouryUpgrading(Structure);
	}

	if (StructureType == STRUCTURE_MARINE_TURRETFACTORY)
	{
		return UTIL_IsTurretFactoryUpgrading(Structure);
	}

	return false;
}

AvHUpgradeMask UTIL_GetResearchMask(const NSResearch Research)
{
	switch (Research)
	{
		case RESEARCH_ARMSLAB_ARMOUR1:
			return MASK_UPGRADE_5;
		case RESEARCH_ARMSLAB_ARMOUR2:
			return MASK_UPGRADE_6;
		case RESEARCH_ARMSLAB_ARMOUR3:
			return MASK_UPGRADE_7;
		case RESEARCH_ARMSLAB_WEAPONS1:
			return MASK_UPGRADE_1;
		case RESEARCH_ARMSLAB_WEAPONS2:
			return MASK_UPGRADE_2;
		case RESEARCH_ARMSLAB_WEAPONS3:
			return MASK_UPGRADE_3;
		case RESEARCH_ARMSLAB_CATALYSTS:
			return MASK_UPGRADE_4;
		case RESEARCH_ARMOURY_GRENADES:
			return MASK_UPGRADE_5;
		case RESEARCH_OBSERVATORY_DISTRESSBEACON:
			return MASK_UPGRADE_5;
		case RESEARCH_OBSERVATORY_MOTIONTRACKING:
			return MASK_UPGRADE_6;
		case RESEARCH_OBSERVATORY_PHASETECH:
			return MASK_UPGRADE_2;
		case RESEARCH_PROTOTYPELAB_HEAVYARMOUR:
			return MASK_UPGRADE_5;
		case RESEARCH_PROTOTYPELAB_JETPACKS:
			return MASK_UPGRADE_1;
		default:
			return MASK_NONE;

	}
}

NSStructureType UTIL_GetStructureTypeFromEdict(const edict_t* StructureEdict)
{
	if (FNullEnt(StructureEdict)) { return STRUCTURE_NONE; }

	return UTIL_IUSER3ToStructureType(StructureEdict->v.iuser3);
}

HiveTechStatus UTIL_GetTechForChamber(NSStructureType ChamberToConstruct)
{
	switch (ChamberToConstruct)
	{
		case STRUCTURE_ALIEN_DEFENCECHAMBER:
			return HIVE_TECH_DEFENCE;
		case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
			return HIVE_TECH_MOVEMENT;
		case STRUCTURE_ALIEN_SENSORYCHAMBER:
			return HIVE_TECH_SENSORY;
		default: return HIVE_TECH_NONE;
	}

	return HIVE_TECH_NONE;
}

bool UTIL_IsEdictPlayer(const edict_t* edict)
{
	for (int i = 0; i < 32; i++)
	{
		if (clients[i] == edict) { return true; }
	}

	return false;
}

void AlienReceiveAlert(bot_t* pBot, const Vector Location, const PlayerAlertType AlertType)
{
	if (IsPlayerGorge(pBot->pEdict) || pBot->CurrentRole == BOT_ROLE_BUILDER) { return; }

	switch (AlertType)
	{
		case HUD_SOUND_ALIEN_RESOURCES_ATTACK:
		case HUD_SOUND_ALIEN_STRUCTURE_ATTACK:
			ReceiveStructureAttackAlert(pBot, Location);
			break;
		case HUD_SOUND_ALIEN_HIVE_ATTACK:
			ReceiveHiveAttackAlert(pBot, UTIL_GetNearestHiveAtLocation(Location)->edict);
			break;
		default:
			break;
	}
}

void ReceiveHiveAttackAlert(bot_t* pBot, edict_t* HiveEdict)
{
	if (IsPlayerGorge(pBot->pEdict) || FNullEnt(HiveEdict)) { return; }

	if (UTIL_GetHiveStatus(HiveEdict) != HIVE_STATUS_UNBUILT)
	{
		float ThisDist = vDist2D(pBot->pEdict->v.origin, HiveEdict->v.origin);

		int NumDefenders = UTIL_GetNumPlayersOfTeamInArea(HiveEdict->v.origin, ThisDist - 1.0f, ALIEN_TEAM, pBot->pEdict, CLASS_GORGE);

		if (NumDefenders < 3)
		{
			pBot->PrimaryBotTask.TaskType = TASK_DEFEND;
			pBot->PrimaryBotTask.bOrderIsUrgent = true;
			pBot->PrimaryBotTask.TaskLocation = HiveEdict->v.origin;
			pBot->PrimaryBotTask.TaskTarget = HiveEdict;
		}
	}
}

void ReceiveStructureAttackAlert(bot_t* pBot, const Vector& AttackLocation)
{

	if (IsPlayerGorge(pBot->pEdict) || IsPlayerCommander(pBot->pEdict)) { return; }

	float DistFromAlert = vDist2DSq(pBot->pEdict->v.origin, AttackLocation);

	edict_t* AttackedStructure = UTIL_GetClosestStructureAtLocation(AttackLocation, IsPlayerOnMarineTeam(pBot->pEdict));

	if (FNullEnt(AttackedStructure)) { return; }

	NSStructureType DamagedStructureType = UTIL_GetStructureTypeFromEdict(AttackedStructure);

	float MaxRelevantDist = (DamagedStructureType == STRUCTURE_MARINE_RESTOWER || DamagedStructureType == STRUCTURE_ALIEN_RESTOWER) ? sqrf(UTIL_MetresToGoldSrcUnits(100.0f)) : sqrf(UTIL_MetresToGoldSrcUnits(50.0f));

	float ThisDist = vDist2DSq(pBot->pEdict->v.origin, AttackedStructure->v.origin);

	// First check that we're close enough to bother with this
	if (ThisDist <= MaxRelevantDist)
	{

		float DistActual = sqrtf(ThisDist);
		// If there are potential defenders closer to the attack then don't bother
		int NumDefenders = UTIL_GetNumPlayersOfTeamInArea(AttackedStructure->v.origin, DistActual - 1.0f, pBot->pEdict->v.team, pBot->pEdict, CLASS_GORGE);

		if (NumDefenders >= 2) 
		{
			return;
		}

		if (!pBot->SecondaryBotTask.bOrderIsUrgent)
		{
			pBot->SecondaryBotTask.TaskType = TASK_DEFEND;
			pBot->SecondaryBotTask.bOrderIsUrgent = true;
			pBot->SecondaryBotTask.TaskLocation = AttackedStructure->v.origin;
			pBot->SecondaryBotTask.TaskTarget = AttackedStructure;
		}
	}
}

void AlienGuardLocation(bot_t* pBot, const Vector Location)
{
	if (Location != pBot->CurrentGuardLocation || pBot->NumGuardPoints == 0)
	{
		UTIL_ClearGuardInfo(pBot);
		UTIL_GenerateGuardWatchPoints(pBot, Location);

		pBot->CurrentGuardLocation = Location;
	}

	if (!pBot->GuardLookLocation)
	{
		int NewGuardLookIndex = irandrange(0, (pBot->NumGuardPoints - 1));

		pBot->GuardLookLocation = pBot->GuardPoints[NewGuardLookIndex];
		LookAt(pBot, pBot->GuardLookLocation);
		pBot->GuardStartLookTime = gpGlobals->time;
		pBot->ThisGuardLookTime = frandrange(2.0f, 5.0f);
	}

	if ((gpGlobals->time - pBot->GuardStartLookTime) > pBot->ThisGuardLookTime)
	{
		if (pBot->NumGuardPoints == 0) { return; }
		int NewGuardLookIndex = irandrange(0, (pBot->NumGuardPoints - 1));

		pBot->GuardLookLocation = pBot->GuardPoints[NewGuardLookIndex];
		pBot->LookTargetLocation = pBot->GuardLookLocation;

		pBot->GuardStartLookTime = gpGlobals->time;
		pBot->ThisGuardLookTime = frandrange(2.0f, 5.0f);

		Vector LookDir = UTIL_GetVectorNormal2D(pBot->GuardLookLocation - Location);
		Vector NewMoveCentre = Location - (LookDir * UTIL_MetresToGoldSrcUnits(2.0f));

		Vector NewMoveLoc = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, NewMoveCentre, UTIL_MetresToGoldSrcUnits(2.0f));

		pBot->CurrentGuardLocation = NewMoveLoc;
		MoveTo(pBot, pBot->CurrentGuardLocation, MOVESTYLE_NORMAL);
	}
	else
	{
		MoveTo(pBot, pBot->CurrentGuardLocation, MOVESTYLE_NORMAL);
		LookAt(pBot, pBot->GuardLookLocation);

	}

}

void BotSwitchToWeapon(bot_t* pBot, NSWeapon NewWeaponSlot)
{
	char* WeaponName = UTIL_WeaponTypeToClassname(NewWeaponSlot);

	FakeClientCommand(pBot->pEdict, WeaponName, NULL, NULL);
}



int GetPlayerMaxArmour(const edict_t* Player)
{

	if (IsPlayerMarine(Player))
	{
		int BaseArmourLevel = (UTIL_PlayerHasHeavyArmour(Player)) ? kMarineBaseHeavyArmor : kMarineBaseArmor;
		int BaseArmourUpgrade = (UTIL_PlayerHasHeavyArmour(Player)) ? kMarineHeavyArmorUpgrade : kMarineBaseArmorUpgrade;
		int ArmourLevel = 0;
		
		if (Player->v.iuser4 & MASK_UPGRADE_6)
		{
			ArmourLevel = 3;
		}
		else if (Player->v.iuser4 & MASK_UPGRADE_5)
		{
			ArmourLevel = 2;
		}
		else if (Player->v.iuser4 & MASK_UPGRADE_4)
		{
			ArmourLevel = 1;
		}

		// We floor it, as we'd rather the max be incorrectly calculated as 1 less than 1 more in case fellow marines keep trying to weld them while at full
		return BaseArmourLevel + (int)floorf((ArmourLevel * 0.33333f) * BaseArmourUpgrade);

	}
	else
	{
		NSPlayerClass PlayerClass = UTIL_GetPlayerClass(Player);

		int ArmourLevel = 0;

		if (Player->v.iuser4 & MASK_UPGRADE_1)
		{
			ArmourLevel = 1;

			if (Player->v.iuser4 & MASK_UPGRADE_11)
			{
				ArmourLevel = 3;
			}
			else if (Player->v.iuser4 & MASK_UPGRADE_10)
			{
				ArmourLevel = 2;
			}
		}

		// We floor the value, as we'd rather the max be incorrectly calculated as 1 less than 1 more in case alien keeps trying to heal when at full armour
		switch (PlayerClass)
		{
			case CLASS_EGG:
				return kGestateBaseArmor;
			case CLASS_SKULK:
				return kSkulkBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kSkulkArmorUpgrade);
			case CLASS_GORGE:
				return kGorgeBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kGorgeArmorUpgrade);
			case CLASS_LERK:
				return kLerkBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kLerkArmorUpgrade);
			case CLASS_FADE:
				return kFadeBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kFadeArmorUpgrade);
			case CLASS_ONOS:
				return kOnosBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kOnosArmorUpgrade);
			default:
				return 0;
		}
	}

	return 0;
}
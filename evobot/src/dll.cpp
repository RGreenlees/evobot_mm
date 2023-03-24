//
// HPB bot - botman's High Ping Bastard bot
//
// (http://planethalflife.com/botman/)
//
// dll.cpp
//

#ifndef _WIN32
#include <string.h>
#endif

#include <time.h>
#include <string>
#include <fstream>

#include <extdll.h>
#include <dllapi.h>
#include <h_export.h>
#include <meta_api.h>
#include <entity_state.h>

#include "bot.h"
#include "bot_func.h"
#include "bot_navigation.h"
#include "bot_config.h"
#include "bot_commander.h"
#include "bot_tactical.h"

extern DLL_FUNCTIONS gFunctionTable;
extern enginefuncs_t g_engfuncs;
extern globalvars_t  *gpGlobals;
extern char g_argv[1024];
extern int numSectors;

extern bot_weapon_t weapon_defs[MAX_WEAPONS];

extern dropped_marine_item AllMarineItems[256];
extern int NumTotalMarineItems;

extern resource_node ResourceNodes[64];
extern int NumTotalResNodes;

extern hive_definition Hives[10];
extern int NumTotalHives;

extern map_location MapLocations[32];
extern int NumMapLocations;

extern bot_t bots[32];

extern bool bGameIsActive;
float GameStartTime;

EvobotDebugMode CurrentDebugMode = EVO_DEBUG_NONE;

bool bGameHasStarted = false;

static FILE *fp;

int m_spriteTexture = 0;

bool isFakeClientCommand = false;
int fake_arg_count;
int IsDedicatedServer;
edict_t *clients[32];
edict_t *listenserver_edict = NULL;
float welcome_time = 0.0;
bool welcome_sent = false;
int bot_stop = 0;

void EvoBot_ServerCommand(void);

// START of Metamod stuff

enginefuncs_t meta_engfuncs;
gamedll_funcs_t *gpGamedllFuncs;
mutil_funcs_t *gpMetaUtilFuncs;
meta_globals_t *gpMetaGlobals;

META_FUNCTIONS gMetaFunctionTable =
{
	NULL, // pfnGetEntityAPI()
	NULL, // pfnGetEntityAPI_Post()
	GetEntityAPI2, // pfnGetEntityAPI2()
	NULL, // pfnGetEntityAPI2_Post()
	NULL, // pfnGetNewDLLFunctions()
	NULL, // pfnGetNewDLLFunctions_Post()
	GetEngineFunctions, // pfnGetEngineFunctions()
	NULL, // pfnGetEngineFunctions_Post()
};

plugin_info_t Plugin_info = {
	META_INTERFACE_VERSION, // interface version
	"CS_Bot", // plugin name
	"1.0.0", // plugin version
	"29/06/2015", // date of creation
	"Neoptolemus", // plugin author
	"http://hpb-bot.bots-united.com/", // plugin URL
	"CS_BOT", // plugin logtag
	PT_STARTUP, // when loadable
	PT_ANYTIME, // when unloadable
};


C_DLLEXPORT int Meta_Query(char *ifvers, plugin_info_t **pPlugInfo, mutil_funcs_t *pMetaUtilFuncs)
{
	// this function is the first function ever called by metamod in the plugin DLL. Its purpose
	// is for metamod to retrieve basic information about the plugin, such as its meta-interface
	// version, for ensuring compatibility with the current version of the running metamod.

	// keep track of the pointers to metamod function tables metamod gives us
	gpMetaUtilFuncs = pMetaUtilFuncs;
	*pPlugInfo = &Plugin_info;

	// check for interface version compatibility
	if (strcmp(ifvers, Plugin_info.ifvers) != 0)
	{
		int mmajor = 0, mminor = 0, pmajor = 0, pminor = 0;

		LOG_CONSOLE(PLID, "%s: meta-interface version mismatch (metamod: %s, %s: %s)", Plugin_info.name, ifvers, Plugin_info.name, Plugin_info.ifvers);
		LOG_MESSAGE(PLID, "%s: meta-interface version mismatch (metamod: %s, %s: %s)", Plugin_info.name, ifvers, Plugin_info.name, Plugin_info.ifvers);

		// if plugin has later interface version, it's incompatible (update metamod)
		sscanf(ifvers, "%d:%d", &mmajor, &mminor);
		sscanf(META_INTERFACE_VERSION, "%d:%d", &pmajor, &pminor);

		if ((pmajor > mmajor) || ((pmajor == mmajor) && (pminor > mminor)))
		{
			LOG_CONSOLE(PLID, "metamod version is too old for this plugin; update metamod");
			LOG_ERROR(PLID, "metamod version is too old for this plugin; update metamod");
			return false;
		}

		// if plugin has older major interface version, it's incompatible (update plugin)
		else if (pmajor < mmajor)
		{
			LOG_CONSOLE(PLID, "metamod version is incompatible with this plugin; please find a newer version of this plugin");
			LOG_ERROR(PLID, "metamod version is incompatible with this plugin; please find a newer version of this plugin");
			return false;
		}
	}

	return true; // tell metamod this plugin looks safe
}


C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME now, META_FUNCTIONS *pFunctionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs)
{
	// this function is called when metamod attempts to load the plugin. Since it's the place
	// where we can tell if the plugin will be allowed to run or not, we wait until here to make
	// our initialization stuff, like registering CVARs and dedicated server commands.

	// are we allowed to load this plugin now ?
	if (now > Plugin_info.loadable)
	{
		LOG_CONSOLE(PLID, "%s: plugin NOT attaching (can't load plugin right now)", Plugin_info.name);
		LOG_ERROR(PLID, "%s: plugin NOT attaching (can't load plugin right now)", Plugin_info.name);
		return false; // returning FALSE prevents metamod from attaching this plugin
	}

	// keep track of the pointers to engine function tables metamod gives us
	gpMetaGlobals = pMGlobals;
	memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));
	gpGamedllFuncs = pGamedllFuncs;

	// print a message to notify about plugin attaching
	LOG_CONSOLE(PLID, "%s: plugin attaching", Plugin_info.name);
	LOG_MESSAGE(PLID, "%s: plugin attaching", Plugin_info.name);

	// ask the engine to register the server commands this plugin uses
	REG_SVR_COMMAND("evobot", EvoBot_ServerCommand);

	return true; // returning TRUE enables metamod to attach this plugin
}


C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME now, PL_UNLOAD_REASON reason)
{
	// this function is called when metamod unloads the plugin. A basic check is made in order
	// to prevent unloading the plugin if its processing should not be interrupted.

	// is metamod allowed to unload the plugin ?
	if ((now > Plugin_info.unloadable) && (reason != PNL_CMD_FORCED))
	{
		LOG_CONSOLE(PLID, "%s: plugin NOT detaching (can't unload plugin right now)", Plugin_info.name);
		LOG_ERROR(PLID, "%s: plugin NOT detaching (can't unload plugin right now)", Plugin_info.name);
		return false; // returning FALSE prevents metamod from unloading this plugin
	}

	return true; // returning TRUE enables metamod to unload this plugin
}

// END of Metamod stuff



void GameDLLInit(void)
{
	int i;

	IsDedicatedServer = IS_DEDICATED_SERVER();

	for (i = 0; i < 32; i++)
		clients[i] = NULL;

	// initialize the bots array of structures...
	memset(bots, 0, sizeof(bots));

	//BotNameInit();


	RETURN_META(MRES_IGNORED);
}

int Spawn(edict_t *pent)
{
	int index;

	if (gpGlobals->deathmatch)
	{
		char *pClassname = (char *)STRING(pent->v.classname);

		if (strcmp(pClassname, "worldspawn") == 0)
		{
			// do level initialization stuff here...

			UnloadNavigationData();

			for (int i = 0; i < 32; i++)
			{
				UTIL_ClearAllBotData(&bots[i]);
			}

			bGameHasStarted = false;
			bGameIsActive = false;
			GameStartTime = 0.0f;

			m_spriteTexture = PRECACHE_MODEL("sprites/zbeam6.spr");

			last_think_time = 0.0f;

			ParseConfigFile(false);
		}

	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void KeyValue(edict_t *pentKeyvalue, KeyValueData *pkvd)
{

	RETURN_META(MRES_IGNORED);
}

BOOL ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128])
{
	if (gpGlobals->deathmatch)
	{
		// check if this client is the listen server client
		if (strcmp(pszAddress, "loopback") == 0)
		{
			// save the edict of the listen server client...
			listenserver_edict = pEntity;
		}

	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void ClientDisconnect(edict_t *pEntity)
{
	if (gpGlobals->deathmatch)
	{
		int i;

		i = 0;
		while ((i < 32) && (clients[i] != pEntity))
			i++;

		if (i < 32)
			clients[i] = NULL;


		for (i = 0; i < 32; i++)
		{
			if (bots[i].pEdict == pEntity)
			{
				// someone kicked this bot off of the server...

				bots[i].is_used = false;  // this slot is now free to use

				UTIL_ClearAllBotData(&bots[i]);

				bots[i].pEdict = nullptr;

				break;
			}
		}
	}

	RETURN_META(MRES_IGNORED);
}

void ClientPutInServer(edict_t *pEntity)
{
	int i = 0;

	while ((i < 32) && (clients[i] != NULL))
		i++;

	if (i < 32)
		clients[i] = pEntity;  // store this clients edict in the clients array

	RETURN_META(MRES_IGNORED);
}


void ClientCommand(edict_t *pEntity)
{
	const char *pcmd = CMD_ARGV(0);
	const char *arg1 = CMD_ARGV(1);
	const char *arg2 = CMD_ARGV(2);
	const char *arg3 = CMD_ARGV(3);
	const char *arg4 = CMD_ARGV(4);
	const char *arg5 = CMD_ARGV(5);

	// only allow custom commands if deathmatch mode and NOT dedicated server and
	// client sending command is the listen server client...

	if (!gpGlobals->deathmatch || IsDedicatedServer || pEntity != listenserver_edict)
	{
		return;
	}

	if (FStrEq(pcmd, "whichhive"))
	{
		for (int i = 0; i < NumTotalHives; i++)
		{
			if (Hives[i].Status != HIVE_STATUS_UNBUILT)
			{
				char buf[32];
				sprintf(buf, "%s\n", UTIL_GetClosestMapLocationToPoint(Hives[i].Location));
				UTIL_SayText(buf, pEntity);
			}
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "amicloaked"))
	{
		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used && !FNullEnt(bots[i].pEdict))
			{
				char buf[32];
				sprintf(buf, "rmode: %d, ramt = %f\n", bots[i].pEdict->v.rendermode, bots[i].pEdict->v.renderamt);
				UTIL_SayText(buf, pEntity);
			}

		}


		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "testbackwardspath"))
	{
		DEBUG_TestBackwardsPathFind(pEntity, UTIL_GetCommChairLocation());



		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "traceentity"))
	{

		Vector TraceStart = UTIL_GetPlayerEyePosition(pEntity);
		Vector LookDir = UTIL_GetForwardVector(pEntity->v.v_angle);

		Vector TraceEnd = TraceStart + (LookDir * 1000.0f);

		TraceResult Hit;

		UTIL_TraceLine(TraceStart, TraceEnd, dont_ignore_monsters, dont_ignore_glass, pEntity, &Hit);

		if (Hit.flFraction < 1.0f)
		{
			char buf[64];
			sprintf(buf, "Hit Entity: %s\n", STRING(Hit.pHit->v.classname));
			UTIL_SayText(buf, pEntity);

			NSStructureType StructType = UTIL_IUSER3ToStructureType(Hit.pHit->v.iuser3);

			sprintf(buf, "StructureType: %s\n", UTIL_StructTypeToChar(StructType));
			UTIL_SayText(buf, pEntity);
		}
		else
		{
			UTIL_SayText("Hit nothing", pEntity);
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "grenadethrow"))
	{
		for (int i = 0; i < 32; i++)
		{
			if (bots[i].is_used && IsPlayerMarine(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))  // not respawning
			{
				Vector GrenadeLoc = UTIL_GetGrenadeThrowTarget(&bots[i], pEntity->v.origin, UTIL_MetresToGoldSrcUnits(3.0f));

				if (GrenadeLoc != ZERO_VECTOR)
				{
					bots[i].PrimaryBotTask.TaskType = TASK_GRENADE;
					bots[i].PrimaryBotTask.TaskLocation = GrenadeLoc;
					bots[i].PrimaryBotTask.bOrderIsUrgent = true;

				}
			}

		}
		

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "randomangle"))
	{
		Vector CurrentAngle = UTIL_GetForwardVector(pEntity->v.v_angle);

		Vector newAngle = random_unit_vector_within_cone(CurrentAngle, 0.087);

		Vector PlayerEyeLocation = UTIL_GetPlayerEyePosition(pEntity);

		UTIL_DrawLine(pEntity, PlayerEyeLocation, PlayerEyeLocation + (CurrentAngle * 1000.0f), 10.0f, 255, 0, 0);

		UTIL_DrawLine(pEntity, PlayerEyeLocation, PlayerEyeLocation + (newAngle * 1000.0f), 10.0f, 255, 255, 0);


		RETURN_META(MRES_SUPERCEDE);
	}


	if (FStrEq(pcmd, "showdoors"))
	{
		FILE* DoorLog = fopen("DoorLog.txt", "w+");

		if (!DoorLog)
		{
			RETURN_META(MRES_SUPERCEDE);
		}

		edict_t* currDoor = NULL;
		while (((currDoor = UTIL_FindEntityByClassname(currDoor, "func_door")) != NULL) && (!FNullEnt(currDoor)))
		{

			Vector StartLine = pEntity->v.origin;
			Vector EndX = StartLine + Vector(50.0f, 0.0f, 0.0f);
			Vector EndY = StartLine + Vector(0.0f, 50.0f, 0.0f);
			Vector EndZ = StartLine + Vector(0.0f, 0.0f, 50.0f);

			Vector LocalMin = currDoor->v.mins;
			Vector LocalMax = currDoor->v.maxs;

			UTIL_DrawLine(pEntity, StartLine, EndX, 10.0f, 255, 0, 0);
			UTIL_DrawLine(pEntity, StartLine, EndY, 10.0f, 255, 255, 0);
			UTIL_DrawLine(pEntity, StartLine, EndZ, 10.0f, 0, 0, 255);


			//UTIL_DrawLine(pEntity, currDoor->v.mins, currDoor->v.maxs, 10.0f, 255, 0, 0);
			UTIL_DrawLine(pEntity, currDoor->v.absmin, currDoor->v.absmax, 10.0f, 255, 255, 255);

			fprintf(DoorLog, "Door %s origin: (%f, %f, %f)\n", STRING(currDoor->v.targetname), currDoor->v.origin.x, currDoor->v.origin.y, currDoor->v.origin.z);
			fprintf(DoorLog, "Door %s containing entity: %s, %s\n", STRING(currDoor->v.targetname), STRING(currDoor->v.pContainingEntity->v.targetname), STRING(currDoor->v.pContainingEntity->v.classname));
			fprintf(DoorLog, "Door %s v_angle: (%f, %f, %f)\n", STRING(currDoor->v.targetname), currDoor->v.v_angle.x, currDoor->v.v_angle.y, currDoor->v.v_angle.z);
			fprintf(DoorLog, "Door %s ideal_yaw: %f\n", STRING(currDoor->v.targetname), currDoor->v.ideal_yaw);
			fprintf(DoorLog, "Door %s: (%f, %f, %f) - (%f, %f, %f)\n", STRING(currDoor->v.targetname), currDoor->v.mins.x, currDoor->v.mins.y, currDoor->v.mins.z, currDoor->v.maxs.x, currDoor->v.maxs.y, currDoor->v.maxs.z);
			fprintf(DoorLog, "Door %s: (%f, %f, %f) - (%f, %f, %f)\n\n", STRING(currDoor->v.targetname), currDoor->v.absmin.x, currDoor->v.absmin.y, currDoor->v.absmin.z, currDoor->v.absmax.x, currDoor->v.absmax.y, currDoor->v.absmax.z);
		}

		fclose(DoorLog);

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "botnavinfo"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		edict_t* SpectatorTarget = INDEXENT(pEntity->v.iuser2);

		if (FNullEnt(SpectatorTarget))
		{
			UTIL_SayText("No Spectator Target\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		int BotIndex = UTIL_GetBotIndex(SpectatorTarget);

		if (BotIndex < 0)
		{
			UTIL_SayText("Not spectating a bot\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		bot_t* pBot = &bots[BotIndex];

		char buf[150];

		if (pBot->BotNavInfo.UnstuckMoveLocation != ZERO_VECTOR)
		{
			UTIL_DrawLine(clients[0], pBot->pEdict->v.origin, pBot->BotNavInfo.UnstuckMoveLocation, 20.0f, 255, 0, 0);
		}

		BotDrawPath(pBot, 20.0f, false);

		sprintf(buf, "Path Status: %s\n", pBot->PathStatus);
		UTIL_SayText(buf, pEntity);

		sprintf(buf, "Move Status: %s\n", pBot->MoveStatus);
		UTIL_SayText(buf, pEntity);

		RETURN_META(MRES_SUPERCEDE);
	}


	if (FStrEq(pcmd, "bottaskinfo"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		edict_t* SpectatorTarget = INDEXENT(pEntity->v.iuser2);

		if (FNullEnt(SpectatorTarget))
		{
			UTIL_SayText("No Spectator Target\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		int BotIndex = UTIL_GetBotIndex(SpectatorTarget);

		if (BotIndex < 0)
		{
			UTIL_SayText("Not spectating a bot\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		bot_t* pBot = &bots[BotIndex];

		char buf[64];
					
		if (pBot->CurrentTask)
		{
			sprintf(buf, "Bot Role: %s\n", UTIL_BotRoleToChar(pBot->CurrentRole));
			UTIL_SayText(buf, listenserver_edict);

			sprintf(buf, "Primary Task: %s\n", UTIL_TaskTypeToChar(pBot->PrimaryBotTask.TaskType));
			UTIL_SayText(buf, listenserver_edict);

			sprintf(buf, "Second Task: %s\n", UTIL_TaskTypeToChar(pBot->SecondaryBotTask.TaskType));
			UTIL_SayText(buf, listenserver_edict);

			sprintf(buf, "Want Task: %s\n", UTIL_TaskTypeToChar(pBot->WantsAndNeedsTask.TaskType));
			UTIL_SayText(buf, listenserver_edict);

			sprintf(buf, "Current Task: %s\n", UTIL_TaskTypeToChar(pBot->CurrentTask->TaskType));
			UTIL_SayText(buf, listenserver_edict);

			if (pBot->CurrentTask->TaskType == TASK_BUILD)
			{
				sprintf(buf, "Build: %s\n", UTIL_StructTypeToChar(pBot->CurrentTask->StructureType));
				UTIL_SayText(buf, listenserver_edict);
			}

			if (!FNullEnt(pBot->CurrentTask->TaskTarget))
			{
				float DistFromTarget = vDist3D(pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(pBot->CurrentTask->TaskTarget));

				sprintf(buf, "Dist to Task: %f\n", DistFromTarget);
				UTIL_SayText(buf, listenserver_edict);
			}

			UTIL_DrawLine(clients[0], pBot->CurrentEyePosition, pBot->CurrentTask->TaskLocation, 10.0f, 255, 255, 0);
		}
		else
		{
			UTIL_SayText("No Current Task\n", listenserver_edict);
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "botgivepoints"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)
			{
				FakeClientCommand(bots[i].pEdict, "givepoints", NULL, NULL);
			}

		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "botkill"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)
			{
				//FakeClientCommand(bots[i].pEdict, "kill", NULL, NULL);
				UTIL_BotSuicide(&bots[i]);
			}

		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "printcommanderactions"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				if (bots[i].bot_ns_class == CLASS_MARINE_COMMANDER)
				{

					FILE* ActionLog = fopen("CommanderActionLog.txt", "w+");

					if (!ActionLog)
					{
						RETURN_META(MRES_SUPERCEDE);
					}

					for (int Priority = 0; Priority < 10; Priority++)
					{
						fprintf(ActionLog, "Priority %d Actions:\n", Priority);

						for (int ActionIndex = 0; ActionIndex < 10; ActionIndex++)
						{
							if (bots[i].CurrentCommanderActions[Priority][ActionIndex].bIsActive)
							{
								commander_action* action = &bots[i].CurrentCommanderActions[Priority][ActionIndex];
								fprintf(ActionLog, "ACTION:\n");
								switch (action->ActionType)
								{
								case ACTION_BUILD:
									fprintf(ActionLog, "Action Type: Build\n");
									fprintf(ActionLog, "Structure Type: %s\n", (action->ActionType == ACTION_BUILD) ? UTIL_StructTypeToChar(action->StructureToBuild) : "N/A");
									fprintf(ActionLog, "Structure is placed: %s\n", (action->ActionType == ACTION_BUILD) ? ((action->StructureOrItem) ? "True" : "False") : "N/A");
									fprintf(ActionLog, "Assigned Player: %s\n", (action->AssignedPlayer > -1) ? STRING(clients[action->AssignedPlayer]->v.netname) : "None");
									break;
								case ACTION_RESEARCH:
									fprintf(ActionLog, "Action Type: Research\n");
									fprintf(ActionLog, "Research Tech: %s\n", UTIL_ResearchTypeToChar(action->ResearchId));
									break;
								case ACTION_RECYCLE:
									fprintf(ActionLog, "Action Type: Recycle\n");
									fprintf(ActionLog, "Structure To Recycle: %s\n", UTIL_StructTypeToChar(action->StructureToBuild));
									break;
								case ACTION_UPGRADE:
									fprintf(ActionLog, "Action Type: Upgrade\n");
									fprintf(ActionLog, "Structure To Upgrade: %s\n", UTIL_StructTypeToChar(action->StructureToBuild));
									break;
								case ACTION_DROPITEM:
									fprintf(ActionLog, "Action Type: Drop Item\n");
									fprintf(ActionLog, "Item To Drop: %s\n", UTIL_DroppableItemTypeToChar(action->ItemToDeploy));
									break;
								}

								fprintf(ActionLog, "Action attempts: %d\n", action->NumActionAttempts);
								fprintf(ActionLog, "\n");
							}
						}

						fprintf(ActionLog, "\n");
					}

					fclose(ActionLog);

				}
			}

		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "cometome"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				bots[i].PrimaryBotTask.TaskType = TASK_MOVE;
				bots[i].PrimaryBotTask.TaskLocation = UTIL_GetFloorUnderEntity(pEntity);
				bots[i].PrimaryBotTask.bOrderIsUrgent = true;
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "evolvegorge"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				if (IsPlayerOnAlienTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))
				{
					bots[i].PrimaryBotTask.TaskType = TASK_EVOLVE;
					bots[i].PrimaryBotTask.Evolution = IMPULSE_ALIEN_EVOLVE_GORGE;
					bots[i].PrimaryBotTask.TaskLocation = UTIL_GetFloorUnderEntity(pEntity);
				}
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "evolvefade"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				if (IsPlayerOnAlienTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))
				{
					bots[i].PrimaryBotTask.TaskType = TASK_EVOLVE;
					bots[i].PrimaryBotTask.Evolution = IMPULSE_ALIEN_EVOLVE_FADE;
					bots[i].PrimaryBotTask.TaskLocation = UTIL_GetFloorUnderEntity(pEntity);
				}
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "evolveonos"))
	{
		if (!NavmeshLoaded()) 
		{ 
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				if (IsPlayerOnAlienTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))
				{
					bots[i].PrimaryBotTask.TaskType = TASK_EVOLVE;
					bots[i].PrimaryBotTask.Evolution = IMPULSE_ALIEN_EVOLVE_ONOS;
					bots[i].PrimaryBotTask.TaskLocation = UTIL_GetFloorUnderEntity(pEntity);
				}
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "randommarinepoint"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		Vector FoundPoint = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, UTIL_GetFloorUnderEntity(pEntity), UTIL_MetresToGoldSrcUnits(5.0f));

		if (FoundPoint != ZERO_VECTOR)
		{
			UTIL_DrawLine(pEntity, FoundPoint, FoundPoint + Vector(0.0f, 0.0f, 100.0f), 5.0f, 0, 0, 255);
		}
		else
		{
			UTIL_SayText("Failed to find a point", pEntity);
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	RETURN_META(MRES_IGNORED);
	
}

void RemoveAllBots()
{
	char cmd[80];

	for (int i = 0; i < 32; i++)
	{
		if (bots[i].is_used && !FNullEnt(bots[i].pEdict))
		{
			sprintf(cmd, "kick \"%s\"\n", STRING(bots[i].pEdict->v.netname));

			SERVER_COMMAND(cmd);  // kick the bot using (kick "name")
		}

	}
}

void RemoveBotFromTeam(const int Team)
{
	edict_t* BotToKick = nullptr;
	int BotValue = 0;

	int TeamToRemoveFrom = Team;

	if (TeamToRemoveFrom != ALIEN_TEAM && TeamToRemoveFrom != MARINE_TEAM)
	{
		int NumMarines = UTIL_GetNumPlayersOnTeam(MARINE_TEAM);
		int NumAliens = UTIL_GetNumPlayersOnTeam(ALIEN_TEAM);

		if (NumMarines > NumAliens)
		{
			TeamToRemoveFrom = MARINE_TEAM;
		}
		else
		{
			TeamToRemoveFrom = ALIEN_TEAM;
		}
	}

	for (int i = 0; i < 32; i++)
	{
		if (bots[i].is_used && !FNullEnt(bots[i].pEdict) && bots[i].pEdict->v.team == TeamToRemoveFrom)
		{
			if (TeamToRemoveFrom == ALIEN_TEAM)
			{
				int ThisBotValue = bots[i].resources;

				if (!IsPlayerDead(bots[i].pEdict))
				{
					if (IsPlayerGorge(bots[i].pEdict))
					{
						ThisBotValue += kGorgeEvolutionCost;
					}
					else if (IsPlayerLerk(bots[i].pEdict))
					{
						ThisBotValue += kLerkEvolutionCost;
					}
					else if (IsPlayerFade(bots[i].pEdict))
					{
						ThisBotValue += kFadeEvolutionCost;
					}
					else if (IsPlayerOnos(bots[i].pEdict))
					{
						ThisBotValue += kOnosEvolutionCost;
					}
				}

				if (FNullEnt(BotToKick) || ThisBotValue < BotValue)
				{
					BotToKick = bots[i].pEdict;
					BotValue = ThisBotValue;
				}
			}
			else
			{
				if (FNullEnt(BotToKick) || (IsPlayerCommander(BotToKick)))
				{
					BotToKick = bots[i].pEdict;
				}
			}
		}
	}

	if (!FNullEnt(BotToKick))
	{
		char cmd[80];

		sprintf(cmd, "kick \"%s\"\n", STRING(BotToKick->v.netname));

		SERVER_COMMAND(cmd);  // kick the bot using (kick "name")
	}

	
}

void AddBotToTeam(const int Team)
{
	if (Team != MARINE_TEAM && Team != ALIEN_TEAM)
	{
		int NumMarines = UTIL_GetNumPlayersOnTeam(MARINE_TEAM);
		int NumAliens = UTIL_GetNumPlayersOnTeam(ALIEN_TEAM);

		if (NumMarines > NumAliens)
		{
			BotCreate(NULL, ALIEN_TEAM);
		}
		else
		{
			BotCreate(NULL, MARINE_TEAM);
		}
	}
	else
	{
		BotCreate(NULL, Team);
	}

	
}

int UTIL_GetNumBotsOnTeam(const int Team)
{
	int Result = 0;

	for (int i = 0; i < 32; i++)
	{
		if (bots[i].is_used && bots[i].bot_team == Team)
		{
			Result++;
		}

	}

	return Result;
}

int UTIL_GetNumHumansOnTeam(const int Team)
{
	int Result = 0;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && IsPlayerHuman(clients[i]))
		{
			Result++;
		}

	}

	return Result;
}

void HandleTeamBalance()
{
	if (!NavmeshLoaded())
	{
		if (!loadNavigationData(STRING(gpGlobals->mapname)))
		{
			return;
		}
	}

	int NumMarines = UTIL_GetNumPlayersOnTeam(MARINE_TEAM);
	int NumAliens = UTIL_GetNumPlayersOnTeam(ALIEN_TEAM);

	if (NumMarines != NumAliens)
	{
		if (NumMarines > NumAliens)
		{
			if (UTIL_GetNumBotsOnTeam(MARINE_TEAM) > 0)
			{
				RemoveBotFromTeam(MARINE_TEAM);
			}
			else
			{
				AddBotToTeam(ALIEN_TEAM);
			}
		}
		else
		{
			if (UTIL_GetNumBotsOnTeam(ALIEN_TEAM) > 0)
			{
				RemoveBotFromTeam(ALIEN_TEAM);
			}
			else
			{
				AddBotToTeam(MARINE_TEAM);
			}
		}
	}
}

void HandleManualFillTeams()
{
	int DesiredNumMarines = CONFIG_GetManualMarineTeamSize();
	int DesiredNumAliens = CONFIG_GetManualAlienTeamSize();

	if (DesiredNumMarines == 0 && DesiredNumAliens == 0) { return; }

	if (!NavmeshLoaded())
	{
		if (!loadNavigationData(STRING(gpGlobals->mapname)))
		{
			return;
		}
	}

	int NumMarines = UTIL_GetNumPlayersOnTeam(MARINE_TEAM);
	int NumAliens = UTIL_GetNumPlayersOnTeam(ALIEN_TEAM);


	if ((DesiredNumMarines + DesiredNumAliens) > gpGlobals->maxClients)
	{
		int Delta = (DesiredNumMarines + DesiredNumAliens) - gpGlobals->maxClients;

		bool bRemoveAlien = true;
		int BotsRemoved = 0;

		while (BotsRemoved < Delta)
		{
			if (bRemoveAlien)
			{
				DesiredNumAliens--;
			}
			else
			{
				DesiredNumMarines--;
			}
			BotsRemoved++;
			bRemoveAlien = !bRemoveAlien;
		}
	}


	if (NumMarines == DesiredNumMarines)
	{
		CONFIG_SetManualMarineTeamSize(0);
	}

	if (NumAliens == DesiredNumAliens)
	{
		CONFIG_SetManualAlienTeamSize(0);
	}

	if (CONFIG_GetManualMarineTeamSize() == 0 && CONFIG_GetManualAlienTeamSize() == 0) { return; }

	if (NumMarines > DesiredNumMarines)
	{
		if (UTIL_GetNumBotsOnTeam(MARINE_TEAM) > 0)
		{
			RemoveBotFromTeam(MARINE_TEAM);
			return;
		}
	}

	if (NumAliens > DesiredNumAliens)
	{
		if (UTIL_GetNumBotsOnTeam(ALIEN_TEAM) > 0)
		{
			RemoveBotFromTeam(ALIEN_TEAM);
			return;
		}
	}

	if (NumMarines <= NumAliens)
	{
		if (NumMarines < DesiredNumMarines)
		{
			AddBotToTeam(MARINE_TEAM);
			return;
		}
	}
	else
	{
		if (NumAliens < DesiredNumAliens)
		{
			AddBotToTeam(ALIEN_TEAM);
			return;
		}
	}
}

void HandleFillTeams()
{
	int NumMarines = UTIL_GetNumPlayersOnTeam(MARINE_TEAM);
	int NumAliens = UTIL_GetNumPlayersOnTeam(ALIEN_TEAM);

	int DesiredNumMarines = CONFIG_GetMarineTeamSizeForMap(STRING(gpGlobals->mapname));
	int DesiredNumAliens = CONFIG_GetAlienTeamSizeForMap(STRING(gpGlobals->mapname));

	if (!NavmeshLoaded())
	{
		if (!loadNavigationData(STRING(gpGlobals->mapname)))
		{
			return;
		}
	}

	// If our server's max capacity isn't enough to satisfy fill requirements, then reduce the desired team sizes accordingly,
	// trying to maintain ratios and balance by alternately removing an alien and marine until conditions satisfied
	if ((DesiredNumMarines + DesiredNumAliens) > gpGlobals->maxClients)
	{
		int Delta = (DesiredNumMarines + DesiredNumAliens) - gpGlobals->maxClients;

		bool bRemoveAlien = true;
		int BotsRemoved = 0;

		while (BotsRemoved < Delta)
		{
			if (bRemoveAlien)
			{
				DesiredNumAliens--;
			}
			else
			{
				DesiredNumMarines--;
			}
			BotsRemoved++;
			bRemoveAlien = !bRemoveAlien;
		}
	}


	if (NumMarines > DesiredNumMarines)
	{
		if (UTIL_GetNumBotsOnTeam(MARINE_TEAM) > 0)
		{
			RemoveBotFromTeam(MARINE_TEAM);
			return;
		}
	}

	if (NumAliens > DesiredNumAliens)
	{
		if (UTIL_GetNumBotsOnTeam(ALIEN_TEAM) > 0)
		{
			RemoveBotFromTeam(ALIEN_TEAM);
			return;
		}
	}

	if (NumMarines <= NumAliens)
	{
		if (NumMarines < DesiredNumMarines)
		{
			AddBotToTeam(MARINE_TEAM);
			return;
		}
	}
	else
	{
		if (NumAliens < DesiredNumAliens)
		{
			AddBotToTeam(ALIEN_TEAM);
			return;
		}
	}
}

void UpdateBotCounts()
{
	BotFillMode FillMode = CONFIG_GetBotFillMode();

	switch (FillMode)
	{
		case BOTFILL_FILLTEAMS:
			HandleFillTeams();
			return;
		case BOTFILL_BALANCEONLY:
			HandleTeamBalance();
			return;
		case BOTFILL_MANUAL:
			HandleManualFillTeams();
		default:
			return;
	}
}

void StartFrame(void)
{
	static clock_t prevtime = 0.0f;
	static clock_t currTime = 0.0f;
	static float last_structure_refresh_time = 0.0f;
	static float last_item_refresh_time = 0.0f;
	static float last_bot_count_check_time = 0.0f;
	static double DeltaTime = 0.0f;
	
	static float previous_time = -1.0;

	currTime = clock();
	DeltaTime = currTime - prevtime;
	DeltaTime = DeltaTime / CLOCKS_PER_SEC;

	if (gpGlobals->deathmatch)
	{

		static int i, index, player_index, bot_index;

		if (last_bot_count_check_time >= gpGlobals->time)
		{
			last_bot_count_check_time = 0.0f;
		}

		if (gpGlobals->time >= 5.0f)
		{
			if (gpGlobals->time - last_bot_count_check_time > 0.25f)
			{
				UpdateBotCounts();
				last_bot_count_check_time = gpGlobals->time;
			}
		}

		if (NavmeshLoaded() && bot_stop == 0)
		{
			if (bGameIsActive)
			{
				if (!bGameHasStarted)
				{
					UTIL_OnGameStart();
					bGameHasStarted = true;
					GameStartTime = gpGlobals->time;
					last_structure_refresh_time = 0.0f;
					last_item_refresh_time = 0.0f;
				}

				if (gpGlobals->time - last_structure_refresh_time >= structure_inventory_refresh_rate)
				{
					UTIL_RefreshBuildableStructures();
					last_structure_refresh_time = gpGlobals->time;
				}

				if (gpGlobals->time - last_item_refresh_time >= item_inventory_refresh_rate)
				{
					UTIL_RefreshMarineItems();
					last_item_refresh_time = gpGlobals->time;
				}

				CommanderMode CommMode = CONFIG_GetCommanderMode();

				if (CommMode == COMMANDERMODE_ALWAYS || (CommMode == COMMANDERMODE_IFNOHUMAN && UTIL_GetNumHumansOnTeam(MARINE_TEAM) == 0))
				{
					if (!UTIL_IsBotCommanderAssigned() && !UTIL_IsThereACommander())
					{
						AssignCommander();
					}
				}

				if (UTIL_GetBotsWithRoleType(BOT_ROLE_GUARD_BASE, true) == 0)
				{
					AssignGuardBot();
				}
				
			}

			float timeSinceLastThink = ((currTime - last_think_time) / CLOCKS_PER_SEC);

			if (timeSinceLastThink >= MIN_FRAME_TIME)
			{
				UTIL_UpdateTileCache();

				for (bot_index = 0; bot_index < gpGlobals->maxClients; bot_index++)
				{
					bot_t* bot = &bots[bot_index];

					if ((bot->is_used) &&  // is this slot used AND
						(bot->respawn_state == RESPAWN_IDLE))  // not respawning
					{
						if (bot->name[0] == 0)  // name filled in yet?
							strcpy(bot->name, STRING(bot->pEdict->v.netname));

						if (bot->not_started)
						{
							BotStartGame(bot);
						}

						for (int i = 0; i < 5; i++)
						{
							if (bot->ChatMessages[i].bIsPending && gpGlobals->time >= bot->ChatMessages[i].SendTime)
							{
								if (bot->ChatMessages[i].bIsTeamSay)
								{
									BotTeamSay(bot, bot->ChatMessages[i].msg);
								}
								else
								{
									BotSay(bot, bot->ChatMessages[i].msg);
								}
								bot->ChatMessages[i].bIsPending = false;
								break;
							}
						}

						BotUpdateViewRotation(bot, timeSinceLastThink);

						if (ShouldBotThink(bot))
						{
							if (bot->bBotThinkPaused)
							{
								OnBotRestartPlay(bot);
							}
							
							StartNewBotFrame(bot);

							switch (CurrentDebugMode)
							{
								case EVO_DEBUG_TESTNAV:
									TestNavThink(bot);
									break;
								case EVO_DEBUG_DRONE:
									DroneThink(bot);
									break;
								default:
									BotThink(bot);
									break;
							}

							BotUpdateDesiredViewRotation(bot);

							NSWeapon DesiredCombatWeapon = (bot->DesiredMoveWeapon != WEAPON_NONE) ? bot->DesiredMoveWeapon : bot->DesiredCombatWeapon;

							if (DesiredCombatWeapon != WEAPON_NONE && UTIL_GetBotCurrentWeapon(bot) != DesiredCombatWeapon)
							{
								BotSwitchToWeapon(bot, DesiredCombatWeapon);
							}

							
						}
						else
						{
							bot->bBotThinkPaused = true;
						}
						// Adjust msec to command time interval
						byte adjustedmsec = ThrottledMsec(bot);

						// save the command time
						bot->f_previous_command_time = gpGlobals->time;

						g_engfuncs.pfnRunPlayerMove(bot->pEdict, bot->pEdict->v.v_angle, bot->ForwardMove,
							bot->SideMove, bot->UpMove, bot->pEdict->v.button, bot->impulse, adjustedmsec);
					}
				}
				last_think_time = currTime;
			}

		}

	}
	prevtime = currTime;
	previous_time = gpGlobals->time;

	RETURN_META(MRES_IGNORED);
}

void FakeClientCommand(edict_t *pBot, const char *arg1, const char *arg2, const char *arg3)
{
	int length;

	memset(g_argv, 0, 1024);

	isFakeClientCommand = true;

	if ((arg1 == NULL) || (*arg1 == 0))
		return;

	if ((arg2 == NULL) || (*arg2 == 0))
	{
		length = sprintf(&g_argv[0], "%s", arg1);
		fake_arg_count = 1;
	}
	else if ((arg3 == NULL) || (*arg3 == 0))
	{
		
		length = sprintf(&g_argv[0], "%s %s", arg1, arg2);
		fake_arg_count = 2;
	}
	else
	{
		length = sprintf(&g_argv[0], "%s %s %s", arg1, arg2, arg3);
		fake_arg_count = 3;
	}

	g_argv[length] = 0;  // null terminate just in case

	strcpy(&g_argv[64], arg1);

	if (arg2)
		strcpy(&g_argv[128], arg2);

	if (arg3)
		strcpy(&g_argv[192], arg3);

	// allow the MOD DLL to execute the ClientCommand...
	MDLL_ClientCommand(pBot);

	isFakeClientCommand = false;
}

void PrintHelpFile()
{
	char HelpFileName[256];

	FILE* HelpFile = NULL;
	GetGameDir(HelpFileName);
	strcat(HelpFileName, "/addons/evobot/Help.txt");
	
	std::ifstream cFile(HelpFileName);

	if (cFile.is_open())
	{
		std::string line;
		while (getline(cFile, line))
		{
			LOG_CONSOLE(PLID, line.c_str());
		}
	}
	else
	{
		LOG_CONSOLE(PLID, "Help not available, Help.txt not found in evobot directory\n");
	}
}

void EvoBot_ServerCommand(void)
{
	char msg[128];

	const char* arg1 = CMD_ARGV(1);
	const char* arg2 = CMD_ARGV(2);
	const char* arg3 = CMD_ARGV(3);
	const char* arg4 = CMD_ARGV(4);
	const char* arg5 = CMD_ARGV(5);

	std::string sArg1 = arg1;

	if (sArg1.empty() || FStrEq(arg1, "help"))
	{
		PrintHelpFile();
		return;
	}

	if (FStrEq(arg1, "addbot"))
	{
		BotFillMode CurrentFillMode = CONFIG_GetBotFillMode();

		if (CurrentFillMode != BOTFILL_MANUAL)
		{
			LOG_CONSOLE(PLID, "Please change bot fill mode to manual before adding/removing bots\n");
			return;
		}

		int Team = 0;
		const char* TeamInput = CMD_ARGV(2);

		if (TeamInput != NULL && isNumber(TeamInput))
		{
			Team = atoi(TeamInput);
		}
		AddBotToTeam(Team);

		return;
	}

	if (FStrEq(arg1, "removebot"))
	{
		const char* TeamInput = CMD_ARGV(2);

		if (FStrEq(TeamInput, "all"))
		{
			CONFIG_SetConfigOverride(true);
			CONFIG_SetBotFillMode(BOTFILL_MANUAL);

			RemoveAllBots();

			return;
		}

		BotFillMode CurrentFillMode = CONFIG_GetBotFillMode();

		if (CurrentFillMode != BOTFILL_MANUAL)
		{
			LOG_CONSOLE(PLID, "Please change bot fill mode to manual before adding/removing bots\n");
			return;
		}

		int Team = 0;
		

		if (TeamInput != NULL && isNumber(TeamInput))
		{
			Team = atoi(TeamInput);
		}
		RemoveBotFromTeam(Team);

		return;
	}

	if (FStrEq(arg1, "disable"))
	{
		LOG_CONSOLE(PLID, "Disabling EvoBot. Use 'loadconfig' to restore defaults\n");
		CONFIG_SetConfigOverride(true);
		CONFIG_SetBotFillMode(BOTFILL_MANUAL);
		CONFIG_SetManualMarineTeamSize(0);
		CONFIG_SetManualAlienTeamSize(0);

		RemoveAllBots();

		return;
	}

	if (FStrEq(arg1, "loadconfig"))
	{
		LOG_CONSOLE(PLID, "Restoring default bot behaviour from evobot.cfg...\n");
		ParseConfigFile(true);

		return;
	}

	if (FStrEq(arg1, "newconfig"))
	{
		CONFIG_RegenerateConfigFile();

		return;
	}

	if (FStrEq(arg1, "commandermode"))
	{
		const char* CommMode = CMD_ARGV(2);

		if (!CommMode)
		{
			LOG_CONSOLE(PLID, "Please specify a mode. Valid arguments are 'never', 'ifnohuman' or 'always'\n");
			return;
		}

		if (FStrEq(CommMode, "never"))
		{
			CONFIG_SetConfigOverride(true);
			CONFIG_SetCommanderMode(COMMANDERMODE_NEVER);

			return;
		}

		if (FStrEq(CommMode, "ifnohuman"))
		{
			CONFIG_SetConfigOverride(true);
			CONFIG_SetCommanderMode(COMMANDERMODE_IFNOHUMAN);

			return;
		}

		if (FStrEq(CommMode, "always"))
		{
			CONFIG_SetConfigOverride(true);
			CONFIG_SetCommanderMode(COMMANDERMODE_ALWAYS);

			return;
		}

		LOG_CONSOLE(PLID, "Invalid mode specified. Valid arguments are 'never', 'ifnohuman' or 'always'\n");

		return;
	}

	if (FStrEq(arg1, "debug"))
	{
		const char* DebugMode = CMD_ARGV(2);

		if (!DebugMode)
		{
			LOG_CONSOLE(PLID, "Please specify a debug mode. Valid arguments are 'stop', 'testnav' or 'drone'\n");
			return;
		}

		if (FStrEq(DebugMode, "stop"))
		{
			CurrentDebugMode = EVO_DEBUG_NONE;

			for (int i = 0; i < 32; i++)
			{
				UTIL_ClearBotTask(&bots[i], &bots[i].PrimaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].SecondaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].WantsAndNeedsTask);
			}

			return;
		}

		if (FStrEq(DebugMode, "testnav"))
		{
			CurrentDebugMode = EVO_DEBUG_TESTNAV;

			for (int i = 0; i < 32; i++)
			{
				UTIL_ClearBotTask(&bots[i], &bots[i].PrimaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].SecondaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].WantsAndNeedsTask);
			}

			return;
		}

		if (FStrEq(DebugMode, "drone"))
		{
			CurrentDebugMode = EVO_DEBUG_DRONE;

			for (int i = 0; i < 32; i++)
			{
				UTIL_ClearBotTask(&bots[i], &bots[i].PrimaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].SecondaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].WantsAndNeedsTask);
			}

			return;
		}

		LOG_CONSOLE(PLID, "Invalid debug mode specified. Valid arguments are 'stop', 'testnav' or 'drone'\n");

		return;

	}

	if (FStrEq(arg1, "botfillmode"))
	{
		const char* FillMode = CMD_ARGV(2);

		if (!FillMode)
		{
			LOG_CONSOLE(PLID, "Please specify a mode. Valid arguments are 'manual', 'balanceonly' or 'fillteams'\n");
			return;
		}

		if (FStrEq(FillMode, "manual"))
		{
			CONFIG_SetConfigOverride(true);
			CONFIG_SetBotFillMode(BOTFILL_MANUAL);

			return;
		}

		if (FStrEq(FillMode, "balanceonly"))
		{
			CONFIG_SetConfigOverride(true);
			CONFIG_SetBotFillMode(BOTFILL_BALANCEONLY);

			return;
		}

		if (FStrEq(FillMode, "fillteams"))
		{
			CONFIG_SetConfigOverride(true);
			CONFIG_SetBotFillMode(BOTFILL_FILLTEAMS);

			return;
		}

		LOG_CONSOLE(PLID, "Invalid mode specified. Valid arguments are 'never', 'ifnohuman' or 'always'\n");

		return;
	}

	if (FStrEq(arg1, "balanceteams"))
	{
		BotFillMode CurrentFillMode = CONFIG_GetBotFillMode();

		if (CurrentFillMode != BOTFILL_MANUAL)
		{
			LOG_CONSOLE(PLID, "Please change bot fill mode to manual before adding/removing bots\n");
			return;
		}

		int NumMarines = UTIL_GetNumPlayersOnTeam(MARINE_TEAM);
		int NumAliens = UTIL_GetNumPlayersOnTeam(ALIEN_TEAM);

		while (NumMarines != NumAliens)
		{
			HandleTeamBalance();

			NumMarines = UTIL_GetNumPlayersOnTeam(MARINE_TEAM);
			NumAliens = UTIL_GetNumPlayersOnTeam(ALIEN_TEAM);
		}

		

		return;
	}

	if (FStrEq(arg1, "fillteams"))
	{
		BotFillMode CurrentFillMode = CONFIG_GetBotFillMode();

		if (CurrentFillMode != BOTFILL_MANUAL)
		{
			LOG_CONSOLE(PLID, "Please change bot fill mode to manual before adding/removing bots\n");
			return;
		}

		int DesiredNumMarines = CONFIG_GetMarineTeamSizeForMap(STRING(gpGlobals->mapname));
		int DesiredNumAliens = CONFIG_GetAlienTeamSizeForMap(STRING(gpGlobals->mapname));

		CONFIG_SetManualMarineTeamSize(DesiredNumMarines);
		CONFIG_SetManualAlienTeamSize(DesiredNumAliens);
		

		return;
	}

	if (FStrEq(arg1, "debug"))
	{
		const char* DebugCommand = CMD_ARGV(2);

		if (FStrEq(DebugCommand, "stop"))
		{

			return;
		}

		if (FStrEq(DebugCommand, "navtest"))
		{

			return;
		}

		return;
	}
	
}


C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion)
{
	gFunctionTable.pfnGameInit = GameDLLInit;
	gFunctionTable.pfnSpawn = Spawn;
	gFunctionTable.pfnKeyValue = KeyValue;
	gFunctionTable.pfnClientConnect = ClientConnect;
	gFunctionTable.pfnClientDisconnect = ClientDisconnect;
	gFunctionTable.pfnClientPutInServer = ClientPutInServer;
	gFunctionTable.pfnClientCommand = ClientCommand;
	gFunctionTable.pfnStartFrame = StartFrame;

	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return true;
}

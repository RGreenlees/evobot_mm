
#include "game_state.h"

#include <extdll.h>
#include <dllapi.h>
#include <meta_api.h>

#include "player_util.h"
#include "bot_util.h"
#include "bot_config.h"
#include "bot_navigation.h"
#include "bot_tactical.h"
#include "bot_task.h"
#include "general_util.h"

edict_t* clients[MAX_CLIENTS];
bot_t bots[MAX_CLIENTS];

int NumClients;
int NumBots;
int NumMarineBots;
int NumAlienBots;
int NumHumans;

int IsDedicatedServer = 0;

edict_t* listenserver_edict = nullptr;

int GameStatus = 0;

bool bGameHasStarted = false;
bool bGameIsActive = false;
float GameStartTime = 0.0f;

float last_bot_count_check_time = 0.0f;

int ServerMSecVal = 0;

extern float last_structure_refresh_time;
extern float last_item_refresh_time;

extern int StructureRefreshFrame;
extern int ItemRefreshFrame;

EvobotDebugMode CurrentDebugMode = EVO_DEBUG_NONE;

NSGameMode CurrentGameMode = GAME_MODE_NONE;

EvobotDebugMode GAME_GetDebugMode()
{
	return CurrentDebugMode;
}

NSGameMode GAME_GetGameMode()
{
	return CurrentGameMode;
}

void GAME_UpdateServerMSecVal(const double DeltaTime)
{
	ServerMSecVal = (int)(1000.0 * DeltaTime);
}

int GAME_GetServerMSecVal()
{
	return ServerMSecVal;
}

void GAME_AddClient(edict_t* NewClient)
{
	NumClients = 0;
	bool bClientAdded = false;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (FNullEnt(clients[i]))
		{
			if (!bClientAdded)
			{
				clients[i] = NewClient;
				bClientAdded = true;
			}
			
		}
		
		if (!FNullEnt(clients[i]))
		{
			NumClients++;

			if ((NewClient->v.flags & FL_FAKECLIENT) || (NewClient->v.flags & FL_THIRDPARTYBOT))
			{
				NumBots++;
			}
			else
			{
				NumHumans++;
			}
		}
	}
}

void GAME_RemoveClient(edict_t* DisconnectedClient)
{
	NumClients = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] == DisconnectedClient)
		{
			clients[i] = nullptr;
		}

		if (!FNullEnt(clients[i]))
		{
			NumClients++;

			if ((clients[i]->v.flags & FL_FAKECLIENT) || (clients[i]->v.flags & FL_THIRDPARTYBOT))
			{
				NumBots++;

				if (clients[i]->v.team == MARINE_TEAM)
				{
					NumMarineBots++;
				}
				else if (clients[i]->v.team == ALIEN_TEAM)
				{
					NumAlienBots++;
				}

			}
			else
			{
				NumHumans++;
			}
		}
	}
}

void GAME_Reset()
{
	memset(&bots, 0, sizeof(bots));

	NumBots = 0;

	IsDedicatedServer = IS_DEDICATED_SERVER();

	bGameHasStarted = false;
	bGameIsActive = false;
	GameStartTime = 0.0f;
	last_bot_count_check_time = 0.0f;
}

void GAME_RemoveAllBotsInReadyRoom()
{
	char cmd[80];

	for (int i = 0; i < 32; i++)
	{
		if (bots[i].is_used && !FNullEnt(bots[i].pEdict) && IsPlayerInReadyRoom(bots[i].pEdict))
		{
			sprintf(cmd, "kick \"%s\"\n", STRING(bots[i].pEdict->v.netname));

			SERVER_COMMAND(cmd);  // kick the bot using (kick "name")

			memset(&bots[i], 0, sizeof(bot_t));
		}
	}
}

void GAME_RemoveAllBots()
{
	char cmd[80];

	for (int i = 0; i < 32; i++)
	{
		if (bots[i].is_used && !FNullEnt(bots[i].pEdict))
		{
			sprintf(cmd, "kick \"%s\"\n", STRING(bots[i].pEdict->v.netname));

			SERVER_COMMAND(cmd);  // kick the bot using (kick "name")

			UTIL_ClearAllBotData(&bots[i]);
		}
	}

	memset(&bots, 0, sizeof(bots));
}

void GAME_SetListenServerEdict(edict_t* ListenEdict)
{
	listenserver_edict = ListenEdict;
}

edict_t* GAME_GetListenServerEdict()
{
	return listenserver_edict;
}

void GAME_ClearClientList()
{
	memset(&clients, 0, sizeof(clients));
}

int GAME_GetNumDeadPlayersOnTeam(const int Team)
{
	int Result = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!FNullEnt(clients[i]) && clients[i]->v.team == Team && IsPlayerDead(clients[i])) { Result++; }
	}

	return Result;
}

int GAME_GetNumPlayersOnTeam(const int Team)
{
	int Result = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!FNullEnt(clients[i]) && clients[i]->v.team == Team) { Result++; }
	}

	return Result;
}

int GAME_GetNumHumansOnTeam(const int Team)
{
	int Result = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!FNullEnt(clients[i]) && IsPlayerHuman(clients[i]) && clients[i]->v.team == Team) { Result++; }
	}

	return Result;
}

bool GAME_IsAnyHumanOnTeam(const int Team)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!clients[i]) { continue; }

		if (IsPlayerHuman(clients[i]) && clients[i]->v.team == Team)
		{
			return true;
		}
	}

	return false;
}

int GAME_GetBotsWithRoleType(BotRole RoleType, const int Team, const edict_t* IgnorePlayer)
{
	int Result = 0;

	for (int i = 0; i < 32; i++)
	{
		if (!bots[i].is_used || FNullEnt(bots[i].pEdict) || bots[i].bot_team != Team || bots[i].pEdict == IgnorePlayer) { continue; }

		if (bots[i].CurrentRole == RoleType)
		{
			Result++;
		}

	}

	return Result;
}

int GAME_GetNumPlayersOnTeamOfClass(const int Team, const NSPlayerClass SearchClass)
{
	int Result = 0;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && clients[i]->v.team == Team)
		{
			const NSPlayerClass ThisPlayerClass = GetPlayerClass(clients[i]);
			if (ThisPlayerClass == SearchClass)
			{
				Result++;
			}
		}
	}

	return Result;
}

void GAME_BotSpawnInit(bot_t* pBot)
{

	memset(&(pBot->current_weapon), 0, sizeof(pBot->current_weapon));
	memset(&(pBot->m_rgAmmo), 0, sizeof(pBot->m_rgAmmo));


	// Force bot to choose a new destination
	ClearBotPath(pBot);

	BotUpdateViewFrustum(pBot);

	pBot->f_previous_command_time = gpGlobals->time;
}

void GAME_BotCreate(edict_t* pPlayer, int Team)
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

	edict_t* BotEnt = nullptr;
	bot_t* pBot = nullptr;

	char c_name[BOT_NAME_LEN + 1];

	CONFIG_GetBotName(c_name);

	BotEnt = (*g_engfuncs.pfnCreateFakeClient)(c_name);

	if (!BotEnt)
	{
		LOG_CONSOLE(PLID, "Max players reached (Server Max: %d)\n", gpGlobals->maxClients);

		return;
	}

	char ptr[128];  // allocate space for message from ClientConnect
	char* infobuffer;
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

	strcpy(pBot->name, c_name);

	pBot->is_used = true;

	//pBot->respawn_state = RESPAWN_IDLE;
	pBot->name[0] = 0;  // name not set by server yet
	pBot->BotNavInfo.PathSize = 0;

	pBot->pEdict = BotEnt;

	pBot->not_started = 1;  // hasn't joined game yet

	UTIL_ClearAllBotData(pBot);
	GAME_BotSpawnInit(pBot);

	pBot->bot_team = Team;

	const bot_skill BotSkillSettings = CONFIG_GetGlobalBotSkillLevel();

	memcpy(&pBot->BotSkillSettings, &BotSkillSettings, sizeof(bot_skill));

	//char logName[64];

	//sprintf(logName, "Bot_%d_log.txt", index);

	//pBot->logFile = fopen(logName, "w+");

	//if (pBot->logFile)
	//{
	//	fprintf(pBot->logFile, "Log for %s\n\n", c_name);
	//}
}

void GAME_UpdateBotCounts()
{
	// Don't do any population checks while the game is in the ended state, as nobody can join a team so it can't assess the player counts properly
	if (GameStatus == kGameStatusEnded)
	{
		GAME_RemoveAllBotsInReadyRoom();
		return; 
	}

	BotFillMode FillMode = CONFIG_GetBotFillMode();

	switch (FillMode)
	{
	case BOTFILL_FILLTEAMS:
		GAME_HandleFillTeams();
		return;
	case BOTFILL_BALANCEONLY:
		GAME_HandleTeamBalance();
		return;
	case BOTFILL_MANUAL:
		GAME_HandleManualFillTeams();
	default:
		return;
	}
}

void GAME_HandleFillTeams()
{
	int NumMarines = GAME_GetNumPlayersOnTeam(MARINE_TEAM);
	int NumAliens = GAME_GetNumPlayersOnTeam(ALIEN_TEAM);

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
		if (GAME_GetNumBotsOnTeam(MARINE_TEAM) > 0)
		{
			GAME_RemoveBotFromTeam(MARINE_TEAM);
			return;
		}
	}

	if (NumAliens > DesiredNumAliens)
	{
		if (GAME_GetNumBotsOnTeam(ALIEN_TEAM) > 0)
		{
			GAME_RemoveBotFromTeam(ALIEN_TEAM);
			return;
		}
	}

	if (NumMarines <= NumAliens)
	{
		if (NumMarines < DesiredNumMarines)
		{
			GAME_AddBotToTeam(MARINE_TEAM);
			return;
		}
	}
	else
	{
		if (NumAliens < DesiredNumAliens)
		{
			GAME_AddBotToTeam(ALIEN_TEAM);
			return;
		}
	}
}

void GAME_HandleTeamBalance()
{
	if (!NavmeshLoaded())
	{
		if (!loadNavigationData(STRING(gpGlobals->mapname)))
		{
			return;
		}
	}

	int NumMarines = GAME_GetNumPlayersOnTeam(MARINE_TEAM);
	int NumAliens = GAME_GetNumPlayersOnTeam(ALIEN_TEAM);

	if (NumMarines != NumAliens)
	{
		if (NumMarines > NumAliens)
		{
			if (GAME_GetNumBotsOnTeam(MARINE_TEAM) > 0)
			{
				GAME_RemoveBotFromTeam(MARINE_TEAM);
			}
			else
			{
				GAME_AddBotToTeam(ALIEN_TEAM);
			}
		}
		else
		{
			if (GAME_GetNumBotsOnTeam(ALIEN_TEAM) > 0)
			{
				GAME_RemoveBotFromTeam(ALIEN_TEAM);
			}
			else
			{
				GAME_AddBotToTeam(MARINE_TEAM);
			}
		}
	}
}

void GAME_HandleManualFillTeams()
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

	int NumMarines = GAME_GetNumPlayersOnTeam(MARINE_TEAM);
	int NumAliens = GAME_GetNumPlayersOnTeam(ALIEN_TEAM);


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
		if (GAME_GetNumBotsOnTeam(MARINE_TEAM) > 0)
		{
			GAME_RemoveBotFromTeam(MARINE_TEAM);
			return;
		}
	}

	if (NumAliens > DesiredNumAliens)
	{
		if (GAME_GetNumBotsOnTeam(ALIEN_TEAM) > 0)
		{
			GAME_RemoveBotFromTeam(ALIEN_TEAM);
			return;
		}
	}

	if (NumMarines <= NumAliens)
	{
		if (NumMarines < DesiredNumMarines)
		{
			GAME_AddBotToTeam(MARINE_TEAM);
			return;
		}
	}
	else
	{
		if (NumAliens < DesiredNumAliens)
		{
			GAME_AddBotToTeam(ALIEN_TEAM);
			return;
		}
	}
}

int GAME_GetNumBotsInGame()
{
	int Result = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (bots[i].is_used && !FNullEnt(bots[i].pEdict))
		{
			Result++;
		}

	}

	return Result;
}

int GAME_GetNumBotsOnTeam(const int Team)
{
	int Result = 0;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (bots[i].is_used && bots[i].bot_team == Team)
		{
			Result++;
		}

	}

	return Result;
}

void GAME_RemoveBotFromTeam(const int Team)
{
	edict_t* BotToKick = nullptr;
	bot_t* BotPointerToKick = nullptr;
	int BotValue = 0;

	int TeamToRemoveFrom = Team;

	if (TeamToRemoveFrom != ALIEN_TEAM && TeamToRemoveFrom != MARINE_TEAM)
	{
		int NumMarines = GAME_GetNumPlayersOnTeam(MARINE_TEAM);
		int NumAliens = GAME_GetNumPlayersOnTeam(ALIEN_TEAM);

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
		if (bots[i].is_used && bots[i].bot_team == TeamToRemoveFrom)
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
					BotPointerToKick = &bots[i];
					BotValue = ThisBotValue;
				}
			}
			else
			{
				if (FNullEnt(BotToKick) || (IsPlayerCommander(BotToKick)))
				{
					BotToKick = bots[i].pEdict;
					BotPointerToKick = &bots[i];
				}
			}
		}
	}

	if (!FNullEnt(BotToKick))
	{
		char cmd[80];

		sprintf(cmd, "kick \"%s\"\n", STRING(BotToKick->v.netname));

		SERVER_COMMAND(cmd);  // kick the bot using (kick "name")

		if (BotPointerToKick != nullptr)
		{
			memset(BotPointerToKick, 0, sizeof(bot_t));
		}
	}
}

void GAME_AddBotToTeam(const int Team)
{
	if (Team != MARINE_TEAM && Team != ALIEN_TEAM)
	{
		int NumMarines = GAME_GetNumPlayersOnTeam(MARINE_TEAM);
		int NumAliens = GAME_GetNumPlayersOnTeam(ALIEN_TEAM);

		if (NumMarines > NumAliens)
		{
			GAME_BotCreate(NULL, ALIEN_TEAM);
		}
		else
		{
			GAME_BotCreate(NULL, MARINE_TEAM);
		}
	}
	else
	{
		GAME_BotCreate(NULL, Team);
	}
}

const char* UTIL_GameModeToChar(const NSGameMode GameMode)
{
	switch (GameMode)
	{
		case GAME_MODE_REGULAR:
			return "Regular";
		case GAME_MODE_COMBAT:
			return "Combat";
		default:
			return "None";
	}
}

void GAME_OnGameStart()
{
	GameStartTime = gpGlobals->time;

	CurrentGameMode = GAME_MODE_NONE;

	const char* theCStrLevelName = STRING(gpGlobals->mapname);
	if (theCStrLevelName && (strlen(theCStrLevelName) > 3))
	{
		if (!strncmp(theCStrLevelName, "ns_", 3))
		{
			bool bHiveExists = UTIL_FindEntityByClassname(NULL, "team_hive") != nullptr;
			bool bCommChairExists = UTIL_FindEntityByClassname(NULL, "team_command") != nullptr;

			if (bHiveExists && bCommChairExists)
			{
				CurrentGameMode = GAME_MODE_REGULAR;
			}
		}
		else if (!strncmp(theCStrLevelName, "co_", 3))
		{
			CurrentGameMode = GAME_MODE_COMBAT;
		}
	}

	UTIL_ClearMapAIData();

	if (!NavmeshLoaded()) { return; }

	if (CurrentGameMode == GAME_MODE_REGULAR)
	{
		UTIL_PopulateResourceNodeLocations();
	}

	PopulateEmptyHiveList();
}


void EvoBot_ServerCommand(void)
{
	char msg[128];

	const char* arg1 = CMD_ARGV(1);
	const char* arg2 = CMD_ARGV(2);
	const char* arg3 = CMD_ARGV(3);
	const char* arg4 = CMD_ARGV(4);
	const char* arg5 = CMD_ARGV(5);

	if (!arg1 || FStrEq(arg1, "help"))
	{
		CONFIG_PrintHelpFile();
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
		GAME_AddBotToTeam(Team);

		return;
	}

	if (FStrEq(arg1, "removebot"))
	{
		const char* TeamInput = CMD_ARGV(2);

		if (FStrEq(TeamInput, "all"))
		{
			CONFIG_SetConfigOverride(true);
			CONFIG_SetBotFillMode(BOTFILL_MANUAL);

			GAME_RemoveAllBots();

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
		GAME_RemoveBotFromTeam(Team);

		return;
	}

	if (FStrEq(arg1, "disable"))
	{
		LOG_CONSOLE(PLID, "Disabling EvoBot. Use 'loadconfig' to restore defaults\n");
		CONFIG_SetConfigOverride(true);
		CONFIG_SetBotFillMode(BOTFILL_MANUAL);
		CONFIG_SetManualMarineTeamSize(0);
		CONFIG_SetManualAlienTeamSize(0);

		GAME_RemoveAllBots();

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

	if (FStrEq(arg1, "botskill"))
	{
		const char* BotSkill = CMD_ARGV(2);

		if (!BotSkill)
		{
			LOG_CONSOLE(PLID, "Please specify a bot skill. See evobot.cfg for valid skill level names\n");
			return;
		}

		if (!CONFIG_BotSkillLevelExists(BotSkill))
		{
			LOG_CONSOLE(PLID, "Bot skill level '%s' does not exist. See evobot.cfg for valid skill level names\n", BotSkill);
			return;
		}

		CONFIG_SetGlobalBotSkillLevel(BotSkill);

		const bot_skill FoundSkill = CONFIG_GetBotSkillLevel(BotSkill);

		for (int i = 0; i < 32; i++)
		{
			if (bots[i].is_used)
			{
				memcpy(&bots[i].BotSkillSettings, &FoundSkill, sizeof(bot_skill));
			}
		}
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
				UTIL_ClearBotTask(&bots[i], &bots[i].CommanderTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].MoveTask);
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
				UTIL_ClearBotTask(&bots[i], &bots[i].CommanderTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].MoveTask);
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
				UTIL_ClearBotTask(&bots[i], &bots[i].CommanderTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].MoveTask);
			}

			return;
		}

		if (FStrEq(DebugMode, "aim"))
		{
			CurrentDebugMode = EVO_DEBUG_AIM;

			for (int i = 0; i < 32; i++)
			{
				UTIL_ClearBotTask(&bots[i], &bots[i].PrimaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].SecondaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].WantsAndNeedsTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].CommanderTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].MoveTask);
			}

			return;
		}

		if (FStrEq(DebugMode, "guard"))
		{
			CurrentDebugMode = EVO_DEBUG_GUARD;

			for (int i = 0; i < 32; i++)
			{
				UTIL_ClearBotTask(&bots[i], &bots[i].PrimaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].SecondaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].WantsAndNeedsTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].CommanderTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].MoveTask);
			}

			return;
		}

		if (FStrEq(DebugMode, "custom"))
		{
			CurrentDebugMode = EVO_DEBUG_CUSTOM;

			for (int i = 0; i < 32; i++)
			{
				UTIL_ClearBotTask(&bots[i], &bots[i].PrimaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].SecondaryBotTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].WantsAndNeedsTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].CommanderTask);
				UTIL_ClearBotTask(&bots[i], &bots[i].MoveTask);
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

		int NumMarines = GAME_GetNumPlayersOnTeam(MARINE_TEAM);
		int NumAliens = GAME_GetNumPlayersOnTeam(ALIEN_TEAM);

		while (NumMarines != NumAliens)
		{
			GAME_HandleTeamBalance();

			NumMarines = GAME_GetNumPlayersOnTeam(MARINE_TEAM);
			NumAliens = GAME_GetNumPlayersOnTeam(ALIEN_TEAM);
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
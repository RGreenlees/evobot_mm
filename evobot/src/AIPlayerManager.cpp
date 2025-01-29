#include "AIPlayerManager.h"
#include "AIPlayer.h"
#include "AIMath.h"
#include "AITactical.h"
#include "AINavigation.h"
#include "AIConfig.h"
#include "AIWeaponHelper.h"
#include "AIHelper.h"
#include "AIPlayerUtil.h"
#include "NSConstants.h"

#include <time.h>

#include <string>

double last_think_time = 0.0;

std::vector<AIPlayer> ActiveAIPlayers;

float LastAIPlayerCountUpdate = 0.0f;

int BotNameIndex = 0;

float AIStartedTime = 0.0f; // Used to give 5-second grace period before adding bots

bool bMapDataInitialised = false;

float NextCommanderAllowedTimeTeamA = 0.0f;
float NextCommanderAllowedTimeTeamB = 0.0f;

extern int m_spriteTexture;

Vector DebugVector1 = ZERO_VECTOR;
Vector DebugVector2 = ZERO_VECTOR;

int DisplayConnectionsMesh = -1;
int DisplayTempObstaclesMesh = -1;

AIPlayer* DebugAIPlayer = nullptr;

std::vector<bot_path_node> DebugPath;

bool bPlayerSpawned = false;

edict_t* listenserver_edict = nullptr;

DynamicMapObject* DebugObject = nullptr;


std::string BotNames[MAX_PLAYERS] = {	"MrRobot",
									"Wall-E",
									"BeepBoop",
									"Robotnik",
									"JonnyAutomaton",
									"Burninator",
									"SteelDeath",
									"Meatbag",
									"Undertaker",
									"Botini",
									"Robottle",
									"Rusty",
									"HeavyMetal",
									"Combot",
									"BagelLover",
									"Screwdriver",
									"LoveBug",
									"iSmash",
									"Chippy",
									"Baymax",
									"BoomerBot",
									"Jarvis",
									"Marvin",
									"Data",
									"Scrappy",
									"Mortis",
									"TerrorHertz",
									"Omicron",
									"Herbie",
									"Robogeddon",
									"Velociripper",
									"TerminalFerocity"
};

void AIMGR_UpdateAIPlayerCounts()
{
	for (auto BotIt = ActiveAIPlayers.begin(); BotIt != ActiveAIPlayers.end();)
	{
		// If bot has been kicked from the server then remove from active AI player list
		if (FNullEnt(BotIt->Edict) || BotIt->Edict->free)
		{
			BotIt = ActiveAIPlayers.erase(BotIt);
		}
		else
		{
			BotIt++;
		}
	}

	// Don't add or remove bots too quickly, otherwise it can cause lag or even overflows
	if (gpGlobals->time - LastAIPlayerCountUpdate < 0.2f) { return; }

	if (!AIMGR_ShouldStartPlayerBalancing()) { return; }

	// Add logic for adding bots here

	// Assume manual mode: do nothing, host can manually add/remove as they wish via sv_addaiplayer
	return;
}

int AIMGR_GetNumPlayersOnTeam(int Team)
{

	int Result = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict->v.team == Team)
		{
			Result++;
		}
	}

	return Result;

}

void AIMGR_RemoveAIPlayerFromTeam(int Team)
{
	if (AIMGR_GetNumAIPlayers() == 0) { return; }

	// We will go through the potential bots we could kick. We want to avoid kicking bots which have a lot of
	// resources tied up in them or are commanding, which could cause big disruption to the team they're leaving

	int MinValue = 0; // Track the least valuable bot on the desired team.
	std::vector<AIPlayer>::iterator ItemToRemove = ActiveAIPlayers.end(); // Current bot to be kicked

	for (auto it = ActiveAIPlayers.begin(); it != ActiveAIPlayers.end(); it++)
	{
		ItemToRemove = it;
		break;
	}
	
	if (ItemToRemove != ActiveAIPlayers.end())
	{		
		AIMGR_KickBot(ItemToRemove->Edict);

		ActiveAIPlayers.erase(ItemToRemove);
	}

}

void AIMGR_AddAIPlayerToTeam(int Team)
{
	int NewBotIndex = -1;
	edict_t* BotEnt = nullptr;
	char SuccMsg[256];

	// If bots aren't enabled or the game has ended, don't allow new bots to be added
	if (!AIMGR_IsBotEnabled())
	{
		g_engfuncs.pfnServerPrint("Evobot is disabled currently\n");
		return;
	}

	if (ActiveAIPlayers.size() >= gpGlobals->maxClients)
	{
		g_engfuncs.pfnServerPrint("Bot limit reached, cannot add more\n");
		return;
	}

	if (AIMGR_GetNumAIPlayers() == 0)
	{
		// Initialise the name index to a random number so we don't always get the same bot names
		BotNameIndex = RANDOM_LONG(0, 31);
	}

	// Retrieve the current bot name and then cycle the index so the names are always unique
	// Slap a [BOT] tag too so players know they're not human
	std::string NewName = CONFIG_GetBotPrefix() + CONFIG_GetNextBotName();

	BotEnt = (*g_engfuncs.pfnCreateFakeClient)(NewName.c_str());

	if (FNullEnt(BotEnt))
	{
		sprintf(SuccMsg, "Failed to create AI player: server is full\n");
		g_engfuncs.pfnServerPrint(SuccMsg);
		return;
	}

	BotNameIndex++;

	if (BotNameIndex > 31)
	{
		BotNameIndex = 0;
	}

	char ptr[128];  // allocate space for message from ClientConnect
	int clientIndex;

	char* infobuffer = (*g_engfuncs.pfnGetInfoKeyBuffer)(BotEnt);
	clientIndex = ENTINDEX(BotEnt);

	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "model", "");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "rate", "3500.000000");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "cl_updaterate", "20");

	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "cl_lw", "0");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "cl_lc", "0");

	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "tracker", "0");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "cl_dlmax", "128");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "lefthand", "1");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "friends", "0");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "dm", "0");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "ah", "1");
	(*g_engfuncs.pfnSetClientKeyValue)(clientIndex, infobuffer, "_vgui_menus", "0");

	MDLL_ClientConnect(BotEnt, STRING(BotEnt->v.netname), "127.0.0.1", ptr);
	MDLL_ClientPutInServer(BotEnt);

	BotEnt->v.flags |= FL_FAKECLIENT; // Shouldn't be needed but just to be sure

	BotEnt->v.idealpitch = BotEnt->v.v_angle.x;
	BotEnt->v.ideal_yaw = BotEnt->v.v_angle.y;

	BotEnt->v.pitch_speed = 270;  // slightly faster than HLDM of 225
	BotEnt->v.yaw_speed = 250; // slightly faster than HLDM of 210

	AIPlayer NewAIPlayer;
	NewAIPlayer.Edict = BotEnt;
	NewAIPlayer.DesiredTeam = Team;

	if (ActiveAIPlayers.size() == 0)
	{
		AIMGR_ResetRound();
	}

	ActiveAIPlayers.push_back(NewAIPlayer);

	sprintf(SuccMsg, "Adding AI Player to team: %d\n", (int)Team);
	g_engfuncs.pfnServerPrint(SuccMsg);
}

bool AIMGR_HasBotStartedGame(AIPlayer* pBot)
{
	return !IsPlayerInReadyRoom(pBot->Edict);
}

byte BotThrottledMsec(AIPlayer* inAIPlayer, float CurrentTime)
{
	// Thanks to The Storm (ePODBot) for this one, finally fixed the bot running speed!
	int newmsec = (int)roundf((CurrentTime - inAIPlayer->LastServerUpdateTime) * 1000.0f);
	
	if (newmsec > 255)
	{
		newmsec = 255;
	}

	return (byte)newmsec;
}


bool AIMGR_HasMatchEnded()
{
	return false;
}

void DEBUG_SetDebugObject(edict_t* Object)
{
	DebugObject = UTIL_GetDynamicObjectByEdict(Object);
}

void AIMGR_UpdateAISystem()
{
	if (!bMapDataInitialised)
	{
		AIStartedTime = gpGlobals->time;

		LastAIPlayerCountUpdate = 0.0f;

		bMapDataInitialised = true;
	}

	AIMGR_UpdateAIPlayerCounts();
	

	if (AIMGR_IsBotEnabled())
	{
		if (!AIMGR_HasMatchEnded())
		{
			if (AIMGR_GetNavMeshStatus() == NAVMESH_STATUS_PENDING)
			{
				AIMGR_LoadNavigationData();
			}

			AIMGR_UpdateAIMapData();

			if (DebugObject)
			{
				DEBUG_PrintObjectInfo(DebugObject);
			}

			if (DisplayConnectionsMesh > -1)
			{
				AIDEBUG_DrawOffMeshConnections(DisplayConnectionsMesh);
			}

			if (DisplayTempObstaclesMesh > -1)
			{
				AIDEBUG_DrawTemporaryObstacles(DisplayTempObstaclesMesh);
			}

			if (DebugPath.size() > 0)
			{
				AIDEBUG_DrawPath(DebugPath);
			}
		}

		AIMGR_UpdateAIPlayers();
	}
}

bool AIMGR_GetRoundHasStarted()
{
	return true;
}

void AIMGR_UpdateAIPlayers()
{
	// If bots are not enabled then do nothing
	if (!AIMGR_IsBotEnabled()) { return; }

	static float PrevTime = 0.0f;
	static float CurrTime = 0.0f;

	static int CurrentBotSkill = 1;

	static int UpdateIndex = 0;

	CurrTime = gpGlobals->time;

	if (CurrTime < PrevTime)
	{
		PrevTime = 0.0f;
	}

	float FrameDelta = CurrTime - PrevTime;
	
	int NumBots = AIMGR_GetNumAIPlayers();

	int NumBotsThinkThisFrame = 0;

	int BotsPerFrame = imaxi(1, (int)round(BOT_THINK_RATE_HZ * NumBots * FrameDelta));

	int BotIndex = 0;
		
	for (auto BotIt = ActiveAIPlayers.begin(); BotIt != ActiveAIPlayers.end();)
	{
		// If bot has been kicked from the server then remove from active AI player list
		if (FNullEnt(BotIt->Edict) || BotIt->Edict->free)
		{
			BotIt = ActiveAIPlayers.erase(BotIt);
			continue;
		}

		AIPlayer* bot = &(*BotIt);

		BotUpdateViewRotation(bot, FrameDelta);

		if (AIMGR_GetRoundHasStarted())
		{
			if (UpdateIndex > -1 && BotIndex >= UpdateIndex && NumBotsThinkThisFrame < BotsPerFrame)
			{
				RunAIPlayerFrame(bot);
				NumBotsThinkThisFrame++;
			}
			BotIndex++;
		}

		UpdateBotChat(bot);

		// Needed to correctly handle client prediction and physics calculations
		byte adjustedmsec = BotThrottledMsec(bot, CurrTime);			

		// Simulate PM_PlayerMove so client prediction and stuff can be executed correctly.
		g_engfuncs.pfnRunPlayerMove(bot->Edict, bot->Edict->v.v_angle, bot->ForwardMove,
			bot->SideMove, bot->UpMove, bot->Button, bot->Impulse, adjustedmsec);

		bot->LastServerUpdateTime = CurrTime;

		BotIt++;
	}

	if (UpdateIndex < 0) 
	{ 
		UpdateIndex = 0; 
	}
	else
	{
		UpdateIndex += NumBotsThinkThisFrame;
	}	

	if (UpdateIndex >= NumBots)
	{
		UpdateIndex = 0;
	}

	PrevTime = CurrTime;

}

int AIMGR_GetNumAIPlayers()
{
	return ActiveAIPlayers.size();
}

std::vector<edict_t*> AIMGR_GetAllPlayersOnTeam(int Team)
{
	std::vector<edict_t*> Result;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict->v.team == Team)
		{
			Result.push_back(PlayerEdict);
		}
	}

	return Result;
}

int AIMGR_GetNumAIPlayersOnTeam(int Team)
{
	int Result = 0;

	for (auto it = ActiveAIPlayers.begin(); it != ActiveAIPlayers.end(); it++)
	{
		if (it->Edict->v.team == Team)
		{
			Result++;
		}
	}

	return Result;
}

int AIMGR_GetNumHumanPlayersOnTeam(int Team)
{
	int Result = 0;

	std::vector<edict_t*> TeamPlayers = AIMGR_GetAllPlayersOnTeam(Team);

	for (auto it = TeamPlayers.begin(); it != TeamPlayers.end(); it++)
	{
		edict_t* PlayerEdict = (*it);

		if (!(PlayerEdict->v.flags & FL_FAKECLIENT))
		{
			Result++;
		}
	}

	return Result;
}

int AIMGR_AIPlayerExistsOnTeam(int Team)
{
	for (auto it = ActiveAIPlayers.begin(); it != ActiveAIPlayers.end(); it++)
	{
		if (it->Edict->v.team == Team)
		{
			return true;
		}
	}

	return false;
}

void AIMGR_ResetRound()
{
	if (!AIMGR_IsBotEnabled()) { return; } // Do nothing if we're not using bots

	// AI Players would be 0 if the round is being reset because a new game is starting. If the round is reset
	// from a console command, or tournament mode readying up etc, then bot logic is unaffected
	if (AIMGR_GetNumAIPlayers() == 0)
	{
		// This is used to track the 5-second "grace period" before adding bots to the game if fill teams is enabled
		AIStartedTime = gpGlobals->time;
	}

	LastAIPlayerCountUpdate = 0.0f;

	AITAC_ClearMapAIData(false);

	bool bTileCacheFullyUpdated = UTIL_UpdateTileCache();

	while (!bTileCacheFullyUpdated)
	{
		bTileCacheFullyUpdated = UTIL_UpdateTileCache();
	}

	bMapDataInitialised = true;
}

void AIMGR_ReloadNavigationData()
{
	if (NavmeshLoaded())
	{
		ReloadNavMeshes();
	}
}

void AIMGR_RoundStarted()
{
	if (!AIMGR_IsBotEnabled()) { return; } // Do nothing if we're not using bots

}

void AIMGR_ClearBotData()
{
	// We have to be careful here, depending on how the nav data is being unloaded, there could be stale references in the ActiveAIPlayers list.
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && !PlayerEdict->free && (PlayerEdict->v.flags & FL_FAKECLIENT))
		{
			for (auto it = ActiveAIPlayers.begin(); it != ActiveAIPlayers.end();)
			{
				if (it->Edict == PlayerEdict && it->Edict)
				{
					AIMGR_KickBot(it->Edict);
					it = ActiveAIPlayers.erase(it);
				}
				else
				{
					it++;
				}
			}
		}
	}

	// We shouldn't have any bots in the server when this is called, but this ensures no bots end up "orphans" and no longer tracked by the system
	ActiveAIPlayers.clear();
}

void AIMGR_NewMap()
{
	AIMGR_BotPrecache();

	if (NavmeshLoaded())
	{
		UnloadNavigationData();
	}

	UTIL_ClearLocalizations();
	NAV_ClearCachedMapData();

	AITAC_ClearMapAIData(true);

	bMapDataInitialised = false;

	ActiveAIPlayers.clear();

	AIStartedTime = gpGlobals->time;
	LastAIPlayerCountUpdate = 0.0f;

	bPlayerSpawned = false;

	CONFIG_ParseConfigFile();
	CONFIG_PopulateBotNames();
}

bool AIMGR_IsNavmeshLoaded()
{
	return NavmeshLoaded();
}

bool AIMGR_IsBotEnabled()
{
	return NAV_GetNavMeshStatus() != NAVMESH_STATUS_FAILED;
}

AINavMeshStatus AIMGR_GetNavMeshStatus()
{
	return NAV_GetNavMeshStatus();
}

void AIMGR_LoadNavigationData()
{
	// Don't reload the nav mesh if it's already loaded
	if (NavmeshLoaded()) { return; }

	CONFIG_ParseConfigFile();

	const char* theCStrLevelName = STRING(gpGlobals->mapname);

	if (!loadNavigationData(theCStrLevelName))
	{
		char ErrMsg[128];
		sprintf(ErrMsg, "Failed to load navigation data for %s\n", theCStrLevelName);
		g_engfuncs.pfnServerPrint(ErrMsg);
	}
	else
	{
		AITAC_InitialiseMapAIData();
	}
}

AIPlayer* AIMGR_GetBotRefFromPlayer(edict_t* PlayerRef)
{
	for (auto BotIt = ActiveAIPlayers.begin(); BotIt != ActiveAIPlayers.end(); BotIt++)
	{
		if (BotIt->Edict == PlayerRef) { return &(*BotIt); }
	}

	return nullptr;
}

AIPlayer* AIMGR_GetBotAtIndex(int Index)
{
	if (Index < 0 || Index >= ActiveAIPlayers.size()) { return nullptr; }

	return &ActiveAIPlayers[Index];
}

std::vector<AIPlayer*> AIMGR_GetAllAIPlayers()
{
	std::vector<AIPlayer*> Result;

	Result.clear();

	for (auto BotIt = ActiveAIPlayers.begin(); BotIt != ActiveAIPlayers.end(); BotIt++)
	{
		if (FNullEnt(BotIt->Edict)) { continue; }

		Result.push_back(&(*BotIt));
	}

	return Result;
}

std::vector<edict_t*> AIMGR_GetAllActivePlayers()
{
	std::vector<edict_t*> Result;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && !PlayerEdict->free && IsPlayerActiveInGame(PlayerEdict))
		{
			Result.push_back(PlayerEdict);
		}
	}

	return Result;
}

std::vector<AIPlayer*> AIMGR_GetAIPlayersOnTeam(int Team)
{
	std::vector<AIPlayer*> Result;

	Result.clear();

	for (auto BotIt = ActiveAIPlayers.begin(); BotIt != ActiveAIPlayers.end(); BotIt++)
	{
		if (FNullEnt(BotIt->Edict)) { continue; }

		if (BotIt->Edict->v.team == Team)
		{
			Result.push_back(&(*BotIt));
		}
	}

	return Result;
}

std::vector<edict_t*> AIMGR_GetNonAIPlayersOnTeam(int Team)
{
	std::vector<edict_t*> TeamPlayers = AIMGR_GetAllPlayersOnTeam(Team);

	for (auto it = ActiveAIPlayers.begin(); it != ActiveAIPlayers.end(); it++)
	{
		edict_t* ThisPlayer = it->Edict;

		if (FNullEnt(ThisPlayer)) { continue; }

		std::vector<edict_t*>::iterator FoundPlayer = std::find(TeamPlayers.begin(), TeamPlayers.end(), ThisPlayer);

		if (FoundPlayer != TeamPlayers.end())
		{
			TeamPlayers.erase(FoundPlayer);
		}
	}

	return TeamPlayers;
}

bool AIMGR_ShouldStartPlayerBalancing()
{
	return (bPlayerSpawned && gpGlobals->time - AIStartedTime > AI_GRACE_PERIOD) || (gpGlobals->time - AIStartedTime > AI_MAX_START_TIMEOUT);
}

void AIMGR_UpdateAIMapData()
{
	if (!NavmeshLoaded()) { return; }

	if (bMapDataInitialised)
	{
		AITAC_UpdateMapAIData();
		UTIL_UpdateTileCache();
		AITAC_CheckNavMeshModified();
	}
}

void AIMGR_RegenBotIni()
{
	CONFIG_RegenerateIniFile();
}

void AIMGR_BotPrecache()
{
	m_spriteTexture = PRECACHE_MODEL("sprites/zbeam6.spr");
}

AIPlayer* AIMGR_GetDebugAIPlayer()
{
	return DebugAIPlayer;
}

void AIMGR_SetDebugAIPlayer(edict_t* AIPlayer)
{
	if (FNullEnt(AIPlayer))
	{
		DebugAIPlayer = nullptr;
		return;
	}

	for (auto it = ActiveAIPlayers.begin(); it != ActiveAIPlayers.end(); it++)
	{
		if (it->Edict == AIPlayer)
		{
			DebugAIPlayer = &(*it);
			return;
		}
	}

	DebugAIPlayer = nullptr;
}

void AIMGR_ClientConnected(edict_t* NewClient)
{

}

void AIMGR_PlayerSpawned()
{
	bPlayerSpawned = true;
}

void AIMGR_KickBot(edict_t* BotToKick)
{
	char cmd[80];

	sprintf(cmd, "kick \"%s\"\n", STRING(BotToKick->v.netname));

	SERVER_COMMAND(cmd);  // kick the bot using (kick "name")
}

AIPlayer* AIMGR_GetBotPointer(const edict_t* pEdict)
{
	for (auto it = ActiveAIPlayers.begin(); it != ActiveAIPlayers.end(); it++)
	{
		if (it->Edict == pEdict)
		{
			return &(*it);
		}
	}

	return nullptr;
}

int AIMGR_GetBotIndex(const edict_t* pEdict)
{
	int Index = 0;

	for (auto it = ActiveAIPlayers.begin(); it != ActiveAIPlayers.end(); it++)
	{
		if (it->Edict == pEdict)
		{
			return Index;
		}
		Index++;
	}

	return -1;
}

void evobot_ServerCommand(void)
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
		int Team = 0;
		const char* TeamInput = CMD_ARGV(2);

		if (TeamInput != NULL && isNumber(TeamInput))
		{
			Team = atoi(TeamInput);
		}
		AIMGR_AddAIPlayerToTeam(Team);

		return;
	}

	if (FStrEq(arg1, "debug"))
	{
		edict_t* ListenEdict = AIMGR_GetListenServerEdict();

		if (FNullEnt(ListenEdict)) { return; }
		
		if (FStrEq(arg2, "cometome"))
		{
			std::vector<AIPlayer*> AllBots = AIMGR_GetAllAIPlayers();

			for (auto it = AllBots.begin(); it != AllBots.end(); it++)
			{
				if (NAV_IsPointInSwimArea(ListenEdict->v.origin))
				{
					(*it)->TestLocation = ListenEdict->v.origin;
				}
				else
				{
					(*it)->TestLocation = UTIL_GetFloorUnderEntity(ListenEdict);
				}
			}

			return;
		}

		if (FStrEq(arg2, "allstop"))
		{
			std::vector<AIPlayer*> AllBots = AIMGR_GetAllAIPlayers();

			for (auto it = AllBots.begin(); it != AllBots.end(); it++)
			{
				ClearBotMovement((*it));
			}

			return;
		}

		if (FStrEq(arg2, "gotopoint1"))
		{
			if (vIsZero(DebugVector1)) { return; }

			std::vector<AIPlayer*> AllBots = AIMGR_GetAllAIPlayers();

			for (auto it = AllBots.begin(); it != AllBots.end(); it++)
			{
				(*it)->TestLocation = DebugVector1;
			}

			return;
		}

		if (FStrEq(arg2, "gotopoint2"))
		{
			if (vIsZero(DebugVector2)) { return; }

			std::vector<AIPlayer*> AllBots = AIMGR_GetAllAIPlayers();

			for (auto it = AllBots.begin(); it != AllBots.end(); it++)
			{
				(*it)->TestLocation = DebugVector2;
			}

			return;
		}

		if (FStrEq(arg2, "setdebugvector1"))
		{
			edict_t* ListenServerEdict = AIMGR_GetListenServerEdict();

			if (ListenServerEdict->v.flags & FL_INWATER)
			{
				DebugVector1 = ListenServerEdict->v.origin;
			}
			else
			{
				DebugVector1 = GetPlayerBottomOfCollisionHull(ListenServerEdict);
			}			

			return;
		}

		if (FStrEq(arg2, "setdebugvector2"))
		{
			edict_t* ListenServerEdict = AIMGR_GetListenServerEdict();

			if (ListenServerEdict->v.flags & FL_INWATER)
			{
				DebugVector2 = ListenServerEdict->v.origin;
			}
			else
			{
				DebugVector2 = GetPlayerBottomOfCollisionHull(ListenServerEdict);
			}

			return;
		}

		if (FStrEq(arg2, "calcdebugpath"))
		{
			if (!vIsZero(DebugVector1) && !vIsZero(DebugVector2))
			{
				FindPathClosestToPoint(GetBaseAgentProfile(NAV_PROFILE_DEFAULT), DebugVector1, DebugVector2, DebugPath, 60.0f);
			}

			return;
		}

		if (FStrEq(arg2, "cleardebugpath"))
		{
			DebugPath.clear();

			return;
		}

		if (FStrEq(arg2, "drawoffmeshconnections"))
		{
			if (arg3 == NULL)
			{
				DisplayConnectionsMesh = -1;
				return;
			}

			int MeshIndex = 0;
			const char* MeshInput = CMD_ARGV(3);

			if (MeshInput != NULL && isNumber(MeshInput))
			{
				MeshIndex = atoi(MeshInput);
			}
			else
			{
				DisplayConnectionsMesh = -1;
				return;
			}

			if (MeshIndex > NUM_NAV_MESHES)
			{
				DisplayConnectionsMesh = -1;
			}
			else
			{
				DisplayConnectionsMesh = MeshIndex - 1;
			}
			

			return;
		}

		if (FStrEq(arg2, "drawtempobstacles"))
		{
			if (arg3 == NULL)
			{
				DisplayTempObstaclesMesh = -1;
				return;
			}

			int MeshIndex = 0;
			const char* MeshInput = CMD_ARGV(3);

			if (MeshInput != NULL && isNumber(MeshInput))
			{
				MeshIndex = atoi(MeshInput);
			}
			else
			{
				DisplayTempObstaclesMesh = -1;
				return;
			}

			if (MeshIndex > NUM_NAV_MESHES)
			{
				DisplayTempObstaclesMesh = -1;
			}
			else
			{
				DisplayTempObstaclesMesh = MeshIndex - 1;
			}

			return;
		}

		if (FStrEq(arg2, "dynamicobjectinfo"))
		{
			Vector StartTrace = GetPlayerEyePosition(ListenEdict);
			Vector EndTrace = StartTrace + UTIL_GetForwardVector(ListenEdict->v.v_angle) * 1000.0f;

			edict_t* HitEdict = UTIL_TraceEntity(AIMGR_GetListenServerEdict(), StartTrace, EndTrace);

			DynamicMapObject* BlockingObject = nullptr;

			if (!FNullEnt(HitEdict))
			{
				BlockingObject = UTIL_GetDynamicObjectByEdict(HitEdict);
			}
			else
			{
				BlockingObject = UTIL_GetObjectBlockingPathPoint(StartTrace, EndTrace, NAV_FLAG_WALK, nullptr, nullptr);
			}

			if (BlockingObject)
			{
				DEBUG_SetDebugObject(BlockingObject->Edict);
			}
			else
			{
				DEBUG_SetDebugObject(nullptr);
			}

			return;
		}

		return;
	}
}

void AIMGR_SetListenServerEdict(edict_t* NewEdict)
{
	listenserver_edict = NewEdict;
}

edict_t* AIMGR_GetListenServerEdict()
{
	return listenserver_edict;
}

Vector DEBUG_GetDebugVector1()
{
	return DebugVector1;
}
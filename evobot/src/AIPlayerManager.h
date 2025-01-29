#ifndef AI_PLAYER_MANAGER_H
#define AI_PLAYER_MANAGER_H

#include "AIConstants.h"
#include "AIPlayer.h"

// The rate at which the bot will call RunPlayerMove in, default is 100hz. WARNING: Increasing the rate past 100hz causes bots to move and turn slowly due to GoldSrc limits!
static const double BOT_SERVER_UPDATE_RATE = (1.0 / 100.0);
// The rate in hz (times per second) at which the bot will call AIPlayerThink, default is 10 times per second.
static const int BOT_THINK_RATE_HZ = 10;
// Once the first human player has joined the game, how long to wait before adding bots
static const float AI_GRACE_PERIOD = 5.0f;
// Max time to wait before spawning players if none connect (e.g. empty dedicated server)
static const float AI_MAX_START_TIMEOUT = 20.0f;


void AIMGR_BotPrecache();

// Called when the round restarts. Clears all tactical information but keeps navigation data.
void	AIMGR_ResetRound();
// Called when a new map is loaded. Clears all tactical information AND loads new navmesh.
void	AIMGR_NewMap();
// Called when a round of play begins
void AIMGR_RoundStarted();

// Called every frame. The entry point for all bot logic
void AIMGR_UpdateAISystem();

// Adds a new AI player to a team (0 = Auto-assign, 1 = Team A, 2 = Team B)
void	AIMGR_AddAIPlayerToTeam(int Team);
// Removed an AI player from the team (0 = Auto-select team, 1 = Team A, 2 = Team B)
void	AIMGR_RemoveAIPlayerFromTeam(int Team);
// Run AI player logic
void	AIMGR_UpdateAIPlayers();

bool AIMGR_HasMatchEnded();

// Has the bot actually started playing, or are they in a lobby, or do they need to pick a team or class before they can play?
bool AIMGR_HasBotStartedGame(AIPlayer* pBot);

// Called every 0.2s to determine if bots need to be added/removed. Calls UpdateTeamBalance or UpdateFillTeams depending on auto-mode
void	AIMGR_UpdateAIPlayerCounts();

std::vector<edict_t*> AIMGR_GetAllPlayersOnTeam(int Team);

// Convenient helper function to get total number of players (human and AI) on a team
int AIMGR_GetNumPlayersOnTeam(int Team);
// How many AI players are in the game (does NOT include third-party bots like RCBot/Whichbot)
int		AIMGR_GetNumAIPlayers();
// Returns true if an AI player is on the requested team (does NOT include third-party bots like RCBot/Whichbot)
int		AIMGR_AIPlayerExistsOnTeam(int Team);

void AIMGR_RegenBotIni();

void	AIMGR_UpdateAIMapData();
bool AIMGR_ShouldStartPlayerBalancing();


int AIMGR_GetNumAIPlayersOnTeam(int Team);
int AIMGR_GetNumHumanPlayersOnTeam(int Team);

bool AIMGR_IsNavmeshLoaded();
AINavMeshStatus AIMGR_GetNavMeshStatus();

bool AIMGR_IsBotEnabled();

void AIMGR_LoadNavigationData();
void AIMGR_ReloadNavigationData();

AIPlayer* AIMGR_GetBotRefFromPlayer(edict_t* PlayerRef);
AIPlayer* AIMGR_GetBotAtIndex(int Index);


// Returns all NS AI players. Does not include third-party bots
std::vector<AIPlayer*> AIMGR_GetAllAIPlayers();
// Returns all NS AI players on the requested team. Does not include third-party bots
std::vector<AIPlayer*> AIMGR_GetAIPlayersOnTeam(int Team);
// Returns all active players (i.e. not dead, commanding, spectating or in the ready room)
std::vector<edict_t*> AIMGR_GetAllActivePlayers();

// Returns all players on a team which are not an internal NS bot. Will still include third party bots such as Whichbot and RCBot
std::vector<edict_t*> AIMGR_GetNonAIPlayersOnTeam(int Team);

void AIMGR_ClearBotData();

AIPlayer* AIMGR_GetDebugAIPlayer();
void AIMGR_SetDebugAIPlayer(edict_t* AIPlayer);

void AIMGR_ClientConnected(edict_t* NewClient);
void AIMGR_PlayerSpawned();

void AIMGR_KickBot(edict_t* BotToKick);

AIPlayer* AIMGR_GetBotPointer(const edict_t* pEdict);
int AIMGR_GetBotIndex(const edict_t* pEdict);

void evobot_ServerCommand(void);

void AIMGR_SetListenServerEdict(edict_t* NewEdict);
edict_t* AIMGR_GetListenServerEdict();

Vector DEBUG_GetDebugVector1();

void DEBUG_SetDebugObject(edict_t* Object);

#endif
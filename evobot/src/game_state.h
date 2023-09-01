//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// game_state.h
// 
// Contains all functionality related to monitoring and maintaining player counts
//

#pragma once

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "bot_structs.h"

#include <dllapi.h>

static const int MAX_CLIENTS = 32;

typedef enum
{
	GAME_STATUS_NOTSTARTED,
	GAME_STATUS_ACTIVE,
	GAME_STATUS_ENDED
}
NSGameStatus;

typedef struct _TRACKED_EVOLUTION
{
	edict_t* PlayerEdict = nullptr;
	int LastKnownRes = 0;
	NSPlayerClass EvolvingClass = CLASS_NONE;
	NSPlayerClass LastSeenClass = CLASS_NONE;
	bool bIsEvolving = false;
} TrackedEvolution;

NSGameMode GAME_GetGameMode();

void GAME_SetGameStatus(NSGameStatus NewStatus);
NSGameStatus GAME_GetGameStatus();

void GAME_AddClient(edict_t* NewClient);
void GAME_RemoveClient(edict_t* DisconnectedClient);

int GAME_GetClientIndex(edict_t* Client);

void GAME_Reset();

void GAME_SetListenServerEdict(edict_t* ListenEdict);
edict_t* GAME_GetListenServerEdict();

void GAME_ClearClientList();

int GAME_GetNumPlayersOnTeam(const int Team);
int GAME_GetNumHumansOnTeam(const int Team);

int GAME_GetNumDeadPlayersOnTeam(const int Team);
int GAME_GetNumActivePlayersOnTeam(const int Team);

void GAME_SetBotDeltaTime(float NewDelta);
float GAME_GetBotDeltaTime();

bool GAME_IsAnyHumanOnTeam(const int Team);

int GAME_GetBotsWithRoleType(BotRole RoleType, const int Team, const edict_t* IgnorePlayer);

int GAME_GetNumPlayersOnTeamOfClass(const int Team, const NSPlayerClass SearchClass);

void GAME_BotSpawnInit(bot_t* pBot);

void GAME_UpdateBotCounts();
void GAME_HandleFillTeams();
void GAME_HandleManualFillTeams();
void GAME_HandleTeamBalance();

int GAME_GetNumBotsInGame();
int GAME_GetNumBotsOnTeam(const int Team);
void GAME_AddBotToTeam(const int Team);
void GAME_RemoveBotFromTeam(const int Team);

bool GAME_IsDedicatedServer();

bool GAME_UseComplexFOV();
void GAME_SetUseComplexFOV(bool bNewValue);

EvobotDebugMode GAME_GetDebugMode();

void GAME_OnGameStart();

void GAME_RemoveAllBots();
void GAME_RemoveAllBotsInReadyRoom();

const char* UTIL_GameModeToChar(const NSGameMode GameMode);

void EvoBot_ServerCommand(void);

void GAME_UpdateServerMSecVal(const double DeltaTime);
int GAME_GetServerMSecVal();

void DEBUG_SetShowBotPath(bool bNewValue);
void DEBUG_SetShowTaskInfo(bool bNewValue);
bool DEBUG_ShouldShowTaskInfo();
bool DEBUG_ShouldShowBotPath();

void GAME_TrackPlayerEvolutions();
bool GAME_IsAnyPlayerEvolvingToClass(NSPlayerClass Class);
int GAME_GetNumPlayersEvolvingToClass(NSPlayerClass Class);
bool GAME_IsAnyPlayerEvolvingToClass(NSPlayerClass Class, edict_t* IgnorePlayer);
int GAME_GetNumPlayersEvolvingToClass(NSPlayerClass Class, edict_t* IgnorePlayer);
bool GAME_IsPlayerEvolvingToClass(NSPlayerClass Class, edict_t* Player);

float GAME_GetLastLerkSeenTime();

#endif
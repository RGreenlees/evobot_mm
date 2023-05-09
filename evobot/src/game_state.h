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

NSGameMode GAME_GetGameMode();

void GAME_AddClient(edict_t* NewClient);
void GAME_RemoveClient(edict_t* DisconnectedClient);

void GAME_Reset();

void GAME_SetListenServerEdict(edict_t* ListenEdict);
edict_t* GAME_GetListenServerEdict();

void GAME_ClearClientList();

int GAME_GetNumPlayersOnTeam(const int Team);
int GAME_GetNumHumansOnTeam(const int Team);

bool GAME_IsAnyHumanOnTeam(const int Team);

int GAME_GetBotsWithRoleType(BotRole RoleType, const int Team, const edict_t* IgnorePlayer);

int GAME_GetNumPlayersOnTeamOfClass(const int Team, const NSPlayerClass SearchClass);

void GAME_BotSpawnInit(bot_t* pBot);

void GAME_UpdateBotCounts();
void GAME_HandleFillTeams();
void GAME_HandleManualFillTeams();
void GAME_HandleTeamBalance();

int GAME_GetNumBotsOnTeam(const int Team);
void GAME_AddBotToTeam(const int Team);
void GAME_RemoveBotFromTeam(const int Team);

EvobotDebugMode GAME_GetDebugMode();

void GAME_OnGameStart();

void GAME_RemoveAllBots();
void GAME_RemoveAllBotsInReadyRoom();

const char* UTIL_GameModeToChar(const NSGameMode GameMode);

void EvoBot_ServerCommand(void);

#endif
#pragma once

#ifndef BOT_TASK_H
#define BOT_TASK_H

#include "bot_structs.h"
#include "bot_tactical.h"

void UTIL_ClearAllBotTasks(bot_t* pBot);
void UTIL_ClearBotTask(bot_t* pBot, bot_task* Task);
void UTIL_ClearGuardInfo(bot_t* pBot);

void BotUpdateAndClearTasks(bot_t* pBot);

bot_task* BotGetNextTask(bot_t* pBot);

bool UTIL_IsTaskCompleted(bot_t* pBot, bot_task* Task);
bool UTIL_IsTaskStillValid(bot_t* pBot, bot_task* Task);

bool UTIL_IsTaskUrgent(bot_t* pBot, bot_task* Task);
bool UTIL_IsMoveTaskUrgent(bot_t* pBot, bot_task* Task);
bool UTIL_IsBuildTaskUrgent(bot_t* pBot, bot_task* Task);
bool UTIL_IsGuardTaskUrgent(bot_t* pBot, bot_task* Task);

void BotOnCompleteCommanderTask(bot_t* pBot, bot_task* Task);

bool UTIL_IsMoveTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsAmmoPickupTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsHealthPickupTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsEquipmentPickupTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsWeaponPickupTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsWeldTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsAttackTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsResupplyTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsGuardTaskStillValid(bot_t* pBot, bot_task* Task);

bool UTIL_IsAlienBuildTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsMarineBuildTaskStillValid(bot_t* pBot, bot_task* Task);

bool UTIL_IsAlienCapResNodeTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsMarineCapResNodeTaskStillValid(bot_t* pBot, bot_task* Task);

bool UTIL_IsDefendTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsEvolveTaskStillValid(bot_t* pBot, bot_task* Task);

bool UTIL_IsAlienGetHealthTaskStillValid(bot_t* pBot, bot_task* Task);
bool UTIL_IsAlienHealTaskStillValid(bot_t* pBot, bot_task* Task);

char* UTIL_TaskTypeToChar(const BotTaskType TaskType);

void TASK_SetAttackTask(bot_t* pBot, bot_task* Task, edict_t* Target, const bool bIsUrgent);
void TASK_SetMoveTask(bot_t* pBot, bot_task* Task, const Vector Location, const bool bIsUrgent);
void TASK_SetBuildTask(bot_t* pBot, bot_task* Task, const NSStructureType StructureType, const Vector Location, const bool bIsUrgent);
void TASK_SetBuildTask(bot_t* pBot, bot_task* Task, edict_t* StructureToBuild, const bool bIsUrgent);
void TASK_SetCapResNodeTask(bot_t* pBot, bot_task* Task, const resource_node* NodeRef, const bool bIsUrgent);
void TASK_SetDefendTask(bot_t* pBot, bot_task* Task, edict_t* Target, const bool bIsUrgent);
void TASK_SetEvolveTask(bot_t* pBot, bot_task* Task, const Vector EvolveLocation, const int EvolveImpulse, const bool bIsUrgent);
void TASK_SetUseTask(bot_t* pBot, bot_task* Task, edict_t* Target, const bool bIsUrgent);
void TASK_SetUseTask(bot_t* pBot, bot_task* Task, edict_t* Target, const Vector UseLocation, const bool bIsUrgent);
void TASK_SetTouchTask(bot_t* pBot, bot_task* Task, edict_t* Target, bool bIsUrgent);

void BotProgressTask(bot_t* pBot, bot_task* Task);

void BotProgressMoveTask(bot_t* pBot, bot_task* Task);
void BotProgressUseTask(bot_t* pBot, bot_task* Task);
void BotProgressTouchTask(bot_t* pBot, bot_task* Task);
void BotProgressPickupTask(bot_t* pBot, bot_task* Task);
void BotProgressGuardTask(bot_t* pBot, bot_task* Task);

void BotProgressResupplyTask(bot_t* pBot, bot_task* Task);
void BotProgressAttackTask(bot_t* pBot, bot_task* Task);
void BotProgressDefendTask(bot_t* pBot, bot_task* Task);
void BotProgressTakeCommandTask(bot_t* pBot);
void BotProgressEvolveTask(bot_t* pBot, bot_task* Task);

void MarineProgressBuildTask(bot_t* pBot, bot_task* Task);
void MarineProgressCapResNodeTask(bot_t* pBot, bot_task* Task);
void MarineProgressWeldTask(bot_t* pBot, bot_task* Task);

void AlienProgressGetHealthTask(bot_t* pBot, bot_task* Task);
void AlienProgressHealTask(bot_t* pBot, bot_task* Task);
void AlienProgressBuildTask(bot_t* pBot, bot_task* Task);
void AlienProgressCapResNodeTask(bot_t* pBot, bot_task* Task);

void BotGuardLocation(bot_t* pBot, const Vector GuardLocation);

void UTIL_GenerateGuardWatchPoints(bot_t* pBot, const Vector& GuardLocation);

bool BotWithBuildTaskExists(NSStructureType StructureType);
edict_t* GetFirstBotWithBuildTask(NSStructureType StructureType, edict_t* IgnorePlayer);

#endif
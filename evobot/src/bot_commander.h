//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_commander.h
// 
// Contains commander-related code. Some stuff should be separated out into helper file
//

#pragma once

#ifndef BOT_COMMANDER_H
#define BOT_COMMANDER_H

#include "bot.h"

bool CommanderProgressAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressBuildAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressRecycleAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressUpgradeAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressResearchAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressItemDropAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressOrderAction(bot_t* CommanderBot, int ActionIndex, int Priority);

bool UTIL_IssueOrderForAction(bot_t* CommanderBot, int PlayerIndex, int ActionIndex, int Priority);
void UTIL_IssueMarineMoveToOrder(bot_t* CommanderBot, edict_t* Recipient, const Vector& Destination);
void UTIL_IssueMarineBuildOrder(bot_t* CommanderBot, edict_t* Recipient, edict_t* StructureToBuild);



void CommanderReceiveAlert(bot_t* pBot, const Vector& Location, const PlayerAlertType AlertType);
void CommanderReceiveHealthRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveAmmoRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveOrderRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveBaseAttackAlert(bot_t* pBot, const Vector& Location);

void UTIL_OrganiseCommanderActions(bot_t* pBot);

void UpdateCommanderOrders(bot_t* Commander);
void UpdateCommanderActions(bot_t* Commander);

void CommanderQueueInfantryPortalBuild(bot_t* pBot, int Priority);
void CommanderQueuePhaseGateBuild(bot_t* pBot, const Vector& Location, int Priority);
void CommanderQueueArmouryBuild(bot_t* pBot, int Priority);
void CommanderQueueResTowerBuild(bot_t* pBot, int Priority);
void CommanderQueueArmsLabBuild(bot_t* pBot, int Priority);
void CommanderQueueResearch(bot_t* pBot, NSResearch Research, int Priority);
void CommanderQueueArmsLabResearch(bot_t* pBot, NSResearch Research, int Priority);
void CommanderQueuePrototypeLabResearch(bot_t* pBot, NSResearch Research, int Priority);
void CommanderQueueObservatoryResearch(bot_t* pBot, NSResearch Research, int Priority);
void CommanderQueueArmouryResearch(bot_t* pBot, NSResearch Research, int Priority);
void CommanderQueueRecycleAction(bot_t* pBot, edict_t* Structure, int Priority);
void CommanderQueueUpgrade(bot_t* pBot, edict_t* BuildingToUpgrade, int Priority);
void CommanderQueueItemDrop(bot_t* pBot, NSDeployableItem ItemToDeploy, const Vector& DropLocation, edict_t* Recipient, int Priority);
void CommanderQueueActionOrder(bot_t* pBot, int Priority, int AssignedPlayer);

commander_action* UTIL_FindCommanderBuildActionOfType(bot_t* pBot, const NSStructureType StructureType, const Vector SearchLocation, const float SearchRadius);

void CommanderThink(bot_t* CommanderBot);

void CommanderGetPrimaryTask(bot_t* pBot, bot_task* Task);

bool UTIL_IsMarineOrderValid(bot_t* CommanderBot, int CommanderOrderIndex);
bool UTIL_IsCommanderActionValid(bot_t* CommanderBot, int CommanderActionIndex, int Priority);
bool UTIL_IsCommanderActionActionable(bot_t* CommanderBot, int CommanderActionIndex, int Priority);
bool UTIL_IsCommanderActionComplete(bot_t* CommanderBot, int ActionIndex, int Priority);
bool UTIL_CommanderBuildActionIsValid(bot_t* CommanderBot, commander_action* Action);

int UTIL_FindFreeResNodeWithMarineNearby(bot_t* CommanderBot);

bool UTIL_ActionHasValidPlayerAssigned(bot_t* CommanderBot, int CommanderActionIndex, int Priority);

bool UTIL_StructureIsScheduledForRecycle(bot_t* CommanderBot, edict_t* Structure);

edict_t* UTIL_GetFirstMarineStructureOffNavmesh();

int UTIL_GetNumPlacedOrQueuedStructuresOfType(bot_t* CommanderBot, NSStructureType StructureType);


void UTIL_CommanderQueueStructureBuildAtLocation(bot_t* pBot, const Vector& Location, NSStructureType StructureType, int Priority);

bool UTIL_ResearchInProgress(NSResearch Research);

void QueueHeavyArmourLoadout(bot_t* CommanderBot, const Vector& Area, int Priority);

void QueueSecureHiveAction(bot_t* CommanderBot, const Vector& Area, int Priority);

void QueueSiegeHiveAction(bot_t* CommanderBot, const Vector& Area, int Priority);

bool UTIL_ResearchIsAlreadyQueued(bot_t* pBot, NSResearch Research);
bool UTIL_HasIdleArmsLab();
edict_t* UTIL_GetFirstIdleArmsLab();

void LinkCommanderActionsToDroppedItems(bot_t* Commander);

bool UTIL_ItemIsAlreadyLinked(bot_t* Commander, edict_t* Item);

void UTIL_LinkItem(bot_t* Commander, edict_t* Item);

bool UTIL_CancelCommanderPlayerOrder(bot_t* Commander, int PlayerIndex);

bool UTIL_ActionExistsInLocation(const bot_t* Commander, const Vector& CheckPoint);

void UTIL_ClearCommanderAction(bot_t* Commander, int ActionIndex, int Priority);
void UTIL_ClearCommanderOrder(bot_t* Commander, int OrderIndex);

bool UTIL_StructureCanBeUpgraded(const edict_t* Structure);

bool UTIL_MarineResearchIsAvailable(const NSResearch Research);
bool UTIL_ArmsLabResearchIsAvailable(const NSResearch Research);
bool UTIL_PrototypeLabResearchIsAvailable(const NSResearch Research);
bool UTIL_ObservatoryResearchIsAvailable(const NSResearch Research);
bool UTIL_ArmouryResearchIsAvailable(const NSResearch Research);

bool BotCommanderPlaceStructure(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderDropItem(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderRecycleStructure(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderUpgradeStructure(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderResearchTech(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderSelectStructure(bot_t* pBot, const edict_t* Structure, int ActionIndex, int Priority);

bool UTIL_ResearchIsComplete(const NSResearch Research);

int UTIL_GetCostOfResearch(const NSResearch Research);


Vector UTIL_FindClearCommanderOriginForBuild(const bot_t* Commander, const Vector& BuildLocation, const float CommanderViewZ);

bool UTIL_CancelMarineOrder(bot_t* CommanderBot, int CommanderOrderIndex);
bool UTIL_IsMarineOrderComplete(bot_t* CommanderBot, int CommanderOrderIndex);
bool UTIL_ConfirmMarineOrderComplete(bot_t* CommanderBot, int CommanderOrderIndex);

void CommanderQueueNextAction(bot_t* pBot);

int UTIL_FindClosestAvailableMarinePlayer(bot_t* CommanderBot, const Vector& Location);

bool UTIL_IsArmouryUpgrading(const edict_t* ArmouryEdict);
bool UTIL_IsTurretFactoryUpgrading(const edict_t* TurretFactoryEdict);



int UTIL_GetNumArmouriesUpgrading();

bool UTIL_ResearchActionAlreadyExists(const bot_t* Commander, const NSResearch Research);

int UTIL_GetQueuedBuildRequestsOfType(bot_t* CommanderBot, NSStructureType StructureType);
int UTIL_GetQueuedUpgradeRequestsOfType(bot_t* CommanderBot, NSStructureType StructureToBeUpgraded);
int UTIL_GetQueuedItemDropRequestsOfType(bot_t* CommanderBot, NSDeployableItem ItemType);

bool UTIL_ItemCanBeDeployed(NSDeployableItem ItemToDeploy);

#endif
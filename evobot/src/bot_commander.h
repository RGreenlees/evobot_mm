//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_commander.h
// 
// Contains commander-related code.
//

#pragma once

#ifndef BOT_COMMANDER_H
#define BOT_COMMANDER_H

#include "bot_structs.h"

// How often should the commander nag you to do as you're told if you're not listening, in seconds
static const float min_order_reminder_time = 20.0f;

// Every time the commander takes an action (moves view, selects building, places structure etc.), how long to wait before doing next action
static const float commander_action_cooldown = 1.0f;

// How close in metres (see UTIL_MetresToGoldSrcUnits function) a marine should be before confirming move order complete
static const float move_order_success_dist_metres = 2.0f;

// What is the minimum acceptable resource towers for a team? Bots will prioritise building them if below this number
static const int min_desired_resource_towers = 3;

bool CommanderProgressAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressBuildAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressRecycleAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressUpgradeAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressResearchAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressItemDropAction(bot_t* CommanderBot, int ActionIndex, int Priority);
bool CommanderProgressOrderAction(bot_t* CommanderBot, int ActionIndex, int Priority);

bool UTIL_IssueOrderForAction(bot_t* CommanderBot, int PlayerIndex, int ActionIndex, int Priority);
void UTIL_IssueMarineMoveToOrder(bot_t* CommanderBot, edict_t* Recipient, const Vector Destination);
void UTIL_IssueMarineBuildOrder(bot_t* CommanderBot, edict_t* Recipient, edict_t* StructureToBuild);

// First free action slot for the commander, for the given priority. -1 if none found
int UTIL_CommanderFirstFreeActionIndex(bot_t* CommanderBot, int Priority);

void CommanderReceiveAlert(bot_t* pBot, const Vector Location, const PlayerAlertType AlertType);
void CommanderReceiveHealthRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveAmmoRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveOrderRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveBaseAttackAlert(bot_t* pBot, const Vector Location);

void UTIL_OrganiseCommanderActions(bot_t* pBot);

void UpdateCommanderOrders(bot_t* Commander);
void UpdateCommanderActions(bot_t* Commander);

void CommanderQueueInfantryPortalBuild(bot_t* pBot, int Priority);
void CommanderQueuePhaseGateBuild(bot_t* pBot, const Vector Location, int Priority);
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
void CommanderQueueItemDrop(bot_t* pBot, NSDeployableItem ItemToDeploy, const Vector DropLocation, edict_t* Recipient, int Priority);
void CommanderQueueActionOrder(bot_t* pBot, int Priority, int AssignedPlayer);
void CommanderQueueElectricResearch(bot_t* pBot, int Priority, edict_t* StructureToElectrify);

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


void UTIL_CommanderQueueStructureBuildAtLocation(bot_t* pBot, const Vector Location, NSStructureType StructureType, int Priority);

bool UTIL_ResearchInProgress(NSResearch Research);

void QueueHeavyArmourLoadout(bot_t* CommanderBot, const Vector Area, int Priority);

void QueueSecureHiveAction(bot_t* CommanderBot, const Vector Area, int Priority);

void QueueSiegeHiveAction(bot_t* CommanderBot, const Vector Area, int Priority);

bool UTIL_ResearchIsAlreadyQueued(bot_t* pBot, NSResearch Research);
bool UTIL_HasIdleArmsLab();
edict_t* UTIL_GetFirstIdleArmsLab();

void LinkCommanderActionsToDroppedItems(bot_t* Commander);

bool UTIL_ItemIsAlreadyLinked(bot_t* Commander, edict_t* Item);

void UTIL_LinkItem(bot_t* Commander, edict_t* Item);

bool UTIL_CancelCommanderPlayerOrder(bot_t* Commander, int PlayerIndex);

bool UTIL_ActionExistsInLocation(const bot_t* Commander, const Vector CheckPoint);

void UTIL_ClearCommanderAction(bot_t* Commander, int ActionIndex, int Priority);
void UTIL_ClearCommanderOrder(bot_t* Commander, int OrderIndex);

bool UTIL_StructureCanBeUpgraded(const edict_t* Structure);

bool UTIL_MarineResearchIsAvailable(const NSResearch Research);
bool UTIL_ArmsLabResearchIsAvailable(const NSResearch Research);
bool UTIL_PrototypeLabResearchIsAvailable(const NSResearch Research);
bool UTIL_ObservatoryResearchIsAvailable(const NSResearch Research);
bool UTIL_ArmouryResearchIsAvailable(const NSResearch Research);
bool UTIL_ElectricalResearchIsAvailable(const edict_t* Structure);



bool BotCommanderPlaceStructure(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderDropItem(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderRecycleStructure(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderUpgradeStructure(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderResearchTech(bot_t* pBot, int ActionIndex, int Priority);
bool BotCommanderSelectStructure(bot_t* pBot, const edict_t* Structure, int ActionIndex, int Priority);



int UTIL_GetCostOfResearch(const NSResearch Research);


Vector UTIL_FindClearCommanderOriginForBuild(const bot_t* Commander, const Vector BuildLocation, const float CommanderViewZ);

bool UTIL_CancelMarineOrder(bot_t* CommanderBot, int CommanderOrderIndex);
bool UTIL_IsMarineOrderComplete(bot_t* CommanderBot, int CommanderOrderIndex);
bool UTIL_ConfirmMarineOrderComplete(bot_t* CommanderBot, int CommanderOrderIndex);

void CommanderQueueNextAction(bot_t* pBot);

int UTIL_FindClosestAvailableMarinePlayer(bot_t* CommanderBot, const Vector Location);

int UTIL_GetNumArmouriesUpgrading();

bool UTIL_ResearchActionAlreadyExists(const bot_t* Commander, const NSResearch Research);

int UTIL_GetQueuedBuildRequestsOfType(bot_t* CommanderBot, NSStructureType StructureType);
int UTIL_GetQueuedUpgradeRequestsOfType(bot_t* CommanderBot, NSStructureType StructureToBeUpgraded);
int UTIL_GetQueuedItemDropRequestsOfType(bot_t* CommanderBot, NSDeployableItem ItemType);

bool UTIL_ItemCanBeDeployed(NSDeployableItem ItemToDeploy);

#endif
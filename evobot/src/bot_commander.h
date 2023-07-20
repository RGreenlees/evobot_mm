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

void COMM_CommanderProgressAction(bot_t* CommanderBot, commander_action* Action);
void BotCommanderDeploy(bot_t* pBot, commander_action* Action);
void BotCommanderUpgradeStructure(bot_t* pBot, commander_action* Action);
void BotCommanderResearchTech(bot_t* pBot, commander_action* Action);
void BotCommanderRecycleStructure(bot_t* pBot, commander_action* Action);
void BotCommanderSelectStructure(bot_t* pBot, const edict_t* Structure, commander_action* Action);

void UTIL_IssueMarineMoveToOrder(bot_t* CommanderBot, edict_t* Recipient, const Vector Destination);
void UTIL_IssueMarineBuildOrder(bot_t* CommanderBot, edict_t* Recipient, edict_t* StructureToBuild);

void CommanderReceiveAlert(bot_t* pBot, const Vector Location, const PlayerAlertType AlertType);
void CommanderReceiveHealthRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveAmmoRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveOrderRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveBaseAttackAlert(bot_t* pBot, const Vector Location);

void UpdateCommanderOrders(bot_t* Commander);

bool ShouldCommanderLeaveChair(bot_t* pBot);

void CommanderThink(bot_t* CommanderBot);

void CommanderGetPrimaryTask(bot_t* pBot, bot_task* Task);

void COMM_UpdateAndClearCommanderActions(bot_t* CommanderBot);

bool COMM_IsWaitingOnBuildLink(bot_t* CommanderBot);

bool UTIL_IsMarineOrderValid(bot_t* CommanderBot, int CommanderOrderIndex);
bool UTIL_IsCommanderActionValid(bot_t* CommanderBot, commander_action* Action);
bool UTIL_CommanderBuildActionIsValid(bot_t* CommanderBot, commander_action* Action);

bool UTIL_ResearchInProgress(NSResearch Research);

bool UTIL_HasIdleArmsLab();
edict_t* UTIL_GetFirstIdleArmsLab();

void UTIL_LinkItem(bot_t* Commander, edict_t* Item);

bool UTIL_CancelCommanderPlayerOrder(bot_t* Commander, int PlayerIndex);

void UTIL_ClearCommanderAction(commander_action* Action);
void UTIL_ClearCommanderOrder(bot_t* Commander, int OrderIndex);

bool UTIL_StructureCanBeUpgraded(const edict_t* Structure);

bool UTIL_MarineResearchIsAvailable(const NSResearch Research);
bool UTIL_ArmsLabResearchIsAvailable(const NSResearch Research);
bool UTIL_PrototypeLabResearchIsAvailable(const NSResearch Research);
bool UTIL_ObservatoryResearchIsAvailable(const NSResearch Research);
bool UTIL_ArmouryResearchIsAvailable(const NSResearch Research);
bool UTIL_ElectricalResearchIsAvailable(const edict_t* Structure);



int UTIL_GetCostOfResearch(const NSResearch Research);

Vector UTIL_FindClearCommanderOriginForBuild(const bot_t* Commander, const Vector BuildLocation, const float CommanderViewZ);

bool UTIL_CancelMarineOrder(bot_t* CommanderBot, int CommanderOrderIndex);
bool UTIL_IsMarineOrderComplete(bot_t* CommanderBot, int CommanderOrderIndex);
bool UTIL_ConfirmMarineOrderComplete(bot_t* CommanderBot, int CommanderOrderIndex);

int UTIL_FindClosestAvailableMarinePlayer(bot_t* CommanderBot, const Vector Location);

int UTIL_GetNumArmouriesUpgrading();

bool UTIL_ItemCanBeDeployed(NSDeployableItem ItemToDeploy);

void COMM_ConfirmObjectDeployed(bot_t* pBot, commander_action* Action, edict_t* DeployedObject);

const resource_node* COMM_GetResNodeCapOpportunityNearestLocation(const Vector SearchLocation);
const hive_definition* COMM_GetEmptyHiveOpportunityNearestLocation(const Vector SearchLocation);
const hive_definition* COMM_GetHiveSiegeOpportunityNearestLocation(const Vector SearchLocation);

void COMM_SetInfantryPortalBuildAction(edict_t* CommChair, commander_action* Action);
void COMM_SetTurretBuildAction(edict_t* TurretFactory, commander_action* Action);
void COMM_SetSiegeTurretBuildAction(edict_t* TurretFactory, commander_action* Action, const Vector SiegeTarget);

void COMM_SetElectrifyStructureAction(edict_t* Structure, commander_action* Action);

Vector UTIL_GetNextTurretPosition(edict_t* TurretFactory);

void COMM_SetNextSecureHiveAction(const hive_definition* Hive, commander_action* Action);
void COMM_SetNextSiegeHiveAction(const hive_definition* Hive, commander_action* Action);
void COMM_SetNextResearchAction(commander_action* Action);

void COMM_SetNextBuildBaseAction(commander_action* Action);

commander_action* COMM_GetNextAction(bot_t* CommanderBot);

#endif
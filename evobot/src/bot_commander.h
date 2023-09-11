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
void COMM_IssueMarineSecureHiveOrder(bot_t* CommanderBot, edict_t* Recipient, const hive_definition* HiveToSecure);
void COMM_IssueMarineSiegeHiveOrder(bot_t* CommanderBot, edict_t* Recipient, const hive_definition* HiveToSiege, const Vector SiegePosition);
void COMM_IssueMarineSecureResNodeOrder(bot_t* CommanderBot, edict_t* Recipient, const resource_node* ResNode);

void CommanderReceiveAlert(bot_t* pBot, const Vector Location, const PlayerAlertType AlertType);

void CommanderReceiveWeaponRequest(bot_t* pBot, edict_t* Requestor, NSStructureType ItemToDrop);
void CommanderReceiveHealthRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveCatalystRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveAmmoRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveOrderRequest(bot_t* pBot, edict_t* Requestor);
void CommanderReceiveBaseAttackAlert(bot_t* pBot, const Vector Location);

void UpdateCommanderOrders(bot_t* Commander);

bool ShouldCommanderLeaveChair(bot_t* pBot);

void CommanderThink(bot_t* CommanderBot);

void CommanderGetPrimaryTask(bot_t* pBot, bot_task* Task);

void COMM_UpdateAndClearCommanderActions(bot_t* CommanderBot);
void COMM_UpdateAndClearCommanderOrders(bot_t* CommanderBot);

bool COMM_ClearCompletedOrders(bot_t* CommanderBot);

int COMM_GetNumMoveOrdersNearLocation(bot_t* CommanderBot, const Vector Location, const float MaxDistance);

int COMM_GetNumMarinesAndOrdersInLocation(bot_t* CommanderBot, const Vector Location, const float SearchDist);

edict_t* COMM_GetNearestMarineWithoutOrder(bot_t* CommanderBot, const Vector SearchLocation, float MinDistance, float MaxDistance);

bool COMM_SecureHiveNeedsDeployment(bot_t* CommanderBot, const hive_definition* Hive);
int COMM_GetNumMarinesSecuringHive(bot_t* CommanderBot, const hive_definition* Hive, float MaxDistance);
int COMM_GetNumMarinesSiegingHive(bot_t* CommanderBot, const hive_definition* Hive, float MaxDistance);
int COMM_GetNumMarinesSecuringResNode(bot_t* CommanderBot, const resource_node* ResNode, float MaxDistance);
Vector COMM_GetGoodSiegeLocation(const hive_definition* HiveToSiege);

bool COMM_IsWaitingOnBuildLink(bot_t* CommanderBot);

bool UTIL_IsMarineOrderValid(bot_t* CommanderBot, int CommanderOrderIndex);
bool UTIL_IsCommanderActionValid(bot_t* CommanderBot, commander_action* Action);

bool UTIL_ResearchInProgress(NSResearch Research);

bool UTIL_HasIdleArmsLab();
edict_t* UTIL_GetFirstIdleArmsLab();

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

bool COMM_IsSecureHiveOrderComplete(bot_t* CommanderBot, commander_order* Order);
bool COMM_IsSiegeHiveOrderComplete(commander_order* Order);
bool COMM_IsSecureResNodeOrderComplete(commander_order* Order);

int UTIL_FindClosestAvailableMarinePlayer(bot_t* CommanderBot, const Vector Location);

int UTIL_GetNumArmouriesUpgrading();

bool UTIL_ItemCanBeDeployed(NSStructureType ItemToDeploy);

void COMM_ConfirmObjectDeployed(bot_t* pBot, commander_action* Action, edict_t* DeployedObject);

const resource_node* COMM_GetResNodeCapOpportunityNearestLocation(const Vector SearchLocation);
const hive_definition* COMM_GetEmptyHiveOpportunityNearestLocation(bot_t* CommanderBot, const Vector SearchLocation);
const hive_definition* COMM_GetUnsecuredEmptyHiveNearestLocation(bot_t* CommanderBot, const Vector SearchLocation);
const hive_definition* COMM_GetUnsecuredEmptyHiveFurthestToLocation(bot_t* CommanderBot, const Vector SearchLocation);
const hive_definition* COMM_GetHiveSiegeOpportunityNearestLocation(bot_t* CommanderBot, const Vector SearchLocation);

void COMM_SetInfantryPortalBuildAction(edict_t* CommChair, commander_action* Action);
void COMM_SetTurretBuildAction(edict_t* TurretFactory, commander_action* Action);
void COMM_SetSiegeTurretBuildAction(edict_t* TurretFactory, commander_action* Action, const Vector SiegeTarget, bool bIsUrgent);

void COMM_SetElectrifyStructureAction(edict_t* Structure, commander_action* Action);

Vector UTIL_GetNextTurretPosition(edict_t* TurretFactory);

void COMM_SetNextSecureHiveAction(bot_t* CommanderBot, const hive_definition* Hive, commander_action* Action);
void COMM_SetNextSiegeHiveAction(bot_t* CommanderBot, const hive_definition* Hive, commander_action* Action);
void COMM_SetNextResearchAction(commander_action* Action);

void COMM_SetNextBuildAction(bot_t* CommanderBot, commander_action* Action);
void COMM_SetNextSupportAction(bot_t* CommanderBot, commander_action* Action);
void COMM_SetNextRecycleAction(bot_t* CommanderBot, commander_action* Action);

commander_action* COMM_GetNextAction(bot_t* CommanderBot);

edict_t* COMM_GetMarineEligibleToBuildSiege(const hive_definition* Hive);

#endif
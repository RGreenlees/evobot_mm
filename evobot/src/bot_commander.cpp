//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_commander.cpp
// 
// Contains commander-related code. Some stuff should be separated out into helper file
//

#include "bot_commander.h"
#include "bot_navigation.h"
#include "bot_tactical.h"
#include "game_state.h"
#include "bot_util.h"
#include "general_util.h"

#include <unordered_map>

#include <extdll.h>
#include <dllapi.h>
#include <meta_api.h>



extern resource_node ResourceNodes[64];
extern int NumTotalResNodes;

extern std::unordered_map<int, buildable_structure> MarineBuildableStructureMap;

extern dropped_marine_item AllMarineItems[256];
extern int NumTotalMarineItems;

extern hive_definition Hives[10];
extern int NumTotalHives;

extern edict_t* clients[MAX_CLIENTS];
extern bot_t bots[MAX_CLIENTS];

extern bool bGameIsActive;

bool CommanderProgressAction(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	switch (action->ActionType)
	{
	case ACTION_BUILD:
		return CommanderProgressBuildAction(CommanderBot, ActionIndex, Priority);
	case ACTION_RECYCLE:
		return CommanderProgressRecycleAction(CommanderBot, ActionIndex, Priority);
	case ACTION_UPGRADE:
		return CommanderProgressUpgradeAction(CommanderBot, ActionIndex, Priority);
	case ACTION_RESEARCH:
		return CommanderProgressResearchAction(CommanderBot, ActionIndex, Priority);
	case ACTION_DROPITEM:
		return CommanderProgressItemDropAction(CommanderBot, ActionIndex, Priority);
	case ACTION_GIVEORDER:
		return CommanderProgressOrderAction(CommanderBot, ActionIndex, Priority);
	default:
		return false;
	}

	return false;
}

bool CommanderProgressBuildAction(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	// Structure has been placed successfully
	if (!FNullEnt(action->StructureOrItem))
	{
		Vector StructureLocation = action->StructureOrItem->v.origin;

		// Clear action if the structure is fully built
		if (UTIL_StructureIsFullyBuilt(action->StructureOrItem))
		{
			UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
			return false;
		}

		// If there is someone nearby, just wait. Hopefully they'll build it by themselves
		if (UTIL_AnyMarinePlayerNearLocation(action->StructureOrItem->v.origin, UTIL_MetresToGoldSrcUnits(15.0f)))
		{
			return false;
		}

		// If we've already assigned someone to build it
		if (UTIL_ActionHasValidPlayerAssigned(CommanderBot, ActionIndex, Priority))
		{
			commander_order* OrderInfo = &CommanderBot->LastPlayerOrders[action->AssignedPlayer];

			// Add this in to prevent order spam
			if ((gpGlobals->time - OrderInfo->LastReminderTime) < 5.0f) { return false; }

			// If a player is assigned, but we've not actually issued the order yet
			if (!OrderInfo->bIsActive)
			{
				return UTIL_IssueOrderForAction(CommanderBot, action->AssignedPlayer, ActionIndex, Priority);
			}

			// We've placed the structure since the last order, so now we have to ensure the order target is set correctly
			if (!FNullEnt(action->StructureOrItem) && FNullEnt(OrderInfo->Target))
			{
				OrderInfo->Target = action->StructureOrItem;
			}


			// Player is eligible for a reminder if it has been a while since we gave them an order and they're still miles away
			bool bEligibleForReminder = (gpGlobals->time - OrderInfo->LastReminderTime) > min_order_reminder_time;

			if (bEligibleForReminder)
			{
				float newDist = UTIL_GetPathCostBetweenLocations(MARINE_REGULAR_NAV_PROFILE, clients[action->AssignedPlayer]->v.origin, OrderInfo->Target->v.origin);

				if (newDist > OrderInfo->LastPlayerDistance)
				{
					return UTIL_IssueOrderForAction(CommanderBot, action->AssignedPlayer, ActionIndex, Priority);
				}
				else
				{
					OrderInfo->LastReminderTime = gpGlobals->time - 15.0f;
					OrderInfo->LastPlayerDistance = newDist;
				}
			}

			// They're on their way, leave them alone
			return false;
		}

		action->AssignedPlayer = -1;

		// Find the nearest person who isn't already under orders to come and build it
		int NewAssignedPlayer = UTIL_FindClosestAvailableMarinePlayer(CommanderBot, action->BuildLocation);

		if (NewAssignedPlayer > -1)
		{
			action->AssignedPlayer = NewAssignedPlayer;
			return UTIL_IssueOrderForAction(CommanderBot, NewAssignedPlayer, ActionIndex, Priority);
		}

		// Didn't find anyone :(
		return false;

	}
	else
	{

		bool bHasEnoughResources = (CommanderBot->resources >= UTIL_GetCostOfStructureType(action->StructureToBuild));
		if (!bHasEnoughResources) { return false; }

		// If we just tried placing a building, but don't yet see it, wait a second to give it time to register. Prevents building spam.
		// We return true to prevent the AI switching to other tasks while they should be waiting to see if their last action worked
		if (action->bHasAttemptedAction && (gpGlobals->time - action->StructureBuildAttemptTime) < build_attempt_retry_time)
		{
			return true;
		}

		// If we are building at base, it's generally safe, so place a structure if any marines are within 20m. Otherwise, they need to be at most 10m away AND have LOS of the build location
		bool bBuildingAtBase = (vDist2DSq(UTIL_GetCommChairLocation(), action->BuildLocation) < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)));
		float MaxDistFromBuildLocation = bBuildingAtBase ? UTIL_MetresToGoldSrcUnits(20.0f) : UTIL_MetresToGoldSrcUnits(10.0f);

		bool IsMarineNearby = (bBuildingAtBase) ? UTIL_AnyMarinePlayerNearLocation(action->BuildLocation, MaxDistFromBuildLocation) : UTIL_AnyPlayerOnTeamWithLOS(action->BuildLocation, MARINE_TEAM, MaxDistFromBuildLocation);

		bool IsAlienNearby = UTIL_AnyPlayerOnTeamWithLOS(action->BuildLocation, ALIEN_TEAM, UTIL_MetresToGoldSrcUnits(10.0f));

		// If a marine is ready to build, and there isn't an alien around or offence chamber that could attack it, then go for it
		if (IsMarineNearby && !IsAlienNearby && !UTIL_AnyTurretWithLOSToLocation(action->BuildLocation, ALIEN_TEAM))
		{
			// Otherwise, move to the location and put it down
			return BotCommanderPlaceStructure(CommanderBot, ActionIndex, Priority);
		}

		// If not, then order someone to go to that location so we can place it
		if (UTIL_ActionHasValidPlayerAssigned(CommanderBot, ActionIndex, Priority))
		{
			edict_t* AssignedPlayerEdict = clients[action->AssignedPlayer];

			// Add this in to prevent order spam in rare circumstances that the player is already at the location but the bot thinks they're not there yet
			if ((gpGlobals->time - CommanderBot->LastPlayerOrders[action->AssignedPlayer].LastReminderTime) < 5.0f) { return false; }

			if (!CommanderBot->LastPlayerOrders[action->AssignedPlayer].bIsActive)
			{
				return UTIL_IssueOrderForAction(CommanderBot, action->AssignedPlayer, ActionIndex, Priority);
			}

			bool bIsPlayerAtLocation = (vDist2DSq(AssignedPlayerEdict->v.origin, action->BuildLocation) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && UTIL_PointIsDirectlyReachable(AssignedPlayerEdict->v.origin, action->BuildLocation));
			bool bHasOrderBeenIssuedRecently = (gpGlobals->time - CommanderBot->LastPlayerOrders[action->AssignedPlayer].LastReminderTime) < min_order_reminder_time;

			if (!bIsPlayerAtLocation && !bHasOrderBeenIssuedRecently)
			{
				float newDist = UTIL_GetPathCostBetweenLocations(MARINE_REGULAR_NAV_PROFILE, clients[action->AssignedPlayer]->v.origin, CommanderBot->LastPlayerOrders[action->AssignedPlayer].MoveLocation);

				if (newDist > CommanderBot->LastPlayerOrders[action->AssignedPlayer].LastPlayerDistance)
				{
					return UTIL_IssueOrderForAction(CommanderBot, action->AssignedPlayer, ActionIndex, Priority);
				}
				else
				{
					CommanderBot->LastPlayerOrders[action->AssignedPlayer].LastReminderTime += 5.0f;
					CommanderBot->LastPlayerOrders[action->AssignedPlayer].LastPlayerDistance = newDist;
				}
			}

			return false;
		}

		action->AssignedPlayer = -1;

		int NewAssignedPlayer = UTIL_FindClosestAvailableMarinePlayer(CommanderBot, action->BuildLocation);

		if (NewAssignedPlayer > -1)
		{
			if (IsPlayerHuman(clients[NewAssignedPlayer]) || !(GetBotPointer(clients[NewAssignedPlayer])) || GetBotPointer(clients[NewAssignedPlayer])->CurrentRole != BOT_ROLE_SWEEPER)
			{
				action->AssignedPlayer = NewAssignedPlayer;
				return UTIL_IssueOrderForAction(CommanderBot, NewAssignedPlayer, ActionIndex, Priority);
			}
		}

		return false;

	}

	// Didn't find anyone :(
	return false;
}

bool CommanderProgressRecycleAction(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	// Building is recycling, we're done here
	if (UTIL_StructureIsRecycling(action->StructureOrItem))
	{
		UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
		return false;
	}
	else
	{
		return BotCommanderRecycleStructure(CommanderBot, ActionIndex, Priority);
	}
}

bool CommanderProgressUpgradeAction(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	switch (action->StructureToBuild)
	{
	case STRUCTURE_MARINE_ARMOURY:
	{
		if (UTIL_IsArmouryUpgrading(action->StructureOrItem))
		{
			UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
			return false;
		}
	}
	break;
	case STRUCTURE_MARINE_TURRETFACTORY:
	{
		if (UTIL_IsTurretFactoryUpgrading(action->StructureOrItem))
		{
			UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
			return false;
		}
	}
	break;
	default:
		break;
	}

	return BotCommanderUpgradeStructure(CommanderBot, ActionIndex, Priority);
}

bool BotCommanderDropItem(bot_t* pBot, int ActionIndex, int Priority)
{

	if (gpGlobals->time < pBot->next_commander_action_time || !UTIL_IsCommanderActionValid(pBot, ActionIndex, Priority))
	{
		return false;
	}

	commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];

	if (action->NumActionAttempts > 10)
	{
		UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
		return false;
	}

	if (!action->BuildLocation && action->ActionTarget)
	{
		action->BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, action->ActionTarget->v.origin, UTIL_MetresToGoldSrcUnits(1.0f));
	}

	if (action->ItemToDeploy == ITEM_MARINE_SCAN)
	{
		if (GetStructureTypeFromEdict(pBot->CommanderCurrentlySelectedBuilding) != STRUCTURE_MARINE_OBSERVATORY)
		{
			return BotCommanderSelectStructure(pBot, UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_OBSERVATORY, action->BuildLocation, UTIL_MetresToGoldSrcUnits(1000.0f), true, false), ActionIndex, Priority);
		}
	}

	pBot->pEdict->v.v_angle = ZERO_VECTOR;

	Vector CommanderViewOrigin = pBot->pEdict->v.origin;
	CommanderViewOrigin.z = GetCommanderViewZHeight();

	if (!action->DesiredCommanderLocation || vDist2DSq(action->DesiredCommanderLocation, action->BuildLocation) > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
	{
		action->DesiredCommanderLocation = UTIL_FindClearCommanderOriginForBuild(pBot, action->BuildLocation, GetCommanderViewZHeight());

		if (action->DesiredCommanderLocation == ZERO_VECTOR)
		{
			action->NumActionAttempts++;
			return false;
		}
	}

	if (action->bHasAttemptedAction)
	{
		if (action->NumActionAttempts > 2)
		{
			UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
			return false;
		}
		else
		{
			action->BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, action->BuildLocation, UTIL_MetresToGoldSrcUnits(1.0f));
			action->DesiredCommanderLocation = UTIL_FindClearCommanderOriginForBuild(pBot, action->BuildLocation, GetCommanderViewZHeight());
		}
	}

	if (vDist2DSq(CommanderViewOrigin, action->DesiredCommanderLocation) > sqrf(8.0f))
	{
		action->bHasAttemptedAction = false;
		pBot->UpMove = action->DesiredCommanderLocation.x / kWorldPosNetworkConstant;
		pBot->SideMove = action->DesiredCommanderLocation.y / kWorldPosNetworkConstant;
		pBot->ForwardMove = 0.0f;

		pBot->impulse = IMPULSE_COMMANDER_MOVETO;

		pBot->next_commander_action_time = gpGlobals->time + 0.5f;

		return true;
	}

	Vector PickRay = UTIL_GetVectorNormal(action->BuildLocation - CommanderViewOrigin);

	pBot->UpMove = PickRay.x * kSelectionNetworkConstant;
	pBot->SideMove = PickRay.y * kSelectionNetworkConstant;
	pBot->ForwardMove = PickRay.z * kSelectionNetworkConstant;

	pBot->impulse = action->ItemToDeploy;

	pBot->pEdict->v.button |= IN_ATTACK;


	action->NumActionAttempts++;
	action->bHasAttemptedAction = true;
	action->StructureBuildAttemptTime = gpGlobals->time;

	pBot->next_commander_action_time = gpGlobals->time + 0.3f;

	if (action->ItemToDeploy == ITEM_MARINE_SCAN)
	{
		pBot->CommanderLastScanTime = gpGlobals->time;
	}

	return true;
}

bool BotCommanderRecycleStructure(bot_t* pBot, int ActionIndex, int Priority)
{
	if (gpGlobals->time < pBot->next_commander_action_time || !UTIL_IsCommanderActionValid(pBot, ActionIndex, Priority))
	{
		UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
		return false;
	}

	commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];
	buildable_structure* BuildRef = UTIL_GetBuildableStructureRefFromEdict(action->StructureOrItem);

	if (action->NumActionAttempts > 10)
	{
		UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
		return false;
	}

	// If we are already selecting the building then we can just send the impulse and finish up
	if (pBot->CommanderCurrentlySelectedBuilding == action->StructureOrItem)
	{
		if (BuildRef)
		{
			if (!vEquals(action->LastAttemptedCommanderLocation, ZERO_VECTOR))
			{
				BuildRef->LastSuccessfulCommanderLocation = action->LastAttemptedCommanderLocation;
			}

			if (!vEquals(action->LastAttemptedCommanderAngle, ZERO_VECTOR))
			{
				BuildRef->LastSuccessfulCommanderAngle = action->LastAttemptedCommanderAngle;
			}
		}

		pBot->impulse = IMPULSE_COMMANDER_RECYCLEBUILDING;
		action->ActionStep = ACTION_STEP_NONE;

		pBot->next_commander_action_time = gpGlobals->time + commander_action_cooldown;
		return true;
	}

	return BotCommanderSelectStructure(pBot, action->StructureOrItem, ActionIndex, Priority);
}

bool CommanderProgressOrderAction(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	if (action->AssignedPlayer < 0)
	{
		UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
		return false;
	}

	edict_t* PlayerEdict = clients[action->AssignedPlayer];

	if (IsPlayerDead(PlayerEdict) || IsPlayerCommander(PlayerEdict) || !IsPlayerOnMarineTeam(PlayerEdict) || IsPlayerBeingDigested(PlayerEdict))
	{
		UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
		return false;
	}

	const resource_node* FreeResNodeIndex = UTIL_FindEligibleResNodeClosestToLocation(PlayerEdict->v.origin, MARINE_TEAM, true);

	if (FreeResNodeIndex)
	{
		if (vDist2DSq(PlayerEdict->v.origin, FreeResNodeIndex->origin) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			Vector MoveLoc = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, FreeResNodeIndex->origin, UTIL_MetresToGoldSrcUnits(2.0f));
			if (MoveLoc == ZERO_VECTOR) { MoveLoc = FreeResNodeIndex->origin; }
			UTIL_IssueMarineMoveToOrder(CommanderBot, PlayerEdict, MoveLoc);
		}
		else
		{
			if (FreeResNodeIndex->bIsOccupied)
			{
				char buf[128];
				sprintf(buf, "Destroy the alien tower %s so I can put down one", STRING(PlayerEdict->v.netname));
				BotTeamSay(CommanderBot, 2.0f, buf);
			}
			else if (CommanderBot->resources < kResourceTowerCost)
			{
				char buf[128];
				sprintf(buf, "Hold on %s, just waiting on resources", STRING(PlayerEdict->v.netname));
				BotTeamSay(CommanderBot, 2.0f, buf);
			}
			else
			{
				char buf[128];
				sprintf(buf, "Hold on %s, will drop a tower for you in a moment", STRING(PlayerEdict->v.netname));
				BotTeamSay(CommanderBot, 2.0f, buf);
			}
		}
		UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
		return true;
	}
	else
	{
		char buf[128];
		sprintf(buf, "Hold on %s, I'll find something for you to do", STRING(PlayerEdict->v.netname));
		BotTeamSay(CommanderBot, buf);
	}


	UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);

	return true;
}

bool UTIL_IssueOrderForAction(bot_t* CommanderBot, int PlayerIndex, int ActionIndex, int Priority)
{
	if (gpGlobals->time < CommanderBot->next_commander_action_time) { return false; }

	if (!UTIL_IsCommanderActionValid(CommanderBot, ActionIndex, Priority)) { return false; }

	edict_t* Recipient = clients[PlayerIndex];

	if (IsPlayerDead(Recipient) || !IsPlayerOnMarineTeam(Recipient) || IsPlayerBeingDigested(Recipient)) { return false; }

	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	if (action->StructureOrItem)
	{
		if (action->StructureOrItem->v.deadflag == DEAD_DEAD) { return false; }
		UTIL_IssueMarineBuildOrder(CommanderBot, Recipient, action->StructureOrItem);
	}
	else
	{
		Vector MoveLoc = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, action->BuildLocation, UTIL_MetresToGoldSrcUnits(2.0f));
		if (MoveLoc == ZERO_VECTOR) { MoveLoc = action->BuildLocation; }
		UTIL_IssueMarineMoveToOrder(CommanderBot, Recipient, MoveLoc);
	}

	return true;
}

// TODO: See if we can get AI marines receiving orders from AI commanders correctly. No idea why it doesn't work
void UTIL_IssueMarineMoveToOrder(bot_t* CommanderBot, edict_t* Recipient, const Vector Destination)
{
	if (gpGlobals->time < CommanderBot->next_commander_action_time) { return; }

	if (!IsPlayerOnMarineTeam(Recipient) || IsPlayerDead(Recipient) || IsPlayerCommander(Recipient) || IsPlayerBeingDigested(Recipient)) { return; }

	MESSAGE_BEGIN(MSG_ONE, REG_USER_MSG("SetOrder", -1), NULL, Recipient);
	WRITE_BYTE(ENTINDEX(Recipient));
	WRITE_BYTE(ORDERTYPEL_MOVE);

	WRITE_COORD(Destination.x);
	WRITE_COORD(Destination.y);
	WRITE_COORD(Destination.z);

	WRITE_BYTE(AVH_USER3_NONE);
	WRITE_BYTE(false);
	WRITE_BYTE(kOrderStatusActive);

	MESSAGE_END();

	if (IsPlayerBot(Recipient))
	{
		bot_t* BotRef = GetBotPointer(Recipient);

		if (BotRef)
		{
			BotReceiveCommanderOrder(BotRef, ORDERTYPEL_MOVE, AVH_USER3_NONE, Destination);
		}
	}

	CommanderBot->next_commander_action_time = gpGlobals->time + 0.5f;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] == Recipient)
		{
			float dist = UTIL_GetPathCostBetweenLocations(MARINE_REGULAR_NAV_PROFILE, Recipient->v.origin, Destination);
			CommanderBot->LastPlayerOrders[i].bIsActive = true;
			CommanderBot->LastPlayerOrders[i].OrderType = ORDERTYPEL_MOVE;
			CommanderBot->LastPlayerOrders[i].MoveLocation = Destination;
			CommanderBot->LastPlayerOrders[i].Target = nullptr;
			CommanderBot->LastPlayerOrders[i].LastReminderTime = gpGlobals->time;
			CommanderBot->LastPlayerOrders[i].LastPlayerDistance = dist;
			break;
		}
	}
}

void UTIL_IssueMarineBuildOrder(bot_t* CommanderBot, edict_t* Recipient, edict_t* StructureToBuild)
{
	if (FNullEnt(StructureToBuild) || StructureToBuild->v.deadflag == DEAD_DEAD) { return; }

	if (gpGlobals->time < CommanderBot->next_commander_action_time) { return; }

	if (IsPlayerDead(Recipient) || !IsPlayerOnMarineTeam(Recipient) || IsPlayerCommander(Recipient) || IsPlayerBeingDigested(Recipient)) { return; }

	MESSAGE_BEGIN(MSG_ONE, REG_USER_MSG("SetOrder", -1), NULL, Recipient);
	WRITE_BYTE(ENTINDEX(Recipient));
	WRITE_BYTE(ORDERTYPET_BUILD);

	WRITE_SHORT(ENTINDEX(StructureToBuild));

	WRITE_BYTE(StructureToBuild->v.iuser3);
	WRITE_BYTE(false);
	WRITE_BYTE(kOrderStatusActive);

	MESSAGE_END();

	if (IsPlayerBot(Recipient))
	{
		bot_t* BotRef = GetBotPointer(Recipient);

		if (BotRef)
		{
			BotReceiveCommanderOrder(BotRef, ORDERTYPET_BUILD, (AvHUser3)StructureToBuild->v.iuser3, StructureToBuild->v.origin);
		}
	}


	CommanderBot->next_commander_action_time = gpGlobals->time + 0.5f;


	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] == Recipient)
		{
			float dist = UTIL_GetPathCostBetweenLocations(MARINE_REGULAR_NAV_PROFILE, Recipient->v.origin, StructureToBuild->v.origin);
			CommanderBot->LastPlayerOrders[i].bIsActive = true;
			CommanderBot->LastPlayerOrders[i].OrderType = ORDERTYPET_BUILD;
			CommanderBot->LastPlayerOrders[i].MoveLocation = ZERO_VECTOR;
			CommanderBot->LastPlayerOrders[i].Target = StructureToBuild;
			CommanderBot->LastPlayerOrders[i].LastReminderTime = gpGlobals->time;
			CommanderBot->LastPlayerOrders[i].LastPlayerDistance = dist;
			return;
		}
	}
}

void CommanderReceiveAlert(bot_t* pBot, const Vector Location, const PlayerAlertType AlertType)
{
	switch (AlertType)
	{
	case HUD_SOUND_MARINE_NEEDS_AMMO:
	{
		int RequestorIndex = UTIL_FindClosestMarinePlayerToLocation(pBot->pEdict, Location, UTIL_MetresToGoldSrcUnits(3.0f));
		if (RequestorIndex > -1)
		{
			CommanderReceiveAmmoRequest(pBot, clients[RequestorIndex]);
		}
	}
	break;
	case HUD_SOUND_MARINE_NEEDS_HEALTH:
	{
		int RequestorIndex = UTIL_FindClosestMarinePlayerToLocation(pBot->pEdict, Location, UTIL_MetresToGoldSrcUnits(3.0f));
		if (RequestorIndex > -1)
		{
			CommanderReceiveHealthRequest(pBot, clients[RequestorIndex]);
		}
	}
	break;
	case HUD_SOUND_MARINE_NEEDS_ORDER:
	{
		int RequestorIndex = UTIL_FindClosestMarinePlayerToLocation(pBot->pEdict, Location, UTIL_MetresToGoldSrcUnits(1.0f));
		if (RequestorIndex > -1)
		{
			CommanderReceiveOrderRequest(pBot, clients[RequestorIndex]);
		}
	}
	break;
	case HUD_SOUND_MARINE_BASE_UNDER_ATTACK:
		CommanderReceiveBaseAttackAlert(pBot, Location);
		break;
	default:
		return;
	}
}

void CommanderReceiveHealthRequest(bot_t* pBot, edict_t* Requestor)
{

	if (Requestor->v.health < 100.0f)
	{
		for (int i = 0; i < 2; i++)
		{
			CommanderQueueItemDrop(pBot, ITEM_MARINE_HEALTHPACK, ZERO_VECTOR, Requestor, 0);
		}

	}
}

void CommanderReceiveAmmoRequest(bot_t* pBot, edict_t* Requestor)
{
	if (!Requestor) { return; }

	if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_ANYARMOURY, Requestor->v.origin, UTIL_MetresToGoldSrcUnits(15.0f)))
	{
		char buf[64];
		sprintf(buf, "Can you use the armoury please, %s?", STRING(Requestor->v.netname));
		BotTeamSay(pBot, 2.0f, buf);
		return;
	}

	if (UTIL_GetItemCountOfTypeInArea(ITEM_MARINE_AMMO, Requestor->v.origin, UTIL_MetresToGoldSrcUnits(10.0f)) > 0)
	{
		char buf[64];
		sprintf(buf, "I've already dropped ammo there, %s", STRING(Requestor->v.netname));
		BotTeamSay(pBot, 2.0f, buf);
		return;
	}

	for (int i = 0; i < 3; i++)
	{
		CommanderQueueItemDrop(pBot, ITEM_MARINE_AMMO, ZERO_VECTOR, Requestor, 0);
	}
}

void CommanderReceiveOrderRequest(bot_t* pBot, edict_t* Requestor)
{
	int PlayerIndex = GetPlayerIndex(Requestor);
	CommanderQueueActionOrder(pBot, 0, PlayerIndex);
}

void CommanderReceiveBaseAttackAlert(bot_t* pBot, const Vector Location)
{

	int MarineIndex = UTIL_FindClosestAvailableMarinePlayer(pBot, Location);
	edict_t* AttackedStructure = UTIL_GetClosestStructureAtLocation(Location, true);

	if (!AttackedStructure) { return; }

	if (MarineIndex > -1)
	{
		if (vDist2DSq(clients[MarineIndex]->v.origin, AttackedStructure->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { return; }

		if (IsPlayerBot(clients[MarineIndex]))
		{
			bot_t* DefenderBot = GetBotPointer(clients[MarineIndex]);

			if (DefenderBot && (DefenderBot->SecondaryBotTask.TaskType == TASK_NONE || !DefenderBot->SecondaryBotTask.bOrderIsUrgent))
			{
				DefenderBot->SecondaryBotTask.TaskType = TASK_DEFEND;
				DefenderBot->SecondaryBotTask.bOrderIsUrgent = true;
				DefenderBot->SecondaryBotTask.TaskTarget = AttackedStructure;
				DefenderBot->SecondaryBotTask.TaskLocation = AttackedStructure->v.origin;
			}
		}
	}
}

void UpdateCommanderOrders(bot_t* Commander)
{
	if (gpGlobals->time < Commander->next_commander_action_time) { return; }

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (Commander->LastPlayerOrders[i].bIsActive)
		{
			if (FNullEnt(clients[i]) || !UTIL_IsMarineOrderValid(Commander, i))
			{
				UTIL_ClearCommanderOrder(Commander, i);
			}
			else
			{
				if (UTIL_IsMarineOrderComplete(Commander, i))
				{
					bool bMessageSent = UTIL_ConfirmMarineOrderComplete(Commander, i);

					// Only send one message per frame
					if (bMessageSent)
					{
						return;
					}
				}
			}
		}
	}
}

void UpdateCommanderActions(bot_t* Commander)
{
	LinkCommanderActionsToDroppedItems(Commander);

	if (gpGlobals->time < Commander->next_commander_action_time) { return; }

	// Loop through all actions, starting with highest priority (0 = highest, 4 = lowest), stop when there's an action we can progress
	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			commander_action* action = &Commander->CurrentCommanderActions[Priority][ActionIndex];

			if (!action->bIsActive) { continue; }

			bool bPerformedAction = CommanderProgressAction(Commander, ActionIndex, Priority);

			if (bPerformedAction)
			{
				return;
			}
		}
	}
}

int UTIL_CommanderFirstFreeActionIndex(bot_t* CommanderBot, int Priority)
{
	for (int i = 0; i < MAX_PRIORITY_ACTIONS; i++)
	{
		if (!CommanderBot->CurrentCommanderActions[Priority][i].bIsActive)
		{
			return i;
		}
	}

	return -1;
}

void CommanderQueueActionOrder(bot_t* pBot, int Priority, int AssignedPlayer)
{
	int NextActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

	if (NextActionIndex > -1)
	{
		commander_action* action = &pBot->CurrentCommanderActions[Priority][NextActionIndex];

		UTIL_ClearCommanderAction(pBot, NextActionIndex, Priority);

		action->bIsActive = true;
		action->ActionType = ACTION_GIVEORDER;
		action->AssignedPlayer = AssignedPlayer;
	}
}

void CommanderQueueItemDrop(bot_t* pBot, NSDeployableItem ItemToDeploy, const Vector DropLocation, edict_t* Recipient, int Priority)
{
	if (!UTIL_ItemCanBeDeployed(ItemToDeploy)) { return; }

	int NextActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

	if (NextActionIndex > -1)
	{
		commander_action* action = &pBot->CurrentCommanderActions[Priority][NextActionIndex];

		UTIL_ClearCommanderAction(pBot, NextActionIndex, Priority);

		action->bIsActive = true;
		action->ActionType = ACTION_DROPITEM;
		action->ItemToDeploy = ItemToDeploy;
		action->ActionTarget = Recipient;

		if (Recipient)
		{
			action->BuildLocation = ZERO_VECTOR;
		}
		else
		{
			action->BuildLocation = DropLocation;
		}

	}
}

void CommanderQueueInfantryPortalBuild(bot_t* pBot, int Priority)
{
	edict_t* ExistingInfantryPortal = UTIL_GetFirstPlacedStructureOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

	Vector BuildLocation = ZERO_VECTOR;

	// First see if we can place the next infantry portal next to the first one
	if (!FNullEnt(ExistingInfantryPortal))
	{
		BuildLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, ExistingInfantryPortal->v.origin, UTIL_MetresToGoldSrcUnits(1.0f), UTIL_MetresToGoldSrcUnits(2.0f));
	}

	// If not then find somewhere near the comm chair
	if (BuildLocation == ZERO_VECTOR)
	{
		Vector CommChairLocation = UTIL_GetCommChairLocation();

		if (!vEquals(CommChairLocation, ZERO_VECTOR))
		{
			Vector SearchPoint = ZERO_VECTOR;

			const resource_node* ResNode = UTIL_FindNearestResNodeToLocation(CommChairLocation);

			if (ResNode)
			{
				SearchPoint = ResNode->origin;
			}
			else
			{
				SearchPoint = UTIL_GetRandomPointOfInterest();
			}

			Vector NearestPointToChair = FindClosestNavigablePointToDestination(BUILDING_REGULAR_NAV_PROFILE, SearchPoint, CommChairLocation, 100.0f);

			if (!vEquals(NearestPointToChair, ZERO_VECTOR))
			{
				BuildLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, NearestPointToChair, UTIL_MetresToGoldSrcUnits(2.0f), UTIL_MetresToGoldSrcUnits(5.0f));

			}
			else
			{
				BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, CommChairLocation, UTIL_MetresToGoldSrcUnits(5.0f));
			}
		}

	}

	if (!vEquals(BuildLocation, ZERO_VECTOR))
	{
		int NewActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

		if (NewActionIndex > -1)
		{
			UTIL_ClearCommanderAction(pBot, NewActionIndex, Priority);

			commander_action* action = &pBot->CurrentCommanderActions[Priority][NewActionIndex];

			action->bIsActive = true;
			action->ActionType = ACTION_BUILD;
			action->BuildLocation = BuildLocation;
			action->StructureToBuild = STRUCTURE_MARINE_INFANTRYPORTAL;
		}
	}
}

void CommanderQueueArmouryBuild(bot_t* pBot, int Priority)
{
	edict_t* InfantryPortal = UTIL_GetFirstPlacedStructureOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

	if (FNullEnt(InfantryPortal)) { return; }

	Vector NewArmouryLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, InfantryPortal->v.origin, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));

	if (!vEquals(NewArmouryLocation, ZERO_VECTOR))
	{
		int NewActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

		if (NewActionIndex > -1)
		{
			UTIL_ClearCommanderAction(pBot, NewActionIndex, Priority);

			commander_action* action = &pBot->CurrentCommanderActions[Priority][NewActionIndex];

			action->bIsActive = true;
			action->ActionType = ACTION_BUILD;
			action->BuildLocation = NewArmouryLocation;
			action->StructureToBuild = STRUCTURE_MARINE_ARMOURY;
		}
	}
}

void CommanderQueueArmsLabBuild(bot_t* pBot, int Priority)
{
	edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(20.0f), true, false);

	if (FNullEnt(Armoury)) { return; }

	Vector NewArmsLabLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));

	if (NewArmsLabLocation != ZERO_VECTOR)
	{
		int NewActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

		if (NewActionIndex > -1)
		{
			UTIL_ClearCommanderAction(pBot, NewActionIndex, Priority);

			commander_action* action = &pBot->CurrentCommanderActions[Priority][NewActionIndex];

			action->bIsActive = true;
			action->ActionType = ACTION_BUILD;
			action->BuildLocation = NewArmsLabLocation;
			action->StructureToBuild = STRUCTURE_MARINE_ARMSLAB;
		}
	}
}

void CommanderQueueResTowerBuild(bot_t* pBot, int Priority)
{
	int NextEmptyActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

	if (NextEmptyActionIndex == -1) { return; }

	int chosenResNode = -1;
	float nearestDist = 0.0f;

	for (int i = 0; i < NumTotalResNodes; i++)
	{
		if (!ResourceNodes[i].bIsOccupied && !UTIL_ActionExistsInLocation(pBot, ResourceNodes[i].origin))
		{
			float dist = UTIL_DistToNearestFriendlyPlayer(ResourceNodes[i].origin, MARINE_TEAM);

			if (chosenResNode == -1 || dist < nearestDist)
			{
				chosenResNode = i;
				nearestDist = dist;
			}
		}
	}

	if (chosenResNode > -1)
	{
		commander_action* action = &pBot->CurrentCommanderActions[Priority][NextEmptyActionIndex];

		UTIL_ClearCommanderAction(pBot, NextEmptyActionIndex, Priority);
		action->bIsActive = true;
		action->ActionType = ACTION_BUILD;
		action->BuildLocation = ResourceNodes[chosenResNode].origin;
		action->StructureToBuild = STRUCTURE_MARINE_RESTOWER;
	}
}

void CommanderQueueUpgrade(bot_t* pBot, edict_t* BuildingToUpgrade, int Priority)
{
	NSStructureType StructureType = GetStructureTypeFromEdict(BuildingToUpgrade);

	if (StructureType != STRUCTURE_NONE)
	{
		int NewActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

		if (NewActionIndex > -1)
		{
			commander_action* action = &pBot->CurrentCommanderActions[Priority][NewActionIndex];


			action->bIsActive = true;
			action->ActionType = ACTION_UPGRADE;
			action->StructureOrItem = BuildingToUpgrade;
			action->StructureToBuild = StructureType;
		}
	}
}

void CommanderQueueResearch(bot_t* pBot, NSResearch Research, int Priority)
{
	switch (Research)
	{
	case RESEARCH_ARMSLAB_ARMOUR1:
	case RESEARCH_ARMSLAB_ARMOUR2:
	case RESEARCH_ARMSLAB_ARMOUR3:
	case RESEARCH_ARMSLAB_WEAPONS1:
	case RESEARCH_ARMSLAB_WEAPONS2:
	case RESEARCH_ARMSLAB_WEAPONS3:
	case RESEARCH_ARMSLAB_CATALYSTS:
		CommanderQueueArmsLabResearch(pBot, Research, Priority);
		break;
	case RESEARCH_PROTOTYPELAB_HEAVYARMOUR:
	case RESEARCH_PROTOTYPELAB_JETPACKS:
		CommanderQueuePrototypeLabResearch(pBot, Research, Priority);
		break;
	case RESEARCH_OBSERVATORY_DISTRESSBEACON:
	case RESEARCH_OBSERVATORY_MOTIONTRACKING:
	case RESEARCH_OBSERVATORY_PHASETECH:
		CommanderQueueObservatoryResearch(pBot, Research, Priority);
	case RESEARCH_ARMOURY_GRENADES:
		CommanderQueueArmouryResearch(pBot, Research, Priority);
	default:
		break;
	}
}

void CommanderQueueArmsLabResearch(bot_t* pBot, NSResearch Research, int Priority)
{
	if (UTIL_MarineResearchIsAvailable(Research))
	{
		edict_t* ArmsLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMSLAB);

		int ActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

		if (ActionIndex > -1 && ArmsLab)
		{
			UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);

			commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];

			action->bIsActive = true;
			action->ActionType = ACTION_RESEARCH;
			action->StructureOrItem = ArmsLab;
			action->ResearchId = Research;

		}
	}
}

void CommanderQueueElectricResearch(bot_t* pBot, int Priority, edict_t* StructureToElectrify)
{
	if (FNullEnt(StructureToElectrify)) { return; }

	NSStructureType StructureTypeToElectrify = GetStructureTypeFromEdict(StructureToElectrify);

	if (!UTIL_StructureTypesMatch(StructureTypeToElectrify, STRUCTURE_MARINE_ANYTURRETFACTORY) && !UTIL_StructureTypesMatch(StructureTypeToElectrify, STRUCTURE_MARINE_RESTOWER)) { return; }

	int ActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

	if (ActionIndex > -1)
	{
		UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);

		commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];

		action->bIsActive = true;
		action->ActionType = ACTION_RESEARCH;
		action->StructureOrItem = StructureToElectrify;
		action->ResearchId = RESEARCH_ELECTRICAL;

	}
}

void CommanderQueueArmouryResearch(bot_t* pBot, NSResearch Research, int Priority)
{
	if (UTIL_MarineResearchIsAvailable(Research))
	{
		edict_t* Armoury = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMOURY);

		if (!Armoury)
		{
			Armoury = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ADVARMOURY);
		}

		int ActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

		if (ActionIndex > -1 && Armoury)
		{
			UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);

			commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];

			action->bIsActive = true;
			action->ActionType = ACTION_RESEARCH;
			action->StructureOrItem = Armoury;
			action->ResearchId = Research;

		}
	}
}

void CommanderQueuePrototypeLabResearch(bot_t* pBot, NSResearch Research, int Priority)
{
	if (UTIL_MarineResearchIsAvailable(Research))
	{
		edict_t* PrototypeLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_PROTOTYPELAB);

		int ActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

		if (ActionIndex > -1 && PrototypeLab)
		{
			UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);

			commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];

			action->bIsActive = true;
			action->ActionType = ACTION_RESEARCH;
			action->StructureOrItem = PrototypeLab;
			action->ResearchId = Research;

		}
	}
}

void CommanderQueueObservatoryResearch(bot_t* pBot, NSResearch Research, int Priority)
{
	if (UTIL_MarineResearchIsAvailable(Research))
	{
		edict_t* Observatory = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_OBSERVATORY);

		int ActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

		if (ActionIndex > -1 && Observatory)
		{
			UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);

			commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];

			action->bIsActive = true;
			action->ActionType = ACTION_RESEARCH;
			action->StructureOrItem = Observatory;
			action->ResearchId = Research;

		}
	}
}

void CommanderThink(bot_t* pBot)
{

	if (!bGameIsActive)
	{
		return;
	}

	UTIL_OrganiseCommanderActions(pBot);

	UpdateCommanderOrders(pBot);

	CommanderQueueNextAction(pBot);

	UpdateCommanderActions(pBot);

}

bool UTIL_IsMarineOrderValid(bot_t* CommanderBot, int CommanderOrderIndex)
{
	if (FNullEnt(clients[CommanderOrderIndex]) || !CommanderBot->LastPlayerOrders[CommanderOrderIndex].bIsActive) { return false; }

	switch (CommanderBot->LastPlayerOrders[CommanderOrderIndex].OrderType)
	{
	case ORDERTYPEL_MOVE:
		return CommanderBot->LastPlayerOrders[CommanderOrderIndex].MoveLocation != ZERO_VECTOR;
	case ORDERTYPET_BUILD:
		return !FNullEnt(CommanderBot->LastPlayerOrders[CommanderOrderIndex].Target) && CommanderBot->LastPlayerOrders[CommanderOrderIndex].Target->v.deadflag == DEAD_NO;
	default:
		return false;
	}

	return false;
}

bool UTIL_IsCommanderActionActionable(bot_t* CommanderBot, int CommanderActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][CommanderActionIndex];

	if (!action->bIsActive || action->ActionType == ACTION_NONE) { return false; }

	switch (action->ActionType)
	{
	case ACTION_BUILD:
	case ACTION_DROPITEM:
		return FNullEnt(action->StructureOrItem);
	case ACTION_RECYCLE:
		return !FNullEnt(action->StructureOrItem) && !UTIL_StructureIsRecycling(action->StructureOrItem) && (action->StructureOrItem->v.deadflag != DEAD_NO);
	case ACTION_RESEARCH:
		return !FNullEnt(action->StructureOrItem) && !UTIL_StructureIsRecycling(action->StructureOrItem) && (action->StructureOrItem->v.deadflag != DEAD_NO) && UTIL_MarineResearchIsAvailable(action->ResearchId);
	case ACTION_UPGRADE:
		return !FNullEnt(action->StructureOrItem) && UTIL_StructureCanBeUpgraded(action->StructureOrItem);
	case ACTION_GIVEORDER:
		return action->AssignedPlayer > -1 && (gpGlobals->time - CommanderBot->LastPlayerOrders[action->AssignedPlayer].LastReminderTime) < min_order_reminder_time;
	default:
		return false;
	}

	return false;
}

bool UTIL_IsCommanderActionValid(bot_t* CommanderBot, int CommanderActionIndex, int Priority)
{
	if (!CommanderBot->CurrentCommanderActions[Priority][CommanderActionIndex].bIsActive || CommanderBot->CurrentCommanderActions[Priority][CommanderActionIndex].ActionType == ACTION_NONE)
	{
		return false;
	}

	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][CommanderActionIndex];

	if (action->NumActionAttempts > 10) { return false; }

	switch (action->ActionType)
	{
	case ACTION_RECYCLE:
		return !FNullEnt(action->StructureOrItem) && UTIL_IsMarineStructure(action->StructureOrItem) && !UTIL_StructureIsRecycling(action->StructureOrItem);
	case ACTION_UPGRADE:
		return !FNullEnt(action->StructureOrItem) && UTIL_StructureCanBeUpgraded(action->StructureOrItem);
	case ACTION_BUILD:
		return UTIL_CommanderBuildActionIsValid(CommanderBot, action);
	case ACTION_RESEARCH:
	{
		if (action->ResearchId == RESEARCH_ELECTRICAL)
		{
			return UTIL_ElectricalResearchIsAvailable(action->StructureOrItem);
		}
		return (UTIL_MarineResearchIsAvailable(action->ResearchId) && !FNullEnt(action->StructureOrItem));
	}
	case ACTION_DROPITEM:
		return FNullEnt(action->StructureOrItem) && UTIL_ItemCanBeDeployed(action->ItemToDeploy) && ((action->BuildLocation != ZERO_VECTOR) || !FNullEnt(action->ActionTarget));
	case ACTION_GIVEORDER:
		return action->AssignedPlayer > -1;
	default:
		return false;
	}

	return false;
}

bool UTIL_StructureIsScheduledForRecycle(bot_t* CommanderBot, edict_t* Structure)
{
	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

			if (action->bIsActive && action->ActionType == ACTION_RECYCLE && action->StructureOrItem == Structure)
			{
				return true;
			}
		}
	}
	return false;
}

bool UTIL_CommanderBuildActionIsValid(bot_t* CommanderBot, commander_action* Action)
{
	if (!Action || Action->StructureToBuild == STRUCTURE_NONE) { return false; }

	if (Action->bHasAttemptedAction && (gpGlobals->time - Action->StructureBuildAttemptTime < build_attempt_retry_time)) { return true; }

	if (Action->NumActionAttempts >= 3) { return false; }

	if (!FNullEnt(Action->StructureOrItem))
	{
		if (Action->StructureOrItem->v.deadflag == DEAD_DEAD || UTIL_StructureIsFullyBuilt(Action->StructureOrItem) || !UTIL_PointIsOnNavmesh(UTIL_GetEntityGroundLocation(Action->StructureOrItem), MARINE_REGULAR_NAV_PROFILE))
		{
			return false;
		}
	}

	// Give up building a resource tower if we have a few already and there's nobody nearby
	if (Action->StructureToBuild == STRUCTURE_MARINE_RESTOWER && FNullEnt(Action->StructureOrItem))
	{
		const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Action->BuildLocation);

		if (ResNodeIndex)
		{
			if (ResNodeIndex->bIsOccupied && ResNodeIndex->bIsOwnedByMarines && UTIL_StructureIsFullyBuilt(ResNodeIndex->edict))
			{
				return false;
			}
		}

		int NumExistingResTowers = UTIL_GetNumPlacedStructuresOfType(STRUCTURE_MARINE_RESTOWER);

		if (NumExistingResTowers >= 3)
		{
			if (!UTIL_AnyMarinePlayerNearLocation(Action->BuildLocation, UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				return false;
			}
		}
	}

	switch (Action->StructureToBuild)
	{
	case STRUCTURE_MARINE_ARMOURY:
	case STRUCTURE_MARINE_ADVARMOURY:
		return (UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, Action->BuildLocation, UTIL_MetresToGoldSrcUnits(10.0f), true, false) == nullptr);
	case STRUCTURE_MARINE_TURRETFACTORY:
	case STRUCTURE_MARINE_ADVTURRETFACTORY:
		return (UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, Action->BuildLocation, UTIL_MetresToGoldSrcUnits(10.0f), true, false) == nullptr);
	case STRUCTURE_MARINE_PHASEGATE:
		return (UTIL_StructureExistsOfType(STRUCTURE_MARINE_OBSERVATORY) && UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, Action->BuildLocation, UTIL_MetresToGoldSrcUnits(10.0f), true, false) == nullptr);
	case STRUCTURE_MARINE_OBSERVATORY:
	case STRUCTURE_MARINE_ARMSLAB:
	case STRUCTURE_MARINE_PROTOTYPELAB:
		return !UTIL_StructureExistsOfType(Action->StructureToBuild);
	case STRUCTURE_MARINE_TURRET:
		return (UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, Action->BuildLocation, UTIL_MetresToGoldSrcUnits(5.0f), true, false) != nullptr);
	case STRUCTURE_MARINE_SIEGETURRET:
		return (UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ADVTURRETFACTORY, Action->BuildLocation, UTIL_MetresToGoldSrcUnits(5.0f), true, false) != nullptr);
	default:
		return true;
	}

	return true;
}

bool UTIL_ActionHasValidPlayerAssigned(bot_t* CommanderBot, int CommanderActionIndex, int Priority)
{
	int AssignedPlayerIndex = CommanderBot->CurrentCommanderActions[Priority][CommanderActionIndex].AssignedPlayer;

	if (AssignedPlayerIndex < 0 || AssignedPlayerIndex > 31) { return false; }

	return (!FNullEnt(clients[AssignedPlayerIndex]) && IsPlayerOnMarineTeam(clients[AssignedPlayerIndex]) && IsPlayerActiveInGame(clients[AssignedPlayerIndex]));
}

int UTIL_GetNumPlacedOrQueuedStructuresOfType(bot_t* CommanderBot, NSStructureType StructureType)
{
	return (UTIL_GetQueuedBuildRequestsOfType(CommanderBot, StructureType) + UTIL_GetStructureCountOfType(StructureType));
}


void UTIL_CommanderQueueStructureBuildAtLocation(bot_t* pBot, const Vector Location, NSStructureType StructureType, int Priority)
{
	int NewActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

	if (NewActionIndex > -1)
	{
		commander_action* action = &pBot->CurrentCommanderActions[Priority][NewActionIndex];

		UTIL_ClearCommanderAction(pBot, NewActionIndex, Priority);
		action->bIsActive = true;
		action->ActionType = ACTION_BUILD;
		action->BuildLocation = Location;
		action->StructureToBuild = StructureType;

	}
}

bool UTIL_HasIdleArmsLab()
{
	for (auto& it : MarineBuildableStructureMap)
	{
		if (!it.second.bOnNavmesh) { continue; }
		if (!UTIL_StructureTypesMatch(STRUCTURE_MARINE_ARMSLAB, it.second.StructureType)) { continue; }

		if (UTIL_StructureIsFullyBuilt(it.second.edict) && !UTIL_StructureIsResearching(it.second.edict)) { return true; }

	}

	return false;
}

edict_t* UTIL_GetFirstIdleArmsLab()
{
	for (auto& it : MarineBuildableStructureMap)
	{
		if (!it.second.bOnNavmesh) { continue; }
		if (!UTIL_StructureTypesMatch(STRUCTURE_MARINE_ARMSLAB, it.second.StructureType)) { continue; }

		if (UTIL_StructureIsFullyBuilt(it.second.edict) && !UTIL_StructureIsResearching(it.second.edict)) { return it.second.edict; }

	}

	return NULL;
}

bool UTIL_ResearchIsAlreadyQueued(bot_t* pBot, NSResearch Research)
{
	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];

			if (action->bIsActive && action->ActionType == ACTION_RESEARCH && action->ResearchId == Research)
			{
				return true;
			}
		}
	}
	return false;
}

bool UTIL_ResearchInProgress(NSResearch Research)
{
	switch (Research)
	{
	case RESEARCH_ARMSLAB_ARMOUR1:
	case RESEARCH_ARMSLAB_ARMOUR2:
	case RESEARCH_ARMSLAB_ARMOUR3:
	case RESEARCH_ARMSLAB_WEAPONS1:
	case RESEARCH_ARMSLAB_WEAPONS2:
	case RESEARCH_ARMSLAB_WEAPONS3:
	case RESEARCH_ARMSLAB_CATALYSTS:
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(STRUCTURE_MARINE_ARMSLAB, it.second.StructureType)) { continue; }

			if (UTIL_StructureIsFullyBuilt(it.second.edict) && UTIL_StructureIsResearching(it.second.edict) && it.second.edict->v.iuser2 == Research) { return true; }

		}

		return false;
	}
	break;

	case RESEARCH_PROTOTYPELAB_HEAVYARMOUR:
	case RESEARCH_PROTOTYPELAB_JETPACKS:
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(STRUCTURE_MARINE_PROTOTYPELAB, it.second.StructureType)) { continue; }

			if (UTIL_StructureIsFullyBuilt(it.second.edict) && UTIL_StructureIsResearching(it.second.edict) && it.second.edict->v.iuser2 == Research) { return true; }

		}

		return false;
	}
	break;

	case RESEARCH_OBSERVATORY_DISTRESSBEACON:
	case RESEARCH_OBSERVATORY_MOTIONTRACKING:
	case RESEARCH_OBSERVATORY_PHASETECH:
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(STRUCTURE_MARINE_OBSERVATORY, it.second.StructureType)) { continue; }

			if (UTIL_StructureIsFullyBuilt(it.second.edict) && UTIL_StructureIsResearching(it.second.edict) && it.second.edict->v.iuser2 == Research) { return true; }

		}

		return false;
	}

	break;

	default:
		return false;

	}

	return false;
}



void LinkCommanderActionsToDroppedItems(bot_t* Commander)
{
	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			commander_action* action = &Commander->CurrentCommanderActions[Priority][ActionIndex];

			if (action->bIsActive && action->ActionType == ACTION_DROPITEM && action->StructureOrItem == nullptr)
			{

				for (int ItemIndex = 0; ItemIndex < NumTotalMarineItems; ItemIndex++)
				{
					if (AllMarineItems[ItemIndex].ItemType == action->ItemToDeploy)
					{
						if (UTIL_ItemIsAlreadyLinked(Commander, AllMarineItems[ItemIndex].edict))
						{
							continue;
						}

						if (vDist2DSq(AllMarineItems[ItemIndex].Location, action->BuildLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
						{
							continue;
						}

						action->StructureOrItem = AllMarineItems[ItemIndex].edict;
						UTIL_LinkItem(Commander, AllMarineItems[ItemIndex].edict);

						if (action->ItemToDeploy == ITEM_MARINE_SCAN)
						{
							Commander->CommanderLastScanTime = gpGlobals->time;
						}

						break;
					}
				}
			}
		}
	}
}

bool UTIL_ItemIsAlreadyLinked(bot_t* Commander, edict_t* Item)
{
	for (int i = 0; i < Commander->NumActionLinkedItems; i++)
	{
		if (Commander->ActionLinkedItems[i] == Item)
		{
			return true;
		}
	}

	return false;
}

void UTIL_LinkItem(bot_t* Commander, edict_t* Item)
{
	Commander->ActionLinkedItems[Commander->NumActionLinkedItems++] = Item;
}

bool UTIL_CancelCommanderPlayerOrder(bot_t* Commander, int PlayerIndex)
{
	UTIL_ClearCommanderOrder(Commander, PlayerIndex);
	return false;
}


bool UTIL_ActionExistsInLocation(const bot_t* Commander, const Vector CheckPoint)
{
	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			if (Commander->CurrentCommanderActions[Priority][ActionIndex].bIsActive && vEquals(Commander->CurrentCommanderActions[Priority][ActionIndex].BuildLocation, CheckPoint))
			{
				return true;
			}
		}
	}
	return false;
}

void UTIL_ClearCommanderAction(bot_t* Commander, commander_action* Action)
{
	memset(&Action, 0, sizeof(commander_action));
	Action->AssignedPlayer = -1;
}

void UTIL_ClearCommanderAction(bot_t* Commander, int ActionIndex, int Priority)
{
	Commander->CurrentCommanderActions[Priority][ActionIndex].bIsActive = false;
	Commander->CurrentCommanderActions[Priority][ActionIndex].ActionType = ACTION_NONE;
	Commander->CurrentCommanderActions[Priority][ActionIndex].ActionStep = ACTION_STEP_NONE;
	Commander->CurrentCommanderActions[Priority][ActionIndex].StructureToBuild = STRUCTURE_NONE;
	Commander->CurrentCommanderActions[Priority][ActionIndex].BuildLocation = ZERO_VECTOR;
	Commander->CurrentCommanderActions[Priority][ActionIndex].DesiredCommanderLocation = ZERO_VECTOR;
	Commander->CurrentCommanderActions[Priority][ActionIndex].LastAttemptedCommanderLocation = ZERO_VECTOR;
	Commander->CurrentCommanderActions[Priority][ActionIndex].LastAttemptedCommanderAngle = ZERO_VECTOR;
	Commander->CurrentCommanderActions[Priority][ActionIndex].AssignedPlayer = -1;
	Commander->CurrentCommanderActions[Priority][ActionIndex].StructureOrItem = nullptr;
	Commander->CurrentCommanderActions[Priority][ActionIndex].bHasAttemptedAction = false;
	Commander->CurrentCommanderActions[Priority][ActionIndex].StructureBuildAttemptTime = 0.0f;
	Commander->CurrentCommanderActions[Priority][ActionIndex].NumActionAttempts = 0;
	Commander->CurrentCommanderActions[Priority][ActionIndex].ResearchId = RESEARCH_NONE;
	Commander->CurrentCommanderActions[Priority][ActionIndex].ItemToDeploy = ITEM_NONE;

}

void UTIL_OrganiseCommanderActions(bot_t* pBot)
{
	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		int CurrIndex = 0;
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			if (!UTIL_IsCommanderActionValid(pBot, ActionIndex, Priority))
			{
				UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
			}
			else
			{
				if (ActionIndex != CurrIndex)
				{
					memcpy(&pBot->CurrentCommanderActions[Priority][CurrIndex], &pBot->CurrentCommanderActions[Priority][ActionIndex], sizeof(commander_action));
					UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
				}

				CurrIndex++;
			}
		}
	}
}

void UTIL_ClearCommanderOrder(bot_t* Commander, int OrderIndex)
{
	Commander->LastPlayerOrders[OrderIndex].bIsActive = false;
	Commander->LastPlayerOrders[OrderIndex].OrderType = ORDERTYPE_UNDEFINED;
	Commander->LastPlayerOrders[OrderIndex].MoveLocation = ZERO_VECTOR;
	Commander->LastPlayerOrders[OrderIndex].Target = nullptr;
	Commander->LastPlayerOrders[OrderIndex].LastReminderTime = 0.0f;

}

bool UTIL_StructureCanBeUpgraded(const edict_t* Structure)
{
	// We can't upgrade a structure if it's not built, destroyed, or already doing something
	if (FNullEnt(Structure)
		|| Structure->v.deadflag != DEAD_NO
		|| !UTIL_StructureIsFullyBuilt(Structure)
		|| UTIL_StructureIsRecycling(Structure)
		|| UTIL_StructureIsResearching(Structure)
		|| UTIL_StructureIsUpgrading(Structure))
	{
		return false;
	}

	NSStructureType StructureType = GetStructureTypeFromEdict(Structure);

	// Only armouries and turret factories can be upgraded
	switch (StructureType)
	{
	case STRUCTURE_MARINE_ARMOURY:
	case STRUCTURE_MARINE_TURRETFACTORY:
		return true;
	default:
		return false;
	}

	return false;
}

bool BotCommanderUpgradeStructure(bot_t* pBot, int ActionIndex, int Priority)
{

	if (gpGlobals->time < pBot->next_commander_action_time || !UTIL_IsCommanderActionValid(pBot, ActionIndex, Priority))
	{
		UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
		return false;
	}

	commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];
	buildable_structure* BuildRef = UTIL_GetBuildableStructureRefFromEdict(action->StructureOrItem);

	if (action->NumActionAttempts > 10)
	{
		UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
		return false;
	}

	// If we are already selecting the building then we can just send the impulse and finish up
	if (pBot->CommanderCurrentlySelectedBuilding == action->StructureOrItem)
	{
		if (BuildRef)
		{
			if (action->LastAttemptedCommanderLocation != ZERO_VECTOR)
			{
				BuildRef->LastSuccessfulCommanderLocation = action->LastAttemptedCommanderLocation;
			}

			if (action->LastAttemptedCommanderAngle != ZERO_VECTOR)
			{
				BuildRef->LastSuccessfulCommanderAngle = action->LastAttemptedCommanderAngle;
			}
		}


		NSStructureType StructureType = UTIL_IUSER3ToStructureType(action->StructureOrItem->v.iuser3);

		switch (StructureType)
		{
		case STRUCTURE_MARINE_ARMOURY:
			pBot->impulse = IMPULSE_COMMANDER_UPGRADE_ARMOURY;
			break;
		case STRUCTURE_MARINE_TURRETFACTORY:
			pBot->impulse = IMPULSE_COMMANDER_UPGRADE_TURRETFACTORY;
			break;
		}

		action->ActionStep = ACTION_STEP_NONE;

		pBot->next_commander_action_time = gpGlobals->time + commander_action_cooldown;
		return true;
	}

	return BotCommanderSelectStructure(pBot, action->StructureOrItem, ActionIndex, Priority);

}

bool BotCommanderSelectStructure(bot_t* pBot, const edict_t* Structure, int ActionIndex, int Priority)
{

	commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];
	buildable_structure* BuildRef = UTIL_GetBuildableStructureRefFromEdict(action->StructureOrItem);

	if (pBot->CommanderCurrentlySelectedBuilding == Structure)
	{
		return false;
	}

	Vector CommanderViewOrigin = pBot->pEdict->v.origin;
	CommanderViewOrigin.z = GetCommanderViewZHeight();

	if (action->DesiredCommanderLocation == ZERO_VECTOR || vDist2DSq(action->DesiredCommanderLocation, Structure->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{

		action->DesiredCommanderLocation = UTIL_FindClearCommanderOriginForBuild(pBot, UTIL_GetCentreOfEntity(Structure), GetCommanderViewZHeight());

		if (action->DesiredCommanderLocation == ZERO_VECTOR)
		{
			action->NumActionAttempts++;
			return false;
		}
	}

	if (action->bHasAttemptedAction)
	{
		if (BuildRef)
		{
			BuildRef->LastSuccessfulCommanderLocation = ZERO_VECTOR;
			BuildRef->LastSuccessfulCommanderAngle = ZERO_VECTOR;
		}
		action->DesiredCommanderLocation = UTIL_RandomPointOnCircle(UTIL_GetCentreOfEntity(Structure), UTIL_MetresToGoldSrcUnits(5.0f));
	}

	if (vDist2DSq(CommanderViewOrigin, action->DesiredCommanderLocation) > sqrf(8.0f))
	{
		action->bHasAttemptedAction = false;
		pBot->UpMove = action->DesiredCommanderLocation.x / kWorldPosNetworkConstant;
		pBot->SideMove = action->DesiredCommanderLocation.y / kWorldPosNetworkConstant;
		pBot->ForwardMove = 0.0f;

		pBot->impulse = IMPULSE_COMMANDER_MOVETO;

		action->ActionStep = ACTION_STEP_NONE;

		pBot->next_commander_action_time = gpGlobals->time + 0.1f;
		return true;
	}

	if (action->ActionStep == ACTION_STEP_NONE)
	{
		action->ActionStep = ACTION_STEP_BEGIN_SELECT;
	}

	if (action->ActionStep == ACTION_STEP_BEGIN_SELECT)
	{

		if (BuildRef && BuildRef->LastSuccessfulCommanderAngle != ZERO_VECTOR)
		{
			pBot->UpMove = BuildRef->LastSuccessfulCommanderAngle.x;
			pBot->SideMove = BuildRef->LastSuccessfulCommanderAngle.y;
			pBot->ForwardMove = BuildRef->LastSuccessfulCommanderAngle.z;
		}
		else
		{
			Vector PickRay = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Structure) - CommanderViewOrigin);

			pBot->UpMove = PickRay.x * kSelectionNetworkConstant;
			pBot->SideMove = PickRay.y * kSelectionNetworkConstant;
			pBot->ForwardMove = PickRay.z * kSelectionNetworkConstant;
		}

		pBot->impulse = IMPULSE_COMMANDER_MOUSECOORD;

		pBot->pEdict->v.button |= IN_ATTACK;

		action->ActionStep = ACTION_STEP_END_SELECT;
		return true;
	}

	if (action->ActionStep == ACTION_STEP_END_SELECT)
	{
		if (BuildRef && BuildRef->LastSuccessfulCommanderAngle != ZERO_VECTOR)
		{
			pBot->UpMove = BuildRef->LastSuccessfulCommanderAngle.x;
			pBot->SideMove = BuildRef->LastSuccessfulCommanderAngle.y;
			pBot->ForwardMove = BuildRef->LastSuccessfulCommanderAngle.z;
		}
		else
		{
			Vector PickRay = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Structure) - CommanderViewOrigin);

			pBot->UpMove = PickRay.x * kSelectionNetworkConstant;
			pBot->SideMove = PickRay.y * kSelectionNetworkConstant;
			pBot->ForwardMove = PickRay.z * kSelectionNetworkConstant;
		}

		action->LastAttemptedCommanderLocation = CommanderViewOrigin;
		action->LastAttemptedCommanderAngle = Vector(pBot->UpMove, pBot->SideMove, pBot->ForwardMove);

		pBot->impulse = IMPULSE_COMMANDER_MOUSECOORD;

		pBot->pEdict->v.button = 0;

		action->bHasAttemptedAction = true;
		action->NumActionAttempts++;
		pBot->next_commander_action_time = gpGlobals->time + 0.2f;

		return true;
	}

	return false;
}

bool BotCommanderResearchTech(bot_t* pBot, int ActionIndex, int Priority)
{
	if (!UTIL_IsCommanderActionValid(pBot, ActionIndex, Priority))
	{
		UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
		return false;
	}

	if (gpGlobals->time < pBot->next_commander_action_time)
	{
		return true;
	}

	commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];
	buildable_structure* BuildRef = UTIL_GetBuildableStructureRefFromEdict(action->StructureOrItem);

	if (action->NumActionAttempts > 10)
	{
		UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
		return false;
	}

	// If we are already selecting the building then we can just send the impulse and finish up
	if (pBot->CommanderCurrentlySelectedBuilding == action->StructureOrItem)
	{
		if (BuildRef)
		{
			if (action->LastAttemptedCommanderLocation != ZERO_VECTOR)
			{

				BuildRef->LastSuccessfulCommanderLocation = action->LastAttemptedCommanderLocation;
			}

			if (action->LastAttemptedCommanderAngle != ZERO_VECTOR)
			{
				BuildRef->LastSuccessfulCommanderAngle = action->LastAttemptedCommanderAngle;
			}
		}

		pBot->impulse = action->ResearchId;

		if (action->ResearchId == RESEARCH_OBSERVATORY_DISTRESSBEACON)
		{
			pBot->CommanderLastBeaconTime = gpGlobals->time;
		}

		action->ActionStep = ACTION_STEP_NONE;

		pBot->next_commander_action_time = gpGlobals->time + commander_action_cooldown;

		return true;
	}

	return BotCommanderSelectStructure(pBot, action->StructureOrItem, ActionIndex, Priority);
}

bool UTIL_MarineResearchIsAvailable(const NSResearch Research)
{
	switch (Research)
	{
	case RESEARCH_ARMSLAB_ARMOUR1:
	case RESEARCH_ARMSLAB_ARMOUR2:
	case RESEARCH_ARMSLAB_ARMOUR3:
	case RESEARCH_ARMSLAB_WEAPONS1:
	case RESEARCH_ARMSLAB_WEAPONS2:
	case RESEARCH_ARMSLAB_WEAPONS3:
	case RESEARCH_ARMSLAB_CATALYSTS:
		return UTIL_ArmsLabResearchIsAvailable(Research);
	case RESEARCH_PROTOTYPELAB_JETPACKS:
	case RESEARCH_PROTOTYPELAB_HEAVYARMOUR:
		return UTIL_PrototypeLabResearchIsAvailable(Research);
	case RESEARCH_OBSERVATORY_DISTRESSBEACON:
	case RESEARCH_OBSERVATORY_MOTIONTRACKING:
	case RESEARCH_OBSERVATORY_PHASETECH:
		return UTIL_ObservatoryResearchIsAvailable(Research);
	case RESEARCH_ARMOURY_GRENADES:
		return UTIL_ArmouryResearchIsAvailable(Research);
	default:
		return false;
	}


	return false;
}

bool UTIL_ArmsLabResearchIsAvailable(const NSResearch Research)
{
	edict_t* ArmsLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMSLAB);

	if (!FNullEnt(ArmsLab))
	{
		switch (Research)
		{
		case RESEARCH_ARMSLAB_ARMOUR1:
			return (ArmsLab->v.iuser4 & MASK_UPGRADE_5);
		case RESEARCH_ARMSLAB_ARMOUR2:
			return (ArmsLab->v.iuser4 & MASK_UPGRADE_6);
		case RESEARCH_ARMSLAB_ARMOUR3:
			return (ArmsLab->v.iuser4 & MASK_UPGRADE_7);
		case RESEARCH_ARMSLAB_WEAPONS1:
			return (ArmsLab->v.iuser4 & MASK_UPGRADE_1);
		case RESEARCH_ARMSLAB_WEAPONS2:
			return (ArmsLab->v.iuser4 & MASK_UPGRADE_2);
		case RESEARCH_ARMSLAB_WEAPONS3:
			return (ArmsLab->v.iuser4 & MASK_UPGRADE_3);
		case RESEARCH_ARMSLAB_CATALYSTS:
			return (ArmsLab->v.iuser4 & MASK_UPGRADE_4);
		default:
			return false;
		}
	}

	return false;
}

bool UTIL_PrototypeLabResearchIsAvailable(const NSResearch Research)
{
	edict_t* PrototypeLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_PROTOTYPELAB);

	if (!FNullEnt(PrototypeLab))
	{
		switch (Research)
		{
		case RESEARCH_PROTOTYPELAB_JETPACKS:
			return (PrototypeLab->v.iuser4 & MASK_UPGRADE_1);
			break;
		case RESEARCH_PROTOTYPELAB_HEAVYARMOUR:
			return (PrototypeLab->v.iuser4 & MASK_UPGRADE_5);
			break;
		default:
			return false;
			break;
		}
	}

	return false;
}



bool UTIL_ObservatoryResearchIsAvailable(const NSResearch Research)
{
	edict_t* Observatory = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_OBSERVATORY);

	if (!FNullEnt(Observatory))
	{
		switch (Research)
		{
		case RESEARCH_OBSERVATORY_DISTRESSBEACON:
			return (Observatory->v.iuser4 & MASK_UPGRADE_5);
		case RESEARCH_OBSERVATORY_MOTIONTRACKING:
			return (Observatory->v.iuser4 & MASK_UPGRADE_6);
		case RESEARCH_OBSERVATORY_PHASETECH:
			return (Observatory->v.iuser4 & MASK_UPGRADE_2);
		case RESEARCH_OBSERVATORY_SCAN:
			return (Observatory->v.iuser4 & MASK_UPGRADE_1);
		default:
			return false;
		}
	}

	return false;
}

bool UTIL_ElectricalResearchIsAvailable(const edict_t* Structure)
{
	if (FNullEnt(Structure)) { return false; }

	if (UTIL_IsStructureElectrified(Structure)) { return false; }

	NSStructureType StructureTypeToElectrify = GetStructureTypeFromEdict(Structure);

	if (!UTIL_StructureTypesMatch(StructureTypeToElectrify, STRUCTURE_MARINE_ANYTURRETFACTORY) && !UTIL_StructureTypesMatch(StructureTypeToElectrify, STRUCTURE_MARINE_RESTOWER)) { return false; }

	return (Structure->v.iuser4 & MASK_UPGRADE_1);
}

bool UTIL_ArmouryResearchIsAvailable(const NSResearch Research)
{
	edict_t* Armoury = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ANYARMOURY);

	if (!FNullEnt(Armoury))
	{
		switch (Research)
		{
		case RESEARCH_ARMOURY_GRENADES:
			return (Armoury->v.iuser4 & MASK_UPGRADE_5);
			break;
		default:
			return false;
			break;
		}
	}

	return false;
}

bool BotCommanderPlaceStructure(bot_t* pBot, int ActionIndex, int Priority)
{
	if (gpGlobals->time < pBot->next_commander_action_time || !UTIL_IsCommanderActionValid(pBot, ActionIndex, Priority)) { return false; }

	commander_action* action = &pBot->CurrentCommanderActions[Priority][ActionIndex];

	if (action->NumActionAttempts > 10)
	{
		UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
		return false;
	}

	pBot->pEdict->v.v_angle = ZERO_VECTOR;

	Vector CommanderViewOrigin = pBot->pEdict->v.origin;
	CommanderViewOrigin.z = GetCommanderViewZHeight();

	if (vEquals(action->DesiredCommanderLocation, ZERO_VECTOR))
	{

		action->DesiredCommanderLocation = UTIL_FindClearCommanderOriginForBuild(pBot, action->BuildLocation, GetCommanderViewZHeight());

		if (vEquals(action->DesiredCommanderLocation, ZERO_VECTOR))
		{
			action->NumActionAttempts++;
			return false;
		}
	}

	if (action->bHasAttemptedAction)
	{
		if (action->NumActionAttempts > 10)
		{
			UTIL_ClearCommanderAction(pBot, ActionIndex, Priority);
			return false;
		}

		if (action->NumActionAttempts < 5 || action->StructureToBuild == STRUCTURE_MARINE_RESTOWER)
		{
			action->DesiredCommanderLocation = UTIL_RandomPointOnCircle(action->BuildLocation, UTIL_MetresToGoldSrcUnits(2.0f));
		}
		else
		{
			action->BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, action->BuildLocation, UTIL_MetresToGoldSrcUnits(2.0f));
			action->DesiredCommanderLocation = UTIL_FindClearCommanderOriginForBuild(pBot, action->BuildLocation, GetCommanderViewZHeight());
		}
	}

	if (vDist2DSq(CommanderViewOrigin, action->DesiredCommanderLocation) > sqrf(8.0f))
	{
		action->bHasAttemptedAction = false;
		pBot->UpMove = action->DesiredCommanderLocation.x / kWorldPosNetworkConstant;
		pBot->SideMove = action->DesiredCommanderLocation.y / kWorldPosNetworkConstant;
		pBot->ForwardMove = 0.0f;

		pBot->impulse = IMPULSE_COMMANDER_MOVETO;

		pBot->next_commander_action_time = gpGlobals->time + 0.1f;

		return true;
	}

	Vector PickRay = UTIL_GetVectorNormal(action->BuildLocation - CommanderViewOrigin);

	pBot->UpMove = PickRay.x * kSelectionNetworkConstant;
	pBot->SideMove = PickRay.y * kSelectionNetworkConstant;
	pBot->ForwardMove = PickRay.z * kSelectionNetworkConstant;

	pBot->impulse = UTIL_StructureTypeToImpulseCommand(action->StructureToBuild);

	pBot->pEdict->v.button |= IN_ATTACK;

	action->NumActionAttempts++;
	action->bHasAttemptedAction = true;
	action->StructureBuildAttemptTime = gpGlobals->time;
	action->LastAttemptedCommanderAngle = Vector(pBot->UpMove, pBot->SideMove, pBot->ForwardMove);
	action->LastAttemptedCommanderLocation = CommanderViewOrigin;

	pBot->next_commander_action_time = gpGlobals->time + commander_action_cooldown;

	return true;
}

int UTIL_GetCostOfResearch(const NSResearch Research)
{
	switch (Research)
	{
	case RESEARCH_ARMSLAB_ARMOUR1:
		return kArmorOneResearchCost;
		break;
	case RESEARCH_ARMSLAB_ARMOUR2:
		return kArmorTwoResearchCost;
		break;
	case RESEARCH_ARMSLAB_ARMOUR3:
		return kArmorThreeResearchCost;
		break;
	case RESEARCH_ARMSLAB_WEAPONS1:
		return kArmorOneResearchCost;
		break;
	case RESEARCH_ARMSLAB_WEAPONS2:
		return kArmorTwoResearchCost;
		break;
	case RESEARCH_ARMSLAB_WEAPONS3:
		return kArmorThreeResearchCost;
		break;
	default:
		return 0;
	}

	return 0;
}

Vector UTIL_FindClearCommanderOriginForBuild(const bot_t* Commander, const Vector BuildLocation, const float CommanderViewZ)
{
	const Vector DirectlyAboveLocation = Vector(BuildLocation.x, BuildLocation.y, CommanderViewZ - 8.0f);
	const Vector DesiredBuildLocation = BuildLocation + Vector(0.0f, 0.0f, 8.0f);
	int NumTries = 100;
	int TryNum = 0;

	if (UTIL_QuickTrace(Commander->pEdict, DirectlyAboveLocation, DesiredBuildLocation))
	{
		return DirectlyAboveLocation;
	}

	while (TryNum < NumTries)
	{
		Vector TestLocation = UTIL_RandomPointOnCircle(DesiredBuildLocation, UTIL_MetresToGoldSrcUnits(3.0f));
		TestLocation.z = CommanderViewZ - 8.0f;

		if (UTIL_QuickTrace(Commander->pEdict, TestLocation, DesiredBuildLocation))
		{
			return TestLocation;
		}

		TryNum++;
	}

	return ZERO_VECTOR;

}

bool UTIL_CancelMarineOrder(bot_t* CommanderBot, int CommanderOrderIndex)
{
	if (gpGlobals->time < CommanderBot->next_commander_action_time) { return false; }

	edict_t* Player = clients[CommanderOrderIndex];
	edict_t* OrderTarget = CommanderBot->LastPlayerOrders[CommanderOrderIndex].Target;
	AvHOrderType OrderType = CommanderBot->LastPlayerOrders[CommanderOrderIndex].OrderType;
	Vector OrderLocation = CommanderBot->LastPlayerOrders[CommanderOrderIndex].MoveLocation;

	if (FNullEnt(Player) || !IsPlayerOnMarineTeam(Player) || IsPlayerDead(Player)) { return false; }

	if (OrderType == ORDERTYPET_BUILD && FNullEnt(OrderTarget)) { return false; }

	MESSAGE_BEGIN(MSG_ONE, REG_USER_MSG("SetOrder", -1), NULL, Player);
	WRITE_BYTE(ENTINDEX(Player));
	WRITE_BYTE(OrderType);

	if (OrderType == ORDERTYPET_BUILD)
	{
		WRITE_SHORT(ENTINDEX(OrderTarget));
	}
	else
	{
		WRITE_COORD(OrderLocation.x);
		WRITE_COORD(OrderLocation.y);
		WRITE_COORD(OrderLocation.z);
	}

	if (OrderTarget)
	{
		WRITE_BYTE(OrderTarget->v.iuser3);
	}
	else
	{
		WRITE_BYTE(AVH_USER3_NONE);
	}
	WRITE_BYTE(true);
	WRITE_BYTE(kOrderStatusComplete);

	MESSAGE_END();

	CommanderBot->next_commander_action_time = gpGlobals->time + 0.5f;

	return true;
}

bool UTIL_IsMarineOrderComplete(bot_t* CommanderBot, int PlayerIndex)
{
	if (!CommanderBot->LastPlayerOrders[PlayerIndex].bIsActive) { return true; }

	edict_t* Player = clients[PlayerIndex];

	if (!Player) { return true; }

	switch (CommanderBot->LastPlayerOrders[PlayerIndex].OrderType)
	{
	case ORDERTYPEL_MOVE:
		return (vEquals(CommanderBot->LastPlayerOrders[PlayerIndex].MoveLocation, ZERO_VECTOR) || vDist3DSq(Player->v.origin, CommanderBot->LastPlayerOrders[PlayerIndex].MoveLocation) < sqrf(UTIL_MetresToGoldSrcUnits(move_order_success_dist_metres)));
		break;
	case ORDERTYPET_BUILD:
		return (FNullEnt(CommanderBot->LastPlayerOrders[PlayerIndex].Target) || UTIL_StructureIsFullyBuilt(CommanderBot->LastPlayerOrders[PlayerIndex].Target) || CommanderBot->LastPlayerOrders[PlayerIndex].Target->v.deadflag != DEAD_NO);
		break;
	default:
		return true;
		break;
	}

	return true;
}

bool CommanderProgressResearchAction(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	if (action->ResearchId == RESEARCH_ELECTRICAL)
	{
		if (UTIL_StructureIsResearching(action->StructureOrItem) || !UTIL_ElectricalResearchIsAvailable(action->StructureOrItem))
		{
			UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
			return false;
		}
	}
	else
	{
		if (UTIL_ResearchInProgress(action->ResearchId) || !UTIL_MarineResearchIsAvailable(action->ResearchId))
		{
			UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
			return false;
		}
	}



	return BotCommanderResearchTech(CommanderBot, ActionIndex, Priority);

}

bool CommanderProgressItemDropAction(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	if (!FNullEnt(action->StructureOrItem))
	{
		UTIL_ClearCommanderAction(CommanderBot, ActionIndex, Priority);
		return false;
	}

	return BotCommanderDropItem(CommanderBot, ActionIndex, Priority);
}

bool UTIL_ConfirmMarineOrderComplete(bot_t* CommanderBot, int CommanderOrderIndex)
{
	if (gpGlobals->time < CommanderBot->next_commander_action_time) { return false; }

	edict_t* Player = clients[CommanderOrderIndex];
	edict_t* OrderTarget = CommanderBot->LastPlayerOrders[CommanderOrderIndex].Target;
	AvHOrderType OrderType = CommanderBot->LastPlayerOrders[CommanderOrderIndex].OrderType;
	Vector OrderLocation = CommanderBot->LastPlayerOrders[CommanderOrderIndex].MoveLocation;

	if (FNullEnt(Player) || !IsPlayerOnMarineTeam(Player)) { return false; }

	if (OrderType == ORDERTYPET_BUILD && FNullEnt(OrderTarget)) { return false; }

	MESSAGE_BEGIN(MSG_ONE, REG_USER_MSG("SetOrder", -1), NULL, Player);
	WRITE_BYTE(ENTINDEX(Player));
	WRITE_BYTE(OrderType);

	if (OrderType == ORDERTYPET_BUILD)
	{
		WRITE_SHORT(ENTINDEX(OrderTarget));
	}
	else
	{
		WRITE_COORD(OrderLocation.x);
		WRITE_COORD(OrderLocation.y);
		WRITE_COORD(OrderLocation.z);
	}

	if (OrderTarget)
	{
		WRITE_BYTE(OrderTarget->v.iuser3);
	}
	else
	{
		WRITE_BYTE(AVH_USER3_NONE);
	}
	WRITE_BYTE(true);
	WRITE_BYTE(kOrderStatusComplete);

	MESSAGE_END();

	UTIL_ClearCommanderOrder(CommanderBot, CommanderOrderIndex);

	CommanderBot->LastPlayerOrders[CommanderOrderIndex].LastReminderTime = gpGlobals->time;

	CommanderBot->next_commander_action_time = gpGlobals->time + 0.1f;

	return true;
}

edict_t* UTIL_GetFirstMarineStructureOffNavmesh()
{

	for (auto& it : MarineBuildableStructureMap)
	{
		if (!it.second.bOnNavmesh) { return it.second.edict; }

	}

	return nullptr;
}

void CommanderQueueRecycleAction(bot_t* pBot, edict_t* Structure, int Priority)
{
	int NewActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

	if (NewActionIndex > -1)
	{
		UTIL_ClearCommanderAction(pBot, NewActionIndex, Priority);

		commander_action* Action = &pBot->CurrentCommanderActions[Priority][NewActionIndex];

		Action->bIsActive = true;
		Action->ActionType = ACTION_RECYCLE;
		Action->StructureOrItem = Structure;
		Action->ActionTarget = Structure;

	}
}

void CommanderQueueNextAction(bot_t* pBot)
{

	if ((gpGlobals->time - pBot->CommanderLastBeaconTime > 5.0f) && UTIL_BaseIsInDistress())
	{
		if (!UTIL_ResearchActionAlreadyExists(pBot, RESEARCH_OBSERVATORY_DISTRESSBEACON) && UTIL_MarineResearchIsAvailable(RESEARCH_OBSERVATORY_DISTRESSBEACON))
		{
			CommanderQueueResearch(pBot, RESEARCH_OBSERVATORY_DISTRESSBEACON, 0);
			BotTeamSay(pBot, 0.5f, "Our base needs defending, I'm going to beacon");
			return;
		}
	}

	/* All the highest-priority tasks. Infantry portals, base armoury, arms lab, phase gate etc. */
	int CurrentPriority = 0;

	int NumPlacedOrQueuedPortals = UTIL_GetNumPlacedOrQueuedStructuresOfType(pBot, STRUCTURE_MARINE_INFANTRYPORTAL);

	if (NumPlacedOrQueuedPortals < 2)
	{
		CommanderQueueInfantryPortalBuild(pBot, CurrentPriority);
	}

	if (!UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_ANYARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f)))
	{
		if (UTIL_GetQueuedBuildRequestsOfType(pBot, STRUCTURE_MARINE_ARMOURY) == 0)
		{
			CommanderQueueArmouryBuild(pBot, CurrentPriority);
		}
	}

	if (UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH))
	{
		if (UTIL_GetQueuedBuildRequestsOfType(pBot, STRUCTURE_MARINE_PHASEGATE) == 0 && !UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_PHASEGATE, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			edict_t* ExistingInfantryPortal = UTIL_GetFirstPlacedStructureOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

			if (!FNullEnt(ExistingInfantryPortal))
			{
				Vector NewPhaseGateLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, ExistingInfantryPortal->v.origin, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));

				if (NewPhaseGateLocation != ZERO_VECTOR)
				{
					CommanderQueuePhaseGateBuild(pBot, NewPhaseGateLocation, CurrentPriority);
				}
			}
		}
	}

	/* Next-highest priority tasks. Make sure we have at least 3 resource nodes. */
	CurrentPriority = 1;

	int ResourceTowerDeficit = min_desired_resource_towers - UTIL_GetNumPlacedOrQueuedStructuresOfType(pBot, STRUCTURE_MARINE_RESTOWER);

	for (int i = 0; i < ResourceTowerDeficit; i++)
	{
		CommanderQueueResTowerBuild(pBot, CurrentPriority);
	}

	/* Next-highest priority tasks. Secure one hive to deny it to aliens. */

	CurrentPriority = 2;

	const hive_definition* NearestUnbuiltHive = UTIL_GetNearestHiveOfStatus(UTIL_GetCommChairLocation(), HIVE_STATUS_UNBUILT);

	if (NearestUnbuiltHive)
	{
		QueueSecureHiveAction(pBot, NearestUnbuiltHive->FloorLocation, CurrentPriority);
	}

	/* Next-highest priority tasks. Place arms lab and do basic research including grenades, armour 1 and weapons 1*/

	CurrentPriority = 3;

	if (UTIL_GetNumPlacedOrQueuedStructuresOfType(pBot, STRUCTURE_MARINE_ARMSLAB) < 1)
	{
		CommanderQueueArmsLabBuild(pBot, CurrentPriority);
	}

	if (UTIL_MarineResearchIsAvailable(RESEARCH_ARMOURY_GRENADES) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_ARMOURY_GRENADES))
	{
		CommanderQueueResearch(pBot, RESEARCH_ARMOURY_GRENADES, CurrentPriority);
	}

	if (UTIL_MarineResearchIsAvailable(RESEARCH_ARMSLAB_ARMOUR1) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_ARMSLAB_ARMOUR1))
	{
		CommanderQueueResearch(pBot, RESEARCH_ARMSLAB_ARMOUR1, CurrentPriority);
	}

	if (UTIL_MarineResearchIsAvailable(RESEARCH_ARMSLAB_WEAPONS1) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_ARMSLAB_WEAPONS1))
	{
		CommanderQueueResearch(pBot, RESEARCH_ARMSLAB_WEAPONS1, CurrentPriority);
	}

	/* Next up: Drop some shotguns and welders for the plebs */

	CurrentPriority = 4;

	const hive_definition* FurthestUnbuiltHive = UTIL_GetFurthestHiveOfStatus(UTIL_GetCommChairLocation(), HIVE_STATUS_UNBUILT);

	if (FurthestUnbuiltHive && FurthestUnbuiltHive != NearestUnbuiltHive)
	{
		QueueSecureHiveAction(pBot, FurthestUnbuiltHive->FloorLocation, CurrentPriority);
	}

	int DesiredNumShotguns = 2;

	if (UTIL_GetNearestPlayerOfClass(UTIL_GetCommChairLocation(), CLASS_FADE, UTIL_MetresToGoldSrcUnits(50.0f), NULL) != nullptr)
	{
		DesiredNumShotguns = 4;
	}

	if (UTIL_GetNumPlacedStructuresOfType(STRUCTURE_MARINE_RESTOWER) >= 4 && UTIL_ItemCanBeDeployed(ITEM_MARINE_SHOTGUN) && UTIL_GetNumWeaponsOfTypeInPlay(WEAPON_MARINE_SHOTGUN) < DesiredNumShotguns && UTIL_GetQueuedItemDropRequestsOfType(pBot, ITEM_MARINE_SHOTGUN) == 0)
	{
		edict_t* NearestArmoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(30.0f), true, false);

		if (!FNullEnt(NearestArmoury))
		{
			Vector NewShotgunLocation = UTIL_GetRandomPointOnNavmeshInDonut(MARINE_REGULAR_NAV_PROFILE, NearestArmoury->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), UTIL_MetresToGoldSrcUnits(3.0f));

			if (NewShotgunLocation != ZERO_VECTOR)
			{
				CommanderQueueItemDrop(pBot, ITEM_MARINE_SHOTGUN, NewShotgunLocation, NULL, CurrentPriority);
			}
		}
	}

	if (UTIL_GetNumPlacedStructuresOfType(STRUCTURE_MARINE_RESTOWER) >= min_desired_resource_towers && UTIL_ItemCanBeDeployed(ITEM_MARINE_WELDER) && UTIL_GetNumWeaponsOfTypeInPlay(WEAPON_MARINE_WELDER) < 2 && UTIL_GetQueuedItemDropRequestsOfType(pBot, ITEM_MARINE_WELDER) == 0)
	{
		edict_t* NearestArmoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(30.0f), true, false);

		if (!FNullEnt(NearestArmoury))
		{
			Vector NewWelderLocation = UTIL_GetRandomPointOnNavmeshInDonut(MARINE_REGULAR_NAV_PROFILE, NearestArmoury->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), UTIL_MetresToGoldSrcUnits(3.0f));

			if (NewWelderLocation != ZERO_VECTOR)
			{
				CommanderQueueItemDrop(pBot, ITEM_MARINE_WELDER, NewWelderLocation, NULL, CurrentPriority);
			}
		}
	}

	/* Next: Build an observatory and research phase tech and motion tracking */

	CurrentPriority = 5;

	if (UTIL_GetNumPlacedOrQueuedStructuresOfType(pBot, STRUCTURE_MARINE_OBSERVATORY) < 1)
	{
		edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f), true, false);

		Vector NewLocation = ZERO_VECTOR;

		if (!FNullEnt(Armoury))
		{
			NewLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));

			if (NewLocation != ZERO_VECTOR)
			{
				UTIL_CommanderQueueStructureBuildAtLocation(pBot, NewLocation, STRUCTURE_MARINE_OBSERVATORY, CurrentPriority);
			}
		}
	}

	if (UTIL_MarineResearchIsAvailable(RESEARCH_OBSERVATORY_PHASETECH) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_OBSERVATORY_PHASETECH))
	{
		CommanderQueueResearch(pBot, RESEARCH_OBSERVATORY_PHASETECH, CurrentPriority);
	}

	if (UTIL_MarineResearchIsAvailable(RESEARCH_OBSERVATORY_MOTIONTRACKING) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_OBSERVATORY_MOTIONTRACKING))
	{
		CommanderQueueResearch(pBot, RESEARCH_OBSERVATORY_MOTIONTRACKING, CurrentPriority);
	}

	/* Next: Armour and Weapons 2 */

	CurrentPriority = 6;

	if (UTIL_MarineResearchIsAvailable(RESEARCH_ARMSLAB_ARMOUR2) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_ARMSLAB_ARMOUR2))
	{
		CommanderQueueResearch(pBot, RESEARCH_ARMSLAB_ARMOUR2, CurrentPriority);
	}

	if (UTIL_MarineResearchIsAvailable(RESEARCH_ARMSLAB_WEAPONS2) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_ARMSLAB_WEAPONS2))
	{
		CommanderQueueResearch(pBot, RESEARCH_ARMSLAB_WEAPONS2, CurrentPriority);
	}

	/* Next: Heavy armour and weapons */

	CurrentPriority = 7;

	if (UTIL_GetStructureCountOfType(STRUCTURE_MARINE_ADVARMOURY) < 1 && UTIL_GetQueuedUpgradeRequestsOfType(pBot, STRUCTURE_MARINE_ARMOURY) < 1)
	{
		edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f), true, false);

		if (!FNullEnt(Armoury) && UTIL_StructureCanBeUpgraded(Armoury))
		{
			CommanderQueueUpgrade(pBot, Armoury, CurrentPriority);
		}
	}

	if (UTIL_GetStructureCountOfType(STRUCTURE_MARINE_ADVARMOURY) > 0 && UTIL_GetStructureCountOfType(STRUCTURE_MARINE_ARMSLAB) > 0)
	{
		if (UTIL_GetNumPlacedOrQueuedStructuresOfType(pBot, STRUCTURE_MARINE_PROTOTYPELAB) < 1)
		{
			edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f), true, false);

			Vector NewLocation = ZERO_VECTOR;

			if (!FNullEnt(Armoury))
			{
				NewLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));

				if (NewLocation != ZERO_VECTOR)
				{
					UTIL_CommanderQueueStructureBuildAtLocation(pBot, NewLocation, STRUCTURE_MARINE_PROTOTYPELAB, CurrentPriority);
				}
			}
		}
	}

	if (UTIL_MarineResearchIsAvailable(RESEARCH_PROTOTYPELAB_HEAVYARMOUR) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_PROTOTYPELAB_HEAVYARMOUR))
	{
		CommanderQueueResearch(pBot, RESEARCH_PROTOTYPELAB_HEAVYARMOUR, CurrentPriority);
	}

	/* Next: Drop heavy armour loadouts */

	CurrentPriority = 8;

	if (UTIL_ItemCanBeDeployed(ITEM_MARINE_HEAVYARMOUR))
	{
		// Don't include commander in the count
		int NumMarinePlayers = GAME_GetNumPlayersOnTeam(MARINE_TEAM) - 1;
		int NumPlayersWithEquipment = UTIL_GetNumEquipmentInPlay();

		if (NumMarinePlayers - NumPlayersWithEquipment > 0)
		{
			QueueHeavyArmourLoadout(pBot, UTIL_GetCommChairLocation(), CurrentPriority);
		}
	}

	/* Now siege a hive */

	CurrentPriority = 9;

	if (UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH) && UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_PHASEGATE, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(20.0f)))
	{
		const hive_definition* NearestBuildHive = UTIL_GetNearestBuiltHiveToLocation(UTIL_GetCommChairLocation());

		if (NearestBuildHive)
		{
			QueueSiegeHiveAction(pBot, NearestBuildHive->FloorLocation, CurrentPriority);
		}
	}

	/* Last: everything else */

	CurrentPriority = 10;

	if (UTIL_MarineResearchIsAvailable(RESEARCH_ARMSLAB_ARMOUR3) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_ARMSLAB_ARMOUR3))
	{
		CommanderQueueResearch(pBot, RESEARCH_ARMSLAB_ARMOUR3, CurrentPriority);
	}

	if (UTIL_MarineResearchIsAvailable(RESEARCH_ARMSLAB_WEAPONS3) && !UTIL_ResearchIsAlreadyQueued(pBot, RESEARCH_ARMSLAB_WEAPONS3))
	{
		CommanderQueueResearch(pBot, RESEARCH_ARMSLAB_WEAPONS3, CurrentPriority);
	}

	/* Optional: grab any empty nodes with marines standing by them waiting to cap */

	int eligibleResNode = UTIL_FindFreeResNodeWithMarineNearby(pBot);

	if (eligibleResNode > -1)
	{
		int NumResTowers = UTIL_GetNumPlacedOrQueuedStructuresOfType(pBot, STRUCTURE_MARINE_RESTOWER);

		int NewPriority = (NumResTowers >= 4) ? 2 : 1;

		int FreeActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, NewPriority);

		if (FreeActionIndex > -1)
		{
			UTIL_ClearCommanderAction(pBot, FreeActionIndex, NewPriority);
			pBot->CurrentCommanderActions[NewPriority][FreeActionIndex].bIsActive = true;
			pBot->CurrentCommanderActions[NewPriority][FreeActionIndex].ActionType = ACTION_BUILD;
			pBot->CurrentCommanderActions[NewPriority][FreeActionIndex].BuildLocation = ResourceNodes[eligibleResNode].origin;
			pBot->CurrentCommanderActions[NewPriority][FreeActionIndex].StructureToBuild = STRUCTURE_MARINE_RESTOWER;
		}
	}
}

int UTIL_FindFreeResNodeWithMarineNearby(bot_t* CommanderBot)
{
	for (int i = 0; i < NumTotalResNodes; i++)
	{
		if (ResourceNodes[i].edict && !ResourceNodes[i].bIsOccupied)
		{

			if (UTIL_AnyMarinePlayerNearLocation(ResourceNodes[i].origin, UTIL_MetresToGoldSrcUnits(10.0f)) && !UTIL_ActionExistsInLocation(CommanderBot, ResourceNodes[i].origin))
			{
				return i;
			}
		}
	}

	return -1;
}

commander_action* UTIL_FindCommanderBuildActionOfType(bot_t* pBot, const NSStructureType StructureType, const Vector SearchLocation, const float SearchRadius)
{
	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			commander_action* Action = &pBot->CurrentCommanderActions[Priority][ActionIndex];
			if (Action->ActionType == ACTION_BUILD && UTIL_StructureTypesMatch(Action->StructureToBuild, StructureType))
			{
				if (vDist2DSq(Action->BuildLocation, SearchLocation) <= sqrf(SearchRadius))
				{
					return Action;
				}
			}
		}
	}

	return nullptr;
}

void QueueSiegeHiveAction(bot_t* CommanderBot, const Vector Area, int Priority)
{
	edict_t* PhaseGate = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, Area, UTIL_MetresToGoldSrcUnits(30.0f), true, false);
	edict_t* TurretFactory = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, Area, UTIL_MetresToGoldSrcUnits(20.0f), true, false);

	int NumCompletedSiegeTurrets = UTIL_GetNumBuiltStructuresOfTypeInRadius(STRUCTURE_MARINE_SIEGETURRET, Area, UTIL_MetresToGoldSrcUnits(20.0f));

	if (!FNullEnt(TurretFactory) && GetStructureTypeFromEdict(TurretFactory) == STRUCTURE_MARINE_ADVTURRETFACTORY && NumCompletedSiegeTurrets > 0)
	{
		if (UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_OBSERVATORY) != nullptr && UTIL_GetQueuedItemDropRequestsOfType(CommanderBot, ITEM_MARINE_SCAN) == 0 && (gpGlobals->time - CommanderBot->CommanderLastScanTime) > 10.0f)
		{
			Vector ScanLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, Area, UTIL_MetresToGoldSrcUnits(5.0f));

			if (ScanLocation != ZERO_VECTOR)
			{
				CommanderQueueItemDrop(CommanderBot, ITEM_MARINE_SCAN, ScanLocation, NULL, 0);
			}
		}
	}

	const hive_definition* HiveIndex = UTIL_GetNearestHiveAtLocation(Area);

	if (FNullEnt(PhaseGate))
	{
		commander_action* ExistingTFAction = UTIL_FindCommanderBuildActionOfType(CommanderBot, STRUCTURE_MARINE_TURRETFACTORY, Area, UTIL_MetresToGoldSrcUnits(30.0f));
		commander_action* ExistingArmouryAction = UTIL_FindCommanderBuildActionOfType(CommanderBot, STRUCTURE_MARINE_ARMOURY, Area, UTIL_MetresToGoldSrcUnits(30.0f));

		if (ExistingTFAction != nullptr)
		{
			UTIL_ClearCommanderAction(CommanderBot, ExistingTFAction);
		}

		if (ExistingArmouryAction != nullptr)
		{
			UTIL_ClearCommanderAction(CommanderBot, ExistingArmouryAction);
		}

		commander_action* ExistingAction = UTIL_FindCommanderBuildActionOfType(CommanderBot, STRUCTURE_MARINE_PHASEGATE, Area, UTIL_MetresToGoldSrcUnits(30.0f));

		// We already have a plan to build a phase gate outside this hive, and we haven't already placed it
		if (ExistingAction && vDist2DSq(ExistingAction->BuildLocation, Area) < sqrf(UTIL_MetresToGoldSrcUnits(30.0f)) && FNullEnt(ExistingAction->StructureOrItem))
		{
			edict_t* NearbyMarine = UTIL_FindSafePlayerInArea(MARINE_TEAM, Area, UTIL_MetresToGoldSrcUnits(10.0f), UTIL_MetresToGoldSrcUnits(20.0f));

			if (!FNullEnt(NearbyMarine) && vDist2DSq(ExistingAction->BuildLocation, NearbyMarine->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)) && !UTIL_QuickTrace(NearbyMarine, NearbyMarine->v.origin, HiveIndex->Location))
			{
				Vector BuildLocation = UTIL_ProjectPointToNavmesh(NearbyMarine->v.origin, Vector(100.0f, 50.0f, 100.0f), BUILDING_REGULAR_NAV_PROFILE);

				if (BuildLocation != ZERO_VECTOR)
				{
					ExistingAction->BuildLocation = BuildLocation;
				}

				return;
			}
		}

		// We don't have any plans to build a phase gate yet
		if (!ExistingAction)
		{
			Vector BuildLocation = ZERO_VECTOR;

			edict_t* NearbyMarine = UTIL_FindSafePlayerInArea(MARINE_TEAM, Area, UTIL_MetresToGoldSrcUnits(10.0f), UTIL_MetresToGoldSrcUnits(20.0f));

			if (!FNullEnt(NearbyMarine))
			{
				BuildLocation = UTIL_ProjectPointToNavmesh(NearbyMarine->v.origin, Vector(100.0f, 50.0f, 100.0f), BUILDING_REGULAR_NAV_PROFILE);
			}

			if (vEquals(BuildLocation, ZERO_VECTOR))
			{
				BuildLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, Area, UTIL_MetresToGoldSrcUnits(15.0f), UTIL_MetresToGoldSrcUnits(20.0f));
			}

			if (BuildLocation != ZERO_VECTOR && (!HiveIndex || !UTIL_QuickTrace(CommanderBot->pEdict, UTIL_GetCentreOfEntity(HiveIndex->edict), BuildLocation + Vector(0.0f, 0.0f, 5.0f))))
			{
				UTIL_CommanderQueueStructureBuildAtLocation(CommanderBot, BuildLocation, STRUCTURE_MARINE_PHASEGATE, Priority);
			}

		}

		return;
	}

	if (!UTIL_StructureIsFullyBuilt(PhaseGate)) { return; }

	edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ARMOURY, PhaseGate->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), true, false);

	if (FNullEnt(Armoury))
	{
		if (UTIL_GetQueuedBuildRequestsOfType(CommanderBot, STRUCTURE_MARINE_ARMOURY) == 0)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, PhaseGate->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), UTIL_MetresToGoldSrcUnits(5.0f));

			if (BuildLocation != ZERO_VECTOR)
			{
				UTIL_CommanderQueueStructureBuildAtLocation(CommanderBot, BuildLocation, STRUCTURE_MARINE_ARMOURY, 0);
			}
		}
	}



	if (FNullEnt(TurretFactory))
	{
		if (UTIL_GetQueuedBuildRequestsOfType(CommanderBot, STRUCTURE_MARINE_ANYTURRETFACTORY) == 0)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInDonut(BUILDING_REGULAR_NAV_PROFILE, PhaseGate->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), UTIL_MetresToGoldSrcUnits(5.0f));

			if (BuildLocation != ZERO_VECTOR && vDist2DSq(BuildLocation, HiveIndex->Location) < sqrf(1100.0f))
			{
				UTIL_CommanderQueueStructureBuildAtLocation(CommanderBot, BuildLocation, STRUCTURE_MARINE_TURRETFACTORY, 0);
			}
		}
		return;
	}
	else
	{
		if (!UTIL_StructureIsFullyBuilt(TurretFactory))
		{
			return;
		}

		if (GetStructureTypeFromEdict(TurretFactory) != STRUCTURE_MARINE_ADVTURRETFACTORY)
		{

			if (UTIL_StructureIsUpgrading(TurretFactory))
			{
				return;
			}

			if (UTIL_GetQueuedUpgradeRequestsOfType(CommanderBot, STRUCTURE_MARINE_TURRETFACTORY) == 0)
			{
				CommanderQueueUpgrade(CommanderBot, TurretFactory, 0);
			}

			return;
		}
		else
		{
			if (UTIL_StructureIsUpgrading(TurretFactory))
			{
				return;
			}

			int NumSiegeTurrets = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_SIEGETURRET, TurretFactory->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

			if (NumSiegeTurrets < 4 && UTIL_GetQueuedBuildRequestsOfType(CommanderBot, STRUCTURE_MARINE_SIEGETURRET) == 0)
			{
				Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, TurretFactory->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

				if (BuildLocation != ZERO_VECTOR && UTIL_PointIsDirectlyReachable(TurretFactory->v.origin, BuildLocation) && vDist2DSq(BuildLocation, HiveIndex->Location) < sqrf(1100.0f))
				{
					UTIL_CommanderQueueStructureBuildAtLocation(CommanderBot, BuildLocation, STRUCTURE_MARINE_SIEGETURRET, 0);
				}
			}

			if (NumSiegeTurrets > 0 && !UTIL_IsStructureElectrified(TurretFactory) && !UTIL_ResearchActionAlreadyExists(CommanderBot, RESEARCH_ELECTRICAL))
			{
				CommanderQueueElectricResearch(CommanderBot, 0, TurretFactory);
			}

		}
	}

}

void QueueSecureHiveAction(bot_t* CommanderBot, const Vector Area, int Priority)
{
	const resource_node* HiveResourceNode = UTIL_FindNearestResNodeToLocation(Area);

	if (HiveResourceNode)
	{
		if (!HiveResourceNode->bIsOccupied)
		{
			if (!UTIL_ActionExistsInLocation(CommanderBot, HiveResourceNode->origin))
			{
				UTIL_CommanderQueueStructureBuildAtLocation(CommanderBot, HiveResourceNode->origin, STRUCTURE_MARINE_RESTOWER, Priority);
			}
		}
	}

	edict_t* ExistingPhaseGate = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, Area, UTIL_MetresToGoldSrcUnits(15.0f), true, false);
	edict_t* BasePhaseGate = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(20.0f), true, false);
	edict_t* ExistingTurretFactory = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, Area, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

	if (!FNullEnt(BasePhaseGate) && FNullEnt(ExistingPhaseGate))
	{
		if (UTIL_GetQueuedBuildRequestsOfTypeInArea(CommanderBot, STRUCTURE_MARINE_PHASEGATE, Area, UTIL_MetresToGoldSrcUnits(10.0f)) == 0)
		{
			if (UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH))
			{
				Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, Area, UTIL_MetresToGoldSrcUnits(5.0f));

				if (BuildLocation != ZERO_VECTOR && UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), BuildLocation, max_player_use_reach))
				{
					// If we already have secured this hive, then make adding a phase gate a top priority to fully secure it
					int PhasePriority = FNullEnt(ExistingTurretFactory) ? Priority : 0;
					CommanderQueuePhaseGateBuild(CommanderBot, BuildLocation, PhasePriority);
				}
			}
		}
	}

	if (FNullEnt(ExistingTurretFactory))
	{
		if (UTIL_GetQueuedBuildRequestsOfTypeInArea(CommanderBot, STRUCTURE_MARINE_TURRETFACTORY, Area, UTIL_MetresToGoldSrcUnits(10.0f)) == 0)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, Area, UTIL_MetresToGoldSrcUnits(5.0f));

			if (BuildLocation != ZERO_VECTOR && UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), BuildLocation, max_player_use_reach))
			{
				UTIL_CommanderQueueStructureBuildAtLocation(CommanderBot, BuildLocation, STRUCTURE_MARINE_TURRETFACTORY, Priority);
			}
		}
	}
	else
	{
		int NumTurrets = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_TURRET, ExistingTurretFactory->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

		if (NumTurrets < 4 && UTIL_GetQueuedBuildRequestsOfTypeInArea(CommanderBot, STRUCTURE_MARINE_TURRET, ExistingTurretFactory->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)) == 0)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, ExistingTurretFactory->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (BuildLocation != ZERO_VECTOR && UTIL_PointIsDirectlyReachable(BuildLocation, ExistingTurretFactory->v.origin))
			{
				UTIL_CommanderQueueStructureBuildAtLocation(CommanderBot, BuildLocation, STRUCTURE_MARINE_TURRET, 0);
			}
		}
	}

	if (!FNullEnt(ExistingPhaseGate))
	{
		if (!UTIL_IsStructureElectrified(ExistingTurretFactory) && !UTIL_ResearchActionAlreadyExists(CommanderBot, RESEARCH_ELECTRICAL))
		{
			CommanderQueueElectricResearch(CommanderBot, Priority, ExistingTurretFactory);
		}
	}

}

void QueueHeavyArmourLoadout(bot_t* CommanderBot, const Vector Area, int Priority)
{

	if (!UTIL_ItemCanBeDeployed(ITEM_MARINE_HEAVYARMOUR)) { return; }

	edict_t* NearestProtoLab = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PROTOTYPELAB, Area, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

	if (FNullEnt(NearestProtoLab)) { return; }

	edict_t* NearestArmoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ADVARMOURY, NearestProtoLab->v.origin, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

	if (FNullEnt(NearestArmoury)) { return; }

	edict_t* MarineNeedingLoadout = UTIL_GetNearestMarineWithoutFullLoadout(NearestArmoury->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

	if (FNullEnt(MarineNeedingLoadout)) { return; }

	if (!PlayerHasEquipment(MarineNeedingLoadout))
	{
		int NumExistingArmours = UTIL_GetItemCountOfTypeInArea(ITEM_MARINE_HEAVYARMOUR, NearestProtoLab->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumExistingArmours == 0)
		{

			if (UTIL_GetQueuedItemDropRequestsOfType(CommanderBot, ITEM_MARINE_HEAVYARMOUR) == 0)
			{
				if (vDist2DSq(MarineNeedingLoadout->v.origin, NearestProtoLab->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					CommanderQueueItemDrop(CommanderBot, ITEM_MARINE_HEAVYARMOUR, ZERO_VECTOR, MarineNeedingLoadout, Priority);
				}
				else
				{
					CommanderQueueItemDrop(CommanderBot, ITEM_MARINE_HEAVYARMOUR, UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, NearestProtoLab->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)), nullptr, Priority);
				}
			}
		}
	}

	if (!PlayerHasSpecialWeapon(MarineNeedingLoadout))
	{
		NSWeapon SpecialWeapon = (UTIL_GetNumWeaponsOfTypeInPlay(WEAPON_MARINE_GL) == 0) ? WEAPON_MARINE_GL : WEAPON_MARINE_HMG;

		NSDeployableItem SpecialWeaponItemType = UTIL_WeaponTypeToDeployableItem(SpecialWeapon);

		int ExistingWeapons = UTIL_GetItemCountOfTypeInArea(SpecialWeaponItemType, NearestProtoLab->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (ExistingWeapons == 0)
		{

			if (UTIL_GetQueuedItemDropRequestsOfType(CommanderBot, SpecialWeaponItemType) == 0)
			{
				if (vDist2DSq(MarineNeedingLoadout->v.origin, NearestArmoury->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					CommanderQueueItemDrop(CommanderBot, SpecialWeaponItemType, ZERO_VECTOR, MarineNeedingLoadout, Priority);
				}
				else
				{
					CommanderQueueItemDrop(CommanderBot, SpecialWeaponItemType, UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, NearestArmoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)), nullptr, Priority);
				}
			}

			return;
		}
	}

	if (!PlayerHasWeapon(MarineNeedingLoadout, WEAPON_MARINE_WELDER))
	{
		
		int ExistingWelders = UTIL_GetItemCountOfTypeInArea(ITEM_MARINE_WELDER, NearestArmoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (ExistingWelders == 0)
		{

			if (UTIL_GetQueuedItemDropRequestsOfType(CommanderBot, ITEM_MARINE_WELDER) == 0)
			{
				if (vDist2DSq(MarineNeedingLoadout->v.origin, NearestArmoury->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					CommanderQueueItemDrop(CommanderBot, ITEM_MARINE_WELDER, ZERO_VECTOR, MarineNeedingLoadout, Priority);
				}
				else
				{
					CommanderQueueItemDrop(CommanderBot, ITEM_MARINE_WELDER, UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, NearestArmoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)), nullptr, Priority);
				}
			}

			return;
		}
	}
}

int UTIL_FindClosestAvailableMarinePlayer(bot_t* CommanderBot, const Vector Location)
{
	int nearestPlayer = -1;
	float nearestDist = 0.0f;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && IsPlayerOnMarineTeam(clients[i]) && IsPlayerActiveInGame(clients[i]))
		{
			if (IsPlayerBot(clients[i]))
			{
				int BotIndex = GetBotIndex(clients[i]);

				if (BotIndex > -1)
				{
					if (bots[BotIndex].CurrentRole == BOT_ROLE_SWEEPER) { continue; }
				}

			}

			bool bPlayerHasOrder = CommanderBot->LastPlayerOrders[i].bIsActive;

			if (!bPlayerHasOrder)
			{
				float playerDist = vDist2DSq(clients[i]->v.origin, Location);

				if (nearestPlayer < 0 || playerDist < nearestDist)
				{
					nearestPlayer = i;
					nearestDist = playerDist;
				}
			}
		}
	}

	return nearestPlayer;
}

int UTIL_GetNumArmouriesUpgrading()
{
	int Result = 0;

	for (auto& it : MarineBuildableStructureMap)
	{
		if (!it.second.bOnNavmesh) { continue; }
		if (!UTIL_StructureTypesMatch(STRUCTURE_MARINE_ARMOURY, it.second.StructureType)) { continue; }

		if (UTIL_StructureIsFullyBuilt(it.second.edict) && UTIL_StructureIsUpgrading(it.second.edict)) { Result++; }

	}

	return Result;
}

bool UTIL_ResearchActionAlreadyExists(const bot_t* Commander, const NSResearch Research)
{
	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			if (Commander->CurrentCommanderActions[Priority][ActionIndex].ActionType == ACTION_RESEARCH && Commander->CurrentCommanderActions[Priority][ActionIndex].ResearchId == Research)
			{
				return true;
			}
		}
	}

	return false;
}

bool UTIL_IsCommanderActionComplete(bot_t* CommanderBot, int ActionIndex, int Priority)
{
	commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];

	if (!action->bIsActive || action->ActionType == ACTION_NONE) { return true; }

	switch (action->ActionType)
	{
	case ACTION_BUILD:
		return (!FNullEnt(action->StructureOrItem) && UTIL_StructureIsFullyBuilt(action->StructureOrItem));
	case ACTION_DROPITEM:
		return (!FNullEnt(action->StructureOrItem));
	case ACTION_GIVEORDER:
		return false;
	case ACTION_RECYCLE:
		return (FNullEnt(action->StructureOrItem) || action->StructureOrItem->v.deadflag != DEAD_NO || UTIL_StructureIsRecycling(action->StructureOrItem));
	case ACTION_RESEARCH:
		return !UTIL_MarineResearchIsAvailable(action->ResearchId);
	case ACTION_UPGRADE:
		return (!UTIL_StructureCanBeUpgraded(action->StructureOrItem));
	default:
		return true;
	}

	return true;
}

int UTIL_GetQueuedUpgradeRequestsOfType(bot_t* CommanderBot, NSStructureType StructureToBeUpgraded)
{
	int Result = 0;

	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			if (CommanderBot->CurrentCommanderActions[Priority][ActionIndex].bIsActive && CommanderBot->CurrentCommanderActions[Priority][ActionIndex].ActionType == ACTION_UPGRADE && CommanderBot->CurrentCommanderActions[Priority][ActionIndex].StructureToBuild == StructureToBeUpgraded)
			{
				Result++;
			}
		}
	}

	return Result;
}

int UTIL_GetQueuedItemDropRequestsOfType(bot_t* CommanderBot, NSDeployableItem ItemType)
{
	int Result = 0;

	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			if (CommanderBot->CurrentCommanderActions[Priority][ActionIndex].bIsActive && CommanderBot->CurrentCommanderActions[Priority][ActionIndex].ActionType == ACTION_DROPITEM && CommanderBot->CurrentCommanderActions[Priority][ActionIndex].ItemToDeploy == ItemType)
			{
				Result++;
			}
		}
	}

	return Result;
}

int UTIL_GetQueuedBuildRequestsOfTypeInArea(bot_t* CommanderBot, NSStructureType StructureType, const Vector SearchLocation, const float SearchRadius)
{
	int result = 0;

	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			if (CommanderBot->CurrentCommanderActions[Priority][ActionIndex].bIsActive && CommanderBot->CurrentCommanderActions[Priority][ActionIndex].ActionType == ACTION_BUILD && UTIL_StructureTypesMatch(CommanderBot->CurrentCommanderActions[Priority][ActionIndex].StructureToBuild, StructureType))
			{
				if (vDist2DSq(CommanderBot->CurrentCommanderActions[Priority][ActionIndex].BuildLocation, SearchLocation) < sqrf(SearchRadius))
				{
					result++;
				}
				
			}
		}
	}
	return result;
}

int UTIL_GetQueuedBuildRequestsOfType(bot_t* CommanderBot, NSStructureType StructureType)
{
	int result = 0;

	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			if (CommanderBot->CurrentCommanderActions[Priority][ActionIndex].bIsActive && CommanderBot->CurrentCommanderActions[Priority][ActionIndex].ActionType == ACTION_BUILD && UTIL_StructureTypesMatch(CommanderBot->CurrentCommanderActions[Priority][ActionIndex].StructureToBuild, StructureType))
			{
				result++;
			}
		}
	}
	return result;
}

bool UTIL_ItemCanBeDeployed(NSDeployableItem ItemToDeploy)
{
	switch (ItemToDeploy)
	{
	case ITEM_MARINE_AMMO:
	case ITEM_MARINE_HEALTHPACK:
		return true;
	case ITEM_MARINE_MINES:
	case ITEM_MARINE_WELDER:
	case ITEM_MARINE_SHOTGUN:
		return (UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_ANYARMOURY) != nullptr);
	case ITEM_MARINE_GRENADELAUNCHER:
	case ITEM_MARINE_HMG:
		return (UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_ADVARMOURY) != nullptr);
	case ITEM_MARINE_HEAVYARMOUR:
		return UTIL_ResearchIsComplete(RESEARCH_PROTOTYPELAB_HEAVYARMOUR);
	case ITEM_MARINE_JETPACK:
		return UTIL_ResearchIsComplete(RESEARCH_PROTOTYPELAB_JETPACKS);
	case ITEM_MARINE_SCAN:
		return UTIL_ObservatoryResearchIsAvailable(RESEARCH_OBSERVATORY_SCAN);
	default:
		return false;
	}

	return false;
}

void CommanderQueuePhaseGateBuild(bot_t* pBot, const Vector Location, int Priority)
{

	if (Location != ZERO_VECTOR)
	{
		int NewActionIndex = UTIL_CommanderFirstFreeActionIndex(pBot, Priority);

		if (NewActionIndex > -1)
		{
			UTIL_ClearCommanderAction(pBot, NewActionIndex, Priority);

			commander_action* action = &pBot->CurrentCommanderActions[Priority][NewActionIndex];

			action->bIsActive = true;
			action->ActionType = ACTION_BUILD;
			action->BuildLocation = Location;
			action->StructureToBuild = STRUCTURE_MARINE_PHASEGATE;
		}
	}
}

void CommanderGetPrimaryTask(bot_t* pBot, bot_task* Task)
{

}
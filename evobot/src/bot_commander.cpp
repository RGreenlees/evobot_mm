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
#include "bot_task.h"
#include "player_util.h"

#include <unordered_map>

#include <extdll.h>
#include <dllapi.h>
#include <meta_api.h>


extern std::unordered_map<int, buildable_structure> MarineBuildableStructureMap;

extern edict_t* clients[MAX_CLIENTS];
extern bot_t bots[MAX_CLIENTS];

void COMM_CommanderProgressAction(bot_t* CommanderBot, commander_action* Action)
{
	if (!Action || Action->ActionType == ACTION_NONE) { return; }

	switch (Action->ActionType)
	{
	case ACTION_DEPLOY:
		BotCommanderDeploy(CommanderBot, Action);
		break;
	case ACTION_UPGRADE:
		BotCommanderUpgradeStructure(CommanderBot, Action);
		break;
	case ACTION_RESEARCH:
		BotCommanderResearchTech(CommanderBot, Action);
		break;
	case ACTION_RECYCLE:
		BotCommanderRecycleStructure(CommanderBot, Action);
		break;
	default:
		return;
	}

	return;
}

void BotCommanderRecycleStructure(bot_t* pBot, commander_action* Action)
{

	if (Action->NumActionAttempts > 5)
	{
		UTIL_ClearCommanderAction(Action);
		return;
	}

	buildable_structure* BuildRef = UTIL_GetBuildableStructureRefFromEdict(Action->ActionTarget);

	// If we are already selecting the building then we can just send the impulse and finish up
	if (pBot->CommanderCurrentlySelectedBuilding == Action->ActionTarget)
	{
		if (BuildRef)
		{
			if (Action->LastAttemptedCommanderLocation != ZERO_VECTOR)
			{
				BuildRef->LastSuccessfulCommanderLocation = Action->LastAttemptedCommanderLocation;
			}

			if (Action->LastAttemptedCommanderAngle != ZERO_VECTOR)
			{
				BuildRef->LastSuccessfulCommanderAngle = Action->LastAttemptedCommanderAngle;
			}
		}

		pBot->impulse = IMPULSE_COMMANDER_RECYCLEBUILDING;
		Action->ActionStep = ACTION_STEP_NONE;

		pBot->next_commander_action_time = gpGlobals->time + commander_action_cooldown;
		return;
	}

	BotCommanderSelectStructure(pBot, Action->ActionTarget, Action);
}

void UTIL_IssueMarineMoveToOrder(bot_t* CommanderBot, edict_t* Recipient, const Vector Destination)
{
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

	CommanderBot->next_commander_action_time = gpGlobals->time + 1.0f;

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

void COMM_IssueMarineSiegeHiveOrder(bot_t* CommanderBot, edict_t* Recipient, const hive_definition* HiveToSiege, const Vector SiegePosition)
{
	if (!HiveToSiege || FNullEnt(Recipient) || !IsPlayerActiveInGame(Recipient)) { return; }

	int ClientIndex = GAME_GetClientIndex(Recipient);

	if (ClientIndex < 0) { return; }

	CommanderBot->LastPlayerOrders[ClientIndex].bIsActive = true;
	CommanderBot->LastPlayerOrders[ClientIndex].MoveLocation = SiegePosition;
	CommanderBot->LastPlayerOrders[ClientIndex].OrderPurpose = ORDERPURPOSE_SIEGE_HIVE;
	CommanderBot->LastPlayerOrders[ClientIndex].Target = HiveToSiege->edict;
	CommanderBot->LastPlayerOrders[ClientIndex].OrderType = ORDERTYPEL_MOVE;

	MESSAGE_BEGIN(MSG_ONE, REG_USER_MSG("SetOrder", -1), NULL, Recipient);
	WRITE_BYTE(ENTINDEX(Recipient));
	WRITE_BYTE(ORDERTYPEL_MOVE);

	WRITE_COORD(SiegePosition.x);
	WRITE_COORD(SiegePosition.y);
	WRITE_COORD(SiegePosition.z);

	WRITE_BYTE(AVH_USER3_NONE);
	WRITE_BYTE(false);
	WRITE_BYTE(kOrderStatusActive);

	MESSAGE_END();

	if (IsPlayerBot(Recipient))
	{
		bot_t* BotRef = GetBotPointer(Recipient);

		if (BotRef)
		{
			BotReceiveCommanderOrder(BotRef, ORDERTYPEL_MOVE, AVH_USER3_NONE, SiegePosition);
		}
	}
}

void COMM_IssueMarineSecureResNodeOrder(bot_t* CommanderBot, edict_t* Recipient, const resource_node* ResNode)
{
	if (!ResNode || FNullEnt(Recipient) || !IsPlayerActiveInGame(Recipient)) { return; }

	int ClientIndex = GAME_GetClientIndex(Recipient);

	if (ClientIndex < 0) { return; }

	Vector MoveLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, ResNode->origin, UTIL_MetresToGoldSrcUnits(3.0f));

	if (MoveLocation == ZERO_VECTOR)
	{
		MoveLocation = ResNode->origin;
	}

	CommanderBot->LastPlayerOrders[ClientIndex].bIsActive = true;
	CommanderBot->LastPlayerOrders[ClientIndex].MoveLocation = MoveLocation;
	CommanderBot->LastPlayerOrders[ClientIndex].OrderPurpose = ORDERPURPOSE_SECURE_RESNODE;
	CommanderBot->LastPlayerOrders[ClientIndex].Target = ResNode->edict;
	CommanderBot->LastPlayerOrders[ClientIndex].OrderType = ORDERTYPEL_MOVE;

	MESSAGE_BEGIN(MSG_ONE, REG_USER_MSG("SetOrder", -1), NULL, Recipient);
	WRITE_BYTE(ENTINDEX(Recipient));
	WRITE_BYTE(ORDERTYPEL_MOVE);

	WRITE_COORD(MoveLocation.x);
	WRITE_COORD(MoveLocation.y);
	WRITE_COORD(MoveLocation.z);

	WRITE_BYTE(AVH_USER3_NONE);
	WRITE_BYTE(false);
	WRITE_BYTE(kOrderStatusActive);

	MESSAGE_END();

	if (IsPlayerBot(Recipient))
	{
		bot_t* BotRef = GetBotPointer(Recipient);

		if (BotRef)
		{
			BotReceiveCommanderOrder(BotRef, ORDERTYPEL_MOVE, AVH_USER3_NONE, MoveLocation);
		}
	}
}


void COMM_IssueMarineSecureHiveOrder(bot_t* CommanderBot, edict_t* Recipient, const hive_definition* HiveToSecure)
{
	if (!HiveToSecure || FNullEnt(Recipient) || !IsPlayerActiveInGame(Recipient)) { return; }

	int ClientIndex = GAME_GetClientIndex(Recipient);

	if (ClientIndex < 0) { return; }

	bool bSecureResNode = true;

	edict_t* TF = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, HiveToSecure->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

	if (FNullEnt(TF) || !UTIL_StructureIsFullyBuilt(TF)) { bSecureResNode = false; }

	bool bPhaseGatesAvailable = UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH);

	if (bPhaseGatesAvailable)
	{
		edict_t* PhaseGate = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, HiveToSecure->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

		if (FNullEnt(PhaseGate) || !UTIL_StructureIsFullyBuilt(PhaseGate)) { bSecureResNode = false; }
	}

	if (!FNullEnt(TF))
	{
		int NumTurrets = UTIL_GetNumBuiltStructuresOfTypeInRadius(STRUCTURE_MARINE_TURRET, TF->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumTurrets < 5) { bSecureResNode = false; }
	}

	Vector OrderLocation = ZERO_VECTOR;

	const resource_node* ResNode = UTIL_GetResourceNodeAtIndex(HiveToSecure->HiveResNodeIndex);

	if (bSecureResNode && ResNode)
	{
		
		OrderLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, ResNode->origin, UTIL_MetresToGoldSrcUnits(2.0f));
	}
	else
	{
		Vector NearestPoint = FindClosestNavigablePointToDestination(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), HiveToSecure->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));

		if (NearestPoint == ZERO_VECTOR || vDist2DSq(NearestPoint, HiveToSecure->FloorLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			NearestPoint = HiveToSecure->FloorLocation;
		}

		OrderLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, NearestPoint, UTIL_MetresToGoldSrcUnits(5.0f));
	}

	CommanderBot->LastPlayerOrders[ClientIndex].bIsActive = true;
	CommanderBot->LastPlayerOrders[ClientIndex].MoveLocation = OrderLocation;
	CommanderBot->LastPlayerOrders[ClientIndex].OrderPurpose = ORDERPURPOSE_SECURE_HIVE;
	CommanderBot->LastPlayerOrders[ClientIndex].Target = HiveToSecure->edict;
	CommanderBot->LastPlayerOrders[ClientIndex].OrderType = ORDERTYPEL_MOVE;

	MESSAGE_BEGIN(MSG_ONE, REG_USER_MSG("SetOrder", -1), NULL, Recipient);
	WRITE_BYTE(ENTINDEX(Recipient));
	WRITE_BYTE(ORDERTYPEL_MOVE);

	WRITE_COORD(OrderLocation.x);
	WRITE_COORD(OrderLocation.y);
	WRITE_COORD(OrderLocation.z);

	WRITE_BYTE(AVH_USER3_NONE);
	WRITE_BYTE(false);
	WRITE_BYTE(kOrderStatusActive);

	MESSAGE_END();

	if (IsPlayerBot(Recipient))
	{
		bot_t* BotRef = GetBotPointer(Recipient);

		if (BotRef)
		{
			BotReceiveCommanderOrder(BotRef, ORDERTYPEL_MOVE, AVH_USER3_NONE, OrderLocation);
		}
	}

}

void UTIL_IssueMarineBuildOrder(bot_t* CommanderBot, edict_t* Recipient, edict_t* StructureToBuild)
{
	if (FNullEnt(StructureToBuild) || StructureToBuild->v.deadflag != DEAD_NO) { return; }


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

	CommanderBot->next_commander_action_time = gpGlobals->time + 1.0f;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i] == Recipient)
		{
			float dist = UTIL_GetPathCostBetweenLocations(MARINE_REGULAR_NAV_PROFILE, Recipient->v.origin, StructureToBuild->v.origin);
			CommanderBot->LastPlayerOrders[i].bIsActive = true;
			CommanderBot->LastPlayerOrders[i].OrderType = ORDERTYPET_BUILD;
			CommanderBot->LastPlayerOrders[i].MoveLocation = StructureToBuild->v.origin;
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

	}
}

void CommanderReceiveAmmoRequest(bot_t* pBot, edict_t* Requestor)
{
	if (!Requestor) { return; }

	if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_ANYARMOURY, Requestor->v.origin, UTIL_MetresToGoldSrcUnits(15.0f)))
	{
		char buf[512];
		sprintf(buf, "Can you use the armoury please, %s?", STRING(Requestor->v.netname));
		BotTeamSay(pBot, 2.0f, buf);
		return;
	}

	if (UTIL_GetItemCountOfTypeInArea(DEPLOYABLE_ITEM_MARINE_AMMO, Requestor->v.origin, UTIL_MetresToGoldSrcUnits(10.0f)) > 0)
	{
		char buf[512];
		sprintf(buf, "I've already dropped ammo there, %s", STRING(Requestor->v.netname));
		BotTeamSay(pBot, 2.0f, buf);
		return;
	}

}

void CommanderReceiveOrderRequest(bot_t* pBot, edict_t* Requestor)
{
	int PlayerIndex = GetPlayerIndex(Requestor);
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

			if (DefenderBot && (DefenderBot->SecondaryBotTask.TaskType == TASK_NONE || !DefenderBot->SecondaryBotTask.bTaskIsUrgent))
			{
				TASK_SetDefendTask(DefenderBot, &DefenderBot->SecondaryBotTask, AttackedStructure, true);
			}
		}
	}
}

void COMM_UpdateAndClearCommanderOrders(bot_t* CommanderBot)
{
	if (COMM_ClearCompletedOrders(CommanderBot)) { return; }

	int MaxMarines = GAME_GetNumActivePlayersOnTeam(MARINE_TEAM);

	const hive_definition* NearestEmptyHive = COMM_GetUnsecuredEmptyHiveNearestLocation(CommanderBot, UTIL_GetCommChairLocation());

	if (NearestEmptyHive != nullptr)
	{

		if (COMM_SecureHiveNeedsDeployment(CommanderBot, NearestEmptyHive))
		{
			int NumMarines = COMM_GetNumMarinesSecuringHive(CommanderBot, NearestEmptyHive, UTIL_MetresToGoldSrcUnits(15.0f));

				int MarineDeficit = 2 - NumMarines;

				if (MarineDeficit > 0)
				{
					edict_t* NearestMarine = COMM_GetNearestMarineWithoutOrder(CommanderBot, NearestEmptyHive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), UTIL_MetresToGoldSrcUnits(500.0f));

						if (!FNullEnt(NearestMarine))
						{
							COMM_IssueMarineSecureHiveOrder(CommanderBot, NearestMarine, NearestEmptyHive);
							return;
						}
				}
		}
	}

	if (UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH))
	{
		const hive_definition* SiegeHive = COMM_GetHiveSiegeOpportunityNearestLocation(CommanderBot, UTIL_GetCommChairLocation());

		if (SiegeHive)
		{
			int NumMarines = COMM_GetNumMarinesSiegingHive(CommanderBot, SiegeHive, UTIL_MetresToGoldSrcUnits(25.0f));

			int MarineDeficit = 2 - NumMarines;

			if (MarineDeficit > 0)
			{
				edict_t* NearestMarine = COMM_GetNearestMarineWithoutOrder(CommanderBot, SiegeHive->FloorLocation, UTIL_MetresToGoldSrcUnits(25.0f), UTIL_MetresToGoldSrcUnits(500.0f));

				if (!FNullEnt(NearestMarine))
				{
					Vector SiegePosition = COMM_GetGoodSiegeLocation(SiegeHive);
					if (SiegePosition != ZERO_VECTOR)
					{
						COMM_IssueMarineSiegeHiveOrder(CommanderBot, NearestMarine, SiegeHive, SiegePosition);
					}					
					return;
				}
			}
		}
	}

	const resource_node* ResNode = UTIL_FindEligibleResNodeClosestToLocation(UTIL_GetCommChairLocation(), MARINE_TEAM, false);

	if (ResNode != nullptr)
	{
		int NumMarines = COMM_GetNumMarinesSecuringResNode(CommanderBot, ResNode, UTIL_MetresToGoldSrcUnits(5.0f));

		int MarineDeficit = 1 - NumMarines;

		if (MarineDeficit > 0)
		{
			edict_t* NearestMarine = COMM_GetNearestMarineWithoutOrder(CommanderBot, ResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f), UTIL_MetresToGoldSrcUnits(30.0f));

			if (!FNullEnt(NearestMarine))
			{
				COMM_IssueMarineSecureResNodeOrder(CommanderBot, NearestMarine, ResNode);
				return;
			}
		}
	}

}

Vector COMM_GetGoodSiegeLocation(const hive_definition* HiveToSiege)
{
	edict_t* ExistingPG = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, HiveToSiege->FloorLocation, UTIL_MetresToGoldSrcUnits(25.0f), true, false);

	if (!FNullEnt(ExistingPG))
	{
		return ExistingPG->v.origin;
	}

	Vector Point = UTIL_GetRandomPointOnNavmeshInDonut(MARINE_REGULAR_NAV_PROFILE, HiveToSiege->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), UTIL_MetresToGoldSrcUnits(20.0f));

	if (Point != ZERO_VECTOR && !UTIL_QuickTrace(nullptr, Point + Vector(0.0f, 0.0f, 32.0f), HiveToSiege->Location))
	{
		return Point;
	}

	return ZERO_VECTOR;
}

int COMM_GetNumMarinesSiegingHive(bot_t* CommanderBot, const hive_definition* Hive, float MaxDistance)
{
	int Result = 0;

	float MaxDistSq = sqrf(MaxDistance);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (IsPlayerMarine(clients[i]) && IsPlayerActiveInGame(clients[i]))
		{
			if (vDist2DSq(clients[i]->v.origin, Hive->FloorLocation) < MaxDistSq)
			{
				Result++;
			}
			else
			{
				if (CommanderBot->LastPlayerOrders[i].bIsActive)
				{
					if (CommanderBot->LastPlayerOrders[i].OrderPurpose == ORDERPURPOSE_SIEGE_HIVE && CommanderBot->LastPlayerOrders[i].Target == Hive->edict)
					{
						Result++;
					}
				}
			}
		}
	}

	return Result;
}

int COMM_GetNumMarinesSecuringResNode(bot_t* CommanderBot, const resource_node* ResNode, float MaxDistance)
{
	int Result = 0;

	float MaxDistSq = sqrf(MaxDistance);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (IsPlayerMarine(clients[i]) && IsPlayerActiveInGame(clients[i]))
		{
			if (vDist2DSq(clients[i]->v.origin, ResNode->origin) < MaxDistSq)
			{
				Result++;
			}
			else
			{
				if (CommanderBot->LastPlayerOrders[i].bIsActive)
				{
					if (CommanderBot->LastPlayerOrders[i].OrderPurpose == ORDERPURPOSE_SECURE_RESNODE && CommanderBot->LastPlayerOrders[i].Target == ResNode->edict)
					{
						Result++;
					}
				}
			}
		}
	}

	return Result;
}

int COMM_GetNumMarinesSecuringHive(bot_t* CommanderBot, const hive_definition* Hive, float MaxDistance)
{
	int Result = 0;

	float MaxDistSq = sqrf(MaxDistance);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (IsPlayerMarine(clients[i]) && IsPlayerActiveInGame(clients[i]))
		{
			if (IsPlayerBot(clients[i]))
			{
				bot_t* BotRef = GetBotPointer(clients[i]);

				if (BotRef)
				{
					if (BotRef->CommanderTask.TaskType == TASK_SECURE_HIVE)
					{
						const hive_definition* TaskHive = UTIL_GetHiveFromEdict(BotRef->CommanderTask.TaskTarget);

						if (TaskHive == Hive)
						{
							Result++;
						}
					}
				}
			}
			else
			{
				if (vDist2DSq(clients[i]->v.origin, Hive->FloorLocation) < MaxDistSq)
				{
					Result++;
				}
				else
				{
					if (CommanderBot->LastPlayerOrders[i].bIsActive)
					{
						if (vDist2DSq(CommanderBot->LastPlayerOrders[i].MoveLocation, Hive->FloorLocation) <= MaxDistSq)
						{
							Result++;
						}
					}
				}
			}
			
		}
	}

	return Result;
}

edict_t* COMM_GetNearestMarineWithoutOrder(bot_t* CommanderBot, const Vector SearchLocation, float MinDistance, float MaxDistance)
{
	edict_t* Result = nullptr;
	float MinDist = 0.0f;
	float MinDistanceSq = sqrf(MinDistance);
	float MaxDistanceSq = sqrf(MaxDistance);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (FNullEnt(clients[i]) || !IsPlayerMarine(clients[i]) || !IsPlayerActiveInGame(clients[i])) { continue; }

		if (CommanderBot->LastPlayerOrders[i].bIsActive)
		{
			continue;
		}

		// Don't issue commands to bots acting as a sweeper or securing a hive, they have important stuff to do
		if (IsPlayerBot(clients[i]))
		{
			bot_t* BotRef = GetBotPointer(clients[i]);

			if (BotRef)
			{
				if (BotRef->CurrentRole == BOT_ROLE_SWEEPER) { continue; }

				if (BotRef->CommanderTask.TaskType == TASK_SECURE_HIVE) { continue; }
			}
		}

		float ThisDist = vDist2DSq(clients[i]->v.origin, SearchLocation);

		if (ThisDist < MinDistanceSq || ThisDist > MaxDistanceSq) { continue; }

		if (!Result || ThisDist < MinDist)
		{
			Result = clients[i];
			MinDist = ThisDist;
		}

	}

	return Result;
}

int COMM_GetNumMoveOrdersNearLocation(bot_t* CommanderBot, const Vector Location, const float MaxDistance)
{
	int Result = 0;
	float MaxDistSq = sqrf(MaxDistance);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (CommanderBot->LastPlayerOrders[i].bIsActive)
		{
			if (FNullEnt(clients[i]))
			{
				continue;
			}

			if (vDist2DSq(CommanderBot->LastPlayerOrders[i].MoveLocation, Location) <= MaxDistSq)
			{
				Result++;
			}
		}
	}

	return Result;
}

int COMM_GetNumMarinesAndOrdersInLocation(bot_t* CommanderBot, const Vector Location, const float SearchDist)
{
	int Result = 0;
	float MaxDistSq = sqrf(SearchDist);

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (CommanderBot->LastPlayerOrders[i].bIsActive)
		{
			if (FNullEnt(clients[i]) || clients[i]->v.team != CommanderBot->pEdict->v.team)
			{
				continue;
			}

			if (vDist2DSq(clients[i]->v.origin, Location) < MaxDistSq)
			{
				Result++;
			}
			else
			{
				if (vDist2DSq(CommanderBot->LastPlayerOrders[i].MoveLocation, Location) <= MaxDistSq)
				{
					Result++;
				}
			}

			
		}
	}

	return Result;
}

bool COMM_ClearCompletedOrders(bot_t* CommanderBot)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (CommanderBot->LastPlayerOrders[i].bIsActive)
		{
			if (FNullEnt(clients[i]))
			{
				UTIL_ClearCommanderOrder(CommanderBot, i);
				continue;
			}

			if (UTIL_IsMarineOrderComplete(CommanderBot, i))
			{
				bool bMessageSent = UTIL_ConfirmMarineOrderComplete(CommanderBot, i);

				// Only send one message per frame
				if (bMessageSent)
				{
					return true;
				}
			}
		}
	}

	return false;
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

bool ShouldCommanderLeaveChair(bot_t* pBot)
{
	int NumAliveMarinesInBase = UTIL_GetNumPlayersOfTeamInArea(UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(30.0f), pBot->pEdict->v.team, pBot->pEdict, CLASS_NONE, true);

	if (NumAliveMarinesInBase > 0) { return false; }

	int NumUnbuiltStructuresInBase = UTIL_GetNumUnbuiltStructuresOfTeamInArea(pBot->pEdict->v.team, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(20.0f));

	if (NumUnbuiltStructuresInBase == 0) { return false; }

	int NumInfantryPortals = UTIL_GetNumBuiltStructuresOfTypeInRadius(STRUCTURE_MARINE_INFANTRYPORTAL, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(10.0f));

	if (NumInfantryPortals == 0) { return true; }

	if (GAME_GetNumDeadPlayersOnTeam(pBot->pEdict->v.team) == 0) { return true; }

	return false;
}

void CommanderThink(bot_t* pBot)
{

	// Thanks to EterniumDev (Alien) for the suggestion to have the commander jump out and build if nobody is around to help
	if (ShouldCommanderLeaveChair(pBot))
	{
		if (IsPlayerCommander(pBot->pEdict))
		{
			BotStopCommanderMode(pBot);
			return;
		}

		edict_t* NearestUnbuiltStructure = UTIL_GetNearestUnbuiltStructureOfTeamInArea(UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f), MARINE_TEAM);

		if (!FNullEnt(NearestUnbuiltStructure))
		{
			TASK_SetBuildTask(pBot, &pBot->PrimaryBotTask, NearestUnbuiltStructure, false);
		}

		BotProgressTask(pBot, &pBot->PrimaryBotTask);

		return;
	}

	if (!IsPlayerCommander(pBot->pEdict))
	{
		BotProgressTakeCommandTask(pBot);
		return;
	}

	bool bCommanderOnCooldown = (gpGlobals->time < pBot->next_commander_action_time);

	if (bCommanderOnCooldown || COMM_IsWaitingOnBuildLink(pBot)) { return; }

	if ((gpGlobals->time - pBot->CommanderLastBeaconTime) > 10.0f && UTIL_ObservatoryResearchIsAvailable(RESEARCH_OBSERVATORY_DISTRESSBEACON) && UTIL_BaseIsInDistress())
	{
		if (pBot->ResearchAction.ActionType != ACTION_RESEARCH || pBot->ResearchAction.ResearchId != RESEARCH_OBSERVATORY_DISTRESSBEACON)
		{
			UTIL_ClearCommanderAction(&pBot->ResearchAction);

			edict_t* Observatory = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_OBSERVATORY);

			if (!FNullEnt(Observatory))
			{
				pBot->ResearchAction.ActionType = ACTION_RESEARCH;
				pBot->ResearchAction.ResearchId = RESEARCH_OBSERVATORY_DISTRESSBEACON;
				pBot->ResearchAction.ActionTarget = Observatory;
				BotTeamSay(pBot, 0.5f, "Going to beacon guys, base is in trouble");
			}		
		}

		pBot->CurrentAction = &pBot->ResearchAction;
	}
	else
	{
		COMM_UpdateAndClearCommanderActions(pBot);
		COMM_UpdateAndClearCommanderOrders(pBot);

		COMM_SetNextBuildAction(pBot, &pBot->BuildAction);

		COMM_SetNextSupportAction(pBot, &pBot->SupportAction);

		COMM_SetNextResearchAction(&pBot->ResearchAction);

		COMM_SetNextRecycleAction(pBot, &pBot->RecycleAction);

		pBot->CurrentAction = COMM_GetNextAction(pBot);
	}

	COMM_CommanderProgressAction(pBot, pBot->CurrentAction);


}

bool COMM_IsWaitingOnBuildLink(bot_t* CommanderBot)
{
	if (CommanderBot->BuildAction.bIsAwaitingBuildLink)
	{
		float TimeElapsed = gpGlobals->time - CommanderBot->BuildAction.StructureBuildAttemptTime;

		if (TimeElapsed > 0.5f)
		{
			CommanderBot->BuildAction.bIsAwaitingBuildLink = false;
			return false;
		}

		return true;
	}

	if (CommanderBot->SupportAction.bIsAwaitingBuildLink)
	{
		float TimeElapsed = gpGlobals->time - CommanderBot->SupportAction.StructureBuildAttemptTime;

		if (TimeElapsed > 0.5f)
		{
			CommanderBot->SupportAction.bIsAwaitingBuildLink = false;
			return false;
		}

		return true;
	}

	return false;
}

commander_action* COMM_GetNextAction(bot_t* CommanderBot)
{
	if (CommanderBot->BuildAction.bIsActionUrgent) { return &CommanderBot->BuildAction; }

	if (CommanderBot->RecycleAction.ActionType != ACTION_NONE) { return &CommanderBot->RecycleAction; }
	
	if (CommanderBot->ResearchAction.ActionType != ACTION_NONE) { return &CommanderBot->ResearchAction; }

	if (CommanderBot->SupportAction.ActionType != ACTION_NONE) { return &CommanderBot->SupportAction; }

	if (CommanderBot->BuildAction.ActionType != ACTION_NONE) { return &CommanderBot->BuildAction; }
	
	return nullptr;
}

void COMM_UpdateAndClearCommanderActions(bot_t* CommanderBot)
{
	if (!UTIL_IsCommanderActionValid(CommanderBot, &CommanderBot->BuildAction))
	{
		UTIL_ClearCommanderAction(&CommanderBot->BuildAction);
	}

	if (!UTIL_IsCommanderActionValid(CommanderBot, &CommanderBot->SupportAction))
	{
		UTIL_ClearCommanderAction(&CommanderBot->SupportAction);
	}

	if (!UTIL_IsCommanderActionValid(CommanderBot, &CommanderBot->ResearchAction))
	{
		UTIL_ClearCommanderAction(&CommanderBot->ResearchAction);
	}
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

bool UTIL_IsCommanderActionValid(bot_t* CommanderBot, commander_action* Action)
{
	if (Action->NumActionAttempts > 5) { return false; }

	if (Action->bIsAwaitingBuildLink) { return true; }

	switch (Action->ActionType)
	{
	case ACTION_RECYCLE:
		return !FNullEnt(Action->ActionTarget) && UTIL_IsMarineStructure(Action->ActionTarget) && !UTIL_StructureIsRecycling(Action->ActionTarget);
	case ACTION_UPGRADE:
		return !FNullEnt(Action->ActionTarget) && UTIL_StructureCanBeUpgraded(Action->ActionTarget);
	case ACTION_DEPLOY:
		return FNullEnt(Action->StructureOrItem);
	case ACTION_RESEARCH:
	{
		if (Action->ResearchId == RESEARCH_ELECTRICAL)
		{
			return UTIL_ElectricalResearchIsAvailable(Action->ActionTarget);
		}
		return (UTIL_MarineResearchIsAvailable(Action->ResearchId) && !FNullEnt(Action->ActionTarget));
	}
	case ACTION_GIVEORDER:
		return Action->AssignedPlayer > -1;
	default:
		return false;
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
		if (Action->StructureOrItem->v.deadflag == DEAD_DEAD || UTIL_StructureIsFullyBuilt(Action->StructureOrItem))
		{
			return false;
		}

		buildable_structure* StructureRef = UTIL_GetBuildableStructureRefFromEdict(Action->StructureOrItem);

		if (StructureRef)
		{
			if (!StructureRef->bIsReachableMarine) { return false; }
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


bool UTIL_CancelCommanderPlayerOrder(bot_t* Commander, int PlayerIndex)
{
	UTIL_ClearCommanderOrder(Commander, PlayerIndex);
	return false;
}

void UTIL_ClearCommanderAction(commander_action* Action)
{
	memset(Action, 0, sizeof(commander_action));
	Action->AssignedPlayer = -1;
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

void BotCommanderResearchTech(bot_t* pBot, commander_action* Action)
{
	if (Action->NumActionAttempts > 5)
	{
		UTIL_ClearCommanderAction(Action);
		return;
	}

	buildable_structure* BuildRef = UTIL_GetBuildableStructureRefFromEdict(Action->ActionTarget);

	// If we are already selecting the building then we can just send the impulse and finish up
	if (pBot->CommanderCurrentlySelectedBuilding == Action->ActionTarget)
	{
		if (BuildRef)
		{
			if (Action->LastAttemptedCommanderLocation != ZERO_VECTOR)
			{

				BuildRef->LastSuccessfulCommanderLocation = Action->LastAttemptedCommanderLocation;
			}

			if (Action->LastAttemptedCommanderAngle != ZERO_VECTOR)
			{
				BuildRef->LastSuccessfulCommanderAngle = Action->LastAttemptedCommanderAngle;
			}
		}

		pBot->impulse = Action->ResearchId;

		if (Action->ResearchId == RESEARCH_OBSERVATORY_DISTRESSBEACON)
		{
			pBot->CommanderLastBeaconTime = gpGlobals->time;
		}

		Action->ActionStep = ACTION_STEP_NONE;

		pBot->next_commander_action_time = gpGlobals->time + commander_action_cooldown;

		return;
	}

	BotCommanderSelectStructure(pBot, Action->ActionTarget, Action);
}

void BotCommanderUpgradeStructure(bot_t* pBot, commander_action* Action)
{
	if (Action->NumActionAttempts > 5)
	{
		UTIL_ClearCommanderAction(Action);
		return;
	}

	buildable_structure* BuildRef = UTIL_GetBuildableStructureRefFromEdict(Action->ActionTarget);

	// If we are already selecting the building then we can just send the impulse and finish up
	if (pBot->CommanderCurrentlySelectedBuilding == Action->ActionTarget)
	{
		if (BuildRef)
		{
			if (Action->LastAttemptedCommanderLocation != ZERO_VECTOR)
			{
				BuildRef->LastSuccessfulCommanderLocation = Action->LastAttemptedCommanderLocation;
			}

			if (Action->LastAttemptedCommanderAngle != ZERO_VECTOR)
			{
				BuildRef->LastSuccessfulCommanderAngle = Action->LastAttemptedCommanderAngle;
			}
		}


		NSStructureType StructureType = UTIL_IUSER3ToStructureType(Action->ActionTarget->v.iuser3);

		switch (StructureType)
		{
		case STRUCTURE_MARINE_ARMOURY:
			pBot->impulse = IMPULSE_COMMANDER_UPGRADE_ARMOURY;
			break;
		case STRUCTURE_MARINE_TURRETFACTORY:
			pBot->impulse = IMPULSE_COMMANDER_UPGRADE_TURRETFACTORY;
			break;
		}

		Action->ActionStep = ACTION_STEP_NONE;

		pBot->next_commander_action_time = gpGlobals->time + commander_action_cooldown;
		return;
	}

	BotCommanderSelectStructure(pBot, Action->ActionTarget, Action);

}

void BotCommanderSelectStructure(bot_t* pBot, const edict_t* Structure, commander_action* Action)
{
	buildable_structure* BuildRef = UTIL_GetBuildableStructureRefFromEdict(Structure);

	if (!BuildRef) { return; }

	if (pBot->CommanderCurrentlySelectedBuilding == Structure) { return; }

	Vector CommanderViewOrigin = pBot->pEdict->v.origin;
	CommanderViewOrigin.z = GetCommanderViewZHeight();

	if (Action->bHasAttemptedAction)
	{
		if (BuildRef)
		{
			BuildRef->LastSuccessfulCommanderLocation = ZERO_VECTOR;
			BuildRef->LastSuccessfulCommanderAngle = ZERO_VECTOR;
		}
		Action->DesiredCommanderLocation = UTIL_RandomPointOnCircle(UTIL_GetCentreOfEntity(Structure), UTIL_MetresToGoldSrcUnits(5.0f));
	}
	else
	{
		if (BuildRef->LastSuccessfulCommanderLocation != ZERO_VECTOR)
		{
			Action->DesiredCommanderLocation = BuildRef->LastSuccessfulCommanderLocation;
		}
		else
		{
			if (Action->DesiredCommanderLocation == ZERO_VECTOR || vDist2DSq(Action->DesiredCommanderLocation, Structure->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
			{
				Action->DesiredCommanderLocation = UTIL_FindClearCommanderOriginForBuild(pBot, UTIL_GetCentreOfEntity(Structure), GetCommanderViewZHeight());
			}
		}
	}

	if (vDist2DSq(CommanderViewOrigin, Action->DesiredCommanderLocation) > sqrf(8.0f))
	{
		Action->bHasAttemptedAction = false;
		pBot->UpMove = Action->DesiredCommanderLocation.x / kWorldPosNetworkConstant;
		pBot->SideMove = Action->DesiredCommanderLocation.y / kWorldPosNetworkConstant;
		pBot->ForwardMove = 0.0f;

		pBot->impulse = IMPULSE_COMMANDER_MOVETO;

		Action->ActionStep = ACTION_STEP_NONE;

		pBot->next_commander_action_time = gpGlobals->time + 0.1f;

		return;
	}

	if (Action->ActionStep == ACTION_STEP_NONE)
	{
		Action->ActionStep = ACTION_STEP_BEGIN_SELECT;
	}

	if (Action->ActionStep == ACTION_STEP_BEGIN_SELECT)
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

		Action->ActionStep = ACTION_STEP_END_SELECT;
		return;
	}

	if (Action->ActionStep == ACTION_STEP_END_SELECT)
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

		Action->LastAttemptedCommanderLocation = CommanderViewOrigin;
		Action->LastAttemptedCommanderAngle = Vector(pBot->UpMove, pBot->SideMove, pBot->ForwardMove);

		pBot->impulse = IMPULSE_COMMANDER_MOUSECOORD;

		pBot->pEdict->v.button = 0;

		Action->bHasAttemptedAction = true;
		Action->NumActionAttempts++;
		pBot->next_commander_action_time = gpGlobals->time + 0.1f;

		return;
	}

	return;
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

	if (UTIL_GetNumBuiltStructuresOfType(STRUCTURE_MARINE_TURRETFACTORY) == 0) { return false; }

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

void BotCommanderDeploy(bot_t* pBot, commander_action* Action)
{

	if (Action->bIsAwaitingBuildLink) { return; }

	if (Action->NumActionAttempts > 5)
	{
		UTIL_ClearCommanderAction(Action);
		return;
	}

	if (Action->StructureToBuild == STRUCTURE_NONE) { return; }

	int DeployCost = UTIL_GetCostOfStructureType(Action->StructureToBuild);

	if (GetPlayerResources(pBot->pEdict) < DeployCost) { return; }

	if (Action->StructureToBuild == DEPLOYABLE_ITEM_MARINE_SCAN)
	{
		if ((gpGlobals->time - pBot->CommanderLastScanTime) < 10.0f) { return; }

		if (GetStructureTypeFromEdict(pBot->CommanderCurrentlySelectedBuilding) != STRUCTURE_MARINE_OBSERVATORY)
		{
			BotCommanderSelectStructure(pBot, UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_OBSERVATORY, Action->BuildLocation, UTIL_MetresToGoldSrcUnits(1000.0f), true, false), Action);
			return;
		}
	}

	pBot->pEdict->v.v_angle = ZERO_VECTOR;

	Vector CommanderViewOrigin = pBot->pEdict->v.origin;
	CommanderViewOrigin.z = GetCommanderViewZHeight();

	if (Action->DesiredCommanderLocation == ZERO_VECTOR)
	{

		Action->DesiredCommanderLocation = UTIL_FindClearCommanderOriginForBuild(pBot, Action->BuildLocation, GetCommanderViewZHeight());

		if (Action->DesiredCommanderLocation == ZERO_VECTOR)
		{
			Action->NumActionAttempts++;
			return;
		}
	}

	if (Action->bHasAttemptedAction)
	{
		Action->DesiredCommanderLocation = UTIL_RandomPointOnCircle(Action->BuildLocation, UTIL_MetresToGoldSrcUnits(2.0f));
	}

	if (vDist2DSq(CommanderViewOrigin, Action->DesiredCommanderLocation) > sqrf(16.0f))
	{
		Action->bHasAttemptedAction = false;
		pBot->UpMove = Action->DesiredCommanderLocation.x / kWorldPosNetworkConstant;
		pBot->SideMove = Action->DesiredCommanderLocation.y / kWorldPosNetworkConstant;
		pBot->ForwardMove = 0.0f;

		pBot->impulse = IMPULSE_COMMANDER_MOVETO;

		pBot->next_commander_action_time = gpGlobals->time + 0.1f;

		return;
	}

	Vector PickRay = UTIL_GetVectorNormal(Action->BuildLocation - CommanderViewOrigin);

	pBot->UpMove = PickRay.x * kSelectionNetworkConstant;
	pBot->SideMove = PickRay.y * kSelectionNetworkConstant;
	pBot->ForwardMove = PickRay.z * kSelectionNetworkConstant;

	pBot->impulse = (byte)Action->StructureToBuild;

	pBot->pEdict->v.button |= IN_ATTACK;

	Action->NumActionAttempts++;
	Action->bHasAttemptedAction = true;
	Action->bIsAwaitingBuildLink = true;
	Action->StructureBuildAttemptTime = gpGlobals->time;
	Action->LastAttemptedCommanderAngle = Vector(pBot->UpMove, pBot->SideMove, pBot->ForwardMove);
	Action->LastAttemptedCommanderLocation = CommanderViewOrigin;

	return;
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

	if (UTIL_CommanderTrace(Commander->pEdict, DirectlyAboveLocation, DesiredBuildLocation))
	{
		return DirectlyAboveLocation;
	}

	while (TryNum < NumTries)
	{
		Vector TestLocation = UTIL_RandomPointOnCircle(DesiredBuildLocation, UTIL_MetresToGoldSrcUnits(3.0f));
		TestLocation.z = CommanderViewZ - 8.0f;

		if (UTIL_CommanderTrace(Commander->pEdict, TestLocation, DesiredBuildLocation))
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
	edict_t* Player = clients[PlayerIndex];

	if (FNullEnt(Player) || !IsPlayerActiveInGame(Player)) { return true; }

	bool bOrderComplete = false;
	AvHOrderType OrderType = CommanderBot->LastPlayerOrders[PlayerIndex].OrderType;
	CommanderOrderPurpose OrderPurpose = CommanderBot->LastPlayerOrders[PlayerIndex].OrderPurpose;

	switch (OrderType)
	{
	case ORDERTYPEL_MOVE:
		bOrderComplete = (CommanderBot->LastPlayerOrders[PlayerIndex].MoveLocation == ZERO_VECTOR || vDist3DSq(Player->v.origin, CommanderBot->LastPlayerOrders[PlayerIndex].MoveLocation) < sqrf(UTIL_MetresToGoldSrcUnits(move_order_success_dist_metres)));
		break;
	case ORDERTYPET_BUILD:
		bOrderComplete = (FNullEnt(CommanderBot->LastPlayerOrders[PlayerIndex].Target) || UTIL_StructureIsFullyBuilt(CommanderBot->LastPlayerOrders[PlayerIndex].Target));
		break;
	default:
		bOrderComplete = false;
		break;
	}

	if (bOrderComplete) { return true; }

	switch (OrderPurpose)
	{
		case ORDERPURPOSE_SECURE_HIVE:
			return COMM_IsSecureHiveOrderComplete(CommanderBot, &CommanderBot->LastPlayerOrders[PlayerIndex]);
		case ORDERPURPOSE_SIEGE_HIVE:
			return COMM_IsSiegeHiveOrderComplete(&CommanderBot->LastPlayerOrders[PlayerIndex]);
		case ORDERPURPOSE_SECURE_RESNODE:
			return COMM_IsSecureResNodeOrderComplete(&CommanderBot->LastPlayerOrders[PlayerIndex]);
		default:
			return false;

	}

	return true;
}

bool COMM_IsSecureHiveOrderComplete(bot_t* CommanderBot, commander_order* Order)
{
	const hive_definition* HiveToSecure = UTIL_GetNearestHiveAtLocation(Order->Target->v.origin);

	if (!HiveToSecure) { return true; }

	return UTIL_IsHiveFullySecuredByMarines(CommanderBot, HiveToSecure);
}

bool COMM_IsSiegeHiveOrderComplete(commander_order* Order)
{
	const hive_definition* HiveToSiege = UTIL_GetNearestHiveAtLocation(Order->Target->v.origin);

	if (!HiveToSiege) { return true; }

	return HiveToSiege->Status == HIVE_STATUS_UNBUILT;
}

bool COMM_IsSecureResNodeOrderComplete(commander_order* Order)
{
	const resource_node* ResNode = UTIL_GetResourceNodeFromEdict(Order->Target);

	if (!ResNode) { return true; }

	return ResNode->bIsOccupied && ResNode->bIsOwnedByMarines;
}

bool UTIL_ConfirmMarineOrderComplete(bot_t* CommanderBot, int CommanderOrderIndex)
{
	edict_t* Player = clients[CommanderOrderIndex];
	edict_t* OrderTarget = CommanderBot->LastPlayerOrders[CommanderOrderIndex].Target;
	AvHOrderType OrderType = CommanderBot->LastPlayerOrders[CommanderOrderIndex].OrderType;
	Vector OrderLocation = CommanderBot->LastPlayerOrders[CommanderOrderIndex].MoveLocation;

	if (FNullEnt(Player)) { return false; }

	bool bIsBuildOrder = (OrderType == ORDERTYPET_BUILD && !FNullEnt(OrderTarget));

	MESSAGE_BEGIN(MSG_ONE, REG_USER_MSG("SetOrder", -1), NULL, Player);
	WRITE_BYTE(ENTINDEX(Player));
	WRITE_BYTE(OrderType);

	if (bIsBuildOrder)
	{
		WRITE_SHORT(ENTINDEX(OrderTarget));
	}
	else
	{
		WRITE_COORD(OrderLocation.x);
		WRITE_COORD(OrderLocation.y);
		WRITE_COORD(OrderLocation.z);
	}

	if (bIsBuildOrder)
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

	return true;
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

bool UTIL_ItemCanBeDeployed(NSStructureType ItemToDeploy)
{
	switch (ItemToDeploy)
	{
	case DEPLOYABLE_ITEM_MARINE_AMMO:
	case DEPLOYABLE_ITEM_MARINE_HEALTHPACK:
		return true;
	case DEPLOYABLE_ITEM_MARINE_MINES:
	case DEPLOYABLE_ITEM_MARINE_WELDER:
	case DEPLOYABLE_ITEM_MARINE_SHOTGUN:
		return (UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_ANYARMOURY) != nullptr);
	case DEPLOYABLE_ITEM_MARINE_GRENADELAUNCHER:
	case DEPLOYABLE_ITEM_MARINE_HMG:
		return (UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_ADVARMOURY) != nullptr);
	case DEPLOYABLE_ITEM_MARINE_HEAVYARMOUR:
		return UTIL_ResearchIsComplete(RESEARCH_PROTOTYPELAB_HEAVYARMOUR);
	case DEPLOYABLE_ITEM_MARINE_JETPACK:
		return UTIL_ResearchIsComplete(RESEARCH_PROTOTYPELAB_JETPACKS);
	case DEPLOYABLE_ITEM_MARINE_SCAN:
		return UTIL_ObservatoryResearchIsAvailable(RESEARCH_OBSERVATORY_SCAN);
	default:
		return false;
	}

	return false;
}



void CommanderGetPrimaryTask(bot_t* pBot, bot_task* Task)
{

}

const resource_node* COMM_GetResNodeCapOpportunityNearestLocation(const Vector SearchLocation)
{
	const resource_node* Result = nullptr;
	float MinDist = 0.0f;

	for (int i = 0; i < UTIL_GetNumResNodes(); i++)
	{
		const resource_node* ResNode = UTIL_GetResourceNodeAtIndex(i);

		if (ResNode->bIsOccupied) { continue; }

		if (!UTIL_AnyPlayerOnTeamWithLOS(ResNode->origin + Vector(0.0f, 0.0f, 32.0f), MARINE_TEAM, UTIL_MetresToGoldSrcUnits(5.0f))) { continue; }

		float ThisDist = vDist2DSq(ResNode->origin, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = ResNode;
			MinDist = ThisDist;
		}

	}

	return Result;
}

const hive_definition* COMM_GetUnsecuredEmptyHiveFurthestToLocation(bot_t* CommanderBot, const Vector SearchLocation)
{
	const hive_definition* Result = nullptr;
	float MaxDist = 0.0f;

	for (int i = 0; i < UTIL_GetNumTotalHives(); i++)
	{
		const hive_definition* Hive = UTIL_GetHiveAtIndex(i);

		if (Hive->Status != HIVE_STATUS_UNBUILT) { continue; }

		if (UTIL_IsHiveFullySecuredByMarines(CommanderBot, Hive)) { continue; }

		float ThisDist = vDist2DSq(Hive->FloorLocation, SearchLocation);

		if (!Result || ThisDist > MaxDist)
		{
			Result = Hive;
			MaxDist = ThisDist;
		}

	}

	return Result;
}

const hive_definition* COMM_GetUnsecuredEmptyHiveNearestLocation(bot_t* CommanderBot, const Vector SearchLocation)
{
	const hive_definition* Result = nullptr;
	float MinDist = 0.0f;

	for (int i = 0; i < UTIL_GetNumTotalHives(); i++)
	{
		const hive_definition* Hive = UTIL_GetHiveAtIndex(i);

		if (Hive->Status != HIVE_STATUS_UNBUILT) { continue; }

		if (UTIL_IsHiveFullySecuredByMarines(CommanderBot, Hive)) { continue; }

		float ThisDist = vDist2DSq(Hive->FloorLocation, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = Hive;
			MinDist = ThisDist;
		}

	}

	return Result;
}

const hive_definition* COMM_GetEmptyHiveOpportunityNearestLocation(bot_t* CommanderBot, const Vector SearchLocation)
{
	const hive_definition* Result = nullptr;
	float MinDist = 0.0f;

	for (int i = 0; i < UTIL_GetNumTotalHives(); i++)
	{
		const hive_definition* Hive = UTIL_GetHiveAtIndex(i);

		if (Hive->Status != HIVE_STATUS_UNBUILT) { continue; }

		if (UTIL_IsHiveFullySecuredByMarines(CommanderBot, Hive)) { continue; }

		if (UTIL_FindSafePlayerInArea(MARINE_TEAM, Hive->Location, 0.0f, UTIL_MetresToGoldSrcUnits(10.0f)) == nullptr) { continue; }

		if (!UTIL_AnyPlayerOnTeamWithLOS(Hive->edict->v.origin, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(10.0f))) 
		{
			edict_t* PG = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f), true, false);

			bool bCanSeePG = (!FNullEnt(PG) && UTIL_AnyPlayerOnTeamWithLOS(UTIL_GetCentreOfEntity(PG), MARINE_TEAM, UTIL_MetresToGoldSrcUnits(10.0f)));

			if (!bCanSeePG)
			{
				edict_t* TF = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f), true, false);

				bool bNeedsElectrifying = false;

				if (!FNullEnt(TF))
				{
					bNeedsElectrifying = (UTIL_StructureIsFullyBuilt(TF) && !UTIL_IsStructureElectrified(TF) && UTIL_GetNumBuiltStructuresOfTypeInRadius(STRUCTURE_MARINE_TURRET, TF->v.origin, UTIL_MetresToGoldSrcUnits(10.0f)) > 0);
				}

				bool bCanSeeTF = (!FNullEnt(TF) && UTIL_AnyPlayerOnTeamWithLOS(UTIL_GetCentreOfEntity(TF), MARINE_TEAM, UTIL_MetresToGoldSrcUnits(10.0f)));

				if (!bNeedsElectrifying && !bCanSeePG && !bCanSeeTF) { continue; }
			}

		}

		float ThisDist = vDist2DSq(Hive->FloorLocation, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = Hive;
			MinDist = ThisDist;
		}

	}

	return Result;
}

const hive_definition* COMM_GetHiveSiegeOpportunityNearestLocation(bot_t* CommanderBot, const Vector SearchLocation)
{
	bool bPhaseGatesAvailable = UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH);

	if (!bPhaseGatesAvailable) { return nullptr; }

	const hive_definition* Result = nullptr;
	float MinDist = 0.0f;

	for (int i = 0; i < UTIL_GetNumTotalHives(); i++)
	{
		const hive_definition* Hive = UTIL_GetHiveAtIndex(i);

		if (Hive->Status == HIVE_STATUS_UNBUILT) { continue; }

		edict_t* BuiltSiegeTurret = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_SIEGETURRET, Hive->Location, UTIL_MetresToGoldSrcUnits(20.0f), true, false);

		if (!FNullEnt(BuiltSiegeTurret) && UTIL_StructureIsFullyBuilt(BuiltSiegeTurret) && UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_ADVTURRETFACTORY, BuiltSiegeTurret->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			if (gpGlobals->time - CommanderBot->CommanderLastScanTime > 10.0f)
			{
				return Hive;
			}
		}

		if (COMM_GetMarineEligibleToBuildSiege(Hive) == nullptr) { continue; }

		edict_t* BuiltPhaseGate = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, Hive->Location, UTIL_MetresToGoldSrcUnits(20.0f), true, false);
		

		if (!FNullEnt(BuiltPhaseGate) && UTIL_StructureIsFullyBuilt(BuiltPhaseGate))
		{
			if (!UTIL_IsPlayerOfTeamInArea(BuiltPhaseGate->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), MARINE_TEAM, nullptr, CLASS_NONE)) { continue; }
		}

		float ThisDist = vDist2DSq(Hive->FloorLocation, SearchLocation);

		if (!Result || ThisDist < MinDist)
		{
			Result = Hive;
			MinDist = ThisDist;
		}

	}

	return Result;
}

void COMM_SetTurretBuildAction(edict_t* TurretFactory, commander_action* Action)
{
	if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_TURRET && Action->ActionTarget == TurretFactory && Action->BuildLocation != ZERO_VECTOR) { return; }

	UTIL_ClearCommanderAction(Action);

	if (FNullEnt(TurretFactory) || !UTIL_StructureIsFullyBuilt(TurretFactory)) { return; }

	Vector BuildLocation = UTIL_GetNextTurretPosition(TurretFactory);

	if (!BuildLocation) { return; }

	Action->ActionType = ACTION_DEPLOY;
	Action->ActionTarget = TurretFactory;
	Action->BuildLocation = BuildLocation;
	Action->StructureToBuild = STRUCTURE_MARINE_TURRET;
	Action->bIsActionUrgent = true;
	Action->ActionPurpose = STRUCTURE_PURPOSE_FORTIFY;

}

void COMM_SetSiegeTurretBuildAction(edict_t* TurretFactory, commander_action* Action, const Vector SiegeTarget, bool bIsUrgent)
{
	if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_SIEGETURRET && Action->ActionTarget == TurretFactory && Action->BuildLocation != ZERO_VECTOR) { return; }

	UTIL_ClearCommanderAction(Action);

	if (FNullEnt(TurretFactory) || !UTIL_StructureIsFullyBuilt(TurretFactory) || GetStructureTypeFromEdict(TurretFactory) != STRUCTURE_MARINE_ADVTURRETFACTORY) { return; }

	Vector BuildLocation = UTIL_GetNextTurretPosition(TurretFactory);

	if (!BuildLocation || vDist2DSq(BuildLocation, SiegeTarget) > sqrf(kSiegeTurretRange)) { return; }

	Action->ActionType = ACTION_DEPLOY;
	Action->ActionTarget = TurretFactory;
	Action->BuildLocation = BuildLocation;
	Action->StructureToBuild = STRUCTURE_MARINE_SIEGETURRET;
	Action->bIsActionUrgent = bIsUrgent;
	Action->ActionPurpose = STRUCTURE_PURPOSE_SIEGE;

}

void COMM_SetInfantryPortalBuildAction(edict_t* CommChair, commander_action* Action)
{
	if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_INFANTRYPORTAL && Action->ActionTarget == CommChair && Action->BuildLocation != ZERO_VECTOR) { return; }

	UTIL_ClearCommanderAction(Action);

	if (FNullEnt(CommChair) || !UTIL_StructureIsFullyBuilt(CommChair)) { return; }

	Vector BuildLocation = ZERO_VECTOR;

	edict_t* ExistingInfantryPortal = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_INFANTRYPORTAL, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), true, false);

	// First see if we can place the next infantry portal next to the first one
	if (!FNullEnt(ExistingInfantryPortal))
	{
		BuildLocation = UTIL_GetRandomPointOnNavmeshInDonut(GORGE_BUILD_NAV_PROFILE, ExistingInfantryPortal->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), UTIL_MetresToGoldSrcUnits(3.0f));
	}

	if (!BuildLocation)
	{
		Vector SearchPoint = ZERO_VECTOR;

		const resource_node* ResNode = UTIL_FindNearestResNodeToLocation(CommChair->v.origin);

		if (ResNode)
		{
			SearchPoint = ResNode->origin;
		}
		else
		{
			SearchPoint = UTIL_GetRandomPointOfInterest();
		}

		Vector NearestPointToChair = FindClosestNavigablePointToDestination(MARINE_REGULAR_NAV_PROFILE, SearchPoint, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NearestPointToChair != ZERO_VECTOR)
		{
			float Distance = vDist2D(NearestPointToChair, CommChair->v.origin);
			float RandomDist = UTIL_MetresToGoldSrcUnits(5.0f) - Distance;

			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, NearestPointToChair, RandomDist);

		}
		else
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
		}
	}

	if (!BuildLocation) { return; }

	Action->ActionType = ACTION_DEPLOY;
	Action->ActionTarget = CommChair;
	Action->BuildLocation = BuildLocation;
	Action->StructureToBuild = STRUCTURE_MARINE_INFANTRYPORTAL;
	Action->bIsActionUrgent = true;

}

Vector UTIL_GetNextTurretPosition(edict_t* TurretFactory)
{
	if (FNullEnt(TurretFactory) || !UTIL_StructureIsFullyBuilt(TurretFactory)) { return ZERO_VECTOR; }

	Vector FwdVector = UTIL_GetForwardVector2D(TurretFactory->v.angles);
	Vector RightVector = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(FwdVector, UP_VECTOR));

	bool bFwd = false;
	bool bRight = false;
	bool bBack = false;
	bool bLeft = false;

	int NumTurrets = 0;

	for (auto& it : MarineBuildableStructureMap)
	{
		if (!it.second.bOnNavmesh || !it.second.bIsReachableMarine) { continue; }
		if (!UTIL_StructureTypesMatch(STRUCTURE_MARINE_TURRET, it.second.StructureType)) { continue; }

		if (vDist2DSq(TurretFactory->v.origin, it.second.Location) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f))) { continue; }

		NumTurrets++;

		Vector Dir = UTIL_GetVectorNormal2D(it.second.Location - TurretFactory->v.origin);

		if (UTIL_GetDotProduct2D(FwdVector, Dir) > 0.7f)
		{
			bFwd = true;
		}

		if (UTIL_GetDotProduct2D(FwdVector, Dir) < -0.7f)
		{
			bBack = true;
		}

		if (UTIL_GetDotProduct2D(RightVector, Dir) > 0.7f)
		{
			bRight = true;
		}

		if (UTIL_GetDotProduct2D(RightVector, Dir) < -0.7f)
		{
			bLeft = true;
		}

	}

	if (NumTurrets >= 5) { return ZERO_VECTOR; }

	if (!bFwd)
	{
		Vector SearchLocation = TurretFactory->v.origin + (FwdVector * UTIL_MetresToGoldSrcUnits(3.0f));

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, SearchLocation, UTIL_MetresToGoldSrcUnits(1.0f));

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	if (!bBack)
	{
		Vector SearchLocation = TurretFactory->v.origin - (FwdVector * UTIL_MetresToGoldSrcUnits(3.0f));

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, SearchLocation, UTIL_MetresToGoldSrcUnits(1.0f));

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	if (!bRight)
	{
		Vector SearchLocation = TurretFactory->v.origin + (RightVector * UTIL_MetresToGoldSrcUnits(3.0f));

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, SearchLocation, UTIL_MetresToGoldSrcUnits(1.0f));

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	if (!bLeft)
	{
		Vector SearchLocation = TurretFactory->v.origin - (RightVector * UTIL_MetresToGoldSrcUnits(3.0f));

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, SearchLocation, UTIL_MetresToGoldSrcUnits(1.0f));

		if (BuildLocation != ZERO_VECTOR)
		{
			return BuildLocation;
		}
	}

	Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, TurretFactory->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

	return BuildLocation;
}

bool COMM_SecureHiveNeedsDeployment(bot_t* CommanderBot, const hive_definition* Hive)
{
	edict_t* TF = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

	if (FNullEnt(TF) || !UTIL_StructureIsFullyBuilt(TF)) { return true; }

	bool bPhaseGatesAvailable = UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH);

	if (bPhaseGatesAvailable)
	{
		edict_t* PhaseGate = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

		if (FNullEnt(PhaseGate) || !UTIL_StructureIsFullyBuilt(PhaseGate)) { return true; }
	}

	int NumTurrets = UTIL_GetNumBuiltStructuresOfTypeInRadius(STRUCTURE_MARINE_TURRET, TF->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

	if (NumTurrets < 5) { return true; }

	const resource_node* ResNode = UTIL_GetResourceNodeAtIndex(Hive->HiveResNodeIndex);

	if (ResNode && (!ResNode->bIsOccupied || !ResNode->bIsOwnedByMarines))
	{
		return true;
	}

	return false;

}

void COMM_SetNextSecureHiveAction(bot_t* CommanderBot, const hive_definition* Hive, commander_action* Action)
{	

	edict_t* TF = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

	Vector PlayerSearchLoc = (FNullEnt(TF)) ? Hive->FloorLocation : TF->v.origin;

	edict_t* NearestPlayer = UTIL_GetNearestPlayerOfTeamInArea(PlayerSearchLoc, UTIL_MetresToGoldSrcUnits(10.0f), MARINE_TEAM, nullptr, CLASS_NONE);

	// If there isn't a player around to build stuff we can still electrify!
	if (FNullEnt(NearestPlayer))
	{
		if (!FNullEnt(TF) && UTIL_StructureIsFullyBuilt(TF) && !UTIL_IsStructureElectrified(TF) && !UTIL_StructureIsResearching(TF))
		{
			int NumTurrets = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_TURRET, TF->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

			if (NumTurrets > 0)
			{
				COMM_SetElectrifyStructureAction(TF, Action);
				return;
			}
		}

		if (CommanderBot->resources <= 100) { return; }

		const resource_node* ResNode = UTIL_GetResourceNodeAtIndex(Hive->HiveResNodeIndex);

		if (ResNode && ResNode->bIsOwnedByMarines)
		{
			if (UTIL_StructureIsFullyBuilt(ResNode->TowerEdict) && !UTIL_IsStructureElectrified(ResNode->TowerEdict) && !UTIL_StructureIsResearching(ResNode->TowerEdict))
			{
				COMM_SetElectrifyStructureAction(ResNode->TowerEdict, Action);
				return;
			}
		}

		return;
	}

	bool bPhaseGatesAvailable = UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH);

	edict_t* PhaseGate = (bPhaseGatesAvailable) ? UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), true, false) : nullptr;

	if (bPhaseGatesAvailable && FNullEnt(PhaseGate))
	{
		
		if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_PHASEGATE && Action->ActionTarget == Hive->edict)
		{
			return;
		}

		Vector BuildLocation = ZERO_VECTOR;
		Vector SearchLoc = (FNullEnt(TF)) ? Hive->Location : TF->v.origin;
		float SearchRadius = (FNullEnt(TF)) ? UTIL_MetresToGoldSrcUnits(15.0f) : UTIL_MetresToGoldSrcUnits(5.0f);
			
		edict_t* NearestAppropriatePlayer = UTIL_GetClosestPlayerOnTeamWithLOS(SearchLoc, MARINE_TEAM, SearchRadius, nullptr);

		if (!FNullEnt(NearestAppropriatePlayer))
		{
			BuildLocation = NearestAppropriatePlayer->v.origin;
		}
		else
		{
			BuildLocation = FindClosestNavigablePointToDestination(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));
		}

		Vector AdjustedLocation = ZERO_VECTOR;
		Vector FinalBuildLocation = ZERO_VECTOR;

		if (BuildLocation != ZERO_VECTOR)
		{
			float Dist = vDist2D(BuildLocation, Hive->FloorLocation);
			float MaxDist = fmaxf(UTIL_MetresToGoldSrcUnits(1.0f), UTIL_MetresToGoldSrcUnits(10.0f) - Dist);

			AdjustedLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, BuildLocation, MaxDist);
		}

		AdjustedLocation = (AdjustedLocation != ZERO_VECTOR) ? AdjustedLocation : BuildLocation;

		FinalBuildLocation = UTIL_ProjectPointToNavmesh(AdjustedLocation, BUILDING_REGULAR_NAV_PROFILE);

		FinalBuildLocation = (FinalBuildLocation != ZERO_VECTOR) ? FinalBuildLocation : AdjustedLocation;
			
		if (FinalBuildLocation != ZERO_VECTOR)
		{
			UTIL_ClearCommanderAction(Action);

			Action->ActionType = ACTION_DEPLOY;
			Action->ActionTarget = Hive->edict;
			Action->StructureToBuild = STRUCTURE_MARINE_PHASEGATE;
			Action->BuildLocation = FinalBuildLocation;
			Action->bIsActionUrgent = true;
			Action->ActionPurpose = STRUCTURE_PURPOSE_FORTIFY;
			return;
		}
	}

	if (FNullEnt(TF))
	{
		if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_TURRETFACTORY && Action->ActionTarget == Hive->edict)
		{
			return;
		}

		Vector BuildLocation = ZERO_VECTOR;

		Vector SearchLoc = (FNullEnt(PhaseGate)) ? Hive->Location : PhaseGate->v.origin;
		float SearchRadius = (FNullEnt(PhaseGate)) ? UTIL_MetresToGoldSrcUnits(15.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

		edict_t* NearestAppropriatePlayer = UTIL_GetClosestPlayerOnTeamWithLOS(SearchLoc, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(15.0f), nullptr);

		if (!FNullEnt(NearestAppropriatePlayer))
		{
			BuildLocation = NearestAppropriatePlayer->v.origin;
		}
		else
		{
			BuildLocation = FindClosestNavigablePointToDestination(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));
		}

		Vector AdjustedLocation = ZERO_VECTOR;
		Vector FinalBuildLocation = ZERO_VECTOR;

		if (BuildLocation != ZERO_VECTOR)
		{
			float Dist = vDist2D(BuildLocation, Hive->FloorLocation);
			float MaxDist = fmaxf(UTIL_MetresToGoldSrcUnits(1.0f), UTIL_MetresToGoldSrcUnits(10.0f) - Dist);

			AdjustedLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, BuildLocation, MaxDist);
		}

		AdjustedLocation = (AdjustedLocation != ZERO_VECTOR) ? AdjustedLocation : BuildLocation;

		FinalBuildLocation = UTIL_ProjectPointToNavmesh(AdjustedLocation, BUILDING_REGULAR_NAV_PROFILE);

		FinalBuildLocation = (FinalBuildLocation != ZERO_VECTOR) ? FinalBuildLocation : AdjustedLocation;

		if (FinalBuildLocation != ZERO_VECTOR)
		{
			UTIL_ClearCommanderAction(Action);

			if (vDist2DSq(Hive->FloorLocation, FinalBuildLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { return; }

			Action->ActionType = ACTION_DEPLOY;
			Action->ActionTarget = Hive->edict;
			Action->StructureToBuild = STRUCTURE_MARINE_TURRETFACTORY;
			Action->BuildLocation = FinalBuildLocation;
			Action->bIsActionUrgent = true;
			Action->ActionPurpose = STRUCTURE_PURPOSE_FORTIFY;

		}

		return;
	}

	if (!UTIL_StructureIsFullyBuilt(TF))
	{
		return;
	}

	if (UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_TURRET, TF->v.origin, UTIL_MetresToGoldSrcUnits(10.0f)) < 5)
	{
		COMM_SetTurretBuildAction(TF, Action);
		return;
	}

	if (!UTIL_IsStructureElectrified(TF) && !UTIL_StructureIsResearching(TF))
	{
		COMM_SetElectrifyStructureAction(TF, Action);
		return;
	}

	if (Hive->HiveResNodeIndex > -1)
	{
		const resource_node* HiveNode = UTIL_GetResourceNodeAtIndex(Hive->HiveResNodeIndex);

		if (HiveNode)
		{
			if (!HiveNode->bIsOccupied)
			{
				if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_RESTOWER && Action->ActionTarget == HiveNode->edict)
				{
					return;
				}

				Action->ActionType = ACTION_DEPLOY;
				Action->ActionTarget = HiveNode->edict;
				Action->StructureToBuild = STRUCTURE_MARINE_RESTOWER;
				Action->BuildLocation = HiveNode->origin;

				return;
			}

			if (HiveNode->bIsOwnedByMarines)
			{
				edict_t* ResTowerEdict = HiveNode->TowerEdict;

				if (UTIL_ElectricalResearchIsAvailable(ResTowerEdict))
				{
					int Resources = CommanderBot->resources;

					if (Resources > 100)
					{
						COMM_SetElectrifyStructureAction(ResTowerEdict, Action);
						return;
					}					
				}
			}
			
		}
	}

	UTIL_ClearCommanderAction(Action);
}

void COMM_SetNextResearchAction(commander_action* Action)
{
	if (UTIL_ArmouryResearchIsAvailable(RESEARCH_ARMOURY_GRENADES))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_ARMOURY_GRENADES) { return; }

		edict_t* Armoury = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMOURY);

		if (!FNullEnt(Armoury))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = Armoury;
			Action->ResearchId = RESEARCH_ARMOURY_GRENADES;

			return;
		}
	}

	if (UTIL_ArmsLabResearchIsAvailable(RESEARCH_ARMSLAB_ARMOUR1))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_ARMSLAB_ARMOUR1) { return; }

		edict_t* ArmsLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMSLAB);

		if (!FNullEnt(ArmsLab))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = ArmsLab;
			Action->ResearchId = RESEARCH_ARMSLAB_ARMOUR1;

			return;
		}
	}

	if (UTIL_ArmsLabResearchIsAvailable(RESEARCH_ARMSLAB_WEAPONS1))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_ARMSLAB_WEAPONS1) { return; }

		edict_t* ArmsLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMSLAB);

		if (!FNullEnt(ArmsLab))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = ArmsLab;
			Action->ResearchId = RESEARCH_ARMSLAB_WEAPONS1;

			return;
		}
	}

	if (UTIL_ObservatoryResearchIsAvailable(RESEARCH_OBSERVATORY_PHASETECH))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_OBSERVATORY_PHASETECH) { return; }

		edict_t* Observatory = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_OBSERVATORY);

		if (!FNullEnt(Observatory))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = Observatory;
			Action->ResearchId = RESEARCH_OBSERVATORY_PHASETECH;

			return;
		}
	}

	if (UTIL_ObservatoryResearchIsAvailable(RESEARCH_OBSERVATORY_MOTIONTRACKING))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_OBSERVATORY_MOTIONTRACKING) { return; }

		edict_t* Observatory = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_OBSERVATORY);

		if (!FNullEnt(Observatory))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = Observatory;
			Action->ResearchId = RESEARCH_OBSERVATORY_MOTIONTRACKING;

			return;
		}
	}

	if (UTIL_ArmsLabResearchIsAvailable(RESEARCH_ARMSLAB_ARMOUR2))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_ARMSLAB_ARMOUR2) { return; }

		edict_t* ArmsLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMSLAB);

		if (!FNullEnt(ArmsLab))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = ArmsLab;
			Action->ResearchId = RESEARCH_ARMSLAB_ARMOUR2;

			return;
		}
	}

	if (UTIL_ArmsLabResearchIsAvailable(RESEARCH_ARMSLAB_WEAPONS2))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_ARMSLAB_WEAPONS2) { return; }

		edict_t* ArmsLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMSLAB);

		if (!FNullEnt(ArmsLab))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = ArmsLab;
			Action->ResearchId = RESEARCH_ARMSLAB_WEAPONS2;

			return;
		}
	}

	if (UTIL_GetFirstPlacedStructureOfType(STRUCTURE_MARINE_PROTOTYPELAB) == nullptr) { return; }

	if (UTIL_PrototypeLabResearchIsAvailable(RESEARCH_PROTOTYPELAB_HEAVYARMOUR))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_PROTOTYPELAB_HEAVYARMOUR) { return; }

		edict_t* PrototypeLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_PROTOTYPELAB);

		if (!FNullEnt(PrototypeLab))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = PrototypeLab;
			Action->ResearchId = RESEARCH_PROTOTYPELAB_HEAVYARMOUR;

			return;
		}
	}

	if (UTIL_ArmsLabResearchIsAvailable(RESEARCH_ARMSLAB_ARMOUR3))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_ARMSLAB_ARMOUR3) { return; }

		edict_t* ArmsLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMSLAB);

		if (!FNullEnt(ArmsLab))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = ArmsLab;
			Action->ResearchId = RESEARCH_ARMSLAB_ARMOUR3;

			return;
		}
	}

	if (UTIL_ArmsLabResearchIsAvailable(RESEARCH_ARMSLAB_WEAPONS3))
	{
		if (Action->ActionType == ACTION_RESEARCH && Action->ResearchId == RESEARCH_ARMSLAB_WEAPONS3) { return; }

		edict_t* ArmsLab = UTIL_GetFirstIdleStructureOfType(STRUCTURE_MARINE_ARMSLAB);

		if (!FNullEnt(ArmsLab))
		{
			Action->ActionType = ACTION_RESEARCH;
			Action->ActionTarget = ArmsLab;
			Action->ResearchId = RESEARCH_ARMSLAB_WEAPONS3;

			return;
		}
	}

	UTIL_ClearCommanderAction(Action);
}

void COMM_SetNextSiegeHiveAction(bot_t* CommanderBot, const hive_definition* Hive, commander_action* Action)
{
	bool bPhaseGatesAvailable = UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH);

	edict_t* TF = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(25.0f), true, false);

	bool bTFIsUpgraded = (!FNullEnt(TF) && GetStructureTypeFromEdict(TF) == STRUCTURE_MARINE_ADVTURRETFACTORY);

	if (!FNullEnt(TF) && UTIL_StructureIsFullyBuilt(TF) && bTFIsUpgraded)
	{
		if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_SIEGETURRET, TF->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), true))
		{
			
			if (UTIL_ItemCanBeDeployed(DEPLOYABLE_ITEM_MARINE_SCAN) && (gpGlobals->time - CommanderBot->CommanderLastScanTime > 10.0f))
			{
				if (Action->ActionType == ACTION_DEPLOY && Action->ActionTarget == Hive->edict && Action->StructureToBuild == DEPLOYABLE_ITEM_MARINE_SCAN) { return; }

				UTIL_ClearCommanderAction(Action);

				Vector ScanLoc = FindClosestNavigablePointToDestination(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

				Vector FinalLoc = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, ScanLoc, UTIL_MetresToGoldSrcUnits(5.0f));

				FinalLoc = (FinalLoc != ZERO_VECTOR) ? FinalLoc : ScanLoc;

				if (ScanLoc != ZERO_VECTOR)
				{
					Action->ActionType = ACTION_DEPLOY;
					Action->ActionTarget = Hive->edict;
					Action->StructureToBuild = DEPLOYABLE_ITEM_MARINE_SCAN;
					Action->BuildLocation = FinalLoc;
					Action->bIsActionUrgent = true;
					return;
				}
			}
		}
	}

	edict_t* NearestPlayer = COMM_GetMarineEligibleToBuildSiege(Hive);

	bool bHasPlayerNearby = !FNullEnt(NearestPlayer);

	if (bHasPlayerNearby)
	{
		edict_t* PhaseGate = (bPhaseGatesAvailable) ? UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(25.0f), true, false) : nullptr;

		

		bool bHasBuiltStructureAlready = ((!FNullEnt(TF) && UTIL_StructureIsFullyBuilt(TF)) || (!FNullEnt(PhaseGate) && UTIL_StructureIsFullyBuilt(PhaseGate)));

		if (bPhaseGatesAvailable)
		{
			if (FNullEnt(PhaseGate))
			{
				if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_PHASEGATE && Action->ActionTarget == Hive->edict)
				{
					return;
				}

				Vector BuildLocation = ZERO_VECTOR;

				if (!FNullEnt(TF) && bHasBuiltStructureAlready)
				{
					BuildLocation = TF->v.origin;
				}
				else
				{
					if (!FNullEnt(NearestPlayer))
					{
						BuildLocation = UTIL_GetEntityGroundLocation(NearestPlayer);
					}
				}

				if (BuildLocation != ZERO_VECTOR)
				{
					Vector NewBuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, BuildLocation, UTIL_MetresToGoldSrcUnits(3.0f));

					Vector FinalBuildLocation = (NewBuildLocation != ZERO_VECTOR) ? NewBuildLocation : BuildLocation;

					UTIL_ClearCommanderAction(Action);

					if (vDist2DSq(FinalBuildLocation, Hive->edict->v.origin) > sqrf(kSiegeTurretRange)) { return; }

					Action->ActionType = ACTION_DEPLOY;
					Action->ActionTarget = Hive->edict;
					Action->StructureToBuild = STRUCTURE_MARINE_PHASEGATE;
					Action->BuildLocation = FinalBuildLocation;
					Action->bIsActionUrgent = true;
					Action->ActionPurpose = STRUCTURE_PURPOSE_SIEGE;

				}

				return;
			}

			if (!UTIL_StructureIsFullyBuilt(PhaseGate) && FNullEnt(TF))
			{
				UTIL_ClearCommanderAction(Action);
				return;
			}
		}

		if (FNullEnt(TF))
		{
			if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_TURRETFACTORY && Action->ActionTarget == Hive->edict)
			{
				return;
			}

			Vector BuildLocation = ZERO_VECTOR;

			if (!FNullEnt(PhaseGate) && bHasBuiltStructureAlready)
			{
				BuildLocation = PhaseGate->v.origin;
			}
			else
			{
				if (!FNullEnt(NearestPlayer))
				{
					BuildLocation = UTIL_GetEntityGroundLocation(NearestPlayer);
				}
			}

			if (BuildLocation != ZERO_VECTOR)
			{
				Vector NewBuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, BuildLocation, UTIL_MetresToGoldSrcUnits(3.0f));

				Vector FinalBuildLocation = (NewBuildLocation != ZERO_VECTOR) ? NewBuildLocation : BuildLocation;

				UTIL_ClearCommanderAction(Action);

				if (vDist2DSq(FinalBuildLocation, Hive->edict->v.origin) > sqrf(kSiegeTurretRange)) { return; }

				Action->ActionType = ACTION_DEPLOY;
				Action->ActionTarget = Hive->edict;
				Action->StructureToBuild = STRUCTURE_MARINE_TURRETFACTORY;
				Action->BuildLocation = FinalBuildLocation;
				Action->ActionPurpose = STRUCTURE_PURPOSE_SIEGE;
				Action->bIsActionUrgent = true;
			}

			return;
		}

		edict_t* Armoury = nullptr;
		
		if (!FNullEnt(PhaseGate))
		{
			Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, PhaseGate->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), true, false);
		}
		else
		{
			Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(30.0f), true, false);
		}

		if (FNullEnt(Armoury))
		{
			if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_ARMOURY && Action->ActionTarget == Hive->edict)
			{
				return;
			}

			Vector BuildLocation = ZERO_VECTOR;

			if (!FNullEnt(PhaseGate) && bHasBuiltStructureAlready)
			{
				BuildLocation = PhaseGate->v.origin;
			}
			else
			{
				if (!FNullEnt(NearestPlayer))
				{
					BuildLocation = UTIL_GetEntityGroundLocation(NearestPlayer);
				}
			}

			if (BuildLocation != ZERO_VECTOR)
			{
				Vector NewBuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, BuildLocation, UTIL_MetresToGoldSrcUnits(2.0f));

				Vector FinalBuildLocation = (NewBuildLocation != ZERO_VECTOR) ? NewBuildLocation : BuildLocation;

				UTIL_ClearCommanderAction(Action);

				Action->ActionType = ACTION_DEPLOY;
				Action->ActionTarget = Hive->edict;
				Action->StructureToBuild = STRUCTURE_MARINE_ARMOURY;
				Action->BuildLocation = FinalBuildLocation;
				Action->ActionPurpose = STRUCTURE_PURPOSE_SIEGE;
				Action->bIsActionUrgent = true;
			}

			return;
		}
	}

	if (FNullEnt(TF) || !UTIL_StructureIsFullyBuilt(TF))
	{
		UTIL_ClearCommanderAction(Action);
		return;
	}

	if (!bTFIsUpgraded && !UTIL_StructureIsUpgrading(TF))
	{

		if (!UTIL_StructureIsUpgrading(TF))
		{
			Action->ActionType = ACTION_UPGRADE;
			Action->ActionTarget = TF;
			Action->bIsActionUrgent = true;
		}
		else
		{
			UTIL_ClearCommanderAction(Action);
		}

		return;
	}

	if (bTFIsUpgraded)
	{
		if (bHasPlayerNearby)
		{
			int NumSiegeTurrets = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_SIEGETURRET, TF->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

			if (NumSiegeTurrets < 3)
			{
				COMM_SetSiegeTurretBuildAction(TF, Action, Hive->edict->v.origin, true);
				return;
			}
		}
		

		if (!UTIL_IsStructureElectrified(TF) && !UTIL_StructureIsResearching(TF))
		{
			COMM_SetElectrifyStructureAction(TF, Action);
			return;
		}
	}



	UTIL_ClearCommanderAction(Action);
}

void COMM_SetElectrifyStructureAction(edict_t* Structure, commander_action* Action)
{
	if (FNullEnt(Structure))
	{
		return;
	}

	if (Action->ActionType == ACTION_RESEARCH && Action->ActionTarget == Structure && Action->ResearchId == RESEARCH_ELECTRICAL) { return; }

	UTIL_ClearCommanderAction(Action);

	NSStructureType StructureTypeToElectrify = GetStructureTypeFromEdict(Structure);

	if (!UTIL_StructureTypesMatch(StructureTypeToElectrify, STRUCTURE_MARINE_ANYTURRETFACTORY) && !UTIL_StructureTypesMatch(StructureTypeToElectrify, STRUCTURE_MARINE_RESTOWER)) { return; }

	Action->ActionType = ACTION_RESEARCH;
	Action->ActionTarget = Structure;
	Action->ResearchId = RESEARCH_ELECTRICAL;

}

void COMM_SetNextSupportAction(bot_t* CommanderBot, commander_action* Action)
{
	edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f), true, false);

	if (FNullEnt(Armoury))
	{
		UTIL_ClearCommanderAction(Action);
		return;
	}

	// Don't drop stuff if we're critically low on resources
	if (CommanderBot->resources < 20)
	{
		UTIL_ClearCommanderAction(Action);
		return;
	}

	int NumMarines = GAME_GetNumPlayersOnTeam(MARINE_TEAM) - 1;

	edict_t* AdvArmoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ADVARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f), true, false);

	if (UTIL_ResearchIsComplete(RESEARCH_PROTOTYPELAB_HEAVYARMOUR) && !FNullEnt(AdvArmoury))
	{
		edict_t* PrototypeLab = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PROTOTYPELAB, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f), true, false);

		NSStructureType SpecialWeaponToDrop = (UTIL_GetNumWeaponsOfTypeInPlay(WEAPON_MARINE_GL) == 0) ? DEPLOYABLE_ITEM_MARINE_GRENADELAUNCHER : DEPLOYABLE_ITEM_MARINE_HMG;

		bool bShouldDropHeavyArmour = false;
		bool bShouldDropWeapon = false;
		bool bShouldDropWelder = false;

		if (CommanderBot->resources > 100)
		{
			bShouldDropHeavyArmour = UTIL_GetItemCountOfTypeInArea(DEPLOYABLE_ITEM_MARINE_HEAVYARMOUR, PrototypeLab->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)) < 3;
			bShouldDropWeapon = UTIL_GetItemCountOfTypeInArea(SpecialWeaponToDrop, AdvArmoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)) < 3;
			bShouldDropWelder = UTIL_GetItemCountOfTypeInArea(DEPLOYABLE_ITEM_MARINE_WELDER, AdvArmoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)) < 3;
		}
		else
		{
			edict_t* MarineNeedingLoadout = UTIL_GetNearestMarineWithoutFullLoadout(AdvArmoury->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

			int NumHeavyArmour = UTIL_GetItemCountOfTypeInArea(DEPLOYABLE_ITEM_MARINE_HEAVYARMOUR, PrototypeLab->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
			int NumWeapons = UTIL_GetItemCountOfTypeInArea(SpecialWeaponToDrop, AdvArmoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
			int NumWelder = UTIL_GetItemCountOfTypeInArea(DEPLOYABLE_ITEM_MARINE_WELDER, AdvArmoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

			bShouldDropHeavyArmour = (!FNullEnt(MarineNeedingLoadout) && !PlayerHasEquipment(MarineNeedingLoadout) && NumHeavyArmour == 0);
			bShouldDropWeapon = (!FNullEnt(MarineNeedingLoadout) && !PlayerHasSpecialWeapon(MarineNeedingLoadout) && NumWeapons == 0);
			bShouldDropWelder = (!FNullEnt(MarineNeedingLoadout) && !PlayerHasWeapon(MarineNeedingLoadout, WEAPON_MARINE_WELDER) && NumWelder == 0);
		}		

		if (bShouldDropHeavyArmour)
		{
			if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == DEPLOYABLE_ITEM_MARINE_HEAVYARMOUR) { return; }

			Action->ActionType = ACTION_DEPLOY;
			Action->StructureToBuild = DEPLOYABLE_ITEM_MARINE_HEAVYARMOUR;
			Action->BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, PrototypeLab->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
			return;
		}

		if (bShouldDropWeapon)
		{
			if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == SpecialWeaponToDrop) { return; }

			Action->ActionType = ACTION_DEPLOY;
			Action->StructureToBuild = SpecialWeaponToDrop;
			Action->BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, AdvArmoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
			return;
		}

		if (bShouldDropWelder)
		{
			if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == DEPLOYABLE_ITEM_MARINE_WELDER) { return; }

			Action->ActionType = ACTION_DEPLOY;
			Action->StructureToBuild = DEPLOYABLE_ITEM_MARINE_WELDER;
			Action->BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, AdvArmoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
			return;
		}

	}

	bool bShouldDropMines = false;

	if (CommanderBot->resources > 100)
	{
		bShouldDropMines = (UTIL_GetItemCountOfTypeInArea(DEPLOYABLE_ITEM_MARINE_MINES, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)) < 1);
	}
	else
	{
		if (UTIL_GetNumWeaponsOfTypeInPlay(WEAPON_MARINE_MINES) == 0 && (UTIL_UnminedStructureOfTypeExists(STRUCTURE_MARINE_INFANTRYPORTAL) || UTIL_UnminedStructureOfTypeExists(STRUCTURE_MARINE_PHASEGATE) || UTIL_UnminedStructureOfTypeExists(STRUCTURE_MARINE_ANYTURRETFACTORY)))
		{
			bShouldDropMines = true;
		}
	}

	if (bShouldDropMines)
	{
		if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == DEPLOYABLE_ITEM_MARINE_MINES) { return; }

		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(3.0f));

		if (DeployLocation != ZERO_VECTOR)
		{
			Action->ActionType = ACTION_DEPLOY;
			Action->StructureToBuild = DEPLOYABLE_ITEM_MARINE_MINES;
			Action->BuildLocation = DeployLocation;
		}

		return;
	}

	bool bShouldDropWelder = false;	

	if (CommanderBot->resources > 100)
	{
		bShouldDropWelder = (UTIL_GetItemCountOfTypeInArea(DEPLOYABLE_ITEM_MARINE_WELDER, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)) < 3);
	}
	else
	{
		int DesiredNumWelders = (CommanderBot->resources > 100) ? NumMarines : (NumMarines / 2);

		int NumWeldersInPlay = UTIL_GetNumWeaponsOfTypeInPlay(WEAPON_MARINE_WELDER);

		bShouldDropWelder = (NumWeldersInPlay < DesiredNumWelders);
	}

	if (bShouldDropWelder)
	{
		if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == DEPLOYABLE_ITEM_MARINE_WELDER) { return; }

		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(3.0f));

		if (DeployLocation != ZERO_VECTOR)
		{
			Action->ActionType = ACTION_DEPLOY;
			Action->StructureToBuild = DEPLOYABLE_ITEM_MARINE_WELDER;
			Action->BuildLocation = DeployLocation;
		}

		return;
	}

	bool bShouldDropShotgun = false;

	if (CommanderBot->resources > 100)
	{
		bShouldDropShotgun = (UTIL_GetItemCountOfTypeInArea(DEPLOYABLE_ITEM_MARINE_SHOTGUN, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)) < 3);
	}
	else
	{
		int DesiredNumShotguns = (NumMarines / 2);

		int NumShotgunsInPlay = UTIL_GetNumWeaponsOfTypeInPlay(WEAPON_MARINE_SHOTGUN);

		bShouldDropShotgun = (NumShotgunsInPlay < DesiredNumShotguns);
	}

	if (bShouldDropShotgun)
	{
		if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == DEPLOYABLE_ITEM_MARINE_SHOTGUN) { return; }

		Vector DeployLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(3.0f));

		if (DeployLocation != ZERO_VECTOR)
		{
			Action->ActionType = ACTION_DEPLOY;
			Action->StructureToBuild = DEPLOYABLE_ITEM_MARINE_SHOTGUN;
			Action->BuildLocation = DeployLocation;
		}

		return;
	}




}

void COMM_SetNextRecycleAction(bot_t* CommanderBot, commander_action* Action)
{
	edict_t* RedundantStructure = UTIL_GetRedundantMarineStructureOfType(STRUCTURE_MARINE_PHASEGATE);

	if (!FNullEnt(RedundantStructure))
	{
		Action->ActionType = ACTION_RECYCLE;
		Action->ActionTarget = RedundantStructure;
		return;
	}

	RedundantStructure = UTIL_GetRedundantMarineStructureOfType(STRUCTURE_MARINE_ANYTURRETFACTORY);

	if (!FNullEnt(RedundantStructure))
	{
		Action->ActionType = ACTION_RECYCLE;
		Action->ActionTarget = RedundantStructure;
		return;
	}

	RedundantStructure = UTIL_GetRedundantMarineStructureOfType(STRUCTURE_MARINE_ANYARMOURY);

	if (!FNullEnt(RedundantStructure))
	{
		Action->ActionType = ACTION_RECYCLE;
		Action->ActionTarget = RedundantStructure;
		return;
	}

	RedundantStructure = UTIL_GetRedundantMarineStructureOfType(STRUCTURE_MARINE_SIEGETURRET);

	if (!FNullEnt(RedundantStructure))
	{
		Action->ActionType = ACTION_RECYCLE;
		Action->ActionTarget = RedundantStructure;
		return;
	}
}

void COMM_SetNextBuildAction(bot_t* CommanderBot, commander_action* Action)
{
	edict_t* CommChair = UTIL_GetCommChair();

	if (FNullEnt(CommChair)) { return; }

	int NumInfantryPortals = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_INFANTRYPORTAL, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

	if (NumInfantryPortals < 2)
	{
		COMM_SetInfantryPortalBuildAction(CommChair, Action);
		return;
	}

	bool bHasArmoury = UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_ANYARMOURY, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

	if (!bHasArmoury)
	{
		if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_ARMOURY && vDist2DSq(Action->BuildLocation, CommChair->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { return; }

		UTIL_ClearCommanderAction(Action);

		edict_t* InfantryPortal = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_INFANTRYPORTAL, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), true, false);

		Vector BuildLocation = ZERO_VECTOR;

		if (!FNullEnt(InfantryPortal))
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, InfantryPortal->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
		}

		if (BuildLocation == ZERO_VECTOR)
		{
			BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
		}

		if (BuildLocation != ZERO_VECTOR)
		{
			Action->ActionType = ACTION_DEPLOY;
			Action->StructureToBuild = STRUCTURE_MARINE_ARMOURY;
			Action->BuildLocation = BuildLocation;
			Action->bIsActionUrgent = true;
		}	
		return;
	}

	const hive_definition* HiveUnderSiege = UTIL_GetNearestHiveUnderActiveSiege(UTIL_GetCommChairLocation());

	if (HiveUnderSiege)
	{
		COMM_SetNextSiegeHiveAction(CommanderBot, HiveUnderSiege, Action);

		if (Action->ActionType != ACTION_NONE)
		{
			return;
		}
	}

	if (UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH))
	{
		bool bHasPhaseGate = UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_PHASEGATE, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f), false);

		if (!bHasPhaseGate)
		{
			if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_PHASEGATE) { return; }

			UTIL_ClearCommanderAction(Action);

			edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ARMOURY, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

			Vector BuildLocation = ZERO_VECTOR;

			if (!FNullEnt(Armoury))
			{
				BuildLocation = UTIL_GetRandomPointOnNavmeshInDonut(GORGE_BUILD_NAV_PROFILE, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));
			}

			if (BuildLocation == ZERO_VECTOR)
			{
				BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));
			}

			if (BuildLocation != ZERO_VECTOR)
			{
				Action->ActionType = ACTION_DEPLOY;
				Action->StructureToBuild = STRUCTURE_MARINE_PHASEGATE;
				Action->BuildLocation = BuildLocation;
				Action->bIsActionUrgent = true;
			}

			return;
		}
	}

	const resource_node* CappableNode = COMM_GetResNodeCapOpportunityNearestLocation(UTIL_GetCommChairLocation());

	if (CappableNode)
	{
		if (Action->ActionType != ACTION_DEPLOY || Action->ActionTarget != CappableNode->edict)
		{
			int NumMarineRTs = UTIL_GetNumPlacedStructuresOfType(STRUCTURE_MARINE_RESTOWER);

			UTIL_ClearCommanderAction(Action);

			Action->ActionType = ACTION_DEPLOY;
			Action->ActionTarget = CappableNode->edict;
			Action->StructureToBuild = STRUCTURE_MARINE_RESTOWER;
			Action->BuildLocation = CappableNode->origin;
			Action->bIsActionUrgent = (NumMarineRTs < 4);
		}

		return;
	}

	const hive_definition* HiveToSecure = COMM_GetEmptyHiveOpportunityNearestLocation(CommanderBot, UTIL_GetCommChairLocation());

	if (HiveToSecure)
	{
		COMM_SetNextSecureHiveAction(CommanderBot, HiveToSecure, Action);
		if (Action->ActionType != ACTION_NONE)
		{
			return;
		}
	}

	bool bHasArmsLab = UTIL_StructureExistsOfType(STRUCTURE_MARINE_ARMSLAB);

	if (!bHasArmsLab)
	{
		if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_ARMSLAB) { return; }

		UTIL_ClearCommanderAction(Action);

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (BuildLocation != ZERO_VECTOR)
		{
			Action->ActionType = ACTION_DEPLOY;
			Action->StructureToBuild = STRUCTURE_MARINE_ARMSLAB;
			Action->BuildLocation = BuildLocation;
			Action->bIsActionUrgent = true;
		}

		return;
	}

	bool bHasObservatory = UTIL_StructureExistsOfType(STRUCTURE_MARINE_OBSERVATORY);

	if (!bHasObservatory)
	{
		if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_OBSERVATORY) { return; }

		UTIL_ClearCommanderAction(Action);

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(GORGE_BUILD_NAV_PROFILE, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (BuildLocation != ZERO_VECTOR)
		{
			Action->ActionType = ACTION_DEPLOY;
			Action->StructureToBuild = STRUCTURE_MARINE_OBSERVATORY;
			Action->BuildLocation = BuildLocation;
		}

		return;
	}

	if (!UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH))
	{
		UTIL_ClearCommanderAction(Action);
		return;
	}

	const hive_definition* HiveToSiege = COMM_GetHiveSiegeOpportunityNearestLocation(CommanderBot, UTIL_GetCommChairLocation());

	if (HiveToSiege)
	{
		COMM_SetNextSiegeHiveAction(CommanderBot, HiveToSiege, Action);
		if (Action->ActionType != ACTION_NONE)
		{
			return;
		}
	}

	bool bHasAdvArmoury = UTIL_StructureExistsOfType(STRUCTURE_MARINE_ADVARMOURY);
	bool bIsResearchingArmoury = false;
	
	if (!bHasAdvArmoury)
	{
		edict_t* NearestArmoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ARMOURY, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f), true, false);

		if (!FNullEnt(NearestArmoury))
		{
			bIsResearchingArmoury = UTIL_StructureIsUpgrading(NearestArmoury);
		}
	}

	if (!bHasAdvArmoury && !bIsResearchingArmoury)
	{
		if (Action->ActionType == ACTION_UPGRADE && Action->StructureToBuild == STRUCTURE_MARINE_ADVARMOURY) { return; }

		UTIL_ClearCommanderAction(Action);

		edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ARMOURY, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

		if (!FNullEnt(Armoury))
		{
			Action->ActionType = ACTION_UPGRADE;
			Action->StructureToBuild = STRUCTURE_MARINE_ADVARMOURY;
			Action->ActionTarget = Armoury;
		}

		return;
	}

	bool bHasPrototypeLab = UTIL_StructureExistsOfType(STRUCTURE_MARINE_PROTOTYPELAB);

	if (!bHasPrototypeLab && bHasAdvArmoury)
	{
		if (Action->ActionType == ACTION_DEPLOY && Action->StructureToBuild == STRUCTURE_MARINE_PROTOTYPELAB) { return; }

		UTIL_ClearCommanderAction(Action);

		edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f), true, false);

		if (!FNullEnt(Armoury))
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInDonut(GORGE_BUILD_NAV_PROFILE, Armoury->v.origin, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));

			if (BuildLocation != ZERO_VECTOR)
			{
				Action->ActionType = ACTION_DEPLOY;
				Action->StructureToBuild = STRUCTURE_MARINE_PROTOTYPELAB;
				Action->BuildLocation = BuildLocation;
				
			}
		}

		return;
	}

	int Resources = CommanderBot->resources;

	if (Resources > 100)
	{
		edict_t* UnelectrifiedResTower = UTIL_GetFurthestStructureOfTypeFromLocation(STRUCTURE_MARINE_RESTOWER, UTIL_GetCommChairLocation(), false);

		if (!FNullEnt(UnelectrifiedResTower) && UTIL_ElectricalResearchIsAvailable(UnelectrifiedResTower))
		{
			COMM_SetElectrifyStructureAction(UnelectrifiedResTower, Action);
			return;
		}
	}

	UTIL_ClearCommanderAction(Action);

}

void COMM_ConfirmObjectDeployed(bot_t* pBot, commander_action* Action, edict_t* DeployedObject)
{
	if (Action->ActionType == ACTION_DEPLOY)
	{
		if (Action->StructureToBuild == DEPLOYABLE_ITEM_MARINE_SCAN)
		{
			pBot->CommanderLastScanTime = gpGlobals->time;
		}

		buildable_structure* Ref = UTIL_GetBuildableStructureRefFromEdict(DeployedObject);

		if (Ref)
		{
			Ref->LastSuccessfulCommanderLocation = Action->LastAttemptedCommanderLocation;
			Ref->LastSuccessfulCommanderAngle = Action->LastAttemptedCommanderAngle;
			Ref->Purpose = Action->ActionPurpose;
		}

		pBot->next_commander_action_time = gpGlobals->time + commander_action_cooldown;

		UTIL_ClearCommanderAction(Action);
	}
}

edict_t* COMM_GetMarineEligibleToBuildSiege(const hive_definition* Hive)
{
	float MinDist = 0.0f;
	edict_t* Result = nullptr;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && IsPlayerMarine(clients[i]) && IsPlayerActiveInGame(clients[i]))
		{
			float ThisDist = vDist2DSq(clients[i]->v.origin, Hive->Location);

			if (ThisDist < sqrf(UTIL_MetresToGoldSrcUnits(25.0f)) && !UTIL_QuickTrace(clients[i], GetPlayerEyePosition(clients[i]), Hive->Location))
			{
				edict_t* DangerTurret = PlayerGetNearestDangerTurret(clients[i], UTIL_MetresToGoldSrcUnits(15.0f));

				if (!FNullEnt(DangerTurret) && UTIL_QuickTrace(DangerTurret, GetPlayerTopOfCollisionHull(DangerTurret), clients[i]->v.origin)) { continue; }

				if (UTIL_AnyPlayerOnTeamWithLOS(clients[i]->v.origin, ALIEN_TEAM, UTIL_MetresToGoldSrcUnits(10.0f))) { continue; }

				if (FNullEnt(Result) || ThisDist < MinDist)
				{
					Result = clients[i];
					MinDist = ThisDist;
				}

			}
		}
	}

	return Result;
}
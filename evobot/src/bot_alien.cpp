
#include "bot_alien.h"

#include <dllapi.h>

#include "bot_task.h"
#include "bot_weapons.h"
#include "player_util.h"
#include "bot_navigation.h"
#include "bot_tactical.h"
#include "general_util.h"
#include "bot_util.h"
#include "bot_config.h"
#include "game_state.h"

void AlienThink(bot_t* pBot)
{

	if (pBot->CurrentEnemy > -1)
	{
		if (pBot->CurrentEnemy > -1)
		{
			AlienCombatThink(pBot);
			return;
		}
	}

	if (!pBot->CurrentTask) { pBot->CurrentTask = &pBot->PrimaryBotTask; }

	BotUpdateAndClearTasks(pBot);

	if (pBot->PrimaryBotTask.TaskType == TASK_NONE || !pBot->PrimaryBotTask.bTaskIsUrgent)
	{
		BotRole RequiredRole = AlienGetBestBotRole(pBot);

		if (pBot->CurrentRole != RequiredRole)
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);

			pBot->CurrentRole = RequiredRole;
			pBot->CurrentTask = &pBot->PrimaryBotTask;
		}

		BotAlienSetPrimaryTask(pBot, &pBot->PrimaryBotTask);
	}

	if (pBot->SecondaryBotTask.TaskType == TASK_NONE || !pBot->SecondaryBotTask.bTaskIsUrgent)
	{
		BotAlienSetSecondaryTask(pBot, &pBot->SecondaryBotTask);
	}

	AlienCheckWantsAndNeeds(pBot);

	pBot->CurrentTask = BotGetNextTask(pBot);

	if (!IsPlayerGorge(pBot->pEdict) || PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_BILEBOMB))
	{
		edict_t* DangerTurret = BotGetNearestDangerTurret(pBot, UTIL_MetresToGoldSrcUnits(10.0f));

		if (!FNullEnt(DangerTurret))
		{
			Vector TaskLocation = (!FNullEnt(pBot->CurrentTask->TaskTarget)) ? pBot->CurrentTask->TaskTarget->v.origin : pBot->CurrentTask->TaskLocation;
			float DistToTurret = vDist2DSq(TaskLocation, DangerTurret->v.origin);

			if (pBot->CurrentTask->TaskType != TASK_ATTACK && DistToTurret < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				BotAttackTarget(pBot, DangerTurret);
				return;
			}
		}
	}	

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}
}

void AlienCombatThink(bot_t* pBot)
{
	if (pBot->CurrentEnemy > -1)
	{
		edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

		if (!FNullEnt(CurrentEnemy) && IsPlayerActiveInGame(CurrentEnemy))
		{
			switch (pBot->bot_ns_class)
			{
			case CLASS_SKULK:
				SkulkCombatThink(pBot);
				return;
			case CLASS_GORGE:
				GorgeCombatThink(pBot);
				return;
			case CLASS_LERK:
				return;
			case CLASS_FADE:
				FadeCombatThink(pBot);
				return;
			case CLASS_ONOS:
				OnosCombatThink(pBot);
				return;
			default:
				return;
			}
		}
	}
}

void AlienCombatModeThink(bot_t* pBot)
{
	if (pBot->CurrentEnemy > -1)
	{
		AlienCombatThink(pBot);
		return;
	}

	BotUpdateAndClearTasks(pBot);
	AlienCheckCombatModeWantsAndNeeds(pBot);

	if (pBot->PrimaryBotTask.TaskType == TASK_NONE || (!pBot->PrimaryBotTask.bTaskIsUrgent && !pBot->PrimaryBotTask.bIssuedByCommander))
	{
		BotRole RequiredRole = AlienGetBestCombatModeRole(pBot);

		if (pBot->CurrentRole != RequiredRole)
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);

			pBot->CurrentRole = RequiredRole;
			pBot->CurrentTask = &pBot->PrimaryBotTask;
		}

		BotAlienSetCombatModePrimaryTask(pBot, &pBot->PrimaryBotTask);
	}

	if (!IsPlayerGorge(pBot->pEdict) && pBot->SecondaryBotTask.TaskType == TASK_NONE)
	{
		AlienSetCombatModeSecondaryTask(pBot, &pBot->SecondaryBotTask);
	}

	pBot->CurrentTask = BotGetNextTask(pBot);

	BotProgressTask(pBot, pBot->CurrentTask);


}

void BotAlienSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	switch (pBot->CurrentRole)
	{
	case BOT_ROLE_HARASS:
		AlienHarasserSetPrimaryTask(pBot, Task);
		break;
	case BOT_ROLE_DESTROYER:
		AlienDestroyerSetPrimaryTask(pBot, Task);
		break;
	case BOT_ROLE_RES_CAPPER:
		AlienCapperSetPrimaryTask(pBot, Task);
		break;
	case BOT_ROLE_BUILDER:
		AlienBuilderSetPrimaryTask(pBot, Task);
		break;
	default:
		break;
	}
}

void AlienSetCombatModeSecondaryTask(bot_t* pBot, bot_task* Task)
{
	const hive_definition* Hive = UTIL_GetNearestBuiltHiveToLocation(pBot->pEdict->v.origin);

	if (pBot->CurrentRole == BOT_ROLE_BUILDER && !IsPlayerGorge(pBot->pEdict) && GetBotAvailableCombatPoints(pBot) >= 1)
	{
		if (Task->TaskType == TASK_EVOLVE) { return; }

		int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

		Vector EvolvePosition = FindClosestNavigablePointToDestination(BotProfile, pBot->pEdict->v.origin, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		if (EvolvePosition == ZERO_VECTOR)
		{
			EvolvePosition = pBot->pEdict->v.origin;
		}
		else
		{
			// Don't evolve right underneath the hive even if we can reach it...
			if (vDist2DSq(EvolvePosition, Hive->FloorLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
			{
				EvolvePosition = UTIL_GetRandomPointOnNavmeshInDonut(BotProfile, EvolvePosition, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));
			}
		}

		Task->TaskType = TASK_EVOLVE;
		Task->TaskLocation = EvolvePosition;
		Task->Evolution = IMPULSE_ALIEN_EVOLVE_GORGE;
		Task->bTaskIsUrgent = true;
		return;

	}

	if (pBot->CurrentRole == BOT_ROLE_DESTROYER && IsPlayerSkulk(pBot->pEdict) && GetBotAvailableCombatPoints(pBot) >= 3)
	{
		if (Task->TaskType == TASK_EVOLVE) { return; }

		int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

		Vector EvolvePosition = FindClosestNavigablePointToDestination(BotProfile, pBot->pEdict->v.origin, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		if (EvolvePosition == ZERO_VECTOR)
		{
			EvolvePosition = pBot->pEdict->v.origin;
		}
		else
		{
			// Don't evolve right underneath the hive even if we can reach it...
			if (vDist2DSq(EvolvePosition, Hive->FloorLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
			{
				EvolvePosition = UTIL_GetRandomPointOnNavmeshInDonut(BotProfile, EvolvePosition, UTIL_MetresToGoldSrcUnits(3.0f), UTIL_MetresToGoldSrcUnits(5.0f));
			}
		}

		Task->TaskType = TASK_EVOLVE;
		Task->TaskLocation = EvolvePosition;

		if ((pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ONOS) && GetBotAvailableCombatPoints(pBot) >= 4)
		{
			Task->Evolution = IMPULSE_ALIEN_EVOLVE_ONOS;
		}
		else
		{
			Task->Evolution = IMPULSE_ALIEN_EVOLVE_FADE;
		}
		
		Task->bTaskIsUrgent = true;
		return;

	}

	if (Hive && Hive->bIsUnderAttack)
	{
		// Already defending
		if (Task->TaskType == TASK_DEFEND) { return; }

		float HiveHealth = (Hive->edict->v.health / Hive->edict->v.max_health);

		if (HiveHealth < 0.7f)
		{
			int NumDefenders = UTIL_GetNumPlayersOfTeamInArea(Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f), ALIEN_TEAM, pBot->pEdict, CLASS_GORGE, false);

			if (NumDefenders < 2)
			{
				TASK_SetDefendTask(pBot, Task, Hive->edict, false);
			}
		}
	}
}

void BotAlienSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task)
{
	switch (pBot->CurrentRole)
	{
		case BOT_ROLE_BUILDER:
			AlienBuilderSetCombatModePrimaryTask(pBot, Task);
			break;
		case BOT_ROLE_HARASS:
			AlienHarasserSetCombatModePrimaryTask(pBot, Task);
			break;
		default:
			AlienDestroyerSetCombatModePrimaryTask(pBot, Task);
			break;
	}
}

void AlienHarasserSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerGorge(pBot->pEdict))
	{
		BotEvolveLifeform(pBot, CLASS_SKULK);
	}

	int NumMarineTowers = UTIL_GetStructureCountOfType(STRUCTURE_MARINE_RESTOWER);
	int NumAlienTowers = UTIL_GetStructureCountOfType(STRUCTURE_ALIEN_RESTOWER);

	if (NumMarineTowers > 2)
	{
		const resource_node* ResNode = UTIL_GetNearestCappedResNodeToLocation(pBot->pEdict->v.origin, MARINE_TEAM, !IsPlayerSkulk(pBot->pEdict));

		if (ResNode)
		{
			TASK_SetAttackTask(pBot, Task, ResNode->TowerEdict, false);
			return;
		}
	}

	if (NumAlienTowers >= 3)
	{
		int NumInfantryPortals = UTIL_GetStructureCountOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

		if (NumInfantryPortals > 0)
		{
			edict_t* InfPortal = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

			if (InfPortal)
			{
				TASK_SetAttackTask(pBot, Task, InfPortal, false);
				return;
			}
		}

		edict_t* CommChair = UTIL_GetCommChair();

		if (CommChair)
		{
			TASK_SetAttackTask(pBot, Task, CommChair, false);
			return;
		}

		edict_t* EnemyPlayer = UTIL_GetNearestPlayerOfTeamInArea(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(100.0f), MARINE_TEAM, nullptr, CLASS_NONE);

		if (!FNullEnt(EnemyPlayer))
		{
			TASK_SetAttackTask(pBot, Task, EnemyPlayer, true);
			return;
		}

	}

	const resource_node* ResNode = UTIL_FindEligibleResNodeClosestToLocation(pBot->pEdict->v.origin, ALIEN_TEAM, !IsPlayerSkulk(pBot->pEdict));

	if (ResNode)
	{
		if (ResNode->bIsOccupied && !FNullEnt(ResNode->TowerEdict))
		{
			TASK_SetAttackTask(pBot, Task, ResNode->TowerEdict, false);
			return;
		}
		else
		{
			TASK_SetMoveTask(pBot, Task, ResNode->origin, false);
			return;
		}
	}
	else
	{
		
		int NavProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
		Vector RandomMovePoint = UTIL_GetRandomPointOnNavmeshInRadius(NavProfile, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(50.0f));

		TASK_SetMoveTask(pBot, Task, RandomMovePoint, false);
		return;
	}

}

void AlienHarasserSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task)
{

}

void AlienCapperSetPrimaryTask(bot_t* pBot, bot_task* Task)
{

	bool bCappingIsUrgent = UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER) < 3;

	int RequiredRes = kResourceTowerCost;

	if (!IsPlayerGorge(pBot->pEdict))
	{
		RequiredRes += kGorgeEvolutionCost;
	}

	if (pBot->resources < RequiredRes)
	{
		if (IsPlayerGorge(pBot->pEdict))
		{
			// Already capping a node, do nothing
			if (Task->TaskType == TASK_CAP_RESNODE) { return; }

			const resource_node* EmptyResNode = UTIL_AlienFindUnclaimedResNodeFurthestFromLocation(pBot, UTIL_GetCommChairLocation(), true);

			if (EmptyResNode)
			{
				TASK_SetCapResNodeTask(pBot, Task, EmptyResNode, bCappingIsUrgent);
				return;
			}
			else
			{
				BotEvolveLifeform(pBot, CLASS_SKULK);
			}
			return;
		}

		// Don't set a new attack or move task if we have one already
		if (Task->TaskType == TASK_ATTACK) { return; }

		edict_t* RandomResTower = UTIL_GetRandomStructureOfType(STRUCTURE_MARINE_RESTOWER, nullptr, true);

		if (!FNullEnt(RandomResTower))
		{
			TASK_SetAttackTask(pBot, Task, RandomResTower, false);
			return;
		}

		if (Task->TaskType == TASK_MOVE) { return; }

		// Attack a random resource node or move to one TODO: Prefer attacking where possible...
		Vector RandomPoint = UTIL_GetRandomPointOfInterest();

		if (RandomPoint != ZERO_VECTOR)
		{
			TASK_SetMoveTask(pBot, Task, RandomPoint, false);
			return;
		}

		return;
	}

	// Already capping a node, do nothing
	if (Task->TaskType == TASK_CAP_RESNODE) { return; }

	const resource_node* RandomResNode = nullptr;

	if (!IsPlayerGorge(pBot->pEdict) || PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_BILEBOMB))
	{
		RandomResNode = UTIL_FindEligibleResNodeFurthestFromLocation(UTIL_GetCommChairLocation(), ALIEN_TEAM, !IsPlayerSkulk(pBot->pEdict));
	}
	else
	{
		RandomResNode = UTIL_AlienFindUnclaimedResNodeFurthestFromLocation(pBot, UTIL_GetCommChairLocation(), !IsPlayerSkulk(pBot->pEdict));
	}

	if (RandomResNode)
	{
		TASK_SetCapResNodeTask(pBot, Task, RandomResNode, bCappingIsUrgent);
		return;
	}

	// Can't do anything as gorge, return to skulk
	if (IsPlayerGorge(pBot->pEdict))
	{
		BotEvolveLifeform(pBot, CLASS_SKULK);
	}
}

void AlienBuilderSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* pEdict = pBot->pEdict;

	// If we already have a build task then do nothing
	if (Task->TaskType == TASK_BUILD) { return; }

	int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	NSStructureType TechChamberToBuild = STRUCTURE_NONE;
	const hive_definition* HiveIndex = nullptr;

	HiveTechStatus HiveTechOne = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(0);
	HiveTechStatus HiveTechTwo = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(1);
	HiveTechStatus HiveTechThree = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(2);

	if (UTIL_ActiveHiveWithTechExists(HiveTechOne) && UTIL_GetStructureCountOfType(UTIL_GetChamberTypeForHiveTech(HiveTechOne)) < 3)
	{
		TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechOne);
		HiveIndex = UTIL_GetHiveWithTech(HiveTechOne);
	}
	else if (UTIL_ActiveHiveWithTechExists(HiveTechTwo) && UTIL_GetStructureCountOfType(UTIL_GetChamberTypeForHiveTech(HiveTechTwo)) < 3)
	{
		TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechTwo);
		HiveIndex = UTIL_GetHiveWithTech(HiveTechTwo);
	}
	else if (UTIL_ActiveHiveWithTechExists(HiveTechThree) && UTIL_GetStructureCountOfType(UTIL_GetChamberTypeForHiveTech(HiveTechThree)) < 3)
	{
		TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechThree);
		HiveIndex = UTIL_GetHiveWithTech(HiveTechThree);
	}

	if (HiveIndex && TechChamberToBuild != STRUCTURE_NONE)
	{
		Vector NearestPointToHive = FindClosestNavigablePointToDestination(MoveProfile, pBot->pEdict->v.origin, HiveIndex->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));

		if (NearestPointToHive == ZERO_VECTOR)
		{
			NearestPointToHive = pBot->pEdict->v.origin;
		}

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, NearestPointToHive, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vEquals(BuildLocation, ZERO_VECTOR))
		{
			TASK_SetBuildTask(pBot, Task, TechChamberToBuild, BuildLocation, true);
			return;
		}

		return;
	}

	HiveIndex = UTIL_GetFirstHiveWithoutTech();

	if (HiveIndex)
	{

		if (!UTIL_ActiveHiveWithTechExists(HiveTechOne))
		{
			TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechOne);
		}
		else if (!UTIL_ActiveHiveWithTechExists(HiveTechTwo))
		{
			TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechTwo);
		}
		else
		{
			TechChamberToBuild = UTIL_GetChamberTypeForHiveTech(HiveTechThree);
		}

		Vector NearestPoint = FindClosestNavigablePointToDestination(MoveProfile, pBot->pEdict->v.origin, HiveIndex->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));

		if (NearestPoint == ZERO_VECTOR)
		{
			NearestPoint = pBot->pEdict->v.origin;
		}

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, NearestPoint, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vEquals(BuildLocation, ZERO_VECTOR))
		{
			TASK_SetBuildTask(pBot, Task, TechChamberToBuild, BuildLocation, true);
			return;
		}
	}

	if (!UTIL_HiveIsInProgress() && UTIL_GetNumUnbuiltHives() > 0)
	{
		const hive_definition* UnbuiltHiveIndex = UTIL_GetClosestViableUnbuiltHive(pEdict->v.origin);

		if (UnbuiltHiveIndex)
		{
			edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(UnbuiltHiveIndex->Location, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(30.0f), pEdict);

			if (FNullEnt(OtherGorge) || GetPlayerResources(OtherGorge) < pBot->resources)
			{
				TASK_SetBuildTask(pBot, Task, STRUCTURE_ALIEN_HIVE, UnbuiltHiveIndex->FloorLocation, false);

				return;
			}
		}
	}

	// Build 2 defence chambers under every hive
	if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_DEFENCE))
	{
		const hive_definition* HiveNeedsSupporting = UTIL_GetActiveHiveWithoutChambers(HIVE_TECH_DEFENCE, 2);

		if (HiveNeedsSupporting)
		{
			Vector NearestPointToHive = FindClosestNavigablePointToDestination(MoveProfile, pBot->pEdict->v.origin, HiveNeedsSupporting->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));

			if (NearestPointToHive != ZERO_VECTOR)
			{
				Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, NearestPointToHive, UTIL_MetresToGoldSrcUnits(5.0f));

				if (BuildLocation != ZERO_VECTOR)
				{
					TASK_SetBuildTask(pBot, Task, STRUCTURE_ALIEN_DEFENCECHAMBER, BuildLocation, false);
					return;
				}
			}

			
		}
	}

	// Make sure every hive has a movement chamber
	if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_MOVEMENT))
	{
		const hive_definition* HiveNeedsSupporting = UTIL_GetActiveHiveWithoutChambers(HIVE_TECH_MOVEMENT, 1);

		if (HiveNeedsSupporting)
		{
			Vector NearestPointToHive = FindClosestNavigablePointToDestination(MoveProfile, pBot->pEdict->v.origin, HiveNeedsSupporting->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));

			if (NearestPointToHive != ZERO_VECTOR)
			{
				Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, NearestPointToHive, UTIL_MetresToGoldSrcUnits(5.0f));

				if (BuildLocation != ZERO_VECTOR)
				{
					TASK_SetBuildTask(pBot, Task, STRUCTURE_ALIEN_MOVEMENTCHAMBER, BuildLocation, false);
					return;
				}
			}
		}
	}

	// Make sure every hive has a sensory chamber
	if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_SENSORY))
	{
		const hive_definition* HiveNeedsSupporting = UTIL_GetActiveHiveWithoutChambers(HIVE_TECH_SENSORY, 1);

		if (HiveNeedsSupporting)
		{
			Vector NearestPointToHive = FindClosestNavigablePointToDestination(MoveProfile, pBot->pEdict->v.origin, HiveNeedsSupporting->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));

			if (NearestPointToHive != ZERO_VECTOR)
			{
				Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, NearestPointToHive, UTIL_MetresToGoldSrcUnits(5.0f));

				if (BuildLocation != ZERO_VECTOR)
				{
					TASK_SetBuildTask(pBot, Task, STRUCTURE_ALIEN_SENSORYCHAMBER, BuildLocation, false);
					return;
				}
				
			}
		}
	}

	// Reinforce resource nodes which are closest to the marine base to start boxing them in and denying them access to the rest of the map
	const resource_node* NearestUnprotectedResNode = UTIL_GetNearestUnprotectedResNode(UTIL_GetCommChairLocation());

	if (NearestUnprotectedResNode)
	{
		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(3.0f));

		if (BuildLocation == ZERO_VECTOR)
		{
			return;
		}

		int NumOffenceChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_OFFENCECHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumOffenceChambers < 2)
		{
			TASK_SetBuildTask(pBot, Task, STRUCTURE_ALIEN_OFFENCECHAMBER, BuildLocation, false);
			return;
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_DEFENCE))
		{
			int NumDefenceChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_DEFENCECHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (NumDefenceChambers < 2)
			{
				TASK_SetBuildTask(pBot, Task, STRUCTURE_ALIEN_DEFENCECHAMBER, BuildLocation, false);
				return;
			}
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_MOVEMENT))
		{
			int NumMovementChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_MOVEMENTCHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (NumMovementChambers < 1)
			{
				TASK_SetBuildTask(pBot, Task, STRUCTURE_ALIEN_MOVEMENTCHAMBER, BuildLocation, false);
				return;
			}
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_SENSORY))
		{
			int NumSensoryChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_SENSORYCHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (NumSensoryChambers < 1)
			{
				TASK_SetBuildTask(pBot, Task, STRUCTURE_ALIEN_SENSORYCHAMBER, BuildLocation, false);
				return;
			}
		}

	}
}

void AlienBuilderSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task)
{
	// If we can't go gorge and are not already gorge then act like a normal attacker until we can
	if (!IsPlayerGorge(pBot->pEdict) && GetBotAvailableCombatPoints(pBot) < 1)
	{
		AlienDestroyerSetCombatModePrimaryTask(pBot, Task);
		return;
	}

	if (Task->TaskType == TASK_HEAL) { return; }

	const hive_definition* Hive = UTIL_GetNearestBuiltHiveToLocation(pBot->pEdict->v.origin);

	if (!Hive) { return; }

	float HiveHealth = (Hive->edict->v.health / Hive->edict->v.max_health);

	edict_t* HurtPlayer = UTIL_GetClosestPlayerNeedsHealing(pBot->pEdict->v.origin, ALIEN_TEAM, UTIL_MetresToGoldSrcUnits(15.0f), pBot->pEdict, false);

	if (Hive && HiveHealth < 0.99f)
	{
		if (FNullEnt(HurtPlayer))
		{
			int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

			Vector HealPosition = FindClosestNavigablePointToDestination(BotProfile, pBot->pEdict->v.origin, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(3.0f));

			if (HealPosition != ZERO_VECTOR)
			{
				Task->TaskType = TASK_HEAL;
				Task->TaskTarget = Hive->edict;
				Task->TaskLocation = HealPosition;
				Task->bTaskIsUrgent = false;
				return;
			}
		}

		if (HiveHealth < 0.5f)
		{
			int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

			Vector HealPosition = FindClosestNavigablePointToDestination(BotProfile, pBot->pEdict->v.origin, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(3.0f));

			if (HealPosition != ZERO_VECTOR)
			{
				Task->TaskType = TASK_HEAL;
				Task->TaskTarget = Hive->edict;
				Task->TaskLocation = HealPosition;
				Task->bTaskIsUrgent = true;
				return;
			}
		}
		else
		{
			Task->TaskType = TASK_HEAL;
			Task->TaskTarget = HurtPlayer;
			Task->TaskLocation = HurtPlayer->v.origin;
			Task->bTaskIsUrgent = false;
			return;
		}


	}

	if (!FNullEnt(HurtPlayer))
	{
		Task->TaskType = TASK_HEAL;
		Task->TaskTarget = HurtPlayer;
		Task->TaskLocation = HurtPlayer->v.origin;
		Task->bTaskIsUrgent = false;
		return;
	}

	if (Task->TaskType == TASK_GUARD) { return; }

	int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	Vector GuardPosition = FindClosestNavigablePointToDestination(BotProfile, pBot->pEdict->v.origin, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

	if (GuardPosition != ZERO_VECTOR)
	{
		GuardPosition = UTIL_GetRandomPointOnNavmeshInRadius(BotProfile, GuardPosition, UTIL_MetresToGoldSrcUnits(5.0f));

		Task->TaskType = TASK_GUARD;
		Task->TaskTarget = nullptr;
		Task->TaskLocation = GuardPosition;
		Task->bTaskIsUrgent = false;
		return;
	}
}

void AlienDestroyerSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (!IsPlayerFade(pBot->pEdict) && !IsPlayerOnos(pBot->pEdict))
	{
		if (pBot->resources >= kFadeEvolutionCost)
		{
			Vector EvolveLocation = ZERO_VECTOR;

			const hive_definition* NearestHive = UTIL_GetNearestHiveOfStatus(pBot->pEdict->v.origin, HIVE_STATUS_BUILT);

			if (NearestHive)
			{
				int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
				EvolveLocation = FindClosestNavigablePointToDestination(MoveProfile, pBot->CurrentFloorPosition, NearestHive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));
			}

			if (EvolveLocation == ZERO_VECTOR)
			{
				EvolveLocation = pBot->pEdict->v.origin;
			}

			if (pBot->resources >= kOnosEvolutionCost)
			{
				Task->TaskType = TASK_EVOLVE;
				Task->Evolution = IMPULSE_ALIEN_EVOLVE_ONOS;
				Task->bTaskIsUrgent = true;
				Task->TaskLocation = EvolveLocation;
				return;
			}

			int NumFades = GAME_GetNumPlayersOnTeamOfClass(ALIEN_TEAM, CLASS_FADE);

			if (NumFades < 2)
			{
				Task->TaskType = TASK_EVOLVE;
				Task->Evolution = IMPULSE_ALIEN_EVOLVE_FADE;
				Task->bTaskIsUrgent = true;
				Task->TaskLocation = EvolveLocation;
				return;
			}
		}
	}

	if (IsPlayerGorge(pBot->pEdict))
	{
		Task->TaskType = TASK_EVOLVE;
		Task->Evolution = IMPULSE_ALIEN_EVOLVE_SKULK;
		Task->bTaskIsUrgent = true;
		Task->TaskLocation = pBot->pEdict->v.origin;
		return;
	}

	bool bAllowElectrified = !IsPlayerSkulk(pBot->pEdict);

	edict_t* DangerStructure = UTIL_GetAnyStructureOfTypeNearActiveHive(STRUCTURE_MARINE_PHASEGATE, bAllowElectrified, false);

	if (FNullEnt(DangerStructure))
	{
		DangerStructure = UTIL_GetAnyStructureOfTypeNearActiveHive(STRUCTURE_MARINE_ANYTURRETFACTORY, bAllowElectrified, false);

		if (FNullEnt(DangerStructure))
		{
			DangerStructure = UTIL_GetAnyStructureOfTypeNearActiveHive(STRUCTURE_ANY_MARINE_STRUCTURE, bAllowElectrified, false);
		}
	}

	if (!FNullEnt(DangerStructure))
	{
		TASK_SetAttackTask(pBot, Task, DangerStructure, true);
		return;
	}

	edict_t* BlockingStructure = UTIL_GetAnyStructureOfTypeNearUnbuiltHive(STRUCTURE_MARINE_PHASEGATE, bAllowElectrified, false);

	if (FNullEnt(BlockingStructure))
	{
		BlockingStructure = UTIL_GetAnyStructureOfTypeNearUnbuiltHive(STRUCTURE_MARINE_ANYTURRETFACTORY, bAllowElectrified, false);

		if (FNullEnt(BlockingStructure))
		{
			BlockingStructure = UTIL_GetAnyStructureOfTypeNearUnbuiltHive(STRUCTURE_ANY_MARINE_STRUCTURE, bAllowElectrified, false);
		}
	}

	if (!FNullEnt(BlockingStructure))
	{
		TASK_SetAttackTask(pBot, Task, BlockingStructure, false);
		return;
	}

	if (Task->TaskType == TASK_ATTACK)
	{
		return;
	}

	edict_t* InfPortal = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

	if (InfPortal)
	{
		TASK_SetAttackTask(pBot, Task, InfPortal, false);
		return;
	}

	edict_t* Armslab = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_ARMSLAB);

	if (Armslab)
	{
		TASK_SetAttackTask(pBot, Task, Armslab, false);
		return;
	}

	edict_t* Obs = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_OBSERVATORY);

	if (Obs)
	{
		TASK_SetAttackTask(pBot, Task, Obs, false);
		return;
	}

	edict_t* CommChair = UTIL_GetCommChair();

	if (CommChair)
	{
		TASK_SetAttackTask(pBot, Task, CommChair, false);
		return;
	}

	edict_t* EnemyPlayer = UTIL_GetNearestPlayerOfTeamInArea(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(100.0f), MARINE_TEAM, nullptr, CLASS_NONE);

	if (!FNullEnt(EnemyPlayer))
	{
		TASK_SetAttackTask(pBot, Task, EnemyPlayer, false);
		return;
	}
}

void AlienDestroyerSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskType != TASK_NONE) { return; }

	edict_t* StructureToAttack = UTIL_GetFirstPlacedStructureOfType(STRUCTURE_ANY_MARINE_STRUCTURE);

	// Always attack if we're close to the enemy base. Prevents bots deciding to wander off when inside the enemy base
	if (!FNullEnt(StructureToAttack))
	{
		float DistFromTarget = vDist2DSq(pBot->pEdict->v.origin, StructureToAttack->v.origin);

		bool bShouldAttack = (DistFromTarget <= sqrf(UTIL_MetresToGoldSrcUnits(15.0f))) ? true : randbool();

		if (bShouldAttack)
		{
			if (!FNullEnt(StructureToAttack))
			{
				TASK_SetAttackTask(pBot, Task, StructureToAttack, false);
				return;
			}
		}
	}

	int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
	Vector RandomPoint = UTIL_GetRandomPointOnNavmeshInRadius(BotProfile, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f));

	TASK_SetMoveTask(pBot, Task, RandomPoint, false);

	return;
}

void BotAlienSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerGorge(pBot->pEdict))
	{
		AlienBuilderSetSecondaryTask(pBot, &pBot->SecondaryBotTask);
	}

	switch (pBot->CurrentRole)
	{
	case BOT_ROLE_HARASS:
		AlienHarasserSetSecondaryTask(pBot, &pBot->SecondaryBotTask);
		break;
	case BOT_ROLE_DESTROYER:
		AlienDestroyerSetSecondaryTask(pBot, Task);
		break;
	case BOT_ROLE_RES_CAPPER:
		AlienCapperSetSecondaryTask(pBot, Task);
		break;
	default:
		break;
	}
}

void AlienBuilderSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerGorge(pBot->pEdict))
	{
		edict_t* HurtNearbyPlayer = UTIL_GetClosestPlayerNeedsHealing(pBot->pEdict->v.origin, pBot->bot_team, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict, true);

		if (!FNullEnt(HurtNearbyPlayer))
		{
			if (pBot->SecondaryBotTask.TaskType != TASK_HEAL)
			{
				pBot->SecondaryBotTask.TaskType = TASK_HEAL;
				pBot->SecondaryBotTask.TaskTarget = HurtNearbyPlayer;
				pBot->SecondaryBotTask.TaskLocation = HurtNearbyPlayer->v.origin;
				pBot->SecondaryBotTask.bTaskIsUrgent = (HurtNearbyPlayer->v.health < (HurtNearbyPlayer->v.max_health * 0.5f));
			}
			return;
		}
	}
}

void AlienHarasserSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* PhaseGate = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), !IsPlayerSkulk(pBot->pEdict), false);

	if (PhaseGate)
	{
		int NumExistingPlayers = UTIL_GetNumPlayersOfTeamInArea(PhaseGate->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), pBot->bot_team, pBot->pEdict, CLASS_GORGE, false);

		if (NumExistingPlayers < 2)
		{
			TASK_SetAttackTask(pBot, Task, PhaseGate, true);
			return;
		}
	}

	edict_t* TurretFactory = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), !IsPlayerSkulk(pBot->pEdict), false);

	if (TurretFactory)
	{
		int NumExistingPlayers = UTIL_GetNumPlayersOfTeamInArea(TurretFactory->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), pBot->bot_team, pBot->pEdict, CLASS_GORGE, false);

		if (NumExistingPlayers < 2)
		{
			TASK_SetAttackTask(pBot, Task, TurretFactory, true);
			return;
		}
	}

	edict_t* AnyMarineStructure = UTIL_FindClosestMarineStructureToLocation(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), !IsPlayerSkulk(pBot->pEdict));

	if (AnyMarineStructure)
	{
		int NumExistingPlayers = UTIL_GetNumPlayersOfTeamInArea(AnyMarineStructure->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), pBot->bot_team, pBot->pEdict, CLASS_GORGE, false);

		if (NumExistingPlayers < 2)
		{
			TASK_SetAttackTask(pBot, Task, AnyMarineStructure, false);
			return;
		}
	}
}

void AlienCapperSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerGorge(pBot->pEdict))
	{
		edict_t* HurtNearbyPlayer = UTIL_GetClosestPlayerNeedsHealing(pBot->pEdict->v.origin, pBot->bot_team, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict, true);

		if (!FNullEnt(HurtNearbyPlayer))
		{
			pBot->SecondaryBotTask.TaskType = TASK_HEAL;
			pBot->SecondaryBotTask.TaskTarget = HurtNearbyPlayer;
			pBot->SecondaryBotTask.TaskLocation = HurtNearbyPlayer->v.origin;
			pBot->SecondaryBotTask.bTaskIsUrgent = true;
		}

		return;
	}

	edict_t* Hive = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_HIVE);

	if (!FNullEnt(Hive))
	{
		TASK_SetDefendTask(pBot, Task, Hive, true);
		return;
	}

	edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_RESTOWER);

	if (!FNullEnt(ResourceTower))
	{
		TASK_SetDefendTask(pBot, Task, ResourceTower, true);
		return;
	}
}

void AlienDestroyerSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* Hive = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_HIVE);

	if (!FNullEnt(Hive))
	{
		TASK_SetDefendTask(pBot, Task, Hive, true);
		return;
	}

	edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_RESTOWER);

	if (!FNullEnt(ResourceTower))
	{
		bool bIsUrgent = (UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER) <= 3);
		TASK_SetDefendTask(pBot, Task, ResourceTower, bIsUrgent);
		return;
	}
}

void SkulkCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0 || pBot->CurrentEnemy > 31) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	if (FNullEnt(CurrentEnemy) || !TrackedEnemyRef || IsPlayerDead(CurrentEnemy)) { return; }

	if (!TrackedEnemyRef->bCurrentlyVisible)
	{
		Vector EnemyLoc = (TrackedEnemyRef->bIsTracked) ? TrackedEnemyRef->TrackedLocation : TrackedEnemyRef->LastSeenLocation;

		MoveTo(pBot, EnemyLoc, MOVESTYLE_AMBUSH);

		return;
	}

	NSWeapon DesiredCombatWeapon = SkulkGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, CurrentEnemy);

	if (LOSCheck == ATTACK_SUCCESS)
	{
		BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
	}

	Vector EnemyFacing = UTIL_GetForwardVector2D(CurrentEnemy->v.angles);
	Vector BotFacing = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->pEdict->v.origin);

	float Dot = UTIL_GetDotProduct2D(EnemyFacing, BotFacing);

	if (Dot < 0.0f || LOSCheck != ATTACK_SUCCESS)
	{
		Vector TargetLocation = UTIL_GetFloorUnderEntity(CurrentEnemy);
		Vector BehindPlayer = TargetLocation - (UTIL_GetForwardVector2D(CurrentEnemy->v.v_angle) * 50.0f);

		int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

		if (UTIL_PointIsReachable(NavProfileIndex, pBot->CurrentFloorPosition, BehindPlayer, 0.0f))
		{
			MoveTo(pBot, BehindPlayer, MOVESTYLE_NORMAL);
		}
		else
		{
			if (UTIL_PointIsReachable(NavProfileIndex, pBot->CurrentFloorPosition, TargetLocation, 50.0f))
			{
				MoveTo(pBot, TargetLocation, MOVESTYLE_NORMAL);
			}
		}
	}	
}

void FadeCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy) || !IsPlayerActiveInGame(CurrentEnemy)) { return; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	float MaxHealthAndArmour = pEdict->v.max_health + GetPlayerMaxArmour(pEdict);
	float CurrentHealthAndArmour = pEdict->v.health + pEdict->v.armorvalue;

	bool bLowOnHealth = (GetPlayerOverallHealthPercent(pEdict) < 0.5f);
	bool bNeedsHealth = (GetPlayerOverallHealthPercent(pEdict) < 0.9f);

	edict_t* NearestHealingSource = (bNeedsHealth) ? UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin) : nullptr;

	// Run away if low on health
	if (!FNullEnt(NearestHealingSource))
	{
		// TODO: Attack enemy if they're in trouble
		float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(5.0f);
		
		if (bLowOnHealth)
		{
			bool bOutOfEnemyLOS = UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, GetPlayerEyePosition(CurrentEnemy));

			if (vDist3DSq(pEdict->v.origin, NearestHealingSource->v.origin) > sqrf(DesiredDistFromHealingSource))
			{
				MoveTo(pBot, UTIL_GetFloorUnderEntity(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

				if (bOutOfEnemyLOS)
				{
					if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_METABOLIZE))
					{
						pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;

						if (GetBotCurrentWeapon(pBot) == WEAPON_FADE_METABOLIZE)
						{
							pBot->pEdict->v.button |= IN_ATTACK;
						}
					}
				}
				else
				{
					Vector EnemyTargetDir = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pEdict->v.origin);

					float Dot = UTIL_GetDotProduct2D(pBot->desiredMovementDir, EnemyTargetDir);

					if (Dot > 0.7f)
					{
						BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, WEAPON_FADE_SWIPE, CurrentEnemy);

						if (LOSCheck == ATTACK_SUCCESS)
						{
							BotShootTarget(pBot, WEAPON_FADE_SWIPE, CurrentEnemy);
						}
					}
				}

				return;
			}

			if (bOutOfEnemyLOS)
			{
				if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_METABOLIZE))
				{
					pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;

					if (GetBotCurrentWeapon(pBot) == WEAPON_FADE_METABOLIZE)
					{
						pBot->pEdict->v.button |= IN_ATTACK;
					}
				}

				if (IsEdictPlayer(NearestHealingSource))
				{
					const hive_definition* Hive = UTIL_GetNearestBuiltHiveToLocation(pEdict->v.origin);

					if (Hive && vDist3DSq(pEdict->v.origin, Hive->FloorLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
					{
						MoveTo(pBot, UTIL_GetFloorUnderEntity(NearestHealingSource), MOVESTYLE_NORMAL, UTIL_MetresToGoldSrcUnits(5.0f));
						return;
					}
				}

				Vector CurrentHealSpot = pBot->BotNavInfo.ActualMoveDestination;

				if (!CurrentHealSpot || UTIL_QuickTrace(pEdict, CurrentHealSpot + Vector(0.0f, 0.0f, 32.0f), GetPlayerEyePosition(CurrentEnemy)))
				{
					int BotMoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
					CurrentHealSpot = UTIL_GetRandomPointOnNavmeshInRadius(BotMoveProfile, pBot->CurrentFloorPosition, UTIL_MetresToGoldSrcUnits(5.0f));

					if (CurrentHealSpot != ZERO_VECTOR && vDist2DSq(CurrentHealSpot, NearestHealingSource->v.origin) < DesiredDistFromHealingSource && !UTIL_QuickTrace(pEdict, CurrentHealSpot + Vector(0.0f, 0.0f, 32.0f), GetPlayerEyePosition(CurrentEnemy)))
					{
						MoveTo(pBot, CurrentHealSpot, MOVESTYLE_NORMAL);
						return;
					}
				}
			}
			else
			{
				return;
			}
			
		}
		else
		{
			if (vDist3DSq(pEdict->v.origin, NearestHealingSource->v.origin) <= sqrf(DesiredDistFromHealingSource) && !UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, GetPlayerEyePosition(CurrentEnemy)))
			{
				if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_METABOLIZE))
				{
					pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;

					if (GetBotCurrentWeapon(pBot) == WEAPON_FADE_METABOLIZE)
					{
						pBot->pEdict->v.button |= IN_ATTACK;
					}
				}

				return;
			}
		}

	}

	// If the enemy is not visible
	if (!TrackedEnemyRef->bCurrentlyVisible && !UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, CurrentEnemy->v.origin))
	{
		Vector EnemyLoc = (TrackedEnemyRef->bIsTracked) ? TrackedEnemyRef->TrackedLocation : TrackedEnemyRef->LastSeenLocation;

		MoveTo(pBot, EnemyLoc, MOVESTYLE_NORMAL);

		return;
	}

	NSWeapon DesiredCombatWeapon = FadeGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, CurrentEnemy);

	if (LOSCheck == ATTACK_SUCCESS)
	{
		BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
	}

	if (IsMeleeWeapon(DesiredCombatWeapon))
	{

		Vector EnemyFacing = UTIL_GetForwardVector2D(CurrentEnemy->v.angles);
		Vector BotFacing = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pEdict->v.origin);

		float Dot = UTIL_GetDotProduct2D(EnemyFacing, BotFacing);

		if (Dot < 0.0f || LOSCheck != ATTACK_SUCCESS)
		{
			Vector TargetLocation = UTIL_GetFloorUnderEntity(CurrentEnemy);
			Vector BehindPlayer = TargetLocation - (UTIL_GetForwardVector2D(CurrentEnemy->v.v_angle) * 50.0f);

			int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

			if (UTIL_PointIsReachable(NavProfileIndex, pBot->CurrentFloorPosition, BehindPlayer, 0.0f))
			{
				MoveTo(pBot, BehindPlayer, MOVESTYLE_NORMAL);
			}
			else
			{
				if (UTIL_PointIsReachable(NavProfileIndex, pBot->CurrentFloorPosition, TargetLocation, 50.0f))
				{
					MoveTo(pBot, TargetLocation, MOVESTYLE_NORMAL);
				}
			}

			if (LOSCheck == ATTACK_OUTOFRANGE)
			{
				if (PlayerHasWeapon(pEdict, WEAPON_FADE_BLINK) && UTIL_PointIsDirectlyReachable(pEdict->v.origin, CurrentEnemy->v.origin))
				{
					BotLeap(pBot, CurrentEnemy->v.origin);
				}
			}
		}

		return;
	}


	float CurrentDistance = vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin);
	float WeaponMaxDistance = sqrf(GetMaxIdealWeaponRange(DesiredCombatWeapon));
	float MinWeaponDistance = GetMinIdealWeaponRange(DesiredCombatWeapon);

	Vector EngagementLocation = pBot->BotNavInfo.TargetDestination;

	float EngagementLocationDist = vDist2DSq(EngagementLocation, CurrentEnemy->v.origin);

	if (!EngagementLocation || EngagementLocationDist > WeaponMaxDistance || EngagementLocationDist < MinWeaponDistance || !UTIL_QuickTrace(pBot->pEdict, EngagementLocation, CurrentEnemy->v.origin))
	{
		int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
		EngagementLocation = UTIL_GetRandomPointOnNavmeshInRadius(MoveProfile, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(2.0f));

		if (!vEquals(EngagementLocation, ZERO_VECTOR) && EngagementLocationDist < WeaponMaxDistance || EngagementLocationDist > MinWeaponDistance && UTIL_QuickTrace(pBot->pEdict, EngagementLocation, CurrentEnemy->v.origin))
		{
			MoveTo(pBot, EngagementLocation, MOVESTYLE_NORMAL);
		}
	}
	else
	{
		MoveTo(pBot, EngagementLocation, MOVESTYLE_NORMAL);
	}

}

void GorgeCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0 || pBot->CurrentEnemy > 31) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	if (!TrackedEnemyRef || FNullEnt(CurrentEnemy) || IsPlayerDead(CurrentEnemy)) { return; }

	if (TrackedEnemyRef->bCurrentlyVisible)
	{
		BotLookAt(pBot, CurrentEnemy);

		if (GetBotCurrentWeapon(pBot) != WEAPON_GORGE_SPIT)
		{
			pBot->DesiredCombatWeapon = WEAPON_GORGE_SPIT;
		}
		else
		{

			Vector LineFrom = (pEdict->v.origin + pEdict->v.view_ofs);
			Vector LineTo = LineFrom + (UTIL_GetForwardVector(pEdict->v.v_angle) * 1000.0f);
			float dist = vDistanceFromLine2D(LineFrom, LineTo, pBot->LookTarget->v.origin);

			if (dist < 30.0f)
			{
				pEdict->v.button |= IN_ATTACK;
			}
		}
	}
	else
	{
		if (gpGlobals->time - TrackedEnemyRef->LastSeenTime < 2.0f)
		{
			BotLookAt(pBot, TrackedEnemyRef->LastSeenLocation);
		}
		else
		{
			if (pBot->pEdict->v.health < pBot->pEdict->v.max_health)
			{
				pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;

				if (GetBotCurrentWeapon(pBot) == WEAPON_GORGE_HEALINGSPRAY)
				{
					pEdict->v.button |= IN_ATTACK;
				}
			}
			else
			{
				pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;
			}
		}
	}

	if (UTIL_IsPlayerOfTeamInArea(CurrentEnemy->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), ALIEN_TEAM, pBot->pEdict, CLASS_GORGE))
	{
		if (UTIL_QuickTrace(pBot->pEdict, (TrackedEnemyRef->LastSeenLocation + TrackedEnemyRef->EnemyEdict->v.view_ofs), pBot->pEdict->v.origin))
		{
			int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_HIDE);

			Vector EscapeLocation = pBot->BotNavInfo.TargetDestination;

			if (!EscapeLocation || UTIL_QuickTrace(pBot->pEdict, (TrackedEnemyRef->LastSeenLocation + TrackedEnemyRef->EnemyEdict->v.view_ofs), EscapeLocation))
			{
				Vector EnemyDir = UTIL_GetVectorNormal2D(TrackedEnemyRef->LastSeenLocation - pEdict->v.origin);

				Vector SearchLocation = pEdict->v.origin - (EnemyDir * UTIL_MetresToGoldSrcUnits(20.0f));

				Vector NewMove = UTIL_GetRandomPointOnNavmeshInRadius(MoveProfile, SearchLocation, UTIL_MetresToGoldSrcUnits(10.0f));

				if (!NewMove)
				{
					NewMove = UTIL_GetRandomPointOnNavmeshInRadius(MoveProfile, SearchLocation, UTIL_MetresToGoldSrcUnits(30.0f));
				}


				EscapeLocation = UTIL_GetRandomPointOnNavmeshInRadius(MoveProfile, NewMove, UTIL_MetresToGoldSrcUnits(10.0f));
			}


			MoveTo(pBot, EscapeLocation, MOVESTYLE_HIDE);
		}
		else
		{
			BotGuardLocation(pBot, pBot->pEdict->v.origin);
		}
	}
	else
	{
		edict_t* NearestFriendly = UTIL_GetNearestPlayerOfTeamInArea(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), ALIEN_TEAM, pBot->pEdict, CLASS_GORGE);

		// We don't want to run to our friend if it brings us closer to the enemy...
		if (FNullEnt(NearestFriendly) || (vDist2DSq(TrackedEnemyRef->LastSeenLocation, NearestFriendly->v.origin) < vDist2DSq(pBot->pEdict->v.origin, NearestFriendly->v.origin)))
		{
			const hive_definition* NearestHive = UTIL_GetNearestHiveOfStatus(pBot->pEdict->v.origin, HIVE_STATUS_BUILT);

			if (NearestHive)
			{
				if (vDist2DSq(pBot->pEdict->v.origin, NearestHive->FloorLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					MoveTo(pBot, NearestHive->FloorLocation, MOVESTYLE_HIDE);
				}
			}
		}
		else
		{
			Vector EscapeLocation = pBot->BotNavInfo.TargetDestination;

			if (!EscapeLocation || !UTIL_PointIsDirectlyReachable(EscapeLocation, NearestFriendly->v.origin) || vDist2DSq(EscapeLocation, TrackedEnemyRef->LastSeenLocation) < vDist2DSq(NearestFriendly->v.origin, TrackedEnemyRef->LastSeenLocation))
			{
				EscapeLocation = NearestFriendly->v.origin;
			}

			MoveTo(pBot, EscapeLocation, MOVESTYLE_HIDE);
		}
	}
}

void OnosCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy)) { return; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	bool bLowOnHealth = (GetPlayerOverallHealthPercent(pEdict) < 0.4f);
	bool bNeedsHealth = (GetPlayerOverallHealthPercent(pEdict) < 0.9f);

	edict_t* NearestHealingSource = (bNeedsHealth) ? UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin) : nullptr;

	// Run away if low on health
	if (!FNullEnt(NearestHealingSource))
	{
		// TODO: Attack enemy if they're in trouble
		float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(5.0f);

		if (bLowOnHealth)
		{
			if (vDist3DSq(pEdict->v.origin, NearestHealingSource->v.origin) > sqrf(DesiredDistFromHealingSource))
			{
				MoveTo(pBot, UTIL_GetFloorUnderEntity(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

				if (UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, GetPlayerEyePosition(CurrentEnemy)))
				{
					Vector EnemyTargetDir = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pEdict->v.origin);

					float Dot = UTIL_GetDotProduct2D(pBot->desiredMovementDir, EnemyTargetDir);

					if (Dot > 0.7f)
					{
						BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, WEAPON_ONOS_GORE, CurrentEnemy);

						if (LOSCheck == ATTACK_SUCCESS)
						{
							BotShootTarget(pBot, WEAPON_ONOS_GORE, CurrentEnemy);
							return;
						}
					}
				}

				if (PlayerHasWeapon(pBot->pEdict, WEAPON_ONOS_CHARGE))
				{
					if (GetBotCurrentWeapon(pBot) != WEAPON_ONOS_CHARGE)
					{
						pBot->DesiredMoveWeapon = WEAPON_ONOS_CHARGE;
					}
					else
					{
						pEdict->v.button |= IN_ATTACK2;
					}
				}

				return;
			}

			if (UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, GetPlayerEyePosition(CurrentEnemy)))
			{
				if (IsEdictPlayer(NearestHealingSource))
				{
					const hive_definition* Hive = UTIL_GetNearestBuiltHiveToLocation(pEdict->v.origin);

					if (Hive && vDist3DSq(pEdict->v.origin, Hive->FloorLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
					{
						MoveTo(pBot, UTIL_GetFloorUnderEntity(NearestHealingSource), MOVESTYLE_NORMAL, UTIL_MetresToGoldSrcUnits(5.0f));
						return;
					}
				}

				Vector CurrentHealSpot = pBot->BotNavInfo.ActualMoveDestination;

				if (!CurrentHealSpot || UTIL_QuickTrace(pEdict, CurrentHealSpot + Vector(0.0f, 0.0f, 32.0f), GetPlayerEyePosition(CurrentEnemy)))
				{
					int BotMoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
					CurrentHealSpot = UTIL_GetRandomPointOnNavmeshInRadius(BotMoveProfile, pBot->CurrentFloorPosition, UTIL_MetresToGoldSrcUnits(5.0f));

					if (CurrentHealSpot != ZERO_VECTOR && vDist2DSq(CurrentHealSpot, NearestHealingSource->v.origin) < DesiredDistFromHealingSource && !UTIL_QuickTrace(pEdict, CurrentHealSpot + Vector(0.0f, 0.0f, 32.0f), GetPlayerEyePosition(CurrentEnemy)))
					{
						MoveTo(pBot, CurrentHealSpot, MOVESTYLE_NORMAL);
						return;
					}
				}
			}
			else
			{
				return;
			}

		}
		else
		{
			if (vDist3DSq(pEdict->v.origin, NearestHealingSource->v.origin) < sqrf(DesiredDistFromHealingSource) && !UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, GetPlayerEyePosition(CurrentEnemy)))
			{
				return;
			}
		}

	}

	if (bLowOnHealth)
	{
		const hive_definition* NearestHive = UTIL_GetNearestHiveOfStatus(pEdict->v.origin, HIVE_STATUS_BUILT);

		if (NearestHive)
		{
			if (vDist2DSq(pBot->pEdict->v.origin, NearestHive->FloorLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				MoveTo(pBot, NearestHive->FloorLocation, MOVESTYLE_NORMAL);
			}
		}

		if (vDist2DSq(CurrentEnemy->v.origin, pBot->pEdict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
			Vector EnemyDir = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->pEdict->v.origin);
			Vector DesiredMoveDir = UTIL_GetVectorNormal2D(pBot->desiredMovementDir);

			bool bCanDevour = !IsPlayerDigesting(pBot->pEdict);

			NSWeapon MeleeWeapon = (bCanDevour) ? WEAPON_ONOS_DEVOUR : WEAPON_ONOS_GORE;

			if (UTIL_GetDotProduct2D(EnemyDir, DesiredMoveDir) > 0.75f)
			{
				if (GetBotCurrentWeapon(pBot) != MeleeWeapon)
				{
					pBot->DesiredCombatWeapon = MeleeWeapon;
				}
				else
				{
					BotShootTarget(pBot, pBot->DesiredCombatWeapon, CurrentEnemy);
				}

				return;
			}
		}		

		if (PlayerHasWeapon(pBot->pEdict, WEAPON_ONOS_CHARGE))
		{
			if (GetBotCurrentWeapon(pBot) != WEAPON_ONOS_CHARGE)
			{
				pBot->DesiredMoveWeapon = WEAPON_ONOS_CHARGE;
			}
			else
			{
				// Only charge if it will leave us with enough energy to stomp after, otherwise Onos charges in and then can't do anything

				float RequiredEnergy = (kStompEnergyCost + GetLeapCost(pBot)) - (GetPlayerEnergyRegenPerSecond(pEdict) * 0.5f); // We allow for around .5s of regen time as well

				if (GetPlayerEnergy(pEdict) >= RequiredEnergy)
				{
					if (UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, CurrentEnemy->v.origin))
					{
						pEdict->v.button |= IN_ATTACK2;
					}
				}

			}
		}

		return;
	}

	if (!TrackedEnemyRef->bCurrentlyVisible)
	{
		Vector EnemyLoc = (TrackedEnemyRef->bIsTracked) ? TrackedEnemyRef->TrackedLocation : TrackedEnemyRef->LastSeenLocation;

		MoveTo(pBot, EnemyLoc, MOVESTYLE_NORMAL);

		return;
	}

	BotLookAt(pBot, CurrentEnemy);

	NSWeapon DesiredCombatWeapon = OnosGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	pBot->DesiredCombatWeapon = DesiredCombatWeapon;

	if (GetBotCurrentWeapon(pBot) == DesiredCombatWeapon)
	{
		BotShootTarget(pBot, pBot->DesiredCombatWeapon, CurrentEnemy);
	}

	MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);

	return;
}

void AlienCheckWantsAndNeeds(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (IsPlayerDead(pEdict)) { return; }

	float MaxHealthAndArmour = pEdict->v.max_health + GetPlayerMaxArmour(pEdict);
	float CurrentHealthAndArmour = pEdict->v.health + pEdict->v.armorvalue;

	if (CurrentHealthAndArmour < MaxHealthAndArmour)
	{
		if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_METABOLIZE) || PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_HEALINGSPRAY))
		{
			NSWeapon HealingWeapon = IsPlayerFade(pBot->pEdict) ? WEAPON_FADE_METABOLIZE : WEAPON_GORGE_HEALINGSPRAY;

			pBot->DesiredCombatWeapon = HealingWeapon;

			if (GetBotCurrentWeapon(pBot) == HealingWeapon)
			{
				pBot->pEdict->v.button |= IN_ATTACK;
			}
		}
	}

	bool bLowOnHealth = ((CurrentHealthAndArmour / MaxHealthAndArmour) <= 0.5f);

	// Don't go hunting for health as a gorge, can heal themselves quickly. Fades will because metabolise isn't enough
	if (bLowOnHealth && !IsPlayerGorge(pBot->pEdict))
	{

		edict_t* HealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);

		if (!FNullEnt(HealingSource))
		{
			int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

			float HealRange = 0.0f;

			if (IsEdictStructure(HealingSource))
			{
				NSStructureType StructType = GetStructureTypeFromEdict(HealingSource);

				if (StructType == STRUCTURE_ALIEN_HIVE)
				{
					HealRange = kHiveHealRadius * 0.9f;
				}
				else
				{
					HealRange = kDefensiveChamberHealRange * 0.9f;
				}
			}
			else
			{
				HealRange = kHealingSprayRange;
			}

			Vector NearestPoint = FindClosestNavigablePointToDestination(MoveProfile, pBot->pEdict->v.origin, UTIL_GetFloorUnderEntity(HealingSource), HealRange);

			if (NearestPoint != ZERO_VECTOR)
			{
				pBot->WantsAndNeedsTask.TaskType = TASK_GET_HEALTH;
				pBot->WantsAndNeedsTask.TaskTarget = HealingSource;
				pBot->WantsAndNeedsTask.TaskLocation = NearestPoint;
				pBot->WantsAndNeedsTask.bTaskIsUrgent = true;
				return;
			}
		}

	}

	if (pBot->CurrentRole != BOT_ROLE_BUILDER && (pBot->PrimaryBotTask.TaskType == TASK_CAP_RESNODE || pBot->PrimaryBotTask.TaskType == TASK_BUILD))
	{
		return;
	}

	if (gpGlobals->time - pBot->LastCombatTime > 5.0f)
	{
		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_MOVEMENT) && !PlayerHasAlienUpgradeOfType(pEdict, HIVE_TECH_MOVEMENT) && UTIL_StructureExistsOfType(STRUCTURE_ALIEN_MOVEMENTCHAMBER))
		{
			pEdict->v.impulse = GetDesiredAlienUpgrade(pBot, HIVE_TECH_MOVEMENT);

			return;
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_DEFENCE) && !PlayerHasAlienUpgradeOfType(pEdict, HIVE_TECH_DEFENCE) && UTIL_StructureExistsOfType(STRUCTURE_ALIEN_DEFENCECHAMBER))
		{
			pEdict->v.impulse = GetDesiredAlienUpgrade(pBot, HIVE_TECH_DEFENCE);

			return;
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_SENSORY) && !PlayerHasAlienUpgradeOfType(pEdict, HIVE_TECH_SENSORY) && UTIL_StructureExistsOfType(STRUCTURE_ALIEN_SENSORYCHAMBER))
		{
			pEdict->v.impulse = GetDesiredAlienUpgrade(pBot, HIVE_TECH_SENSORY);

			return;
		}
	}
}

void AlienCheckCombatModeWantsAndNeeds(bot_t* pBot)
{
	// Don't bother going for healing if skulk. DEATH OR GLORY.
	if (IsPlayerSkulk(pBot->pEdict)) { return; }

	// Already got a heal-up task
	if (pBot->WantsAndNeedsTask.TaskType == TASK_GET_HEALTH) { return; }

	float MaxHealthAndArmour = pBot->pEdict->v.max_health + GetPlayerMaxArmour(pBot->pEdict);
	float CurrentHealthAndArmour = pBot->pEdict->v.health + pBot->pEdict->v.armorvalue;

	if (CurrentHealthAndArmour < MaxHealthAndArmour)
	{
		if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_METABOLIZE) || PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_HEALINGSPRAY))
		{
			NSWeapon HealingWeapon = IsPlayerFade(pBot->pEdict) ? WEAPON_FADE_METABOLIZE : WEAPON_GORGE_HEALINGSPRAY;

			pBot->DesiredCombatWeapon = HealingWeapon;

			if (GetBotCurrentWeapon(pBot) == HealingWeapon)
			{
				pBot->pEdict->v.button |= IN_ATTACK;
			}
		}
	}

	if (GetPlayerOverallHealthPercent(pBot->pEdict) < 0.5f)
	{
		const hive_definition* NearestHive = UTIL_GetNearestBuiltHiveToLocation(pBot->pEdict->v.origin);

		if (NearestHive)
		{
			int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
			Vector HealLocation = FindClosestNavigablePointToDestination(BotProfile, pBot->CurrentFloorPosition, NearestHive->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			if (HealLocation != ZERO_VECTOR)
			{
				pBot->WantsAndNeedsTask.TaskType = TASK_GET_HEALTH;
				pBot->WantsAndNeedsTask.TaskTarget = NearestHive->edict;
				pBot->WantsAndNeedsTask.TaskLocation = HealLocation;
				pBot->WantsAndNeedsTask.bTaskIsUrgent = true;
				return;
			}
		}
	}
}

int GetDesiredAlienUpgrade(const bot_t* pBot, const HiveTechStatus TechType)
{
	edict_t* pEdict = pBot->pEdict;

	if (TechType == HIVE_TECH_DEFENCE)
	{
		switch (pBot->bot_ns_class)
		{
		case CLASS_SKULK:
		{
			return IMPULSE_ALIEN_UPGRADE_CARAPACE;
		}
		case CLASS_GORGE:
		case CLASS_FADE:
		{
			if (randbool())
			{
				return IMPULSE_ALIEN_UPGRADE_CARAPACE;
			}
			else
			{
				if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_METABOLIZE))
				{
					return IMPULSE_ALIEN_UPGRADE_REDEMPTION;
				}
				else
				{
					if (randbool())
					{
						return IMPULSE_ALIEN_UPGRADE_REGENERATION;
					}
					else
					{
						return IMPULSE_ALIEN_UPGRADE_REDEMPTION;
					}
				}
			}
		}
		case CLASS_ONOS:
		{
			if (randbool())
			{
				return IMPULSE_ALIEN_UPGRADE_CARAPACE;
			}
			else
			{
				if (randbool())
				{
					return IMPULSE_ALIEN_UPGRADE_REGENERATION;
				}
				else
				{
					return IMPULSE_ALIEN_UPGRADE_REDEMPTION;
				}


			}
		}
		default:
			return 0;
		}
	}

	if (TechType == HIVE_TECH_MOVEMENT)
	{
		switch (pBot->bot_ns_class)
		{
		case CLASS_SKULK:
		{
			if (randbool())
			{
				return IMPULSE_ALIEN_UPGRADE_CELERITY;
			}
			else
			{
				return IMPULSE_ALIEN_UPGRADE_SILENCE;
			}
		}
		case CLASS_GORGE:
		case CLASS_FADE:
		case CLASS_ONOS:
		{
			if (randbool())
			{
				return IMPULSE_ALIEN_UPGRADE_CELERITY;
			}
			else
			{
				return IMPULSE_ALIEN_UPGRADE_ADRENALINE;
			}
		}
		default:
			return 0;
		}

	}

	if (TechType == HIVE_TECH_SENSORY)
	{
		switch (pBot->bot_ns_class)
		{
		case CLASS_GORGE:
			return IMPULSE_ALIEN_UPGRADE_CLOAK;
		case CLASS_SKULK:
		case CLASS_FADE:
		case CLASS_ONOS:
		{
			if (randbool())
			{
				return IMPULSE_ALIEN_UPGRADE_CLOAK;
			}
			else
			{
				return IMPULSE_ALIEN_UPGRADE_FOCUS;
			}
		}
		default:
			return 0;
		}
	}

	return 0;
}

BotRole AlienGetBestBotRole(bot_t* pBot)
{
	// Don't switch roles if already fade/onos or those resources are potentially wasted
	if (IsPlayerFade(pBot->pEdict) || IsPlayerOnos(pBot->pEdict))
	{
		return BOT_ROLE_DESTROYER;
	}

	// Likewise for lerks
	if (IsPlayerLerk(pBot->pEdict))
	{
		return BOT_ROLE_HARASS;
	}

	int NumPlayersOnTeam = GAME_GetNumPlayersOnTeam(ALIEN_TEAM);

	if (NumPlayersOnTeam == 0) { return BOT_ROLE_DESTROYER; } // Shouldn't ever happen but let's not risk a divide by zero later on...

	// If we have enough resources, or nearly enough, and we don't have any fades already on the team then prioritise this
	if (GetPlayerResources(pBot->pEdict) > ((float)kFadeEvolutionCost * 0.8f))
	{
		int NumFadesAndOnos = GAME_GetNumPlayersOnTeamOfClass(ALIEN_TEAM, CLASS_FADE) + GAME_GetNumPlayersOnTeamOfClass(ALIEN_TEAM, CLASS_ONOS);
		int NumDestroyers = GAME_GetBotsWithRoleType(BOT_ROLE_DESTROYER, ALIEN_TEAM, pBot->pEdict);
		int Existing = NumPlayersOnTeam - NumDestroyers;

		if (Existing > 0 && ((float)NumFadesAndOnos / (float)Existing < 0.33f))
		{
			return BOT_ROLE_DESTROYER;
		}
	}

	int NumTotalResNodes = UTIL_GetNumResNodes();

	// Again, shouldn't ever have a map with no resource nodes, but avoids a potential divide by zero
	if (NumTotalResNodes == 0)
	{
		return BOT_ROLE_DESTROYER;
	}

	int NumAlienResTowers = UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER);

	int NumRemainingResNodes = NumTotalResNodes - NumAlienResTowers;

	int NumCappers = GAME_GetBotsWithRoleType(BOT_ROLE_RES_CAPPER, ALIEN_TEAM, pBot->pEdict);

	// Always have one capper on the team as long as there are nodes we can cap
	if (NumRemainingResNodes > 1 && NumCappers == 0)
	{
		return BOT_ROLE_RES_CAPPER;
	}

	// How much of the map do we currently dominate?
	float ResTowerRatio = ((float)NumAlienResTowers / (float)NumTotalResNodes);

	// If we own less than a third of the map, prioritise capping resource nodes
	if (ResTowerRatio < 0.30f && NumCappers < 3)
	{
		return BOT_ROLE_RES_CAPPER;
	}

	if (ResTowerRatio <= 0.5f)
	{
		float CapperRatio = ((float)NumCappers / (float)NumPlayersOnTeam);

		if (CapperRatio < 0.2f)
		{
			return BOT_ROLE_RES_CAPPER;
		}
	}

	/*
	if (pBot->resources >= 35)
	{
		bot_task PotentialBuildTask;
		AlienBuilderSetPrimaryTask(pBot, &PotentialBuildTask);

		if (PotentialBuildTask.TaskType == TASK_BUILD && PotentialBuildTask.StructureType == STRUCTURE_ALIEN_HIVE)
		{
			memcpy(&pBot->PrimaryBotTask, &PotentialBuildTask, sizeof(bot_task));
			return BOT_ROLE_BUILDER;
		}
	}*/

	int NumRequiredBuilders = CalcNumAlienBuildersRequired();
	int NumBuilders = GAME_GetBotsWithRoleType(BOT_ROLE_BUILDER, ALIEN_TEAM, pBot->pEdict);

	if (NumBuilders < NumRequiredBuilders)
	{
		return BOT_ROLE_BUILDER;
	}

	return BOT_ROLE_DESTROYER;
}

int CalcNumAlienBuildersRequired()
{
	// If we have all 3 hives up and all have chambers, then we're probably in good shape and only need one token builder to polish off
	if (UTIL_GetNumActiveHives() > 2 && UTIL_GetFirstHiveWithoutTech() == nullptr)
	{
		return 1;
	}

	// Roughly want 1/5 players to be builders, rounded up
	return (int)ceilf((float)GAME_GetNumPlayersOnTeam(ALIEN_TEAM) * 0.2f);
}

BotRole AlienGetBestCombatModeRole(const bot_t* pBot)
{
	int NumDefenders = GAME_GetBotsWithRoleType(BOT_ROLE_BUILDER, ALIEN_TEAM, pBot->pEdict);

	if (NumDefenders < 1)
	{
		return BOT_ROLE_BUILDER;
	}

	return BOT_ROLE_DESTROYER;
}

void OnAlienLevelUp(bot_t* pBot)
{
	if (pBot->BotNextCombatUpgrade == COMBAT_ALIEN_UPGRADE_NONE)
	{
		pBot->BotNextCombatUpgrade = (int)AlienGetNextCombatUpgrade(pBot);
	}

	if (pBot->BotNextCombatUpgrade != COMBAT_ALIEN_UPGRADE_NONE)
	{
		int cost = GetAlienCombatUpgradeCost((CombatModeAlienUpgrade)pBot->BotNextCombatUpgrade);

		if (GetBotAvailableCombatPoints(pBot) >= cost)
		{
			pBot->pEdict->v.impulse = GetImpulseForAlienCombatUpgrade((CombatModeAlienUpgrade)pBot->BotNextCombatUpgrade);
			pBot->CombatUpgradeMask |= pBot->BotNextCombatUpgrade;
			pBot->NumUpgradePoints -= cost;
			pBot->BotNextCombatUpgrade = 0;
		}
	}
}

CombatModeAlienUpgrade AlienGetNextCombatUpgrade(bot_t* pBot)
{
	// Always get carapace as first upgrade regardless, for survivability
	if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_CARAPACE))
	{
		return COMBAT_ALIEN_UPGRADE_CARAPACE;
	}

	int NumAvailablePoints = GetBotAvailableCombatPoints(pBot);

	// If we are defending our base and are not gorge yet, make sure we save a point at all times so we can evolve when we want to
	if (pBot->CurrentRole == BOT_ROLE_BUILDER && !IsPlayerGorge(pBot->pEdict))
	{
		// We need one point to evolve into a gorge
		if (NumAvailablePoints <= 1) { return COMBAT_ALIEN_UPGRADE_NONE; }
	}

	// Get adrenaline if we're defending
	if (pBot->CurrentRole == BOT_ROLE_BUILDER)
	{
		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ADRENALINE))
		{
			return COMBAT_ALIEN_UPGRADE_ADRENALINE;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_REGENERATION))
		{
			return COMBAT_ALIEN_UPGRADE_REGENERATION;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_CELERITY))
		{
			return COMBAT_ALIEN_UPGRADE_CELERITY;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_CLOAKING))
		{
			return COMBAT_ALIEN_UPGRADE_CLOAKING;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_SILENCE))
		{
			return COMBAT_ALIEN_UPGRADE_SILENCE;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_REDEMPTION))
		{
			return COMBAT_ALIEN_UPGRADE_REDEMPTION;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ABILITY3))
		{
			return COMBAT_ALIEN_UPGRADE_ABILITY3;
		}

		return COMBAT_ALIEN_UPGRADE_NONE;
	}
	
	// Here we decide if we want to evolve to onos eventually, or stay as a jacked-up fade
	if (pBot->CurrentRole == BOT_ROLE_DESTROYER)
	{
		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_FADE) && !(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ONOS))
		{
			// Random 50:50 chance of going onos or staying fade (this is permanent, not re-evaluated)
			if (randbool())
			{
				pBot->CombatUpgradeMask |= COMBAT_ALIEN_UPGRADE_FADE;
			}
			else
			{
				//pBot->CombatUpgradeMask |= COMBAT_ALIEN_UPGRADE_ONOS;
				pBot->CombatUpgradeMask |= COMBAT_ALIEN_UPGRADE_FADE;
			}
		}
	}

	// If we are going onos, we want to play as fade for a bit first, as we will struggle to level up enough if we stay skulk the whole time
	if (pBot->CurrentRole == BOT_ROLE_DESTROYER && IsPlayerSkulk(pBot->pEdict))
	{
		// We must have at least 3 available points regardless of whether we're fade or onos
		if (NumAvailablePoints <= 3) { return COMBAT_ALIEN_UPGRADE_NONE; }
	}

	// Get regeneration for even more survivability and not needing to retreat to hive to heal up = bigger attacking threat
	if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_REGENERATION))
	{
		return COMBAT_ALIEN_UPGRADE_REGENERATION;
	}

	// At this point we are likely a fade with carapace and regen. Now if we want to go onos, let's do it
	if (pBot->CurrentRole == BOT_ROLE_DESTROYER && !IsPlayerOnos(pBot->pEdict) && (pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ONOS))
	{
		if (IsPlayerSkulk(pBot->pEdict) && NumAvailablePoints <= 4)
		{
			return COMBAT_ALIEN_UPGRADE_NONE;
		}

		if (IsPlayerFade(pBot->pEdict) && NumAvailablePoints <= 1)
		{
			return COMBAT_ALIEN_UPGRADE_NONE;
		}
	}

	// Get ability 3
	if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ABILITY3))
	{
		return COMBAT_ALIEN_UPGRADE_ABILITY3;
	}

	if ((pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ADRENALINE))
	{
		return COMBAT_ALIEN_UPGRADE_CELERITY;
	}

	if ((pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_CELERITY))
	{
		return COMBAT_ALIEN_UPGRADE_CELERITY;
	}

	if ((pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_FOCUS))
	{
		return COMBAT_ALIEN_UPGRADE_CELERITY;
	}

	if ((pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_SILENCE))
	{
		return COMBAT_ALIEN_UPGRADE_CELERITY;
	}


	return COMBAT_ALIEN_UPGRADE_NONE;
}
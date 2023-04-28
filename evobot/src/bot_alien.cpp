
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

	if (!pBot->CurrentTask) { pBot->CurrentTask = &pBot->PrimaryBotTask; }

	BotUpdateAndClearTasks(pBot);

	if (pBot->PrimaryBotTask.TaskType == TASK_NONE || !pBot->PrimaryBotTask.bOrderIsUrgent)
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

	if (pBot->SecondaryBotTask.TaskType == TASK_NONE || !pBot->SecondaryBotTask.bOrderIsUrgent)
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
			if (pBot->CurrentTask->TaskType == TASK_NONE || pBot->CurrentTask->TaskType == TASK_GUARD || pBot->CurrentTask->TaskType == TASK_DEFEND)
			{
				BotAttackStructure(pBot, DangerTurret);
				return;
			}
			else
			{
				Vector TaskLocation = (!FNullEnt(pBot->CurrentTask->TaskTarget)) ? pBot->CurrentTask->TaskTarget->v.origin : pBot->CurrentTask->TaskLocation;
				float DistToTurret = vDist2DSq(TaskLocation, DangerTurret->v.origin);

				if (pBot->CurrentTask->TaskType != TASK_ATTACK && DistToTurret < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
				{
					BotAttackStructure(pBot, DangerTurret);
					return;
				}

			}
		}
	}

	

	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}
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
			Task->TaskType = TASK_ATTACK;
			Task->TaskLocation = ResNode->origin;
			Task->TaskTarget = ResNode->TowerEdict;
			Task->bOrderIsUrgent = false;
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
				Task->TaskType = TASK_ATTACK;
				Task->TaskTarget = InfPortal;
				Task->TaskLocation = InfPortal->v.origin;
				Task->bOrderIsUrgent = false;
				return;
			}
		}

		edict_t* CommChair = UTIL_GetCommChair();

		if (CommChair)
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = CommChair;
			Task->TaskLocation = CommChair->v.origin;
			Task->bOrderIsUrgent = false;
			return;
		}

		edict_t* EnemyPlayer = UTIL_GetNearestPlayerOfTeamInArea(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(100.0f), MARINE_TEAM, nullptr, CLASS_NONE);

		if (!FNullEnt(EnemyPlayer))
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = EnemyPlayer;
			Task->TaskLocation = EnemyPlayer->v.origin;
			Task->bTargetIsPlayer = true;
			return;
		}

	}

	const resource_node* ResNode = UTIL_FindEligibleResNodeClosestToLocation(pBot->pEdict->v.origin, ALIEN_TEAM, !IsPlayerSkulk(pBot->pEdict));

	if (ResNode)
	{
		if (ResNode->bIsOccupied && !FNullEnt(ResNode->TowerEdict))
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskLocation = ResNode->origin;
			Task->TaskTarget = ResNode->TowerEdict;
			Task->bOrderIsUrgent = false;
			return;
		}
		else
		{
			Task->TaskType = TASK_MOVE;
			Task->TaskLocation = ResNode->origin;
			Task->bOrderIsUrgent = false;
			return;
		}
	}
	else
	{
		Task->TaskType = TASK_MOVE;
		int NavProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(NavProfile, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(50.0f));
		Task->bOrderIsUrgent = false;
		return;
	}

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
				Task->TaskType = TASK_CAP_RESNODE;
				Task->TaskLocation = EmptyResNode->origin;
				Task->StructureType = STRUCTURE_ALIEN_RESTOWER;
				Task->bOrderIsUrgent = bCappingIsUrgent;
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
			Task->TaskType = TASK_ATTACK;
			Task->TaskLocation = RandomResTower->v.origin;
			Task->TaskTarget = RandomResTower;
			Task->bOrderIsUrgent = false;
			return;
		}

		if (Task->TaskType == TASK_MOVE) { return; }

		// Attack a random resource node or move to one TODO: Prefer attacking where possible...
		Vector RandomPoint = UTIL_GetRandomPointOfInterest();

		if (RandomPoint != ZERO_VECTOR)
		{
			Task->TaskType = TASK_MOVE;
			Task->TaskLocation = RandomPoint;
			Task->bOrderIsUrgent = false;
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
		Task->TaskType = TASK_CAP_RESNODE;
		Task->TaskLocation = RandomResNode->origin;
		Task->bOrderIsUrgent = bCappingIsUrgent;
		Task->StructureType = STRUCTURE_ALIEN_RESTOWER;
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
		Vector NearestPointToHive = FindClosestNavigablePointToDestination(BUILDING_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, HiveIndex->FloorLocation, UTIL_MetresToGoldSrcUnits(3.0f));

		if (NearestPointToHive == ZERO_VECTOR)
		{
			NearestPointToHive = HiveIndex->FloorLocation;
		}

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, NearestPointToHive, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vEquals(BuildLocation, ZERO_VECTOR))
		{
			Task->TaskType = TASK_BUILD;
			Task->TaskLocation = BuildLocation;
			Task->StructureType = TechChamberToBuild;
			Task->bOrderIsUrgent = true;
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

		Vector NearestPoint = FindClosestNavigablePointToDestination(BUILDING_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, HiveIndex->FloorLocation, UTIL_MetresToGoldSrcUnits(3.0f));

		if (NearestPoint == ZERO_VECTOR)
		{
			NearestPoint = HiveIndex->FloorLocation;
		}

		Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, NearestPoint, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!vEquals(BuildLocation, ZERO_VECTOR))
		{
			Task->TaskType = TASK_BUILD;
			Task->TaskLocation = BuildLocation;
			Task->StructureType = TechChamberToBuild;
			Task->bOrderIsUrgent = true;
			return;
		}
	}

	if (!UTIL_HiveIsInProgress() && UTIL_GetNumUnbuiltHives() > 0 && !BotWithBuildTaskExists(STRUCTURE_ALIEN_HIVE))
	{
		const hive_definition* UnbuiltHiveIndex = UTIL_GetClosestViableUnbuiltHive(pEdict->v.origin);

		if (UnbuiltHiveIndex)
		{
			edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(UnbuiltHiveIndex->Location, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(10.0f), pEdict);

			if (FNullEnt(OtherGorge) || GetPlayerResources(OtherGorge) < pBot->resources)
			{
				Task->TaskType = TASK_BUILD;
				Task->TaskLocation = UnbuiltHiveIndex->FloorLocation;
				Task->StructureType = STRUCTURE_ALIEN_HIVE;
				Task->bOrderIsUrgent = false;
				char buf[64];
				sprintf(buf, "I'll drop hive at %s", UTIL_GetClosestMapLocationToPoint(Task->TaskLocation));

				BotTeamSay(pBot, 1.0f, buf);

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
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, HiveNeedsSupporting->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			Task->TaskType = TASK_BUILD;
			Task->TaskLocation = BuildLocation;
			Task->StructureType = STRUCTURE_ALIEN_DEFENCECHAMBER;
			Task->bOrderIsUrgent = false;
		}
	}

	// Make sure every hive has a movement chamber
	if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_MOVEMENT))
	{
		const hive_definition* HiveNeedsSupporting = UTIL_GetActiveHiveWithoutChambers(HIVE_TECH_MOVEMENT, 1);

		if (HiveNeedsSupporting)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, HiveNeedsSupporting->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			Task->TaskType = TASK_BUILD;
			Task->TaskLocation = BuildLocation;
			Task->StructureType = STRUCTURE_ALIEN_MOVEMENTCHAMBER;
			Task->bOrderIsUrgent = false;
		}
	}

	// Make sure every hive has a sensory chamber
	if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_SENSORY))
	{
		const hive_definition* HiveNeedsSupporting = UTIL_GetActiveHiveWithoutChambers(HIVE_TECH_SENSORY, 1);

		if (HiveNeedsSupporting)
		{
			Vector BuildLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, HiveNeedsSupporting->FloorLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			Task->TaskType = TASK_BUILD;
			Task->TaskLocation = BuildLocation;
			Task->StructureType = STRUCTURE_ALIEN_SENSORYCHAMBER;
			Task->bOrderIsUrgent = false;
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
			Task->TaskType = TASK_BUILD;
			Task->TaskLocation = BuildLocation;
			Task->StructureType = STRUCTURE_ALIEN_OFFENCECHAMBER;
			Task->bOrderIsUrgent = false;
			return;
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_DEFENCE))
		{
			int NumDefenceChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_DEFENCECHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (NumDefenceChambers < 2)
			{
				Task->TaskType = TASK_BUILD;
				Task->TaskLocation = BuildLocation;
				Task->StructureType = STRUCTURE_ALIEN_DEFENCECHAMBER;
				Task->bOrderIsUrgent = false;
				return;
			}
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_MOVEMENT))
		{
			int NumMovementChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_MOVEMENTCHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (NumMovementChambers < 1)
			{
				Task->TaskType = TASK_BUILD;
				Task->TaskLocation = BuildLocation;
				Task->StructureType = STRUCTURE_ALIEN_MOVEMENTCHAMBER;
				Task->bOrderIsUrgent = false;
				return;
			}
		}

		if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_SENSORY))
		{
			int NumSensoryChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_SENSORYCHAMBER, NearestUnprotectedResNode->origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (NumSensoryChambers < 1)
			{
				Task->TaskType = TASK_BUILD;
				Task->TaskLocation = BuildLocation;
				Task->StructureType = STRUCTURE_ALIEN_SENSORYCHAMBER;
				Task->bOrderIsUrgent = false;
				return;
			}
		}

	}
}

void AlienDestroyerSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (!IsPlayerFade(pBot->pEdict) && !IsPlayerOnos(pBot->pEdict))
	{
		if (pBot->resources >= kFadeEvolutionCost)
		{
			if (pBot->resources >= kOnosEvolutionCost)
			{
				Task->TaskType = TASK_EVOLVE;
				Task->Evolution = IMPULSE_ALIEN_EVOLVE_ONOS;
				Task->bOrderIsUrgent = true;
				return;
			}

			int NumFades = GAME_GetNumPlayersOnTeamOfClass(ALIEN_TEAM, CLASS_FADE);

			if (NumFades < 2)
			{
				Task->TaskType = TASK_EVOLVE;
				Task->Evolution = IMPULSE_ALIEN_EVOLVE_FADE;
				Task->bOrderIsUrgent = true;
				return;
			}
		}
	}

	if (IsPlayerGorge(pBot->pEdict))
	{
		Task->TaskType = TASK_EVOLVE;
		Task->Evolution = IMPULSE_ALIEN_EVOLVE_SKULK;
		Task->bOrderIsUrgent = true;
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
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = DangerStructure;
		Task->TaskLocation = DangerStructure->v.origin;
		Task->bOrderIsUrgent = true;

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
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = BlockingStructure;
		Task->TaskLocation = BlockingStructure->v.origin;
		Task->bOrderIsUrgent = false;
		return;
	}

	if (Task->TaskType == TASK_ATTACK)
	{
		return;
	}

	edict_t* InfPortal = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

	if (InfPortal)
	{
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = InfPortal;
		Task->TaskLocation = InfPortal->v.origin;
		Task->bOrderIsUrgent = false;
		return;
	}

	edict_t* Armslab = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_ARMSLAB);

	if (Armslab)
	{
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = Armslab;
		Task->TaskLocation = Armslab->v.origin;
		Task->bOrderIsUrgent = false;
		return;
	}

	edict_t* Obs = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_OBSERVATORY);

	if (Obs)
	{
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = Obs;
		Task->TaskLocation = Obs->v.origin;
		Task->bOrderIsUrgent = false;
		return;
	}

	edict_t* CommChair = UTIL_GetCommChair();

	if (CommChair)
	{
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = CommChair;
		Task->TaskLocation = CommChair->v.origin;
		Task->bOrderIsUrgent = false;
		return;
	}

	edict_t* EnemyPlayer = UTIL_GetNearestPlayerOfTeamInArea(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(100.0f), MARINE_TEAM, nullptr, CLASS_NONE);

	if (!FNullEnt(EnemyPlayer))
	{
		Task->TaskType = TASK_ATTACK;
		Task->TaskTarget = EnemyPlayer;
		Task->TaskLocation = EnemyPlayer->v.origin;
		Task->bTargetIsPlayer = false;
		return;
	}
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
				pBot->SecondaryBotTask.bOrderIsUrgent = (HurtNearbyPlayer->v.health < (HurtNearbyPlayer->v.max_health * 0.5f));
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
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = PhaseGate;
			Task->TaskLocation = PhaseGate->v.origin;
			Task->bOrderIsUrgent = true;
			return;
		}
	}

	edict_t* TurretFactory = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYTURRETFACTORY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), !IsPlayerSkulk(pBot->pEdict), false);

	if (TurretFactory)
	{
		int NumExistingPlayers = UTIL_GetNumPlayersOfTeamInArea(TurretFactory->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), pBot->bot_team, pBot->pEdict, CLASS_GORGE, false);

		if (NumExistingPlayers < 2)
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = TurretFactory;
			Task->TaskLocation = TurretFactory->v.origin;
			Task->bOrderIsUrgent = true;
			return;
		}
	}

	edict_t* AnyMarineStructure = UTIL_FindClosestMarineStructureToLocation(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(20.0f), !IsPlayerSkulk(pBot->pEdict));

	if (AnyMarineStructure)
	{
		int NumExistingPlayers = UTIL_GetNumPlayersOfTeamInArea(AnyMarineStructure->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), pBot->bot_team, pBot->pEdict, CLASS_GORGE, false);

		if (NumExistingPlayers < 2)
		{
			Task->TaskType = TASK_ATTACK;
			Task->TaskTarget = AnyMarineStructure;
			Task->TaskLocation = AnyMarineStructure->v.origin;
			Task->bOrderIsUrgent = false;
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
			pBot->SecondaryBotTask.bOrderIsUrgent = true;
		}

		return;
	}

	edict_t* Hive = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_HIVE);

	if (!FNullEnt(Hive))
	{
		Task->TaskType = TASK_DEFEND;
		Task->TaskTarget = Hive;
		Task->TaskLocation = UTIL_GetEntityGroundLocation(Hive);
		Task->bOrderIsUrgent = true;

		return;
	}

	edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_RESTOWER);

	if (!FNullEnt(ResourceTower))
	{
		Task->TaskType = TASK_DEFEND;
		Task->TaskTarget = ResourceTower;
		Task->TaskLocation = UTIL_GetEntityGroundLocation(ResourceTower);
		Task->bOrderIsUrgent = true;

		return;
	}
}

void AlienDestroyerSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* Hive = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_HIVE);

	if (!FNullEnt(Hive))
	{
		Task->TaskType = TASK_DEFEND;
		Task->TaskTarget = Hive;
		Task->TaskLocation = UTIL_GetEntityGroundLocation(Hive);
		Task->bOrderIsUrgent = true;

		return;
	}

	edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_RESTOWER);

	if (!FNullEnt(ResourceTower))
	{
		Task->TaskType = TASK_DEFEND;
		Task->TaskTarget = ResourceTower;
		Task->TaskLocation = UTIL_GetEntityGroundLocation(ResourceTower);
		Task->bOrderIsUrgent = UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER) <= 3;

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

		//BotLookAt(pBot, EnemyLoc);

		return;
	}

	NSWeapon DesiredCombatWeapon = SkulkGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	float CurrentDistance = vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin);
	float WeaponMaxDistance = sqrf(GetMaxIdealWeaponRange(DesiredCombatWeapon));

	if (CurrentDistance > WeaponMaxDistance)
	{
		BotLookAt(pBot, CurrentEnemy);
		MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_AMBUSH);

		// We check that by leaping, we won't leave ourselves without enough energy to perform our main attack.

		float CombatWeaponEnergyCost = GetEnergyCostForWeapon(pBot->DesiredCombatWeapon);
		float RequiredEnergy = (CombatWeaponEnergyCost + GetLeapCost(pBot)) - (GetPlayerEnergyRegenPerSecond(pEdict) * 0.5f); // We allow for around .5s of regen time as well

		if (GetPlayerEnergy(pEdict) >= RequiredEnergy)
		{
			if (UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, CurrentEnemy->v.origin))
			{
				BotLeap(pBot, CurrentEnemy->v.origin);
			}
		}

		return;
	}

	pBot->DesiredCombatWeapon = DesiredCombatWeapon;

	if (GetBotCurrentWeapon(pBot) != DesiredCombatWeapon)
	{
		return;
	}

	if (GetBotCurrentWeapon(pBot) == WEAPON_SKULK_BITE)
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

		BotAttackTarget(pBot, CurrentEnemy);

		return;
	}

	MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);
	BotAttackTarget(pBot, CurrentEnemy);
	return;
}

void FadeCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0 || pBot->CurrentEnemy > 31) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy)) { return; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	float MaxHealthAndArmour = pEdict->v.max_health + GetPlayerMaxArmour(pEdict);
	float CurrentHealthAndArmour = pEdict->v.health + pEdict->v.armorvalue;

	bool bLowOnHealth = (GetPlayerOverallHealthPercent(pEdict) < 0.5f);

	// Run away if low on health
	if (bLowOnHealth)
	{
		edict_t* NearestHealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);

		if (!FNullEnt(NearestHealingSource))
		{
			float DesiredDist = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(10.0f);

			if (vDist2DSq(pBot->pEdict->v.origin, NearestHealingSource->v.origin) > sqrf(DesiredDist))
			{
				MoveTo(pBot, NearestHealingSource->v.origin, MOVESTYLE_HIDE);

				if (vDist2DSq(CurrentEnemy->v.origin, pBot->pEdict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
				{
					Vector EnemyDir = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->pEdict->v.origin);
					Vector DesiredMoveDir = UTIL_GetVectorNormal2D(pBot->desiredMovementDir);

					if (UTIL_GetDotProduct2D(EnemyDir, DesiredMoveDir) > 0.75f)
					{
						if (GetBotCurrentWeapon(pBot) != WEAPON_FADE_SWIPE)
						{
							pBot->DesiredMoveWeapon = WEAPON_FADE_SWIPE;
						}
						else
						{
							BotAttackTarget(pBot, CurrentEnemy);
						}

						return;
					}
				}

				if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_METABOLIZE))
				{
					pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;

					if (GetBotCurrentWeapon(pBot) == WEAPON_FADE_METABOLIZE)
					{
						pEdict->v.button |= IN_ATTACK;
					}
				}
				return;
			}
			else
			{
				if (TrackedEnemyRef->bCurrentlyVisible)
				{
					if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_ACIDROCKET))
					{
						pBot->DesiredCombatWeapon = WEAPON_FADE_ACIDROCKET;
					}
					else
					{
						pBot->DesiredCombatWeapon = WEAPON_FADE_SWIPE;
					}

					if (GetBotCurrentWeapon(pBot) != pBot->DesiredCombatWeapon)
					{
						return;
					}


					if (GetBotCurrentWeapon(pBot) == WEAPON_FADE_SWIPE)
					{
						if (vDist2DSq(CurrentEnemy->v.origin, NearestHealingSource->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
						{
							MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_HIDE);
						}
					}

					BotAttackTarget(pBot, CurrentEnemy);
				}
			}

			return;
		}

		if (!UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, CurrentEnemy->v.origin))
		{
			if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_METABOLIZE))
			{
				if (GetBotCurrentWeapon(pBot) != WEAPON_FADE_METABOLIZE)
				{
					pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;
				}
				else
				{
					pEdict->v.button |= IN_ATTACK;
				}
			}

			return;
		}


	}

	// If the enemy is not visible
	if (!TrackedEnemyRef->bCurrentlyVisible && !UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, CurrentEnemy->v.origin))
	{
		Vector EnemyLoc = (TrackedEnemyRef->bIsTracked) ? TrackedEnemyRef->TrackedLocation : TrackedEnemyRef->LastSeenLocation;

		MoveTo(pBot, EnemyLoc, MOVESTYLE_NORMAL);

		//BotLookAt(pBot, EnemyLoc);

		return;
	}

	NSWeapon DesiredCombatWeapon = FadeGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	pBot->DesiredCombatWeapon = DesiredCombatWeapon;

	if (IsMeleeWeapon(DesiredCombatWeapon))
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
			MoveTo(pBot, TargetLocation, MOVESTYLE_NORMAL);
		}

		if (GetBotCurrentWeapon(pBot) == DesiredCombatWeapon)
		{
			BotAttackTarget(pBot, CurrentEnemy);
		}

		return;
	}

	float CurrentDistance = vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin);
	float WeaponMaxDistance = sqrf(GetMaxIdealWeaponRange(DesiredCombatWeapon));

	if (CurrentDistance > WeaponMaxDistance)
	{
		MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);
		if (UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, CurrentEnemy->v.origin))
		{
			BotLeap(pBot, CurrentEnemy->v.origin);
		}

		return;
	}

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

	if (GetBotCurrentWeapon(pBot) == DesiredCombatWeapon)
	{
		BotAttackTarget(pBot, CurrentEnemy);
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
					pBot->DesiredMoveWeapon = MeleeWeapon;
				}
				else
				{
					BotAttackTarget(pBot, CurrentEnemy);
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
		BotAttackTarget(pBot, CurrentEnemy);
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

		edict_t* HealingSource = nullptr;

		if (pBot->bot_ns_class == CLASS_GORGE)
		{
			HealingSource = pEdict;
		}
		else
		{
			HealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);
		}


		if (!FNullEnt(HealingSource))
		{
			pBot->WantsAndNeedsTask.TaskType = TASK_GET_HEALTH;
			pBot->WantsAndNeedsTask.TaskTarget = HealingSource;
			pBot->WantsAndNeedsTask.TaskLocation = UTIL_GetFloorUnderEntity(HealingSource);
			pBot->WantsAndNeedsTask.bOrderIsUrgent = true;
			return;
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

BotRole AlienGetBestBotRole(const bot_t* pBot)
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

	if (NumPlayersOnTeam == 0) { return BOT_ROLE_DESTROYER; }

	// If we have enough resources, or nearly enough, and we don't have any fades already on the team then prioritise this
	if (GetPlayerResources(pBot->pEdict) > ((float)kFadeEvolutionCost * 0.8f))
	{
		int NumFadesAndOnos = GAME_GetNumPlayersOnTeamOfClass(ALIEN_TEAM, CLASS_FADE) + GAME_GetNumPlayersOnTeamOfClass(ALIEN_TEAM, CLASS_ONOS);
		int NumDestroyers = GAME_GetBotsWithRoleType(BOT_ROLE_DESTROYER, ALIEN_TEAM, pBot->pEdict);
		int Existing = NumPlayersOnTeam - NumDestroyers;

		if (Existing > 0 && ((float)NumFadesAndOnos / (float)Existing < 0.33))
		{
			return BOT_ROLE_DESTROYER;
		}
	}

	int NumTotalResNodes = UTIL_GetNumResNodes();

	if (NumTotalResNodes == 0)
	{
		return BOT_ROLE_DESTROYER;
	}

	int NumAlienResTowers = UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER);

	int NumRemainingResNodes = NumTotalResNodes - NumAlienResTowers;

	int NumCappers = GAME_GetBotsWithRoleType(BOT_ROLE_RES_CAPPER, ALIEN_TEAM, pBot->pEdict);

	if (NumRemainingResNodes > 1 && NumCappers == 0)
	{
		return BOT_ROLE_RES_CAPPER;
	}

	// How much of the map do we currently dominate?
	float ResTowerRatio = ((float)NumAlienResTowers / (float)NumTotalResNodes);

	// If we own less than a third of the map, prioritise capping resource nodes
	if (ResTowerRatio < 0.30f)
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
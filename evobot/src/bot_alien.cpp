
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

	if (!BotHasTaskOfType(pBot, TASK_EVOLVE) && pBot->CurrentEnemy > -1)
	{
		if (pBot->CurrentEnemy > -1)
		{
			AlienCombatThink(pBot);
			return;
		}
	}

	pBot->bRetreatForHealth = false;

	if (!pBot->CurrentTask) { pBot->CurrentTask = &pBot->PrimaryBotTask; }

	if (gpGlobals->time < pBot->NextTaskEvaluation)
	{
		if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
		{
			BotProgressTask(pBot, pBot->CurrentTask);
		}

		return;
	}

	pBot->NextTaskEvaluation = gpGlobals->time + frandrange(0.2f, 0.5f);

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

	// We don't want the bot trying to attack or defend as a gorge
	if (IsPlayerGorge(pBot->pEdict) && pBot->SecondaryBotTask.TaskType != TASK_HEAL)
	{
		UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);
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
				LerkCombatThink(pBot);
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

	if (GetBotAvailableCombatPoints(pBot) >= 1)
	{
		if ((gpGlobals->time - pBot->LastCombatTime > 5.0f) && (gpGlobals->time - pBot->LastGestateAttemptTime > 1.0f))
		{
			pBot->BotNextCombatUpgrade = (int)AlienGetNextCombatUpgrade(pBot);

			if (pBot->BotNextCombatUpgrade != COMBAT_ALIEN_UPGRADE_NONE)
			{
				int cost = GetAlienCombatUpgradeCost((CombatModeAlienUpgrade)pBot->BotNextCombatUpgrade);

				// Aliens aren't guaranteed to get their requested upgrade due to limitations on when/where they can gestate
				// See OnBotBeginGestation() for where this check is performed and the upgrade confirmed
				if (GetBotAvailableCombatPoints(pBot) >= cost)
				{
					pBot->pEdict->v.impulse = GetImpulseForAlienCombatUpgrade((CombatModeAlienUpgrade)pBot->BotNextCombatUpgrade);
					pBot->LastGestateAttemptTime = gpGlobals->time;
					return;
				}

			}
		}
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
	// If we're in the middle of building something and close enough to finish it off, then do that first
	if (IsPlayerGorge(pBot->pEdict))
	{
		edict_t* StructureBeingBuilt = nullptr;
		if ((Task->TaskType == TASK_BUILD || Task->TaskType == TASK_CAP_RESNODE))
		{
			StructureBeingBuilt = Task->TaskTarget;
		}

		if (Task->TaskType == TASK_REINFORCE_STRUCTURE) 
		{
			StructureBeingBuilt = Task->TaskSecondaryTarget;
		}

		if (!FNullEnt(StructureBeingBuilt))
		{
			if (vDist2DSq(pBot->pEdict->v.origin, StructureBeingBuilt->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				return;
			}
		}
	}


	int RequiredSourcesForHive = (IsPlayerGorge(pBot->pEdict)) ? 35 : 45;

	if ((IsPlayerSkulk(pBot->pEdict) || IsPlayerGorge(pBot->pEdict)) && GetPlayerResources(pBot->pEdict) > RequiredSourcesForHive && !UTIL_HiveIsInProgress() && UTIL_GetNumUnbuiltHives() > 0)
	{
		if (Task->TaskType == TASK_BUILD && Task->StructureType == STRUCTURE_ALIEN_HIVE) { return; }

		const hive_definition* UnbuiltHiveIndex = UTIL_GetClosestViableUnbuiltHive(pBot->pEdict->v.origin);

		if (UnbuiltHiveIndex)
		{
			bot_t* OtherBuilderBot = GetFirstBotWithBuildTask(STRUCTURE_ALIEN_HIVE, pBot->pEdict);
			edict_t* OtherBuilder = nullptr;

			if (!OtherBuilderBot)
			{
				OtherBuilder = UTIL_GetNearestPlayerOfClass(UnbuiltHiveIndex->Location, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(30.0f), pBot->pEdict);
			}
			else
			{
				OtherBuilder = OtherBuilderBot->pEdict;
			}

			if (FNullEnt(OtherBuilder) || GetPlayerResources(OtherBuilder) < GetPlayerResources(pBot->pEdict))
			{
				Vector BuildLoc = FindClosestNavigablePointToDestination(GORGE_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, UnbuiltHiveIndex->FloorLocation, UTIL_MetresToGoldSrcUnits(8.0f));

				if (BuildLoc == ZERO_VECTOR)
				{
					BuildLoc = UnbuiltHiveIndex->FloorLocation;
				}

				TASK_SetBuildTask(pBot, Task, STRUCTURE_ALIEN_HIVE, BuildLoc, false);
				return;
			}
		}
	}

	if (IsPlayerGorge(pBot->pEdict))
	{
		edict_t* IncompleteTower = UTIL_GetNearestUnbuiltStructureOfTypeInLocation(STRUCTURE_ALIEN_RESTOWER, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (!FNullEnt(IncompleteTower))
		{
			TASK_SetBuildTask(pBot, Task, IncompleteTower, false);
			return;
		}
	}

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
		if (Hive)
		{
			TASK_SetEvolveTask(pBot, Task, Hive->edict, IMPULSE_ALIEN_EVOLVE_GORGE, true);
		}
		else
		{
			TASK_SetEvolveTask(pBot, Task, pBot->pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_GORGE, true);
		}

		return;

	}

	if (pBot->CurrentRole == BOT_ROLE_HARASS && !IsPlayerLerk(pBot->pEdict) && GetBotAvailableCombatPoints(pBot) >= 2)
	{
		if (Hive)
		{
			TASK_SetEvolveTask(pBot, Task, Hive->edict, IMPULSE_ALIEN_EVOLVE_LERK, true);
		}
		else
		{
			TASK_SetEvolveTask(pBot, Task, pBot->pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_LERK, true);
		}

		return;

	}

	if (pBot->CurrentRole == BOT_ROLE_DESTROYER && IsPlayerSkulk(pBot->pEdict) && GetBotAvailableCombatPoints(pBot) >= 3)
	{
		if (Hive)
		{
			TASK_SetEvolveTask(pBot, Task, Hive->edict, IMPULSE_ALIEN_EVOLVE_FADE, true);
		}
		else
		{
			TASK_SetEvolveTask(pBot, Task, pBot->pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_FADE, true);
		}

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

	if (!IsPlayerLerk(pBot->pEdict))
	{
		if (pBot->resources >= kLerkEvolutionCost)
		{
			Vector EvolveLocation = ZERO_VECTOR;

			const hive_definition* NearestHive = UTIL_GetNearestHiveOfStatus(pBot->pEdict->v.origin, HIVE_STATUS_BUILT);

			if (NearestHive)
			{
				TASK_SetEvolveTask(pBot, Task, NearestHive->edict, IMPULSE_ALIEN_EVOLVE_LERK, true);
			}
			else
			{
				TASK_SetEvolveTask(pBot, Task, pBot->pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_LERK, true);
			}
			
			return;
		}
	}

	// Taking out arms labs gimps marines, make that next priority
	edict_t* Armslab = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_ARMSLAB);

	if (Armslab)
	{
		TASK_SetAttackTask(pBot, Task, Armslab, false);
		return;
	}

	// Then observatories
	edict_t* Obs = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_OBSERVATORY);

	if (Obs)
	{
		TASK_SetAttackTask(pBot, Task, Obs, false);
		return;
	}


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

	edict_t* EnemyPlayer = UTIL_GetNearestPlayerOfTeamInArea(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(500.0f), MARINE_TEAM, nullptr, CLASS_NONE);

	if (!FNullEnt(EnemyPlayer))
	{
		TASK_SetAttackTask(pBot, Task, EnemyPlayer, true);
		return;
	}

}

void AlienHarasserSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task)
{
	AlienDestroyerSetCombatModePrimaryTask(pBot, Task);
}

void AlienCapperSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	bool bCappingIsUrgent = UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER) < 3;

	int RequiredRes = kResourceTowerCost;

	if (!IsPlayerGorge(pBot->pEdict))
	{
		RequiredRes += kGorgeEvolutionCost;
	}

	// Have enough or nearly enough to go cap a res node
	if (pBot->resources >= (RequiredRes - 5))
	{
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
			// Already capping a node, do nothing
			if (Task->TaskType == TASK_CAP_RESNODE && (vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) <= vDist2DSq(pBot->pEdict->v.origin, RandomResNode->origin))) { return; }
			TASK_SetCapResNodeTask(pBot, Task, RandomResNode, false);
			return;
		}
	}

	// Don't have enough to cap right now, take out marine towers
	if (!IsPlayerGorge(pBot->pEdict) || PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_BILEBOMB))
	{
		edict_t* EnemyResTower = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_RESTOWER, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(200.0f), false, false);

		if (!FNullEnt(EnemyResTower))
		{
			// Don't set a new attack or move task if we have one already
			if (Task->TaskType == TASK_ATTACK && Task->TaskTarget == EnemyResTower) { return; }
			TASK_SetAttackTask(pBot, Task, EnemyResTower, false);
			return;
		}
	}
	else
	{
		TASK_SetEvolveTask(pBot, Task, pBot->pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_SKULK, true);
	}


}

bool IsAlienCapperTaskNeeded()
{
	const resource_node* RandomResNode = UTIL_FindEligibleResNodeFurthestFromLocation(UTIL_GetCommChairLocation(), ALIEN_TEAM, false);

	if (RandomResNode) { return true; }

	return false;
}

bool IsAlienBuilderTaskNeeded(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	HiveTechStatus HiveTechOne = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(0);
	HiveTechStatus HiveTechTwo = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(1);
	HiveTechStatus HiveTechThree = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(2);

	if (UTIL_ActiveHiveWithTechExists(HiveTechOne) && UTIL_GetStructureCountOfType(UTIL_GetChamberTypeForHiveTech(HiveTechOne)) < 3)
	{
		return true;
	}
	
	if (UTIL_ActiveHiveWithTechExists(HiveTechTwo) && UTIL_GetStructureCountOfType(UTIL_GetChamberTypeForHiveTech(HiveTechTwo)) < 3)
	{
		return true;
	}
	
	if (UTIL_ActiveHiveWithTechExists(HiveTechThree) && UTIL_GetStructureCountOfType(UTIL_GetChamberTypeForHiveTech(HiveTechThree)) < 3)
	{
		return true;
	}

	const hive_definition* HiveIndex = UTIL_GetFirstHiveWithoutTech();

	if (HiveIndex)
	{
		return true;
	}

	const hive_definition* NearestUnclaimedHive = UTIL_GetNearestUnbuiltHiveNeedsReinforcing(pBot);

	if (NearestUnclaimedHive != nullptr)
	{
		return true;
	}

	const resource_node* NearestUnprotectedResNode = UTIL_GetNearestResNodeNeedsReinforcing(pBot, UTIL_GetCommChairLocation());

	if (NearestUnprotectedResNode)
	{
		return true;
	}

	const resource_node* ResNode = nullptr;

	if (!IsPlayerGorge(pBot->pEdict) || PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_BILEBOMB))
	{
		ResNode = UTIL_FindEligibleResNodeClosestToLocation(pBot->pEdict->v.origin, ALIEN_TEAM, IsPlayerSkulk(pBot->pEdict));
	}
	else
	{
		ResNode = UTIL_AlienFindUnclaimedResNodeFurthestFromLocation(pBot, UTIL_GetCommChairLocation(), false);
	}

	if (ResNode)
	{
		return true;
	}

	return false;
}

void AlienBuilderSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* pEdict = pBot->pEdict;

	int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	NSStructureType TechChamberToBuild = STRUCTURE_NONE;
	const hive_definition* HiveIndex = nullptr;

	HiveTechStatus HiveTechOne = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(0);
	HiveTechStatus HiveTechTwo = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(1);
	HiveTechStatus HiveTechThree = (HiveTechStatus)CONFIG_GetHiveTechAtIndex(2);

	NSStructureType ChamberTypeOne = UTIL_GetChamberTypeForHiveTech(HiveTechOne);
	NSStructureType ChamberTypeTwo = UTIL_GetChamberTypeForHiveTech(HiveTechTwo);
	NSStructureType ChamberTypeThree = UTIL_GetChamberTypeForHiveTech(HiveTechThree);

	bool bHiveWithoutTechExists = UTIL_ActiveHiveWithoutTechExists();

	bool bCanBuildChamberTypeOne = (bHiveWithoutTechExists || UTIL_ActiveHiveWithTechExists(HiveTechOne));
	bool bCanBuildChamberTypeTwo = (bHiveWithoutTechExists || UTIL_ActiveHiveWithTechExists(HiveTechTwo));
	bool bCanBuildChamberTypeThree = (bHiveWithoutTechExists || UTIL_ActiveHiveWithTechExists(HiveTechThree));

	int NumChamberTypeOne = (bCanBuildChamberTypeOne) ? UTIL_GetNumPlacedStructuresOfType(ChamberTypeOne) : 0;
	int NumChamberTypeTwo = (bCanBuildChamberTypeTwo) ? UTIL_GetNumPlacedStructuresOfType(ChamberTypeTwo) : 0;
	int NumChamberTypeThree = (bCanBuildChamberTypeThree) ? UTIL_GetNumPlacedStructuresOfType(ChamberTypeThree) : 0;

	NSStructureType DeficitStructure = STRUCTURE_NONE;
	int DeficitSize = 0;

	if (bCanBuildChamberTypeOne && NumChamberTypeOne < 3)
	{
		DeficitStructure = ChamberTypeOne;
		DeficitSize = 3 - NumChamberTypeOne;
	}
	else if (bCanBuildChamberTypeTwo && NumChamberTypeTwo < 3)
	{
		DeficitStructure = ChamberTypeTwo;
		DeficitSize = 3 - NumChamberTypeTwo;
	}
	else if (bCanBuildChamberTypeThree && NumChamberTypeThree < 3)
	{
		DeficitStructure = ChamberTypeThree;
		DeficitSize = 3 - NumChamberTypeThree;
	}

	if (DeficitStructure != STRUCTURE_NONE)
	{
		const hive_definition* NearestHive = UTIL_GetNearestBuiltHiveToLocation(pBot->pEdict->v.origin);
		const resource_node* NearestResNode = UTIL_GetNearestCappedResNodeToLocation(pBot->pEdict->v.origin, ALIEN_TEAM, true);

		edict_t* StructureToReinforce = nullptr;

		if (NearestHive && NearestResNode)
		{
			float HiveDist = vDist2DSq(pBot->pEdict->v.origin, NearestHive->FloorLocation);
			float ResDist = vDist2DSq(pBot->pEdict->v.origin, NearestResNode->origin);

			StructureToReinforce = (HiveDist <= ResDist) ? NearestHive->edict : NearestResNode->TowerEdict;
		}
		else
		{
			StructureToReinforce = (NearestHive) ? NearestHive->edict : NearestResNode->TowerEdict;
		}

		bot_t* OtherBuilderBot = GetFirstBotWithBuildTask(DeficitStructure, pBot->pEdict);

		if (DeficitSize > 1 || !OtherBuilderBot)
		{
			TASK_SetReinforceStructureTask(pBot, Task, StructureToReinforce, DeficitStructure, false);
			return;
		}
		else
		{
			if (vDist2DSq(OtherBuilderBot->PrimaryBotTask.TaskLocation, OtherBuilderBot->pEdict->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				TASK_SetReinforceStructureTask(pBot, Task, StructureToReinforce, DeficitStructure, false);
				return;
			}
		}		
	}

	const hive_definition* NearestUnclaimedHive = UTIL_GetNearestUnbuiltHiveNeedsReinforcing(pBot);

	if (NearestUnclaimedHive != nullptr)
	{
		edict_t* HiveEdict = NearestUnclaimedHive->edict;

		TASK_SetReinforceStructureTask(pBot, Task, HiveEdict, STRUCTURE_ALIEN_OFFENCECHAMBER, false);
		return;
	}

	const resource_node* NearestUnprotectedResNode = UTIL_GetNearestResNodeNeedsReinforcing(pBot, UTIL_GetCommChairLocation());

	if (NearestUnprotectedResNode)
	{
		edict_t* ResNodeEdict = NearestUnprotectedResNode->TowerEdict;

		TASK_SetReinforceStructureTask(pBot, Task, ResNodeEdict, STRUCTURE_ALIEN_OFFENCECHAMBER, false);
		return;
	}

	const resource_node* ResNode = nullptr;

	if (!IsPlayerGorge(pBot->pEdict) || PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_BILEBOMB))
	{
		ResNode = UTIL_FindEligibleResNodeClosestToLocation(pBot->pEdict->v.origin, ALIEN_TEAM, IsPlayerSkulk(pBot->pEdict));
	}
	else
	{
		ResNode = UTIL_AlienFindUnclaimedResNodeFurthestFromLocation(pBot, UTIL_GetCommChairLocation(), false);
	}

	if (ResNode)
	{
		TASK_SetCapResNodeTask(pBot, Task, ResNode, false);
		return;
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
			int NumOnos = GAME_GetNumPlayersOnTeamOfClass(pBot->pEdict->v.team, CLASS_ONOS);
			int Evolution = IMPULSE_ALIEN_EVOLVE_FADE;

			if (pBot->resources >= kOnosEvolutionCost && NumOnos < 2)
			{
				Evolution = IMPULSE_ALIEN_EVOLVE_ONOS;
			}

			const hive_definition* NearestHive = UTIL_GetNearestHiveOfStatus(pBot->pEdict->v.origin, HIVE_STATUS_BUILT);

			if (NearestHive)
			{
				TASK_SetEvolveTask(pBot, Task, NearestHive->edict, Evolution, true);
			}
			else
			{
				TASK_SetEvolveTask(pBot, Task, pBot->pEdict->v.origin, Evolution, true);
			}
			return;
		}
	}

	if (IsPlayerGorge(pBot->pEdict))
	{
		TASK_SetEvolveTask(pBot, Task, pBot->pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_SKULK, true);
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
		TASK_SetAttackTask(pBot, Task, DangerStructure, false);
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

	// Take out the observatory first to prevent beacon and phase gates
	edict_t* Obs = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_OBSERVATORY);

	if (Obs)
	{
		TASK_SetAttackTask(pBot, Task, Obs, false);
		return;
	}

	// Taking out arms labs gimps marines, make that next priority
	edict_t* Armslab = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_ARMSLAB);

	if (Armslab)
	{
		TASK_SetAttackTask(pBot, Task, Armslab, false);
		return;
	}

	// Then infantry portals
	edict_t* InfPortal = UTIL_GetFirstCompletedStructureOfType(STRUCTURE_MARINE_INFANTRYPORTAL);

	if (InfPortal)
	{
		TASK_SetAttackTask(pBot, Task, InfPortal, false);
		return;
	}

	// And finally, the comm chair
	edict_t* CommChair = UTIL_GetCommChair();

	if (CommChair)
	{
		TASK_SetAttackTask(pBot, Task, CommChair, false);
		return;
	}

	// Hunt down any last straggling marines once everything destroyed (assuming they don't just drop to the ready room and surrender)
	edict_t* EnemyPlayer = UTIL_GetNearestPlayerOfTeamInArea(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(500.0f), MARINE_TEAM, nullptr, CLASS_NONE);

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
		return;
	}

	// Don't let anything distract us from building hives. They're kind of important...
	if (pBot->PrimaryBotTask.TaskType == TASK_BUILD && pBot->PrimaryBotTask.StructureType == STRUCTURE_ALIEN_HIVE)
	{
		UTIL_ClearBotTask(pBot, Task);
		return;
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

	UTIL_ClearBotTask(pBot, Task);
}

void AlienHarasserSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* DangerStructure = UTIL_GetAnyStructureOfTypeNearActiveHive(STRUCTURE_MARINE_PHASEGATE, true, false);

	if (FNullEnt(DangerStructure))
	{
		DangerStructure = UTIL_GetAnyStructureOfTypeNearActiveHive(STRUCTURE_MARINE_ANYTURRETFACTORY, true, false);

		if (FNullEnt(DangerStructure))
		{
			DangerStructure = UTIL_GetAnyStructureOfTypeNearActiveHive(STRUCTURE_ANY_MARINE_STRUCTURE, true, false);
		}
	}

	if (!FNullEnt(DangerStructure))
	{
		TASK_SetAttackTask(pBot, Task, DangerStructure, true);
		return;
	}


	edict_t* Hive = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_HIVE, true);

	if (!FNullEnt(Hive))
	{
		// Don't rush to defend if there isn't an enemy actively attacking it (i.e. is turret doing damage)
		if (UTIL_AnyPlayerOnTeamHasLOSToLocation(MARINE_TEAM, Hive->v.origin, UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			TASK_SetDefendTask(pBot, Task, Hive, true);
			return;
		}
	}

	edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_RESTOWER, true);

	if (!FNullEnt(ResourceTower))
	{
		// Don't rush to defend if there isn't an enemy actively attacking it (i.e. is turret doing damage)
		if (UTIL_AnyPlayerOnTeamHasLOSToLocation(MARINE_TEAM, UTIL_GetCentreOfEntity(ResourceTower), UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			TASK_SetDefendTask(pBot, Task, ResourceTower, true);
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

	edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_RESTOWER, true);

	if (!FNullEnt(ResourceTower))
	{
		if (!UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_SIEGETURRET, ResourceTower->v.origin, UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			if (Task->TaskType == TASK_DEFEND && Task->TaskTarget == ResourceTower) { return; }
			TASK_SetDefendTask(pBot, Task, ResourceTower, true);
			return;
		}
	}

	edict_t* Hive = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_HIVE, true);

	if (!FNullEnt(Hive))
	{
		if (!UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_SIEGETURRET, Hive->v.origin, UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			if (Task->TaskType == TASK_DEFEND && Task->TaskTarget == Hive) { return; }
			TASK_SetDefendTask(pBot, Task, Hive, true);
			return;
		}
	}
}

void AlienDestroyerSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	// Don't defend stuff if we're onos, we have smash smash to do
	if (IsPlayerOnos(pBot->pEdict)) { return; }

	// Don't defend if we're a lowly skulk and could go fade/onos, prioritise that first
	if (IsPlayerSkulk(pBot->pEdict) && GetPlayerResources(pBot->pEdict) >= kFadeEvolutionCost)
	{
		return;
	}

	// Don't interrupt an attack task as destroyer if we're already close to attacking. Means fades and onos will focus on doing what they do best
	if (pBot->PrimaryBotTask.TaskType == TASK_ATTACK && vDist2DSq(pBot->pEdict->v.origin, pBot->PrimaryBotTask.TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { return; }

	edict_t* Hive = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_HIVE, true);

	if (!FNullEnt(Hive))
	{
		// Don't rush to defend the hive if it's under siege, primary task will focus on taking out the infrastructure
		if (!UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_SIEGETURRET, Hive->v.origin, UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			TASK_SetDefendTask(pBot, Task, Hive, true);
			return;
		}
	}
		
	edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ALIEN_RESTOWER, true);

	if (!FNullEnt(ResourceTower))
	{
		// Don't rush to defend the RT if it's under siege, primary task will focus on taking out siege bases
		if (!UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_SIEGETURRET, ResourceTower->v.origin, UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			bool bIsUrgent = (UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER) <= 3);
			TASK_SetDefendTask(pBot, Task, ResourceTower, bIsUrgent);
			return;
		}
	}
}

void SkulkCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0 || pBot->CurrentEnemy > 31) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	if (FNullEnt(CurrentEnemy) || !TrackedEnemyRef || IsPlayerDead(CurrentEnemy)) { return; }

	NSWeapon DesiredCombatWeapon = SkulkGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, CurrentEnemy);

	if (LOSCheck == ATTACK_SUCCESS)
	{
		BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
	}

	float DistToEnemy = vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin);

	if (DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		Vector MoveTarget = CurrentEnemy->v.origin;

		if (TrackedEnemyRef->bHasLOS && DistToEnemy < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
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
					MoveTarget = BehindPlayer;
				}
			}
		}

		MoveTo(pBot, MoveTarget, MOVESTYLE_NORMAL);

		if (TrackedEnemyRef->bHasLOS && DistToEnemy > sqrf(UTIL_MetresToGoldSrcUnits(3.0f)) && CanBotLeap(pBot) && UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, UTIL_GetFloorUnderEntity(CurrentEnemy)))
		{
			BotLeap(pBot, CurrentEnemy->v.origin);
		}

		return;
	}

	int NumEnemyAllies = UTIL_GetNumPlayersOnTeamWithLOS(CurrentEnemy->v.origin, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(30.0f), CurrentEnemy);
	int NumFriends = UTIL_GetNumPlayersOfTeamInArea(CurrentEnemy->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), ALIEN_TEAM, pBot->pEdict, CLASS_GORGE, false);

	edict_t* HealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pBot->pEdict->v.origin);

	bool bHealingSourceIsStructure = (!FNullEnt(HealingSource)) ? IsEdictStructure(HealingSource) : false;

	// If we have backup, or we're near a source of healing then charge in
	if (NumFriends >= NumEnemyAllies || ( bHealingSourceIsStructure && vDist2DSq(CurrentEnemy->v.origin, HealingSource->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)) ) )
	{
		MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);

		if (TrackedEnemyRef->bHasLOS)
		{
			if (CanBotLeap(pBot) && UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, UTIL_GetFloorUnderEntity(CurrentEnemy)))
			{
				BotLeap(pBot, CurrentEnemy->v.origin);
				return;
			}

			Vector EnemyFacing = UTIL_GetForwardVector2D(CurrentEnemy->v.angles);
			Vector BotFacing = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->pEdict->v.origin);

			float Dot = UTIL_GetDotProduct2D(EnemyFacing, BotFacing);

			if (Dot < 0.0f && UTIL_GetBotCurrentPathArea(pBot) == SAMPLE_POLYAREA_GROUND)
			{
				Vector RightDir = UTIL_GetCrossProduct(pBot->desiredMovementDir, UP_VECTOR);

				pBot->desiredMovementDir = (pBot->BotNavInfo.bZig) ? UTIL_GetVectorNormal2D(pBot->desiredMovementDir + RightDir) : UTIL_GetVectorNormal2D(pBot->desiredMovementDir - RightDir);

				if (gpGlobals->time > pBot->BotNavInfo.NextZigTime)
				{
					pBot->BotNavInfo.bZig = !pBot->BotNavInfo.bZig;
					pBot->BotNavInfo.NextZigTime = gpGlobals->time + frandrange(0.5f, 1.0f);
				}

				BotMovementInputs(pBot);
				BotJump(pBot);
			}
		}



		return;
	}

	if (DoesAnyPlayerOnTeamHaveLOSToPlayer(MARINE_TEAM, pEdict))
	{
		int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_HIDE);

		Vector EscapeLocation = ZERO_VECTOR;//pBot->LastSafeLocation;

		if (!EscapeLocation)
		{
			const hive_definition* NearestHive = UTIL_GetNearestHiveAtLocation(pBot->pEdict->v.origin);

			if (NearestHive)
			{
				EscapeLocation = NearestHive->FloorLocation;
			}
		}

		MoveTo(pBot, EscapeLocation, MOVESTYLE_NORMAL);

		return;
	}

	if (TrackedEnemyRef->LastLOSPosition != ZERO_VECTOR)
	{
		BotLookAt(pBot, TrackedEnemyRef->LastSeenLocation);

		return;
	}

	MoveTo(pBot, TrackedEnemyRef->LastFloorPosition, MOVESTYLE_AMBUSH);
	
}

void FadeCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy) || !IsPlayerActiveInGame(CurrentEnemy)) { return; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	float MaxHealthAndArmour = pEdict->v.max_health + GetPlayerMaxArmour(pEdict);
	float CurrentHealthAndArmour = pEdict->v.health + pEdict->v.armorvalue;

	float OverallHealthPercent = GetPlayerOverallHealthPercent(pEdict);

	bool bLowOnHealth = (OverallHealthPercent < 0.5f);
	bool bNeedsHealth = (OverallHealthPercent < 0.9f);

	if (!bNeedsHealth)
	{
		pBot->bRetreatForHealth = false;
	}

	if (pBot->bRetreatForHealth)
	{
		edict_t* NearestHealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);

		if (!FNullEnt(NearestHealingSource))
		{
			float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(8.0f);

			bool bOutOfEnemyLOS = !DoesPlayerHaveLOSToPlayer(CurrentEnemy, pEdict);

			float DistFromHealingSourceSq = vDist2DSq(pBot->pEdict->v.origin, NearestHealingSource->v.origin);

			bool bInHealingRange = (DistFromHealingSourceSq <= sqrf(DesiredDistFromHealingSource));

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

				if (bInHealingRange)
				{
					BotGuardLocation(pBot, NearestHealingSource->v.origin);
				}
				else
				{
					MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				}

				return;
			}

			if (!bInHealingRange)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				return;
			}
		}		
	}

	if (bLowOnHealth)
	{
		pBot->bRetreatForHealth = true;
	}

	// If the enemy is not visible
	if (!TrackedEnemyRef->bHasLOS)
	{
		MoveTo(pBot, TrackedEnemyRef->LastFloorPosition, MOVESTYLE_NORMAL);

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
		if (LOSCheck == ATTACK_OUTOFRANGE)
		{
			if (PlayerHasWeapon(pEdict, WEAPON_FADE_BLINK) && UTIL_PointIsDirectlyReachable(pEdict->v.origin, CurrentEnemy->v.origin))
			{
				MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);
				BotLeap(pBot, CurrentEnemy->v.origin);
				return;
			}
		}

		if (vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
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
					return;
				}				
			}
		}

		MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);

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

	if (TrackedEnemyRef->bHasLOS)
	{
		NSWeapon DesiredWeapon = GorgeGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

		BotShootTarget(pBot, DesiredWeapon, CurrentEnemy);
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

void LerkCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy) || !IsPlayerActiveInGame(CurrentEnemy)) { return; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	float HealthPercent = GetPlayerOverallHealthPercent(pEdict);

	bool bLowOnHealth = (HealthPercent < 0.5f);
	bool bNeedsHealth = (HealthPercent < 0.9f);

	if (!bNeedsHealth)
	{
		pBot->bRetreatForHealth = false;
	}

	if (pBot->bRetreatForHealth)
	{
		edict_t* NearestHealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);

		// Run away if low on health and have a healing spot
		if (!FNullEnt(NearestHealingSource))
		{
			edict_t* NearestHealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);

			if (!FNullEnt(NearestHealingSource))
			{
				float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(8.0f);

				bool bOutOfEnemyLOS = !DoesPlayerHaveLOSToPlayer(CurrentEnemy, pEdict);

				float DistFromHealingSourceSq = vDist2DSq(pBot->pEdict->v.origin, NearestHealingSource->v.origin);

				bool bInHealingRange = (DistFromHealingSourceSq <= sqrf(DesiredDistFromHealingSource));

				Vector SporeLocation = (TrackedEnemyRef->bHasLOS) ? TrackedEnemyRef->LastSeenLocation : TrackedEnemyRef->LastLOSPosition;

				// We will cover our tracks with spores if we have a valid target location, we have enough energy, the area isn't affected by spores already and we have LOS to the spore location
				bool bCanSpore = (SporeLocation != ZERO_VECTOR && pBot->Adrenaline > (GetEnergyCostForWeapon(WEAPON_LERK_SPORES) * 1.1f) && !UTIL_IsAreaAffectedBySpores(SporeLocation) && UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, SporeLocation));

				// If we are super low on health then just get the hell out of there
				if (HealthPercent <= 0.2) { bCanSpore = false; }

				if (bOutOfEnemyLOS)
				{
					if (bInHealingRange)
					{
						BotGuardLocation(pBot, NearestHealingSource->v.origin);

						if (bCanSpore)
						{
							BotShootLocation(pBot, WEAPON_LERK_SPORES, SporeLocation);
						}

					}
					else
					{
						MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

						if (bCanSpore)
						{
							BotShootLocation(pBot, WEAPON_LERK_SPORES, SporeLocation);
						}
					}

					return;
				}

				if (!bInHealingRange)
				{
					MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);

					if (bCanSpore)
					{
						BotShootLocation(pBot, WEAPON_LERK_SPORES, SporeLocation);
					}

					return;
				}

				
			}
		}
	}

	if (bLowOnHealth)
	{
		pBot->bRetreatForHealth = true;
		return;
	}

	// How many allies does our target have providing cover? We don't want to charge in and get shot to pieces
	int NumFriends = UTIL_GetNumPlayersOnTeamWithLOS(CurrentEnemy->v.origin, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(30.0f), CurrentEnemy);
	

	// If the enemy is not visible
	if (!TrackedEnemyRef->bHasLOS)
	{
		// Our target has at least one friend covering them, or we're weaker than the target
		if (NumFriends > 0 || HealthPercent < GetPlayerOverallHealthPercent(CurrentEnemy))
		{
			pBot->DesiredCombatWeapon = WEAPON_LERK_SPORES;

			if (pBot->Adrenaline > (GetEnergyCostForWeapon(WEAPON_LERK_SPORES) * 1.1f) && GetBotCurrentWeapon(pBot) == WEAPON_LERK_SPORES && (gpGlobals->time - pBot->current_weapon.LastFireTime >= pBot->current_weapon.MinRefireTime) && !UTIL_IsAreaAffectedBySpores(TrackedEnemyRef->LastSeenLocation))
			{
				Vector MoveLocation = (TrackedEnemyRef->LastLOSPosition != ZERO_VECTOR) ? TrackedEnemyRef->LastLOSPosition : TrackedEnemyRef->LastSeenLocation;
				BotMoveStyle MoveStyle = (vDist2DSq(pBot->pEdict->v.origin, MoveLocation) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) ? MOVESTYLE_NORMAL : MOVESTYLE_HIDE;

				MoveTo(pBot, MoveLocation, MoveStyle);
			}
			else
			{
				BotLookAt(pBot, TrackedEnemyRef->LastSeenLocation);

				if (pBot->Adrenaline > (GetEnergyCostForWeapon(WEAPON_LERK_SPORES) * 1.1f) && TrackedEnemyRef->LastLOSPosition != ZERO_VECTOR && vDist2DSq(CurrentEnemy->v.origin, TrackedEnemyRef->LastLOSPosition) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, TrackedEnemyRef->LastLOSPosition) && !UTIL_IsAreaAffectedBySpores(TrackedEnemyRef->LastLOSPosition))
				{
					BotShootLocation(pBot, WEAPON_LERK_SPORES, TrackedEnemyRef->LastLOSPosition);
				}
			}
			return;
		}

		// Target doesn't have any backup, go for the kill
		if (!TrackedEnemyRef->LastLOSPosition || vDist2DSq(pBot->pEdict->v.origin, TrackedEnemyRef->LastLOSPosition) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			MoveTo(pBot, TrackedEnemyRef->LastFloorPosition, MOVESTYLE_NORMAL);
		}
		else
		{
			MoveTo(pBot, TrackedEnemyRef->LastFloorPosition, MOVESTYLE_HIDE);
		}

		return;
	}

	// Enemy is in LOS

	// Target has people covering them or we're weaker than they are. Fire off some spores and get into cover
	if (NumFriends > 0 || HealthPercent < GetPlayerOverallHealthPercent(CurrentEnemy))
	{
		int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_HIDE);

		Vector EscapeLocation = pBot->LastSafeLocation;

		if (!EscapeLocation)
		{
			const hive_definition* NearestHive = UTIL_GetNearestHiveAtLocation(pBot->pEdict->v.origin);

			if (NearestHive)
			{
				EscapeLocation = NearestHive->FloorLocation;
			}
		}

		pBot->DesiredCombatWeapon = WEAPON_LERK_SPORES;

		if (pBot->Adrenaline > (GetEnergyCostForWeapon(WEAPON_LERK_SPORES) * 1.1f) && GetBotCurrentWeapon(pBot) == WEAPON_LERK_SPORES && (gpGlobals->time - pBot->current_weapon.LastFireTime >= pBot->current_weapon.MinRefireTime) && !UTIL_IsAreaAffectedBySpores(TrackedEnemyRef->LastSeenLocation))
		{			
			BotShootTarget(pBot, WEAPON_LERK_SPORES, TrackedEnemyRef->EnemyEdict);
			return;
		}

		if (vDist2DSq(pBot->pEdict->v.origin, EscapeLocation) > sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
		{
			
			MoveTo(pBot, EscapeLocation, MOVESTYLE_NORMAL);
		}
		else
		{
			MoveTo(pBot, EscapeLocation, MOVESTYLE_HIDE);
		}

		return;
	}

	// Enemy has no backup, get in and attack

	NSWeapon DesiredCombatWeapon = LerkGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, CurrentEnemy);

	if (LOSCheck == ATTACK_SUCCESS)
	{
		BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
		if (DesiredCombatWeapon == WEAPON_LERK_SPORES)
		{
			return;
		}
	}

	if (vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
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
				return;
			}
		}
	}

	MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);

}

void OnosCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

	if (FNullEnt(CurrentEnemy) || !IsPlayerActiveInGame(CurrentEnemy)) { return; }

	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	bool bLowOnHealth = (GetPlayerOverallHealthPercent(pEdict) < 0.35f);
	bool bNeedsHealth = (GetPlayerOverallHealthPercent(pEdict) < 0.9f);

	if (!bNeedsHealth)
	{
		pBot->bRetreatForHealth = false;
	}

	if (pBot->bRetreatForHealth)
	{
		edict_t* NearestHealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);

		if (!FNullEnt(NearestHealingSource))
		{
			float DesiredDistFromHealingSource = (IsEdictPlayer(NearestHealingSource)) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(8.0f);

			bool bOutOfEnemyLOS = !DoesPlayerHaveLOSToPlayer(CurrentEnemy, pEdict);

			float DistFromHealingSourceSq = vDist2DSq(pBot->pEdict->v.origin, NearestHealingSource->v.origin);

			bool bInHealingRange = (DistFromHealingSourceSq <= sqrf(DesiredDistFromHealingSource));

			if (bOutOfEnemyLOS)
			{
				if (bInHealingRange)
				{
					BotGuardLocation(pBot, NearestHealingSource->v.origin);
				}
				else
				{
					MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				}

				return;
			}

			if (!bInHealingRange)
			{
				MoveTo(pBot, UTIL_GetEntityGroundLocation(NearestHealingSource), MOVESTYLE_NORMAL, DesiredDistFromHealingSource);
				return;
			}
		}
	}

	if (bLowOnHealth)
	{
		pBot->bRetreatForHealth = true;
	}

	// If the enemy is not visible
	if (!TrackedEnemyRef->bHasLOS)
	{
		MoveTo(pBot, TrackedEnemyRef->LastFloorPosition, MOVESTYLE_NORMAL);

		return;
	}

	NSWeapon DesiredCombatWeapon = OnosGetBestWeaponForCombatTarget(pBot, CurrentEnemy);

	BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, CurrentEnemy);

	if (LOSCheck == ATTACK_SUCCESS)
	{
		BotShootTarget(pBot, DesiredCombatWeapon, CurrentEnemy);
	}

	MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL, 200.0f);
}

void AlienCheckWantsAndNeeds(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

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

	float OverallHealthPercent = (CurrentHealthAndArmour / MaxHealthAndArmour);

	bool bLowOnHealth = (OverallHealthPercent <= 0.5f);

	// Don't go hunting for health as a gorge, can heal themselves quickly. Fades will because metabolise isn't enough
	if (bLowOnHealth && !IsPlayerGorge(pBot->pEdict))
	{
		// Already getting health
		if (pBot->WantsAndNeedsTask.TaskType == TASK_GET_HEALTH && (OverallHealthPercent > 0.35f || IsEdictStructure(pBot->WantsAndNeedsTask.TaskTarget)) )
		{
			return;
		}

		edict_t* HealingSource = nullptr;
		
		// If we're REALLY low on health then go to the hive, don't try to heal at a gorge
		if (OverallHealthPercent > 0.35f)
		{
			HealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);
		}
		else
		{
			const hive_definition* NearestHive = UTIL_GetNearestBuiltHiveToLocation(pBot->pEdict->v.origin);

			if (NearestHive)
			{
				HealingSource = NearestHive->edict;
			}
			else
			{
				HealingSource = UTIL_AlienFindNearestHealingSpot(pBot, pEdict->v.origin);
			}
		}

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
		case CLASS_LERK:
		case CLASS_GORGE: // Gorges can heal themselves so regen not worth it. Redemption probably not worth it at 10 res cost to evolve
			return IMPULSE_ALIEN_UPGRADE_CARAPACE; // Lerks are fragile so best get carapace while the bot is still not great at staying alive with them...
		case CLASS_FADE:
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
		case CLASS_LERK:
		case CLASS_GORGE:
			return IMPULSE_ALIEN_UPGRADE_ADRENALINE;
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
		case CLASS_FADE:
		case CLASS_LERK:
		case CLASS_ONOS:
			return IMPULSE_ALIEN_UPGRADE_FOCUS;
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
	if (GetPlayerResources(pBot->pEdict) >= ((float)kFadeEvolutionCost * 0.8f))
	{
		if (GetPlayerResources(pBot->pEdict) > 60) { return BOT_ROLE_DESTROYER; }

		int NumFadesAndOnos = GAME_GetNumPlayersOnTeamOfClass(ALIEN_TEAM, CLASS_FADE) + GAME_GetNumPlayersOnTeamOfClass(ALIEN_TEAM, CLASS_ONOS);
		int NumDestroyers = GAME_GetBotsWithRoleType(BOT_ROLE_DESTROYER, ALIEN_TEAM, pBot->pEdict);
		int Existing = NumPlayersOnTeam - NumDestroyers;

		if (Existing > 0 && ((float)NumFadesAndOnos / (float)Existing < 0.33f))
		{
			return BOT_ROLE_DESTROYER;
		}
	}

	// If we have enough resources, or nearly enough, and we don't have any lerks already on the team then prioritise this
	if (GetPlayerResources(pBot->pEdict) >= ((float)kLerkEvolutionCost * 0.8f))
	{
		int NumLerks = GAME_GetNumPlayersOnTeamOfClass(ALIEN_TEAM, CLASS_LERK);
		int NumHarassers = GAME_GetBotsWithRoleType(BOT_ROLE_HARASS, ALIEN_TEAM, pBot->pEdict);

		if (NumLerks + NumHarassers < 1)
		{
			return BOT_ROLE_HARASS;
		}
	}

	int NumTotalResNodes = UTIL_GetNumResNodes();

	// Again, shouldn't ever have a map with no resource nodes, but avoids a potential divide by zero
	if (NumTotalResNodes == 0)
	{
		return BOT_ROLE_DESTROYER;
	}

	// Don't go capper if we have nothing to cap...
	if (IsAlienCapperTaskNeeded())
	{
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
	}

	// Don't go builder if we have nothing to build...
	if (IsAlienBuilderTaskNeeded(pBot))
	{
		int NumRequiredBuilders = CalcNumAlienBuildersRequired();
		int NumBuilders = GAME_GetBotsWithRoleType(BOT_ROLE_BUILDER, ALIEN_TEAM, pBot->pEdict);

		if (NumBuilders < NumRequiredBuilders)
		{
			return BOT_ROLE_BUILDER;
		}
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

	int NumHarassers = GAME_GetBotsWithRoleType(BOT_ROLE_HARASS, ALIEN_TEAM, pBot->pEdict);

	if (NumHarassers < 1)
	{
		return BOT_ROLE_HARASS;
	}

	return BOT_ROLE_DESTROYER;
}

CombatModeAlienUpgrade AlienGetNextCombatUpgrade(bot_t* pBot)
{
	int NumAvailablePoints = GetBotAvailableCombatPoints(pBot);

	// Get adrenaline if we're defending
	if (pBot->CurrentRole == BOT_ROLE_HARASS)
	{
		// If we are defending our base and are not gorge yet, make sure we save a point at all times so we can evolve when we want to
		if (!IsPlayerLerk(pBot->pEdict))
		{
			// We need two points to evolve into a lerk
			if (NumAvailablePoints <= 2) { return COMBAT_ALIEN_UPGRADE_NONE; }
		}

		// Always get carapace as first upgrade regardless, for survivability
		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_CARAPACE))
		{
			return COMBAT_ALIEN_UPGRADE_CARAPACE;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ADRENALINE))
		{
			return COMBAT_ALIEN_UPGRADE_ADRENALINE;
		}

		// Get ability 3
		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ABILITY3))
		{
			return COMBAT_ALIEN_UPGRADE_ABILITY3;
		}

		// Get ability 3
		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ABILITY4))
		{
			return COMBAT_ALIEN_UPGRADE_ABILITY4;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_CELERITY))
		{
			return COMBAT_ALIEN_UPGRADE_CELERITY;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_FOCUS))
		{
			return COMBAT_ALIEN_UPGRADE_FOCUS;
		}

		return COMBAT_ALIEN_UPGRADE_NONE;
	}



	// Always get carapace as first upgrade regardless, for survivability
	if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_CARAPACE))
	{
		return COMBAT_ALIEN_UPGRADE_CARAPACE;
	}

	

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

		// Bile bomb to help take down heavies
		if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ABILITY3))
		{
			return COMBAT_ALIEN_UPGRADE_ABILITY3;
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
				pBot->CombatUpgradeMask |= COMBAT_ALIEN_UPGRADE_ONOS;
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

	if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ADRENALINE))
	{
		return COMBAT_ALIEN_UPGRADE_ADRENALINE;
	}

	if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_CELERITY))
	{
		return COMBAT_ALIEN_UPGRADE_CELERITY;
	}

	if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_FOCUS))
	{
		return COMBAT_ALIEN_UPGRADE_FOCUS;
	}

	if (!(pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_SILENCE))
	{
		return COMBAT_ALIEN_UPGRADE_SILENCE;
	}


	return COMBAT_ALIEN_UPGRADE_NONE;
}
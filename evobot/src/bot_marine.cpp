//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_marine.cpp
// 
// Contains marine-specific logic.
//

#include "bot_marine.h"

#include <extdll.h>
#include <dllapi.h>

#include "bot_tactical.h"
#include "bot_navigation.h"
#include "bot_config.h"
#include "bot_util.h"
#include "bot_task.h"
#include "general_util.h"
#include "bot_weapons.h"
#include "game_state.h"

extern edict_t* clients[32];

void MarineThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	// Don't engage enemies if attempting to take command
	if (pBot->CurrentEnemy > -1)
	{
		if (MarineCombatThink(pBot))
		{
			if (pBot->DesiredCombatWeapon == WEAPON_NONE)
			{
				pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict);
			}

			return;
		}
	}

	if (!pBot->CurrentTask) { pBot->CurrentTask = &pBot->PrimaryBotTask; }

	edict_t* DangerTurret = BotGetNearestDangerTurret(pBot, UTIL_MetresToGoldSrcUnits(15.0f));

	if (!FNullEnt(DangerTurret))
	{
		Vector TaskLocation = (!FNullEnt(pBot->CurrentTask->TaskTarget)) ? pBot->CurrentTask->TaskTarget->v.origin : pBot->CurrentTask->TaskLocation;
		float DistToTurret = vDist2DSq(TaskLocation, DangerTurret->v.origin);

		if (pBot->CurrentTask->TaskType != TASK_ATTACK && DistToTurret < sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
		{
			BotAttackTarget(pBot, DangerTurret);
			return;
		}
	}

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

	if (pBot->PrimaryBotTask.TaskType == TASK_NONE || (!pBot->PrimaryBotTask.bTaskIsUrgent && !pBot->PrimaryBotTask.bIssuedByCommander))
	{
		BotRole RequiredRole = MarineGetBestBotRole(pBot);

		if (pBot->CurrentRole != RequiredRole)
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);

			pBot->CurrentRole = RequiredRole;
			pBot->CurrentTask = &pBot->PrimaryBotTask;
		}

		BotMarineSetPrimaryTask(pBot, &pBot->PrimaryBotTask);

	}

	if (pBot->SecondaryBotTask.TaskType == TASK_NONE || !pBot->SecondaryBotTask.bTaskIsUrgent)
	{
		MarineSetSecondaryTask(pBot, &pBot->SecondaryBotTask);
	}

	MarineCheckWantsAndNeeds(pBot);

	pBot->CurrentTask = BotGetNextTask(pBot);



	if (pBot->CurrentTask && pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}

	if (pBot->DesiredCombatWeapon == WEAPON_NONE)
	{
		pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, nullptr);
	}

}

void MarineCombatModeThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy > -1)
	{
		if (MarineCombatThink(pBot))
		{
			if (pBot->DesiredCombatWeapon == WEAPON_NONE)
			{
				pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict);
			}

			return;
		}
	}

	// If the bot has points to spend
	if (GetBotAvailableCombatPoints(pBot) >= 1)
	{
		if (gpGlobals->time - pBot->LastCombatTime > 2.0f)
		{
			pBot->BotNextCombatUpgrade = (int)MarineGetNextCombatUpgrade(pBot);

			if (pBot->BotNextCombatUpgrade != COMBAT_MARINE_UPGRADE_NONE)
			{
				int cost = GetMarineCombatUpgradeCost((CombatModeMarineUpgrade)pBot->BotNextCombatUpgrade);

				// Marines are guaranteed to get their upgrade since they don't gestate, so send the input and assume it was successful
				if (GetBotAvailableCombatPoints(pBot) >= cost)
				{
					pBot->pEdict->v.impulse = GetImpulseForMarineCombatUpgrade((CombatModeMarineUpgrade)pBot->BotNextCombatUpgrade);
					pBot->CombatUpgradeMask |= pBot->BotNextCombatUpgrade;
					pBot->BotNextCombatUpgrade = 0;
					return;
				}

			}
		}
	}

	BotUpdateAndClearTasks(pBot);

	if (pBot->PrimaryBotTask.TaskType == TASK_NONE || (!pBot->PrimaryBotTask.bTaskIsUrgent && !pBot->PrimaryBotTask.bIssuedByCommander))
	{
		BotRole RequiredRole = MarineGetBestCombatModeRole(pBot);

		if (pBot->CurrentRole != RequiredRole)
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);

			pBot->CurrentRole = RequiredRole;
			pBot->CurrentTask = &pBot->PrimaryBotTask;
		}

		BotMarineSetCombatModePrimaryTask(pBot, &pBot->PrimaryBotTask);

	}

	if (pBot->SecondaryBotTask.TaskType == TASK_NONE || !pBot->SecondaryBotTask.bTaskIsUrgent)
	{
		MarineSetCombatModeSecondaryTask(pBot, &pBot->SecondaryBotTask);
	}

	MarineCombatModeCheckWantsAndNeeds(pBot);

	pBot->CurrentTask = BotGetNextTask(pBot);

	BotProgressTask(pBot, pBot->CurrentTask);

	if (pBot->DesiredCombatWeapon == WEAPON_NONE)
	{
		pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, nullptr);
	}
}

void BotMarineSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (pBot->CurrentRole == BOT_ROLE_COMMAND)
	{
		Task->TaskType = TASK_COMMAND;
		Task->bTaskIsUrgent = false;
		Task->TaskLength = 0.0f;
		return;
	}

	edict_t* UndefendedTF = UTIL_GetNearestUndefendedStructureOfType(pBot, STRUCTURE_MARINE_TURRETFACTORY);

	if (!FNullEnt(UndefendedTF))
	{
		if (Task->TaskType == TASK_DEFEND && Task->TaskTarget == UndefendedTF) { return; }

		TASK_SetDefendTask(pBot, Task, UndefendedTF, false);
		return;
	}

	switch (pBot->CurrentRole)
	{
	case BOT_ROLE_SWEEPER:
		MarineSweeperSetPrimaryTask(pBot, Task);
		return;
	case BOT_ROLE_FIND_RESOURCES:
		MarineCapperSetPrimaryTask(pBot, Task);
		return;
	case BOT_ROLE_ASSAULT:
		MarineAssaultSetPrimaryTask(pBot, Task);
		return;
	default:
		return;
	}

}

void BotMarineSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task)
{
	switch (pBot->CurrentRole)
	{
	case BOT_ROLE_SWEEPER:
		MarineSweeperSetCombatModePrimaryTask(pBot, Task);
		return;
	case BOT_ROLE_ASSAULT:
		MarineAssaultSetCombatModePrimaryTask(pBot, Task);
		return;
	default:
		return;
	}

}

void MarineSweeperSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskType == TASK_WELD) { return; }

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_WELDER))
	{
		edict_t* DamagedStructure = UTIL_FindClosestDamagedStructure(pBot->pEdict->v.origin, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(DamagedStructure))
		{
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = DamagedStructure;
			Task->TaskLocation = DamagedStructure->v.origin;
			Task->bTaskIsUrgent = true;
			return;
		}
	}

	if (Task->TaskType == TASK_GUARD) { return; }

	Task->TaskType = TASK_GUARD;
	Task->TaskLocation = UTIL_GetCommChairLocation();
	Task->bTaskIsUrgent = false;
	Task->TaskLength = frandrange(20.0f, 30.0f);
	return;
}

void MarineAssaultSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (pBot->PrimaryBotTask.TaskType != TASK_NONE) { return; }
	
	const hive_definition* Hive = UTIL_GetNearestHiveAtLocation(pBot->pEdict->v.origin);

	if (!Hive) { return; }

	float DistToHive = vDist2DSq(pBot->pEdict->v.origin, Hive->FloorLocation);

	bool bShouldAttack = (DistToHive <= sqrf(UTIL_MetresToGoldSrcUnits(15.0f))) ? true : randbool();

	if (bShouldAttack)
	{
		if (Hive)
		{
			int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

			Vector AttackLocation = FindClosestNavigablePointToDestination(BotProfile, pBot->pEdict->v.origin, Hive->FloorLocation, UTIL_MetresToGoldSrcUnits(10.0f));

			if (vDist2DSq(AttackLocation, Hive->FloorLocation) < sqrf(32.0f))
			{
				Vector NewAttackLocation = UTIL_GetRandomPointOnNavmeshInRadius(BotProfile, AttackLocation, UTIL_MetresToGoldSrcUnits(3.0f));

				if (NewAttackLocation != ZERO_VECTOR)
				{
					AttackLocation = NewAttackLocation;
				}
			}

			TASK_SetAttackTask(pBot, Task, Hive->edict, true);
			return;
		}
	}
	else
	{
		int BotNavProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
		Vector NewLocation = UTIL_GetRandomPointOnNavmeshInRadius(BotNavProfile, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f));

		if (NewLocation != ZERO_VECTOR)
		{
			TASK_SetMoveTask(pBot, Task, NewLocation, false);
		}
	}
}

void MarineSweeperSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskType == TASK_GUARD) { return; }

	if (UTIL_GetNumBuiltStructuresOfType(STRUCTURE_MARINE_PHASEGATE) < 2)
	{
		Task->TaskType = TASK_GUARD;
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(10.0f));
		Task->bTaskIsUrgent = false;
		Task->TaskLength = frandrange(20.0f, 30.0f);
		return;
	}

	edict_t* CurrentPhase = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), true, false);
	edict_t* RandPhase = UTIL_GetRandomStructureOfType(STRUCTURE_MARINE_PHASEGATE, CurrentPhase, true);

	if (!FNullEnt(RandPhase))
	{
		Task->TaskType = TASK_GUARD;
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, RandPhase->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));
		Task->bTaskIsUrgent = false;
		Task->TaskLength = frandrange(20.0f, 30.0f);
		return;
	}
		
}

void MarineCapperSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskType == TASK_CAP_RESNODE) { return; }

	const resource_node* UnclaimedResourceNode = UTIL_MarineFindUnclaimedResNodeNearestLocation(pBot, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

	if (UnclaimedResourceNode)
	{
		TASK_SetCapResNodeTask(pBot, Task, UnclaimedResourceNode, false);
		return;
	}

	edict_t* EnemyResTower = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_ALIEN_RESTOWER, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(100.0f), true, true);

	if (!FNullEnt(EnemyResTower))
	{
		const resource_node* EnemyResNode = UTIL_FindNearestResNodeToLocation(EnemyResTower->v.origin);

		TASK_SetCapResNodeTask(pBot, Task, EnemyResNode, false);
		return;
	}
}

void MarineAssaultSetPrimaryTask(bot_t* pBot, bot_task* Task)
{
	const hive_definition* SiegedHive = UTIL_GetNearestHiveUnderSiege(pBot->pEdict->v.origin);

	if (SiegedHive)
	{
		edict_t* Phasegate = UTIL_GetNearestUnbuiltStructureOfTypeInLocation(STRUCTURE_MARINE_PHASEGATE, SiegedHive->Location, UTIL_MetresToGoldSrcUnits(30.0f));

		if (!FNullEnt(Phasegate))
		{
			float Dist = vDist2D(pBot->pEdict->v.origin, Phasegate->v.origin) - 1.0f;

			int NumMarinesNearby = UTIL_GetNumPlayersOfTeamInArea(Phasegate->v.origin, Dist, MARINE_TEAM, pBot->pEdict, CLASS_NONE, false);

			if (NumMarinesNearby < 1)
			{
				TASK_SetBuildTask(pBot, Task, Phasegate, true);
				return;
			}
		}

		edict_t* TurretFactory = UTIL_GetNearestUnbuiltStructureOfTypeInLocation(STRUCTURE_MARINE_TURRETFACTORY, SiegedHive->Location, UTIL_MetresToGoldSrcUnits(30.0f));

		if (!FNullEnt(TurretFactory))
		{
			float Dist = vDist2D(pBot->pEdict->v.origin, TurretFactory->v.origin) - 1.0f;

			int NumMarinesNearby = UTIL_GetNumPlayersOfTeamInArea(TurretFactory->v.origin, Dist, MARINE_TEAM, pBot->pEdict, CLASS_NONE, false);

			if (NumMarinesNearby < 1)
			{
				TASK_SetBuildTask(pBot, Task, TurretFactory, true);
				return;
			}
		}

		edict_t* SiegeTurret = UTIL_GetNearestUnbuiltStructureOfTypeInLocation(STRUCTURE_MARINE_SIEGETURRET, SiegedHive->Location, UTIL_MetresToGoldSrcUnits(30.0f));

		if (!FNullEnt(SiegeTurret))
		{
			float Dist = vDist2D(pBot->pEdict->v.origin, SiegeTurret->v.origin) - 1.0f;

			int NumMarinesNearby = UTIL_GetNumPlayersOfTeamInArea(SiegeTurret->v.origin, Dist, MARINE_TEAM, pBot->pEdict, CLASS_NONE, false);

			if (NumMarinesNearby < 1)
			{
				TASK_SetBuildTask(pBot, Task, SiegeTurret, true);
				return;
			}
		}

		edict_t* Observatory = UTIL_GetNearestUnbuiltStructureOfTypeInLocation(STRUCTURE_MARINE_OBSERVATORY, SiegedHive->Location, UTIL_MetresToGoldSrcUnits(30.0f));

		if (!FNullEnt(Observatory))
		{
			float Dist = vDist2D(pBot->pEdict->v.origin, Observatory->v.origin) - 1.0f;

			int NumMarinesNearby = UTIL_GetNumPlayersOfTeamInArea(Observatory->v.origin, Dist, MARINE_TEAM, pBot->pEdict, CLASS_NONE, false);

			if (NumMarinesNearby < 1)
			{
				TASK_SetBuildTask(pBot, Task, Observatory, true);
				return;
			}
		}

		edict_t* Armoury = UTIL_GetNearestUnbuiltStructureOfTypeInLocation(STRUCTURE_MARINE_ARMOURY, SiegedHive->Location, UTIL_MetresToGoldSrcUnits(30.0f));

		if (!FNullEnt(Armoury))
		{
			float Dist = vDist2D(pBot->pEdict->v.origin, Armoury->v.origin) - 1.0f;

			int NumMarinesNearby = UTIL_GetNumPlayersOfTeamInArea(Armoury->v.origin, Dist, MARINE_TEAM, pBot->pEdict, CLASS_NONE, false);

			if (NumMarinesNearby < 1)
			{
				TASK_SetBuildTask(pBot, Task, Armoury, true);
				return;
			}
		}

		if (Task->TaskType != TASK_ATTACK || Task->TaskTarget != SiegedHive->edict)
		{
			TASK_SetAttackTask(pBot, Task, SiegedHive->edict, false);
			return;
		}

		
		return;
	}

	const hive_definition* BuiltHive = UTIL_GetNearestBuiltHiveToLocation(pBot->pEdict->v.origin);

	if (BuiltHive)
	{
		if (UTIL_ResearchIsComplete(RESEARCH_OBSERVATORY_PHASETECH))
		{
			// We're already waiting patiently to start a siege
			if (Task->TaskType == TASK_GUARD && vDist2DSq(Task->TaskLocation, BuiltHive->FloorLocation) <= sqrf(UTIL_MetresToGoldSrcUnits(25.0f))) { return; }

			Vector WaitPoint = UTIL_GetRandomPointOnNavmeshInDonut(MARINE_REGULAR_NAV_PROFILE, BuiltHive->FloorLocation, UTIL_MetresToGoldSrcUnits(15.0f), UTIL_MetresToGoldSrcUnits(20.0f));

			if (WaitPoint != ZERO_VECTOR && !UTIL_QuickTrace(pBot->pEdict, WaitPoint + Vector(0.0f, 0.0f, 10.0f), BuiltHive->Location))
			{
				Task->TaskType = TASK_GUARD;
				Task->TaskLocation = WaitPoint;
				Task->bTaskIsUrgent = false;
				Task->TaskLength = 30.0f;
				return;
			}
		}

		// Bot has a proper weapon equipped
		if (!PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_MG))
		{
			TASK_SetAttackTask(pBot, Task, BuiltHive->edict, false);
			return;
		}
	}

	edict_t* ResourceTower = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_ALIEN_RESTOWER, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(100.0f), true, true);

	if (!FNullEnt(ResourceTower))
	{
		TASK_SetAttackTask(pBot, Task, ResourceTower, false);
		return;
	}

	if (Task->TaskType != TASK_MOVE)
	{
		// Randomly patrol the map I guess...
		Vector NewMoveLocation = UTIL_GetRandomPointOfInterest();

		if (NewMoveLocation != ZERO_VECTOR)
		{
			TASK_SetMoveTask(pBot, Task, NewMoveLocation, false);
			return;
		}
	}
}

void MarineSetSecondaryTask(bot_t* pBot, bot_task* Task)
{
	// Don't interrupt a build task if we're doing one. We don't want unfinished structures being left lying around
	if (Task->TaskType == TASK_BUILD) { return; }


	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_MINES))
	{
		if (Task->TaskType == TASK_PLACE_MINE) { return; }

		Vector CommChairLocation = UTIL_GetCommChairLocation();

		edict_t* UnminedStructure = UTIL_GetFurthestUnminedStructureOfType(STRUCTURE_MARINE_PHASEGATE, CommChairLocation, UTIL_MetresToGoldSrcUnits(500.0f), false);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_INFANTRYPORTAL, CommChairLocation, UTIL_MetresToGoldSrcUnits(10.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetFurthestUnminedStructureOfType(STRUCTURE_MARINE_ANYTURRETFACTORY, CommChairLocation, UTIL_MetresToGoldSrcUnits(100.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_ARMSLAB, CommChairLocation, UTIL_MetresToGoldSrcUnits(100.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_OBSERVATORY, CommChairLocation, UTIL_MetresToGoldSrcUnits(100.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetFurthestUnminedStructureOfType(STRUCTURE_MARINE_RESTOWER, CommChairLocation, UTIL_MetresToGoldSrcUnits(100.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}
	}	

	int MarineNavProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	edict_t* UnbuiltStructure = UTIL_FindClosestMarineStructureUnbuiltWithoutBuilders(pBot, 2, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

	if (!FNullEnt(UnbuiltStructure))
	{
		TASK_SetBuildTask(pBot, Task, UnbuiltStructure, true);
		return;
	}

	const hive_definition* UnsecuredHive = UTIL_GetNearestHiveOfStatus(pBot->pEdict->v.origin, HIVE_STATUS_UNBUILT);

	if (!UnsecuredHive || vDist2DSq(UnsecuredHive->FloorLocation, pBot->pEdict->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		if (Task->TaskType == TASK_DEFEND) { return; }

		edict_t* AttackedStructure = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ANY_MARINE_STRUCTURE, true);

		if (!FNullEnt(AttackedStructure))
		{
			NSStructureType AttackedStructureType = GetStructureTypeFromEdict(AttackedStructure);

			// Critical structure if it's in base, or it's a turret factory or phase gate
			bool bCriticalStructure = (UTIL_StructureTypesMatch(AttackedStructureType, STRUCTURE_MARINE_ANYTURRETFACTORY) || UTIL_StructureTypesMatch(AttackedStructureType, STRUCTURE_MARINE_PHASEGATE));

			// Always defend if it's critical structure regardless of distance
			if (bCriticalStructure || UTIL_GetPhaseDistanceBetweenPointsSq(pBot->pEdict->v.origin, AttackedStructure->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(30.0f)))
			{
				TASK_SetDefendTask(pBot, Task, AttackedStructure, true);
				return;
			}
		}
	}	

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_WELDER))
	{
		if (Task->TaskType == TASK_WELD) { return; }

		edict_t* HurtPlayer = UTIL_GetClosestPlayerNeedsHealing(pBot->pEdict->v.origin, pBot->pEdict->v.team, UTIL_MetresToGoldSrcUnits(10.0f), pBot->pEdict, true);

		if (!FNullEnt(HurtPlayer) && HurtPlayer->v.armorvalue < GetPlayerMaxArmour(HurtPlayer))
		{
			UTIL_ClearBotTask(pBot, Task);
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = HurtPlayer;
			Task->TaskLocation = HurtPlayer->v.origin;
			Task->bTaskIsUrgent = false;
			return;
		}

		edict_t* DamagedStructure = UTIL_FindClosestDamagedStructure(pBot->pEdict->v.origin, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(DamagedStructure))
		{
			UTIL_ClearBotTask(pBot, Task);
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = DamagedStructure;
			Task->TaskLocation = DamagedStructure->v.origin;
			Task->bTaskIsUrgent = false;
			return;
		}
	}


}

void MarineSetCombatModeSecondaryTask(bot_t* pBot, bot_task* Task)
{
	edict_t* AttackedStructure = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ANY_MARINE_STRUCTURE, true);

	if (!FNullEnt(AttackedStructure) && vDist2DSq(pBot->pEdict->v.origin, AttackedStructure->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
	{
		TASK_SetDefendTask(pBot, Task, AttackedStructure, true);
		return;
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_WELDER))
	{
		edict_t* HurtPlayer = UTIL_GetClosestPlayerNeedsHealing(pBot->pEdict->v.origin, pBot->pEdict->v.team, UTIL_MetresToGoldSrcUnits(10.0f), pBot->pEdict, true);

		if (!FNullEnt(HurtPlayer) && HurtPlayer->v.armorvalue < GetPlayerMaxArmour(HurtPlayer))
		{
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = HurtPlayer;
			Task->TaskLocation = HurtPlayer->v.origin;
			Task->bTaskIsUrgent = false;
			return;
		}
	}
}

void MarineSweeperSetSecondaryTask(bot_t* pBot, bot_task* Task)
{

	if (Task->TaskType == TASK_DEFEND) { return; }

	edict_t* AttackedStructure = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_ANY_MARINE_STRUCTURE, true);

	if (!FNullEnt(AttackedStructure))
	{
		if (UTIL_GetPhaseDistanceBetweenPointsSq(pBot->pEdict->v.origin, AttackedStructure->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(30.0f)))
		{
			TASK_SetDefendTask(pBot, Task, AttackedStructure, true);
			return;
		}
	}

	edict_t* UnbuiltStructure = UTIL_FindClosestMarineStructureUnbuilt(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

	if (!FNullEnt(UnbuiltStructure))
	{
		if (Task->TaskType == TASK_BUILD) { return; }
		TASK_SetBuildTask(pBot, Task, UnbuiltStructure, true);
		return;
	}

	

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_WELDER))
	{

		if (Task->TaskType == TASK_WELD) { return; }
		edict_t* HurtPlayer = UTIL_GetClosestPlayerNeedsHealing(pBot->pEdict->v.origin, pBot->pEdict->v.team, UTIL_MetresToGoldSrcUnits(10.0f), pBot->pEdict, true);

		if (!FNullEnt(HurtPlayer) && HurtPlayer->v.armorvalue < GetPlayerMaxArmour(HurtPlayer))
		{
			UTIL_ClearBotTask(pBot, Task);
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = HurtPlayer;
			Task->TaskLocation = HurtPlayer->v.origin;
			Task->bTaskIsUrgent = false;
			return;
		}

		edict_t* DamagedStructure = UTIL_FindClosestDamagedStructure(pBot->pEdict->v.origin, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(DamagedStructure))
		{
			UTIL_ClearBotTask(pBot, Task);
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = DamagedStructure;
			Task->TaskLocation = DamagedStructure->v.origin;
			Task->bTaskIsUrgent = false;
			return;
		}
	}

	
	
	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_MINES))
	{
		if (Task->TaskType == TASK_PLACE_MINE) { return; }

		edict_t* UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_PHASEGATE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_ANYTURRETFACTORY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_ARMSLAB, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_RESTOWER, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}
	}

}

void MarineCapperSetSecondaryTask(bot_t* pBot, bot_task* Task)
{

	edict_t* UnbuiltStructure = UTIL_FindClosestMarineStructureUnbuilt(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), false);

	if (!FNullEnt(UnbuiltStructure))
	{
		float Dist = vDist2D(pBot->pEdict->v.origin, UnbuiltStructure->v.origin) - 1.0f;

		int NumBuilders = UTIL_GetNumPlayersOfTeamInArea(UnbuiltStructure->v.origin, Dist, MARINE_TEAM, pBot->pEdict, CLASS_NONE, false);

		if (NumBuilders < 1)
		{
			bool bIsUrgent = GetStructureTypeFromEdict(UnbuiltStructure) == STRUCTURE_MARINE_RESTOWER;
			TASK_SetBuildTask(pBot, Task, UnbuiltStructure, bIsUrgent);
			return;
		}

		if (NumBuilders < 2)
		{
			if (Task->TaskType != TASK_GUARD)
			{
				Task->TaskType = TASK_GUARD;
				Task->TaskTarget = UnbuiltStructure;
				Task->TaskLocation = UnbuiltStructure->v.origin;
				Task->TaskLength = 10.0f;
				return;
			}
		}
	}

	edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_MARINE_RESTOWER, true);

	if (!FNullEnt(ResourceTower))
	{
		TASK_SetDefendTask(pBot, Task, ResourceTower, true);
		return;
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_MINES))
	{
		if (Task->TaskType == TASK_PLACE_MINE) { return; }

		edict_t* UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_PHASEGATE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_ANYTURRETFACTORY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_ARMSLAB, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_RESTOWER, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_WELDER))
	{
		if (Task->TaskType == TASK_WELD) { return; }

		edict_t* DamagedStructure = UTIL_FindClosestDamagedStructure(pBot->pEdict->v.origin, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(20.0f), false);

		if (!FNullEnt(DamagedStructure))
		{
			UTIL_ClearBotTask(pBot, Task);
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = DamagedStructure;
			Task->TaskLocation = DamagedStructure->v.origin;
			Task->bTaskIsUrgent = false;
			return;
		}
	}
}

void MarineAssaultSetSecondaryTask(bot_t* pBot, bot_task* Task)
{

	edict_t* UnbuiltStructure = UTIL_FindClosestMarineStructureUnbuilt(pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), false);

	if (!FNullEnt(UnbuiltStructure))
	{
		float Dist = vDist2D(pBot->pEdict->v.origin, UnbuiltStructure->v.origin) - 1.0f;

		int NumBuilders = UTIL_GetNumPlayersOfTeamInArea(UnbuiltStructure->v.origin, Dist, MARINE_TEAM, pBot->pEdict, CLASS_NONE, false);

		if (NumBuilders < 1)
		{
			TASK_SetBuildTask(pBot, Task, UnbuiltStructure, true);
			return;
		}
	}

	edict_t* AttackedPhaseGate = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_MARINE_PHASEGATE, true);

	if (!FNullEnt(AttackedPhaseGate))
	{
		float Dist = UTIL_GetPhaseDistanceBetweenPointsSq(pBot->pEdict->v.origin, AttackedPhaseGate->v.origin);

		if (Dist < sqrf(UTIL_MetresToGoldSrcUnits(30.0f)))
		{
			TASK_SetDefendTask(pBot, Task, AttackedPhaseGate, true);
			return;
		}
	}

	edict_t* AttackedTurretFactory = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_MARINE_ANYTURRETFACTORY, true);

	if (!FNullEnt(AttackedTurretFactory))
	{
		float Dist = UTIL_GetPhaseDistanceBetweenPoints(pBot->pEdict->v.origin, AttackedTurretFactory->v.origin);

		if (Dist < UTIL_MetresToGoldSrcUnits(20.0f))
		{
			TASK_SetDefendTask(pBot, Task, AttackedTurretFactory, true);
			return;
		}
	}
		
	edict_t* ResourceTower = nullptr;
	
	if (UTIL_GetNearestHiveUnderSiege(pBot->pEdict->v.origin) != nullptr)
	{
		ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(pBot, STRUCTURE_MARINE_RESTOWER, true);
	}

	edict_t* WeldTargetStructure = nullptr;
	edict_t* WeldTargetPlayer = nullptr;

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_WELDER))
	{
		edict_t* HurtPlayer = UTIL_GetClosestPlayerNeedsHealing(pBot->pEdict->v.origin, pBot->pEdict->v.team, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict, true);

		if (!FNullEnt(HurtPlayer) && HurtPlayer->v.armorvalue < GetPlayerMaxArmour(HurtPlayer))
		{
			WeldTargetPlayer = HurtPlayer;
		}

		edict_t* DamagedStructure = UTIL_FindClosestDamagedStructure(pBot->pEdict->v.origin, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(10.0f), false);

		if (!FNullEnt(DamagedStructure) && UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, DamagedStructure->v.origin))
		{
			WeldTargetStructure = DamagedStructure;
		}
	}
	
	if (!FNullEnt(WeldTargetPlayer))
	{
		if (WeldTargetPlayer->v.armorvalue < GetPlayerMaxArmour(WeldTargetPlayer) / 2)
		{
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = WeldTargetPlayer;
			Task->TaskLength = 10.0f;
			return;
		}
	}

	if (!FNullEnt(WeldTargetStructure))
	{
		if (FNullEnt(ResourceTower))
		{
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = WeldTargetStructure;
			Task->TaskLength = 20.0f;
			return;
		}

		if (WeldTargetStructure->v.health < WeldTargetStructure->v.max_health * 0.5f)
		{
			Task->TaskType = TASK_WELD;
			Task->TaskTarget = WeldTargetStructure;
			Task->TaskLength = 20.0f;
			return;
		}
	}

	if (!FNullEnt(ResourceTower))
	{
		float DistToStructure = vDist2D(pBot->pEdict->v.origin, ResourceTower->v.origin);

		// Assault won't defend structures unless they're close by, so they don't get too distracted from their attacking duties
		if (DistToStructure < UTIL_MetresToGoldSrcUnits(10.0f))
		{
			int NumPotentialDefenders = UTIL_GetNumPlayersOfTeamInArea(ResourceTower->v.origin, DistToStructure, MARINE_TEAM, pBot->pEdict, CLASS_NONE, false);

			if (NumPotentialDefenders < 1)
			{
				TASK_SetDefendTask(pBot, Task, ResourceTower, false);
				Task->TaskLength = 20.0f;
				return;
			}
		}
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_MINES))
	{
		if (Task->TaskType == TASK_PLACE_MINE) { return; }

		edict_t* UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_PHASEGATE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_ANYTURRETFACTORY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_ARMSLAB, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}

		UnminedStructure = UTIL_GetNearestUnminedStructureOfType(STRUCTURE_MARINE_RESTOWER, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(UnminedStructure))
		{
			TASK_SetMineStructureTask(pBot, Task, UnminedStructure, false);
			return;
		}
	}
	
}

bool MarineCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0) { return false; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	// ENEMY IS OUT OF SIGHT

	if (!TrackedEnemyRef->bHasLOS)
	{
		edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(50.0f), true, true);

		if (!FNullEnt(Armoury))
		{
			float Dist = vDist2DSq(Armoury->v.origin, pEdict->v.origin);

			bool bShouldHeal = (pBot->pEdict->v.health < 50.0f || (pBot->pEdict->v.health < 100.0f && Dist <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f))));
			bool bNeedsAmmo = (BotGetCurrentWeaponClipAmmo(pBot) < BotGetCurrentWeaponMaxClipAmmo(pBot) && BotGetPrimaryWeaponAmmoReserve(pBot) == 0);

			// If we're out of primary ammo or badly hurt, then use opportunity to disengage and head to the nearest armoury to resupply
			if (bNeedsAmmo || bShouldHeal || bNeedsAmmo)
			{
				if (IsPlayerInUseRange(pBot->pEdict, Armoury))
				{
					pBot->DesiredCombatWeapon = GetBotMarinePrimaryWeapon(pBot);

					if (GetBotCurrentWeapon(pBot) == pBot->DesiredCombatWeapon)
					{
						BotUseObject(pBot, Armoury, true);
					}
				}
				else
				{
					MoveTo(pBot, Armoury->v.origin, MOVESTYLE_NORMAL);
					BotReloadWeapons(pBot);
				}
				return true;
			}
		}

		MarineHuntEnemy(pBot, TrackedEnemyRef);
		return true;
	}

	// ENEMY IS VISIBLE

	NSWeapon DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, CurrentEnemy);
	NSWeapon PrimaryWeapon = GetBotMarinePrimaryWeapon(pBot);

	BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, DesiredCombatWeapon, TrackedEnemyRef->LastSeenLocation, CurrentEnemy);

	if (LOSCheck == ATTACK_SUCCESS)
	{
		BotShootLocation(pBot, DesiredCombatWeapon, TrackedEnemyRef->LastSeenLocation);
	}

	float DistFromEnemy = vDist2DSq(pBot->pEdict->v.origin, CurrentEnemy->v.origin);

	if (DesiredCombatWeapon != WEAPON_MARINE_KNIFE)
	{
		if (DistFromEnemy < sqrf(100.0f))
		{
			if (IsBotReloading(pBot) && CanInterruptWeaponReload(GetBotCurrentWeapon(pBot)) && BotGetCurrentWeaponClipAmmo(pBot) > 0)
			{
				InterruptReload(pBot);
			}
			BotJump(pBot);
		}
	}

	// We're going to have the marine always try and use their primary weapon, which means
	// that they will try and put enough distance between themselves and the enemy to use it effectively,
	// and retreat if they need to reload or are out of ammo


	// We are using our primary weapon right now (has ammo left in the clip)
	if (DesiredCombatWeapon == PrimaryWeapon)
	{
		BotLookAt(pBot, CurrentEnemy);
		if (LOSCheck == ATTACK_OUTOFRANGE)
		{			
			MoveTo(pBot, TrackedEnemyRef->LastFloorPosition, MOVESTYLE_NORMAL);
			BotReloadWeapons(pBot);
			return true;
		}

		// Note that we already do visibility checks above, so blocked here means there is another player or structure in the way
		if (LOSCheck == ATTACK_BLOCKED)
		{
			edict_t* TracedEntity = UTIL_TraceEntity(pEdict, pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(CurrentEnemy));

			// Just blast through an alien structure if it's in the way
			if (!FNullEnt(TracedEntity) && TracedEntity != CurrentEnemy)
			{
				if (TracedEntity->v.team != 0 && TracedEntity->v.team != pEdict->v.team)
				{
					BotShootTarget(pBot, DesiredCombatWeapon, TracedEntity);
				}
			}

			float MinDesiredDist = GetMinIdealWeaponRange(DesiredCombatWeapon);

			Vector EngagementLocation = pBot->BotNavInfo.TargetDestination;

			float EngagementLocationDist = vDist2DSq(EngagementLocation, CurrentEnemy->v.origin);

			if (!EngagementLocation || EngagementLocationDist < sqrf(MinDesiredDist) || PerformAttackLOSCheck(EngagementLocation + Vector(0.0f, 0.0f, 50.0f), DesiredCombatWeapon, CurrentEnemy) != ATTACK_SUCCESS)
			{
				int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
				EngagementLocation = UTIL_GetRandomPointOnNavmeshInRadius(MoveProfile, CurrentEnemy->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

				if (EngagementLocation != ZERO_VECTOR && PerformAttackLOSCheck(EngagementLocation + Vector(0.0f, 0.0f, 50.0f), DesiredCombatWeapon, CurrentEnemy) != ATTACK_SUCCESS)
				{
					EngagementLocation = ZERO_VECTOR;
				}
			}

			MoveTo(pBot, EngagementLocation, MOVESTYLE_NORMAL);
			return true;
		}

		if (LOSCheck == ATTACK_SUCCESS)
		{
			float MinDesiredDist = GetMinIdealWeaponRange(DesiredCombatWeapon);
			Vector Orientation = UTIL_GetVectorNormal2D(CurrentEnemy->v.origin - pBot->pEdict->v.origin);

			float EnemyMoveDot = UTIL_GetDotProduct2D(UTIL_GetVectorNormal2D(CurrentEnemy->v.velocity), -Orientation);

			// Enemy is too close for comfort, or is moving towards us. Back up
			if (DistFromEnemy < MinDesiredDist || EnemyMoveDot > 0.7f)
			{
				Vector RetreatLocation = pBot->CurrentFloorPosition - (Orientation * 50.0f);

				if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, RetreatLocation))
				{
					MoveDirectlyTo(pBot, RetreatLocation);
				}
			}
		}

		return true;
	}

	// Retreat and try to reload so we can use our primary weapon again
	if (LOSCheck != ATTACK_SUCCESS)
	{
		edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(50.0f), true, true);

		Vector RetreatLocation = ZERO_VECTOR;

		if (!FNullEnt(Armoury))
		{
			RetreatLocation = Armoury->v.origin;

			if (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxClipSize(pBot))
			{
				if (IsPlayerInUseRange(pBot->pEdict, Armoury))
				{
					BotUseObject(pBot, Armoury, true);
				}
			}
		}
		else
		{
			RetreatLocation = UTIL_GetCommChairLocation();

		}

		MoveTo(pBot, RetreatLocation, MOVESTYLE_NORMAL);

		BotReloadWeapons(pBot);
	}


	return true;
}

void BotReceiveCommanderOrder(bot_t* pBot, AvHOrderType orderType, AvHUser3 TargetType, Vector destination)
{
	UTIL_ClearBotTask(pBot, &pBot->CommanderTask);

	switch (orderType)
	{
	case ORDERTYPEL_MOVE:
		BotReceiveMoveToOrder(pBot, destination);
		break;
	case ORDERTYPET_BUILD:
		BotReceiveBuildOrder(pBot, TargetType, destination);
		break;
	case ORDERTYPET_ATTACK:
		BotReceiveAttackOrder(pBot, TargetType, destination);
		break;
	case ORDERTYPET_GUARD:
		BotReceiveGuardOrder(pBot, TargetType, destination);
		break;
	case ORDERTYPET_WELD:
		BotReceiveWeldOrder(pBot, TargetType, destination);
		break;
	}

	pBot->CommanderTask.bIssuedByCommander = true;
}

void BotReceiveAttackOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination)
{
	NSStructureType StructType = UTIL_IUSER3ToStructureType(TargetType);

	if (StructType != STRUCTURE_NONE)
	{
		edict_t* NearestStructure = UTIL_GetNearestStructureIndexOfType(destination, StructType, UTIL_MetresToGoldSrcUnits(2.0f), false, IsPlayerMarine(pBot->pEdict));

		if (NearestStructure)
		{
			TASK_SetAttackTask(pBot, &pBot->CommanderTask, NearestStructure, false);
			return;
		}
	}
	else
	{
		edict_t* NearestEnemy = NULL;
		float MinDist = 0.0f;

		for (int i = 0; i < 32; i++)
		{
			if (clients[i] && IsPlayerOnAlienTeam(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
			{
				float Dist = vDist2DSq(clients[i]->v.origin, destination);

				if (!NearestEnemy || Dist < MinDist)
				{
					NearestEnemy = clients[i];
					MinDist = Dist;
				}
			}
		}

		if (NearestEnemy)
		{
			TASK_SetAttackTask(pBot, &pBot->CommanderTask, NearestEnemy, false);
			return;
		}
		else
		{
			return;
		}

	}

}

void BotReceiveBuildOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination)
{
	NSStructureType StructType = UTIL_IUSER3ToStructureType(TargetType);

	edict_t* NearestStructure = UTIL_GetNearestStructureIndexOfType(destination, StructType, UTIL_MetresToGoldSrcUnits(2.0f), false, false);

	if (!FNullEnt(NearestStructure))
	{
		TASK_SetBuildTask(pBot, &pBot->CommanderTask, NearestStructure, false);
		pBot->CommanderTask.bIssuedByCommander = true;
	}
}

void BotReceiveMoveToOrder(bot_t* pBot, Vector destination)
{
	const resource_node* ResNodeRef = UTIL_FindEligibleResNodeClosestToLocation(destination, pBot->bot_team, true);

	if (ResNodeRef && vDist2DSq(ResNodeRef->origin, destination) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		TASK_SetCapResNodeTask(pBot, &pBot->CommanderTask, ResNodeRef, false);
		
	}
	else
	{
		const hive_definition* HiveRef = UTIL_GetNearestHiveAtLocation(destination);
		
		if (HiveRef && HiveRef->Status == HIVE_STATUS_UNBUILT && vDist2DSq(HiveRef->Location, destination) <= sqrf(UTIL_MetresToGoldSrcUnits(15.0f)))
		{
			TASK_SetSecureHiveTask(pBot, &pBot->CommanderTask, HiveRef->edict, destination, false);
		}
		else
		{
			TASK_SetMoveTask(pBot, &pBot->CommanderTask, destination, false);
		}

		
	}

	pBot->CommanderTask.bIssuedByCommander = true;
}

void BotReceiveGuardOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination)
{
	return;

	NSStructureType StructType = UTIL_IUSER3ToStructureType(TargetType);

	if (StructType != STRUCTURE_NONE)
	{
		edict_t* NearestStructure = UTIL_GetNearestStructureIndexOfType(destination, StructType, UTIL_MetresToGoldSrcUnits(2.0f), false, IsPlayerMarine(pBot->pEdict));

		if (NearestStructure)
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			pBot->CommanderTask.TaskType = TASK_GUARD;
			pBot->CommanderTask.TaskLocation = NearestStructure->v.origin;
			pBot->CommanderTask.TaskTarget = NearestStructure;
			pBot->CommanderTask.bTargetIsPlayer = false;
			pBot->CommanderTask.bIssuedByCommander = true;
			pBot->CommanderTask.TaskLength = 30.0f;

		}
		else
		{
			return;
		}
	}
	else
	{
		edict_t* NearestEnemy = NULL;
		float MinDist = 0.0f;

		for (int i = 0; i < 32; i++)
		{
			if (clients[i] && IsPlayerOnAlienTeam(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
			{
				float Dist = vDist2DSq(clients[i]->v.origin, destination);

				if (!NearestEnemy || Dist < MinDist)
				{
					NearestEnemy = clients[i];
					MinDist = Dist;
				}
			}
		}

		if (NearestEnemy)
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			pBot->PrimaryBotTask.TaskType = TASK_GUARD;
			pBot->PrimaryBotTask.TaskTarget = NearestEnemy;
			pBot->PrimaryBotTask.bTargetIsPlayer = true;
			pBot->PrimaryBotTask.bIssuedByCommander = true;
			pBot->PrimaryBotTask.TaskLength = 30.0f;
		}
		else
		{
			return;
		}

	}
}

void BotReceiveWeldOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination)
{
	NSStructureType StructType = UTIL_IUSER3ToStructureType(TargetType);

	if (StructType != STRUCTURE_NONE)
	{
		edict_t* NearestStructure = UTIL_GetNearestStructureIndexOfType(destination, StructType, UTIL_MetresToGoldSrcUnits(2.0f), false, IsPlayerMarine(pBot->pEdict));

		if (NearestStructure)
		{
			pBot->CommanderTask.TaskType = TASK_WELD;
			pBot->CommanderTask.TaskLocation = UTIL_GetFloorUnderEntity(NearestStructure);
			pBot->CommanderTask.TaskTarget = NearestStructure;
			pBot->CommanderTask.bTargetIsPlayer = false;

		}
		else
		{
			return;
		}
	}
}

void MarineHuntEnemy(bot_t* pBot, enemy_status* TrackedEnemy)
{
	edict_t* CurrentEnemy = TrackedEnemy->EnemyEdict;

	if (FNullEnt(CurrentEnemy) || IsPlayerDead(CurrentEnemy)) { return; }

	float TimeSinceLastSighting = (gpGlobals->time - TrackedEnemy->LastSeenTime);

	// If the enemy is being motion tracked, or the last seen time was within the last 5 seconds, and the suspected location is close enough, then throw a grenade!
	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_GRENADE) || ((PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_GL) && (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0))))
	{
		if (TimeSinceLastSighting < 5.0f && vDist3DSq(pBot->pEdict->v.origin, TrackedEnemy->LastSeenLocation) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			Vector GrenadeThrowLocation = UTIL_GetGrenadeThrowTarget(pBot, TrackedEnemy->LastSeenLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			if (GrenadeThrowLocation != ZERO_VECTOR)
			{
				BotThrowGrenadeAtTarget(pBot, GrenadeThrowLocation);
				return;
			}
		}
	}

	pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, CurrentEnemy);

	if (GetBotCurrentWeapon(pBot) != pBot->DesiredCombatWeapon) { return; }

	if (BotGetCurrentWeaponClipAmmo(pBot) < BotGetCurrentWeaponMaxClipAmmo(pBot) && BotGetCurrentWeaponReserveAmmo(pBot) > 0)
	{
		

	}

	int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	if (UTIL_PointIsReachable(NavProfileIndex, pBot->pEdict->v.origin, TrackedEnemy->LastSeenLocation, max_player_use_reach))
	{
		MoveTo(pBot, TrackedEnemy->LastFloorPosition, MOVESTYLE_NORMAL);
	}
	
	return;
}

void MarineCombatModeCheckWantsAndNeeds(bot_t* pBot)
{
	edict_t* NearestArmoury = UTIL_GetNearestStructureIndexOfType(pBot->pEdict->v.origin, STRUCTURE_MARINE_ANYARMOURY, UTIL_MetresToGoldSrcUnits(100.0f), true, IsPlayerMarine(pBot->pEdict));

	if (FNullEnt(NearestArmoury) || pBot->WantsAndNeedsTask.TaskType != TASK_NONE) { return; }

	bool bNeedsAmmoOrHealth = false;	

	if (vDist2DSq(pBot->pEdict->v.origin, NearestArmoury->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		bNeedsAmmoOrHealth = (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxClipSize(pBot) || BotGetSecondaryWeaponAmmoReserve(pBot) < BotGetSecondaryWeaponMaxClipSize(pBot) || pBot->pEdict->v.health < pBot->pEdict->v.max_health);
	}
	else
	{
		const hive_definition* Hive = UTIL_GetNearestHiveAtLocation(pBot->pEdict->v.origin);

		bool bNearHive = (Hive && vDist2DSq(pBot->pEdict->v.origin, Hive->FloorLocation) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)));

		bNeedsAmmoOrHealth = (bNearHive) ? (BotGetPrimaryWeaponAmmoReserve(pBot) == 0 && BotGetPrimaryWeaponClipAmmo(pBot) == 0) : (pBot->pEdict->v.health < 50.0f || BotGetPrimaryWeaponAmmoReserve(pBot) == 0);
	}

	if (bNeedsAmmoOrHealth)
	{
		pBot->WantsAndNeedsTask.TaskType = TASK_RESUPPLY;
		pBot->WantsAndNeedsTask.bTaskIsUrgent = false;
		pBot->WantsAndNeedsTask.TaskLocation = NearestArmoury->v.origin;
		pBot->WantsAndNeedsTask.TaskTarget = NearestArmoury;
	}


}

void MarineCheckWantsAndNeeds(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (FNullEnt(pEdict)) { return; }

	if (pBot->CurrentRole == BOT_ROLE_COMMAND)
	{
		UTIL_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);
		return;
	}

	bool bUrgentlyNeedsHealth = (pEdict->v.health < 50.0f);

	// GL is a terrible choice to defend the base with...
	if (pBot->CurrentRole == BOT_ROLE_SWEEPER && PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_GL))
	{
		BotDropWeapon(pBot);
	}

	edict_t* NearestArmoury = UTIL_GetNearestStructureIndexOfType(pEdict->v.origin, STRUCTURE_MARINE_ANYARMOURY, UTIL_MetresToGoldSrcUnits(100.0f), true, IsPlayerMarine(pBot->pEdict));

	if (bUrgentlyNeedsHealth)
	{
		edict_t* HealthPackIndex = UTIL_GetNearestItemIndexOfType(DEPLOYABLE_ITEM_MARINE_HEALTHPACK, pEdict->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

		if (HealthPackIndex)
		{
			pBot->WantsAndNeedsTask.TaskType = TASK_GET_HEALTH;
			pBot->WantsAndNeedsTask.bTaskIsUrgent = true;
			pBot->WantsAndNeedsTask.TaskLocation = HealthPackIndex->v.origin;
			pBot->WantsAndNeedsTask.TaskTarget = HealthPackIndex;

			return;
		}

		if (!FNullEnt(NearestArmoury))
		{
			float PhaseDist = UTIL_GetPhaseDistanceBetweenPoints(pBot->pEdict->v.origin, NearestArmoury->v.origin);

			if (PhaseDist < UTIL_MetresToGoldSrcUnits(30.0f))
			{
				pBot->WantsAndNeedsTask.TaskType = TASK_RESUPPLY;
				pBot->WantsAndNeedsTask.bTaskIsUrgent = true;
				pBot->WantsAndNeedsTask.TaskLocation = NearestArmoury->v.origin;
				pBot->WantsAndNeedsTask.TaskTarget = NearestArmoury;

				return;
			}
		}
	}

	if (GetBotMarinePrimaryWeapon(pBot) == WEAPON_MARINE_MG)
	{
		if (pBot->WantsAndNeedsTask.TaskType != TASK_GET_WEAPON)
		{
			NSStructureType ExcludeItem = (pBot->CurrentRole == BOT_ROLE_SWEEPER) ? DEPLOYABLE_ITEM_MARINE_GRENADELAUNCHER : STRUCTURE_NONE;

			edict_t* NewWeaponIndex = UTIL_GetNearestSpecialPrimaryWeapon(pEdict->v.origin, ExcludeItem, UTIL_MetresToGoldSrcUnits(15.0f), true);


			if (NewWeaponIndex)
			{
				// Don't grab the good stuff if there are humans who need it. Sweepers don't need grenade launchers
				if (!UTIL_IsAnyHumanNearLocationWithoutSpecialWeapon(NewWeaponIndex->v.origin, UTIL_MetresToGoldSrcUnits(10.0f)))
				{
					pBot->WantsAndNeedsTask.TaskType = TASK_GET_WEAPON;
					pBot->WantsAndNeedsTask.bTaskIsUrgent = false;
					pBot->WantsAndNeedsTask.TaskLocation = NewWeaponIndex->v.origin;
					pBot->WantsAndNeedsTask.TaskTarget = NewWeaponIndex;

					return;
				}
			}
			else
			{
				if (!FNullEnt(NearestArmoury) && PlayerHasEquipment(pBot->pEdict))
				{
					pBot->WantsAndNeedsTask.TaskType = TASK_GUARD;
					pBot->WantsAndNeedsTask.bTaskIsUrgent = false;
					pBot->WantsAndNeedsTask.TaskLocation = NearestArmoury->v.origin;
					pBot->WantsAndNeedsTask.TaskTarget = NearestArmoury;

					return;
				}
			}
		}
		else
		{
			return;
		}
	}

	bool bNeedsAmmo = (BotGetPrimaryWeaponAmmoReserve(pBot) < (BotGetPrimaryWeaponMaxAmmoReserve(pBot) / 2));

	if (bNeedsAmmo)
	{
		edict_t* AmmoPackIndex = UTIL_GetNearestItemIndexOfType(DEPLOYABLE_ITEM_MARINE_AMMO, pEdict->v.origin, UTIL_MetresToGoldSrcUnits(10.0f));

		if (AmmoPackIndex)
		{
			pBot->WantsAndNeedsTask.TaskType = TASK_GET_AMMO;
			pBot->WantsAndNeedsTask.bTaskIsUrgent = BotGetPrimaryWeaponAmmoReserve(pBot) == 0;
			pBot->WantsAndNeedsTask.TaskLocation = AmmoPackIndex->v.origin;
			pBot->WantsAndNeedsTask.TaskTarget = AmmoPackIndex;

			return;
		}

		float DistanceWillingToTravel = (BotGetPrimaryWeaponAmmoReserve(pBot) == 0) ? UTIL_MetresToGoldSrcUnits(50.0f) : UTIL_MetresToGoldSrcUnits(15.0f);

		edict_t* NearestArmoury = UTIL_GetNearestStructureIndexOfType(pEdict->v.origin, STRUCTURE_MARINE_ANYARMOURY, UTIL_MetresToGoldSrcUnits(100.0f), true, IsPlayerMarine(pBot->pEdict));

		if (!FNullEnt(NearestArmoury))
		{
			float PhaseDist = UTIL_GetPhaseDistanceBetweenPoints(pBot->pEdict->v.origin, NearestArmoury->v.origin);

			if (PhaseDist <= DistanceWillingToTravel)
			{
				pBot->WantsAndNeedsTask.TaskType = TASK_RESUPPLY;
				pBot->WantsAndNeedsTask.bTaskIsUrgent = BotGetPrimaryWeaponAmmoReserve(pBot) == 0;
				pBot->WantsAndNeedsTask.TaskLocation = NearestArmoury->v.origin;
				pBot->WantsAndNeedsTask.TaskTarget = NearestArmoury;

				return;
			}
		}

	}

	if (!PlayerHasEquipment(pEdict))
	{
		if (pBot->WantsAndNeedsTask.TaskType == TASK_GET_EQUIPMENT) { return; }
		
		edict_t* EquipmentIndex = UTIL_GetNearestEquipment(pEdict->v.origin, UTIL_MetresToGoldSrcUnits(15.0f), true);

		if (EquipmentIndex)
		{
			// Don't grab the good stuff if there are humans who need it
			if (!UTIL_IsAnyHumanNearLocationWithoutEquipment(EquipmentIndex->v.origin, UTIL_MetresToGoldSrcUnits(10.0f)))
			{

				pBot->WantsAndNeedsTask.TaskType = TASK_GET_EQUIPMENT;
				pBot->WantsAndNeedsTask.bTaskIsUrgent = false;
				pBot->WantsAndNeedsTask.TaskLocation = EquipmentIndex->v.origin;
				pBot->WantsAndNeedsTask.TaskTarget = EquipmentIndex;

				return;
			}
		}
		else
		{
			if (PlayerHasSpecialWeapon(pBot->pEdict))
			{
				if (!FNullEnt(NearestArmoury) && PlayerHasEquipment(pBot->pEdict))
				{
					pBot->WantsAndNeedsTask.TaskType = TASK_GUARD;
					pBot->WantsAndNeedsTask.bTaskIsUrgent = false;
					pBot->WantsAndNeedsTask.TaskLocation = NearestArmoury->v.origin;
					pBot->WantsAndNeedsTask.TaskTarget = NearestArmoury;

					return;
				}
			}
		}

	}


	if (!PlayerHasWeapon(pEdict, WEAPON_MARINE_WELDER))
	{
		if (pBot->WantsAndNeedsTask.TaskType == TASK_GET_WEAPON) { return; }
		
		edict_t* WelderIndex = UTIL_GetNearestItemIndexOfType(DEPLOYABLE_ITEM_MARINE_WELDER, pEdict->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

		if (WelderIndex)
		{
			if (!UTIL_IsAnyHumanNearLocationWithoutWeapon(WEAPON_MARINE_WELDER, WelderIndex->v.origin, UTIL_MetresToGoldSrcUnits(5.0f)))
			{
				pBot->WantsAndNeedsTask.TaskType = TASK_GET_WEAPON;
				pBot->WantsAndNeedsTask.bTaskIsUrgent = false;
				pBot->WantsAndNeedsTask.TaskLocation = WelderIndex->v.origin;
				pBot->WantsAndNeedsTask.TaskTarget = WelderIndex;

				return;
			}
		}
	}

	if (pBot->CurrentRole == BOT_ROLE_SWEEPER && !PlayerHasWeapon(pEdict, WEAPON_MARINE_MINES))
	{
		if (pBot->WantsAndNeedsTask.TaskType == TASK_GET_WEAPON) { return; }

		edict_t* MineIndex = UTIL_GetNearestItemIndexOfType(DEPLOYABLE_ITEM_MARINE_MINES, pEdict->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

		if (MineIndex)
		{
			pBot->WantsAndNeedsTask.TaskType = TASK_GET_WEAPON;
			pBot->WantsAndNeedsTask.bTaskIsUrgent = false;
			pBot->WantsAndNeedsTask.TaskLocation = MineIndex->v.origin;
			pBot->WantsAndNeedsTask.TaskTarget = MineIndex;
			return;
		}
	}


}

BotRole MarineGetBestCombatModeRole(const bot_t* pBot)
{
	int NumDefenders = GAME_GetBotsWithRoleType(BOT_ROLE_SWEEPER, MARINE_TEAM, pBot->pEdict);

	if (NumDefenders < 1)
	{
		return BOT_ROLE_SWEEPER;
	}

	return BOT_ROLE_ASSAULT;
}

BotRole MarineGetBestBotRole(const bot_t* pBot)
{
	

	// Take command if configured to and nobody is already commanding

	if (!UTIL_IsThereACommander())
	{
		CommanderMode BotCommanderMode = CONFIG_GetCommanderMode();

		if (BotCommanderMode != COMMANDERMODE_NEVER)
		{
			bool bCanCommand = false;

			if (BotCommanderMode == COMMANDERMODE_IFNOHUMAN)
			{
				if (!GAME_IsAnyHumanOnTeam(MARINE_TEAM) && GAME_GetBotsWithRoleType(BOT_ROLE_COMMAND, MARINE_TEAM, pBot->pEdict) < 1)
				{
					bCanCommand = true;
				}
			}
			else
			{
				if (GAME_GetBotsWithRoleType(BOT_ROLE_COMMAND, MARINE_TEAM, pBot->pEdict) < 1)
				{
					bCanCommand = true;
				}
			}

			if (bCanCommand)
			{
				// Thanks to EterniumDev (Alien) for the suggestion to have the commander jump out and build if nobody is around to help

				int NumAliveMarinesInBase = UTIL_GetNumPlayersOfTeamInArea(UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(30.0f), pBot->pEdict->v.team, pBot->pEdict, CLASS_NONE, true);

				if (NumAliveMarinesInBase > 0) { return BOT_ROLE_COMMAND; }

				int NumUnbuiltStructuresInBase = UTIL_GetNumUnbuiltStructuresOfTeamInArea(pBot->pEdict->v.team, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(15.0f));

				if (NumUnbuiltStructuresInBase > 0)
				{
					return BOT_ROLE_SWEEPER;
				}

				return BOT_ROLE_COMMAND;
			}
		}
	}


	// Only guard the base if there isn't a phase gate or turret factory in base
	
	int NumDefenders = GAME_GetBotsWithRoleType(BOT_ROLE_SWEEPER, MARINE_TEAM, pBot->pEdict);

	// One marine to play sweeper at all times
	if (NumDefenders < 1)
	{
		return BOT_ROLE_SWEEPER;
	}

	int NumPlayersOnTeam = GAME_GetNumPlayersOnTeam(MARINE_TEAM);

	if (NumPlayersOnTeam == 0) { return BOT_ROLE_ASSAULT; } // This shouldn't happen, but let's avoid a potential divide by zero later on anyway...

	int NumTotalResNodes = UTIL_GetNumResNodes();

	if (NumTotalResNodes == 0)
	{
		return BOT_ROLE_ASSAULT; // Again, shouldn't happen, but avoids potential divide by zero
	}

	int NumMarineResTowers = UTIL_GetNumPlacedStructuresOfType(STRUCTURE_MARINE_RESTOWER);

	int NumRemainingResNodes = NumTotalResNodes - NumMarineResTowers;

	int NumCappers = GAME_GetBotsWithRoleType(BOT_ROLE_FIND_RESOURCES, MARINE_TEAM, pBot->pEdict);

	if (NumRemainingResNodes > 1 && NumCappers == 0)
	{
		return BOT_ROLE_FIND_RESOURCES;
	}

	// How much of the map do we currently dominate?
	float ResTowerRatio = ((float)NumMarineResTowers / (float)NumTotalResNodes);

	// If we own less than a third of the map, prioritise capping resource nodes
	if (ResTowerRatio < 0.5f)
	{
		return BOT_ROLE_FIND_RESOURCES;
	}

	return BOT_ROLE_ASSAULT;
}

CombatModeMarineUpgrade MarineGetNextCombatUpgrade(bot_t* pBot)
{

	if (pBot->CurrentRole == BOT_ROLE_SWEEPER)
	{
		if (!PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_WELDER))
		{
			return COMBAT_MARINE_UPGRADE_WELDER;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_ARMOUR1))
		{
			return COMBAT_MARINE_UPGRADE_ARMOUR1;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_DAMAGE1))
		{
			return COMBAT_MARINE_UPGRADE_DAMAGE1;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_SHOTGUN))
		{
			return COMBAT_MARINE_UPGRADE_SHOTGUN;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_DAMAGE2))
		{
			return COMBAT_MARINE_UPGRADE_DAMAGE2;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_ARMOUR2))
		{
			return COMBAT_MARINE_UPGRADE_ARMOUR2;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_HEAVYARMOUR))
		{
			return COMBAT_MARINE_UPGRADE_HEAVYARMOUR;
		}

		if (randbool())
		{
			return COMBAT_MARINE_UPGRADE_ARMOUR3;
		}
		else
		{
			return COMBAT_MARINE_UPGRADE_DAMAGE3;
		}

	}

	if (pBot->CurrentRole == BOT_ROLE_ASSAULT)
	{

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_ARMOUR1))
		{
			return COMBAT_MARINE_UPGRADE_ARMOUR1;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_DAMAGE1))
		{
			return COMBAT_MARINE_UPGRADE_DAMAGE1;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_GRENADE))
		{
			return COMBAT_MARINE_UPGRADE_GRENADE;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_SHOTGUN))
		{
			return COMBAT_MARINE_UPGRADE_SHOTGUN;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_HMG))
		{
			return COMBAT_MARINE_UPGRADE_HMG;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_DAMAGE2))
		{
			return COMBAT_MARINE_UPGRADE_DAMAGE2;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_ARMOUR2))
		{
			return COMBAT_MARINE_UPGRADE_ARMOUR2;
		}

		if (!(pBot->CombatUpgradeMask & COMBAT_MARINE_UPGRADE_HEAVYARMOUR))
		{
			return COMBAT_MARINE_UPGRADE_HEAVYARMOUR;
		}

	}

	return COMBAT_MARINE_UPGRADE_NONE;

}
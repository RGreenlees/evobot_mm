
#include "bot_task.h"

#include <extdll.h>
#include <meta_api.h>
#include <dllapi.h>

#include "player_util.h"
#include "bot_navigation.h"
#include "bot_tactical.h"
#include "bot_weapons.h"
#include "general_util.h"
#include "bot_util.h"
#include "bot_config.h"
#include "game_state.h"

extern float GameStartTime;

extern bot_t bots[MAX_CLIENTS];

void UTIL_ClearAllBotTasks(bot_t* pBot)
{
	UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);
	UTIL_ClearBotTask(pBot, &pBot->CommanderTask);
}

void UTIL_ClearBotTask(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return; }

	if (Task->TaskType == TASK_GUARD)
	{
		UTIL_ClearGuardInfo(pBot);
	}

	Task->TaskType = TASK_NONE;
	Task->TaskLocation = ZERO_VECTOR;
	Task->TaskTarget = NULL;
	Task->TaskStartedTime = 0.0f;
	Task->TaskLength = 0.0f;
	Task->bIssuedByCommander = false;
	Task->bTargetIsPlayer = false;
	Task->bTaskIsUrgent = false;
	Task->bIsWaitingForBuildLink = false;
	Task->LastBuildAttemptTime = 0.0f;
	Task->BuildAttempts = 0;
	Task->StructureType = STRUCTURE_NONE;
}

void BotUpdateAndClearTasks(bot_t* pBot)
{
	if (pBot->CommanderTask.TaskType != TASK_NONE)
	{
		if (!UTIL_IsTaskStillValid(pBot, &pBot->CommanderTask))
		{
			if (UTIL_IsTaskCompleted(pBot, &pBot->CommanderTask))
			{
				BotOnCompleteCommanderTask(pBot, &pBot->CommanderTask);
			}
			else
			{
				UTIL_ClearBotTask(pBot, &pBot->CommanderTask);
			}
		}
	}

	if (pBot->PrimaryBotTask.TaskType != TASK_NONE)
	{
		if (!UTIL_IsTaskStillValid(pBot, &pBot->PrimaryBotTask))
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
		}
	}

	if (pBot->SecondaryBotTask.TaskType != TASK_NONE)
	{
		if (!UTIL_IsTaskStillValid(pBot, &pBot->SecondaryBotTask))
		{
			UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);
		}
	}

	if (pBot->WantsAndNeedsTask.TaskType != TASK_NONE)
	{
		if (!UTIL_IsTaskStillValid(pBot, &pBot->WantsAndNeedsTask))
		{
			UTIL_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);
		}
	}

}

bool UTIL_IsTaskUrgent(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskType == TASK_NONE) { return false; }

	if (Task->bTaskIsUrgent) { return true; }

	switch (Task->TaskType)
	{
	case TASK_GET_AMMO:
		return (BotGetPrimaryWeaponAmmoReserve(pBot) == 0);
	case TASK_GET_HEALTH:
		return (IsPlayerMarine(pBot->pEdict)) ? (pBot->pEdict->v.health < 50.0f) : (GetPlayerOverallHealthPercent(pBot->pEdict) < 50.0f);
	case TASK_ATTACK:
	case TASK_GET_WEAPON:
	case TASK_GET_EQUIPMENT:
	case TASK_WELD:
		return false;
	case TASK_RESUPPLY:
		return (pBot->pEdict->v.health < 50.0f) || (BotGetPrimaryWeaponAmmoReserve(pBot) == 0);
	case TASK_MOVE:
		return UTIL_IsMoveTaskUrgent(pBot, Task);
	case TASK_BUILD:
		return UTIL_IsBuildTaskUrgent(pBot, Task);
	case TASK_GUARD:
		return UTIL_IsGuardTaskUrgent(pBot, Task);
	default:
		return false;
	}

	return false;
}

bool UTIL_IsGuardTaskUrgent(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskTarget)
	{
		NSStructureType StructType = GetStructureTypeFromEdict(Task->TaskTarget);

		if (StructType == STRUCTURE_MARINE_PHASEGATE || StructType == STRUCTURE_MARINE_TURRETFACTORY)
		{
			return true;
		}
	}

	return UTIL_IsAlienPlayerInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(20.0f));
}

bool UTIL_IsBuildTaskUrgent(bot_t* pBot, bot_task* Task)
{
	if (!Task->TaskTarget) { return false; }

	NSStructureType StructType = GetStructureTypeFromEdict(Task->TaskTarget);

	if (StructType == STRUCTURE_MARINE_PHASEGATE || StructType == STRUCTURE_MARINE_TURRETFACTORY) { return true; }

	return false;
}

bool UTIL_IsMoveTaskUrgent(bot_t* pBot, bot_task* Task)
{
	return UTIL_IsNearActiveHive(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(30.0f)) || UTIL_IsAlienPlayerInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(20.0f));
}

bot_task* BotGetNextTask(bot_t* pBot)
{
	// Any orders issued by the commander take priority over everything else
	if (pBot->CommanderTask.TaskType != TASK_NONE)
	{
		if (pBot->SecondaryBotTask.bTaskIsUrgent)
		{
			return &pBot->SecondaryBotTask;
		}
		else
		{
			return &pBot->CommanderTask;
		}
	}

	// Prioritise healing our friends (heal tasks are only valid if the target is close by anyway)
	if (pBot->SecondaryBotTask.TaskType == TASK_HEAL)
	{
		return &pBot->SecondaryBotTask;
	}

	if (UTIL_IsTaskUrgent(pBot, &pBot->WantsAndNeedsTask))
	{
		return &pBot->WantsAndNeedsTask;
	}

	if (UTIL_IsTaskUrgent(pBot, &pBot->PrimaryBotTask))
	{
		return &pBot->PrimaryBotTask;
	}

	if (UTIL_IsTaskUrgent(pBot, &pBot->SecondaryBotTask))
	{
		return &pBot->SecondaryBotTask;
	}

	if (pBot->WantsAndNeedsTask.TaskType != TASK_NONE)
	{
		return &pBot->WantsAndNeedsTask;
	}

	if (pBot->SecondaryBotTask.TaskType != TASK_NONE)
	{
		return &pBot->SecondaryBotTask;
	}

	return &pBot->PrimaryBotTask;
}

void BotOnCompleteCommanderTask(bot_t* pBot, bot_task* Task)
{
	if (!Task || !IsPlayerMarine(pBot->pEdict)) { return; }

	BotTaskType OldTaskType = Task->TaskType;
	UTIL_ClearBotTask(pBot, Task);

	if (OldTaskType == TASK_GUARD)
	{
		UTIL_ClearGuardInfo(pBot);
	}
	
	if (OldTaskType == TASK_MOVE)
	{
		edict_t* NearbyAlienTower = UTIL_GetNearestStructureIndexOfType(pBot->pEdict->v.origin, STRUCTURE_ALIEN_RESTOWER, UTIL_MetresToGoldSrcUnits(5.0f), false, IsPlayerMarine(pBot->pEdict));

		if (!FNullEnt(NearbyAlienTower))
		{
			const resource_node* NodeRef = UTIL_FindNearestResNodeToLocation(NearbyAlienTower->v.origin);
			TASK_SetCapResNodeTask(pBot, Task, NodeRef, false);
			Task->bIssuedByCommander = true;
			return;
		}	
	}

	// After completing a move or build task, wait a bit in case the commander wants to do something else
	if (OldTaskType == TASK_MOVE || OldTaskType == TASK_BUILD)
	{
		Task->TaskType = TASK_GUARD;
		Task->TaskLocation = pBot->pEdict->v.origin;
		Task->TaskLength = (OldTaskType == TASK_MOVE) ? 30.0f : 20.0f;
		Task->TaskStartedTime = gpGlobals->time;
		Task->bIssuedByCommander = true;
	}

}

bool UTIL_IsTaskCompleted(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }

	switch (Task->TaskType)
	{
	case TASK_MOVE:
		return vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) <= sqrf(max_player_use_reach);
	case TASK_BUILD:
		return !FNullEnt(Task->TaskTarget) && UTIL_StructureIsFullyBuilt(Task->TaskTarget);
	case TASK_ATTACK:
		return FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO);
	case TASK_GUARD:
		return (gpGlobals->time - Task->TaskStartedTime) > Task->TaskLength;
	default:
		return !UTIL_IsTaskStillValid(pBot, Task);
	}

	return false;
}

bool UTIL_IsTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task || FNullEnt(pBot->pEdict)) { return false; }

	if ((Task->TaskStartedTime > 0.0f && Task->TaskLength > 0.0f) && (gpGlobals->time - Task->TaskStartedTime >= Task->TaskLength)) { return false; }

	switch (Task->TaskType)
	{
	case TASK_NONE:
		return false;
	case TASK_MOVE:
		return UTIL_IsMoveTaskStillValid(pBot, Task);
	case TASK_GET_AMMO:
		return UTIL_IsAmmoPickupTaskStillValid(pBot, Task);
	case TASK_GET_HEALTH:
	{
		if (IsPlayerMarine(pBot->pEdict))
		{
			return UTIL_IsHealthPickupTaskStillValid(pBot, Task);
		}
		else
		{
			return UTIL_IsAlienGetHealthTaskStillValid(pBot, Task);
		}
	}
	case TASK_GET_EQUIPMENT:
		return UTIL_IsEquipmentPickupTaskStillValid(pBot, Task);
	case TASK_GET_WEAPON:
		return UTIL_IsWeaponPickupTaskStillValid(pBot, Task);
	case TASK_RESUPPLY:
		return UTIL_IsResupplyTaskStillValid(pBot, Task);
	case TASK_ATTACK:
		return UTIL_IsAttackTaskStillValid(pBot, Task);
	case TASK_GUARD:
		return UTIL_IsGuardTaskStillValid(pBot, Task);
	case TASK_BUILD:
	{
		if (IsPlayerMarine(pBot->pEdict))
		{
			return UTIL_IsMarineBuildTaskStillValid(pBot, Task);
		}
		else
		{
			return UTIL_IsAlienBuildTaskStillValid(pBot, Task);
		}
	}
	case TASK_CAP_RESNODE:
	{
		if (IsPlayerMarine(pBot->pEdict))
		{
			return UTIL_IsMarineCapResNodeTaskStillValid(pBot, Task);
		}
		else
		{
			return UTIL_IsAlienCapResNodeTaskStillValid(pBot, Task);
		}
	}
	case TASK_DEFEND:
		return UTIL_IsDefendTaskStillValid(pBot, Task);
	case TASK_WELD:
		return UTIL_IsWeldTaskStillValid(pBot, Task);
	case TASK_EVOLVE:
		return UTIL_IsEvolveTaskStillValid(pBot, Task);
	case TASK_HEAL:
		return UTIL_IsAlienHealTaskStillValid(pBot, Task);
	default:
		return false;
	}

	return false;
}

bool UTIL_IsMoveTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task->TaskLocation) { return false; }

	return (vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) > sqrf(max_player_use_reach) || !UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, Task->TaskLocation));
}

bool UTIL_IsWeldTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }
	if (FNullEnt(Task->TaskTarget)) { return false; }
	if (Task->TaskTarget == pBot->pEdict) { return false; }
	if (!PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_WELDER)) { return false; }

	if (IsEdictPlayer(Task->TaskTarget))
	{
		if (!IsPlayerMarine(Task->TaskTarget) || !IsPlayerActiveInGame(Task->TaskTarget)) { return false; }
		return (Task->TaskTarget->v.armorvalue < GetPlayerMaxArmour(Task->TaskTarget));
	}
	else
	{
		if (!UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

		return (Task->TaskTarget->v.health < Task->TaskTarget->v.max_health);
	}
}

bool UTIL_IsAmmoPickupTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->pEdict) || !IsPlayerActiveInGame(pBot->pEdict) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_IsDroppedItemStillReachable(pBot, Task->TaskTarget)) { return false; }

	return (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f))) && (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxAmmoReserve(pBot));
}

bool UTIL_IsHealthPickupTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->pEdict) || !IsPlayerActiveInGame(pBot->pEdict) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_IsDroppedItemStillReachable(pBot, Task->TaskTarget)) { return false; }

	return ((vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f))) && (pBot->pEdict->v.health < pBot->pEdict->v.max_health));
}

bool UTIL_IsEquipmentPickupTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->pEdict) || !IsPlayerActiveInGame(pBot->pEdict) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_IsDroppedItemStillReachable(pBot, Task->TaskTarget)) { return false; }

	return !PlayerHasEquipment(pBot->pEdict);
}

bool UTIL_IsWeaponPickupTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->pEdict) || !IsPlayerActiveInGame(pBot->pEdict) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_IsDroppedItemStillReachable(pBot, Task->TaskTarget)) { return false; }

	NSWeapon WeaponType = UTIL_GetWeaponTypeFromEdict(Task->TaskTarget);

	if (WeaponType == WEAPON_NONE) { return false; }

	return !PlayerHasWeapon(pBot->pEdict, WeaponType);
}

bool UTIL_IsAttackTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget)) { return false; }

	if ((Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (!UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (IsPlayerSkulk(pBot->pEdict))
	{
		if (UTIL_IsStructureElectrified(Task->TaskTarget)) { return false; }
	}

	if (IsPlayerGorge(pBot->pEdict) && !PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_BILEBOMB)) { return false; }

	NSStructureType StructureType = GetStructureTypeFromEdict(Task->TaskTarget);

	if (IsPlayerMarine(pBot->pEdict))
	{
		if (StructureType == STRUCTURE_ALIEN_HIVE || StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
		{
			if (BotGetPrimaryWeaponClipAmmo(pBot) <= 0 && BotGetPrimaryWeaponAmmoReserve(pBot) <= 0)
			{
				return false;
			}
		}
	}

	return Task->TaskTarget->v.team != pBot->pEdict->v.team;

}

bool UTIL_IsResupplyTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || !IsPlayerMarine(pBot->pEdict) || !IsPlayerActiveInGame(pBot->pEdict) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (!UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (!UTIL_IsMarineStructure(Task->TaskTarget) || !UTIL_StructureTypesMatch(GetStructureTypeFromEdict(Task->TaskTarget), STRUCTURE_MARINE_ANYARMOURY) || !UTIL_StructureIsFullyBuilt(Task->TaskTarget) || UTIL_StructureIsRecycling(Task->TaskTarget)) { return false; }

	return ((pBot->pEdict->v.health < pBot->pEdict->v.max_health)
		|| (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxAmmoReserve(pBot))
		|| (BotGetSecondaryWeaponAmmoReserve(pBot) < BotGetSecondaryWeaponMaxAmmoReserve(pBot))
		);
}

bool UTIL_IsGuardTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }

	if (vEquals(Task->TaskLocation, ZERO_VECTOR))
	{
		return false;
	}

	return true;
}

bool UTIL_IsMarineBuildTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (!UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (UTIL_StructureIsRecycling(Task->TaskTarget) == true)
	{
		return false;
	}

	NSStructureType StructureType = GetStructureTypeFromEdict(Task->TaskTarget);

	if (StructureType == STRUCTURE_NONE) { return false; }

	if (!Task->bIssuedByCommander)
	{
		int NumBuilders = UTIL_GetNumPlayersOfTeamInArea(Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(2.0f), MARINE_TEAM, pBot->pEdict, CLASS_MARINE_COMMANDER, false);

		if (NumBuilders >= 2)
		{
			return false;
		}
	}

	return !UTIL_StructureIsFullyBuilt(Task->TaskTarget);
}

bool UTIL_IsAlienBuildTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }

	if (!Task->TaskLocation) { return false; }

	if (Task->StructureType == STRUCTURE_NONE) { return false; }

	if (Task->BuildAttempts >= 3) { return false; }

	if (Task->bIsWaitingForBuildLink && (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f)) { return true; }

	if (!FNullEnt(Task->TaskTarget) && !UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (Task->StructureType == STRUCTURE_ALIEN_HIVE)
	{
		const hive_definition* HiveIndex = UTIL_GetNearestHiveAtLocation(Task->TaskLocation);

		if (!HiveIndex) { return false; }

		if (HiveIndex->Status != HIVE_STATUS_UNBUILT) { return false; }

		edict_t* OtherHiveBuilder = GetFirstBotWithBuildTask(STRUCTURE_ALIEN_HIVE, pBot->pEdict);

		if (!FNullEnt(OtherHiveBuilder) && GetPlayerResources(OtherHiveBuilder) > GetPlayerResources(pBot->pEdict)) { return false; }

		edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(HiveIndex->Location, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(10.0f), pBot->pEdict);

		if (!FNullEnt(OtherGorge) && GetPlayerResources(OtherGorge) > pBot->resources)
		{
			char buf[128];
			sprintf(buf, "I won't drop hive, %s can do it", STRING(OtherGorge->v.netname));
			BotTeamSay(pBot, 1.0f, buf);
			return false;
		}

		if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_PHASEGATE, HiveIndex->Location, UTIL_MetresToGoldSrcUnits(15.0f)))
		{
			char buf[128];
			sprintf(buf, "We need to clear %s before I can build the hive", UTIL_GetClosestMapLocationToPoint(HiveIndex->Location));
			BotTeamSay(pBot, 1.0f, buf);
			return false;
		}

		if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_TURRETFACTORY, HiveIndex->Location, UTIL_MetresToGoldSrcUnits(15.0f)))
		{
			char buf[128];
			sprintf(buf, "We need to clear %s before I can build the hive", UTIL_GetClosestMapLocationToPoint(HiveIndex->Location));
			BotTeamSay(pBot, 1.0f, buf);
			return false;
		}

		

		return true;
	}

	if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER)
	{
		const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

		if (!ResNodeIndex) { return false; }

		if (ResNodeIndex->bIsOccupied)
		{
			if (ResNodeIndex->bIsOwnedByMarines) { return false; }

			if (!IsPlayerGorge(pBot->pEdict)) { return false; }

			if (UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict)) { return false; }

			if (vDist2DSq(pBot->pEdict->v.origin, ResNodeIndex->origin) > sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { return false; }
		}

		if (FNullEnt(Task->TaskTarget))
		{
			edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

			if (!FNullEnt(OtherGorge) && (GetPlayerResources(OtherGorge) >= kResourceTowerCost && vDist2DSq(OtherGorge->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation)))
			{
				return false;
			}

			edict_t* Egg = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_EGG, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

			if (!FNullEnt(Egg) && (GetPlayerResources(Egg) >= kResourceTowerCost && vDist2DSq(Egg->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation)))
			{
				return false;
			}
		}
		else
		{
			edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

			if (!FNullEnt(OtherGorge) && vDist2DSq(OtherGorge->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation))
			{
				return false;
			}

			edict_t* OtherEgg = UTIL_GetNearestPlayerOfClass(Task->TaskLocation, CLASS_EGG, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

			if (!FNullEnt(OtherEgg) && vDist2DSq(OtherEgg->v.origin, Task->TaskLocation) < vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation))
			{
				return false;
			}
		}
	}

	if (Task->StructureType == STRUCTURE_ALIEN_DEFENCECHAMBER || Task->StructureType == STRUCTURE_ALIEN_MOVEMENTCHAMBER || Task->StructureType == STRUCTURE_ALIEN_SENSORYCHAMBER || Task->StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
	{
		if (UTIL_GetNumPlacedStructuresOfTypeInRadius(Task->StructureType, Task->TaskLocation, UTIL_MetresToGoldSrcUnits(10.0f)) >= 3)
		{
			return false;
		}
	}



	if (!FNullEnt(Task->TaskTarget))
	{
		if ((Task->TaskTarget->v.effects & EF_NODRAW) || (Task->TaskTarget->v.deadflag == DEAD_DEAD)) { return false; }
		return !UTIL_StructureIsFullyBuilt(Task->TaskTarget);
	}
	else
	{
		if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER) { return true; }

		return UTIL_GetNavAreaAtLocation(Task->TaskLocation) == SAMPLE_POLYAREA_GROUND;
	}
}

bool UTIL_IsAlienCapResNodeTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task->TaskLocation)
	{
		return false;
	}

	if (!IsPlayerSkulk(pBot->pEdict) && !IsPlayerGorge(pBot->pEdict)) { return false; }

	const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex)
	{
		return false;
	}

	if (ResNodeIndex->bIsOccupied)
	{
		if (IsPlayerGorge(pBot->pEdict))
		{
			if (!ResNodeIndex->bIsOwnedByMarines)
			{
				return !UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict);
			}
			else
			{
				return PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_BILEBOMB);
			}

		}
		else
		{
			return true;
		}
	}
	else
	{
		if (Task->BuildAttempts > 3)
		{
			return false;
		}
	}

	return true;
}

bool UTIL_IsMarineCapResNodeTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task || !Task->TaskLocation) { return false; }

	const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return false; }

	// Always obey commander orders even if there's a bunch of other marines already there
	if (!Task->bIssuedByCommander)
	{
		int NumMarinesNearby = UTIL_GetNumPlayersOfTeamInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(4.0f), MARINE_TEAM, pBot->pEdict, CLASS_MARINE_COMMANDER, false);

		if (NumMarinesNearby >= 2 && vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(4.0f))) { return false; }
	}

	if (ResNodeIndex->bIsOccupied)
	{
		if (ResNodeIndex->bIsOwnedByMarines && !FNullEnt(ResNodeIndex->TowerEdict))
		{
			return !UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict);
		}
		else
		{
			return true;
		}
	}

	return true;
}

bool UTIL_IsDefendTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (!UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (Task->TaskTarget->v.team != pBot->pEdict->v.team) { return false; }

	int NumExistingDefenders = UTIL_GetNumPlayersOfTeamInArea(Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(10.0f), pBot->pEdict->v.team, pBot->pEdict, CLASS_GORGE, false);

	if (NumExistingDefenders >= 2) { return false; }

	if (gpGlobals->time - pBot->LastCombatTime < 5.0f) { return true; }

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) <= sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && gpGlobals->time - pBot->LastCombatTime > 10.0f) { return false; }

	return true;
}

bool UTIL_IsEvolveTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task || !Task->Evolution || !IsPlayerAlien(pBot->pEdict)) { return false; }

	switch (Task->Evolution)
	{
	case IMPULSE_ALIEN_EVOLVE_FADE:
		return !IsPlayerFade(pBot->pEdict) && pBot->resources >= kFadeEvolutionCost;
	case IMPULSE_ALIEN_EVOLVE_ONOS:
		return !IsPlayerOnos(pBot->pEdict) && pBot->resources >= kOnosEvolutionCost && (GAME_GetNumPlayersOnTeamOfClass(pBot->pEdict->v.team, CLASS_ONOS) < 2);
	case IMPULSE_ALIEN_EVOLVE_LERK:
		return !IsPlayerLerk(pBot->pEdict) && pBot->resources >= kLerkEvolutionCost;
	case IMPULSE_ALIEN_EVOLVE_GORGE:
		return !IsPlayerGorge(pBot->pEdict) && pBot->resources >= kGorgeEvolutionCost;
	case IMPULSE_ALIEN_EVOLVE_SKULK:
		return !IsPlayerSkulk(pBot->pEdict);
	default:
		return false;
	}

	return false;
}

bool UTIL_IsAlienGetHealthTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.deadflag != DEAD_NO)) { return false; }

	if (IsEdictStructure(Task->TaskTarget) && !UTIL_IsBuildableStructureStillReachable(pBot, Task->TaskTarget)) { return false; }

	if (IsEdictPlayer(Task->TaskTarget))
	{
		if (!IsPlayerGorge(Task->TaskTarget)) { return false; }
	}
	return (pBot->pEdict->v.health < pBot->pEdict->v.max_health) || (!IsPlayerSkulk(pBot->pEdict) && pBot->pEdict->v.armorvalue < (GetPlayerMaxArmour(pBot->pEdict) * 0.7f));
}

bool UTIL_IsAlienHealTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (FNullEnt(Task->TaskTarget) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }

	if (!IsPlayerGorge(pBot->pEdict)) { return false; }

	if (GetPlayerOverallHealthPercent(Task->TaskTarget) >= 0.99f) { return false; }

	if (IsEdictStructure(Task->TaskTarget)) { return true; }

	// If our target is a player, give up if they are too far away. I'm not going to waste time chasing you around the map!
	float MaxHealRelevant = sqrf(UTIL_MetresToGoldSrcUnits(5.0f));

	return (vDist2DSq(pBot->CurrentFloorPosition, Task->TaskTarget->v.origin) <= MaxHealRelevant);
}

void BotProgressMoveTask(bot_t* pBot, bot_task* Task)
{
	Task->TaskStartedTime = gpGlobals->time;

	if (IsPlayerLerk(pBot->pEdict))
	{
		if (UTIL_ShouldBotBeCautious(pBot))
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_HIDE, 100.0f);
		}
		else
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL, 100.0f);
		}

		return;
	}

	MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);

	if (IsPlayerMarine(pBot->pEdict))
	{
		BotReloadWeapons(pBot);
	}
}

void BotProgressPickupTask(bot_t* pBot, bot_task* Task)
{
	if (Task->TaskType == TASK_GET_AMMO)
	{
		pBot->DesiredCombatWeapon = GetBotMarinePrimaryWeapon(pBot);
	}

	MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

	Task->TaskStartedTime = gpGlobals->time;

	float DistFromItem = vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin);

	if (DistFromItem < sqrf(UTIL_MetresToGoldSrcUnits(1.0f)))
	{
		BotLookAt(pBot, Task->TaskTarget);

		if (Task->TaskType == TASK_GET_WEAPON)
		{
			NSDeployableItem ItemType = UTIL_GetItemTypeFromEdict(Task->TaskTarget);

			if (UTIL_DroppedItemIsPrimaryWeapon(ItemType))
			{
				NSWeapon CurrentPrimaryWeapon = GetBotMarinePrimaryWeapon(pBot);

				if (CurrentPrimaryWeapon != WEAPON_NONE && CurrentPrimaryWeapon != WEAPON_MARINE_MG)
				{
					if (GetBotCurrentWeapon(pBot) != CurrentPrimaryWeapon)
					{
						pBot->DesiredCombatWeapon = CurrentPrimaryWeapon;
					}
					else
					{
						BotDropWeapon(pBot);
					}
				}
			}
		}
	}
}

void BotProgressResupplyTask(bot_t* pBot, bot_task* Task)
{
	if (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxAmmoReserve(pBot))
	{
		pBot->DesiredCombatWeapon = GetBotMarinePrimaryWeapon(pBot);
	}
	else
	{
		pBot->DesiredCombatWeapon = GetBotMarineSecondaryWeapon(pBot);
	}

	if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Task->TaskTarget, max_player_use_reach, false))
	{
		BotUseObject(pBot, Task->TaskTarget, true);
		if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(50.0f))
		{
			MoveDirectlyTo(pBot, Task->TaskTarget->v.origin);
		}
		return;
	}

	MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		BotLookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
	}

}

void MarineProgressBuildTask(bot_t* pBot, bot_task* Task)
{
	edict_t* pEdict = pBot->pEdict;

	if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
	{
		pBot->DesiredCombatWeapon = GetBotMarinePrimaryWeapon(pBot);
	}
	else
	{
		if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			pBot->DesiredCombatWeapon = GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			pBot->DesiredCombatWeapon = GetBotMarinePrimaryWeapon(pBot);
		}
	}

	// If we're not already building
	if (pBot->pEdict->v.viewmodel != 0)
	{
		// If someone else is building, then we will guard
		edict_t* OtherBuilder = UTIL_GetClosestPlayerOnTeamWithLOS(Task->TaskLocation, MARINE_TEAM, UTIL_MetresToGoldSrcUnits(2.0f), pBot->pEdict);

		if (!FNullEnt(OtherBuilder) && OtherBuilder->v.weaponmodel == 0)
		{
			BotGuardLocation(pBot, Task->TaskLocation);
			return;
		}
	}

	if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Task->TaskTarget, max_player_use_reach, false))
	{
		// If we were ducking before then keep ducking
		if (pBot->pEdict->v.oldbuttons & IN_DUCK)
		{
			pBot->pEdict->v.button |= IN_DUCK;
		}

		BotUseObject(pBot, Task->TaskTarget, true);

		// Haven't started building, maybe not quite looking at the right angle
		if (pBot->pEdict->v.weaponmodel != 0)
		{
			if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(60.0f))
			{
				MoveDirectlyTo(pBot, Task->TaskTarget->v.origin);
			}
			else
			{
				Vector NewViewPoint = UTIL_GetRandomPointInBoundingBox(Task->TaskTarget->v.absmin, Task->TaskTarget->v.absmax);

				BotLookAt(pBot, NewViewPoint);
			}
		}

		return;
	}
	else
	{
		// Might need to duck if it's an infantry portal
		if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < sqrf(max_player_use_reach))
		{
			pBot->pEdict->v.button |= IN_DUCK;
		}
	}

	MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

	if (IsPlayerMarine(pBot->pEdict))
	{
		BotReloadWeapons(pBot);
	}

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		BotLookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
	}

}

void BotProgressGuardTask(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerMarine(pBot->pEdict))
	{
		BotReloadWeapons(pBot);
	}

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		if (IsPlayerLerk(pBot->pEdict))
		{
			if (UTIL_ShouldBotBeCautious(pBot))
			{
				MoveTo(pBot, Task->TaskLocation, MOVESTYLE_HIDE, 100.0f);
			}
			else
			{
				MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL, 100.0f);
			}

			return;
		}


		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}
	else
	{
		if (Task->TaskStartedTime == 0.0f)
		{
			Task->TaskStartedTime = gpGlobals->time;
		}
		BotGuardLocation(pBot, Task->TaskLocation);
	}
}

void BotProgressAttackTask(bot_t* pBot, bot_task* Task)
{
	if (!Task || FNullEnt(Task->TaskTarget)) { return; }

	if (Task->bTargetIsPlayer)
	{
		// For now just move to the target, the combat code will take over once the enemy is sighted
		MoveTo(pBot, UTIL_GetEntityGroundLocation(Task->TaskTarget), MOVESTYLE_AMBUSH);
		return;
	}


	BotAttackTarget(pBot, Task->TaskTarget);
	return;
}

void BotProgressDefendTask(bot_t* pBot, bot_task* Task)
{
	BotProgressGuardTask(pBot, Task);
}

void BotProgressTakeCommandTask(bot_t* pBot)
{
	edict_t* CommChair = UTIL_GetCommChair();

	if (!CommChair) { return; }

	float DistFromChair = vDist2DSq(pBot->pEdict->v.origin, CommChair->v.origin);

	if (!IsPlayerInUseRange(pBot->pEdict, CommChair))
	{
		MoveTo(pBot, CommChair->v.origin, MOVESTYLE_NORMAL);

		if (DistFromChair < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			BotLookAt(pBot, CommChair);
		}
	}
	else
	{
		float CommanderWaitTime = CONFIG_GetCommanderWaitTime();

		if ((gpGlobals->time - GameStartTime) > CommanderWaitTime)
		{
			BotUseObject(pBot, CommChair, false);
		}
		else
		{
			edict_t* NearestHuman = UTIL_GetNearestHumanAtLocation(CommChair->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

			if (!NearestHuman)
			{
				BotUseObject(pBot, CommChair, false);
			}
			else
			{
				BotLookAt(pBot, NearestHuman);
			}
		}

	}
}

void BotProgressEvolveTask(bot_t* pBot, bot_task* Task)
{
	if (!Task || !Task->Evolution) { return; }

	// We tried evolving a second ago and nothing happened. Must be in a bad spot
	if (Task->TaskStartedTime > 0.0f)
	{
		if ((gpGlobals->time - Task->TaskStartedTime) > 1.0f)
		{
			Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, pBot->CurrentFloorPosition, UTIL_MetresToGoldSrcUnits(3.0f));
			Task->TaskStartedTime = 0.0f;
		}
		return;
	}

	if (Task->TaskLocation != ZERO_VECTOR)
	{

		if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) > sqrf(32.0f))
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
			return;
		}
		else
		{
			pBot->pEdict->v.impulse = Task->Evolution;
			Task->TaskStartedTime = gpGlobals->time;
		}
	}
	else
	{
		Task->TaskLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(3.0f));
	}
}

void AlienProgressGetHealthTask(bot_t* pBot, bot_task* Task)
{

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_METABOLIZE))
	{
		pBot->DesiredCombatWeapon = WEAPON_FADE_METABOLIZE;
		if (GetBotCurrentWeapon(pBot) == WEAPON_FADE_METABOLIZE)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
		}
	}

	if (!FNullEnt(Task->TaskTarget))
	{
		Vector MoveLocation = (IsPlayerGorge(Task->TaskTarget)) ? Task->TaskTarget->v.origin : Task->TaskLocation;

		BotGuardLocation(pBot, MoveLocation);

		if (IsPlayerGorge(Task->TaskTarget) && vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
			BotLookAt(pBot, Task->TaskTarget);

			if (gpGlobals->time - pBot->LastCommanderRequestTime > min_request_spam_time)
			{
				pBot->pEdict->v.impulse = IMPULSE_ALIEN_REQUEST_HEALTH;
				pBot->LastCommanderRequestTime = gpGlobals->time;
			}
		}
	}
}

void AlienProgressHealTask(bot_t* pBot, bot_task* Task)
{
	if (!IsPlayerGorge(pBot->pEdict) || FNullEnt(Task->TaskTarget) || IsPlayerDead(Task->TaskTarget)) { return; }

	float DesiredDist = (IsEdictStructure(Task->TaskTarget)) ? kHealingSprayRange : (kHealingSprayRange * 0.5f);

	BotAttackResult LOSCheck = PerformAttackLOSCheck(pBot, WEAPON_GORGE_HEALINGSPRAY, Task->TaskTarget);

	if (LOSCheck == ATTACK_SUCCESS)
	{
		pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;
		BotLookAt(pBot, Task->TaskTarget->v.origin);
		if (GetBotCurrentWeapon(pBot) == WEAPON_GORGE_HEALINGSPRAY)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
		}

		return;
	}
	else
	{
		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL, kHealingSprayRange);
	}
}

void AlienProgressBuildTask(bot_t* pBot, bot_task* Task)
{
	if (!FNullEnt(Task->TaskTarget))
	{

		if (IsPlayerInUseRange(pBot->pEdict, Task->TaskTarget))
		{
			BotUseObject(pBot, Task->TaskTarget, true);
			if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(60.0f))
			{
				MoveDirectlyTo(pBot, Task->TaskTarget->v.origin);
			}
			return;
		}

		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);

		return;
	}

	if (Task->StructureType == STRUCTURE_ALIEN_RESTOWER)
	{
		const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

		if (ResNodeIndex)
		{
			if (ResNodeIndex->bIsOccupied && !ResNodeIndex->bIsOwnedByMarines)
			{
				Task->TaskTarget = ResNodeIndex->TowerEdict;
				return;
			}
		}
	}

	// If we are building a chamber
	if (Task->StructureType != STRUCTURE_ALIEN_RESTOWER && Task->StructureType != STRUCTURE_ALIEN_HIVE)
	{
		dtPolyRef Poly = UTIL_GetNavAreaAtLocation(BUILDING_REGULAR_NAV_PROFILE, Task->TaskLocation);

		if (Poly != SAMPLE_POLYAREA_GROUND)
		{
			Vector NewLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, Task->TaskLocation, UTIL_MetresToGoldSrcUnits(2.0f));

			if (NewLocation != ZERO_VECTOR)
			{
				Task->TaskLocation = NewLocation;
				return;
			}
		}
	}

	int ResRequired = UTIL_GetCostOfStructureType(Task->StructureType);

	if (!IsPlayerGorge(pBot->pEdict))
	{
		ResRequired += kGorgeEvolutionCost;
	}

	if (pBot->resources >= ResRequired)
	{
		float DistFromBuildLocation = vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation);

		if (Task->StructureType == STRUCTURE_ALIEN_HIVE)
		{
			const hive_definition* HiveToBuild = UTIL_GetNearestHiveAtLocation(Task->TaskLocation);

			if (DistFromBuildLocation < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && UTIL_QuickTrace(pBot->pEdict, pBot->pEdict->v.origin, HiveToBuild->edict->v.origin))
			{
				if (!IsPlayerGorge(pBot->pEdict))
				{
					BotEvolveLifeform(pBot, CLASS_GORGE);
					return;
				}

				Vector LookLocation = HiveToBuild->edict->v.origin;

				BotLookAt(pBot, LookLocation);

				float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->pEdict->v.v_angle), UTIL_GetVectorNormal2D(Task->TaskLocation - pBot->pEdict->v.origin));

				if (LookDot > 0.9f)
				{
					pBot->pEdict->v.impulse = UTIL_StructureTypeToImpulseCommand(Task->StructureType);
					Task->LastBuildAttemptTime = gpGlobals->time;
					Task->BuildAttempts++;
					Task->bIsWaitingForBuildLink = true;
					Task->bTaskIsUrgent = true;
				}

				return;
			}
			else
			{
				MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
				return;
			}
		}


		float DesiredDist = (Task->StructureType == STRUCTURE_ALIEN_RESTOWER) ? UTIL_MetresToGoldSrcUnits(2.0f) : UTIL_MetresToGoldSrcUnits(1.1f);

		

		if (DistFromBuildLocation > sqrf(DesiredDist) || !UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, (Task->TaskLocation + Vector(0.0f, 0.0f, 50.0f))))
		{
			MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
			return;
		}

		if (!IsPlayerGorge(pBot->pEdict))
		{
			BotEvolveLifeform(pBot, CLASS_GORGE);
			return;
		}

		if (DistFromBuildLocation < sqrf(UTIL_MetresToGoldSrcUnits(1.0f)))
		{
			Vector MoveDir = UTIL_GetVectorNormal2D(pBot->pEdict->v.origin - Task->TaskLocation);
			Vector NewLocation = Task->TaskLocation + (MoveDir * UTIL_MetresToGoldSrcUnits(1.1f));
			BotLookAt(pBot, Task->TaskLocation);
			MoveDirectlyTo(pBot, NewLocation);
			return;
		}

		Vector LookLocation = Task->TaskLocation;
		LookLocation.z = Task->TaskLocation.z + GetPlayerHeight(pBot->pEdict, false);
		BotLookAt(pBot, LookLocation);

		if (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f) { return; }

		float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->pEdict->v.v_angle), UTIL_GetVectorNormal2D(Task->TaskLocation - pBot->pEdict->v.origin));

		if (LookDot > 0.9f)
		{
			pBot->pEdict->v.impulse = UTIL_StructureTypeToImpulseCommand(Task->StructureType);
			Task->LastBuildAttemptTime = gpGlobals->time;
			Task->BuildAttempts++;
			Task->bIsWaitingForBuildLink = true;
			Task->bTaskIsUrgent = true;
		}

	}
	else
	{
		BotGuardLocation(pBot, Task->TaskLocation);
	}


}

void AlienProgressCapResNodeTask(bot_t* pBot, bot_task* Task)
{
	const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return; }

	int NumResourcesRequired = (IsPlayerGorge(pBot->pEdict) ? kResourceTowerCost : (kResourceTowerCost + kGorgeEvolutionCost));

	Task->bTaskIsUrgent = (UTIL_GetNumPlacedStructuresOfType(STRUCTURE_ALIEN_RESTOWER) < 3) || (pBot->resources >= (NumResourcesRequired - 5) && !ResNodeIndex->bIsOccupied);

	float DistFromNode = vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation);

	if (DistFromNode > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)) || !UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, (Task->TaskLocation + Vector(0.0f, 0.0f, 50.0f))))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		return;
	}

	if (ResNodeIndex->bIsOccupied && !FNullEnt(ResNodeIndex->TowerEdict))
	{
		Task->TaskTarget = ResNodeIndex->TowerEdict;
		if (ResNodeIndex->bIsOwnedByMarines)
		{

			if (IsPlayerGorge(pBot->pEdict) && !PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_BILEBOMB))
			{
				if (UTIL_IsAlienPlayerInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict))
				{
					BotGuardLocation(pBot, Task->TaskLocation);
				}
				else
				{
					BotEvolveLifeform(pBot, CLASS_SKULK);
				}
			}
			else
			{
				NSWeapon AttackWeapon = BotAlienChooseBestWeaponForStructure(pBot, Task->TaskTarget);

				float MaxRange = GetMaxIdealWeaponRange(AttackWeapon);
				bool bHullSweep = IsMeleeWeapon(AttackWeapon);

				if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Task->TaskTarget, MaxRange, bHullSweep))
				{
					pBot->DesiredCombatWeapon = AttackWeapon;

					if (GetBotCurrentWeapon(pBot) == AttackWeapon)
					{
						BotShootTarget(pBot, pBot->DesiredCombatWeapon, Task->TaskTarget);
						return;
					}
				}
				else
				{
					MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
				}
			}
			return;
		}
		else
		{
			if (!UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict))
			{
				if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, ResNodeIndex->TowerEdict, max_player_use_reach, true))
				{
					BotUseObject(pBot, ResNodeIndex->TowerEdict, true);
					if (vDist2DSq(pBot->pEdict->v.origin, ResNodeIndex->TowerEdict->v.origin) > sqrf(60.0f))
					{
						MoveDirectlyTo(pBot, ResNodeIndex->TowerEdict->v.origin);
					}
					return;
				}

				MoveTo(pBot, ResNodeIndex->TowerEdict->v.origin, MOVESTYLE_NORMAL);

				if (vDist2DSq(pBot->pEdict->v.origin, ResNodeIndex->TowerEdict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					BotLookAt(pBot, UTIL_GetCentreOfEntity(ResNodeIndex->TowerEdict));
				}

				return;
			}
		}

		return;
	}

	if (!IsPlayerGorge(pBot->pEdict))
	{
		BotEvolveLifeform(pBot, CLASS_GORGE);
		return;
	}

	BotLookAt(pBot, Task->TaskLocation);

	if (gpGlobals->time - Task->LastBuildAttemptTime < 1.0f) { return; }

	float LookDot = UTIL_GetDotProduct2D(UTIL_GetForwardVector2D(pBot->pEdict->v.v_angle), UTIL_GetVectorNormal2D(Task->TaskLocation - pBot->pEdict->v.origin));

	if (LookDot > 0.9f)
	{

		pBot->pEdict->v.impulse = IMPULSE_ALIEN_BUILD_RESTOWER;
		Task->LastBuildAttemptTime = gpGlobals->time + 1.0f;
		Task->bIsWaitingForBuildLink = true;
		Task->BuildAttempts++;
	}
}

void BotProgressTask(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return; }

	switch (Task->TaskType)
	{
	case TASK_MOVE:
		BotProgressMoveTask(pBot, Task);
		break;
	case TASK_GET_AMMO:
	case TASK_GET_EQUIPMENT:
	case TASK_GET_WEAPON:
		BotProgressPickupTask(pBot, Task);
		break;
	case TASK_GET_HEALTH:
	{
		if (IsPlayerMarine(pBot->pEdict))
		{
			BotProgressPickupTask(pBot, Task);
		}
		else
		{
			AlienProgressGetHealthTask(pBot, Task);
		}
	}	
	break;
	case TASK_RESUPPLY:
		BotProgressResupplyTask(pBot, Task);
		break;
	case TASK_BUILD:
	{
		if (IsPlayerMarine(pBot->pEdict))
		{
			MarineProgressBuildTask(pBot, Task);
		}
		else
		{
			AlienProgressBuildTask(pBot, Task);
		}
	}
	break;
	case TASK_GUARD:
		BotProgressGuardTask(pBot, Task);
		break;
	case TASK_ATTACK:
		BotProgressAttackTask(pBot, Task);
		break;
	case TASK_CAP_RESNODE:
	{
		if (IsPlayerMarine(pBot->pEdict))
		{
			MarineProgressCapResNodeTask(pBot, Task);
		}
		else
		{
			AlienProgressCapResNodeTask(pBot, Task);
		}
	}
	break;
	case TASK_WELD:
		MarineProgressWeldTask(pBot, Task);
		break;
	case TASK_DEFEND:
		BotProgressDefendTask(pBot, Task);
		break;
	case TASK_COMMAND:
		BotProgressTakeCommandTask(pBot);
		break;
	case TASK_EVOLVE:
		BotProgressEvolveTask(pBot, Task);
		break;
	case TASK_HEAL:
		AlienProgressHealTask(pBot, Task);
		break;
	default:
		break;

	}
}

void MarineProgressWeldTask(bot_t* pBot, bot_task* Task)
{
	//float DistFromWeldLocation = vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin);


	if (IsPlayerInUseRange(pBot->pEdict, Task->TaskTarget))
	{
		BotLookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));
		pBot->DesiredCombatWeapon = WEAPON_MARINE_WELDER;

		if (GetBotCurrentWeapon(pBot) != WEAPON_MARINE_WELDER)
		{
			return;
		}

		pBot->pEdict->v.button |= IN_ATTACK;

		return;
	}
	else
	{
		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
	}

	return;

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
	{
		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
		return;
	}

	if (!UTIL_PlayerHasLOSToEntity(pBot->pEdict, Task->TaskTarget, 9999.0f, false))
	{
		BotLookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));

		Vector WeldLocation = pBot->BotNavInfo.TargetDestination;

		if (!WeldLocation || vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(1.5f)))
		{
			int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);
			WeldLocation = UTIL_GetRandomPointOnNavmeshInDonut(MoveProfile, Task->TaskTarget->v.origin, UTIL_MetresToGoldSrcUnits(1.0f), UTIL_MetresToGoldSrcUnits(1.5f));

			if (!WeldLocation)
			{
				WeldLocation = Task->TaskTarget->v.origin;
			}
		}

		MoveTo(pBot, WeldLocation, MOVESTYLE_NORMAL);

		return;
	}
	else
	{
		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
	}
}

void MarineProgressCapResNodeTask(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return; }

	float DistFromNode = vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation);

	if (DistFromNode > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) || !UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, (Task->TaskLocation + Vector(0.0f, 0.0f, 50.0f))))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);

		BotReloadWeapons(pBot);

		return;
	}

	const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return; }

	if (ResNodeIndex->bIsOccupied && !FNullEnt(ResNodeIndex->TowerEdict))
	{
		Task->TaskTarget = ResNodeIndex->TowerEdict;
		// Cancel the waiting timeout since a tower has been placed for us
		Task->TaskLength = 0.0f;

		if (ResNodeIndex->bIsOwnedByMarines)
		{
			if (!UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict))
			{
				// Now we're committed, don't get distracted
				Task->bTaskIsUrgent = true;
				if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, ResNodeIndex->TowerEdict, max_player_use_reach, true))
				{
					BotUseObject(pBot, ResNodeIndex->TowerEdict, true);
					if (vDist2DSq(pBot->pEdict->v.origin, ResNodeIndex->TowerEdict->v.origin) > sqrf(50.0f))
					{
						MoveDirectlyTo(pBot, ResNodeIndex->TowerEdict->v.origin);
					}
					return;
				}

				MoveTo(pBot, ResNodeIndex->TowerEdict->v.origin, MOVESTYLE_NORMAL);

				if (vDist2DSq(pBot->pEdict->v.origin, ResNodeIndex->TowerEdict->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					BotLookAt(pBot, UTIL_GetCentreOfEntity(ResNodeIndex->TowerEdict));
				}

				return;
			}
		}
		else
		{
			NSWeapon AttackWeapon = BotMarineChooseBestWeaponForStructure(pBot, Task->TaskTarget);

			float MaxRange = GetMaxIdealWeaponRange(AttackWeapon);
			bool bHullSweep = IsMeleeWeapon(AttackWeapon);

			if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Task->TaskTarget, MaxRange, bHullSweep))
			{
				pBot->DesiredCombatWeapon = AttackWeapon;

				if (GetBotCurrentWeapon(pBot) == AttackWeapon)
				{
					BotShootTarget(pBot, pBot->DesiredCombatWeapon, Task->TaskTarget);
				}
			}
			else
			{
				MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
			}
		}
	}
	else
	{
		// Give the commander 30 seconds to drop a tower for us, or give up and move on
		if (Task->TaskLength == 0.0f)
		{
			Task->TaskStartedTime = gpGlobals->time;
			Task->TaskLength = 30.0f;
		}
		BotGuardLocation(pBot, Task->TaskLocation);
	}


}

void BotGuardLocation(bot_t* pBot, const Vector GuardLocation)
{
	float DistFromGuardLocation = vDist2DSq(pBot->pEdict->v.origin, GuardLocation);

	if (DistFromGuardLocation > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		pBot->GuardInfo.GuardLocation = ZERO_VECTOR;
		MoveTo(pBot, GuardLocation, MOVESTYLE_NORMAL);
		return;
	}

	if (!pBot->GuardInfo.GuardLocation)
	{
		UTIL_GenerateGuardWatchPoints(pBot, GuardLocation);
		pBot->GuardInfo.GuardLocation = GuardLocation;
	}

	if (gpGlobals->time - pBot->GuardInfo.GuardStartLookTime > pBot->GuardInfo.ThisGuardLookTime)
	{
		if (pBot->GuardInfo.NumGuardPoints > 0)
		{
			int NewGuardLookIndex = irandrange(0, (pBot->GuardInfo.NumGuardPoints - 1));

			pBot->GuardInfo.GuardLookLocation = pBot->GuardInfo.GuardPoints[NewGuardLookIndex];
		}
		else
		{
			pBot->GuardInfo.GuardLookLocation = UTIL_GetRandomPointOnNavmeshInRadius(SKULK_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

			pBot->GuardInfo.GuardLookLocation.z = pBot->CurrentEyePosition.z;
		}

		Vector LookDir = UTIL_GetVectorNormal2D(pBot->GuardInfo.GuardLookLocation - GuardLocation);

		Vector NewMoveCentre = GuardLocation - (LookDir * UTIL_MetresToGoldSrcUnits(2.0f));

		Vector NewMoveLoc = UTIL_GetRandomPointOnNavmeshInRadius(MARINE_REGULAR_NAV_PROFILE, NewMoveCentre, UTIL_MetresToGoldSrcUnits(2.0f));

		if (NewMoveLoc != ZERO_VECTOR)
		{
			pBot->GuardInfo.GuardStandPosition = NewMoveLoc;
		}
		else
		{
			pBot->GuardInfo.GuardStandPosition = GuardLocation;
		}

		pBot->GuardInfo.ThisGuardLookTime = frandrange(2.0f, 5.0f);
		pBot->GuardInfo.GuardStartLookTime = gpGlobals->time;
	}

	if (IsPlayerLerk(pBot->pEdict))
	{
		MoveTo(pBot, pBot->GuardInfo.GuardStandPosition, MOVESTYLE_HIDE);
	}
	else
	{
		MoveTo(pBot, pBot->GuardInfo.GuardStandPosition, MOVESTYLE_NORMAL);
	}

	

	BotLookAt(pBot, pBot->GuardInfo.GuardLookLocation);


}

void UTIL_ClearGuardInfo(bot_t* pBot)
{
	memset(&pBot->GuardInfo, 0, sizeof(bot_guard_info));
}

void UTIL_GenerateGuardWatchPoints(bot_t* pBot, const Vector& GuardLocation)
{
	const edict_t* pEdict = pBot->pEdict;

	UTIL_ClearGuardInfo(pBot);

	int MoveProfileIndex = (IsPlayerOnMarineTeam(pEdict)) ? SKULK_REGULAR_NAV_PROFILE : MARINE_REGULAR_NAV_PROFILE;

	bot_path_node path[MAX_PATH_SIZE];
	int pathSize = 0;

	if (UTIL_GetNumTotalHives() == 0)
	{
		PopulateEmptyHiveList();
	}

	for (int i = 0; i < UTIL_GetNumTotalHives(); i++)
	{
		const hive_definition* Hive = UTIL_GetHiveAtIndex(i);

		if (!Hive || FNullEnt(Hive->edict)) { continue; }

		if (UTIL_QuickTrace(pEdict, GuardLocation + Vector(0.0f, 0.0f, 10.0f), Hive->Location) || vDist2DSq(GuardLocation, Hive->Location) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f))) { continue; }

		dtStatus SearchResult = FindPathClosestToPoint(MoveProfileIndex, Hive->FloorLocation, GuardLocation, path, &pathSize, 500.0f);

		if (dtStatusSucceed(SearchResult))
		{
			Vector FinalApproachDir = UTIL_GetVectorNormal2D(path[pathSize - 1].Location - path[pathSize - 2].Location);
			Vector ProspectiveNewGuardLoc = GuardLocation - (FinalApproachDir * 300.0f);

			ProspectiveNewGuardLoc.z = path[pathSize - 2].Location.z;

			pBot->GuardInfo.GuardPoints[pBot->GuardInfo.NumGuardPoints++] = ProspectiveNewGuardLoc;
		}
	}

	if (vDist2DSq(GuardLocation, UTIL_GetCommChairLocation()) > sqrf(UTIL_MetresToGoldSrcUnits(20.0f)))
	{
		dtStatus SearchResult = FindPathClosestToPoint(MoveProfileIndex, UTIL_GetCommChairLocation(), GuardLocation, path, &pathSize, 500.0f);

		if (dtStatusSucceed(SearchResult))
		{
			Vector FinalApproachDir = UTIL_GetVectorNormal2D(path[pathSize - 1].Location - path[pathSize - 2].Location);
			Vector ProspectiveNewGuardLoc = GuardLocation - (FinalApproachDir * 300.0f);

			ProspectiveNewGuardLoc.z = path[pathSize - 2].Location.z;

			pBot->GuardInfo.GuardPoints[pBot->GuardInfo.NumGuardPoints++] = ProspectiveNewGuardLoc;
		}
	}

}

bool BotWithBuildTaskExists(NSStructureType StructureType)
{
	for (int i = 0; i < 32; i++)
	{
		if (!bots[i].is_used || FNullEnt(bots[i].pEdict)) { continue; }

		if ((bots[i].PrimaryBotTask.TaskType == TASK_BUILD && bots[i].PrimaryBotTask.StructureType == StructureType) || (bots[i].SecondaryBotTask.TaskType == TASK_BUILD && bots[i].SecondaryBotTask.StructureType == StructureType))
		{
			return true;
		}
	}

	return false;
}

edict_t* GetFirstBotWithBuildTask(NSStructureType StructureType, edict_t* IgnorePlayer)
{
	for (int i = 0; i < 32; i++)
	{
		if (!bots[i].is_used || FNullEnt(bots[i].pEdict || bots[i].pEdict == IgnorePlayer)) { continue; }

		if ((bots[i].PrimaryBotTask.TaskType == TASK_BUILD && bots[i].PrimaryBotTask.StructureType == StructureType) || (bots[i].SecondaryBotTask.TaskType == TASK_BUILD && bots[i].SecondaryBotTask.StructureType == StructureType))
		{
			return bots[i].pEdict;
		}
	}

	return nullptr;
}

char* UTIL_TaskTypeToChar(const BotTaskType TaskType)
{
	switch (TaskType)
	{
	case TASK_NONE:
		return "None";
	case TASK_BUILD:
		return "Build";
	case TASK_GET_AMMO:
		return "Get Ammo";
	case TASK_ATTACK:
		return "Attack";
	case TASK_GET_EQUIPMENT:
		return "Get Equipment";
	case TASK_GET_HEALTH:
		return "Get Health";
	case TASK_GET_WEAPON:
		return "Get Weapon";
	case TASK_GUARD:
		return "Guard";
	case TASK_HEAL:
		return "Heal";
	case TASK_MOVE:
		return "Move";
	case TASK_RESUPPLY:
		return "Resupply";
	case TASK_CAP_RESNODE:
		return "Cap Resource Node";
	case TASK_WELD:
		return "Weld";
	case TASK_DEFEND:
		return "Defend";
	case TASK_EVOLVE:
		return "Evolve";
	default:
		return "INVALID";
	}
}

void TASK_SetAttackTask(bot_t* pBot, bot_task* Task, edict_t* Target, const bool bIsUrgent)
{
	if (Task->TaskType == TASK_ATTACK && Task->TaskTarget == Target) 
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	UTIL_ClearBotTask(pBot, Task);

	// Don't set the task if the target is invalid, dead or on the same team as the bot (can't picture a situation where you want them to teamkill...)
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO) || Target->v.team == pBot->pEdict->v.team) { return; }

	Task->TaskType = TASK_ATTACK;
	Task->TaskTarget = Target;
	Task->bTaskIsUrgent = bIsUrgent;

	// We don't need an attack location for players since this will be moving around anyway
	if (IsEdictPlayer(Target))
	{
		Task->bTargetIsPlayer = true;
		return;
	}

	int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	// Get as close as possible to the target
	Vector AttackLocation = FindClosestNavigablePointToDestination(BotProfile, pBot->CurrentFloorPosition, UTIL_GetEntityGroundLocation(Target), UTIL_MetresToGoldSrcUnits(20.0f));

	if (AttackLocation != ZERO_VECTOR)
	{
		Task->TaskLocation = AttackLocation;
	}
	else
	{
		AttackLocation = Target->v.origin;
	}
}

void TASK_SetMoveTask(bot_t* pBot, bot_task* Task, const Vector Location, bool bIsUrgent)
{
	if (Task->TaskType == TASK_MOVE && vDist2DSq(Task->TaskLocation, Location) < sqrf(100.0f))
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	UTIL_ClearBotTask(pBot, Task);

	if (!Location) { return; }

	int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	// Get as close as possible to desired location
	Vector MoveLocation = FindClosestNavigablePointToDestination(BotProfile, pBot->CurrentFloorPosition, Location, UTIL_MetresToGoldSrcUnits(20.0f));

	if (MoveLocation != ZERO_VECTOR)
	{
		Task->TaskType = TASK_MOVE;
		Task->TaskLocation = MoveLocation;
		Task->bTaskIsUrgent = bIsUrgent;
		Task->TaskLength = 60.0f; // Set a maximum time to reach destination. Helps avoid bots getting permanently stuck
	}
}

void TASK_SetBuildTask(bot_t* pBot, bot_task* Task, const NSStructureType StructureType, const Vector Location, const bool bIsUrgent)
{
	UTIL_ClearBotTask(pBot, Task);

	if (!Location) { return; }

	float MaxDist = (StructureType == STRUCTURE_ALIEN_HIVE) ? UTIL_MetresToGoldSrcUnits(5.0f) : UTIL_MetresToGoldSrcUnits(1.0f);

	if (Task->TaskType == TASK_BUILD && Task->StructureType == StructureType && vDist2DSq(Task->TaskLocation, Location) < sqrf(MaxDist)) { return; }

	int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	// Get as close as possible to desired location
	Vector BuildLocation = FindClosestNavigablePointToDestination(BotProfile, pBot->CurrentFloorPosition, Location, MaxDist);

	if (BuildLocation != ZERO_VECTOR)
	{
		Task->TaskType = TASK_BUILD;
		Task->TaskLocation = BuildLocation;
		Task->StructureType = StructureType;
		Task->bTaskIsUrgent = bIsUrgent;

		if (StructureType == STRUCTURE_ALIEN_HIVE)
		{
			char buf[64];
			sprintf(buf, "I'll drop hive at %s", UTIL_GetClosestMapLocationToPoint(Task->TaskLocation));

			BotTeamSay(pBot, 1.0f, buf);
		}
	}
}

void TASK_SetBuildTask(bot_t* pBot, bot_task* Task, edict_t* StructureToBuild, const bool bIsUrgent)
{
	UTIL_ClearBotTask(pBot, Task);

	if (FNullEnt(StructureToBuild) || UTIL_StructureIsFullyBuilt(StructureToBuild)) { return; }

	if (Task->TaskType == TASK_BUILD && Task->TaskTarget == StructureToBuild) { return; }

	int BotProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	// Get as close as possible to desired location
	Vector BuildLocation = FindClosestNavigablePointToDestination(BotProfile, pBot->CurrentFloorPosition, UTIL_GetFloorUnderEntity(StructureToBuild), max_player_use_reach);

	if (BuildLocation != ZERO_VECTOR)
	{
		Task->TaskType = TASK_BUILD;
		Task->TaskTarget = StructureToBuild;
		Task->TaskLocation = BuildLocation;
		Task->bTaskIsUrgent = bIsUrgent;
		Task->StructureType = GetStructureTypeFromEdict(StructureToBuild);
	}
}

void TASK_SetCapResNodeTask(bot_t* pBot, bot_task* Task, const resource_node* NodeRef, const bool bIsUrgent)
{
	UTIL_ClearBotTask(pBot, Task);

	if (!NodeRef) { return; }

	NSStructureType NodeStructureType = (IsPlayerMarine(pBot->pEdict)) ? STRUCTURE_MARINE_RESTOWER : STRUCTURE_ALIEN_RESTOWER;

	Task->TaskType = TASK_CAP_RESNODE;
	Task->StructureType = NodeStructureType;
	Task->TaskLocation = NodeRef->origin;

	if (!FNullEnt(NodeRef->TowerEdict))
	{
		Task->TaskTarget = NodeRef->TowerEdict;
	}

	Task->bTaskIsUrgent = bIsUrgent;
}

void TASK_SetDefendTask(bot_t* pBot, bot_task* Task, edict_t* Target, const bool bIsUrgent)
{
	if (Task->TaskType == TASK_DEFEND && Task->TaskTarget == Target)
	{
		Task->bTaskIsUrgent = bIsUrgent;
		return;
	}

	UTIL_ClearBotTask(pBot, Task);

	// Can't defend an invalid or dead target
	if (FNullEnt(Target) || Target->v.deadflag != DEAD_NO) { return; }

	Task->TaskType = TASK_DEFEND;
	Task->TaskTarget = Target;
	Task->bTaskIsUrgent = bIsUrgent;

	int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	Vector DefendPoint = FindClosestNavigablePointToDestination(MoveProfile, pBot->CurrentFloorPosition, UTIL_GetEntityGroundLocation(Target), UTIL_MetresToGoldSrcUnits(10.0f));

	if (DefendPoint != ZERO_VECTOR)
	{
		Task->TaskLocation = DefendPoint;
	}
	else
	{
		Task->TaskLocation = UTIL_GetEntityGroundLocation(Target);
	}

	
}

void TASK_SetEvolveTask(bot_t* pBot, bot_task* Task, const Vector EvolveLocation, const int EvolveImpulse, const bool bIsUrgent)
{
	UTIL_ClearBotTask(pBot, Task);

	if (EvolveImpulse <= 0) { return; }

	Task->TaskType = TASK_EVOLVE;
	Task->TaskLocation = EvolveLocation;
	Task->Evolution = EvolveImpulse;
	Task->bTaskIsUrgent = bIsUrgent;
}
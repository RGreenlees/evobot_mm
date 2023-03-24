//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_marine.cpp
// 
// Contains marine-specific logic.
//

#include <extdll.h>
#include <dllapi.h>
#include <h_export.h>
#include <meta_api.h>

#include "bot_marine.h"
#include "bot_tactical.h"
#include "bot_navigation.h"

void MarineThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy > -1)
	{
		MarineCombatThink(pBot);

		if (pBot->DesiredCombatWeapon == WEAPON_NONE)
		{
			pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict);
		}

		return;
	}

	edict_t* DangerTurret = BotGetNearestDangerTurret(pBot, UTIL_MetresToGoldSrcUnits(15.0f));

	if (!FNullEnt(DangerTurret) && UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, DangerTurret->v.origin))
	{

		if (pBot->SecondaryBotTask.TaskTarget != DangerTurret)
		{
			if (BotGetCurrentWeaponClipAmmo(pBot) > 0 || BotGetCurrentWeaponReserveAmmo(pBot) > 0)
			{
				pBot->SecondaryBotTask.TaskType = TASK_ATTACK;
				pBot->SecondaryBotTask.TaskTarget = DangerTurret;
				pBot->SecondaryBotTask.TaskLocation = DangerTurret->v.origin;
				pBot->SecondaryBotTask.bOrderIsUrgent = true;
			}
		}
	}

	if (pBot->CurrentRole == BOT_ROLE_NONE)
	{
		pBot->CurrentRole = BOT_ROLE_FIND_RESOURCES;
		return;
	}

	if (pBot->CurrentRole == BOT_ROLE_COMMAND)
	{
		if (!UTIL_CommChairExists() || UTIL_IsThereACommander() || UTIL_GetBotsWithRoleType(BOT_ROLE_COMMAND, true) > 1)
		{
			pBot->CurrentRole = BOT_ROLE_GUARD_BASE;
			return;
		}

		BotProgressTakeCommandTask(pBot);

		return;
	}

	UpdateAndClearTasks(pBot);

	if (pBot->PrimaryBotTask.TaskType == TASK_NONE)
	{
		BotMarineSetPrimaryTask(pBot, &pBot->PrimaryBotTask);
	}
	else
	{
		if (!pBot->PrimaryBotTask.bOrderIsUrgent)
		{
			BotMarineSetPrimaryTask(pBot, &pBot->PendingTask);

			if (pBot->PendingTask.TaskType != TASK_NONE && pBot->PendingTask.bOrderIsUrgent)
			{
				memcpy(&pBot->PrimaryBotTask, &pBot->PendingTask, sizeof(bot_task));
			}
		}
	}

	if (pBot->SecondaryBotTask.TaskType == TASK_NONE)
	{
		BotMarineSetSecondaryTask(pBot, &pBot->SecondaryBotTask);
	}
	else
	{
		if (!pBot->SecondaryBotTask.bOrderIsUrgent)
		{
			BotMarineSetSecondaryTask(pBot, &pBot->PendingTask);

			if (pBot->PendingTask.TaskType != TASK_NONE && pBot->PendingTask.bOrderIsUrgent)
			{
				memcpy(&pBot->SecondaryBotTask, &pBot->PendingTask, sizeof(bot_task));
			}
		}
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

void MarineCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	// ENEMY IS OUT OF SIGHT

	if (!TrackedEnemyRef->bCurrentlyVisible && !UTIL_QuickTrace(pEdict, pBot->CurrentEyePosition, CurrentEnemy->v.origin))
	{

		// If we're out of primary ammo or badly hurt, then use opportunity to disengage and head to the nearest armoury to resupply
		if ((BotGetCurrentWeaponClipAmmo(pBot) < BotGetCurrentWeaponMaxClipAmmo(pBot) && BotGetPrimaryWeaponAmmoReserve(pBot) == 0) || pBot->pEdict->v.health < 50.0f)
		{
			edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(50.0f), true);

			if (!FNullEnt(Armoury))
			{
				if (UTIL_PlayerInUseRange(pBot->pEdict, Armoury))
				{
					pBot->DesiredCombatWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);

					if (UTIL_GetBotCurrentWeapon(pBot) == pBot->DesiredCombatWeapon)
					{
						BotUseObject(pBot, Armoury, true);
					}
				}
				else
				{
					MoveTo(pBot, Armoury->v.origin, MOVESTYLE_NORMAL);
				}
				return;
			}
		}

		MarineHuntEnemy(pBot, TrackedEnemyRef);

		return;
	}

	// ENEMY IS VISIBLE

	pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, CurrentEnemy);

	if (UTIL_GetBotCurrentWeapon(pBot) != pBot->DesiredCombatWeapon) { return; }

	NSPlayerClass EnemyClass = UTIL_GetPlayerClass(CurrentEnemy);
	float DistFromEnemySq = vDist2DSq(pEdict->v.origin, CurrentEnemy->v.origin);
	float MaxWeaponRange = UTIL_GetMaxIdealWeaponRange(UTIL_GetBotCurrentWeapon(pBot));
	float MinWeaponRange = UTIL_GetMinIdealWeaponRange(UTIL_GetBotCurrentWeapon(pBot));

	if (UTIL_GetBotCurrentWeapon(pBot) != WEAPON_MARINE_KNIFE && BotGetCurrentWeaponClipAmmo(pBot) == 0)
	{
		LookAt(pBot, CurrentEnemy);
		pEdict->v.button |= IN_RELOAD;

		Vector EnemyOrientation = UTIL_GetVectorNormal2D(pBot->pEdict->v.origin - CurrentEnemy->v.origin);

		Vector RetreatLocation = pBot->pEdict->v.origin + (EnemyOrientation * 100.0f);

		MoveTo(pBot, RetreatLocation, MOVESTYLE_NORMAL);

		return;
	}

	// If we're really low on ammo then retreat to the nearest armoury while continuing to engage
	if (BotGetPrimaryWeaponClipAmmo(pBot) < BotGetPrimaryWeaponMaxClipSize(pBot) && BotGetPrimaryWeaponAmmoReserve(pBot) == 0 && BotGetSecondaryWeaponAmmoReserve(pBot) == 0)
	{
		edict_t* Armoury = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_MARINE_ANYARMOURY, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(50.0f), true);

		if (!FNullEnt(Armoury))
		{
			if (UTIL_PlayerInUseRange(pBot->pEdict, Armoury))
			{
				pBot->DesiredCombatWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);

				if (UTIL_GetBotCurrentWeapon(pBot) == pBot->DesiredCombatWeapon)
				{
					BotUseObject(pBot, Armoury, true);
				}

				return;
			}
			else
			{
				MoveTo(pBot, Armoury->v.origin, MOVESTYLE_NORMAL);
			}
		}

		if (UTIL_GetBotCurrentWeapon(pBot) == WEAPON_MARINE_KNIFE || BotGetPrimaryWeaponClipAmmo(pBot) > 0)
		{
			BotAttackTarget(pBot, CurrentEnemy);
		}
		return;
	}

	if (EnemyClass == CLASS_GORGE || IsPlayerGestating(CurrentEnemy))
	{
		BotAttackTarget(pBot, CurrentEnemy);
		if (DistFromEnemySq > sqrf(MaxWeaponRange))
		{
			MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);
		}
		else
		{

			Vector EngagementLocation = pBot->BotNavInfo.TargetDestination;

			float EngagementDist = vDist2DSq(EngagementLocation, CurrentEnemy->v.origin);

			if (!EngagementLocation || EngagementDist > sqrf(MaxWeaponRange) || EngagementDist < sqrf(MinWeaponRange) || !UTIL_QuickTrace(pBot->pEdict, EngagementLocation + Vector(0.0f, 0.0f, 10.0f), CurrentEnemy->v.origin))
			{
				float MinMaxDiff = MaxWeaponRange - MinWeaponRange;

				Vector EnemyOrientation = UTIL_GetVectorNormal2D(pBot->pEdict->v.origin - CurrentEnemy->v.origin);
				float MidDist = MinWeaponRange + (MinMaxDiff * 0.5f);

				Vector MidPoint = CurrentEnemy->v.origin + (EnemyOrientation * MidDist);

				MidPoint = UTIL_ProjectPointToNavmesh(MidPoint, NavProfileIndex);

				if (MidPoint != ZERO_VECTOR)
				{
					EngagementLocation = UTIL_GetRandomPointOnNavmeshInRadius(NavProfileIndex, MidPoint, MinMaxDiff);
				}
				else
				{
					EngagementLocation = UTIL_GetRandomPointOnNavmeshInRadius(NavProfileIndex, CurrentEnemy->v.origin, MinWeaponRange);
				}

				if (!UTIL_QuickTrace(pBot->pEdict, EngagementLocation + Vector(0.0f, 0.0f, 10.0f), CurrentEnemy->v.origin))
				{
					EngagementLocation = ZERO_VECTOR;
				}
			}

			if (EngagementLocation != ZERO_VECTOR)
			{
				MoveTo(pBot, EngagementLocation, MOVESTYLE_NORMAL);
			}

		}

		return;
	}

	if (DistFromEnemySq > sqrf(MaxWeaponRange))
	{
		BotAttackTarget(pBot, CurrentEnemy);
		MoveTo(pBot, CurrentEnemy->v.origin, MOVESTYLE_NORMAL);

	}
	else
	{
		if (DistFromEnemySq < sqrf(MinWeaponRange))
		{
			Vector CurrentBackOffLocation = pBot->BotNavInfo.TargetDestination;

			if (!CurrentBackOffLocation || vDist2DSq(CurrentBackOffLocation, CurrentEnemy->v.origin) < sqrf(MinWeaponRange) || !UTIL_QuickTrace(pBot->pEdict, CurrentBackOffLocation + Vector(0.0f, 0.0f, 10.0f), CurrentEnemy->v.origin))
			{

				NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

				float MinMaxDiff = MaxWeaponRange - MinWeaponRange;

				Vector EnemyOrientation = UTIL_GetVectorNormal2D(pBot->pEdict->v.origin - CurrentEnemy->v.origin);
				float MidDist = MinWeaponRange + (MinMaxDiff * 0.5f);

				Vector MidPoint = CurrentEnemy->v.origin + (EnemyOrientation * MidDist);

				MidPoint = UTIL_ProjectPointToNavmesh(MidPoint, NavProfileIndex);

				if (MidPoint != ZERO_VECTOR)
				{
					CurrentBackOffLocation = UTIL_GetRandomPointOnNavmeshInRadius(NavProfileIndex, MidPoint, MinMaxDiff);
				}
				else
				{
					CurrentBackOffLocation = UTIL_GetRandomPointOnNavmeshInRadius(NavProfileIndex, CurrentEnemy->v.origin, MinWeaponRange);
				}
			}

			BotAttackTarget(pBot, CurrentEnemy);
			MoveTo(pBot, CurrentBackOffLocation, MOVESTYLE_NORMAL);

			if (DistFromEnemySq < sqrf(UTIL_MetresToGoldSrcUnits(1.0f)))
			{
				BotJump(pBot);
			}

			return;
		}

		Vector EnemyVelocity = UTIL_GetVectorNormal2D(CurrentEnemy->v.velocity);
		Vector EnemyOrientation = UTIL_GetVectorNormal2D(pBot->pEdict->v.origin - CurrentEnemy->v.origin);

		float MoveDot = UTIL_GetDotProduct2D(EnemyVelocity, EnemyOrientation);

		// Enemy is coming at us
		if (MoveDot > 0.0f)
		{
			if (!IsPlayerOnLadder(pBot->pEdict))
			{

				Vector RetreatLocation = pBot->pEdict->v.origin + (EnemyOrientation * 100.0f);

				BotAttackTarget(pBot, CurrentEnemy);
				MoveTo(pBot, RetreatLocation, MOVESTYLE_NORMAL);
			}
			else
			{
				Vector LadderTop = UTIL_GetNearestLadderTopPoint(pBot->pEdict);
				Vector LadderBottom = UTIL_GetNearestLadderBottomPoint(pBot->pEdict);

				Vector EnemyPointOnLine = vClosestPointOnLine(LadderBottom, LadderTop, CurrentEnemy->v.origin);

				bool bGoDownLadder = (vDist3DSq(EnemyPointOnLine, LadderBottom) > vDist3DSq(EnemyPointOnLine, LadderTop));

				Vector RetreatLocation = ZERO_VECTOR;

				if (bGoDownLadder)
				{
					RetreatLocation = UTIL_ProjectPointToNavmesh(LadderBottom);
				}
				else
				{
					RetreatLocation = UTIL_ProjectPointToNavmesh(LadderTop);
				}

				BotAttackTarget(pBot, CurrentEnemy);
				MoveTo(pBot, RetreatLocation, MOVESTYLE_NORMAL);
			}
		}
		else
		{
			if (!IsPlayerOnLadder(pBot->pEdict))
			{
				NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

				Vector EngagementLocation = pBot->BotNavInfo.TargetDestination;

				float EngagementDist = vDist2DSq(EngagementLocation, CurrentEnemy->v.origin);

				if (!EngagementLocation || EngagementDist > sqrf(MaxWeaponRange) || EngagementDist < sqrf(MinWeaponRange) || !UTIL_QuickTrace(pBot->pEdict, EngagementLocation + Vector(0.0f, 0.0f, 10.0f), CurrentEnemy->v.origin))
				{
					EngagementLocation = UTIL_GetRandomPointOnNavmeshInRadius(NavProfileIndex, pBot->pEdict->v.origin, UTIL_MetresToGoldSrcUnits(2.0f));
				}

				BotAttackTarget(pBot, CurrentEnemy);
				MoveTo(pBot, EngagementLocation, MOVESTYLE_NORMAL);
			}
			else
			{
				Vector LadderTop = UTIL_GetNearestLadderTopPoint(pBot->pEdict);
				Vector LadderBottom = UTIL_GetNearestLadderBottomPoint(pBot->pEdict);

				Vector EnemyPointOnLine = vClosestPointOnLine(LadderBottom, LadderTop, CurrentEnemy->v.origin);

				bool bGoDownLadder = (vDist3DSq(EnemyPointOnLine, LadderBottom) < vDist3DSq(EnemyPointOnLine, LadderTop));

				Vector EngagementLocation = ZERO_VECTOR;

				if (bGoDownLadder)
				{
					EngagementLocation = UTIL_ProjectPointToNavmesh(LadderBottom);
				}
				else
				{
					EngagementLocation = UTIL_ProjectPointToNavmesh(LadderTop);
				}

				BotAttackTarget(pBot, CurrentEnemy);
				MoveTo(pBot, EngagementLocation, MOVESTYLE_NORMAL);
			}

		}

	}

}

void MarineHuntEnemy(bot_t* pBot, enemy_status* TrackedEnemy)
{
	edict_t* CurrentEnemy = TrackedEnemy->EnemyEdict;

	if (FNullEnt(CurrentEnemy) || IsPlayerDead(CurrentEnemy)) { return; }

	Vector LastSeenLocation = (TrackedEnemy->bIsTracked) ? TrackedEnemy->TrackedLocation : TrackedEnemy->LastSeenLocation;
	float LastSeenTime = (TrackedEnemy->bIsTracked) ? TrackedEnemy->LastTrackedTime : TrackedEnemy->LastSeenTime;
	float TimeSinceLastSighting = (gpGlobals->time - LastSeenTime);

	// If the enemy is being motion tracked, or the last seen time was within the last 5 seconds, and the suspected location is close enough, then throw a grenade!
	if (BotHasGrenades(pBot) || (BotHasWeapon(pBot, WEAPON_MARINE_GL) && (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)))
	{
		if (TimeSinceLastSighting < 5.0f && vDist3DSq(pBot->pEdict->v.origin, LastSeenLocation) <= sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
		{
			Vector GrenadeThrowLocation = UTIL_GetGrenadeThrowTarget(pBot, LastSeenLocation, UTIL_MetresToGoldSrcUnits(5.0f));

			if (GrenadeThrowLocation != ZERO_VECTOR)
			{
				BotThrowGrenadeAtTarget(pBot, GrenadeThrowLocation);
				return;
			}
		}
	}

	pBot->DesiredCombatWeapon = BotMarineChooseBestWeapon(pBot, CurrentEnemy);

	if (UTIL_GetBotCurrentWeapon(pBot) != pBot->DesiredCombatWeapon) { return; }

	if (BotGetCurrentWeaponClipAmmo(pBot) < BotGetCurrentWeaponMaxClipAmmo(pBot) && BotGetCurrentWeaponReserveAmmo(pBot) > 0)
	{
		if (TrackedEnemy->bIsTracked)
		{
			if (vDist2DSq(pBot->pEdict->v.origin, LastSeenLocation) >= sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
			{
				pBot->pEdict->v.button |= IN_RELOAD;
			}
		}
		else
		{
			float ReloadTime = BotGetCurrentWeaponClipAmmo(pBot) < (BotGetCurrentWeaponMaxClipAmmo(pBot) * 0.5f) ? 2.0f : 5.0f;
			if (gpGlobals->time - LastSeenTime >= ReloadTime)
			{
				pBot->pEdict->v.button |= IN_RELOAD;
			}
		}

	}

	MoveTo(pBot, LastSeenLocation, MOVESTYLE_NORMAL);
	LookAt(pBot, LastSeenLocation);

	return;
}

NSWeapon BotMarineChooseBestWeapon(bot_t* pBot, edict_t* target)
{

	if (!target)
	{
		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			return UTIL_GetBotMarinePrimaryWeapon(pBot);
		}
		else if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			return UTIL_GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			return UTIL_GetBotMarinePrimaryWeapon(pBot);
		}
	}

	if (UTIL_IsEdictPlayer(target))
	{
		float DistFromEnemy = vDist2DSq(pBot->pEdict->v.origin, target->v.origin);

		if (DistFromEnemy <= sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
			if (BotGetPrimaryWeaponClipAmmo(pBot) == 0)
			{
				if (BotGetSecondaryWeaponClipAmmo(pBot) > 0)
				{
					return UTIL_GetBotMarineSecondaryWeapon(pBot);
				}
				else
				{
					return WEAPON_MARINE_KNIFE;
				}
			}
			else
			{
				return UTIL_GetBotMarinePrimaryWeapon(pBot);
			}
		}
		else
		{
			NSWeapon PrimaryWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);

			if (PrimaryWeapon == WEAPON_MARINE_SHOTGUN)
			{
				if (DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
				{
					if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
					{
						return UTIL_GetBotMarineSecondaryWeapon(pBot);
					}
					else
					{
						if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
						{
							return PrimaryWeapon;
						}
						else
						{
							return WEAPON_MARINE_KNIFE;
						}
					}
				}
				else
				{
					if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
					{
						return PrimaryWeapon;
					}
					else
					{
						if (BotGetSecondaryWeaponClipAmmo(pBot) > 0)
						{
							return UTIL_GetBotMarineSecondaryWeapon(pBot);
						}
						else
						{
							return WEAPON_MARINE_KNIFE;
						}
					}
				}
			}
			else
			{
				if (DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
					{
						return PrimaryWeapon;
					}

					if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
					{
						return UTIL_GetBotMarineSecondaryWeapon(pBot);
					}

					return WEAPON_MARINE_KNIFE;
				}
				else
				{
					if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || (DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)) && BotGetPrimaryWeaponAmmoReserve(pBot) > 0))
					{
						return PrimaryWeapon;
					}
					else
					{
						if (BotGetSecondaryWeaponClipAmmo(pBot) > 0)
						{
							return UTIL_GetBotMarineSecondaryWeapon(pBot);
						}
						else
						{
							return WEAPON_MARINE_KNIFE;
						}
					}
				}
			}
		}
	}
	else
	{
		return BotMarineChooseBestWeaponForStructure(pBot, target);
	}
}

NSWeapon BotMarineChooseBestWeaponForStructure(bot_t* pBot, edict_t* target)
{
	NSStructureType StructureType = UTIL_GetStructureTypeFromEdict(target);

	if (StructureType == STRUCTURE_NONE)
	{
		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			return UTIL_GetBotMarinePrimaryWeapon(pBot);
		}
		else if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			return UTIL_GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			return UTIL_GetBotMarinePrimaryWeapon(pBot);
		}
	}

	if (StructureType == STRUCTURE_ALIEN_HIVE || StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
	{
		if (UTIL_GetBotMarinePrimaryWeapon(pBot) == WEAPON_MARINE_MG)
		{
			if (BotHasGrenades(pBot))
			{
				return WEAPON_MARINE_GRENADE;
			}
		}

		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			return UTIL_GetBotMarinePrimaryWeapon(pBot);
		}
		else if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			return UTIL_GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			return WEAPON_MARINE_KNIFE;
		}
	}
	else
	{
		NSWeapon PrimaryWeapon = UTIL_GetBotMarinePrimaryWeapon(pBot);

		if ((PrimaryWeapon == WEAPON_MARINE_GL || PrimaryWeapon == WEAPON_MARINE_SHOTGUN) && (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0))
		{
			return PrimaryWeapon;
		}

		return WEAPON_MARINE_KNIFE;
	}

	return UTIL_GetBotMarinePrimaryWeapon(pBot);
}

void MarineGuardLocation(bot_t* pBot, const Vector Location, const float GuardTime)
{
	UTIL_ClearGuardInfo(pBot);

	UTIL_GenerateGuardWatchPoints(pBot, Location);

	pBot->GuardStartedTime = gpGlobals->time;
	pBot->GuardLengthTime = GuardTime;

	pBot->CurrentGuardLocation = Location;
}

void MarineCheckWantsAndNeeds(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (FNullEnt(pEdict)) { return; }

	bool bUrgentlyNeedsHealth = (pEdict->v.health < 50.0f);

	// GL is a terrible choice to defend the base with...
	if (pBot->CurrentRole == BOT_ROLE_GUARD_BASE && BotHasWeapon(pBot, WEAPON_MARINE_GL))
	{
		BotDropWeapon(pBot);
	}

	if (bUrgentlyNeedsHealth)
	{
		const dropped_marine_item* HealthPackIndex = UTIL_GetNearestItemIndexOfType(ITEM_MARINE_HEALTHPACK, pEdict->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

		if (HealthPackIndex)
		{
			pBot->WantsAndNeedsTask.TaskType = TASK_GET_HEALTH;
			pBot->WantsAndNeedsTask.bOrderIsUrgent = true;
			pBot->WantsAndNeedsTask.TaskLocation = HealthPackIndex->Location;
			pBot->WantsAndNeedsTask.TaskTarget = HealthPackIndex->edict;

			return;
		}

		edict_t* NearestArmoury = UTIL_GetNearestStructureIndexOfType(pEdict->v.origin, STRUCTURE_MARINE_ANYARMOURY, UTIL_MetresToGoldSrcUnits(30.0f), true);

		if (!FNullEnt(NearestArmoury))
		{
			pBot->WantsAndNeedsTask.TaskType = TASK_RESUPPLY;
			pBot->WantsAndNeedsTask.bOrderIsUrgent = true;
			pBot->WantsAndNeedsTask.TaskLocation = NearestArmoury->v.origin;
			pBot->WantsAndNeedsTask.TaskTarget = NearestArmoury;

			return;
		}
	}

	if (UTIL_GetBotMarinePrimaryWeapon(pBot) == WEAPON_MARINE_MG)
	{
		if (pBot->WantsAndNeedsTask.TaskType != TASK_GET_WEAPON)
		{
			const dropped_marine_item* NewWeaponIndex = UTIL_GetNearestSpecialPrimaryWeapon(pEdict->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

			if (NewWeaponIndex)
			{
				// Don't grab the good stuff if there are humans who need it
				if (!UTIL_IsAnyHumanNearLocationWithoutSpecialWeapon(NewWeaponIndex->Location, UTIL_MetresToGoldSrcUnits(10.0f)))
				{
					if (pBot->CurrentRole != BOT_ROLE_GUARD_BASE || NewWeaponIndex->ItemType != ITEM_MARINE_GRENADELAUNCHER)
					{
						pBot->WantsAndNeedsTask.TaskType = TASK_GET_WEAPON;
						pBot->WantsAndNeedsTask.bOrderIsUrgent = false;
						pBot->WantsAndNeedsTask.TaskLocation = NewWeaponIndex->Location;
						pBot->WantsAndNeedsTask.TaskTarget = NewWeaponIndex->edict;

						return;
					}
				}
			}
		}
		else
		{
			return;
		}
	}

	bool bUrgentlyNeedsAmmo = (BotGetPrimaryWeaponAmmoReserve(pBot) == 0);

	if (bUrgentlyNeedsAmmo)
	{
		const dropped_marine_item* AmmoPackIndex = UTIL_GetNearestItemIndexOfType(ITEM_MARINE_AMMO, pEdict->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

		if (AmmoPackIndex)
		{
			pBot->WantsAndNeedsTask.TaskType = TASK_GET_AMMO;
			pBot->WantsAndNeedsTask.bOrderIsUrgent = true;
			pBot->WantsAndNeedsTask.TaskLocation = AmmoPackIndex->Location;
			pBot->WantsAndNeedsTask.TaskTarget = AmmoPackIndex->edict;

			return;
		}

		edict_t* NearestArmoury = UTIL_GetNearestStructureIndexOfType(pEdict->v.origin, STRUCTURE_MARINE_ANYARMOURY, UTIL_MetresToGoldSrcUnits(100.0f), true);

		if (!FNullEnt(NearestArmoury))
		{
			pBot->WantsAndNeedsTask.TaskType = TASK_RESUPPLY;
			pBot->WantsAndNeedsTask.bOrderIsUrgent = true;
			pBot->WantsAndNeedsTask.TaskLocation = NearestArmoury->v.origin;
			pBot->WantsAndNeedsTask.TaskTarget = NearestArmoury;

			return;
		}

	}

	if (!UTIL_PlayerHasWeapon(pEdict, WEAPON_MARINE_WELDER))
	{
		if (pBot->WantsAndNeedsTask.TaskType != TASK_GET_WEAPON)
		{
			const dropped_marine_item* WelderIndex = UTIL_GetNearestItemIndexOfType(ITEM_MARINE_WELDER, pEdict->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

			if (WelderIndex)
			{
				if (!UTIL_IsAnyHumanNearLocation(WelderIndex->Location, UTIL_MetresToGoldSrcUnits(5.0f)))
				{
					pBot->WantsAndNeedsTask.TaskType = TASK_GET_WEAPON;
					pBot->WantsAndNeedsTask.bOrderIsUrgent = false;
					pBot->WantsAndNeedsTask.TaskLocation = WelderIndex->Location;
					pBot->WantsAndNeedsTask.TaskTarget = WelderIndex->edict;

					return;
				}
			}
		}
		else
		{
			return;
		}
	}

	if (!UTIL_PlayerHasEquipment(pEdict))
	{
		if (pBot->WantsAndNeedsTask.TaskType != TASK_GET_EQUIPMENT)
		{
			const dropped_marine_item* EquipmentIndex = UTIL_GetNearestEquipment(pEdict->v.origin, UTIL_MetresToGoldSrcUnits(15.0f));

			if (EquipmentIndex)
			{
				// Don't grab the good stuff if there are humans who need it
				if (!UTIL_IsAnyHumanNearLocationWithoutEquipment(EquipmentIndex->Location, UTIL_MetresToGoldSrcUnits(10.0f)))
				{

					pBot->WantsAndNeedsTask.TaskType = TASK_GET_EQUIPMENT;
					pBot->WantsAndNeedsTask.bOrderIsUrgent = false;
					pBot->WantsAndNeedsTask.TaskLocation = EquipmentIndex->Location;
					pBot->WantsAndNeedsTask.TaskTarget = EquipmentIndex->edict;

					return;
				}
			}
		}
		else
		{
			return;
		}
	}
}

void MarineProgressCapResNodeTask(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return; }

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		MoveTo(pBot, Task->TaskLocation, MOVESTYLE_NORMAL);
		Task->TaskStartedTime = gpGlobals->time;
		return;
	}
	else
	{
		const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

		if (ResNodeIndex)
		{
			if (!ResNodeIndex->bIsOccupied)
			{
				BotProgressGuardTask(pBot, Task);
				return;
			}
			else
			{
				if (ResNodeIndex->bIsOwnedByMarines)
				{
					if (ResNodeIndex->TowerEdict && !UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict))
					{
						Task->TaskType = TASK_BUILD;
						Task->bOrderIsUrgent = true;
						Task->TaskTarget = ResNodeIndex->TowerEdict;
						Task->TaskLocation = ResNodeIndex->TowerEdict->v.origin;
						Task->TaskStartedTime = gpGlobals->time;
						return;
					}
				}
				else
				{
					if (ResNodeIndex->TowerEdict && UTIL_IsAlienStructure(ResNodeIndex->TowerEdict))
					{
						pBot->SecondaryBotTask.TaskType = TASK_ATTACK;
						pBot->SecondaryBotTask.TaskTarget = ResNodeIndex->TowerEdict;
						pBot->SecondaryBotTask.TaskLocation = ResNodeIndex->TowerEdict->v.origin;
						Task->TaskStartedTime = gpGlobals->time;
					}
				}
			}
		}
	}
}

void MarineProgressWeldTask(bot_t* pBot, bot_task* Task)
{
	float DistFromWeldLocation = vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin);


	if (UTIL_PlayerInUseRange(pBot->pEdict, Task->TaskTarget))
	{
		LookAt(pBot, Task->TaskTarget);
		pBot->DesiredCombatWeapon = WEAPON_MARINE_WELDER;

		if (UTIL_GetBotCurrentWeapon(pBot) != WEAPON_MARINE_WELDER)
		{
			return;
		}

		pBot->pEdict->v.button |= IN_ATTACK;

		return;
	}

	if (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
	{
		MoveTo(pBot, Task->TaskTarget->v.origin, MOVESTYLE_NORMAL);
		return;
	}

	if (!UTIL_PlayerHasLOSToEntity(pBot->pEdict, Task->TaskTarget))
	{
		LookAt(pBot, UTIL_GetCentreOfEntity(Task->TaskTarget));

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

bool UTIL_IsMarineTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }

	switch (Task->TaskType)
	{
	case TASK_NONE:
		return false;
	case TASK_MOVE:
		return UTIL_IsMoveTaskStillValid(pBot, Task);
	case TASK_GET_AMMO:
		return UTIL_IsAmmoPickupTaskStillValid(pBot, Task);
	case TASK_GET_HEALTH:
		return UTIL_IsHealthPickupTaskStillValid(pBot, Task);
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
		return UTIL_IsBuildTaskStillValid(pBot, Task);
	case TASK_CAP_RESNODE:
		return UTIL_IsMarineCapResNodeTaskStillValid(pBot, Task);
	case TASK_DEFEND:
		return UTIL_IsDefendTaskStillValid(pBot, Task);
	case TASK_WELD:
		return UTIL_IsWeldTaskStillValid(pBot, Task);
	case TASK_GRENADE:
		return BotHasGrenades(pBot) || (BotHasWeapon(pBot, WEAPON_MARINE_GL) && (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0));
	default:
		return true;
	}

	return false;
}

bool UTIL_IsMarineCapResNodeTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task || !Task->TaskLocation) { return false; }

	const resource_node* ResNodeIndex = UTIL_FindNearestResNodeToLocation(Task->TaskLocation);

	if (!ResNodeIndex) { return false; }

	// Always obey commander orders even if there's a bunch of other marines already there
	if (!Task->bIssuedByCommander)
	{
		int NumMarinesNearby = UTIL_GetNumPlayersOfTeamInArea(Task->TaskLocation, UTIL_MetresToGoldSrcUnits(4.0f), MARINE_TEAM, pBot->pEdict, CLASS_MARINE_COMMANDER);

		if (NumMarinesNearby >= 2 && vDist2DSq(pBot->pEdict->v.origin, Task->TaskLocation) > sqrf(UTIL_MetresToGoldSrcUnits(4.0f))) { return false; }
	}

	if (ResNodeIndex->bIsOccupied)
	{
		if (ResNodeIndex->bIsOwnedByMarines && ResNodeIndex->TowerEdict)
		{
			return !UTIL_StructureIsFullyBuilt(ResNodeIndex->TowerEdict);
		}
		else
		{
			return true;
		}
	}

	if (gpGlobals->time - Task->TaskStartedTime > 30.0f) { return false; }

	return true;
}

bool UTIL_IsWeldTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (!Task) { return false; }
	if (FNullEnt(Task->TaskTarget) || Task->TaskTarget->v.deadflag != DEAD_NO) { return false; }
	if (Task->TaskTarget == pBot->pEdict) { return false; }
	if (!BotHasWeapon(pBot, WEAPON_MARINE_WELDER)) { return false; }

	if (UTIL_IsEdictPlayer(Task->TaskTarget))
	{
		if (!IsPlayerMarine(Task->TaskTarget) || IsPlayerBeingDigested(Task->TaskTarget)) { return false; }
		return (Task->TaskTarget->v.armorvalue < GetPlayerMaxArmour(Task->TaskTarget));
	}
	else
	{
		return (Task->TaskTarget->v.health < Task->TaskTarget->v.max_health);
	}
}

bool UTIL_IsAmmoPickupTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerDead(pBot->pEdict) || IsPlayerBeingDigested(pBot->pEdict) || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, Task->TaskTarget->v.origin, 32.0f)) { return false; }

	return (vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f))) && (BotGetPrimaryWeaponAmmoReserve(pBot) < BotGetPrimaryWeaponMaxAmmoReserve(pBot));
}

bool UTIL_IsHealthPickupTaskStillValid(bot_t* pBot, bot_task* Task)
{
	if (IsPlayerDead(pBot->pEdict) || IsPlayerBeingDigested(pBot->pEdict) || FNullEnt(Task->TaskTarget) || (Task->TaskTarget->v.effects & EF_NODRAW)) { return false; }

	if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, pBot->pEdict->v.origin, Task->TaskTarget->v.origin, 32.0f)) { return false; }

	return ((vDist2DSq(pBot->pEdict->v.origin, Task->TaskTarget->v.origin) < sqrf(UTIL_MetresToGoldSrcUnits(20.0f))) && (pBot->pEdict->v.health < pBot->pEdict->v.max_health));
}

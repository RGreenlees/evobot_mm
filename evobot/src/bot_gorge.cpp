//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_gorge.cpp
// 
// Contains gorge-related functions. Needs refactoring into helper function file
//

#include "bot_gorge.h"
#include "bot_navigation.h"
#include "bot_tactical.h"
#include "bot_structs.h"
#include <meta_api.h>

extern resource_node ResourceNodes[64];
extern int NumTotalResNodes;

extern hive_definition Hives[10];
extern int NumTotalHives;

extern edict_t *clients[32];


void GorgeCombatThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->CurrentEnemy < 0 || pBot->CurrentEnemy > 31) { return; }

	edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;
	enemy_status* TrackedEnemyRef = &pBot->TrackedEnemies[pBot->CurrentEnemy];

	if (!TrackedEnemyRef || FNullEnt(CurrentEnemy) || IsPlayerDead(CurrentEnemy)) { return; }

	if (TrackedEnemyRef->bCurrentlyVisible)
	{
		LookAt(pBot, CurrentEnemy);

		if (UTIL_GetBotCurrentWeapon(pBot) != WEAPON_GORGE_SPIT)
		{
			BotSwitchToWeapon(pBot, WEAPON_GORGE_SPIT);
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
			LookAt(pBot, TrackedEnemyRef->LastSeenLocation);
		}
		else
		{
			if (pBot->pEdict->v.health < pBot->pEdict->v.max_health)
			{
				pBot->DesiredCombatWeapon = WEAPON_GORGE_HEALINGSPRAY;

				if (UTIL_GetBotCurrentWeapon(pBot) == WEAPON_GORGE_HEALINGSPRAY)
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
			AlienGuardLocation(pBot, pBot->pEdict->v.origin);
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
				if (vDist2DSq(pBot->pEdict->v.origin, NearestHive->FloorLocation) > UTIL_MetresToGoldSrcUnits(5.0f))
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


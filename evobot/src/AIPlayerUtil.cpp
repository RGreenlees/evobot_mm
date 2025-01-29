
#include "AIPlayerUtil.h"
#include "AIPlayer.h"
#include "AIHelper.h"
#include "AIPlayerManager.h"
#include "AITactical.h"
#include "AIWeaponHelper.h"
#include "NSConstants.h"

#include <extdll.h>

#include "AIMath.h"
#include "../pm_shared/pm_shared.h"
#include "../pm_shared/pm_defs.h"

#include <cfloat>


bool IsPlayerActiveInGame(const edict_t* Player)
{
	return !IsPlayerSpectator(Player) && !IsPlayerDead(Player) && !IsPlayerInReadyRoom(Player);
}

bool IsPlayerHuman(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (!(Player->v.flags & FL_FAKECLIENT));
}

bool IsPlayerBot(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.flags & FL_FAKECLIENT);
}

bool IsPlayerDead(const edict_t* Player)
{
	if (FNullEnt(Player)) { return true; }
	return (Player->v.deadflag != DEAD_NO || Player->v.health <= 0.0f);
}

bool IsPlayerSpectator(const edict_t* Player)
{
	return false;
}

bool IsPlayerOnLadder(const edict_t* Player)
{
	return (Player->v.movetype == MOVETYPE_FLY);
}

int GetPlayerMaxArmour(const edict_t* Player)
{
	return 100;
}

float GetPlayerRadius(const edict_t* Player)
{
	if (!Player) { return 0.0f; }

	int hullnum = GetPlayerHullIndex(Player);

	switch (hullnum)
	{
	case human_hull:
	case head_hull:
		return 16.0f;
		break;
	case large_hull:
		return 32.0f;
		break;
	default:
		return 16.0f;
		break;

	}
}

bool CanPlayerCrouch(const edict_t* Player)
{
	if (FNullEnt(Player) || Player->free || !IsEdictPlayer(Player)) { return false; }

	return true;
}

int GetPlayerHullIndex(const edict_t* Player, const bool bIsCrouching)
{
	if (FNullEnt(Player)) { return 0; }

	return (bIsCrouching) ? head_hull : human_hull;
}

int GetPlayerHullIndex(const edict_t* Player)
{
	if (!Player) { return 0; }

	bool bIsCrouching = (Player->v.flags & FL_DUCKING);

	return (bIsCrouching) ? head_hull : human_hull;
}


float GetPlayerOverallHealthPercent(const edict_t* Player)
{
	float MaxHealthAndArmour = Player->v.max_health + GetPlayerMaxArmour(Player);
	float CurrentHealthAndArmour = Player->v.health + Player->v.armorvalue;

	return (CurrentHealthAndArmour / MaxHealthAndArmour);
}

Vector GetPlayerEyePosition(const edict_t* Player)
{
	if (FNullEnt(Player)) { return ZERO_VECTOR; }

	return (Player->v.origin + Player->v.view_ofs);
}

float GetPlayerHeight(const edict_t* Player, const bool bIsCrouching)
{
	if (FNullEnt(Player)) { return 0.0f; }

	return GetPlayerOriginOffsetFromFloor(Player, bIsCrouching).z * 2.0f;
}

Vector GetPlayerOriginOffsetFromFloor(const edict_t* pEdict, const bool bIsCrouching)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	return (bIsCrouching) ? Vector(0.0f, 0.0f, 18.0f) : Vector(0.0f, 0.0f, 36.0f);
}

Vector GetPlayerBottomOfCollisionHull(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);
	Vector origin = pEdict->v.origin;

	return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 18.0f)) : (origin - Vector(0.0f, 0.0f, 36.0f));
}

Vector GetPlayerTopOfCollisionHull(const edict_t* pEdict, const bool bIsCrouching)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }
	
	Vector origin = pEdict->v.origin;

	return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
}

Vector GetPlayerTopOfCollisionHull(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	if (!IsEdictPlayer(pEdict))
	{
		Vector Centre = UTIL_GetCentreOfEntity(pEdict);
		Centre.z = pEdict->v.absmax.z;

		return Centre;
	}

	int iuser3 = pEdict->v.iuser3;
	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);
	Vector origin = pEdict->v.origin;

	return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
}

Vector GetPlayerAttemptedMoveDirection(const edict_t* Player)
{
	if (Player->v.button == 0) { return ZERO_VECTOR; }

	Vector ForwardDir = UTIL_GetForwardVector2D(Player->v.angles);
	Vector RightDir = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(ForwardDir, UP_VECTOR));

	if (Player->v.button & IN_FORWARD)
	{
		if (Player->v.button & IN_RIGHT)
		{
			return UTIL_GetVectorNormal2D(ForwardDir + RightDir);
		}

		if (Player->v.button & IN_LEFT)
		{
			return UTIL_GetVectorNormal2D(ForwardDir - RightDir);
		}

		return ForwardDir;
	}

	if (Player->v.button & IN_BACK)
	{
		Vector BackwardDir = -ForwardDir;
		Vector RightDir = UTIL_GetCrossProduct(BackwardDir, UP_VECTOR);

		if (Player->v.button & IN_RIGHT)
		{
			return UTIL_GetVectorNormal2D(BackwardDir - RightDir);
		}

		if (Player->v.button & IN_LEFT)
		{
			return UTIL_GetVectorNormal2D(BackwardDir + RightDir);
		}

		return BackwardDir;
	}

	if (Player->v.button & IN_RIGHT)
	{
		return RightDir;
	}

	if (Player->v.button & IN_LEFT)
	{
		return -RightDir;
	}

	return ZERO_VECTOR;
}


bool IsEdictPlayer(const edict_t* edict)
{
	if (FNullEnt(edict)) { return false; }

	return ((edict->v.flags & FL_CLIENT) || (edict->v.flags & FL_FAKECLIENT));
}

bool IsPlayerTouchingEntity(const edict_t* Player, const edict_t* TargetEntity)
{
	edict_t* TouchingEdict = nullptr;

	while ((TouchingEdict = UTIL_FindEntityInSphere(TouchingEdict, Player->v.origin, 5.0f)) != NULL)
	{
		if (TouchingEdict == TargetEntity) { return true; }
	}

	return false;
}

bool IsPlayerInUseRange(const edict_t* Player, const edict_t* Target)
{
	if (FNullEnt(Player) || FNullEnt(Target)) { return false; }

	edict_t* UseObject = nullptr;

	while ((UseObject = UTIL_FindEntityInSphere(UseObject, Player->v.origin, 64.0f)) != NULL)
	{
		if (UseObject == Target) { return true; }
	}

	return false;
}

bool UTIL_PlayerHasLOSToEntity(const edict_t* Player, const edict_t* Target, const float MaxRange, const bool bUseHullSweep)
{
	if (FNullEnt(Player) || FNullEnt(Target)) { return false; }
	Vector StartTrace = GetPlayerEyePosition(Player);
	Vector EndTrace = UTIL_GetCentreOfEntity(Target);

	float Dist = vDist3D(StartTrace, EndTrace);

	TraceResult hit;

	if (bUseHullSweep)
	{
		UTIL_TraceHull(StartTrace, EndTrace, dont_ignore_monsters, head_hull, Player->v.pContainingEntity, &hit);
	}
	else
	{
		UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, Player->v.pContainingEntity, &hit);
	}



	if (hit.fStartSolid || (hit.flFraction < 1.0f && ((Dist * hit.flFraction) <= MaxRange)))
	{
		return (hit.pHit == Target);
	}
	else
	{
		return false;
	}
}

bool UTIL_PlayerHasLOSToLocation(const edict_t* Player, const Vector Target, const float MaxRange)
{
	if (FNullEnt(Player)) { return false; }
	Vector StartTrace = GetPlayerEyePosition(Player);

	if (vDist3DSq(StartTrace, Target) > sqrf(MaxRange)) { return false; }

	TraceResult hit;

	UTIL_TraceLine(StartTrace, Target, ignore_monsters, ignore_glass, Player->v.pContainingEntity, &hit);

	return (hit.flFraction >= 1.0f);

}

bool PlayerHasWeapon(const edict_t* Player, const AIWeaponType DesiredCombatWeapon)
{
	if (FNullEnt(Player)) { return false; }

	return (Player->v.weapons & (1 << (int)DesiredCombatWeapon));
}


bool IsPlayerReloading(const edict_t* Player)
{
	AIWeaponType CurrentWeapon = WEAP_GetPlayerCurrentWeapon(Player);

	if (!WEAP_WeaponCanBeReloaded(CurrentWeapon)) { return false; }

	if (WEAP_GetPlayerCurrentWeaponClipAmmo(Player) == WEAP_GetPlayerCurrentWeaponMaxClipAmmo(Player) || WEAP_GetPlayerCurrentWeaponReserveAmmo(Player) == 0) { return false; }

	switch (CurrentWeapon)
	{
		// This is a tricky one, you could do it by checking the Player->v.weaponanim based on which weapon is being held
		default:
			return false;
	}

	return false;
}

bool IsPlayerStandingOnPlayer(const edict_t* Player)
{
	return (IsEdictPlayer(Player->v.groundentity));
}

edict_t* UTIL_GetNearestLadderAtPoint(const Vector SearchLocation)
{
	edict_t* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	edict_t* closestLadderRef = entity;
	float lowestDist = FLT_MAX;

	while (entity)
	{
		Vector LadderMin = entity->v.absmin;
		Vector LadderMax = entity->v.absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, SearchLocation);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	return (closestLadderRef) ? closestLadderRef : nullptr;
}

Vector UTIL_GetNearestLadderNormal(edict_t* pEdict)
{
	return UTIL_GetNearestLadderNormal(pEdict->v.origin);
}

Vector UTIL_GetNearestSurfaceNormal(Vector SearchLocation)
{

		Vector Trace1End, Trace2End, Trace3End, Trace4End, Trace5End, Trace6End, Trace7End, Trace8End;
		Trace1End = Trace2End = Trace3End = Trace4End = Trace5End = Trace6End = Trace7End = Trace8End = SearchLocation;

		Trace1End.x += 32.0f;
		Trace1End.y += 32.0f;

		Trace2End.x += 32.0f;
		Trace2End.y -= 32.0f;

		Trace3End.x -= 32.0f;
		Trace3End.y -= 32.0f;

		Trace4End.x -= 32.0f;
		Trace4End.y += 32.0f;

		Trace5End.x += 32.0f;

		Trace6End.x -= 32.0f;

		Trace7End.y -= 32.0f;

		Trace8End.y += 32.0f;

		Vector ClosestNormal = ZERO_VECTOR;
		float MinDist = 0.0f;

		TraceResult HitResult;
		UTIL_TraceHull(SearchLocation, Trace1End, ignore_monsters, head_hull, nullptr, &HitResult);

		if (HitResult.flFraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(HitResult.vecEndPos);

			if (vIsZero(ClosestNormal) || HitResult.flFraction < MinDist)
			{
				ClosestNormal = HitResult.vecPlaneNormal;
				MinDist = HitResult.flFraction;
			}
		}

		UTIL_TraceHull(SearchLocation, Trace2End, ignore_monsters, head_hull, nullptr, &HitResult);

		if (HitResult.flFraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(HitResult.vecEndPos);

			if (vIsZero(ClosestNormal) || HitResult.flFraction < MinDist)
			{
				ClosestNormal = HitResult.vecPlaneNormal;
				MinDist = HitResult.flFraction;
			}
		}

		UTIL_TraceHull(SearchLocation, Trace3End, ignore_monsters, head_hull, nullptr, &HitResult);

		if (HitResult.flFraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(HitResult.vecEndPos);

			if (vIsZero(ClosestNormal) || HitResult.flFraction < MinDist)
			{
				ClosestNormal = HitResult.vecPlaneNormal;
				MinDist = HitResult.flFraction;
			}
		}

		UTIL_TraceHull(SearchLocation, Trace4End, ignore_monsters, head_hull, nullptr, &HitResult);

		if (HitResult.flFraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(HitResult.vecEndPos);

			if (vIsZero(ClosestNormal) || HitResult.flFraction < MinDist)
			{
				ClosestNormal = HitResult.vecPlaneNormal;
				MinDist = HitResult.flFraction;
			}
		}

		UTIL_TraceHull(SearchLocation, Trace5End, ignore_monsters, head_hull, nullptr, &HitResult);

		if (HitResult.flFraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(HitResult.vecEndPos);

			if (vIsZero(ClosestNormal) || HitResult.flFraction < MinDist)
			{
				ClosestNormal = HitResult.vecPlaneNormal;
				MinDist = HitResult.flFraction;
			}
		}

		UTIL_TraceHull(SearchLocation, Trace6End, ignore_monsters, head_hull, nullptr, &HitResult);

		if (HitResult.flFraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(HitResult.vecEndPos);

			if (vIsZero(ClosestNormal) || HitResult.flFraction < MinDist)
			{
				ClosestNormal = HitResult.vecPlaneNormal;
				MinDist = HitResult.flFraction;
			}
		}

		UTIL_TraceHull(SearchLocation, Trace7End, ignore_monsters, head_hull, nullptr, &HitResult);

		if (HitResult.flFraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(HitResult.vecEndPos);

			if (vIsZero(ClosestNormal) || HitResult.flFraction < MinDist)
			{
				ClosestNormal = HitResult.vecPlaneNormal;
				MinDist = HitResult.flFraction;
			}
		}

		UTIL_TraceHull(SearchLocation, Trace8End, ignore_monsters, head_hull, nullptr, &HitResult);

		if (HitResult.flFraction < 1.0f)
		{
			int PointContents = UTIL_PointContents(HitResult.vecEndPos);

			if (vIsZero(ClosestNormal) || HitResult.flFraction < MinDist)
			{
				ClosestNormal = HitResult.vecPlaneNormal;
				MinDist = HitResult.flFraction;
			}
		}

		return ClosestNormal;
}

Vector UTIL_GetNearestLadderNormal(Vector SearchLocation)
{
	TraceResult result;
	edict_t* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	edict_t* closestLadderRef = entity;
	float lowestDist = 999999.0f;

	while (!FNullEnt(entity))
	{
		Vector LadderMin = entity->v.absmin;
		Vector LadderMax = entity->v.absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, SearchLocation);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	if (closestLadderRef && !FNullEnt(closestLadderRef))
	{
		Vector CentrePoint = UTIL_GetCentreOfEntity(closestLadderRef);
		CentrePoint.z = SearchLocation.z;

		UTIL_TraceHull(SearchLocation, CentrePoint, ignore_monsters, head_hull, nullptr, &result);

		if (result.flFraction < 1.0f)
		{
			return result.vecPlaneNormal;
		}
	}

	return Vector(0.0f, 0.0f, 0.0f);
}

Vector UTIL_GetNearestLadderBottomPoint(edict_t* pEdict)
{
	TraceResult result;
	edict_t* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	edict_t* closestLadderRef = entity;
	float lowestDist = 999999.0f;

	while (entity)
	{
		Vector LadderMin = entity->v.absmin;
		Vector LadderMax = entity->v.absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, pEdict->v.origin);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	if (closestLadderRef)
	{
		Vector Centre = (closestLadderRef->v.absmin + (closestLadderRef->v.size * 0.5f));
		Centre.z = closestLadderRef->v.absmin.z;
		return Centre;

	}

	return pEdict->v.origin;
}

Vector UTIL_GetNearestLadderTopPoint(const Vector SearchLocation)
{
	TraceResult result;
	edict_t* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	edict_t* closestLadderRef = entity;
	float lowestDist = 999999.0f;

	while (entity)
	{
		Vector LadderMin = entity->v.absmin;
		Vector LadderMax = entity->v.absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, SearchLocation);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	if (closestLadderRef)
	{
		Vector Centre = (closestLadderRef->v.absmin + ((closestLadderRef->v.absmax - closestLadderRef->v.absmin) * 0.5f));
		Centre.z = closestLadderRef->v.absmax.z;
		return Centre;

	}

	return SearchLocation;
}

Vector UTIL_GetNearestLadderTopPoint(edict_t* pEdict)
{
	return UTIL_GetNearestLadderTopPoint(pEdict->v.origin);
}

Vector UTIL_GetNearestLadderCentrePoint(edict_t* pEdict)
{
	return UTIL_GetNearestLadderCentrePoint(pEdict->v.origin);
}

Vector UTIL_GetNearestLadderCentrePoint(const Vector SearchLocation)
{
	TraceResult result;
	edict_t* entity = NULL;

	entity = UTIL_FindEntityByClassname(entity, "func_ladder");

	edict_t* closestLadderRef = entity;
	float lowestDist = 999999.0f;

	while (entity)
	{
		Vector LadderMin = entity->v.absmin;
		Vector LadderMax = entity->v.absmax;

		float dist = vDistanceFromLine3D(LadderMin, LadderMax, SearchLocation);

		if (dist < lowestDist)
		{
			closestLadderRef = entity;
			lowestDist = dist;
		}

		entity = UTIL_FindEntityByClassname(entity, "func_ladder");
	}

	if (closestLadderRef)
	{
		return (closestLadderRef->v.absmin + ((closestLadderRef->v.absmax - closestLadderRef->v.absmin) * 0.5f));

	}

	return SearchLocation;
}

float GetPlayerMaxJumpHeight(const edict_t* Player)
{
	return (CanPlayerCrouch(Player) ? max_player_crouchjump_height : max_player_normaljump_height);
}

bool IsPlayerInReadyRoom(const edict_t* Player)
{
	return Player->v.playerclass == PLAYMODE_READYROOM;
}

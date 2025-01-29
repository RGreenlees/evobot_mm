
#include "AIWeaponHelper.h"
#include "AIPlayerUtil.h"
#include "AIMath.h"
#include "AINavigation.h"
#include "AIHelper.h"
#include "AITactical.h"
#include "AIPlayerManager.h"

extern nav_mesh NavMeshes[NUM_NAV_MESHES]; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)

AIWeaponTypeDefinition WeaponList[32];
AIPlayerInventory PlayerInventories[32];

int WEAP_GetPlayerCurrentWeaponClipAmmo(const edict_t* Player)
{
	int PlayerIndex = ENTINDEX(Player) - 1;

	if (PlayerIndex < 0 || PlayerIndex >= 32) { return WEAPON_INVALID; }

	AIPlayerInventory PlayerInventory = PlayerInventories[PlayerIndex];

	if (PlayerInventory.CurrentWeapon == WEAPON_INVALID) { return 0; }

	return PlayerInventory.ClipAmount[PlayerInventory.CurrentWeapon];

}

int WEAP_GetPlayerCurrentWeaponMaxClipAmmo(const edict_t* Player)
{
	int PlayerIndex = ENTINDEX(Player) - 1;

	if (PlayerIndex < 0 || PlayerIndex >= 32) { return WEAPON_INVALID; }

	AIPlayerInventory PlayerInventory = PlayerInventories[PlayerIndex];

	if (PlayerInventory.CurrentWeapon == WEAPON_INVALID) { return 0; }

	return WeaponList[PlayerInventory.CurrentWeapon].MaxClipSize;
}

int WEAP_GetPlayerCurrentWeaponReserveAmmo(const edict_t* Player)
{
	int PlayerIndex = ENTINDEX(Player) - 1;

	if (PlayerIndex < 0 || PlayerIndex >= 32) { return WEAPON_INVALID; }

	AIPlayerInventory PlayerInventory = PlayerInventories[PlayerIndex];

	if (PlayerInventory.CurrentWeapon == WEAPON_INVALID) { return 0; }

	int AmmoIndex = WeaponList[PlayerInventory.CurrentWeapon].AmmoIndex;

	return PlayerInventory.AmmoCounts[AmmoIndex];
}

int WEAP_GetPlayerCurrentWeaponMaxReserveAmmo(const edict_t* Player)
{
	int PlayerIndex = ENTINDEX(Player) - 1;

	if (PlayerIndex < 0 || PlayerIndex >= 32) { return WEAPON_INVALID; }

	AIPlayerInventory PlayerInventory = PlayerInventories[PlayerIndex];

	if (PlayerInventory.CurrentWeapon == WEAPON_INVALID) { return 0; }

	return WeaponList[PlayerInventory.CurrentWeapon].MaxAmmoReserve;
}

float WEAP_GetProjectileVelocityForWeapon(const AIWeaponType Weapon)
{
	switch (Weapon)
	{
		default:
			return 800.0f; // Replace with something else
	}
}

bool WEAP_CanInterruptWeaponReload(AIWeaponType Weapon)
{
	switch (Weapon)
	{
		default:
			return false;
	}

	return false;
}

float WEAP_GetReloadTimeForWeapon(AIWeaponType Weapon)
{
	switch (Weapon)
	{
		default:
			return 2.0f;
	}

	return 2.0f;
}

void InterruptReload(AIPlayer* pBot)
{
	if (!IsPlayerReloading(pBot->Edict)) { return; }

	pBot->Button |= IN_ATTACK;
}

bool WEAP_IsHitScanWeapon(AIWeaponType Weapon)
{
	switch (Weapon)
	{
		default:
			return true;
	}

	return false;
}

bool WEAP_IsWeaponAffectedByGravity(AIWeaponType Weapon)
{
	switch (Weapon)
	{
		default:
			return false;
	}

	return false;
}

float WEAP_GetTimeUntilPlayerNextRefire(const edict_t* Player)
{
	int PlayerIndex = ENTINDEX(Player) - 1;

	if (PlayerIndex < 0 || PlayerIndex >= 32) { return WEAPON_INVALID; }

	AIPlayerInventory PlayerInventory = PlayerInventories[PlayerIndex];

	if (PlayerInventory.CurrentWeapon == WEAPON_INVALID) { return 0.0f; }

	return fmaxf(0.0f, PlayerInventory.RefireTimes[PlayerInventory.CurrentWeapon] - gpGlobals->time);
}

float WEAP_GetMaxIdealWeaponRange(const AIWeaponType Weapon, edict_t* Target)
{
	switch (Weapon)
	{
		default:
			return UTIL_MetresToGoldSrcUnits(5.0f);
	}
}

float WEAP_GetMinIdealWeaponRange(const AIWeaponType Weapon, edict_t* Target)
{
	switch (Weapon)
	{
		default:
			return 0.0f;
	}
}

bool WEAP_IsMeleeWeapon(const AIWeaponType Weapon)
{
	switch (Weapon)
	{
		default:
			return false;
	}
}

bool WEAP_WeaponCanBeReloaded(const AIWeaponType CheckWeapon)
{
	switch (CheckWeapon)
	{
		default:
			return true;
	}
}


Vector UTIL_GetGrenadeThrowTarget(edict_t* Player, const Vector TargetLocation, const float ExplosionRadius, bool bPrecise)
{
	if (UTIL_PlayerHasLOSToLocation(Player, TargetLocation, UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		return TargetLocation;
	}

	if (UTIL_PointIsDirectlyReachable(Player->v.origin, TargetLocation))
	{
		Vector Orientation = UTIL_GetVectorNormal(Player->v.origin - TargetLocation);

		Vector NewSpot = TargetLocation + (Orientation * UTIL_MetresToGoldSrcUnits(1.5f));

		NewSpot = UTIL_ProjectPointToNavmesh(NAV_MESH_DEFAULT, NewSpot);

		if (NewSpot != ZERO_VECTOR)
		{
			NewSpot.z += 10.0f;
		}

		return NewSpot;
	}

	std::vector<bot_path_node> CheckPath;
	CheckPath.clear();

	dtStatus Status = FindPathClosestToPoint(GetBaseAgentProfile(NAV_PROFILE_DEFAULT), Player->v.origin, TargetLocation, CheckPath, ExplosionRadius);

	if (dtStatusSucceed(Status))
	{
		Vector FurthestPointVisible = UTIL_GetFurthestVisiblePointOnPath(GetPlayerEyePosition(Player), CheckPath, bPrecise);

		if (vDist3DSq(FurthestPointVisible, TargetLocation) <= sqrf(ExplosionRadius))
		{
			return FurthestPointVisible;
		}

		Vector ThrowDir = UTIL_GetVectorNormal(FurthestPointVisible - Player->v.origin);

		Vector LineEnd = FurthestPointVisible + (ThrowDir * UTIL_MetresToGoldSrcUnits(5.0f));

		Vector ClosestPointInTrajectory = vClosestPointOnLine(FurthestPointVisible, LineEnd, TargetLocation);

		ClosestPointInTrajectory = UTIL_ProjectPointToNavmesh(NAV_MESH_DEFAULT, ClosestPointInTrajectory);
		ClosestPointInTrajectory.z += 10.0f;

		if (vDist2DSq(ClosestPointInTrajectory, TargetLocation) < sqrf(ExplosionRadius) && UTIL_PlayerHasLOSToLocation(Player, ClosestPointInTrajectory, UTIL_MetresToGoldSrcUnits(10.0f)) && UTIL_PointIsDirectlyReachable(ClosestPointInTrajectory, TargetLocation))
		{
			return ClosestPointInTrajectory;
		}
		else
		{
			return ZERO_VECTOR;
		}
	}
	else
	{
		return ZERO_VECTOR;
	}
}

void WEAP_BotReloadCurrentWeapon(AIPlayer* pBot)
{
	AIWeaponType CurrentWeapon = WEAP_GetPlayerCurrentWeapon(pBot->Edict);

	if (!WEAP_WeaponCanBeReloaded(CurrentWeapon)) { return; }

	if (!IsPlayerReloading(pBot->Edict))
	{
		if (gpGlobals->time - pBot->LastUseTime > 1.0f)
		{
			pBot->Button |= IN_RELOAD;
			pBot->LastUseTime = gpGlobals->time;
		}
	}
}

BotAttackResult PerformAttackLOSCheck(AIPlayer* pBot, const AIWeaponType Weapon, const edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return ATTACK_INVALIDTARGET; }

	if (Weapon == WEAPON_INVALID) { return ATTACK_NOWEAPON; }

	// Add a LITTLE bit of give to avoid edge cases where the bot is a smidge out of range
	float MaxWeaponRange = WEAP_GetMaxIdealWeaponRange(Weapon) - 5.0f;

	TraceResult hit;

	Vector StartTrace = pBot->CurrentEyePosition;

	Vector AttackDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - StartTrace);

	Vector EndTrace = pBot->CurrentEyePosition + (AttackDir * MaxWeaponRange);

	UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

	if (FNullEnt(hit.pHit)) { return ATTACK_OUTOFRANGE; }

	if (hit.pHit != Target)
	{
		if (vDist3DSq(pBot->CurrentEyePosition, Target->v.origin) > sqrf(MaxWeaponRange))
		{
			return ATTACK_OUTOFRANGE;
		}
		else
		{
			return ATTACK_BLOCKED;
		}
	}

	return ATTACK_SUCCESS;
}

BotAttackResult PerformAttackLOSCheck(AIPlayer* pBot, const AIWeaponType Weapon, const Vector TargetLocation, const edict_t* Target)
{
	if (!TargetLocation) { return ATTACK_INVALIDTARGET; }

	if (Weapon == WEAPON_INVALID) { return ATTACK_NOWEAPON; }

	// Add a LITTLE bit of give to avoid edge cases where the bot is a smidge out of range
	float MaxWeaponRange = WEAP_GetMaxIdealWeaponRange(Weapon) - 5.0f;

	TraceResult hit;

	Vector StartTrace = pBot->CurrentEyePosition;

	Vector AttackDir = UTIL_GetVectorNormal(TargetLocation - StartTrace);

	Vector EndTrace = pBot->CurrentEyePosition + (AttackDir * MaxWeaponRange);

	UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &hit);

	if (FNullEnt(hit.pHit)) { return ATTACK_OUTOFRANGE; }

	if (hit.pHit != Target)
	{
		if (vDist3DSq(pBot->CurrentEyePosition, TargetLocation) > sqrf(MaxWeaponRange))
		{
			return ATTACK_OUTOFRANGE;
		}
		else
		{
			return ATTACK_BLOCKED;
		}
	}

	return ATTACK_SUCCESS;
}

BotAttackResult PerformAttackLOSCheck(const Vector Location, const AIWeaponType Weapon, const edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return ATTACK_INVALIDTARGET; }

	if (Weapon == WEAPON_INVALID) { return ATTACK_NOWEAPON; }

	float MaxWeaponRange = WEAP_GetMaxIdealWeaponRange(Weapon);

	bool bIsMeleeWeapon = WEAP_IsMeleeWeapon(Weapon);

	TraceResult hit;

	Vector StartTrace = Location;

	Vector AttackDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - StartTrace);

	Vector EndTrace = Location + (AttackDir * MaxWeaponRange);

	UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, nullptr, &hit);

	if (FNullEnt(hit.pHit)) { return ATTACK_OUTOFRANGE; }

	if (hit.pHit != Target)
	{
		if (vDist3DSq(Location, Target->v.origin) > sqrf(MaxWeaponRange))
		{
			return ATTACK_OUTOFRANGE;
		}
		else
		{
			return ATTACK_BLOCKED;
		}
	}

	return ATTACK_SUCCESS;
}


float UTIL_GetProjectileVelocityForWeapon(const AIWeaponType Weapon)
{
	switch (Weapon)
	{
		default:
			return 0.0f; // Hitscan.
	}
}


void UTIL_RegisterNewWeaponType(int WeaponIndex, AIWeaponTypeDefinition* WeaponSettings)
{
	memcpy(&WeaponList[WeaponIndex], WeaponSettings, sizeof(AIWeaponTypeDefinition));
}

void UTIL_RegisterPlayerNewWeapon(int player_index, int WeaponIndex)
{

}

void UTIL_UpdatePlayerAmmo(int player_index, int AmmoIndex, int NewAmmo)
{
	edict_t* PlayerEdict = INDEXENT(player_index);

	int ZeroIndex = player_index - 1;

	if (ZeroIndex < 0 || ZeroIndex >= 32 || AmmoIndex >= MAX_AMMO_SLOTS) { return; }

	AIPlayerInventory* Inventory = &PlayerInventories[ZeroIndex];

	Inventory->AmmoCounts[AmmoIndex] = NewAmmo;

}

void UTIL_RegisterPlayerCurrentWeapon(int player_index, int WeaponIndex, int ClipAmmo)
{
	edict_t* PlayerEdict = INDEXENT(player_index);

	int ZeroIndex = player_index - 1;

	if (ZeroIndex < 0 || ZeroIndex >= 32) { return; }

	AIPlayerInventory* Inventory = &PlayerInventories[ZeroIndex];

	Inventory->ClipAmount[WeaponIndex] = ClipAmmo;

	AIWeaponType NewWeaponType = (AIWeaponType)WeaponIndex;
	
	Inventory->CurrentWeapon = (AIWeaponType)WeaponIndex;
}

AIWeaponType WEAP_GetPlayerCurrentWeapon(const edict_t* Player)
{
	int PlayerIndex = ENTINDEX(Player);

	int ZeroIndex = PlayerIndex - 1;

	if (ZeroIndex < 0 || ZeroIndex >= 32) { return WEAPON_INVALID; }

	AIPlayerInventory* Inventory = &PlayerInventories[ZeroIndex];

	return Inventory->CurrentWeapon;
}

void WEAP_PlayerFiredWeapon(const edict_t* Player)
{
	int PlayerIndex = ENTINDEX(Player) - 1;

	if (PlayerIndex < 0 || PlayerIndex >= 32) { return; }

	AIPlayerInventory* Inventory = &PlayerInventories[PlayerIndex];

	if (Inventory->CurrentWeapon == WEAPON_INVALID) { return; }

	Inventory->RefireTimes[Inventory->CurrentWeapon] = gpGlobals->time + WeaponList[Inventory->CurrentWeapon].MinRefireTime;
}

const char* UTIL_WeaponTypeToClassname(const AIWeaponType WeaponType)
{
	return WeaponList[WeaponType].szClassname;
}
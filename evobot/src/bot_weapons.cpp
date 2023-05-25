
#include "bot_weapons.h"

#include <extdll.h>
#include <dllapi.h>

#include "bot_navigation.h"
#include "general_util.h"
#include "bot_tactical.h"

bot_weapon_t weapon_defs[MAX_WEAPONS]; // array of weapon definitions

int BotGetCurrentWeaponClipAmmo(const bot_t* pBot)
{
	return pBot->current_weapon.iClip;
}

int BotGetCurrentWeaponMaxClipAmmo(const bot_t* pBot)
{
	return pBot->current_weapon.iClipMax;
}

int BotGetCurrentWeaponReserveAmmo(const bot_t* pBot)
{
	return pBot->current_weapon.iAmmo1;
}

NSWeapon GetBotCurrentWeapon(const bot_t* pBot)
{
	return (NSWeapon)pBot->current_weapon.iId;
}

float UTIL_GetProjectileVelocityForWeapon(const NSWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_GORGE_SPIT:
		return (float)kSpitVelocity;
	case WEAPON_LERK_SPORES:
		return (float)kShootCloudVelocity;
	case WEAPON_FADE_ACIDROCKET:
		return (float)kAcidRocketVelocity;
	default:
		return 0.0f; // Hitscan. We don't bother with bile bomb as it's so short range that it doesn't really need leading the target
	}
}

float GetEnergyCostForWeapon(const NSWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_SKULK_BITE:
		return kBiteEnergyCost;
	case WEAPON_SKULK_PARASITE:
		return kParasiteEnergyCost;
	case WEAPON_SKULK_LEAP:
		return kLeapEnergyCost;
	case WEAPON_SKULK_XENOCIDE:
		return kDivineWindEnergyCost;

	case WEAPON_GORGE_SPIT:
		return kSpitEnergyCost;
	case WEAPON_GORGE_HEALINGSPRAY:
		return kHealingSprayEnergyCost;
	case WEAPON_GORGE_BILEBOMB:
		return kBileBombEnergyCost;
	case WEAPON_GORGE_WEB:
		return kWebEnergyCost;

	case WEAPON_LERK_BITE:
		return kBite2EnergyCost;
	case WEAPON_LERK_SPORES:
		return kSporesEnergyCost;
	case WEAPON_LERK_UMBRA:
		return kUmbraEnergyCost;
	case WEAPON_LERK_PRIMALSCREAM:
		return kPrimalScreamEnergyCost;

	case WEAPON_FADE_SWIPE:
		return kSwipeEnergyCost;
	case WEAPON_FADE_BLINK:
		return kBlinkEnergyCost;
	case WEAPON_FADE_METABOLIZE:
		return kMetabolizeEnergyCost;
	case WEAPON_FADE_ACIDROCKET:
		return kAcidRocketEnergyCost;

	case WEAPON_ONOS_GORE:
		return kClawsEnergyCost;
	case WEAPON_ONOS_DEVOUR:
		return kDevourEnergyCost;
	case WEAPON_ONOS_STOMP:
		return kStompEnergyCost;
	case WEAPON_ONOS_CHARGE:
		return kChargeEnergyCost;

	default:
		return 0.0f;
	}
}

NSWeapon UTIL_GetBotAlienPrimaryWeapon(const bot_t* pBot)
{
	switch (pBot->bot_ns_class)
	{
	case CLASS_SKULK:
		return WEAPON_SKULK_BITE;
	case CLASS_GORGE:
		return WEAPON_GORGE_SPIT;
	case CLASS_LERK:
		return WEAPON_LERK_BITE;
	case CLASS_FADE:
		return WEAPON_FADE_SWIPE;
	case CLASS_ONOS:
		return WEAPON_ONOS_GORE;
	default:
		return WEAPON_NONE;
	}

	return WEAPON_NONE;
}

NSWeapon GetBotMarinePrimaryWeapon(const bot_t* pBot)
{
	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_MG))
	{
		return WEAPON_MARINE_MG;
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_HMG))
	{
		return WEAPON_MARINE_HMG;
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_GL))
	{
		return WEAPON_MARINE_GL;
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_SHOTGUN))
	{
		return WEAPON_MARINE_SHOTGUN;
	}

	return WEAPON_NONE;
}

NSWeapon GetBotMarineSecondaryWeapon(const bot_t* pBot)
{
	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_PISTOL))
	{
		return WEAPON_MARINE_PISTOL;
	}

	return WEAPON_NONE;
}

int BotGetPrimaryWeaponMaxAmmoReserve(bot_t* pBot)
{
	NSWeapon PrimaryWeapon = GetBotMarinePrimaryWeapon(pBot);

	if (PrimaryWeapon == WEAPON_NONE) { return 0; }

	return weapon_defs[PrimaryWeapon].iAmmo1Max;
}

int BotGetPrimaryWeaponAmmoReserve(bot_t* pBot)
{
	NSWeapon PrimaryWeapon = GetBotMarinePrimaryWeapon(pBot);

	if (PrimaryWeapon == WEAPON_NONE) { return 0; }

	int PrimaryAmmoIndex = weapon_defs[PrimaryWeapon].iAmmo1;

	return pBot->m_rgAmmo[PrimaryAmmoIndex];
}

int BotGetSecondaryWeaponAmmoReserve(bot_t* pBot)
{
	NSWeapon SecondaryWeapon = GetBotMarineSecondaryWeapon(pBot);

	if (SecondaryWeapon == WEAPON_NONE) { return 0; }

	int SecondaryAmmoIndex = weapon_defs[SecondaryWeapon].iAmmo1;

	return pBot->m_rgAmmo[SecondaryAmmoIndex];
}

int BotGetPrimaryWeaponClipAmmo(const bot_t* pBot)
{
	NSWeapon PrimaryWeapon = GetBotMarinePrimaryWeapon(pBot);

	if (PrimaryWeapon == WEAPON_NONE) { return 0; }

	return pBot->m_clipAmmo[PrimaryWeapon];
}

int BotGetSecondaryWeaponClipAmmo(const bot_t* pBot)
{
	NSWeapon SecondaryWeapon = GetBotMarineSecondaryWeapon(pBot);

	if (SecondaryWeapon == WEAPON_NONE) { return 0; }

	return pBot->m_clipAmmo[SecondaryWeapon];
}

int BotGetPrimaryWeaponMaxClipSize(const bot_t* pBot)
{
	NSWeapon PrimaryWeapon = GetBotMarinePrimaryWeapon(pBot);

	if (PrimaryWeapon == WEAPON_NONE) { return 0; }

	return weapon_defs[PrimaryWeapon].iClipSize;
}

int BotGetSecondaryWeaponMaxClipSize(const bot_t* pBot)
{
	NSWeapon SecondaryWeapon = GetBotMarineSecondaryWeapon(pBot);

	if (SecondaryWeapon == WEAPON_NONE) { return 0; }

	return weapon_defs[SecondaryWeapon].iClipSize;
}

int BotGetSecondaryWeaponMaxAmmoReserve(bot_t* pBot)
{
	NSWeapon SecondaryWeapon = GetBotMarineSecondaryWeapon(pBot);

	if (SecondaryWeapon == WEAPON_NONE) { return 0; }

	return weapon_defs[SecondaryWeapon].iAmmo1Max;
}

float GetMaxIdealWeaponRange(const NSWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_MARINE_GL:
	case WEAPON_MARINE_MG:
	case WEAPON_MARINE_PISTOL:
	case WEAPON_FADE_ACIDROCKET:
	case WEAPON_SKULK_PARASITE:
	case WEAPON_SKULK_LEAP:
	case WEAPON_LERK_SPORES:
	case WEAPON_LERK_UMBRA:
	case WEAPON_ONOS_CHARGE:
		return UTIL_MetresToGoldSrcUnits(20.0f);
	case WEAPON_MARINE_HMG:
	case WEAPON_MARINE_GRENADE:
		return UTIL_MetresToGoldSrcUnits(10.0f);
	case WEAPON_MARINE_SHOTGUN:
	case WEAPON_GORGE_BILEBOMB:
	case WEAPON_ONOS_STOMP:
		return UTIL_MetresToGoldSrcUnits(8.0f);
	case WEAPON_SKULK_XENOCIDE:
		return UTIL_MetresToGoldSrcUnits(5.0f);
	case WEAPON_ONOS_GORE:
		return kClawsRange;
	case WEAPON_ONOS_DEVOUR:
		return kDevourRange;
	case WEAPON_FADE_SWIPE:
		return kSwipeRange;
	case WEAPON_SKULK_BITE:
		return kBiteRange;
	case WEAPON_LERK_BITE:
		return kBite2Range;
	case WEAPON_GORGE_HEALINGSPRAY:
		return kHealingSprayRange;
	default:
		return max_player_use_reach;
	}
}

float GetMinIdealWeaponRange(const NSWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_MARINE_GL:
	case WEAPON_MARINE_GRENADE:
	case WEAPON_FADE_ACIDROCKET:
		return UTIL_MetresToGoldSrcUnits(5.0f);
	case WEAPON_SKULK_LEAP:
		return UTIL_MetresToGoldSrcUnits(3.0f);
	case WEAPON_MARINE_MG:
	case WEAPON_MARINE_PISTOL:
	case WEAPON_MARINE_HMG:
	case WEAPON_SKULK_PARASITE:
		return UTIL_MetresToGoldSrcUnits(5.0f);
	case WEAPON_MARINE_SHOTGUN:
		return UTIL_MetresToGoldSrcUnits(2.0f);
	case WEAPON_GORGE_BILEBOMB:
	case WEAPON_ONOS_STOMP:
		return UTIL_MetresToGoldSrcUnits(2.0f);
	default:
		return max_player_use_reach * 0.5f;
	}
}

bool IsMeleeWeapon(const NSWeapon Weapon)
{
	switch (Weapon)
	{
	case WEAPON_MARINE_KNIFE:
	case WEAPON_SKULK_BITE:
	case WEAPON_FADE_SWIPE:
	case WEAPON_ONOS_GORE:
	case WEAPON_ONOS_DEVOUR:
	case WEAPON_LERK_BITE:
		return true;
	default:
		return false;
	}
}

bool WeaponCanBeReloaded(const NSWeapon CheckWeapon)
{
	switch (CheckWeapon)
	{
	case WEAPON_MARINE_GL:
	case WEAPON_MARINE_HMG:
	case WEAPON_MARINE_MG:
	case WEAPON_MARINE_PISTOL:
	case WEAPON_MARINE_SHOTGUN:
		return true;
	default:
		return false;

	}
}

NSWeapon GetBotAlienPrimaryWeapon(const bot_t* pBot)
{
	switch (pBot->bot_ns_class)
	{
	case CLASS_SKULK:
		return WEAPON_SKULK_BITE;
	case CLASS_GORGE:
		return WEAPON_GORGE_SPIT;
	case CLASS_LERK:
		return WEAPON_LERK_BITE;
	case CLASS_FADE:
		return WEAPON_FADE_SWIPE;
	case CLASS_ONOS:
		return WEAPON_ONOS_GORE;
	default:
		return WEAPON_NONE;
	}

	return WEAPON_NONE;
}

Vector UTIL_GetGrenadeThrowTarget(bot_t* pBot, const Vector TargetLocation, const float ExplosionRadius)
{
	if (UTIL_PlayerHasLOSToLocation(pBot->pEdict, TargetLocation, UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		return TargetLocation;
	}

	if (UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, TargetLocation))
	{
		Vector Orientation = UTIL_GetVectorNormal(pBot->pEdict->v.origin - TargetLocation);

		Vector NewSpot = TargetLocation + (Orientation * UTIL_MetresToGoldSrcUnits(1.5f));

		NewSpot = UTIL_ProjectPointToNavmesh(NewSpot);

		if (NewSpot != ZERO_VECTOR)
		{
			NewSpot.z += 10.0f;
		}

		return NewSpot;
	}

	bot_path_node CheckPath[MAX_PATH_SIZE];
	int PathSize = 0;

	dtStatus Status = FindPathClosestToPoint(ALL_NAV_PROFILE, pBot->pEdict->v.origin, TargetLocation, CheckPath, &PathSize, ExplosionRadius);

	if (dtStatusSucceed(Status))
	{
		Vector FurthestPointVisible = UTIL_GetFurthestVisiblePointOnPath(pBot->CurrentEyePosition, CheckPath, PathSize);

		if (vDist3DSq(FurthestPointVisible, TargetLocation) <= sqrf(ExplosionRadius))
		{
			return FurthestPointVisible;
		}

		Vector ThrowDir = UTIL_GetVectorNormal(FurthestPointVisible - pBot->pEdict->v.origin);

		Vector LineEnd = FurthestPointVisible + (ThrowDir * UTIL_MetresToGoldSrcUnits(5.0f));

		Vector ClosestPointInTrajectory = vClosestPointOnLine(FurthestPointVisible, LineEnd, TargetLocation);

		ClosestPointInTrajectory = UTIL_ProjectPointToNavmesh(ClosestPointInTrajectory);
		ClosestPointInTrajectory.z += 10.0f;

		if (vDist2DSq(ClosestPointInTrajectory, TargetLocation) < sqrf(ExplosionRadius) && UTIL_PlayerHasLOSToLocation(pBot->pEdict, ClosestPointInTrajectory, UTIL_MetresToGoldSrcUnits(10.0f)) && UTIL_PointIsDirectlyReachable(ClosestPointInTrajectory, TargetLocation))
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

NSWeapon BotMarineChooseBestWeapon(bot_t* pBot, edict_t* target)
{

	if (!target)
	{
		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			return GetBotMarinePrimaryWeapon(pBot);
		}
		else if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			return GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			return GetBotMarinePrimaryWeapon(pBot);
		}
	}

	if (IsEdictPlayer(target))
	{
		float DistFromEnemy = vDist2DSq(pBot->pEdict->v.origin, target->v.origin);

		if (DistFromEnemy <= sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
			if (BotGetPrimaryWeaponClipAmmo(pBot) == 0)
			{
				if (BotGetSecondaryWeaponClipAmmo(pBot) > 0)
				{
					return GetBotMarineSecondaryWeapon(pBot);
				}
				else
				{
					return WEAPON_MARINE_KNIFE;
				}
			}
			else
			{
				return GetBotMarinePrimaryWeapon(pBot);
			}
		}
		else
		{
			NSWeapon PrimaryWeapon = GetBotMarinePrimaryWeapon(pBot);

			if (PrimaryWeapon == WEAPON_MARINE_SHOTGUN)
			{
				if (DistFromEnemy > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
				{
					if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
					{
						return GetBotMarineSecondaryWeapon(pBot);
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
							return GetBotMarineSecondaryWeapon(pBot);
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
						return GetBotMarineSecondaryWeapon(pBot);
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
							return GetBotMarineSecondaryWeapon(pBot);
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

NSWeapon BotAlienChooseBestWeaponForStructure(bot_t* pBot, edict_t* target)
{
	NSStructureType StructureType = GetStructureTypeFromEdict(target);

	if (StructureType == STRUCTURE_NONE)
	{
		return UTIL_GetBotAlienPrimaryWeapon(pBot);
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_GORGE_BILEBOMB))
	{
		return WEAPON_GORGE_BILEBOMB;
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_SKULK_XENOCIDE))
	{
		int NumTargetsInArea = UTIL_GetNumPlayersOfTeamInArea(target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), MARINE_TEAM, NULL, CLASS_NONE, false);

		NumTargetsInArea += UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_ANYTURRET, target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		NumTargetsInArea += UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_PHASEGATE, target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumTargetsInArea > 2)
		{
			return WEAPON_SKULK_XENOCIDE;
		}
	}

	return UTIL_GetBotAlienPrimaryWeapon(pBot);
}

NSWeapon BotMarineChooseBestWeaponForStructure(bot_t* pBot, edict_t* target)
{
	NSStructureType StructureType = GetStructureTypeFromEdict(target);

	if (StructureType == STRUCTURE_NONE)
	{
		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			return GetBotMarinePrimaryWeapon(pBot);
		}
		else if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			return GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			return GetBotMarinePrimaryWeapon(pBot);
		}
	}

	if (StructureType == STRUCTURE_ALIEN_HIVE || StructureType == STRUCTURE_ALIEN_OFFENCECHAMBER)
	{
		if (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			return GetBotMarinePrimaryWeapon(pBot);
		}
		else if (BotGetSecondaryWeaponClipAmmo(pBot) > 0 || BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			return GetBotMarineSecondaryWeapon(pBot);
		}
		else
		{
			return WEAPON_MARINE_KNIFE;
		}
	}
	else
	{
		NSWeapon PrimaryWeapon = GetBotMarinePrimaryWeapon(pBot);

		if ((PrimaryWeapon == WEAPON_MARINE_GL || PrimaryWeapon == WEAPON_MARINE_SHOTGUN) && (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0))
		{
			return PrimaryWeapon;
		}

		return WEAPON_MARINE_KNIFE;
	}

	return GetBotMarinePrimaryWeapon(pBot);
}

NSWeapon SkulkGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || IsPlayerDead(Target))
	{
		return WEAPON_SKULK_BITE;
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_SKULK_XENOCIDE))
	{
		int NumTargetsInArea = UTIL_GetNumPlayersOfTeamInArea(Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), MARINE_TEAM, NULL, CLASS_NONE, false);

		NumTargetsInArea += UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_ANYTURRET, Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		NumTargetsInArea += UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_MARINE_PHASEGATE, Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumTargetsInArea > 2)
		{
			return WEAPON_SKULK_XENOCIDE;
		}
	}

	if (!IsPlayerParasited(Target))
	{
		float DistFromTarget = vDist2DSq(pBot->pEdict->v.origin, Target->v.origin);

		if (DistFromTarget >= sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			Vector EnemyFacing = UTIL_GetForwardVector2D(Target->v.angles);
			Vector BotFacing = UTIL_GetVectorNormal2D(Target->v.origin - pBot->pEdict->v.origin);

			float Dot = UTIL_GetDotProduct2D(EnemyFacing, BotFacing);

			// Only use parasite if the enemy is facing towards us. Means we don't ruin the element of surprise if sneaking up on an enemy
			if (Dot < 0.0f)
			{
				return WEAPON_SKULK_PARASITE;
			}
		}
	}

	return WEAPON_SKULK_BITE;

}

NSWeapon OnosGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || IsPlayerDead(Target))
	{
		return WEAPON_ONOS_GORE;
	}

	float DistFromTarget = vDist2DSq(pBot->pEdict->v.origin, Target->v.origin);

	if (DistFromTarget > sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
	{
		if (PlayerHasWeapon(pBot->pEdict, WEAPON_ONOS_CHARGE) && UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, Target->v.origin))
		{
			return WEAPON_ONOS_CHARGE;
		}

		return WEAPON_ONOS_GORE;
	}

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_ONOS_STOMP) && !IsPlayerStunned(Target) && DistFromTarget > sqrf(UTIL_MetresToGoldSrcUnits(2.0f)) && DistFromTarget < sqrf(UTIL_MetresToGoldSrcUnits(8.0f)))
	{
		return WEAPON_ONOS_STOMP;
	}

	if (!IsPlayerDigesting(pBot->pEdict) && DistFromTarget < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
	{
		return WEAPON_ONOS_DEVOUR;
	}

	return WEAPON_ONOS_GORE;
}

NSWeapon FadeGetBestWeaponForCombatTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || IsPlayerDead(Target))
	{
		return WEAPON_FADE_SWIPE;
	}

	if (!PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_ACIDROCKET))
	{
		return WEAPON_FADE_SWIPE;
	}

	float DistFromTarget = vDist2DSq(pBot->pEdict->v.origin, Target->v.origin);

	int NumEnemyAllies = UTIL_GetNumPlayersOfTeamInArea(Target->v.origin, UTIL_MetresToGoldSrcUnits(5.0f), MARINE_TEAM, NULL, CLASS_NONE, false);

	if (NumEnemyAllies > 2)
	{
		return WEAPON_FADE_ACIDROCKET;
	}

	if (PlayerHasWeapon(Target, WEAPON_MARINE_SHOTGUN))
	{
		if (DistFromTarget > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
		{
			return WEAPON_FADE_ACIDROCKET;
		}
	}

	return WEAPON_FADE_SWIPE;

}
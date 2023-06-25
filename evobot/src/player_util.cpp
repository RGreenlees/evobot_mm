
#include "player_util.h"

#include <extdll.h>
#include <dllapi.h>

#include "bot_structs.h"
#include "general_util.h"
#include "game_state.h"
#include "bot_tactical.h"
#include "bot_util.h"

extern edict_t* clients[MAX_CLIENTS];

bool IsPlayerSkulk(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER1);
}

bool IsPlayerGorge(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER2);
}

bool IsPlayerLerk(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER3);
}

bool IsPlayerFade(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER4);
}

bool IsPlayerOnos(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_ALIEN_PLAYER5);
}

bool IsPlayerMarine(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_MARINE_PLAYER);
}

bool IsPlayerAlien(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 != AVH_USER3_MARINE_PLAYER && Player->v.iuser3 != AVH_USER3_COMMANDER_PLAYER);
}

bool IsPlayerCommander(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser3 == AVH_USER3_COMMANDER_PLAYER);
}

bool IsPlayerClimbingWall(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (IsPlayerSkulk(Player) && (Player->v.iuser4 & MASK_WALLSTICKING));
}

bool IsPlayerInReadyRoom(const edict_t* Player)
{
	return Player->v.playerclass == PLAYMODE_READYROOM;
}

bool IsPlayerActiveInGame(const edict_t* Player)
{
	return !IsPlayerInReadyRoom(Player) && Player->v.team != 0 && !IsPlayerSpectator(Player) && !IsPlayerDead(Player) && !IsPlayerBeingDigested(Player) && !IsPlayerCommander(Player);
}

bool IsPlayerHuman(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (!(Player->v.flags & FL_FAKECLIENT) && !(Player->v.flags & FL_THIRDPARTYBOT));
}

bool IsPlayerBot(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return Player && ((Player->v.flags & FL_FAKECLIENT) || (Player->v.flags & FL_THIRDPARTYBOT));
}

bool IsPlayerDead(const edict_t* Player)
{
	if (FNullEnt(Player)) { return true; }
	return (Player->v.deadflag != DEAD_NO);
}

bool IsPlayerStunned(const edict_t* Player)
{
	return !FNullEnt(Player) && !IsPlayerDead(Player) && !IsPlayerDigesting(Player) && (Player->v.iuser4 & MASK_PLAYER_STUNNED);
}

bool IsPlayerSpectator(const edict_t* Player)
{
	return !FNullEnt(Player) && (Player->v.playerclass == PLAYMODE_OBSERVER);
}

bool IsPlayerBeingDigested(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_DIGESTING && Player->v.effects & EF_NODRAW);
}

bool IsPlayerDigesting(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_DIGESTING && !(Player->v.effects & EF_NODRAW));
}

bool IsPlayerGestating(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_ALIEN_EMBRYO);
}

bool IsPlayerCharging(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_ALIEN_MOVEMENT);
}

bool IsPlayerBuffed(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.iuser4 & MASK_BUFFED);
}

bool IsPlayerOnLadder(const edict_t* Player)
{
	return (Player->v.movetype == MOVETYPE_FLY);
}

bool IsPlayerOnMarineTeam(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player && (Player->v.team == MARINE_TEAM) && !IsPlayerInReadyRoom(Player) && !IsPlayerSpectator(Player));
}

bool IsPlayerOnAlienTeam(const edict_t* Player)
{
	if (FNullEnt(Player)) { return false; }
	return (Player->v.team == ALIEN_TEAM && !IsPlayerInReadyRoom(Player) && !IsPlayerSpectator(Player));
}

bool IsPlayerParasited(const edict_t* Player)
{
	if (FNullEnt(Player) || !IsPlayerOnMarineTeam(Player)) { return false; }
	return (Player->v.iuser4 & MASK_PARASITED);
}

bool IsPlayerMotionTracked(const edict_t* Player)
{
	if (FNullEnt(Player) || !IsPlayerOnAlienTeam(Player)) { return false; }
	return (Player->v.iuser4 & MASK_VIS_DETECTED);
}

float GetPlayerEnergy(const edict_t* Player)
{
	return (Player->v.fuser3 * 0.001f);
}

int GetPlayerMaxArmour(const edict_t* Player)
{
	if (IsEdictStructure(Player)) { return 0; }

	if (IsPlayerMarine(Player))
	{
		int BaseArmourLevel = (PlayerHasHeavyArmour(Player)) ? kMarineBaseHeavyArmor : kMarineBaseArmor;
		int BaseArmourUpgrade = (PlayerHasHeavyArmour(Player)) ? kMarineHeavyArmorUpgrade : kMarineBaseArmorUpgrade;
		int ArmourLevel = 0;

		if (Player->v.iuser4 & MASK_UPGRADE_6)
		{
			ArmourLevel = 3;
		}
		else if (Player->v.iuser4 & MASK_UPGRADE_5)
		{
			ArmourLevel = 2;
		}
		else if (Player->v.iuser4 & MASK_UPGRADE_4)
		{
			ArmourLevel = 1;
		}

		// We floor it, as we'd rather the max be incorrectly calculated as 1 less than 1 more in case fellow marines keep trying to weld them while at full
		return BaseArmourLevel + (int)floorf((ArmourLevel * 0.33333f) * BaseArmourUpgrade);

	}
	else
	{
		NSPlayerClass PlayerClass = GetPlayerClass(Player);

		int ArmourLevel = 0;

		if (Player->v.iuser4 & MASK_UPGRADE_1)
		{
			ArmourLevel = 1;

			if (Player->v.iuser4 & MASK_UPGRADE_11)
			{
				ArmourLevel = 3;
			}
			else if (Player->v.iuser4 & MASK_UPGRADE_10)
			{
				ArmourLevel = 2;
			}
		}

		// We floor the value, as we'd rather the max be incorrectly calculated as 1 less than 1 more in case alien keeps trying to heal when at full armour
		switch (PlayerClass)
		{
		case CLASS_EGG:
			return kGestateBaseArmor;
		case CLASS_SKULK:
			return kSkulkBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kSkulkArmorUpgrade);
		case CLASS_GORGE:
			return kGorgeBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kGorgeArmorUpgrade);
		case CLASS_LERK:
			return kLerkBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kLerkArmorUpgrade);
		case CLASS_FADE:
			return kFadeBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kFadeArmorUpgrade);
		case CLASS_ONOS:
			return kOnosBaseArmor + (int)floorf((ArmourLevel * 0.33333f) * kOnosArmorUpgrade);
		default:
			return 0;
		}
	}

	return 0;
}

NSPlayerClass GetPlayerClass(const edict_t* Player)
{
	if (FNullEnt(Player)) { return CLASS_NONE; }

	if (IsPlayerGestating(Player)) { return CLASS_EGG; }

	int iuser3 = Player->v.iuser3;

	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return CLASS_MARINE;
	case AVH_USER3_COMMANDER_PLAYER:
		return CLASS_MARINE_COMMANDER;
	case AVH_USER3_ALIEN_EMBRYO:
		return CLASS_EGG;
	case AVH_USER3_ALIEN_PLAYER1:
		return CLASS_SKULK;
	case AVH_USER3_ALIEN_PLAYER2:
		return CLASS_GORGE;
	case AVH_USER3_ALIEN_PLAYER3:
		return CLASS_LERK;
	case AVH_USER3_ALIEN_PLAYER4:
		return CLASS_FADE;
	case AVH_USER3_ALIEN_PLAYER5:
		return CLASS_ONOS;
	default:
		return CLASS_NONE;
	}

	return CLASS_NONE;
}

int GetPlayerResources(const edict_t* Player)
{
	if (FNullEnt(Player)) { return 0; }

	return (int)ceil(Player->v.vuser4.z / kNumericNetworkConstant);
}

int GetPlayerCombatExperience(const edict_t* Player)
{
	if (FNullEnt(Player)) { return 0; }

	return (int)ceil(Player->v.vuser4.z / kNumericNetworkConstant);
}

int GetPlayerCombatLevel(const edict_t* Player)
{
	// This is taken from the NS source, so should be fully accurate
	int thePlayerLevel = 1;

	int theCombatBaseExperience = 100;
	float theCombatLevelExperienceModifier = 0.5f;

	float CurrentExperience = (float)GetPlayerCombatExperience(Player);

	while ((CurrentExperience > 0) && (theCombatLevelExperienceModifier > 0))
	{
		CurrentExperience -= (1.0f + (thePlayerLevel - 1) * theCombatLevelExperienceModifier) * theCombatBaseExperience;

		if (CurrentExperience > 0)
		{
			thePlayerLevel++;
		}
	}

	thePlayerLevel = imaxi(imini(thePlayerLevel, 10), 1);

	return thePlayerLevel;
}

float GetPlayerRadius(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return 0.0f; }

	int hullnum = GetPlayerHullIndex(pEdict);

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

int GetPlayerHullIndex(const edict_t* pEdict, const bool bIsCrouching)
{
	if (FNullEnt(pEdict)) { return 0; }

	NSPlayerClass PlayerClass = GetPlayerClass(pEdict);

	switch (PlayerClass)
	{
	case CLASS_MARINE:
		return (bIsCrouching) ? head_hull : human_hull;
	case CLASS_MARINE_COMMANDER:
		return head_hull;
	case CLASS_EGG:
		return head_hull;
	case CLASS_SKULK:
		return head_hull;
	case CLASS_GORGE:
		return head_hull;
	case CLASS_LERK:
		return head_hull;
	case CLASS_FADE:
		return (bIsCrouching) ? head_hull : human_hull;
	case CLASS_ONOS:
		return (bIsCrouching) ? human_hull : large_hull;
	default:
		return head_hull;
	}

	return head_hull;
}

int GetPlayerHullIndex(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return 0; }

	NSPlayerClass PlayerClass = GetPlayerClass(pEdict);

	if (PlayerClass == CLASS_NONE) { return head_hull; }

	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);

	switch (PlayerClass)
	{
	case CLASS_MARINE:
		return (bIsCrouching) ? head_hull : human_hull;
	case CLASS_MARINE_COMMANDER:
		return head_hull;
	case CLASS_EGG:
		return head_hull;
	case CLASS_SKULK:
		return head_hull;
	case CLASS_GORGE:
		return head_hull;
	case CLASS_LERK:
		return head_hull;
	case CLASS_FADE:
		return (bIsCrouching) ? head_hull : human_hull;
	case CLASS_ONOS:
		return (bIsCrouching) ? human_hull : large_hull;
	default:
		return head_hull;
	}

	return head_hull;
}

float GetPlayerEnergyRegenPerSecond(edict_t* Player)
{
	int AdrenalineLevel = 0;

	if (Player->v.iuser4 & MASK_UPGRADE_5)
	{
		AdrenalineLevel = 1;

		if (Player->v.iuser4 & MASK_UPGRADE_13)
		{
			AdrenalineLevel = 3;
		}
		else if (Player->v.iuser4 & MASK_UPGRADE_12)
		{
			AdrenalineLevel = 2;
		}
	}

	return kAlienEnergyRate * (1.0f + (AdrenalineLevel * kAdrenalineEnergyPercentPerLevel));
}

float GetPlayerOverallHealthPercent(const edict_t* Player)
{
	if (IsEdictStructure(Player)) { return (Player->v.health / Player->v.max_health); }

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

	int iuser3 = pEdict->v.iuser3;

	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? Vector(0.0f, 0.0f, 18.0f) : Vector(0.0f, 0.0f, 36.0f);
		break;
	case AVH_USER3_COMMANDER_PLAYER:
		return Vector(0.0f, 0.0f, 36.0f);
		break;
	case AVH_USER3_ALIEN_EMBRYO:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER1:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER2:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER3:
		return Vector(0.0f, 0.0f, 18.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? Vector(0.0f, 0.0f, 18.0f) : Vector(0.0f, 0.0f, 36.0f);
		break;
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? Vector(0.0f, 0.0f, 36.0f) : Vector(0.0f, 0.0f, 54.0f);
		break;
	default:
		return Vector(0.0f, 0.0f, 36.0f);
		break;
	}
}

Vector GetPlayerBottomOfCollisionHull(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	int iuser3 = pEdict->v.iuser3;
	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);
	Vector origin = pEdict->v.origin;


	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 18.0f)) : (origin - Vector(0.0f, 0.0f, 36.0f));
		break;
	case AVH_USER3_COMMANDER_PLAYER:
		return origin;
		break;
	case AVH_USER3_ALIEN_EMBRYO:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER1:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER2:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER3:
		return (origin - Vector(0.0f, 0.0f, 18.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 18.0f)) : (origin - Vector(0.0f, 0.0f, 36.0f));
		break;
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? (origin - Vector(0.0f, 0.0f, 36.0f)) : (origin - Vector(0.0f, 0.0f, 54.0f));
		break;
	default:
		return origin;
		break;
	}
}

Vector GetPlayerTopOfCollisionHull(const edict_t* pEdict, const bool bIsCrouching)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	int iuser3 = pEdict->v.iuser3;
	Vector origin = pEdict->v.origin;


	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_COMMANDER_PLAYER:
		return origin;
	case AVH_USER3_ALIEN_EMBRYO:
	case AVH_USER3_ALIEN_PLAYER1:
	case AVH_USER3_ALIEN_PLAYER2:
	case AVH_USER3_ALIEN_PLAYER3:
		return (origin + Vector(0.0f, 0.0f, 19.0f));
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 37.0f)) : (origin + Vector(0.0f, 0.0f, 55.0f));
	default:
		return origin;
	}
}

Vector GetPlayerTopOfCollisionHull(const edict_t* pEdict)
{
	if (FNullEnt(pEdict)) { return ZERO_VECTOR; }

	int iuser3 = pEdict->v.iuser3;
	bool bIsCrouching = (pEdict->v.flags & FL_DUCKING);
	Vector origin = pEdict->v.origin;


	switch (iuser3)
	{
	case AVH_USER3_MARINE_PLAYER:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_COMMANDER_PLAYER:
		return origin;
	case AVH_USER3_ALIEN_EMBRYO:
	case AVH_USER3_ALIEN_PLAYER1:
	case AVH_USER3_ALIEN_PLAYER2:
	case AVH_USER3_ALIEN_PLAYER3:
		return (origin + Vector(0.0f, 0.0f, 19.0f));
	case AVH_USER3_ALIEN_PLAYER4:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 19.0f)) : (origin + Vector(0.0f, 0.0f, 37.0f));
	case AVH_USER3_ALIEN_PLAYER5:
		return (bIsCrouching) ? (origin + Vector(0.0f, 0.0f, 37.0f)) : (origin + Vector(0.0f, 0.0f, 55.0f));
	default:
		return origin;
	}
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

int GetPlayerIndex(const edict_t* Edict)
{
	if (!IsEdictPlayer(Edict)) { return -1; }

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] == Edict) { return i; }
	}

	return -1;
}

bool IsEdictPlayer(const edict_t* edict)
{
	if (FNullEnt(edict)) { return false; }

	return (edict->v.flags & FL_CLIENT) || (edict->v.flags & FL_FAKECLIENT);
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
	
	if (vDist3DSq(Player->v.origin, UTIL_GetCentreOfEntity(Target)) > sqrf(vSize3D(Target->v.size) + vSize3D(Player->v.size))) { return false; }

	edict_t* UseObject = nullptr;

	while ((UseObject = UTIL_FindEntityInSphere(UseObject, Player->v.origin, 60.0f)) != NULL)
	{
		if (UseObject == Target) { return true; }
	}

	return false;
	
	/*Vector StartTrace = GetPlayerEyePosition(Player);
	Vector UseDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - StartTrace);
	// Sometimes if the bot is REALLY close to the target, the trace fails. Give it 5 units extra of room to avoid this and compensate during the trace.
	StartTrace = StartTrace - (UseDir * 5.0f);
		
	Vector EndTrace = StartTrace + (UseDir * (max_player_use_reach + 5.0f));

	TraceResult hit;

	UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, Player->v.pContainingEntity, &hit);

	if (!FNullEnt(hit.pHit))
	{
		const char* HitName = STRING(hit.pHit->v.classname);
	}
	

	return hit.pHit == Target;*/
}

bool PlayerHasHeavyArmour(const edict_t* Player)
{
	if (!IsPlayerMarine(Player)) { return false; }
	return (Player->v.iuser4 & MASK_UPGRADE_13);
}

bool PlayerHasJetpack(edict_t* Player)
{
	if (!IsPlayerMarine(Player)) { return false; }
	return (Player->v.iuser4 & MASK_UPGRADE_7);
}

bool PlayerHasEquipment(edict_t* Player)
{
	if (!IsPlayerMarine(Player)) { return false; }
	return PlayerHasHeavyArmour(Player) || PlayerHasJetpack(Player);
}

bool PlayerHasSpecialWeapon(edict_t* Player)
{
	if (!IsPlayerMarine(Player)) { return false; }
	return !PlayerHasWeapon(Player, WEAPON_MARINE_MG);
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

bool PlayerHasWeapon(const edict_t* Player, const NSWeapon DesiredCombatWeapon)
{

	bool bIsCombatMode = (GAME_GetGameMode() == GAME_MODE_COMBAT);
	bool bUnlockedAbility3 = false;
	bool bUnlockedAbility4 = false;

	if (bIsCombatMode)
	{
		bot_t* BotRef = GetBotPointer(Player);

		if (BotRef)
		{
			bUnlockedAbility3 = (BotRef->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ABILITY3);
			bUnlockedAbility4 = (BotRef->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ABILITY4);
		}
	}


	switch (DesiredCombatWeapon)
	{
	case WEAPON_MARINE_MG:
	case WEAPON_MARINE_HMG:
	case WEAPON_MARINE_SHOTGUN:
	case WEAPON_MARINE_WELDER:
	case WEAPON_MARINE_GL:
	case WEAPON_MARINE_GRENADE:
	case WEAPON_MARINE_KNIFE:
	case WEAPON_MARINE_MINES:
	case WEAPON_MARINE_PISTOL:
		return (IsPlayerMarine(Player) && (Player->v.weapons & (1 << DesiredCombatWeapon)));

	case WEAPON_SKULK_BITE:
	case WEAPON_SKULK_PARASITE:
		return IsPlayerSkulk(Player);
	case WEAPON_SKULK_LEAP:
		return (IsPlayerSkulk(Player) && ((bIsCombatMode) ? bUnlockedAbility3 : (UTIL_GetNumActiveHives() >= 2)));
	case WEAPON_SKULK_XENOCIDE:
		return (IsPlayerSkulk(Player) && ((bIsCombatMode) ? bUnlockedAbility4 : (UTIL_GetNumActiveHives() >= 3)));

	case WEAPON_GORGE_SPIT:
	case WEAPON_GORGE_HEALINGSPRAY:
		return IsPlayerGorge(Player);
	case WEAPON_GORGE_BILEBOMB:
		return (IsPlayerGorge(Player) && ((bIsCombatMode) ? bUnlockedAbility3 : (UTIL_GetNumActiveHives() >= 2)));
	case WEAPON_GORGE_WEB:
		return (IsPlayerGorge(Player) && ((bIsCombatMode) ? bUnlockedAbility4 : (UTIL_GetNumActiveHives() >= 3)));

	case WEAPON_LERK_BITE:
	case WEAPON_LERK_SPORES:
		return IsPlayerLerk(Player);
	case WEAPON_LERK_UMBRA:
		return (IsPlayerLerk(Player) && ((bIsCombatMode) ? bUnlockedAbility3 : (UTIL_GetNumActiveHives() >= 2)));
	case WEAPON_LERK_PRIMALSCREAM:
		return (IsPlayerLerk(Player) && ((bIsCombatMode) ? bUnlockedAbility4 : (UTIL_GetNumActiveHives() >= 3)));

	case WEAPON_FADE_SWIPE:
	case WEAPON_FADE_BLINK:
		return IsPlayerFade(Player);
	case WEAPON_FADE_METABOLIZE:
		return (IsPlayerFade(Player) && ((bIsCombatMode) ? bUnlockedAbility3 : (UTIL_GetNumActiveHives() >= 2)));
	case WEAPON_FADE_ACIDROCKET:
		return (IsPlayerFade(Player) && ((bIsCombatMode) ? bUnlockedAbility4 : (UTIL_GetNumActiveHives() >= 3)));

	case WEAPON_ONOS_GORE:
	case WEAPON_ONOS_DEVOUR:
		return IsPlayerOnos(Player);
	case WEAPON_ONOS_STOMP:
		return (IsPlayerOnos(Player) && (bIsCombatMode) ? bUnlockedAbility3 : (UTIL_GetNumActiveHives() >= 2));
	case WEAPON_ONOS_CHARGE:
		return (IsPlayerOnos(Player) && (bIsCombatMode) ? bUnlockedAbility4 : (UTIL_GetNumActiveHives() >= 3));

	}

	return false;
}

bool PlayerHasAlienUpgradeOfType(const edict_t* Player, const HiveTechStatus TechType)
{
	if (!IsPlayerOnAlienTeam(Player)) { return false; }

	switch (TechType)
	{
	case HIVE_TECH_DEFENCE:
		return ((Player->v.iuser4 & MASK_UPGRADE_1) || (Player->v.iuser4 & MASK_UPGRADE_2) || (Player->v.iuser4 & MASK_UPGRADE_3));
	case HIVE_TECH_MOVEMENT:
		return ((Player->v.iuser4 & MASK_UPGRADE_4) || (Player->v.iuser4 & MASK_UPGRADE_5) || (Player->v.iuser4 & MASK_UPGRADE_6));
	case HIVE_TECH_SENSORY:
		return ((Player->v.iuser4 & MASK_UPGRADE_7) || (Player->v.iuser4 & MASK_UPGRADE_8) || (Player->v.iuser4 & MASK_UPGRADE_9));
	default:
		return false;
	}
}

float GetPlayerCloakAmount(const edict_t* Player)
{
	if (!(Player->v.iuser4 & MASK_UPGRADE_7) && !(Player->v.iuser4 & MASK_SENSORY_NEARBY)) { return 0.0f; }

	if (Player->v.iuser4 & MASK_VIS_SIGHTED)
	{
		return 0.0f; 
	}
	else
	{
		return 1.0f;
	}
}
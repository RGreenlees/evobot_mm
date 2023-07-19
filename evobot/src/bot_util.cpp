
#include "bot_util.h"

#include <extdll.h>
#include <dllapi.h>
#include <meta_api.h>

#include "general_util.h"
#include "player_util.h"
#include "NS_Constants.h"
#include "bot_math.h"
#include "bot_navigation.h"
#include "bot_tactical.h"
#include "game_state.h"
#include "bot_weapons.h"
#include "bot_task.h"
#include "bot_marine.h"
#include "bot_alien.h"
#include "bot_commander.h"

extern bot_t bots[MAX_CLIENTS];
extern edict_t* clients[MAX_CLIENTS];

extern bool bGameIsActive;

extern char g_argv[1024];
bool isFakeClientCommand;
int fake_arg_count;

// Updates the bot's viewing frustum. Done once per frame per bot
void BotUpdateViewFrustum(bot_t* pBot)
{

	MAKE_VECTORS(pBot->pEdict->v.v_angle);
	Vector up = gpGlobals->v_up;
	Vector forward = gpGlobals->v_forward;
	Vector right = gpGlobals->v_right;

	Vector fc = (pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs) + (forward * BOT_MAX_VIEW);

	Vector fbl = fc + (up * f_ffheight / 2.0f) - (right * f_ffwidth / 2.0f);
	Vector fbr = fc + (up * f_ffheight / 2.0f) + (right * f_ffwidth / 2.0f);
	Vector ftl = fc - (up * f_ffheight / 2.0f) - (right * f_ffwidth / 2.0f);
	Vector ftr = fc - (up * f_ffheight / 2.0f) + (right * f_ffwidth / 2.0f);

	Vector nc = (pBot->pEdict->v.origin + pBot->pEdict->v.view_ofs) + (forward * BOT_MIN_VIEW);

	Vector nbl = nc + (up * f_fnheight / 2.0f) - (right * f_fnwidth / 2.0f);
	Vector nbr = nc + (up * f_fnheight / 2.0f) + (right * f_fnwidth / 2.0f);
	Vector ntl = nc - (up * f_fnheight / 2.0f) - (right * f_fnwidth / 2.0f);
	Vector ntr = nc - (up * f_fnheight / 2.0f) + (right * f_fnwidth / 2.0f);

	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_TOP], ftl, ntl, ntr);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_BOTTOM], fbr, nbr, nbl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_LEFT], fbl, nbl, ntl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_RIGHT], ftr, ntr, nbr);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_NEAR], nbr, ntr, ntl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_FAR], fbl, ftl, ftr);

}

enemy_status* UTIL_GetTrackedEnemyRefForTarget(bot_t* pBot, edict_t* Target)
{
	for (int i = 0; i < 32; i++)
	{
		if (pBot->TrackedEnemies[i].EnemyEdict == Target)
		{
			return &pBot->TrackedEnemies[i];
		}
	}

	return nullptr;
}

/* Makes the bot look at the specified entity */
void BotLookAt(bot_t* pBot, edict_t* target)
{
	if (!target) { return; }

	pBot->LookTarget = target;

	if (!IsEdictPlayer(target))
	{
		pBot->LookTargetLocation = UTIL_GetCentreOfEntity(pBot->LookTarget);
		pBot->LastTargetTrackUpdate = gpGlobals->time;
		return;
	}

	enemy_status* TrackedEnemyRef = UTIL_GetTrackedEnemyRefForTarget(pBot, target);

	Vector TargetVelocity = (TrackedEnemyRef) ? TrackedEnemyRef->LastSeenVelocity : pBot->LookTarget->v.velocity;
	Vector TargetLocation = (TrackedEnemyRef) ? TrackedEnemyRef->LastSeenLocation : UTIL_GetCentreOfEntity(pBot->LookTarget);

	NSWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

	Vector NewLoc = UTIL_GetAimLocationToLeadTarget(pBot->CurrentEyePosition, TargetLocation, TargetVelocity, UTIL_GetProjectileVelocityForWeapon(CurrentWeapon));

	float Offset = frandrange(30.0f, 50.0f);

	float motion_tracking_skill = (IsPlayerMarine(pBot->pEdict)) ? pBot->BotSkillSettings.marine_bot_motion_tracking_skill : pBot->BotSkillSettings.alien_bot_motion_tracking_skill;

	Offset -= Offset * motion_tracking_skill;

	if (randbool())
	{
		Offset *= -1.0f;
	}

	float NewDist = vDist3D(TargetLocation, NewLoc) + Offset;


	float MoveSpeed = vSize3D(target->v.velocity);

	Vector MoveVector = (MoveSpeed > 5.0f) ? UTIL_GetVectorNormal(target->v.velocity) : ZERO_VECTOR;

	Vector NewAimLoc = TargetLocation + (MoveVector * NewDist);

	pBot->LookTargetLocation = NewAimLoc;
	pBot->LastTargetTrackUpdate = gpGlobals->time;
}

/* Makes the bot look at the specified position */
void BotLookAt(bot_t* pBot, const Vector target)
{

	pBot->LookTargetLocation.x = target.x;
	pBot->LookTargetLocation.y = target.y;
	pBot->LookTargetLocation.z = target.z;

}

void BotMoveLookAt(bot_t* pBot, const Vector target)
{
	pBot->MoveLookLocation.x = target.x;
	pBot->MoveLookLocation.y = target.y;
	pBot->MoveLookLocation.z = target.z;
}

bool BotUseObject(bot_t* pBot, edict_t* Target, bool bContinuous)
{
	if (FNullEnt(Target)) { return false; }

	Vector ClosestPoint = UTIL_GetClosestPointOnEntityToLocation(pBot->pEdict->v.origin, Target);
	Vector TargetCentre = UTIL_GetCentreOfEntity(Target);

	Vector AimPoint = ClosestPoint;

	if (IsEdictStructure(Target))
	{
		AimPoint = TargetCentre;
		AimPoint.z = ClosestPoint.z;
	}

	BotLookAt(pBot, AimPoint);

	if (!bContinuous && ((gpGlobals->time - pBot->LastUseTime) < min_player_use_interval)) { return false; }

	Vector AimDir = UTIL_GetForwardVector2D(pBot->pEdict->v.v_angle);
	Vector TargetAimDir = (IsEdictStructure(Target) ? UTIL_GetVectorNormal2D(TargetCentre - pBot->CurrentEyePosition) : UTIL_GetVectorNormal2D(ClosestPoint - pBot->CurrentEyePosition));

	float AimDot = UTIL_GetDotProduct2D(AimDir, TargetAimDir);

	if (AimDot >= 0.95f)
	{
		pBot->pEdict->v.button |= IN_USE;
		pBot->LastUseTime = gpGlobals->time;
		return true;
	}

	return false;
}

void BotJump(bot_t* pBot)
{
	if (pBot->BotNavInfo.IsOnGround)
	{
		if (gpGlobals->time - pBot->BotNavInfo.LandedTime >= 0.5f)
		{
			pBot->pEdict->v.button |= IN_JUMP;
			pBot->BotNavInfo.bIsJumping = true;
			pBot->BotNavInfo.bHasAttemptedJump = true;
		}
	}
	else
	{
		if (pBot->BotNavInfo.bIsJumping)
		{
			// Skulks, gorges and lerks can't duck jump...
			if (!IsPlayerSkulk(pBot->pEdict) && !IsPlayerGorge(pBot->pEdict) && !IsPlayerLerk(pBot->pEdict))
			{
				pBot->pEdict->v.button |= IN_DUCK;
			}
		}
	}
}

bool CanBotLeap(bot_t* pBot)
{
	return (PlayerHasWeapon(pBot->pEdict, WEAPON_SKULK_LEAP)) || (PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_BLINK));
}

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector Location, edict_t* Entity)
{
	return Vector(clampf(Location.x, Entity->v.absmin.x, Entity->v.absmax.x), clampf(Location.y, Entity->v.absmin.y, Entity->v.absmax.y), clampf(Location.z, Entity->v.absmin.z, Entity->v.absmax.z));
}

void BotLeap(bot_t* pBot, const Vector TargetLocation)
{

	if (!CanBotLeap(pBot))
	{
		BotJump(pBot);
		return;
	}

	bool bShouldLeap = pBot->BotNavInfo.IsOnGround && (gpGlobals->time - pBot->BotNavInfo.LandedTime >= 0.2f && gpGlobals->time - pBot->BotNavInfo.LeapAttemptedTime >= 0.5f);

	if (!bShouldLeap) { return; }

	NSWeapon LeapWeapon = (IsPlayerSkulk(pBot->pEdict)) ? WEAPON_SKULK_LEAP : WEAPON_FADE_BLINK;

	if (GetBotCurrentWeapon(pBot) != LeapWeapon)
	{
		pBot->DesiredMoveWeapon = LeapWeapon;
		return;
	}

	Vector LookLocation = TargetLocation;

	int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

	unsigned char NavArea = UTIL_GetNavAreaAtLocation(NavProfileIndex, pBot->pEdict->v.origin);

	if (NavArea == SAMPLE_POLYAREA_CROUCH)
	{
		Vector MoveDir = UTIL_GetVectorNormal2D(TargetLocation - pBot->pEdict->v.origin);
		LookLocation = (pBot->CurrentEyePosition + (MoveDir * 50.0f) + Vector(0.0f, 0.0f, 10.0f));
	}
	else
	{
		LookLocation = LookLocation + Vector(0.0f, 0.0f, 200.0f);

		if (LeapWeapon == WEAPON_FADE_BLINK)
		{
			float PlayerCurrentSpeed = vSize3D(pBot->pEdict->v.velocity);
			float LaunchVelocity = PlayerCurrentSpeed + 255.0f;

			Vector LaunchAngle = GetPitchForProjectile(pBot->CurrentEyePosition, TargetLocation, LaunchVelocity, GOLDSRC_GRAVITY);

			if (LaunchAngle != ZERO_VECTOR)
			{
				LaunchAngle = UTIL_GetVectorNormal(LaunchAngle);
				LookLocation = pBot->CurrentEyePosition + (LaunchAngle * 200.0f);
			}
		}

		if (LeapWeapon == WEAPON_SKULK_LEAP)
		{
			float PlayerCurrentSpeed = vSize3D(pBot->pEdict->v.velocity);
			float LaunchVelocity = PlayerCurrentSpeed + 500.0f;

			Vector LaunchAngle = GetPitchForProjectile(pBot->CurrentEyePosition, TargetLocation, LaunchVelocity, GOLDSRC_GRAVITY);

			if (LaunchAngle != ZERO_VECTOR)
			{
				LaunchAngle = UTIL_GetVectorNormal(LaunchAngle);
				LookLocation = pBot->CurrentEyePosition + (LaunchAngle * 200.0f);
			}
		}
	}

	BotMoveLookAt(pBot, LookLocation);

	if (IsPlayerFade(pBot->pEdict) && !pBot->BotNavInfo.IsOnGround)
	{
		float RequiredVelocity = UTIL_GetVelocityRequiredToReachTarget(pBot->pEdict->v.origin, TargetLocation, GOLDSRC_GRAVITY);
		float CurrentVelocity = vSize3D(pBot->pEdict->v.velocity);

		bShouldLeap = (CurrentVelocity < RequiredVelocity);
	}

	if (bShouldLeap)
	{

		Vector FaceAngle = UTIL_GetForwardVector2D(pBot->pEdict->v.v_angle);
		Vector MoveDir = UTIL_GetVectorNormal2D(TargetLocation - pBot->pEdict->v.origin);

		float Dot = UTIL_GetDotProduct2D(FaceAngle, MoveDir);

		if (Dot >= 0.98f)
		{
			pBot->pEdict->v.button = IN_ATTACK2;
			pBot->BotNavInfo.bIsJumping = true;
			pBot->BotNavInfo.LeapAttemptedTime = gpGlobals->time;
		}
	}
	else
	{
		if (pBot->BotNavInfo.bIsJumping)
		{
			// Skulks, gorges and lerks can't duck jump...
			if (!IsPlayerSkulk(pBot->pEdict) && !IsPlayerGorge(pBot->pEdict) && !IsPlayerLerk(pBot->pEdict))
			{
				pBot->pEdict->v.button |= IN_DUCK;
			}
		}
	}

}



float GetLeapCost(bot_t* pBot)
{
	switch (pBot->bot_ns_class)
	{
	case CLASS_SKULK:
		return ((PlayerHasWeapon(pBot->pEdict, WEAPON_SKULK_LEAP)) ? kLeapEnergyCost : 0.0f);
	case CLASS_FADE:
		return ((PlayerHasWeapon(pBot->pEdict, WEAPON_FADE_BLINK)) ? kBlinkEnergyCost : 0.0f);
	case CLASS_ONOS:
		return ((PlayerHasWeapon(pBot->pEdict, WEAPON_ONOS_CHARGE)) ? kChargeEnergyCost : 0.0f);
	default:
		return 0.0f;
	}
}

void BotSuicide(bot_t* pBot)
{
	if (pBot && !IsPlayerDead(pBot->pEdict) && !pBot->bIsPendingKill)
	{
		pBot->bIsPendingKill = true;
		MDLL_ClientKill(pBot->pEdict);
	}
}

void BotTakeDamage(bot_t* pBot, int damageTaken, edict_t* aggressor)
{
	int aggressorIndex = GetPlayerIndex(aggressor);

	if (aggressorIndex > -1 && !IsPlayerDead(aggressor) && aggressor->v.team != pBot->pEdict->v.team && !IsPlayerBeingDigested(aggressor))
	{
		pBot->TrackedEnemies[aggressorIndex].LastSeenTime = gpGlobals->time;

		// If the bot can't see the enemy (bCurrentlyVisible is false) then set the last seen location to a random point in the vicinity so the bot doesn't immediately know where they are
		if (pBot->TrackedEnemies[aggressorIndex].bInFOV)
		{
			pBot->TrackedEnemies[aggressorIndex].LastSeenLocation = aggressor->v.origin;
		}
		else
		{
			// The further the enemy is, the more inaccurate the bot's guess will be where they are
			pBot->TrackedEnemies[aggressorIndex].LastSeenLocation = UTIL_GetRandomPointOnNavmeshInRadius(ALL_NAV_PROFILE, aggressor->v.origin, UTIL_MetresToGoldSrcUnits(5.0f));

			if (!UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, pBot->TrackedEnemies[aggressorIndex].LastSeenLocation + Vector(0.0f, 0.0f, 18.0f)))
			{
				pBot->TrackedEnemies[aggressorIndex].LastSeenLocation = aggressor->v.origin;
			}
		}

		//UTIL_DrawLine(clients[0], pBot->pEdict->v.origin, pBot->TrackedEnemies[aggressorIndex].LastSeenLocation, 5.0f);

		pBot->TrackedEnemies[aggressorIndex].LastSeenVelocity = aggressor->v.velocity;
		pBot->TrackedEnemies[aggressorIndex].bIsAwareOfPlayer = true;
	}
}

// The bot was killed by another bot or human player (or blew themselves up)
void BotDied(bot_t* pBot, edict_t* killer)
{
	//UTIL_ClearGuardInfo(pBot);
	ClearBotMovement(pBot);

	pBot->LastCombatTime = 0.0f;

	for (int i = 0; i < 32; i++)
	{
		pBot->TrackedEnemies[i].LastSeenTime = 0.0f;
	}

	UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);
	UTIL_ClearBotTask(pBot, &pBot->MoveTask);
	

	pBot->bIsPendingKill = false;

	pBot->BotNavInfo.LastNavMeshPosition = ZERO_VECTOR;
	pBot->BotNavInfo.LastPathFollowPosition = ZERO_VECTOR;
}

// Bot killed another player or bot
void BotKilledPlayer(bot_t* pBot, edict_t* victim)
{

}

bot_t* GetBotPointer(const edict_t* pEdict)
{

	for (int index = 0; index < MAX_CLIENTS; index++)
	{
		if (bots[index].is_used && bots[index].pEdict == pEdict)
		{
			return &bots[index];
		}
	}

	return nullptr;  // return NULL if edict is not a bot
}

int GetBotIndex(edict_t* pEdict)
{
	if (!pEdict) { return -1; }
	int index;

	for (index = 0; index < MAX_CLIENTS; index++)
	{
		if (bots[index].pEdict == pEdict)
		{
			return index;
		}
	}

	return -1;  // return -1 if edict is not a bot
}

void BotSay(bot_t* pBot, char* textToSay)
{
	UTIL_HostSay(pBot->pEdict, 0, textToSay);
}

void BotTeamSay(bot_t* pBot, char* textToSay)
{
	UTIL_HostSay(pBot->pEdict, 1, textToSay);
}

bot_msg* UTIL_GetAvailableBotMsgSlot(bot_t* pBot)
{
	for (int i = 0; i < 5; i++)
	{
		if (!pBot->ChatMessages[i].bIsPending) { return &pBot->ChatMessages[i]; }
	}

	return nullptr;
}

void BotSay(bot_t* pBot, float Delay, char* textToSay)
{
	bot_msg* msgSlot = UTIL_GetAvailableBotMsgSlot(pBot);

	if (msgSlot)
	{
		msgSlot->bIsPending = true;
		msgSlot->bIsTeamSay = false;
		msgSlot->SendTime = gpGlobals->time + Delay;
		sprintf(msgSlot->msg, textToSay);
	}
}

void BotTeamSay(bot_t* pBot, float Delay, char* textToSay)
{
	bot_msg* msgSlot = UTIL_GetAvailableBotMsgSlot(pBot);

	if (msgSlot)
	{
		msgSlot->bIsPending = true;
		msgSlot->bIsTeamSay = true;
		msgSlot->SendTime = gpGlobals->time + Delay;
		sprintf(msgSlot->msg, textToSay);
	}
}

BotAttackResult PerformAttackLOSCheck(bot_t* pBot, const NSWeapon Weapon, const edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return ATTACK_INVALIDTARGET; }

	if (Weapon == WEAPON_NONE) { return ATTACK_NOWEAPON; }

	// Don't need aiming or special LOS checks for primal scream as it's AoE buff
	if (Weapon == WEAPON_LERK_PRIMALSCREAM)
	{
		return ATTACK_SUCCESS;
	}

	// Add a LITTLE bit of give to avoid edge cases where the bot is a smidge out of range
	float MaxWeaponRange = GetMaxIdealWeaponRange(Weapon) - 5.0f;

	// Don't need aiming or special LOS checks for Xenocide as it's an AOE attack, just make sure we're close enough and don't have a wall in the way
	if (Weapon == WEAPON_SKULK_XENOCIDE)
	{
		if (vDist3DSq(pBot->pEdict->v.origin, Target->v.origin) <= sqrf(MaxWeaponRange) && UTIL_QuickTrace(pBot->pEdict, pBot->pEdict->v.origin, Target->v.origin))
		{
			return ATTACK_SUCCESS;
		}
		else
		{
			return ATTACK_OUTOFRANGE;
		}
	}

	// For charge and stomp, we can go through stuff so don't need to check for being blocked
	if (Weapon == WEAPON_ONOS_CHARGE || Weapon == WEAPON_ONOS_STOMP)
	{
		if (vDist3DSq(pBot->pEdict->v.origin, Target->v.origin) > sqrf(MaxWeaponRange)) { return ATTACK_OUTOFRANGE; }

		if (!UTIL_QuickTrace(pBot->pEdict, pBot->pEdict->v.origin, Target->v.origin) || fabsf(Target->v.origin.z - Target->v.origin.z) > 50.0f) { return ATTACK_OUTOFRANGE; }

		return ATTACK_SUCCESS;
	}

	TraceResult hit;

	Vector StartTrace = pBot->CurrentEyePosition;

	Vector AttackDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - StartTrace);

	Vector EndTrace = pBot->CurrentEyePosition + (AttackDir * MaxWeaponRange);

	UTIL_TraceLine(StartTrace, EndTrace, dont_ignore_monsters, dont_ignore_glass, pBot->pEdict->v.pContainingEntity, &hit);

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

BotAttackResult PerformAttackLOSCheck(const Vector Location, const NSWeapon Weapon, const edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return ATTACK_INVALIDTARGET; }

	if (Weapon == WEAPON_NONE) { return ATTACK_NOWEAPON; }

	float MaxWeaponRange = GetMaxIdealWeaponRange(Weapon);

	// Don't need aiming or special LOS checks for Xenocide as it's an AOE attack, just make sure we're close enough and don't have a wall in the way
	if (Weapon == WEAPON_SKULK_XENOCIDE)
	{
		if (vDist3DSq(Location, Target->v.origin) <= sqrf(MaxWeaponRange) && UTIL_QuickTrace(nullptr, Location, Target->v.origin))
		{
			return ATTACK_SUCCESS;
		}
		else
		{
			return ATTACK_OUTOFRANGE;
		}
	}

	// For charge and stomp, we can go through stuff so don't need to check for being blocked
	if (Weapon == WEAPON_ONOS_CHARGE || Weapon == WEAPON_ONOS_STOMP)
	{
		if (vDist3DSq(Location, Target->v.origin) > sqrf(MaxWeaponRange)) { return ATTACK_OUTOFRANGE; }

		if (!UTIL_QuickTrace(nullptr, Location, Target->v.origin) || fabsf(Target->v.origin.z - Target->v.origin.z) > 50.0f) { return ATTACK_OUTOFRANGE; }

		return ATTACK_SUCCESS;
	}

	bool bIsMeleeWeapon = IsMeleeWeapon(Weapon);

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

void BotShootLocation(bot_t* pBot, NSWeapon AttackWeapon, const Vector TargetLocation)
{
	if (!TargetLocation) { return; }

	NSWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

	pBot->DesiredCombatWeapon = AttackWeapon;

	if (CurrentWeapon != AttackWeapon)
	{
		return;
	}

	if (CurrentWeapon == WEAPON_NONE) { return; }

	if (CurrentWeapon == WEAPON_SKULK_XENOCIDE)
	{
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) >= pBot->current_weapon.MinRefireTime)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}

		return;
	}

	if (AttackWeapon == WEAPON_LERK_SPORES || AttackWeapon == WEAPON_LERK_UMBRA)
	{
		BotLookAt(pBot, TargetLocation);
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) < pBot->current_weapon.MinRefireTime)
		{
			return;
		}

		Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);

		TraceResult Hit;
		Vector TraceEnd = pBot->CurrentEyePosition + (AimDir * 3000.0f);

		UTIL_TraceLine(pBot->CurrentEyePosition, TraceEnd, dont_ignore_monsters, dont_ignore_glass, pBot->pEdict->v.pContainingEntity, &Hit);

		if (Hit.flFraction >= 1.0f) { return; }

		if (vDist3DSq(Hit.vecEndPos, TargetLocation) <= sqrf(kSporeCloudRadius))
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}

		return;
	}

	// For charge and stomp, we can go through stuff so don't need to check for being blocked
	if (CurrentWeapon == WEAPON_ONOS_CHARGE || CurrentWeapon == WEAPON_ONOS_STOMP)
	{
		BotLookAt(pBot, TargetLocation);

		Vector DirToTarget = UTIL_GetVectorNormal2D(TargetLocation - pBot->pEdict->v.origin);
		float DotProduct = UTIL_GetDotProduct2D(UTIL_GetForwardVector(pBot->pEdict->v.v_angle), DirToTarget);

		float MinDotProduct = (CurrentWeapon == WEAPON_ONOS_STOMP) ? 0.95f : 0.75f;

		if (DotProduct >= MinDotProduct)
		{
			if ((gpGlobals->time - pBot->current_weapon.LastFireTime) < pBot->current_weapon.MinRefireTime)
			{
				return;
			}

			if (CurrentWeapon == WEAPON_ONOS_CHARGE)
			{
				pBot->pEdict->v.button |= IN_ATTACK2;
			}
			else
			{
				pBot->pEdict->v.button |= IN_ATTACK;
			}

			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}

		return;
	}

	if (IsMeleeWeapon(CurrentWeapon))
	{
		BotLookAt(pBot, TargetLocation);
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) >= pBot->current_weapon.MinRefireTime)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}
		return;
	}

	Vector TargetAimDir = ZERO_VECTOR;

	if (CurrentWeapon == WEAPON_MARINE_GL || CurrentWeapon == WEAPON_MARINE_GRENADE)
	{
		Vector AimLocation = TargetLocation;
		Vector NewAimAngle = GetPitchForProjectile(pBot->CurrentEyePosition, AimLocation, 800.0f, 640.0f);

		NewAimAngle = UTIL_GetVectorNormal(NewAimAngle);

		AimLocation = pBot->CurrentEyePosition + (NewAimAngle * 200.0f);

		BotLookAt(pBot, AimLocation);
		TargetAimDir = UTIL_GetVectorNormal(AimLocation - pBot->CurrentEyePosition);
	}
	else
	{
		BotLookAt(pBot, TargetLocation);
		TargetAimDir = UTIL_GetVectorNormal(TargetLocation - pBot->CurrentEyePosition);
	}

	if (WeaponCanBeReloaded(CurrentWeapon))
	{
		bool bShouldReload = (BotGetCurrentWeaponReserveAmmo(pBot) > 0);

		if (CurrentWeapon == WEAPON_MARINE_SHOTGUN)
		{
			bShouldReload = bShouldReload && ((float)BotGetCurrentWeaponClipAmmo(pBot) / (float)BotGetCurrentWeaponMaxClipAmmo(pBot) < 0.5f);
		}
		else
		{
			bShouldReload = bShouldReload && BotGetCurrentWeaponClipAmmo(pBot) == 0;
		}

		if (bShouldReload)
		{
			if (!pBot->current_weapon.bIsReloading)
			{
				pBot->pEdict->v.button |= IN_RELOAD;
				pBot->current_weapon.bIsReloading = true;
			}
			return;
		}

	}

	if (IsBotReloading(pBot))
	{
		if (BotGetCurrentWeaponClipAmmo(pBot) == BotGetCurrentWeaponMaxClipAmmo(pBot) || BotGetCurrentWeaponReserveAmmo(pBot) == 0)
		{
			pBot->current_weapon.bIsReloading = false;
		}
		else
		{
			return;
		}
	}

	Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	float MinAcceptableAccuracy = (CurrentWeapon == WEAPON_LERK_SPORES || CurrentWeapon == WEAPON_LERK_UMBRA) ? 0.8f : 0.9f;

	if (AimDot >= MinAcceptableAccuracy)
	{
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) >= pBot->current_weapon.MinRefireTime)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}
	}
}

void BotShootTarget(bot_t* pBot, NSWeapon AttackWeapon, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	NSWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

	pBot->DesiredCombatWeapon = AttackWeapon;

	if (CurrentWeapon != AttackWeapon)
	{
		return;
	}

	if (CurrentWeapon == WEAPON_NONE) { return; }


	if (CurrentWeapon == WEAPON_SKULK_XENOCIDE || CurrentWeapon == WEAPON_LERK_PRIMALSCREAM)
	{
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) >= pBot->current_weapon.MinRefireTime)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}

		return;
	}

	if (AttackWeapon == WEAPON_LERK_SPORES || AttackWeapon == WEAPON_LERK_UMBRA)
	{
		BotLookAt(pBot, Target);
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) < pBot->current_weapon.MinRefireTime)
		{
			return;
		}

		Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);

		TraceResult Hit;
		Vector TraceEnd = pBot->CurrentEyePosition + (AimDir * 3000.0f);

		UTIL_TraceLine(pBot->CurrentEyePosition, TraceEnd, dont_ignore_monsters, dont_ignore_glass, pBot->pEdict->v.pContainingEntity, &Hit);

		if (Hit.flFraction >= 1.0f) { return; }

		if (vDist3DSq(Hit.vecEndPos, Target->v.origin) <= sqrf(kSporeCloudRadius))
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}

		return;
	}

	// For charge and stomp, we can go through stuff so don't need to check for being blocked
	if (CurrentWeapon == WEAPON_ONOS_CHARGE || CurrentWeapon == WEAPON_ONOS_STOMP)
	{
		BotLookAt(pBot, Target);

		Vector DirToTarget = UTIL_GetVectorNormal2D(Target->v.origin - pBot->pEdict->v.origin);
		float DotProduct = UTIL_GetDotProduct2D(UTIL_GetForwardVector(pBot->pEdict->v.v_angle), DirToTarget);

		float MinDotProduct = (CurrentWeapon == WEAPON_ONOS_STOMP) ? 0.95f : 0.75f;

		if (DotProduct >= MinDotProduct)
		{
			if ((gpGlobals->time - pBot->current_weapon.LastFireTime) < pBot->current_weapon.MinRefireTime)
			{
				return;
			}

			if (CurrentWeapon == WEAPON_ONOS_CHARGE)
			{
				pBot->pEdict->v.button |= IN_ATTACK2;
			}
			else
			{
				pBot->pEdict->v.button |= IN_ATTACK;
			}

			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}

		return;
	}

	if (IsMeleeWeapon(CurrentWeapon))
	{
		BotLookAt(pBot, Target);
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) >= pBot->current_weapon.MinRefireTime)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}
		return;
	}

	Vector TargetAimDir = ZERO_VECTOR;

	if (CurrentWeapon == WEAPON_MARINE_GL || CurrentWeapon == WEAPON_MARINE_GRENADE || CurrentWeapon == WEAPON_GORGE_BILEBOMB)
	{
		Vector AimLocation = UTIL_GetCentreOfEntity(Target);

		float ProjectileVelocity = (CurrentWeapon == WEAPON_GORGE_BILEBOMB) ? 750.0f : 800.0f;

		Vector NewAimAngle = GetPitchForProjectile(pBot->CurrentEyePosition, AimLocation, ProjectileVelocity, 640.0f);

		NewAimAngle = UTIL_GetVectorNormal(NewAimAngle);

		AimLocation = pBot->CurrentEyePosition + (NewAimAngle * 200.0f);

		BotLookAt(pBot, AimLocation);
		TargetAimDir = UTIL_GetVectorNormal(AimLocation - pBot->CurrentEyePosition);
	}
	else
	{
		BotLookAt(pBot, Target);
		TargetAimDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - pBot->CurrentEyePosition);
	}

	if (WeaponCanBeReloaded(CurrentWeapon))
	{
		bool bShouldReload = (BotGetCurrentWeaponReserveAmmo(pBot) > 0);

		if (CurrentWeapon == WEAPON_MARINE_SHOTGUN && IsEdictStructure(Target))
		{
			bShouldReload = bShouldReload && ((float)BotGetCurrentWeaponClipAmmo(pBot) / (float)BotGetCurrentWeaponMaxClipAmmo(pBot) < 0.5f);
		}
		else
		{
			bShouldReload = bShouldReload && BotGetCurrentWeaponClipAmmo(pBot) == 0;
		}

		if (bShouldReload)
		{
			if (!pBot->current_weapon.bIsReloading)
			{
				pBot->pEdict->v.button |= IN_RELOAD;
				pBot->current_weapon.bIsReloading = true;
			}
			return;
		}

	}

	if (IsBotReloading(pBot))
	{
		if (IsEdictStructure(Target))
		{
			if (BotGetCurrentWeaponClipAmmo(pBot) == BotGetCurrentWeaponMaxClipAmmo(pBot) || BotGetCurrentWeaponReserveAmmo(pBot) == 0)
			{
				pBot->current_weapon.bIsReloading = false;
			}
			else
			{
				return;
			}
		}
		else
		{
			if (BotGetCurrentWeaponClipAmmo(pBot) > 0)
			{
				pBot->current_weapon.bIsReloading = false;
			}
			else
			{
				return;
			}
		}
	}

	Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	// We can be less accurate with spores and umbra since they have AoE effects
	float MinAcceptableAccuracy = 0.9f;

	if (AimDot >= MinAcceptableAccuracy)
	{
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) >= pBot->current_weapon.MinRefireTime)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}
	}
}

void BotAttackTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	NSWeapon Weapon = WEAPON_NONE;

	if (IsPlayerMarine(pBot->pEdict))
	{
		Weapon = BotMarineChooseBestWeaponForStructure(pBot, Target);
	}
	else
	{
		Weapon = BotAlienChooseBestWeaponForStructure(pBot, Target);
	}

	BotAttackResult AttackResult = PerformAttackLOSCheck(pBot, Weapon, Target);

	float WeaponRange = GetMaxIdealWeaponRange(Weapon);

	NSStructureType StructureType = GetStructureTypeFromEdict(Target);

	if (AttackResult == ATTACK_OUTOFRANGE)
	{
		// Might need to duck if it's an infantry portal
		if (vDist2DSq(pBot->pEdict->v.origin, Target->v.origin) < sqrf(max_player_use_reach))
		{
			pBot->pEdict->v.button |= IN_DUCK;
		}

		if (IsPlayerLerk(pBot->pEdict))
		{
			if (UTIL_ShouldBotBeCautious(pBot))
			{
				MoveTo(pBot, Target->v.origin, MOVESTYLE_HIDE, 100.0f);
			}
			else
			{
				MoveTo(pBot, Target->v.origin, MOVESTYLE_NORMAL, 100.0f);
			}

			return;
		}

		Vector AttackPoint = Target->v.origin;

		if (StructureType == STRUCTURE_ALIEN_HIVE)
		{
			const hive_definition* HiveDefinition = UTIL_GetHiveFromEdict(Target);

			if (HiveDefinition)
			{
				AttackPoint = HiveDefinition->FloorLocation;
			}
		}

		MoveTo(pBot, AttackPoint, MOVESTYLE_NORMAL, WeaponRange);

		if (IsPlayerMarine(pBot->pEdict))
		{
			BotReloadWeapons(pBot);
		}
		
		return;
	}

	if (AttackResult == ATTACK_BLOCKED)
	{
		if (!pBot->BotNavInfo.ActualMoveDestination || UTIL_TraceEntity(pBot->pEdict, pBot->BotNavInfo.ActualMoveDestination + Vector(0.0f, 0.0f, 32.0f), UTIL_GetCentreOfEntity(Target)) != Target)
		{
			Vector NewAttackLocation = ZERO_VECTOR;

			int BotMoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

			if (!pBot->BotNavInfo.ActualMoveDestination)
			{
				NewAttackLocation = FindClosestNavigablePointToDestination(BotMoveProfile, pBot->CurrentFloorPosition, UTIL_GetEntityGroundLocation(Target), WeaponRange);
			}
			else
			{
				NewAttackLocation = UTIL_GetRandomPointOnNavmeshInRadius(BotMoveProfile, pBot->CurrentFloorPosition, 2.0f);

				// Did we find a clear spot we could attack from? If so, make that our new move destination
				if (NewAttackLocation != ZERO_VECTOR && UTIL_TraceEntity(pBot->pEdict, NewAttackLocation + Vector(0.0f, 0.0f, 32.0f), UTIL_GetCentreOfEntity(Target)) == Target)
				{
					MoveTo(pBot, NewAttackLocation, MOVESTYLE_NORMAL);
				}
			}
		}
		else
		{
			MoveTo(pBot, pBot->BotNavInfo.TargetDestination, MOVESTYLE_NORMAL);
		}

		return;
	}

	if (AttackResult == ATTACK_SUCCESS)
	{
		// If we were ducking before then keep ducking
		if (pBot->pEdict->v.oldbuttons & IN_DUCK)
		{
			pBot->pEdict->v.button |= IN_DUCK;
		}

		BotShootTarget(pBot, Weapon, Target);
	}
}

void BotDropWeapon(bot_t* pBot)
{
	pBot->pEdict->v.impulse = IMPULSE_MARINE_DROP_WEAPON;
}

void BotReloadWeapons(bot_t* pBot)
{
	// Aliens and commander don't reload
	if (!IsPlayerMarine(pBot->pEdict)) { return; }

	if (gpGlobals->time - pBot->LastCombatTime > 5.0f)
	{
		NSWeapon PrimaryWeapon = GetBotMarinePrimaryWeapon(pBot);
		NSWeapon SecondaryWeapon = GetBotMarineSecondaryWeapon(pBot);
		NSWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

		if (WeaponCanBeReloaded(PrimaryWeapon) && BotGetPrimaryWeaponClipAmmo(pBot) < BotGetPrimaryWeaponMaxClipSize(pBot) && BotGetPrimaryWeaponAmmoReserve(pBot) > 0)
		{
			pBot->DesiredCombatWeapon = PrimaryWeapon;

			if (CurrentWeapon == PrimaryWeapon)
			{
				pBot->pEdict->v.button |= IN_RELOAD;
				return;
			}
		}

		if (WeaponCanBeReloaded(SecondaryWeapon) && BotGetSecondaryWeaponClipAmmo(pBot) < BotGetSecondaryWeaponMaxClipSize(pBot) && BotGetSecondaryWeaponAmmoReserve(pBot) > 0)
		{
			pBot->DesiredCombatWeapon = SecondaryWeapon;

			if (CurrentWeapon == SecondaryWeapon)
			{
				pBot->pEdict->v.button |= IN_RELOAD;
				return;
			}
		}
	}
}

void BotThrowGrenadeAtTarget(bot_t* pBot, const Vector TargetPoint)
{
	float ProjectileSpeed = 800.0f;
	float ProjectileGravity = 640.0f;

	if (PlayerHasWeapon(pBot->pEdict, WEAPON_MARINE_GL) && (BotGetPrimaryWeaponClipAmmo(pBot) > 0 || BotGetPrimaryWeaponAmmoReserve(pBot) > 0))
	{
		pBot->DesiredCombatWeapon = WEAPON_MARINE_GL;

	}
	else
	{
		pBot->DesiredCombatWeapon = WEAPON_MARINE_GRENADE;
	}

	if (GetBotCurrentWeapon(pBot) != pBot->DesiredCombatWeapon)
	{
		return;
	}

	if (pBot->DesiredCombatWeapon == WEAPON_MARINE_GL)
	{
		// I *think* the grenade launcher projectiles have lower gravity than a thrown grenade, but the same velocity.
		// Lower gravity means the bot has to aim lower as it has a flatter arc. Seems to work in practice anyway...
		ProjectileGravity = 400.0f;
	}

	Vector ThrowAngle = GetPitchForProjectile(pBot->CurrentEyePosition, TargetPoint, ProjectileSpeed, ProjectileGravity);

	ThrowAngle = UTIL_GetVectorNormal(ThrowAngle);

	Vector ThrowTargetLocation = pBot->CurrentEyePosition + (ThrowAngle * 200.0f);

	BotLookAt(pBot, ThrowTargetLocation);

	if (GetBotCurrentWeapon(pBot) == WEAPON_MARINE_GL && BotGetPrimaryWeaponClipAmmo(pBot) == 0)
	{
		pBot->pEdict->v.button |= IN_RELOAD;
		return;
	}

	if ((gpGlobals->time - pBot->current_weapon.LastFireTime) < pBot->current_weapon.MinRefireTime)
	{
		return;
	}

	Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);
	Vector TargetAimDir = UTIL_GetVectorNormal(ThrowTargetLocation - pBot->CurrentEyePosition);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	if (AimDot >= 0.95f)
	{
		pBot->pEdict->v.button |= IN_ATTACK;
		pBot->current_weapon.LastFireTime = gpGlobals->time;
	}
}

bool IsBotReloading(bot_t* pBot)
{
	return pBot->current_weapon.bIsReloading;
}

void BotEvolveLifeform(bot_t* pBot, NSPlayerClass TargetLifeform)
{
	if (TargetLifeform == pBot->bot_ns_class) { return; }

	switch (TargetLifeform)
	{
	case CLASS_SKULK:
		pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_SKULK;
		return;
	case CLASS_GORGE:
		pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_GORGE;
		return;
	case CLASS_LERK:
		pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_LERK;
		return;
	case CLASS_FADE:
		pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_FADE;
		return;
	case CLASS_ONOS:
		pBot->pEdict->v.impulse = IMPULSE_ALIEN_EVOLVE_ONOS;
		return;
	default:
		return;
	}
}

void UTIL_ClearAllBotData(bot_t* pBot)
{
	if (!pBot) { return; }

	UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);
	UTIL_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);
	UTIL_ClearBotTask(pBot, &pBot->MoveTask);

	UTIL_ClearGuardInfo(pBot);

	pBot->CurrentTask = nullptr;

	memset(pBot->CurrentCommanderActions, 0, sizeof(pBot->CurrentCommanderActions));

	memset(pBot->TrackedEnemies, 0, sizeof(pBot->TrackedEnemies));
	pBot->CurrentEnemyRef = nullptr;
	pBot->CurrentEnemy = -1;

	memset(pBot->ChatMessages, 0, sizeof(pBot->ChatMessages));

	pBot->DesiredCombatWeapon = WEAPON_NONE;
	ClearBotPath(pBot);

	pBot->BotNavInfo.bIsJumping = false;
	pBot->BotNavInfo.LandedTime = 0.0f;
	pBot->BotNavInfo.IsOnGround = false;

	memset(&(pBot->current_weapon), 0, sizeof(pBot->current_weapon));
	memset(&(pBot->m_rgAmmo), 0, sizeof(pBot->m_rgAmmo));

	pBot->LastCommanderRequestTime = 0.0f;
	pBot->LastCombatTime = 0.0f;
	pBot->next_commander_action_time = 0.0f;
	pBot->LastTargetTrackUpdate = 0.0f;

	pBot->DesiredLookDirection = ZERO_VECTOR;
	pBot->desiredMovementDir = ZERO_VECTOR;
	pBot->InterpolatedLookDirection = ZERO_VECTOR;
	pBot->LookTarget = nullptr;
	pBot->LookTargetLocation = ZERO_VECTOR;
	pBot->MoveLookLocation = ZERO_VECTOR;

	memset(&pBot->BotSkillSettings, 0, sizeof(bot_skill));

	pBot->CurrentRole = BOT_ROLE_NONE;

	pBot->bot_ns_class = CLASS_NONE;

	pBot->LastUseTime = 0.0f;

	pBot->CommanderLastScanTime = 0.0f;

	pBot->CombatLevel = 1;
	pBot->CombatUpgradeMask = 0;

	if (pBot->logFile)
	{
		fflush(pBot->logFile);
		fclose(pBot->logFile);
	}
}

void BotUpdateViewRotation(bot_t* pBot, float DeltaTime)
{
	if (!vEquals(pBot->DesiredLookDirection, ZERO_VECTOR))
	{
		edict_t* pEdict = pBot->pEdict;

		float Delta = pBot->DesiredLookDirection.y - pBot->InterpolatedLookDirection.y;

		if (Delta > 180.0f)
			Delta -= 360.0f;
		if (Delta < -180.0f)
			Delta += 360.0f;


		pBot->InterpolatedLookDirection.x = fInterpConstantTo(pBot->InterpolatedLookDirection.x, pBot->DesiredLookDirection.x, DeltaTime, (IsPlayerClimbingWall(pEdict) ? 400.0f : pBot->ViewInterpolationSpeed));

		float DeltaInterp = fInterpConstantTo(0.0f, Delta, DeltaTime, pBot->ViewInterpolationSpeed);

		pBot->InterpolatedLookDirection.y += DeltaInterp;

		if (pBot->InterpolatedLookDirection.y > 180.0f)
			pBot->InterpolatedLookDirection.y -= 360.0f;
		if (pBot->InterpolatedLookDirection.y < -180.0f)
			pBot->InterpolatedLookDirection.y += 360.0f;

		if (fNearlyEqual(pBot->InterpolatedLookDirection.x, pBot->DesiredLookDirection.x) && fNearlyEqual(pBot->InterpolatedLookDirection.y, pBot->DesiredLookDirection.y))
		{
			pBot->DesiredLookDirection = ZERO_VECTOR;
		}
		else
		{
			// If the interp gets stuck for some reason then abandon it after 2 seconds. It should have completed way before then anyway
			if (gpGlobals->time - pBot->ViewInterpStartedTime > 2.0f)
			{
				pBot->DesiredLookDirection = ZERO_VECTOR;
			}
		}

		pEdict->v.v_angle.x = pBot->InterpolatedLookDirection.x;
		pEdict->v.v_angle.y = pBot->InterpolatedLookDirection.y;

		// set the body angles to point the gun correctly
		pEdict->v.angles.x = pEdict->v.v_angle.x / 3;
		pEdict->v.angles.y = pEdict->v.v_angle.y;
		pEdict->v.angles.z = 0;

		// adjust the view angle pitch to aim correctly (MUST be after body v.angles stuff)
		pEdict->v.v_angle.x = -pEdict->v.v_angle.x;
		// Paulo-La-Frite - END

		pEdict->v.ideal_yaw = pEdict->v.v_angle.y;

		if (pEdict->v.ideal_yaw > 180)
			pEdict->v.ideal_yaw -= 360;

		if (pEdict->v.ideal_yaw < -180)
			pEdict->v.ideal_yaw += 360;
	}

	if (pBot->bot_ns_class != CLASS_MARINE_COMMANDER && (gpGlobals->time - pBot->LastViewUpdateTime) > pBot->ViewUpdateRate)
	{
		BotUpdateView(pBot);
	}
}

void BotUpdateDesiredViewRotation(bot_t* pBot)
{
	// We always prioritise MoveLookLocation if it is set so the bot doesn't screw up wall climbing or ladder movement
	Vector NewLookLocation = (!vEquals(pBot->MoveLookLocation, ZERO_VECTOR)) ? pBot->MoveLookLocation : pBot->LookTargetLocation;

	// We make an exception for Lerks, they always look where they need to go when flying UNLESS they're aiming at someone/something
	if (IsPlayerLerk(pBot->pEdict))
	{
		NewLookLocation = (!vEquals(pBot->LookTargetLocation, ZERO_VECTOR)) ? pBot->LookTargetLocation : pBot->MoveLookLocation;
	}

	bool bIsMoveLook = !vEquals(pBot->MoveLookLocation, ZERO_VECTOR);

	// We're already interpolating to an existing desired look direction (see BotUpdateViewRotation()) or we don't have a desired look target
	if (!vEquals(pBot->DesiredLookDirection, ZERO_VECTOR) || vEquals(NewLookLocation, ZERO_VECTOR)) { return; }

	edict_t* pEdict = pBot->pEdict;

	Vector dir = UTIL_GetVectorNormal(NewLookLocation - pBot->CurrentEyePosition);

	// Obtain the desired view angles the bot needs to look directly at the target position
	pBot->DesiredLookDirection = UTIL_VecToAngles(dir);

	// Sanity check to make sure we don't end up with NaN values. This causes the bot to start slowly rotating like they're adrift in space
	if (isnan(pBot->DesiredLookDirection.x))
	{
		pBot->DesiredLookDirection = ZERO_VECTOR;
	}

	// Clamp the pitch and yaw to valid ranges

	if (pBot->DesiredLookDirection.y > 180)
		pBot->DesiredLookDirection.y -= 360;

	// Paulo-La-Frite - START bot aiming bug fix
	if (pBot->DesiredLookDirection.y < -180)
		pBot->DesiredLookDirection.y += 360;

	if (pBot->DesiredLookDirection.x > 180)
		pBot->DesiredLookDirection.x -= 360;

	// Now figure out how far we have to turn to reach our desired target
	float yDelta = pBot->DesiredLookDirection.y - pBot->InterpolatedLookDirection.y;
	float xDelta = pBot->DesiredLookDirection.x - pBot->InterpolatedLookDirection.x;

	// This prevents them turning the long way around

	if (yDelta > 180.0f)
		yDelta -= 360.0f;
	if (yDelta < -180.0f)
		yDelta += 360.0f;

	float maxDelta = fmaxf(fabsf(yDelta), fabsf(xDelta));

	float motion_tracking_skill = (IsPlayerMarine(pBot->pEdict)) ? pBot->BotSkillSettings.marine_bot_motion_tracking_skill : pBot->BotSkillSettings.alien_bot_motion_tracking_skill;
	float bot_view_speed = (IsPlayerMarine(pBot->pEdict)) ? pBot->BotSkillSettings.marine_bot_view_speed : pBot->BotSkillSettings.alien_bot_view_speed;
	float bot_aim_skill = (IsPlayerMarine(pBot->pEdict)) ? pBot->BotSkillSettings.marine_bot_aim_skill : pBot->BotSkillSettings.alien_bot_aim_skill;

	// We add a random offset to the view angles based on how far the bot has to move its view
	// This simulates the fact that humans can't spin and lock their cross-hair exactly on the target, the further you have the spin, the more off your view will be first attempt
	if (fabsf(maxDelta) >= 45.0f)
	{
		pBot->ViewInterpolationSpeed = 500.0f;

		if (!bIsMoveLook)
		{
			pBot->ViewInterpolationSpeed *= bot_view_speed;
			float xOffset = frandrange(10.0f, 20.0f);
			xOffset -= xOffset * bot_aim_skill;

			float yOffset = frandrange(10.0f, 20.0f);
			yOffset -= yOffset * bot_aim_skill;

			if (randbool())
			{
				xOffset *= -1.0f;
			}

			if (randbool())
			{
				yOffset *= -1.0f;
			}



			pBot->DesiredLookDirection.x += xOffset;
			pBot->DesiredLookDirection.y += yOffset;
		}
	}
	else if (fabsf(maxDelta) >= 25.0f)
	{
		pBot->ViewInterpolationSpeed = 250.0f;

		if (!bIsMoveLook)
		{
			pBot->ViewInterpolationSpeed *= bot_view_speed;
			float xOffset = frandrange(5.0f, 10.0f);
			xOffset -= xOffset * bot_aim_skill;

			float yOffset = frandrange(5.0f, 10.0f);
			yOffset -= yOffset * bot_aim_skill;

			if (randbool())
			{
				xOffset *= -1.0f;
			}

			if (randbool())
			{
				yOffset *= -1.0f;
			}

			pBot->DesiredLookDirection.x += xOffset;
			pBot->DesiredLookDirection.y += yOffset;
		}
	}
	else if (fabsf(maxDelta) >= 5.0f)
	{
		pBot->ViewInterpolationSpeed = 50.0f;

		if (!bIsMoveLook)
		{
			pBot->ViewInterpolationSpeed *= bot_view_speed;
			float xOffset = frandrange(2.0f, 5.0f);
			xOffset -= xOffset * bot_aim_skill;

			float yOffset = frandrange(2.0f, 5.0f);
			yOffset -= yOffset * bot_aim_skill;

			if (randbool())
			{
				xOffset *= -1.0f;
			}

			if (randbool())
			{
				yOffset *= -1.0f;
			}

			pBot->DesiredLookDirection.x += xOffset;
			pBot->DesiredLookDirection.y += yOffset;
		}
	}
	else
	{
		pBot->ViewInterpolationSpeed = 50.0f * bot_view_speed;

		
	}

	if (IsPlayerLerk(pBot->pEdict))
	{
		pBot->ViewInterpolationSpeed *= 2.0f;
	}


	// We once again clamp everything to valid values in case the offsets we applied above took us above that

	if (pBot->DesiredLookDirection.y > 180)
		pBot->DesiredLookDirection.y -= 360;

	// Paulo-La-Frite - START bot aiming bug fix
	if (pBot->DesiredLookDirection.y < -180)
		pBot->DesiredLookDirection.y += 360;

	if (pBot->DesiredLookDirection.x > 180)
		pBot->DesiredLookDirection.x -= 360;

	// We finally have our desired turn movement, ready for BotUpdateViewRotation() to pick up and make happen
	pBot->ViewInterpStartedTime = gpGlobals->time;
}

byte BotThrottledMsec(bot_t* pBot)
{

	// Thanks to The Storm (ePODBot) for this one, finally fixed the bot running speed!
	int newmsec = (int)((gpGlobals->time - pBot->f_previous_command_time) * 1000);
	if (newmsec > 255)  // Doh, bots are going to be slower than they should if this happens.
		newmsec = 255;		 // Upgrade that CPU or use fewer bots!

	return (byte)newmsec;
}

void BotUpdateView(bot_t* pBot)
{
	int visibleCount = 0;

	// Updates the view frustum based on the bot's position and v_angle
	BotUpdateViewFrustum(pBot);

	bool bHasLOSToAnyEnemy = false;

	int EnemyTeam = 0;

	// Update list of currently visible players
	for (int i = 0; i < 32; i++)
	{
		pBot->TrackedEnemies[i].EnemyEdict = clients[i];
		if (FNullEnt(clients[i]) || !IsPlayerActiveInGame(clients[i]) || clients[i]->v.team == pBot->pEdict->v.team)
		{
			BotClearEnemyTrackingInfo(&pBot->TrackedEnemies[i]);
			continue;
		}

		enemy_status* TrackingInfo = &pBot->TrackedEnemies[i];

		if (gpGlobals->time < TrackingInfo->NextUpdateTime)
		{
			continue;
		}

		edict_t* Enemy = clients[i];

		bool bInFOV = IsPlayerInBotFOV(pBot, Enemy);
		bool bHasLOS = DoesBotHaveLOSToPlayer(pBot, Enemy);

		if (pBot->LastSafeLocation != ZERO_VECTOR && UTIL_PlayerHasLOSToLocation(Enemy, pBot->LastSafeLocation, UTIL_MetresToGoldSrcUnits(50.0f)))
		{
			pBot->LastSafeLocation = ZERO_VECTOR;
		}

		if (bHasLOS)
		{
			bHasLOSToAnyEnemy = true;
		}

		float bot_reaction_time = (IsPlayerMarine(pBot->pEdict)) ? pBot->BotSkillSettings.marine_bot_reaction_time : pBot->BotSkillSettings.alien_bot_reaction_time;

		if (bInFOV != TrackingInfo->bInFOV || bHasLOS != TrackingInfo->bHasLOS)
		{
			TrackingInfo->bInFOV = bInFOV;
			TrackingInfo->bHasLOS = bHasLOS;

			TrackingInfo->NextUpdateTime = gpGlobals->time + bot_reaction_time;
			continue;
		}

		bool bIsTracked = (!bHasLOS && (IsPlayerParasited(Enemy) || IsPlayerMotionTracked(Enemy)));

		if (bInFOV && (bHasLOS || bIsTracked))
		{
			Vector BotLocation = UTIL_GetFloorUnderEntity(Enemy);
			Vector BotVelocity = Enemy->v.velocity;

			if (gpGlobals->time >= TrackingInfo->NextVelocityUpdateTime)
			{
				TrackingInfo->LastSeenVelocity = TrackingInfo->PendingSeenVelocity;
			}

			if (BotVelocity != TrackingInfo->LastSeenVelocity)
			{
				TrackingInfo->PendingSeenVelocity = BotVelocity;
				TrackingInfo->NextVelocityUpdateTime = gpGlobals->time + bot_reaction_time;
			}

			TrackingInfo->bIsAwareOfPlayer = true;
			TrackingInfo->LastSeenLocation = BotLocation;

			if (bHasLOS)
			{
				TrackingInfo->LastLOSPosition = pBot->CurrentFloorPosition + Vector(0.0f, 0.0f, 5.0f);
				TrackingInfo->LastSeenTime = gpGlobals->time;

				if (vDist2DSq(pBot->pEdict->v.origin, TrackingInfo->LastHiddenPosition) < sqrf(18.0f))
				{
					TrackingInfo->LastHiddenPosition = ZERO_VECTOR;
				}
				
			}
			else
			{
				TrackingInfo->LastHiddenPosition = pBot->CurrentFloorPosition + Vector(0.0f, 0.0f, 5.0f);
				TrackingInfo->LastTrackedTime = gpGlobals->time;
			}

			continue;
		}

		if (bHasLOS)
		{
			TrackingInfo->LastLOSPosition = pBot->CurrentFloorPosition + Vector(0.0f, 0.0f, 5.0f);

			if (TrackingInfo->LastHiddenPosition != ZERO_VECTOR && UTIL_QuickTrace(pBot->pEdict, TrackingInfo->LastHiddenPosition, Enemy->v.origin))
			{
				TrackingInfo->LastHiddenPosition = ZERO_VECTOR;
			}
		}
		else
		{
			TrackingInfo->LastHiddenPosition = pBot->pEdict->v.origin;

			if (TrackingInfo->LastLOSPosition != ZERO_VECTOR && !UTIL_QuickTrace(pBot->pEdict, TrackingInfo->LastLOSPosition, Enemy->v.origin))
			{
				TrackingInfo->LastLOSPosition = ZERO_VECTOR;
			}

		}

		// If we've not been aware of the enemy's location for over 10 seconds, forget about them
		float LastDetectedTime = fmaxf(TrackingInfo->LastSeenTime, TrackingInfo->LastTrackedTime);

		if ((gpGlobals->time - LastDetectedTime) > 10.0f)
		{
			BotClearEnemyTrackingInfo(TrackingInfo);
			continue;
		}
	}

	if (!bHasLOSToAnyEnemy)
	{
		pBot->LastSafeLocation = pBot->pEdict->v.origin;
	}
}

bool DoesBotHaveLOSToPlayer(bot_t* Observer, edict_t* TargetPlayer)
{
	TraceResult hit;
	UTIL_TraceLine(GetPlayerEyePosition(Observer->pEdict), TargetPlayer->v.origin, ignore_monsters, ignore_glass, Observer->pEdict->v.pContainingEntity, &hit);

	if (hit.flFraction >= 1.0f) { return true; }

	UTIL_TraceLine(GetPlayerEyePosition(Observer->pEdict), GetPlayerEyePosition(TargetPlayer), ignore_monsters, ignore_glass, Observer->pEdict->v.pContainingEntity, &hit);

	if (hit.flFraction >= 1.0f) { return true; }

	UTIL_TraceLine(GetPlayerEyePosition(Observer->pEdict), GetPlayerBottomOfCollisionHull(TargetPlayer) + Vector(0.0f, 0.0f, 5.0f), ignore_monsters, ignore_glass, Observer->pEdict->v.pContainingEntity, &hit);

	return (hit.flFraction >= 1.0f);
}

bool IsPlayerInBotFOV(bot_t* Observer, edict_t* TargetPlayer)
{
	if (FNullEnt(TargetPlayer) || !IsPlayerActiveInGame(TargetPlayer)) { return false; }
	// To make things a little more accurate, we're going to treat players as cylinders rather than boxes
	for (int i = 0; i < 6; i++)
	{
		// Our cylinder must be inside all planes to be visible, otherwise return false
		if (!UTIL_CylinderInsidePlane(&Observer->viewFrustum[i], TargetPlayer->v.origin - Vector(0, 0, 5), 60.0f, 16.0f))
		{
			return false;
		}
	}

	return true;
}

// Checks to see if pBot can see player. Returns true if player is visible
bool IsPlayerVisibleToBot(bot_t* Observer, edict_t* TargetPlayer)
{
	if (FNullEnt(TargetPlayer) || !IsPlayerActiveInGame(TargetPlayer) || GetPlayerCloakAmount(TargetPlayer) > 0.7f) { return false; }
	// To make things a little more accurate, we're going to treat players as cylinders rather than boxes
	for (int i = 0; i < 6; i++)
	{
		// Our cylinder must be inside all planes to be visible, otherwise return false
		if (!UTIL_CylinderInsidePlane(&Observer->viewFrustum[i], TargetPlayer->v.origin - Vector(0, 0, 5), 60.0f, 16.0f))
		{
			return false;
		}
	}

	// TODO: Think of a better way than simply checking to see if bot has line of sight with origin.
	//       Probably should just check the head, middle and feet.

	TraceResult hit;
	UTIL_TraceLine(GetPlayerEyePosition(Observer->pEdict), TargetPlayer->v.origin, ignore_monsters, ignore_glass, Observer->pEdict->v.pContainingEntity, &hit);

	return hit.flFraction >= 1.0f;
}

void BotClearEnemyTrackingInfo(enemy_status* TrackingInfo)
{
	TrackingInfo->bInFOV = false;
	TrackingInfo->bHasLOS = false;
	TrackingInfo->LastSeenLocation = ZERO_VECTOR;
	TrackingInfo->LastSeenVelocity = ZERO_VECTOR;
	TrackingInfo->bIsAwareOfPlayer = false;
	TrackingInfo->LastSeenTime = 0.0f;
	TrackingInfo->LastLOSPosition = ZERO_VECTOR;
	TrackingInfo->LastHiddenPosition = ZERO_VECTOR;
}

void StartNewBotFrame(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	// NS specifically blocks fake clients from using the command console, so we need to switch off that flag if the bot is commanding
	if (!IsPlayerCommander(pBot->pEdict) && pBot->CurrentRole != BOT_ROLE_COMMAND)
	{
		pEdict->v.flags |= FL_FAKECLIENT;
	}
	else
	{
		pEdict->v.flags &= ~FL_FAKECLIENT;
	}
	
	pEdict->v.button = 0;
	pBot->ForwardMove = 0.0f;
	pBot->SideMove = 0.0f;
	pBot->UpMove = 0.0f;
	pBot->impulse = 0;
	pBot->resources = GetPlayerResources(pBot->pEdict);
	pBot->CurrentEyePosition = GetPlayerEyePosition(pEdict);
	pBot->CurrentFloorPosition = UTIL_GetEntityGroundLocation(pEdict);
	pBot->LookTargetLocation = ZERO_VECTOR;
	pBot->MoveLookLocation = ZERO_VECTOR;
	pBot->LookTarget = nullptr;

	pBot->DesiredCombatWeapon = WEAPON_NONE;
	pBot->DesiredMoveWeapon = WEAPON_NONE;

	pBot->bot_ns_class =GetPlayerClass(pBot->pEdict);

	if (IsPlayerOnAlienTeam(pEdict))
	{
		pBot->Adrenaline = pEdict->v.fuser3 * 0.001f;
	}

	if (IsPlayerSkulk(pEdict))
	{
		pEdict->v.button |= IN_DUCK;
	}

	if ((pEdict->v.flags & FL_ONGROUND) || IsPlayerOnLadder(pEdict))
	{
		if (!pBot->BotNavInfo.IsOnGround || pBot->BotNavInfo.bHasAttemptedJump)
		{
			pBot->BotNavInfo.LandedTime = gpGlobals->time;
		}

		pBot->BotNavInfo.IsOnGround = true;
		pBot->BotNavInfo.bIsJumping = false;
	}
	else
	{
		pBot->BotNavInfo.IsOnGround = false;

	}

	pBot->BotNavInfo.bHasAttemptedJump = false;
}

void BotThink(bot_t* pBot)
{
	StartNewBotFrame(pBot);

	if (pBot->bBotThinkPaused)
	{
		BotRestartPlay(pBot);
	}

	if (IsPlayerGestating(pBot->pEdict)) { return; }

	if (IsPlayerInReadyRoom(pBot->pEdict))
	{
		ReadyRoomThink(pBot);
		
		return;
	}

	switch (GAME_GetDebugMode())
	{
	case EVO_DEBUG_TESTNAV:
		TestNavThink(pBot);
		break;
	case EVO_DEBUG_DRONE:
		DroneThink(pBot);
		break;
	case EVO_DEBUG_AIM:
		TestAimThink(pBot);
		break;
	case EVO_DEBUG_GUARD:
		TestGuardThink(pBot);
		break;
	case EVO_DEBUG_CUSTOM:
		CustomThink(pBot);
		break;
	default:
	{
		if (!bGameIsActive)
		{
			WaitGameStartThink(pBot);
		}
		else
		{
			NSGameMode CurrentGameMode = GAME_GetGameMode();

			if (CurrentGameMode == GAME_MODE_REGULAR)
			{
				RegularModeThink(pBot);
			}
			else if (CurrentGameMode == GAME_MODE_COMBAT)
			{
				CombatModeThink(pBot);
			}
			else
			{
				InvalidModeThink(pBot);
			}
		}
	}
		
	break;
	}

	BotUpdateDesiredViewRotation(pBot);

	NSWeapon DesiredWeapon = (pBot->DesiredMoveWeapon != WEAPON_NONE) ? pBot->DesiredMoveWeapon : pBot->DesiredCombatWeapon;

	if (DesiredWeapon != WEAPON_NONE && GetBotCurrentWeapon(pBot) != DesiredWeapon)
	{
		BotSwitchToWeapon(pBot, DesiredWeapon);
	}
}

void RegularModeThink(bot_t* pBot)
{

	if (IsPlayerCommander(pBot->pEdict))
	{
		CommanderThink(pBot);
		return;
	}

	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);
	
	if (pBot->CurrentEnemy > -1)
	{
		pBot->LastCombatTime = gpGlobals->time;
	}

	if (IsPlayerMarine(pBot->pEdict))
	{
		MarineThink(pBot);
	}
	else
	{
		AlienThink(pBot);
	}
}

void CombatModeThink(bot_t* pBot)
{
	pBot->CombatLevel = GetPlayerCombatLevel(pBot->pEdict);

	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

	if (pBot->CurrentEnemy > -1)
	{
		pBot->LastCombatTime = gpGlobals->time;
	}

	if (IsPlayerMarine(pBot->pEdict))
	{
		MarineCombatModeThink(pBot);
	}
	else
	{
		AlienCombatModeThink(pBot);
	}
}

void InvalidModeThink(bot_t* pBot)
{
	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

	if (pBot->CurrentEnemy > -1)
	{
		pBot->LastCombatTime = gpGlobals->time;

		if (IsPlayerMarine(pBot->pEdict))
		{
			MarineCombatThink(pBot);
			return;
		}
		else
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

	BotUpdateAndClearTasks(pBot);

	if (pBot->PrimaryBotTask.TaskType != TASK_MOVE)
	{
		Vector NewLocation = UTIL_GetRandomPointOnNavmesh(pBot);

		if (NewLocation != ZERO_VECTOR)
		{
			TASK_SetMoveTask(pBot, &pBot->PrimaryBotTask, NewLocation, false);
		}
	}

	BotProgressTask(pBot, &pBot->PrimaryBotTask);

}

void ReadyRoomThink(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	if (pBot->bot_team == 1)
		FakeClientCommand(pEdict, "jointeamone", NULL, NULL);
	else if (pBot->bot_team == 2)
		FakeClientCommand(pEdict, "jointeamtwo", NULL, NULL);
	else
		FakeClientCommand(pEdict, "autoassign", NULL, NULL);

	return;
}

void WaitGameStartThink(bot_t* pBot)
{
	Vector NewGuardLocation = pBot->GuardInfo.GuardLocation;

	if (NewGuardLocation == ZERO_VECTOR || vDist2DSq(NewGuardLocation, pBot->pEdict->v.origin) > sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		NewGuardLocation = pBot->pEdict->v.origin;
	}

	BotGuardLocation(pBot, NewGuardLocation);
}

int BotGetNextEnemyTarget(bot_t* pBot)
{
	int NearestVisibleEnemy = -1;
	int NearestUnseenEnemy = -1;

	float ClosestVisibleDist = 0.0f;
	float ClosestUnseenDist = 0.0f;

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (!pBot->TrackedEnemies[i].bIsAwareOfPlayer) { continue; }

		enemy_status* TrackingInfo = &pBot->TrackedEnemies[i];

		float Dist = vDist2DSq(TrackingInfo->LastSeenLocation, pBot->pEdict->v.origin);

		if (TrackingInfo->bHasLOS)
		{
			if (NearestVisibleEnemy < 0 || Dist < ClosestVisibleDist)
			{
				NearestVisibleEnemy = i;
				ClosestVisibleDist = Dist;
			}
		}
		else
		{
			// Ignore tracked enemies if we've not seen them in a while and we have something important to do
			if (pBot->CurrentTask && (pBot->CurrentTask->bTaskIsUrgent || pBot->CurrentTask->bIssuedByCommander))
			{
				if ((gpGlobals->time - TrackingInfo->LastSeenTime) > 5.0f) { continue; }
			}

			if (Dist > sqrf(UTIL_MetresToGoldSrcUnits(15.0f)) && (gpGlobals->time - TrackingInfo->LastSeenTime) > 10.0f)
			{
				continue;
			}

			if (NearestUnseenEnemy < 0 || Dist < ClosestUnseenDist)
			{
				NearestUnseenEnemy = i;
				ClosestUnseenDist = Dist;
			}
		}
	}

	return (NearestVisibleEnemy > -1) ? NearestVisibleEnemy : NearestUnseenEnemy;

}


void DroneThink(bot_t* pBot)
{
	BotUpdateAndClearTasks(pBot);

	pBot->CurrentTask = BotGetNextTask(pBot);

	if (pBot->CurrentTask->TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, pBot->CurrentTask);
	}

	//DEBUG_DrawPath(pBot->BotNavInfo.CurrentPath, pBot->BotNavInfo.PathSize, 0.0f);
}

void CustomThink(bot_t* pBot)
{
	if (!IsPlayerAlien(pBot->pEdict)) { return; }

	pBot->CurrentRole = BOT_ROLE_BUILDER;

	if (!pBot->CurrentTask) { pBot->CurrentTask = &pBot->PrimaryBotTask; }

	BotUpdateAndClearTasks(pBot);

	if (pBot->PrimaryBotTask.TaskType == TASK_NONE || !pBot->PrimaryBotTask.bTaskIsUrgent)
	{
		BotAlienSetPrimaryTask(pBot, &pBot->PrimaryBotTask);
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

void TestAimThink(bot_t* pBot)
{
	if (!bGameIsActive)
	{
		WaitGameStartThink(pBot);
		return;
	}

	pBot->CurrentEnemy = BotGetNextEnemyTarget(pBot);

	if (pBot->CurrentEnemy > -1)
	{
		edict_t* CurrentEnemy = pBot->TrackedEnemies[pBot->CurrentEnemy].EnemyEdict;

		BotAttackTarget(pBot, CurrentEnemy);

		return;
	}

}

void TestGuardThink(bot_t* pBot)
{
	if (!bGameIsActive)
	{
		WaitGameStartThink(pBot);
		return;
	}

	BotUpdateAndClearTasks(pBot);

	BotGuardLocation(pBot, pBot->pEdict->v.origin);
}

void TestNavThink(bot_t* pBot)
{
	if (!bGameIsActive)
	{
		WaitGameStartThink(pBot);
		return;
	}

	BotUpdateAndClearTasks(pBot);

	pBot->CurrentTask = &pBot->PrimaryBotTask;

	if (pBot->MoveTask.TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, &pBot->MoveTask);
		return;
	}

	if (pBot->PrimaryBotTask.TaskType == TASK_MOVE)
	{
		if (vDist2DSq(pBot->pEdict->v.origin, pBot->PrimaryBotTask.TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			return;
		}

		BotProgressTask(pBot, &pBot->PrimaryBotTask);
		//BotDrawPath(pBot, 0.0f, true);
	}
	else
	{
		int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

		Vector RandomPoint = UTIL_GetRandomPointOfInterest();

		if (RandomPoint != ZERO_VECTOR && UTIL_PointIsReachable(MoveProfile, pBot->pEdict->v.origin, RandomPoint, max_player_use_reach))
		{
			TASK_SetMoveTask(pBot, &pBot->PrimaryBotTask, RandomPoint, true);
		}
		else
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
		}
	}
}

void OnBotBeginGestation(bot_t* pBot)
{
	// If the game mode is combat and we previously input a request to obtain an upgrade, then confirm it was successful since we're now gestating as expected
	if (GAME_GetGameMode() == GAME_MODE_COMBAT)
	{
		if (pBot->BotNextCombatUpgrade > 0)
		{
			pBot->CombatUpgradeMask |= pBot->BotNextCombatUpgrade;
			pBot->BotNextCombatUpgrade = COMBAT_ALIEN_UPGRADE_NONE;
			return;
		}
	}


}

bool ShouldBotThink(const bot_t* bot)
{
	edict_t* pEdict = bot->pEdict;
	return (!IsPlayerSpectator(pEdict) && !IsPlayerDead(pEdict) && !IsPlayerBeingDigested(pEdict) && !IsPlayerGestating(pEdict));
}

void BotRestartPlay(bot_t* pBot)
{
	ClearBotPath(pBot);
	pBot->bBotThinkPaused = false;
}

char* UTIL_WeaponTypeToClassname(const NSWeapon WeaponType)
{
	switch (WeaponType)
	{
	case WEAPON_MARINE_MG:
		return "weapon_machinegun";
	case WEAPON_MARINE_PISTOL:
		return "weapon_pistol";
	case WEAPON_MARINE_KNIFE:
		return "weapon_knife";
	case WEAPON_MARINE_SHOTGUN:
		return "weapon_shotgun";
	case WEAPON_MARINE_HMG:
		return "weapon_heavymachinegun";
	case WEAPON_MARINE_WELDER:
		return "weapon_welder";
	case WEAPON_MARINE_MINES:
		return "weapon_mine";
	case WEAPON_MARINE_GRENADE:
		return "weapon_grenade";
	case WEAPON_MARINE_GL:
		return "weapon_grenadegun";

	case WEAPON_SKULK_BITE:
		return "weapon_bitegun";
	case WEAPON_SKULK_PARASITE:
		return "weapon_parasite";
	case WEAPON_SKULK_LEAP:
		return "weapon_leap";
	case WEAPON_SKULK_XENOCIDE:
		return "weapon_divinewind";

	case WEAPON_GORGE_SPIT:
		return "weapon_spit";
	case WEAPON_GORGE_HEALINGSPRAY:
		return "weapon_healingspray";
	case WEAPON_GORGE_BILEBOMB:
		return "weapon_bilebombgun";
	case WEAPON_GORGE_WEB:
		return "weapon_webspinner";

	case WEAPON_LERK_BITE:
		return "weapon_bite2gun";
	case WEAPON_LERK_SPORES:
		return "weapon_spore";
	case WEAPON_LERK_UMBRA:
		return "weapon_umbra";
	case WEAPON_LERK_PRIMALSCREAM:
		return "weapon_primalscream";

	case WEAPON_FADE_SWIPE:
		return "weapon_swipe";
	case WEAPON_FADE_BLINK:
		return "weapon_blink";
	case WEAPON_FADE_METABOLIZE:
		return "weapon_metabolize";
	case WEAPON_FADE_ACIDROCKET:
		return "weapon_acidrocketgun";

	case WEAPON_ONOS_GORE:
		return "weapon_claws";
	case WEAPON_ONOS_DEVOUR:
		return "weapon_devour";
	case WEAPON_ONOS_STOMP:
		return "weapon_stomp";
	case WEAPON_ONOS_CHARGE:
		return "weapon_charge";
	default:
		return "";
	}

	return "";
}

void BotSwitchToWeapon(bot_t* pBot, NSWeapon NewWeaponSlot)
{
	pBot->current_weapon.bIsReloading = false;

	char* WeaponName = UTIL_WeaponTypeToClassname(NewWeaponSlot);

	FakeClientCommand(pBot->pEdict, WeaponName, NULL, NULL);
}

void FakeClientCommand(edict_t* pBot, const char* arg1, const char* arg2, const char* arg3)
{
	int length;

	memset(g_argv, 0, 1024);

	isFakeClientCommand = true;

	if ((arg1 == NULL) || (*arg1 == 0))
		return;

	if ((arg2 == NULL) || (*arg2 == 0))
	{
		length = sprintf(&g_argv[0], "%s", arg1);
		fake_arg_count = 1;
	}
	else if ((arg3 == NULL) || (*arg3 == 0))
	{

		length = sprintf(&g_argv[0], "%s %s", arg1, arg2);
		fake_arg_count = 2;
	}
	else
	{
		length = sprintf(&g_argv[0], "%s %s %s", arg1, arg2, arg3);
		fake_arg_count = 3;
	}

	g_argv[length] = 0;  // null terminate just in case

	strcpy(&g_argv[64], arg1);

	if (arg2)
		strcpy(&g_argv[128], arg2);

	if (arg3)
		strcpy(&g_argv[192], arg3);

	// allow the MOD DLL to execute the ClientCommand...
	MDLL_ClientCommand(pBot);

	isFakeClientCommand = false;
}

void DEBUG_BotMeleeTarget(bot_t* pBot, edict_t* Target)
{
	NSWeapon DesiredWeapon = (IsPlayerMarine(pBot->pEdict)) ? WEAPON_MARINE_KNIFE : GetBotAlienPrimaryWeapon(pBot);

	float range = GetMaxIdealWeaponRange(DesiredWeapon);

	float Dist = vDist2DSq(pBot->pEdict->v.origin, Target->v.origin);

	if (Dist < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)))
	{
		BotLookAt(pBot, Target);

		edict_t* HitTarget = UTIL_TraceEntity(pBot->pEdict, pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(Target));

		if (HitTarget == Target || (IsEdictPlayer(HitTarget) && Dist < vDist2DSq(HitTarget->v.origin, Target->v.origin)))
		{
			TraceResult hit;
			UTIL_TraceLine(pBot->CurrentEyePosition, UTIL_GetCentreOfEntity(Target), dont_ignore_monsters, ignore_glass, pBot->pEdict->v.pContainingEntity, &hit);

			Vector hitLoc = hit.vecEndPos;

			float MeleeDist = vDist2DSq(pBot->CurrentEyePosition, hitLoc);

			if (MeleeDist <= sqrf(range * 0.95f))
			{
				pBot->DesiredCombatWeapon = DesiredWeapon;

				if (GetBotCurrentWeapon(pBot) != DesiredWeapon) { return; }

				pBot->pEdict->v.button |= IN_ATTACK;

				if (IsEdictPlayer(Target))
				{
					Vector TargetLocation = UTIL_GetFloorUnderEntity(Target);
					Vector BehindPlayer = TargetLocation - (UTIL_GetForwardVector2D(Target->v.v_angle) * 50.0f);

					int NavProfileIndex = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

					if (UTIL_PointIsReachable(NavProfileIndex, pBot->CurrentFloorPosition, BehindPlayer, 18.0f))
					{
						MoveTo(pBot, BehindPlayer, MOVESTYLE_NORMAL);
					}
				}
			}
			else
			{
				MoveTo(pBot, Target->v.origin, MOVESTYLE_NORMAL);
			}
		}
		else
		{
			Vector TargetLocation = UTIL_GetFloorUnderEntity(Target);
			Vector BehindTarget = TargetLocation + (UTIL_GetVectorNormal2D(Target->v.origin - pBot->pEdict->v.origin) * 50.0f);

			UTIL_DrawLine(clients[0], pBot->pEdict->v.origin, BehindTarget, 255, 0, 0);

			MoveTo(pBot, BehindTarget, MOVESTYLE_NORMAL);
		}
	}
	else
	{
		MoveTo(pBot, Target->v.origin, MOVESTYLE_NORMAL);
	}
}

int GetMarineCombatUpgradeCost(const CombatModeMarineUpgrade Upgrade)
{
	switch (Upgrade)
	{
		case COMBAT_MARINE_UPGRADE_HEAVYARMOUR:
		case COMBAT_MARINE_UPGRADE_JETPACK:
			return 2;
		default:
			return 1;
	}

	return 1;
}

int GetAlienCombatUpgradeCost(const CombatModeAlienUpgrade Upgrade)
{
	switch (Upgrade)
	{
	case COMBAT_ALIEN_UPGRADE_ONOS:
		return 4;
	case COMBAT_ALIEN_UPGRADE_FADE:
		return 3;
	case COMBAT_ALIEN_UPGRADE_LERK:
	case COMBAT_ALIEN_UPGRADE_FOCUS:
		return 2;
	default:
		return 1;
	}

	return 1;
}

int GetBotSpentCombatPoints(bot_t* pBot)
{
	// First count all the upgrades the bot has so far
	int NumUpgrades = UTIL_CountSetBitsInInteger(pBot->CombatUpgradeMask);
	int NumSpentPoints = 0;

	// Remove any flags for fade or onos as these are hacks to indicate what they WANT to evolve to, not necessarily what they HAVE evolved to
	// Also, focus uses 2 points so take into consideration
	if (IsPlayerAlien(pBot->pEdict))
	{
		if (pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_FADE)
		{
			NumUpgrades--;
		}

		if (pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_ONOS)
		{
			NumUpgrades--;
		}

		if (pBot->CombatUpgradeMask & COMBAT_ALIEN_UPGRADE_FOCUS)
		{
			NumSpentPoints += 2;
			NumUpgrades--;
		}
	}

	// Now remove points based on if they've already evolved. Onos requires 4 points, so if they are Onos then they must have spent 4 points etc.
	if (IsPlayerOnos(pBot->pEdict))
	{
		NumSpentPoints += 4;
	}

	if (IsPlayerFade(pBot->pEdict))
	{
		NumSpentPoints += 3;
	}

	if (IsPlayerLerk(pBot->pEdict))
	{
		NumSpentPoints += 2;
	}

	if (IsPlayerGorge(pBot->pEdict))
	{
		NumSpentPoints += 1;
	}

	// Same goes for if the bot is a marine player and has jetpack or heavy armour
	if (PlayerHasEquipment(pBot->pEdict))
	{
		NumSpentPoints += 2;
		NumUpgrades--;
	}

	// All other upgrades cost 1 point, so whatever remaining upgrades they have must all have cost one point each
	NumSpentPoints += NumUpgrades;

	// Return the total amount of points they've spent!
	return NumSpentPoints;
}

int GetBotAvailableCombatPoints(bot_t* pBot)
{
	return (pBot->CombatLevel - 1) - GetBotSpentCombatPoints(pBot);
}

int GetImpulseForMarineCombatUpgrade(const CombatModeMarineUpgrade Upgrade)
{
	switch (Upgrade)
	{
		case COMBAT_MARINE_UPGRADE_ARMOUR1:
			return RESEARCH_ARMSLAB_ARMOUR1;
		case COMBAT_MARINE_UPGRADE_ARMOUR2:
			return RESEARCH_ARMSLAB_ARMOUR2;
		case COMBAT_MARINE_UPGRADE_ARMOUR3:
			return RESEARCH_ARMSLAB_ARMOUR3;
		case COMBAT_MARINE_UPGRADE_DAMAGE1:
			return RESEARCH_ARMSLAB_WEAPONS1;
		case COMBAT_MARINE_UPGRADE_DAMAGE2:
			return RESEARCH_ARMSLAB_WEAPONS2;
		case COMBAT_MARINE_UPGRADE_DAMAGE3:
			return RESEARCH_ARMSLAB_WEAPONS3;
		case COMBAT_MARINE_UPGRADE_CATALYST:
			return ITEM_MARINE_CATALYSTS;
		case COMBAT_MARINE_UPGRADE_GRENADE:
			return RESEARCH_ARMOURY_GRENADES;
		case COMBAT_MARINE_UPGRADE_GRENADELAUNCHER:
			return ITEM_MARINE_GRENADELAUNCHER;
		case COMBAT_MARINE_UPGRADE_HEAVYARMOUR:
			return ITEM_MARINE_HEAVYARMOUR;
		case COMBAT_MARINE_UPGRADE_HMG:
			return ITEM_MARINE_HMG;
		case COMBAT_MARINE_UPGRADE_JETPACK:
			return ITEM_MARINE_JETPACK;
		case COMBAT_MARINE_UPGRADE_MINES:
			return ITEM_MARINE_MINES;
		case COMBAT_MARINE_UPGRADE_MOTIONTRACKING:
			return RESEARCH_OBSERVATORY_MOTIONTRACKING;
		case COMBAT_MARINE_UPGRADE_RESUPPLY:
			return ITEM_MARINE_RESUPPLY;
		case COMBAT_MARINE_UPGRADE_SCAN:
			return ITEM_MARINE_SCAN;
		case COMBAT_MARINE_UPGRADE_SHOTGUN:
			return ITEM_MARINE_SHOTGUN;
		case COMBAT_MARINE_UPGRADE_WELDER:
			return ITEM_MARINE_WELDER;
		default:
			return 0;
	}
}

int GetImpulseForAlienCombatUpgrade(const CombatModeAlienUpgrade Upgrade)
{
	switch (Upgrade)
	{
	case COMBAT_ALIEN_UPGRADE_CARAPACE:
		return IMPULSE_ALIEN_UPGRADE_CARAPACE;
	case COMBAT_ALIEN_UPGRADE_REGENERATION:
		return IMPULSE_ALIEN_UPGRADE_REGENERATION;
	case COMBAT_ALIEN_UPGRADE_REDEMPTION:
		return IMPULSE_ALIEN_UPGRADE_REDEMPTION;
	case COMBAT_ALIEN_UPGRADE_ADRENALINE:
		return IMPULSE_ALIEN_UPGRADE_ADRENALINE;
	case COMBAT_ALIEN_UPGRADE_CELERITY:
		return IMPULSE_ALIEN_UPGRADE_CELERITY;
	case COMBAT_ALIEN_UPGRADE_SILENCE:
		return IMPULSE_ALIEN_UPGRADE_SILENCE;
	case COMBAT_ALIEN_UPGRADE_FOCUS:
		return IMPULSE_ALIEN_UPGRADE_FOCUS;
	case COMBAT_ALIEN_UPGRADE_CLOAKING:
		return IMPULSE_ALIEN_UPGRADE_CLOAK;
	case COMBAT_ALIEN_UPGRADE_SCENTOFFEAR:
		return IMPULSE_ALIEN_UPGRADE_SCENTOFFEAR;
	case COMBAT_ALIEN_UPGRADE_ABILITY3:
		return IMPULSE_ALIEN_UPGRADE_ABILITY3_UNLOCK;
	case COMBAT_ALIEN_UPGRADE_ABILITY4:
		return IMPULSE_ALIEN_UPGRADE_ABILITY4_UNLOCK;
	default:
		return 0;
	}
}

void BotDirectLookAt(bot_t* pBot, Vector target)
{
	pBot->DesiredLookDirection = ZERO_VECTOR;
	pBot->InterpolatedLookDirection = ZERO_VECTOR;

	edict_t* pEdict = pBot->pEdict;

	Vector viewPos = pBot->CurrentEyePosition;

	Vector dir = target - viewPos;
	UTIL_NormalizeVector(&dir);

	pEdict->v.v_angle = UTIL_VecToAngles(dir);

	if (pEdict->v.v_angle.y > 180)
		pEdict->v.v_angle.y -= 360;

	// Paulo-La-Frite - START bot aiming bug fix
	if (pEdict->v.v_angle.x > 180)
		pEdict->v.v_angle.x -= 360;

	// set the body angles to point the gun correctly
	pEdict->v.angles.x = pEdict->v.v_angle.x / 3;
	pEdict->v.angles.y = pEdict->v.v_angle.y;
	pEdict->v.angles.z = 0;

	// adjust the view angle pitch to aim correctly (MUST be after body v.angles stuff)
	pEdict->v.v_angle.x = -pEdict->v.v_angle.x;
	// Paulo-La-Frite - END

	pEdict->v.ideal_yaw = pEdict->v.v_angle.y;

	if (pEdict->v.ideal_yaw > 180)
		pEdict->v.ideal_yaw -= 360;

	if (pEdict->v.ideal_yaw < -180)
		pEdict->v.ideal_yaw += 360;
}

void UTIL_DisplayBotInfo(bot_t* pBot)
{
	char buf[511];
	char interbuff[164];

	sprintf(buf, "Bot Role: %s\n", UTIL_BotRoleToChar(pBot->CurrentRole));

	sprintf(interbuff, "Primary Task: %s\n", UTIL_TaskTypeToChar(pBot->PrimaryBotTask.TaskType));

	strcat(buf, interbuff);

	sprintf(interbuff, "Second Task: %s\n", UTIL_TaskTypeToChar(pBot->SecondaryBotTask.TaskType));

	strcat(buf, interbuff);

	sprintf(interbuff, "Want Task: %s\n", UTIL_TaskTypeToChar(pBot->WantsAndNeedsTask.TaskType));

	strcat(buf, interbuff);

	if (BotGetNextEnemyTarget(pBot) > -1)
	{
		sprintf(interbuff, "Current Task: COMBAT\n");

		strcat(buf, interbuff);

		UTIL_DrawHUDText(GAME_GetListenServerEdict(), 0, 0.1f, 0.1f, 255, 255, 255, buf);

		return;
	}

	if (pBot->CurrentTask)
	{
		sprintf(interbuff, "Current Task: %s", UTIL_TaskTypeToChar(pBot->CurrentTask->TaskType));

		if (!FNullEnt(pBot->CurrentTask->TaskTarget) && (pBot->CurrentTask->StructureType != STRUCTURE_NONE || IsEdictStructure(pBot->CurrentTask->TaskTarget)))
		{
			NSStructureType StructType = (pBot->CurrentTask->StructureType != STRUCTURE_NONE) ? pBot->CurrentTask->StructureType : GetStructureTypeFromEdict(pBot->CurrentTask->TaskTarget);
			char structname[32];
			sprintf(structname, " (%s)\n", UTIL_StructTypeToChar(StructType));
			strcat(interbuff, structname);
		}
		else
		{
			strcat(interbuff, "\n");
		}

		strcat(buf, interbuff);
	}

	if (GAME_GetGameMode() == GAME_MODE_COMBAT)
	{
		sprintf(interbuff, "Available Points: %d\n", GetBotAvailableCombatPoints(pBot));

		strcat(buf, interbuff);
	}

	UTIL_DrawHUDText(GAME_GetListenServerEdict(), 0, 0.1f, 0.1f, 255, 255, 255, buf);
}

bot_t* UTIL_GetSpectatedBot(const edict_t* Observer)
{
	if (FNullEnt(Observer)) { return nullptr; }

	if (!NavmeshLoaded())
	{
		return nullptr;
	}

	edict_t* SpectatorTarget = INDEXENT(Observer->v.iuser2);

	if (FNullEnt(SpectatorTarget))
	{
		return nullptr;
	}

	int BotIndex = GetBotIndex(SpectatorTarget);

	if (BotIndex < 0)
	{
		return nullptr;
	}

	return &bots[BotIndex];
}

bool UTIL_ShouldBotBeCautious(bot_t* pBot)
{
	if (pBot->BotNavInfo.PathSize == 0) { return false; }

	if (UTIL_GetBotCurrentPathArea(pBot) != SAMPLE_POLYAREA_GROUND) { return false; }

	int EnemyTeam = (pBot->bot_team == MARINE_TEAM) ? ALIEN_TEAM : MARINE_TEAM;

	int NumEnemiesAtDestination = UTIL_GetNumPlayersOnTeamWithLOS(pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint].Location, EnemyTeam, UTIL_MetresToGoldSrcUnits(50.0f), pBot->pEdict);

	if (NumEnemiesAtDestination > 1)
	{
		return (vDist2DSq(pBot->pEdict->v.origin, pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint].Location) < sqrf(UTIL_MetresToGoldSrcUnits(5.0f)));
	}

	return false;
}

bool BotHasTaskOfType(bot_t* pBot, BotTaskType TaskType)
{
	return (pBot->PrimaryBotTask.TaskType == TaskType || pBot->SecondaryBotTask.TaskType == TaskType || pBot->WantsAndNeedsTask.TaskType == TaskType || pBot->CommanderTask.TaskType == TaskType);
}

void BotStopCommanderMode(bot_t* pBot)
{
	// Thanks EterniumDev (Alien) for logic to allow commander AI to leave the chair and build structures when needed

	if (IsPlayerCommander(pBot->pEdict))
	{
		FakeClientCommand(pBot->pEdict, "stopcommandermode", NULL, NULL);
	}	
}
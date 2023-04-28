
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

	Offset -= Offset * pBot->BotSkillSettings.bot_motion_tracking_skill;

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

void BotUseObject(bot_t* pBot, edict_t* Target, bool bContinuous)
{
	if (FNullEnt(Target)) { return; }

	Vector AimPoint = (Target->v.size.z < 32.0f) ? Target->v.origin : UTIL_GetCentreOfEntity(Target);

	BotLookAt(pBot, AimPoint);

	if (!bContinuous && ((gpGlobals->time - pBot->LastUseTime) < min_player_use_interval)) { return; }

	Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);
	Vector TargetAimDir = UTIL_GetVectorNormal(AimPoint - pBot->CurrentEyePosition);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	if (AimDot >= 0.95f)
	{
		pBot->pEdict->v.button |= IN_USE;
		pBot->LastUseTime = gpGlobals->time;
	}
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

void BotLeap(bot_t* pBot, const Vector TargetLocation)
{

	if (!CanBotLeap(pBot))
	{
		BotJump(pBot);
		return;
	}

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

	bool bShouldLeap = pBot->BotNavInfo.IsOnGround && (gpGlobals->time - pBot->BotNavInfo.LandedTime >= 0.2f && gpGlobals->time - pBot->BotNavInfo.LeapAttemptedTime >= 0.5f);

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
		if (pBot->TrackedEnemies[aggressorIndex].bCurrentlyVisible)
		{
			pBot->TrackedEnemies[aggressorIndex].LastSeenLocation = aggressor->v.origin;
		}
		else
		{
			// The further the enemy is, the more inaccurate the bot's guess will be where they are
			pBot->TrackedEnemies[aggressorIndex].LastSeenLocation = UTIL_GetRandomPointOnNavmeshInRadius(ALL_NAV_PROFILE, aggressor->v.origin, vDist2D(pBot->pEdict->v.origin, aggressor->v.origin));
		}

		//UTIL_DrawLine(clients[0], pBot->pEdict->v.origin, pBot->TrackedEnemies[aggressorIndex].LastSeenLocation, 5.0f);

		pBot->TrackedEnemies[aggressorIndex].LastSeenVelocity = aggressor->v.velocity;
		pBot->TrackedEnemies[aggressorIndex].bIsValidTarget = true;
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

	//UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
	//UTIL_ClearBotTask(pBot, &pBot->SecondaryBotTask);
	//UTIL_ClearBotTask(pBot, &pBot->WantsAndNeedsTask);

	pBot->bIsPendingKill = false;

	pBot->BotNavInfo.LastNavMeshPosition = ZERO_VECTOR;
	pBot->BotNavInfo.LastPathFollowPosition = ZERO_VECTOR;
}

// Bot killed another player or bot
void BotKilledPlayer(bot_t* pBot, edict_t* victim)
{

}

bot_t* GetBotPointer(edict_t* pEdict)
{
	int index;

	for (index = 0; index < MAX_CLIENTS; index++)
	{
		if (bots[index].pEdict == pEdict)
		{
			break;
		}
	}

	if (index < 32)
		return (&bots[index]);

	return NULL;  // return NULL if edict is not a bot
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

void BotAttackStructure(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO) || !IsEdictStructure(Target)) { return; }

	NSWeapon DesiredWeapon = WEAPON_NONE;

	if (IsPlayerMarine(pBot->pEdict))
	{
		DesiredWeapon = BotMarineChooseBestWeaponForStructure(pBot, Target);
	}
	else
	{
		DesiredWeapon = BotAlienChooseBestWeaponForStructure(pBot, Target);
	}

	if (DesiredWeapon == WEAPON_NONE) { return; }

	float CurrentDist = vDist2DSq(pBot->pEdict->v.origin, Target->v.origin);
	float MaxDist = sqrf(GetMaxIdealWeaponRange(DesiredWeapon));

	if (CurrentDist > MaxDist)
	{
		MoveTo(pBot, Target->v.origin, MOVESTYLE_NORMAL);
	}
	else
	{
		pBot->DesiredCombatWeapon = DesiredWeapon;

		if (GetBotCurrentWeapon(pBot) == DesiredWeapon)
		{
			BotAttackTarget(pBot, Target);
		}
	}

}

void BotAttackTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	NSWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

	if (IsMeleeWeapon(CurrentWeapon))
	{
		BotLookAt(pBot, Target);
		Vector TargetAimDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - pBot->CurrentEyePosition);

		float MaxWeaponRange = GetMaxIdealWeaponRange(CurrentWeapon);

		if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Target, MaxWeaponRange, true))
		{
			Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);


			float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

			if (AimDot >= 0.90f)
			{
				pBot->pEdict->v.button |= IN_ATTACK;
				pBot->current_weapon.LastFireTime = gpGlobals->time;
			}
		}

		return;
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
			pBot->pEdict->v.button |= IN_RELOAD;
			pBot->current_weapon.bIsReloading = true;
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

	Vector TargetAimDir = ZERO_VECTOR;

	if (CurrentWeapon == WEAPON_MARINE_GL || CurrentWeapon == WEAPON_MARINE_GRENADE)
	{
		Vector AimLocation = UTIL_GetCentreOfEntity(Target);
		Vector NewAimAngle = GetPitchForProjectile(pBot->CurrentEyePosition, AimLocation, 800.0f, 640.0f);

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



	// Don't need aiming or LOS checks for Xenocide as it's an AOE attack, just make sure we're close enough
	if (CurrentWeapon == WEAPON_SKULK_XENOCIDE)
	{
		float MaxXenoDist = GetMaxIdealWeaponRange(CurrentWeapon);

		if (vDist3DSq(pBot->pEdict->v.origin, Target->v.origin) <= sqrf(MaxXenoDist))
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
			return;
		}
	}



	// For charge and stomp, we don't need to be precise about aiming: only facing the correct direction
	if (CurrentWeapon == WEAPON_ONOS_CHARGE || CurrentWeapon == WEAPON_ONOS_STOMP)
	{
		Vector DirToTarget = UTIL_GetVectorNormal2D(Target->v.origin - pBot->pEdict->v.origin);
		float DotProduct = UTIL_GetDotProduct2D(UTIL_GetForwardVector(pBot->pEdict->v.v_angle), DirToTarget);

		float MinDotProduct = (CurrentWeapon == WEAPON_ONOS_STOMP) ? 0.95f : 0.75f;

		if (DotProduct >= MinDotProduct)
		{
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

	if (WeaponCanBeReloaded(CurrentWeapon) && BotGetCurrentWeaponClipAmmo(pBot) == 0)
	{
		if (BotGetCurrentWeaponReserveAmmo(pBot) > 0)
		{
			pBot->pEdict->v.button |= IN_RELOAD;
		}
		return;
	}

	if ((gpGlobals->time - pBot->current_weapon.LastFireTime) < pBot->current_weapon.MinRefireTime)
	{
		return;
	}

	float MaxWeaponRange = GetMaxIdealWeaponRange(CurrentWeapon);

	if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Target, MaxWeaponRange, false))
	{
		Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);

		float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

		if (AimDot >= 0.90f)
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}
	}
}

void DEBUG_BotAttackTarget(bot_t* pBot, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	NSWeapon CurrentWeapon = GetBotCurrentWeapon(pBot);

	Vector TargetAimDir = ZERO_VECTOR;

	if (CurrentWeapon == WEAPON_MARINE_GL || CurrentWeapon == WEAPON_MARINE_GRENADE)
	{
		Vector AimLocation = UTIL_GetCentreOfEntity(Target);
		Vector NewAimAngle = GetPitchForProjectile(pBot->CurrentEyePosition, AimLocation, 800.0f, 640.0f);

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



	// Don't need aiming or LOS checks for Xenocide as it's an AOE attack, just make sure we're close enough
	if (CurrentWeapon == WEAPON_SKULK_XENOCIDE)
	{
		float MaxXenoDist = GetMaxIdealWeaponRange(CurrentWeapon);

		if (vDist3DSq(pBot->pEdict->v.origin, Target->v.origin) <= sqrf(MaxXenoDist))
		{
			pBot->pEdict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
			return;
		}
	}

	if (IsMeleeWeapon(CurrentWeapon))
	{
		if (IsPlayerInUseRange(pBot->pEdict, Target))
		{
			Vector DirToTarget = UTIL_GetVectorNormal2D(Target->v.origin - pBot->pEdict->v.origin);
			float DotProduct = UTIL_GetDotProduct2D(UTIL_GetForwardVector(pBot->pEdict->v.v_angle), DirToTarget);

			if (DotProduct >= 0.45f)
			{
				pBot->pEdict->v.button |= IN_ATTACK;
				pBot->current_weapon.LastFireTime = gpGlobals->time;
			}
		}

		return;
	}

	// For charge and stomp, we don't need to be precise about aiming: only facing the correct direction
	if (CurrentWeapon == WEAPON_ONOS_CHARGE || CurrentWeapon == WEAPON_ONOS_STOMP)
	{
		Vector DirToTarget = UTIL_GetVectorNormal2D(Target->v.origin - pBot->pEdict->v.origin);
		float DotProduct = UTIL_GetDotProduct2D(UTIL_GetForwardVector(pBot->pEdict->v.v_angle), DirToTarget);

		float MinDotProduct = (CurrentWeapon == WEAPON_ONOS_STOMP) ? 0.95f : 0.75f;

		if (DotProduct >= MinDotProduct)
		{
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

	if (WeaponCanBeReloaded(CurrentWeapon) && BotGetCurrentWeaponClipAmmo(pBot) == 0)
	{
		if (BotGetCurrentWeaponReserveAmmo(pBot) > 0)
		{
			pBot->pEdict->v.button |= IN_RELOAD;
		}
		return;
	}

	if ((gpGlobals->time - pBot->current_weapon.LastFireTime) < pBot->current_weapon.MinRefireTime)
	{
		return;
	}

	float MaxWeaponRange = GetMaxIdealWeaponRange(CurrentWeapon);
	bool bHullSweep = IsMeleeWeapon(CurrentWeapon);

	if (UTIL_PlayerHasLOSToEntity(pBot->pEdict, Target, MaxWeaponRange, bHullSweep))
	{
		Vector AimDir = UTIL_GetForwardVector(pBot->pEdict->v.v_angle);

		float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

		if (AimDot >= 0.90f)
		{
			UTIL_DrawLine(clients[0], pBot->CurrentEyePosition, pBot->CurrentEyePosition + (AimDir * 500.0f), 255, 255, 0);
		}
		else
		{
			UTIL_DrawLine(clients[0], pBot->CurrentEyePosition, pBot->CurrentEyePosition + (AimDir * 500.0f));
		}
	}
}

void BotDropWeapon(bot_t* pBot)
{
	pBot->pEdict->v.impulse = IMPULSE_MARINE_DROP_WEAPON;
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

	pBot->BotSkillSettings.bot_aim_skill = 0.0f;
	pBot->BotSkillSettings.bot_motion_tracking_skill = 0.0f;
	pBot->BotSkillSettings.bot_reaction_time = 0.0f;
	pBot->BotSkillSettings.bot_view_speed = 0.0f;

	pBot->CurrentRole = BOT_ROLE_NONE;

	pBot->bot_ns_class = CLASS_NONE;

	pBot->LastUseTime = 0.0f;

	pBot->CommanderLastScanTime = 0.0f;

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

	// We add a random offset to the view angles based on how far the bot has to move its view
	// This simulates the fact that humans can't spin and lock their cross-hair exactly on the target, the further you have the spin, the more off your view will be first attempt
	if (fabsf(maxDelta) >= 45.0f)
	{
		pBot->ViewInterpolationSpeed = 500.0f;

		if (!bIsMoveLook)
		{
			pBot->ViewInterpolationSpeed *= pBot->BotSkillSettings.bot_view_speed;
			float xOffset = frandrange(10.0f, 20.0f);
			xOffset -= xOffset * pBot->BotSkillSettings.bot_aim_skill;

			float yOffset = frandrange(10.0f, 20.0f);
			yOffset -= yOffset * pBot->BotSkillSettings.bot_aim_skill;

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
			pBot->ViewInterpolationSpeed *= pBot->BotSkillSettings.bot_view_speed;
			float xOffset = frandrange(5.0f, 10.0f);
			xOffset -= xOffset * pBot->BotSkillSettings.bot_aim_skill;

			float yOffset = frandrange(5.0f, 10.0f);
			yOffset -= yOffset * pBot->BotSkillSettings.bot_aim_skill;

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
			pBot->ViewInterpolationSpeed *= pBot->BotSkillSettings.bot_view_speed;
			float xOffset = frandrange(2.0f, 5.0f);
			xOffset -= xOffset * pBot->BotSkillSettings.bot_aim_skill;

			float yOffset = frandrange(2.0f, 5.0f);
			yOffset -= yOffset * pBot->BotSkillSettings.bot_aim_skill;

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
		pBot->ViewInterpolationSpeed = 50.0f * pBot->BotSkillSettings.bot_view_speed;
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

		pBot->TrackedEnemies[i].EnemyEdict = clients[i];
		edict_t* Enemy = clients[i];

		bool bBotCanSeePlayer = IsPlayerVisibleToBot(pBot, Enemy);

		if (bBotCanSeePlayer != TrackingInfo->bCurrentlyVisible)
		{
			TrackingInfo->bCurrentlyVisible = bBotCanSeePlayer;
			TrackingInfo->NextUpdateTime = gpGlobals->time + pBot->BotSkillSettings.bot_reaction_time;
			continue;
		}

		if (bBotCanSeePlayer)
		{
			Vector BotLocation = UTIL_GetCentreOfEntity(Enemy);
			Vector BotVelocity = Enemy->v.velocity;

			if (gpGlobals->time >= TrackingInfo->NextVelocityUpdateTime)
			{
				TrackingInfo->LastSeenVelocity = TrackingInfo->PendingSeenVelocity;
			}

			if (BotVelocity != TrackingInfo->LastSeenVelocity)
			{
				TrackingInfo->PendingSeenVelocity = BotVelocity;
				TrackingInfo->NextVelocityUpdateTime = gpGlobals->time + pBot->BotSkillSettings.bot_reaction_time;
			}

			TrackingInfo->bIsValidTarget = true;
			TrackingInfo->bCurrentlyVisible = true;
			TrackingInfo->LastSeenLocation = BotLocation;

			TrackingInfo->bIsTracked = false;
			TrackingInfo->LastSeenTime = gpGlobals->time;

			continue;

		}


		TrackingInfo->bCurrentlyVisible = false;

		if (IsPlayerParasited(Enemy) || IsPlayerMotionTracked(Enemy))
		{
			TrackingInfo->TrackedLocation = Enemy->v.origin;
			TrackingInfo->LastSeenVelocity = Enemy->v.velocity;
			TrackingInfo->LastTrackedTime = gpGlobals->time;
			TrackingInfo->bIsTracked = true;

		}
		else
		{
			TrackingInfo->bIsTracked = false;
		}

		if (TrackingInfo->bIsTracked)
		{
			bool IsCloseEnoughToBeRelevant = vDist2DSq(pBot->pEdict->v.origin, TrackingInfo->TrackedLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f));
			bool RecentlyTracked = (gpGlobals->time - TrackingInfo->LastTrackedTime) < 5.0f;
			TrackingInfo->bIsValidTarget = IsCloseEnoughToBeRelevant && RecentlyTracked;
		}
		else
		{
			if (vDist2DSq(pBot->pEdict->v.origin, TrackingInfo->LastSeenLocation) < sqrf(GetPlayerRadius(pBot->pEdict)))
			{
				if (UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, Enemy->v.origin))
				{
					TrackingInfo->LastSeenLocation = Enemy->v.origin;
					TrackingInfo->LastSeenTime = gpGlobals->time;
				}
				else
				{
					TrackingInfo->LastSeenTime = 0.0f;
				}
			}
			TrackingInfo->bIsValidTarget = (gpGlobals->time - TrackingInfo->LastSeenTime) < 10.0f;
		}

	}
}

// Checks to see if pBot can see player. Returns true if player is visible
bool IsPlayerVisibleToBot(bot_t* Observer, edict_t* TargetPlayer)
{
	if (!TargetPlayer || IsPlayerBeingDigested(TargetPlayer) || IsPlayerCommander(TargetPlayer) || IsPlayerDead(TargetPlayer)) { return false; }
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
	UTIL_TraceLine((Observer->pEdict->v.origin + Observer->pEdict->v.view_ofs), TargetPlayer->v.origin, ignore_monsters, ignore_glass, Observer->pEdict->v.pContainingEntity, &hit);

	return hit.flFraction >= 1.0f;
}

void BotClearEnemyTrackingInfo(enemy_status* TrackingInfo)
{
	TrackingInfo->bCurrentlyVisible = false;
	TrackingInfo->bIsTracked = false;
	TrackingInfo->TrackedLocation = ZERO_VECTOR;
	TrackingInfo->LastSeenLocation = ZERO_VECTOR;
	TrackingInfo->LastSeenVelocity = ZERO_VECTOR;
	TrackingInfo->bIsValidTarget = false;
	TrackingInfo->LastSeenTime = 0.0f;
}

void StartNewBotFrame(bot_t* pBot)
{
	edict_t* pEdict = pBot->pEdict;

	pEdict->v.flags |= FL_THIRDPARTYBOT;
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
	default:
		RegularModeThink(pBot);
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
	if (!bGameIsActive)
	{
		WaitGameStartThink(pBot);
		return;
	}

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
	if (IsPlayerGorge(pBot->pEdict))
	{
		return GorgeGetNextEnemyTarget(pBot);
	}

	int ClosestVisibleEnemy = -1;
	float MinVisibleDist = 0.0f;

	int ClosestNonVisibleEnemy = -1;
	float MinNonVisibleDist = 0.0f;

	edict_t* ClosestVisibleEnemyEdict = nullptr;
	edict_t* ClosestNonVisibleEnemyEdict = nullptr;

	for (int i = 0; i < 32; i++)
	{
		if (pBot->TrackedEnemies[i].bIsValidTarget)
		{
			float thisDist = vDist2DSq(pBot->pEdict->v.origin, pBot->TrackedEnemies[i].EnemyEdict->v.origin);

			if (pBot->TrackedEnemies[i].bCurrentlyVisible)
			{
				if (ClosestVisibleEnemy < 0 || thisDist < MinVisibleDist)
				{
					ClosestVisibleEnemy = i;
					MinVisibleDist = thisDist;
				}
			}
			else
			{
				bool bHasLOS = UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, pBot->TrackedEnemies[i].EnemyEdict->v.origin);
				// Don't hunt an enemy if we have something important to do
				if (!bHasLOS && (pBot->PrimaryBotTask.bOrderIsUrgent || pBot->SecondaryBotTask.bOrderIsUrgent || pBot->WantsAndNeedsTask.bOrderIsUrgent)) { continue; }
				if (ClosestNonVisibleEnemy < 0 || thisDist < MinNonVisibleDist)
				{
					ClosestNonVisibleEnemy = i;
					MinNonVisibleDist = thisDist;
				}
			}
		}
	}

	if (ClosestNonVisibleEnemy == -1 || ClosestVisibleEnemy == -1)
	{
		return fmaxf(ClosestNonVisibleEnemy, ClosestVisibleEnemy);
	}
	else
	{
		ClosestVisibleEnemyEdict = pBot->TrackedEnemies[ClosestVisibleEnemy].EnemyEdict;
		ClosestNonVisibleEnemyEdict = pBot->TrackedEnemies[ClosestNonVisibleEnemy].EnemyEdict;

		if (pBot->TrackedEnemies[ClosestNonVisibleEnemy].LastSeenTime > 5.0f || IsPlayerGorge(ClosestNonVisibleEnemyEdict))
		{
			return ClosestVisibleEnemy;
		}
		else
		{
			bool bNonVisibleEnemyHasLOS = UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, ClosestNonVisibleEnemyEdict->v.origin);

			if (bNonVisibleEnemyHasLOS)
			{
				return (MinVisibleDist < MinNonVisibleDist) ? ClosestVisibleEnemy : ClosestNonVisibleEnemy;
			}
			else
			{
				return ClosestVisibleEnemy;
			}
		}
	}

	return -1;
}

int GorgeGetNextEnemyTarget(bot_t* pBot)
{
	int ClosestVisibleEnemy = -1;
	float MinVisibleDist = 0.0f;

	int ClosestNonVisibleEnemy = -1;
	float MinNonVisibleDist = 0.0f;

	edict_t* ClosestVisibleEnemyEdict = nullptr;
	edict_t* ClosestNonVisibleEnemyEdict = nullptr;

	for (int i = 0; i < 32; i++)
	{
		enemy_status* TrackingInfo = &pBot->TrackedEnemies[i];
		if (TrackingInfo->bIsValidTarget)
		{
			float thisDist = vDist2DSq(pBot->pEdict->v.origin, TrackingInfo->EnemyEdict->v.origin);

			if (TrackingInfo->bCurrentlyVisible)
			{
				if (ClosestVisibleEnemy < 0 || thisDist < MinVisibleDist)
				{
					ClosestVisibleEnemy = i;
					MinVisibleDist = thisDist;
				}
			}
			else
			{
				// Ignore enemies we haven't directly seen for more than 5 seconds. Stops gorges constantly freaking out if a parasited marine is nearby
				if ((gpGlobals->time - TrackingInfo->LastSeenTime) > 5.0f) { continue; }

				if (ClosestNonVisibleEnemy < 0 || thisDist < MinNonVisibleDist)
				{
					ClosestNonVisibleEnemy = i;
					MinNonVisibleDist = thisDist;
				}
			}
		}
	}

	if (ClosestNonVisibleEnemy == -1 || ClosestVisibleEnemy == -1)
	{
		return fmaxf(ClosestNonVisibleEnemy, ClosestVisibleEnemy);
	}
	else
	{
		ClosestVisibleEnemyEdict = pBot->TrackedEnemies[ClosestVisibleEnemy].EnemyEdict;
		ClosestNonVisibleEnemyEdict = pBot->TrackedEnemies[ClosestNonVisibleEnemy].EnemyEdict;

		if (pBot->TrackedEnemies[ClosestNonVisibleEnemy].LastSeenTime > 5.0f || IsPlayerGorge(ClosestNonVisibleEnemyEdict))
		{
			return ClosestVisibleEnemy;
		}
		else
		{
			bool bNonVisibleEnemyHasLOS = UTIL_QuickTrace(pBot->pEdict, pBot->CurrentEyePosition, ClosestNonVisibleEnemyEdict->v.origin);

			if (bNonVisibleEnemyHasLOS)
			{
				return (MinVisibleDist < MinNonVisibleDist) ? ClosestVisibleEnemy : ClosestNonVisibleEnemy;
			}
			else
			{
				return ClosestVisibleEnemy;
			}
		}
	}

	return -1;
}

void DroneThink(bot_t* pBot)
{
	BotUpdateAndClearTasks(pBot);

	if (pBot->PrimaryBotTask.TaskType != TASK_NONE)
	{
		BotProgressTask(pBot, &pBot->PrimaryBotTask);
	}

	if (pBot->BotNavInfo.PathSize > 0)
	{
		DEBUG_DrawPath(pBot->BotNavInfo.CurrentPath, pBot->BotNavInfo.PathSize, 0.0f);
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

		DEBUG_BotAttackTarget(pBot, CurrentEnemy);

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

	if (pBot->PrimaryBotTask.TaskType == TASK_MOVE)
	{
		if (vDist2DSq(pBot->pEdict->v.origin, pBot->PrimaryBotTask.TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
			return;
		}

		BotProgressTask(pBot, &pBot->PrimaryBotTask);
	}
	else
	{

		int MoveProfile = UTIL_GetMoveProfileForBot(pBot, MOVESTYLE_NORMAL);

		Vector RandomPoint = UTIL_GetRandomPointOfInterest();

		if (RandomPoint != ZERO_VECTOR && UTIL_PointIsReachable(MoveProfile, pBot->pEdict->v.origin, RandomPoint, max_player_use_reach))
		{
			pBot->PrimaryBotTask.TaskType = TASK_MOVE;
			pBot->PrimaryBotTask.TaskLocation = RandomPoint;
			pBot->PrimaryBotTask.bOrderIsUrgent = true;
		}
		else
		{
			UTIL_ClearBotTask(pBot, &pBot->PrimaryBotTask);
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
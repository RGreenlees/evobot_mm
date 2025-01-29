
#include "AIPlayer.h"
#include "AIPlayerUtil.h"
#include "AIHelper.h"
#include "AIMath.h"
#include "AIHelper.h"
#include "AINavigation.h"
#include "AIWeaponHelper.h"
#include "AITactical.h"
#include "AIPlayerManager.h"
#include "AIConfig.h"
#include "NSConstants.h"

extern nav_mesh NavMeshes[NUM_NAV_MESHES]; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)

int fake_arg_count;
extern char g_argv[1024];
bool isFakeClientCommand;

void BotJump(AIPlayer* pBot, bool bDuckJump)
{
	if (pBot->BotNavInfo.IsOnGround)
	{
		if (gpGlobals->time - pBot->BotNavInfo.LandedTime >= 0.1f)
		{
			pBot->Button |= IN_JUMP;
			pBot->BotNavInfo.bIsJumping = true;
			pBot->BotNavInfo.bHasAttemptedJump = true;
		}
	}
	else
	{
		if (pBot->BotNavInfo.bIsJumping && bDuckJump)
		{
			pBot->Button |= IN_DUCK;
		}
	}
}

void BotSuicide(AIPlayer* pBot)
{
	if (pBot && !IsPlayerDead(pBot->Edict) && !pBot->bIsPendingKill)
	{
		pBot->bIsPendingKill = true;
		MDLL_ClientKill(pBot->Edict);
	}
}

/* Makes the bot look at the specified position */
void BotLookAt(AIPlayer* pBot, const Vector target)
{
	pBot->LookTargetLocation.x = target.x;
	pBot->LookTargetLocation.y = target.y;
	pBot->LookTargetLocation.z = target.z;
}

void BotMoveLookAt(AIPlayer* pBot, const Vector target)
{
	pBot->MoveLookLocation.x = target.x;
	pBot->MoveLookLocation.y = target.y;
	pBot->MoveLookLocation.z = target.z;
}

void BotDirectLookAt(AIPlayer* pBot, Vector target)
{
	pBot->DesiredLookDirection = ZERO_VECTOR;
	pBot->InterpolatedLookDirection = ZERO_VECTOR;

	edict_t* pEdict = pBot->Edict;

	Vector viewPos = pBot->CurrentEyePosition;

	Vector dir = (target - viewPos);

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

void BotLookAt(AIPlayer* pBot, edict_t* target)
{
	if (FNullEnt(target)) { return; }

	// For team mates we don't track enemy refs, so just look at the friendly player
	if (!IsEdictPlayer(target) || target->v.team == pBot->Edict->v.team)
	{
		pBot->LookTargetLocation = UTIL_GetCentreOfEntity(target);
		return;
	}

	Vector TargetVelocity = target->v.velocity;
	Vector TargetLocation = UTIL_GetCentreOfEntity(target);

	AIWeaponType CurrentWeapon = WEAP_GetPlayerCurrentWeapon(pBot->Edict);

	Vector NewLoc = UTIL_GetAimLocationToLeadTarget(pBot->CurrentEyePosition, TargetLocation, TargetVelocity, WEAP_GetProjectileVelocityForWeapon(CurrentWeapon));

	float Offset = frandrange(30.0f, 50.0f);

	if (randbool())
	{
		Offset *= -1.0f;
	}

	float NewDist = vDist3D(TargetLocation, NewLoc) + Offset;


	float MoveSpeed = vSize3D(target->v.velocity);

	Vector MoveVector = (MoveSpeed > 5.0f) ? UTIL_GetVectorNormal(target->v.velocity) : ZERO_VECTOR;

	Vector NewAimLoc = TargetLocation + (MoveVector * NewDist);

	pBot->LookTargetLocation = NewAimLoc;
}

bool BotUseObject(AIPlayer* pBot, edict_t* Target, bool bContinuous)
{
	if (FNullEnt(Target)) { return false; }

	Vector ClosestPoint = UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, Target);
	Vector TargetCentre = UTIL_GetCentreOfEntity(Target);

	Vector AimPoint = ClosestPoint;

	BotLookAt(pBot, AimPoint);

	if (!bContinuous && ((gpGlobals->time - pBot->LastUseTime) < min_player_use_interval)) { return false; }

	Vector AimDir = UTIL_GetForwardVector2D(pBot->Edict->v.v_angle);
	Vector TargetAimDir = UTIL_GetVectorNormal2D(ClosestPoint - pBot->CurrentEyePosition);

	float AimDot = UTIL_GetDotProduct2D(AimDir, TargetAimDir);

	if (AimDot >= 0.95f)
	{
		MDLL_Use(Target, pBot->Edict);
		pBot->Button |= IN_USE;
		pBot->LastUseTime = gpGlobals->time;
		return true;
	}

	return false;
}

AIChatMessage* GetAvailableBotMsgSlot(AIPlayer* pBot)
{
	for (int i = 0; i < 5; i++)
	{
		if (!pBot->ChatMessages[i].bIsPending) { return &pBot->ChatMessages[i]; }
	}

	return nullptr;
}

void BotSay(AIPlayer* pBot, bool bTeamSay, float Delay, char* textToSay)
{
	AIChatMessage* msgSlot = GetAvailableBotMsgSlot(pBot);

	if (msgSlot)
	{
		msgSlot->bIsPending = true;
		msgSlot->bIsTeamSay = bTeamSay;
		msgSlot->SendTime = gpGlobals->time + Delay;
		sprintf(msgSlot->msg, textToSay);
	}
}

void BotDropWeapon(AIPlayer* pBot)
{
	// Look straight ahead so we don't accidentally drop the weapon right at our feet and pick it up again instantly

	Vector AimDir = UTIL_GetForwardVector(pBot->Edict->v.v_angle);
	Vector TargetAimDir = Vector(AimDir.x, AimDir.y, 0.0f);

	Vector LookLoc = pBot->CurrentEyePosition + (TargetAimDir * 100.0f);

	BotLookAt(pBot, LookLoc);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	if (AimDot >= 0.95f)
	{
		pBot->Impulse = WEAPON_DROP;
	}
}

void BotShootTarget(AIPlayer* pBot, AIWeaponType AttackWeapon, edict_t* Target)
{
	if (FNullEnt(Target) || (Target->v.deadflag != DEAD_NO)) { return; }

	AIWeaponType CurrentWeapon = WEAP_GetPlayerCurrentWeapon(pBot->Edict);

	pBot->DesiredCombatWeapon = AttackWeapon;

	if (CurrentWeapon != AttackWeapon)
	{
		return;
	}

	if (CurrentWeapon == WEAPON_INVALID) { return; }

	if (WEAP_IsMeleeWeapon(CurrentWeapon))
	{
		BotLookAt(pBot, Target);
		pBot->Button |= IN_ATTACK;
		return;
	}

	Vector TargetAimDir = ZERO_VECTOR;

	if (WEAP_IsWeaponAffectedByGravity(CurrentWeapon))
	{
		Vector AimLocation = UTIL_GetCentreOfEntity(Target);

		float ProjectileVelocity = WEAP_GetProjectileVelocityForWeapon(CurrentWeapon);

		Vector NewAimAngle = vGetLaunchAngleForProjectile(pBot->CurrentEyePosition, AimLocation, ProjectileVelocity, GOLDSRC_GRAVITY);

		AimLocation = pBot->CurrentEyePosition + (NewAimAngle * 200.0f);

		BotLookAt(pBot, AimLocation);
		TargetAimDir = UTIL_GetVectorNormal(AimLocation - pBot->CurrentEyePosition);
	}
	else
	{
		BotLookAt(pBot, Target);
		TargetAimDir = UTIL_GetVectorNormal(UTIL_GetCentreOfEntity(Target) - pBot->CurrentEyePosition);
	}

	if (WEAP_WeaponCanBeReloaded(CurrentWeapon))
	{
		bool bShouldReload = (WEAP_GetPlayerCurrentWeaponReserveAmmo(pBot->Edict) > 0) && WEAP_GetPlayerCurrentWeaponClipAmmo(pBot->Edict) == 0;

		if (bShouldReload)
		{
			WEAP_BotReloadCurrentWeapon(pBot);
			return;
		}

	}

	if (IsPlayerReloading(pBot->Edict))
	{
		return;
	}

	Vector AimDir = UTIL_GetForwardVector(pBot->Edict->v.v_angle);

	bool bWillHit = false;

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	// We can be less accurate with spores and umbra since they have AoE effects
	float MinAcceptableAccuracy = 0.9f;

	bWillHit = (AimDot >= MinAcceptableAccuracy);

	if (!bWillHit && WEAP_IsHitScanWeapon(CurrentWeapon))
	{

		edict_t* HitEntity = UTIL_TraceEntity(pBot->Edict, pBot->CurrentEyePosition, pBot->CurrentEyePosition + (AimDir * WEAP_GetMaxIdealWeaponRange(CurrentWeapon)));

		bWillHit = (HitEntity == Target);
	}

	if (bWillHit)
	{
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) >= pBot->current_weapon.MinRefireTime)
		{
			pBot->Button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}
	}
}

void BotShootLocation(AIPlayer* pBot, AIWeaponType AttackWeapon, const Vector TargetLocation)
{
	if (vIsZero(TargetLocation)) { return; }

	AIWeaponType CurrentWeapon = WEAP_GetPlayerCurrentWeapon(pBot->Edict);

	pBot->DesiredCombatWeapon = AttackWeapon;

	// We will switch to this weapon and return to this function
	if (CurrentWeapon != AttackWeapon)
	{
		return;
	}

	if (CurrentWeapon == WEAPON_INVALID) { return; }

	if (WEAP_IsMeleeWeapon(CurrentWeapon))
	{
		BotLookAt(pBot, TargetLocation);
		pBot->Button |= IN_ATTACK;
		return;
	}

	Vector TargetAimDir = ZERO_VECTOR;

	if (WEAP_IsWeaponAffectedByGravity(CurrentWeapon))
	{
		Vector AimLocation = TargetLocation;
		Vector NewAimAngle = vGetLaunchAngleForProjectile(pBot->CurrentEyePosition, AimLocation, WEAP_GetProjectileVelocityForWeapon(CurrentWeapon), GOLDSRC_GRAVITY);

		AimLocation = pBot->CurrentEyePosition + (NewAimAngle * 200.0f);

		BotLookAt(pBot, AimLocation);
		TargetAimDir = UTIL_GetVectorNormal(AimLocation - pBot->CurrentEyePosition);
	}
	else
	{
		BotLookAt(pBot, TargetLocation);
		TargetAimDir = UTIL_GetVectorNormal(TargetLocation - pBot->CurrentEyePosition);
	}

	if (WEAP_WeaponCanBeReloaded(CurrentWeapon))
	{
		bool bShouldReload = (WEAP_GetPlayerCurrentWeaponReserveAmmo(pBot->Edict) > 0) && (WEAP_GetPlayerCurrentWeaponClipAmmo(pBot->Edict) == 0);

		if (bShouldReload)
		{
			WEAP_BotReloadCurrentWeapon(pBot);
			return;
		}

	}

	if (IsPlayerReloading(pBot->Edict))
	{
		return;
	}

	Vector AimDir = UTIL_GetForwardVector(pBot->Edict->v.v_angle);

	float AimDot = UTIL_GetDotProduct(AimDir, TargetAimDir);

	// 1.0 means exactly dead-on.
	float MinAcceptableAccuracy = 0.9f;

	if (AimDot >= MinAcceptableAccuracy)
	{
		if ((gpGlobals->time - pBot->current_weapon.LastFireTime) >= pBot->current_weapon.MinRefireTime)
		{
			pBot->Edict->v.button |= IN_ATTACK;
			pBot->current_weapon.LastFireTime = gpGlobals->time;
		}
	}
}

void BotUpdateDesiredViewRotation(AIPlayer* pBot)
{
	// We always prioritise MoveLookLocation if it is set so the bot doesn't screw up wall climbing or ladder movement
	Vector NewLookLocation = (!vIsZero(pBot->MoveLookLocation)) ? pBot->MoveLookLocation : pBot->LookTargetLocation;

	bool bIsMoveLook = !vIsZero(pBot->MoveLookLocation);

	// We're already interpolating to an existing desired look direction (see BotUpdateViewRotation()) or we don't have a desired look target
	if (!vIsZero(pBot->DesiredLookDirection) || vIsZero(NewLookLocation)) { return; }

	edict_t* pEdict = pBot->Edict;

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
		pBot->ViewInterpolationSpeed = 350.0f;

		if (!bIsMoveLook)
		{
			float xOffset = frandrange(10.0f, 20.0f);

			float yOffset = frandrange(10.0f, 20.0f);

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
		pBot->ViewInterpolationSpeed = 175.0f;

		if (!bIsMoveLook)
		{
			float xOffset = frandrange(5.0f, 10.0f);
			float yOffset = frandrange(5.0f, 10.0f);

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
		pBot->ViewInterpolationSpeed = 35.0f;

		if (!bIsMoveLook)
		{
			float xOffset = frandrange(2.0f, 5.0f);
			float yOffset = frandrange(2.0f, 5.0f);

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
		pBot->ViewInterpolationSpeed = 50.0f;
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

void BotUpdateViewRotation(AIPlayer* pBot, float DeltaTime)
{
	if (!vIsZero(pBot->DesiredLookDirection))
	{
		edict_t* pEdict = pBot->Edict;

		float Delta = pBot->DesiredLookDirection.y - pBot->InterpolatedLookDirection.y;

		if (Delta > 180.0f)
			Delta -= 360.0f;
		if (Delta < -180.0f)
			Delta += 360.0f;

		pBot->InterpolatedLookDirection.x = fInterpConstantTo(pBot->InterpolatedLookDirection.x, pBot->DesiredLookDirection.x, DeltaTime, pBot->ViewInterpolationSpeed);

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

	if ((gpGlobals->time - pBot->LastViewUpdateTime) > pBot->ViewUpdateRate)
	{
		BotUpdateView(pBot);
		pBot->LastViewUpdateTime = gpGlobals->time;
	}
}

void BotUpdateView(AIPlayer* pBot)
{
	pBot->ViewForwardVector = UTIL_GetForwardVector(pBot->Edict->v.v_angle);
	UpdateAIPlayerViewFrustum(pBot);

	// Logic here for detecting and tracking visibility of enemies or other objects
	
}

bool IsEntityVisibleToBot(AIPlayer* pBot, const edict_t* Entity)
{
	if (!IsEntityInBotFOV(pBot, Entity)) { return false; }

	float EntityRadius = Entity->v.size.Length2D() * 0.5f;
	float EntityHeight = Entity->v.absmax.z - Entity->v.absmin.z;

	if (UTIL_TraceEntity(pBot->Edict, pBot->CurrentEyePosition, Entity->v.origin) == Entity)
	{
		return true;
	}

	Vector FeetLocation = Vector(Entity->v.origin.x, Entity->v.origin.y, Entity->v.absmin.z + 5.0f);

	if (UTIL_TraceEntity(pBot->Edict, pBot->CurrentEyePosition, FeetLocation) == Entity)
	{
		return true;
	}

	Vector HeadLocation = Vector(Entity->v.origin.x, Entity->v.origin.y, Entity->v.absmax.z - 5.0f);

	if (UTIL_TraceEntity(pBot->Edict, pBot->CurrentEyePosition, FeetLocation) == Entity)
	{
		return true;
	}

	Vector ViewAngle = UTIL_GetVectorNormal2D(Entity->v.origin - pBot->CurrentEyePosition);
	Vector RightAngle = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(ViewAngle, UP_VECTOR));

	Vector RightSide = Entity->v.origin + (RightAngle * EntityRadius);

	if (UTIL_TraceEntity(pBot->Edict, pBot->CurrentEyePosition, RightSide) == Entity)
	{
		return true;
	}

	Vector LeftSide = Entity->v.origin - (RightAngle * EntityRadius);

	if (UTIL_TraceEntity(pBot->Edict, pBot->CurrentEyePosition, LeftSide) == Entity)
	{
		return true;
	}

	return false;
}

void UpdateAIPlayerViewFrustum(AIPlayer* pBot)
{
	MAKE_VECTORS(pBot->Edict->v.v_angle);
	Vector up = gpGlobals->v_up;
	Vector forward = gpGlobals->v_forward;
	Vector right = gpGlobals->v_right;

	Vector fc = (pBot->Edict->v.origin + pBot->Edict->v.view_ofs) + (forward * BOT_MAX_VIEW);

	Vector fbl = fc + (up * f_ffheight * 0.5f) - (right * f_ffwidth * 0.5f);
	Vector fbr = fc + (up * f_ffheight * 0.5f) + (right * f_ffwidth * 0.5f);
	Vector ftl = fc - (up * f_ffheight * 0.5f) - (right * f_ffwidth * 0.5f);
	Vector ftr = fc - (up * f_ffheight * 0.5f) + (right * f_ffwidth * 0.5f);

	Vector nc = (pBot->Edict->v.origin + pBot->Edict->v.view_ofs) + (forward * BOT_MIN_VIEW);

	Vector nbl = nc + (up * f_fnheight * 0.5f) - (right * f_fnwidth * 0.5f);
	Vector nbr = nc + (up * f_fnheight * 0.5f) + (right * f_fnwidth * 0.5f);
	Vector ntl = nc - (up * f_fnheight * 0.5f) - (right * f_fnwidth * 0.5f);
	Vector ntr = nc - (up * f_fnheight * 0.5f) + (right * f_fnwidth * 0.5f);

	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_TOP], ftl, ntl, ntr);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_BOTTOM], fbr, nbr, nbl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_LEFT], fbl, nbl, ntl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_RIGHT], ftr, ntr, nbr);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_NEAR], nbr, ntr, ntl);
	UTIL_SetFrustumPlane(&pBot->viewFrustum[FRUSTUM_PLANE_FAR], fbl, ftl, ftr);
}

bool IsEntityInBotFOV(AIPlayer* Observer, const edict_t* Entity)
{
	// Obviously can't see the object if it's a null entity, or is set not to be drawn
	if (FNullEnt(Entity) || (Entity->v.effects & EF_NODRAW)) { return false; }

	float EntityRadius = Entity->v.size.Length2D() * 0.5f;
	float EntityHeight = Entity->v.absmax.z - Entity->v.absmin.z;

	// To make things a little more accurate, we're going to treat players as cylinders rather than boxes
	for (int i = 0; i < 6; i++)
	{
		// Our cylinder must be inside all planes to be visible, otherwise return false
		if (!UTIL_CylinderInsidePlane(&Observer->viewFrustum[i], Entity->v.origin, EntityHeight, EntityRadius))
		{
			return false;
		}
	}

	return true;
}

Vector GetVisiblePointOnPlayerFromObserver(edict_t* Observer, edict_t* TargetPlayer)
{
	Vector TargetCentre = UTIL_GetCentreOfEntity(TargetPlayer);

	TraceResult hit;
	UTIL_TraceLine(GetPlayerEyePosition(Observer), TargetCentre, ignore_monsters, ignore_glass, Observer->v.pContainingEntity, &hit);

	if (hit.flFraction >= 1.0f) { return TargetCentre; }

	UTIL_TraceLine(GetPlayerEyePosition(Observer), GetPlayerEyePosition(TargetPlayer), ignore_monsters, ignore_glass, Observer->v.pContainingEntity, &hit);

	if (hit.flFraction >= 1.0f) { return GetPlayerEyePosition(TargetPlayer); }

	UTIL_TraceLine(GetPlayerEyePosition(Observer), GetPlayerBottomOfCollisionHull(TargetPlayer) + Vector(0.0f, 0.0f, 5.0f), ignore_monsters, ignore_glass, Observer->v.pContainingEntity, &hit);

	if (hit.flFraction >= 1.0f) { return GetPlayerBottomOfCollisionHull(TargetPlayer) + Vector(0.0f, 0.0f, 5.0f); }

	return ZERO_VECTOR;
}

void UpdateBotChat(AIPlayer* pBot)
{
	for (int i = 0; i < 5; i++)
	{
		if (pBot->ChatMessages[i].bIsPending && gpGlobals->time >= pBot->ChatMessages[i].SendTime)
		{
			if (pBot->ChatMessages[i].bIsTeamSay)
			{
			 	CLIENT_COMMAND(pBot->Edict, "say_team %s", pBot->ChatMessages[i].msg);
			}
			else
			{
				CLIENT_COMMAND(pBot->Edict, "say %s", pBot->ChatMessages[i].msg);
			}
			pBot->ChatMessages[i].bIsPending = false;
			break;
		}
	}
}

void ClearBotInputs(AIPlayer* pBot)
{
	pBot->Button = 0;
	pBot->ForwardMove = 0.0f;
	pBot->SideMove = 0.0f;
	pBot->UpMove = 0.0f;
	pBot->Impulse = 0;
	pBot->Button = 0;
}

void StartNewBotFrame(AIPlayer* pBot)
{
	edict_t* pEdict = pBot->Edict;

	ClearBotInputs(pBot);
	pBot->CurrentEyePosition = GetPlayerEyePosition(pEdict);

	Vector NewFloorPosition = UTIL_GetEntityGroundLocation(pEdict);

	if (vDist2DSq(NewFloorPosition, pBot->CurrentFloorPosition) > sqrf(UTIL_MetresToGoldSrcUnits(3.0f)))
	{
		OnBotTeleport(pBot);
	}

	pBot->CurrentFloorPosition = NewFloorPosition;

	Vector ProjectPoint = pBot->CollisionHullBottomLocation;

	if (UTIL_IsTileCacheUpToDate() && vDist3DSq(pBot->BotNavInfo.LastNavMeshCheckPosition, ProjectPoint) > sqrf(16.0f))
	{
		if (UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, pBot->SpawnLocation, ProjectPoint, 16.0f))
		{
			Vector NavPoint = UTIL_ProjectPointToNavmesh(pBot->BotNavInfo.NavProfile.NavMeshIndex, ProjectPoint, pBot->BotNavInfo.NavProfile);
			UTIL_AdjustPointAwayFromNavWall(NavPoint, 8.0f);

			pBot->BotNavInfo.LastNavMeshPosition = NavPoint;

			if (pBot->BotNavInfo.IsOnGround)
			{
				Vector ForwardVector = UTIL_GetForwardVector2D(pBot->Edict->v.angles);
				Vector RightVector = UTIL_GetCrossProduct(ForwardVector, UP_VECTOR);

				Vector TraceEndPoints[4];

				TraceEndPoints[0] = pBot->Edict->v.origin + (ForwardVector * (GetPlayerRadius(pBot->Edict) * 2.0f));
				TraceEndPoints[1] = pBot->Edict->v.origin - (ForwardVector * (GetPlayerRadius(pBot->Edict) * 2.0f));
				TraceEndPoints[2] = pBot->Edict->v.origin + (RightVector * (GetPlayerRadius(pBot->Edict) * 2.0f));
				TraceEndPoints[3] = pBot->Edict->v.origin - (RightVector * (GetPlayerRadius(pBot->Edict) * 2.0f));

				int NumDirectionsChecked = 0;
				bool bHasRoom = true;

				while (NumDirectionsChecked < 4 && bHasRoom)
				{
					Vector EndTrace = TraceEndPoints[NumDirectionsChecked];
					Vector EndNavTrace = EndTrace;
					EndNavTrace.z = pBot->CollisionHullBottomLocation.z;

					if (!UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, EndTrace))
					{
						bHasRoom = false;
						break;
					}

					if (!UTIL_TraceNav(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, EndNavTrace, 0.0f))
					{
						bHasRoom = false;
						break;
					}

					NumDirectionsChecked++;
				}

				if (bHasRoom)
				{
					pBot->BotNavInfo.LastOpenLocation = pBot->CurrentFloorPosition;
				}

			}
		}

		pBot->BotNavInfo.LastNavMeshCheckPosition = pBot->CurrentFloorPosition;
	}

	pBot->LookTargetLocation = ZERO_VECTOR;
	pBot->MoveLookLocation = ZERO_VECTOR;
	pBot->desiredMovementDir = ZERO_VECTOR;

	pBot->DesiredCombatWeapon = WEAPON_INVALID;
	pBot->DesiredMoveWeapon = WEAPON_INVALID;

	if ((pEdict->v.flags & FL_ONGROUND) || IsPlayerOnLadder(pEdict))
	{
		if (!pBot->BotNavInfo.IsOnGround || pBot->BotNavInfo.bHasAttemptedJump)
		{
			pBot->BotNavInfo.LandedTime = gpGlobals->time;
		}

		pBot->BotNavInfo.IsOnGround = true;
		pBot->BotNavInfo.bIsJumping = false;
		pBot->BotNavInfo.AirStartedTime = 0.0f;
	}
	else
	{
		if (pBot->BotNavInfo.IsOnGround)
		{
			pBot->BotNavInfo.AirStartedTime = gpGlobals->time;
		}

		pBot->BotNavInfo.IsOnGround = false;

		// It's possible that players can get stuck in angled geometry where they're permanently in the air
		// This just checks for that possibility: if a bot is falling for more than 15 seconds they're probably stuck and should suicide
		if (pBot->BotNavInfo.AirStartedTime > 0.0f && gpGlobals->time - pBot->BotNavInfo.AirStartedTime > 15.0f)
		{
			BotSuicide(pBot);
		}

	}

	pBot->BotNavInfo.bHasAttemptedJump = false;

	pBot->BotNavInfo.bShouldWalk = false;
}

void EndBotFrame(AIPlayer* pBot)
{
	UpdateBotStuck(pBot);

	AIWeaponType DesiredWeapon = (pBot->DesiredMoveWeapon != WEAPON_INVALID) ? pBot->DesiredMoveWeapon : pBot->DesiredCombatWeapon;

	if (DesiredWeapon != WEAPON_INVALID && WEAP_GetPlayerCurrentWeapon(pBot->Edict) != DesiredWeapon)
	{
		BotSwitchToWeapon(pBot, DesiredWeapon);
	}
}

void OnBotTeleport(AIPlayer* pBot)
{
	ClearBotStuck(pBot);
	ClearBotStuckMovement(pBot);
	pBot->BotNavInfo.LastOpenLocation = ZERO_VECTOR;
	pBot->LastTeleportTime = gpGlobals->time;
}

void AIPlayerKilled(AIPlayer* pBot, edict_t* Killer)
{
	// Logic here for the bot to react being killed by someone
}

void AIPlayerTakeDamage(AIPlayer* pBot, int damageTaken, edict_t* aggressor)
{
	// Logic here for the bot to react being damaged by someone (friendly or enemy)
}

void RunAIPlayerFrame(AIPlayer* pBot)
{
	pBot->ThinkDelta = fminf(gpGlobals->time - pBot->LastThinkTime, 0.1f);
	pBot->LastThinkTime = gpGlobals->time;

	bool bShouldThink = ShouldBotThink(pBot);

	if (bShouldThink)
	{
		if (pBot->bIsInactive)
		{
			BotResumePlay(pBot);
		}
	}
	else
	{
		ClearBotInputs(pBot);
		pBot->bIsInactive = true;
		return;
	}

	StartNewBotFrame(pBot);

	if (AIMGR_HasBotStartedGame(pBot))
	{
		AIPlayerThink(pBot);
	}
	else
	{
		AIPlayerStartGameThink(pBot);
	}	

	BotUpdateDesiredViewRotation(pBot);

	EndBotFrame(pBot);
}

void AIPlayerStartGameThink(AIPlayer* pBot)
{
	switch (pBot->DesiredTeam)
	{
		case MARINE_TEAM:
			FakeClientCommand(pBot->Edict, "jointeamone", NULL, NULL);
			break;
		case ALIEN_TEAM:
			FakeClientCommand(pBot->Edict, "jointeamtwo", NULL, NULL);
			break;
		case MARINE_TEAM_TWO:
			FakeClientCommand(pBot->Edict, "jointeamthree", NULL, NULL);
			break;
		case ALIEN_TEAM_TWO:
			FakeClientCommand(pBot->Edict, "jointeamfour", NULL, NULL);
			break;
		default:
			FakeClientCommand(pBot->Edict, "autoassign", NULL, NULL);
			break;
	}
}

void AIPlayerThink(AIPlayer* pBot)
{
	// Entry point for all the bot's decision making and actions

	if (!vIsZero(pBot->TestLocation))
	{
		if (vDist3DSq(pBot->CollisionHullBottomLocation, pBot->TestLocation) < sqrf(50.0f))
		{
			pBot->TestLocation = ZERO_VECTOR;
		}
		else
		{
			NAV_MoveTo(pBot, pBot->TestLocation, MOVESTYLE_NORMAL);
			AIDEBUG_DrawBotPath(pBot);
		}
	}
}

void BotSwitchToWeapon(AIPlayer* pBot, AIWeaponType NewWeaponSlot)
{
	const char* WeaponName = UTIL_WeaponTypeToClassname(NewWeaponSlot);

	FakeClientCommand(pBot->Edict, WeaponName, NULL, NULL);
}

bool ShouldBotThink(AIPlayer* pBot)
{
	return NavmeshLoaded() && (IsPlayerInReadyRoom(pBot->Edict) || IsPlayerActiveInGame(pBot->Edict));
}

void BotResumePlay(AIPlayer* pBot)
{
	ClearBotMovement(pBot);
	SetBaseNavProfile(pBot);

	pBot->bIsInactive = false;

	pBot->SpawnLocation = UTIL_GetFloorUnderEntity(pBot->Edict);
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
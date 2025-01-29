//
// evobot - Neoptolemus' Recast/Detour base GoldSrc bot, based on Botman's HPB bot template
//
// AIClient.cpp
// 
// Contains all network message handling by the bot
//

#ifndef _WIN32
#include <string.h>
#endif

#include "AIClient.h"

#include <extdll.h>
#include <dllapi.h>
#include <h_export.h>
#include <meta_api.h>

#include "AIPlayerManager.h"
#include "AIWeaponHelper.h"
#include "AIPlayerUtil.h"
#include "AITactical.h"

// instant damage (from cbase.h)
#define DMG_CRUSH			(1 << 0)	// crushed by falling or moving object
#define DMG_BURN			(1 << 3)	// heat burned
#define DMG_FREEZE			(1 << 4)	// frozen
#define DMG_FALL			(1 << 5)	// fell too far
#define DMG_SHOCK			(1 << 8)	// electric shock
#define DMG_DROWN			(1 << 14)	// Drowning
#define DMG_NERVEGAS		(1 << 16)	// nerve toxins, very bad
#define DMG_RADIATION		(1 << 18)	// radiation exposure
#define DMG_DROWNRECOVER	(1 << 19)	// drowning recovery
#define DMG_ACID			(1 << 20)	// toxic chemicals or acid burns
#define DMG_SLOWBURN		(1 << 21)	// in an oven
#define DMG_SLOWFREEZE		(1 << 22)	// in a subzero freezer

// types of damage to ignore...
#define IGNORE_DAMAGE (DMG_CRUSH | DMG_BURN | DMG_FREEZE | DMG_FALL | \
                       DMG_SHOCK | DMG_DROWN | DMG_NERVEGAS | DMG_RADIATION | \
                       DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN | \
                       DMG_SLOWFREEZE | 0xFF000000)

damage_msg DamageInfo;
death_msg DeathInfo;
current_weapon_msg CurrentWeaponInfo;
ammox_msg AmmoXInfo;

// This message is sent when a client joins the game.  All of the weapons
// are sent with the weapon ID and information about what ammo is used.
void BotClient_Valve_WeaponList(void* p, int bot_index)
{
	static int state = 0;   // current state machine state
	static AIWeaponTypeDefinition bot_weapon;
	static int WeaponIndex = 0;

	if (state == 0)
	{
		state++;
		strcpy(bot_weapon.szClassname, (char*)p);

		bot_weapon.MinRefireTime = 0.0f;

		if (!strcmp(bot_weapon.szClassname, "weapon_pistol"))
		{
			bot_weapon.MaxClipSize = 10;
			bot_weapon.MinRefireTime = 0.15f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_machinegun"))
		{
			bot_weapon.MaxClipSize = 50;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_shotgun"))
		{
			bot_weapon.MaxClipSize = 8;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_heavymachinegun"))
		{
			bot_weapon.MaxClipSize = 125;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_grenadegun"))
		{
			bot_weapon.MaxClipSize = 4;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_grenade"))
		{
			bot_weapon.MaxClipSize = 2;
			bot_weapon.MinRefireTime = 2.0f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_divinewind"))
		{
			bot_weapon.MinRefireTime = 0.5f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_umbra"))
		{
			bot_weapon.MinRefireTime = 0.5f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_stomp"))
		{
			bot_weapon.MinRefireTime = 1.0f;
		}
		else if (!strcmp(bot_weapon.szClassname, "weapon_spore"))
		{
			bot_weapon.MinRefireTime = 1.0f;
		}
	}
	else if (state == 1)
	{
		state++;
		bot_weapon.AmmoIndex = *(int*)p;  // ammo index 1
	}
	else if (state == 2)
	{
		state++;
		bot_weapon.MaxAmmoReserve = *(int*)p;  // max ammo1
	}
	else if (state == 3)
	{
		state++;
	}
	else if (state == 4)
	{
		state++;
	}
	else if (state == 5)
	{
		state++;
		bot_weapon.Slot = *(int*)p;  // slot for this weapon
	}
	else if (state == 6)
	{
		state++;
		bot_weapon.SlotPosition = *(int*)p;  // position in slot
	}
	else if (state == 7)
	{
		state++;
		bot_weapon.WeaponType = *(AIWeaponType*)p;  // weapon ID
		WeaponIndex = *(int*)p;
	}
	else if (state == 8)
	{
		state = 0;

		UTIL_RegisterNewWeaponType(WeaponIndex, &bot_weapon);
	}
}

void BotClient_Valve_WeapPickup(void* p, int player_index)
{
	int WeaponIndex = *(int*)p;

	UTIL_RegisterPlayerNewWeapon(player_index, WeaponIndex);
}

void BotClient_Valve_CurrentWeapon_Reset()
{
	memset(&CurrentWeaponInfo, 0, sizeof(current_weapon_msg));
}

void BotClient_Valve_CurrentWeapon(void* p, int player_index)
{
	if (CurrentWeaponInfo.state == 0)
	{
		CurrentWeaponInfo.state++;
		CurrentWeaponInfo.iState = *(int*)p;  // state of the current weapon
	}
	else if (CurrentWeaponInfo.state == 1)
	{
		CurrentWeaponInfo.state++;
		CurrentWeaponInfo.iId = *(int*)p;  // weapon ID of current weapon
	}
	else if (CurrentWeaponInfo.state == 2)
	{
		CurrentWeaponInfo.state = 0;

		CurrentWeaponInfo.iClip = *(int*)p;  // ammo currently in the clip for this weapon

		if (CurrentWeaponInfo.iId <= 31)
		{
			if ((CurrentWeaponInfo.iState & WEAPON_ON_TARGET))
			{
				UTIL_RegisterPlayerCurrentWeapon(player_index, CurrentWeaponInfo.iId, CurrentWeaponInfo.iClip);
			}
		}

	}
}

void BotClient_Valve_AmmoX_Reset()
{
	memset(&AmmoXInfo, 0, sizeof(ammox_msg));
}

// This message is sent whenever ammo ammounts are adjusted (up or down).
void BotClient_Valve_AmmoX(void* p, int player_index)
{

	if (AmmoXInfo.state == 0)
	{
		AmmoXInfo.state++;
		AmmoXInfo.index = *(int*)p;  // ammo index (for type of ammo)
	}
	else if (AmmoXInfo.state == 1)
	{
		AmmoXInfo.state = 0;

		AmmoXInfo.amount = *(int*)p;  // the amount of ammo currently available

		UTIL_UpdatePlayerAmmo(player_index, AmmoXInfo.index, AmmoXInfo.amount);

	}
}

void BotClient_Valve_Damage_Reset()
{
	memset(&DamageInfo, 0, sizeof(damage_msg));
}

void BotClient_Valve_Damage(void* p, int bot_index)
{
	if (DamageInfo.state == 0)
	{
		DamageInfo.state++;
		DamageInfo.damage_armor = *(int*)p;
	}
	else if (DamageInfo.state == 1)
	{
		DamageInfo.state++;
		DamageInfo.damage_taken = *(int*)p;
	}
	else if (DamageInfo.state == 2)
	{
		DamageInfo.state++;
		DamageInfo.damage_bits = *(int*)p;
	}
	else if (DamageInfo.state == 3)
	{
		DamageInfo.state++;
		DamageInfo.damage_origin.x = *(float*)p;
	}
	else if (DamageInfo.state == 4)
	{
		DamageInfo.state++;
		DamageInfo.damage_origin.y = *(float*)p;
	}
	else if (DamageInfo.state == 5)
	{
		DamageInfo.damage_origin.z = *(float*)p;

		if (DamageInfo.damage_taken > 0)
		{
			edict_t* aggressor = nullptr;
			float MinDistSq = 0.0f;
			float MaxDistSq = sqrf(50.0f);

			for (int i = 0; i < gpGlobals->maxClients; i++)
			{
				edict_t* Player = INDEXENT(i + 1);
				if (!FNullEnt(Player) && IsPlayerActiveInGame(Player))
				{
					float Dist = vDist3DSq(Player->v.origin, DamageInfo.damage_origin);

					if (Dist <= MaxDistSq)
					{
						if (!aggressor || Dist < MinDistSq)
						{
							aggressor = Player;
							MinDistSq = Dist;
						}
					}
				}
			}

			if (!FNullEnt(aggressor))
			{
				AIPlayer* DamagedBot = AIMGR_GetBotAtIndex(bot_index);
				AIPlayerTakeDamage(DamagedBot, DamageInfo.damage_taken, aggressor);
			}
		}

		DamageInfo.state = 0;
	}
}

void BotClient_Valve_DeathMessage_Reset()
{
	memset(&DeathInfo, 0, sizeof(death_msg));
}

// This message gets sent when the bots get killed
void BotClient_Valve_DeathMsg(void* p, int bot_index)
{
	if (DeathInfo.state == 0)
	{
		DeathInfo.state++;
		DeathInfo.killer_index = *(int*)p;  // ENTINDEX() of killer
	}
	else if (DeathInfo.state == 1)
	{
		DeathInfo.state++;
		DeathInfo.victim_index = *(int*)p;  // ENTINDEX() of victim
	}
	else if (DeathInfo.state == 2)
	{
		DeathInfo.state = 0;

		DeathInfo.killer_edict = INDEXENT(DeathInfo.killer_index);
		DeathInfo.victim_edict = INDEXENT(DeathInfo.victim_index);

		// get the bot index of the victim...
		DeathInfo.index = AIMGR_GetBotIndex(DeathInfo.victim_edict);

		// is this message about a bot being killed?
		if (DeathInfo.index != -1 && !FNullEnt(DeathInfo.killer_edict))
		{
			AIPlayer* DamagedBot = AIMGR_GetBotPointer(DeathInfo.victim_edict);
			AIPlayerKilled(DamagedBot, DeathInfo.killer_edict);
		}
	}
}

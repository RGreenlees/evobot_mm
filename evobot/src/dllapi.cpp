// vi: set ts=4 sw=4 :
// vim: set tw=75 :

/*
 * Copyright (c) 2001-2006 Will Day <willday@hpgx.net>
 *
 *    This file is part of Metamod.
 *
 *    Metamod is free software; you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the
 *    Free Software Foundation; either version 2 of the License, or (at
 *    your option) any later version.
 *
 *    Metamod is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Metamod; if not, write to the Free Software Foundation,
 *    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    In addition, as a special exception, the author gives permission to
 *    link the code of this program with the Half-Life Game Engine ("HL
 *    Engine") and Modified Game Libraries ("MODs") developed by Valve,
 *    L.L.C ("Valve").  You must obey the GNU General Public License in all
 *    respects for all of the code used other than the HL Engine and MODs
 *    from Valve.  If you modify this file, you may extend this exception
 *    to your version of the file, but you are not obligated to do so.  If
 *    you do not wish to do so, delete this exception statement from your
 *    version.
 *
 */

#include <time.h>
#include <extdll.h>

#include <dllapi.h>
#include <meta_api.h>

#include "AIPlayerManager.h"
#include "AIHelper.h"
#include "AIWeaponHelper.h"
#include "AINavigation.h"

extern int m_spriteTexture;

extern edict_t* clients[MAX_PLAYERS];
extern int NumClients;

extern edict_t* listenserver_edict;

extern bool bGameHasStarted;

float last_think_time;

extern float last_bot_count_check_time;

bool bInitialFrame = true;


void ClientCommand(edict_t* pEntity)
{
	const char* pcmd = CMD_ARGV(0);
	const char* arg1 = CMD_ARGV(1);
	const char* arg2 = CMD_ARGV(2);
	const char* arg3 = CMD_ARGV(3);
	const char* arg4 = CMD_ARGV(4);
	const char* arg5 = CMD_ARGV(5);

	// commands that anyone on your team is allowed to use
	if (FStrEq(pcmd, "say") || FStrEq(pcmd, "say_team"))
	{
		RETURN_META(MRES_IGNORED);
	}

	// only allow custom commands if deathmatch mode and NOT dedicated server and
	// client sending command is the listen server client...
	if (!gpGlobals->deathmatch || IS_DEDICATED_SERVER() || pEntity != listenserver_edict)
	{
		RETURN_META(MRES_IGNORED);
	}

	// Insert any custom commands you want to enable for the bot here

	RETURN_META(MRES_IGNORED);
}

void GameDLLInit(void)
{

	//GAME_Reset();
	//GAME_ClearClientList();
	
	RETURN_META(MRES_IGNORED);
}

BOOL ClientConnect(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128])
{
	
	if (gpGlobals->deathmatch)
	{
		// check if this client is the listen server client
		if (!IS_DEDICATED_SERVER() && strcmp(pszAddress, "loopback") == 0)
		{
			// save the edict of the listen server client...
			AIMGR_SetListenServerEdict(pEntity);
		}
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

// Called whenever a client joins the server
void ClientPutInServer(edict_t* pEntity)
{
	RETURN_META(MRES_IGNORED);
}

// Called whenever a client leaves the server
void ClientDisconnect(edict_t* pEntity)
{
	RETURN_META(MRES_IGNORED);
}

int Spawn(edict_t* pent)
{
	if (gpGlobals->deathmatch)
	{
		m_spriteTexture = PRECACHE_MODEL("sprites/zbeam6.spr");

		char* pClassname = (char*)STRING(pent->v.classname);

		// Detect when a new map has been loaded
		if (strcmp(pClassname, "worldspawn") == 0)
		{
			AIMGR_NewMap();
			bInitialFrame = true;
		}
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void SV_Use(edict_t* pentUsed, edict_t* pentOther)
{
	RETURN_META(MRES_IGNORED);
}

void KeyValue(edict_t* pentKeyvalue, KeyValueData* pkvd)
{
	if (!pkvd) { RETURN_META(MRES_IGNORED); }

	int EntityIndex = ENTINDEX(pentKeyvalue);


	if (FClassnameIs(pentKeyvalue, "multi_manager"))
	{
		NAV_AddPrototypeTarget(EntityIndex, pkvd->szKeyName);
	}

	if (	FClassnameIs(pentKeyvalue, "func_door")
		||	FClassnameIs(pentKeyvalue, "func_door_rotating")
		|| FClassnameIs(pentKeyvalue, "func_seethroughdoor")
		|| FClassnameIs(pentKeyvalue, "func_plat")
		|| FClassnameIs(pentKeyvalue, "func_train")
		|| FClassnameIs(pentKeyvalue, "func_button")
		|| FClassnameIs(pentKeyvalue, "trigger_once")
		|| FClassnameIs(pentKeyvalue, "trigger_multiple")
		|| FClassnameIs(pentKeyvalue, "func_breakable")
		|| FClassnameIs(pentKeyvalue, "multi_manager")
		|| FClassnameIs(pentKeyvalue, "multisource")
		|| FClassnameIs(pentKeyvalue, "env_global")
		)
	{		
		if (FStrEq(pkvd->szKeyName, "master"))
		{
			NAV_SetPrototypeMaster(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "targetname"))
		{
			NAV_SetPrototypeName(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "lip"))
		{
			NAV_SetPrototypeLip(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "wait"))
		{
			NAV_SetPrototypeWait(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "delay"))
		{
			NAV_SetPrototypeDelay(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "height"))
		{
			NAV_SetPrototypeHeight(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "target"))
		{
			NAV_AddPrototypeTarget(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "health"))
		{
			NAV_SetPrototypeHealth(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "globalstate"))
		{
			NAV_SetPrototypeGlobalState(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "triggermode"))
		{
			NAV_SetPrototypeTriggerMode(EntityIndex, pkvd->szValue);
		}
		else if (FStrEq(pkvd->szKeyName, "initialstate"))
		{
			NAV_SetPrototypeTriggerState(EntityIndex, pkvd->szValue);
		}
	}

	RETURN_META(MRES_IGNORED);
}

void StartFrame(void)
{
	AIMGR_UpdateAISystem();

	RETURN_META(MRES_IGNORED);
}

static DLL_FUNCTIONS gFunctionTable =
{
	GameDLLInit,			// pfnGameInit
	Spawn,					// pfnSpawn
	NULL,					// pfnThink
	SV_Use,					// pfnUse
	NULL,					// pfnTouch
	NULL,					// pfnBlocked
	KeyValue,				// pfnKeyValue
	NULL,					// pfnSave
	NULL,					// pfnRestore
	NULL,					// pfnSetAbsBox

	NULL,					// pfnSaveWriteFields
	NULL,					// pfnSaveReadFields

	NULL,					// pfnSaveGlobalState
	NULL,					// pfnRestoreGlobalState
	NULL,					// pfnResetGlobalState

	ClientConnect,			// pfnClientConnect
	ClientDisconnect,		// pfnClientDisconnect
	NULL,					// pfnClientKill
	ClientPutInServer,		// pfnClientPutInServer
	ClientCommand,			// pfnClientCommand
	NULL,					// pfnClientUserInfoChanged
	NULL,					// pfnServerActivate
	NULL,					// pfnServerDeactivate

	NULL,					// pfnPlayerPreThink
	NULL,					// pfnPlayerPostThink

	StartFrame,				// pfnStartFrame
	NULL,					// pfnParmsNewLevel
	NULL,					// pfnParmsChangeLevel

	NULL,					// pfnGetGameDescription
	NULL,					// pfnPlayerCustomization

	NULL,					// pfnSpectatorConnect
	NULL,					// pfnSpectatorDisconnect
	NULL,					// pfnSpectatorThink

	NULL,					// pfnSys_Error

	NULL,					// pfnPM_Move
	NULL,					// pfnPM_Init
	NULL,					// pfnPM_FindTextureType

	NULL,					// pfnSetupVisibility
	NULL,					// pfnUpdateClientData
	NULL,					// pfnAddToFullPack
	NULL,					// pfnCreateBaseline
	NULL,					// pfnRegisterEncoders
	NULL,					// pfnGetWeaponData
	NULL,					// pfnCmdStart
	NULL,					// pfnCmdEnd
	NULL,					// pfnConnectionlessPacket
	NULL,					// pfnGetHullBounds
	NULL,					// pfnCreateInstancedBaselines
	NULL,					// pfnInconsistentFile
	NULL,					// pfnAllowLagCompensation
};

C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, 
		int *interfaceVersion)
{
	if(!pFunctionTable) {
		UTIL_LogPrintf("GetEntityAPI2 called with null pFunctionTable");
		return(FALSE);
	}
	else if(*interfaceVersion != INTERFACE_VERSION) {
		UTIL_LogPrintf("GetEntityAPI2 version mismatch; requested=%d ours=%d", *interfaceVersion, INTERFACE_VERSION);
		//! Tell metamod what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return(FALSE);
	}
	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return(TRUE);
}

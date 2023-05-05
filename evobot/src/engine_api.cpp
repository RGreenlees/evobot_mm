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

#include <extdll.h>

#include <meta_api.h>

#include "game_state.h"
#include "bot_client.h"
#include "bot_config.h"
#include "bot_util.h"

extern bot_t bots[32];
extern char g_argv[1024];
extern bool isFakeClientCommand;
extern int fake_arg_count;


void(*botMsgFunction)(void*, int) = NULL;
void(*botMsgEndFunction)(void*, int) = NULL;
int botMsgIndex;

void pfnChangeLevel(char* s1, char* s2)
{
	GAME_RemoveAllBots();

	RETURN_META(MRES_IGNORED);
}

edict_t* pfnFindEntityByString(edict_t* pEdictStartSearchAfter, const char* pszField, const char* pszValue)
{

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}


void pfnEmitSound(edict_t* entity, int channel, const char* sample, /*int*/float volume, float attenuation, int fFlags, int pitch)
{

	RETURN_META(MRES_IGNORED);
}


void pfnClientCommand(edict_t* pEdict, char* szFmt, ...)
{
	if ((pEdict->v.flags & FL_FAKECLIENT) || (pEdict->v.flags & FL_THIRDPARTYBOT))
		RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

void pfnMessageBegin(int msg_dest, int msg_type, const float* pOrigin, edict_t* ed)
{
	if (gpGlobals->deathmatch)
	{
		int index = -1;

		botMsgFunction = NULL;     // no msg function until known otherwise
		botMsgEndFunction = NULL;  // no msg end function until known otherwise
		botMsgIndex = -1;       // index of bot receiving message

		if (ed)
		{
			index = GetBotIndex(ed);

			// is this message for a bot?
			if (index != -1)
			{
				botMsgIndex = index;       // index of bot receiving message

				if (msg_type == GET_USER_MSG_ID(PLID, "SetOrder", NULL))
				{
					botMsgFunction = BotClient_NS_ReceiveOrder;
				}

				// Network messages have been modified slightly between 3.2 and 3.3
				if (msg_type == GET_USER_MSG_ID(PLID, "PlayHUDNot", NULL))
				{
					if (CONFIG_GetNSVersion() == 33)
					{
						botMsgFunction = BotClient_NS_Alert_33;
					}
					else
					{
						botMsgFunction = BotClient_NS_Alert_32;
					}
				}

				if (msg_type == GET_USER_MSG_ID(PLID, "SetupMap", NULL))
					botMsgFunction = BotClient_NS_SetupMap;

				if (msg_type == GET_USER_MSG_ID(PLID, "SetSelect", NULL))
					botMsgFunction = BotClient_NS_SetSelect;

				if (msg_type == GET_USER_MSG_ID(PLID, "AmmoX", NULL))
					botMsgFunction = BotClient_Valve_AmmoX;

				if (msg_type == GET_USER_MSG_ID(PLID, "WeaponList", NULL))
					botMsgFunction = BotClient_Valve_WeaponList;

				if (msg_type == GET_USER_MSG_ID(PLID, "CurWeapon", NULL))
					botMsgFunction = BotClient_Valve_CurrentWeapon;

				// Will cause a crash if not parsing for the correct version of NS
				if (msg_type == GET_USER_MSG_ID(PLID, "AlienInfo", NULL))
				{
					if (CONFIG_GetNSVersion() == 33)
					{
						botMsgFunction = BotClient_NS_AlienInfo_33;
					}
					else
					{
						botMsgFunction = BotClient_NS_AlienInfo_32;
					}
				}

				if (msg_type == GET_USER_MSG_ID(PLID, "Damage", NULL))
					botMsgFunction = BotClient_NS_Damage;

			}

		}
		else if (msg_dest == MSG_ALL)
		{
			botMsgFunction = NULL;  // no msg function until known otherwise
			botMsgIndex = -1;       // index of bot receiving message (none)

			if (msg_type == GET_USER_MSG_ID(PLID, "DeathMsg", NULL))
				botMsgFunction = BotClient_NS_DeathMsg;

			if (msg_type == GET_USER_MSG_ID(PLID, "GameStatus", NULL))
				botMsgFunction = BotClient_NS_GameStatus;

			if (msg_type == GET_USER_MSG_ID(PLID, "SetupMap", NULL))
			{
				botMsgFunction = BotClient_NS_SetupMap;
			}

		}
		else
		{
			// Steam makes the WeaponList message be sent differently

			botMsgFunction = NULL;  // no msg function until known otherwise
			botMsgIndex = -1;       // index of bot receiving message (none)


			if (msg_type == GET_USER_MSG_ID(PLID, "WeaponList", NULL))
				botMsgFunction = BotClient_Valve_WeaponList;

			if (msg_type == GET_USER_MSG_ID(PLID, "GameStatus", NULL))
				botMsgFunction = BotClient_NS_GameStatus;

			if (msg_type == GET_USER_MSG_ID(PLID, "SetupMap", NULL))
				botMsgFunction = BotClient_NS_SetupMap;
		}
	}

	RETURN_META(MRES_IGNORED);
}


void pfnMessageEnd(void)
{
	if (gpGlobals->deathmatch)
	{
		if (botMsgEndFunction)
			(*botMsgEndFunction)(NULL, botMsgIndex);  // NULL indicated msg end

		// clear out the bot message function pointers...
		botMsgFunction = NULL;
		botMsgEndFunction = NULL;
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteByte(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void*)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteChar(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void*)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteShort(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void*)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteLong(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void*)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteAngle(float flValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void*)&flValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteCoord(float flValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void*)&flValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteString(const char* sz)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void*)sz, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteEntity(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void*)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnClientPrintf(edict_t* pEdict, PRINT_TYPE ptype, const char* szMsg)
{
	if ((pEdict->v.flags & FL_FAKECLIENT) || (pEdict->v.flags & FL_THIRDPARTYBOT))
		RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}


const char* pfnCmd_Args(void)
{
	if (isFakeClientCommand)
		RETURN_META_VALUE(MRES_SUPERCEDE, &g_argv[0]);

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}


const char* pfnCmd_Argv(int argc)
{
	if (isFakeClientCommand)
	{
		if (argc == 0)
			RETURN_META_VALUE(MRES_SUPERCEDE, &g_argv[64]);
		else if (argc == 1)
			RETURN_META_VALUE(MRES_SUPERCEDE, &g_argv[128]);
		else if (argc == 2)
			RETURN_META_VALUE(MRES_SUPERCEDE, &g_argv[192]);
		else
			RETURN_META_VALUE(MRES_SUPERCEDE, NULL);
	}

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}


int pfnCmd_Argc(void)
{
	if (isFakeClientCommand)
		RETURN_META_VALUE(MRES_SUPERCEDE, fake_arg_count);

	RETURN_META_VALUE(MRES_IGNORED, 0);
}


void pfnSetClientMaxspeed(const edict_t* pEdict, float fNewMaxspeed)
{
	/*int index;

	index = UTIL_GetBotIndex((edict_t *)pEdict);

	// is this message for a bot?
	if (index != -1)
		bots[index].f_max_speed = fNewMaxspeed;

	RETURN_META(MRES_IGNORED);*/

	((edict_t*)pEdict)->v.maxspeed = fNewMaxspeed;
	RETURN_META(MRES_IGNORED);
}


int pfnGetPlayerUserId(edict_t* e)
{
	RETURN_META_VALUE(MRES_IGNORED, 0);
}


const char* pfnGetPlayerAuthId(edict_t* e)
{
	if ((e->v.flags & FL_FAKECLIENT) || (e->v.flags & FL_THIRDPARTYBOT))
		RETURN_META_VALUE(MRES_SUPERCEDE, "0");

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}

enginefuncs_t meta_engfuncs = 
{
	NULL,						// pfnPrecacheModel()
	NULL,						// pfnPrecacheSound()
	NULL,						// pfnSetModel()
	NULL,						// pfnModelIndex()
	NULL,						// pfnModelFrames()

	NULL,						// pfnSetSize()
	pfnChangeLevel,				// pfnChangeLevel()
	NULL,						// pfnGetSpawnParms()
	NULL,						// pfnSaveSpawnParms()

	NULL,						// pfnVecToYaw()
	NULL,						// pfnVecToAngles()
	NULL,						// pfnMoveToOrigin()
	NULL,						// pfnChangeYaw()
	NULL,						// pfnChangePitch()

	pfnFindEntityByString,		// pfnFindEntityByString()
	NULL,						// pfnGetEntityIllum()
	NULL,						// pfnFindEntityInSphere()
	NULL,						// pfnFindClientInPVS()
	NULL,						// pfnEntitiesInPVS()

	NULL,						// pfnMakeVectors()
	NULL,						// pfnAngleVectors()

	NULL,						// pfnCreateEntity()
	NULL,						// pfnRemoveEntity()
	NULL,						// pfnCreateNamedEntity()

	NULL,						// pfnMakeStatic()
	NULL,						// pfnEntIsOnFloor()
	NULL,						// pfnDropToFloor()

	NULL,						// pfnWalkMove()
	NULL,						// pfnSetOrigin()

	pfnEmitSound,				// pfnEmitSound()
	NULL,						// pfnEmitAmbientSound()

	NULL,						// pfnTraceLine()
	NULL,						// pfnTraceToss()
	NULL,						// pfnTraceMonsterHull()
	NULL,						// pfnTraceHull()
	NULL,						// pfnTraceModel()
	NULL,						// pfnTraceTexture()
	NULL,						// pfnTraceSphere()
	NULL,						// pfnGetAimVector()

	NULL,						// pfnServerCommand()
	NULL,						// pfnServerExecute()
	pfnClientCommand,			// pfnClientCommand()

	NULL,						// pfnParticleEffect()
	NULL,						// pfnLightStyle()
	NULL,						// pfnDecalIndex()
	NULL,						// pfnPointContents()

	pfnMessageBegin,			// pfnMessageBegin()
	pfnMessageEnd,				// pfnMessageEnd()

	pfnWriteByte,				// pfnWriteByte()
	pfnWriteChar,				// pfnWriteChar()
	pfnWriteShort,				// pfnWriteShort()
	pfnWriteLong,				// pfnWriteLong()
	pfnWriteAngle,				// pfnWriteAngle()
	pfnWriteCoord,				// pfnWriteCoord()
	pfnWriteString,				// pfnWriteString()
	pfnWriteEntity,				// pfnWriteEntity()

	NULL,						// pfnCVarRegister()
	NULL,						// pfnCVarGetFloat()
	NULL,						// pfnCVarGetString()
	NULL,						// pfnCVarSetFloat()
	NULL,						// pfnCVarSetString()

	NULL,						// pfnAlertMessage()
	NULL,						// pfnEngineFprintf()

	NULL,						// pfnPvAllocEntPrivateData()
	NULL,						// pfnPvEntPrivateData()
	NULL,						// pfnFreeEntPrivateData()

	NULL,						// pfnSzFromIndex()
	NULL,						// pfnAllocString()

	NULL, 						// pfnGetVarsOfEnt()
	NULL,						// pfnPEntityOfEntOffset()
	NULL,						// pfnEntOffsetOfPEntity()
	NULL,						// pfnIndexOfEdict()
	NULL,						// pfnPEntityOfEntIndex()
	NULL,						// pfnFindEntityByVars()
	NULL,						// pfnGetModelPtr()

	NULL,						// pfnRegUserMsg()

	NULL,						// pfnAnimationAutomove()
	NULL,						// pfnGetBonePosition()

	NULL,						// pfnFunctionFromName()
	NULL,						// pfnNameForFunction()

	pfnClientPrintf,			// pfnClientPrintf()
	NULL,						// pfnServerPrint()

	pfnCmd_Args,			// pfnCmd_Args()
	pfnCmd_Argv,				// pfnCmd_Argv()
	pfnCmd_Argc,				// pfnCmd_Argc()

	NULL,						// pfnGetAttachment()

	NULL,						// pfnCRC32_Init()
	NULL,						// pfnCRC32_ProcessBuffer()
	NULL,						// pfnCRC32_ProcessByte()
	NULL,						// pfnCRC32_Final()

	NULL,						// pfnRandomLong()
	NULL,						// pfnRandomFloat()

	NULL,						// pfnSetView()
	NULL,						// pfnTime()
	NULL,						// pfnCrosshairAngle()

	NULL,						// pfnLoadFileForMe()
	NULL,						// pfnFreeFile()

	NULL,						// pfnEndSection()
	NULL,						// pfnCompareFileTime()
	NULL,						// pfnGetGameDir()
	NULL,						// pfnCvar_RegisterVariable()
	NULL,						// pfnFadeClientVolume()
	pfnSetClientMaxspeed,		// pfnSetClientMaxspeed()
	NULL,						// pfnCreateFakeClient()
	NULL,						// pfnRunPlayerMove()
	NULL,						// pfnNumberOfEntities()

	NULL,						// pfnGetInfoKeyBuffer()
	NULL,						// pfnInfoKeyValue()
	NULL,						// pfnSetKeyValue()
	NULL,						// pfnSetClientKeyValue()

	NULL,						// pfnIsMapValid()
	NULL,						// pfnStaticDecal()
	NULL,						// pfnPrecacheGeneric()
	pfnGetPlayerUserId,			// pfnGetPlayerUserId()
	NULL,						// pfnBuildSoundMsg()
	NULL,						// pfnIsDedicatedServer()
	NULL,						// pfnCVarGetPointer()
	NULL,						// pfnGetPlayerWONId()

	NULL,						// pfnInfo_RemoveKey()
	NULL,						// pfnGetPhysicsKeyValue()
	NULL,						// pfnSetPhysicsKeyValue()
	NULL,						// pfnGetPhysicsInfoString()
	NULL,						// pfnPrecacheEvent()
	NULL,						// pfnPlaybackEvent()

	NULL,						// pfnSetFatPVS()
	NULL,						// pfnSetFatPAS()

	NULL,						// pfnCheckVisibility()

	NULL,						// pfnDeltaSetField()
	NULL,						// pfnDeltaUnsetField()
	NULL,						// pfnDeltaAddEncoder()
	NULL,						// pfnGetCurrentPlayer()
	NULL,						// pfnCanSkipPlayer()
	NULL,						// pfnDeltaFindField()
	NULL,						// pfnDeltaSetFieldByIndex()
	NULL,						// pfnDeltaUnsetFieldByIndex()

	NULL,						// pfnSetGroupMask()

	NULL,						// pfnCreateInstancedBaseline()
	NULL,						// pfnCvar_DirectSet()

	NULL,						// pfnForceUnmodified()

	NULL,						// pfnGetPlayerStats()

	NULL,						// pfnAddServerCommand()

	// Added in SDK 2.2:
	NULL,						// pfnVoice_GetClientListening()
	NULL,						// pfnVoice_SetClientListening()

	// Added for HL 1109 (no SDK update):
	pfnGetPlayerAuthId,			// pfnGetPlayerAuthId()

	// Added 2003/11/10 (no SDK update):
	NULL,						// pfnSequenceGet()
	NULL,						// pfnSequencePickSentence()
	NULL,						// pfnGetFileSize()
	NULL,						// pfnGetApproxWavePlayLen()
	NULL,						// pfnIsCareerMatch()
	NULL,						// pfnGetLocalizedStringLength()
	NULL,						// pfnRegisterTutorMessageShown()
	NULL,						// pfnGetTimesTutorMessageShown()
	NULL,						// pfnProcessTutorMessageDecayBuffer()
	NULL,						// pfnConstructTutorMessageDecayBuffer()
	NULL,						// pfnResetTutorMessageDecayData()

	// Added Added 2005-08-11 (no SDK update)
	NULL,						// pfnQueryClientCvarValue()
	// Added Added 2005-11-22 (no SDK update)
	NULL,						// pfnQueryClientCvarValue2()
	// Added 2009-06-17 (no SDK update)
	NULL,						// pfnEngCheckParm()
};

C_DLLEXPORT int GetEngineFunctions(enginefuncs_t *pengfuncsFromEngine, 
		int *interfaceVersion) 
{
	if(!pengfuncsFromEngine) {
		UTIL_LogPrintf("GetEngineFunctions called with null pengfuncsFromEngine");
		return(FALSE);
	}
	else if(*interfaceVersion != ENGINE_INTERFACE_VERSION) {
		UTIL_LogPrintf("GetEngineFunctions version mismatch; requested=%d ours=%d", *interfaceVersion, ENGINE_INTERFACE_VERSION);
		// Tell metamod what version we had, so it can figure out who is out of date.
		*interfaceVersion = ENGINE_INTERFACE_VERSION;
		return(FALSE);
	}
	memcpy(pengfuncsFromEngine, &meta_engfuncs, sizeof(enginefuncs_t));
	return(TRUE);
}

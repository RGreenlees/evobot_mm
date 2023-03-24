//
// HPB bot - botman's High Ping Bastard bot
//
// (http://planethalflife.com/botman/)
//
// engine.cpp
//

#ifndef _WIN32
#include <string.h>
#endif

#include <extdll.h>
#include <dllapi.h>
#include <h_export.h>
#include <meta_api.h>

#include "bot.h"
#include "bot_client.h"
#include "bot_func.h"
#include "bot_config.h"

extern enginefuncs_t g_engfuncs;
extern bot_t bots[32];
extern char g_argv[1024];
extern bool isFakeClientCommand;
extern int fake_arg_count;


void(*botMsgFunction)(void *, int) = NULL;
void(*botMsgEndFunction)(void *, int) = NULL;
int botMsgIndex;

static FILE *fp;


void pfnChangeLevel(char* s1, char* s2)
{
	// kick any bot off of the server after time/frag limit...
	for (int index = 0; index < 32; index++)
	{
		if (bots[index].is_used)  // is this slot used?
		{
			char cmd[50];

			sprintf(cmd, "kick \"%s\"\n", bots[index].name);

			bots[index].respawn_state = RESPAWN_NEED_TO_RESPAWN;

			SERVER_COMMAND(cmd);  // kick the bot using (kick "name")
		}
	}

	RETURN_META(MRES_IGNORED);
}


edict_t* pfnFindEntityByString(edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue)
{
	if (gpGlobals->deathmatch)
	{
		int bot_index;
		bot_t *pBot;


			// new round in CS 1.5
			if (strcmp("info_map_parameters", pszValue) == 0)
			{
				for (bot_index = 0; bot_index < 32; bot_index++)
				{
					pBot = &bots[bot_index];

					if (pBot->is_used)
						BotSpawnInit(pBot); // reset bots for new round
				}
			}
		
	}

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}


void pfnEmitSound(edict_t *entity, int channel, const char *sample, /*int*/float volume, float attenuation, int fFlags, int pitch)
{

	RETURN_META(MRES_IGNORED);
}


void pfnClientCommand(edict_t* pEdict, char* szFmt, ...)
{
	if ((pEdict->v.flags & FL_FAKECLIENT) || (pEdict->v.flags & FL_THIRDPARTYBOT))
		RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

void pfnMessageBegin(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
{
	if (gpGlobals->deathmatch)
	{
		int index = -1;

		if (ed)
		{
			index = UTIL_GetBotIndex(ed);

			// is this message for a bot?
			if (index != -1)
			{
				botMsgFunction = NULL;     // no msg function until known otherwise
				botMsgEndFunction = NULL;  // no msg end function until known otherwise
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

			else if (msg_type == GET_USER_MSG_ID(PLID, "HLTV", NULL))
				botMsgFunction = BotClient_CS_HLTV;

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
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteChar(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteShort(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteLong(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteAngle(float flValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&flValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteCoord(float flValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&flValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteString(const char *sz)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)sz, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnWriteEntity(int iValue)
{
	if (gpGlobals->deathmatch)
	{
		// if this message is for a bot, call the client message function...
		if (botMsgFunction)
			(*botMsgFunction)((void *)&iValue, botMsgIndex);
	}

	RETURN_META(MRES_IGNORED);
}


void pfnClientPrintf(edict_t* pEdict, PRINT_TYPE ptype, const char *szMsg)
{
	if ((pEdict->v.flags & FL_FAKECLIENT) || (pEdict->v.flags & FL_THIRDPARTYBOT))
		RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}


const char *pfnCmd_Args(void)
{
	if (isFakeClientCommand)
		RETURN_META_VALUE(MRES_SUPERCEDE, &g_argv[0]);

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}


const char *pfnCmd_Argv(int argc)
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


void pfnSetClientMaxspeed(const edict_t *pEdict, float fNewMaxspeed)
{
	/*int index;

	index = UTIL_GetBotIndex((edict_t *)pEdict);

	// is this message for a bot?
	if (index != -1)
		bots[index].f_max_speed = fNewMaxspeed;

	RETURN_META(MRES_IGNORED);*/

	((edict_t *)pEdict)->v.maxspeed = fNewMaxspeed;
	RETURN_META(MRES_IGNORED);
}


int pfnGetPlayerUserId(edict_t *e)
{
	RETURN_META_VALUE(MRES_IGNORED, 0);
}


const char *pfnGetPlayerAuthId(edict_t *e)
{
	if ((e->v.flags & FL_FAKECLIENT) || (e->v.flags & FL_THIRDPARTYBOT))
		RETURN_META_VALUE(MRES_SUPERCEDE, "0");

	RETURN_META_VALUE(MRES_IGNORED, NULL);
}


C_DLLEXPORT int GetEngineFunctions(enginefuncs_t *pengfuncsFromEngine, int *interfaceVersion)
{
	
	meta_engfuncs.pfnChangeLevel = pfnChangeLevel;
	meta_engfuncs.pfnFindEntityByString = pfnFindEntityByString;
	meta_engfuncs.pfnEmitSound = pfnEmitSound;
	meta_engfuncs.pfnClientCommand = pfnClientCommand;
	meta_engfuncs.pfnMessageBegin = pfnMessageBegin;
	meta_engfuncs.pfnMessageEnd = pfnMessageEnd;
	meta_engfuncs.pfnWriteByte = pfnWriteByte;
	meta_engfuncs.pfnWriteChar = pfnWriteChar;
	meta_engfuncs.pfnWriteShort = pfnWriteShort;
	meta_engfuncs.pfnWriteLong = pfnWriteLong;
	meta_engfuncs.pfnWriteAngle = pfnWriteAngle;
	meta_engfuncs.pfnWriteCoord = pfnWriteCoord;
	meta_engfuncs.pfnWriteString = pfnWriteString;
	meta_engfuncs.pfnWriteEntity = pfnWriteEntity;
	meta_engfuncs.pfnClientPrintf = pfnClientPrintf;
	meta_engfuncs.pfnCmd_Args = pfnCmd_Args;
	meta_engfuncs.pfnCmd_Argv = pfnCmd_Argv;
	meta_engfuncs.pfnCmd_Argc = pfnCmd_Argc;
	meta_engfuncs.pfnSetClientMaxspeed = pfnSetClientMaxspeed;
	meta_engfuncs.pfnGetPlayerUserId = pfnGetPlayerUserId;
	meta_engfuncs.pfnGetPlayerAuthId = pfnGetPlayerAuthId;

	memcpy(pengfuncsFromEngine, &meta_engfuncs, sizeof(enginefuncs_t));
	return true;
}

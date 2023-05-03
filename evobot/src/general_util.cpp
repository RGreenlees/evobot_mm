// vi: set ts=4 sw=4 :
// vim: set tw=75 :

// Selected portions of dlls/util.cpp from SDK 2.1.
// Functions copied from there as needed...
// And modified to avoid buffer overflows (argh).

/***
*
*	Copyright (c) 1999, 2000 Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== util.cpp ========================================================

  Utility code.  Really not optional after all.

*/

#include "general_util.h"

#ifndef _WIN32
#include <string.h>
#endif

#include <enginecallback.h>		// ALERT()
#include "osdep.h"				// win32 vsnprintf, etc

#include <dllapi.h>
#include <meta_api.h>

#include "bot_math.h"
#include "player_util.h"
#include "bot_tactical.h"

int m_spriteTexture;


void GetGameDir(char* game_dir)
{
	unsigned char length, fieldstart, fieldstop;

	GET_GAME_DIR(game_dir); // call the engine macro and let it mallocate for the char pointer

	length = (unsigned char)strlen(game_dir); // get the length of the returned string
	length--; // ignore the trailing string terminator

	// format the returned string to get the last directory name
	fieldstop = length;
	while (((game_dir[fieldstop] == '\\') || (game_dir[fieldstop] == '/')) && (fieldstop > 0))
		fieldstop--; // shift back any trailing separator

	fieldstart = fieldstop;
	while ((game_dir[fieldstart] != '\\') && (game_dir[fieldstart] != '/') && (fieldstart > 0))
		fieldstart--; // shift back to the start of the last subdirectory name

	if ((game_dir[fieldstart] == '\\') || (game_dir[fieldstart] == '/'))
		fieldstart++; // if we reached a separator, step over it

	// now copy the formatted string back onto itself character per character
	for (length = fieldstart; length <= fieldstop; length++)
		game_dir[length - fieldstart] = game_dir[length];
	game_dir[length - fieldstart] = 0; // terminate the string

	return;
}

void UTIL_BuildFileName(char* filename, const char* arg1, const char* arg2, const char* arg3, const char* arg4)
{
	filename[0] = 0;

	GetGameDir(filename);


	if (arg1 != NULL)
	{
		if (*arg1)
		{
			strcat(filename, "/");
			strcat(filename, arg1);
		}
	}

	if (arg2 != NULL)
	{
		if (*arg2)
		{
			strcat(filename, "/");
			strcat(filename, arg2);
		}
	}

	if (arg3 != NULL)
	{
		if (*arg3)
		{
			strcat(filename, "/");
			strcat(filename, arg3);
		}
	}

	if (arg4 != NULL)
	{
		if (*arg4)
		{
			strcat(filename, "/");
			strcat(filename, arg4);
		}
	}
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end)
{
	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(1);               // framestart
	WRITE_BYTE(10);              // framerate
	WRITE_BYTE(1);              // life in 0.1's
	WRITE_BYTE(5);           // width
	WRITE_BYTE(0);           // noise

	WRITE_BYTE(255);             // r, g, b
	WRITE_BYTE(255);           // r, g, b
	WRITE_BYTE(255);            // r, g, b

	WRITE_BYTE(250);      // brightness
	WRITE_BYTE(5);           // speed
	MESSAGE_END();
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds)
{
	int timeTenthSeconds = (int)floorf(drawTimeSeconds * 10.0f);
	timeTenthSeconds = fmaxf(timeTenthSeconds, 1);

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(1);               // framestart
	WRITE_BYTE(10);              // framerate
	WRITE_BYTE(timeTenthSeconds);              // life in 0.1's
	WRITE_BYTE(5);           // width
	WRITE_BYTE(0);           // noise

	WRITE_BYTE(255);             // r, g, b
	WRITE_BYTE(255);           // r, g, b
	WRITE_BYTE(255);            // r, g, b

	WRITE_BYTE(250);      // brightness
	WRITE_BYTE(5);           // speed
	MESSAGE_END();
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds, int r, int g, int b)
{
	int timeTenthSeconds = (int)floorf(drawTimeSeconds * 10.0f);

	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(1);               // framestart
	WRITE_BYTE(10);              // framerate
	WRITE_BYTE(timeTenthSeconds);              // life in 0.1's
	WRITE_BYTE(5);           // width
	WRITE_BYTE(0);           // noise

	WRITE_BYTE(r);             // r, g, b
	WRITE_BYTE(g);           // r, g, b
	WRITE_BYTE(b);            // r, g, b

	WRITE_BYTE(250);      // brightness
	WRITE_BYTE(5);           // speed
	MESSAGE_END();
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, int r, int g, int b)
{
	MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	WRITE_SHORT(m_spriteTexture);
	WRITE_BYTE(1);               // framestart
	WRITE_BYTE(10);              // framerate
	WRITE_BYTE(1);              // life in 0.1's
	WRITE_BYTE(5);           // width
	WRITE_BYTE(0);           // noise

	WRITE_BYTE(r);             // r, g, b
	WRITE_BYTE(g);           // r, g, b
	WRITE_BYTE(b);            // r, g, b

	WRITE_BYTE(250);      // brightness
	WRITE_BYTE(5);           // speed
	MESSAGE_END();
}

bool UTIL_QuickTrace(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, ignore_monsters, ignore_glass, IgnoreEdict, &hit);
	return (hit.flFraction >= 1.0f || hit.fStartSolid > 0);
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	int hullNum = 0;// GetPlayerHullIndex(pEdict);
	TraceResult hit;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, pEdict->v.pContainingEntity, &hit);

	return (hit.flFraction >= 1.0f);
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum)
{
	TraceResult hit;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, pEdict->v.pContainingEntity, &hit);

	return (hit.flFraction >= 1.0f);
}

edict_t* UTIL_TraceEntity(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, dont_ignore_monsters, ignore_glass, IgnoreEdict, &hit);
	return hit.pHit;
}

edict_t* UTIL_FindEntityInSphere(edict_t* pentStart, const Vector& vecCenter, float flRadius)
{
	edict_t* pentEntity;

	pentEntity = FIND_ENTITY_IN_SPHERE(pentStart, vecCenter, flRadius);

	if (!FNullEnt(pentEntity))
		return pentEntity;

	return NULL;
}


edict_t* UTIL_FindEntityByString(edict_t* pentStart, const char* szKeyword, const char* szValue)
{
	edict_t* pentEntity;

	pentEntity = FIND_ENTITY_BY_STRING(pentStart, szKeyword, szValue);

	if (!FNullEnt(pentEntity))
		return pentEntity;
	return NULL;
}


edict_t* UTIL_FindEntityByClassname(edict_t* pentStart, const char* szName)
{
	return UTIL_FindEntityByString(pentStart, "classname", szName);
}

edict_t* UTIL_FindEntityByTarget(edict_t* pentStart, const char* szName)
{
	return UTIL_FindEntityByString(pentStart, "target", szName);
}


edict_t* UTIL_FindEntityByTargetname(edict_t* pentStart, const char* szName)
{
	return UTIL_FindEntityByString(pentStart, "targetname", szName);
}

void ClientPrint(edict_t* pEntity, int msg_dest, const char* msg_name)
{
	if (FNullEnt(pEntity)) { return; }

	if (GET_USER_MSG_ID(PLID, "TextMsg", NULL) <= 0)
		REG_USER_MSG("TextMsg", -1);

	MESSAGE_BEGIN(MSG_ONE, GET_USER_MSG_ID(PLID, "TextMsg", NULL), NULL, pEntity);

	WRITE_BYTE(msg_dest);
	WRITE_STRING(msg_name);
	MESSAGE_END();
}

void UTIL_SayText(const char* pText, edict_t* pEdict)
{
	if (GET_USER_MSG_ID(PLID, "SayText", NULL) <= 0)
		REG_USER_MSG("SayText", -1);

	MESSAGE_BEGIN(MSG_ONE, GET_USER_MSG_ID(PLID, "SayText", NULL), NULL, pEdict);
	WRITE_BYTE(ENTINDEX(pEdict));
	WRITE_STRING(pText);
	MESSAGE_END();
}


void UTIL_HostSay(edict_t* pEntity, int teamonly, char* message)
{
	int   j;
	char  text[128];
	char* pc;
	int   sender_team, player_team;
	edict_t* client;

	// make sure the text has content
	for (pc = message; pc != NULL && *pc != 0; pc++)
	{
		if (isprint(*pc) && !isspace(*pc))
		{
			pc = NULL;   // we've found an alphanumeric character,  so text is valid
			break;
		}
	}

	if (pc != NULL)
		return;  // no character found, so say nothing

	// turn on color set 2  (color on,  no sound)
	if (teamonly)
		sprintf(text, "%c(TEAM) %s: ", 2, STRING(pEntity->v.netname));
	else
		sprintf(text, "%c%s: ", 2, STRING(pEntity->v.netname));

	j = sizeof(text) - 2 - (int)strlen(text);  // -2 for /n and null terminator
	if ((int)strlen(message) > j)
		message[j] = 0;

	strcat(text, message);
	strcat(text, "\n");

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	if (GET_USER_MSG_ID(PLID, "SayText", NULL) <= 0)
		REG_USER_MSG("SayText", -1);

	sender_team = pEntity->v.team;

	client = NULL;
	while (((client = UTIL_FindEntityByClassname(client, "player")) != NULL) &&
		(!FNullEnt(client)))
	{
		if (client == pEntity)  // skip sender of message
			continue;

		player_team = client->v.team;

		if (teamonly && (sender_team != player_team))
			continue;

		MESSAGE_BEGIN(MSG_ONE, GET_USER_MSG_ID(PLID, "SayText", NULL), NULL, client);
		WRITE_BYTE(ENTINDEX(pEntity));
		WRITE_STRING(text);
		MESSAGE_END();
	}

	// print to the sending client
	MESSAGE_BEGIN(MSG_ONE, GET_USER_MSG_ID(PLID, "SayText", NULL), NULL, pEntity);
	WRITE_BYTE(ENTINDEX(pEntity));
	WRITE_STRING(text);
	MESSAGE_END();

	// echo to server console
	SERVER_PRINT(text);
}

Vector UTIL_GetEntityGroundLocation(const edict_t* pEntity)
{
	
	if (FNullEnt(pEntity)) { return ZERO_VECTOR; }

	bool bIsPlayer = IsEdictPlayer(pEntity);

	if (bIsPlayer)
	{
		if (IsPlayerOnLadder(pEntity))
		{
			return UTIL_GetFloorUnderEntity(pEntity);
		}

		if (pEntity->v.flags & FL_ONGROUND)
		{
			if (FNullEnt(pEntity->v.groundentity))
			{
				return GetPlayerBottomOfCollisionHull(pEntity);
			}

			if (!IsEdictPlayer(pEntity->v.groundentity) && GetStructureTypeFromEdict(pEntity->v.groundentity) == STRUCTURE_NONE)
			{
				return GetPlayerBottomOfCollisionHull(pEntity);
			}
		}

		return UTIL_GetFloorUnderEntity(pEntity);
	}

	if (GetStructureTypeFromEdict(pEntity) == STRUCTURE_ALIEN_HIVE)
	{
		return UTIL_GetFloorUnderEntity(pEntity);
	}

	return GetPlayerBottomOfCollisionHull(pEntity);
}

Vector UTIL_GetCentreOfEntity(const edict_t* Entity)
{
	if (!Entity) { return ZERO_VECTOR; }

	return (Entity->v.absmin + (Entity->v.size * 0.5f));
}

Vector UTIL_GetFloorUnderEntity(const edict_t* Edict)
{
	if (FNullEnt(Edict)) { return ZERO_VECTOR; }

	TraceResult hit;
	UTIL_TraceHull(Edict->v.origin, (Edict->v.origin - Vector(0.0f, 0.0f, 1000.0f)), ignore_monsters, GetPlayerHullIndex(Edict), Edict->v.pContainingEntity, &hit);

	if (hit.flFraction < 1.0f)
	{
		return (hit.vecEndPos + Vector(0.0f, 0.0f, 1.0f));
	}

	return Edict->v.origin;
}

Vector UTIL_GetTraceHitLocation(const Vector Start, const Vector End)
{
	TraceResult hit;
	UTIL_TraceLine(Start, End, ignore_monsters, ignore_glass, NULL, &hit);

	if (hit.flFraction < 1.0f)
	{
		return hit.vecEndPos;
	}

	return ZERO_VECTOR;
}

bool IsEdictStructure(const edict_t* edict)
{
	return (GetStructureTypeFromEdict(edict) != STRUCTURE_NONE);
}
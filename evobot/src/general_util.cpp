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
#include "game_state.h"

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

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax)
{
	Vector LowerBottomLeftCorner = bMin;
	Vector LowerTopLeftCorner = Vector(bMin.x, bMax.y, bMin.z);
	Vector LowerTopRightCorner = Vector(bMax.x, bMax.y, bMin.z);
	Vector LowerBottomRightCorner = Vector(bMax.x, bMin.y, bMin.z);

	Vector UpperBottomLeftCorner = Vector(bMin.x, bMin.y, bMax.z);
	Vector UpperTopLeftCorner = Vector(bMin.x, bMax.y, bMax.z);
	Vector UpperTopRightCorner = Vector(bMax.x, bMax.y, bMax.z);
	Vector UpperBottomRightCorner = Vector(bMax.x, bMin.y, bMax.z);


	UTIL_DrawLine(pEntity, LowerTopLeftCorner, LowerTopRightCorner);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, LowerBottomRightCorner);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, LowerBottomLeftCorner);

	UTIL_DrawLine(pEntity, UpperBottomLeftCorner, UpperTopLeftCorner);
	UTIL_DrawLine(pEntity, UpperTopLeftCorner, UpperTopRightCorner);
	UTIL_DrawLine(pEntity, UpperTopRightCorner, UpperBottomRightCorner);
	UTIL_DrawLine(pEntity, UpperBottomRightCorner, UpperBottomLeftCorner);

	UTIL_DrawLine(pEntity, LowerBottomLeftCorner, UpperBottomLeftCorner);
	UTIL_DrawLine(pEntity, LowerTopLeftCorner, UpperTopLeftCorner);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, UpperTopRightCorner);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, UpperBottomRightCorner);
}

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds)
{
	Vector LowerBottomLeftCorner = bMin;
	Vector LowerTopLeftCorner = Vector(bMin.x, bMax.y, bMin.z);
	Vector LowerTopRightCorner = Vector(bMax.x, bMax.y, bMin.z);
	Vector LowerBottomRightCorner = Vector(bMax.x, bMin.y, bMin.z);

	Vector UpperBottomLeftCorner = Vector(bMin.x, bMin.y, bMax.z);
	Vector UpperTopLeftCorner = Vector(bMin.x, bMax.y, bMax.z);
	Vector UpperTopRightCorner = Vector(bMax.x, bMax.y, bMax.z);
	Vector UpperBottomRightCorner = Vector(bMax.x, bMin.y, bMax.z);


	UTIL_DrawLine(pEntity, LowerTopLeftCorner, LowerTopRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, LowerBottomRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, LowerBottomLeftCorner, drawTimeSeconds, 255, 255, 255);

	UTIL_DrawLine(pEntity, UpperBottomLeftCorner, UpperTopLeftCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, UpperTopLeftCorner, UpperTopRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, UpperTopRightCorner, UpperBottomRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, UpperBottomRightCorner, UpperBottomLeftCorner, drawTimeSeconds, 255, 255, 255);

	UTIL_DrawLine(pEntity, LowerBottomLeftCorner, UpperBottomLeftCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerTopLeftCorner, UpperTopLeftCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, UpperTopRightCorner, drawTimeSeconds, 255, 255, 255);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, UpperBottomRightCorner, drawTimeSeconds, 255, 255, 255);
}

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds, int r, int g, int b)
{
	Vector LowerBottomLeftCorner = bMin;
	Vector LowerTopLeftCorner = Vector(bMin.x, bMax.y, bMin.z);
	Vector LowerTopRightCorner = Vector(bMax.x, bMax.y, bMin.z);
	Vector LowerBottomRightCorner = Vector(bMax.x, bMin.y, bMin.z);

	Vector UpperBottomLeftCorner = Vector(bMin.x, bMin.y, bMax.z);
	Vector UpperTopLeftCorner = Vector(bMin.x, bMax.y, bMax.z);
	Vector UpperTopRightCorner = Vector(bMax.x, bMax.y, bMax.z);
	Vector UpperBottomRightCorner = Vector(bMax.x, bMin.y, bMax.z);


	UTIL_DrawLine(pEntity, LowerTopLeftCorner, LowerTopRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, LowerBottomRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, LowerBottomLeftCorner, drawTimeSeconds, r, g, b);

	UTIL_DrawLine(pEntity, UpperBottomLeftCorner, UpperTopLeftCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, UpperTopLeftCorner, UpperTopRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, UpperTopRightCorner, UpperBottomRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, UpperBottomRightCorner, UpperBottomLeftCorner, drawTimeSeconds, r, g, b);

	UTIL_DrawLine(pEntity, LowerBottomLeftCorner, UpperBottomLeftCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerTopLeftCorner, UpperTopLeftCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerTopRightCorner, UpperTopRightCorner, drawTimeSeconds, r, g, b);
	UTIL_DrawLine(pEntity, LowerBottomRightCorner, UpperBottomRightCorner, drawTimeSeconds, r, g, b);
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

void UTIL_DrawHUDText(edict_t* pEntity, char channel, float x, float y, unsigned char r, unsigned char g, unsigned char b, const char* string)
{
	

	// higher level wrapper for hudtextparms TE_TEXTMESSAGEs. This function is meant to be called
	// every frame, since the duration of the display is roughly worth the duration of a video
	// frame. The X and Y coordinates are unary fractions which are bound to this rule:
	// 0: top of the screen (Y) or left of the screen (X), left aligned text
	// 1: bottom of the screen (Y) or right of the screen (X), right aligned text
	// -1(only one negative value possible): center of the screen (X and Y), centered text
	// Any value ranging from 0 to 1 will represent a valid position on the screen.

	static short duration;

	if (FNullEnt(pEntity)) { return; }

	duration = (int)GAME_GetServerMSecVal() * 256 / 750; // compute text message duration
	if (duration < 5)
		duration = 5;

	MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, pEntity);
	WRITE_BYTE(TE_TEXTMESSAGE);
	WRITE_BYTE(channel); // channel
	WRITE_SHORT((int)(x * 8192.0f)); // x coordinates * 8192
	WRITE_SHORT((int)(y * 8192.0f)); // y coordinates * 8192
	WRITE_BYTE(0); // effect (fade in/out)
	WRITE_BYTE(r); // initial RED
	WRITE_BYTE(g); // initial GREEN
	WRITE_BYTE(b); // initial BLUE
	WRITE_BYTE(1); // initial ALPHA
	WRITE_BYTE(r); // effect RED
	WRITE_BYTE(g); // effect GREEN
	WRITE_BYTE(b); // effect BLUE
	WRITE_BYTE(1); // effect ALPHA
	WRITE_SHORT(0); // fade-in time in seconds * 256
	WRITE_SHORT(0); // fade-out time in seconds * 256
	WRITE_SHORT(duration); // hold time in seconds * 256
	WRITE_STRING(string);//string); // send the string
	MESSAGE_END(); // end

	return;
}

bool UTIL_CommanderTrace(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, ignore_monsters, ignore_glass, IgnoreEdict, &hit);
	return (hit.flFraction >= 1.0f);
}

bool UTIL_QuickTrace(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, ignore_monsters, ignore_glass, IgnoreEdict, &hit);
	return (hit.flFraction >= 1.0f && !hit.fAllSolid);
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	int hullNum = 0;// GetPlayerHullIndex(pEdict);
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	TraceResult hit;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, IgnoreEdict, &hit);

	return (hit.flFraction >= 1.0f && !hit.fAllSolid);
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, IgnoreEdict, &hit);

	return (hit.flFraction >= 1.0f && !hit.fAllSolid);
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
	if (FNullEnt(pEdict)) { return; }

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

	Vector Centre = UTIL_GetCentreOfEntity(pEntity);
	Centre.z = pEntity->v.absmin.z + 1.0f;

	return Centre;
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

	Vector EntityCentre = UTIL_GetCentreOfEntity(Edict);

	UTIL_TraceHull(EntityCentre, (EntityCentre - Vector(0.0f, 0.0f, 1000.0f)), ignore_monsters, GetPlayerHullIndex(Edict), Edict->v.pContainingEntity, &hit);
	
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
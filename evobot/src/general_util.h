//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// general_util.h
// 
// Contains all useful helper functions for general plugin usage.
//

#pragma once

#ifndef GENERAL_UTIL_H
#define GENERAL_UTIL_H

#include <extdll.h>

// Returns the mod directory (e.g. half-life/ns)
void GetGameDir(char* game_dir);

// Takes the game dir (see GetGameDir()) and appends the args to it. Does NOT append a / at the end
void UTIL_BuildFileName(char* filename, const char* arg1, const char* arg2, const char* arg3, const char* arg4);


// Draws a white line between start and end for the given player (pEntity) for 0.1s
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end);
// Draws a white line between start and end for the given player (pEntity) for given number of seconds
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds);
// Draws a coloured line using RGB input, between start and end for the given player (pEntity) for 0.1s
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, int r, int g, int b);
// Draws a coloured line using RGB input, between start and end for the given player (pEntity) for given number of seconds
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds, int r, int g, int b);

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax);;

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds);

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds, int r, int g, int b);

// Draw a text message on the HUD. X and Y range from 0.0/0.0 (top left of screen) to 1.0/1.0 (bottom right)
void UTIL_DrawHUDText(edict_t* pEntity, char channel, float x, float y, unsigned char r, unsigned char g, unsigned char b, const char* string);

// new UTIL.CPP functions...
edict_t* UTIL_FindEntityInSphere(edict_t* pentStart, const Vector& vecCenter, float flRadius);
edict_t* UTIL_FindEntityByString(edict_t* pentStart, const char* szKeyword, const char* szValue);
edict_t* UTIL_FindEntityByClassname(edict_t* pentStart, const char* szName);
edict_t* UTIL_FindEntityByTarget(edict_t* pentStart, const char* szName);
edict_t* UTIL_FindEntityByTargetname(edict_t* pentStart, const char* szName);

void UTIL_HostSay(edict_t* pEntity, int teamonly, char* message);
void UTIL_SayText(const char* pText, edict_t* pEdict);
void ClientPrint(edict_t* pEntity, int msg_dest, const char* msg_name);

// Performs a simple line trace between start and end, ignoring monsters and glass. Returns true if the trace does NOT hit anything.
bool UTIL_QuickTrace(const edict_t* pEdict, const Vector& start, const Vector& end);
// Performs a simple hull trace between start and end, ignoring monsters and using the requested hull index. Returns true if the trace does NOT hit anything.
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum);
// Performs a simple hull trace between start and end, ignoring monsters and using the visibility hull index. Returns true if the trace does NOT hit anything.
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end);
// Same as quick trace, but allows fAllSolid (since commander can place structures through walls)
bool UTIL_CommanderTrace(const edict_t* pEdict, const Vector& start, const Vector& end);

edict_t* UTIL_TraceEntity(const edict_t* pEdict, const Vector& start, const Vector& end);

Vector UTIL_GetTraceHitLocation(const Vector Start, const Vector End);

Vector UTIL_GetEntityGroundLocation(const edict_t* pEntity);
Vector UTIL_GetCentreOfEntity(const edict_t* Entity);
Vector UTIL_GetFloorUnderEntity(const edict_t* Edict);

bool IsEdictStructure(const edict_t* edict);

#endif
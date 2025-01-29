#pragma once

#ifndef AI_HELPER_H
#define AI_HELPER_H

#include "AIConstants.h"

#include <string>


bool UTIL_QuickTrace(const edict_t* pEdict, const Vector& start, const Vector& end, bool bAllowStartSolid = false);
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, bool bAllowStartSolid = false);
bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum, bool bAllowStartSolid = false);
edict_t* UTIL_TraceEntity(const edict_t* pEdict, const Vector& start, const Vector& end);
edict_t* UTIL_TraceEntityHull(const edict_t* pEdict, const Vector& start, const Vector& end);
Vector UTIL_GetTraceHitLocation(const Vector Start, const Vector End);
Vector UTIL_GetHullTraceHitLocation(const Vector Start, const Vector End, int HullNum);

Vector UTIL_GetGroundLocation(const Vector CheckLocation);
Vector UTIL_GetEntityGroundLocation(const edict_t* pEntity);
Vector UTIL_GetCentreOfEntity(const edict_t* Entity);
Vector UTIL_GetFloorUnderEntity(const edict_t* Edict);

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector UserLocation, const edict_t* Entity);
Vector UTIL_GetClosestPointOnEntityToLocation(const Vector Location, const edict_t* Entity, const Vector EntityLocation);

void AIDEBUG_DrawBotPath(AIPlayer* pBot, float DrawTime = 0.0f);
void AIDEBUG_DrawPath(std::vector<bot_path_node>& path, float DrawTime = 0.0f);

// Draws a white line between start and end for the given player (pEntity) for 0.1s
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end);
// Draws a white line between start and end for the given player (pEntity) for given number of seconds
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds);
// Draws a coloured line using RGB input, between start and end for the given player (pEntity) for 0.1s
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, int r, int g, int b);
// Draws a coloured line using RGB input, between start and end for the given player (pEntity) for given number of seconds
void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, float drawTimeSeconds, int r, int g, int b);

void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds);
void UTIL_DrawBox(edict_t* pEntity, Vector bMin, Vector bMax, float drawTimeSeconds, int r, int g, int b);

void UTIL_DrawHUDText(edict_t* pEntity, char channel, float x, float y, unsigned char r, unsigned char g, unsigned char b, const char* string);

void UTIL_ClearLocalizations();
void UTIL_LocalizeText(const char* InputText, std::string& OutputText);

void GetGameDir(char* game_dir);

// Takes the game dir (see GetGameDir()) and appends the args to it. Does NOT append a / at the end
void UTIL_BuildFileName(char* filename, const char* arg1, const char* arg2, const char* arg3, const char* arg4);

// new UTIL.CPP functions...
edict_t* UTIL_FindEntityInSphere(edict_t* pentStart, const Vector& vecCenter, float flRadius);
edict_t* UTIL_FindEntityByString(edict_t* pentStart, const char* szKeyword, const char* szValue);
edict_t* UTIL_FindEntityByClassname(edict_t* pentStart, const char* szName);
edict_t* UTIL_FindEntityByTarget(edict_t* pentStart, const char* szName);
edict_t* UTIL_FindEntityByTargetname(edict_t* pentStart, const char* szName);

#endif
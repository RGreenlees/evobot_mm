#include "AIHelper.h"
#include "AIMath.h"
#include "AIPlayerUtil.h"
#include "AITactical.h"
#include "AINavigation.h"

#include <enginecallback.h>		// ALERT()
#include "osdep.h"				// win32 vsnprintf, etc

#include <dllapi.h>
#include <meta_api.h>

#include "nav_constants.h"

#include <unordered_map>

#include <fstream>

int m_spriteTexture;

std::unordered_map<const char*, std::string> LocalizedLocationsMap;

bool UTIL_QuickTrace(const edict_t* pEdict, const Vector& start, const Vector& end, bool bAllowStartSolid)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, ignore_monsters, ignore_glass, IgnoreEdict, &hit);
	return (hit.flFraction >= 1.0f && !hit.fAllSolid && (bAllowStartSolid || !hit.fStartSolid));
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, bool bAllowStartSolid)
{
	int hullNum = (!FNullEnt(pEdict)) ? GetPlayerHullIndex(pEdict) : point_hull;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	TraceResult hit;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, IgnoreEdict, &hit);

	return (hit.flFraction >= 1.0f && !hit.fAllSolid && (bAllowStartSolid || !hit.fStartSolid));
}

bool UTIL_QuickHullTrace(const edict_t* pEdict, const Vector& start, const Vector& end, int hullNum, bool bAllowStartSolid)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceHull(start, end, ignore_monsters, hullNum, IgnoreEdict, &hit);

	return (hit.flFraction >= 1.0f && !hit.fAllSolid && (bAllowStartSolid || !hit.fStartSolid));
}

edict_t* UTIL_TraceEntity(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceLine(start, end, dont_ignore_monsters, dont_ignore_glass, IgnoreEdict, &hit);
	return hit.pHit;
}

edict_t* UTIL_TraceEntityHull(const edict_t* pEdict, const Vector& start, const Vector& end)
{
	TraceResult hit;
	edict_t* IgnoreEdict = (!FNullEnt(pEdict)) ? pEdict->v.pContainingEntity : NULL;
	UTIL_TraceHull(start, end, dont_ignore_monsters, head_hull, IgnoreEdict, &hit);
	return hit.pHit;
}

Vector UTIL_GetTraceHitLocation(const Vector Start, const Vector End)
{
	TraceResult hit;
	UTIL_TraceHull(Start, End, ignore_monsters, point_hull, NULL, &hit);

	if (hit.flFraction < 1.0f && !hit.fAllSolid)
	{
		return hit.vecEndPos;
	}

	return Start;
}

Vector UTIL_GetHullTraceHitLocation(const Vector Start, const Vector End, int HullNum)
{
	TraceResult hit;
	UTIL_TraceHull(Start, End, ignore_monsters, HullNum, NULL, &hit);

	if (hit.flFraction < 1.0f && !hit.fAllSolid)
	{
		return hit.vecEndPos;
	}

	return Start;
}

Vector UTIL_GetGroundLocation(const Vector CheckLocation)
{
	if (vIsZero(CheckLocation)) { return ZERO_VECTOR; }

	TraceResult hit;

	UTIL_TraceHull(CheckLocation, (CheckLocation - Vector(0.0f, 0.0f, 1000.0f)), ignore_monsters, head_hull, nullptr, &hit);

	if (hit.flFraction < 1.0f)
	{
		return hit.vecEndPos;
	}

	return CheckLocation;
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
		}

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

	Vector EntityCentre = UTIL_GetCentreOfEntity(Edict) + Vector(0.0f, 0.0f, 1.0f);
	Vector TraceEnd = (EntityCentre - Vector(0.0f, 0.0f, 1000.0f));

	UTIL_TraceHull(EntityCentre, TraceEnd, ignore_monsters, head_hull, Edict->v.pContainingEntity, &hit);

	if (hit.flFraction < 1.0f)
	{
		return (hit.vecEndPos + Vector(0.0f, 0.0f, 1.0f));
	}

	return Edict->v.origin;
}

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector Location, const edict_t* Entity)
{
	return Vector(clampf(Location.x, Entity->v.absmin.x, Entity->v.absmax.x), clampf(Location.y, Entity->v.absmin.y, Entity->v.absmax.y), clampf(Location.z, Entity->v.absmin.z, Entity->v.absmax.z));
}

Vector UTIL_GetClosestPointOnEntityToLocation(const Vector Location, const edict_t* Entity, const Vector EntityLocation)
{
	Vector MinVec = EntityLocation - (Entity->v.size * 0.5f);
	Vector MaxVec = EntityLocation + (Entity->v.size * 0.5f);

	return Vector(clampf(Location.x, MinVec.x, MaxVec.x), clampf(Location.y, MinVec.y, MaxVec.y), clampf(Location.z, MinVec.z, MaxVec.z));
}

void AIDEBUG_DrawBotPath(AIPlayer* pBot, float DrawTime)
{
	AIDEBUG_DrawPath(pBot->BotNavInfo.CurrentPath, DrawTime);

	if (pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size())
	{
		bot_path_node CurrentNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

		DynamicMapObject* BlockingObject = UTIL_GetObjectBlockingPathPoint(&CurrentNode, nullptr, nullptr);

		if (BlockingObject && BlockingObject->State == OBJECTSTATE_IDLE)
		{
			UTIL_DrawLine(INDEXENT(1), pBot->Edict->v.origin, CurrentNode.Location, DrawTime, 255, 255, 0);
		}
		else
		{
			unsigned char R, G, B;

			GetDebugColorForFlag((NavMovementFlag)CurrentNode.flag, R, G, B);

			UTIL_DrawLine(INDEXENT(1), pBot->Edict->v.origin, CurrentNode.Location, DrawTime, R, G, B);
		}


	}
}

void AIDEBUG_DrawPath(std::vector<bot_path_node>& path, float DrawTime)
{
	if (path.size() == 0) { return; }

	for (auto it = path.begin(); it != path.end(); it++)
	{
		bot_path_node CurrentNode = *it;

		DynamicMapObject* BlockingObject = UTIL_GetObjectBlockingPathPoint(&CurrentNode, nullptr, nullptr);

		if (BlockingObject && BlockingObject->State == OBJECTSTATE_IDLE)
		{
			UTIL_DrawLine(INDEXENT(1), CurrentNode.FromLocation, CurrentNode.Location, DrawTime, 255, 255, 0);
		}
		else
		{
			unsigned char R, G, B;

			GetDebugColorForFlag((NavMovementFlag)CurrentNode.flag, R, G, B);

			UTIL_DrawLine(INDEXENT(1), CurrentNode.FromLocation, CurrentNode.Location, DrawTime, R, G, B);
		}
	}
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end)
{
	if (FNullEnt(pEntity) || pEntity->free) { return; }

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
	if (FNullEnt(pEntity) || pEntity->free) { return; }

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
	if (FNullEnt(pEntity) || pEntity->free) { return; }

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

	WRITE_BYTE(r);             // r, g, b
	WRITE_BYTE(g);           // r, g, b
	WRITE_BYTE(b);            // r, g, b

	WRITE_BYTE(250);      // brightness
	WRITE_BYTE(5);           // speed
	MESSAGE_END();
}

void UTIL_DrawLine(edict_t* pEntity, Vector start, Vector end, int r, int g, int b)
{
	if (FNullEnt(pEntity) || pEntity->free) { return; }

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

void UTIL_DrawHUDText(edict_t* pEntity, char channel, float x, float y, unsigned char r, unsigned char g, unsigned char b, const char* string)
{
	// higher level wrapper for hudtextparms TE_TEXTMESSAGEs. This function is meant to be called
	// every frame, since the duration of the display is roughly worth the duration of a video
	// frame. The X and Y coordinates are unary fractions which are bound to this rule:
	// 0: top of the screen (Y) or left of the screen (X), left aligned text
	// 1: bottom of the screen (Y) or right of the screen (X), right aligned text
	// -1(only one negative value possible): center of the screen (X and Y), centered text
	// Any value ranging from 0 to 1 will represent a valid position on the screen.

	//static short duration;

	if (FNullEnt(pEntity)) { return; }

	//duration = (int)GAME_GetServerMSecVal() * 256 / 750; // compute text message duration
	//if (duration < 5)
	//	duration = 5;

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
	WRITE_SHORT(5); // hold time in seconds * 256
	WRITE_STRING(string);//string); // send the string
	MESSAGE_END(); // end

	return;
}

void UTIL_ClearLocalizations()
{
	LocalizedLocationsMap.clear();
}

void UTIL_LocalizeText(const char* InputText, std::string& OutputText)
{
	// Don't localize empty strings
	if (!strcmp(InputText, ""))
	{
		OutputText = "";
	}

	char theInputString[1024];

	sprintf(theInputString, "%s", InputText);

	std::unordered_map<const char*, std::string>::const_iterator FoundLocalization = LocalizedLocationsMap.find(theInputString);

	if (FoundLocalization != LocalizedLocationsMap.end())
	{
		OutputText = FoundLocalization->second;
		return;
	}

	char filename[256];
	char modDir[256];

	std::string localizedString(theInputString);

	GetGameDir(modDir);

	std::string titlesPath = std::string(modDir) + "/titles.txt";
	strcpy(filename, titlesPath.c_str());

	std::ifstream cFile(filename);
	if (cFile.is_open())
	{
		std::string line;
		while (getline(cFile, line))
		{
			line.erase(std::remove_if(line.begin(), line.end(), ::isspace),
				line.end());
			if (line[0] == '/' || line.empty())
				continue;

			if (line.compare(theInputString) == 0)
			{
				getline(cFile, line);
				getline(cFile, localizedString);
				break;

			}
		}
	}

	char theOutputString[1024];

	sprintf(theOutputString, "%s", localizedString.c_str());

	std::string Delimiter = "Hive -";
	auto delimiterPos = localizedString.find(Delimiter);

	if (delimiterPos == std::string::npos)
	{
		Delimiter = "Hive Location -";
		delimiterPos = localizedString.find(Delimiter);
	}

	if (delimiterPos == std::string::npos)
	{
		Delimiter = "Hive Location  -";
		delimiterPos = localizedString.find("Hive Location  -");
	}

	if (delimiterPos != std::string::npos)
	{
		auto AreaName = localizedString.substr(delimiterPos + Delimiter.length());

		AreaName.erase(0, AreaName.find_first_not_of(" \r\n\t\v\f"));

		sprintf(theOutputString, "%s", AreaName.c_str());
	}

	OutputText = theOutputString;

	LocalizedLocationsMap[InputText] = OutputText;

}

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
	edict_t* pentEntity;

	pentEntity = FIND_ENTITY_BY_CLASSNAME(pentStart, szName);

	if (!FNullEnt(pentEntity))
		return pentEntity;
	return NULL;
}

edict_t* UTIL_FindEntityByTarget(edict_t* pentStart, const char* szName)
{
	edict_t* pentEntity;

	pentEntity = FIND_ENTITY_BY_TARGET(pentStart, szName);

	if (!FNullEnt(pentEntity))
		return pentEntity;
	return NULL;
}


edict_t* UTIL_FindEntityByTargetname(edict_t* pentStart, const char* szName)
{
	edict_t* pentEntity;

	pentEntity = FIND_ENTITY_BY_STRING(pentStart, "targetname", szName);

	if (!FNullEnt(pentEntity))
		return pentEntity;
	return NULL;
}
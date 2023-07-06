//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_bsp.cpp
// 
// Handles parsing of BSP files for useful info that cannot be obtained from within the game
//

#include "bot_bsp.h"
#include "general_util.h"
#include "bot_navigation.h"

#include <extdll.h>
#include <dllapi.h>

const char* BSP_GetEntityKeyValue(const edict_t* Entity, const char* Key)
{
	char filename[256]; // Full path to BSP file

	UTIL_BuildFileName(filename, "maps", STRING(gpGlobals->mapname), NULL, NULL);
	strcat(filename, ".bsp");

	FILE* bspfile = fopen(filename, "rb");

	if (!bspfile)
	{
		return false;
	}

	BSPHEADER fileHeader; // The BSP file header

	fread(&fileHeader, sizeof(fileHeader), 1, bspfile);

	int entitiesLength = fileHeader.lump[LUMP_ENTITIES].nLength;

	char* entitiesText = (char*)malloc(entitiesLength);
	memset(entitiesText, 0, entitiesLength);

	fseek(bspfile, fileHeader.lump[LUMP_ENTITIES].nOffset, SEEK_SET);
	fread(entitiesText, entitiesLength, 1, bspfile);

	// First count how many entities we have
	int nEntities = 0;

	int propertyCounter = 0;
	char* propPos = strchr(entitiesText, '"');

	BSPENTITYDEF CurrEntityDef;
	memset(&CurrEntityDef, 0, sizeof(BSPENTITYDEF));

	BUTTONDEF Buttons[32];
	memset(Buttons, 0, sizeof(Buttons));
	int nButtons = 0;

	float CurrDelay = 0.0f;

	char CurrTargetOnFinish[64];

	char ThisKey[1024];
	char ThisValue[1024];

	char CurrTargetName[64];
	bool bIsWeldable = false;

	while (propPos != NULL)
	{
		int startName = (int)(propPos - entitiesText + 1);
		propPos = strchr(propPos + 1, '"');
		int endName = (int)(propPos - entitiesText);

		memcpy(ThisKey, &entitiesText[startName], endName - startName);
		ThisKey[endName - startName] = '\0';

		propPos = strchr(propPos + 1, '"');
		int startValue = (int)(propPos - entitiesText + 1);
		propPos = strchr(propPos + 1, '"');
		int endValue = (int)(propPos - entitiesText);

		memcpy(ThisValue, &entitiesText[startValue], endValue - startValue);
		ThisValue[endValue - startValue] = '\0';

		if (FStrEq(ThisKey, "targetOnFinish"))
		{
			bIsWeldable = true;
			memcpy(CurrTargetOnFinish, ThisValue, endValue - startValue);
			CurrTargetOnFinish[endValue - startValue] = '\0';
		}

		if (FStrEq(ThisKey, "classname"))
		{
			if (bIsWeldable)
			{
				UTIL_MarkDoorWeldable(CurrTargetOnFinish);
			}
			bIsWeldable = false;
		}

		propPos = strchr(propPos + 1, '"');
	}

	return "";

}

char* GetEntityDefClassname(BSPENTITYDEF* entity)
{
	return entity->properties[entity->numProperties - 1].propertyValue;
}
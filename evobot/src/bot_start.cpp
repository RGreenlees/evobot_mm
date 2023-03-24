//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_start.cpp
// 
// Probably needs refactoring into something else, kind of pointless
//

#ifndef _WIN32
#include <string.h>
#endif

#include <extdll.h>
#include <dllapi.h>
#include <h_export.h>
#include <meta_api.h>

#include "bot.h"
#include "bot_func.h"


void BotStartGame(bot_t *pBot)
{

	edict_t *pEdict = pBot->pEdict;

	if (pBot->bot_team == 1)
		FakeClientCommand(pEdict, "jointeamone", NULL, NULL);
	else if (pBot->bot_team == 2)
		FakeClientCommand(pEdict, "jointeamtwo", NULL, NULL);
	else
		FakeClientCommand(pEdict, "autoassign", NULL, NULL);	

}


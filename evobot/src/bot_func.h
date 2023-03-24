//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_func.h
// 
// Core bot functions including tick
//

#pragma once

#ifndef BOT_FUNC_H
#define BOT_FUNC_H

typedef struct _BOTTIMER
{
	char* timerName;
	void (*function) (void);
	float interval;
	bool repeat;
} BOTTIMER;


//prototypes of bot functions...

// Called when bot first joins the server
void BotSpawnInit( bot_t *pBot );
// Spawns a new bot and adds them to the supplied Team. Will load the navigation data if not already loaded
void BotCreate( edict_t *pPlayer, int Team);
// Will have the bot join a team
void BotStartGame( bot_t *pBot );
// Main bot tick. Called 60 times per second by default (see MIN_FRAME_TIME in bot.h)
void BotThink( bot_t *pBot );
// Called by BotThink if the debug mode is "drone"
void DroneThink(bot_t* pBot);
// Called by BotThink if the debug mode is "testnav"
void TestNavThink(bot_t* pBot);
// Caps bot sync speed to avoid them running too slowly or too fast
byte ThrottledMsec(bot_t *pBot);




#endif // BOT_FUNC_H


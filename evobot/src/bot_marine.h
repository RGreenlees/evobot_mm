//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_gorge.h
// 
// Contains gorge-related functions. Needs refactoring into helper function file
//

#pragma once

#ifndef BOT_MARINE_H
#define BOT_MARINE_H

#include "bot_structs.h"

void MarineThink(bot_t* pBot);
void MarineCombatModeThink(bot_t* pBot);

bool MarineCombatThink(bot_t* pBot);
void MarineBombardierCombatThink(bot_t* pBot);
void MarineHuntEnemy(bot_t* pBot, enemy_status* TrackedEnemy);



void MarineCheckWantsAndNeeds(bot_t* pBot);
void MarineCombatModeCheckWantsAndNeeds(bot_t* pBot);

void MarineSetSecondaryTask(bot_t* pBot, bot_task* Task);
void MarineSetCombatModeSecondaryTask(bot_t* pBot, bot_task* Task);

void MarineSweeperSetPrimaryTask(bot_t* pBot, bot_task* Task);
void MarineCapperSetPrimaryTask(bot_t* pBot, bot_task* Task);
void MarineAssaultSetPrimaryTask(bot_t* pBot, bot_task* Task);
void MarineBombardierSetPrimaryTask(bot_t* pBot, bot_task* Task);

void MarineSweeperSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task);
void MarineAssaultSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task);

void MarineSweeperSetSecondaryTask(bot_t* pBot, bot_task* Task);
void MarineCapperSetSecondaryTask(bot_t* pBot, bot_task* Task);
void MarineAssaultSetSecondaryTask(bot_t* pBot, bot_task* Task);

// Determines the individual bot's most appropriate role at this moment based on the state of play.
BotRole MarineGetBestBotRole(const bot_t* pBot);
BotRole MarineGetBestCombatModeRole(const bot_t* pBot);

void BotMarineSetPrimaryTask(bot_t* pBot, bot_task* Task);
void BotMarineSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task);

void BotReceiveCommanderOrder(bot_t* pBot, AvHOrderType orderType, AvHUser3 TargetType, Vector destination);
void BotReceiveMoveToOrder(bot_t* pBot, Vector destination);
void BotReceiveBuildOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination);
void BotReceiveAttackOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination);
void BotReceiveGuardOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination);
void BotReceiveWeldOrder(bot_t* pBot, AvHUser3 TargetType, Vector destination);

// Sets the bot's next desired combat upgrade to get / save for
CombatModeMarineUpgrade MarineGetNextCombatUpgrade(bot_t* pBot);

#endif
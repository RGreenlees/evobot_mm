//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_alien.h
// 
// Contains all behaviour code for alien AI players
//

#pragma once

#ifndef BOT_ALIEN_H
#define BOT_ALIEN_H

#include "bot_structs.h"


void AlienThink(bot_t* pBot);
void AlienCombatModeThink(bot_t* pBot);

void AlienCheckWantsAndNeeds(bot_t* pBot);
void AlienCheckCombatModeWantsAndNeeds(bot_t* pBot);

int GetDesiredAlienUpgrade(const bot_t* pBot, const HiveTechStatus TechType);

BotRole AlienGetBestBotRole(bot_t* pBot);
BotRole AlienGetBestCombatModeRole(const bot_t* pBot);

void BotAlienSetPrimaryTask(bot_t* pBot, bot_task* Task);
void AlienCapperSetPrimaryTask(bot_t* pBot, bot_task* Task);
void AlienHarasserSetPrimaryTask(bot_t* pBot, bot_task* Task);
void AlienBuilderSetPrimaryTask(bot_t* pBot, bot_task* Task);
void AlienDestroyerSetPrimaryTask(bot_t* pBot, bot_task* Task);

void BotAlienSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task);
void AlienHarasserSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task);
void AlienBuilderSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task);
void AlienDestroyerSetCombatModePrimaryTask(bot_t* pBot, bot_task* Task);

void BotAlienSetSecondaryTask(bot_t* pBot, bot_task* Task);
void AlienSetCombatModeSecondaryTask(bot_t* pBot, bot_task* Task);
void AlienBuilderSetSecondaryTask(bot_t* pBot, bot_task* Task);
void AlienCapperSetSecondaryTask(bot_t* pBot, bot_task* Task);
void AlienHarasserSetSecondaryTask(bot_t* pBot, bot_task* Task);
void AlienDestroyerSetSecondaryTask(bot_t* pBot, bot_task* Task);

void AlienCombatThink(bot_t* pBot);
void SkulkCombatThink(bot_t* pBot);
void GorgeCombatThink(bot_t* pBot);
void LerkCombatThink(bot_t* pBot);
void FadeCombatThink(bot_t* pBot);
void OnosCombatThink(bot_t* pBot);

int CalcNumAlienBuildersRequired();

CombatModeAlienUpgrade AlienGetNextCombatUpgrade(bot_t* pBot);

bool IsAlienBuilderTaskNeeded(bot_t* pBot);
bool IsAlienCapperTaskNeeded();

#endif
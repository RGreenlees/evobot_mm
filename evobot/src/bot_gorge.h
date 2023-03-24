//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_gorge.h
// 
// Contains gorge-related functions. Needs refactoring into helper function file
//

#pragma once

#ifndef BOT_GORGE_H
#define BOT_GORGE_H

#include "bot.h"

// Handles all decision making for gorges that are in combat (either seen an enemy, or are under attack from one)
void GorgeCombatThink(bot_t* pBot);




#endif
#pragma once

#ifndef AI_CONFIG_H
#define AI_CONFIG_H

#include "AIConstants.h"

#include <string>


// Reads evobot.cfg in addons/evobot and populates all the settings from it
void CONFIG_ParseConfigFile();

std::string CONFIG_GetBotPrefix();

// Returns the max time a bot is allowed to be stuck before suiciding (0 means forever)
float CONFIG_GetMaxStuckTime();

void CONFIG_RegenerateIniFile();

void CONFIG_PopulateBotNames();
std::string CONFIG_GetNextBotName();

void CONFIG_PrintHelpFile();

#endif
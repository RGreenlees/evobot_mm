//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_config.h
// 
// Handles parsing of evobot.cfg and managing bot settings
//

#pragma once

#ifndef BOT_CONFIG_H
#define BOT_CONFIG_H

#include "bot_structs.h"

// Bot fill mode determines how bots should be automatically added/removed from teams
typedef enum _BOTFILLMODE
{
	BOTFILL_MANUAL = 0,  // Manual, no automatic adding or removal of bots
	BOTFILL_BALANCEONLY, // Bots are automatically added/removed to ensure teams remain balanced
	BOTFILL_FILLTEAMS    // Bots are automatically added/removed to ensure teams maintain a certain number of players (see TeamSizeDefinitions)

} BotFillMode;

// Bot commander mode, should the bot go commander and when
typedef enum _COMMANDERMODE
{
	COMMANDERMODE_NEVER = 0, // Bot never tries to command
	COMMANDERMODE_IFNOHUMAN, // Bot only commands if no human is on the marine team
	COMMANDERMODE_ALWAYS     // Bot will always take command if no human does after CommanderWaitTime expires

} CommanderMode;

// Each map can have a desired marine and alien team size
typedef struct _TEAMSIZEDEFINITIONS
{
	int MarineSize = 6;
	int AlienSize = 6;
} TeamSizeDefinitions;

// Reads evobot.cfg in addons/evobot and populates all the settings from it
void ParseConfigFile(bool bOverride);

// Returns the current bot fill mode
BotFillMode CONFIG_GetBotFillMode();
// Returns the current commander wait time is COMMANDERMODE_ALWAYS (see CONFIG_GetCommanderMode())
float CONFIG_GetCommanderWaitTime();
// Returns the current commander mode (see CommanderMode enum)
CommanderMode CONFIG_GetCommanderMode();
// Returns the current lerk cooldown (how long aliens wait before evolving another lerk after the last one died)
float CONFIG_GetLerkCooldown();

bool CONFIG_IsLerkAllowed();
bool CONFIG_IsFadeAllowed();
bool CONFIG_IsOnosAllowed();

// Returns the max time a bot is allowed to be stuck before suiciding (0 means forever)
float CONFIG_GetMaxStuckTime();

// Returns the desired marine team size for the given map, indexes into TeamSizeMap
int CONFIG_GetMarineTeamSizeForMap(const char* MapName);
// Returns the desired alien team size for the given map, indexes into TeamSizeMap
int CONFIG_GetAlienTeamSizeForMap(const char* MapName);

// Will regenerate the existing evobot.cfg file in addons/evobot, overwrite any existing file. Useful if you've broken your current copy somehow
void CONFIG_RegenerateConfigFile();

// If set to true, will ignore the configured settings in evobot.cfg and keep whatever settings are already in place. Useful if you want to temporarily disable bots or change settings for a match
void CONFIG_SetConfigOverride(bool bNewValue);

// Manually overrides the configured bot fill mode. Make sure config override is on.
void CONFIG_SetBotFillMode(BotFillMode NewMode);

// Manually overrides the configured commander mode. Make sure config override is on.
void CONFIG_SetCommanderMode(CommanderMode NewMode);

// Returns the configured NS version (32 or 33). Used in bot_client.cpp to determine how to process certain network messages. Will cause crashes if incorrectly set.
int CONFIG_GetNSVersion();

// Populates the BotName input with a randomly-chosen name from the list (see evobot_names.txt)
void CONFIG_GetBotName(char* BotName);

// Reads evobot_names.txt and populates the bot_names array with all the names it finds. If file is missing, will populate it with just "Bot"
void CONFIG_PopulateBotNames();

// Some official NS maps use a lookup system to resolve location names held in the BSP file to descriptive names held in titles.txt. This performs that lookup
void UTIL_LookUpLocationName(const char* InputName, char* Result);

// Returns the configured hive tech at that index (chamber build sequence)
int CONFIG_GetHiveTechAtIndex(const int Index);

// Manually set the marine team size. Used by the "evobot fillteams" console command. Make sure config override is on.
void CONFIG_SetManualMarineTeamSize(const int NewValue);
// Manually set the alien team size. Used by the "evobot fillteams" console command. Make sure config override is on.
void CONFIG_SetManualAlienTeamSize(const int NewValue);

// Returns the manually-set marine team size when using "evobot fillteams"
int CONFIG_GetManualMarineTeamSize();
// Returns the manually-set alien team size when using "evobot fillteams"
int CONFIG_GetManualAlienTeamSize();

bot_skill CONFIG_GetBotSkillLevel(const char* SkillName);

bool CONFIG_BotSkillLevelExists(const char* SkillName);

bot_skill CONFIG_GetGlobalBotSkillLevel();
void CONFIG_SetGlobalBotSkillLevel(const char* NewSkillLevel);

void CONFIG_PrintHelpFile();

#endif
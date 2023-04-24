//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_config.cpp
// 
// Handles parsing of evobot.cfg and managing bot settings
//

#include "bot_config.h"
#include "NS_Constants.h"
#include "general_util.h"

#include <extdll.h>
#include <dllapi.h>
#include <meta_api.h>

#include <string>
#include <fstream>

#include <algorithm>
#include <unordered_map>

constexpr auto MAX_BOT_NAMES = 100;


int NumBotNames = 0;
char bot_names[MAX_BOT_NAMES][BOT_NAME_LEN + 1];
int CurrNameIndex = 0;

// If true then the config won't be parsed on map change. Preserves any manual settings made during this session
bool bConfigOverride = false;

float fCommanderWaitTime = 10.0f;
BotFillMode eBotFillMode = BOTFILL_MANUAL;
CommanderMode eCommanderMode = COMMANDERMODE_ALWAYS;
int NSVersion = 33; // 32 for 3.2, 33 for 3.3 (including betas)
char BotPrefix[32] = "";

HiveTechStatus ChamberSequence[3];

std::unordered_map<std::string, TeamSizeDefinitions> TeamSizeMap;

std::unordered_map<std::string, bot_skill> BotSkillLevelsMap;
std::string CurrentSkillLevel;

std::string GlobalSkillLevel = "default";

int ManualMarineTeamSize = 0;
int ManualAlienTeamSize = 0;

const char* UTIL_HiveTechToChar(const HiveTechStatus HiveTech)
{
    switch (HiveTech)
    {
    case HIVE_TECH_MOVEMENT:
        return "Movement";
    case HIVE_TECH_DEFENCE:
        return "Defence";
    case HIVE_TECH_SENSORY:
        return "Sensory";
    default:
        return "None";
    }
}

void CONFIG_PrintHelpFile()
{
    char HelpFileName[256];

    FILE* HelpFile = NULL;
    GetGameDir(HelpFileName);
    strcat(HelpFileName, "/addons/evobot/Help.txt");

    std::ifstream cFile(HelpFileName);

    if (cFile.is_open())
    {
        std::string line;
        while (getline(cFile, line))
        {
            LOG_CONSOLE(PLID, line.c_str());
        }
    }
    else
    {
        LOG_CONSOLE(PLID, "Help not available, Help.txt not found in evobot directory\n");
    }
}

void ParseConfigFile(bool bOverride)
{
    if (bOverride) { bConfigOverride = false; }

    if (bConfigOverride) { return; }

    TeamSizeMap.clear();
    TeamSizeMap["default"].MarineSize = 6;
    TeamSizeMap["default"].AlienSize = 6;

    BotSkillLevelsMap.clear();

    BotSkillLevelsMap["default"].bot_aim_skill = 0.3f;
    BotSkillLevelsMap["default"].bot_motion_tracking_skill = 0.3f;
    BotSkillLevelsMap["default"].bot_reaction_time = 0.3f;
    BotSkillLevelsMap["default"].bot_view_speed = 1.0f;

    CurrentSkillLevel = "default";

    char filename[256];

    UTIL_BuildFileName(filename, "addons", "evobot", "evobot.cfg", NULL);

    std::ifstream cFile(filename);
    if (cFile.is_open())
    {
        LOG_CONSOLE(PLID, "Parsing evobot.cfg...\n");
        std::string line;
        while (getline(cFile, line))
        {
            line.erase(std::remove_if(line.begin(), line.end(), isspace),
                line.end());
            if (line[0] == '#' || line.empty())
                continue;
            auto delimiterPos = line.find("=");
            auto key = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);

            if (key.compare("BotFillMode") == 0)
            {
                if (value.compare("manual") == 0)
                {
                    eBotFillMode = BOTFILL_MANUAL;
                }
                else if (value.compare("balanceonly") == 0)
                {
                    eBotFillMode = BOTFILL_BALANCEONLY;
                }
                else if (value.compare("fillteams") == 0)
                {
                    eBotFillMode = BOTFILL_FILLTEAMS;
                }
                else
                {
                    LOG_CONSOLE(PLID, "Invalid setting '%s' for bot fill mode. Expected values are 'manual', 'balanceonly', 'fillteams'. Defaulting to 'manual'\n", value.c_str());
                    eBotFillMode = BOTFILL_MANUAL;
                }

                continue;
            }

            if (key.compare("TeamSize") == 0)
            {
                auto mapDelimiterPos = value.find(":");

                if (mapDelimiterPos == std::string::npos)
                {
                    LOG_CONSOLE(PLID, "Invalid TeamSize '%s', format should be TeamSize=mapname:nummarines/numaliens e.g. 'TeamSize=ns_eclipse:6/6'\n", value.c_str());
                    LOG_CONSOLE(PLID, "Set the map name to 'Default' if you wish to set a generic default for any map not listed\n", value.c_str());
                    continue;
                }

                auto mapName = value.substr(0, mapDelimiterPos);
                auto teamSizes = value.substr(mapDelimiterPos + 1);
                auto sizeDelimiterPos = teamSizes.find("/");
                if (sizeDelimiterPos == std::string::npos)
                {
                    LOG_CONSOLE(PLID, "Invalid TeamSize '%s', format should be TeamSize=mapname:nummarines/numaliens e.g. 'TeamSize=ns_eclipse:6/6'\n", value.c_str());
                    LOG_CONSOLE(PLID, "Set the map name to 'Default' if you wish to set a generic default for any map not listed\n", value.c_str());
                    continue;
                }
                auto marineSize = teamSizes.substr(0, sizeDelimiterPos);
                auto alienSize = teamSizes.substr(sizeDelimiterPos + 1);

                if (isNumber(marineSize.c_str()) && isNumber(alienSize.c_str()))
                {
                    int iMarineSize = atoi(marineSize.c_str());
                    int iAlienSize = atoi(alienSize.c_str());

                    if (iMarineSize >= 0 && iMarineSize <= 32 && iAlienSize >= 0 && iAlienSize <= 32)
                    {
                        TeamSizeMap[mapName].MarineSize = atoi(marineSize.c_str());
                        TeamSizeMap[mapName].AlienSize = atoi(alienSize.c_str());
                    }
                    else
                    {
                        LOG_CONSOLE(PLID, "Invalid TeamSize for '%s', must be number between 0 and 32 (current: %s/%s)'\n", mapName.c_str(), marineSize.c_str(), alienSize.c_str());
                    }
                }
                else
                {
                    LOG_CONSOLE(PLID, "Invalid TeamSize for '%s', must be number between 0 and 32 (current: %s/%s)'\n", mapName.c_str(), marineSize.c_str(), alienSize.c_str());
                }

                continue;
            }

            if (key.compare("nsversion") == 0)
            {
                if (value.compare("32") == 0)
                {
                    NSVersion = 32;
                }
                else
                {
                    NSVersion = 33;
                }

                continue;
            }

            if (key.compare("prefix") == 0)
            {
                sprintf(BotPrefix, value.c_str());

                continue;
            }

            if (key.compare("EnableCommander") == 0)
            {
                if (value.compare("never") == 0)
                {
                    eCommanderMode = COMMANDERMODE_NEVER;
                }
                else if (value.compare("ifnohuman") == 0)
                {
                    eCommanderMode = COMMANDERMODE_IFNOHUMAN;

                }
                else if (value.compare("always") == 0)
                {
                    eCommanderMode = COMMANDERMODE_ALWAYS;
                }
                else
                {
                    LOG_CONSOLE(PLID, "Invalid setting '%s' for commander mode. Expected values are 'never', 'ifnohuman', 'always'. Defaulting to 'always'\n", value.c_str());
                    eCommanderMode = COMMANDERMODE_ALWAYS;
                }
                continue;
            }

            if (key.compare("CommanderWaitTime") == 0)
            {
                if (isNumber(value.c_str()))
                {
                    fCommanderWaitTime = (float)atoi(value.c_str());
                }
                continue;
            }

            if (key.compare("BotSkillName") == 0)
            {
                BotSkillLevelsMap[value.c_str()].bot_aim_skill = 1.0f;
                BotSkillLevelsMap[value.c_str()].bot_motion_tracking_skill = 1.0f;
                BotSkillLevelsMap[value.c_str()].bot_reaction_time = 0.1f;
                BotSkillLevelsMap[value.c_str()].bot_view_speed = 1.0f;

                CurrentSkillLevel = value;
                continue;
            }

            if (key.compare("ReactionTime") == 0)
            {
                if (!isFloat(value.c_str()))
                {
                    LOG_CONSOLE(PLID, "Invalid reaction time setting '%s' for skill level '%s', must be floating point value between 0.0 and 1.0\n", value.c_str(), CurrentSkillLevel.c_str());
                }

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].bot_reaction_time = clampf(NewValue, 0.0f, 1.0f);

                continue;
            }

            if (key.compare("AimSkill") == 0)
            {
                if (!isFloat(value.c_str()))
                {
                    LOG_CONSOLE(PLID, "Invalid aim skill setting '%s' for skill level '%s', must be floating point value between 0.0 and 1.0\n", value.c_str(), CurrentSkillLevel.c_str());
                }

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].bot_aim_skill = clampf(NewValue, 0.0f, 1.0f);

                continue;
            }

            if (key.compare("MovementTracking") == 0)
            {
                if (!isFloat(value.c_str()))
                {
                    LOG_CONSOLE(PLID, "Invalid movement tracking setting '%s' for skill level '%s', must be floating point value between 0.0 and 1.0\n", value.c_str(), CurrentSkillLevel.c_str());
                }

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].bot_motion_tracking_skill = clampf(NewValue, 0.0f, 1.0f);

                continue;
            }

            if (key.compare("ViewSpeed") == 0)
            {
                if (!isFloat(value.c_str()))
                {
                    LOG_CONSOLE(PLID, "Invalid view speed setting '%s' for skill level '%s', must be floating point value between 0.0 and 5.0\n", value.c_str(), CurrentSkillLevel.c_str());
                }

                float NewValue = std::stof(value.c_str());

                BotSkillLevelsMap[CurrentSkillLevel.c_str()].bot_view_speed = clampf(NewValue, 0.0f, 5.0f);

                continue;
            }

            if (key.compare("DefaultSkillLevel") == 0)
            {
                GlobalSkillLevel = value;

                continue;
            }

            if (key.compare("ChamberSequence") == 0)
            {
                HiveTechStatus HiveOneTech = HIVE_TECH_NONE;
                HiveTechStatus HiveTwoTech = HIVE_TECH_NONE;
                HiveTechStatus HiveThreeTech = HIVE_TECH_NONE;

                std::vector<HiveTechStatus> AvailableTechs = { HIVE_TECH_MOVEMENT, HIVE_TECH_DEFENCE, HIVE_TECH_SENSORY };

                auto firstTechDelimiter = value.find("/");

                if (firstTechDelimiter == std::string::npos)
                {
                    LOG_CONSOLE(PLID, "Invalid chamber sequence config '%s', format should be ChamberSequence=techone/techtwo/techthree e.g. 'ChamberSequence=movement/defense/sensory'\n", value.c_str());
                    LOG_CONSOLE(PLID, "Use '?' for random choices, e.g. 'ChamberSequence=movement/?/?' to make movement always first, and then defense or sensory random\n", value.c_str());
                    continue;
                }

                auto FirstTech = value.substr(0, firstTechDelimiter);
                auto NextTechs = value.substr(firstTechDelimiter + 1);

                auto SecondTechDelimiter = NextTechs.find("/");

                if (SecondTechDelimiter == std::string::npos)
                {
                    LOG_CONSOLE(PLID, "Invalid chamber sequence config '%s', format should be ChamberSequence=techone/techtwo/techthree e.g. 'ChamberSequence=movement/defense/sensory'\n", value.c_str());
                    LOG_CONSOLE(PLID, "Use '?' for random choices, e.g. 'ChamberSequence=movement/?/?' to make movement always first, and then defense or sensory random\n", value.c_str());
                    continue;
                }

                auto SecondTech = NextTechs.substr(0, SecondTechDelimiter);
                auto ThirdTech = NextTechs.substr(SecondTechDelimiter + 1);

                if (FirstTech.compare("movement") == 0)
                {
                    HiveOneTech = HIVE_TECH_MOVEMENT;

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_MOVEMENT), AvailableTechs.end());

                }
                else if (FirstTech.compare("defense") == 0)
                {
                    HiveOneTech = HIVE_TECH_DEFENCE;

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_DEFENCE), AvailableTechs.end());
                }
                else if (FirstTech.compare("sensory") == 0)
                {
                    HiveOneTech = HIVE_TECH_SENSORY;

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_SENSORY), AvailableTechs.end());
                }

                if (SecondTech.compare("movement") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_MOVEMENT) != AvailableTechs.end())
                    {
                        HiveTwoTech = HIVE_TECH_MOVEMENT;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_MOVEMENT), AvailableTechs.end());
                    }
                }
                else if (SecondTech.compare("defense") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_DEFENCE) != AvailableTechs.end())
                    {
                        HiveTwoTech = HIVE_TECH_DEFENCE;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_DEFENCE), AvailableTechs.end());
                    }
                }
                else if (SecondTech.compare("sensory") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_SENSORY) != AvailableTechs.end())
                    {
                        HiveTwoTech = HIVE_TECH_SENSORY;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_SENSORY), AvailableTechs.end());
                    }
                }

                if (ThirdTech.compare("movement") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_MOVEMENT) != AvailableTechs.end())
                    {
                        HiveThreeTech = HIVE_TECH_MOVEMENT;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_MOVEMENT), AvailableTechs.end());
                    }
                }
                else if (ThirdTech.compare("defense") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_DEFENCE) != AvailableTechs.end())
                    {
                        HiveThreeTech = HIVE_TECH_DEFENCE;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_DEFENCE), AvailableTechs.end());
                    }
                }
                else if (ThirdTech.compare("sensory") == 0)
                {
                    if (std::find(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_SENSORY) != AvailableTechs.end())
                    {
                        HiveThreeTech = HIVE_TECH_SENSORY;
                        AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HIVE_TECH_SENSORY), AvailableTechs.end());
                    }
                }

                if (HiveOneTech == HIVE_TECH_NONE)
                {
                    int random = rand() % AvailableTechs.size();
                    HiveOneTech = AvailableTechs[random];

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HiveOneTech), AvailableTechs.end());
                }

                if (HiveTwoTech == HIVE_TECH_NONE)
                {
                    int random = rand() % AvailableTechs.size();
                    HiveTwoTech = AvailableTechs[random];

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HiveTwoTech), AvailableTechs.end());
                }

                if (HiveThreeTech == HIVE_TECH_NONE)
                {
                    int random = rand() % AvailableTechs.size();
                    HiveThreeTech = AvailableTechs[random];

                    AvailableTechs.erase(std::remove(AvailableTechs.begin(), AvailableTechs.end(), HiveTwoTech), AvailableTechs.end());
                }


                LOG_CONSOLE(PLID, "Chamber Build Sequence will be: %s / %s / %s\n", UTIL_HiveTechToChar(HiveOneTech), UTIL_HiveTechToChar(HiveTwoTech), UTIL_HiveTechToChar(HiveThreeTech));

                ChamberSequence[0] = HiveOneTech;
                ChamberSequence[1] = HiveTwoTech;
                ChamberSequence[2] = HiveThreeTech;

                continue;
            }


        }
        LOG_CONSOLE(PLID, "Config loaded for NS Version %s. If this is incorrect, please update evobot.cfg with the correct version (32 or 33)\n", (CONFIG_GetNSVersion() == 33) ? "3.3" : "3.2");
    }
    else
    {
        LOG_CONSOLE(PLID, "Could not load evobot.cfg, please ensure it is at addons/evobot/evobot.cfg\n");
        LOG_CONSOLE(PLID, "Use command 'evobot newconfig' to regenerate a new one if yours is missing or corrupted\n");
    }

    if (!CONFIG_BotSkillLevelExists(GlobalSkillLevel.c_str()))
    {
        LOG_CONSOLE(PLID, "Default skill level '%s' does not exist, falling back to default.\n", GlobalSkillLevel.c_str());
        GlobalSkillLevel = "default";
    }

    LOG_CONSOLE(PLID, "Default bot skill level will be %s.\n", GlobalSkillLevel.c_str());

    CONFIG_PopulateBotNames();
}

int CONFIG_GetHiveTechAtIndex(const int Index)
{
    if (Index < 0 || Index > 2) { return (int)HIVE_TECH_NONE; }

    return (int)ChamberSequence[Index];
}

void CONFIG_RegenerateConfigFile()
{
    LOG_CONSOLE(PLID, "Generating a new evobot.cfg...\n");

    char filename[256];

    UTIL_BuildFileName(filename, "addons", "evobot", "evobot.cfg", NULL);

    FILE* NewConfigFile = fopen(filename, "w+");

    if (!NewConfigFile)
    {
        LOG_CONSOLE(PLID, "Unable to write to %s, please ensure the user has privileges\n", filename);
        return;
    }

    fprintf(NewConfigFile, "### General bot settings ###\n\n");

    fprintf(NewConfigFile, "# NS Version. The following options are valid:\n");
    fprintf(NewConfigFile, "# '32' - NS 3.2.X including vanilla 3.2\n");
    fprintf(NewConfigFile, "# '33' - NS 3.3 and all betas\n");
    fprintf(NewConfigFile, "nsversion=33\n\n");

    fprintf(NewConfigFile, "# Bot fill mode. The following options are valid:\n");
    fprintf(NewConfigFile, "# 'fillteams' - Plugin will add and remove bots to meet the desired team sizes (see below)\n");
    fprintf(NewConfigFile, "# 'balanceonly' - Plugin will only add/remove bots to keep teams balanced, it will prefer removing bots over adding\n");
    fprintf(NewConfigFile, "# 'manual' - Plugin will not add/remove any bots automatically, you have to add/remove via the console\n");
    fprintf(NewConfigFile, "BotFillMode=manual\n\n");

    fprintf(NewConfigFile, "# Desired team sizes. Only used if bot fill mode is 'fillteams'.\n");
    fprintf(NewConfigFile, "# Format is TeamSize=mapname:nummarines/numaliens\n");
    fprintf(NewConfigFile, "# 'default' will be used if playing a map not listed below\n");
    fprintf(NewConfigFile, "TeamSize=default:6/6\n");
    fprintf(NewConfigFile, "#TeamSize=ns_machina:8/8\n\n\n\n");

    fprintf(NewConfigFile, "### Commander Settings ###\n\n");
    fprintf(NewConfigFile, "# Commander enabled mode. The following options are valid:\n");
    fprintf(NewConfigFile, "# 'never' - Bot will never take command\n");
    fprintf(NewConfigFile, "# 'ifnohuman' - Bot will only take command if no human is on the team\n");
    fprintf(NewConfigFile, "# 'always' - Bot will take command after giving humans a chance to (see fCommanderWaitTime)\n");
    fprintf(NewConfigFile, "EnableCommander=always\n\n");

    fprintf(NewConfigFile, "# If commander is enabled then after match start, how long should the bot wait to allow a human to take command (in seconds)\n");
    fprintf(NewConfigFile, "# Note that the bot will ignore this if no humans are present at the base\n");
    fprintf(NewConfigFile, "CommanderWaitTime=10\n\n\n\n");

    fprintf(NewConfigFile, "### NB: BELOW SETTINGS NOT OPERATIONAL IN 0.2a! ###\n");
    fprintf(NewConfigFile, "### Alien Settings ###\n\n");
    fprintf(NewConfigFile, "# Preferred chamber sequence. Valid entries are 'defense', 'movement' and 'sensory'. Separate sequence with /\n");
    fprintf(NewConfigFile, "# You can also use ? for random, so if you want movement always first but then defense and sensory at random, use\n");
    fprintf(NewConfigFile, "# ChamberSequence:movement/?/?\n");
    fprintf(NewConfigFile, "# # Or if you want sensory always last, but movement and defence random, use\n");
    fprintf(NewConfigFile, "# ChamberSequence=?/?/sensory\n");
    fprintf(NewConfigFile, "ChamberSequence:movement/defense/sensory\n\n");

    fprintf(NewConfigFile, "# Enabled lifeforms. Skulk is mandatory for obvious reasons. List separated by /\n");
    fprintf(NewConfigFile, "# If bots are prevented from using fade/onos but can use gorge, they will spend their resources capping resource nodes so its not wasted\n");
    fprintf(NewConfigFile, "EnabledLifeforms:gorge/fade/onos\n");

    fclose(NewConfigFile);

    LOG_CONSOLE(PLID, "New config generated at %s\n", filename);

}

void CONFIG_SetConfigOverride(bool bNewValue)
{
    bConfigOverride = bNewValue;
}

void CONFIG_SetBotFillMode(BotFillMode NewMode)
{
    eBotFillMode = NewMode;
}

void CONFIG_SetCommanderMode(CommanderMode NewMode)
{
    eCommanderMode = NewMode;
}

BotFillMode CONFIG_GetBotFillMode()
{
    return eBotFillMode;
}

float CONFIG_GetCommanderWaitTime()
{
    return fCommanderWaitTime;
}

CommanderMode CONFIG_GetCommanderMode()
{
    return eCommanderMode;
}

int CONFIG_GetMarineTeamSizeForMap(const char* MapName)
{
    std::string s = MapName;
    std::unordered_map<std::string, TeamSizeDefinitions>::const_iterator got = TeamSizeMap.find(s);

    if (got == TeamSizeMap.end())
    {
        return TeamSizeMap["default"].MarineSize;
    }
    else
    {
        return got->second.MarineSize;
    }
}

int CONFIG_GetAlienTeamSizeForMap(const char* MapName)
{
    std::string s = MapName;
    std::unordered_map<std::string, TeamSizeDefinitions>::const_iterator got = TeamSizeMap.find(s);

    if (got == TeamSizeMap.end())
    {
        return TeamSizeMap["default"].AlienSize;
    }
    else
    {
        return got->second.AlienSize;
    }
}

int CONFIG_GetNSVersion()
{
    return NSVersion;
}

const char* UTIL_LookUpLocationName(const char* InputName)
{
    char filename[256];

    std::string InputString(InputName);

    UTIL_BuildFileName(filename, "titles.txt", NULL, NULL, NULL);

    std::ifstream cFile(filename);
    if (cFile.is_open())
    {
        std::string line;
        while (getline(cFile, line))
        {
            line.erase(std::remove_if(line.begin(), line.end(), isspace),
                line.end());
            if (line[0] == '/' || line.empty())
                continue;

            if (line.compare(InputName) == 0)
            {
                getline(cFile, line);
                getline(cFile, InputString);
                break;

            }
        }
    }

    auto delimiterPos = InputString.find("-");

    if (delimiterPos != std::string::npos)
    {
        auto AreaName = InputString.substr(delimiterPos + 1);

        AreaName.erase(0, AreaName.find_first_not_of(" \r\n\t\v\f"));

        return AreaName.c_str();
    }

    return InputString.c_str();
}

void CONFIG_PopulateBotNames()
{
    char filename[256];

    NumBotNames = 0;
    memset(bot_names, 0, sizeof(bot_names));

    UTIL_BuildFileName(filename, "addons", "evobot", "evobot_names.txt", NULL);

    std::ifstream cFile(filename);
    if (cFile.is_open())
    {
        std::string line;
        while (getline(cFile, line) && NumBotNames < MAX_BOT_NAMES)
        {
            //line.erase(std::remove_if(line.begin(), line.end(), isspace),
             //   line.end());
            if (line[0] == '/' || line.empty())
                continue;

            strcpy(bot_names[NumBotNames], line.c_str());
            NumBotNames++;
        }
    }

    if (NumBotNames == 0)
    {
        strcpy(bot_names[NumBotNames++], "Bot");
    }
    CurrNameIndex = irandrange(0, NumBotNames - 1);
}

void CONFIG_GetBotName(char* BotName)
{
    strcpy(BotName, BotPrefix);

    if (NumBotNames > 0)
    {
        if (CurrNameIndex >= NumBotNames)
        {
            CurrNameIndex = 0;
        }

        strcat(BotName, bot_names[CurrNameIndex]);

        CurrNameIndex++;
    }
    else
    {
        strcat(BotName, "Bot");
    }

}

void CONFIG_SetManualMarineTeamSize(const int NewValue)
{
    ManualMarineTeamSize = NewValue;
}

void CONFIG_SetManualAlienTeamSize(const int NewValue)
{
    ManualAlienTeamSize = NewValue;
}

int CONFIG_GetManualMarineTeamSize()
{
    return ManualMarineTeamSize;
}

int CONFIG_GetManualAlienTeamSize()
{
    return ManualAlienTeamSize;
}

bool CONFIG_BotSkillLevelExists(const char* SkillName)
{
    std::string s = SkillName;
    std::unordered_map<std::string, bot_skill>::const_iterator got = BotSkillLevelsMap.find(s);

    return (got != BotSkillLevelsMap.end());
}

bot_skill CONFIG_GetBotSkillLevel(const char* SkillName)
{
    std::string s = SkillName;
    std::unordered_map<std::string, bot_skill>::const_iterator got = BotSkillLevelsMap.find(s);

    if (got == BotSkillLevelsMap.end())
    {
        return BotSkillLevelsMap["default"];
    }
    else
    {
        return got->second;
    }
}

bot_skill CONFIG_GetGlobalBotSkillLevel()
{
    return BotSkillLevelsMap[GlobalSkillLevel.c_str()];
}

void CONFIG_SetGlobalBotSkillLevel(const char* NewSkillLevel)
{
    if (!CONFIG_BotSkillLevelExists(NewSkillLevel))
    {
        LOG_CONSOLE(PLID, "Error when setting new global bot skill level '%s', does not exist! Check evobot.cfg for valid skill levels\n", NewSkillLevel);
        return;
    }

    GlobalSkillLevel = NewSkillLevel;
}
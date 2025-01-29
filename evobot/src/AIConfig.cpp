
#include "AIConfig.h"
#include "AIMath.h"
#include "AIHelper.h"

#include <unordered_map>
#include <algorithm>
#include <random>
#include <fstream>

char BotPrefix[32] = "";

std::string DefaultBotNames[MAX_PLAYERS] = { "MrRobot",
                                    "Wall-E",
                                    "BeepBoop",
                                    "Robotnik",
                                    "JonnyAutomaton",
                                    "Burninator",
                                    "SteelDeath",
                                    "Meatbag",
                                    "Undertaker",
                                    "Botini",
                                    "Robottle",
                                    "Rusty",
                                    "HeavyMetal",
                                    "Combot",
                                    "BagelLover",
                                    "Screwdriver",
                                    "LoveBug",
                                    "iSmash",
                                    "Chippy",
                                    "Baymax",
                                    "BoomerBot",
                                    "Jarvis",
                                    "Marvin",
                                    "Data",
                                    "Scrappy",
                                    "Mortis",
                                    "TerrorHertz",
                                    "Omicron",
                                    "Herbie",
                                    "Robogeddon",
                                    "Velociripper",
                                    "TerminalFerocity" };

std::vector<std::string> BotNames;
int CurrentNameIndex = 0;

float BotMaxStuckTime = 15.0f;



float CONFIG_GetMaxStuckTime()
{
    return BotMaxStuckTime;
}

std::string CONFIG_GetBotPrefix()
{
    return std::string(BotPrefix);
}

void CONFIG_ParseConfigFile()
{

    char filename[256];

    UTIL_BuildFileName(filename, "addons", "evobot", "evobot.cfg", NULL);

    std::ifstream cFile(filename);
    if (cFile.is_open())
    {
        std::string line;
        int CurrSkillIndex = -1;
        bool bSkillLevelWarningGiven = false;

        while (getline(cFile, line))
        {
            line.erase(std::remove_if(line.begin(), line.end(), isspace),
                line.end());
            if (line[0] == '#' || line.empty())
                continue;
            auto delimiterPos = line.find("=");
            auto key = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);

            const char* keyChar = key.c_str();

            if (!stricmp(keyChar, "Prefix"))
            {
                sprintf(BotPrefix, value.c_str());

                continue;
            }  
        }
    }
    else
    {
        ALERT(at_console, "evobot.cfg was not found in the evobot folder. You can regenerate it with the console command 'evobot regencfg'");
    }
}

void CONFIG_RegenerateIniFile()
{
    char filename[256];
    UTIL_BuildFileName(filename, "addons", "evobot", "evobot.cfg", NULL);

    FILE* NewConfigFile = fopen(filename, "w+");

    if (!NewConfigFile)
    {
        char ErrMsg[256];
        sprintf(ErrMsg, "Unable to write to %s, please ensure the user has privileges\n", filename);
        g_engfuncs.pfnServerPrint(ErrMsg);
        return;
    }

    fprintf(NewConfigFile, "### General bot settings ###\n\n");

    fprintf(NewConfigFile, "# What prefix to put in front of a bot's name (can leave blank)\n");
    fprintf(NewConfigFile, "Prefix=[BOT]\n\n");

    fflush(NewConfigFile);
    fclose(NewConfigFile);

    char ErrMsg[256];
    sprintf(ErrMsg, "Regenerated %s\n", filename);
    g_engfuncs.pfnServerPrint(ErrMsg);

}

void CONFIG_PopulateBotNames()
{
    BotNames.clear();

    char filename[256];
    UTIL_BuildFileName(filename, "addons", "evobot", "botnames.txt", NULL);

    std::ifstream cFile(filename);

    if (cFile.is_open())
    {
        std::string line;
        while (getline(cFile, line))
        {
            if (line[0] == '/' || line.empty())
                continue;

            BotNames.push_back(line);
        }
    }

    if (BotNames.size() > 2)
    {
        auto rng = std::default_random_engine{};
        std::shuffle(begin(BotNames), end(BotNames), rng);
    }

    std::vector<std::string> DefaultNames;

    // Ensure we have 32 names for all bots
    for (int i = BotNames.size(); i < MAX_PLAYERS; i++)
    {
        DefaultNames.push_back(DefaultBotNames[i]);
    }

    if (DefaultNames.size() > 2)
    {
        auto rng = std::default_random_engine{};
        std::shuffle(begin(DefaultNames), end(DefaultNames), rng);
    }

    BotNames.insert(BotNames.end(), DefaultNames.begin(), DefaultNames.end());

    CurrentNameIndex = 0;

}

std::string CONFIG_GetNextBotName()
{
    if (BotNames.size() == 0) { return "Bot"; }

    std::string Result = BotNames[CurrentNameIndex];

    CurrentNameIndex++;

    if (CurrentNameIndex >= BotNames.size())
    {
        CurrentNameIndex = 0;
    }

    return Result;
}

void CONFIG_PrintHelpFile()
{
    char HelpFileName[256];

    FILE* HelpFile = NULL;
    UTIL_BuildFileName(HelpFileName, "addons", "evobot", "Help.txt", NULL);

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
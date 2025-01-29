//
// evobot - Neoptolemus' Recast/Detour base GoldSrc bot, based on Botman's HPB bot template
//
// bot_gorge.cpp
// 
// Contains gorge-related functions. Needs refactoring into helper function file
//

#include "AITactical.h"
#include "AINavigation.h"
#include "AIMath.h"
#include "AIPlayerUtil.h"
#include "AIHelper.h"
#include "AIConstants.h"
#include "AIPlayerManager.h"
#include "AIConfig.h"

#include <float.h>

#include "DetourTileCacheBuilder.h"

#include <unordered_map>

#include <string.h>


extern nav_mesh NavMeshes[NUM_NAV_MESHES]; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)

bool bNavMeshModified = false;
extern bool bTileCacheUpToDate;


void AITAC_UpdateMapAIData()
{
	NAV_UpdateDynamicMapObjects();
}

void AITAC_CheckNavMeshModified()
{
	if (bNavMeshModified)
	{
		AITAC_OnNavMeshModified();
	}
}

void AITAC_OnNavMeshModified()
{
	if (!NavmeshLoaded()) { return; }

	std::vector<AIPlayer*> AllAIPlayers = AIMGR_GetAllAIPlayers();

	for (auto it = AllAIPlayers.begin(); it != AllAIPlayers.end(); it++)
	{
		AIPlayer* ThisPlayer = (*it);

		if (IsPlayerActiveInGame(ThisPlayer->Edict) && ThisPlayer->BotNavInfo.CurrentPath.size() > 0)
		{
			ThisPlayer->BotNavInfo.NextForceRecalc = gpGlobals->time + frandrange(0.0f, 1.0f);
		}
	}

	bNavMeshModified = false;
}

void AITAC_InitialiseMapAIData()
{
	NAV_PopulateDynamicMapObjects();

	while (!bTileCacheUpToDate)
	{
		UTIL_UpdateTileCache();
	}
}

void AITAC_ClearMapAIData(bool bInitialMapLoad)
{
	if (bInitialMapLoad)
	{
		while (!bTileCacheUpToDate)
		{
			UTIL_UpdateTileCache();
		}
	}
}

int AITAC_GetNumActivePlayersOnTeam(const int Team)
{
	int Result = 0;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && !PlayerEdict->free && IsPlayerActiveInGame(PlayerEdict)) { Result++; }		
	}

	return Result;
}


std::vector<edict_t*> AITAC_GetAllPlayersOfTeamInArea(const int Team, const Vector SearchLocation, const float SearchRadius, const edict_t* IgnorePlayer)
{
	std::vector<edict_t*> Result;

	float MaxRadiusSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		if (PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float Dist = vDist2DSq(PlayerEdict->v.origin, SearchLocation);

			if (Dist <= MaxRadiusSq)
			{
				Result.push_back(PlayerEdict);
			}
		}
	}

	return Result;
}

int AITAC_GetNumPlayersOfTeamInArea(const int Team, const Vector SearchLocation, const float SearchRadius, const edict_t* IgnorePlayer)
{
	int Result = 0;
	float MaxRadiusSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		if (PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float Dist = vDist2DSq(PlayerEdict->v.origin, SearchLocation);

			if (Dist <= MaxRadiusSq)
			{
				Result++;
			}
		}
	}

	return Result;

}

bool AITAC_AnyPlayersOfTeamInArea(const int Team, const Vector SearchLocation, const float SearchRadius, const edict_t* IgnorePlayer)
{
	const float MaxRadiusSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (FNullEnt(PlayerEdict) || PlayerEdict->free || PlayerEdict == IgnorePlayer) { continue; }

		if (PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			const float Dist = vDist2DSq(PlayerEdict->v.origin, SearchLocation);

			if (Dist <= MaxRadiusSq)
			{
				return true;
			}
		}
	}

	return false;
}

edict_t* AITAC_GetClosestPlayerOnTeamWithLOS(const int Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer)
{
	const float distSq = sqrf(SearchRadius);
	float MinDist = 0.0f;
	edict_t* Result = nullptr;

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict != IgnorePlayer && PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq && UTIL_QuickTrace(PlayerEdict, GetPlayerEyePosition(PlayerEdict), Location))
			{
				if (FNullEnt(Result) || ThisDist < MinDist)
				{
					Result = PlayerEdict;
					MinDist = ThisDist;
				}

			}
		}
	}

	return Result;
}

bool AITAC_AnyPlayerOnTeamHasLOSToLocation(int Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer)
{
	const float distSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict != IgnorePlayer && PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq && UTIL_QuickTrace(PlayerEdict, GetPlayerEyePosition(PlayerEdict), Location))
			{
				return true;
			}
		}
	}

	return false;
}

std::vector<edict_t*> AITAC_GetAllPlayersOnTeamWithLOS(int Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer)
{
	std::vector<edict_t*> Results;

	const float distSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict != IgnorePlayer && PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq && UTIL_QuickTrace(PlayerEdict, GetPlayerEyePosition(PlayerEdict), Location))
			{
				Results.push_back(PlayerEdict);
			}
		}
	}

	return Results;
}

int AITAC_GetNumPlayersOnTeamWithLOS(int Team, const Vector& Location, float SearchRadius, edict_t* IgnorePlayer)
{
	int Result = 0;

	const float distSq = sqrf(SearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict != IgnorePlayer && PlayerEdict->v.team == Team && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq && UTIL_QuickTrace(PlayerEdict, GetPlayerEyePosition(PlayerEdict), Location))
			{
				Result++;
			}
		}
	}

	return Result;
}

edict_t* AITAC_GetNearestHumanAtLocation(const int Team, const Vector Location, const float MaxSearchRadius)
{
	edict_t* Result = nullptr;

	const float distSq = sqrf(MaxSearchRadius);

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* PlayerEdict = INDEXENT(i);

		if (!FNullEnt(PlayerEdict) && PlayerEdict->v.team == Team && !(PlayerEdict->v.flags & FL_FAKECLIENT) && IsPlayerActiveInGame(PlayerEdict))
		{
			float ThisDist = vDist2DSq(PlayerEdict->v.origin, Location);

			if (ThisDist <= distSq)
			{
				Result = PlayerEdict;
			}
		}
	}

	return Result;
}

int AITAC_GetNumDeadPlayersOnTeam(const int Team)
{
	int Result = 0;

	const std::vector<edict_t*> TeamPlayers = AIMGR_GetAllPlayersOnTeam(Team);

	for (auto it = TeamPlayers.begin(); it != TeamPlayers.end(); it++)
	{
		if (IsPlayerDead(*it)) { Result++; }
	}

	return Result;
}

bool AITAC_AnyPlayerOnTeamWithLOS(int Team, const Vector& Location, float SearchRadius)
{
	const float distSq = sqrf(SearchRadius);

	std::vector<edict_t*> Players = AIMGR_GetAllPlayersOnTeam(Team);

	for (auto it = Players.begin(); it != Players.end(); it++)
	{
		edict_t* PlayerRef = (*it);

		if (!IsPlayerActiveInGame(PlayerRef)) { continue; }

		if (vDist2DSq(PlayerRef->v.origin, Location) <= distSq && UTIL_QuickTrace(PlayerRef, GetPlayerEyePosition(PlayerRef), Location))
		{
			return true;
		}
	}

	return false;
}

Vector AITAC_GetRandomHintInLocation(unsigned int NavMeshIndex, const unsigned int HintFlags, const Vector SearchLocation, const float SearchRadius)
{
	Vector Result = ZERO_VECTOR;

	std::vector<NavHint*> IPHints = NAV_GetHintsOfTypeInRadius(NavMeshIndex, HintFlags, SearchLocation, SearchRadius);
	int WinningRoll = 0;

	for (auto it = IPHints.begin(); it != IPHints.end(); it++)
	{
		NavHint* ThisHint = (*it);

		int ThisRoll = irandrange(0, 10);

		if (vIsZero(Result) || ThisRoll > WinningRoll)
		{
			Result = ThisHint->Position;
			WinningRoll = ThisRoll;
		}
	}

	return Result;
}
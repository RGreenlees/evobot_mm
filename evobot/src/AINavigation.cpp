//
// evobot - Neoptolemus' Recast/Detour base GoldSrc bot, based on Botman's HPB bot template
//
// bot_navigation.cpp
// 
// Handles all bot path finding and movement
//

#include "AINavigation.h"
#include "AIMath.h"
#include "AIPlayerUtil.h"
#include "AIHelper.h"
#include "AIPlayerManager.h"
#include "AITactical.h"
#include "AIWeaponHelper.h"
#include "AIConfig.h"

#include <stdlib.h>
#include <math.h>

#include <extdll.h>
#include <dllapi.h>

#include "DetourNavMesh.h"
#include "DetourCommon.h"
#include "DetourTileCache.h"
#include "DetourTileCacheBuilder.h"
#include "DetourNavMeshBuilder.h"
#include "fastlz.c"
#include "DetourAlloc.h"

#include <cfloat>

using namespace std;

vector<DynamicMapPrototype> MapObjectPrototypes;
vector<DynamicMapObject> DynamicMapObjects;

nav_mesh NavMeshes[NUM_NAV_MESHES] = { };

vector<NavAgentProfile> BaseAgentProfiles;

AINavMeshStatus NavmeshStatus = NAVMESH_STATUS_PENDING;

extern bool bNavMeshModified;

bool bTileCacheUpToDate = false;

struct NavMeshSetHeader
{
	int magic;
	int version;
	int numTiles;
	dtNavMeshParams params;
	int MeshBuildOffset;
};

struct TileCacheSetHeader
{
	int magic = 0;
	int version = 0;
	int numTiles = 0;
	dtNavMeshParams meshParams;
	dtTileCacheParams cacheParams;

	int NumOffMeshCons = 0;
	int OffMeshConsOffset = 0;

	int NumConvexVols = 0;
	int ConvexVolsOffset = 0;

	int NumNavHints = 0;
	int NavHintsOffset = 0;
};

struct TileCacheExportHeader
{
	int magic;
	int version;

	int numTileCaches;
	int tileCacheDataOffset = 0;

	int tileCacheOffsets[8];

	int NumSurfTypes;
	int SurfTypesOffset;
};

struct TileCacheTileHeader
{
	dtCompressedTileRef tileRef;
	int dataSize;
};

struct NavMeshTileHeader
{
	dtTileRef tileRef;
	int dataSize;
};

struct OffMeshConnectionDef
{
	unsigned int UserID = 0;
	float spos[3] = { 0.0f, 0.0f, 0.0f };
	float epos[3] = { 0.0f, 0.0f, 0.0f };
	bool bBiDir = false;
	float Rad = 0.0f;
	unsigned char Area = 0;
	unsigned int Flag = 0;
	bool bPendingDelete = false;
	bool bDirty = false;
};

struct FastLZCompressor : public dtTileCacheCompressor
{
	virtual int maxCompressedSize(const int bufferSize)
	{
		return (int)(bufferSize * 1.05f);
	}

	virtual dtStatus compress(const unsigned char* buffer, const int bufferSize,
		unsigned char* compressed, const int /*maxCompressedSize*/, int* compressedSize)
	{
		*compressedSize = fastlz_compress((const void* const)buffer, bufferSize, compressed);
		return DT_SUCCESS;
	}

	virtual dtStatus decompress(const unsigned char* compressed, const int compressedSize,
		unsigned char* buffer, const int maxBufferSize, int* bufferSize)
	{
		*bufferSize = fastlz_decompress(compressed, compressedSize, buffer, maxBufferSize);
		return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
	}
};

struct LinearAllocator : public dtTileCacheAlloc
{
	unsigned char* buffer;
	size_t capacity;
	size_t top;
	size_t high;

	LinearAllocator(const size_t cap) : buffer(0), capacity(0), top(0), high(0)
	{
		resize(cap);
	}

	~LinearAllocator()
	{
		dtFree(buffer);
	}

	void resize(const size_t cap)
	{
		if (buffer) dtFree(buffer);
		buffer = (unsigned char*)dtAlloc(cap, DT_ALLOC_PERM);
		capacity = cap;
	}

	virtual void reset()
	{
		high = dtMax(high, top);
		top = 0;
	}

	virtual void* alloc(const size_t size)
	{
		if (!buffer)
			return 0;
		if (top + size > capacity)
			return 0;
		unsigned char* mem = &buffer[top];
		top += size;
		return mem;
	}

	virtual void free(void* /*ptr*/)
	{
		// Empty
	}
};

struct MeshProcess : public dtTileCacheMeshProcess
{

	inline MeshProcess()
	{}

	inline void init(OffMeshConnectionDef* OffMeshConnData, int NumConns)
	{

	}

	virtual void process(struct dtNavMeshCreateParams* params,
		unsigned char* polyAreas, unsigned int* polyFlags)
	{
		// Update poly flags from areas.
		for (int i = 0; i < params->polyCount; ++i)
		{
			const NavMovementFlag NewFlag = GetFlagForArea((NavArea)polyAreas[i]);

			if (NewFlag != NAV_FLAG_ALL)
			{
				polyFlags[i] = GetFlagForArea((NavArea)polyAreas[i]);
			}
		}

	}
};

void AIDEBUG_DrawOffMeshConnections(unsigned int NavMeshIndex, float DrawTime)
{
	if (NavMeshes[NavMeshIndex].tileCache)
	{
		int DrawnConnections = 0;

		for (auto it = NavMeshes[NavMeshIndex].MeshConnections.begin(); it != NavMeshes[NavMeshIndex].MeshConnections.end(); it++)
		{

			unsigned char R, G, B;
			GetDebugColorForFlag((NavMovementFlag)it->ConnectionFlags, R, G, B);

			UTIL_DrawLine(INDEXENT(1), it->FromLocation, it->ToLocation, DrawTime, R, G, B);
			DrawnConnections++;

			if (DrawnConnections == 20) { break; }
		}
	}
}

void AIDEBUG_DrawTemporaryObstacles(unsigned int NavMeshIndex, float DrawTime)
{
	if (NavMeshes[NavMeshIndex].tileCache)
	{
		int DrawnConnections = 0;

		for (auto it = NavMeshes[NavMeshIndex].TempObstacles.begin(); it != NavMeshes[NavMeshIndex].TempObstacles.end(); it++)
		{

			unsigned char R, G, B;
			GetDebugColorForArea((NavArea)it->Area, R, G, B);

			Vector bMin = Vector(it->Location.x - it->Radius, it->Location.y - it->Radius, it->Location.z - (it->Height * 0.5f));
			Vector bMax = Vector(it->Location.x + it->Radius, it->Location.y + it->Radius, it->Location.z + (it->Height * 0.5f));

			UTIL_DrawBox(INDEXENT(1), bMin, bMax, DrawTime, R, G, B);
			DrawnConnections++;

			if (DrawnConnections == 20) { break; }
		}
	}
}

bool UTIL_UpdateTileCache()
{
	bool bNewTileCacheUpToDate = true;

	for (int i = 0; i < NUM_NAV_MESHES; i++)
	{
		if (NavMeshes[i].tileCache)
		{
			bool bUpToDate;
			NavMeshes[i].tileCache->update(0.0f, NavMeshes[i].navMesh, &bUpToDate);
			if (!bUpToDate) { bNewTileCacheUpToDate = false; }
		}
	}

	if (!bTileCacheUpToDate && bNewTileCacheUpToDate)
	{
		bNavMeshModified = true;
	}

	bTileCacheUpToDate = bNewTileCacheUpToDate;

	return bTileCacheUpToDate;
}

Vector UTIL_AdjustPointAwayFromNavWall(const Vector Location, const float MaxDistanceFromWall)
{

	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(BaseAgentProfiles[0]);
	const dtQueryFilter* m_navFilter = &GetBaseAgentProfile(NAV_PROFILE_DEFAULT).Filters;

	float Pos[3] = { Location.x, Location.z, -Location.y };

	float HitDist = 0.0f;
	float HitPos[3] = { 0.0f, 0.0f, 0.0f };
	float HitNorm[3] = { 0.0f, 0.0f, 0.0f };

	dtPolyRef StartPoly = UTIL_GetNearestPolyRefForLocation(Location);

	dtStatus Result = m_navQuery->findDistanceToWall(StartPoly, Pos, MaxDistanceFromWall, m_navFilter, &HitDist, HitPos, HitNorm);

	if (dtStatusSucceed(Result))
	{
		float AdjustDistance = MaxDistanceFromWall - HitDist;

		Vector HitPosVector = Vector(HitPos[0], -HitPos[2], HitPos[1]);

		Vector AdjustDir = (HitDist > 0.1f) ? UTIL_GetVectorNormal2D(Location - HitPosVector) : Vector(HitNorm[0], -HitNorm[2], HitNorm[1]);

		Vector AdjustLocation = Location + (AdjustDir * AdjustDistance);

		float AdjustLoc[3] = { AdjustLocation.x, AdjustLocation.z, -AdjustLocation.y };

		if (UTIL_TraceNav(GetBaseAgentProfile(NAV_PROFILE_DEFAULT), Location, AdjustLocation, 0.1f))
		{
			return AdjustLocation;
		}
		else
		{
			return Location;
		}
	}

	return Location;
}

Vector UTIL_GetNearestPointOnNavWall(AIPlayer* pBot, const float MaxRadius)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	Vector Location = pBot->CurrentFloorPosition;

	float Pos[3] = { Location.x, Location.z, -Location.y };

	float HitDist = 0.0f;
	float HitPos[3] = { 0.0f, 0.0f, 0.0f };
	float HitNorm[3] = { 0.0f, 0.0f, 0.0f };

	dtStatus Result = m_navQuery->findDistanceToWall(pBot->BotNavInfo.CurrentPoly, Pos, MaxRadius, m_navFilter, &HitDist, HitPos, HitNorm);

	if (dtStatusSucceed(Result) && HitDist > 0.0f)
	{
		Vector HitResult = Vector(HitPos[0], -HitPos[2], HitPos[1]);
		return HitResult;
	}

	return ZERO_VECTOR;
}

Vector UTIL_GetNearestPointOnNavWall(const NavAgentProfile &NavProfile, const Vector Location, const float MaxRadius)
{

	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	// Invalid nav profile
	if (!m_navQuery) { return ZERO_VECTOR; }

	dtPolyRef StartPoly = UTIL_GetNearestPolyRefForLocation(NavProfile, Location);

	// Not on the nav mesh
	if (StartPoly == 0) { return Location; }

	float Pos[3] = { Location.x, Location.z, -Location.y };

	float HitDist = 0.0f;
	float HitPos[3] = { 0.0f, 0.0f, 0.0f };
	float HitNorm[3] = { 0.0f, 0.0f, 0.0f };

	dtStatus Result = m_navQuery->findDistanceToWall(StartPoly, Pos, MaxRadius, m_navFilter, &HitDist, HitPos, HitNorm);

	// We hit something
	if (dtStatusSucceed(Result) && HitDist < MaxRadius)
	{
		Vector HitResult = Vector(HitPos[0], -HitPos[2], HitPos[1]);
		return HitResult;
	}

	// Didn't hit anything
	return ZERO_VECTOR;
}

NavTempObstacle* NAV_AddTemporaryObstacleToNavmesh(unsigned int NavMeshIndex, Vector Position, float Radius, float Height, unsigned char Area)
{
	if (vIsZero(Position) || Radius == 0.0f || Height == 0.0f) { return nullptr; }

	nav_mesh* ChosenMesh = &NavMeshes[NavMeshIndex];

	if (ChosenMesh->tileCache)
	{
		// Convert to Detour coordinate system, and adjust so position is the centre of the obstacle rather than bottom
		float Pos[3] = { Position.x, Position.z - (Height * 0.5f), -Position.y };

		dtObstacleRef ObsRef = 0;
		dtStatus status = NavMeshes[NavMeshIndex].tileCache->addObstacle(Pos, Radius, Height, Area, &ObsRef);

		if (dtStatusSucceed(status))
		{
			NavTempObstacle NewObstacle;
			NewObstacle.NavMeshIndex = NavMeshIndex;
			NewObstacle.Location = Position;
			NewObstacle.Radius = Radius;
			NewObstacle.Height = Height;
			NewObstacle.Area = Area;
			NewObstacle.ObstacleRef = (unsigned int)ObsRef;

			ChosenMesh->TempObstacles.push_back(NewObstacle);

			return &(*prev(ChosenMesh->TempObstacles.end()));

		}
	}

	return nullptr;
}

bool NAV_RemoveTemporaryObstacleFromNavmesh(NavTempObstacle& ObstacleToRemove)
{
	if (ObstacleToRemove.NavMeshIndex >= NUM_NAV_MESHES) { return true; }

	nav_mesh* ChosenMesh = &NavMeshes[ObstacleToRemove.NavMeshIndex];

	if (!ChosenMesh->tileCache) { return true; }

	dtStatus status = ChosenMesh->tileCache->removeObstacle(ObstacleToRemove.ObstacleRef);

	return dtStatusSucceed(status);
}

void GetFullFilePath(char* buffer, const char* mapname)
{
	char filename[256];
	char NavName[128];

	strcpy(NavName, mapname);
	strcat(NavName, ".nav");

	UTIL_BuildFileName(filename, "addons", "evobot", "navmeshes", NavName);
	strcpy(buffer, filename);
}


void ReloadNavMeshes()
{
	vector<AIPlayer*> AllBots = AIMGR_GetAllAIPlayers();

	for (auto it = AllBots.begin(); it != AllBots.end(); it++)
	{
		AIPlayer* ThisPlayer = (*it);

		ClearBotMovement(ThisPlayer);
	}
	AITAC_ClearMapAIData(false);
	UnloadNavMeshes();
	bool bSuccess = LoadNavMesh(STRING(gpGlobals->mapname));

	if (bSuccess)
	{
		bool bTileCacheFullyUpdated = UTIL_UpdateTileCache();

		while (!bTileCacheFullyUpdated)
		{
			bTileCacheFullyUpdated = UTIL_UpdateTileCache();
		}
	}
}

void UnloadNavMeshes()
{
	for (int i = 0; i < NUM_NAV_MESHES; i++)
	{
		if (NavMeshes[i].navMesh)
		{
			dtFreeNavMesh(NavMeshes[i].navMesh);
			NavMeshes[i].navMesh = nullptr;
		}

		if (NavMeshes[i].navQuery)
		{
			dtFreeNavMeshQuery(NavMeshes[i].navQuery);
			NavMeshes[i].navQuery = nullptr;
		}

		if (NavMeshes[i].tileCache)
		{
			dtFreeTileCache(NavMeshes[i].tileCache);
			NavMeshes[i].tileCache = nullptr;
		}
		
		NavMeshes[i].MeshConnections.clear();
		NavMeshes[i].MeshHints.clear();
	}

	NavmeshStatus = NAVMESH_STATUS_PENDING;
}

void UnloadNavigationData()
{
	UnloadNavMeshes();

	BaseAgentProfiles.clear();

	AIMGR_ClearBotData();

}

bool LoadNavMesh(const char* mapname)
{
	UnloadNavMeshes();

	char filename[256]; // Full path to BSP file
	char SuccMsg[256];

	GetFullFilePath(filename, mapname);

	FILE* savedFile = fopen(filename, "rb");

	if (!savedFile) 
	{
		sprintf(SuccMsg, "No nav file found for %s in the navmeshes folder\n", mapname);
		g_engfuncs.pfnServerPrint(SuccMsg);
		g_engfuncs.pfnServerPrint("You will need to create one using the Nav Editor tool in the navmeshes folder, or download one\n");
		return false; 
	}

	LinearAllocator* m_talloc = new LinearAllocator(32000);
	FastLZCompressor* m_tcomp = new FastLZCompressor;
	MeshProcess* m_tmproc = new MeshProcess;

	// Read header.
	TileCacheExportHeader fileHeader;
	size_t headerReadReturnCode = fread(&fileHeader, sizeof(TileCacheExportHeader), 1, savedFile);
	if (headerReadReturnCode != 1)
	{
		// Error or early EOF
		UnloadNavMeshes();
		sprintf(SuccMsg, "The nav file for %s is corrupted or incompatible\n", mapname);
		g_engfuncs.pfnServerPrint(SuccMsg);
		g_engfuncs.pfnServerPrint("You will need to create one using the Nav Editor tool in the navmeshes folder, or download one\n");
		fclose(savedFile);
		return false;
	}
	if (fileHeader.magic != TILECACHESET_MAGIC)
	{
		UnloadNavMeshes();
		sprintf(SuccMsg, "The nav file for %s is using a different file version than expected\n", mapname);
		g_engfuncs.pfnServerPrint(SuccMsg);
		g_engfuncs.pfnServerPrint("You will need to create one using the Nav Editor tool in the navmeshes folder, or download one\n");
		fclose(savedFile);
		return false;
	}
	if (fileHeader.version != TILECACHESET_VERSION)
	{
		UnloadNavMeshes();
		sprintf(SuccMsg, "The nav file for %s is using a different file version than expected\n", mapname);
		g_engfuncs.pfnServerPrint(SuccMsg);
		g_engfuncs.pfnServerPrint("You will need to create one using the Nav Editor tool in the navmeshes folder, or download one\n");
		fclose(savedFile);
		return false;
	}

	fseek(savedFile, fileHeader.tileCacheDataOffset, SEEK_SET);

	for (int i = 0; i < fileHeader.numTileCaches; i++)
	{
		fseek(savedFile, fileHeader.tileCacheOffsets[i], SEEK_SET);

		dtFreeNavMesh(NavMeshes[i].navMesh);
		dtFreeTileCache(NavMeshes[i].tileCache);
		dtFreeNavMeshQuery(NavMeshes[i].navQuery);

		TileCacheSetHeader tcHeader;

		size_t headerReadReturnCode = fread(&tcHeader, sizeof(TileCacheSetHeader), 1, savedFile);
		if (headerReadReturnCode != 1)
		{
			// Error or early EOF
			UnloadNavMeshes();
			sprintf(SuccMsg, "The nav file for %s is corrupted or incompatible\n", mapname);
			g_engfuncs.pfnServerPrint(SuccMsg);
			g_engfuncs.pfnServerPrint("You will need to create one using the Nav Editor tool in the navmeshes folder, or download one\n");
			fclose(savedFile);
			return false;
		}

		NavMeshes[i].navMesh = dtAllocNavMesh();
		if (!NavMeshes[i].navMesh) 
		{
			// Error or early EOF
			UnloadNavMeshes();
			sprintf(SuccMsg, "Could not allocate memory for nav data.\n", mapname);
			g_engfuncs.pfnServerPrint(SuccMsg);
			fclose(savedFile);
			return false;
		}

		NavMeshes[i].tileCache = dtAllocTileCache();
		if (!NavMeshes[i].tileCache) 
		{
			// Error or early EOF
			UnloadNavMeshes();
			sprintf(SuccMsg, "Could not allocate memory for nav data.\n", mapname);
			g_engfuncs.pfnServerPrint(SuccMsg);
			fclose(savedFile);
			return false;
		}

		NavMeshes[i].navQuery = dtAllocNavMeshQuery();
		if (!NavMeshes[i].navQuery)
		{
			// Error or early EOF
			UnloadNavMeshes();
			sprintf(SuccMsg, "Could not allocate memory for nav data.\n", mapname);
			g_engfuncs.pfnServerPrint(SuccMsg);
			fclose(savedFile);
			return false;
		}

		dtStatus status = NavMeshes[i].navMesh->init(&tcHeader.meshParams);
		if (dtStatusFailed(status)) 
		{
			// Error or early EOF
			UnloadNavMeshes();
			sprintf(SuccMsg, "Failed to initialise nav mesh (bad data?)\n", mapname);
			g_engfuncs.pfnServerPrint(SuccMsg);
			fclose(savedFile);
			return false;
		}

		status = NavMeshes[i].tileCache->init(&tcHeader.cacheParams, m_talloc, m_tcomp, m_tmproc);
		if (dtStatusFailed(status))
		{
			// Error or early EOF
			UnloadNavMeshes();
			sprintf(SuccMsg, "Failed to initialise tile cache (bad data?)\n", mapname);
			g_engfuncs.pfnServerPrint(SuccMsg);
			fclose(savedFile);
			return false;
		}

		// Read tiles.
		for (int ii = 0; ii < tcHeader.numTiles; ++ii)
		{
			TileCacheTileHeader tileHeader;
			size_t tileHeaderReadReturnCode = fread(&tileHeader, sizeof(tileHeader), 1, savedFile);
			if (tileHeaderReadReturnCode != 1) { continue; }

			if (!tileHeader.tileRef || !tileHeader.dataSize)
				break;

			unsigned char* data = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
			if (!data) break;
			memset(data, 0, tileHeader.dataSize);
			size_t tileDataReadReturnCode = fread(data, tileHeader.dataSize, 1, savedFile);
			if (tileDataReadReturnCode != 1)
			{
				// Error or early EOF
				dtFree(data);
				UnloadNavMeshes();
				sprintf(SuccMsg, "The nav file for %s is corrupted or incompatible\n", mapname);
				g_engfuncs.pfnServerPrint(SuccMsg);
				fclose(savedFile);
				return false;
			}

			dtCompressedTileRef tile = 0;
			dtStatus addTileStatus = NavMeshes[i].tileCache->addTile(data, tileHeader.dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tile);
			if (dtStatusFailed(addTileStatus))
			{
				dtFree(data);
			}

			if (tile)
				NavMeshes[i].tileCache->buildNavMeshTile(tile, NavMeshes[i].navMesh);
		}

		status = NavMeshes[i].navQuery->init(NavMeshes[i].navMesh, 2048);

		if (dtStatusFailed(status))
		{
			UnloadNavMeshes();
			sprintf(SuccMsg, "Failed to initialise nav query (bad data?)\n", mapname);
			g_engfuncs.pfnServerPrint(SuccMsg);
			fclose(savedFile);
			return false;
		}

		fseek(savedFile, tcHeader.OffMeshConsOffset, SEEK_SET);

		for (int ii = 0; ii < tcHeader.NumOffMeshCons; ii++)
		{
			dtOffMeshConnection def;
			fread(&def, sizeof(dtOffMeshConnection), 1, savedFile);

			Vector Start = Vector(def.pos[0], -def.pos[2], def.pos[1]);
			Vector End = Vector(def.pos[3], -def.pos[5], def.pos[4]);

			NAV_AddOffMeshConnectionToNavmesh(i, Start, End, def.area, def.flags, def.bBiDir);
		}

		fseek(savedFile, tcHeader.NavHintsOffset, SEEK_SET);

		for (int ii = 0; ii < tcHeader.NumNavHints; ii++)
		{
			NavHint def;

			fread(&def, sizeof(NavHint), 1, savedFile);

			Vector Position = Vector(def.Position[0], -def.Position[2], def.Position[1]);

			NAV_AddHintToNavmesh(i, Position, def.HintTypes);
		}
	}

	fclose(savedFile);
	
	sprintf(SuccMsg, "Navigation data for %s loaded successfully\n", mapname);
	g_engfuncs.pfnServerPrint(SuccMsg);

	return true;
}

bool loadNavigationData(const char* mapname)
{
	// Unload the previous nav meshes if they're still loaded
	UnloadNavigationData();

	PopulateBaseAgentProfiles();

	if (!LoadNavMesh(mapname))
	{
		NavmeshStatus = NAVMESH_STATUS_FAILED;
		return false;
	}

	NavmeshStatus = NAVMESH_STATUS_SUCCESS;
	
	return true;
}

bool NavmeshLoaded()
{
	return NavMeshes[0].navMesh != nullptr;
}

AINavMeshStatus NAV_GetNavMeshStatus()
{
	return NavmeshStatus;
}

Vector UTIL_GetRandomPointOnNavmesh(const AIPlayer* pBot)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	if (!m_navQuery)
	{
		return ZERO_VECTOR;
	}

	Vector Result;

	dtPolyRef refPoly;

	float result[3];
	std::memset(result, 0, sizeof(result));

	dtStatus status = m_navQuery->findRandomPoint(m_navFilter, frand, &refPoly, result);

	if (dtStatusFailed(status))
	{
		return ZERO_VECTOR;
	}

	Result.x = result[0];
	Result.y = -result[2];
	Result.z = result[1];

	return Result;
}

Vector UTIL_GetRandomPointOnNavmeshInRadiusOfAreaType(NavMovementFlag Flag, const Vector origin, const float MaxRadius)
{
	const dtNavMeshQuery* m_NavQuery = UTIL_GetNavMeshQueryForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));

	if (!m_NavQuery) { return ZERO_VECTOR; }

	dtQueryFilter filter;
	filter.setExcludeFlags(0);
	filter.setIncludeFlags(Flag);

	Vector Result = ZERO_VECTOR;

	float pCheckLoc[3] = { origin.x, origin.z, -origin.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus foundPolyResult = m_NavQuery->findNearestPoly(pCheckLoc, pExtents, &filter, &FoundPoly, NavNearest);

	if (dtStatusFailed(foundPolyResult))
	{
		return ZERO_VECTOR;
	}

	dtPolyRef RandomPoly;
	float RandomPoint[3];

	dtStatus foundRandomPointResult = m_NavQuery->findRandomPointAroundCircle(FoundPoly, NavNearest, MaxRadius, &filter, frand, &RandomPoly, RandomPoint);


	if (dtStatusFailed(foundRandomPointResult))
	{
		return ZERO_VECTOR;
	}

	Result.x = RandomPoint[0];
	Result.y = -RandomPoint[2];
	Result.z = RandomPoint[1];

	return Result;
}

Vector UTIL_GetRandomPointOnNavmeshInRadius(const NavAgentProfile &NavProfile, const Vector origin, const float MaxRadius)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return ZERO_VECTOR; }

	Vector Result = ZERO_VECTOR;

	float pCheckLoc[3] = { origin.x, origin.z, -origin.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus foundPolyResult = m_navQuery->findNearestPoly(pCheckLoc, pExtents, m_navFilter, &FoundPoly, NavNearest);

	if (dtStatusFailed(foundPolyResult))
	{
		return ZERO_VECTOR;
	}

	dtPolyRef RandomPoly;
	float RandomPoint[3];

	dtStatus foundRandomPointResult = m_navQuery->findRandomPointAroundCircle(FoundPoly, NavNearest, MaxRadius, m_navFilter, frand, &RandomPoly, RandomPoint);


	if (dtStatusFailed(foundRandomPointResult))
	{
		return ZERO_VECTOR;
	}

	Result.x = RandomPoint[0];
	Result.y = -RandomPoint[2];
	Result.z = RandomPoint[1];

	return Result;
}

Vector UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(const NavAgentProfile& NavProfile, const Vector origin, const float MaxRadius)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return ZERO_VECTOR; }

	Vector Result = ZERO_VECTOR;

	float pCheckLoc[3] = { origin.x, origin.z, -origin.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus foundPolyResult = m_navQuery->findNearestPoly(pCheckLoc, pExtents, m_navFilter, &FoundPoly, NavNearest);

	if (dtStatusFailed(foundPolyResult))
	{
		return ZERO_VECTOR;
	}

	dtPolyRef RandomPoly;
	float RandomPoint[3];

	dtStatus foundRandomPointResult = m_navQuery->findRandomPointAroundCircleIgnoreReachability(FoundPoly, NavNearest, MaxRadius, m_navFilter, frand, &RandomPoly, RandomPoint);


	if (dtStatusFailed(foundRandomPointResult))
	{
		return ZERO_VECTOR;
	}

	Result.x = RandomPoint[0];
	Result.y = -RandomPoint[2];
	Result.z = RandomPoint[1];

	return Result;
}

Vector UTIL_GetRandomPointOnNavmeshInDonut(const NavAgentProfile& NavProfile, const Vector origin, const float MinRadius, const float MaxRadius)
{
	int maxIterations = 0;
	float MinRadiusSq = sqrf(MinRadius);

	while (maxIterations < 100)
	{
		Vector StartPoint = UTIL_GetRandomPointOnNavmeshInRadius(NavProfile, origin, MaxRadius);

		if (vDist2DSq(StartPoint, origin) > MinRadiusSq)
		{
			return StartPoint;
		}

		maxIterations++;
	}

	return ZERO_VECTOR;
}

Vector UTIL_GetRandomPointOnNavmeshInDonutIgnoreReachability(const NavAgentProfile& NavProfile, const Vector origin, const float MinRadius, const float MaxRadius)
{
	int maxIterations = 0;
	float MinRadiusSq = sqrf(MinRadius);

	while (maxIterations < 100)
	{
		Vector StartPoint = UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(NavProfile, origin, MaxRadius);

		if (vDist2DSq(StartPoint, origin) > MinRadiusSq)
		{
			return StartPoint;
		}

		maxIterations++;
	}

	return ZERO_VECTOR;
}

static float frand()
{
	return (float)rand() / (float)RAND_MAX;
}

Vector AdjustPointForPathfinding(unsigned int NavMeshIndex, const Vector Point, const NavAgentProfile& NavProfile)
{
	if (vIsZero(Point) || NavMeshIndex > NUM_NAV_MESHES) { return ZERO_VECTOR; }

	if (NAV_IsPointInSwimArea(Point)) { return Point; }

	Vector ProjectedPoint = UTIL_ProjectPointToNavmesh(NavMeshIndex, Point, NavProfile);

	int PointContents = UTIL_PointContents(ProjectedPoint);	

	if (PointContents == CONTENTS_SOLID)
	{
		int PointContents = UTIL_PointContents(ProjectedPoint + Vector(0.0f, 0.0f, 32.0f));

		if (PointContents != CONTENTS_SOLID && PointContents != CONTENTS_LADDER)
		{
			Vector TraceStart = ProjectedPoint + Vector(0.0f, 0.0f, 32.0f);
			Vector TraceEnd = TraceStart - Vector(0.0f, 0.0f, 50.0f);
			Vector NewPoint = UTIL_GetHullTraceHitLocation(TraceStart, TraceEnd, point_hull);

			if (!vIsZero(NewPoint)) { return NewPoint; }
		}
	}
	else
	{
		Vector TraceStart = ProjectedPoint + Vector(0.0f, 0.0f, 5.0f);
		Vector TraceEnd = TraceStart - Vector(0.0f, 0.0f, 32.0f);
		Vector NewPoint = UTIL_GetHullTraceHitLocation(TraceStart, TraceEnd, point_hull);

		if (!vIsZero(NewPoint)) { return NewPoint; }
	}

	return ProjectedPoint;

}

// Special path finding that takes flight movement into account
dtStatus FindFlightPathToPoint(const NavAgentProfile &NavProfile, Vector FromLocation, Vector ToLocation, vector<bot_path_node>& path, float MaxAcceptableDistance)
{
	TraceResult directHit;

	if (UTIL_QuickHullTrace(nullptr, FromLocation, ToLocation, head_hull, false))
	{
		path.clear();

		bot_path_node NewPathNode;
		NewPathNode.FromLocation = FromLocation;
		NewPathNode.Location = ToLocation;
		NewPathNode.area = NAV_AREA_WALK;
		NewPathNode.flag = NAV_FLAG_WALK;
		NewPathNode.poly = 0;
		NewPathNode.requiredZ = ToLocation.z;

		path.push_back(NewPathNode);

		return DT_SUCCESS;
	}

	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery || !m_navMesh || !m_navFilter || vIsZero(FromLocation) || vIsZero(ToLocation))
	{
		return DT_FAILURE;
	}

	Vector FromFloorLocation = AdjustPointForPathfinding(NavProfile.NavMeshIndex, FromLocation, NavProfile);
	Vector ToFloorLocation = AdjustPointForPathfinding(NavProfile.NavMeshIndex, ToLocation, NavProfile);

	float pStartPos[3] = { FromFloorLocation.x, FromFloorLocation.z, -FromFloorLocation.y };
	float pEndPos[3] = { ToFloorLocation.x, ToFloorLocation.z, -ToFloorLocation.y };

	dtStatus status;
	dtPolyRef StartPoly = 0;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	dtPolyRef EndPoly = 0;
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };
	dtPolyRef PolyPath[MAX_PATH_POLY];
	dtPolyRef StraightPolyPath[MAX_AI_PATH_SIZE];
	int nPathCount = 0;
	float StraightPath[MAX_AI_PATH_SIZE * 3];
	unsigned char straightPathFlags[MAX_AI_PATH_SIZE];
	std::memset(straightPathFlags, 0, sizeof(straightPathFlags));
	int nVertCount = 0;

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, pExtents, m_navFilter, &StartPoly, StartNearest);
	if (!StartPoly || (status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		//BotSay(pBot, "findNearestPoly start failed!");
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, pExtents, m_navFilter, &EndPoly, EndNearest);
	if (!EndPoly || (status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		//BotSay(pBot, "findNearestPoly end failed!");
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (nPathCount == 0) { return DT_FAILURE; }

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[nPathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(MaxAcceptableDistance))
		{
			return DT_FAILURE;
		}
		else
		{
			dtVcopy(EndNearest, epos);
		}
	}

	status = m_navQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, straightPathFlags, StraightPolyPath, &nVertCount, MAX_AI_PATH_SIZE, DT_STRAIGHTPATH_ALL_CROSSINGS);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't create a path
	}

	if (nVertCount == 0)
	{
		return DT_FAILURE; // couldn't find a path
	}

	path.clear();

	unsigned char CurrArea;
	unsigned char ThisArea;

	unsigned int CurrFlags;
	unsigned int ThisFlags;

	m_navMesh->getPolyArea(StraightPolyPath[0], &CurrArea);
	m_navMesh->getPolyFlags(StraightPolyPath[0], &CurrFlags);


	// At this point we have our path.  Copy it to the path store
	int nIndex = 0;
	TraceResult hit;
	Vector TraceStart;

	for (int nVert = 0; nVert < nVertCount; nVert++)
	{
		Vector NextPathPoint = ZERO_VECTOR;
		Vector PrevPoint = (path.size() > 0) ? path.back().Location : FromFloorLocation;

		// The path point output by Detour uses the OpenGL, right-handed coordinate system. Convert to Goldsrc coordinates
		NextPathPoint.x = StraightPath[nIndex++];
		NextPathPoint.z = StraightPath[nIndex++];
		NextPathPoint.y = -StraightPath[nIndex++];		

		m_navMesh->getPolyArea(StraightPolyPath[nVert], &ThisArea);
		m_navMesh->getPolyFlags(StraightPolyPath[nVert], &ThisFlags);

		if (ThisArea == NAV_AREA_WALK || ThisArea == NAV_AREA_CROUCH)
		{
			NextPathPoint = UTIL_AdjustPointAwayFromNavWall(NextPathPoint, 16.0f);
		}

		AdjustPointForPathfinding(NavProfile.NavMeshIndex, NextPathPoint, NavProfile);

		NextPathPoint.z += 20.0f;

		float NewRequiredZ = NextPathPoint.z;

		bot_path_node NextPathNode;

		if (CurrFlags == NAV_FLAG_PLATFORM)
		{
			NextPathNode.flag = CurrFlags;
		}
		else
		{
			NextPathNode.flag = NAV_FLAG_WALK;
		}
		
		NextPathNode.area = CurrArea;
		NextPathNode.poly = StraightPolyPath[nVert];


		if (CurrFlags == NAV_FLAG_JUMP)
		{
			float MaxHeight = (CurrFlags == NAV_FLAG_JUMP) ? fmaxf(PrevPoint.z, NextPathPoint.z) + 60.0f : UTIL_FindZHeightForWallClimb(PrevPoint, NextPathPoint, head_hull);

			NextPathNode.requiredZ = MaxHeight;
			NextPathNode.Location = PrevPoint;
			NextPathNode.Location.z = MaxHeight;
			NextPathNode.FromLocation = PrevPoint;

			PrevPoint = NextPathNode.Location;

			path.push_back(NextPathNode);

			NextPathNode.requiredZ = MaxHeight;
			NextPathNode.Location = NextPathPoint;
			NextPathNode.Location.z = MaxHeight;
			NextPathNode.FromLocation = PrevPoint;

			PrevPoint = NextPathNode.Location;

			path.push_back(NextPathNode);
		}
		else if (CurrFlags == NAV_FLAG_LADDER || CurrFlags == NAV_FLAG_FALL)
		{
			float MaxHeight = fmaxf(PrevPoint.z, NextPathPoint.z);

			NextPathNode.requiredZ = MaxHeight;
			NextPathNode.Location = (PrevPoint.z < NextPathPoint.z) ? PrevPoint : NextPathPoint;
			NextPathNode.Location.z = MaxHeight;
			NextPathNode.FromLocation = PrevPoint;

			PrevPoint = NextPathNode.Location;

			path.push_back(NextPathNode);
		}

		NextPathNode.requiredZ = NextPathPoint.z;
		NextPathNode.Location = NextPathPoint;
		NextPathNode.FromLocation = PrevPoint;

		path.push_back(NextPathNode);

		CurrArea = ThisArea;
		CurrFlags = ThisFlags;

	}

	bot_path_node FinalInitialPathNode;
	FinalInitialPathNode.FromLocation = (path.size() > 0) ? path.back().Location : FromLocation;
	FinalInitialPathNode.Location = ToLocation;
	FinalInitialPathNode.area = NAV_AREA_WALK;
	FinalInitialPathNode.flag = NAV_FLAG_LADDER;
	FinalInitialPathNode.poly = 0;
	FinalInitialPathNode.requiredZ = ToLocation.z;

	path.push_back(FinalInitialPathNode);


	return DT_SUCCESS;
}

Vector UTIL_FindHighestSuccessfulTracePoint(const Vector TraceFrom, const Vector TargetPoint, const Vector NextPoint, const float IterationStep, const float MinIdealHeight, const float MaxHeight)
{
	Vector OriginTrace = TraceFrom;
	float AddedHeight = 0.0f;

	bool bFoundInitialPoint = false;
	Vector CurrentHighest = ZERO_VECTOR;

	int NumIterations = (int)ceilf(MaxHeight / IterationStep);

	Vector CurrentTarget = TargetPoint;

	for (int i = 0; i <= NumIterations; i++)
	{
		if (!UTIL_QuickTrace(nullptr, TargetPoint, CurrentTarget)) { return CurrentHighest; }

		if (!UTIL_QuickHullTrace(nullptr, OriginTrace, CurrentTarget, head_hull))
		{
			if (bFoundInitialPoint) { break; }
		}
		else
		{
			bFoundInitialPoint = true;
			if (AddedHeight >= MinIdealHeight)
			{
				return CurrentTarget;
			}
			else
			{
				if (!vIsZero(NextPoint) && UTIL_QuickHullTrace(nullptr, CurrentTarget, NextPoint, head_hull, false))
				{
					CurrentHighest = CurrentTarget;
				}

			}

		}

		CurrentTarget.z += IterationStep;
		AddedHeight += IterationStep;
	}

	return CurrentHighest;
}

dtStatus FindPathClosestToPoint(const NavAgentProfile& NavProfile, const Vector FromLocation, const Vector ToLocation, vector<bot_path_node>& path, float MaxAcceptableDistance)
{
	if (NavProfile.bFlyingProfile)
	{
		return FindFlightPathToPoint(NavProfile, FromLocation, ToLocation, path, MaxAcceptableDistance);
	}

	if (NAV_IsPointInSwimArea(FromLocation) && NAV_IsPointInSwimArea(ToLocation))
	{
		if (UTIL_QuickTrace(nullptr, FromLocation, ToLocation, false))
		{
			path.clear();

			bot_path_node StartPoint;
			StartPoint.FromLocation = FromLocation;
			StartPoint.Location = ToLocation;
			StartPoint.area = NAV_AREA_WALK;
			StartPoint.flag = NAV_FLAG_WALK;
			
			path.push_back(StartPoint);

			return DT_SUCCESS;

		}
	}

	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery || !m_navMesh || !m_navFilter || vIsZero(FromLocation) || vIsZero(ToLocation))
	{
		return DT_FAILURE;
	}

	Vector FromFloorLocation = FromLocation;
	Vector ToFloorLocation = ToLocation;

	if (!NAV_IsPointInSwimArea(FromLocation))
	{
		FromFloorLocation = AdjustPointForPathfinding(NavProfile.NavMeshIndex, FromLocation, NavProfile);
	}
	else
	{
		TraceResult Hit;

		UTIL_TraceLine(FromLocation, FromLocation - Vector(0.0f, 0.0f, 1000.0f), ignore_monsters, nullptr, &Hit);

		if (Hit.flFraction < 1.0f)
		{
			FromFloorLocation = Hit.vecEndPos;
		}
	}

	if (!NAV_IsPointInSwimArea(ToLocation))
	{
		ToFloorLocation = AdjustPointForPathfinding(NavProfile.NavMeshIndex, ToLocation, NavProfile);
	}
	else
	{
		TraceResult Hit;

		UTIL_TraceLine(ToLocation, ToLocation - Vector(0.0f, 0.0f, 1000.0f), ignore_monsters, nullptr, &Hit);

		if (Hit.flFraction < 1.0f)
		{
			ToFloorLocation = Hit.vecEndPos;
		}
	}

	float pStartPos[3] = { FromFloorLocation.x, FromFloorLocation.z, -FromFloorLocation.y };
	float pEndPos[3] = { ToFloorLocation.x, ToFloorLocation.z, -ToFloorLocation.y };

	dtStatus status;
	dtPolyRef StartPoly;
	float StartNearest[3];
	dtPolyRef EndPoly;
	float EndNearest[3];
	dtPolyRef PolyPath[MAX_PATH_POLY];
	dtPolyRef StraightPolyPath[MAX_AI_PATH_SIZE];
	int nPathCount = 0;
	float StraightPath[MAX_AI_PATH_SIZE * 3];
	unsigned char straightPathFlags[MAX_AI_PATH_SIZE];
	std::memset(straightPathFlags, 0, sizeof(straightPathFlags));
	int nVertCount = 0;

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, pExtents, m_navFilter, &StartPoly, StartNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, pExtents, m_navFilter, &EndPoly, EndNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[nPathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(MaxAcceptableDistance))
		{
			if (!NAV_IsPointInSwimArea(ToLocation))
			{
				return DT_FAILURE;
			}
			else
			{
				TraceResult Hit;
				Vector StartTrace = Vector(epos[0], -epos[2], epos[1] + 5.0f);
				UTIL_TraceLine(StartTrace, ToLocation, ignore_monsters, nullptr, &Hit);

				if (Hit.fAllSolid || (Hit.flFraction < 1.0f && vDist3DSq(Hit.vecEndPos, ToLocation) > sqrf(MaxAcceptableDistance)))
				{
					return DT_FAILURE;
				}
				else
				{
					dtVcopy(EndNearest, epos);
				}
			}
		}
		else
		{
			dtVcopy(EndNearest, epos);
		}
	}

	status = m_navQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, straightPathFlags, StraightPolyPath, &nVertCount, MAX_AI_PATH_SIZE, DT_STRAIGHTPATH_AREA_CROSSINGS);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't create a path
	}

	if (nVertCount == 0)
	{
		return DT_FAILURE; // couldn't find a path
	}

	path.clear();

	unsigned int CurrFlags;
	unsigned char CurrArea;
	unsigned char ThisArea;
	unsigned int ThisFlags;

	m_navMesh->getPolyFlags(StraightPolyPath[0], &CurrFlags);
	m_navMesh->getPolyArea(StraightPolyPath[0], &CurrArea);

	// At this point we have our path.  Copy it to the path store
	int nIndex = 0;
	TraceResult hit;
	Vector TraceStart;

	Vector NodeFromLocation = FromFloorLocation;

	for (int nVert = 0; nVert < nVertCount; nVert++)
	{
		bot_path_node NextPathNode;

		NextPathNode.FromLocation = NodeFromLocation;

		NextPathNode.Location.x = StraightPath[nIndex++];
		NextPathNode.Location.z = StraightPath[nIndex++];
		NextPathNode.Location.y = -StraightPath[nIndex++];

		m_navMesh->getPolyArea(StraightPolyPath[nVert], &ThisArea);
		m_navMesh->getPolyFlags(StraightPolyPath[nVert], &ThisFlags);

		if (ThisArea == NAV_AREA_WALK || ThisArea == NAV_AREA_CROUCH)
		{
			NextPathNode.Location = UTIL_AdjustPointAwayFromNavWall(NextPathNode.Location, 16.0f);
		}

		TraceStart.x = NextPathNode.Location.x;
		TraceStart.y = NextPathNode.Location.y;
		TraceStart.z = NextPathNode.Location.z;

		UTIL_TraceLine(TraceStart, (TraceStart - Vector(0.0f, 0.0f, 100.0f)), ignore_monsters, ignore_glass, nullptr, &hit);

		if (hit.flFraction < 1.0f)
		{
			NextPathNode.Location = hit.vecEndPos;

			if (CurrFlags != NAV_FLAG_JUMP)
			{
				NextPathNode.Location.z += 20.0f;
			}
		}

		NextPathNode.requiredZ = NextPathNode.Location.z;

		if (CurrFlags == NAV_FLAG_LADDER)
		{
			float NewRequiredZ = UTIL_FindZHeightForWallClimb(NextPathNode.FromLocation, NextPathNode.Location, head_hull);
			//NextPathNode.requiredZ = fmaxf(NewRequiredZ, NextPathNode.Location.z);
			NextPathNode.requiredZ = NewRequiredZ;

			NextPathNode.requiredZ += 5.0f;
		}
		else
		{
			NextPathNode.requiredZ = NextPathNode.Location.z;
		}

		NextPathNode.flag = CurrFlags;
		NextPathNode.area = CurrArea;
		NextPathNode.poly = StraightPolyPath[nVert];

		CurrArea = ThisArea;
		CurrFlags = ThisFlags;

		NodeFromLocation = NextPathNode.Location;

		path.push_back(NextPathNode);
	}

	if (NAV_IsPointInSwimArea(ToLocation))
	{
		bot_path_node FinalSwimBit;
		FinalSwimBit.area = NAV_AREA_WALK;
		FinalSwimBit.flag = NAV_FLAG_WALK;
		FinalSwimBit.FromLocation = path.back().Location;
		FinalSwimBit.Location = ToLocation;

		path.push_back(FinalSwimBit);
	}

	return DT_SUCCESS;
}

Vector NAV_GetBotPathStartPoint(AIPlayer* pBot, const Vector& Destination)
{
	int NavMeshIndex = pBot->BotNavInfo.NavProfile.NavMeshIndex;

	if (pBot->Edict->v.flags & FL_INWATER)
	{
		return pBot->CurrentFloorPosition;
	}

	Vector Result = AdjustPointForPathfinding(NavMeshIndex, pBot->CurrentFloorPosition, pBot->BotNavInfo.NavProfile);

	// If the bot currently has a path, then let's calculate the navigation from the "from" point rather than our exact position right now
	if (pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size())
	{
		bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

		if (CurrentPathNode.flag == NAV_FLAG_WALK || CurrentPathNode.flag == NAV_FLAG_CROUCH)
		{
			bool bFromReachable = UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, CurrentPathNode.Location);
			bool bToReachable = UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, CurrentPathNode.FromLocation);
			if (bFromReachable && bToReachable)
			{
				Result = pBot->CurrentFloorPosition;
			}
			else if (bFromReachable)
			{
				Result = CurrentPathNode.FromLocation;
			}
			else
			{
				Result = CurrentPathNode.Location;
			}
		}
		else
		{
			Result = CurrentPathNode.FromLocation;
		}
	}
	else
	{
		// Add a slight bias towards trying to move forward if on a railing or other narrow bit of navigable terrain
		// rather than potentially dropping back off it the wrong way
		Vector GeneralDir = UTIL_GetVectorNormal2D(Destination - pBot->CurrentFloorPosition);
		Vector CheckLocation = Result + (GeneralDir * 16.0f);

		Vector AdjustedCheckLocation = AdjustPointForPathfinding(NavMeshIndex, CheckLocation, pBot->BotNavInfo.NavProfile);

		if (!vIsZero(AdjustedCheckLocation))
		{
			Result = AdjustedCheckLocation;
		}
	}

	return Result;
}

dtStatus FindPathClosestToPoint(AIPlayer* pBot, const BotMoveStyle MoveStyle, const Vector ToLocation, vector<bot_path_node>& path, float MaxAcceptableDistance)
{
	if (!pBot) { return DT_FAILURE; }

	bool bBotIsSwimming = (pBot->Edict->v.flags & FL_INWATER);

	// First check: if we're swimming, see if we can just swim directly to it!
	if (bBotIsSwimming && NAV_IsPointInSwimArea(ToLocation))
	{
		TraceResult Hit;
		int hull_index = GetPlayerHullIndex(pBot->Edict);

		UTIL_TraceHull(pBot->Edict->v.origin, ToLocation, ignore_monsters, hull_index, nullptr, &Hit);

		if (!Hit.fStartSolid && !Hit.fAllSolid)
		{
			if (Hit.flFraction >= 1.0f || vDist3DSq(Hit.vecEndPos, ToLocation) < sqrf(MaxAcceptableDistance))
			{
				path.clear();

				bot_path_node StartPoint;
				StartPoint.FromLocation = pBot->Edict->v.origin;
				StartPoint.Location = ToLocation;
				StartPoint.area = NAV_AREA_WALK;
				StartPoint.flag = NAV_FLAG_WALK;

				path.push_back(StartPoint);

				return DT_SUCCESS;
			}
		}
	}

	if (pBot->BotNavInfo.NavProfile.bFlyingProfile)
	{
		return FindFlightPathToPoint(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, ToLocation, path, MaxAcceptableDistance);
	}

	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	int NavMeshIndex = pBot->BotNavInfo.NavProfile.NavMeshIndex;

	if (!m_navQuery || !m_navMesh || !m_navFilter || vIsZero(ToLocation))
	{
		return DT_FAILURE;
	}

	Vector FromFloorLocation = NAV_GetBotPathStartPoint(pBot, ToLocation);

	DynamicMapObject* LiftReference = UTIL_GetLiftReferenceByEdict(pBot->Edict->v.groundentity);
	bool bMustDisembarkLiftFirst = false;
	Vector LiftStart = ZERO_VECTOR;
	Vector LiftEnd = ZERO_VECTOR;

	if (LiftReference)
	{
		LiftEnd = NAV_GetNearestPlatformDisembarkPoint(pBot->BotNavInfo.NavProfile, pBot->Edict, LiftReference);

		if (!vIsZero(LiftEnd))
		{
			FromFloorLocation = LiftEnd;

			NavOffMeshConnection LiftOffMesh = UTIL_GetOffMeshConnectionForPlatform(pBot->BotNavInfo.NavProfile, LiftReference);

			if (LiftOffMesh.IsValid())
			{
				LiftStart = (vEquals(LiftEnd, LiftOffMesh.ToLocation, 5.0f)) ? LiftOffMesh.FromLocation : LiftOffMesh.ToLocation;
				bMustDisembarkLiftFirst = true;
				FromFloorLocation = LiftEnd;
			}
		}
	}

	Vector ToFloorLocation = AdjustPointForPathfinding(NavMeshIndex, ToLocation, pBot->BotNavInfo.NavProfile);

	if (NAV_IsPointInSwimArea(ToLocation))
	{
		TraceResult Hit;

		UTIL_TraceLine(ToLocation, ToLocation - Vector(0.0f, 0.0f, 1000.0f), ignore_monsters, nullptr, &Hit);

		if (Hit.flFraction < 1.0f)
		{
			ToFloorLocation = Hit.vecEndPos;
		}
	}

	float pStartPos[3] = { FromFloorLocation.x, FromFloorLocation.z, -FromFloorLocation.y };
	float pEndPos[3] = { ToFloorLocation.x, ToFloorLocation.z, -ToFloorLocation.y };

	dtStatus status;
	dtPolyRef StartPoly;
	float StartNearest[3];
	dtPolyRef EndPoly;
	float EndNearest[3];
	dtPolyRef PolyPath[MAX_PATH_POLY];
	dtPolyRef StraightPolyPath[MAX_AI_PATH_SIZE];
	int nPathCount = 0;
	float StraightPath[MAX_AI_PATH_SIZE * 3];
	unsigned char straightPathFlags[MAX_AI_PATH_SIZE];
	std::memset(straightPathFlags, 0, sizeof(straightPathFlags));
	int nVertCount = 0;

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, pExtents, m_navFilter, &StartPoly, StartNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, pExtents, m_navFilter, &EndPoly, EndNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[nPathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(MaxAcceptableDistance))
		{
			if (!NAV_IsPointInSwimArea(ToLocation))
			{
				return DT_FAILURE;
			}
			else
			{
				TraceResult Hit;
				Vector StartTrace = Vector(epos[0], -epos[2], epos[1] + 5.0f);
				UTIL_TraceLine(StartTrace, ToLocation, ignore_monsters, nullptr, &Hit);

				if (Hit.fAllSolid || (Hit.flFraction < 1.0f && vDist3DSq(Hit.vecEndPos, ToLocation) > sqrf(MaxAcceptableDistance)))
				{
					return DT_FAILURE;
				}
				else
				{
					dtVcopy(EndNearest, epos);
				}
			}
		}
		else
		{
			dtVcopy(EndNearest, epos);
		}
	}

	status = m_navQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, straightPathFlags, StraightPolyPath, &nVertCount, MAX_AI_PATH_SIZE, DT_STRAIGHTPATH_AREA_CROSSINGS);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't create a path
	}

	if (nVertCount == 0)
	{
		return DT_FAILURE; // couldn't find a path
	}

	path.clear();

	unsigned int CurrFlags;
	unsigned char CurrArea;

	m_navMesh->getPolyFlags(StraightPolyPath[0], &CurrFlags);
	m_navMesh->getPolyArea(StraightPolyPath[0], &CurrArea);

	// At this point we have our path.  Copy it to the path store
	int nIndex = 0;
	TraceResult hit;

	pBot->BotNavInfo.SpecialMovementFlags = 0;

	Vector NodeFromLocation = FromFloorLocation;

	if (bMustDisembarkLiftFirst)
	{
		bot_path_node StartPathNode;
		StartPathNode.FromLocation = LiftStart;
		StartPathNode.Location = LiftEnd;
		StartPathNode.flag = NAV_FLAG_PLATFORM;
		StartPathNode.area = NAV_AREA_WALK;

		path.push_back(StartPathNode);

		NodeFromLocation = LiftEnd;
	}

	for (int nVert = 0; nVert < nVertCount; nVert++)
	{
		bot_path_node NextPathNode;

		NextPathNode.FromLocation = NodeFromLocation;

		// The nav mesh doesn't always align perfectly with the floor, so align each nav point with the floor after generation
		NextPathNode.Location.x = StraightPath[nIndex++];
		NextPathNode.Location.z = StraightPath[nIndex++];
		NextPathNode.Location.y = -StraightPath[nIndex++];

		NextPathNode.Location = UTIL_AdjustPointAwayFromNavWall(NextPathNode.Location, 16.0f);

		NextPathNode.Location = AdjustPointForPathfinding(NavMeshIndex, NextPathNode.Location, pBot->BotNavInfo.NavProfile);

		if (CurrFlags != NAV_FLAG_JUMP || NextPathNode.FromLocation.z > NextPathNode.Location.z)
		{
			NextPathNode.Location.z += GetPlayerOriginOffsetFromFloor(pBot->Edict, (CurrArea == NAV_FLAG_CROUCH)).z;
		}

		if (CurrFlags == NAV_FLAG_PLATFORM)
		{
			DynamicMapObject* PlatformRef = UTIL_GetClosestPlatformToPoints(NextPathNode.FromLocation, NextPathNode.Location);

			if (PlatformRef)
			{
				NextPathNode.Platform = PlatformRef->Edict;
			}
		}

		pBot->BotNavInfo.SpecialMovementFlags |= CurrFlags;

		// End alignment to floor

		// For ladders and wall climbing, calculate the climb height needed to complete the move.
		// This what allows bots to climb over railings without having to explicitly place nav points on the railing itself
		NextPathNode.requiredZ = NextPathNode.Location.z;

		if (CurrFlags == NAV_FLAG_LADDER)
		{
			int HullNum = GetPlayerHullIndex(pBot->Edict, false);
			Vector FromLocation = (path.size() > 0) ? path.back().Location : pBot->CurrentFloorPosition;
			float NewRequiredZ = UTIL_FindZHeightForWallClimb(FromLocation, NextPathNode.Location, head_hull);
			NextPathNode.requiredZ = fmaxf(NewRequiredZ, NextPathNode.Location.z);

			if (CurrFlags == NAV_FLAG_LADDER)
			{
				NextPathNode.requiredZ += 5.0f;
			}

		}
		else
		{
			NextPathNode.requiredZ = NextPathNode.Location.z;
		}

		NextPathNode.flag = CurrFlags;
		NextPathNode.area = CurrArea;
		NextPathNode.poly = StraightPolyPath[nVert];

		m_navMesh->getPolyFlags(StraightPolyPath[nVert], &CurrFlags);
		m_navMesh->getPolyArea(StraightPolyPath[nVert], &CurrArea);

		NodeFromLocation = NextPathNode.Location;

		path.push_back(NextPathNode);

	}

	if (NAV_IsPointInSwimArea(ToLocation))
	{
		bot_path_node FinalSwimBit;
		FinalSwimBit.area = NAV_AREA_WALK;
		FinalSwimBit.flag = NAV_FLAG_WALK;
		FinalSwimBit.FromLocation = path.back().Location;
		FinalSwimBit.Location = ToLocation;

		path.push_back(FinalSwimBit);
	}

	return DT_SUCCESS;
}

Vector NAV_GetNearestPlatformDisembarkPoint(const NavAgentProfile& NavProfile, edict_t* Rider, DynamicMapObject* LiftReference)
{
	if (!LiftReference) { return ZERO_VECTOR; }

	NavOffMeshConnection* NearestConnection = nullptr;
	float MinDist = 0.0f;

	nav_mesh ChosenNavMesh = NavMeshes[NavProfile.NavMeshIndex];


	for (auto it = ChosenNavMesh.MeshConnections.begin(); it != ChosenNavMesh.MeshConnections.end(); it++)
	{
		if (!(it->ConnectionFlags & NAV_FLAG_PLATFORM)) { continue; }

		if (it->LinkedObject == LiftReference->Edict)
		{
			float ThisDist = fminf(vDist3DSq(it->FromLocation, UTIL_GetClosestPointOnEntityToLocation(it->FromLocation, LiftReference->Edict)), vDist3DSq(it->ToLocation, UTIL_GetClosestPointOnEntityToLocation(it->ToLocation, LiftReference->Edict)));

			if (ThisDist < sqrf(100.0f) && (!NearestConnection || ThisDist < MinDist))
			{
				NearestConnection = &(*it);
				MinDist = ThisDist;
			}
		}
	}

	if (NearestConnection)
	{
		Vector NearestPointFromLocation = UTIL_GetClosestPointOnEntityToLocation(NearestConnection->FromLocation, LiftReference->Edict);
		NearestPointFromLocation.z = Rider->v.origin.z;

		Vector NearestPointToLocation = UTIL_GetClosestPointOnEntityToLocation(NearestConnection->ToLocation, LiftReference->Edict);
		NearestPointToLocation.z = Rider->v.origin.z;

		float DistFromLocation = vDist3DSq(NearestConnection->FromLocation, NearestPointFromLocation);
		float DistToLocation = vDist3DSq(NearestConnection->ToLocation, NearestPointToLocation);
		return (DistFromLocation < DistToLocation) ? NearestConnection->FromLocation : NearestConnection->ToLocation;
	}
	else
	{
		Vector NearestProjectedPoint = ZERO_VECTOR;
		Vector LiftCentre = UTIL_GetCentreOfEntity(LiftReference->Edict);
		float DisembarkHeight = (!FNullEnt(Rider)) ? GetPlayerBottomOfCollisionHull(Rider).z : LiftReference->Edict->v.absmax.z;

		Vector FrontLocation = Vector(LiftReference->Edict->v.absmax.x, LiftCentre.y, DisembarkHeight);
		Vector RearLocation = Vector(LiftReference->Edict->v.absmin.x, LiftCentre.y, DisembarkHeight);
		Vector LeftLocation = Vector(LiftCentre.x, LiftReference->Edict->v.absmin.y, DisembarkHeight);
		Vector RightLocation = Vector(LiftCentre.x, LiftReference->Edict->v.absmax.y, DisembarkHeight);

		float ProjectWidth = fmaxf((LiftReference->Edict->v.absmax.x - LiftReference->Edict->v.absmin.x) * 0.5f, (LiftReference->Edict->v.absmax.y - LiftReference->Edict->v.absmin.y) * 0.5f);
		ProjectWidth += 100.0f;

		Vector ProjectedLoc = UTIL_ProjectPointToNavmesh(NavProfile.NavMeshIndex, FrontLocation, NavProfile, Vector(ProjectWidth, ProjectWidth, 50.0f));

		if (!vIsZero(ProjectedLoc) && !vPointOverlaps2D(ProjectedLoc, LiftReference->Edict->v.absmin, LiftReference->Edict->v.absmax))
		{
			return ProjectedLoc;
		}

		ProjectedLoc = UTIL_ProjectPointToNavmesh(NavProfile.NavMeshIndex, RearLocation, NavProfile, Vector(ProjectWidth, ProjectWidth, 50.0f));

		if (!vIsZero(ProjectedLoc) && !vPointOverlaps2D(ProjectedLoc, LiftReference->Edict->v.absmin, LiftReference->Edict->v.absmax))
		{
			return ProjectedLoc;
		}

		ProjectedLoc = UTIL_ProjectPointToNavmesh(NavProfile.NavMeshIndex, LeftLocation, NavProfile, Vector(ProjectWidth, ProjectWidth, 50.0f));

		if (!vIsZero(ProjectedLoc) && !vPointOverlaps2D(ProjectedLoc, LiftReference->Edict->v.absmin, LiftReference->Edict->v.absmax))
		{
			return ProjectedLoc;
		}

		ProjectedLoc = UTIL_ProjectPointToNavmesh(NavProfile.NavMeshIndex, RightLocation, NavProfile, Vector(ProjectWidth, ProjectWidth, 50.0f));

		if (!vIsZero(ProjectedLoc) && !vPointOverlaps2D(ProjectedLoc, LiftReference->Edict->v.absmin, LiftReference->Edict->v.absmax))
		{
			return ProjectedLoc;
		}


		return ZERO_VECTOR;
	}

}

DynamicMapObject* UTIL_GetLiftReferenceByEdict(const edict_t* SearchEdict)
{
	if (FNullEnt(SearchEdict)) { return nullptr; }

	for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
	{
		if (it->Edict == SearchEdict)
		{			
			return &(*it);
		}
	}

	return nullptr;
}

NavOffMeshConnection UTIL_GetOffMeshConnectionForPlatform(const NavAgentProfile& NavProfile, DynamicMapObject* LiftRef)
{
	NavOffMeshConnection Result;

	if (!LiftRef) { return Result; }

	NavOffMeshConnection* NearestConnection = nullptr;
	float MinDist = 0.0f;

	nav_mesh ChosenNavMesh = NavMeshes[NavProfile.NavMeshIndex];

	for (auto it = ChosenNavMesh.MeshConnections.begin(); it != ChosenNavMesh.MeshConnections.end(); it++)
	{
		if (!(it->ConnectionFlags & NAV_FLAG_PLATFORM)) { continue; }

		if (it->LinkedObject == LiftRef->Edict)
		{
			return (*it);
		}
	}

	return Result;
}

bool UTIL_PointIsReachable(const NavAgentProfile &NavProfile, const Vector FromLocation, const Vector ToLocation, const float MaxAcceptableDistance)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery || vIsZero(FromLocation) || vIsZero(ToLocation))
	{
		return false;
	}

	bool bStartInWater = NAV_IsPointInSwimArea(FromLocation);
	bool bEndInWater = NAV_IsPointInSwimArea(ToLocation);

	if (bStartInWater && bEndInWater)
	{
		if (UTIL_QuickHullTrace(nullptr, FromLocation, ToLocation)) { return true; }
	}

	float pStartPos[3] = {FromLocation.x, FromLocation.z, -FromLocation.y};
	float pEndPos[3] = {ToLocation.x, ToLocation.z, -ToLocation.y};

	if (bStartInWater)
	{
		TraceResult Hit;
		UTIL_TraceLine(FromLocation, FromLocation - Vector(0.0f, 0.0f, 1000.0f), ignore_monsters, nullptr, &Hit);

		if (Hit.flFraction < 1.0f)
		{
			pStartPos[0] = Hit.vecEndPos.x;
			pStartPos[1] = Hit.vecEndPos.z;
			pStartPos[2] = -Hit.vecEndPos.y;
		}
	}

	if (bEndInWater)
	{
		TraceResult Hit;
		UTIL_TraceLine(ToLocation, ToLocation - Vector(0.0f, 0.0f, 1000.0f), ignore_monsters, nullptr, &Hit);

		if (Hit.flFraction < 1.0f)
		{
			pEndPos[0] = Hit.vecEndPos.x;
			pEndPos[1] = Hit.vecEndPos.z;
			pEndPos[2] = -Hit.vecEndPos.y;
		}
	}

	dtStatus status;
	dtPolyRef StartPoly;
	float StartNearest[3];
	dtPolyRef EndPoly;
	float EndNearest[3];
	dtPolyRef PolyPath[MAX_PATH_POLY];
	int nPathCount = 0;
	
	float searchExtents[3] = { MaxAcceptableDistance, 50.0f, MaxAcceptableDistance };

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, searchExtents, m_navFilter, &StartPoly, StartNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return false; // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, searchExtents, m_navFilter, &EndPoly, EndNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return false; // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (nPathCount == 0)
	{
		return false; // couldn't find a path
	}

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[nPathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) <= sqrf(MaxAcceptableDistance))
		{
			return true;
		}
		else
		{
			if (NAV_IsPointInSwimArea(epos) && NAV_IsPointInSwimArea(ToLocation))
			{
				return true;
			}
		}

	}

	return true;
}

bool HasBotReachedPathPoint(const AIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		return true;
	}

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	NavMovementFlag CurrentNavFlag = (NavMovementFlag)CurrentPathNode.flag;
	Vector MoveFrom = CurrentPathNode.FromLocation;
	Vector MoveTo = CurrentPathNode.Location;
	float RequiredClimbHeight = CurrentPathNode.requiredZ;

	Vector NextMoveLocation = ZERO_VECTOR;
	NavMovementFlag NextMoveFlag = NAV_FLAG_DISABLED;

	if ((pBot->BotNavInfo.CurrentPathPoint + 1) < pBot->BotNavInfo.CurrentPath.size())
	{
		bot_path_node NextPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint + 1];
		NextMoveLocation = NextPathNode.Location;
		NextMoveFlag = (NavMovementFlag)NextPathNode.flag;
	}

	if (NAV_IsPointInSwimArea(MoveTo) || pBot->BotNavInfo.NavProfile.bFlyingProfile)
	{
		Vector ClosestPointToPath = vClosestPointOnLine(MoveFrom, MoveTo, pBot->Edict->v.origin);
		bool bAtOrPastDestination = vEquals(ClosestPointToPath, MoveTo, 32.0f);

		return vPointOverlaps3D(MoveTo, pBot->Edict->v.absmin, pBot->Edict->v.absmax) || bAtOrPastDestination;
	}

	switch (CurrentNavFlag)
	{		
		case NAV_FLAG_WALK:
		case NAV_FLAG_CROUCH:
			return HasBotCompletedWalkMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case NAV_FLAG_LADDER:
			return HasBotCompletedLadderMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case NAV_FLAG_FALL:
			return HasBotCompletedFallMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case NAV_FLAG_JUMP:
			return HasBotCompletedJumpMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		case NAV_FLAG_PLATFORM:
			return HasBotCompletedLiftMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
		default:
			return HasBotCompletedWalkMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	}

	return HasBotCompletedWalkMove(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
}

bool HasBotCompletedWalkMove(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	bool bNextPointReachable = false;

	if (NextMoveFlag != NAV_FLAG_DISABLED)
	{
		bNextPointReachable = UTIL_PointIsDirectlyReachable(pBot->CollisionHullBottomLocation, NextMoveDestination);
	}

	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax) || (bNextPointReachable && vDist2DSq(pBot->Edict->v.origin, MoveEnd) < sqrf(GetPlayerRadius(pBot->Edict) * 2.0f));
}

bool HasBotCompletedObstacleMove(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

bool HasBotCompletedLadderMove(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	if (IsPlayerOnLadder(pBot->Edict)) { return false; }

	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

bool HasBotCompletedFallMove(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	if (NextMoveFlag != NAV_FLAG_DISABLED)
	{
		Vector ThisMoveDir = UTIL_GetVectorNormal2D(MoveEnd - MoveStart);
		Vector NextMoveDir = UTIL_GetVectorNormal2D(NextMoveDestination - MoveEnd);

		float MoveDot = UTIL_GetDotProduct2D(ThisMoveDir, NextMoveDir);

		if (MoveDot > 0.0f)
		{
			if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, NextMoveDestination)
				&& UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, NextMoveDestination)
				&& fabsf(pBot->CollisionHullBottomLocation.z - MoveEnd.z) < 100.0f) { return true; }
		}
	}

	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

bool HasBotCompletedJumpMove(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	Vector PositionInMove = vClosestPointOnLine2D(MoveStart, MoveEnd, pBot->Edict->v.origin);

	if (!vEquals2D(PositionInMove, MoveEnd, 2.0f)) { return false; }

	if (NextMoveFlag != NAV_FLAG_DISABLED)
	{
		Vector ThisMoveDir = UTIL_GetVectorNormal2D(MoveEnd - MoveStart);
		Vector NextMoveDir = UTIL_GetVectorNormal2D(NextMoveDestination - MoveEnd);

		float MoveDot = UTIL_GetDotProduct2D(ThisMoveDir, NextMoveDir);

		if (MoveDot >= 0.0f)
		{
			Vector HullTraceEnd = MoveEnd;
			HullTraceEnd.z = pBot->Edict->v.origin.z;

			if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, NextMoveDestination)
				&& UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, HullTraceEnd, head_hull, false)
				&& fabsf(pBot->CollisionHullBottomLocation.z - MoveEnd.z) < 100.0f)
			{
				return true;
			}
		}
	}

	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

bool HasBotCompletedLiftMove(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	return vPointOverlaps3D(MoveEnd, pBot->Edict->v.absmin, pBot->Edict->v.absmax);
}

void CheckAndHandleDoorObstruction(AIPlayer* pBot, bot_path_node CurrentPathNode)
{
	if (CurrentPathNode.flag == NAV_FLAG_DISABLED) { return; }

	DynamicMapObject* IgnoreObject = UTIL_GetDynamicObjectByEdict(CurrentPathNode.Platform);

	DynamicMapObject* BlockingNavObject = UTIL_GetObjectBlockingPathPoint(pBot->Edict->v.origin, CurrentPathNode.Location, CurrentPathNode.flag, nullptr, IgnoreObject);
	
	if (!BlockingNavObject)
	{
		int NumIterations = 0;

		for (int i = (pBot->BotNavInfo.CurrentPathPoint + 1); i < pBot->BotNavInfo.CurrentPath.size(); i++)
		{
			bot_path_node ThisPathNode = pBot->BotNavInfo.CurrentPath[i];

			IgnoreObject = UTIL_GetDynamicObjectByEdict(ThisPathNode.Platform);

			BlockingNavObject = UTIL_GetObjectBlockingPathPoint(ThisPathNode.FromLocation, ThisPathNode.Location, ThisPathNode.flag, nullptr, IgnoreObject);

			NumIterations++;

			if (BlockingNavObject || NumIterations >= 2)
			{
				break;
			}
		}
	}

	if (!BlockingNavObject) { return; }

	edict_t* BlockingObjectEdict = BlockingNavObject->Edict;

	Vector NearestPoint = UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, BlockingObjectEdict);


	// If the door is in the process of opening or closing, let it finish before doing anything else
	if (BlockingObjectEdict->v.velocity.Length() > 0.0f)
	{
		if (IsPlayerTouchingEntity(pBot->Edict, BlockingObjectEdict))
		{
			Vector MoveDir = UTIL_GetVectorNormal2D(CurrentPathNode.Location - CurrentPathNode.FromLocation);

			pBot->desiredMovementDir = MoveDir;
			return;
		}

		if (vDist2DSq(pBot->Edict->v.origin, NearestPoint) < sqrf(UTIL_MetresToGoldSrcUnits(1.5f)))
		{
			// Wait for the door to finish opening
			pBot->desiredMovementDir = ZERO_VECTOR;
			BotLookAt(pBot, CurrentPathNode.Location);
		}
		return;
	}

	DynamicMapObject* NearestTrigger = NAV_GetBestTriggerForObject(BlockingNavObject, pBot->Edict, pBot->BotNavInfo.NavProfile);

	if (NearestTrigger)
	{
		NAV_AddTriggerMovementTask(pBot, NearestTrigger, BlockingNavObject);
		return;
	}
	
}

DynamicMapObject* UTIL_GetObjectBlockingPathPoint(bot_path_node* PathNode, DynamicMapObject* SearchObject, DynamicMapObject* IgnoreObject)
{
	Vector FromLocation = PathNode->FromLocation;
	Vector ToLocation = PathNode->Location;
	ToLocation.z = PathNode->requiredZ;

	return UTIL_GetObjectBlockingPathPoint(FromLocation, ToLocation, PathNode->flag, SearchObject, IgnoreObject);
}

edict_t* UTIL_GetBreakableBlockingPathPoint(bot_path_node* PathNode, edict_t* SearchBreakable)
{
	Vector FromLocation = PathNode->FromLocation;
	Vector ToLocation = PathNode->Location;
	ToLocation.z = PathNode->requiredZ;

	return UTIL_GetBreakableBlockingPathPoint(FromLocation, ToLocation, PathNode->flag, SearchBreakable);
}

edict_t* UTIL_GetBreakableBlockingPathPoint(const Vector FromLocation, const Vector ToLocation, const unsigned int MovementFlag, edict_t* SearchBreakable)
{
	Vector FromLoc = FromLocation;
	Vector ToLoc = ToLocation;

	TraceResult doorHit;

	if (MovementFlag == NAV_FLAG_LADDER)
	{
		Vector TargetLoc = (ToLocation.z > FromLocation.z) ? Vector(FromLoc.x, FromLoc.y, ToLoc.z) : Vector(ToLoc.x, ToLoc.y, FromLoc.z);

		if (!FNullEnt(SearchBreakable))
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), SearchBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
			{
				return SearchBreakable;
			}
		}
		else
		{
			edict_t* currBreakable = NULL;

			// Marine Structures
			while (((currBreakable = UTIL_FindEntityByClassname(currBreakable, "func_breakable")) != NULL) && currBreakable)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, currBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), currBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
				{
					return currBreakable;
				}
			}
		}

		if (!FNullEnt(SearchBreakable))
		{
			if (vlineIntersectsAABB(TargetLoc, ToLoc, SearchBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), SearchBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
			{
				return SearchBreakable;
			}
		}
		else
		{
			edict_t* currBreakable = NULL;

			// Marine Structures
			while (((currBreakable = UTIL_FindEntityByClassname(currBreakable, "func_breakable")) != NULL) && currBreakable)
			{
				if (vlineIntersectsAABB(TargetLoc, ToLoc, currBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), currBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
				{
					return currBreakable;
				}
			}
		}

	}
	else if (MovementFlag == NAV_FLAG_FALL)
	{
		Vector TargetLoc = Vector(ToLoc.x, ToLoc.y, FromLoc.z);

		if (!FNullEnt(SearchBreakable))
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), SearchBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
			{
				return SearchBreakable;
			}
		}
		else
		{
			edict_t* currBreakable = NULL;

			// Marine Structures
			while (((currBreakable = UTIL_FindEntityByClassname(currBreakable, "func_breakable")) != NULL) && currBreakable)
			{
				if (vlineIntersectsAABB(FromLoc, TargetLoc, currBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), currBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
				{
					return currBreakable;
				}
			}
		}

		if (!FNullEnt(SearchBreakable))
		{
			if (vlineIntersectsAABB(TargetLoc, ToLoc, SearchBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), SearchBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
			{
				return SearchBreakable;
			}
		}
		else
		{
			edict_t* currBreakable = NULL;

			// Marine Structures
			while (((currBreakable = UTIL_FindEntityByClassname(currBreakable, "func_breakable")) != NULL) && currBreakable)
			{
				if (vlineIntersectsAABB(TargetLoc, ToLoc, currBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), currBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
				{
					return currBreakable;
				}
			}
		}

	}

	Vector TargetLoc = ToLoc + Vector(0.0f, 0.0f, 10.0f);

	if (!FNullEnt(SearchBreakable))
	{
		if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), SearchBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
		{
			return SearchBreakable;
		}
	}
	else
	{
		edict_t* currBreakable = NULL;

		// Marine Structures
		while (((currBreakable = UTIL_FindEntityByClassname(currBreakable, "func_breakable")) != NULL) && currBreakable)
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, currBreakable->v.absmin - Vector(10.0f, 10.0f, 10.0f), currBreakable->v.absmax + Vector(10.0f, 10.0f, 10.0f)))
			{
				return currBreakable;
			}
		}
	}

	return nullptr;
}

DynamicMapObject* UTIL_GetObjectBlockingPathPoint(const Vector FromLocation, const Vector ToLocation, const unsigned int MovementFlag, DynamicMapObject* SearchObject, DynamicMapObject* IgnoreObject)
{
	if (IsFlagTeleportType((NavMovementFlag)MovementFlag)) { return nullptr;}

	Vector FromLoc = FromLocation;
	Vector ToLoc = ToLocation;

	TraceResult doorHit;

	if (MovementFlag == NAV_FLAG_LADDER)
	{
		Vector TargetLoc = (ToLocation.z > FromLocation.z) ? Vector(FromLoc.x, FromLoc.y, ToLoc.z) : Vector(ToLoc.x, ToLoc.y, FromLoc.z);

		if (SearchObject != nullptr)
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchObject->Edict->v.absmin, SearchObject->Edict->v.absmax))
			{
				return SearchObject;
			}
		}
		else
		{
			for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
			{
				if (it->Type == MAPOBJECT_PLATFORM || (IgnoreObject && it->Edict == IgnoreObject->Edict)) { continue; }

				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->Edict->v.absmin, it->Edict->v.absmax))
				{
					return &(*it);
				}
			}
		}

		if (SearchObject != nullptr)
		{
			if (vlineIntersectsAABB(TargetLoc, ToLoc, SearchObject->Edict->v.absmin, SearchObject->Edict->v.absmax))
			{
				return SearchObject;
			}
		}
		else
		{
			for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
			{
				if (it->Type == MAPOBJECT_PLATFORM || (IgnoreObject && it->Edict == IgnoreObject->Edict)) { continue; }

				if (vlineIntersectsAABB(TargetLoc, ToLoc, it->Edict->v.absmin, it->Edict->v.absmax))
				{
					return &(*it);
				}
			}
		}

	}
	else if (MovementFlag == NAV_FLAG_FALL)
	{
		Vector TargetLoc = Vector(ToLoc.x, ToLoc.y, FromLoc.z);

		if (SearchObject != nullptr)
		{
			if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchObject->Edict->v.absmin, SearchObject->Edict->v.absmax))
			{
				return SearchObject;
			}
		}
		else
		{
			for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
			{
				if (it->Type == MAPOBJECT_PLATFORM || (IgnoreObject && it->Edict == IgnoreObject->Edict)) { continue; }

				if (vlineIntersectsAABB(FromLoc, TargetLoc, it->Edict->v.absmin, it->Edict->v.absmax))
				{
					return &(*it);
				}
			}
		}

		if (SearchObject != nullptr)
		{
			if (vlineIntersectsAABB(TargetLoc, ToLoc, SearchObject->Edict->v.absmin, SearchObject->Edict->v.absmax))
			{
				return SearchObject;
			}
		}
		else
		{
			for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
			{
				if (it->Type == MAPOBJECT_PLATFORM || (IgnoreObject && it->Edict == IgnoreObject->Edict)) { continue; }

				if (vlineIntersectsAABB(TargetLoc, ToLoc, it->Edict->v.absmin, it->Edict->v.absmax))
				{
					return &(*it);
				}
			}
		}

	}

	Vector TargetLoc = ToLoc + Vector(0.0f, 0.0f, 10.0f);

	if (SearchObject != nullptr)
	{
		if (vlineIntersectsAABB(FromLoc, TargetLoc, SearchObject->Edict->v.absmin, SearchObject->Edict->v.absmax))
		{
			return SearchObject;
		}
	}
	else
	{
		for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
		{
			if (it->Type == MAPOBJECT_PLATFORM || (IgnoreObject && it->Edict == IgnoreObject->Edict)) { continue; }

			if (vlineIntersectsAABB(FromLoc, TargetLoc, it->Edict->v.absmin, it->Edict->v.absmax))
			{
				return &(*it);
			}
		}
	}

	return nullptr;
}

bool UTIL_IsPathBlockedByObject(const NavAgentProfile& NavProfile, const Vector StartLoc, const Vector EndLoc, DynamicMapObject* SearchObject)
{
	if (NAV_IsPointInSwimArea(StartLoc) && NAV_IsPointInSwimArea(EndLoc))
	{
		if (UTIL_QuickHullTrace(nullptr, StartLoc, EndLoc))
		{
			return vlineIntersectsAABB(StartLoc, EndLoc, SearchObject->Edict->v.absmin, SearchObject->Edict->v.absmax);
		}
	}

	Vector ValidNavmeshPoint = UTIL_ProjectPointToNavmesh(NavProfile.NavMeshIndex, EndLoc, NavProfile);

	if (NAV_IsPointInSwimArea(EndLoc))
	{
		TraceResult Hit;
		UTIL_TraceLine(EndLoc, EndLoc - Vector(0.0f, 0.0f, 1000.0f), ignore_monsters, nullptr, &Hit);

		if (Hit.flFraction < 1.0f)
		{
			ValidNavmeshPoint = UTIL_ProjectPointToNavmesh(NavProfile.NavMeshIndex, Hit.vecEndPos, NavProfile);
		}
	}
	
	if (!ValidNavmeshPoint)
	{
		return false;
	}

	vector<bot_path_node> TestPath;
	TestPath.clear();

	// Now we find a path backwards from the valid nav mesh point to our location, trying to get as close as we can to it

	dtStatus PathFindingStatus = FindPathClosestToPoint(NavProfile, StartLoc, ValidNavmeshPoint, TestPath, 50.0f);

	if (dtStatusSucceed(PathFindingStatus))
	{
		for (auto it = TestPath.begin(); it != TestPath.end(); it++)
		{
			if (UTIL_GetObjectBlockingPathPoint(&(*it), SearchObject, nullptr) != nullptr)
			{
				return true;
			}
		}

		return false;
	}

	return true;
}

DynamicMapObject* UTIL_GetNearestObjectTrigger(const NavAgentProfile& NavProfile, const Vector Location, DynamicMapObject* Object, edict_t* IgnoreTrigger, bool bCheckBlockedByDoor)
{
	if (!Object) { return nullptr; }

	if (Object->Triggers.size() == 0) { return nullptr; }

	DynamicMapObject* NearestTrigger = nullptr;
	float NearestDist = 0.0f;

	Vector DoorLocation = UTIL_GetCentreOfEntity(Object->Edict);

	for (auto it = Object->Triggers.begin(); it != Object->Triggers.end(); it++)
	{
		DynamicMapObject* ThisTrigger = UTIL_GetDynamicObjectByEdict((*it));

		if (!FNullEnt(ThisTrigger->Edict) && ThisTrigger->Edict != IgnoreTrigger && ThisTrigger->bIsActive)
		{
			Vector ButtonLocation = UTIL_GetButtonFloorLocation(NavProfile, Location, ThisTrigger->Edict);

			if ((!bCheckBlockedByDoor || !UTIL_IsPathBlockedByObject(NavProfile, Location, ButtonLocation, Object)) && UTIL_PointIsReachable(NavProfile, Location, ButtonLocation, 64.0f))
			{
				float ThisDist = vDist3DSq(Location, ButtonLocation);

				if (!NearestTrigger || ThisDist < NearestDist)
				{
					NearestTrigger = ThisTrigger;
					NearestDist = ThisDist;
				}

			}
		}
	}

	return NearestTrigger;
}

void CheckAndHandleBreakableObstruction(AIPlayer* pBot, const Vector MoveFrom, const Vector MoveTo, unsigned int MovementFlags)
{
	if (pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size()) { return; }

	Vector MoveDir = UTIL_GetVectorNormal2D(MoveTo - pBot->Edict->v.origin);

	if (vIsZero(MoveDir))
	{
		MoveDir = UTIL_GetForwardVector2D(pBot->Edict->v.angles);
	}

	TraceResult breakableHit;

	edict_t* BlockingBreakableEdict = nullptr;

	UTIL_TraceLine(pBot->Edict->v.origin, pBot->Edict->v.origin + (MoveDir * 100.0f), dont_ignore_monsters, dont_ignore_glass, pBot->Edict->v.pContainingEntity, &breakableHit);

	if (!FNullEnt(breakableHit.pHit))
	{
		if (strcmp(STRING(breakableHit.pHit->v.classname), "func_breakable") == 0)
		{
			BlockingBreakableEdict = breakableHit.pHit;
		}
	}

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	if (FNullEnt(BlockingBreakableEdict))
	{
		BlockingBreakableEdict = UTIL_GetBreakableBlockingPathPoint(&CurrentPathNode, nullptr);
	}

	if (FNullEnt(BlockingBreakableEdict))
	{
		int NumIterations = 0;

		for (int i = (pBot->BotNavInfo.CurrentPathPoint + 1); i < pBot->BotNavInfo.CurrentPath.size(); i++)
		{
			bot_path_node ThisPathNode = pBot->BotNavInfo.CurrentPath[i];
			BlockingBreakableEdict = UTIL_GetBreakableBlockingPathPoint(&ThisPathNode, nullptr);

			NumIterations++;

			if (!FNullEnt(BlockingBreakableEdict) || NumIterations >= 2)
			{
				break;
			}
		}
	}

	if (FNullEnt(BlockingBreakableEdict)) { return; }

	Vector ClosestPoint = UTIL_GetClosestPointOnEntityToLocation(pBot->Edict->v.origin, BlockingBreakableEdict);

	AIWeaponType DesiredWeapon = WEAP_GetPlayerCurrentWeapon(pBot->Edict);

	float DesiredRange = WEAP_GetMaxIdealWeaponRange(DesiredWeapon);

	if (vDist2DSq(pBot->Edict->v.origin, ClosestPoint) < sqrf(16.0f))
	{
		if (pBot->Edict->v.oldbuttons & IN_DUCK)
		{
			pBot->Button |= IN_DUCK;
		}
		else
		{
			if (pBot->CurrentEyePosition.z - ClosestPoint.z > 32.0f)
			{
				pBot->Button |= IN_DUCK;
			}
		}
	}

	if (vDist3DSq(ClosestPoint, pBot->CurrentEyePosition) < sqrf(DesiredRange))
	{
		BotLookAt(pBot, BlockingBreakableEdict);

		pBot->DesiredMoveWeapon = DesiredWeapon;

		if (WEAP_GetPlayerCurrentWeapon(pBot->Edict) == DesiredWeapon)
		{
			pBot->Button |= IN_ATTACK;
		}
	}
	
}

void NewMove(AIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		return;
	}

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	Vector MoveFrom = CurrentPathNode.FromLocation;
	Vector MoveTo = CurrentPathNode.Location;

	NavArea CurrentNavArea = (NavArea)CurrentPathNode.area;
	NavMovementFlag CurrentNavFlags = (NavMovementFlag)CurrentPathNode.flag;

	// Used to anticipate if we're about to enter a crouch area so we can start crouching early
	unsigned char NextArea = NAV_AREA_WALK;

	if (pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size() - 1)
	{
		bot_path_node NextPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint + 1];

		NextArea = NextPathNode.area;

		bool bIsNearNextPoint = (vDist2DSq(pBot->Edict->v.origin, NextPathNode.FromLocation) <= sqrf(50.0f));

		// Start crouching early if we're about to enter a crouch path point
		if (CanPlayerCrouch(pBot->Edict) && (CurrentNavArea == NAV_AREA_CROUCH || (NextArea == NAV_AREA_CROUCH && bIsNearNextPoint)))
		{
			pBot->Button |= IN_DUCK;
		}
	}

	switch (CurrentNavFlags)
	{
	case NAV_FLAG_WALK:
	case NAV_FLAG_CROUCH:
		GroundMove(pBot, MoveFrom, MoveTo);
		break;
	case NAV_FLAG_FALL:
		FallMove(pBot, MoveFrom, MoveTo);
		break;
	case NAV_FLAG_JUMP:
		JumpMove(pBot, MoveFrom, MoveTo);
		break;
	case NAV_FLAG_LADDER:
		LadderMove(pBot, MoveFrom, MoveTo, CurrentPathNode.requiredZ, NextArea);
		break;
	case NAV_FLAG_PLATFORM:
		PlatformMove(pBot, MoveFrom, MoveTo, CurrentPathNode.Platform);
		break;
	default:
		GroundMove(pBot, MoveFrom, MoveTo);
		break;
	}

	if (vIsZero(pBot->LookTargetLocation) && vIsZero(pBot->MoveLookLocation))
	{
		Vector FurthestView = UTIL_GetFurthestVisiblePointOnPath(pBot);

		if (vIsZero(FurthestView) || vDist2DSq(FurthestView, pBot->CurrentEyePosition) < sqrf(200.0f))
		{
			FurthestView = MoveTo;

			Vector LookNormal = UTIL_GetVectorNormal2D(FurthestView - pBot->CurrentEyePosition);

			FurthestView = FurthestView + (LookNormal * 1000.0f);
		}

		BotLookAt(pBot, FurthestView);
	}

	// While moving, check to make sure we're not obstructed by a func_breakable, e.g. vent or window.
	CheckAndHandleBreakableObstruction(pBot, MoveFrom, MoveTo, CurrentNavFlags);

	HandlePlayerAvoidance(pBot, MoveTo);

}

void NewSwimMove(AIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		return;
	}

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	Vector MoveFrom = CurrentPathNode.FromLocation;
	Vector MoveTo = CurrentPathNode.Location;

	NavArea CurrentNavArea = (NavArea)CurrentPathNode.area;
	NavMovementFlag CurrentNavFlags = (NavMovementFlag)CurrentPathNode.flag;

	// Used to anticipate if we're about to enter a crouch area so we can start crouching early
	unsigned char NextArea = NAV_AREA_WALK;

	if (pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size() - 1)
	{
		bot_path_node NextPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint + 1];

		NextArea = NextPathNode.area;

		bool bIsNearNextPoint = (vDist2DSq(pBot->Edict->v.origin, NextPathNode.FromLocation) <= sqrf(50.0f));

		// Start crouching early if we're about to enter a crouch path point
		if (CanPlayerCrouch(pBot->Edict) && (CurrentNavArea == NAV_AREA_CROUCH || (NextArea == NAV_AREA_CROUCH && bIsNearNextPoint)))
		{
			pBot->Button |= IN_DUCK;
		}
	}

	bool TargetPointIsInWater = NAV_IsPointInSwimArea(MoveTo);

	bool bHasNextPoint = pBot->BotNavInfo.CurrentPathPoint < (pBot->BotNavInfo.CurrentPath.size() - 1);
	bool NextPointInWater = TargetPointIsInWater;

	bool bShouldSurface = (bHasNextPoint && !NextPointInWater && vDist2DSq(pBot->Edict->v.origin, CurrentPathNode.Location) < sqrf(100.0f));

	if (TargetPointIsInWater && !bShouldSurface)
	{
		BotMoveLookAt(pBot, CurrentPathNode.Location);
		pBot->desiredMovementDir = UTIL_GetForwardVector2D(pBot->Edict->v.angles);

		// While moving, check to make sure we're not obstructed by a func_breakable, e.g. vent or window.
		CheckAndHandleBreakableObstruction(pBot, MoveFrom, MoveTo, CurrentNavFlags);

		HandlePlayerAvoidance(pBot, MoveTo);

		return;
	}

	if (CurrentPathNode.flag == NAV_FLAG_LADDER)
	{
		if (IsPlayerOnLadder(pBot->Edict))
		{
			NewMove(pBot);
			return;
		}
		else
		{
			edict_t* MountLadder = UTIL_GetNearestLadderAtPoint(CurrentPathNode.FromLocation);

			if (!FNullEnt(MountLadder))
			{
				Vector LadderMountPoint = GetLadderMountPoint(MountLadder, CurrentPathNode.Location);
				Vector LadderNormal = UTIL_GetVectorNormal2D(LadderMountPoint - UTIL_GetCentreOfEntity(MountLadder));
				LadderMountPoint = LadderMountPoint + (LadderNormal * 48.0f);

				if (pBot->Edict->v.origin.z >= MountLadder->v.absmin.z + 16.0f)
				{
					Vector ClosestPointOnLadder = UTIL_GetClosestPointOnEntityToLocation(LadderMountPoint, MountLadder);
					Vector MountAngle = UTIL_GetVectorNormal2D(ClosestPointOnLadder - LadderMountPoint);
					Vector BotAngle = UTIL_GetVectorNormal2D(ClosestPointOnLadder - pBot->Edict->v.origin);

					float Dot = UTIL_GetDotProduct2D(MountAngle, BotAngle);

					if (Dot > 0.9f)
					{
						LadderMountPoint = ClosestPointOnLadder;
					}
				}

				BotDirectLookAt(pBot, LadderMountPoint);
				pBot->desiredMovementDir = UTIL_GetForwardVector2D(pBot->Edict->v.v_angle);

				return;
			}
		}
	}

	// Our target point is out of the water, so surface and try to get out of the water

	float WaterLevel = UTIL_WaterLevel(pBot->Edict->v.origin, pBot->Edict->v.origin.z, pBot->Edict->v.origin.z + 500.0f);

	float WaterDiff = WaterLevel - pBot->Edict->v.origin.z;

	// If we're below the waterline by a significant amount, then swim up to surface before we move on
	if (WaterDiff > 5.0f)
	{
		Vector MoveDir = UTIL_GetVectorNormal2D(CurrentPathNode.Location - pBot->Edict->v.origin);
		pBot->desiredMovementDir = MoveDir;

		if (WaterDiff > 10.0f)
		{
			BotMoveLookAt(pBot, pBot->Edict->v.origin + (MoveDir * 5.0f) + Vector(0.0f, 0.0f, 100.0f));
		}
		else
		{
			BotMoveLookAt(pBot, pBot->CurrentEyePosition + (MoveDir * 50.0f) + Vector(0.0f, 0.0f, 50.0f));
		}
	}
	else
	{
		// We're at the surface, now tackle the path the usual way
		if (pBot->BotNavInfo.NavProfile.bFlyingProfile)
		{
			NewFlightMove(pBot);
		}
		else
		{
			NewMove(pBot);
		}

		return;
	}

	// While moving, check to make sure we're not obstructed by a func_breakable, e.g. vent or window.
	CheckAndHandleBreakableObstruction(pBot, MoveFrom, MoveTo, CurrentNavFlags);

	HandlePlayerAvoidance(pBot, MoveTo);
}

void NewFlightMove(AIPlayer* pBot)
{

}

void GroundMove(AIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	edict_t* pEdict = pBot->Edict;

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	Vector CurrentPos = (pBot->BotNavInfo.IsOnGround) ? pBot->Edict->v.origin : pBot->CurrentFloorPosition;

	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - CurrentPos);
	// Same goes for the right vector, might not be the same as the bot's right
	Vector vRight = UTIL_GetVectorNormal(UTIL_GetCrossProduct(vForward, UP_VECTOR));

	bool bAdjustingForCollision = false;

	float PlayerRadius = GetPlayerRadius(pBot->Edict) + 2.0f;

	Vector stTrcLft = CurrentPos - (vRight * PlayerRadius);
	Vector stTrcRt = CurrentPos + (vRight * PlayerRadius);
	Vector endTrcLft = stTrcLft + (vForward * 24.0f);
	Vector endTrcRt = stTrcRt + (vForward * 24.0f);

	bool bumpLeft = !UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, stTrcLft, endTrcLft);
	bool bumpRight = !UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, stTrcRt, endTrcRt);

	pBot->desiredMovementDir = vForward;

	if (bumpRight && !bumpLeft)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
	}
	else if (bumpLeft && !bumpRight)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
	}
	else if (bumpLeft && bumpRight)
	{
		stTrcLft.z = pBot->Edict->v.origin.z;
		stTrcRt.z = pBot->Edict->v.origin.z;
		endTrcLft.z = pBot->Edict->v.origin.z;
		endTrcRt.z = pBot->Edict->v.origin.z;

		if (!UTIL_QuickTrace(pBot->Edict, stTrcLft, endTrcLft))
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
		}
		else
		{
			pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
		}
	}
	else
	{
		float DistFromLine = vDistanceFromLine2D(StartPoint, EndPoint, CurrentPos);

		if (DistFromLine > 18.0f)
		{
			float modifier = (float)vPointOnLine(StartPoint, EndPoint, CurrentPos);
			pBot->desiredMovementDir = pBot->desiredMovementDir + (vRight * modifier);
		}
	}

	pBot->desiredMovementDir = UTIL_GetVectorNormal2D(pBot->desiredMovementDir);

	if (CanPlayerCrouch(pEdict))
	{
		Vector HeadLocation = GetPlayerTopOfCollisionHull(pEdict, false);

		// Crouch if we have something in our way at head height
		if (!UTIL_QuickTrace(pBot->Edict, HeadLocation, (HeadLocation + (pBot->desiredMovementDir * 50.0f))))
		{
			pBot->Button |= IN_DUCK;
		}
	}
}

void FallMove(AIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	Vector vBotOrientation = UTIL_GetVectorNormal2D(EndPoint - pBot->Edict->v.origin);
	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);

	if (pBot->BotNavInfo.IsOnGround)
	{
		if (vDist2DSq(pBot->Edict->v.origin, EndPoint) > sqrf(GetPlayerRadius(pBot->Edict)))
		{
			pBot->desiredMovementDir = vBotOrientation;
		}
		else
		{
			pBot->desiredMovementDir = vForward;
		}

		if (!CanPlayerCrouch(pBot->Edict)) { return; }

		Vector HeadLocation = GetPlayerTopOfCollisionHull(pBot->Edict, false);

		if (!UTIL_QuickTrace(pBot->Edict, HeadLocation, (HeadLocation + (pBot->desiredMovementDir * 50.0f))))
		{
			pBot->Button |= IN_DUCK;
		}
	}
	else
	{
		pBot->desiredMovementDir = vBotOrientation;
	}
}

void BlockedMove(AIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);

	if (vIsZero(vForward))
	{
		vForward = UTIL_GetForwardVector2D(pBot->Edict->v.angles);
	}

	pBot->desiredMovementDir = vForward;

	Vector CurrVelocity = UTIL_GetVectorNormal2D(pBot->Edict->v.velocity);

	float Dot = UTIL_GetDotProduct2D(vForward, CurrVelocity);

	Vector FaceDir = UTIL_GetForwardVector2D(pBot->Edict->v.angles);

	float FaceDot = UTIL_GetDotProduct2D(FaceDir, vForward);

	// Yes this is cheating, but is it not cheating for humans to have millions of years of evolution
	// driving their ability to judge a jump, while the bots have a single year of coding from a moron?
	if (FaceDot < 0.95f)
	{
		float MoveSpeed = vSize2D(pBot->Edict->v.velocity);
		Vector NewVelocity = vForward * MoveSpeed;
		NewVelocity.z = pBot->Edict->v.velocity.z;

		pBot->Edict->v.velocity = NewVelocity;
	}

	BotJump(pBot);
}

void JumpMove(AIPlayer* pBot, const Vector StartPoint, const Vector EndPoint)
{
	Vector vForward = UTIL_GetVectorNormal2D(EndPoint - pBot->Edict->v.origin);

	if (vIsZero(vForward))
	{
		vForward = UTIL_GetVectorNormal2D(EndPoint - StartPoint);
	}

	pBot->desiredMovementDir = vForward;

	Vector CurrVelocity = UTIL_GetVectorNormal2D(pBot->Edict->v.velocity);

	float Dot = UTIL_GetDotProduct2D(vForward, CurrVelocity);

	// Yes this is cheating, but I'm up against millions of years of human evolution here...
	if (pBot->BotNavInfo.IsOnGround && Dot < 0.95f)
	{
		float MoveSpeed = vSize2D(pBot->Edict->v.velocity);
		Vector NewVelocity = vForward * fmaxf(MoveSpeed, GetDesiredBotMovementSpeed(pBot));
		NewVelocity.z = pBot->Edict->v.velocity.z;

		pBot->Edict->v.velocity = NewVelocity;
	}

	BotJump(pBot);

	if (!CanPlayerCrouch(pBot->Edict)) { return; }

	Vector HeadLocation = GetPlayerTopOfCollisionHull(pBot->Edict, false);

	if (!UTIL_QuickTrace(pBot->Edict, HeadLocation, (HeadLocation + (pBot->desiredMovementDir * 50.0f))))
	{
		pBot->Button |= IN_DUCK;
	}
}

Vector GetLadderMountPoint(const edict_t* MountLadder, const Vector StartPoint)
{
	Vector LadderCentre = UTIL_GetCentreOfEntity(MountLadder);

	Vector LadderTop = Vector(LadderCentre.x, LadderCentre.y, MountLadder->v.absmax.z);
	Vector LadderBottom = Vector(LadderCentre.x, LadderCentre.y, MountLadder->v.absmin.z);

	Vector MountPoint = LadderCentre;
	MountPoint.z = clampf(MountPoint.z, LadderBottom.z + 10.0f, LadderTop.z - 10.0f);

	bool bUseXAxis = (MountLadder->v.size.x < MountLadder->v.size.y);

	if (bUseXAxis)
	{
		Vector FirstSamplePoint = LadderCentre;
		FirstSamplePoint.z = clampf(StartPoint.z, LadderBottom.z + 5.0f, LadderTop.z - 5.0f);
		FirstSamplePoint.x = (StartPoint.x > LadderCentre.x) ? MountLadder->v.absmax.x + 17.0f : MountLadder->v.absmin.x - 17.0f;

		Vector SecondSamplePoint = LadderCentre;
		SecondSamplePoint.x = (StartPoint.x > LadderCentre.x) ? MountLadder->v.absmin.x - 17.0f : MountLadder->v.absmax.x + 17.0f;

		float Modifier = (StartPoint.x > LadderCentre.x) ? 1.0f : -1.0f;

		if (UTIL_PointContents(FirstSamplePoint) != CONTENTS_SOLID)
		{
			MountPoint = FirstSamplePoint;
		}
		else if (UTIL_PointContents(SecondSamplePoint) != CONTENTS_SOLID)
		{
			MountPoint = SecondSamplePoint;
		}
	}
	else
	{
		Vector FirstSamplePoint = LadderCentre;
		FirstSamplePoint.z = clampf(StartPoint.z, LadderBottom.z + 5.0f, LadderTop.z - 5.0f);
		FirstSamplePoint.y = (StartPoint.y > LadderCentre.y) ? MountLadder->v.absmax.y + 17.0f : MountLadder->v.absmin.y - 17.0f;

		Vector SecondSamplePoint = LadderCentre;
		SecondSamplePoint.y = (StartPoint.y > LadderCentre.y) ? MountLadder->v.absmin.y - 17.0f : MountLadder->v.absmax.y + 17.0f;

		if (UTIL_PointContents(FirstSamplePoint) != CONTENTS_SOLID)
		{
			MountPoint = FirstSamplePoint;
		}
		else if (UTIL_PointContents(SecondSamplePoint) != CONTENTS_SOLID)
		{
			MountPoint = SecondSamplePoint;
		}
	}

	return MountPoint;
}

void MountLadderMove(AIPlayer* pBot, const Vector StartPoint, const Vector EndPoint, float RequiredClimbHeight, unsigned char NextArea)
{
	edict_t* MountLadder = UTIL_GetNearestLadderAtPoint(StartPoint);

	if (FNullEnt(MountLadder))
	{
		MoveToWithoutNav(pBot, EndPoint);
		return;
	}

	Vector LadderCentre = UTIL_GetCentreOfEntity(MountLadder);

	Vector MountPoint = GetLadderMountPoint(MountLadder, EndPoint);

	bool bMountingFromTop = pBot->Edict->v.origin.z > MountLadder->v.absmax.z;

	if (!vEquals(MountPoint, LadderCentre) && !bMountingFromTop)
	{
		Vector AnglePlayerToLadder = UTIL_GetVectorNormal2D(LadderCentre - pBot->Edict->v.origin);
		Vector AngleMountPointToLadder = UTIL_GetVectorNormal2D(LadderCentre - MountPoint);

		if (UTIL_GetDotProduct2D(AnglePlayerToLadder, AngleMountPointToLadder) > 0.9f)
		{
			MountPoint = LadderCentre;
		}
	}

	if (bMountingFromTop)
	{
		pBot->BotNavInfo.bShouldWalk = true;
	}

	pBot->desiredMovementDir = UTIL_GetVectorNormal2D(MountPoint - pBot->Edict->v.origin);
}

void LadderMove(AIPlayer* pBot, const Vector StartPoint, const Vector EndPoint, float RequiredClimbHeight, unsigned char NextArea)
{
	edict_t* pEdict = pBot->Edict;

	bool bIsGoingUpLadder = (StartPoint.z < EndPoint.z);
	bool bAtAppropriateClimbHeight = (bIsGoingUpLadder) ? (pBot->Edict->v.origin.z >= RequiredClimbHeight) : (pBot->Edict->v.origin.z <= RequiredClimbHeight);

	if (!IsPlayerOnLadder(pBot->Edict))
	{
		if (!pBot->BotNavInfo.IsOnGround || UTIL_PointIsDirectlyReachable(pBot->CollisionHullBottomLocation, EndPoint))
		{
			MoveToWithoutNav(pBot, EndPoint);
			return;
		}
		else
		{
			MountLadderMove(pBot, StartPoint, EndPoint, RequiredClimbHeight, NextArea);
			return;
		}
	}

	edict_t* CurrentLadder = UTIL_GetNearestLadderAtPoint(pBot->Edict->v.origin);
	Vector LadderTop = UTIL_GetCentreOfEntity(CurrentLadder);
	LadderTop.z = CurrentLadder->v.absmax.z;

	// We're on the ladder and actively climbing

	Vector LadderNormalCheck = pBot->CollisionHullBottomLocation + Vector(0.0f, 0.0f, 18.0f);
	LadderNormalCheck = LadderNormalCheck + UTIL_GetVectorNormal2D(LadderNormalCheck - LadderTop);

	Vector CurrentLadderNormal = UTIL_GetNearestLadderNormal(LadderNormalCheck);

	CurrentLadderNormal = UTIL_GetVectorNormal2D(CurrentLadderNormal);

	if (vIsZero(CurrentLadderNormal))
	{

		if (EndPoint.z > StartPoint.z)
		{
			CurrentLadderNormal = UTIL_GetVectorNormal2D(StartPoint - EndPoint);
		}
		else
		{
			CurrentLadderNormal = UTIL_GetVectorNormal2D(EndPoint - StartPoint);
		}
	}

	const Vector LadderRightNormal = UTIL_GetVectorNormal(UTIL_GetCrossProduct(CurrentLadderNormal, UP_VECTOR));

	Vector ClimbRightNormal = (bIsGoingUpLadder) ? -LadderRightNormal : LadderRightNormal;

	Vector ClimbDir = (bIsGoingUpLadder) ? -CurrentLadderNormal : CurrentLadderNormal;

	Vector DisembarkDir = UTIL_GetVectorNormal2D(EndPoint - pBot->Edict->v.origin);
	float DisembarkDot = UTIL_GetDotProduct2D(CurrentLadderNormal, DisembarkDir);

	float DesiredClimbHeight = RequiredClimbHeight;

	if (DisembarkDot > 0.75f)
	{
		float JumpDist = vDist2DSq(pBot->Edict->v.origin, EndPoint);

		float ExtraClimbHeight = (JumpDist > sqrf(2.0f)) ? 100.0f : 50.0f;

		DesiredClimbHeight = fminf((RequiredClimbHeight + ExtraClimbHeight), LadderTop.z + GetPlayerOriginOffsetFromFloor(pBot->Edict, true).z);
	}

	// First check if we should dismount the ladder

	bIsGoingUpLadder = pBot->Edict->v.origin.z < DesiredClimbHeight;

	if (bIsGoingUpLadder)
	{
		// We've reached the end of the ladder, try and make it to the disembark point if we can
		if (LadderTop.z - pBot->CollisionHullBottomLocation.z <= 2.0f || pBot->Edict->v.origin.z >= DesiredClimbHeight)
		{
			MoveToWithoutNav(pBot, EndPoint);
			BotMoveLookAt(pBot, (pBot->CurrentEyePosition + (DisembarkDir * 50.0f)) + Vector(0.0f, 0.0f, 50.0f));

			if (DisembarkDot > 0.75f)
			{
				BotJump(pBot, true);
			}

			return;
		}
	}
	else
	{
		bool bDesiredGoingDownLadder = StartPoint.z > EndPoint.z;

		if (bDesiredGoingDownLadder && (pBot->Edict->v.origin.z <= DesiredClimbHeight || (pBot->CollisionHullBottomLocation.z - EndPoint.z < 100.0f)))
		{
			// We're close enough to the end that we can jump off the ladder
			if (UTIL_QuickTrace(pBot->Edict, pBot->CollisionHullTopLocation, EndPoint))
			{
				MoveToWithoutNav(pBot, EndPoint);
				BotJump(pBot);
				return;
			}
		}
	}

	// Still climbing

	Vector TraceStartPosition = (bIsGoingUpLadder) ? pBot->CollisionHullTopLocation : pBot->CollisionHullBottomLocation;

	Vector StartLeftTrace = TraceStartPosition - (ClimbRightNormal * GetPlayerRadius(pBot->Edict));
	Vector StartRightTrace = TraceStartPosition + (ClimbRightNormal * GetPlayerRadius(pBot->Edict));

	Vector EndLeftTrace = (bIsGoingUpLadder) ? StartLeftTrace + Vector(0.0f, 0.0f, 2.0f) : StartLeftTrace - Vector(0.0f, 0.0f, 2.0f);
	Vector EndRightTrace = (bIsGoingUpLadder) ? StartRightTrace + Vector(0.0f, 0.0f, 2.0f) : StartRightTrace - Vector(0.0f, 0.0f, 2.0f);

	bool bBlockedLeft = !UTIL_QuickTrace(pEdict, StartLeftTrace, EndLeftTrace);
	bool bBlockedRight = !UTIL_QuickTrace(pEdict, StartRightTrace, EndRightTrace);

	// Look up at the top of the ladder

	// If we are blocked going up the ladder, face the ladder and slide left/right to avoid blockage
	if (bBlockedLeft && !bBlockedRight)
	{
		Vector LookLocation = pBot->Edict->v.origin - (CurrentLadderNormal * 50.0f);
		LookLocation.z = RequiredClimbHeight + 100.0f;
		BotMoveLookAt(pBot, LookLocation);

		pBot->desiredMovementDir = ClimbRightNormal;
		return;
	}

	if (bBlockedRight && !bBlockedLeft)
	{
		Vector LookLocation = pBot->Edict->v.origin - (CurrentLadderNormal * 50.0f);
		LookLocation.z = RequiredClimbHeight + 100.0f;
		BotMoveLookAt(pBot, LookLocation);

		pBot->desiredMovementDir = -ClimbRightNormal;
		return;
	}

	if (CanPlayerCrouch(pBot->Edict))
	{
		Vector HeadTraceLocation = GetPlayerTopOfCollisionHull(pEdict, false);

		bool bHittingHead = !UTIL_QuickTrace(pBot->Edict, HeadTraceLocation, HeadTraceLocation + Vector(0.0f, 0.0f, 2.0f));

		if (bHittingHead)
		{
			pBot->Button |= IN_DUCK;
		}
	}

	pBot->desiredMovementDir = ClimbDir;

	Vector LookTarget = EndPoint;

	if (bIsGoingUpLadder)
	{
		LookTarget = LadderTop + (ClimbDir * 50.0f);
		LookTarget.z = DesiredClimbHeight + 50.0f;
	}

	BotMoveLookAt(pBot, LookTarget);
}

bool UTIL_TriggerHasBeenRecentlyActivated(edict_t* TriggerEntity)
{
	return true;
}

void NAV_ForceActivateTrigger(AIPlayer* pBot, DynamicMapObject* TriggerRef)
{
	if (!TriggerRef || FNullEnt(TriggerRef->Edict)) { return; }

	switch (TriggerRef->Type)
	{
	case TRIGGER_TOUCH:
		MDLL_Touch(TriggerRef->Edict, pBot->Edict);
		break;
	case TRIGGER_USE:
		MDLL_Use(TriggerRef->Edict, pBot->Edict);
		break;
	default:
		break;
	}
}

NavOffMeshConnection* NAV_GetNearestOffMeshConnectionToPoint(const NavAgentProfile& Profile, const Vector SearchPoint, NavMovementFlag SearchFlags)
{
	NavOffMeshConnection* Result = nullptr;

	float MinDist = FLT_MAX;

	nav_mesh ChosenNavMesh = NavMeshes[Profile.NavMeshIndex];

	for (auto it = ChosenNavMesh.MeshConnections.begin(); it != ChosenNavMesh.MeshConnections.end(); it++)
	{
		if (!(SearchFlags & it->ConnectionFlags)) { continue; }

		float ThisDist = fminf(vDist3DSq(SearchPoint, it->FromLocation), vDist3DSq(SearchPoint, it->ToLocation));

		if (ThisDist < MinDist)
		{
			Result = &(*it);
		}
	}

	return Result;
}

void GetDesiredPlatformStartAndEnd(DynamicMapObject* PlatformRef, Vector EmbarkPoint, Vector DisembarkPoint, DynamicMapObjectStop& StartLocation, DynamicMapObjectStop& EndLocation)
{
	float minStartDist = FLT_MAX;
	float minEndDist = FLT_MAX;

	// Find the desired stop point for us to get onto the lift
	for (auto it = PlatformRef->StopPoints.begin(); it != PlatformRef->StopPoints.end(); it++)
	{
		Vector CheckLocation = it->StopLocation;
		CheckLocation.z += PlatformRef->Edict->v.size.z * 0.25f;

		Vector NearestPointStart = UTIL_GetClosestPointOnEntityToLocation(EmbarkPoint, PlatformRef->Edict, it->StopLocation);
		Vector NearestPointEnd = UTIL_GetClosestPointOnEntityToLocation(DisembarkPoint, PlatformRef->Edict, it->StopLocation);

		NearestPointStart.z = it->StopLocation.z + (PlatformRef->Edict->v.size.z * 0.5f);
		NearestPointEnd.z = it->StopLocation.z + (PlatformRef->Edict->v.size.z * 0.5f);

		float thisStartTouchingDist = vDist2DSq(EmbarkPoint, NearestPointStart);
		float thisEndTouchingDist = vDist2DSq(DisembarkPoint, NearestPointEnd);

		float thisStartDist = vDist3DSq(EmbarkPoint, NearestPointStart);
		float thisEndDist = vDist3DSq(DisembarkPoint, NearestPointEnd);

		if (thisStartTouchingDist <= sqrf(100.0f) && thisStartDist < minStartDist)
		{
			StartLocation = (*it);
			minStartDist = thisStartDist;
		}

		if (thisEndTouchingDist <= sqrf(100.0f) && thisEndDist < minEndDist)
		{
			EndLocation = (*it);
			minEndDist = thisEndDist;
		}
	}

	if (vIsZero(StartLocation.StopLocation))
	{
		float MinDist = FLT_MAX;

		for (int i = 0; i < PlatformRef->StopPoints.size(); i++)
		{
			int NextIndex = (i < PlatformRef->StopPoints.size() - 1) ? (i + 1) : 0;

			DynamicMapObjectStop ThisStop = PlatformRef->StopPoints[i];
			DynamicMapObjectStop NextStop = PlatformRef->StopPoints[NextIndex];

			Vector ThisLoc = vClosestPointOnLine(ThisStop.StopLocation, NextStop.StopLocation, EmbarkPoint);

			float ThisDist = vDist3DSq(EmbarkPoint, ThisLoc);

			if (ThisDist < MinDist)
			{
				StartLocation.StopLocation = ThisLoc;
				StartLocation.CornerEdict = NextStop.CornerEdict;
				StartLocation.WaitTime = 0.0f;
				StartLocation.bWaitForRetrigger = false;

				MinDist = ThisDist;
			}
		}
	}

	if (vIsZero(EndLocation.StopLocation))
	{
		float MinDist = FLT_MAX;

		for (int i = 0; i < PlatformRef->StopPoints.size(); i++)
		{
			int NextIndex = (i < PlatformRef->StopPoints.size() - 1) ? (i + 1) : 0;

			DynamicMapObjectStop ThisStop = PlatformRef->StopPoints[i];
			DynamicMapObjectStop NextStop = PlatformRef->StopPoints[NextIndex];

			Vector ThisLoc = vClosestPointOnLine(ThisStop.StopLocation, NextStop.StopLocation, DisembarkPoint);

			float ThisDist = vDist3DSq(DisembarkPoint, ThisLoc);

			if (ThisDist < MinDist)
			{
				EndLocation.StopLocation = ThisLoc;
				EndLocation.CornerEdict = NextStop.CornerEdict;
				EndLocation.WaitTime = 0.0f;
				EndLocation.bWaitForRetrigger = false;
				MinDist = ThisDist;
			}
		}
	}
}

void RidePlatformMove(AIPlayer* pBot, DynamicMapObject* Lift, const DynamicMapObjectStop DesiredLiftStop, const Vector DesiredDisembarkPoint)
{
	if (!Lift) { return; }

	Vector LiftPosition = UTIL_GetCentreOfEntity(Lift->Edict);

	if (Lift->State == OBJECTSTATE_IDLE)
	{
		// If the lift is idle and we are on it, we are going to double check we're not at the right stop already
		const Vector Offset = pBot->Edict->v.origin - LiftPosition;

		DynamicMapObjectStop NearestDesiredStop;
		float MinDist = FLT_MAX;

		// Find the desired stop point for us to get onto the lift
		for (auto it = Lift->StopPoints.begin(); it != Lift->StopPoints.end(); it++)
		{
			const Vector CheckPoint = it->StopLocation + Offset;
			const float ThisDist = vDist3DSq(CheckPoint, DesiredDisembarkPoint);

			if (ThisDist < MinDist)
			{
				NearestDesiredStop = *it;
				MinDist = ThisDist;
			}
		}

		if (vEquals(LiftPosition, NearestDesiredStop.StopLocation, 5.0f))
		{
			Vector DisembarkPoint = NAV_GetNearestPlatformDisembarkPoint(pBot->BotNavInfo.NavProfile, pBot->Edict, Lift);

			MoveToWithoutNav(pBot, DisembarkPoint);
			return;
		}
	}

	bool bLiftHasReachedDestination = false;

	// If the lift is going to wait at the destination, then let it fully stop before disembarking
	if (DesiredLiftStop.bWaitForRetrigger || DesiredLiftStop.WaitTime > 1.0f)
	{
		bLiftHasReachedDestination = vEquals(LiftPosition, DesiredLiftStop.StopLocation, 5.0f);
	}
	// Otherwise, get off as soon as it's close enough
	else
	{
		bLiftHasReachedDestination = (vDist3DSq(LiftPosition, DesiredLiftStop.StopLocation) < sqrf(32.0f));
	}

	if (bLiftHasReachedDestination)
	{
		Vector DisembarkPoint = NAV_GetNearestPlatformDisembarkPoint(pBot->BotNavInfo.NavProfile, pBot->Edict, Lift);

		MoveToWithoutNav(pBot, DisembarkPoint);
		return;
	}

	// We're on a moving platform (or it is about to move, ensure we're properly on it and not likely to fall off
	if (Lift->State == OBJECTSTATE_MOVING || Lift->State == OBJECTSTATE_PREPARING)
	{
		float DistToCentre = vDist2DSq(pBot->Edict->v.origin, LiftPosition);

		// Check we're not already in the middle, we might be on a really small moving platform
		if (DistToCentre > sqrf(8.0f))
		{
			Vector NearestEdgePoint = vClampPointToAABBEdge2D(pBot->Edict->v.origin, Lift->Edict->v.absmin, Lift->Edict->v.absmax);

			float DistToEdge = vDist2DSq(NearestEdgePoint, pBot->Edict->v.origin);

			// We're a little too close to the edge of the lift, we might fall off. Move to the centre a bit
			if (DistToEdge < sqrf(GetPlayerRadius(pBot->Edict)))
			{
				MoveToWithoutNav(pBot, LiftPosition);
			}
		}

		return;
	}
	// We're on the lift and it has stopped somewhere we didn't want to
	else
	{		
		DynamicMapObject* Trigger = NAV_GetTriggerReachableFromPlatform(pBot->Edict->v.origin.z, Lift);

		if (Trigger)
		{
			if (IsPlayerInUseRange(pBot->Edict, Trigger->Edict))
			{
				BotUseObject(pBot, Trigger->Edict, false);
				return;
			}
			else
			{
				pBot->desiredMovementDir = UTIL_GetVectorNormal2D(UTIL_GetCentreOfEntity(Trigger->Edict) - pBot->Edict->v.origin);
			}

			return;
		}
		else
		{
			Vector DisembarkPoint = NAV_GetNearestPlatformDisembarkPoint(pBot->BotNavInfo.NavProfile, pBot->Edict, Lift);
			if (!vIsZero(DisembarkPoint))
			{
				MoveToWithoutNav(pBot, DisembarkPoint);
				return;
			}
			// We couldn't find a trigger within reach and have nowhere to get off. Use our telepathic powers to re-activate it so we're not trapped forever
			else
			{
				for (auto it = Lift->Triggers.begin(); it != Lift->Triggers.end(); it++)
				{
					DynamicMapObject* ThisTrigger = UTIL_GetDynamicObjectByEdict(*it);

					if (!ThisTrigger) { continue; }

					if (ThisTrigger->bIsActive && ThisTrigger->State == OBJECTSTATE_IDLE)
					{
						NAV_ForceActivateTrigger(pBot, ThisTrigger);
						return;
					}
				}
			}
		}
	}
}

void BoardPlatformMove(AIPlayer* pBot, const Vector EmbarkPoint, DynamicMapObject* Platform)
{
	if (!Platform) { return; }

	Vector ClosestPointOnLift = UTIL_GetClosestPointOnEntityToLocation(EmbarkPoint, Platform->Edict);

	Vector ClosestPointOnLine = vClosestPointOnLine2D(EmbarkPoint, ClosestPointOnLift, pBot->Edict->v.origin);

	if (vEquals2D(ClosestPointOnLine, EmbarkPoint) && vDist2DSq(pBot->Edict->v.origin, EmbarkPoint) > sqrf(32.0f))
	{
		NAV_AddMoveMovementTask(pBot, EmbarkPoint, nullptr);
	}
	else
	{
		MoveToWithoutNav(pBot, UTIL_GetCentreOfEntity(Platform->Edict));
	}
}

bool NAV_CanBoardPlatform(AIPlayer* pBot, DynamicMapObject* Platform, Vector BoardingPoint, Vector DesiredStop)
{
	Vector IdealClosestPoint = UTIL_GetClosestPointOnEntityToLocation(BoardingPoint, Platform->Edict, DesiredStop);

	float Dist = vDist2D(IdealClosestPoint, BoardingPoint);

	Vector ClosestCurrentPoint = UTIL_GetClosestPointOnEntityToLocation(BoardingPoint, Platform->Edict);
	ClosestCurrentPoint.z = BoardingPoint.z;

	NavAgentProfile CheckProfile = (pBot) ? pBot->BotNavInfo.NavProfile : GetBaseAgentProfile(NAV_PROFILE_DEFAULT);

	Vector ProjectedLocation = UTIL_ProjectPointToNavmesh(CheckProfile.NavMeshIndex, ClosestCurrentPoint, CheckProfile, Vector(Dist, Dist, 50.0f));

	return (!vIsZero(ProjectedLocation) && UTIL_PointIsDirectlyReachable(BoardingPoint, ProjectedLocation) && vDist2DSq(BoardingPoint, ProjectedLocation) <= sqrf(Dist + 16.0f));
}

void PlatformMove(AIPlayer* pBot, const Vector StartPoint, const Vector EndPoint, edict_t* PlatformEdict)
{
	DynamicMapObject* Platform = UTIL_GetDynamicObjectByEdict(PlatformEdict);
	
	if (!FNullEnt(pBot->Edict->v.groundentity))
	{
		Platform = UTIL_GetDynamicObjectByEdict(pBot->Edict->v.groundentity);
	}
	
	if (!Platform)
	{
		Platform = UTIL_GetClosestPlatformToPoints(StartPoint, EndPoint);
	}

	if (!Platform)
	{
		GroundMove(pBot, StartPoint, EndPoint);
		return;
	}

	Vector PlatformPosition = UTIL_GetCentreOfEntity(Platform->Edict);

	pBot->desiredMovementDir = ZERO_VECTOR;

	DynamicMapObjectStop DesiredStartStop, DesiredEndStop;

	GetDesiredPlatformStartAndEnd(Platform, StartPoint, EndPoint, DesiredStartStop, DesiredEndStop);

	// We are on the lift
	if (pBot->Edict->v.groundentity == Platform->Edict)
	{
		RidePlatformMove(pBot, Platform, DesiredEndStop, EndPoint);
		return;
	}

	// We have disembarked and just need to head to the end point
	if (UTIL_PointIsDirectlyReachable(pBot->BotNavInfo.NavProfile, pBot->CollisionHullBottomLocation, EndPoint) || vDist3DSq(pBot->CollisionHullBottomLocation, EndPoint) < sqrf(100.0f))
	{
		MoveToWithoutNav(pBot, EndPoint);
		return;
	}

	int currIndex = Platform->NextStopIndex;
	int prevIndex = (currIndex == 0) ? Platform->StopPoints.size() - 1 : currIndex - 1;

	int desiredStopIndex = 0;

	DynamicMapObjectStop NextStop = Platform->StopPoints[Platform->NextStopIndex];
	DynamicMapObjectStop PreviousStop = Platform->StopPoints[prevIndex];

	if (NAV_PlatformNeedsActivating(pBot, Platform, StartPoint, EndPoint))
	{
		if (vEquals(PlatformPosition, DesiredStartStop.StopLocation, 5.0f) || (vEquals(NextStop.StopLocation, DesiredStartStop.StopLocation, 5.0f) && Platform->State != OBJECTSTATE_IDLE) )
		{
			DynamicMapObject* PlatTrigger = NAV_GetTriggerReachableFromPlatform(StartPoint.z + 32.0f, Platform, DesiredStartStop.StopLocation);

			if (PlatTrigger)
			{
				if (Platform->State == OBJECTSTATE_IDLE)
				{
					BoardPlatformMove(pBot, StartPoint, Platform);
					return;
				}
			}
		}

		DynamicMapObject* BestTrigger = nullptr;
		bool bForceTrigger = true;

		BestTrigger = NAV_GetBestTriggerForObject(Platform, pBot->Edict, pBot->BotNavInfo.NavProfile);

		if (BestTrigger)
		{
			bForceTrigger = false;
		}

		if (bForceTrigger)
		{
			if (Platform->State == OBJECTSTATE_IDLE)
			{
				for (auto it = Platform->Triggers.begin(); it != Platform->Triggers.end(); it++)
				{
					DynamicMapObject* ThisTrigger = NAV_GetTriggerByEdict((*it));

					if (ThisTrigger && ThisTrigger->bIsActive && ThisTrigger->State == OBJECTSTATE_IDLE)
					{
						NAV_ForceActivateTrigger(pBot, ThisTrigger);
						return;
					}
				}
			}
			else
			{
				NAV_AddMoveMovementTask(pBot, StartPoint, nullptr);
				return;
			}
		}
		else
		{
			if (Platform->State == OBJECTSTATE_IDLE)
			{
				NAV_AddTriggerMovementTask(pBot, BestTrigger, Platform);
				return;
			}
			else
			{
				// Where we will stand to activate this trigger
				const Vector ButtonFloorLoc = UTIL_GetButtonFloorLocation(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, BestTrigger->Edict);

				// Make sure the button we want to use isn't underneath the lift. We don't want to get crushed!
				const Vector HalfSize = PlatformEdict->v.size * 0.5f;

				bool bWillBeCrushedByPlatform = vPointOverlaps2D(ButtonFloorLoc, DesiredStartStop.StopLocation - HalfSize, DesiredStartStop.StopLocation + HalfSize);

				if (bWillBeCrushedByPlatform)
				{
					NAV_AddMoveMovementTask(pBot, StartPoint, nullptr);
				}
				else
				{
					NAV_AddMoveMovementTask(pBot, ButtonFloorLoc, nullptr);
				}
				
				return;
			}
		}
	}
	else
	{
		if (NAV_CanBoardPlatform(pBot, Platform, StartPoint, DesiredStartStop.StopLocation))
		{
			BoardPlatformMove(pBot, StartPoint, Platform);
			return;
		}
		else
		{
			NAV_AddMoveMovementTask(pBot, StartPoint, nullptr);
			return;
		}
	}
}

bool IsBotOffSwimPath(const AIPlayer* pBot)
{
	return false;
}

bool IsBotOffFlightPath(const AIPlayer* pBot)
{
	return false;
}

bool IsBotOffPath(const AIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		return true;
	}

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	NavMovementFlag CurrentNavFlag = (NavMovementFlag)CurrentPathNode.flag;
	Vector MoveFrom = CurrentPathNode.FromLocation;
	Vector MoveTo = CurrentPathNode.Location;

	Vector NextMoveLocation = ZERO_VECTOR;
	NavMovementFlag NextMoveFlag = NAV_FLAG_DISABLED;

	if (pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size() - 1)
	{
		bot_path_node NextPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

		NextMoveLocation = NextPathNode.Location;
		NextMoveFlag = (NavMovementFlag)NextPathNode.flag;
	}

	switch (CurrentNavFlag)
	{
	case NAV_FLAG_WALK:
	case NAV_FLAG_CROUCH:
		return IsBotOffWalkNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case NAV_FLAG_LADDER:
		return IsBotOffLadderNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case NAV_FLAG_FALL:
		return IsBotOffFallNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case NAV_FLAG_JUMP:
		return IsBotOffJumpNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	case NAV_FLAG_PLATFORM:
		return IsBotOffLiftNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	default:
		return IsBotOffWalkNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);
	}

	return IsBotOffWalkNode(pBot, MoveFrom, MoveTo, NextMoveLocation, NextMoveFlag);

}

bool IsBotOffLadderNode(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	if (!IsPlayerOnLadder(pBot->Edict))
	{
		if (pBot->BotNavInfo.IsOnGround)
		{
			if (!UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveStart) && !UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveEnd)) { return true; }
		}
	}

	return false;
}

bool IsBotOffWalkNode(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	if (!pBot->BotNavInfo.IsOnGround) { return false; }

	Vector NearestPointOnLine = vClosestPointOnLine(MoveStart, MoveEnd, pBot->Edict->v.origin);

	if (vPointOverlaps3D(NearestPointOnLine, pBot->Edict->v.absmin, pBot->Edict->v.absmax)) { return false; }

	if (vDist2DSq(pBot->Edict->v.origin, NearestPointOnLine) > sqrf(GetPlayerRadius(pBot->Edict) * 3.0f)) { return true; }

	if (vEquals2D(NearestPointOnLine, MoveStart) && !UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveStart)) { return true; }
	if (vEquals2D(NearestPointOnLine, MoveEnd) && !UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveEnd)) { return true; }

	return false;

}

bool IsBotOffFallNode(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	if (!pBot->BotNavInfo.IsOnGround) { return false; }

	Vector NearestPointOnLine = vClosestPointOnLine2D(MoveStart, MoveEnd, pBot->Edict->v.origin);

	if (!UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveStart) && !UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveEnd)) { return true; }

	return false;
}

bool IsBotOffJumpNode(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	if (!pBot->BotNavInfo.IsOnGround) { return false; }

	Vector ClosestPointOnLine = vClosestPointOnLine2D(MoveStart, MoveEnd, pBot->Edict->v.origin);

	if (vEquals2D(ClosestPointOnLine, MoveStart) || vEquals2D(ClosestPointOnLine, MoveEnd))
	{
		return (!UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveStart) && !UTIL_PointIsDirectlyReachable(GetPlayerBottomOfCollisionHull(pBot->Edict), MoveEnd));
	}

	return vDist2DSq(pBot->Edict->v.origin, ClosestPointOnLine) > sqrf(GetPlayerRadius(pBot->Edict) * 2.0f);
}

bool IsBotOffLiftNode(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	return false;
}

bool IsBotOffObstacleNode(const AIPlayer* pBot, Vector MoveStart, Vector MoveEnd, Vector NextMoveDestination, NavMovementFlag NextMoveFlag)
{
	return IsBotOffJumpNode(pBot, MoveStart, MoveEnd, NextMoveDestination, NextMoveFlag);
}

void MoveToWithoutNav(AIPlayer* pBot, const Vector Destination)
{
	pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;

	if (vIsZero(Destination)) { return; }

	Vector CurrentPos = (pBot->BotNavInfo.IsOnGround) ? pBot->Edict->v.origin : pBot->CurrentFloorPosition + GetPlayerOriginOffsetFromFloor(pBot->Edict, false);
	CurrentPos.z += 18.0f;

	const Vector vForward = UTIL_GetVectorNormal2D(Destination - CurrentPos);
	// Same goes for the right vector, might not be the same as the bot's right
	const Vector vRight = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(vForward, UP_VECTOR));

	const float PlayerRadius = GetPlayerRadius(pBot->Edict);

	Vector stTrcLft = CurrentPos - (vRight * PlayerRadius);
	Vector stTrcRt = CurrentPos + (vRight * PlayerRadius);
	Vector endTrcLft = stTrcLft + (vForward * (PlayerRadius * 1.5f));
	Vector endTrcRt = stTrcRt + (vForward * (PlayerRadius * 1.5f));

	TraceResult hit;

	UTIL_TraceHull(stTrcLft, endTrcLft, ignore_monsters, head_hull, pBot->Edict->v.pContainingEntity, &hit);

	const bool bumpLeft = (hit.flFraction < 1.0f || hit.fAllSolid > 0 || hit.fStartSolid > 0);

	UTIL_TraceHull(stTrcRt, endTrcRt, ignore_monsters, head_hull, pBot->Edict->v.pContainingEntity, &hit);

	const bool bumpRight = (hit.flFraction < 1.0f || hit.fAllSolid > 0 || hit.fStartSolid > 0);

	pBot->desiredMovementDir = vForward;

	if (bumpRight && !bumpLeft)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
	}
	else if (bumpLeft && !bumpRight)
	{
		pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
	}
	else if (bumpLeft && bumpRight)
	{
		float MaxScaleHeight = GetPlayerMaxJumpHeight(pBot->Edict);

		float JumpHeight = 0.0f;

		bool bFoundJumpHeight = false;

		Vector StartTrace = pBot->CurrentFloorPosition;
		Vector EndTrace = StartTrace + (vForward * 50.0f);
		EndTrace.z = StartTrace.z;

		TraceResult JumpTestHit;

		while (JumpHeight < MaxScaleHeight && !bFoundJumpHeight)
		{
			UTIL_TraceHull(StartTrace, EndTrace, ignore_monsters, head_hull, pBot->Edict->v.pContainingEntity, &JumpTestHit);

			if (JumpTestHit.flFraction >= 1.0f && !JumpTestHit.fAllSolid)
			{
				bFoundJumpHeight = true;
				break;
			}

			JumpHeight += 5.0f;

			StartTrace.z += 5.0f;
			EndTrace.z += 5.0f;
		}

		if (JumpHeight <= MaxScaleHeight)
		{
			BotJump(pBot);
		}
		else
		{
			stTrcLft.z = pBot->Edict->v.origin.z;
			stTrcRt.z = pBot->Edict->v.origin.z;
			endTrcLft.z = pBot->Edict->v.origin.z;
			endTrcRt.z = pBot->Edict->v.origin.z;

			if (!UTIL_QuickTrace(pBot->Edict, stTrcLft, endTrcLft))
			{
				pBot->desiredMovementDir = pBot->desiredMovementDir + vRight;
			}
			else
			{
				pBot->desiredMovementDir = pBot->desiredMovementDir - vRight;
			}
		}

	}

	float DistFromDestination = vDist2DSq(pBot->Edict->v.origin, Destination);

	if (vIsZero(pBot->LookTargetLocation))
	{
		Vector LookTarget = Destination;

		if (DistFromDestination < sqrf(200.0f))
		{
			Vector LookNormal = UTIL_GetVectorNormal2D(LookTarget - pBot->CurrentEyePosition);

			LookTarget = LookTarget + (LookNormal * 1000.0f);
		}

		BotLookAt(pBot, LookTarget);
	}

	HandlePlayerAvoidance(pBot, Destination);
	BotMovementInputs(pBot);
}

bool UTIL_PointIsDirectlyReachable(const AIPlayer* pBot, const Vector targetPoint)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	if (!m_navQuery) { return false; }

	edict_t* pEdict = pBot->Edict;

	Vector CurrentPos = (pBot->BotNavInfo.IsOnGround) ? pBot->Edict->v.origin : pBot->CurrentFloorPosition;

	float pStartPos[3] = { CurrentPos.x, CurrentPos.z, -CurrentPos.y };
	float pEndPos[3] = { targetPoint.x, targetPoint.z, -targetPoint.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3];
	float EndNearest[3];

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;


	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_navFilter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_navFilter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_navFilter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f) { return false; }

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);

}

bool UTIL_PointIsDirectlyReachable(const AIPlayer* pBot, const Vector start, const Vector target)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(pBot->BotNavInfo.NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(pBot->BotNavInfo.NavProfile);
	const dtQueryFilter* m_navFilter = &pBot->BotNavInfo.NavProfile.Filters;

	if (!m_navQuery) { return false; }

	if (vIsZero(start) || vIsZero(target)) { return false; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3];
	float EndNearest[3];

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;

	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_navFilter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_navFilter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_navFilter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f) { return false; }

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);

}

const dtNavMesh* UTIL_GetNavMeshForProfile(const NavAgentProfile& NavProfile)
{
	if (NavProfile.NavMeshIndex >= NUM_NAV_MESHES) { return nullptr; }

	return NavMeshes[NavProfile.NavMeshIndex].navMesh;
}

const dtNavMeshQuery* UTIL_GetNavMeshQueryForProfile(const NavAgentProfile& NavProfile)
{
	if (NavProfile.NavMeshIndex >= NUM_NAV_MESHES) { return nullptr; }

	return NavMeshes[NavProfile.NavMeshIndex].navQuery;
}

const dtTileCache* UTIL_GetTileCacheForProfile(const NavAgentProfile& NavProfile)
{
	if (NavProfile.NavMeshIndex >= NUM_NAV_MESHES) { return nullptr; }

	return NavMeshes[NavProfile.NavMeshIndex].tileCache;
}

bool UTIL_PointIsDirectlyReachable(const NavAgentProfile &NavProfile, const Vector start, const Vector target)
{
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navMesh) { return false; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;


	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_navFilter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_navFilter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_navFilter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f)
	{
		if (pathCount == 0) { return false; }

		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[pathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(max_ai_use_reach))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);
}

bool UTIL_TraceNav(const NavAgentProfile &NavProfile, const Vector start, const Vector target, const float MaxAcceptableDistance)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_Filter = &NavProfile.Filters;

	if (!m_navQuery) { return false; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;

	float MaxReachableExtents[3] = { MaxAcceptableDistance, 50.0f, MaxAcceptableDistance };

	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, MaxReachableExtents, m_Filter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, MaxReachableExtents, m_Filter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_Filter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f)
	{
		if (pathCount == 0) { return false; }

		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[pathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(MaxAcceptableDistance))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);
}

void UTIL_TraceNavLine(const NavAgentProfile &NavProfile, const Vector Start, const Vector End, nav_hitresult* HitResult)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_Filter = &NavProfile.Filters;

	if (!m_navQuery)
	{
		HitResult->flFraction = 0.0f;
		HitResult->bStartOffMesh = true;
		HitResult->TraceEndPoint = Start;
		return;
	}

	float pStartPos[3] = { Start.x, Start.z, -Start.y };
	float pEndPos[3] = { End.x, End.z, -End.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;

	float MaxReachableExtents[3] = { 18.0f, 32.0f, 18.0f };

	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, MaxReachableExtents, m_Filter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		HitResult->flFraction = 0.0f;
		HitResult->bStartOffMesh = true;
		HitResult->TraceEndPoint = Start;
		return;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, MaxReachableExtents, m_Filter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		HitResult->flFraction = 0.0f;
		HitResult->bStartOffMesh = true;
		HitResult->TraceEndPoint = Start;
		return;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly)
	{

		HitResult->flFraction = 1.0f;
		HitResult->bStartOffMesh = false;
		HitResult->TraceEndPoint = Vector(EndNearest[0], -EndNearest[2], EndNearest[1]);
		return;
	}	

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_Filter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	HitResult->flFraction = hitDist;
	HitResult->bStartOffMesh = false;

	Vector HitLocation = ZERO_VECTOR;

	if (hitDist >= 1.0f)
	{
		HitLocation = Vector(EndNearest[0], -EndNearest[2], EndNearest[1]);
	}
	else
	{
		Vector Dir = UTIL_GetVectorNormal(End - Start);
		Vector Point = Start + (Dir * HitResult->flFraction);

		HitLocation = UTIL_ProjectPointToNavmesh(NavProfile.NavMeshIndex, Point, NavProfile, Vector(100.0f, 100.0f, 100.0f));
	}

	HitResult->TraceEndPoint = HitLocation;
}

bool UTIL_PointIsDirectlyReachable(const Vector start, const Vector target)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtQueryFilter* m_Filter = &GetBaseAgentProfile(NAV_PROFILE_DEFAULT).Filters;

	if (!m_navQuery) { return false; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3] = { 0.0f, 0.0f, 0.0f };
	float EndNearest[3] = { 0.0f, 0.0f, 0.0f };

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;


	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_Filter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return false;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_Filter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return false;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return true; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_Filter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	if (hitDist < 1.0f)
	{
		if (pathCount == 0) { return false; }

		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[pathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(max_ai_use_reach))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	if (EndPoly == PolyPath[pathCount - 1]) { return true; }

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);


	return (Height == 0.0f || Height == EndNearest[1]);
}

float UTIL_PointIsDirectlyReachable_DEBUG(const Vector start, const Vector target)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtQueryFilter* m_Filter = &GetBaseAgentProfile(NAV_PROFILE_DEFAULT).Filters;

	if (!m_navQuery) { return 0.0f; }

	float pStartPos[3] = { start.x, start.z, -start.y };
	float pEndPos[3] = { target.x, target.z, -target.y };

	dtPolyRef StartPoly;
	dtPolyRef EndPoly;
	float StartNearest[3];
	float EndNearest[3];

	float hitDist;
	float HitNormal[3];

	dtPolyRef PolyPath[MAX_PATH_POLY];
	int pathCount = 0;


	dtStatus FoundStartPoly = m_navQuery->findNearestPoly(pStartPos, pReachableExtents, m_Filter, &StartPoly, StartNearest);

	if (!dtStatusSucceed(FoundStartPoly))
	{
		return 1.1f;
	}

	dtStatus FoundEndPoly = m_navQuery->findNearestPoly(pEndPos, pReachableExtents, m_Filter, &EndPoly, EndNearest);

	if (!dtStatusSucceed(FoundEndPoly))
	{
		return 1.2f;
	}

	// All polys are convex, therefore definitely reachable if start and end points are within the same poly
	if (StartPoly == EndPoly) { return 2.1f; }

	m_navQuery->raycast(StartPoly, StartNearest, EndNearest, m_Filter, &hitDist, HitNormal, PolyPath, &pathCount, MAX_AI_PATH_SIZE);

	float ClosestPoint[3] = { 0.0f, 0.0f, 0.0f };
	float Height = 0.0f;
	m_navQuery->closestPointOnPolyBoundary(PolyPath[pathCount - 1], EndNearest, ClosestPoint);
	m_navQuery->getPolyHeight(PolyPath[pathCount - 1], ClosestPoint, &Height);

	float Dist = dtVdistSqr(EndNearest, ClosestPoint);

	if (hitDist < 1.0f)
	{
		if (pathCount == 0) { return 1.3f; }

		float epos[3];
		dtVcopy(epos, EndNearest);

		m_navQuery->closestPointOnPoly(PolyPath[pathCount - 1], EndNearest, epos, 0);

		if (dtVdistSqr(EndNearest, epos) > sqrf(max_ai_use_reach))
		{
			return 1.5f;
		}
		else
		{
			return 2.4f;
		}


		return 1.3f;
	}

	if (EndPoly != PolyPath[pathCount - 1])
	{
		if (Height == 0.0f || Height == EndNearest[1])
		{
			return 2.3f;
		}
		return 1.4f;
	}

	return 2.2f;
}

dtPolyRef UTIL_GetNearestPolyRefForLocation(const NavAgentProfile& NavProfile, const Vector Location)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return 0; }

	float ConvertedFloorCoords[3] = { Location.x, Location.z, -Location.y };

	float pPolySearchExtents[3] = { 50.0f, 50.0f, 50.0f };

	dtPolyRef result;
	float nearestPoint[3] = { 0.0f, 0.0f, 0.0f };

	m_navQuery->findNearestPoly(ConvertedFloorCoords, pPolySearchExtents, m_navFilter, &result, nearestPoint);

	return result;
}

dtPolyRef UTIL_GetNearestPolyRefForLocation(const Vector Location)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtQueryFilter* m_navFilter = &GetBaseAgentProfile(NAV_PROFILE_DEFAULT).Filters;

	if (!m_navQuery) { return 0; }

	float ConvertedFloorCoords[3] = { Location.x, Location.z, -Location.y };

	float pPolySearchExtents[3] = { 50.0f, 50.0f, 50.0f };

	dtPolyRef result;
	float nearestPoint[3] = { 0.0f, 0.0f, 0.0f };

	m_navQuery->findNearestPoly(ConvertedFloorCoords, pPolySearchExtents, m_navFilter, &result, nearestPoint);

	return result;
}

dtPolyRef UTIL_GetNearestPolyRefForEntity(const edict_t* Edict)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtQueryFilter* m_navFilter = &GetBaseAgentProfile(NAV_PROFILE_DEFAULT).Filters;

	if (!m_navQuery) { return 0; }

	Vector Floor = UTIL_GetFloorUnderEntity(Edict);

	float ConvertedFloorCoords[3] = { Floor.x, Floor.z, -Floor.y };

	float pPolySearchExtents[3] = { 50.0f, 50.0f, 50.0f };

	dtPolyRef result;
	float nearestPoint[3] = { 0.0f, 0.0f, 0.0f };

	m_navQuery->findNearestPoly(ConvertedFloorCoords, pPolySearchExtents, m_navFilter, &result, nearestPoint);

	return result;
}

unsigned char UTIL_GetNavAreaAtLocation(const NavAgentProfile &NavProfile, const Vector Location)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return (unsigned char)NAV_AREA_UNWALKABLE; }

	Vector TraceHit = UTIL_GetTraceHitLocation(Location + Vector(0.0f, 0.0f, 10.0f), Location - Vector(0.0f, 0.0f, 500.0f));

	Vector PointToProject = (TraceHit != ZERO_VECTOR) ? TraceHit : Location;

	float pCheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, pReachableExtents, m_navFilter, &FoundPoly, NavNearest);

	if (dtStatusSucceed(success))
	{
		unsigned char area = 0;
		m_navMesh->getPolyArea(FoundPoly, &area);
		return area;
	}
	else
	{
		return (unsigned char)NAV_AREA_UNWALKABLE;
	}
}

unsigned char UTIL_GetNavAreaAtLocation(const Vector Location)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(GetBaseAgentProfile(NAV_PROFILE_DEFAULT));
	const dtQueryFilter* m_navFilter = &GetBaseAgentProfile(NAV_PROFILE_DEFAULT).Filters;

	if (!m_navQuery) { return 0; }

	Vector TraceHit = UTIL_GetTraceHitLocation(Location + Vector(0.0f, 0.0f, 10.0f), Location - Vector(0.0f, 0.0f, 500.0f));

	Vector PointToProject = (TraceHit != ZERO_VECTOR) ? TraceHit : Location;

	float pCheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, pReachableExtents, m_navFilter, &FoundPoly, NavNearest);

	if (dtStatusSucceed(success))
	{
		unsigned char area = 0;
		m_navMesh->getPolyArea(FoundPoly, &area);
		return area;
	}
	else
	{
		return 0;
	}
}

void UTIL_UpdateBotMovementStatus(AIPlayer* pBot)
{
	if (pBot->Edict->v.movetype != pBot->BotNavInfo.CurrentMoveType)
	{
		if (pBot->BotNavInfo.CurrentMoveType == MOVETYPE_FLY)
		{
			OnBotEndLadder(pBot);
		}


		if (pBot->Edict->v.movetype == MOVETYPE_FLY)
		{
			OnBotStartLadder(pBot);
		}

		pBot->BotNavInfo.CurrentMoveType = pBot->Edict->v.movetype;
	}

	pBot->BotNavInfo.CurrentPoly = UTIL_GetNearestPolyRefForEntity(pBot->Edict);

	pBot->CollisionHullBottomLocation = GetPlayerBottomOfCollisionHull(pBot->Edict);
	pBot->CollisionHullTopLocation = GetPlayerTopOfCollisionHull(pBot->Edict);
}


bool AbortCurrentMove(AIPlayer* pBot, const Vector NewDestination)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size() || pBot->BotNavInfo.NavProfile.bFlyingProfile) { return true; }

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	Vector MoveFrom = CurrentPathNode.FromLocation;
	Vector MoveTo = CurrentPathNode.Location;
	unsigned int flag = CurrentPathNode.flag;

	Vector ClosestPointOnLine = vClosestPointOnLine2D(MoveFrom, MoveTo, pBot->Edict->v.origin);

	bool bAtOrPastMovement = (vEquals2D(ClosestPointOnLine, MoveFrom, 1.0f) && fabsf(pBot->Edict->v.origin.z - MoveFrom.z) <= 50.0f ) || (vEquals2D(ClosestPointOnLine, MoveTo, 1.0f) && fabsf(pBot->Edict->v.origin.z - MoveTo.z) <= 50.0f) ;

	if ((pBot->Edict->v.flags & FL_ONGROUND) && bAtOrPastMovement)
	{
		return true;
	}

	Vector DestinationPointOnLine = vClosestPointOnLine(MoveFrom, MoveTo, NewDestination);

	bool bReverseCourse = (vDist3DSq(DestinationPointOnLine, MoveFrom) < vDist3DSq(DestinationPointOnLine, MoveTo));

	if (flag == NAV_FLAG_PLATFORM)
	{
		if (bReverseCourse)
		{
			if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveFrom)) { return true; }
			PlatformMove(pBot, MoveTo, MoveFrom, CurrentPathNode.Platform);
		}
		else
		{
			if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveTo)) { return true; }
			PlatformMove(pBot, MoveFrom, MoveTo, CurrentPathNode.Platform);
		}
	}

	if (flag == NAV_FLAG_WALK || flag == NAV_FLAG_CROUCH)
	{
		if (UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveFrom) || UTIL_PointIsDirectlyReachable(pBot->CurrentFloorPosition, MoveTo))
		{
			return true;
		}

		if (bReverseCourse)
		{
			GroundMove(pBot, MoveTo, MoveFrom);
		}
		else
		{
			GroundMove(pBot, MoveFrom, MoveTo);
		}
	}

	if (flag == NAV_FLAG_LADDER)
	{
		if (bReverseCourse)
		{

			LadderMove(pBot, MoveTo, MoveFrom, CurrentPathNode.requiredZ, (unsigned char)NAV_AREA_CROUCH);

			// We're going DOWN the ladder
			if (MoveTo.z > MoveFrom.z)
			{
				if (pBot->Edict->v.origin.z - MoveFrom.z < 150.0f)
				{
					BotJump(pBot);
				}
			}
		}
		else
		{

			LadderMove(pBot, MoveFrom, MoveTo, CurrentPathNode.requiredZ, (unsigned char)NAV_AREA_CROUCH);

			// We're going DOWN the ladder
			if (MoveFrom.z > MoveTo.z)
			{
				if (pBot->Edict->v.origin.z - MoveFrom.z < 150.0f)
				{
					BotJump(pBot);
				}
			}
		}
	}

	if (flag == NAV_FLAG_JUMP)
	{
		if (bReverseCourse)
		{
			JumpMove(pBot, MoveTo, MoveFrom);
		}
		else
		{
			JumpMove(pBot, MoveFrom, MoveTo);
		}
	}

	if (flag == NAV_FLAG_FALL)
	{
		FallMove(pBot, MoveFrom, MoveTo);
	}

	BotMovementInputs(pBot);

	return false;
}

void UpdateBotStuck(AIPlayer* pBot)
{
	if (vIsZero(pBot->desiredMovementDir) && !pBot->BotNavInfo.StuckInfo.bPathFollowFailed)
	{
		return;
	}

	if (!pBot->BotNavInfo.StuckInfo.bPathFollowFailed)
	{

		bool bIsFollowingPath = (pBot->BotNavInfo.CurrentPath.size() > 0 && pBot->BotNavInfo.CurrentPathPoint < pBot->BotNavInfo.CurrentPath.size());

		bool bDist3D = pBot->BotNavInfo.NavProfile.bFlyingProfile || (pBot->Edict->v.flags & FL_INWATER) || (bIsFollowingPath && (pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint].flag == NAV_FLAG_LADDER));

		float DistFromLastPoint = (bDist3D) ? vDist3DSq(pBot->Edict->v.origin, pBot->BotNavInfo.StuckInfo.LastBotPosition) : vDist2DSq(pBot->Edict->v.origin, pBot->BotNavInfo.StuckInfo.LastBotPosition);

		if (DistFromLastPoint >= sqrf(8.0f))
		{
			pBot->BotNavInfo.StuckInfo.TotalStuckTime = 0.0f;
			pBot->BotNavInfo.StuckInfo.LastBotPosition = pBot->Edict->v.origin;
		}
		else
		{
			pBot->BotNavInfo.StuckInfo.TotalStuckTime += pBot->ThinkDelta;
		}
	}
	else
	{
		pBot->BotNavInfo.StuckInfo.TotalStuckTime += pBot->ThinkDelta;
	}

	if (pBot->BotNavInfo.StuckInfo.TotalStuckTime > 0.25f)
	{
		if (pBot->BotNavInfo.StuckInfo.TotalStuckTime > CONFIG_GetMaxStuckTime())
		{
			BotSuicide(pBot);
			return;
		}

		if (pBot->BotNavInfo.StuckInfo.TotalStuckTime > 5.0f)
		{
			ClearBotPath(pBot);
		}

		if (!vIsZero(pBot->desiredMovementDir))
		{
			BotJump(pBot, true);
		}		
	}
}

void SetBaseNavProfile(AIPlayer* pBot)
{
	pBot->BotNavInfo.bNavProfileChanged = true;

}

void UpdateBotMoveProfile(AIPlayer* pBot, BotMoveStyle MoveStyle)
{	
	pBot->BotNavInfo.NavProfile = GetBaseAgentProfile(NAV_PROFILE_MARINE);
	pBot->BotNavInfo.bNavProfileChanged = false;
}

bool NAV_GenerateNewBasePath(AIPlayer* pBot, const Vector NewDestination, const BotMoveStyle MoveStyle, const float MaxAcceptableDist)
{
	nav_status* BotNavInfo = &pBot->BotNavInfo;

	dtStatus PathFindingStatus = DT_FAILURE;

	vector<bot_path_node> PendingPath;
	bool bIsFlyingProfile = BotNavInfo->NavProfile.bFlyingProfile;


	if (bIsFlyingProfile)
	{
		PathFindingStatus = FindFlightPathToPoint(BotNavInfo->NavProfile, pBot->CurrentFloorPosition, NewDestination, PendingPath, MaxAcceptableDist);
	}
	else
	{
		Vector NavAdjustedDestination = AdjustPointForPathfinding(BotNavInfo->NavProfile.NavMeshIndex, NewDestination, BotNavInfo->NavProfile);
		if (vIsZero(NavAdjustedDestination)) { return false; }

		PathFindingStatus = FindPathClosestToPoint(pBot, BotNavInfo->MoveStyle, NavAdjustedDestination, PendingPath, MaxAcceptableDist);
	}

	BotNavInfo->NextForceRecalc = 0.0f;
	BotNavInfo->bNavProfileChanged = false;

	if (dtStatusSucceed(PathFindingStatus))
	{
		if (!NAV_MergeAndUpdatePath(pBot, PendingPath))
		{
			if (!AbortCurrentMove(pBot, NewDestination))
			{
				return true;
			}
			else
			{
				pBot->BotNavInfo.CurrentPath.clear();
				pBot->BotNavInfo.CurrentPath.insert(pBot->BotNavInfo.CurrentPath.begin(), PendingPath.begin(), PendingPath.end());
				BotNavInfo->CurrentPathPoint = 0;
			}
		}

		pBot->BotNavInfo.CurrentPath.clear();
		pBot->BotNavInfo.CurrentPath.insert(pBot->BotNavInfo.CurrentPath.begin(), PendingPath.begin(), PendingPath.end());
		BotNavInfo->CurrentPathPoint = 0;

		BotNavInfo->ActualMoveDestination = BotNavInfo->CurrentPath.back().Location;

		pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;
		ClearBotStuckMovement(pBot);
		pBot->BotNavInfo.TotalStuckTime = 0.0f;
		BotNavInfo->PathDestination = NewDestination;

		return true;
	}

	return false;
}

bool NAV_MergeAndUpdatePath(AIPlayer* pBot, vector<bot_path_node>& NewPath)
{
	if (pBot->BotNavInfo.NavProfile.bFlyingProfile || pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		pBot->BotNavInfo.CurrentPath.clear();
		pBot->BotNavInfo.CurrentPath.insert(pBot->BotNavInfo.CurrentPath.end(), NewPath.begin(), NewPath.end());
		pBot->BotNavInfo.CurrentPathPoint = 0;
		return true;
	}

	// Start with the bot's current path point
	vector<bot_path_node>::iterator OldPathStart = (pBot->BotNavInfo.CurrentPath.begin() + pBot->BotNavInfo.CurrentPathPoint);
	vector<bot_path_node>::iterator OldPathEnd;
	vector<bot_path_node>::iterator NewPathStart;

	// We skip ahead in the path until we reach the first non-walk node in our CURRENT path
	for (OldPathEnd = OldPathStart; OldPathEnd != pBot->BotNavInfo.CurrentPath.end(); OldPathEnd++)
	{
		if (OldPathEnd->flag != NAV_FLAG_WALK && OldPathEnd->flag != NAV_FLAG_CROUCH)
		{
			break;
		}
	}

	// Our path is all walk, so we will return false which will basically cause us to cancel this path completely and adopt a new one
	if (OldPathEnd == pBot->BotNavInfo.CurrentPath.end())
	{
		return false;
	}

	// We have reached the next non-walk node in our CURRENT path, now we find the next non-walk path in a prospective NEW path
	for (NewPathStart = NewPath.begin(); NewPathStart != NewPath.end(); NewPathStart++)
	{
		if (NewPathStart->flag != NAV_FLAG_WALK && OldPathEnd->flag != NAV_FLAG_CROUCH)
		{
			break;
		}
	}

	// New path is all walk, just embrace it and forget the old path
	if (NewPathStart == NewPath.end())
	{
		return false;
	}

	// The upcoming non-walk node in our current path and the upcoming non-walk node in our new path are different: we have a different path entirely so just get the new path instead
	if (OldPathEnd->flag != NewPathStart->flag || !vEquals(OldPathEnd->FromLocation, NewPathStart->FromLocation, 16.0f) || !vEquals(OldPathEnd->Location, NewPathStart->Location, 16.0f))
	{
		return false;
	}

	// Now we truncate the current path at the non-walk node, and append the new path from that point
	OldPathEnd = next(OldPathEnd);
	NewPathStart = next(NewPathStart);

	for (auto it = OldPathEnd; it != pBot->BotNavInfo.CurrentPath.end();)
	{
		it = pBot->BotNavInfo.CurrentPath.erase(it);
	}

	pBot->BotNavInfo.CurrentPath.insert(pBot->BotNavInfo.CurrentPath.end(), NewPathStart, NewPath.end());
	return true;
}

bool NAV_PlatformNeedsActivating(AIPlayer* pBot, DynamicMapObject* Platform, const Vector EmbarkPoint, const Vector DisembarkPoint)
{
	if (Platform->Triggers.size() > 0 && Platform->Triggers[0] == Platform->Edict) { return false; }

	// Obviously if the platform is idle then it needs activating regardless of where it is
	if (Platform->State == OBJECTSTATE_IDLE) { return true; }

	DynamicMapObjectStop NextFullStop;
	DynamicMapObjectStop CurrentStop = Platform->StopPoints[Platform->NextStopIndex];

	DynamicMapObjectStop StartStop, EndStop;

	GetDesiredPlatformStartAndEnd(Platform, EmbarkPoint, DisembarkPoint, StartStop, EndStop);

	int currIndex = Platform->NextStopIndex;
	int prevIndex = (currIndex == 0) ? Platform->StopPoints.size() - 1 : currIndex - 1;

	int desiredStopIndex = 0;

	DynamicMapObjectStop PreviousStop = Platform->StopPoints[prevIndex];

	// Platform is at or leaving our desired stop, check if we can get on. If so then we don't need to retrigger
	if (vEquals(PreviousStop.StopLocation, StartStop.StopLocation))
	{
		if (NAV_CanBoardPlatform(pBot, Platform, EmbarkPoint, StartStop.StopLocation)) { return false; }
	}

	// If the platform is going to wait at our embark point then we must need to activate it regardless. If we can activate it from the platform then carry on as normal
	if (StartStop.bWaitForRetrigger)
	{
		return true;
	}

	// Platform is coming to our desired stop, we already checked above if we need to retrigger, so now assume it's not needed
	if (vEquals(CurrentStop.StopLocation, StartStop.StopLocation))
	{
		return StartStop.bWaitForRetrigger;
	}

	// We will go through every stop the platform will go to on its way to us, and if it doesn't stop before reaching us
	// (and isn't stopping at our desired stop) then we don't need to trigger it
	while(true)
	{
		DynamicMapObjectStop PrevStop = Platform->StopPoints[prevIndex];
		DynamicMapObjectStop NextStop = Platform->StopPoints[currIndex];

		if (vEquals(Platform->StopPoints[currIndex].StopLocation, StartStop.StopLocation))
		{
			return StartStop.bWaitForRetrigger;
		}

		Vector ClosestPointOnLine = vClosestPointOnLine(PrevStop.StopLocation, NextStop.StopLocation, StartStop.StopLocation);

		// The platform will pass by our desired stop point, so don't need to trigger it
		if (vEquals(ClosestPointOnLine, StartStop.StopLocation, 5.0f) && !vEquals(PrevStop.StopLocation, ClosestPointOnLine, 5.0f) && !vEquals(NextStop.StopLocation, ClosestPointOnLine, 5.0f))
		{
			return false;
		}

		if (NextStop.bWaitForRetrigger) { return true; }

		currIndex++;

		if (currIndex > Platform->StopPoints.size() - 1)
		{
			currIndex = 0;
		}

		prevIndex = (currIndex == 0) ? Platform->StopPoints.size() - 1 : currIndex - 1;
	}

	return false;
}

bool NAV_GeneratePathForTask(AIPlayer* pBot, AIPlayerMoveTask& Task)
{
	Task.bPathGenerated = NAV_GenerateNewBasePath(pBot, Task.TaskLocation, pBot->BotNavInfo.MoveStyle, 60.0f);

	nav_status* BotNavInfo = &pBot->BotNavInfo;

	if (!Task.bPathGenerated)
	{
		pBot->BotNavInfo.StuckInfo.bPathFollowFailed = true;

		if (!UTIL_PointIsOnNavmesh(pBot->CollisionHullBottomLocation, pBot->BotNavInfo.NavProfile, Vector(50.0f, 50.0f, 50.0f)))
		{
			if (!vIsZero(BotNavInfo->LastNavMeshPosition))
			{
				MoveToWithoutNav(pBot, BotNavInfo->LastNavMeshPosition);

				if (vDist2DSq(pBot->CurrentFloorPosition, BotNavInfo->LastNavMeshPosition) < 1.0f)
				{
					BotNavInfo->LastNavMeshPosition = ZERO_VECTOR;
				}

				return false;
			}
			else
			{
				if (!vIsZero(BotNavInfo->UnstuckMoveLocation) && vDist2DSq(pBot->CurrentFloorPosition, BotNavInfo->UnstuckMoveLocation) < 1.0f)
				{
					BotNavInfo->UnstuckMoveLocation = ZERO_VECTOR;
				}

				if (vIsZero(BotNavInfo->UnstuckMoveLocation))
				{
					BotNavInfo->UnstuckMoveLocation = FindClosestPointBackOnPath(pBot, Task.TaskLocation);
				}

				if (!vIsZero(BotNavInfo->UnstuckMoveLocation))
				{
					MoveToWithoutNav(pBot, BotNavInfo->UnstuckMoveLocation);
					return false;
				}
			}
		}
		else
		{
			if (!vIsZero(BotNavInfo->UnstuckMoveLocation) && vDist2DSq(pBot->CurrentFloorPosition, BotNavInfo->UnstuckMoveLocation) < 1.0f)
			{
				BotNavInfo->UnstuckMoveLocation = ZERO_VECTOR;
			}

			if (vIsZero(BotNavInfo->UnstuckMoveLocation))
			{
				BotNavInfo->UnstuckMoveLocation = FindClosestPointBackOnPath(pBot, Task.TaskLocation);
			}

			if (!vIsZero(BotNavInfo->UnstuckMoveLocation))
			{
				MoveToWithoutNav(pBot, BotNavInfo->UnstuckMoveLocation);
				return false;
			}
			else
			{
				MoveToWithoutNav(pBot, Task.TaskLocation);
			}


			return false;
		}

		return false;
	}

	return true;
}

void NAV_ProgressMovementTask(AIPlayer* pBot, AIPlayerMoveTask& Task)
{
	if (Task.TaskType == MOVE_TASK_NONE) { return; }

	if (Task.TaskType == MOVE_TASK_USE)
	{
		if (IsPlayerInUseRange(pBot->Edict, Task.TaskTarget))
		{
			BotUseObject(pBot, Task.TaskTarget, false);
			ClearBotStuck(pBot);
			return;
		}
	}

	if (Task.TaskType == MOVE_TASK_BREAK)
	{
		AIWeaponType Weapon = WEAP_GetPlayerCurrentWeapon(pBot->Edict);

		BotAttackResult AttackResult = PerformAttackLOSCheck(pBot, Weapon, Task.TaskTarget);

		if (AttackResult == ATTACK_SUCCESS)
		{
			// If we were ducking before then keep ducking
			if (pBot->Edict->v.oldbuttons & IN_DUCK)
			{
				pBot->Button |= IN_DUCK;
			}

			BotShootTarget(pBot, Weapon, Task.TaskTarget);

			ClearBotStuck(pBot);

			return;
		}
	}

	if (!vEquals(pBot->BotNavInfo.PathDestination, Task.TaskLocation, GetPlayerRadius(pBot->Edict)))
	{
		NAV_GeneratePathForTask(pBot, Task);
		return;
	}

	BotFollowPath(pBot);

	BotMovementInputs(pBot);
}

bool NAV_MoveTo(AIPlayer* pBot, const Vector Destination, const BotMoveStyle MoveStyle, const float MaxAcceptableDist)
{
	// Trying to move nowhere, or our current location. Do nothing
	if (vIsZero(Destination) || (vDist2D(pBot->Edict->v.origin, Destination) <= 6.0f && (fabs(pBot->CollisionHullBottomLocation.z - Destination.z) < 50.0f)))
	{
		pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;
		ClearBotMovement(pBot);

		return true;
	}

	if (!UTIL_IsTileCacheUpToDate()) { return true; }

	nav_status* BotNavInfo = &pBot->BotNavInfo;

	pBot->BotNavInfo.MoveStyle = MoveStyle;
	UTIL_UpdateBotMovementStatus(pBot);

	UpdateBotMoveProfile(pBot, MoveStyle);

	bool bIsFlyingProfile = pBot->BotNavInfo.NavProfile.bFlyingProfile;
	bool bNavProfileChanged = pBot->BotNavInfo.bNavProfileChanged;
	bool bForceRecalculation = (pBot->BotNavInfo.NextForceRecalc > 0.0f && gpGlobals->time >= pBot->BotNavInfo.NextForceRecalc);

	bool bEndGoalChanged = (!vEquals(Destination, BotNavInfo->TargetDestination, GetPlayerRadius(pBot->Edict)));

	if (bEndGoalChanged)
	{
		ClearBotMovement(pBot);
	}

	if (pBot->BotNavInfo.MovementTasks.empty())
	{
		AIPlayerMoveTask PrimaryMoveTask;
		PrimaryMoveTask.TaskType = MOVE_TASK_MOVE;
		PrimaryMoveTask.TaskLocation = Destination;
		PrimaryMoveTask.bPathGenerated = false;

		pBot->BotNavInfo.MovementTasks.push_back(PrimaryMoveTask);
		BotNavInfo->TargetDestination = Destination;
	}

	while (pBot->BotNavInfo.MovementTasks.size() > 0 && !NAV_IsMovementTaskStillValid(pBot, pBot->BotNavInfo.MovementTasks.back()))
	{
		pBot->BotNavInfo.MovementTasks.pop_back();
	}

	if (pBot->BotNavInfo.UnstuckTask.TaskType != MOVE_TASK_NONE)
	{
		if (!NAV_IsMovementTaskStillValid(pBot, pBot->BotNavInfo.UnstuckTask))
		{
			pBot->BotNavInfo.UnstuckTask.TaskType = MOVE_TASK_NONE;
		}
		else
		{
			NAV_ProgressMovementTask(pBot, pBot->BotNavInfo.UnstuckTask);
			return true;
		}
	}

	if (pBot->BotNavInfo.MovementTasks.size() > 0)
	{
		NAV_ProgressMovementTask(pBot, pBot->BotNavInfo.MovementTasks.back());
		return true;
	}

	return false;

}

bool NAV_GenerateNewMoveTaskPath(AIPlayer* pBot, const Vector NewDestination, const BotMoveStyle MoveStyle)
{
	nav_status* BotNavInfo = &pBot->BotNavInfo;

	dtStatus PathFindingStatus = DT_FAILURE;

	vector<bot_path_node> PendingPath;
	bool bIsFlyingProfile = BotNavInfo->NavProfile.bFlyingProfile;

	if (bIsFlyingProfile)
	{
		PathFindingStatus = FindFlightPathToPoint(BotNavInfo->NavProfile, pBot->CurrentFloorPosition, NewDestination, PendingPath, max_player_use_reach);
	}
	else
	{
		Vector NavAdjustedDestination = AdjustPointForPathfinding(BotNavInfo->NavProfile.NavMeshIndex, NewDestination, BotNavInfo->NavProfile);
		if (vIsZero(NavAdjustedDestination)) { return false; }

		PathFindingStatus = FindPathClosestToPoint(pBot, BotNavInfo->MoveStyle, NavAdjustedDestination, PendingPath, 100.0f);
	}

	if (dtStatusSucceed(PathFindingStatus))
	{
		pBot->BotNavInfo.CurrentPath.clear();
		pBot->BotNavInfo.CurrentPathPoint = 0;
		pBot->BotNavInfo.SpecialMovementFlags = 0;

		pBot->BotNavInfo.CurrentPath.insert(pBot->BotNavInfo.CurrentPath.begin(), PendingPath.begin(), PendingPath.end());
		BotNavInfo->CurrentPathPoint = 0;

		pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;
		ClearBotStuckMovement(pBot);
		pBot->BotNavInfo.TotalStuckTime = 0.0f;
		BotNavInfo->PathDestination = NewDestination;

		return true;
	}

	return false;
}

Vector FindClosestPointBackOnPath(AIPlayer* pBot, Vector Destination)
{
	Vector AdjustedEndPoint = AdjustPointForPathfinding(pBot->BotNavInfo.NavProfile.NavMeshIndex, Destination, pBot->BotNavInfo.NavProfile);
	AdjustedEndPoint = UTIL_ProjectPointToNavmesh(pBot->BotNavInfo.NavProfile.NavMeshIndex, Destination, pBot->BotNavInfo.NavProfile);

	vector<bot_path_node> BackwardsPath;
	BackwardsPath.clear();

	// Now we find a path backwards from the valid nav mesh point to our location, trying to get as close as we can to it

	dtStatus BackwardFindingStatus = FindPathClosestToPoint(pBot->BotNavInfo.NavProfile, AdjustedEndPoint, pBot->CurrentFloorPosition, BackwardsPath, UTIL_MetresToGoldSrcUnits(50.0f));

	if (dtStatusSucceed(BackwardFindingStatus))
	{
		Vector NewMoveLocation = prev(BackwardsPath.end())->Location;
		Vector NewMoveFromLocation = prev(BackwardsPath.end())->FromLocation;

		for (auto it = BackwardsPath.rbegin(); it != BackwardsPath.rend(); it++)
		{
			if (vDist2DSq(pBot->Edict->v.origin, it->Location) > sqrf(GetPlayerRadius(pBot->Edict)) && UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, it->Location))
			{
				NewMoveLocation = it->Location;
				NewMoveFromLocation = it->FromLocation;
				break;
			}
		}

		if (!vIsZero(NewMoveLocation))
		{
			if (vDist2DSq(pBot->Edict->v.origin, NewMoveLocation) < sqrf(GetPlayerRadius(pBot->Edict)))
			{
				NewMoveLocation = NewMoveLocation - (UTIL_GetVectorNormal2D(NewMoveLocation - NewMoveFromLocation) * 100.0f);
			}
		}

		return NewMoveLocation;
	}

	return ZERO_VECTOR;
}

Vector FindClosestNavigablePointToDestination(const NavAgentProfile& NavProfile, const Vector FromLocation, const Vector ToLocation, float MaxAcceptableDistance)
{
	vector<bot_path_node> Path;
	Path.clear();

	// Now we find a path backwards from the valid nav mesh point to our location, trying to get as close as we can to it

	dtStatus PathFindingResult = FindPathClosestToPoint(NavProfile, FromLocation, ToLocation, Path, MaxAcceptableDistance);

	if (dtStatusSucceed(PathFindingResult) && Path.size() > 0)
	{
		return Path.back().Location;
	}

	return ZERO_VECTOR;
}

void SkipAheadInFlightPath(AIPlayer* pBot)
{
	nav_status* BotNavInfo = &pBot->BotNavInfo;

	// Early exit if we don't have a path, or we're already on the last path point
	if (BotNavInfo->CurrentPath.size() == 0 || BotNavInfo->CurrentPathPoint >= (pBot->BotNavInfo.CurrentPath.size() - 1)) { return; }

	vector<bot_path_node>::iterator CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

	if (UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, prev(BotNavInfo->CurrentPath.end())->Location, head_hull, false))
	{
		pBot->BotNavInfo.CurrentPathPoint = (BotNavInfo->CurrentPath.size() - 1);
		return;
	}

	// If we are currently in a low area or approaching one, don't try to skip ahead in case it screws us up
	if (CurrentPathPoint->area == NAV_AREA_CROUCH || (next(CurrentPathPoint) != BotNavInfo->CurrentPath.end() && next(CurrentPathPoint)->area == NAV_AREA_CROUCH)) { return; }

	for (auto it = prev(BotNavInfo->CurrentPath.end()); it != next(CurrentPathPoint); it--)
	{
		Vector NextFlightPoint = UTIL_FindHighestSuccessfulTracePoint(pBot->Edict->v.origin, it->FromLocation, it->Location, 5.0f, 50.0f, 200.0f);

		// If we can directly reach the end point, set our path point to the end of the path and go for it
		if (!vIsZero(NextFlightPoint))
		{
			it->FromLocation = NextFlightPoint;

			pBot->BotNavInfo.CurrentPathPoint = distance(BotNavInfo->CurrentPath.begin(), prev(it));
			CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

			CurrentPathPoint->FromLocation = pBot->Edict->v.origin;
			CurrentPathPoint->Location = it->FromLocation;

			

			return;
		}
	}
}

void BotFollowFlightPath(AIPlayer* pBot, bool bAllowSkip)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		ClearBotPath(pBot);
		return;
	}

	nav_status* BotNavInfo = &pBot->BotNavInfo;
	edict_t* pEdict = pBot->Edict;

	vector<bot_path_node>::iterator CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

	Vector CurrentMoveDest = CurrentPathPoint->Location;
	Vector ClosestPointToPath = vClosestPointOnLine(CurrentPathPoint->FromLocation, CurrentMoveDest, pEdict->v.origin);
	bool bAtOrPastDestination = vEquals(ClosestPointToPath, CurrentMoveDest, 32.0f);

	// If we've reached our current path point
	if (bAtOrPastDestination)
	{
		// End of the whole path, stop all movement
		if (BotNavInfo->CurrentPathPoint >= (pBot->BotNavInfo.CurrentPath.size() - 1))
		{
			ClearBotMovement(pBot);
			return;
		}
		else
		{
			// Pick the next point in the path
			BotNavInfo->CurrentPathPoint++;
			ClearBotStuck(pBot);
			CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);
		}
	}

	if (CurrentPathPoint->flag == NAV_FLAG_PLATFORM)
	{
		PlatformMove(pBot, CurrentPathPoint->FromLocation, CurrentMoveDest, CurrentPathPoint->Platform);
		return;
	}

	if (bAllowSkip)
	{
		vector<bot_path_node>::iterator NextPathPoint = next(CurrentPathPoint);

		if (NextPathPoint != pBot->BotNavInfo.CurrentPath.end())
		{
			if (UTIL_QuickHullTrace(pBot->Edict, pBot->Edict->v.origin, NextPathPoint->Location, head_hull, false))
			{
				CurrentPathPoint->Location = pBot->Edict->v.origin;
				NextPathPoint->FromLocation = pBot->Edict->v.origin;

				BotNavInfo->CurrentPathPoint++;
				ClearBotStuck(pBot);
				CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);
			}
		}
	}

	Vector ClosestPoint = vClosestPointOnLine(CurrentPathPoint->FromLocation, CurrentPathPoint->Location, pBot->Edict->v.origin);

	if (vDist3DSq(pBot->Edict->v.origin, ClosestPoint) > sqrf(64.0f))
	{
		ClearBotPath(pBot);
		return;
	}

	if (CurrentPathPoint->flag == NAV_FLAG_WALK || CurrentPathPoint->flag == NAV_FLAG_CROUCH)
	{
		if (!UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, CurrentPathPoint->FromLocation) && !UTIL_QuickTrace(pBot->Edict, pBot->Edict->v.origin, CurrentPathPoint->Location))
		{
			ClearBotPath(pBot);
			return;
		}
	}

	CurrentMoveDest = CurrentPathPoint->Location;
	Vector MoveFrom = CurrentPathPoint->FromLocation;

	unsigned char CurrentMoveArea = CurrentPathPoint->area;
	unsigned char NextMoveArea = (next(CurrentPathPoint) != BotNavInfo->CurrentPath.end()) ? next(CurrentPathPoint)->area : CurrentMoveArea;

	Vector StartTrace = pBot->Edict->v.origin;
	Vector EndTrace = StartTrace + (UTIL_GetVectorNormal2D(pBot->Edict->v.angles) * 32.0f);

	edict_t* BlockingEdict = UTIL_TraceEntityHull(pBot->Edict, StartTrace, EndTrace);

	// If we're bumping into a player, rise above them
	if (!FNullEnt(BlockingEdict) && IsEdictPlayer(BlockingEdict))
	{
		pBot->Edict->v.velocity.z = 150.0f;
	}

	CheckAndHandleBreakableObstruction(pBot, MoveFrom, CurrentMoveDest, NAV_FLAG_WALK);

	BotMovementInputs(pBot);

}

void BotFollowSwimPath(AIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		ClearBotPath(pBot);
		return;
	}

	nav_status* BotNavInfo = &pBot->BotNavInfo;
	edict_t* pEdict = pBot->Edict;

	vector<bot_path_node>::iterator CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

	// If we've reached our current path point
	if (vPointOverlaps3D(CurrentPathPoint->Location, pBot->Edict->v.absmin, pBot->Edict->v.absmax))
	{
		ClearBotStuck(pBot);

		pBot->BotNavInfo.CurrentPathPoint++;

		// No more path points, we've reached the end of our path
		if (pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
		{
			ClearBotPath(pBot);
			return;
		}
		else
		{
			CurrentPathPoint = (BotNavInfo->CurrentPath.begin() + BotNavInfo->CurrentPathPoint);

			if (CurrentPathPoint->flag == NAV_FLAG_WALK || CurrentPathPoint->flag == NAV_FLAG_CROUCH)
			{
				CurrentPathPoint->FromLocation = pBot->Edict->v.origin;
			}
		}
	}

	bool TargetPointIsInWater = NAV_IsPointInSwimArea(CurrentPathPoint->Location);

	bool bHasNextPoint = (next(CurrentPathPoint) != BotNavInfo->CurrentPath.end());
	bool NextPointInWater = TargetPointIsInWater;

	bool bShouldSurface = (bHasNextPoint && !NextPointInWater && vDist2DSq(pEdict->v.origin, next(CurrentPathPoint)->FromLocation) < sqrf(100.0f));

	if (TargetPointIsInWater && !bShouldSurface)
	{
		BotMoveLookAt(pBot, CurrentPathPoint->Location);
		pBot->desiredMovementDir = UTIL_GetVectorNormal2D(CurrentPathPoint->Location - pEdict->v.origin);

		unsigned char NextArea = (next(CurrentPathPoint) != BotNavInfo->CurrentPath.end()) ? next(CurrentPathPoint)->area : NAV_AREA_WALK;

		if (CurrentPathPoint->area == NAV_AREA_CROUCH || (NextArea == NAV_AREA_CROUCH && vDist2DSq(pEdict->v.origin, next(CurrentPathPoint)->FromLocation) < sqrf(50.0f)))
		{
			pBot->Button |= IN_DUCK;
		}

		return;
	}

	if (CurrentPathPoint->flag == NAV_FLAG_LADDER)
	{
		if (IsPlayerOnLadder(pBot->Edict))
		{
			BotFollowPath(pBot);
			return;
		}
		else
		{
			edict_t* MountLadder = UTIL_GetNearestLadderAtPoint(CurrentPathPoint->FromLocation);

			if (!FNullEnt(MountLadder))
			{
				Vector LadderMountPoint = GetLadderMountPoint(MountLadder, CurrentPathPoint->Location);
				Vector LadderNormal = UTIL_GetVectorNormal2D(LadderMountPoint - UTIL_GetCentreOfEntity(MountLadder));
				LadderMountPoint = LadderMountPoint + (LadderNormal * 48.0f);

				if (pBot->Edict->v.origin.z >= MountLadder->v.absmin.z + 16.0f)
				{
					Vector ClosestPointOnLadder = UTIL_GetClosestPointOnEntityToLocation(LadderMountPoint, MountLadder);
					Vector MountAngle = UTIL_GetVectorNormal2D(ClosestPointOnLadder - LadderMountPoint);
					Vector BotAngle = UTIL_GetVectorNormal2D(ClosestPointOnLadder - pBot->Edict->v.origin);

					float Dot = UTIL_GetDotProduct2D(MountAngle, BotAngle);

					if (Dot > 0.9f)
					{
						LadderMountPoint = ClosestPointOnLadder;
					}


				}

				BotMoveLookAt(pBot, LadderMountPoint);
				pBot->desiredMovementDir = UTIL_GetForwardVector2D(pBot->Edict->v.v_angle);

				return;
			}
		}
	}

	float WaterLevel = UTIL_WaterLevel(pEdict->v.origin, pEdict->v.origin.z, pEdict->v.origin.z + 500.0f);

	float WaterDiff = WaterLevel - pEdict->v.origin.z;

	// If we're below the waterline by a significant amount, then swim up to surface before we move on
	if (WaterDiff > 5.0f)
	{
		Vector MoveDir = UTIL_GetVectorNormal2D(CurrentPathPoint->Location - pEdict->v.origin);
		pBot->desiredMovementDir = MoveDir;

		if (WaterDiff > 10.0f)
		{
			BotMoveLookAt(pBot, pEdict->v.origin + (MoveDir * 5.0f) + Vector(0.0f, 0.0f, 100.0f));
		}
		else
		{
			BotMoveLookAt(pBot, pBot->CurrentEyePosition + (MoveDir * 50.0f) + Vector(0.0f, 0.0f, 50.0f));
		}

		return;
	}

	// We're at the surface, now tackle the path the usual way
	if (pBot->BotNavInfo.NavProfile.bFlyingProfile)
	{
		BotFollowFlightPath(pBot, true);
	}
	else
	{
		BotFollowPath(pBot);
	}
}

void BotFollowPath(AIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
	{
		ClearBotPath(pBot);
		return;
	}

	nav_status* BotNavInfo = &pBot->BotNavInfo;
	edict_t* pEdict = pBot->Edict;


	// If we've reached our current path point
	if (HasBotReachedPathPoint(pBot))
	{
		ClearBotStuck(pBot);

		pBot->BotNavInfo.CurrentPathPoint++;

		// No more path points, we've reached the end of our path
		if (pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size())
		{
			ClearBotPath(pBot);
			return;
		}
	}

	if (pBot->Edict->v.flags & FL_INWATER)
	{
		TraceResult Hit;

		for (int i = pBot->BotNavInfo.CurrentPathPoint + 1; i < pBot->BotNavInfo.CurrentPath.size(); i++)
		{
			if (!NAV_IsPointInSwimArea(pBot->BotNavInfo.CurrentPath[i].Location)) { break; }

			UTIL_TraceHull(pBot->Edict->v.origin, pBot->BotNavInfo.CurrentPath[i].Location, ignore_monsters, head_hull, nullptr, &Hit);

			if (!Hit.fAllSolid && !Hit.fStartSolid && Hit.flFraction >= 1.0f)
			{
				pBot->BotNavInfo.CurrentPathPoint = i;
				pBot->BotNavInfo.CurrentPath[i].FromLocation = pBot->Edict->v.origin;
			}
		}

	}

	bot_path_node CurrentNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	if (IsPlayerStandingOnPlayer(pBot->Edict) && CurrentNode.flag != NAV_FLAG_LADDER)
	{
		if (pBot->Edict->v.groundentity->v.velocity.Length2D() > 10.0f)
		{
			pBot->desiredMovementDir = UTIL_GetVectorNormal2D(-pBot->Edict->v.groundentity->v.velocity);
			return;
		}
		MoveToWithoutNav(pBot, CurrentNode.Location);
		return;
	}

	vector<edict_t*> PotentialRiders = AITAC_GetAllPlayersOfTeamInArea(pBot->Edict->v.team, pBot->Edict->v.origin, pBot->Edict->v.size.Length(), pBot->Edict);

	for (auto it = PotentialRiders.begin(); it != PotentialRiders.end(); it++)
	{
		if ((*it)->v.groundentity == pBot->Edict)
		{
			if (vDist2DSq(pBot->Edict->v.origin, CurrentNode.FromLocation) > sqrf(GetPlayerRadius(pBot->Edict)))
			{
				MoveToWithoutNav(pBot, CurrentNode.FromLocation);
				return;
			}
			else
			{
				if (pBot->BotNavInfo.CurrentPathPoint > 0)
				{
					bot_path_node PrevNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint - 1];
					MoveToWithoutNav(pBot, PrevNode.FromLocation);
					return;
				}
			}
		}
	}

	bool bIsOffPath = false;

	if (pBot->Edict->v.flags & FL_INWATER)
	{
		bIsOffPath = IsBotOffSwimPath(pBot);
	}
	else if (pBot->BotNavInfo.NavProfile.bFlyingProfile)
	{
		bIsOffPath = IsBotOffFlightPath(pBot);
	}
	else
	{
		bIsOffPath = IsBotOffPath(pBot);
	}

	if (bIsOffPath)
	{
		pBot->BotNavInfo.StuckInfo.bPathFollowFailed = true;
		ClearBotPath(pBot);
		return;
	}

	pBot->BotNavInfo.StuckInfo.bPathFollowFailed = false;

	bot_path_node CurrentPathNode = pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint];

	// HERE! Check if trigger is reachable from where the lift is now! It's currently messing up the button floor position
	if (!(CurrentPathNode.flag & NAV_FLAG_PLATFORM))
	{
		for (int i = pBot->BotNavInfo.CurrentPathPoint; i < pBot->BotNavInfo.CurrentPath.size(); i++)
		{
			bot_path_node FutureNode = pBot->BotNavInfo.CurrentPath[i];

			if (FutureNode.flag & NAV_FLAG_PLATFORM)
			{
				DynamicMapObject* PlatformObject = UTIL_GetDynamicObjectByEdict(FutureNode.Platform);

				if (PlatformObject && NAV_PlatformNeedsActivating(pBot, PlatformObject, FutureNode.FromLocation, FutureNode.Location))
				{
					DynamicMapObjectStop DesiredStartStop, DesiredEndStop;
					GetDesiredPlatformStartAndEnd(PlatformObject, FutureNode.FromLocation, FutureNode.Location, DesiredStartStop, DesiredEndStop);

					DynamicMapObject* Trigger = nullptr;

					if (vEquals(UTIL_GetCentreOfEntity(PlatformObject->Edict), DesiredStartStop.StopLocation, 5.0f))
					{
						Trigger = NAV_GetTriggerReachableFromPlatform(FutureNode.FromLocation.z + 32.0f, PlatformObject);
					}

					if (!Trigger)
					{
						Trigger = NAV_GetBestTriggerForObject(PlatformObject, pBot->Edict, pBot->BotNavInfo.NavProfile);

						if (Trigger)
						{
							if (PlatformObject->State == OBJECTSTATE_IDLE)
							{
								NAV_AddUseMovementTask(pBot, Trigger->Edict, Trigger);
							}
							else
							{
								// Where we will stand to activate this trigger
								const Vector ButtonFloorLoc = UTIL_GetButtonFloorLocation(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, Trigger->Edict);

								// Make sure the button we want to use isn't underneath the lift. We don't want to get crushed!
								const Vector HalfSize = PlatformObject->Edict->v.size * 0.5f;
						
								bool bWillBeCrushedByPlatform = vPointOverlaps2D(ButtonFloorLoc, DesiredStartStop.StopLocation - HalfSize, DesiredStartStop.StopLocation + HalfSize);

								if (bWillBeCrushedByPlatform)
								{
									NAV_AddMoveMovementTask(pBot, FutureNode.FromLocation, nullptr);
								}
								else
								{
									NAV_AddMoveMovementTask(pBot, ButtonFloorLoc, nullptr);
								}								
							}
							return;
						}
					}
				}
			}
			else
			{
				DynamicMapObject* BlockingObject = UTIL_GetObjectBlockingPathPoint(&FutureNode, nullptr, nullptr);

				if (BlockingObject && BlockingObject->State == OBJECTSTATE_IDLE)
				{
					DynamicMapObject* Trigger = NAV_GetBestTriggerForObject(BlockingObject, FutureNode.FromLocation, pBot->BotNavInfo.NavProfile);

					if (Trigger)
					{
						NAV_AddTriggerMovementTask(pBot, Trigger, BlockingObject);
						return;
					}
				}
			}
		}
	}

	Vector MoveTo = CurrentNode.Location;

	if (pBot->Edict->v.flags & FL_INWATER)
	{
		NewSwimMove(pBot);
	}
	else if (pBot->BotNavInfo.NavProfile.bFlyingProfile)
	{
		NewFlightMove(pBot);
	}
	else
	{
		NewMove(pBot);
	}
}

void PerformUnstuckMove(AIPlayer* pBot, const Vector MoveDestination)
{

	Vector FwdDir = UTIL_GetVectorNormal2D(MoveDestination - pBot->Edict->v.origin);
	pBot->desiredMovementDir = FwdDir;

	Vector HeadLocation = GetPlayerTopOfCollisionHull(pBot->Edict, false);

	bool bMustCrouch = false;

	if (CanPlayerCrouch(pBot->Edict) && !UTIL_QuickTrace(pBot->Edict, HeadLocation, (HeadLocation + (FwdDir * 50.0f))))
	{
		pBot->Button |= IN_DUCK;
		bMustCrouch = true;
	}

	Vector MoveRightVector = UTIL_GetVectorNormal2D(UTIL_GetCrossProduct(FwdDir, UP_VECTOR));

	Vector BotRightSide = (pBot->Edict->v.origin + (MoveRightVector * GetPlayerRadius(pBot->Edict)));
	Vector BotLeftSide = (pBot->Edict->v.origin - (MoveRightVector * GetPlayerRadius(pBot->Edict)));

	bool bBlockedLeftSide = !UTIL_QuickTrace(pBot->Edict, BotRightSide, BotRightSide + (FwdDir * 50.0f));
	bool bBlockedRightSide = !UTIL_QuickTrace(pBot->Edict, BotLeftSide, BotLeftSide + (FwdDir * 50.0f));

	if (!bMustCrouch)
	{
		BotJump(pBot);
	}


	if (bBlockedRightSide && !bBlockedLeftSide)
	{
		pBot->desiredMovementDir = MoveRightVector;
		return;
	}
	else if (!bBlockedRightSide && bBlockedLeftSide)
	{
		pBot->desiredMovementDir = -MoveRightVector;
		return;
	}
	else
	{
		bBlockedLeftSide = !UTIL_QuickTrace(pBot->Edict, BotRightSide, BotRightSide - (MoveRightVector * 50.0f));
		bBlockedRightSide = !UTIL_QuickTrace(pBot->Edict, BotLeftSide, BotLeftSide + (MoveRightVector * 50.0f));

		if (bBlockedRightSide)
		{
			pBot->desiredMovementDir = -MoveRightVector;
		}
		else if (bBlockedLeftSide)
		{
			pBot->desiredMovementDir = MoveRightVector;
		}
		else
		{
			pBot->desiredMovementDir = FwdDir;
		}

	}

}

bool BotIsAtLocation(const AIPlayer* pBot, const Vector Destination)
{
	if (vIsZero(Destination) || !(pBot->Edict->v.flags & FL_ONGROUND)) { return false; }

	return (vDist2DSq(pBot->Edict->v.origin, Destination) < sqrf(GetPlayerRadius(pBot->Edict)) && fabs(pBot->CurrentFloorPosition.z - Destination.z) <= GetPlayerHeight(pBot->Edict, false));
}

Vector UTIL_ProjectPointToNavmesh(const int NavmeshIndex, const Vector Location, const NavAgentProfile& NavProfile, const Vector Extents)
{
	if (vIsZero(Location) || NavmeshIndex >= NUM_NAV_MESHES) { return ZERO_VECTOR; }

	nav_mesh ChosenNavMesh = NavMeshes[NavmeshIndex];

	const dtNavMeshQuery* m_navQuery = ChosenNavMesh.navQuery;
	const dtNavMesh* m_navMesh = ChosenNavMesh.navMesh;
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery || !m_navMesh) { return ZERO_VECTOR; }

	Vector PointToProject = Location;

	float pCheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, Extents, m_navFilter, &FoundPoly, NavNearest);

	if (FoundPoly > 0 && dtStatusSucceed(success))
	{
		return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
	}
	else
	{
		int PointContents = UTIL_PointContents(PointToProject);

		if (PointContents != CONTENTS_SOLID && PointContents != CONTENTS_LADDER)
		{
			Vector TraceHit = UTIL_GetTraceHitLocation(PointToProject + Vector(0.0f, 0.0f, 1.0f), PointToProject - Vector(0.0f, 0.0f, 1000.0f));

			PointToProject = (!vIsZero(TraceHit)) ? TraceHit : Location;
		}

		float pRecheckLoc[3] = { PointToProject.x, PointToProject.z, -PointToProject.y };

		dtStatus successRetry = m_navQuery->findNearestPoly(pRecheckLoc, Extents, m_navFilter, &FoundPoly, NavNearest);

		if (FoundPoly > 0 && dtStatusSucceed(success))
		{
			return Vector(NavNearest[0], -NavNearest[2], NavNearest[1]);
		}
		else
		{
			return ZERO_VECTOR;
		}
	}

	return ZERO_VECTOR;
}

bool UTIL_PointIsOnNavmesh(const Vector Location, const NavAgentProfile &NavProfile, const Vector SearchExtents)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return false; }

	float pCheckLoc[3] = { Location.x, Location.z, -Location.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	float pCheckExtents[3] = { SearchExtents.x, SearchExtents.z, SearchExtents.y };

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, pCheckExtents, m_navFilter, &FoundPoly, NavNearest);

	return dtStatusSucceed(success) && FoundPoly > 0;

}

bool UTIL_PointIsOnNavmesh(const NavAgentProfile& NavProfile, const Vector Location, const Vector SearchExtents)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery) { return false; }

	float pCheckLoc[3] = { Location.x, Location.z, -Location.y };

	dtPolyRef FoundPoly;
	float NavNearest[3];

	float pCheckExtents[3] = { SearchExtents.x, SearchExtents.z, SearchExtents.y };

	dtStatus success = m_navQuery->findNearestPoly(pCheckLoc, pCheckExtents, m_navFilter, &FoundPoly, NavNearest);

	return dtStatusSucceed(success) && FoundPoly > 0;

}

void HandlePlayerAvoidance(AIPlayer* pBot, const Vector MoveDestination)
{
	// Don't handle player avoidance if climbing a wall, ladder or in the air, as it will mess up the move and cause them to get stuck most likely
	if (IsPlayerOnLadder(pBot->Edict) || !pBot->BotNavInfo.IsOnGround) { return; }

	float MyRadius = GetPlayerRadius(pBot->Edict);
		
	const Vector BotLocation = pBot->Edict->v.origin;
	const Vector MoveDir = UTIL_GetVectorNormal2D((MoveDestination - pBot->Edict->v.origin));

	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t* OtherPlayer = INDEXENT(i);

		if (!FNullEnt(OtherPlayer) && OtherPlayer != pBot->Edict && IsPlayerActiveInGame(OtherPlayer))
		{
			float OtherPlayerRadius = GetPlayerRadius(OtherPlayer);

			float avoidDistSq = sqrf(MyRadius + OtherPlayerRadius + 16.0f);

			// Don't do avoidance for a player if they're moving in broadly the same direction as us
			Vector OtherMoveDir = GetPlayerAttemptedMoveDirection(OtherPlayer);

			if (vDist3DSq(BotLocation, OtherPlayer->v.origin) <= avoidDistSq)
			{
				Vector BlockAngle = UTIL_GetVectorNormal2D(OtherPlayer->v.origin - BotLocation);
				float MoveBlockDot = UTIL_GetDotProduct2D(MoveDir, BlockAngle);

				// If other player is between us and our destination
				if (MoveBlockDot > 0.0f)
				{
					// If the other player is in the air or on top of us, back up and let them land
					if (!(OtherPlayer->v.flags & FL_ONGROUND) || OtherPlayer->v.groundentity == pBot->Edict)
					{
						pBot->desiredMovementDir = UTIL_GetVectorNormal2D(BotLocation - OtherPlayer->v.origin);
						return;
					}

					// Determine if we should move left or right to clear them
					Vector MoveRightVector = UTIL_GetCrossProduct(MoveDir, UP_VECTOR);

					int modifier = vPointOnLine(pBot->Edict->v.origin, MoveDestination, OtherPlayer->v.origin);

					float OtherPersonDistFromLine = vDistanceFromLine2D(pBot->Edict->v.origin, MoveDestination, OtherPlayer->v.origin);

					if (modifier == 0) { modifier = 1; }

					Vector PreferredMoveDir = (MoveRightVector * modifier);

					float TraceLength = OtherPersonDistFromLine + (fmaxf(MyRadius, OtherPlayerRadius) * 2.0f);


					// First see if we have enough room to move in our preferred avoidance direction
					if (UTIL_TraceNav(pBot->BotNavInfo.NavProfile, BotLocation, BotLocation + (PreferredMoveDir * TraceLength), 0.0f))
					{
						pBot->desiredMovementDir = PreferredMoveDir;
						return;
					}

					// Then try the opposite direction
					if (UTIL_TraceNav(pBot->BotNavInfo.NavProfile, BotLocation, BotLocation - (PreferredMoveDir * TraceLength), 0.0f))
					{
						pBot->desiredMovementDir = -PreferredMoveDir;
						return;
					}

					// If we have a point we can go back to, and we can reach it, then go for it. Otherwise, keep pushing on and hope the other guy moves
					if (!vIsZero(pBot->BotNavInfo.LastOpenLocation))
					{
						if (UTIL_PointIsReachable(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, pBot->BotNavInfo.LastOpenLocation, GetPlayerRadius(pBot->Edict)))
						{
							pBot->BotNavInfo.UnstuckTask.TaskType = MOVE_TASK_MOVE;
							pBot->BotNavInfo.UnstuckTask.TaskLocation = pBot->BotNavInfo.LastOpenLocation;
							return;
						}
					}
				}
			}
		}
	}
}

float UTIL_GetPathCostBetweenLocations(const NavAgentProfile &NavProfile , const Vector FromLocation, const Vector ToLocation)
{
	vector<bot_path_node> path;
	path.clear();

	dtStatus pathFindResult = FindPathClosestToPoint(NavProfile, FromLocation, ToLocation, path, max_ai_use_reach);

	if (!dtStatusSucceed(pathFindResult)) { return 0.0f; }

	int currPathPoint = 1;
	float result = 0.0f;

	for (auto it = path.begin(); it != path.end(); it++)
	{
		result += vDist2DSq(it->FromLocation, it->Location) * NavProfile.Filters.getAreaCost(it->area);
	}

	return sqrtf(result);
}

void ClearBotMovement(AIPlayer* pBot)
{
	pBot->BotNavInfo.TargetDestination = ZERO_VECTOR;
	pBot->BotNavInfo.ActualMoveDestination = ZERO_VECTOR;

	ClearBotPath(pBot);
	ClearBotStuck(pBot);
	ClearBotStuckMovement(pBot);

}

void ClearBotStuck(AIPlayer* pBot)
{
	pBot->BotNavInfo.LastDistanceFromDestination = 0.0f;
	pBot->BotNavInfo.LastStuckCheckTime = gpGlobals->time;
	pBot->BotNavInfo.TotalStuckTime = 0.0f;
	pBot->BotNavInfo.UnstuckMoveLocation = ZERO_VECTOR;
	pBot->BotNavInfo.StuckCheckMoveLocation = ZERO_VECTOR;
}

bool BotRecalcPath(AIPlayer* pBot, const Vector Destination)
{
	ClearBotPath(pBot);

	Vector ValidNavmeshPoint = UTIL_ProjectPointToNavmesh(pBot->BotNavInfo.NavProfile.NavMeshIndex, Destination, pBot->BotNavInfo.NavProfile, Vector(max_ai_use_reach, max_ai_use_reach, max_ai_use_reach));

	// We can't actually get close enough to this point to consider it "reachable"
	if (vIsZero(ValidNavmeshPoint))
	{
		return false;
	}

	dtStatus FoundPath = FindPathClosestToPoint(pBot, pBot->BotNavInfo.MoveStyle, ValidNavmeshPoint, pBot->BotNavInfo.CurrentPath, max_ai_use_reach);

	if (dtStatusSucceed(FoundPath) && pBot->BotNavInfo.CurrentPath.size() > 0)
	{
		pBot->BotNavInfo.TargetDestination = Destination;
		pBot->BotNavInfo.ActualMoveDestination = pBot->BotNavInfo.CurrentPath.back().Location;

		if (next(pBot->BotNavInfo.CurrentPath.begin()) == pBot->BotNavInfo.CurrentPath.end() || vDist2DSq(pBot->BotNavInfo.CurrentPath.front().Location, pBot->Edict->v.origin) > sqrf(GetPlayerRadius(pBot->Edict)))
		{
			pBot->BotNavInfo.CurrentPathPoint = 0;
		}
		else
		{
			pBot->BotNavInfo.CurrentPathPoint = 1;
		}


		return true;
	}

	return false;
}

float UTIL_FindZHeightForWallClimb(const Vector ClimbStart, const Vector ClimbEnd, const int HullNum)
{
	TraceResult hit;

	Vector StartTrace = ClimbEnd;

	UTIL_TraceLine(ClimbEnd, ClimbEnd - Vector(0.0f, 0.0f, 50.0f), ignore_monsters, nullptr, &hit);

	if (hit.fAllSolid || hit.fStartSolid || hit.flFraction < 1.0f)
	{
		StartTrace.z = hit.vecEndPos.z + 18.0f;
	}

	Vector EndTrace = ClimbStart;
	EndTrace.z = StartTrace.z;

	Vector CurrTraceStart = StartTrace;

	UTIL_TraceHull(StartTrace, EndTrace, ignore_monsters, HullNum, nullptr, &hit);

	if (hit.flFraction >= 1.0f && !hit.fAllSolid && !hit.fStartSolid)
	{
		return StartTrace.z;
	}
	else
	{
		int maxTests = 100;
		int testCount = 0;

		while ((hit.flFraction < 1.0f || hit.fStartSolid || hit.fAllSolid) && testCount < maxTests)
		{
			CurrTraceStart.z += 1.0f;
			EndTrace.z = CurrTraceStart.z;
			UTIL_TraceHull(CurrTraceStart, EndTrace, ignore_monsters, HullNum, nullptr, &hit);
			testCount++;
		}

		if (hit.flFraction >= 1.0f && !hit.fStartSolid)
		{
			return CurrTraceStart.z;
		}
		else
		{
			return StartTrace.z;
		}
	}

	return StartTrace.z;
}

void ClearBotPath(AIPlayer* pBot)
{
	pBot->BotNavInfo.CurrentPath.clear();
	pBot->BotNavInfo.CurrentPathPoint = 0;

	pBot->BotNavInfo.SpecialMovementFlags = 0;

	pBot->BotNavInfo.bNavProfileChanged = false;

	pBot->BotNavInfo.TargetDestination = ZERO_VECTOR;
	pBot->BotNavInfo.PathDestination = ZERO_VECTOR;

	pBot->BotNavInfo.MovementTasks.clear();
}

void ClearBotStuckMovement(AIPlayer* pBot)
{
	pBot->BotNavInfo.UnstuckMoveLocation = ZERO_VECTOR;
}

float GetDesiredBotMovementSpeed(AIPlayer* pBot)
{
	const float MaxSpeed = pBot->Edict->v.maxspeed;
	return (pBot->BotNavInfo.bShouldWalk) ? MaxSpeed * 0.4f : MaxSpeed;
}

void BotMovementInputs(AIPlayer* pBot)
{
	if (vIsZero(pBot->desiredMovementDir)) { return; }

	edict_t* pEdict = pBot->Edict;

	UTIL_NormalizeVector2D(&pBot->desiredMovementDir);

	float currentYaw = pBot->Edict->v.v_angle.y;
	float moveDelta = UTIL_VecToAngles(pBot->desiredMovementDir).y;
	float angleDelta = currentYaw - moveDelta;

	float botSpeed = GetDesiredBotMovementSpeed(pBot);

	if (pBot->BotNavInfo.bShouldWalk)
	{
		pBot->Button |= IN_RUN;
	}

	if (angleDelta < -180.0f)
	{
		angleDelta += 360.0f;
	}
	else if (angleDelta > 180.0f)
	{
		angleDelta -= 360.0f;
	}

	if (angleDelta >= -22.5f && angleDelta < 22.5f)
	{
		pBot->ForwardMove = botSpeed;
		pBot->SideMove = 0.0f;
		pBot->Button |= IN_FORWARD;
	}
	else if (angleDelta >= 22.5f && angleDelta < 67.5f)
	{
		pBot->ForwardMove = botSpeed;
		pBot->SideMove = botSpeed;
		pBot->Button |= IN_FORWARD;
		pBot->Button |= IN_MOVERIGHT;
	}
	else if (angleDelta >= 67.5f && angleDelta < 112.5f)
	{
		pBot->ForwardMove = 0.0f;
		pBot->SideMove = botSpeed;
		pBot->Button |= IN_MOVERIGHT;
	}
	else if (angleDelta >= 112.5f && angleDelta < 157.5f)
	{
		pBot->ForwardMove = -botSpeed;
		pBot->SideMove = botSpeed;
		pBot->Button |= IN_BACK;
		pBot->Button |= IN_MOVERIGHT;
	}
	else if (angleDelta >= 157.5f || angleDelta <= -157.5f)
	{
		pBot->ForwardMove = -botSpeed;
		pBot->SideMove = 0.0f;
		pBot->Button |= IN_BACK;
	}
	else if (angleDelta >= -157.5f && angleDelta < -112.5f)
	{
		pBot->ForwardMove = -botSpeed;
		pBot->SideMove = -botSpeed;
		pBot->Button |= IN_BACK;
		pBot->Button |= IN_MOVELEFT;
	}
	else if (angleDelta >= -112.5f && angleDelta < -67.5f)
	{
		pBot->ForwardMove = 0.0f;
		pBot->SideMove = -botSpeed;
		pBot->Button |= IN_MOVELEFT;
	}
	else if (angleDelta >= -67.5f && angleDelta < -22.5f)
	{
		pBot->ForwardMove = botSpeed;
		pBot->SideMove = -botSpeed;
		pBot->Button |= IN_FORWARD;
		pBot->Button |= IN_MOVELEFT;
	}

	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size() || pBot->BotNavInfo.CurrentPath[pBot->BotNavInfo.CurrentPathPoint].flag != NAV_FLAG_LADDER)
	{
		if (IsPlayerOnLadder(pBot->Edict))
		{
			BotJump(pBot);
		}
	}
}

void OnBotStartLadder(AIPlayer* pBot)
{

}

void OnBotEndLadder(AIPlayer* pBot)
{

}

Vector UTIL_GetFurthestVisiblePointOnPath(const AIPlayer* pBot)
{
	if (pBot->BotNavInfo.CurrentPath.size() == 0 || pBot->BotNavInfo.CurrentPathPoint >= pBot->BotNavInfo.CurrentPath.size()) { return ZERO_VECTOR; }

	vector<bot_path_node>::const_iterator CurrentPathPoint = (pBot->BotNavInfo.CurrentPath.begin() + pBot->BotNavInfo.CurrentPathPoint);

	if (CurrentPathPoint == prev(pBot->BotNavInfo.CurrentPath.end()))
	{
		Vector MoveDir = UTIL_GetVectorNormal2D(CurrentPathPoint->Location - pBot->Edict->v.origin);
		return CurrentPathPoint->Location + (MoveDir * 300.0f);
	}

	Vector FurthestVisiblePoint = CurrentPathPoint->Location;
	FurthestVisiblePoint.z = pBot->CurrentEyePosition.z;

	for (auto it = next(CurrentPathPoint); it != pBot->BotNavInfo.CurrentPath.end(); it++)
	{
		Vector CheckPoint = it->Location + Vector(0.0f, 0.0f, 32.0f);

		if (UTIL_QuickTrace(pBot->Edict, pBot->CurrentEyePosition, CheckPoint))
		{
			FurthestVisiblePoint = CheckPoint;
		}
		else
		{
			break;
		}
	}

	return FurthestVisiblePoint;
}

Vector UTIL_GetFurthestVisiblePointOnLineWithHull(const Vector ViewerLocation, const Vector LineStart, const Vector LineEnd, int HullNumber)
{
	Vector Dir = UTIL_GetVectorNormal(LineEnd - LineStart);

	float Dist = vDist3D(LineStart, LineEnd);
	int Steps = (int)floorf(Dist / 10.0f);

	if (Steps == 0) { return ZERO_VECTOR; }

	Vector FinalView = ZERO_VECTOR;
	Vector ThisView = LineStart;

	for (int i = 0; i < Steps; i++)
	{
		if (UTIL_QuickHullTrace(NULL, ViewerLocation, ThisView, HullNumber))
		{
			FinalView = ThisView;
		}

		ThisView = ThisView + (Dir * 10.0f);
	}

	return FinalView;
}

Vector UTIL_GetFurthestVisiblePointOnPath(const Vector ViewerLocation, vector<bot_path_node>& path, bool bPrecise)
{
	if (path.size() == 0) { return ZERO_VECTOR; }

	for (auto it = path.rbegin(); it != path.rend(); it++)
	{

		if (UTIL_QuickTrace(NULL, ViewerLocation, it->Location))
		{
			if (!bPrecise || it == path.rbegin())
			{
				return it->Location;
			}
			else
			{
				Vector FromLoc = it->Location;
				Vector ToLoc = prev(it)->Location;

				Vector Dir = UTIL_GetVectorNormal(ToLoc - FromLoc);

				float Dist = vDist3D(FromLoc, ToLoc);
				int Steps = (int)floorf(Dist / 50.0f);

				if (Steps == 0) { return FromLoc; }

				Vector FinalView = FromLoc;
				Vector ThisView = FromLoc + (Dir * 50.0f);

				for (int i = 0; i < Steps; i++)
				{
					if (UTIL_QuickTrace(NULL, ViewerLocation, ThisView))
					{
						FinalView = ThisView;
					}

					ThisView = ThisView + (Dir * 50.0f);
				}

				return FinalView;
			}
		}
		else
		{
			if (bPrecise && it != path.rbegin())
			{
				Vector FromLoc = it->Location;
				Vector ToLoc = prev(it)->Location;

				Vector Dir = UTIL_GetVectorNormal(ToLoc - FromLoc);

				float Dist = vDist3D(FromLoc, ToLoc);
				int Steps = (int)floorf(Dist / 50.0f);

				if (Steps == 0) { continue; }

				Vector FinalView = ZERO_VECTOR;
				Vector ThisView = FromLoc + (Dir * 50.0f);

				for (int i = 0; i < Steps; i++)
				{
					if (UTIL_QuickTrace(NULL, ViewerLocation, ThisView))
					{
						FinalView = ThisView;
					}

					ThisView = ThisView + (Dir * 50.0f);
				}

				if (FinalView != ZERO_VECTOR)
				{
					return FinalView;
				}
				
			}
		}
	}

	return ZERO_VECTOR;
}

Vector UTIL_GetButtonFloorLocation(const NavAgentProfile& NavProfile, const Vector UserLocation, edict_t* ButtonEdict)
{
	if (NAV_IsPointInSwimArea(UserLocation))
	{
		Vector NearestTriggerPoint = UTIL_GetClosestPointOnEntityToLocation(UserLocation, ButtonEdict);

		TraceResult Hit;

		UTIL_TraceHull(UserLocation, NearestTriggerPoint, ignore_monsters, head_hull, nullptr, &Hit);

		if (Hit.fInWater)
		{
			if (vDist3DSq(NearestTriggerPoint, Hit.vecEndPos) < sqrf(max_player_use_reach)) { return Hit.vecEndPos; }
		}
	}

	Vector ClosestPoint = ZERO_VECTOR;

	if (ButtonEdict->v.size.x > 64.0f || ButtonEdict->v.size.y > 64.0f)
	{
		ClosestPoint = UTIL_GetClosestPointOnEntityToLocation(UserLocation, ButtonEdict);
	}
	else
	{
		ClosestPoint = UTIL_GetCentreOfEntity(ButtonEdict);
	}

	if (NAV_IsPointInSwimArea(ClosestPoint))
	{
		return ClosestPoint;
	}

	Vector ButtonAccessPoint = UTIL_ProjectPointToNavmesh(NavProfile.NavMeshIndex, ClosestPoint, NavProfile, Vector(100.0f, 100.0f, 100.0f));

	if (vIsZero(ButtonAccessPoint))
	{
		ButtonAccessPoint = ClosestPoint;
	}

	Vector PlayerAccessLoc = ButtonAccessPoint;

	if (ButtonAccessPoint.z > ClosestPoint.z)
	{
		PlayerAccessLoc.z += 18.0f;
	}
	else
	{
		PlayerAccessLoc.z += 36.0f;
	}

	if (fabsf(PlayerAccessLoc.z - ClosestPoint.z) <= max_player_use_reach)
	{
		return ButtonAccessPoint;
	}

	Vector NewProjection = ClosestPoint;

	if (ButtonAccessPoint.z > ClosestPoint.z)
	{
		NewProjection = ClosestPoint - Vector(0.0f, 0.0f, 100.0f);
	}
	else
	{
		NewProjection = ClosestPoint + Vector(0.0f, 0.0f, 100.0f);
	}

	Vector NewButtonAccessPoint = UTIL_ProjectPointToNavmesh(NavProfile.NavMeshIndex, NewProjection, NavProfile);

	if (vIsZero(NewButtonAccessPoint))
	{
		NewButtonAccessPoint = ClosestPoint;
	}
	else
	{
		if (NAV_IsPointInSwimArea(NewButtonAccessPoint))
		{
			Vector NewClosestPoint = NewButtonAccessPoint + ((ClosestPoint - NewButtonAccessPoint) * 0.95f);

			if (NAV_IsPointInSwimArea(NewClosestPoint))
			{
				NewButtonAccessPoint = NewClosestPoint;
			}
		}
	}

	return NewButtonAccessPoint;
}

bool NAV_IsPointInSwimArea(const Vector& Point)
{
	return UTIL_PointContents(Point) == CONTENTS_WATER
		|| UTIL_PointContents(Point) == CONTENTS_SLIME
		|| UTIL_PointContents(Point) == CONTENTS_LAVA;
}

void NAV_PopulateConnectionsAffectedByDynamicObject(DynamicMapObject* Object)
{

	Vector HalfExtents = (Object->Edict->v.size * 0.5f);
	HalfExtents.x += 16.0f;
	HalfExtents.y += 16.0f;
	HalfExtents.z += 16.0f;

	for (int i = 0; i < NUM_NAV_MESHES; i++)
	{
		if (!NavMeshes[i].tileCache) { continue; }

		for (auto stopIt = Object->StopPoints.begin(); stopIt != Object->StopPoints.end(); stopIt++)
		{
			stopIt->AffectedConnections.clear();

			for (auto it = NavMeshes[i].MeshConnections.begin(); it != NavMeshes[i].MeshConnections.end(); it++)
			{
				if (IsFlagTeleportType((NavMovementFlag)it->ConnectionFlags)) { continue; }

				Vector ConnStart = it->FromLocation + Vector(0.0f, 0.0f, 15.0f);
				Vector ConnEnd = it->ToLocation + Vector(0.0f, 0.0f, 15.0f);
				Vector MidPoint = ConnStart + ((ConnEnd - ConnStart) * 0.5f);
				MidPoint.z = fmaxf(ConnStart.z, ConnEnd.z);


				Vector ObjectCentre = (*stopIt).StopLocation;

				if (vlineIntersectsAABB(ConnStart, MidPoint, ObjectCentre - HalfExtents, ObjectCentre + HalfExtents))
				{
					stopIt->AffectedConnections.push_back(&(*it));
					continue;
				}

				if (vlineIntersectsAABB(MidPoint, ConnEnd, ObjectCentre - HalfExtents, ObjectCentre + HalfExtents))
				{
					stopIt->AffectedConnections.push_back(&(*it));
					continue;
				}
			}
		}
	}	
}

void NAV_ModifyOffMeshConnectionFlag(NavOffMeshConnection* Connection, const unsigned int NewFlag)
{
	if (!Connection || Connection->ConnectionFlags == NewFlag) { return; }

	Connection->ConnectionFlags = NewFlag;

	if (NavMeshes[Connection->NavMeshIndex].tileCache && Connection->ConnectionRef)
	{
		NavMeshes[Connection->NavMeshIndex].tileCache->modifyOffMeshConnection(Connection->ConnectionRef, NewFlag);
	}
}

void NAV_ApplyTempObstaclesToObject(DynamicMapObject* Object, const int Area)
{
	if (!Object) { return; }

	for (auto ObsIt = Object->TempObstacles.begin(); ObsIt != Object->TempObstacles.begin(); ObsIt++)
	{
		NAV_RemoveTemporaryObstacleFromNavmesh(*ObsIt);
	}

	Object->TempObstacles.clear();

	if (FNullEnt(Object->Edict) || Object->Edict->free)
	{
		return;
	}

	float SizeX = Object->Edict->v.size.x;
	float SizeY = Object->Edict->v.size.y;
	float SizeZ = Object->Edict->v.size.z;

	bool bUseXAxis = (SizeX >= SizeY);

	float CylinderRadius = fminf(SizeX, SizeY) * 0.5f;

	float Ratio = (bUseXAxis) ? (SizeX / (CylinderRadius * 2.0f)) : (SizeY / (CylinderRadius * 2.0f));

	int NumObstacles = (int)ceil(Ratio);

	Vector Dir = (bUseXAxis) ? RIGHT_VECTOR : FWD_VECTOR;

	Vector StartPoint = UTIL_GetCentreOfEntity(Object->Edict);

	if (bUseXAxis)
	{
		StartPoint.x = Object->Edict->v.absmin.x + CylinderRadius;
	}
	else
	{
		StartPoint.y = Object->Edict->v.absmin.y + CylinderRadius;
	}

	StartPoint.z -= 25.0f;

	Vector CurrentPoint = StartPoint;

	for (int ii = 0; ii < NumObstacles; ii++)
	{
		for (int NavIndex = 0; NavIndex < NUM_NAV_MESHES; NavIndex++)
		{
			NavTempObstacle* NewObstacle = NAV_AddTemporaryObstacleToNavmesh(NavIndex, CurrentPoint, CylinderRadius, SizeZ, Area);

			if (NewObstacle)
			{
				Object->TempObstacles.push_back(*NewObstacle);
			}
		}

		if (bUseXAxis)
		{
			CurrentPoint.x += CylinderRadius * 2.0f;
		}
		else
		{
			CurrentPoint.y += CylinderRadius * 2.0f;
		}
	}

}

void NAV_ClearDynamicMapData(bool bOnMapLoad)
{
	if (!bOnMapLoad)
	{
		for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
		{
			for (auto ObsIt = it->TempObstacles.begin(); ObsIt != it->TempObstacles.end(); ObsIt++)
			{
				NAV_RemoveTemporaryObstacleFromNavmesh(*ObsIt);
			}

			it->TempObstacles.clear();
			it->StopPoints.clear();
		}
	}

	DynamicMapObjects.clear();
	MapObjectPrototypes.clear();
}

void NAV_PopulateTrainStopPoints(DynamicMapObject* Train)
{
	edict_t* currTrain = Train->Edict;
	edict_t* StartCorner = nullptr;

	if (currTrain->v.target)
	{
		StartCorner = UTIL_FindEntityByTargetname(NULL, STRING(currTrain->v.target));
	}

	// We aren't using path corners, so treat like a door
	if (FNullEnt(StartCorner))
	{	
		edict_t* ObjectEdict = Train->Edict;

		Vector ObjectCentre = UTIL_GetCentreOfEntity(ObjectEdict);

		DynamicMapObjectStop FirstStop;
		FirstStop.StopLocation = ObjectCentre;
		FirstStop.WaitTime = 0.0f;
		FirstStop.bWaitForRetrigger = true;

		Train->StopPoints.push_back(FirstStop);

		Vector OpenPosition;
		Vector MoveOffset = (ObjectEdict->v.movedir * (fabs(ObjectEdict->v.movedir.x * (ObjectEdict->v.size.x - 2)) + fabs(ObjectEdict->v.movedir.y * (ObjectEdict->v.size.y - 2)) + fabs(ObjectEdict->v.movedir.z * (ObjectEdict->v.size.z - 2))));

		OpenPosition = ObjectCentre + MoveOffset;

		DynamicMapObjectStop SecondStop;
		SecondStop.StopLocation = OpenPosition;
		SecondStop.WaitTime = 0.0f;
		SecondStop.bWaitForRetrigger = (ObjectEdict->v.spawnflags & SF_DOOR_NO_AUTO_RETURN);

		Train->StopPoints.push_back(SecondStop);

		return;
	}

	DynamicMapObjectStop NextStop;
	NextStop.CornerEdict = StartCorner;
	NextStop.StopLocation = StartCorner->v.origin;
	NextStop.WaitTime = 0.0f;
	NextStop.bWaitForRetrigger = (StartCorner->v.spawnflags & SF_TRAIN_WAIT_RETRIGGER);

	Train->StopPoints.push_back(NextStop);

	// Populate all path corners at which this func_train stops. Bot will use this to determine when to board the train

	const char* StartCornerName = STRING(StartCorner->v.targetname);

	edict_t* CurrentCorner = UTIL_FindEntityByTargetname(NULL, STRING(StartCorner->v.target));

	while (CurrentCorner != NULL && CurrentCorner != StartCorner)
	{
		NextStop.CornerEdict = CurrentCorner;
		NextStop.StopLocation = CurrentCorner->v.origin;
		NextStop.WaitTime = 0.0f;
		NextStop.bWaitForRetrigger = (CurrentCorner->v.spawnflags & SF_TRAIN_WAIT_RETRIGGER);

		Train->StopPoints.push_back(NextStop);

		CurrentCorner = UTIL_FindEntityByTargetname(NULL, STRING(CurrentCorner->v.target));
	}

	Train->NextStopIndex = 1;
}

DynamicMapObject* UTIL_GetDynamicObjectByEdict(const edict_t* SearchEdict)
{
	if (FNullEnt(SearchEdict)) { return nullptr; }

	for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
	{
		if (it->Edict == SearchEdict)
		{
			return &(*it);
		}
	}

	return nullptr;
}

// TODO: Find the topmost point when open, and topmost point when closed, and see how closely they align to the top and bottom point parameters
DynamicMapObject* UTIL_GetClosestPlatformToPoints(const Vector StartPoint, const Vector EndPoint)
{
	DynamicMapObject* Result = nullptr;

	float minDist = 0.0f;

	for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
	{
		if (it->Type != MAPOBJECT_DOOR && it->Type != MAPOBJECT_PLATFORM) { continue; }

		float distTopPoint = FLT_MAX;
		float distBottomPoint = FLT_MAX;

		for (auto stop = it->StopPoints.begin(); stop != it->StopPoints.end(); stop++)
		{
			distTopPoint = fminf(distTopPoint, vDist3D(UTIL_GetClosestPointOnEntityToLocation(StartPoint, it->Edict, (*stop).StopLocation), StartPoint));
			distBottomPoint = fminf(distBottomPoint, vDist3D(UTIL_GetClosestPointOnEntityToLocation(EndPoint, it->Edict, (*stop).StopLocation), EndPoint));
		}

		// Get the average distance from our desired start and end points, whichever scores lowest is probably the lift/train/door we want to ride
		float thisDist = fminf(distTopPoint, distBottomPoint);// ((distTopPoint + distBottomPoint) * 0.5f);

		if (!Result || thisDist < minDist)
		{
			Result = &(*it);
			minDist = thisDist;
		}
	}

	return Result;
}

NavOffMeshConnection* NAV_AddOffMeshConnectionToNavmesh(unsigned int NavMeshIndex, Vector StartLoc, Vector EndLoc, unsigned char area, unsigned int flags, bool bBiDirectional)
{
	if (NavMeshIndex >= NUM_NAV_MESHES || !NavMeshes[NavMeshIndex].tileCache) { return nullptr; }

	Vector ProjectedStart = UTIL_ProjectPointToNavmesh(NavMeshIndex, StartLoc);
	Vector ProjectedEnd = UTIL_ProjectPointToNavmesh(NavMeshIndex, EndLoc);

	if (vIsZero(ProjectedStart) || vIsZero(ProjectedEnd)) { return nullptr; }

	NavOffMeshConnection NewConnectionDef;
	NewConnectionDef.NavMeshIndex = NavMeshIndex;
	NewConnectionDef.FromLocation = ProjectedStart;
	NewConnectionDef.ToLocation = ProjectedEnd;
	NewConnectionDef.DefaultConnectionFlags = flags;
	NewConnectionDef.ConnectionFlags = flags;

	// Now flip the coordinates for Detour

	ProjectedStart = Vector(ProjectedStart.x, ProjectedStart.z, -ProjectedStart.y);
	ProjectedEnd = Vector(ProjectedEnd.x, ProjectedEnd.z, -ProjectedEnd.y);

	dtOffMeshConnectionRef ref = 0;
	NewConnectionDef.ConnectionRef = 0;

	dtStatus AddStatus = NavMeshes[NavMeshIndex].tileCache->addOffMeshConnection(ProjectedStart, ProjectedEnd, 18.0f, area, flags, bBiDirectional, &ref);

	if (dtStatusSucceed(AddStatus))
	{
		NewConnectionDef.ConnectionRef = (unsigned int)ref;

		NavMeshes[NavMeshIndex].MeshConnections.push_back(NewConnectionDef);

		return &(*prev(NavMeshes[NavMeshIndex].MeshConnections.end()));
	}

	return nullptr;
}

bool NAV_AddOffMeshConnectionToAllNavmeshes(Vector StartLoc, Vector EndLoc, unsigned char area, unsigned int flags, bool bBiDirectional)
{
	bool bSuccess = true;

	for (int i = 0; i < NUM_NAV_MESHES; i++)
	{
		NavOffMeshConnection* NewConnection = NAV_AddOffMeshConnectionToNavmesh(i, StartLoc, EndLoc, area, flags, bBiDirectional);

		if (!NewConnection) { bSuccess = false; }
	}

	return bSuccess;
}

NavHint* NAV_AddHintToNavmesh(unsigned int NavMeshIndex, Vector Location, unsigned int HintFlags)
{
	if (NavMeshIndex >= NUM_NAV_MESHES || !NavMeshes[NavMeshIndex].tileCache) { return nullptr; }

	NavHint NewHint;
	NewHint.Position = Location;
	NewHint.HintTypes = HintFlags;

	NavMeshes[NavMeshIndex].MeshHints.push_back(NewHint);

	return &(*prev(NavMeshes[NavMeshIndex].MeshHints.end()));
}

bool NAV_RemoveOffMeshConnection(NavOffMeshConnection& RemoveConnectionDef)
{
	if (RemoveConnectionDef.NavMeshIndex < 0 || RemoveConnectionDef.NavMeshIndex >= NUM_NAV_MESHES) { return false; }

	if (!NavMeshes[RemoveConnectionDef.NavMeshIndex].tileCache) { return false; }

	dtStatus RemoveStatus = NavMeshes[RemoveConnectionDef.NavMeshIndex].tileCache->removeOffMeshConnection(RemoveConnectionDef.ConnectionRef);

	if (dtStatusSucceed(RemoveStatus))
	{
		RemoveConnectionDef.ConnectionRef = 0;
		return true;
	}
	else
	{
		return false;
	}
}

const dtOffMeshConnection* DEBUG_FindNearestOffMeshConnectionToPoint(const Vector Point, unsigned int FilterFlags)
{
	const dtOffMeshConnection* Result = nullptr;

	if (NavMeshes[NAV_MESH_DEFAULT].tileCache)
	{
		float PointConverted[3] = { Point.x, Point.z, -Point.y };

		float minDist = 0.0f;
		

		for (int i = 0; i < NavMeshes[NAV_MESH_DEFAULT].tileCache->getOffMeshCount(); i++)
		{
			const dtOffMeshConnection* con = NavMeshes[NAV_MESH_DEFAULT].tileCache->getOffMeshConnection(i);

			if (!con || con->state == DT_OFFMESH_EMPTY || con->state == DT_OFFMESH_REMOVING || !(con->flags & FilterFlags)) { continue; }

			float distSpos = dtVdistSqr(PointConverted, &con->pos[0]);
			float distEpos = dtVdistSqr(PointConverted, &con->pos[3]);

			float thisDist = dtMin(distSpos, distEpos);

			if (!Result || thisDist < minDist)
			{
				Result = con;
				minDist = thisDist;
			}
		}
	}

	return Result;
}

dtStatus DEBUG_TestFindPath(const NavAgentProfile& NavProfile, const Vector FromLocation, const Vector ToLocation, vector<bot_path_node>& path, float MaxAcceptableDistance)
{
	const dtNavMeshQuery* m_navQuery = UTIL_GetNavMeshQueryForProfile(NavProfile);
	const dtNavMesh* m_navMesh = UTIL_GetNavMeshForProfile(NavProfile);
	const dtQueryFilter* m_navFilter = &NavProfile.Filters;

	if (!m_navQuery || !m_navMesh || !m_navFilter || vIsZero(FromLocation) || vIsZero(ToLocation))
	{
		return DT_FAILURE;
	}

	Vector FromFloorLocation = AdjustPointForPathfinding(NavProfile.NavMeshIndex, FromLocation, NavProfile);
	Vector ToFloorLocation = AdjustPointForPathfinding(NavProfile.NavMeshIndex, ToLocation, NavProfile);

	float pStartPos[3] = { FromFloorLocation.x, FromFloorLocation.z, -FromFloorLocation.y };
	float pEndPos[3] = { ToFloorLocation.x, ToFloorLocation.z, -ToFloorLocation.y };

	dtStatus status;
	dtPolyRef StartPoly;
	float StartNearest[3];
	dtPolyRef EndPoly;
	float EndNearest[3];
	dtPolyRef PolyPath[MAX_PATH_POLY];
	dtPolyRef StraightPolyPath[MAX_AI_PATH_SIZE];
	int nPathCount = 0;
	float StraightPath[MAX_AI_PATH_SIZE * 3];
	unsigned char straightPathFlags[MAX_AI_PATH_SIZE];
	memset(straightPathFlags, 0, sizeof(straightPathFlags));
	int nVertCount = 0;

	// find the start polygon
	status = m_navQuery->findNearestPoly(pStartPos, pExtents, m_navFilter, &StartPoly, StartNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		//BotSay(pBot, "findNearestPoly start failed!");
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	// find the end polygon
	status = m_navQuery->findNearestPoly(pEndPos, pExtents, m_navFilter, &EndPoly, EndNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		//BotSay(pBot, "findNearestPoly end failed!");
		return (status & DT_STATUS_DETAIL_MASK); // couldn't find a polygon
	}

	status = m_navQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, m_navFilter, PolyPath, &nPathCount, MAX_PATH_POLY);

	if (PolyPath[nPathCount - 1] != EndPoly)
	{
		return DT_FAILURE;
	}

	status = m_navQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, straightPathFlags, StraightPolyPath, &nVertCount, MAX_AI_PATH_SIZE, DT_STRAIGHTPATH_AREA_CROSSINGS);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
	{
		return (status & DT_STATUS_DETAIL_MASK); // couldn't create a path
	}

	if (nVertCount == 0)
	{
		return DT_FAILURE; // couldn't find a path
	}

	path.clear();

	// At this point we have our path.  Copy it to the path store
	int nIndex = 0;

	Vector NodeFromLocation = FromFloorLocation;

	for (int nVert = 0; nVert < nVertCount; nVert++)
	{
		bot_path_node NextPathNode;

		NextPathNode.FromLocation = NodeFromLocation;

		NextPathNode.Location.x = StraightPath[nIndex++];
		NextPathNode.Location.z = StraightPath[nIndex++];
		NextPathNode.Location.y = -StraightPath[nIndex++];
		NextPathNode.area = NAV_AREA_WALK;
		NextPathNode.flag = NAV_FLAG_WALK;

		path.push_back(NextPathNode);

		NodeFromLocation = NextPathNode.Location;
	}

	return DT_SUCCESS;
}

void NAV_AddMoveMovementTask(AIPlayer* pBot, Vector MoveLocation, DynamicMapObject* TriggerToActivate)
{
	if (pBot->BotNavInfo.MovementTasks.size() >= 10) { return; }

	if (vIsZero(MoveLocation)) { return; }

	if (vDist2DSq(pBot->CurrentFloorPosition, MoveLocation) < sqrf(GetPlayerRadius(pBot->Edict)) && fabsf(pBot->CollisionHullBottomLocation.z - MoveLocation.z) < 50.0f) { return; }

	AIPlayerMoveTask NewTask;

	NewTask.TaskType = MOVE_TASK_MOVE;
	NewTask.TaskLocation = MoveLocation;

	vector<bot_path_node> Path;
	dtStatus PathStatus = FindPathClosestToPoint(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, MoveLocation, Path, 200.0f);

	if (dtStatusSucceed(PathStatus) && Path.size() > 0)
	{
		NewTask.TaskLocation = Path.back().Location;
	}

	pBot->BotNavInfo.MovementTasks.push_back(NewTask);
}

void NAV_AddTouchMovementTask(AIPlayer* pBot, edict_t* EntityToTouch, DynamicMapObject* TriggerToActivate)
{
	if (pBot->BotNavInfo.MovementTasks.size() >= 10) { return; }

	AIPlayerMoveTask NewTask;

	NewTask.TaskType = MOVE_TASK_TOUCH;
	NewTask.TaskTarget = EntityToTouch;
	NewTask.TriggerToActivate = TriggerToActivate->Edict;

	vector<bot_path_node> Path;
	dtStatus PathStatus = FindPathClosestToPoint(pBot->BotNavInfo.NavProfile, pBot->CurrentFloorPosition, UTIL_GetCentreOfEntity(EntityToTouch), Path, 200.0f);

	if (dtStatusSucceed(PathStatus) && Path.size() > 0)
	{
		NewTask.TaskLocation = Path.back().Location;
	}
}

void NAV_AddUseMovementTask(AIPlayer* pBot, edict_t* EntityToUse, DynamicMapObject* TriggerToActivate)
{
	if (pBot->BotNavInfo.MovementTasks.size() >= 10) { return; }

	AIPlayerMoveTask NewTask;

	NewTask.TaskType = MOVE_TASK_USE;
	NewTask.TaskTarget = EntityToUse;
	NewTask.TriggerToActivate = TriggerToActivate->Edict;
	NewTask.TaskLocation = UTIL_GetButtonFloorLocation(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, EntityToUse);

	pBot->BotNavInfo.MovementTasks.push_back(NewTask);
}

void NAV_AddBreakMovementTask(AIPlayer* pBot, edict_t* EntityToBreak, DynamicMapObject* TriggerToActivate)
{
	if (pBot->BotNavInfo.MovementTasks.size() >= 10) { return; }

	AIPlayerMoveTask NewTask;

	NewTask.TaskType = MOVE_TASK_BREAK;
	NewTask.TaskTarget = EntityToBreak;
	NewTask.TriggerToActivate = TriggerToActivate->Edict;

	NewTask.TaskLocation = UTIL_GetButtonFloorLocation(pBot->BotNavInfo.NavProfile, pBot->Edict->v.origin, EntityToBreak);

	pBot->BotNavInfo.MovementTasks.push_back(NewTask);
}

bool UTIL_IsTileCacheUpToDate()
{
	return bTileCacheUpToDate;
}

bool NAV_IsMovementTaskStillValid(AIPlayer* pBot, AIPlayerMoveTask& Task)
{

	if (Task.TaskType == MOVE_TASK_NONE) { return false; }
	
	if (Task.TriggerToActivate)
	{
		DynamicMapObject* TriggerTarget = NAV_GetTriggerByEdict(Task.TriggerToActivate);

		if (!TriggerTarget->bIsActive || TriggerTarget->State != OBJECTSTATE_IDLE) { return false; }
	}

	if (Task.TaskType == MOVE_TASK_MOVE)
	{
		return (vDist2DSq(pBot->Edict->v.origin, Task.TaskLocation) > sqrf(GetPlayerRadius(pBot->Edict)) || fabsf(pBot->Edict->v.origin.z - Task.TaskLocation.z) > 50.0f);
	}

	if (Task.TaskType == MOVE_TASK_USE)
	{
		DynamicMapObject* TriggerObject = NAV_GetTriggerByEdict(Task.TaskTarget);

		return (TriggerObject && TriggerObject->bIsActive && TriggerObject->State == OBJECTSTATE_IDLE);
	}

	if (Task.TaskType == MOVE_TASK_PICKUP)
	{
		return (!FNullEnt(Task.TaskTarget) && !(Task.TaskTarget->v.effects & EF_NODRAW));
	}

	if (Task.TaskType == MOVE_TASK_TOUCH)
	{
		DynamicMapObject* TriggerObject = NAV_GetTriggerByEdict(Task.TaskTarget);

		if (TriggerObject && (!TriggerObject->bIsActive || TriggerObject->State != OBJECTSTATE_IDLE)) { return false; }

		return (!FNullEnt(Task.TaskTarget) && !IsPlayerTouchingEntity(pBot->Edict, Task.TaskTarget));
	}

	if (Task.TaskType == MOVE_TASK_BREAK)
	{
		return (!FNullEnt(Task.TaskTarget) && Task.TaskTarget->v.deadflag == DEAD_NO && Task.TaskTarget->v.health > 0.0f);
	}

	return false;

}
void NAV_AddTriggerMovementTask(AIPlayer* pBot, DynamicMapObject* Trigger, DynamicMapObject* TriggerTarget)
{
	if (pBot->BotNavInfo.MovementTasks.size() >= 10) { return; }

	if (!pBot || !Trigger || !TriggerTarget)
	{
		return;
	}
	switch (Trigger->Type)
	{
		case TRIGGER_SHOOT:
		case TRIGGER_BREAK:
			NAV_AddBreakMovementTask(pBot, Trigger->Edict, TriggerTarget);
			break;
		case TRIGGER_TOUCH:
			NAV_AddTouchMovementTask(pBot, Trigger->Edict, TriggerTarget);
			break;
		case TRIGGER_USE:
			NAV_AddUseMovementTask(pBot, Trigger->Edict, TriggerTarget);
			break;
		default:
			NAV_AddUseMovementTask(pBot, Trigger->Edict, TriggerTarget);
			break;
	}
	
}

void NAV_AddPickupMovementTask(AIPlayer* pBot, edict_t* ThingToPickup, DynamicMapObject* TriggerToActivate)
{
	if (pBot->BotNavInfo.MovementTasks.size() >= 10) { return; }

	AIPlayerMoveTask NewTask;

	NewTask.TaskType = MOVE_TASK_PICKUP;
	NewTask.TaskTarget = ThingToPickup;
	NewTask.TriggerToActivate = TriggerToActivate->Edict;
	NewTask.TaskLocation = ThingToPickup->v.origin;

	pBot->BotNavInfo.MovementTasks.push_back(NewTask);
}

vector<NavHint*> NAV_GetHintsOfType(unsigned int NavMeshIndex, unsigned int HintType)
{
	vector<NavHint*> Result;

	Result.clear();

	for (auto it = NavMeshes[NavMeshIndex].MeshHints.begin(); it != NavMeshes[NavMeshIndex].MeshHints.end(); it++)
	{
		if (HintType != 0 && !(it->HintTypes & HintType)) { continue; }

		Result.push_back(&(*it));
	}

	return Result;

}

vector<NavHint*> NAV_GetHintsOfTypeInRadius(unsigned int NavMeshIndex, unsigned int HintType, Vector SearchLocation, float Radius)
{
	vector<NavHint*> Result;
	Result.clear();

	if (NavMeshIndex >= NUM_NAV_MESHES) { return Result; }

	float SearchRadius = sqrf(Radius);

	for (auto it = NavMeshes[NavMeshIndex].MeshHints.begin(); it != NavMeshes[NavMeshIndex].MeshHints.end(); it++)
	{
		if (HintType != 0 && !(it->HintTypes & HintType)) { continue; }

		if (vDist3DSq(it->Position, SearchLocation) < SearchRadius)
		{
			Result.push_back(&(*it));
		}
	}

	return Result;

}

void NAV_ClearCachedMapData()
{
	MapObjectPrototypes.clear();
	DynamicMapObjects.clear();
}

void NAV_AddDynamicMapObject(DynamicMapPrototype* Prototype)
{
	DynamicMapObject NewObject;
	NewObject.EdictIndex = Prototype->EdictIndex;
	NewObject.Edict = Prototype->Edict;
	NewObject.ObjectName = STRING(Prototype->Edict->v.targetname);
	NewObject.GlobalState = Prototype->GlobalState;
	NewObject.Triggers.clear();
	NewObject.NextStopIndex = 1;
	NewObject.Wait = Prototype->Wait;
	NewObject.Delay = Prototype->Delay;

	edict_t* ObjectEdict = NewObject.Edict;

	if (!Prototype->Master.empty())
	{
		edict_t* MasterEdict = UTIL_FindEntityByTargetname(NULL, Prototype->Master.c_str());
		NewObject.Master = MasterEdict;
	}

	const char* classname = STRING(ObjectEdict->v.classname);

	if (FStrEq(classname, "func_train"))
	{
		NewObject.Type = MAPOBJECT_PLATFORM;

		NAV_PopulateTrainStopPoints(&NewObject);
	}
	else if (FStrEq(classname, "func_plat"))
	{
		NewObject.Type = MAPOBJECT_PLATFORM;

		Vector PlatformCentre = UTIL_GetCentreOfEntity(ObjectEdict);

		DynamicMapObjectStop FirstStop;
		FirstStop.StopLocation = PlatformCentre;
		FirstStop.WaitTime = 0.0f;
		FirstStop.bWaitForRetrigger = true;

		NewObject.StopPoints.push_back(FirstStop);

		float RiseHeight = (Prototype->Height > 0.0f) ? Prototype->Height : ObjectEdict->v.size.z;

		DynamicMapObjectStop SecondStop;
		SecondStop.StopLocation = PlatformCentre + Vector(0.0f, 0.0f, RiseHeight - 8.0f);
		SecondStop.WaitTime = 3.0f;
		SecondStop.bWaitForRetrigger = (ObjectEdict->v.spawnflags & SF_PLAT_TRIGGER_ONLY);

		NewObject.StopPoints.push_back(SecondStop);
		
	}
	else if (FStrEq(classname, "func_door") || FStrEq(classname, "func_door_rotating") || FStrEq(classname, "func_seethroughdoor"))
	{
		NewObject.Type = MAPOBJECT_DOOR;

		Vector DoorCentre = UTIL_GetCentreOfEntity(ObjectEdict);

		DynamicMapObjectStop FirstStop;
		FirstStop.StopLocation = DoorCentre;
		FirstStop.WaitTime = 0.0f;
		FirstStop.bWaitForRetrigger = true;

		NewObject.StopPoints.push_back(FirstStop);

		Vector OpenPosition;
		Vector MoveOffset = (ObjectEdict->v.movedir * (fabs(ObjectEdict->v.movedir.x * (ObjectEdict->v.size.x - 2)) + fabs(ObjectEdict->v.movedir.y * (ObjectEdict->v.size.y - 2)) + fabs(ObjectEdict->v.movedir.z * (ObjectEdict->v.size.z - 2)) - Prototype->Lip));

		if (ObjectEdict->v.spawnflags & DOOR_START_OPEN)
		{
			OpenPosition = DoorCentre - MoveOffset;
		}
		else
		{
			OpenPosition = DoorCentre + MoveOffset;
		}

		DynamicMapObjectStop SecondStop;
		SecondStop.StopLocation = OpenPosition;
		SecondStop.WaitTime = Prototype->Wait;
		SecondStop.bWaitForRetrigger = ((ObjectEdict->v.spawnflags & SF_DOOR_NO_AUTO_RETURN) || Prototype->Wait < 0.0f);

		NewObject.bToggleActive = (ObjectEdict->v.spawnflags & SF_DOOR_NO_AUTO_RETURN);

		NewObject.StopPoints.push_back(SecondStop);
	}
	else if (FStrEq(classname, "trigger_once") || FStrEq(classname, "trigger_multiple"))
	{
		NewObject.Type = TRIGGER_TOUCH;
	}
	else if (FStrEq(classname, "func_button"))
	{
		if (Prototype->Health > 0.0f)
		{
			NewObject.Type = TRIGGER_SHOOT;
		}
		else
		{
			NewObject.Type = TRIGGER_USE;
		}

		Vector ButtonCentre = UTIL_GetCentreOfEntity(ObjectEdict);

		DynamicMapObjectStop NextStop;
		NextStop.StopLocation = ButtonCentre;
		NextStop.WaitTime = 0.0f;
		NextStop.bWaitForRetrigger = true;

		NewObject.StopPoints.push_back(NextStop);

		if (!(ObjectEdict->v.spawnflags & SF_BUTTON_DONT_MOVE))
		{
			Vector MoveOffset = (ObjectEdict->v.movedir * (fabs(ObjectEdict->v.movedir.x * (ObjectEdict->v.size.x - 2)) + fabs(ObjectEdict->v.movedir.y * (ObjectEdict->v.size.y - 2)) + fabs(ObjectEdict->v.movedir.z * (ObjectEdict->v.size.z - 2)) - Prototype->Lip));

			NextStop.StopLocation = ButtonCentre + MoveOffset;
		}
		
		NextStop.bWaitForRetrigger = (ObjectEdict->v.spawnflags & SF_DOOR_NO_AUTO_RETURN);
		NextStop.WaitTime = Prototype->Wait;

		NewObject.StopPoints.push_back(NextStop);
	}
	else if (FStrEq(classname, "func_breakable"))
	{
		NewObject.Type = TRIGGER_BREAK;
	}
	else if (FStrEq(classname, "env_global"))
	{
		NewObject.Type = TRIGGER_ENV;
		NewObject.bIsActive = Prototype->TriggerState != 0;
		NewObject.bToggleActive = Prototype->TriggerMode == 3;
	}
	else if (FStrEq(classname, "multisource"))
	{
		NewObject.Type = TRIGGER_MULTISOURCE;
	}
	else
	{
		NewObject.Type = TRIGGER_NONE;
	}

	for (auto it = Prototype->Targets.begin(); it != Prototype->Targets.end(); it++)
	{
		edict_t* TargetEdict = UTIL_FindEntityByTargetname(NULL, (*it).c_str());

		if (!FNullEnt(TargetEdict))
		{
			NewObject.Targets.push_back(TargetEdict);
		}
	}

	DynamicMapObjects.push_back(NewObject);
}

void NAV_PopulateDynamicMapObjects()
{
	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		NAV_AddDynamicMapObject(&(*it));
	}

	NAV_LinkDynamicMapMasters();
	NAV_LinkDynamicMapObjectsToTriggers();
	NAV_LinkDynamicMapObjectsToOffmeshConnections();
	NAV_SetTrainStartPoints();
	NAV_PopulateAllConnectionsAffectedByDynamicObjects();
}

void NAV_SetTrainStartPoints()
{
	for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
	{
		if (it->Type == MAPOBJECT_PLATFORM && it->StopPoints.size() > 2 && it->Targets.size() > 0)
		{
			edict_t* TargetEdict = it->Targets[0];
			const char* TargetEdictName = STRING(TargetEdict->v.targetname);

			int CurrIndex = 0;

			for (auto stopIt = it->StopPoints.begin(); stopIt != it->StopPoints.end(); stopIt++)
			{
				if (stopIt->CornerEdict == TargetEdict)
				{
					break;
				}

				CurrIndex++;
			}

			it->NextStopIndex = CurrIndex + 1;

			if (it->NextStopIndex >= it->StopPoints.size())
			{
				it->NextStopIndex = 0;
			}
		}
	}
}

void NAV_LinkDynamicMapObjectsToOffmeshConnections()
{
	for (int i = 0; i < NUM_NAV_MESHES; i++)
	{
		for (auto it = NavMeshes[i].MeshConnections.begin(); it != NavMeshes[i].MeshConnections.end(); it++)
		{
			if (it->DefaultConnectionFlags & NAV_FLAG_PLATFORM)
			{
				DynamicMapObject* NearestPlatform = UTIL_GetClosestPlatformToPoints(it->FromLocation, it->ToLocation);

				if (NearestPlatform)
				{
					it->LinkedObject = NearestPlatform->Edict;
				}
			}
		}
	}
}

void NAV_PopulateAllConnectionsAffectedByDynamicObjects()
{
	for (auto objectIt = DynamicMapObjects.begin(); objectIt != DynamicMapObjects.end(); objectIt++)
	{
		NAV_PopulateConnectionsAffectedByDynamicObject(&(*objectIt));
	}
}

void NAV_OnDynamicMapObjectBecomeIdle(DynamicMapObject* Object)
{
	// Object will not move again, so block off all connections permanently, and place null obstacles to block nav mesh
	if (Object->Type == MAPOBJECT_STATIC)
	{
		NAV_ApplyTempObstaclesToObject(Object, DT_AREA_NULL);

		int CurrStopIndex = (Object->NextStopIndex > 0) ? Object->NextStopIndex - 1 : Object->StopPoints.size() - 1;

		for (auto it = Object->StopPoints[CurrStopIndex].AffectedConnections.begin(); it != Object->StopPoints[CurrStopIndex].AffectedConnections.end(); it++)
		{
			NavOffMeshConnection* ThisConnection = (*it);

			NAV_ModifyOffMeshConnectionFlag(ThisConnection, NAV_FLAG_DISABLED);
		}

		return;
	}

	// Door is not permanently blocking anything and can still be activated. We need to do a more nuanced take on how connections are modified
	// The idea is that if a connection is one-way, we detect how we can activate the door from that side and modify the connection flag accordingly
	// Two-way connections will end up being treated like a one-way connection, so use 2 one-way connections if a door requires different capabilities for each side!

	int CurrStopIndex = (Object->NextStopIndex > 0) ? Object->NextStopIndex - 1 : Object->StopPoints.size() - 1;

	NavAgentProfile TestProfile = GetBaseAgentProfile(NAV_PROFILE_DEFAULT);

	for (auto it = Object->StopPoints[CurrStopIndex].AffectedConnections.begin(); it != Object->StopPoints[CurrStopIndex].AffectedConnections.end(); it++)
	{
		NavOffMeshConnection* ThisConnection = (*it);

		TestProfile.NavMeshIndex = ThisConnection->NavMeshIndex;

		DynamicMapObject* ThisTrigger = NAV_GetBestTriggerForObject(Object, ThisConnection->FromLocation, TestProfile);

		if (!ThisTrigger)
		{
			NAV_ModifyOffMeshConnectionFlag(ThisConnection, NAV_FLAG_DISABLED);
			continue;
		}
	}
}

DynamicMapObject* NAV_GetNearestTriggerForObjectReachableFromPoint(const NavAgentProfile& NavProfile, DynamicMapObject* ObjectToTrigger, Vector ActivateLocation)
{
	DynamicMapObject* Result = nullptr;
	float MinDist = FLT_MAX;

	for (auto it = ObjectToTrigger->Triggers.begin(); it != ObjectToTrigger->Triggers.end(); it++)
	{
		DynamicMapObject* ThisTriggerObject = UTIL_GetDynamicObjectByEdict(*it);

		if (!ThisTriggerObject) { continue; }

		Vector ButtonLocation = UTIL_GetButtonFloorLocation(NavProfile, ActivateLocation, ThisTriggerObject->Edict);

		if (!UTIL_IsPathBlockedByObject(NavProfile, ActivateLocation, ButtonLocation, ObjectToTrigger))
		{
			// TODO: Update this to ensure door is always flat. Prevents angled doors messing things up
			float ThisDist = vDist3DSq(ActivateLocation, ButtonLocation);

			if (ThisDist < MinDist)
			{
				Result = ThisTriggerObject;
				MinDist = ThisDist;
			}
		}
	}

	return Result;
}

void NAV_OnDynamicMapObjectStopIdle(DynamicMapObject* Object)
{
	for (auto stopIt = Object->StopPoints.begin(); stopIt != Object->StopPoints.end(); stopIt++)
	{
		for (auto it = stopIt->AffectedConnections.begin(); it != stopIt->AffectedConnections.end(); it++)
		{
			NavOffMeshConnection* ThisConnection = (*it);

			NAV_ModifyOffMeshConnectionFlag(ThisConnection, ThisConnection->DefaultConnectionFlags);
		}
	}

	for (auto it = Object->TempObstacles.begin(); it != Object->TempObstacles.end(); it++)
	{
		NAV_RemoveTemporaryObstacleFromNavmesh(*it);
	}

	Object->TempObstacles.clear();
}

void NAV_SetDynamicObjectStatus(DynamicMapObject* Object, DynamicMapObjectState NewState)
{
	if (NewState == Object->State) { return; }

	Object->State = NewState;

	if (NewState == OBJECTSTATE_MOVING)
	{		
		NAV_OnDynamicMapObjectStopIdle(Object);
	}

	if (NewState == OBJECTSTATE_IDLE)
	{
		NAV_OnDynamicMapObjectBecomeIdle(Object);
	}
}

void NAV_UpdateDynamicMapObjects()
{
	for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end();)
	{
		DynamicMapObject* ThisObject = &(*it);

		if (ThisObject->Type == MAPOBJECT_STATIC)
		{
			if (ThisObject->State != OBJECTSTATE_IDLE)
			{
				if (ThisObject->Edict->v.velocity.Length() <= 0.0f)
				{
					NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_IDLE);
				}
			}

			it++;
			continue;
		}

		if ((ThisObject->Type == TRIGGER_BREAK && ThisObject->Edict->v.health <= 0))
		{
			NAV_OnTriggerActivated(ThisObject);

			it = DynamicMapObjects.erase(it);
			continue;
		}

		if (FNullEnt(ThisObject->Edict) || ThisObject->Edict->v.deadflag != DEAD_NO)
		{
			it = DynamicMapObjects.erase(it);
			continue;
		}

		if (ThisObject->Type == TRIGGER_MULTISOURCE)
		{
			ThisObject->bIsActive = true;
			NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_PREPARING);

			for (auto triggerIt = ThisObject->Triggers.begin(); triggerIt != ThisObject->Triggers.end(); triggerIt++)
			{
				DynamicMapObject* ThisTrigger = UTIL_GetDynamicObjectByEdict((*triggerIt));

				if (ThisTrigger && (!ThisTrigger->bIsActive || ThisTrigger->State == OBJECTSTATE_IDLE))
				{
					ThisObject->bIsActive = false;
					break;
				}
			}

			it++;
			continue;
		}

		if (!FNullEnt(ThisObject->Master))
		{
			DynamicMapObject* Master = UTIL_GetDynamicObjectByEdict(ThisObject->Master);

			if (Master)
			{
				ThisObject->bIsActive = Master->bIsActive;
			}
			else
			{
				ThisObject->bIsActive = true;
			}
		}
		else
		{
			if (ThisObject->Type != TRIGGER_ENV)
			{
				ThisObject->bIsActive = true;
			}
		}

		if (ThisObject->Type == TRIGGER_ENV || ThisObject->Type == TRIGGER_MULTISOURCE || ThisObject->Type == TRIGGER_NONE)
		{
			NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_PREPARING);
			it++;
			continue;
		}

		if (ThisObject->State == OBJECTSTATE_IDLE && ThisObject->Edict->v.nextthink > 0.0f)
		{
			NAV_OnTriggerActivated(ThisObject);
			it++;
			continue;
		}

		Vector ObjectCentre = UTIL_GetCentreOfEntity(ThisObject->Edict);

		if (ThisObject->Edict->v.velocity.Length() > 0.0f)
		{
			NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_MOVING);

			if (ThisObject->StopPoints.size() > 0)
			{
				DynamicMapObjectStop NextStop = ThisObject->StopPoints[ThisObject->NextStopIndex];

				// This object is not going to stop at the next point at all, so we need to check if we've reached it
				if (!NextStop.bWaitForRetrigger && NextStop.WaitTime == 0.0f)
				{
					if (vEquals(ObjectCentre, NextStop.StopLocation, 5.0f))
					{
						ThisObject->NextStopIndex++;

						if (ThisObject->NextStopIndex >= ThisObject->StopPoints.size())
						{
							ThisObject->NextStopIndex = 0;
						}
					}
				}
			}

			it++;
			continue;
		}

		// We were moving but now we're not
		if (ThisObject->State == OBJECTSTATE_MOVING && ThisObject->StopPoints.size() > 0)
		{
			DynamicMapObjectStop NextStop = ThisObject->StopPoints[ThisObject->NextStopIndex];

			if (vEquals(ObjectCentre, NextStop.StopLocation, 1.0f))
			{
				ThisObject->NextStopIndex++;

				if (ThisObject->NextStopIndex >= ThisObject->StopPoints.size())
				{
					ThisObject->NextStopIndex = 0;
				}

				if (NextStop.bWaitForRetrigger)
				{
					if (ThisObject->Type == MAPOBJECT_DOOR)
					{
						if (NextStop.WaitTime < 0.0f && !ThisObject->bToggleActive)
						{
							ThisObject->Type = MAPOBJECT_STATIC;							
						}
						else
						{
							NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_IDLE);
						}
					}
					else
					{
						NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_IDLE);
					}
					
				}
				else
				{
					ThisObject->State = OBJECTSTATE_PREPARING;
				}
			}
			else
			{
				// This is safety code in case a lift got messed with and deviated from its original move target (e.g. blocked and returns back rather than completes move)
				DynamicMapObjectStop NewStop;
				bool bAtStop = false;

				for (int i = 0; i < ThisObject->StopPoints.size(); i++)
				{
					DynamicMapObjectStop CheckStop = ThisObject->StopPoints[i];

					if (vEquals(ObjectCentre, CheckStop.StopLocation))
					{
						bAtStop = true;
						NewStop = CheckStop;
						ThisObject->NextStopIndex = i + 1;
						if (ThisObject->NextStopIndex >= ThisObject->StopPoints.size())
						{
							ThisObject->NextStopIndex = 0;
						}

						break;
					}

				}

				if (!bAtStop || NewStop.bWaitForRetrigger)
				{
					NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_IDLE);
				}
				else
				{
					NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_PREPARING);
				}
			}

			it++;
			continue;
		}

		if (ThisObject->State == OBJECTSTATE_PREPARING && (ThisObject->StopPoints.size() < 2 || vEquals(ThisObject->StopPoints[0].StopLocation, ThisObject->StopPoints[1].StopLocation)))
		{
			if (gpGlobals->time - ThisObject->LastActivatedTime >= ThisObject->Delay)
			{
				NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_OPEN);
				ThisObject->LastActivatedTime = gpGlobals->time;
				it++;
				continue;
			}
		}

		if (ThisObject->State == OBJECTSTATE_OPEN)
		{
			if (gpGlobals->time - ThisObject->LastActivatedTime >= ThisObject->Wait)
			{
				NAV_SetDynamicObjectStatus(ThisObject, OBJECTSTATE_IDLE);
				it++;
				continue;
			}
		}

		if ((ThisObject->Type == MAPOBJECT_DOOR || ThisObject->Type == MAPOBJECT_PLATFORM) && ThisObject->State == OBJECTSTATE_IDLE)
		{
			// We have no way of activating this door. It's now a permanent blockage
			if (ThisObject->Triggers.size() == 0)
			{
				ThisObject->Type = MAPOBJECT_STATIC;
				it++;
				continue;
			}
		}

		it++;
	}
}

void DEBUG_ShowDynamicObjects()
{
	for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
	{
		DynamicMapObject* ThisObject = &(*it);
		DynamicMapObject* Master = nullptr;
		vector<DynamicMapObject*> Triggers;
		vector<DynamicMapObject*> Targets;

		if (!FNullEnt(it->Master))
		{
			Master = UTIL_GetDynamicObjectByEdict(it->Master);
		}

		for (auto trigIt = ThisObject->Triggers.begin(); trigIt != ThisObject->Triggers.end(); trigIt++)
		{
			DynamicMapObject* TriggerObject = UTIL_GetDynamicObjectByEdict((*trigIt));

			Triggers.push_back(TriggerObject);
		}

		for (auto targIt = ThisObject->Targets.begin(); targIt != ThisObject->Targets.end(); targIt++)
		{
			DynamicMapObject* TargetObject = UTIL_GetDynamicObjectByEdict((*targIt));

			Targets.push_back(TargetObject);
		}

		bool bBoop = true;
	}
}

void NAV_OnTriggerActivated(DynamicMapObject* UsedObject)
{
	if (!UsedObject) { return; }

	UsedObject->NumTimesActivated++;

	if (UsedObject->Type == TRIGGER_ENV)
	{
		if (UsedObject->bToggleActive || UsedObject->NumTimesActivated == 1)
		{
			UsedObject->bIsActive = !UsedObject->bIsActive;
		}
		return;
	}

	if (UsedObject->Type == TRIGGER_MULTISOURCE || UsedObject->Type == TRIGGER_NONE) { return; }

	UsedObject->LastActivatedTime = gpGlobals->time;

	UsedObject->State = OBJECTSTATE_PREPARING;

	for (auto it = UsedObject->Targets.begin(); it != UsedObject->Targets.end(); it++)
	{
		DynamicMapObject* TargetObject = UTIL_GetDynamicObjectByEdict((*it));

		if (TargetObject)
		{
			if (UsedObject->Type != TRIGGER_ENV && UsedObject->Type != TRIGGER_NONE)
			{
				NAV_OnTriggerActivated(TargetObject);
			}
		}
	}

	bool bBoop = true;
}

void NAV_LinkDynamicMapMasters()
{
	for (auto objectIt = DynamicMapObjects.begin(); objectIt != DynamicMapObjects.end(); objectIt++)
	{
		DynamicMapObject* ThisObject = &(*objectIt);

		if (ThisObject->Type == TRIGGER_ENV && !ThisObject->GlobalState.empty())
		{
			for (auto masterIt = DynamicMapObjects.begin(); masterIt != DynamicMapObjects.end(); masterIt++)
			{
				DynamicMapObject* OtherObject = &(*masterIt);

				if (ThisObject == OtherObject || OtherObject->Type == TRIGGER_ENV) { continue; }

				if (OtherObject->GlobalState == ThisObject->GlobalState)
				{
					OtherObject->Master = ThisObject->Edict;
					ThisObject->Targets.push_back(OtherObject->Edict);
				}
			}
		}
		else
		{
			if (!FNullEnt(ThisObject->Master))
			{
				DynamicMapObject* MasterObject = UTIL_GetDynamicObjectByEdict(ThisObject->Master);

				if (MasterObject)
				{
					bool bAlreadyHasTrigger = false;

					for (auto trigIt = MasterObject->Targets.begin(); trigIt != MasterObject->Targets.end(); trigIt++)
					{
						if ((*trigIt) == ThisObject->Edict)
						{
							bAlreadyHasTrigger = true;
							break;
						}
					}

					if (!bAlreadyHasTrigger)
					{
						MasterObject->Targets.push_back(ThisObject->Edict);
					}
				}
			}
		}
	}
}

void NAV_LinkDynamicMapObjectsToTriggers()
{
	for (auto objectIt = DynamicMapObjects.begin(); objectIt != DynamicMapObjects.end(); objectIt++)
	{
		DynamicMapObject* ThisObject = &(*objectIt);

		if (ThisObject->Type == MAPOBJECT_DOOR)
		{
			// Gotta use this door to open it, so add the door itself as its own trigger
			if (ThisObject->Edict->v.spawnflags & DOOR_USE_ONLY)
			{
				ThisObject->Triggers.push_back(ThisObject->Edict);
			}
			// Must be one o' them fancy touch-activamated doors. Technically the bot will still try to use the door, but they will touch it in the process so no big deal
			else if (ThisObject->ObjectName == NULL || ThisObject->ObjectName[0] == '\0')
			{
				ThisObject->Triggers.push_back(ThisObject->Edict);
			}

			continue;
		}

		if (ThisObject->Type == MAPOBJECT_PLATFORM)
		{
			if (FClassnameIs(ThisObject->Edict, "func_plat"))
			{
				ThisObject->Triggers.push_back(ThisObject->Edict);
			}

			continue;
		}

		if (ThisObject->Type == TRIGGER_USE
			|| ThisObject->Type == TRIGGER_TOUCH
			|| ThisObject->Type == TRIGGER_SHOOT
			|| ThisObject->Type == TRIGGER_BREAK)
		{
			for (auto targetsIt = DynamicMapObjects.begin(); targetsIt != DynamicMapObjects.end(); targetsIt++)
			{
				DynamicMapObject* OtherObject = &(*targetsIt);

				if (OtherObject == ThisObject) { continue; }

				vector<DynamicMapObject*> CheckedObjects;
				if (NAV_IsDynamicMapTriggerLinkedToObject(ThisObject, OtherObject, CheckedObjects))
				{
					OtherObject->Triggers.push_back(ThisObject->Edict);
				}
			}
		}
	}

	for (auto objectIt = DynamicMapObjects.begin(); objectIt != DynamicMapObjects.end(); objectIt++)
	{
		if (!FNullEnt(objectIt->Master))
		{
			DynamicMapObject* Master = UTIL_GetDynamicObjectByEdict(objectIt->Master);

			objectIt->bIsActive = Master->bIsActive;
		}
	}

	for (auto objectIt = DynamicMapObjects.begin(); objectIt != DynamicMapObjects.end(); objectIt++)
	{
		if (!FNullEnt(objectIt->Master))
		{
			DynamicMapObject* Master = UTIL_GetDynamicObjectByEdict(objectIt->Master);

			objectIt->bIsActive = Master->bIsActive;
		}
	}
}

bool NAV_IsDynamicMapTriggerLinkedToObject(DynamicMapObject* TriggerObject, DynamicMapObject* TargetObject, vector<DynamicMapObject*> CheckedObjects)
{
	if (!TriggerObject || !TargetObject) { return false; }

	if (TriggerObject == TargetObject) { return true; }

	CheckedObjects.push_back(TriggerObject);

	for (auto it = TriggerObject->Targets.begin(); it != TriggerObject->Targets.end(); it++)
	{
		DynamicMapObject* ThisTarget = UTIL_GetDynamicObjectByEdict((*it));

		if (!ThisTarget || ThisTarget == TriggerObject) { continue; }

		// We've already checked this object. Prevents infinite circular references
		if (find(CheckedObjects.begin(), CheckedObjects.end(), ThisTarget) != CheckedObjects.end()) { continue; }

		if (NAV_IsDynamicMapTriggerLinkedToObject(ThisTarget, TargetObject, CheckedObjects)) { return true; }
	}

	return false;
}

void NAV_SetPrototypeWait(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;
	
	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.Wait = atof(Value);

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->Wait = atof(Value);
	}	
}

void NAV_SetPrototypeDelay(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.Delay = atof(Value);

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->Delay = atof(Value);
	}
}

void NAV_SetPrototypeLip(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.Lip = atof(Value);

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->Lip = atof(Value);
	}
}

void NAV_SetPrototypeHeight(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.Height = atof(Value);

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->Height = atof(Value);
	}
}

void NAV_SetPrototypeMaster(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.Master = Value;

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->Master = Value;
	}
}

void NAV_SetPrototypeName(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.ObjectName = Value;

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->ObjectName = Value;
	}
}

void NAV_AddPrototypeTarget(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.Targets.push_back(string(Value));

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->Targets.push_back(string(Value));
	}
}

void NAV_SetPrototypeGlobalState(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.GlobalState = Value;

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->GlobalState = Value;
	}
}

void NAV_SetPrototypeTriggerState(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.TriggerState = atoi(Value);

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->TriggerState = atoi(Value);
	}
}

void NAV_SetPrototypeTriggerMode(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.TriggerMode = atoi(Value);

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->TriggerMode = atoi(Value);
	}
}

void NAV_SetPrototypeHealth(int EntityIndex, char* Value)
{
	if (!Value || strlen(Value) == 0) { return; }

	DynamicMapPrototype* thePrototype = nullptr;

	for (auto it = MapObjectPrototypes.begin(); it != MapObjectPrototypes.end(); it++)
	{
		if (it->EdictIndex == EntityIndex)
		{
			thePrototype = &(*it);
		}
	}

	if (!thePrototype)
	{
		DynamicMapPrototype NewPrototype;
		NewPrototype.EdictIndex = EntityIndex;
		NewPrototype.Edict = INDEXENT(EntityIndex);
		NewPrototype.Health = atof(Value);

		MapObjectPrototypes.push_back(NewPrototype);
	}
	else
	{
		thePrototype->Health = atof(Value);
	}
}

DynamicMapObject* NAV_GetTriggerByEdict(edict_t* Edict)
{
	for (auto it = DynamicMapObjects.begin(); it != DynamicMapObjects.end(); it++)
	{
		if (it->Edict == Edict)
		{
			return &(*it);
		}
	}

	return nullptr;
}

DynamicMapObject* NAV_GetTriggerReachableFromPlatform(float LiftHeight, DynamicMapObject* Platform, const Vector PlatformPosition)
{
	Vector CheckPlatformLocation = (vIsZero(PlatformPosition)) ? UTIL_GetCentreOfEntity(Platform->Edict) : PlatformPosition;

	for (auto it = Platform->Triggers.begin(); it != Platform->Triggers.end(); it++)
	{
		DynamicMapObject* ThisTrigger = UTIL_GetDynamicObjectByEdict((*it));

		if (!ThisTrigger || !ThisTrigger->bIsActive) { continue; }

		Vector ClosestPointOnButton = UTIL_GetClosestPointOnEntityToLocation(CheckPlatformLocation, ThisTrigger->Edict);
		Vector ClosestPointOnLift = UTIL_GetClosestPointOnEntityToLocation(ClosestPointOnButton, Platform->Edict, CheckPlatformLocation);

		if (vDist2DSq(ClosestPointOnButton, ClosestPointOnLift) < sqrf(max_player_use_reach) && fabsf(LiftHeight - ClosestPointOnButton.z) < 64.0f)
		{
			return ThisTrigger;
		}
	}

	return nullptr;
}

DynamicMapObject* NAV_GetBestTriggerForObject(DynamicMapObject* ObjectToActivate, Vector ActivateLocation, const NavAgentProfile& NavProfile)
{
	if (ObjectToActivate->Triggers.size() == 0 || !ObjectToActivate || vIsZero(ActivateLocation)) { return nullptr; }

	DynamicMapObject* WinningTrigger = nullptr;

	Vector FromLoc = ActivateLocation;

	float MinDist = FLT_MAX;

	// This object is triggered by itself, such as a door set to USE_ONLY or a func_plat which needs to be touched to activate
	if (ObjectToActivate->Triggers.size() == 1 && ObjectToActivate->Triggers[0] == ObjectToActivate->Edict) { return UTIL_GetDynamicObjectByEdict(ObjectToActivate->Triggers[0]); }

	for (auto it = ObjectToActivate->Triggers.begin(); it != ObjectToActivate->Triggers.end(); it++)
	{
		DynamicMapObject* ThisTrigger = UTIL_GetDynamicObjectByEdict((*it));

		if (!ThisTrigger || !ThisTrigger->bIsActive) { continue; }

		// For triggers we can activate from a distance and are in our LOS, short-cut and add them to the list
		if (ThisTrigger->Type == TRIGGER_SHOOT || ThisTrigger->Type == TRIGGER_BREAK)
		{
			TraceResult hit;

			UTIL_TraceLine(ActivateLocation + Vector(0.0f, 0.0f, 5.0f), UTIL_GetCentreOfEntity(ThisTrigger->Edict), ignore_monsters, ignore_glass, nullptr, &hit);

			if (hit.pHit == ThisTrigger->Edict)
			{
				float ThisDist = vDist3DSq(FromLoc, UTIL_GetCentreOfEntity(ThisTrigger->Edict));

				if (ThisDist < MinDist)
				{
					WinningTrigger = ThisTrigger;
					MinDist = ThisDist;
				}

				continue;
			}
		}

		Vector TriggerLocation = UTIL_GetButtonFloorLocation(NavProfile, FromLoc, ThisTrigger->Edict);

		if (vIsZero(TriggerLocation))
		{
			TriggerLocation = UTIL_GetClosestPointOnEntityToLocation(FromLoc, ThisTrigger->Edict);
		}

		float MaxDist = (ThisTrigger->Type == TRIGGER_BREAK || ThisTrigger->Type == TRIGGER_SHOOT) ? UTIL_MetresToGoldSrcUnits(5.0f) : 64.0f;

		if (!UTIL_PointIsReachable(NavProfile, FromLoc, TriggerLocation, MaxDist)) { continue; }

		if (ObjectToActivate->Type != MAPOBJECT_PLATFORM)
		{
			if (UTIL_IsPathBlockedByObject(NavProfile, FromLoc, TriggerLocation, ObjectToActivate)) { continue; }
		}
		else
		{
			vector<bot_path_node> CheckPath;

			dtStatus PathFindStatus = FindPathClosestToPoint(NavProfile, FromLoc, TriggerLocation, CheckPath, MaxDist);

			if (!dtStatusSucceed(PathFindStatus)) { continue; }

			bool bOtherSideOfLift = false;

			for (auto pathIt = CheckPath.begin(); pathIt != CheckPath.end(); pathIt++)
			{
				if (pathIt->flag & NAV_FLAG_PLATFORM)
				{
					if (UTIL_GetClosestPlatformToPoints(pathIt->FromLocation, pathIt->Location) == ObjectToActivate)
					{
						bOtherSideOfLift = true;
						break;
					}
				}
			}

			if (bOtherSideOfLift) { continue; }
		}

		float ThisDist = vDist3DSq(FromLoc, TriggerLocation);

		if (ThisDist < MinDist)
		{
			WinningTrigger = ThisTrigger;
		}
	}

	return WinningTrigger;
}

DynamicMapObject* NAV_GetBestTriggerForObject(DynamicMapObject* ObjectToActivate, edict_t* PlayerToTrigger, const NavAgentProfile& NavProfile)
{
	if (ObjectToActivate->Triggers.size() == 0 || !ObjectToActivate || FNullEnt(PlayerToTrigger)) { return nullptr; }

	DynamicMapObject* WinningTrigger = nullptr;

	Vector FromLoc = GetPlayerBottomOfCollisionHull(PlayerToTrigger);

	float MinDist = FLT_MAX;

	// This object is triggered by itself, such as a door set to USE_ONLY or a func_plat which needs to be touched to activate
	if (ObjectToActivate->Triggers.size() == 1 && ObjectToActivate->Triggers[0] == ObjectToActivate->Edict) { return UTIL_GetDynamicObjectByEdict(ObjectToActivate->Triggers[0]); }

	for (auto it = ObjectToActivate->Triggers.begin(); it != ObjectToActivate->Triggers.end(); it++)
	{
		DynamicMapObject* ThisTrigger = UTIL_GetDynamicObjectByEdict((*it));

		if (!ThisTrigger || !ThisTrigger->bIsActive) { continue; }

		// For triggers we can activate from a distance and are in our LOS, short-cut and add them to the list
		if (ThisTrigger->Type == TRIGGER_SHOOT || ThisTrigger->Type == TRIGGER_BREAK)
		{
			if (UTIL_PlayerHasLOSToEntity(PlayerToTrigger, ThisTrigger->Edict, UTIL_MetresToGoldSrcUnits(20.0f), false))
			{
				float ThisDist = vDist3DSq(FromLoc, UTIL_GetCentreOfEntity(ThisTrigger->Edict));

				if (ThisDist < MinDist)
				{
					WinningTrigger = ThisTrigger;
					MinDist = ThisDist;
				}

				continue;
			}
		}

		Vector TriggerLocation = UTIL_GetButtonFloorLocation(NavProfile, FromLoc, ThisTrigger->Edict);

		if (vIsZero(TriggerLocation))
		{
			TriggerLocation = UTIL_GetClosestPointOnEntityToLocation(FromLoc, ThisTrigger->Edict);
		}

		float MaxDist = (ThisTrigger->Type == TRIGGER_BREAK || ThisTrigger->Type == TRIGGER_SHOOT) ? UTIL_MetresToGoldSrcUnits(5.0f) : 64.0f;

		if (!UTIL_PointIsReachable(NavProfile, FromLoc, TriggerLocation, MaxDist)) { continue; }

		if (ObjectToActivate->Type != MAPOBJECT_PLATFORM)
		{
			if (UTIL_IsPathBlockedByObject(NavProfile, FromLoc, TriggerLocation, ObjectToActivate)) { continue; }
		}
		else
		{
			vector<bot_path_node> CheckPath;

			dtStatus PathFindStatus = FindPathClosestToPoint(NavProfile, FromLoc, TriggerLocation, CheckPath, MaxDist);

			if (!dtStatusSucceed(PathFindStatus)) { continue; }

			bool bOtherSideOfLift = false;

			for (auto pathIt = CheckPath.begin(); pathIt != CheckPath.end(); pathIt++)
			{
				if (pathIt->flag & NAV_FLAG_PLATFORM)
				{
					if (UTIL_GetClosestPlatformToPoints(pathIt->FromLocation, pathIt->Location) == ObjectToActivate) 
					{
						bOtherSideOfLift = true;
						break;
					}
				}
			}

			if (bOtherSideOfLift) { continue; }
		}

		float ThisDist = vDist3DSq(FromLoc, TriggerLocation);

		if (ThisDist < MinDist)
		{
			WinningTrigger = ThisTrigger;
		}
	}

	return WinningTrigger;
}

void DEBUG_PrintObjectInfo(DynamicMapObject* Object)
{
	char buf[511];
	char interbuf[164];

	sprintf(buf, "Info for %s:\n\n", (Object->Edict->v.targetname != 0) ? STRING(Object->Edict->v.targetname) : STRING(Object->Edict->v.classname));

	string CurrentType;

	switch (Object->Type)
	{
	case MAPOBJECT_STATIC:
		CurrentType = "Static";
		break;
	case MAPOBJECT_DOOR:
		CurrentType = "Door";
		break;
	case MAPOBJECT_PLATFORM:
		CurrentType = "Platform";
		break;
	default:
		CurrentType = "Invalid";
		break;
	}

	sprintf(interbuf, "Type: %s\n", CurrentType.c_str());
	strcat(buf, interbuf);

	string CurrentState;

	switch (Object->State)
	{
		case OBJECTSTATE_IDLE:
			CurrentState = "Idle";
			break;
		case OBJECTSTATE_PREPARING:
			CurrentState = "Activated";
			break;
		case OBJECTSTATE_MOVING:
			CurrentState = "Moving";
			break;
		case OBJECTSTATE_OPEN:
			CurrentState = "Open";
			break;
		default:
			CurrentState = "Invalid";
			break;
	}

	sprintf(interbuf, "State: %s\n", CurrentState.c_str());
	strcat(buf, interbuf);

	sprintf(interbuf, "Is Active: %s\n", (Object->bIsActive) ? "True" : "False");
	strcat(buf, interbuf);

	if (!vIsZero(DEBUG_GetDebugVector1()))
	{
		bool bShouldActivate = NAV_PlatformNeedsActivating(nullptr, Object, AIMGR_GetListenServerEdict()->v.origin, DEBUG_GetDebugVector1());

		sprintf(interbuf, "Needs Activating: %s\n", (bShouldActivate) ? "True" : "False");
		strcat(buf, interbuf);
	}

	UTIL_DrawHUDText(AIMGR_GetListenServerEdict(), 0, 0.1, 0.1f, 255, 255, 255, buf);

	Vector ObjectLocation = UTIL_GetCentreOfEntity(Object->Edict);

	Vector NextStop = Object->StopPoints[Object->NextStopIndex].StopLocation;

	UTIL_DrawLine(AIMGR_GetListenServerEdict(), ObjectLocation, NextStop, 0, 0, 255);

	sprintf(buf, "Trigger States:\n\n");

	if (Object->Triggers.size() == 0)
	{
		sprintf(interbuf, "NONE");
		strcat(buf, interbuf);
		UTIL_DrawHUDText(AIMGR_GetListenServerEdict(), 1, 0.6, 0.1f, 255, 255, 255, buf);
		return;
	}

	for (auto it = Object->Triggers.begin(); it != Object->Triggers.end(); it++)
	{
		DynamicMapObject* ThisTrigger = UTIL_GetDynamicObjectByEdict((*it));

		if (!ThisTrigger) { continue; }

		sprintf(interbuf, "Trigger %s (%s):\n", (ThisTrigger->Edict->v.targetname != 0) ? STRING(ThisTrigger->Edict->v.targetname) : "Unnamed", STRING(ThisTrigger->Edict->v.classname));
		strcat(buf, interbuf);

		switch (ThisTrigger->State)
		{
		case OBJECTSTATE_IDLE:
			CurrentState = "Idle";
			break;
		case OBJECTSTATE_PREPARING:
			CurrentState = "Activated";
			break;
		case OBJECTSTATE_MOVING:
			CurrentState = "Moving";
			break;
		case OBJECTSTATE_OPEN:
			CurrentState = "Open";
			break;
		default:
			CurrentState = "Invalid";
			break;
		}

		sprintf(interbuf, "State: %s\n", CurrentState.c_str());
		strcat(buf, interbuf);

		switch (ThisTrigger->Type)
		{
		case TRIGGER_TOUCH:
			CurrentState = "Touch";
			break;
		case TRIGGER_USE:
			CurrentState = "Use";
			break;
		case TRIGGER_SHOOT:
			CurrentState = "Shoot";
			break;
		case TRIGGER_BREAK:
			CurrentState = "Break";
			break;
		case MAPOBJECT_PLATFORM:
			CurrentState = "Touch";
			break;
		case MAPOBJECT_DOOR:
			CurrentState = "Use";
			break;
		default:
			CurrentState = "Other";
			break;
		}

		sprintf(interbuf, "Activation Method: %s\n", CurrentState.c_str());
		strcat(buf, interbuf);

		sprintf(interbuf, "Is Active: %s\n\n", (ThisTrigger->bIsActive) ? "True" : "False");
		strcat(buf, interbuf);
	}

	UTIL_DrawHUDText(AIMGR_GetListenServerEdict(), 1, 0.6, 0.1f, 255, 255, 255, buf);

	DynamicMapObject* BestTrigger = NAV_GetBestTriggerForObject(Object, AIMGR_GetListenServerEdict(), GetBaseAgentProfile(NAV_PROFILE_DEFAULT));

	if (BestTrigger)
	{
		UTIL_DrawLine(AIMGR_GetListenServerEdict(), AIMGR_GetListenServerEdict()->v.origin, UTIL_GetCentreOfEntity(BestTrigger->Edict), 0, 128, 0);

		Vector TriggerFloorLoc = UTIL_GetButtonFloorLocation(GetBaseAgentProfile(NAV_PROFILE_DEFAULT), INDEXENT(1)->v.origin, BestTrigger->Edict);

		UTIL_DrawLine(AIMGR_GetListenServerEdict(), AIMGR_GetListenServerEdict()->v.origin, UTIL_GetButtonFloorLocation(GetBaseAgentProfile(NAV_PROFILE_DEFAULT), AIMGR_GetListenServerEdict()->v.origin, BestTrigger->Edict), 0, 0, 255);
	}

	if (AIMGR_GetListenServerEdict()->v.groundentity == Object->Edict)
	{
		Vector NearestDisembark = NAV_GetNearestPlatformDisembarkPoint(GetBaseAgentProfile(NAV_PROFILE_DEFAULT), AIMGR_GetListenServerEdict(), Object);

		if (!vIsZero(NearestDisembark))
		{
			UTIL_DrawLine(AIMGR_GetListenServerEdict(), AIMGR_GetListenServerEdict()->v.origin, NearestDisembark, 255, 255, 0);
		}
	}


}

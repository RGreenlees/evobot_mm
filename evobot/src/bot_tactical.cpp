//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_gorge.cpp
// 
// Contains gorge-related functions. Needs refactoring into helper function file
//

#include "bot_tactical.h"
#include "bot_navigation.h"
#include "bot_config.h"
#include <unordered_map>

#include <meta_api.h>

resource_node ResourceNodes[64];
int NumTotalResNodes;

dropped_marine_item AllMarineItems[256];
int NumTotalMarineItems;

hive_definition Hives[10];
int NumTotalHives;

map_location MapLocations[64];
int NumMapLocations;


extern edict_t* clients[32];

extern bot_t bots[32];

extern bool bGameIsActive;

std::unordered_map<edict_t*, buildable_structure> MarineBuildableStructureMap;

std::unordered_map<edict_t*, buildable_structure> AlienBuildableStructureMap;


void PopulateEmptyHiveList()
{
	memset(Hives, 0, sizeof(Hives));
	NumTotalHives = 0;

	edict_t* currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_hive")) != NULL) && (!FNullEnt(currStructure)))
	{
		Hives[NumTotalHives].edict = currStructure;
		Hives[NumTotalHives].Location = currStructure->v.origin;
		Hives[NumTotalHives].FloorLocation = UTIL_GetFloorUnderEntity(currStructure);

		if (Hives[NumTotalHives].HiveResNodeIndex < 0)
		{
			Hives[NumTotalHives].HiveResNodeIndex = UTIL_FindNearestResNodeIndexToLocation(currStructure->v.origin);
		}

		NumTotalHives++;
	}
}

const dropped_marine_item* UTIL_GetNearestEquipment(const Vector Location, const float SearchDist)
{
	float SearchDistSq = sqrf(SearchDist);
	int Result = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalMarineItems; i++)
	{
		if (FNullEnt(AllMarineItems[i].edict) || (AllMarineItems[i].edict->v.effects & EF_NODRAW) || !AllMarineItems[i].bOnNavMesh) { continue; }

		if (AllMarineItems[i].ItemType != ITEM_MARINE_JETPACK && AllMarineItems[i].ItemType != ITEM_MARINE_HEAVYARMOUR) { continue; }

		float DistSq = vDist2DSq(AllMarineItems[i].Location, Location);

		if (DistSq < SearchDistSq && (Result < 0 || DistSq < MinDist))
		{
			Result = i;
			MinDist = DistSq;
		}
	}

	if (Result > -1)
	{
		return &AllMarineItems[Result];
	}

	return nullptr;
}

void SetNumberofHives(int NewValue)
{
	NumTotalHives = NewValue;

	for (int i = 0; i < NumTotalHives; i++)
	{
		Hives[i].bIsValid = true;
	}
}

void SetHiveLocation(int HiveIndex, const Vector NewLocation)
{
	Hives[HiveIndex].Location = NewLocation;

	edict_t* ClosestHive = NULL;
	float minDist = 0.0f;

	edict_t* currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_hive")) != NULL) && (!FNullEnt(currStructure)))
	{
		float Dist = vDist3DSq(currStructure->v.origin, NewLocation);

		if (!ClosestHive || Dist < minDist)
		{
			ClosestHive = currStructure;
			minDist = Dist;
		}
	}

	Hives[HiveIndex].edict = ClosestHive;
	Hives[HiveIndex].FloorLocation = UTIL_GetFloorUnderEntity(ClosestHive);

	if (Hives[HiveIndex].HiveResNodeIndex < 0)
	{
		Hives[HiveIndex].HiveResNodeIndex = UTIL_FindNearestResNodeIndexToLocation(NewLocation);
	}

}

void SetHiveStatus(int HiveIndex, int NewStatus)
{

	switch (NewStatus)
	{
	case kHiveInfoStatusUnbuilt:
		Hives[HiveIndex].Status = HIVE_STATUS_UNBUILT;
		break;
	case kHiveInfoStatusBuildingStage1:
	case kHiveInfoStatusBuildingStage2:
	case kHiveInfoStatusBuildingStage3:
	case kHiveInfoStatusBuildingStage4:
	case kHiveInfoStatusBuildingStage5:
		Hives[HiveIndex].Status = HIVE_STATUS_BUILDING;
		break;
	case kHiveInfoStatusBuilt:
		Hives[HiveIndex].Status = HIVE_STATUS_BUILT;
		break;
	default: break;
	}

	if (Hives[HiveIndex].Status != HIVE_STATUS_UNBUILT && Hives[HiveIndex].ObstacleRef == 0)
	{
		Hives[HiveIndex].ObstacleRef = UTIL_AddTemporaryObstacle(UTIL_GetCentreOfEntity(Hives[HiveIndex].edict), 125.0f, 250.0f, DT_AREA_NULL);
	}

	if (Hives[HiveIndex].Status == HIVE_STATUS_UNBUILT && Hives[HiveIndex].ObstacleRef > 0)
	{
		UTIL_RemoveTemporaryObstacle(Hives[HiveIndex].ObstacleRef);
		Hives[HiveIndex].ObstacleRef = 0;
	}
}

void SetHiveTechStatus(int HiveIndex, int NewTechStatus)
{
	switch (NewTechStatus)
	{
	case 0:
		Hives[HiveIndex].TechStatus = HIVE_TECH_NONE;
		break;
	case 1:
		Hives[HiveIndex].TechStatus = HIVE_TECH_DEFENCE;
		break;
	case 2:
		Hives[HiveIndex].TechStatus = HIVE_TECH_SENSORY;
		break;
	case 3:
		Hives[HiveIndex].TechStatus = HIVE_TECH_MOVEMENT;
		break;
	default: Hives[HiveIndex].TechStatus = HIVE_TECH_NONE; break;
	}
}

void SetHiveUnderAttack(int HiveIndex, bool bNewUnderAttack)
{
	Hives[HiveIndex].bIsUnderAttack = bNewUnderAttack;
}

void SetHiveHealthPercent(int HiveIndex, float NewHealthPercent)
{
	Hives[HiveIndex].HealthPercent = NewHealthPercent;
}

buildable_structure* UTIL_GetBuildableStructureRefFromEdict(const edict_t* Structure)
{
	NSStructureType StructureType = UTIL_GetStructureTypeFromEdict(Structure);

	if (StructureType == STRUCTURE_NONE) { return nullptr; }

	if (UTIL_IsMarineStructure(StructureType))
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (it.first == Structure)
			{
				return &it.second;
			}
		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (it.first == Structure)
			{
				return &it.second;
			}
		}
	}

	return nullptr;
}

edict_t* UTIL_GetClosestStructureAtLocation(const Vector& Location, bool bMarineStructures)
{
	edict_t* Result = NULL;
	float MinDist = 0.0f;

	if (bMarineStructures)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			float thisDist = vDist2DSq(it.second.Location, Location);

			if (!Result || thisDist < MinDist)
			{
				Result = it.first;
				MinDist = thisDist;
			}

		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			float thisDist = vDist2DSq(it.second.Location, Location);

			if (!Result || thisDist < MinDist)
			{
				Result = it.first;
				MinDist = thisDist;
			}

		}

	}

	return Result;
}

edict_t* UTIL_GetNearestItemOfType(const NSDeployableItem ItemType, const Vector Location, const float SearchDist)
{
	float SearchDistSq = sqrf(SearchDist);
	edict_t* Result = nullptr;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalMarineItems; i++)
	{
		if (AllMarineItems[i].ItemType != ItemType || FNullEnt(AllMarineItems[i].edict) || (AllMarineItems[i].edict->v.effects & EF_NODRAW) || !AllMarineItems[i].bOnNavMesh) { continue; }

		float DistSq = vDist2DSq(AllMarineItems[i].Location, Location);

		if (DistSq < SearchDistSq && (!Result || DistSq < MinDist))
		{
			Result = AllMarineItems[i].edict;
			MinDist = DistSq;
		}
	}

	return Result;
}

edict_t* UTIL_GetNearestUnbuiltStructureWithLOS(bot_t* pBot, const Vector Location, const float SearchDist, const int Team)
{
	edict_t* Result = nullptr;
	float MaxDist = sqrf(SearchDist);
	float MinDist = 0.0f;

	if (Team == MARINE_TEAM)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }

			if (it.second.bFullyConstructed) { continue; }

			if (!UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, it.second.Location)) { continue; }

			float thisDist = vDist2DSq(Location, it.second.Location);

			if (thisDist < MaxDist && (!Result || thisDist < MinDist))
			{
				Result = it.first;
				MinDist = thisDist;
			}
		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }

			if (it.second.bFullyConstructed) { continue; }

			if (!UTIL_PointIsDirectlyReachable(pBot->pEdict->v.origin, it.second.Location)) { continue; }

			float thisDist = vDist2DSq(Location, it.second.Location);

			if (thisDist < MaxDist && (!Result || thisDist < MinDist))
			{
				Result = it.first;
				MinDist = thisDist;
			}
		}
	}

	return Result;
}

const dropped_marine_item* UTIL_GetNearestItemIndexOfType(const NSDeployableItem ItemType, const Vector Location, const float SearchDist)
{
	float SearchDistSq = sqrf(SearchDist);
	int Result = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalMarineItems; i++)
	{
		if (AllMarineItems[i].ItemType != ItemType || FNullEnt(AllMarineItems[i].edict) || (AllMarineItems[i].edict->v.effects & EF_NODRAW) || !AllMarineItems[i].bOnNavMesh) { continue; }

		float DistSq = vDist2DSq(AllMarineItems[i].Location, Location);

		if (DistSq < SearchDistSq && (Result < 0 || DistSq < MinDist))
		{
			Result = i;
			MinDist = DistSq;
		}
	}

	if (Result > -1)
	{
		return &AllMarineItems[Result];
	}

	return nullptr;
}

const dropped_marine_item* UTIL_GetNearestSpecialPrimaryWeapon(const Vector Location, const float SearchDist)
{
	float SearchDistSq = sqrf(SearchDist);
	int Result = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalMarineItems; i++)
	{
		if (FNullEnt(AllMarineItems[i].edict) || (AllMarineItems[i].edict->v.effects & EF_NODRAW) || !AllMarineItems[i].bOnNavMesh) { continue; }

		if (AllMarineItems[i].ItemType != ITEM_MARINE_HMG && AllMarineItems[i].ItemType != ITEM_MARINE_SHOTGUN && AllMarineItems[i].ItemType != ITEM_MARINE_GRENADELAUNCHER) { continue; }

		float DistSq = vDist2DSq(AllMarineItems[i].Location, Location);

		if (DistSq < SearchDistSq && (Result < 0 || DistSq < MinDist))
		{
			Result = i;
			MinDist = DistSq;
		}
	}

	if (Result > -1)
	{
		return &AllMarineItems[Result];
	}

	return nullptr;
}

char* UTIL_GetClosestMapLocationToPoint(const Vector Point)
{
	if (NumMapLocations == 0)
	{
		return "";
	}

	for (int i = 0; i < NumMapLocations; i++)
	{
		if (Point.x >= MapLocations[i].MinLocation.x && Point.y >= MapLocations[i].MinLocation.y
			&& Point.x <= MapLocations[i].MaxLocation.x && Point.y <= MapLocations[i].MaxLocation.y)
		{
			return MapLocations[i].LocationName;
		}
	}

	int ClosestIndex = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumMapLocations; i++)
	{
		Vector CentrePoint = MapLocations[i].MinLocation + ((MapLocations[i].MaxLocation - MapLocations[i].MinLocation) * 0.5f);

		float ThisDist = vDist2DSq(Point, CentrePoint);

		if (ClosestIndex < 0 || ThisDist < MinDist)
		{
			ClosestIndex = i;
			MinDist = ThisDist;
		}
	}

	if (ClosestIndex > -1)
	{
		return MapLocations[ClosestIndex].LocationName;
	}
	else
	{
		return "";
	}
}

void AddMapLocation(const char* LocationName, Vector MinLocation, Vector MaxLocation)
{
	for (int i = 0; i < 64; i++)
	{
		if (!MapLocations[i].bIsValid)
		{
			sprintf(MapLocations[i].LocationName, "%s", UTIL_LookUpLocationName(LocationName));
			MapLocations[i].MinLocation = MinLocation;
			MapLocations[i].MaxLocation = MaxLocation;
			MapLocations[i].bIsValid = true;
			NumMapLocations = i + 1;
			return;
		}
		else
		{
			if (FStrEq(LocationName, MapLocations[i].LocationName))
			{
				return;
			}
		}
	}
}

void PrintHiveInfo()
{
	FILE* HiveLog = fopen("HiveInfo.txt", "w+");

	if (!HiveLog) { return; }

	for (int i = 0; i < NumTotalHives; i++)
	{
		fprintf(HiveLog, "Hive: %d\n", i);
		fprintf(HiveLog, "Hive Location: %f, %f, %f\n", Hives[i].Location.x, Hives[i].Location.y, Hives[i].Location.z);
		fprintf(HiveLog, "Hive Edict Location: %f, %f, %f\n", Hives[i].edict->v.origin.x, Hives[i].edict->v.origin.y, Hives[i].edict->v.origin.z);

		switch (Hives[i].Status)
		{
		case HIVE_STATUS_UNBUILT:
			fprintf(HiveLog, "Hive Status: Unbuilt\n");
			break;
		case HIVE_STATUS_BUILDING:
			fprintf(HiveLog, "Hive Status: Building\n");
			break;
		case HIVE_STATUS_BUILT:
			fprintf(HiveLog, "Hive Status: Built\n");
			break;
		default: break;
		}

		switch (Hives[i].TechStatus)
		{
		case HIVE_TECH_NONE:
			fprintf(HiveLog, "Hive Tech: None\n");
			break;
		case HIVE_TECH_DEFENCE:
			fprintf(HiveLog, "Hive Tech: Defence\n");
			break;
		case HIVE_TECH_MOVEMENT:
			fprintf(HiveLog, "Hive Tech: Movement\n");
			break;
		case HIVE_TECH_SENSORY:
			fprintf(HiveLog, "Hive Tech: Sensory\n");
			break;
		default: break;
		}

		fprintf(HiveLog, "Hive Under Attack: %s\n", (Hives[i].bIsUnderAttack) ? "True" : "False");
	}

	fflush(HiveLog);
	fclose(HiveLog);
}

void UTIL_RefreshBuildableStructures()
{
	edict_t* currStructure = NULL;

	// Marine Structures
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_command")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "resourcetower")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_infportal")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_armory")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_turretfactory")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_advturretfactory")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "siegeturret")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "turret")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_advarmory")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_armslab")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_prototypelab")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_observatory")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "phasegate")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}


	// Alien Structures
	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "alienresourcetower")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "defensechamber")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "offensechamber")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "movementchamber")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "sensorychamber")) != NULL) && (!FNullEnt(currStructure)))
	{
		UTIL_UpdateBuildableStructure(currStructure);
	}

	for (auto x : MarineBuildableStructureMap)
	{
		if (x.second.LastSeen < StructureRefreshFrame)
		{
			if (x.second.StructureType == STRUCTURE_MARINE_RESTOWER)
			{
				for (int i = 0; i < NumTotalResNodes; i++)
				{
					if (ResourceNodes[i].TowerEdict == x.first)
					{
						ResourceNodes[i].TowerEdict = nullptr;
						ResourceNodes[i].bIsOccupied = false;
						ResourceNodes[i].bIsOwnedByMarines = false;
					}
				}
			}

			UTIL_OnStructureDestroyed(x.second.StructureType, x.second.Location);
			UTIL_RemoveTemporaryObstacle(x.second.ObstacleRef);
			MarineBuildableStructureMap.erase(x.first);
		}
	}

	for (auto x : AlienBuildableStructureMap)
	{
		if (x.second.LastSeen < StructureRefreshFrame)
		{
			if (x.second.StructureType == STRUCTURE_ALIEN_RESTOWER)
			{
				for (int i = 0; i < NumTotalResNodes; i++)
				{
					if (ResourceNodes[i].TowerEdict == x.first)
					{
						ResourceNodes[i].TowerEdict = nullptr;
						ResourceNodes[i].bIsOccupied = false;
						ResourceNodes[i].bIsOwnedByMarines = false;
					}
				}
			}

			UTIL_OnStructureDestroyed(x.second.StructureType, x.second.Location);
			UTIL_RemoveTemporaryObstacle(x.second.ObstacleRef);
			AlienBuildableStructureMap.erase(x.first);
		}
	}

	StructureRefreshFrame++;
}

void UTIL_OnStructureCreated(buildable_structure* NewStructure)
{
	NSStructureType StructureType = UTIL_GetStructureTypeFromEdict(NewStructure->edict);

	bool bIsMarineStructure = UTIL_IsMarineStructure(StructureType);

	if (bGameIsActive)
	{
		if (bIsMarineStructure)
		{
			for (int i = 0; i < 32; i++)
			{
				if (clients[i] && IsPlayerBot(clients[i]) && IsPlayerCommander(clients[i]))
				{
					bot_t* BotRef = UTIL_GetBotPointer(clients[i]);

					if (BotRef)
					{
						UTIL_LinkPlacedStructureToAction(BotRef, NewStructure);
					}
				}
			}
		}
		else
		{
			for (int i = 0; i < 32; i++)
			{
				if (clients[i] && IsPlayerOnAlienTeam(clients[i]) && IsPlayerBot(clients[i]))
				{
					bot_t* BotRef = UTIL_GetBotPointer(clients[i]);

					if (BotRef)
					{
						UTIL_LinkAlienStructureToTask(BotRef, NewStructure->edict);
					}
				}
			}
		}
	}

	if (StructureType == STRUCTURE_MARINE_RESTOWER || StructureType == STRUCTURE_ALIEN_RESTOWER)
	{

		int NearestResNodeIndex = UTIL_FindNearestResNodeIndexToLocation(NewStructure->edict->v.origin);

		if (NearestResNodeIndex > -1)
		{
			ResourceNodes[NearestResNodeIndex].bIsOccupied = true;
			ResourceNodes[NearestResNodeIndex].bIsOwnedByMarines = bIsMarineStructure;
			ResourceNodes[NearestResNodeIndex].TowerEdict = NewStructure->edict;
			ResourceNodes[NearestResNodeIndex].bIsMarineBaseNode = false;
		}
	}

}

void UTIL_LinkPlacedStructureToAction(bot_t* CommanderBot, buildable_structure* NewStructure)
{
	if (FNullEnt(NewStructure->edict)) { return; }

	NSStructureType StructureType = UTIL_GetStructureTypeFromEdict(NewStructure->edict);

	for (int Priority = 0; Priority < MAX_ACTION_PRIORITIES; Priority++)
	{
		for (int ActionIndex = 0; ActionIndex < MAX_PRIORITY_ACTIONS; ActionIndex++)
		{
			commander_action* action = &CommanderBot->CurrentCommanderActions[Priority][ActionIndex];
			if (!action->bIsActive || action->ActionType != ACTION_BUILD || !action->bHasAttemptedAction || !FNullEnt(action->StructureOrItem) || !UTIL_StructureTypesMatch(StructureType, action->StructureToBuild)) { continue; }

			if (vDist2DSq(NewStructure->edict->v.origin, action->BuildLocation) < sqrf(UTIL_MetresToGoldSrcUnits(10.0f)))
			{
				action->StructureOrItem = NewStructure->edict;
				NewStructure->LastSuccessfulCommanderLocation = action->LastAttemptedCommanderLocation;
				NewStructure->LastSuccessfulCommanderAngle = action->LastAttemptedCommanderAngle;

			}
		}

	}
}

void UTIL_OnStructureDestroyed(const NSStructureType Structure, const Vector Location)
{
	if (Structure == STRUCTURE_MARINE_RESTOWER || Structure == STRUCTURE_ALIEN_RESTOWER)
	{
		int NearestResNodeIndex = UTIL_FindNearestResNodeIndexToLocation(Location);

		if (NearestResNodeIndex > -1)
		{
			ResourceNodes[NearestResNodeIndex].bIsOccupied = false;
			ResourceNodes[NearestResNodeIndex].bIsOwnedByMarines = false;
			ResourceNodes[NearestResNodeIndex].TowerEdict = nullptr;
		}
	}

	for (int i = 0; i < 32; i++)
	{
		if (bots[i].is_used)
		{
			BotNotifyStructureDestroyed(&bots[i], Structure, Location);
		}
	}
}

bool UTIL_AlienResNodeNeedsReinforcing(int ResNodeIndex)
{
	if (ResNodeIndex < 0 || ResNodeIndex > NumTotalResNodes - 1) { return false; }

	int NumOffenceChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_OFFENCECHAMBER, ResourceNodes[ResNodeIndex].origin, UTIL_MetresToGoldSrcUnits(5.0f));

	if (NumOffenceChambers < 2) { return true; }

	if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_DEFENCE))
	{
		int NumDefenceChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_DEFENCECHAMBER, ResourceNodes[ResNodeIndex].origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumDefenceChambers < 2) { return true; }
	}

	if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_MOVEMENT))
	{
		int NumMovementChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_MOVEMENTCHAMBER, ResourceNodes[ResNodeIndex].origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumMovementChambers < 1) { return true; }
	}

	if (UTIL_ActiveHiveWithTechExists(HIVE_TECH_SENSORY))
	{
		int NumSensoryChambers = UTIL_GetNumPlacedStructuresOfTypeInRadius(STRUCTURE_ALIEN_SENSORYCHAMBER, ResourceNodes[ResNodeIndex].origin, UTIL_MetresToGoldSrcUnits(5.0f));

		if (NumSensoryChambers < 1) { return true; }
	}

	return false;
}

const resource_node* UTIL_GetNearestUnprotectedResNode(const Vector Location)
{
	int result = -1;
	float minDist = 0.0f;
	float MaxDistToClaim = sqrf(UTIL_MetresToGoldSrcUnits(10.0f));

	for (int i = 0; i < NumTotalResNodes; i++)
	{
		if (!ResourceNodes[i].edict || !ResourceNodes[i].bIsOccupied || ResourceNodes[i].bIsOwnedByMarines) { continue; }

		if (!UTIL_AlienResNodeNeedsReinforcing(i)) { continue; }

		float thisDist = vDist2DSq(ResourceNodes[i].edict->v.origin, Location);

		if (result < 0 || thisDist < minDist)
		{
			result = i;
			minDist = thisDist;
		}
	}

	if (result > -1)
	{
		return &ResourceNodes[result];
	}

	return nullptr;
}

const hive_definition* UTIL_GetClosestViableUnbuiltHive(const Vector SearchLocation)
{
	int Result = -1;
	float MinDist = 0.0f;


	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].bIsValid && Hives[i].Status == HIVE_STATUS_UNBUILT)
		{
			if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_PHASEGATE, Hives[i].Location, UTIL_MetresToGoldSrcUnits(15.0f))) { continue; }

			if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_TURRETFACTORY, Hives[i].Location, UTIL_MetresToGoldSrcUnits(15.0f))) { continue; }

			float ThisDist = vDist2DSq(SearchLocation, Hives[i].Location);

			if (Result < 0 || ThisDist < MinDist)
			{
				Result = i;
				MinDist = ThisDist;
			}
		}
	}

	if (Result > -1)
	{
		return &Hives[Result];
	}

	return nullptr;
}

const hive_definition* UTIL_GetFirstHiveWithoutTech()
{
	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].Status == HIVE_STATUS_BUILT && Hives[i].TechStatus == HIVE_TECH_NONE) { return &Hives[i]; }
	}

	return nullptr;
}

const hive_definition* UTIL_GetHiveWithTech(HiveTechStatus Tech)
{
	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].Status == HIVE_STATUS_BUILT && Hives[i].TechStatus == Tech) { return &Hives[i]; }
	}

	return nullptr;
}


edict_t* UTIL_GetClosestPlayerNeedsHealing(const Vector Location, const int Team, const float SearchRadius, edict_t* IgnorePlayer, bool bMustBeDirectlyReachable)
{
	edict_t* Result = nullptr;
	float MaxDist = sqrf(SearchRadius);
	float MinDist = 0.0f;


	for (int i = 0; i < 32; i++)
	{
		if (clients[i] && clients[i] != IgnorePlayer && clients[i]->v.team == Team && (clients[i]->v.health < clients[i]->v.max_health || clients[i]->v.armorvalue < GetPlayerMaxArmour(clients[i])))
		{
			if (bMustBeDirectlyReachable && !UTIL_PointIsDirectlyReachable(Location, UTIL_GetFloorUnderEntity(clients[i]))) { continue; }

			float ThisDist = vDist2DSq(Location, clients[i]->v.origin);

			if (ThisDist < MaxDist && (!Result || ThisDist < MinDist))
			{
				Result = clients[i];
				MinDist = ThisDist;
			}
		}
	}

	return Result;
}

bool UTIL_ActiveHiveWithTechExists(HiveTechStatus Tech)
{
	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].Status == HIVE_STATUS_BUILT && Hives[i].TechStatus == Tech) { return true; }
	}

	return false;
}

const hive_definition* UTIL_GetNearestHiveOfStatus(const Vector SearchLocation, const HiveStatusType Status)
{
	int Result = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].Status != Status) { continue; }

		float ThisDist = vDist2DSq(SearchLocation, Hives[i].FloorLocation);

		if (Result < 0 || ThisDist < MinDist)
		{
			MinDist = ThisDist;
			Result = i;
		}
	}

	if (Result > -1)
	{
		return &Hives[Result];
	}

	return nullptr;
}

int UTIL_GetNearestBuiltHiveIndex(const Vector SearchLocation)
{
	int Result = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].Status == HIVE_STATUS_UNBUILT) { continue; }

		float ThisDist = vDist2DSq(SearchLocation, Hives[i].FloorLocation);

		if (Result < 0 || ThisDist < MinDist)
		{
			MinDist = ThisDist;
			Result = i;
		}
	}

	return Result;
}

edict_t* UTIL_GetNearestPlayerOfTeamInArea(const Vector Location, const float SearchRadius, const int Team, edict_t* IgnorePlayer, NSPlayerClass IgnoreClass)
{
	edict_t* Result = nullptr;
	float CheckDist = sqrf(SearchRadius);
	float MinDist = 0.0f;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && clients[i] != IgnorePlayer && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && clients[i]->v.team == Team && UTIL_GetPlayerClass(clients[i]) != IgnoreClass)
		{
			float ThisDist = vDist2DSq(clients[i]->v.origin, Location);

			if (!Result || ThisDist < MinDist)
			{
				Result = clients[i];
				MinDist = ThisDist;
			}
		}
	}

	return Result;
}

int UTIL_GetNumPlayersOfTeamInArea(const Vector Location, const float SearchRadius, const int Team, edict_t* IgnorePlayer, NSPlayerClass IgnoreClass)
{
	int Result = 0;
	float CheckDist = sqrf(SearchRadius);
	float MinDist = 0.0f;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && clients[i] != IgnorePlayer && !IsPlayerCommander(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && clients[i]->v.team == Team && UTIL_GetPlayerClass(clients[i]) != IgnoreClass)
		{
			float ThisDist = vDist2DSq(clients[i]->v.origin, Location);

			if (ThisDist < CheckDist)
			{
				Result++;
			}
		}
	}

	return Result;
}

bool UTIL_IsPlayerOfTeamInArea(const Vector Location, const float SearchRadius, const int Team, edict_t* IgnorePlayer, NSPlayerClass IgnoreClass)
{
	float CheckDist = sqrf(SearchRadius);

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && (FNullEnt(IgnorePlayer) || clients[i] != IgnorePlayer) && !IsPlayerCommander(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && clients[i]->v.team == Team && UTIL_GetPlayerClass(clients[i]) != IgnoreClass)
		{
			if (vDist2DSq(clients[i]->v.origin, Location) <= CheckDist)
			{
				return true;
			}
		}
	}

	return false;
}

bool UTIL_IsAlienPlayerInArea(const Vector Location, float SearchRadius)
{
	float MaxDist = sqrf(SearchRadius);

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] && !IsPlayerDead(clients[i]) && IsPlayerOnAlienTeam(clients[i]))
		{
			if (vDist2DSq(clients[i]->v.origin, Location) <= MaxDist)
			{
				return true;
			}
		}
	}

	return false;
}

bool UTIL_IsAlienPlayerInArea(const Vector Location, float SearchRadius, edict_t* IgnorePlayer)
{
	float MaxDist = sqrf(SearchRadius);

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] && !IsPlayerDead(clients[i]) && IsPlayerOnAlienTeam(clients[i]) && clients[i] != IgnorePlayer)
		{
			if (vDist2DSq(clients[i]->v.origin, Location) <= MaxDist)
			{
				return true;
			}
		}
	}

	return false;
}

bool UTIL_IsNearActiveHive(const Vector Location, float SearchRadius)
{
	float MaxDist = sqrf(SearchRadius);

	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].Status != HIVE_STATUS_UNBUILT && vDist2DSq(Hives[i].Location, Location) <= MaxDist) { return true; }
	}

	return false;
}

edict_t* UTIL_GetFirstCompletedStructureOfType(const NSStructureType StructureType)
{
	bool bIsMarineStructure = UTIL_IsMarineStructure(StructureType);

	if (bIsMarineStructure)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }
			if (it.second.bFullyConstructed) { return it.first; }
		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }
			if (it.second.bFullyConstructed) { return it.first; }
		}
	}

	return nullptr;
}

edict_t* UTIL_GetFirstIdleStructureOfType(const NSStructureType StructureType)
{
	bool bIsMarineStructure = UTIL_IsMarineStructure(StructureType);

	if (bIsMarineStructure)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }
			if (UTIL_StructureIsRecycling(it.first)) { continue; }
			if (!it.second.bFullyConstructed) { continue; }

			if (!UTIL_StructureIsResearching(it.first) && !UTIL_StructureIsUpgrading(it.first)) { return it.first; }

		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }
			if (it.second.bFullyConstructed) { return it.first; }
		}
	}

	return nullptr;

}

int UTIL_GetNumPlacedStructuresOfType(const NSStructureType StructureType)
{
	bool bIsMarineStructure = UTIL_IsMarineStructure(StructureType);

	int result = 0;

	if (bIsMarineStructure)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (UTIL_StructureTypesMatch(it.second.StructureType, StructureType)) { result++; }
		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (UTIL_StructureTypesMatch(it.second.StructureType, StructureType)) { result++; }
		}
	}

	return result;
}

int UTIL_GetNumPlacedStructuresOfTypeInRadius(const NSStructureType StructureType, const Vector Location, const float MaxRadius)
{
	bool bIsMarineStructure = UTIL_IsMarineStructure(StructureType);

	int result = 0;
	float MaxDist = sqrf(MaxRadius);

	if (bIsMarineStructure)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (UTIL_StructureTypesMatch(it.second.StructureType, StructureType))
			{
				if (vDist2DSq(it.second.Location, Location) <= MaxDist)
				{
					result++;
				}

			}
		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (UTIL_StructureTypesMatch(it.second.StructureType, StructureType))
			{
				if (vDist2DSq(it.second.Location, Location) <= MaxDist)
				{
					result++;
				}

			}
		}
	}

	return result;
}

int UTIL_GetNumBuiltStructuresOfType(const NSStructureType StructureType)
{
	bool bIsMarineStructure = UTIL_IsMarineStructure(StructureType);

	int result = 0;

	if (bIsMarineStructure)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (it.second.StructureType == StructureType && it.second.bFullyConstructed) { result++; }
		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (it.second.StructureType == StructureType && it.second.bFullyConstructed) { result++; }
		}
	}

	return result;
}

int UTIL_GetNearestAvailableResourcePointIndex(const Vector& SearchPoint)
{
	int result = -1;
	float minDist = 0.0f;

	for (int i = 0; i < NumTotalResNodes; i++)
	{
		if (!ResourceNodes[i].edict || ResourceNodes[i].bIsOccupied) { continue; }

		float thisDist = vDist2DSq(ResourceNodes[i].edict->v.origin, SearchPoint);
		if (result < 0 || thisDist < minDist)
		{
			result = i;
			minDist = thisDist;
		}
	}

	return result;
}

int UTIL_GetNearestOccupiedResourcePointIndex(const Vector& SearchPoint)
{
	int result = -1;
	float minDist = 0.0f;

	for (int i = 0; i < NumTotalResNodes; i++)
	{
		if (!ResourceNodes[i].edict || !ResourceNodes[i].bIsOccupied) { continue; }

		float thisDist = vDist2DSq(ResourceNodes[i].edict->v.origin, SearchPoint);
		if (result < 0 || thisDist < minDist)
		{
			result = i;
			minDist = thisDist;
		}
	}

	return result;
}

void UTIL_PopulateResourceNodeLocations()
{
	memset(ResourceNodes, 0, sizeof(ResourceNodes));
	NumTotalResNodes = 0;

	edict_t* commChair = NULL;
	Vector CommChairLocation = ZERO_VECTOR;

	commChair = UTIL_FindEntityByClassname(commChair, "team_command");

	if (!FNullEnt(commChair))
	{
		CommChairLocation = commChair->v.origin;
	}

	edict_t* currNode = NULL;
	while (((currNode = UTIL_FindEntityByClassname(currNode, "func_resource")) != NULL) && (!FNullEnt(currNode)))
	{

		bool bReachable = UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, CommChairLocation, currNode->v.origin, 8.0f);

		if (bReachable || !CommChairLocation)
		{
			ResourceNodes[NumTotalResNodes].edict = currNode;
			ResourceNodes[NumTotalResNodes].origin = currNode->v.origin;
			ResourceNodes[NumTotalResNodes].TowerEdict = nullptr;
			ResourceNodes[NumTotalResNodes].bIsOccupied = false;
			ResourceNodes[NumTotalResNodes].bIsOwnedByMarines = false;
			ResourceNodes[NumTotalResNodes].bIsMarineBaseNode = false;

			UTIL_AddTemporaryObstacle(currNode->v.origin, 30.0f, 60.0f, DT_AREA_BLOCKED);

			NumTotalResNodes++;
		}
	}

	edict_t* currStructure = NULL;
	while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_command")) != NULL) && (!FNullEnt(currStructure)))
	{
		int NearestResNode = UTIL_FindNearestResNodeIndexToLocation(currStructure->v.origin);

		if (NearestResNode > -1)
		{
			ResourceNodes[NearestResNode].bIsMarineBaseNode = true;
		}
	}
}

HiveStatusType UTIL_GetHiveStatus(const edict_t* Hive)
{
	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].edict == Hive)
		{
			return Hives[i].Status;
		}
	}

	return HIVE_STATUS_UNBUILT;
}

int UTIL_GetItemCountOfTypeInArea(const NSDeployableItem ItemType, const Vector& SearchLocation, const float Radius)
{
	int Result = 0;
	float RadiusSq = sqrf(Radius);

	for (int i = 0; i < NumTotalMarineItems; i++)
	{
		if (AllMarineItems[i].ItemType == ItemType && vDist2DSq(AllMarineItems[i].edict->v.origin, SearchLocation) <= RadiusSq)
		{
			Result++;
		}
	}

	return Result;
}

bool UTIL_StructureIsFullyBuilt(const edict_t* Structure)
{
	NSStructureType StructureType = UTIL_GetStructureTypeFromEdict(Structure);

	if (StructureType == STRUCTURE_ALIEN_HIVE)
	{
		const hive_definition* Hive = UTIL_GetNearestHiveAtLocation(Structure->v.origin);
		
		return (Hive && Hive->Status != HIVE_STATUS_UNBUILT);
	}
	else
	{
		return !(Structure->v.iuser4 & MASK_BUILDABLE);
	}

}

int UTIL_GetStructureCountOfType(const NSStructureType StructureType)
{
	bool bIsMarineStructure = UTIL_IsMarineStructure(StructureType);

	int Result = 0;

	if (bIsMarineStructure)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { Result++; }

		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { Result++; }
		}
	}

	return Result;
}

int UTIL_FindNearestResNodeIndexToLocation(const Vector& Location)
{
	int Result = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalResNodes; i++)
	{
		if (ResourceNodes[i].edict)
		{
			float ThisDist = vDist3DSq(Location, ResourceNodes[i].origin);

			if (Result < 0 || ThisDist < MinDist)
			{
				Result = i;
				MinDist = ThisDist;
			}
		}
	}

	return Result;
}

const resource_node* UTIL_FindNearestResNodeToLocation(const Vector& Location)
{
	int Result = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalResNodes; i++)
	{
		if (ResourceNodes[i].edict)
		{
			float ThisDist = vDist3DSq(Location, ResourceNodes[i].origin);

			if (Result < 0 || ThisDist < MinDist)
			{
				Result = i;
				MinDist = ThisDist;
			}
		}
	}
	
	if (Result > -1)
	{
		return &ResourceNodes[Result];
	}

	return nullptr;
}

void UTIL_ClearMapLocations()
{
	memset(MapLocations, 0, sizeof(MapLocations));
	NumMapLocations = 0;
}

void UTIL_ClearMapAIData()
{
	memset(ResourceNodes, 0, sizeof(ResourceNodes));
	NumTotalResNodes = 0;

	memset(Hives, 0, sizeof(Hives));
	NumTotalHives = 0;

	memset(AllMarineItems, 0, sizeof(AllMarineItems));
	NumTotalMarineItems = 0;

	MarineBuildableStructureMap.clear();
	AlienBuildableStructureMap.clear();

	StructureRefreshFrame = 0;
}

const resource_node* UTIL_FindEligibleResNodeClosestToLocation(const Vector& Location, const int Team, bool bIgnoreElectrified)
{
	int Result = -1;
	float MinDist = 0.0f;


	for (int i = 0; i < NumTotalResNodes; i++)
	{
		bool ResNodeOccupiedByEnemy = (Team == MARINE_TEAM) ? !ResourceNodes[i].bIsOwnedByMarines : ResourceNodes[i].bIsOwnedByMarines;

		if (!ResourceNodes[i].bIsOccupied || (ResNodeOccupiedByEnemy && (bIgnoreElectrified || !UTIL_IsStructureElectrified(ResourceNodes[i].edict))))
		{

			if (Team == ALIEN_TEAM)
			{
				if (ResourceNodes[i].bIsOccupied && ResourceNodes[i].bIsMarineBaseNode) { continue; }

				if (!ResourceNodes[i].bIsOccupied)
				{
					bool bClaimedByOtherBot = false;

					for (int i = 0; i < 32; i++)
					{
						if (bots[i].is_used && IsPlayerOnAlienTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))
						{
							if (bots[i].PrimaryBotTask.TaskLocation == ResourceNodes[i].origin || bots[i].SecondaryBotTask.TaskLocation == ResourceNodes[i].origin)
							{
								bClaimedByOtherBot = true;
								break;
							}
						}
					}

					if (bClaimedByOtherBot) { continue; }
				}

			}

			float Dist = vDist2DSq(Location, ResourceNodes[i].origin);
			if (Result < 0 || Dist < MinDist)
			{
				Result = i;
				MinDist = Dist;
			}
		}
	}

	if (Result > -1)
	{
		return &ResourceNodes[Result];
	}

	return nullptr;
}

const resource_node* UTIL_FindEligibleResNodeFurthestFromLocation(const Vector& Location, const int Team, bool bIgnoreElectrified)
{
	int Result = -1;
	float MaxDist = 0.0f;


	for (int i = 0; i < NumTotalResNodes; i++)
	{
		bool ResNodeOccupiedByEnemy = (Team == MARINE_TEAM) ? !ResourceNodes[i].bIsOwnedByMarines : ResourceNodes[i].bIsOwnedByMarines;

		if (!ResourceNodes[i].bIsOccupied || (ResNodeOccupiedByEnemy && (bIgnoreElectrified || !UTIL_IsStructureElectrified(ResourceNodes[i].edict))))
		{

			if (Team == ALIEN_TEAM)
			{
				if (ResourceNodes[i].bIsOccupied && ResourceNodes[i].bIsMarineBaseNode) { continue; }

				if (!ResourceNodes[i].bIsOccupied)
				{
					bool bClaimedByOtherBot = false;

					for (int ii = 0; ii < 32; ii++)
					{
						if (bots[ii].is_used && IsPlayerOnAlienTeam(bots[ii].pEdict) && !IsPlayerDead(bots[ii].pEdict))
						{
							if (bots[ii].PrimaryBotTask.TaskLocation == ResourceNodes[i].origin || bots[ii].SecondaryBotTask.TaskLocation == ResourceNodes[i].origin)
							{
								bClaimedByOtherBot = true;
								break;
							}
						}
					}

					if (bClaimedByOtherBot)
					{
						continue;
					}
				}

			}

			float Dist = vDist2DSq(Location, ResourceNodes[i].origin);
			if (Result < 0 || Dist > MaxDist)
			{
				Result = i;
				MaxDist = Dist;
			}
		}
	}

	if (Result > -1)
	{
		return &ResourceNodes[Result];
	}

	return nullptr;
}

const resource_node* UTIL_AlienFindUnclaimedResNodeFurthestFromLocation(const bot_t* pBot, const Vector& Location, bool bIgnoreElectrified)
{
	int Result = -1;
	float MaxDist = 0.0f;


	for (int i = 0; i < NumTotalResNodes; i++)
	{
		if (ResourceNodes[i].bIsOccupied) { continue; }

		bool bClaimedByOtherBot = false;

		for (int ii = 0; ii < 32; ii++)
		{
			if (bots[ii].is_used && bots[ii].pEdict != pBot->pEdict && IsPlayerOnAlienTeam(bots[ii].pEdict) && !IsPlayerDead(bots[ii].pEdict))
			{
				if (vEquals(bots[ii].PrimaryBotTask.TaskLocation, ResourceNodes[i].origin, 10.0f) || vEquals(bots[ii].SecondaryBotTask.TaskLocation, ResourceNodes[i].origin, 10.0f))
				{

					bClaimedByOtherBot = true;
					break;
				}
			}
		}

		if (bClaimedByOtherBot)
		{
			continue;
		}

		edict_t* OtherGorge = UTIL_GetNearestPlayerOfClass(ResourceNodes[i].origin, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

		if (OtherGorge && (GetPlayerResources(OtherGorge) >= kResourceTowerCost && vDist2DSq(OtherGorge->v.origin, ResourceNodes[i].origin) < vDist2DSq(pBot->pEdict->v.origin, ResourceNodes[i].origin)))
		{
			continue;
		}

		edict_t* Egg = UTIL_GetNearestPlayerOfClass(ResourceNodes[i].origin, CLASS_EGG, UTIL_MetresToGoldSrcUnits(5.0f), pBot->pEdict);

		if (Egg && (GetPlayerResources(Egg) >= kResourceTowerCost && vDist2DSq(Egg->v.origin, ResourceNodes[i].origin) < vDist2DSq(pBot->pEdict->v.origin, ResourceNodes[i].origin)))
		{
			continue;
		}

		float Dist = vDist2DSq(Location, ResourceNodes[i].origin);
		if (Result < 0 || Dist > MaxDist)
		{
			Result = i;
			MaxDist = Dist;
		}
	}

	if (Result > -1)
	{
		return &ResourceNodes[Result];
	}

	return nullptr;
}

edict_t* UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(bot_t* pBot, const NSStructureType StructureType)
{
	edict_t* Result = nullptr;
	float MinDist = 0.0f;

	bool bMarineStructure = UTIL_IsMarineStructure(StructureType);

	if (bMarineStructure)
	{

		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bUnderAttack || !it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }

			int NumPotentialDefenders = UTIL_GetNumPlayersOfTeamInArea(it.second.Location, UTIL_MetresToGoldSrcUnits(10.0f), pBot->pEdict->v.team, pBot->pEdict, CLASS_GORGE);

			if (NumPotentialDefenders >= 2) { continue; }

			float ThisDist = vDist2DSq(it.second.Location, pBot->pEdict->v.origin);

			if (FNullEnt(Result) || ThisDist < MinDist)
			{
				Result = it.first;
				MinDist = ThisDist;
			}

		}

	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bUnderAttack || !it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }

			int NumPotentialDefenders = UTIL_GetNumPlayersOfTeamInArea(it.second.Location, UTIL_MetresToGoldSrcUnits(10.0f), pBot->pEdict->v.team, pBot->pEdict, CLASS_GORGE);

			if (NumPotentialDefenders >= 2) { continue; }

			float ThisDist = vDist2DSq(it.second.Location, pBot->pEdict->v.origin);

			if (FNullEnt(Result) || ThisDist < MinDist)
			{
				Result = it.first;
				MinDist = ThisDist;
			}
		}
	}

	return Result;
}


edict_t* UTIL_GetNearestStructureOfTypeInLocation(const NSStructureType StructureType, const Vector& Location, const float SearchRadius, bool bAllowElectrified)
{
	edict_t* Result = nullptr;
	float DistSq = sqrf(SearchRadius);
	float MinDist = 0.0f;

	bool bMarineStructure = UTIL_IsMarineStructure(StructureType);

	if (bMarineStructure)
	{

		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType) || (!bAllowElectrified && UTIL_IsStructureElectrified(it.first))) { continue; }

			float ThisDist = vDist2DSq(it.second.Location, Location);

			if (ThisDist <= DistSq && (FNullEnt(Result) || ThisDist < MinDist))
			{
				Result = it.first;
				MinDist = ThisDist;
			}

		}

	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }

			float ThisDist = vDist2DSq(it.second.Location, Location);

			if (ThisDist <= DistSq && (FNullEnt(Result) || ThisDist < MinDist))
			{
				Result = it.first;
				MinDist = ThisDist;
			}
		}
	}

	return Result;
}

bool UTIL_StructureOfTypeExistsInLocation(const NSStructureType StructureType, const Vector& Location, const float SearchRadius)
{
	float DistSq = sqrf(SearchRadius);

	bool bMarineStructure = UTIL_IsMarineStructure(StructureType);

	if (bMarineStructure)
	{

		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }

			if (vDist2DSq(it.second.Location, Location) <= DistSq) { return true; }

		}

	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }

			if (vDist2DSq(it.second.Location, Location) <= DistSq) { return true; }

		}
	}

	return false;
}

const resource_node* UTIL_GetNearestCappedResNodeToLocation(const Vector Location, int Team, bool bIgnoreElectrified)
{
	int Result = -1;
	float MinDist = 0.0f;

	bool bFindMarine = (Team == MARINE_TEAM);

	for (int i = 0; i < NumTotalResNodes; i++)
	{
		if (ResourceNodes[i].bIsOccupied && ResourceNodes[i].bIsOwnedByMarines == bFindMarine && (bIgnoreElectrified || !UTIL_IsStructureElectrified(ResourceNodes[i].edict)))
		{
			if (Team == ALIEN_TEAM && ResourceNodes[i].bIsOccupied && ResourceNodes[i].bIsMarineBaseNode) { continue; }

			float ThisDist = vDist2DSq(Location, ResourceNodes[i].origin);

			if (Result < 0 || ThisDist < MinDist)
			{
				Result = i;
				MinDist = ThisDist;
			}
		}
	}

	if (Result > -1)
	{
		return &ResourceNodes[Result];
	}

	return nullptr;
}

bool UTIL_CommChairExists()
{
	for (auto& it : MarineBuildableStructureMap)
	{
		if (!it.second.bOnNavmesh) { continue; }
		if (UTIL_StructureTypesMatch(STRUCTURE_MARINE_COMMCHAIR, it.second.StructureType)) { return true; }
	}

	return false;
}

Vector UTIL_GetCommChairLocation()
{
	if (MarineBuildableStructureMap.size() > 0)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (UTIL_StructureTypesMatch(STRUCTURE_MARINE_COMMCHAIR, it.second.StructureType)) { return it.second.Location; }
		}
	}
	else
	{
		edict_t* currStructure = NULL;

		// Marine Structures
		while (((currStructure = UTIL_FindEntityByClassname(currStructure, "team_command")) != NULL) && (!FNullEnt(currStructure)))
		{
			return currStructure->v.origin;
		}
	}

	return ZERO_VECTOR;
}

edict_t* UTIL_GetCommChair()
{
	for (auto& it : MarineBuildableStructureMap)
	{
		if (!it.second.bOnNavmesh) { continue; }
		if (UTIL_StructureTypesMatch(STRUCTURE_MARINE_COMMCHAIR, it.second.StructureType)) { return it.first; }
	}

	return NULL;
}

edict_t* UTIL_GetNearestPlayerOfClass(const Vector Location, const NSPlayerClass SearchClass, const float SearchDist, const edict_t* PlayerToIgnore)
{
	edict_t* result = nullptr;
	float MaxDist = sqrf(SearchDist);
	float MinDist = 0.0f;

	for (int i = 0; i < 32; i++)
	{
		if (FNullEnt(clients[i]) || clients[i] == PlayerToIgnore || IsPlayerDead(clients[i])) { continue; }

		if (!IsPlayerInReadyRoom(clients[i]) && !IsPlayerBeingDigested(clients[i]) && UTIL_GetPlayerClass(clients[i]) == SearchClass)
		{
			float ThisDist = vDist2DSq(Location, clients[i]->v.origin);

			if (ThisDist < MaxDist && (!result || ThisDist < MinDist))
			{
				result = clients[i];
				MinDist = ThisDist;
			}
		}
	}

	return result;
}

int UTIL_GetNumResNodes()
{
	return NumTotalResNodes;
}

Vector UTIL_GetRandomPointOfInterest()
{

	int NumPointsOfInterest = NumTotalResNodes + NumTotalHives + 1; // +1 for the comm chair

	int RandomIndex = irandrange(0, NumPointsOfInterest - 1);

	// Comm chair is last index
	if (RandomIndex == NumPointsOfInterest - 1)
	{
		return UTIL_GetCommChairLocation();
	}

	// Hives are indexed after resource nodes
	if (RandomIndex > NumTotalResNodes - 1)
	{
		return Hives[RandomIndex - NumTotalResNodes].FloorLocation;
	}
	else
	{
		return ResourceNodes[RandomIndex].origin;
	}
}

const hive_definition* UTIL_GetNearestHiveUnderSiege(const Vector SearchLocation)
{
	int Result = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].Status == HIVE_STATUS_UNBUILT) { continue; }

		if (UTIL_StructureOfTypeExistsInLocation(STRUCTURE_MARINE_PHASEGATE, Hives[i].Location, UTIL_MetresToGoldSrcUnits(20.0f)))
		{
			float ThisDist = vDist2DSq(SearchLocation, Hives[i].Location);

			if (Result < 0 || ThisDist < MinDist)
			{
				Result = i;
				MinDist = ThisDist;
			}
		}
	}

	if (Result > -1)
	{
		return &Hives[Result];
	}

	return nullptr;
}

bool UTIL_IsAnyHumanNearLocation(const Vector& Location, const float SearchDist)
{
	float SearchDistSq = sqrf(SearchDist);

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] && IsPlayerHuman(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && !IsPlayerCommander(clients[i]))
		{
			if (vDist2DSq(clients[i]->v.origin, Location) <= SearchDistSq)
			{
				return true;
			}
		}
	}

	return false;
}

edict_t* UTIL_GetNearestHumanAtLocation(const Vector& Location, const float SearchDist)
{
	float SearchDistSq = sqrf(SearchDist);
	edict_t* NearestHuman = NULL;
	float NearestDist = 0.0f;

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] && IsPlayerHuman(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
		{
			float CurrDist = vDist2DSq(clients[i]->v.origin, Location);
			if (CurrDist <= SearchDistSq)
			{
				if (!NearestHuman || CurrDist < NearestDist)
				{
					NearestHuman = clients[i];
					NearestDist = CurrDist;
				}

			}
		}
	}

	return NearestHuman;
}

bool UTIL_IsAnyHumanNearLocationWithoutSpecialWeapon(const Vector& Location, const float SearchDist)
{
	float SearchDistSq = sqrf(SearchDist);

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] && IsPlayerHuman(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && !IsPlayerCommander(clients[i]))
		{
			if (UTIL_PlayerHasWeapon(clients[i], WEAPON_MARINE_MG) && vDist2DSq(clients[i]->v.origin, Location) <= SearchDistSq)
			{
				return true;
			}
		}
	}

	return false;
}

bool UTIL_IsAnyHumanNearLocationWithoutEquipment(const Vector& Location, const float SearchDist)
{
	float SearchDistSq = sqrf(SearchDist);

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] && IsPlayerHuman(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && !IsPlayerCommander(clients[i]))
		{
			if (!UTIL_PlayerHasEquipment(clients[i]) && vDist2DSq(clients[i]->v.origin, Location) <= SearchDistSq)
			{
				return true;
			}
		}
	}

	return false;
}

edict_t* UTIL_GetNearestStructureIndexOfType(const Vector& Location, NSStructureType StructureType, const float SearchDist, bool bFullyConstructedOnly)
{
	if (StructureType == STRUCTURE_ALIEN_HIVE)
	{
		edict_t* NearestHive = nullptr;
		float MaxDist = sqrf(SearchDist);
		float MinDist = 0.0f;

		for (int i = 0; i < NumTotalHives; i++)
		{
			if (!bFullyConstructedOnly || Hives[i].Status == HIVE_STATUS_BUILT)
			{
				float ThisDist = vDist2DSq(Hives[i].Location, Location);

				if (ThisDist < MaxDist && (!NearestHive || ThisDist < MinDist))
				{
					NearestHive = Hives[i].edict;
					MinDist = ThisDist;
				}
			}
		}

		return NearestHive;
	}

	bool bIsMarineStructure = UTIL_IsMarineStructure(StructureType);

	edict_t* Result = nullptr;
	float MinDist = 0.0f;
	float SearchDistSq = sqrf(SearchDist);

	if (bIsMarineStructure)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }
			if (bFullyConstructedOnly && !it.second.bFullyConstructed) { continue; }

			float thisDist = vDist2DSq(Location, it.second.Location);

			if (thisDist < SearchDistSq && (!Result || thisDist < MinDist))
			{
				Result = it.first;
				MinDist = thisDist;
			}
		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }
			if (!UTIL_StructureTypesMatch(StructureType, it.second.StructureType)) { continue; }
			if (bFullyConstructedOnly && !it.second.bFullyConstructed) { continue; }

			float thisDist = vDist2DSq(Location, it.second.Location);

			if (thisDist < SearchDistSq && (!Result || thisDist < MinDist))
			{
				Result = it.first;
				MinDist = thisDist;
			}
		}
	}

	return Result;
}

bool UTIL_AnyMarinePlayerWithLOS(const Vector& Location, float SearchRadius)
{
	float distSq = sqrf(SearchRadius);

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] && IsPlayerOnMarineTeam(clients[i]) && !IsPlayerCommander(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
		{
			if (vDist2DSq(clients[i]->v.origin, Location) <= distSq && UTIL_PointIsDirectlyReachable(clients[i]->v.origin, Location))
			{
				return true;
			}
		}
	}

	return false;
}

int UTIL_FindClosestMarinePlayerToLocation(const edict_t* SearchingPlayer, const Vector& Location, const float SearchRadius)
{
	int nearestPlayer = -1;
	float nearestDist = 0.0f;
	const float maxDist = sqrf(SearchRadius);

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] != NULL && clients[i] != SearchingPlayer && IsPlayerOnMarineTeam(clients[i]) && !IsPlayerCommander(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
		{
			float playerDist = vDist2DSq(clients[i]->v.origin, Location);

			if (playerDist < maxDist && (nearestPlayer < 0 || playerDist < nearestDist))
			{
				nearestPlayer = i;
				nearestDist = playerDist;
			}
		}
	}

	return nearestPlayer;
}

edict_t* UTIL_FindClosestMarineStructureToLocation(const Vector& Location, const float SearchRadius, bool bAllowElectrified)
{
	edict_t* Result = nullptr;
	float MinDist = 0.0f;
	float MaxDistSq = sqrf(SearchRadius);

	for (auto& it : MarineBuildableStructureMap)
	{
		if (!it.second.bOnNavmesh) { continue; }
		if (!bAllowElectrified && it.second.bIsElectrified) { continue; }

		float thisDist = vDist2DSq(Location, it.second.Location);

		if (thisDist < MaxDistSq && (!Result || thisDist < MinDist))
		{
			Result = it.first;
			MinDist = thisDist;
		}
	}

	return Result;
}

bool UTIL_AnyMarinePlayerNearLocation(const Vector& Location, float SearchRadius)
{
	float distSq = (SearchRadius * SearchRadius);

	for (int i = 0; i < 32; i++)
	{
		if (clients[i] && IsPlayerOnMarineTeam(clients[i]) && !IsPlayerCommander(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
		{
			if (vDist2DSq(clients[i]->v.origin, Location) <= distSq)
			{
				return true;
			}
		}
	}

	return false;
}

edict_t* UTIL_FindClosestMarineStructureUnbuilt(const Vector& SearchLocation, float SearchRadius)
{
	edict_t* NearestStructure = NULL;
	float nearestDist = 0.0f;

	float SearchDistSq = sqrf(SearchRadius);

	for (auto& it : MarineBuildableStructureMap)
	{
		if (!it.second.bOnNavmesh) { continue; }
		if (it.second.bFullyConstructed) { continue; }

		float thisDist = vDist2DSq(SearchLocation, it.second.Location);

		if (thisDist < SearchDistSq && (!NearestStructure || thisDist < nearestDist))
		{
			NearestStructure = it.first;
			nearestDist = thisDist;
		}
	}

	return NearestStructure;
}

edict_t* UTIL_FindClosestDamagedStructure(const Vector& SearchLocation, const int Team, float SearchRadius)
{
	edict_t* Result = nullptr;
	float MaxDist = sqrf(SearchRadius);
	float MinDist = 0.0f;

	if (Team == MARINE_TEAM)
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }

			if (!it.second.bFullyConstructed) { continue; }

			if (it.second.healthPercent >= 1.0f) { continue; }

			float thisDist = vDist2DSq(SearchLocation, it.second.Location);

			if (thisDist < MaxDist && (!Result || thisDist < MinDist))
			{
				Result = it.first;
				MinDist = thisDist;
			}
		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }

			if (!it.second.bFullyConstructed) { continue; }

			if (it.second.healthPercent >= 1.0f) { continue; }

			float thisDist = vDist2DSq(SearchLocation, it.second.Location);

			if (thisDist < MaxDist && (!Result || thisDist < MinDist))
			{
				Result = it.first;
				MinDist = thisDist;
			}
		}
	}

	return Result;
}

edict_t* UTIL_FindMarineWithDamagedArmour(const Vector& SearchLocation, float SearchRadius, edict_t* IgnoreEdict)
{
	edict_t* Result = nullptr;
	float MaxDist = sqrf(SearchRadius);
	float MinDist = 0.0f;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && clients[i] != IgnoreEdict && IsPlayerMarine(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]))
		{
			if (clients[i]->v.armorvalue < GetPlayerMaxArmour(clients[i]))
			{
				float ThisDist = vDist2DSq(SearchLocation, clients[i]->v.origin);

				if (ThisDist < MaxDist && (!Result || ThisDist < MinDist))
				{
					Result = clients[i];
					MinDist = ThisDist;
				}

			}
		}
	}

	return Result;
}

int UTIL_GetNumUnbuiltHives()
{
	int Result = 0;

	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].bIsValid && Hives[i].Status == HIVE_STATUS_UNBUILT) { Result++; }
	}

	return Result;
}

void UTIL_RefreshMarineItems()
{
	memset(AllMarineItems, 0, sizeof(AllMarineItems));
	NumTotalMarineItems = 0;

	edict_t* currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "item_health")) != NULL) && (!FNullEnt(currItem)))
	{
		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;
			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_HEALTHPACK;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "item_genericammo")) != NULL) && (!FNullEnt(currItem)))
	{
		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;
			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_AMMO;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "item_heavyarmor")) != NULL) && (!FNullEnt(currItem)))
	{


		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), currItem->v.origin, 32.0f)) { continue; }

			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;

			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_HEAVYARMOUR;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "item_jetpack")) != NULL) && (!FNullEnt(currItem)))
	{
		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), currItem->v.origin, 32.0f)) { continue; }
			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;

			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_JETPACK;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "item_catalyst")) != NULL) && (!FNullEnt(currItem)))
	{
		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;

			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_CATALYSTS;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "weapon_mine")) != NULL) && (!FNullEnt(currItem)))
	{
		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), currItem->v.origin, 32.0f)) { continue; }
			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;

			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_MINES;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "weapon_shotgun")) != NULL) && (!FNullEnt(currItem)))
	{
		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), currItem->v.origin, 32.0f)) { continue; }
			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;

			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_SHOTGUN;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "weapon_heavymachinegun")) != NULL) && (!FNullEnt(currItem)))
	{
		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), currItem->v.origin, 32.0f)) { continue; }
			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;

			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_HMG;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "weapon_grenadegun")) != NULL) && (!FNullEnt(currItem)))
	{
		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), currItem->v.origin, 32.0f)) { continue; }
			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;

			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_GRENADELAUNCHER;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "weapon_welder")) != NULL) && (!FNullEnt(currItem)))
	{
		if (!(currItem->v.effects & EF_NODRAW) && UTIL_PointIsOnNavmesh(currItem->v.origin, MARINE_REGULAR_NAV_PROFILE))
		{
			if (!UTIL_PointIsReachable(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), currItem->v.origin, 32.0f)) { continue; }
			AllMarineItems[NumTotalMarineItems].edict = currItem;
			AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;

			AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
			AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_WELDER;

			NumTotalMarineItems++;
		}
	}

	currItem = NULL;
	while (((currItem = UTIL_FindEntityByClassname(currItem, "scan")) != NULL) && (!FNullEnt(currItem)))
	{

		AllMarineItems[NumTotalMarineItems].edict = currItem;
		AllMarineItems[NumTotalMarineItems].bOnNavMesh = true;

		AllMarineItems[NumTotalMarineItems].Location = currItem->v.origin;
		AllMarineItems[NumTotalMarineItems].ItemType = ITEM_MARINE_SCAN;

		NumTotalMarineItems++;
	}

}

NSDeployableItem UTIL_GetItemTypeFromEdict(const edict_t* ItemEdict)
{
	if (!ItemEdict) { return ITEM_NONE; }

	for (int i = 0; i < NumTotalMarineItems; i++)
	{
		if (AllMarineItems[i].edict == ItemEdict) { return AllMarineItems[i].ItemType; }
	}

	return ITEM_NONE;
}

bool IsAlienTraitCategoryAvailable(HiveTechStatus TraitCategory)
{
	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].TechStatus == TraitCategory) { return true; }
	}

	return false;
}

bool UTIL_ShouldStructureCollide(NSStructureType StructureType)
{
	if (StructureType == STRUCTURE_NONE) { return false; }

	switch (StructureType)
	{
	case STRUCTURE_MARINE_INFANTRYPORTAL:
	case STRUCTURE_MARINE_PHASEGATE:
	case STRUCTURE_MARINE_TURRET:
		return false;
	default:
		return true;

	}

	return true;
}

void UTIL_UpdateBuildableStructure(edict_t* Structure)
{
	if (FNullEnt(Structure)) { return; }

	NSStructureType StructureType = UTIL_IUSER3ToStructureType(Structure->v.iuser3);

	if (StructureType == STRUCTURE_NONE) { return; }

	bool bShouldCollide = UTIL_ShouldStructureCollide(StructureType);

	if (UTIL_IsMarineStructure(StructureType))
	{
		MarineBuildableStructureMap[Structure].edict = Structure;

		if (Structure->v.origin != MarineBuildableStructureMap[Structure].Location)
		{
			MarineBuildableStructureMap[Structure].bOnNavmesh = UTIL_PointIsOnNavmesh(BUILDING_REGULAR_NAV_PROFILE, Structure->v.origin, Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));
		}

		MarineBuildableStructureMap[Structure].Location = Structure->v.origin;
		MarineBuildableStructureMap[Structure].healthPercent = (Structure->v.health / Structure->v.max_health);
		MarineBuildableStructureMap[Structure].bFullyConstructed = !(Structure->v.iuser4 & MASK_BUILDABLE);
		MarineBuildableStructureMap[Structure].bIsParasited = (Structure->v.iuser4 & MASK_PARASITED);
		MarineBuildableStructureMap[Structure].bIsElectrified = UTIL_IsStructureElectrified(Structure);
		MarineBuildableStructureMap[Structure].bDead = (Structure->v.deadflag != DEAD_NO);
		MarineBuildableStructureMap[Structure].StructureType = UTIL_IUSER3ToStructureType(Structure->v.iuser3);

		if (MarineBuildableStructureMap[Structure].LastSeen == 0)
		{
			if (bShouldCollide)
			{
				float radius = 32.0f;
				MarineBuildableStructureMap[Structure].ObstacleRef = UTIL_AddTemporaryObstacle(UTIL_GetCentreOfEntity(MarineBuildableStructureMap[Structure].edict), radius, 100.0f, DT_AREA_NULL);
			}
			else
			{
				MarineBuildableStructureMap[Structure].ObstacleRef = 0;
			}

			UTIL_OnStructureCreated(&MarineBuildableStructureMap[Structure]);
			MarineBuildableStructureMap[Structure].healthPercent = (Structure->v.health / Structure->v.max_health);
			MarineBuildableStructureMap[Structure].bUnderAttack = false;
			MarineBuildableStructureMap[Structure].lastDamagedTime = 0.0f;
			MarineBuildableStructureMap[Structure].bOnNavmesh = UTIL_PointIsOnNavmesh(BUILDING_REGULAR_NAV_PROFILE, Structure->v.origin, Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));
		}
		else
		{
			float NewHealthPercent = (Structure->v.health / Structure->v.max_health);

			if (NewHealthPercent < MarineBuildableStructureMap[Structure].healthPercent)
			{
				MarineBuildableStructureMap[Structure].lastDamagedTime = gpGlobals->time;
			}
			MarineBuildableStructureMap[Structure].healthPercent = NewHealthPercent;
		}

		if (!MarineBuildableStructureMap[Structure].bDead)
		{
			MarineBuildableStructureMap[Structure].LastSeen = StructureRefreshFrame;
			MarineBuildableStructureMap[Structure].bUnderAttack = (gpGlobals->time - MarineBuildableStructureMap[Structure].lastDamagedTime) < 10.0f;
		}

	}
	else
	{
		AlienBuildableStructureMap[Structure].edict = Structure;

		if (Structure->v.origin != AlienBuildableStructureMap[Structure].Location)
		{
			AlienBuildableStructureMap[Structure].bOnNavmesh = UTIL_PointIsOnNavmesh(BUILDING_REGULAR_NAV_PROFILE, Structure->v.origin, Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));
		}

		AlienBuildableStructureMap[Structure].Location = Structure->v.origin;

		AlienBuildableStructureMap[Structure].bFullyConstructed = !(Structure->v.iuser4 & MASK_BUILDABLE);
		AlienBuildableStructureMap[Structure].bIsParasited = (Structure->v.iuser4 & MASK_PARASITED);
		AlienBuildableStructureMap[Structure].bIsElectrified = UTIL_IsStructureElectrified(Structure);
		AlienBuildableStructureMap[Structure].bDead = (Structure->v.deadflag != DEAD_NO);
		AlienBuildableStructureMap[Structure].StructureType = UTIL_IUSER3ToStructureType(Structure->v.iuser3);

		if (AlienBuildableStructureMap[Structure].LastSeen == 0)
		{
			AlienBuildableStructureMap[Structure].healthPercent = (Structure->v.health / Structure->v.max_health);
			AlienBuildableStructureMap[Structure].lastDamagedTime = 0.0f;

			if (bShouldCollide)
			{
				float radius = 32.0f;
				AlienBuildableStructureMap[Structure].ObstacleRef = UTIL_AddTemporaryObstacle(UTIL_GetCentreOfEntity(AlienBuildableStructureMap[Structure].edict), radius, 100.0f, DT_AREA_NULL);
			}
			else
			{
				AlienBuildableStructureMap[Structure].ObstacleRef = 0;
			}

			UTIL_OnStructureCreated(&AlienBuildableStructureMap[Structure]);

			AlienBuildableStructureMap[Structure].bOnNavmesh = UTIL_PointIsOnNavmesh(BUILDING_REGULAR_NAV_PROFILE, Structure->v.origin, Vector(max_player_use_reach, max_player_use_reach, max_player_use_reach));
		}
		else
		{
			float NewHealthPercent = (Structure->v.health / Structure->v.max_health);

			if (NewHealthPercent < AlienBuildableStructureMap[Structure].healthPercent)
			{
				AlienBuildableStructureMap[Structure].lastDamagedTime = gpGlobals->time;
			}
			AlienBuildableStructureMap[Structure].healthPercent = NewHealthPercent;
		}

		if (!AlienBuildableStructureMap[Structure].bDead)
		{
			AlienBuildableStructureMap[Structure].LastSeen = StructureRefreshFrame;
			AlienBuildableStructureMap[Structure].bUnderAttack = (gpGlobals->time - AlienBuildableStructureMap[Structure].lastDamagedTime) < 10.0f;
		}
	}
}

NSWeapon UTIL_GetWeaponTypeFromEdict(const edict_t* ItemEdict)
{
	if (!ItemEdict) { return WEAPON_NONE; }

	for (int i = 0; i < NumTotalMarineItems; i++)
	{
		if (AllMarineItems[i].edict == ItemEdict)
		{
			switch (AllMarineItems[i].ItemType)
			{
			case ITEM_MARINE_WELDER:
				return WEAPON_MARINE_WELDER;
			case ITEM_MARINE_HMG:
				return WEAPON_MARINE_HMG;
			case ITEM_MARINE_GRENADELAUNCHER:
				return WEAPON_MARINE_GL;
			case ITEM_MARINE_SHOTGUN:
				return WEAPON_MARINE_SHOTGUN;
			case ITEM_MARINE_MINES:
				return WEAPON_MARINE_MINES;
			default:
				return WEAPON_NONE;
			}
		}
	}

	return WEAPON_NONE;
}

const hive_definition* UTIL_GetHiveAtIndex(int Index)
{
	if (Index > -1 && Index < NumTotalHives)
	{
		return &Hives[Index];
	}

	return nullptr;
}

int UTIL_GetNumTotalHives()
{
	return NumTotalHives;
}

int UTIL_GetNumActiveHives()
{
	int Result = 0;

	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].Status == HIVE_STATUS_BUILT) { Result++; }
	}

	return Result;
}

const hive_definition* UTIL_GetNearestHiveAtLocation(const Vector Location)
{
	int Result = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < NumTotalHives; i++)
	{
		if (Hives[i].bIsValid)
		{
			float ThisDist = vDist2DSq(Location, Hives[i].Location);

			if (Result < 0 || ThisDist < MinDist)
			{
				Result = i;
				MinDist = ThisDist;
			}
		}
	}

	if (Result > -1)
	{
		return &Hives[Result];
	}

	return nullptr;
}

edict_t* UTIL_AlienFindNearestHealingSpot(bot_t* pBot, const Vector SearchLocation)
{
	edict_t* HealingSources[3];

	HealingSources[0] = UTIL_GetNearestStructureIndexOfType(SearchLocation, STRUCTURE_ALIEN_HIVE, UTIL_MetresToGoldSrcUnits(100.0f), true);
	HealingSources[1] = UTIL_GetNearestStructureIndexOfType(SearchLocation, STRUCTURE_ALIEN_DEFENCECHAMBER, UTIL_MetresToGoldSrcUnits(100.0f), true);
	HealingSources[2] = UTIL_GetNearestPlayerOfClass(SearchLocation, CLASS_GORGE, UTIL_MetresToGoldSrcUnits(100.0f), pBot->pEdict);

	int NearestHealingSource = -1;
	float MinDist = 0.0f;

	for (int i = 0; i < 3; i++)
	{
		if (!FNullEnt(HealingSources[i]))
		{
			float ThisDist = vDist2DSq(HealingSources[i]->v.origin, SearchLocation);

			if (NearestHealingSource < 0 || ThisDist < MinDist)
			{
				NearestHealingSource = i;
				MinDist = ThisDist;
			}
		}
	}

	if (NearestHealingSource > -1)
	{
		return HealingSources[NearestHealingSource];
	}

	return nullptr;
}

edict_t* BotGetNearestDangerTurret(bot_t* pBot, float MaxDistance)
{
	edict_t* Result = nullptr;
	float MinDist = 0;
	float MaxDist = sqrf(MaxDistance);

	Vector Location = pBot->pEdict->v.origin;

	if (IsPlayerOnAlienTeam(pBot->pEdict))
	{
		for (auto& it : MarineBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }

			if (!UTIL_StructureTypesMatch(it.second.StructureType, STRUCTURE_MARINE_ANYTURRET)) { continue; }

			float thisDist = vDist2DSq(Location, it.second.Location);

			if (thisDist < MaxDist)
			{
				if (!UTIL_QuickTrace(pBot->pEdict, Location, it.second.Location)) { continue; }

				if (!Result || thisDist < MinDist)
				{
					Result = it.first;
					MinDist = thisDist;
				}
			}
		}
	}
	else
	{
		for (auto& it : AlienBuildableStructureMap)
		{
			if (!it.second.bOnNavmesh) { continue; }

			if (it.second.StructureType != STRUCTURE_ALIEN_OFFENCECHAMBER) { continue; }

			float thisDist = vDist2DSq(Location, it.second.Location);

			if (thisDist < MaxDist)
			{
				if (!UTIL_QuickTrace(pBot->pEdict, Location, it.second.Location)) { continue; }

				if (!Result || thisDist < MinDist)
				{
					Result = it.first;
					MinDist = thisDist;
				}
			}
		}
	}

	return Result;


}

bool UTIL_DroppedItemIsPrimaryWeapon(NSDeployableItem ItemType)
{
	switch (ItemType)
	{
	case ITEM_MARINE_GRENADELAUNCHER:
	case ITEM_MARINE_HMG:
	case ITEM_MARINE_SHOTGUN:
		return true;
	default:
		return false;
	}

	return false;
}


bool UTIL_IsMarineStructure(const edict_t* Structure)
{
	if (FNullEnt(Structure)) { return false; }

	NSStructureType StructureType = UTIL_IUSER3ToStructureType(Structure->v.iuser3);
	return UTIL_IsMarineStructure(StructureType);
}

bool UTIL_IsAlienStructure(const edict_t* Structure)
{
	if (!Structure) { return false; }

	NSStructureType StructureType = UTIL_IUSER3ToStructureType(Structure->v.iuser3);

	return UTIL_IsAlienStructure(StructureType);
}

bool UTIL_IsMarineStructure(const NSStructureType StructureType)
{
	switch (StructureType)
	{
	case STRUCTURE_MARINE_ARMOURY:
	case STRUCTURE_MARINE_ADVARMOURY:
	case STRUCTURE_MARINE_ANYARMOURY:
	case STRUCTURE_MARINE_TURRETFACTORY:
	case STRUCTURE_MARINE_ADVTURRETFACTORY:
	case STRUCTURE_MARINE_ANYTURRETFACTORY:
	case STRUCTURE_MARINE_ARMSLAB:
	case STRUCTURE_MARINE_COMMCHAIR:
	case STRUCTURE_MARINE_INFANTRYPORTAL:
	case STRUCTURE_MARINE_OBSERVATORY:
	case STRUCTURE_MARINE_PHASEGATE:
	case STRUCTURE_MARINE_PROTOTYPELAB:
	case STRUCTURE_MARINE_RESTOWER:
	case STRUCTURE_MARINE_SIEGETURRET:
	case STRUCTURE_MARINE_TURRET:
	case STRUCTURE_MARINE_ANYTURRET:
		return true;
	default:
		return false;
	}
}

bool UTIL_IsAlienStructure(const NSStructureType StructureType)
{
	switch (StructureType)
	{
	case STRUCTURE_ALIEN_HIVE:
	case STRUCTURE_ALIEN_DEFENCECHAMBER:
	case STRUCTURE_ALIEN_MOVEMENTCHAMBER:
	case STRUCTURE_ALIEN_OFFENCECHAMBER:
	case STRUCTURE_ALIEN_SENSORYCHAMBER:
	case STRUCTURE_ALIEN_RESTOWER:
		return true;
	default:
		return false;
	}
}

void UTIL_LinkAlienStructureToTask(bot_t* pBot, edict_t* NewStructure)
{
	if (!NewStructure) { return; }

	NSStructureType StructureType = UTIL_GetStructureTypeFromEdict(NewStructure);

	if (StructureType == STRUCTURE_NONE) { return; }

	if ((pBot->PrimaryBotTask.TaskType == TASK_BUILD || pBot->PrimaryBotTask.TaskType == TASK_CAP_RESNODE) && pBot->PrimaryBotTask.bIsWaitingForBuildLink)
	{
		if (pBot->PrimaryBotTask.StructureType == StructureType)
		{

			if (vDist2DSq(NewStructure->v.origin, pBot->PrimaryBotTask.TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
			{
				pBot->PrimaryBotTask.TaskTarget = NewStructure;
				pBot->PrimaryBotTask.bIsWaitingForBuildLink = false;
			}
		}
	}

	if ((pBot->SecondaryBotTask.TaskType == TASK_BUILD || pBot->SecondaryBotTask.TaskType == TASK_CAP_RESNODE) && pBot->SecondaryBotTask.bIsWaitingForBuildLink)
	{
		if (pBot->SecondaryBotTask.StructureType == StructureType)
		{

			if (vDist2DSq(NewStructure->v.origin, pBot->SecondaryBotTask.TaskLocation) < sqrf(UTIL_MetresToGoldSrcUnits(2.0f)))
			{
				pBot->SecondaryBotTask.TaskTarget = NewStructure;
				pBot->SecondaryBotTask.bIsWaitingForBuildLink = false;
			}
		}
	}
}

int UTIL_GetNumWeaponsOfTypeInPlay(const NSWeapon WeaponType)
{
	int NumPlacedWeapons = UTIL_GetItemCountOfTypeInArea(UTIL_WeaponTypeToDeployableItem(WeaponType), UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(30.0f));
	int NumHeldWeapons = 0;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && IsPlayerOnMarineTeam(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && !IsPlayerCommander(clients[i]))
		{
			if (UTIL_PlayerHasWeapon(clients[i], WeaponType))
			{
				NumHeldWeapons++;
			}
		}
	}


	return NumPlacedWeapons + NumHeldWeapons;
}

int UTIL_GetNumEquipmentInPlay()
{

	int NumPlacedEquipment = UTIL_GetItemCountOfTypeInArea(ITEM_MARINE_HEAVYARMOUR, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(30.0f));
	NumPlacedEquipment += UTIL_GetItemCountOfTypeInArea(ITEM_MARINE_JETPACK, UTIL_GetCommChairLocation(), UTIL_MetresToGoldSrcUnits(30.0f));
	int NumUsedEquipment = 0;

	for (int i = 0; i < 32; i++)
	{
		if (!FNullEnt(clients[i]) && IsPlayerOnMarineTeam(clients[i]) && !IsPlayerDead(clients[i]) && !IsPlayerBeingDigested(clients[i]) && !IsPlayerCommander(clients[i]))
		{
			if (UTIL_PlayerHasEquipment(clients[i]))
			{
				NumUsedEquipment++;
			}
		}
	}


	return NumPlacedEquipment + NumUsedEquipment;
}
//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_tactical.h
// 
// Contains all helper functions for making tactical decisions
//

#pragma once

#ifndef BOT_TACTICAL_H
#define BOT_TACTICAL_H

#include "bot_structs.h"

// Data structure used to track resource nodes in the map
typedef struct _RESOURCE_NODE
{
	edict_t* edict = nullptr; // The func_resource edict reference
	Vector origin = ZERO_VECTOR; // origin of the func_resource edict (not the tower itself)
	bool bIsOccupied = false; // True if there is any resource tower on it
	bool bIsOwnedByMarines = false; // True if capped and has a marine res tower on it
	edict_t* TowerEdict = nullptr; // Reference to the resource tower edict (if capped)
	bool bIsMarineBaseNode = false; // Is this the pre-capped node that appears in the marine base?
} resource_node;

// Defines a named location on the map. Used by the bot to communicate with humans (e.g. "I want to place hive at X")
typedef struct _MAP_LOCATION
{
	char LocationName[64] = "\0";
	Vector MinLocation = ZERO_VECTOR;
	Vector MaxLocation = ZERO_VECTOR;
	bool bIsValid = false;
} map_location;

// Data structure to hold information on any kind of buildable structure (hive, resource tower, chamber, marine building etc)
typedef struct _BUILDABLE_STRUCTURE
{
	edict_t* edict = nullptr; // Reference to structure edict
	float healthPercent = 0.0f; // Current health of the building
	float lastDamagedTime = 0.0f; // When it was last damaged by something. Used by bots to determine if still needs defending
	bool bUnderAttack = false; // If last damaged time < certain amount of time ago
	bool bDead = false; // Has structure just been destroyed, but edict not yet freed up
	bool bFullyConstructed = false; // Has the structure been fully built, or under construction?
	bool bIsParasited = false; // Has been parasited by skulk
	bool bIsElectrified = false; // Is electrified or not
	Vector Location = ZERO_VECTOR; // origin of the structure edict
	NSStructureType StructureType = STRUCTURE_NONE; // Type of structure it is (e.g. hive, comm chair, infantry portal, defence chamber etc.)
	int LastSeen = 0; // Which refresh cycle was this last seen on? Used to determine if the building has been removed from play
	unsigned int ObstacleRef = 0; // If the building places an obstacle on the nav mesh, hold the reference so it can be removed if building is destroyed
	bool bOnNavmesh = false; // Is the building on the nav mesh? Used to determine if the building ended up out of play or sinking into the floor somehow
	Vector LastSuccessfulCommanderLocation = ZERO_VECTOR; // Tracks the last commander view location where it successfully placed or selected the building
	Vector LastSuccessfulCommanderAngle = ZERO_VECTOR; // Tracks the last commander input angle ("click" location) used to successfully place or select building

} buildable_structure;

// Any kind of pickup that has been dropped either by the commander or by a player
typedef struct _DROPPED_MARINE_ITEM
{
	edict_t* edict = nullptr; // Reference to the item edict
	Vector Location = ZERO_VECTOR; // Origin of the entity
	NSDeployableItem ItemType = ITEM_NONE; // Is it a weapon, health pack, ammo pack etc?
	bool bOnNavMesh = false; // Is it on the nav mesh? Important to prevent bots trying to grab stuff that's inaccessible

} dropped_marine_item;

// How frequently to update the global list of built structures (in seconds)
static const float structure_inventory_refresh_rate = 0.2f;

// Increments by 1 every time the structure list is refreshed. Used to detect if structures have been destroyed and no longer show up
static int StructureRefreshFrame = 0;

// How frequently to update the global list of dropped marine items (in seconds)
static const float item_inventory_refresh_rate = 0.1f;


// Clears out the marine and alien buildable structure maps, resource node and hive lists, and the marine item list
void UTIL_ClearMapAIData();
// Clears out the MapLocations array
void UTIL_ClearMapLocations();

void PopulateEmptyHiveList();

// Returns the location of a randomly-selected resource node, comm chair, or hive
Vector UTIL_GetRandomPointOfInterest();

bool IsAlienTraitCategoryAvailable(HiveTechStatus TraitCategory);

bool UTIL_ShouldStructureCollide(NSStructureType StructureType);
void UTIL_UpdateBuildableStructure(edict_t* Structure);

// Will cycle through all structures in the map and update the marine and alien buildable structure maps
void UTIL_RefreshBuildableStructures();
// Will cycle through all dropped items in the map and update the marine items array
void UTIL_RefreshMarineItems();
// Will cycle through all func_resource entities and populate the ResourceNodes array
void UTIL_PopulateResourceNodeLocations();

int UTIL_GetStructureCountOfType(const NSStructureType StructureType);

bool UTIL_StructureIsFullyBuilt(const edict_t* Structure);

int UTIL_GetItemCountOfTypeInArea(const NSDeployableItem ItemType, const Vector& SearchLocation, const float Radius);

NSDeployableItem UTIL_GetItemTypeFromEdict(const edict_t* ItemEdict);
NSWeapon UTIL_GetWeaponTypeFromEdict(const edict_t* ItemEdict);

void UTIL_OnStructureCreated(buildable_structure* NewStructure);
void UTIL_OnStructureDestroyed(const NSStructureType Structure, const Vector Location);

void UTIL_LinkPlacedStructureToAction(bot_t* CommanderBot, buildable_structure* NewStructure);

// Is there a hive with the alien tech (Defence, Sensory, Movement) assigned to it?
bool UTIL_ActiveHiveWithTechExists(HiveTechStatus Tech);

const hive_definition* UTIL_GetNearestHiveOfStatus(const Vector SearchLocation, const HiveStatusType Status);


// Get the nearest hive to the location which is fully built (not in progress)
int UTIL_GetNearestBuiltHiveIndex(const Vector SearchLocation);

void SetNumberofHives(int NewValue);
void SetHiveLocation(int HiveIndex, const Vector NewLocation);
void SetHiveStatus(int HiveIndex, int NewStatus);
void SetHiveTechStatus(int HiveIndex, int NewTechStatus);
void SetHiveUnderAttack(int HiveIndex, bool bNewUnderAttack);
void SetHiveHealthPercent(int HiveIndex, float NewHealthPercent);

int UTIL_GetNumResNodes();

void PrintHiveInfo();

void AddMapLocation(const char* LocationName, Vector MinLocation, Vector MaxLocation);

char* UTIL_GetClosestMapLocationToPoint(const Vector Point);

buildable_structure* UTIL_GetBuildableStructureRefFromEdict(const edict_t* Structure);

edict_t* UTIL_GetClosestStructureAtLocation(const Vector& Location, bool bMarineStructures);

edict_t* UTIL_GetNearestItemOfType(const NSDeployableItem ItemType, const Vector Location, const float SearchDist);

const dropped_marine_item* UTIL_GetNearestItemIndexOfType(const NSDeployableItem ItemType, const Vector Location, const float SearchDist);
const dropped_marine_item* UTIL_GetNearestSpecialPrimaryWeapon(const Vector Location, const float SearchDist);
const dropped_marine_item* UTIL_GetNearestEquipment(const Vector Location, const float SearchDist);

edict_t* UTIL_GetNearestUnbuiltStructureWithLOS(bot_t* pBot, const Vector Location, const float SearchDist, const int Team);

edict_t* UTIL_GetNearestPlayerOfTeamInArea(const Vector Location, const float SearchRadius, const int Team, edict_t* IgnorePlayer, NSPlayerClass IgnoreClass);
int UTIL_GetNumPlayersOfTeamInArea(const Vector Location, const float SearchRadius, const int Team, edict_t* IgnorePlayer, NSPlayerClass IgnoreClass);
bool UTIL_IsPlayerOfTeamInArea(const Vector Location, const float SearchRadius, const int Team, edict_t* IgnorePlayer, NSPlayerClass IgnoreClas);
bool UTIL_IsAlienPlayerInArea(const Vector Location, float SearchRadius);
bool UTIL_IsAlienPlayerInArea(const Vector Location, float SearchRadius, edict_t* IgnorePlayer);
bool UTIL_IsNearActiveHive(const Vector Location, float SearchRadius);

edict_t* UTIL_GetFirstCompletedStructureOfType(const NSStructureType StructureType);
edict_t* UTIL_GetFirstIdleStructureOfType(const NSStructureType StructureType);

int UTIL_GetNumPlacedStructuresOfType(const NSStructureType StructureType);
int UTIL_GetNumPlacedStructuresOfTypeInRadius(const NSStructureType StructureType, const Vector Location, const float MaxRadius);
int UTIL_GetNumBuiltStructuresOfType(const NSStructureType StructureType);

int UTIL_GetNearestAvailableResourcePointIndex(const Vector& SearchPoint);
int UTIL_GetNearestOccupiedResourcePointIndex(const Vector& SearchPoint);

const resource_node* UTIL_FindNearestResNodeToLocation(const Vector& Location);
int UTIL_FindNearestResNodeIndexToLocation(const Vector& Location);

const resource_node* UTIL_FindEligibleResNodeClosestToLocation(const Vector& Location, const int Team, bool bIgnoreElectrified);
const resource_node* UTIL_FindEligibleResNodeFurthestFromLocation(const Vector& Location, const int Team, bool bIgnoreElectrified);

const resource_node* UTIL_AlienFindUnclaimedResNodeFurthestFromLocation(const bot_t* pBot, const Vector& Location, bool bIgnoreElectrified);

edict_t* UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(bot_t* pBot, const NSStructureType StructureType);
edict_t* UTIL_GetNearestStructureOfTypeInLocation(const NSStructureType StructureType, const Vector& Location, const float SearchRadius, bool bAllowElectrified);
bool UTIL_StructureOfTypeExistsInLocation(const NSStructureType StructureType, const Vector& Location, const float SearchRadius);

const resource_node* UTIL_GetNearestCappedResNodeToLocation(const Vector Location, int Team, bool bIgnoreElectrified);

bool UTIL_CommChairExists();
Vector UTIL_GetCommChairLocation();
edict_t* UTIL_GetCommChair();

edict_t* UTIL_GetNearestPlayerOfClass(const Vector Location, const NSPlayerClass SearchClass, const float SearchDist, const edict_t* PlayerToIgnore);

// Returns the index of the closest hive to the search location that has a phase gate within siege range of it (-1 if no hive found)
const hive_definition* UTIL_GetNearestHiveUnderSiege(const Vector SearchLocation);

bool UTIL_IsAnyHumanNearLocation(const Vector& Location, const float SearchDist);
bool UTIL_IsAnyHumanNearLocationWithoutEquipment(const Vector& Location, const float SearchDist);
bool UTIL_IsAnyHumanNearLocationWithoutSpecialWeapon(const Vector& Location, const float SearchDist);
edict_t* UTIL_GetNearestHumanAtLocation(const Vector& Location, const float SearchDist);

edict_t* UTIL_GetNearestStructureIndexOfType(const Vector& Location, NSStructureType StructureType, const float SearchDist, bool bFullyConstructedOnly);

int UTIL_FindClosestMarinePlayerToLocation(const edict_t* SearchingPlayer, const Vector& Location, const float SearchRadius);

edict_t* UTIL_FindClosestMarineStructureToLocation(const Vector& Location, const float SearchRadius, bool bAllowElectrified);

bool UTIL_AnyMarinePlayerNearLocation(const Vector& Location, float SearchRadius);
bool UTIL_AnyMarinePlayerWithLOS(const Vector& Location, float SearchRadius);

edict_t* UTIL_FindClosestMarineStructureUnbuilt(const Vector& SearchLocation, float SearchRadius);
edict_t* UTIL_FindClosestDamagedStructure(const Vector& SearchLocation, const int Team, float SearchRadius);
edict_t* UTIL_FindMarineWithDamagedArmour(const Vector& SearchLocation, float SearchRadius, edict_t* IgnoreEdict);

HiveStatusType UTIL_GetHiveStatus(const edict_t* Hive);

const hive_definition* UTIL_GetHiveAtIndex(int Index);
int UTIL_GetNumTotalHives();
int UTIL_GetNumActiveHives();
int UTIL_GetNumUnbuiltHives();

const hive_definition* UTIL_GetNearestHiveAtLocation(const Vector Location);

// Will find the nearest gorge, defence chamber or hive, whichever is closest to SearchLocation
edict_t* UTIL_AlienFindNearestHealingSpot(bot_t* pBot, const Vector SearchLocation);

/*	A resource node needs reinforcing if:
	1) It does not have at least 2 offence chambers by it
	2) Defence chambers are available and it has less than 2 near it
	3) Movement chambers are available and it doesn't have one near it
*/
bool UTIL_AlienResNodeNeedsReinforcing(int ResNodeIndex);

// Returns the nearest alien resource node that needs reinforcing (see UTIL_AlienResNodeNeedsReinforcing())
const resource_node* UTIL_GetNearestUnprotectedResNode(const Vector Location);

// Returns the first completed hive index which does not yet have a tech assigned to it. -1 if not found.
const hive_definition* UTIL_GetFirstHiveWithoutTech();

// Returns the index of a completed hive with the associated tech. -1 if not found.
const hive_definition* UTIL_GetHiveWithTech(HiveTechStatus Tech);

// Returns the closest unbuilt hive index which does not have a phase gate or turret factory in it. -1 if not found.
const hive_definition* UTIL_GetClosestViableUnbuiltHive(const Vector SearchLocation);

// Returns the nearest friendly player whose health or armour are not at max
edict_t* UTIL_GetClosestPlayerNeedsHealing(const Vector Location, const int Team, const float SearchRadius, edict_t* IgnorePlayer, bool bMustBeDirectlyReachable);

// Looks for any offence chambers or marine turrets that are a threat to the bot
edict_t* BotGetNearestDangerTurret(bot_t* pBot, float MaxDistance);

// Checks if the item lying on the floor is a primary weapon (MG, HMG, GL, Shotgun)
bool UTIL_DroppedItemIsPrimaryWeapon(NSDeployableItem ItemType);

// Is the input edict a valid marine structure?
bool UTIL_IsMarineStructure(const edict_t* Structure);
// Is the input edict a valid alien structure?
bool UTIL_IsAlienStructure(const edict_t* Structure);

// Is the input structure type a marine structure?
bool UTIL_IsMarineStructure(const NSStructureType StructureType);
// Is the input structure type an alien structure?
bool UTIL_IsAlienStructure(const NSStructureType StructureType);

// Checks to see if the gorge recently tried to place a building, and whether this structure was likely a result of it
void UTIL_LinkAlienStructureToTask(bot_t* pBot, edict_t* NewStructure);

// Returns how many of this weapon type are either equipped by a player, or dropped at the armoury. Ignores weapons dropped elsewhere in the map by dead marines
int UTIL_GetNumWeaponsOfTypeInPlay(const NSWeapon WeaponType);
// Returns how many heavy armour and jetpacks have been placed at base, and how many marines have heavy armour or a jetpack equipped
int UTIL_GetNumEquipmentInPlay();

#endif
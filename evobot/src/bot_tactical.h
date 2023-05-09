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
	bool bIsValid = false; // Is this a valid list entry?
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
	bool bIsReachableMarine = false; // Is the building reachable by marines? Checks from the comm chair location
	bool bIsReachableAlien = false; // Is the building reachable by aliens? Checks from the comm chair location
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
	bool bIsReachableMarine = false; // Is the item reachable by marines? Checks from the comm chair location
	int LastSeen = 0; // Which refresh cycle was this last seen on? Used to determine if the item has been removed from play
} dropped_marine_item;

// Data structure to hold information about each hive in the map
typedef struct _HIVE_DEFINITION_T
{
	bool bIsValid = false; // Doesn't exist. Array holds up to 10 hives even though usually only 3 exist
	edict_t* edict = NULL; // Refers to the hive edict. Always exists even if not built yet
	Vector Location = ZERO_VECTOR; // Origin of the hive
	Vector FloorLocation = ZERO_VECTOR; // Some hives are suspended in the air, this is the floor location directly beneath it
	HiveStatusType Status = HIVE_STATUS_UNBUILT; // Can be unbuilt, in progress, or fully built
	HiveTechStatus TechStatus = HIVE_TECH_NONE; // What tech (if any) is assigned to this hive right now
	int HealthPercent = 0; // How much health it has
	bool bIsUnderAttack = false; // Is the hive currently under attack? Becomes false if not taken damage for more than 10 seconds
	int HiveResNodeIndex = -1; // Which resource node (indexes into ResourceNodes array) belongs to this hive?
	unsigned int ObstacleRef = 0; // When in progress or built, will place an obstacle so bots don't try to walk through it

} hive_definition;

// How frequently to update the global list of built structures (in seconds)
static const float structure_inventory_refresh_rate = 0.2f;

// Increments by 1 every time the structure list is refreshed. Used to detect if structures have been destroyed and no longer show up
static int StructureRefreshFrame = 0;
// Increments by 1 every time the item list is refreshed. Used to detect if items have been removed from play and no longer show up
static int ItemRefreshFrame = 0;

// How frequently to update the global list of dropped marine items (in seconds)
static const float item_inventory_refresh_rate = 0.1f;


// Clears out the marine and alien buildable structure maps, resource node and hive lists, and the marine item list
void UTIL_ClearMapAIData();
// Clears out the MapLocations array
void UTIL_ClearMapLocations();
// Clear out all the hive information
void UTIL_ClearHiveInfo();

void PopulateEmptyHiveList();

// Returns the location of a randomly-selected resource node, comm chair, or hive
Vector UTIL_GetRandomPointOfInterest();

Vector UTIL_GetNearestPointOfInterestToLocation(const Vector SearchLocation, bool bUsePhaseDistance);

bool IsAlienTraitCategoryAvailable(HiveTechStatus TraitCategory);

unsigned char UTIL_GetAreaForObstruction(NSStructureType StructureType);
float UTIL_GetStructureRadiusForObstruction(NSStructureType StructureType);
bool UTIL_ShouldStructureCollide(NSStructureType StructureType);
void UTIL_UpdateBuildableStructure(edict_t* Structure);
void UTIL_UpdateMarineItem(edict_t* Item, NSDeployableItem ItemType);

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
void UTIL_OnItemDropped(const dropped_marine_item* NewItem);

void UTIL_LinkPlacedStructureToAction(bot_t* CommanderBot, buildable_structure* NewStructure);
void UTIL_LinkDroppedItemToAction(bot_t* CommanderBot, const dropped_marine_item* NewItem);

// Is there a hive with the alien tech (Defence, Sensory, Movement) assigned to it?
bool UTIL_ActiveHiveWithTechExists(HiveTechStatus Tech);

char* UTIL_BotRoleToChar(const BotRole Role);
const char* UTIL_StructTypeToChar(const NSStructureType StructureType);
const char* UTIL_DroppableItemTypeToChar(const NSDeployableItem ItemType);
const char* UTIL_ResearchTypeToChar(const NSResearch ResearchType);

const hive_definition* UTIL_GetActiveHiveWithoutChambers(HiveTechStatus ChamberType, int NumDesiredChambers);

const hive_definition* UTIL_GetNearestHiveOfStatus(const Vector SearchLocation, const HiveStatusType Status);
const hive_definition* UTIL_GetFurthestHiveOfStatus(const Vector SearchLocation, const HiveStatusType Status);

edict_t* UTIL_GetNearestMarineWithoutFullLoadout(const Vector SearchLocation, const float SearchRadius);


// Get the nearest hive to the location which is fully built (not in progress)
const hive_definition* UTIL_GetNearestBuiltHiveToLocation(const Vector SearchLocation);

// Taking phase gates into account, how far are the two points? Allows bots to intuit shortcuts using phase gates
float UTIL_GetPhaseDistanceBetweenPoints(const Vector StartPoint, const Vector EndPoint);

// Taking phase gates into account, how far are the two points? Allows bots to intuit shortcuts using phase gates. Returns squared distance.
float UTIL_GetPhaseDistanceBetweenPointsSq(const Vector StartPoint, const Vector EndPoint);

void SetNumberofHives(int NewValue);
void SetHiveLocation(int HiveIndex, const Vector NewLocation);
void SetHiveStatus(int HiveIndex, int NewStatus);
void SetHiveTechStatus(int HiveIndex, int NewTechStatus);
void SetHiveUnderAttack(int HiveIndex, bool bNewUnderAttack);
void SetHiveHealthPercent(int HiveIndex, int NewHealthPercent);

int UTIL_GetNumResNodes();

void PrintHiveInfo();

float GetCommanderViewZHeight();
void SetCommanderViewZHeight(float NewValue);
void AddMapLocation(const char* LocationName, Vector MinLocation, Vector MaxLocation);

char* UTIL_GetClosestMapLocationToPoint(const Vector Point);

bool UTIL_IsBuildableStructureStillReachable(bot_t* pBot, const edict_t* Structure);
bool UTIL_IsDroppedItemStillReachable(bot_t* pBot, const edict_t* Item);

buildable_structure* UTIL_GetBuildableStructureRefFromEdict(const edict_t* Structure);

edict_t* UTIL_GetClosestStructureAtLocation(const Vector& Location, bool bMarineStructures);

edict_t* UTIL_GetNearestItemOfType(const NSDeployableItem ItemType, const Vector Location, const float SearchDist);

edict_t* UTIL_GetNearestItemIndexOfType(const NSDeployableItem ItemType, const Vector Location, const float SearchDist);
edict_t* UTIL_GetNearestSpecialPrimaryWeapon(const Vector Location, const NSDeployableItem ExcludeItem, const float SearchDist, bool bUsePhaseDist);
edict_t* UTIL_GetNearestEquipment(const Vector Location, const float SearchDist, bool bUsePhaseDist);

edict_t* UTIL_GetNearestUnbuiltStructureWithLOS(bot_t* pBot, const Vector Location, const float SearchDist, const int Team);

edict_t* UTIL_GetNearestPlayerOfTeamInArea(const Vector Location, const float SearchRadius, const int Team, edict_t* IgnorePlayer, NSPlayerClass IgnoreClass);
int UTIL_GetNumPlayersOfTeamInArea(const Vector Location, const float SearchRadius, const int Team, edict_t* IgnorePlayer, NSPlayerClass IgnoreClass, bool bUsePhaseDist);
bool UTIL_IsPlayerOfTeamInArea(const Vector Location, const float SearchRadius, const int Team, edict_t* IgnorePlayer, NSPlayerClass IgnoreClas);
bool UTIL_IsAlienPlayerInArea(const Vector Location, float SearchRadius);
bool UTIL_IsAlienPlayerInArea(const Vector Location, float SearchRadius, edict_t* IgnorePlayer);
bool UTIL_IsNearActiveHive(const Vector Location, float SearchRadius);

edict_t* UTIL_GetFirstPlacedStructureOfType(const NSStructureType StructureType);
edict_t* UTIL_GetFirstCompletedStructureOfType(const NSStructureType StructureType);
edict_t* UTIL_GetFirstIdleStructureOfType(const NSStructureType StructureType);

int UTIL_GetNumPlacedStructuresOfType(const NSStructureType StructureType);
int UTIL_GetNumPlacedStructuresOfTypeInRadius(const NSStructureType StructureType, const Vector Location, const float MaxRadius);
int UTIL_GetNumBuiltStructuresOfTypeInRadius(const NSStructureType StructureType, const Vector Location, const float MaxRadius);
int UTIL_GetNumBuiltStructuresOfType(const NSStructureType StructureType);

int UTIL_GetNearestAvailableResourcePointIndex(const Vector& SearchPoint);
int UTIL_GetNearestOccupiedResourcePointIndex(const Vector& SearchPoint);

const resource_node* UTIL_FindNearestResNodeToLocation(const Vector& Location);
int UTIL_FindNearestResNodeIndexToLocation(const Vector& Location);

const resource_node* UTIL_FindEligibleResNodeClosestToLocation(const Vector& Location, const int Team, bool bIgnoreElectrified);
const resource_node* UTIL_FindEligibleResNodeFurthestFromLocation(const Vector& Location, const int Team, bool bIgnoreElectrified);

const resource_node* UTIL_MarineFindUnclaimedResNodeNearestLocation(const bot_t* pBot, const Vector& Location, float MinDist);
const resource_node* UTIL_AlienFindUnclaimedResNodeFurthestFromLocation(const bot_t* pBot, const Vector& Location, bool bIgnoreElectrified);

edict_t* UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(bot_t* pBot, const NSStructureType StructureType);
edict_t* UTIL_GetNearestStructureOfTypeInLocation(const NSStructureType StructureType, const Vector& Location, const float SearchRadius, bool bAllowElectrified, bool bUsePhaseDistance);
edict_t* UTIL_GetNearestUnbuiltStructureOfTypeInLocation(const NSStructureType StructureType, const Vector& Location, const float SearchRadius);
bool UTIL_StructureOfTypeExistsInLocation(const NSStructureType StructureType, const Vector& Location, const float SearchRadius);

edict_t* UTIL_GetNearestUnattackedStructureOfTeamInLocation(const Vector Location, edict_t* IgnoreStructure, const int Team, const float SearchRadius);

edict_t* UTIL_GetAnyStructureOfTypeNearActiveHive(const NSStructureType StructureType, bool bAllowElectrical, bool bFullyConstructedOnly);
edict_t* UTIL_GetAnyStructureOfTypeNearUnbuiltHive(const NSStructureType StructureType, bool bAllowElectrical, bool bFullyConstructedOnly);

const resource_node* UTIL_GetNearestCappedResNodeToLocation(const Vector Location, int Team, bool bIgnoreElectrified);

edict_t* UTIL_GetRandomStructureOfType(const NSStructureType StructureType, const edict_t* IgnoreInstance, bool bFullyConstructedOnly);

bool UTIL_CommChairExists();
Vector UTIL_GetCommChairLocation();
edict_t* UTIL_GetCommChair();

edict_t* UTIL_GetNearestPlayerOfClass(const Vector Location, const NSPlayerClass SearchClass, const float SearchDist, const edict_t* PlayerToIgnore);

// Returns the index of the closest hive to the search location that has a phase gate within siege range of it (-1 if no hive found)
const hive_definition* UTIL_GetNearestHiveUnderSiege(const Vector SearchLocation);

bool UTIL_IsAnyHumanNearLocation(const Vector& Location, const float SearchDist);
bool UTIL_IsAnyHumanNearLocationWithoutEquipment(const Vector& Location, const float SearchDist);
bool UTIL_IsAnyHumanNearLocationWithoutSpecialWeapon(const Vector& Location, const float SearchDist);
bool UTIL_IsAnyHumanNearLocationWithoutWeapon(const NSWeapon WeaponType, const Vector& Location, const float SearchDist);
edict_t* UTIL_GetNearestHumanAtLocation(const Vector& Location, const float SearchDist);

edict_t* UTIL_GetNearestStructureIndexOfType(const Vector& Location, NSStructureType StructureType, const float SearchDist, bool bFullyConstructedOnly, bool bUsePhaseGates);

int UTIL_FindClosestMarinePlayerToLocation(const edict_t* SearchingPlayer, const Vector& Location, const float SearchRadius);

edict_t* UTIL_FindClosestMarineStructureToLocation(const Vector& Location, const float SearchRadius, bool bAllowElectrified);

bool UTIL_AnyMarinePlayerNearLocation(const Vector& Location, float SearchRadius);
bool UTIL_AnyPlayerOnTeamWithLOS(const Vector& Location, const int Team, float SearchRadius);

edict_t* UTIL_GetClosestPlayerOnTeamWithLOS(const Vector& Location, const int Team, float SearchRadius, edict_t* IgnorePlayer);

edict_t* UTIL_FindClosestMarineStructureUnbuilt(const Vector& SearchLocation, float SearchRadius, bool bUsePhaseDistance);
edict_t* UTIL_FindClosestMarineStructureUnbuiltWithoutBuilders(bot_t* pBot, const int MaxBuilders, const Vector& SearchLocation, float SearchRadius, bool bUsePhaseDistance);
edict_t* UTIL_FindClosestMarineStructureOfTypeUnbuilt(const NSStructureType StructureType, const Vector& SearchLocation, float SearchRadius, bool bUsePhaseDistance);
edict_t* UTIL_FindClosestDamagedStructure(const Vector& SearchLocation, const int Team, float SearchRadius, bool bUsePhaseDistance);
edict_t* UTIL_FindMarineWithDamagedArmour(const Vector& SearchLocation, float SearchRadius, edict_t* IgnoreEdict);

HiveStatusType UTIL_GetHiveStatus(const edict_t* Hive);

edict_t* UTIL_FindSafePlayerInArea(const int Team, const Vector SearchLocation, float MinRadius, float MaxRadius);

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

// Returns true if there is a hive in progress
bool UTIL_HiveIsInProgress();

// Returns the index of a completed hive with the associated tech. -1 if not found.
const hive_definition* UTIL_GetHiveWithTech(HiveTechStatus Tech);

// Returns the closest unbuilt hive index which does not have a phase gate or turret factory in it. -1 if not found.
const hive_definition* UTIL_GetClosestViableUnbuiltHive(const Vector SearchLocation);

// Returns the nearest friendly player whose health or armour are not at max
edict_t* UTIL_GetClosestPlayerNeedsHealing(const Vector Location, const int Team, const float SearchRadius, edict_t* IgnorePlayer, bool bMustBeDirectlyReachable);

// Looks for any offence chambers or marine turrets that are a threat to the bot
edict_t* BotGetNearestDangerTurret(bot_t* pBot, float MaxDistance);
edict_t* PlayerGetNearestDangerTurret(const edict_t* Player, float MaxDistance);

// Checks if the item lying on the floor is a primary weapon (MG, HMG, GL, Shotgun)
bool UTIL_DroppedItemIsPrimaryWeapon(NSDeployableItem ItemType);

// Is the input edict a valid marine structure?
bool UTIL_IsMarineStructure(const edict_t* Structure);
// Is the input edict a valid alien structure?
bool UTIL_IsAlienStructure(const edict_t* Structure);

bool UTIL_AnyTurretWithLOSToLocation(const Vector Location, const int TurretTeam);

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

// Should the commander use distress beacon? Determines if the base is being overwhelmed by aliens
bool UTIL_BaseIsInDistress();

// Returns true if the marines have completed this research. NOTE: This will return false if the research is complete but the building is missing
// e.g. phase tech will return false if there are no completed observatories even if the tech itself was previously researched
bool UTIL_ResearchIsComplete(const NSResearch Research);

bool UTIL_StructureExistsOfType(const NSStructureType StructureType);

float UTIL_DistToNearestFriendlyPlayer(const Vector& Location, int DesiredTeam);

NSStructureType GetStructureTypeFromEdict(const edict_t* StructureEdict);
NSStructureType UTIL_IUSER3ToStructureType(const int inIUSER3);

bool UTIL_StructureTypesMatch(const NSStructureType TypeOne, const NSStructureType TypeTwo);

NSStructureType UTIL_GetChamberTypeForHiveTech(const HiveTechStatus HiveTech);

bool UTIL_StructureIsRecycling(const edict_t* Structure);
bool UTIL_StructureIsUpgrading(const edict_t* Structure);

bool UTIL_IsArmouryUpgrading(const edict_t* ArmouryEdict);
bool UTIL_IsTurretFactoryUpgrading(const edict_t* TurretFactoryEdict);

bool UTIL_StructureIsResearching(const edict_t* Structure);
bool UTIL_StructureIsResearching(const edict_t* Structure, const NSResearch Research);
bool UTIL_IsStructureElectrified(const edict_t* Structure);

NSDeployableItem UTIL_WeaponTypeToDeployableItem(const NSWeapon WeaponType);

AvHUpgradeMask UTIL_GetResearchMask(const NSResearch Research);

int UTIL_GetCostOfStructureType(NSStructureType StructureType);

int UTIL_StructureTypeToImpulseCommand(const NSStructureType StructureType);

bool UTIL_IsThereACommander();

#endif
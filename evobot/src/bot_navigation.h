//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_navigation.h
// 
// Handles all bot path finding and movement
//

#pragma once
#ifndef BOT_NAVIGATION_H
#define BOT_NAVIGATION_H

//#include <extdll.h>
//#include "bot_math.h"
#include "DetourStatus.h"
#include "DetourNavMeshQuery.h"
#include "player_util.h"
#include "bot_structs.h"

/*	Navigation profiles determine which nav mesh (regular, onos, building) is used for queries, and what
	types of movement are allowed and their costs. For example, marine nav profile uses regular nav mesh,
	cannot wall climb, and has a higher cost for crouch movement since it's slower.
*/

constexpr auto MARINE_REGULAR_NAV_PROFILE = 0;

constexpr auto SKULK_REGULAR_NAV_PROFILE = 1;
constexpr auto SKULK_AMBUSH_NAV_PROFILE = 2;

constexpr auto GORGE_REGULAR_NAV_PROFILE = 3;
constexpr auto GORGE_HIDE_NAV_PROFILE = 4;

constexpr auto FADE_REGULAR_NAV_PROFILE = 5;

constexpr auto ONOS_REGULAR_NAV_PROFILE = 6;

constexpr auto BUILDING_REGULAR_NAV_PROFILE = 7;

constexpr auto ALL_NAV_PROFILE = 8;


constexpr auto MIN_PATH_RECALC_TIME = 0.33f; // How frequently can a bot recalculate its path? Default to max 3 times per second


#define MAX_PATH_POLY 512 // Max nav mesh polys that can be traversed in a path. This should be sufficient for any sized map.

// Possible area types. Water, Road, Door and Grass are not used (left-over from Detour library)
enum SamplePolyAreas
{
	SAMPLE_POLYAREA_GROUND = 0,
	SAMPLE_POLYAREA_CROUCH = 1,
	SAMPLE_POLYAREA_WATER = 2,
	SAMPLE_POLYAREA_BLOCKED = 3,
	SAMPLE_POLYAREA_WALLCLIMB = 4,
	SAMPLE_POLYAREA_LADDER = 5,
	SAMPLE_POLYAREA_DOOR = 6,
	SAMPLE_POLYAREA_JUMP = 7,
	SAMPLE_POLYAREA_HIGHJUMP = 8,
	SAMPLE_POLYAREA_FALL = 9,
	SAMPLE_POLYAREA_HIGHFALL = 10,
	SAMPLE_POLYAREA_PHASEGATE = 11,
	SAMPLE_POLYAREA_MSTRUCTURE = 12,
	SAMPLE_POLYAREA_ASTRUCTURE = 13,
	SAMPLE_POLYAREA_FLY = 14
};

// Possible movement types. Swim and door are not used
enum SamplePolyFlags
{
	SAMPLE_POLYFLAGS_WALK = 1 << 0,		// Simple walk to traverse
	SAMPLE_POLYFLAGS_CROUCH = 1 << 1,		// Required crouching to traverse
	SAMPLE_POLYFLAGS_SWIM = 1 << 2,		// Requires swimming to traverse (not used)
	SAMPLE_POLYFLAGS_BLOCKED = 1 << 3,		// Blocked by an obstruction, but can be jumped over
	SAMPLE_POLYFLAGS_WALLCLIMB = 1 << 4,		// Requires climbing a wall to traverse
	SAMPLE_POLYFLAGS_LADDER = 1 << 5,		// Requires climbing a ladder to traverse
	SAMPLE_POLYFLAGS_DOOR = 1 << 6,		// Requires opening a door to traverse (not used)
	SAMPLE_POLYFLAGS_JUMP = 1 << 7,		// Requires a jump to traverse
	SAMPLE_POLYFLAGS_HIGHJUMP = 1 << 8,		// Requires a jump from a high height to traverse
	SAMPLE_POLYFLAGS_FALL = 1 << 9,		// Requires dropping down from a safe height to traverse
	SAMPLE_POLYFLAGS_HIGHFALL = 1 << 10,		// Requires dropping from a high height to traverse
	SAMPLE_POLYFLAGS_DISABLED = 1 << 11,		// Disabled, not usable by anyone
	SAMPLE_POLYFLAGS_NOONOS = 1 << 12,		// This movement is not allowed by onos
	SAMPLE_POLYFLAGS_PHASEGATE = 1 << 13,		// Requires using a phase gate to traverse
	SAMPLE_POLYFLAGS_MSTRUCTURE = 1 << 14,		// Marine Structure in the way, must be destroyed if alien, or impassable if marine
	SAMPLE_POLYFLAGS_ASTRUCTURE = 1 << 15,		// Structure in the way, must be destroyed if marine, or impassable if alien
	SAMPLE_POLYFLAGS_FLY = 1 << 16,		// Structure in the way, must be destroyed if marine, or impassable if alien
	SAMPLE_POLYFLAGS_ALL = 0xffff		// All abilities.
};

// Door type. Not currently used, future feature so bots know how to open a door
enum DoorActivationType
{
	DOOR_NONE,   // No type
	DOOR_USE,    // Door activated by using it
	DOOR_TRIGGER,// Door activated by trigger_once or trigger_multiple
	DOOR_BUTTON, // Door activated by pressing a button
	DOOR_WELD,   // Door activated by welding something
	DOOR_SHOOT   // Door activated by being shot
};

// Door reference. Not used, but is a future feature to allow bots to track if a door is open or not, and how to open it etc.
typedef struct _NAV_DOOR
{
	edict_t* DoorEdict = nullptr; // Reference to the func_door
	unsigned int ObstacleRef = 0; // Dynamic obstacle ref. Used to add/remove the obstacle as the door is opened/closed
	edict_t* TriggerEdict = nullptr; // Reference to the trigger edict (e.g. func_trigger, func_button etc.)
	DoorActivationType ActivationType = DOOR_NONE; // How the door should be opened
	Vector PositionOne = ZERO_VECTOR; // Door's starting position
	Vector PositionTwo = ZERO_VECTOR; // Door's open/close position (depending on if it starts open or not)
	Vector CurrentPosition = ZERO_VECTOR; // Current world position
} nav_door;

// Links together a tile cache, nav query and the nav mesh into one handy structure for all your querying needs
typedef struct _NAV_MESH
{
	class dtTileCache* tileCache;
	class dtNavMeshQuery* navQuery;
	class dtNavMesh* navMesh;
} nav_mesh;

// A nav profile combines a nav mesh reference (indexed into NavMeshes) and filters to determine how a bot should find paths
typedef struct _NAV_PROFILE
{
	int NavMeshIndex = -1;
	dtQueryFilter Filters;
} nav_profile;

static const int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T'; //'MSET', used to confirm the nav mesh we're loading is compatible;
static const int NAVMESHSET_VERSION = 1;

static const int TILECACHESET_MAGIC = 'T' << 24 | 'S' << 16 | 'E' << 8 | 'T'; //'TSET', used to confirm the tile cache we're loading is compatible;
static const int TILECACHESET_VERSION = 1;

static const float pExtents[3] = { 400.0f, 50.0f, 400.0f }; // Default extents (in GoldSrc units) to find the nearest spot on the nav mesh
static const float pReachableExtents[3] = { max_player_use_reach, max_player_use_reach, max_player_use_reach }; // Extents (in GoldSrc units) to determine if something is on the nav mesh

static const int MAX_NAV_MESHES = 8; // Max number of nav meshes allowed. Currently 3 are used (one for building placement, one for the onos, and a regular one for everyone else)
static const int MAX_NAV_PROFILES = 16; // Max number of possible nav profiles. Currently 9 are used (see top of this header file)

static const int REGULAR_NAV_MESH = 0;
static const int ONOS_NAV_MESH = 1;
static const int BUILDING_NAV_MESH = 2;

static const int DT_AREA_NULL = 0; // Represents a null area on the nav mesh. Not traversable and considered not on the nav mesh
static const int DT_AREA_BLOCKED = 3; // Area occupied by an obstruction (e.g. building). Not traversable, but considered to be on the nav mesh

static const int MAX_OFFMESH_CONNS = 1024; // Max number of dynamic connections that can be placed. Not currently used (connections are baked into the nav mesh using the external tool)

static const int DOOR_USE_ONLY = 256; // Flag used by GoldSrc to determine if a door entity can only be used to open (i.e. can't be triggered)

static const float CHECK_STUCK_INTERVAL = 0.1f; // How frequently should the bot check if it's stuck?

static nav_mesh NavMeshes[MAX_NAV_MESHES]; // Array of nav meshes. Currently only 3 are used (building, onos, and regular)
static nav_profile NavProfiles[MAX_NAV_PROFILES]; // Array of nav profiles

// Returns true if a valid nav mesh has been loaded into memory
bool NavmeshLoaded();
// Unloads all data, including loaded nav meshes, nav profiles, all the map data such as buildable structure maps and hive locations.
void UnloadNavigationData();
// Searches for the corresponding .nav file for the input map name, and loads/initiatialises the nav meshes and nav profiles.
bool loadNavigationData(const char* mapname);

// FUTURE FEATURE: Will eventually link a door to the trigger than opens it
void UTIL_LinkTriggerToDoor(const edict_t* DoorEdict, nav_door* DoorRef);

// Finds any random point on the navmesh that is relevant for the bot. Returns ZERO_VECTOR if none found
Vector UTIL_GetRandomPointOnNavmesh(const bot_t* pBot);

/*	Finds any random point on the navmesh that is relevant for the bot within a given radius of the origin point,
	taking reachability into account(will not return impossible to reach location).

	Returns ZERO_VECTOR if none found
*/
Vector UTIL_GetRandomPointOnNavmeshInRadius(const int NavProfileIndex, const Vector origin, const float MaxRadius);

/*	Finds any random point on the navmesh that is relevant for the bot within a given radius of the origin point,
	ignores reachability (could return a location that isn't actually reachable for the bot).

	Returns ZERO_VECTOR if none found
*/
Vector UTIL_GetRandomPointOnNavmeshInRadiusIgnoreReachability(const int NavProfileIndex, const Vector origin, const float MaxRadius);

/*	Finds any random point on the navmesh of the area type (e.g. crouch area) that is relevant for the bot within a given radius of the origin point,
	taking reachability into account(will not return impossible to reach location).

	Returns ZERO_VECTOR if none found
*/
Vector UTIL_GetRandomPointOnNavmeshInRadiusOfAreaType(SamplePolyFlags Flag, const Vector origin, const float MaxRadius);

/*	Finds any random point on the navmesh of the area type (e.g. crouch area) that is relevant for the bot within the min and max radius of the origin point,
	taking reachability into account(will not return impossible to reach location).

	Returns ZERO_VECTOR if none found
*/
Vector UTIL_GetRandomPointOnNavmeshInDonut(const int NavProfile, const Vector origin, const float MinRadius, const float MaxRadius);

/*	Finds any random point on the navmesh of the area type (e.g. crouch area) that is relevant for the bot within the min and max radius of the origin point,
	ignores reachability (could return a location that isn't actually reachable for the bot).

	Returns ZERO_VECTOR if none found
*/
Vector UTIL_GetRandomPointOnNavmeshInDonutIgnoreReachability(const int NavProfile, const Vector origin, const float MinRadius, const float MaxRadius);

// Roughly estimates the movement cost to move between FromLocation and ToLocation. Uses simple formula of distance between points x cost modifier for that movement
float UTIL_GetPathCostBetweenLocations(const int NavProfileIndex, const Vector FromLocation, const Vector ToLocation);

// Returns true is the bot is grounded, on the nav mesh, and close enough to the Destination to be considered at that point
bool BotIsAtLocation(const bot_t* pBot, const Vector Destination);

// Sets the bot's desired movement direction and performs jumps/crouch/etc. to traverse the current path point
void NewMove(bot_t* pBot);
// Returns true if the bot has completed the current movement along their path
bool HasBotReachedPathPoint(const bot_t* pBot);
// Returns true if the bot is considered to have strayed off the path (e.g. missed a jump and fallen)
bool IsBotOffPath(const bot_t* pBot);

// Called by NewMove, determines the movement direction and inputs required to walk/crouch between start and end points
void GroundMove(bot_t* pBot, const Vector StartPoint, const Vector EndPoint);
// Called by NewMove, determines the movement direction and inputs required to jump between start and end points
void JumpMove(bot_t* pBot, const Vector StartPoint, const Vector EndPoint);
// Called by NewMove, determines movement direction and jump inputs to hop over obstructions (structures)
void BlockedMove(bot_t* pBot, const Vector StartPoint, const Vector EndPoint);
// Called by NewMove, determines the movement direction and inputs required to drop down from start to end points
void FallMove(bot_t* pBot, const Vector StartPoint, const Vector EndPoint);
// Called by NewMove, determines the movement direction and inputs required to climb a ladder to reach endpoint
void LadderMove(bot_t* pBot, const Vector StartPoint, const Vector EndPoint, float RequiredClimbHeight, unsigned char NextArea);
// Called by NewMove, determines the movement direction and inputs required to climb a wall to reach endpoint
void WallClimbMove(bot_t* pBot, const Vector StartPoint, const Vector EndPoint, float RequiredClimbHeight);
void BlinkClimbMove(bot_t* pBot, const Vector StartPoint, const Vector EndPoint, float RequiredClimbHeight);
// Called by NewMove, determines the movement direction and inputs required to use a phase gate to reach end point
void PhaseGateMove(bot_t* pBot, const Vector StartPoint, const Vector EndPoint);

// Will check for any func_breakable which might be in the way (e.g. window, vent) and make the bot aim and attack it to break it. Marines will switch to knife to break it.
void CheckAndHandleBreakableObstruction(bot_t* pBot, const Vector MoveFrom, const Vector MoveTo);

// Clears all tracking of a bot's stuck status
void ClearBotStuck(bot_t* pBot);
// Clears all bot movement data, including the current path, their stuck status. Effectively stops all movement the bot is performing.
void ClearBotMovement(bot_t* pBot);

// Will draw all temporary obstacles placed on the nav mesh, within a 10 metre (525 unit) radius
void UTIL_DrawTemporaryObstacles();

// Checks if the bot has managed to make progress towards MoveDestination. Move destination should be the bot's current path point, not overall destination
bool IsBotStuck(bot_t* pBot, const Vector MoveDestination);

// Called every bot frame (default is 60fps). Ensures the tile cache is updated after obstacles are placed
void UTIL_UpdateTileCache();

void RecalcAllBotPaths();

Vector UTIL_GetNearestPointOnNavWall(bot_t* pBot, const float MaxRadius);
Vector UTIL_GetNearestPointOnNavWall(const int NavProfileIndex, const Vector Location, const float MaxRadius);

/*	Places a temporary obstacle of the given height and radius on the mesh.Will modify that part of the nav mesh to be the given area.
	An example use case is to place an obstacle of area type SAMPLE_POLYAREA_OBSTRUCTION to mark where buildings are.
	Using DT_AREA_NULL will effectively cut a hole in the nav mesh, meaning it's no longer considered a valid mesh position.
*/
unsigned int UTIL_AddTemporaryObstacle(const Vector Location, float Radius, float Height, int area);
unsigned int UTIL_AddTemporaryBoxObstacle(const Vector Location, Vector HalfExtents, float OrientationInRadians, int area);

/*	Removes the temporary obstacle from the mesh. The area will return to its default type (either walk or crouch).
	Removing a DT_AREA_NULL obstacle will "fill in" the hole again, making it traversable and considered a valid mesh position.
*/
void UTIL_RemoveTemporaryObstacle(unsigned int ObstacleRef);

// Not currently working. Intended to draw the nav mesh polys within a radius of the player, colour coded by area. Does nothing right now.
void DEBUG_DrawNavMesh(const Vector DrawCentre, const int NavMeshIndex);

// Will draw a line between each path node in the path, colour coded by the movement flag (e.g. white for walk, red for crouch, yellow for jump etc.)
void DEBUG_DrawPath(const bot_path_node* Path, const int PathSize, const float DrawTimeInSeconds);

/*
	Safely aborts the current movement the bot is performing. Returns true if the bot has successfully aborted, and is ready to calculate a new path.

	The purpose of this is to avoid sudden path changes while on a ladder or performing a wall climb, which can cause the bot to get confused.

	NewDestination is where the bot wants to go now, so it can figure out how best to abort the current move.
*/
bool AbortCurrentMove(bot_t* pBot, const Vector NewDestination);


// Will clear current path and recalculate it for the supplied destination
bool BotRecalcPath(bot_t* pBot, const Vector Destination);

// Checks if the bot is able to navigate to the comm chair or hives. Will return true the moment it finds a reachable destination.
// If false, this indicates the bot is stuck and doesn't know how to get to any of the key map points.
bool AreKeyPointsReachableForBot(bot_t* pBot);

/*	Main function for instructing a bot to move to the destination, and what type of movement to favour. Should be called every frame you want the bot to move.
	Will handle path calculation, following the path, detecting if stuck and trying to unstick itself.
	Will only recalculate paths if it decides it needs to, so is safe to call every frame.
*/
bool MoveTo(bot_t* pBot, const Vector Destination, const BotMoveStyle MoveStyle);

// Used by the MoveTo command, handles the bot's movement and inputs to follow a path it has calculated for itself
void BotFollowPath(bot_t* pBot);
void BotFollowFlightPath(bot_t* pBot);

int GetNextDirectFlightPath(bot_t* pBot);

// Walks directly towards the destination. No path finding, just raw movement input. Will detect obstacles and try to jump/duck under them.
void MoveDirectlyTo(bot_t* pBot, const Vector Destination);

// Check if there are any players in our way and try to move around them. If we can't, then back up to let them through
void HandlePlayerAvoidance(bot_t* pBot, const Vector MoveDestination);

// Finds a path between two supplied points, using the supplied nav profile. Will return a failure if it can't reach the exact given location, and bAllowPartial is false.
// If bAllowPartial is true, it will return success and provide the partial path that got it as close as possible to the destination
dtStatus FindPathToPoint(const int NavProfileIndex, const Vector FromLocation, const Vector ToLocation, bot_path_node* path, int* pathSize, bool bAllowPartial);

// Special path finding that takes the presence of phase gates into account 
dtStatus FindPhaseGatePathToPoint(const int NavProfileIndex, Vector FromLocation, Vector ToLocation, bot_path_node* path, int* pathSize, float MaxAcceptableDistance);

// Special path finding that takes the presence of phase gates into account 
dtStatus FindFlightPathToPoint(const int NavProfileIndex, Vector FromLocation, Vector ToLocation, bot_path_node* path, int* pathSize, float MaxAcceptableDistance);

Vector UTIL_FindHighestSuccessfulTracePoint(const Vector TraceFrom, const Vector TargetPoint, const Vector NextPoint, const float IterationStep, const float MinIdealHeight, const float MaxHeight);

// Similar to FindPathToPoint, but you can specify a max acceptable distance for partial results. Will return a failure if it can't reach at least MaxAcceptableDistance away from the ToLocation
dtStatus FindPathClosestToPoint(bot_t* pBot, const BotMoveStyle MoveStyle, const Vector FromLocation, const Vector ToLocation, bot_path_node* path, int* pathSize, float MaxAcceptableDistance);
dtStatus FindPathClosestToPoint(const int NavProfileIndex, const Vector FromLocation, const Vector ToLocation, bot_path_node* path, int* pathSize, float MaxAcceptableDistance);
dtStatus FindDetailedPathClosestToPoint(const int NavProfileIndex, const Vector FromLocation, const Vector ToLocation, bot_path_node* path, int* pathSize, float MaxAcceptableDistance);

// If the bot is stuck and off the path or nav mesh, this will try to find a point it can directly move towards to get it back on track
Vector FindClosestPointBackOnPath(bot_t* pBot);

Vector FindClosestNavigablePointToDestination(const int NavProfileIndex, const Vector FromLocation, const Vector ToLocation, float MaxAcceptableDistance);

// Will attempt to move directly towards MoveDestination while jumping/ducking as needed, and avoiding obstacles in the way
void PerformUnstuckMove(bot_t* pBot, const Vector MoveDestination);

// Used by Detour for the FindRandomPointInCircle type functions
static float frand();

// Finds the appropriate nav mesh for the requested profile
const dtNavMesh* UTIL_GetNavMeshForProfile(const int NavProfileIndex);
// Finds the appropriate nav mesh query for the requested profile
const dtNavMeshQuery* UTIL_GetNavMeshQueryForProfile(const int NavProfileIndex);
// Finds the appropriate query filter for the requested profile
const dtQueryFilter* UTIL_GetNavMeshFilterForProfile(const int NavProfileIndex);
// Finds the appropriatetile cache for the requested profile
const dtTileCache* UTIL_GetTileCacheForProfile(const int NavProfileIndex);

float UTIL_PointIsDirectlyReachable_DEBUG(const Vector start, const Vector target);

/*
	Point is directly reachable:
	Determines if the bot can walk directly between the two points.
	Ignores map geometry, so will return true even if stairs are in the way, provided the bot can walk up/down them unobstructed
*/
bool UTIL_PointIsDirectlyReachable(const bot_t* pBot, const Vector targetPoint);
bool UTIL_PointIsDirectlyReachable(const bot_t* pBot, const Vector start, const Vector target);
bool UTIL_PointIsDirectlyReachable(const Vector start, const Vector target);
bool UTIL_PointIsDirectlyReachable(const int NavProfileIndex, const Vector start, const Vector target);

// Will trace along the nav mesh from start to target and return true if the trace reaches within MaxAcceptableDistance
bool UTIL_TraceNav(const int NavProfileIndex, const Vector start, const Vector target, const float MaxAcceptableDistance);

/*
	Project point to navmesh:
	Takes the supplied location in the game world, and returns the nearest point on the nav mesh within the supplied extents.
	Uses pExtents by default if not supplying one.
	Returns ZERO_VECTOR if not projected successfully
*/
Vector UTIL_ProjectPointToNavmesh(const Vector Location);
Vector UTIL_ProjectPointToNavmesh(const Vector Location, const Vector Extents);
Vector UTIL_ProjectPointToNavmesh(const Vector Location, const int NavProfileIndex);
Vector UTIL_ProjectPointToNavmesh(const Vector Location, const Vector Extents, const int NavProfileIndex);

/*
	Point is on navmesh:
	Returns true if it was able to project the point to the navmesh (see UTIL_ProjectPointToNavmesh())
*/
bool UTIL_PointIsOnNavmesh(const Vector Location, const int NavProfileIndex);
bool UTIL_PointIsOnNavmesh(const int NavProfileIndex, const Vector Location, const Vector SearchExtents);

int UTIL_GetMoveProfileForBot(const bot_t* pBot, BotMoveStyle MoveStyle);


int UTIL_GetMoveProfileForMarine(const BotMoveStyle MoveStyle);
int UTIL_GetMoveProfileForSkulk(const BotMoveStyle MoveStyle);
int UTIL_GetMoveProfileForGorge(const BotMoveStyle MoveStyle);
int UTIL_GetMoveProfileForLerk(const BotMoveStyle MoveStyle);
int UTIL_GetMoveProfileForFade(const BotMoveStyle MoveStyle);
int UTIL_GetMoveProfileForOnos(const BotMoveStyle MoveStyle);

// Sets the BotNavInfo so the bot can track if it's on the ground, in the air, climbing a wall, on a ladder etc.
void UTIL_UpdateBotMovementStatus(bot_t* pBot);

// Returns true if a path could be found between From and To location. Cheaper than full path finding, only a rough check to confirm it can be done.
bool UTIL_PointIsReachable(const int NavProfileIndex, const Vector FromLocation, const Vector ToLocation, const float MaxAcceptableDistance);

// If the bot has a path, it will work out how far along the path it can see and return the furthest point. Used so that the bot looks ahead along the path rather than just at its next path point
Vector UTIL_GetFurthestVisiblePointOnPath(const bot_t* pBot);
// For the given viewer location and path, will return the furthest point along the path the viewer could see
Vector UTIL_GetFurthestVisiblePointOnPath(const Vector ViewerLocation, const bot_path_node* path, const int pathSize);


// Returns the nearest nav mesh poly reference for the edict's current world position
dtPolyRef UTIL_GetNearestPolyRefForEntity(const edict_t* Edict);
dtPolyRef UTIL_GetNearestPolyRefForLocation(const Vector Location);
dtPolyRef UTIL_GetNearestPolyRefForLocation(const int NavProfileIndex, const Vector Location);

// Returns the area for the nearest nav mesh poly to the given location. Returns BLOCKED if none found
unsigned char UTIL_GetNavAreaAtLocation(const Vector Location);
unsigned char UTIL_GetNavAreaAtLocation(const int NavProfile, const Vector Location);

// For printing out human-readable nav mesh areas
const char* UTIL_NavmeshAreaToChar(const unsigned char Area);

Vector UTIL_GetNearestLadderNormal(edict_t* pEdict);
Vector UTIL_GetNearestLadderCentrePoint(edict_t* pEdict);
Vector UTIL_GetNearestLadderTopPoint(edict_t* pEdict);
Vector UTIL_GetNearestLadderBottomPoint(edict_t* pEdict);

// From the given start point, determine how high up the bot needs to climb to get to climb end. Will allow the bot to climb over railings
float UTIL_FindZHeightForWallClimb(const Vector ClimbStart, const Vector ClimbEnd, const int HullNum);


// Clears the bot's path and sets the path size to 0
void ClearBotPath(bot_t* pBot);
// Clears just the bot's current stuck movement attempt (see PerformUnstuckMove())
void ClearBotStuckMovement(bot_t* pBot);

// Draws just the bot's next movement on its path. Colour-coded based on the movement type (e.g. walk, crouch, jump, ladder)
void DEBUG_DrawBotNextPathPoint(bot_t* pBot, float TimeInSeconds);

// Based on the direction the bot wants to move and it's current facing angle, sets the forward and side move, and the directional buttons to make the bot actually move
void BotMovementInputs(bot_t* pBot);

// Event called when a bot starts climbing a ladder
void OnBotStartLadder(bot_t* pBot);
// Event called when a bot leaves a ladder
void OnBotEndLadder(bot_t* pBot);

// Not in use yet, will track all doors and their current status
void UTIL_PopulateDoors();

// If the bot has a path, will draw it out in full if bShort is false, or just the first 5 path nodes if bShort is true
void BotDrawPath(bot_t* pBot, float DrawTimeInSeconds, bool bShort);

void DEBUG_TestFlightPathFind(edict_t* pEdict, const Vector Destination);

Vector UTIL_AdjustPointAwayFromNavWall(const Vector Location, const float MaxDistanceFromWall);

#endif // BOT_NAVIGATION_H


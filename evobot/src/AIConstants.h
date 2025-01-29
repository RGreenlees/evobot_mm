#pragma once

#ifndef AI_CONSTANTS_H
#define AI_CONSTANTS_H

#include <extdll.h>
#include <meta_api.h>
#include <dllapi.h>

#include <vector>

#include "AIMath.h"

#include "DetourStatus.h"
#include "DetourNavMeshQuery.h"

#include "nav_constants.h"


static const float commander_action_cooldown = 1.0f;
static const float min_request_spam_time = 10.0f;

constexpr auto MAX_AI_PATH_SIZE = 512; // Maximum number of points allowed in a path (this should be enough for any sized map)

static const int MAX_PLAYERS = 32;

// Weapon types. Each number refers to the GoldSrc weapon index. Rename these to something more descriptive if you wish
typedef enum
{
	WEAPON_INVALID = 0,

	WEAPON_TYPE_1 = 1,
	WEAPON_TYPE_2 = 2,
	WEAPON_TYPE_3 = 3,
	WEAPON_TYPE_4 = 4,
	WEAPON_TYPE_5 = 5,
	WEAPON_TYPE_6 = 6,
	WEAPON_TYPE_7 = 7,
	WEAPON_TYPE_8 = 8,
	WEAPON_TYPE_9 = 9,
	WEAPON_TYPE_10 = 10,
	WEAPON_TYPE_11 = 11,
	WEAPON_TYPE_12 = 12,
	WEAPON_TYPE_13 = 13,
	WEAPON_TYPE_14 = 14,
	WEAPON_TYPE_15 = 15,
	WEAPON_TYPE_16 = 16,
	WEAPON_TYPE_17 = 17,
	WEAPON_TYPE_18 = 18,
	WEAPON_TYPE_19 = 19,
	WEAPON_TYPE_20 = 20,
	WEAPON_TYPE_21 = 21,
	WEAPON_TYPE_22 = 22,
	WEAPON_TYPE_23 = 23,
	WEAPON_TYPE_24 = 24,
	WEAPON_TYPE_25 = 25,
	WEAPON_TYPE_26 = 26,
	WEAPON_TYPE_27 = 27,
	WEAPON_TYPE_28 = 28,
	WEAPON_TYPE_29 = 29,
	WEAPON_TYPE_30 = 30,	

	WEAPON_MAX = 31
} AIWeaponType;

// Reachability flag, quick short-hand to determine if a bot can reach a point or not
typedef enum _AI_REACHABILITY_STATUS
{
	AI_REACHABILITY_NONE = 0,
	AI_REACHABILITY_REGULAR = 1u << 0,

	AI_REACHABILITY_UNREACHABLE = 1u << 1,

	AI_REACHABILITY_ALL = -1
} AIReachabilityStatus;

typedef enum _AINAVMESHSTATUS
{
	NAVMESH_STATUS_PENDING = 0,	// Waiting to try loading the navmesh
	NAVMESH_STATUS_FAILED,		// Failed to load the navmesh
	NAVMESH_STATUS_SUCCESS		// Successfully loaded the navmesh
} AINavMeshStatus;

typedef enum
{
	// Special or misc. actions
	MESSAGE_NULL = 0,

	// Use an item or ability (these are currently forced to be less than 10 because it's
	// used as an index into some weapons/equipment array <sigh>)
	WEAPON_NEXT = 1,
	WEAPON_RELOAD = 2,
	WEAPON_DROP = 3

} AIImpulseMessages;

typedef struct _OFF_MESH_CONN
{
	unsigned int NavMeshIndex = 0;
	Vector FromLocation = ZERO_VECTOR; // The start point of the connection
	Vector ToLocation = ZERO_VECTOR; // The end point of the connection
	unsigned int ConnectionFlags = 0; // The type of connection it is
	unsigned int DefaultConnectionFlags = 0; // If this connection is being temporarily modified, what it should normally be
	unsigned int ConnectionRef = 0; // References to this connection on all defined nav meshes
	edict_t* LinkedObject = nullptr;

	bool IsValid()
	{
		return !vIsZero(FromLocation) && !vIsZero(ToLocation);
	}
} NavOffMeshConnection;

typedef struct _TEMPORARY_OBSTACLE
{
	unsigned int NavMeshIndex = 0;
	Vector Location = ZERO_VECTOR; // The location of the obstacle. This will be at the BASE of the cylinder
	float Radius = 0.0f; // How wide the cylindrical obstacle is
	float Height = 0.0f; // How tall the cylinder is
	unsigned char Area = 0; // The area to mark on the nav mesh
	unsigned int ObstacleRef = 0; // References to this obstacle on all defined nav meshes for easy removal
} NavTempObstacle;

// Pending message a bot wants to say. Allows for a delay in sending a message to simulate typing, or prevent too many messages on the same frame
typedef struct _AI_CHAT_MESSAGE
{
	char msg[64] = { '\0' }; // Message to send
	float SendTime = 0.0f; // When the bot should send this message
	bool bIsPending = false; // Represents a valid pending message
	bool bIsTeamSay = false; // Is this a team-only message?
} AIChatMessage;

// How far a bot can be from a useable object when trying to interact with it. Used also for melee attacks. We make it slightly less than actual to avoid edge cases
static const float max_ai_use_reach = 55.0f;

// Minimum time a bot can wait between attempts to use something in seconds (when not holding the use key down)
static const float min_ai_use_interval = 0.5f;

// Minimum time a bot can wait between attempts to use something in seconds (when not holding the use key down)
static const float max_ai_jump_height = 62.0f;

// Affects the bot's pathfinding choices
typedef enum
{
	MOVESTYLE_NORMAL, // Most direct route to target
	MOVESTYLE_AMBUSH, // Prefer wall climbing and vents
	MOVESTYLE_HIDE // Prefer crouched areas like vents
} BotMoveStyle;

// The list of potential task types for the bot_task structure
typedef enum
{
	TASK_NONE,
	TASK_GET_HEALTH,
	TASK_GET_AMMO,
	TASK_GET_WEAPON,
	TASK_GET_EQUIPMENT,
	TASK_BUILD,
	TASK_ATTACK,
	TASK_MOVE,
	TASK_CAP_RESNODE,
	TASK_DEFEND,
	TASK_GUARD,
	TASK_HEAL,
	TASK_WELD,
	TASK_RESUPPLY,
	TASK_EVOLVE,
	TASK_COMMAND,
	TASK_USE,
	TASK_TOUCH,
	TASK_REINFORCE_STRUCTURE,
	TASK_SECURE_HIVE,
	TASK_PLACE_MINE
} BotTaskType;

// The result of an attack check, determining whether the attack was/would be successful
typedef enum
{
	ATTACK_SUCCESS,
	ATTACK_BLOCKED,
	ATTACK_OUTOFRANGE,
	ATTACK_INVALIDTARGET,
	ATTACK_NOWEAPON
} BotAttackResult;

typedef enum
{
	BUILD_ATTEMPT_NONE = 0,
	BUILD_ATTEMPT_PENDING,
	BUILD_ATTEMPT_SUCCESS,
	BUILD_ATTEMPT_FAILED
} BotBuildAttemptStatus;

typedef enum
{
	MOVE_TASK_NONE = 0,
	MOVE_TASK_MOVE,
	MOVE_TASK_USE,
	MOVE_TASK_BREAK,
	MOVE_TASK_TOUCH,
	MOVE_TASK_PICKUP
} BotMovementTaskType;

// Dynamic map object type
typedef enum
{
	MAPOBJECT_STATIC = 0,   // Cannot be moved, permanent obstacle
	MAPOBJECT_DOOR,			// Object type that moves between two points (e.g. a door) and can block off routes
	MAPOBJECT_PLATFORM,		// Object is a moving platform which doesn't block routes when stationary
	TRIGGER_NONE,			// Not a useable trigger, e.g. multi_manager or multisource
	TRIGGER_USE,			// Trigger activated by using it
	TRIGGER_TOUCH,			// Trigger activated by touching it
	TRIGGER_SHOOT,			// Trigger activated by shooting it (damage to activate)
	TRIGGER_BREAK,			// Trigger activated by breaking it (permanently destroy)
	TRIGGER_ENV,			// Special trigger type for env_global
	TRIGGER_MULTISOURCE		// Special trigger type for multisource
} DynamicMapObjectType;

// Dynamic map object type
typedef enum 
{
	OBJECTSTATE_IDLE = 0,   // Object is idling and not going to move until triggered
	OBJECTSTATE_PREPARING,	// Object has been triggered and is getting ready to move
	OBJECTSTATE_MOVING,		// Object is on the move
	OBJECTSTATE_OPEN		// For buttons which don't move, this marks a button which has been pressed and is waiting to release for another use
} DynamicMapObjectState;

// Bot path node. A path will be several of these strung together to lead the bot to its destination
typedef struct _BOT_PATH_NODE
{
	Vector FromLocation = ZERO_VECTOR; // Location to move from
	Vector Location = ZERO_VECTOR; // Location to move to
	float requiredZ = 0.0f; // If climbing a up ladder or wall, how high should they aim to get before dismounting.
	unsigned int flag = NAV_FLAG_DISABLED; // Is this a ladder movement, wall climb, walk etc
	unsigned char area = NAV_AREA_UNWALKABLE; // Is this a crouch area, normal walking area etc
	unsigned int poly = 0; // The nav mesh poly this point resides on
	edict_t* Platform = nullptr;
} bot_path_node;


typedef struct _AI_PLAYER_MOVE_TASK
{
	BotMovementTaskType TaskType = MOVE_TASK_NONE;
	Vector TaskLocation = ZERO_VECTOR;
	edict_t* TaskTarget = nullptr;
	edict_t* TriggerToActivate = nullptr;
	bool bPathGenerated = false;
} AIPlayerMoveTask;

typedef struct _AI_STUCK_TRACKER
{
	Vector LastBotPosition = ZERO_VECTOR;
	Vector MoveDestination = ZERO_VECTOR;
	float TotalStuckTime = 0.0f; // Total time the bot has spent stuck
	bool bPathFollowFailed = false;

} AIPlayerStuckTracker;

// Contains the bot's current navigation info, such as current path
typedef struct _NAV_STATUS
{
	std::vector<bot_path_node> CurrentPath; // Bot's path nodes
	unsigned int CurrentPathPoint = 0;

	Vector TargetDestination = ZERO_VECTOR; // Desired destination
	Vector ActualMoveDestination = ZERO_VECTOR; // Actual destination on nav mesh
	Vector PathDestination = ZERO_VECTOR; // Where the path is currently headed to

	Vector LastNavMeshCheckPosition = ZERO_VECTOR;
	Vector LastNavMeshPosition = ZERO_VECTOR; // Tracks the last place the bot was on the nav mesh. Useful if accidentally straying off it
	Vector LastOpenLocation = ZERO_VECTOR; // Tracks the last place the bot had enough room to move around people. Useful if in a vent and need to back up somewhere to let another player past.

	int CurrentMoveType = MOVETYPE_NONE; // Tracks the edict's current movement type

	unsigned int CurrentPoly = 0; // Which nav mesh poly the bot is currently on

	float LastStuckCheckTime = 0.0f; // Last time the bot checked if it had successfully moved
	float TotalStuckTime = 0.0f; // Total time the bot has spent stuck
	float LastDistanceFromDestination = 0.0f; // How far from its destination was it last stuck check

	Vector StuckCheckMoveLocation = ZERO_VECTOR; // Where is the bot trying to go that we're checking if they're stuck?
	Vector UnstuckMoveLocation = ZERO_VECTOR; // If the bot is unable to find a path, blindly move here to try and fix the problem

	float LandedTime = 0.0f; // When the bot last landed after a fall/jump.
	float AirStartedTime = 0.0f; // When the bot left the ground if in the air
	float LeapAttemptedTime = 0.0f; // When the bot last attempted to leap/blink. Avoid spam that sends it flying around too fast
	bool bIsJumping = false; // Is the bot in the air from a jump? Will duck so it can duck-jump
	bool IsOnGround = true; // Is the bot currently on the ground, or on a ladder?
	bool bHasAttemptedJump = false; // Last frame, the bot tried a jump. If the bot is still on the ground, it probably tried to jump in a vent or something
	float LastFlapTime = 0.0f; // When the bot last flapped its wings (if Lerk). Prevents per-frame spam draining adrenaline

	bool bShouldWalk = false; // Should the bot walk at this point?

	BotMoveStyle PreviousMoveStyle = MOVESTYLE_NORMAL; // Previous desired move style (e.g. normal, ambush, hide). Will trigger new path calculations if this changes
	BotMoveStyle MoveStyle = MOVESTYLE_NORMAL; // Current desired move style (e.g. normal, ambush, hide). Will trigger new path calculations if this changes
	float LastPathCalcTime = 0.0f; // When the bot last calculated a path, to limit how frequently it can recalculate

	float NextForceRecalc = 0.0f; // If set, then the bot will force-recalc its current path

	NavAgentProfile NavProfile;
	bool bNavProfileChanged = false;

	AIPlayerStuckTracker StuckInfo;

	unsigned int SpecialMovementFlags = 0; // Any special movement flags required for the current path (e.g. needs to pick up an item)

	std::vector<AIPlayerMoveTask> MovementTasks;
	AIPlayerMoveTask UnstuckTask;
} nav_status;

typedef struct _BOT_CURRENT_WEAPON_T
{
	int  iId = 0;     // weapon ID
	int  iClip = 0;   // amount of ammo in the clip
	int  iClipMax = 0; // Max clip size
	int  iAmmo1 = 0;  // amount of ammo in primary reserve
	int	iAmmo1Max = 0; // Max ammo in primary reserve
	int  iAmmo2 = 0;  // amount of ammo in secondary reserve (not used in NS)
	int  iAmmo2Max = 0; // Max ammo in secondary reserve (not used in NS)
	float MinRefireTime = 0.0f; // For non-automatic weapons, how frequently (in seconds) should the bot fire. 0 for automatic weapons.
	float LastFireTime = 0.0f; // When bot last pressed the fire button. Only used if MinRefireTime > 0.
	bool bIsReloading = false; // Is this weapon currently being reloaded?
	float bReloadStartTime = 0.0f; // When this weapon began reloading
} bot_current_weapon_t;

typedef struct AI_PLAYER
{
	edict_t* Edict = nullptr; // Reference to the bot's player edict
	float			ForwardMove = 0.0f;
	float			SideMove = 0.0f;
	float			UpMove = 0.0f;
	int				Button = 0.0f;
	int				Impulse = 0.0f;
	byte			AdjustedMsec = 0;

	int				DesiredTeam = 0;

	bool			bIsPendingKill = false;
	bool bIsInactive = false;

	float LastUseTime = 0.0f;

	Vector SpawnLocation = ZERO_VECTOR; // Where did the bot spawn in and start playing?

	Vector desiredMovementDir = ZERO_VECTOR;
	Vector CurrentEyePosition = ZERO_VECTOR;
	Vector CurrentFloorPosition = ZERO_VECTOR;

	Vector CollisionHullBottomLocation = ZERO_VECTOR;
	Vector CollisionHullTopLocation = ZERO_VECTOR;

	AIWeaponType DesiredMoveWeapon = WEAPON_INVALID;
	AIWeaponType DesiredCombatWeapon = WEAPON_INVALID;

	frustum_plane_t viewFrustum[6]; // Bot's view frustum. Essentially, their "screen" for determining visibility of stuff

	nav_status BotNavInfo; // Bot's movement information, their current path, where in the path they are etc.

	float LastTeleportTime = 0.0f; // Last time the bot teleported somewhere
		
	AIChatMessage ChatMessages[5]; // Bot can have up to 5 chat messages pending

	Vector DesiredLookDirection = ZERO_VECTOR; // What view angle is the bot currently turning towards
	Vector InterpolatedLookDirection = ZERO_VECTOR; // Used to smoothly interpolate the bot's view rather than snap instantly like an aimbot
	Vector LookTargetLocation = ZERO_VECTOR; // This is the bot's current desired look target. Could be an enemy (see LookTarget), or point of interest
	Vector MoveLookLocation = ZERO_VECTOR; // If the bot has to look somewhere specific for movement (e.g. up for a ladder or wall-climb), this will override LookTargetLocation so the bot doesn't get distracted and mess the move up
	float ViewInterpolationSpeed = 0.0f; // How fast should the bot turn its view? Depends on distance to turn
	float ViewInterpStartedTime = 0.0f; // Used for interpolation

	float ViewUpdateRate = 0.2f; // How frequently the bot can react to new sightings of enemies etc.
	float LastViewUpdateTime = 0.0f; // Used to throttle view updates based on ViewUpdateRate

	Vector ViewForwardVector = ZERO_VECTOR; // Bot's current forward unit vector

	float ThinkDelta = 0.0f; // How long since this bot last ran AIPlayerThink
	float LastThinkTime = 0.0f; // When the bot last ran AIPlayerThink

	float LastServerUpdateTime = 0.0f; // When we last called RunPlayerMove

	bot_current_weapon_t current_weapon;  // one current weapon for the bot

	Vector TestLocation = ZERO_VECTOR;

} AIPlayer;


#endif
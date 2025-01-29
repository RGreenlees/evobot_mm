#pragma once

#ifndef NAV_CONSTANTS_H
#define NAV_CONSTANTS_H

#define NAV_MESH_DEFAULT 0

#define NUM_NAV_MESHES 3

#include <vector>

// Possible movement types. Defines the actions the bot needs to take to traverse this node
enum NavMovementFlag
{
	NAV_FLAG_DISABLED = 1 << 31,		// Disabled
	NAV_FLAG_WALK = 1 << 0,		// Walk
	NAV_FLAG_CROUCH = 1 << 1,		// Crouch
	NAV_FLAG_JUMP = 1 << 2,		// Jump
	NAV_FLAG_LADDER = 1 << 3,		// Ladder
	NAV_FLAG_FALL = 1 << 4,		// Fall
	NAV_FLAG_PLATFORM = 1 << 5,		// Platform
	NAV_FLAG_TELEPORT = 1 << 6,		// Teleport
	NAV_FLAG_WALLCLIMB = 1 << 7,		// Wallclimb
	NAV_FLAG_PHASEGATE1 = 1 << 8,		// Phasegate Team One
	NAV_FLAG_PHASEGATE2 = 1 << 9,		// Phasegate Team Two
	NAV_FLAG_WELD = 1 << 10,		// Weld
	NAV_FLAG_FLY = 1 << 11,		// Fly
	NAV_FLAG_LEAP = 1 << 12,		// Leap
	NAV_FLAG_ALL = -1		// All flags
};

// Nav hint types
enum NavHintType
{
	NAV_HINT_BUILD_COMMCHAIR = 1 << 0,		// Build Command Chair
	NAV_HINT_BUILD_INFPORTAL = 1 << 1,		// Build Infantry Portal
	NAV_HINT_BUILD_ARMORY = 1 << 2,		// Build Armory
	NAV_HINT_BUILD_TF = 1 << 3,		// Build Turret Factory
	NAV_HINT_BUILD_OBS = 1 << 4,		// Build Observatory
	NAV_HINT_BUILD_RESLAB = 1 << 5,		// Build Research Lab
	NAV_HINT_BUILD_TURRET = 1 << 6,		// Build Sentry Turret
	NAV_HINT_BUILD_SIEGETURRET = 1 << 7,		// Build Siege Turret
	NAV_HINT_BUILD_DEFCHAMBER = 1 << 8,		// Build Defense Chamber
	NAV_HINT_BUILD_MOVCHAMBER = 1 << 9,		// Build Movement Chamber
	NAV_HINT_BUILD_SENCHAMBER = 1 << 10,		// Build Sensory Chamber
	NAV_HINT_BUILD_OFFCHAMBER = 1 << 11,		// Build Offense Chamber
	NAV_HINT_BUILD_PHASEGATE = 1 << 12,		// Build Phase Gate
	NAV_HINT_BUILD_PROTOLAB = 1 << 13,		// Build Prototype Lab
	NAV_HINT_SIEGELOC = 1 << 14,		// Siege Location
	NAV_HINT_ANY = -1		// Any hint type
};

// Area types. Defines the cost of movement through an area and which flag to use
enum NavArea
{
	NAV_AREA_NULL = 0,		// Null area, cuts a hole in the mesh
	NAV_AREA_UNWALKABLE = 60,		// Unwalkable
	NAV_AREA_WALK = 1,		// Walk
	NAV_AREA_CROUCH = 2,		// Crouch
	NAV_AREA_OBSTRUCTED = 3,		// Obstructed
	NAV_AREA_HAZARD = 4,		// Hazard
	NAV_AREA_TELEPORT = 5,		// Teleport
	NAV_AREA_WELD = 6,		// Weld
};

// Profile indices. Use these when retrieving base agent profile information
enum NavProfileIndex
{
	NAV_PROFILE_MARINE = 0,		// Marine
	NAV_PROFILE_SKULK = 1,		// Skulk
	NAV_PROFILE_GORGE = 2,		// Gorge
	NAV_PROFILE_LERK = 3,		// Lerk
	NAV_PROFILE_FADE = 4,		// Fade
	NAV_PROFILE_ONOS = 5,		// Onos
	NAV_PROFILE_DEFAULT = 6,		// Default profile which has all capabilities except disabled flags, and 1.0 area costs for everything 
};

// Profile indices. Use these when retrieving base agent profile information
enum NavMeshIndex
{
	NAV_MESH_REGULAR = 0,		// Regular Player Navmesh
	NAV_MESH_ONOS = 1,		// Onos Navmesh
	NAV_MESH_STRUCTURE = 2,		// Structure Navmesh
};

// Agent profile definition. Holds all information an agent needs when querying the nav mesh
typedef struct _NAV_AGENT_PROFILE
{
	unsigned int NavMeshIndex = 0;
	class dtQueryFilter Filters;
	bool bFlyingProfile = false;
} NavAgentProfile;

// Declared in DTNavigation.cpp
// List of base agent profiles
extern std::vector<NavAgentProfile> BaseAgentProfiles;

// Agent profile definition. Holds all information an agent needs when querying the nav mesh
typedef struct _NAV_HINT
{
	unsigned int NavMeshIndex = 0;
	unsigned int HintTypes = 0;
	Vector Position;
} NavHint;

// Retrieve appropriate flag for area (See process() in the MeshProcess struct)
inline NavMovementFlag GetFlagForArea(NavArea Area)
{
	switch (Area)
	{
	case NAV_AREA_UNWALKABLE:
		return NAV_FLAG_DISABLED;
	case NAV_AREA_WALK:
		return NAV_FLAG_WALK;
	case NAV_AREA_CROUCH:
		return NAV_FLAG_WALK;
	case NAV_AREA_OBSTRUCTED:
		return NAV_FLAG_JUMP;
	case NAV_AREA_HAZARD:
		return NAV_FLAG_WALK;
	case NAV_AREA_TELEPORT:
		return NAV_FLAG_TELEPORT;
	case NAV_AREA_WELD:
		return NAV_FLAG_WELD;
	default:
		return NAV_FLAG_ALL;
	}
}

// Get appropriate debug colour for the area. Returns RGB as 3 unsigned chars encoded into a single unsigned int
inline void GetDebugColorForArea(NavArea Area, unsigned char& R, unsigned char& G, unsigned char& B)
{
	switch (Area)
	{
	case NAV_AREA_NULL:
		R = 128;
		G = 128;
		B = 128;
		break;
	case NAV_AREA_UNWALKABLE:
		R = 10;
		G = 10;
		B = 10;
		break;
	case NAV_AREA_WALK:
		R = 0;
		G = 192;
		B = 255;
		break;
	case NAV_AREA_CROUCH:
		R = 9;
		G = 130;
		B = 150;
		break;
	case NAV_AREA_OBSTRUCTED:
		R = 255;
		G = 64;
		B = 64;
		break;
	case NAV_AREA_HAZARD:
		R = 192;
		G = 32;
		B = 32;
		break;
	case NAV_AREA_TELEPORT:
		R = 255;
		G = 255;
		B = 255;
		break;
	case NAV_AREA_WELD:
		R = 255;
		G = 151;
		B = 69;
		break;
	default:
		R = 255;
		G = 255;
		B = 255;
		break;
	}
}

// Get appropriate debug colour for the movement flag. Returns RGB as 3 unsigned chars encoded into a single unsigned int
inline void GetDebugColorForFlag(NavMovementFlag Flag, unsigned char& R, unsigned char& G, unsigned char& B)
{
	switch (Flag)
	{
	case NAV_FLAG_DISABLED:
		R = 8;
		G = 8;
		B = 8;
		break;
	case NAV_FLAG_WALK:
		R = 0;
		G = 192;
		B = 255;
		break;
	case NAV_FLAG_CROUCH:
		R = 9;
		G = 130;
		B = 150;
		break;
	case NAV_FLAG_JUMP:
		R = 200;
		G = 200;
		B = 0;
		break;
	case NAV_FLAG_LADDER:
		R = 64;
		G = 64;
		B = 255;
		break;
	case NAV_FLAG_FALL:
		R = 255;
		G = 64;
		B = 64;
		break;
	case NAV_FLAG_PLATFORM:
		R = 255;
		G = 0;
		B = 255;
		break;
	case NAV_FLAG_TELEPORT:
		R = 255;
		G = 255;
		B = 255;
		break;
	case NAV_FLAG_WALLCLIMB:
		R = 0;
		G = 128;
		B = 0;
		break;
	case NAV_FLAG_PHASEGATE1:
		R = 203;
		G = 203;
		B = 255;
		break;
	case NAV_FLAG_PHASEGATE2:
		R = 159;
		G = 159;
		B = 255;
		break;
	case NAV_FLAG_WELD:
		R = 255;
		G = 143;
		B = 0;
		break;
	case NAV_FLAG_FLY:
		R = 63;
		G = 107;
		B = 25;
		break;
	case NAV_FLAG_LEAP:
		R = 255;
		G = 255;
		B = 120;
		break;
	default:
		R = 255;
		G = 255;
		B = 255;
		break;
	}
}

// Return name of a flag for debugging purposes
inline void GetFlagName(NavMovementFlag Flag, char* outName)
{
	if (!outName) { return; }

	switch (Flag)
	{
	case NAV_FLAG_DISABLED:
		sprintf(outName, "Disabled");
		break;
	case NAV_FLAG_WALK:
		sprintf(outName, "Walk");
		break;
	case NAV_FLAG_CROUCH:
		sprintf(outName, "Crouch");
		break;
	case NAV_FLAG_JUMP:
		sprintf(outName, "Jump");
		break;
	case NAV_FLAG_LADDER:
		sprintf(outName, "Ladder");
		break;
	case NAV_FLAG_FALL:
		sprintf(outName, "Fall");
		break;
	case NAV_FLAG_PLATFORM:
		sprintf(outName, "Platform");
		break;
	case NAV_FLAG_TELEPORT:
		sprintf(outName, "Teleport");
		break;
	case NAV_FLAG_WALLCLIMB:
		sprintf(outName, "Wallclimb");
		break;
	case NAV_FLAG_PHASEGATE1:
		sprintf(outName, "Phasegate Team One");
		break;
	case NAV_FLAG_PHASEGATE2:
		sprintf(outName, "Phasegate Team Two");
		break;
	case NAV_FLAG_WELD:
		sprintf(outName, "Weld");
		break;
	case NAV_FLAG_FLY:
		sprintf(outName, "Fly");
		break;
	case NAV_FLAG_LEAP:
		sprintf(outName, "Leap");
		break;
	default:
		sprintf(outName, "Undefined");
		break;
	}
}

// Returns true if this flag is a teleport move (i.e. not affected by doors or other obstacles)
inline bool IsFlagTeleportType(NavMovementFlag Flag)
{
	switch (Flag)
	{
	case NAV_FLAG_DISABLED:
		return false;
	case NAV_FLAG_WALK:
		return false;
	case NAV_FLAG_CROUCH:
		return false;
	case NAV_FLAG_JUMP:
		return false;
	case NAV_FLAG_LADDER:
		return false;
	case NAV_FLAG_FALL:
		return false;
	case NAV_FLAG_PLATFORM:
		return false;
	case NAV_FLAG_TELEPORT:
		return true;
	case NAV_FLAG_WALLCLIMB:
		return false;
	case NAV_FLAG_PHASEGATE1:
		return false;
	case NAV_FLAG_PHASEGATE2:
		return false;
	case NAV_FLAG_WELD:
		return false;
	case NAV_FLAG_FLY:
		return false;
	case NAV_FLAG_LEAP:
		return false;
	default:
		return false;
	}
}

// Return name of a flag for debugging purposes
inline void GetAreaName(NavArea Area, char* outName)
{
	if (!outName) { return; }

	switch (Area)
	{
	case NAV_AREA_UNWALKABLE:
		sprintf(outName, "Unwalkable");
		break;
	case NAV_AREA_WALK:
		sprintf(outName, "Walk");
		break;
	case NAV_AREA_CROUCH:
		sprintf(outName, "Crouch");
		break;
	case NAV_AREA_OBSTRUCTED:
		sprintf(outName, "Obstructed");
		break;
	case NAV_AREA_HAZARD:
		sprintf(outName, "Hazard");
		break;
	case NAV_AREA_TELEPORT:
		sprintf(outName, "Teleport");
		break;
	case NAV_AREA_WELD:
		sprintf(outName, "Weld");
		break;
	default:
		sprintf(outName, "Undefined");
		break;
	}
}

// Populate the base nav profiles. Should be called once after loading the navigation data
inline void PopulateBaseAgentProfiles()
{
	BaseAgentProfiles.clear();

	NavAgentProfile NewProfile;
	NewProfile.NavMeshIndex = 0;
	NewProfile.Filters.setIncludeFlags(1151);
	NewProfile.Filters.setExcludeFlags(NAV_FLAG_DISABLED);
	NewProfile.Filters.setAreaCost(0, 0.0);
	NewProfile.Filters.setAreaCost(1, 1.0);
	NewProfile.Filters.setAreaCost(2, 2.0);
	NewProfile.Filters.setAreaCost(3, 2.0);
	NewProfile.Filters.setAreaCost(4, 10.0);
	NewProfile.Filters.setAreaCost(5, 0.1);
	NewProfile.Filters.setAreaCost(6, 5.0);
	BaseAgentProfiles.push_back(NewProfile);

	NavAgentProfile NewProfile1;
	NewProfile1.NavMeshIndex = 0;
	NewProfile1.Filters.setIncludeFlags(4351);
	NewProfile1.Filters.setExcludeFlags(NAV_FLAG_DISABLED);
	NewProfile1.Filters.setAreaCost(0, 0.0);
	NewProfile1.Filters.setAreaCost(1, 1.0);
	NewProfile1.Filters.setAreaCost(2, 1.0);
	NewProfile1.Filters.setAreaCost(3, 2.0);
	NewProfile1.Filters.setAreaCost(4, 10.0);
	NewProfile1.Filters.setAreaCost(5, 0.1);
	NewProfile1.Filters.setAreaCost(6, 1.0);
	BaseAgentProfiles.push_back(NewProfile1);

	NavAgentProfile NewProfile2;
	NewProfile2.NavMeshIndex = 0;
	NewProfile2.Filters.setIncludeFlags(127);
	NewProfile2.Filters.setExcludeFlags(NAV_FLAG_DISABLED);
	NewProfile2.Filters.setAreaCost(0, 0.0);
	NewProfile2.Filters.setAreaCost(1, 1.0);
	NewProfile2.Filters.setAreaCost(2, 1.0);
	NewProfile2.Filters.setAreaCost(3, 2.0);
	NewProfile2.Filters.setAreaCost(4, 10.0);
	NewProfile2.Filters.setAreaCost(5, 0.1);
	NewProfile2.Filters.setAreaCost(6, 1.0);
	BaseAgentProfiles.push_back(NewProfile2);

	NavAgentProfile NewProfile3;
	NewProfile3.NavMeshIndex = 0;
	NewProfile3.Filters.setIncludeFlags(6399);
	NewProfile3.Filters.setExcludeFlags(NAV_FLAG_DISABLED);
	NewProfile3.Filters.setAreaCost(0, 0.0);
	NewProfile3.Filters.setAreaCost(1, 1.0);
	NewProfile3.Filters.setAreaCost(2, 1.0);
	NewProfile3.Filters.setAreaCost(3, 2.0);
	NewProfile3.Filters.setAreaCost(4, 1.0);
	NewProfile3.Filters.setAreaCost(5, 0.1);
	NewProfile3.Filters.setAreaCost(6, 1.0);
	BaseAgentProfiles.push_back(NewProfile3);

	NavAgentProfile NewProfile4;
	NewProfile4.NavMeshIndex = 0;
	NewProfile4.Filters.setIncludeFlags(6399);
	NewProfile4.Filters.setExcludeFlags(NAV_FLAG_DISABLED);
	NewProfile4.Filters.setAreaCost(0, 0.0);
	NewProfile4.Filters.setAreaCost(1, 1.0);
	NewProfile4.Filters.setAreaCost(2, 1.0);
	NewProfile4.Filters.setAreaCost(3, 2.0);
	NewProfile4.Filters.setAreaCost(4, 1.0);
	NewProfile4.Filters.setAreaCost(5, 0.1);
	NewProfile4.Filters.setAreaCost(6, 1.0);
	BaseAgentProfiles.push_back(NewProfile4);

	NavAgentProfile NewProfile5;
	NewProfile5.NavMeshIndex = 1;
	NewProfile5.Filters.setIncludeFlags(127);
	NewProfile5.Filters.setExcludeFlags(NAV_FLAG_DISABLED);
	NewProfile5.Filters.setAreaCost(0, 0.0);
	NewProfile5.Filters.setAreaCost(1, 1.0);
	NewProfile5.Filters.setAreaCost(2, 3.0);
	NewProfile5.Filters.setAreaCost(3, 3.0);
	NewProfile5.Filters.setAreaCost(4, 10.0);
	NewProfile5.Filters.setAreaCost(5, 0.1);
	NewProfile5.Filters.setAreaCost(6, 1.0);
	BaseAgentProfiles.push_back(NewProfile5);

	NavAgentProfile DefaultProfile;
	DefaultProfile.NavMeshIndex = 0;
	DefaultProfile.Filters.setIncludeFlags(0x7fffffff);
	DefaultProfile.Filters.setExcludeFlags(NAV_FLAG_DISABLED);
	DefaultProfile.Filters.setAreaCost(0, 1.0);
	DefaultProfile.Filters.setAreaCost(1, 1.0);
	DefaultProfile.Filters.setAreaCost(2, 1.0);
	DefaultProfile.Filters.setAreaCost(3, 1.0);
	DefaultProfile.Filters.setAreaCost(4, 1.0);
	DefaultProfile.Filters.setAreaCost(5, 1.0);
	DefaultProfile.Filters.setAreaCost(6, 1.0);
	BaseAgentProfiles.push_back(DefaultProfile);
}

// Return the appropriate base nav profile information
inline const NavAgentProfile GetBaseAgentProfile(const NavProfileIndex Index)
{
	return BaseAgentProfiles[Index];
}

#endif // NAV_CONSTANTS_H
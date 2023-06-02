//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// NS_Constants.h
// 
// Contains all NS-specific global values
//

#pragma once

#ifndef NS_CONSTANTS_H
#define NS_CONSTANTS_H

#define MARINE_TEAM 1
#define ALIEN_TEAM 2

#define IMPULSE_ALIEN_EVOLVE_SKULK 113
#define IMPULSE_ALIEN_EVOLVE_GORGE 114
#define IMPULSE_ALIEN_EVOLVE_LERK 115
#define IMPULSE_ALIEN_EVOLVE_FADE 116
#define IMPULSE_ALIEN_EVOLVE_ONOS 117

#define IMPULSE_ALIEN_UPGRADE_CARAPACE 101
#define IMPULSE_ALIEN_UPGRADE_REGENERATION 102
#define IMPULSE_ALIEN_UPGRADE_REDEMPTION 103
#define IMPULSE_ALIEN_UPGRADE_CELERITY 107
#define IMPULSE_ALIEN_UPGRADE_ADRENALINE 108
#define IMPULSE_ALIEN_UPGRADE_SILENCE 109
#define IMPULSE_ALIEN_UPGRADE_CLOAK 110
#define IMPULSE_ALIEN_UPGRADE_FOCUS 111
#define IMPULSE_ALIEN_UPGRADE_SCENTOFFEAR 112

#define IMPULSE_ALIEN_UPGRADE_ABILITY3_UNLOCK 118 // Unlock ability 3 in combat mode
#define IMPULSE_ALIEN_UPGRADE_ABILITY4_UNLOCK 126 // Unlock ability 4 in combat mode

#define IMPULSE_ALIEN_TAUNT 9
#define IMPULSE_ALIEN_REQUEST_HEAL 14

#define IMPULSE_ALIEN_BUILD_RESTOWER 90
#define IMPULSE_ALIEN_BUILD_OFFENCECHAMBER 91
#define IMPULSE_ALIEN_BUILD_DEFENCECHAMBER 92
#define IMPULSE_ALIEN_BUILD_SENSORYCHAMBER 93
#define IMPULSE_ALIEN_BUILD_MOVEMENTCHAMBER 94
#define IMPULSE_ALIEN_BUILD_HIVE 95

#define IMPULSE_MARINE_DROP_WEAPON 3
#define IMPULSE_MARINE_REQUEST_ORDER 80
#define IMPULSE_MARINE_REQUEST_HEALTH 10
#define IMPULSE_ALIEN_REQUEST_HEALTH 10
#define IMPULSE_MARINE_REQUEST_AMMO 11
#define IMPULSE_MARINE_REQUEST_WELD 14

#define IMPULSE_COMMANDER_RESEARCH_ARMOR_ONE 20
#define IMPULSE_COMMANDER_RESEARCH_ARMOR_TWO 21
#define IMPULSE_COMMANDER_RESEARCH_ARMOR_THREE 22
#define IMPULSE_COMMANDER_RESEARCH_WEAPONS_ONE 23
#define IMPULSE_COMMANDER_RESEARCH_WEAPONS_TWO 24
#define IMPULSE_COMMANDER_RESEARCH_WEAPONS_THREE 25
#define IMPULSE_COMMANDER_TURRET_FACTORY_UPGRADE 26
#define IMPULSE_COMMANDER_BUILD_CAT 27
#define IMPULSE_COMMANDER_RESEARCH_JETPACKS 28
#define IMPULSE_COMMANDER_RESEARCH_HEAVYARMOR 29
#define IMPULSE_COMMANDER_RESEARCH_DISTRESSBEACON 30
#define IMPULSE_COMMANDER_MESSAGE_CANCEL 32
#define IMPULSE_COMMANDER_RESEARCH_MOTIONTRACK 33
#define IMPULSE_COMMANDER_RESEARCH_PHASETECH 34
#define IMPULSE_COMMANDER_RESOURCE_UPGRADE 35
#define IMPULSE_COMMANDER_RESEARCH_ELECTRICAL 36
#define IMPULSE_COMMANDER_RESEARCH_GRENADES 37

#define IMPULSE_COMMANDER_BUILD_INFANTRYPORTAL 40
#define IMPULSE_COMMANDER_BUILD_RESTOWER 41
#define IMPULSE_COMMANDER_BUILD_TURRETFACTORY 43
#define IMPULSE_COMMANDER_BUILD_ARMSLAB 45
#define IMPULSE_COMMANDER_BUILD_PROTOTYPELAB 46
#define IMPULSE_COMMANDER_BUILD_ARMOURY 48
#define IMPULSE_COMMANDER_BUILD_OBSERVATORY 51
#define IMPULSE_COMMANDER_BUILD_PHASEGATE 55
#define IMPULSE_COMMANDER_BUILD_TURRET 56
#define IMPULSE_COMMANDER_BUILD_SIEGETURRET 57
#define IMPULSE_COMMANDER_BUILD_COMMCHAIR 58

#define IMPULSE_COMMANDER_MOUSECOORD 82
#define IMPULSE_COMMANDER_MOVETO 83

#define IMPULSE_COMMANDER_UPGRADE_ARMOURY 49
#define IMPULSE_COMMANDER_UPGRADE_TURRETFACTORY 26

// Nice
#define IMPULSE_COMMANDER_RECYCLEBUILDING 69


const float kResearchFuser1Base = 2.0f;

const float kSelectionNetworkConstant = 1000.0f;
const float kNormalizationNetworkFactor = 1000.0f;
const float kWorldPosNetworkConstant = 6.0f;
const float	kNumericNetworkConstant = 100.0f;

const int kOrderStatusActive = 0;
const int kOrderStatusComplete = 1;
const int kOrderStatusCancelled = 2;

const int kHiveInfoStatusUnbuilt = 0;
const int kHiveInfoStatusBuildingStage1 = 1;
const int kHiveInfoStatusBuildingStage2 = 2;
const int kHiveInfoStatusBuildingStage3 = 3;
const int kHiveInfoStatusBuildingStage4 = 4;
const int kHiveInfoStatusBuildingStage5 = 5;
const int kHiveInfoStatusBuilt = 6;
const int kHiveInfoStatusUnderAttack = 7;

const int kMarineBaseArmor = 30;
const int kMarineBaseArmorUpgrade = 60;
const int kMarineBaseHeavyArmor = 200;
const int kMarineHeavyArmorUpgrade = 90;

const int kSkulkBaseArmor = 10;
const int kSkulkArmorUpgrade = 20;
const int kGorgeBaseArmor = 70;
const int kGorgeArmorUpgrade = 50;
const int kLerkBaseArmor = 30;
const int kLerkArmorUpgrade = 30;
const int kFadeBaseArmor = 150;
const int kFadeArmorUpgrade = 100;
const int kOnosBaseArmor = 600;
const int kOnosArmorUpgrade = 350;
const int kGestateBaseArmor = 150;

const int kSpitVelocity = 1500;
const int kShootCloudVelocity = 1100;
const int kAcidRocketVelocity = 2000;

const float kBiteRange = 75.0f;
const float kBite2Range = 75.0f; // Lerk bite range

const float kHealingSprayRange = 300.0f;

const float kSwipeRange = 90.0f; // Fade swipe range

const float kClawsRange = 90.0f; // Onos gore range
const float kDevourRange = 100.0f; // Onos gore range

const float kDefensiveChamberHealRange = 400.0f;
const float kHiveHealRadius = 500.0f;

// TODO: Try and retrieve these from balance variables rather than hard-coded
#define kInfantryPortalCost 20
#define kResourceTowerCost 15
#define kTurretFactoryCost 10
#define kTurretFactoryUpgradeCost 15
#define kArmsLabCost 20
#define kPrototypeLabCost 40
#define kArmoryCost 10
#define kArmoryUpgradeCost 30
#define kObservatoryCost 15
#define kPhaseGateCost 15
#define kSentryCost 10
#define kSiegeCost 15
#define kSiegeTurretRange 1100
#define kCommandStationCost 20



#define kArmorOneResearchCost 20
#define kArmorTwoResearchCost 30
#define kArmorThreeResearchCost 40
#define kWeaponsOneResearchCost 20
#define kWeaponsTwoResearchCost 30
#define kWeaponsThreeResearchCost 40

#define kOffenseChamberCost 10
#define kDefenseChamberCost 10
#define kMovementChamberCost 10
#define kSensoryChamberCost 10
#define kHiveCost 40

#define kGorgeEvolutionCost 10
#define kLerkEvolutionCost 30
#define kFadeEvolutionCost 50
#define kOnosEvolutionCost 75

#define kBiteEnergyCost 0.05f // Skulk bite cost
#define kParasiteEnergyCost 0.30f
#define kLeapEnergyCost 0.25f
#define kDivineWindEnergyCost 0.70f

#define kSpitEnergyCost 0.12f
#define kHealingSprayEnergyCost 0.15f
#define kBileBombEnergyCost 0.22f
#define kWebEnergyCost 0.18f

#define kBite2EnergyCost 0.05f // Lerk bite cost
#define kSporesEnergyCost 0.35f
#define kUmbraEnergyCost 0.30f
#define kPrimalScreamEnergyCost 0.45f

#define kSwipeEnergyCost 0.06f // Fade swipe cost
#define kBlinkEnergyCost 0.04f
#define kMetabolizeEnergyCost 0.25f
#define kAcidRocketEnergyCost 0.10f

#define kClawsEnergyCost 0.07f // Onos gore cost
#define kDevourEnergyCost 0.20f
#define kStompEnergyCost 0.30f
#define kChargeEnergyCost 0.15f

#define kAlienEnergyRate 0.08f // How much energy is regenerated per second (max 1.0). 12.5 seconds to regen from empty
#define kAdrenalineEnergyPercentPerLevel 0.33f // How much faster energy regens with adrenaline. That's an extra 33% faster per level for 2x speed regen at level 3

#define kSporeCloudRadius 225.0f

// Game Status Constants
const int		kGameStatusReset = 0;
const int		kGameStatusResetNewMap = 1;
const int		kGameStatusEnded = 2;
const int		kGameStatusGameTime = 3;
const int		kGameStatusUnspentLevels = 4;

const int		kNumHotkeyGroups = 5;
const int		kSelectAllHotGroup = kNumHotkeyGroups + 1;

// Game mode (e.g. combat, regular, MvM, AvA etc)
enum NSGameMode
{
	GAME_MODE_NONE = 0,		// Undefined game mode
	GAME_MODE_REGULAR = 1,	// Regular NS game mode
	GAME_MODE_COMBAT = 2,	// Combat game mode
	GAME_MODE_MVM = 3,		// Marine vs Marine mode
	GAME_MODE_AVA = 4		// Alien vs Alien mode
};

// All iuser3 values to denote entity classification (e.g. player class, or building type)
typedef enum
{
	AVH_USER3_NONE = 0,
	AVH_USER3_MARINE_PLAYER, // Marine
	AVH_USER3_COMMANDER_PLAYER, // Marine Commander
	AVH_USER3_ALIEN_PLAYER1, // Skulk
	AVH_USER3_ALIEN_PLAYER2, // Gorge
	AVH_USER3_ALIEN_PLAYER3, // Lerk
	AVH_USER3_ALIEN_PLAYER4, // Fade
	AVH_USER3_ALIEN_PLAYER5, // Onos
	AVH_USER3_ALIEN_EMBRYO,  // Egg
	AVH_USER3_SPAWN_TEAMA,   // Marine spawn
	AVH_USER3_SPAWN_TEAMB,   // Alien spawn
	AVH_USER3_PARTICLE_ON,				// only valid for AvHParticleEntity: entindex as int in fuser1, template index stored in fuser2
	AVH_USER3_PARTICLE_OFF,				// only valid for AvHParticleEntity: particle system handle in fuser1
	AVH_USER3_WELD,						// float progress (0 - 100) stored in fuser1
	AVH_USER3_ALPHA,					// fuser1 indicates how much alpha this entity toggles to in commander mode, fuser2 for players
	AVH_USER3_MARINEITEM,				// Something a friendly marine can pick up
	AVH_USER3_WAYPOINT,
	AVH_USER3_HIVE,
	AVH_USER3_NOBUILD,
	AVH_USER3_USEABLE,
	AVH_USER3_AUDIO_ON,
	AVH_USER3_AUDIO_OFF,
	AVH_USER3_FUNC_RESOURCE,
	AVH_USER3_COMMANDER_STATION,
	AVH_USER3_TURRET_FACTORY,
	AVH_USER3_ARMORY,
	AVH_USER3_ADVANCED_ARMORY,
	AVH_USER3_ARMSLAB,
	AVH_USER3_PROTOTYPE_LAB,
	AVH_USER3_OBSERVATORY,
	AVH_USER3_CHEMLAB,
	AVH_USER3_MEDLAB,
	AVH_USER3_NUKEPLANT,
	AVH_USER3_TURRET,
	AVH_USER3_SIEGETURRET,
	AVH_USER3_RESTOWER,
	AVH_USER3_PLACEHOLDER,
	AVH_USER3_INFANTRYPORTAL,
	AVH_USER3_NUKE,
	AVH_USER3_BREAKABLE,
	AVH_USER3_UMBRA,
	AVH_USER3_PHASEGATE,
	AVH_USER3_DEFENSE_CHAMBER,
	AVH_USER3_MOVEMENT_CHAMBER,
	AVH_USER3_OFFENSE_CHAMBER,
	AVH_USER3_SENSORY_CHAMBER,
	AVH_USER3_ALIENRESTOWER,
	AVH_USER3_HEAVY,
	AVH_USER3_JETPACK,
	AVH_USER3_ADVANCED_TURRET_FACTORY,
	AVH_USER3_SPAWN_READYROOM,
	AVH_USER3_CLIENT_COMMAND,
	AVH_USER3_FUNC_ILLUSIONARY,
	AVH_USER3_MENU_BUILD,
	AVH_USER3_MENU_BUILD_ADVANCED,
	AVH_USER3_MENU_ASSIST,
	AVH_USER3_MENU_EQUIP,
	AVH_USER3_MINE,
	AVH_USER3_UNKNOWN,
	AVH_USER3_MAX
} AvHUser3;

// Type of order a marine has received. I think the only ones actually used are ORDERTYPEL_MOVE and ORDERTYPET_BUILD. Might be wrong though
typedef enum
{
	ORDERTYPE_UNDEFINED = 0,
	ORDERTYPEL_DEFAULT,
	ORDERTYPEL_MOVE,

	ORDERTYPET_ATTACK,
	ORDERTYPET_BUILD,
	ORDERTYPET_GUARD,
	ORDERTYPET_WELD,
	ORDERTYPET_GET,

	ORDERTYPEG_HOLD_POSITION,
	ORDERTYPEG_CODE_DEPLOY_MINES,
	ORDERTYPEG_CODE_GREEN,
	ORDERTYPEG_CODE_YELLOW,
	ORDERTYPEG_CODE_RED,

	ORDERTYPE_MAX
}
AvHOrderType;

// All iuser4 masks that can be applied to a player to denote what upgrades or status effects they have applied
typedef enum
{
	MASK_NONE = 0x00000000,
	MASK_VIS_SIGHTED = 0x00000001,	// This entity can be seen by at least one member of the opposing team (shows on the minimap for the enemy). Commanders can never be seen.
	MASK_VIS_DETECTED = 0x00000002,	// This entity has been detected by the other team but isn't currently seen (i.e. motion tracking). Shows up as a dot on the minimap
	MASK_BUILDABLE = 0x00000004,	// This entity is buildable
	MASK_UPGRADE_1 = 0x00000008,	// Marine weapons 1, armor, marine basebuildable slot #0
	MASK_UPGRADE_2 = 0x00000010,	// Marine weapons 2, regen, marine basebuildable slot #1
	MASK_UPGRADE_3 = 0x00000020,	// Marine weapons 3, redemption, marine basebuildable slot #2
	MASK_UPGRADE_4 = 0x00000040,	// Marine armor 1, speed, marine basebuildable slot #3
	MASK_UPGRADE_5 = 0x00000080,	// Marine armor 2, adrenaline, marine basebuildable slot #4
	MASK_UPGRADE_6 = 0x00000100,	// Marine armor 3, silence, marine basebuildable slot #5
	MASK_UPGRADE_7 = 0x00000200,	// Marine jetpacks, Cloaking, marine basebuildable slot #6
	MASK_UPGRADE_8 = 0x00000400,	// Pheromone, motion-tracking, marine basebuildable slot #7
	MASK_UPGRADE_9 = 0x00000800,	// Scent of fear, exoskeleton
	MASK_UPGRADE_10 = 0x00001000,	// Defensive level 2, power armor
	MASK_UPGRADE_11 = 0x00002000,	// Defensive level 3, electrical defense
	MASK_UPGRADE_12 = 0x00004000,	// Movement level 2, 
	MASK_UPGRADE_13 = 0x00008000,	// Movement level 3, marine heavy armor
	MASK_UPGRADE_14 = 0x00010000,	// Sensory level 2
	MASK_UPGRADE_15 = 0x00020000,	// Sensory level 3
	MASK_ALIEN_MOVEMENT = 0x00040000,	// Onos is charging
	MASK_WALLSTICKING = 0x00080000,	// Flag for wall-sticking
	MASK_BUFFED = 0x00100000,	// Alien is in range of active primal scream, or marine is under effects of catalyst
	MASK_UMBRA = 0x00200000,	// Alien is being protected by umbra, takes less damage
	MASK_DIGESTING = 0x00400000,	// When set on a visible player, player is digesting.  When set on invisible player, player is being digested
	MASK_RECYCLING = 0x00800000,
	MASK_TOPDOWN = 0x01000000,
	MASK_PLAYER_STUNNED = 0x02000000,	// Player has been stunned by stomp
	MASK_ENSNARED = 0x04000000,			// Player caught in gorge web
	MASK_ALIEN_EMBRYO = 0x08000000,
	MASK_SELECTABLE = 0x10000000,
	MASK_PARASITED = 0x20000000,	// Marine or structure is visible to all aliens
	MASK_SENSORY_NEARBY = 0x40000000	// Player is cloaked by a sensory chamber
} AvHUpgradeMask;

// All iuser4 masks that can be applied to a player to denote what upgrades or status effects they have applied
typedef enum
{
	COMBAT_MARINE_UPGRADE_NONE = 0,
	COMBAT_MARINE_UPGRADE_CATALYST = 1 << 0,
	COMBAT_MARINE_UPGRADE_RESUPPLY = 1 << 1,
	COMBAT_MARINE_UPGRADE_SCAN = 1 << 2,
	COMBAT_MARINE_UPGRADE_MOTIONTRACKING = 1 << 3,
	COMBAT_MARINE_UPGRADE_MINES = 1 << 4,
	COMBAT_MARINE_UPGRADE_WELDER = 1 << 5,
	COMBAT_MARINE_UPGRADE_GRENADE = 1 << 6,
	COMBAT_MARINE_UPGRADE_ARMOUR1 = 1 << 7,
	COMBAT_MARINE_UPGRADE_ARMOUR2 = 1 << 8,
	COMBAT_MARINE_UPGRADE_ARMOUR3 = 1 << 9,
	COMBAT_MARINE_UPGRADE_JETPACK = 1 << 10,
	COMBAT_MARINE_UPGRADE_HEAVYARMOUR = 1 << 11,
	COMBAT_MARINE_UPGRADE_DAMAGE1 = 1 << 12,
	COMBAT_MARINE_UPGRADE_DAMAGE2 = 1 << 13,
	COMBAT_MARINE_UPGRADE_DAMAGE3 = 1 << 14,
	COMBAT_MARINE_UPGRADE_SHOTGUN = 1 << 15,
	COMBAT_MARINE_UPGRADE_HMG = 1 << 16,
	COMBAT_MARINE_UPGRADE_GRENADELAUNCHER = 1 << 17
} CombatModeMarineUpgrade;

// All iuser4 masks that can be applied to a player to denote what upgrades or status effects they have applied
typedef enum
{
	COMBAT_ALIEN_UPGRADE_NONE = 0,
	COMBAT_ALIEN_UPGRADE_ABILITY3 = 1 << 0,
	COMBAT_ALIEN_UPGRADE_ABILITY4 = 1 << 1,
	COMBAT_ALIEN_UPGRADE_SILENCE = 1 << 2,
	COMBAT_ALIEN_UPGRADE_CELERITY = 1 << 3,
	COMBAT_ALIEN_UPGRADE_ADRENALINE = 1 << 4,
	COMBAT_ALIEN_UPGRADE_SCENTOFFEAR = 1 << 5,
	COMBAT_ALIEN_UPGRADE_FOCUS = 1 << 6,
	COMBAT_ALIEN_UPGRADE_CLOAKING = 1 << 7,
	COMBAT_ALIEN_UPGRADE_CARAPACE = 1 << 8,
	COMBAT_ALIEN_UPGRADE_REDEMPTION = 1 << 9,
	COMBAT_ALIEN_UPGRADE_REGENERATION = 1 << 10,
	COMBAT_ALIEN_UPGRADE_GORGE = 1 << 11,
	COMBAT_ALIEN_UPGRADE_LERK = 1 << 12,
	COMBAT_ALIEN_UPGRADE_FADE = 1 << 13,
	COMBAT_ALIEN_UPGRADE_ONOS = 1 << 14,
} CombatModeAlienUpgrade;

// Used in network messages to work out what information about a hive is being sent (see BotClient_NS_AlienInfo in bot_client.cpp)
enum AlienInfo_ChangeFlags
{
	NO_CHANGE = 0,
	COORDS_CHANGED = 1,
	STATUS_CHANGED = 2,
	HEALTH_CHANGED = 4
};

// All tech statuses that can be assigned to a hive
typedef enum
{
	HIVE_TECH_NONE = 0, // Hive doesn't have any tech assigned to it yet (no chambers built for it)
	HIVE_TECH_DEFENCE = 1,
	HIVE_TECH_SENSORY = 2,
	HIVE_TECH_MOVEMENT = 3
} HiveTechStatus;


// Used in bot_client to determine if ammo change notifications relate to the currently-held weapon
typedef enum
{
	WEAPON_ON_TARGET = 0x01,
	WEAPON_IS_CURRENT = 0x02,
	WEAPON_IS_ENABLED = 0x04
} CurWeaponStateFlags;

// Alien traits
typedef enum
{
	TRAIT_DEFENSIVE,
	TRAIT_MOVEMENT,
	TRAIT_SENSORY
} AlienTraitCategory;

// Research commander can perform. Number relates to the impulse the commander sends to initiate research (building must be selected first)
typedef enum
{
	RESEARCH_NONE = 0,

	RESEARCH_ARMSLAB_ARMOUR1 = 20,
	RESEARCH_ARMSLAB_ARMOUR2 = 21,
	RESEARCH_ARMSLAB_ARMOUR3 = 22,
	RESEARCH_ARMSLAB_WEAPONS1 = 23,
	RESEARCH_ARMSLAB_WEAPONS2 = 24,
	RESEARCH_ARMSLAB_WEAPONS3 = 25,
	RESEARCH_ARMSLAB_CATALYSTS = 47,

	RESEARCH_PROTOTYPELAB_JETPACKS = 28,
	RESEARCH_PROTOTYPELAB_HEAVYARMOUR = 29,

	RESEARCH_OBSERVATORY_DISTRESSBEACON = 30,
	RESEARCH_OBSERVATORY_MOTIONTRACKING = 33,
	RESEARCH_OBSERVATORY_PHASETECH = 34,
	RESEARCH_ELECTRICAL = 36,

	RESEARCH_OBSERVATORY_SCAN = 53,

	RESEARCH_ARMOURY_GRENADES = 37


} NSResearch;

// All structure types
typedef enum
{
	STRUCTURE_NONE = 0,

	STRUCTURE_MARINE_COMMCHAIR,
	STRUCTURE_MARINE_RESTOWER,
	STRUCTURE_MARINE_INFANTRYPORTAL,
	STRUCTURE_MARINE_ARMOURY,
	STRUCTURE_MARINE_ADVARMOURY,
	STRUCTURE_MARINE_ANYARMOURY, // Can be armoury or advanced armoury (see UTIL_StructureTypesMatch())
	STRUCTURE_MARINE_TURRETFACTORY,
	STRUCTURE_MARINE_ADVTURRETFACTORY,
	STRUCTURE_MARINE_ANYTURRETFACTORY, // Can be turret factory or advanced turret factory (see UTIL_StructureTypesMatch())
	STRUCTURE_MARINE_TURRET,
	STRUCTURE_MARINE_SIEGETURRET,
	STRUCTURE_MARINE_ANYTURRET, // Can be turret or siege turret (see UTIL_StructureTypesMatch())
	STRUCTURE_MARINE_ARMSLAB,
	STRUCTURE_MARINE_PROTOTYPELAB,
	STRUCTURE_MARINE_OBSERVATORY,
	STRUCTURE_MARINE_PHASEGATE,

	STRUCTURE_ALIEN_HIVE,
	STRUCTURE_ALIEN_RESTOWER,
	STRUCTURE_ALIEN_DEFENCECHAMBER,
	STRUCTURE_ALIEN_SENSORYCHAMBER,
	STRUCTURE_ALIEN_MOVEMENTCHAMBER,
	STRUCTURE_ALIEN_OFFENCECHAMBER,

	STRUCTURE_ANY_MARINE_STRUCTURE,
	STRUCTURE_ANY_ALIEN_STRUCTURE

} NSStructureType;


// Items that the commander can place for marines to pick up. Also, weapons dropped by players
typedef enum
{
	ITEM_NONE = 0,

	ITEM_MARINE_RESUPPLY = 31, // For combat mode
	ITEM_MARINE_HEAVYARMOUR = 38,
	ITEM_MARINE_JETPACK = 39,
	ITEM_MARINE_CATALYSTS = 47,
	ITEM_MARINE_SCAN = 53,
	ITEM_MARINE_HEALTHPACK = 59,
	ITEM_MARINE_AMMO = 60,
	ITEM_MARINE_MINES = 61,
	ITEM_MARINE_WELDER = 62,
	ITEM_MARINE_SHOTGUN = 64,
	ITEM_MARINE_HMG = 65,
	ITEM_MARINE_GRENADELAUNCHER = 66
	

} NSDeployableItem;

// All player classes
typedef enum
{
	CLASS_NONE,
	CLASS_MARINE,
	CLASS_MARINE_COMMANDER,
	CLASS_EGG,
	CLASS_SKULK,
	CLASS_GORGE,
	CLASS_LERK,
	CLASS_FADE,
	CLASS_ONOS
} NSPlayerClass;

// These represent alerts the player can receive. These usually represent sounds and HUD pop-ups (e.g. "Soldier is under attack", "
typedef enum
{
	HUD_SOUND_INVALID = 0,
	HUD_SOUND_POINTS_SPENT,
	HUD_SOUND_COUNTDOWN,
	HUD_SOUND_SELECT,
	HUD_SOUND_SQUAD1,
	HUD_SOUND_SQUAD2,
	HUD_SOUND_SQUAD3,
	HUD_SOUND_SQUAD4,
	HUD_SOUND_SQUAD5,
	HUD_SOUND_PLACE_BUILDING,
	HUD_SOUND_MARINE_RESEARCHCOMPLETE,
	HUD_SOUND_MARINE_SOLDIER_UNDER_ATTACK,
	HUD_SOUND_MARINE_BASE_UNDER_ATTACK,
	HUD_SOUND_MARINE_UPGRADE_COMPLETE,
	HUD_SOUND_MARINE_MORE,
	HUD_SOUND_MARINE_SOLDIERLOST,
	HUD_SOUND_MARINE_CCONLINE,
	HUD_SOUND_MARINE_CCUNDERATTACK,
	HUD_SOUND_MARINE_COMMANDER_EJECTED,
	HUD_SOUND_MARINE_RESOURCES_LOW,
	HUD_SOUND_MARINE_NEEDS_AMMO,
	HUD_SOUND_MARINE_NEEDS_HEALTH,
	HUD_SOUND_MARINE_NEEDS_ORDER,
	HUD_SOUND_MARINE_POINTS_RECEIVED,
	HUD_SOUND_MARINE_SOLDIER_LOST,
	HUD_SOUND_MARINE_SENTRYFIRING,
	HUD_SOUND_MARINE_SENTRYDAMAGED,
	HUD_SOUND_MARINE_GIVEORDERS,
	HUD_SOUND_MARINE_NEEDPORTAL,
	HUD_SOUND_MARINE_GOTOALERT,
	HUD_SOUND_MARINE_COMMANDERIDLE,
	HUD_SOUND_MARINE_ARMORYUPGRADING,

	HUD_SOUND_ALIEN_ENEMY_APPROACHES, // "The enemy approaches"
	HUD_SOUND_ALIEN_GAMEOVERMAN, // Meme one, don't think this is in use...
	HUD_SOUND_ALIEN_HIVE_ATTACK, // "Our hive is under attack"
	HUD_SOUND_ALIEN_HIVE_COMPLETE, // "Our hive is complete"
	HUD_SOUND_ALIEN_HIVE_DYING, // "Our hive is dying"
	HUD_SOUND_ALIEN_LIFEFORM_ATTACK, // "Life form under attack"
	HUD_SOUND_ALIEN_RESOURCES_LOW,
	HUD_SOUND_ALIEN_MESS, // Meme one: "this place is a mess"
	HUD_SOUND_ALIEN_MORE,
	HUD_SOUND_ALIEN_NEED_BETTER,
	HUD_SOUND_ALIEN_NEED_BUILDERS, // "We need builders"
	HUD_SOUND_ALIEN_NEW_TRAIT, // "New trait available"
	HUD_SOUND_ALIEN_NOW_DONCE, // This is a meme one that plays when aliens have 3 hives and 3 of each chamber. "Now, we dance"
	HUD_SOUND_ALIEN_POINTS_RECEIVED, // Plays a kind of gloop noise when using the givepoints command
	HUD_SOUND_ALIEN_RESOURCES_ATTACK, // "Resource tower is under attack
	HUD_SOUND_ALIEN_STRUCTURE_ATTACK, // "Structure is under attack"
	HUD_SOUND_ALIEN_UPGRADELOST, // Plays a kind of pained moan

	HUD_SOUND_ORDER_MOVE,
	HUD_SOUND_ORDER_ATTACK,
	HUD_SOUND_ORDER_BUILD,
	HUD_SOUND_ORDER_WELD,
	HUD_SOUND_ORDER_GUARD,
	HUD_SOUND_ORDER_GET,
	HUD_SOUND_ORDER_COMPLETE,

	HUD_SOUND_GAMESTART, // "Let's move out" / "Kill the intruders"
	HUD_SOUND_YOU_WIN, // Victory music sting
	HUD_SOUND_YOU_LOSE, // Defeat music sting
	HUD_SOUND_TOOLTIP, // Ping when a tooltip pops up on screen
	// : bug 0000767
	HUD_SOUND_PLAYERJOIN, // Plays a gloopy noise

	HUD_SOUND_MAX = 61
} PlayerAlertType;


// NS weapon types. Each number refers to the GoldSrc weapon index
typedef enum
{
	WEAPON_NONE = 0,
	WEAPON_LERK_SPIKE = 4, // I think this is an early NS weapon, replaced by primal scream

	// Marine Weapons

	WEAPON_MARINE_KNIFE = 13,
	WEAPON_MARINE_PISTOL = 14,
	WEAPON_MARINE_MG = 15,
	WEAPON_MARINE_SHOTGUN = 16,
	WEAPON_MARINE_HMG = 17,
	WEAPON_MARINE_WELDER = 18,
	WEAPON_MARINE_MINES = 19,
	WEAPON_MARINE_GL = 20,
	WEAPON_MARINE_GRENADE = 28,

	// Alien Abilities

	WEAPON_SKULK_BITE = 5,
	WEAPON_SKULK_PARASITE = 10,
	WEAPON_SKULK_LEAP = 21,
	WEAPON_SKULK_XENOCIDE = 12,

	WEAPON_GORGE_SPIT = 2,
	WEAPON_GORGE_HEALINGSPRAY = 27,
	WEAPON_GORGE_BILEBOMB = 25,
	WEAPON_GORGE_WEB = 8,

	WEAPON_LERK_BITE = 6,
	WEAPON_LERK_SPORES = 3,
	WEAPON_LERK_UMBRA = 23,
	WEAPON_LERK_PRIMALSCREAM = 24,

	WEAPON_FADE_SWIPE = 7,
	WEAPON_FADE_BLINK = 11,
	WEAPON_FADE_METABOLIZE = 9,
	WEAPON_FADE_ACIDROCKET = 26,

	WEAPON_ONOS_GORE = 1,
	WEAPON_ONOS_DEVOUR = 30,
	WEAPON_ONOS_STOMP = 29,
	WEAPON_ONOS_CHARGE = 22,

	WEAPON_MAX = 31
}
NSWeapon;

// Hives can either be unbuilt ("ghost" hive), in progress or fully built (active)
typedef enum
{
	HIVE_STATUS_UNBUILT = 0,
	HIVE_STATUS_BUILDING = 1,
	HIVE_STATUS_BUILT = 2
} HiveStatusType;



#define PLAYMODE_UNDEFINED 0
#define PLAYMODE_READYROOM 1
#define PLAYMODE_PLAYING 2
#define PLAYMODE_AWAITINGREINFORCEMENT 3 // Player is waiting to respawn
#define	PLAYMODE_REINFORCING 4			// Player is next in line to respawn and is in the process of coming back into the game
#define PLAYMODE_OBSERVER 5
#define PLAYMODE_REINFORCINGCOMPLETE 6	// Combat only: 'press fire to respawn'


#endif
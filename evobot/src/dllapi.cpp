// vi: set ts=4 sw=4 :
// vim: set tw=75 :

/*
 * Copyright (c) 2001-2006 Will Day <willday@hpgx.net>
 *
 *    This file is part of Metamod.
 *
 *    Metamod is free software; you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the
 *    Free Software Foundation; either version 2 of the License, or (at
 *    your option) any later version.
 *
 *    Metamod is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Metamod; if not, write to the Free Software Foundation,
 *    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    In addition, as a special exception, the author gives permission to
 *    link the code of this program with the Half-Life Game Engine ("HL
 *    Engine") and Modified Game Libraries ("MODs") developed by Valve,
 *    L.L.C ("Valve").  You must obey the GNU General Public License in all
 *    respects for all of the code used other than the HL Engine and MODs
 *    from Valve.  If you modify this file, you may extend this exception
 *    to your version of the file, but you are not obligated to do so.  If
 *    you do not wish to do so, delete this exception statement from your
 *    version.
 *
 */

#include <time.h>
#include <extdll.h>

#include <dllapi.h>
#include <meta_api.h>

#include "bot_config.h"
#include "game_state.h"
#include "bot_navigation.h"
#include "bot_util.h"
#include "bot_tactical.h"
#include "general_util.h"
#include "player_util.h"
#include "bot_task.h"
#include "bot_bsp.h"
#include "bot_commander.h"
#include "bot_weapons.h"

extern int m_spriteTexture;

extern edict_t* clients[MAX_CLIENTS];
extern int NumClients;

extern bot_t bots[MAX_CLIENTS];

extern edict_t* listenserver_edict;

extern bool bGameHasStarted;

float last_think_time;

extern float last_bot_count_check_time;


void ClientCommand(edict_t* pEntity)
{
	const char* pcmd = CMD_ARGV(0);
	const char* arg1 = CMD_ARGV(1);
	const char* arg2 = CMD_ARGV(2);
	const char* arg3 = CMD_ARGV(3);
	const char* arg4 = CMD_ARGV(4);
	const char* arg5 = CMD_ARGV(5);

	// commands that anyone on your team is allowed to use
	if (FStrEq(pcmd, "say") || FStrEq(pcmd, "say_team"))
	{
		// Don't allow aliens to request stuff from the commander...
		if (!IsPlayerMarine(pEntity)) { RETURN_META(MRES_IGNORED); }

		if (FStrEq(arg1, "ammo"))
		{
			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && IsPlayerCommander(bots[i].pEdict))
				{
					CommanderReceiveAmmoRequest(&bots[i], pEntity);
					RETURN_META(MRES_IGNORED);
				}
			}

			RETURN_META(MRES_IGNORED);
		}

		if (FStrEq(arg1, "med") || FStrEq(arg1, "heal") || FStrEq(arg1, "medpack") || FStrEq(arg1, "health"))
		{
			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && IsPlayerCommander(bots[i].pEdict))
				{
					CommanderReceiveHealthRequest(&bots[i], pEntity);
					RETURN_META(MRES_IGNORED);
				}
			}

			RETURN_META(MRES_IGNORED);
		}

		if (FStrEq(arg1, "welder"))
		{
			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && IsPlayerCommander(bots[i].pEdict))
				{
					CommanderReceiveWeaponRequest(&bots[i], pEntity, DEPLOYABLE_ITEM_MARINE_WELDER);
					RETURN_META(MRES_IGNORED);
				}
			}

			RETURN_META(MRES_IGNORED);
		}

		if (FStrEq(arg1, "shotgun") || FStrEq(arg1, "sg") || FStrEq(arg1, "shotty"))
		{
			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && IsPlayerCommander(bots[i].pEdict))
				{
					CommanderReceiveWeaponRequest(&bots[i], pEntity, DEPLOYABLE_ITEM_MARINE_SHOTGUN);
					RETURN_META(MRES_IGNORED);
				}
			}

			RETURN_META(MRES_IGNORED);
		}

		if (FStrEq(arg1, "mines") || FStrEq(arg1, "mine"))
		{
			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && IsPlayerCommander(bots[i].pEdict))
				{
					CommanderReceiveWeaponRequest(&bots[i], pEntity, DEPLOYABLE_ITEM_MARINE_MINES);
					RETURN_META(MRES_IGNORED);
				}
			}

			RETURN_META(MRES_IGNORED);
		}

		if (FStrEq(arg1, "gl") || FStrEq(arg1, "GL"))
		{
			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && IsPlayerCommander(bots[i].pEdict))
				{
					CommanderReceiveWeaponRequest(&bots[i], pEntity, DEPLOYABLE_ITEM_MARINE_GRENADELAUNCHER);
					RETURN_META(MRES_IGNORED);
				}
			}

			RETURN_META(MRES_IGNORED);
		}

		if (FStrEq(arg1, "hmg") || FStrEq(arg1, "HMG"))
		{
			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && IsPlayerCommander(bots[i].pEdict))
				{
					CommanderReceiveWeaponRequest(&bots[i], pEntity, DEPLOYABLE_ITEM_MARINE_HMG);
					RETURN_META(MRES_IGNORED);
				}
			}

			RETURN_META(MRES_IGNORED);
		}

		if (FStrEq(arg1, "pg") || FStrEq(arg1, "phase") || FStrEq(arg1, "gate") || FStrEq(arg1, "phasegate"))
		{
			RETURN_META(MRES_IGNORED);
		}

		if (FStrEq(arg1, "catalyst") || FStrEq(arg1, "catpack") || FStrEq(arg1, "cat") || FStrEq(arg1, "cats"))
		{
			for (int i = 0; i < 32; i++)
			{
				if (bots[i].is_used && IsPlayerCommander(bots[i].pEdict))
				{
					CommanderReceiveCatalystRequest(&bots[i], pEntity);
					RETURN_META(MRES_IGNORED);
				}
			}

			RETURN_META(MRES_IGNORED);
		}
	}

	// only allow custom commands if deathmatch mode and NOT dedicated server and
	// client sending command is the listen server client...

	if (!gpGlobals->deathmatch || GAME_IsDedicatedServer() || pEntity != listenserver_edict)
	{
		RETURN_META(MRES_IGNORED);
	}

	if (FStrEq(pcmd, "drawobstacles"))
	{
		UTIL_DrawTemporaryObstacles();

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "teleportbot"))
	{
		Vector TraceStart = GetPlayerEyePosition(pEntity); // origin + pev->view_ofs
		Vector LookDir = UTIL_GetForwardVector(pEntity->v.v_angle); // Converts view angles to normalized unit vector

		Vector TraceEnd = TraceStart + (LookDir * 1000.0f);

		TraceResult Hit;

		UTIL_TraceHull(TraceStart, TraceEnd, ignore_monsters, head_hull, pEntity->v.pContainingEntity, &Hit);

		if (Hit.flFraction < 1.0f)
		{
			for (int i = 0; i < MAX_CLIENTS; i++)
			{
				if (bots[i].is_used && !FNullEnt(bots[i].pEdict))
				{
					bots[i].pEdict->v.origin = Hit.vecEndPos + GetPlayerOriginOffsetFromFloor(bots[i].pEdict, false) + Vector(0.0f, 0.0f, 5.0f);
				}
			}
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "grenadetest"))
	{
		edict_t* ResTower = UTIL_GetNearestStructureOfTypeInLocation(STRUCTURE_ALIEN_OFFENCECHAMBER, pEntity->v.origin, UTIL_MetresToGoldSrcUnits(500.0f), true, false);

		if (!FNullEnt(ResTower))
		{
			Vector GrenLoc = UTIL_GetGrenadeThrowTarget(pEntity, ResTower->v.origin, UTIL_MetresToGoldSrcUnits(3.0f), true);

			if (GrenLoc != ZERO_VECTOR)
			{
				UTIL_DrawLine(pEntity, pEntity->v.origin, GrenLoc, 10.0f);
			}
			else
			{
				UTIL_SayText("No Grenade Location\n", pEntity);
			}
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "testambush"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used && !FNullEnt(bots[i].pEdict))
			{
				Vector AmbushLoc = UTIL_GetAmbushPositionForTarget2(&bots[i], pEntity);

				if (AmbushLoc != ZERO_VECTOR)
				{
					UTIL_DrawLine(pEntity, pEntity->v.origin, AmbushLoc, 10.0f, 255, 0, 0);
				}
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "lookatme"))
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (bots[i].is_used && !FNullEnt(bots[i].pEdict))
			{
				BotDirectLookAt(&bots[i], UTIL_GetCentreOfEntity(pEntity));
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "testweldables"))
	{
		edict_t* currWeldable = NULL;
		while (((currWeldable = UTIL_FindEntityByClassname(currWeldable, "avhweldable")) != NULL) && (!FNullEnt(currWeldable)))
		{
			if (currWeldable->v.solid == SOLID_BSP && vDist2DSq(UTIL_GetCentreOfEntity(currWeldable), pEntity->v.origin) <= sqrf(100.0f))
			{
				UTIL_DrawBox(pEntity, currWeldable->v.absmin, currWeldable->v.absmax, 10.0f);
			}
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "traceentity"))
	{

		Vector TraceStart = GetPlayerEyePosition(pEntity); // origin + pev->view_ofs
		Vector LookDir = UTIL_GetForwardVector(pEntity->v.v_angle); // Converts view angles to normalized unit vector

		Vector TraceEnd = TraceStart + (LookDir * 1000.0f);

		TraceResult Hit;

		UTIL_TraceLine(TraceStart, TraceEnd, ignore_monsters, dont_ignore_glass, pEntity, &Hit);

		//UTIL_TraceLine(TraceStart, TraceEnd, dont_ignore_monsters, dont_ignore_glass, pEntity, &Hit);

		if (!FNullEnt(Hit.pHit))
		{
			char buf[128];
			sprintf(buf, "Hit Entity: %s (name: %s)\n", STRING(Hit.pHit->v.classname), STRING(Hit.pHit->v.targetname));
			UTIL_SayText(buf, pEntity);

			NSStructureType StructType = UTIL_IUSER3ToStructureType(Hit.pHit->v.iuser3);

			sprintf(buf, "Distance: %f\n", (1000.0f * Hit.flFraction));
			UTIL_SayText(buf, pEntity);

			sprintf(buf, "Height: %f\n", (Hit.pHit->v.size.z));
			UTIL_SayText(buf, pEntity);

		}
		else
		{
			UTIL_SayText("Hit nothing", pEntity);
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "testevolveloc"))
	{
		Vector EvolveLocation = ZERO_VECTOR;

		const hive_definition* NearestHive = UTIL_GetNearestHiveOfStatus(pEntity->v.origin, HIVE_STATUS_BUILT);

		if (NearestHive)
		{
			EvolveLocation = FindClosestNavigablePointToDestination(BUILDING_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), NearestHive->FloorLocation, UTIL_MetresToGoldSrcUnits(20.0f));

			if (EvolveLocation == ZERO_VECTOR)
			{
				EvolveLocation = FindClosestNavigablePointToDestination(MARINE_REGULAR_NAV_PROFILE, UTIL_GetCommChairLocation(), NearestHive->FloorLocation, UTIL_MetresToGoldSrcUnits(20.0f));
			}
		}

		if (EvolveLocation == ZERO_VECTOR)
		{
			EvolveLocation = pEntity->v.origin;
		}

		Vector FinalEvolveLocation = UTIL_GetRandomPointOnNavmeshInRadius(BUILDING_REGULAR_NAV_PROFILE, EvolveLocation, UTIL_MetresToGoldSrcUnits(5.0f));

		EvolveLocation = ((FinalEvolveLocation != ZERO_VECTOR) ? FinalEvolveLocation : EvolveLocation);

		if (EvolveLocation != ZERO_VECTOR)
		{
			UTIL_DrawLine(pEntity, pEntity->v.origin, EvolveLocation, 10.0f, 255, 255, 0);
		}

		RETURN_META(MRES_SUPERCEDE);
	}
	

	if (FStrEq(pcmd, "showconnections"))
	{
		if (NavmeshLoaded())
		{
			DEBUG_DrawOffMeshConnections();
		}

		RETURN_META(MRES_SUPERCEDE);
	}
	
	if (FStrEq(pcmd, "trackevolutions"))
	{

		char buf[128];

		if (GAME_IsAnyPlayerEvolvingToClass(CLASS_LERK))
		{
			sprintf(buf, "True (%4.2f)\n", GAME_GetLastLerkSeenTime());
		}
		else
		{
			sprintf(buf, "False (%4.2f)\n", GAME_GetLastLerkSeenTime());
		}

		UTIL_SayText(buf, pEntity);

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "multimanagers"))
	{
		edict_t* currMM = NULL;
		while (((currMM = UTIL_FindEntityByClassname(currMM, "multi_manager")) != NULL) && (!FNullEnt(currMM)))
		{
			UTIL_SayText("x\n", pEntity);
		}

		RETURN_META(MRES_SUPERCEDE);
	}


	if (FStrEq(pcmd, "tracedoor"))
	{
		
		Vector TraceStart = GetPlayerEyePosition(pEntity); // origin + pev->view_ofs
		Vector LookDir = UTIL_GetForwardVector(pEntity->v.v_angle); // Converts view angles to normalized unit vector

		Vector TraceEnd = TraceStart + (LookDir * 1000.0f);

		edict_t* TracedEntity = UTIL_TraceEntity(pEntity, TraceStart, TraceEnd);

		if (!FNullEnt(TracedEntity))
		{
			const nav_door* Door = UTIL_GetNavDoorByEdict(TracedEntity);

			if (Door)
			{
				edict_t* Trigger = UTIL_GetNearestDoorTrigger(pEntity->v.origin, Door, nullptr);

				if (!FNullEnt(Trigger))
				{
					UTIL_DrawLine(pEntity, pEntity->v.origin, UTIL_GetCentreOfEntity(Trigger), 10.0f);
				}

				char ActType[64];

				switch (Door->ActivationType)
				{
					case DOOR_BUTTON:
						sprintf(ActType, "Button\n");
						break;
					case DOOR_SHOOT:
						sprintf(ActType, "Shoot\n");
						break;
					case DOOR_TRIGGER:
						sprintf(ActType, "Trigger\n");
						break;
					case DOOR_USE:
						sprintf(ActType, "Use\n");
						break;
					case DOOR_WELD:
						sprintf(ActType, "Weld\n");
						break;
					default:
						sprintf(ActType, "None\n");
						break;
				}

				UTIL_SayText(ActType, pEntity);

				sprintf(ActType, "%4.2f, %4.2f, %4.2f\n", TracedEntity->v.absmin.x, TracedEntity->v.absmin.y, TracedEntity->v.absmin.z);
				UTIL_SayText(ActType, pEntity);
			}
		}		

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "defendtask"))
	{
		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used && IsPlayerAlien(bots[i].pEdict) && IsPlayerActiveInGame(bots[i].pEdict))
			{
				edict_t* ResourceTower = UTIL_GetNearestUndefendedStructureOfTypeUnderAttack(&bots[i], STRUCTURE_ALIEN_RESTOWER, true);

				if (!FNullEnt(ResourceTower))
				{
					UTIL_SayText("True\n", pEntity);
					UTIL_DrawLine(pEntity, bots[i].pEdict->v.origin, ResourceTower->v.origin, 10.0f, 255, 255, 0);
				}
				else
				{
					UTIL_SayText("False\n", pEntity);
				}
			}

		}
		

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "showdoors"))
	{
		FILE* DoorLog = fopen("DoorLog.txt", "w+");

		if (!DoorLog)
		{
			RETURN_META(MRES_SUPERCEDE);
		}

		edict_t* currDoor = NULL;
		while (((currDoor = UTIL_FindEntityByClassname(currDoor, "func_door")) != NULL) && (!FNullEnt(currDoor)))
		{

			Vector StartLine = pEntity->v.origin;
			Vector EndX = StartLine + Vector(50.0f, 0.0f, 0.0f);
			Vector EndY = StartLine + Vector(0.0f, 50.0f, 0.0f);
			Vector EndZ = StartLine + Vector(0.0f, 0.0f, 50.0f);

			Vector LocalMin = currDoor->v.mins;
			Vector LocalMax = currDoor->v.maxs;

			UTIL_DrawLine(pEntity, StartLine, EndX, 10.0f, 255, 0, 0);
			UTIL_DrawLine(pEntity, StartLine, EndY, 10.0f, 255, 255, 0);
			UTIL_DrawLine(pEntity, StartLine, EndZ, 10.0f, 0, 0, 255);


			//UTIL_DrawLine(pEntity, currDoor->v.mins, currDoor->v.maxs, 10.0f, 255, 0, 0);
			UTIL_DrawLine(pEntity, currDoor->v.absmin, currDoor->v.absmax, 10.0f, 255, 255, 255);

			fprintf(DoorLog, "Door %s origin: (%f, %f, %f)\n", STRING(currDoor->v.targetname), currDoor->v.origin.x, currDoor->v.origin.y, currDoor->v.origin.z);
			fprintf(DoorLog, "Door %s containing entity: %s, %s\n", STRING(currDoor->v.targetname), STRING(currDoor->v.pContainingEntity->v.targetname), STRING(currDoor->v.pContainingEntity->v.classname));
			fprintf(DoorLog, "Door %s v_angle: (%f, %f, %f)\n", STRING(currDoor->v.targetname), currDoor->v.v_angle.x, currDoor->v.v_angle.y, currDoor->v.v_angle.z);
			fprintf(DoorLog, "Door %s ideal_yaw: %f\n", STRING(currDoor->v.targetname), currDoor->v.ideal_yaw);
			fprintf(DoorLog, "Door %s: (%f, %f, %f) - (%f, %f, %f)\n", STRING(currDoor->v.targetname), currDoor->v.mins.x, currDoor->v.mins.y, currDoor->v.mins.z, currDoor->v.maxs.x, currDoor->v.maxs.y, currDoor->v.maxs.z);
			fprintf(DoorLog, "Door %s: (%f, %f, %f) - (%f, %f, %f)\n\n", STRING(currDoor->v.targetname), currDoor->v.absmin.x, currDoor->v.absmin.y, currDoor->v.absmin.z, currDoor->v.absmax.x, currDoor->v.absmax.y, currDoor->v.absmax.z);
		}

		currDoor = NULL;
		while (((currDoor = UTIL_FindEntityByClassname(currDoor, "func_door_rotating")) != NULL) && (!FNullEnt(currDoor)))
		{

			Vector StartLine = pEntity->v.origin;
			Vector EndX = StartLine + Vector(50.0f, 0.0f, 0.0f);
			Vector EndY = StartLine + Vector(0.0f, 50.0f, 0.0f);
			Vector EndZ = StartLine + Vector(0.0f, 0.0f, 50.0f);

			Vector LocalMin = currDoor->v.mins;
			Vector LocalMax = currDoor->v.maxs;

			UTIL_DrawLine(pEntity, StartLine, EndX, 10.0f, 255, 0, 0);
			UTIL_DrawLine(pEntity, StartLine, EndY, 10.0f, 255, 255, 0);
			UTIL_DrawLine(pEntity, StartLine, EndZ, 10.0f, 0, 0, 255);


			//UTIL_DrawLine(pEntity, currDoor->v.mins, currDoor->v.maxs, 10.0f, 255, 0, 0);
			UTIL_DrawLine(pEntity, currDoor->v.absmin, currDoor->v.absmax, 10.0f, 255, 255, 255);

			fprintf(DoorLog, "Door %s origin: (%f, %f, %f)\n", STRING(currDoor->v.targetname), currDoor->v.origin.x, currDoor->v.origin.y, currDoor->v.origin.z);
			fprintf(DoorLog, "Door %s containing entity: %s, %s\n", STRING(currDoor->v.targetname), STRING(currDoor->v.pContainingEntity->v.targetname), STRING(currDoor->v.pContainingEntity->v.classname));
			fprintf(DoorLog, "Door %s v_angle: (%f, %f, %f)\n", STRING(currDoor->v.targetname), currDoor->v.v_angle.x, currDoor->v.v_angle.y, currDoor->v.v_angle.z);
			fprintf(DoorLog, "Door %s ideal_yaw: %f\n", STRING(currDoor->v.targetname), currDoor->v.ideal_yaw);
			fprintf(DoorLog, "Door %s: (%f, %f, %f) - (%f, %f, %f)\n", STRING(currDoor->v.targetname), currDoor->v.mins.x, currDoor->v.mins.y, currDoor->v.mins.z, currDoor->v.maxs.x, currDoor->v.maxs.y, currDoor->v.maxs.z);
			fprintf(DoorLog, "Door %s: (%f, %f, %f) - (%f, %f, %f)\n\n", STRING(currDoor->v.targetname), currDoor->v.absmin.x, currDoor->v.absmin.y, currDoor->v.absmin.z, currDoor->v.absmax.x, currDoor->v.absmax.y, currDoor->v.absmax.z);
		}

		fclose(DoorLog);

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "botnavinfo"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		edict_t* SpectatorTarget = INDEXENT(pEntity->v.iuser2);

		if (FNullEnt(SpectatorTarget))
		{
			UTIL_SayText("No Spectator Target\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		int BotIndex = GetBotIndex(SpectatorTarget);

		if (BotIndex < 0)
		{
			UTIL_SayText("Not spectating a bot\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		bot_t* pBot = &bots[BotIndex];

		char buf[150];

		if (pBot->BotNavInfo.UnstuckMoveLocation != ZERO_VECTOR)
		{
			UTIL_DrawLine(clients[0], pBot->pEdict->v.origin, pBot->BotNavInfo.UnstuckMoveLocation, 20.0f, 255, 0, 0);
		}

		BotDrawPath(pBot, 20.0f, false);

		DEBUG_DrawBotNextPathPoint(pBot, 20.0f);

		sprintf(buf, "Path Status: %s\n", pBot->PathStatus);
		UTIL_SayText(buf, pEntity);

		sprintf(buf, "Move Status: %s\n", pBot->MoveStatus);
		UTIL_SayText(buf, pEntity);

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "bottaskinfo"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		edict_t* SpectatorTarget = INDEXENT(pEntity->v.iuser2);

		if (FNullEnt(SpectatorTarget))
		{
			UTIL_SayText("No Spectator Target\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		int BotIndex = GetBotIndex(SpectatorTarget);

		if (BotIndex < 0)
		{
			UTIL_SayText("Not spectating a bot\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		bot_t* pBot = &bots[BotIndex];

		char buf[64];

		if (pBot->CurrentTask)
		{
			sprintf(buf, "Bot Role: %s\n", UTIL_BotRoleToChar(pBot->CurrentRole));
			UTIL_SayText(buf, listenserver_edict);

			sprintf(buf, "Primary Task: %s\n", UTIL_TaskTypeToChar(pBot->PrimaryBotTask.TaskType));
			UTIL_SayText(buf, listenserver_edict);

			sprintf(buf, "Second Task: %s\n", UTIL_TaskTypeToChar(pBot->SecondaryBotTask.TaskType));
			UTIL_SayText(buf, listenserver_edict);

			sprintf(buf, "Want Task: %s\n", UTIL_TaskTypeToChar(pBot->WantsAndNeedsTask.TaskType));
			UTIL_SayText(buf, listenserver_edict);

			if (BotGetNextEnemyTarget(pBot) > -1)
			{
				sprintf(buf, "Current Task: COMBAT\n");
				UTIL_SayText(buf, listenserver_edict);
				RETURN_META(MRES_SUPERCEDE);
			}

			sprintf(buf, "Current Task: %s\n", UTIL_TaskTypeToChar(pBot->CurrentTask->TaskType));
			UTIL_SayText(buf, listenserver_edict);

			if (pBot->CurrentTask->TaskType == TASK_BUILD)
			{
				sprintf(buf, "Build: %s\n", UTIL_StructTypeToChar(pBot->CurrentTask->StructureType));
				UTIL_SayText(buf, listenserver_edict);
			}

			if (!FNullEnt(pBot->CurrentTask->TaskTarget))
			{
				sprintf(buf, "Target Location = (%4.2f, %4.2f, %4.2f)\n\n", pBot->CurrentTask->TaskTarget->v.origin.x, pBot->CurrentTask->TaskTarget->v.origin.y, pBot->CurrentTask->TaskTarget->v.origin.z);
				UTIL_SayText(buf, listenserver_edict);
			}

			UTIL_DrawLine(clients[0], pBot->CurrentEyePosition, pBot->CurrentTask->TaskLocation, 20.0f, 255, 255, 0);
			if (!FNullEnt(pBot->CurrentTask->TaskTarget))
			{
				UTIL_DrawLine(clients[0], pBot->CurrentEyePosition, pBot->CurrentTask->TaskTarget->v.origin, 20.0f, 255, 0, 0);
			}
		}
		else
		{
			UTIL_SayText("No Current Task\n", listenserver_edict);
		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "botstucktime"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		edict_t* SpectatorTarget = INDEXENT(pEntity->v.iuser2);

		if (FNullEnt(SpectatorTarget))
		{
			UTIL_SayText("No Spectator Target\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		int BotIndex = GetBotIndex(SpectatorTarget);

		if (BotIndex < 0)
		{
			UTIL_SayText("Not spectating a bot\n", listenserver_edict);
			RETURN_META(MRES_SUPERCEDE);
		}

		bot_t* pBot = &bots[BotIndex];

		char buf[32];
		sprintf(buf, "%3.2f\n", pBot->BotNavInfo.TotalStuckTime);

		UTIL_SayText(buf, pEntity);

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "botgivepoints"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)
			{
				FakeClientCommand(bots[i].pEdict, "givepoints", NULL, NULL);
			}

		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "killbots"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)
			{
				//FakeClientCommand(bots[i].pEdict, "kill", NULL, NULL);
				BotSuicide(&bots[i]);
			}

		}

		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "cometome"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				TASK_SetMoveTask(&bots[i], &bots[i].PrimaryBotTask, UTIL_GetFloorUnderEntity(pEntity), true);
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "testattack"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				edict_t* PhaseGate = UTIL_GetNearestStructureIndexOfType(bots[i].pEdict->v.origin, STRUCTURE_MARINE_PHASEGATE, UTIL_MetresToGoldSrcUnits(200.0f), false, false);

				if (!FNullEnt(PhaseGate))
				{
					TASK_SetAttackTask(&bots[i], &bots[i].PrimaryBotTask, PhaseGate, true);
				}				
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "evolveskulk"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				if (IsPlayerOnAlienTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))
				{
					TASK_SetEvolveTask(&bots[i], &bots[i].PrimaryBotTask, bots[i].pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_SKULK, true);
				}
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "evolvegorge"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				if (IsPlayerOnAlienTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))
				{
					TASK_SetEvolveTask(&bots[i], &bots[i].PrimaryBotTask, bots[i].pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_GORGE, true);
				}
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "evolvelerk"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				if (IsPlayerOnAlienTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))
				{
					TASK_SetEvolveTask(&bots[i], &bots[i].PrimaryBotTask, bots[i].pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_LERK, true);
				}
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "evolvefade"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				if (IsPlayerOnAlienTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))
				{
					TASK_SetEvolveTask(&bots[i], &bots[i].PrimaryBotTask, bots[i].pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_FADE, true);
				}
			}
		}
		RETURN_META(MRES_SUPERCEDE);
	}

	if (FStrEq(pcmd, "evolveonos"))
	{
		if (!NavmeshLoaded())
		{
			UTIL_SayText("Navmesh is not loaded", pEntity);
			RETURN_META(MRES_SUPERCEDE);
		}

		for (int i = 0; i < gpGlobals->maxClients; i++)
		{
			if (bots[i].is_used)  // not respawning
			{
				if (IsPlayerOnAlienTeam(bots[i].pEdict) && !IsPlayerDead(bots[i].pEdict))
				{
					TASK_SetEvolveTask(&bots[i], &bots[i].PrimaryBotTask, bots[i].pEdict->v.origin, IMPULSE_ALIEN_EVOLVE_ONOS, true);
				}
			}
		}

		RETURN_META(MRES_SUPERCEDE);
	}


	RETURN_META(MRES_IGNORED);

}

void GameDLLInit(void)
{

	GAME_Reset();
	GAME_ClearClientList();
	
	RETURN_META(MRES_IGNORED);
}

BOOL ClientConnect(edict_t* pEntity, const char* pszName, const char* pszAddress, char szRejectReason[128])
{
	
	if (gpGlobals->deathmatch)
	{
		// check if this client is the listen server client
		if (!GAME_IsDedicatedServer() && strcmp(pszAddress, "loopback") == 0)
		{
			// save the edict of the listen server client...
			GAME_SetListenServerEdict(pEntity);
		}
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void ClientPutInServer(edict_t* pEntity)
{
	//GAME_AddClient(pEntity);

	RETURN_META(MRES_IGNORED);
}

void ClientDisconnect(edict_t* pEntity)
{
	if (gpGlobals->deathmatch)
	{
		//GAME_RemoveClient(pEntity);
	}

	RETURN_META(MRES_IGNORED);
}

int Spawn(edict_t* pent)
{
	if (gpGlobals->deathmatch)
	{
		m_spriteTexture = PRECACHE_MODEL("sprites/zbeam6.spr");

		char* pClassname = (char*)STRING(pent->v.classname);

		if (strcmp(pClassname, "worldspawn") == 0)
		{
			UnloadNavigationData();
			GAME_Reset();
			ParseConfigFile(false);
		}

	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

void KeyValue(edict_t* pentKeyvalue, KeyValueData* pkvd)
{

	RETURN_META(MRES_IGNORED);
}

void StartFrame(void)
{
	static clock_t prevtime = 0.0f;
	static clock_t currTime = 0.0f;
	static float last_structure_refresh_time = 0.0f;
	static float last_item_refresh_time = 0.0f;

	currTime = clock();

	if (gpGlobals->deathmatch)
	{
		static int bot_index;

		GAME_RefreshClientList();

		if (gpGlobals->time >= 5.0f)
		{
			if (gpGlobals->time - last_bot_count_check_time > 0.25f)
			{
				
				GAME_UpdateBotCounts();
				last_bot_count_check_time = gpGlobals->time;
			}
		}

		if (NavmeshLoaded())
		{
			if (GAME_GetGameStatus() == GAME_STATUS_ACTIVE)
			{
				UTIL_UpdateMapAIData();

				if (!FNullEnt(GAME_GetListenServerEdict()))
				{
					edict_t* SpectatorTarget = INDEXENT(GAME_GetListenServerEdict()->v.iuser2);

					bot_t* SpectatedBot = nullptr;

					if (!FNullEnt(SpectatorTarget))
					{
						int BotIndex = GetBotIndex(SpectatorTarget);

						if (BotIndex >= 0)
						{
							SpectatedBot = &bots[BotIndex];
						}
					}

					if (SpectatedBot)
					{
						if (DEBUG_ShouldShowTaskInfo())
						{
							UTIL_DisplayBotInfo(SpectatedBot);
						}

						if (DEBUG_ShouldShowBotPath())
						{
							BotDrawPath(SpectatedBot, 0.0f, false);
						}
					}					
				}
			}

			double timeSinceLastThink = (double)((currTime - last_think_time) / CLOCKS_PER_SEC);

			if (GAME_IsDedicatedServer() || timeSinceLastThink >= BOT_MIN_FRAME_TIME)
			{
				GAME_SetBotDeltaTime(timeSinceLastThink);

				UTIL_UpdateWeldableDoors();
				UTIL_UpdateWeldableObstacles();

				UTIL_UpdateTileCache();

				GAME_TrackPlayerEvolutions();

				for (bot_index = 0; bot_index < gpGlobals->maxClients; bot_index++)
				{
					bot_t* bot = &bots[bot_index];

					if (bot->is_used && !FNullEnt(bot->pEdict))
					{

						for (int i = 0; i < 5; i++)
						{
							if (bot->ChatMessages[i].bIsPending && gpGlobals->time >= bot->ChatMessages[i].SendTime)
							{
								if (bot->ChatMessages[i].bIsTeamSay)
								{
									BotTeamSay(bot, bot->ChatMessages[i].msg);
								}
								else
								{
									BotSay(bot, bot->ChatMessages[i].msg);
								}
								bot->ChatMessages[i].bIsPending = false;
								break;
							}
						}

						BotUpdateViewRotation(bot, timeSinceLastThink);

						if (ShouldBotThink(bot))
						{
							BotThink(bot);
						}
						else
						{
							if (!bot->bBotThinkPaused && IsPlayerGestating(bot->pEdict))
							{
								OnBotBeginGestation(bot);
							}

							bot->bBotThinkPaused = true;
						}
						// Adjust msec to command time interval
						byte adjustedmsec = BotThrottledMsec(bot);

						// save the command time
						bot->f_previous_command_time = gpGlobals->time;

						g_engfuncs.pfnRunPlayerMove(bot->pEdict, bot->pEdict->v.v_angle, bot->ForwardMove,
							bot->SideMove, bot->UpMove, bot->pEdict->v.button, bot->impulse, adjustedmsec);
					}
				}
				last_think_time = currTime;
			}

		}

	}

	prevtime = currTime;

	RETURN_META(MRES_IGNORED);
}

static DLL_FUNCTIONS gFunctionTable =
{
	GameDLLInit,			// pfnGameInit
	Spawn,					// pfnSpawn
	NULL,					// pfnThink
	NULL,					// pfnUse
	NULL,					// pfnTouch
	NULL,					// pfnBlocked
	NULL,					// pfnKeyValue
	NULL,					// pfnSave
	NULL,					// pfnRestore
	NULL,					// pfnSetAbsBox

	NULL,					// pfnSaveWriteFields
	NULL,					// pfnSaveReadFields

	NULL,					// pfnSaveGlobalState
	NULL,					// pfnRestoreGlobalState
	NULL,					// pfnResetGlobalState

	ClientConnect,			// pfnClientConnect
	ClientDisconnect,		// pfnClientDisconnect
	NULL,					// pfnClientKill
	ClientPutInServer,		// pfnClientPutInServer
	ClientCommand,			// pfnClientCommand
	NULL,					// pfnClientUserInfoChanged
	NULL,					// pfnServerActivate
	NULL,					// pfnServerDeactivate

	NULL,					// pfnPlayerPreThink
	NULL,					// pfnPlayerPostThink

	StartFrame,				// pfnStartFrame
	NULL,					// pfnParmsNewLevel
	NULL,					// pfnParmsChangeLevel

	NULL,					// pfnGetGameDescription
	NULL,					// pfnPlayerCustomization

	NULL,					// pfnSpectatorConnect
	NULL,					// pfnSpectatorDisconnect
	NULL,					// pfnSpectatorThink

	NULL,					// pfnSys_Error

	NULL,					// pfnPM_Move
	NULL,					// pfnPM_Init
	NULL,					// pfnPM_FindTextureType

	NULL,					// pfnSetupVisibility
	NULL,					// pfnUpdateClientData
	NULL,					// pfnAddToFullPack
	NULL,					// pfnCreateBaseline
	NULL,					// pfnRegisterEncoders
	NULL,					// pfnGetWeaponData
	NULL,					// pfnCmdStart
	NULL,					// pfnCmdEnd
	NULL,					// pfnConnectionlessPacket
	NULL,					// pfnGetHullBounds
	NULL,					// pfnCreateInstancedBaselines
	NULL,					// pfnInconsistentFile
	NULL,					// pfnAllowLagCompensation
};

C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, 
		int *interfaceVersion)
{
	if(!pFunctionTable) {
		UTIL_LogPrintf("GetEntityAPI2 called with null pFunctionTable");
		return(FALSE);
	}
	else if(*interfaceVersion != INTERFACE_VERSION) {
		UTIL_LogPrintf("GetEntityAPI2 version mismatch; requested=%d ours=%d", *interfaceVersion, INTERFACE_VERSION);
		//! Tell metamod what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return(FALSE);
	}
	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return(TRUE);
}

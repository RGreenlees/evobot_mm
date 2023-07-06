//
// EvoBot - Neoptolemus' Natural Selection bot, based on Botman's HPB bot template
//
// bot_bsp.h
// 
// Contains some helper functions for parsing BSP files
//

#pragma once

#ifndef BOT_BSP_H
#define BOT_BSP_H

#include <extdll.h>


#define LUMP_ENTITIES      0
#define HEADER_LUMPS      15

#define MAX_MAP_ENTITIES     2048

typedef struct _BSPLUMP
{
	int nOffset; // File offset to data
	int nLength; // Length of data
} BSPLUMP;


typedef struct _BSPHEADER
{
	int nVersion;           // Must be 30 for a valid HL BSP file
	BSPLUMP lump[HEADER_LUMPS]; // Stores the directory of lumps
} BSPHEADER;

typedef struct _BSPENTITYPROPERTY
{
	char propertyName[1024];
	char propertyValue[1024];
} BSPENTITYPROPERTY;

typedef struct _BSPENTITYDEF
{
	char TargetName[1024];
	char Target[1024];
	int startPos;
	int endPos;
	char data[1024];
	int numProperties;
	BSPENTITYPROPERTY properties[10];
} BSPENTITYDEF;

typedef struct _BUTTONDEF
{
	char TargetName[64];
	char Target[64];
	float Delay = 0.0f;

} BUTTONDEF;



void BSP_RegisterWeldables();
char* GetEntityDefClassname(BSPENTITYDEF* entity);

#endif
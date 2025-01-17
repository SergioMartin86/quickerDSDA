//
// Copyright(C) 2022 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//  DSDA MapInfo Hexen
//

#include "doomstat.h"
#include "g_game.h"
#include "lprintf.h"
#include "m_misc.h"
#include "p_setup.h"
#include "r_data.h"
#include "sc_man.h"
#include "sounds.h"
#include "w_wad.h"

#include "hexen/p_acs.h"

#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/sndinfo.h"

#include "hexen.h"

#define MAPINFO_SCRIPT_NAME "MAPINFO"
#define MCMD_SKY1 1
#define MCMD_SKY2 2
#define MCMD_LIGHTNING 3
#define MCMD_FADETABLE 4
#define MCMD_DOUBLESKY 5
#define MCMD_CLUSTER 6
#define MCMD_WARPTRANS 7
#define MCMD_NEXT 8
#define MCMD_CDTRACK 9
#define MCMD_CD_STARTTRACK 10
#define MCMD_CD_END1TRACK 11
#define MCMD_CD_END2TRACK 12
#define MCMD_CD_END3TRACK 13
#define MCMD_CD_INTERTRACK 14
#define MCMD_CD_TITLETRACK 15

#define UNKNOWN_MAP_NAME "DEVELOPMENT MAP"
#define DEFAULT_SKY_NAME "SKY1"
#define DEFAULT_FADE_TABLE "COLORMAP"

typedef struct mapInfo_s {
  short cluster;
  short warpTrans;
  short nextMap;
  char name[32];
  short sky1Texture;
  short sky2Texture;
  fixed_t sky1ScrollDelta;
  fixed_t sky2ScrollDelta;
  dboolean doubleSky;
  dboolean lightning;
  int fadetable;
  char songLump[10];
} mapInfo_t;

static int MapCount = 98;

static mapInfo_t MapInfo[99];

static mapInfo_t *CurrentMap = MapInfo;

static const char *MapCmdNames[] = {
  "SKY1",
  "SKY2",
  "DOUBLESKY",
  "LIGHTNING",
  "FADETABLE",
  "CLUSTER",
  "WARPTRANS",
  "NEXT",
  "CDTRACK",
  "CD_START_TRACK",
  "CD_END1_TRACK",
  "CD_END2_TRACK",
  "CD_END3_TRACK",
  "CD_INTERMISSION_TRACK",
  "CD_TITLE_TRACK",
  NULL
};

static int MapCmdIDs[] = {
  MCMD_SKY1,
  MCMD_SKY2,
  MCMD_DOUBLESKY,
  MCMD_LIGHTNING,
  MCMD_FADETABLE,
  MCMD_CLUSTER,
  MCMD_WARPTRANS,
  MCMD_NEXT,
  MCMD_CDTRACK,
  MCMD_CD_STARTTRACK,
  MCMD_CD_END1TRACK,
  MCMD_CD_END2TRACK,
  MCMD_CD_END3TRACK,
  MCMD_CD_INTERTRACK,
  MCMD_CD_TITLETRACK
};

static int QualifyMap(int map) {
  return (map < 1 || map > MapCount) ? 0 : map;
}

static int P_TranslateMap(int map) {
  int i;

  for (i = 1; i < 99; i++)
    if (MapInfo[i].warpTrans == map)
      return i;

  return -1;
}

int dsda_HexenNameToMap(int* found, const char* name, int* episode, int* map) {
  return false;
}

int dsda_HexenFirstMap(int* episode, int* map) {
    return false;
}

int dsda_HexenNewGameMap(int* episode, int* map) {
    return false;

}

int dsda_HexenResolveWarp(int* args, int arg_count, int* episode, int* map) {
    return false;
}

int dsda_HexenNextMap(int* episode, int* map) {
    return false;

}

int dsda_HexenPrevMap(int* episode, int* map) {

    return false;

}

int dsda_HexenShowNextLocBehaviour(int* behaviour) {
  return false; // TODO
}

int dsda_HexenSkipDrawShowNextLoc(int* skip) {
  return false; // TODO
}

void dsda_HexenUpdateMapInfo(void) {
  CurrentMap = &MapInfo[QualifyMap(gamemap)];
}

void dsda_HexenUpdateLastMapInfo(void) {
  // TODO
}

void dsda_HexenUpdateNextMapInfo(void) {
  // TODO
}

int dsda_HexenResolveCLEV(int* clev, int* episode, int* map) {

    return false;
}

dboolean partial_reset = false;

int dsda_HexenResolveINIT(int* init) {
    return false;
}

int dsda_HexenMusicIndexToLumpNum(int* lump, int music_index) {

    return false;

}

int dsda_HexenMapMusic(int* music_index, int* music_lump, int episode, int map) {
    return false;
}

int dsda_HexenIntermissionMusic(int* music_index, int* music_lump) {
  return false; // TODO
}

int dsda_HexenInterMusic(int* music_index, int* music_lump) {
  return false; // TODO
}

int dsda_HexenStartFinale(void) {
  return false; // TODO
}

int dsda_HexenFTicker(void) {
  return false; // TODO
}

void dsda_HexenFDrawer(void) {
  return; // TODO
}

int dsda_HexenBossAction(mobj_t* mo) {
  return false; // TODO
}

int dsda_HexenMapLumpName(const char** name, int episode, int map) {
  return false; // TODO
}

int dsda_HexenMapAuthor(const char** author) {
  return false;
}

int dsda_HexenHUTitle(dsda_string_t* str) {
    return false;
}

int dsda_HexenSkyTexture(int* sky) {
  return false; // TODO
}

int dsda_HexenPrepareInitNew(void) {

    return false;
}
int dsda_HexenPrepareIntermission(int* result) {
    return false;
}

int dsda_HexenPrepareFinale(int* result) {
  return false; // TODO
}

void dsda_HexenLoadMapInfo(void) {
    return;
}

int dsda_HexenExitPic(const char** exit_pic) {
  return false; // TODO
}

int dsda_HexenEnterPic(const char** enter_pic) {
  return false; // TODO
}

int dsda_HexenBorderTexture(const char** border_texture) {
    return false;
}

int dsda_HexenPrepareEntering(void) {
  return false; // TODO
}

int dsda_HexenPrepareFinished(void) {
  return false; // TODO
}

int dsda_HexenMapLightning(int* lightning) {
    return false;
}

int dsda_HexenApplyFadeTable(void) {
    return false;
}

int dsda_HexenMapCluster(int* cluster, int map) {
    return false;
}

int dsda_HexenSky1Texture(short* texture) {
    return false;
}

int dsda_HexenSky2Texture(short* texture) {
    return false;
}

int dsda_HexenGravity(fixed_t* gravity) {
  return false;
}

int dsda_HexenAirControl(fixed_t* air_control) {
    return false;
}

int dsda_HexenInitSky(void) {
    return false;
}

int dsda_HexenMapFlags(map_info_flags_t* flags) {
    return false;
}

int dsda_HexenMapColorMap(int* colormap) {
  return false;
}

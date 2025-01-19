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
//	DSDA MapInfo
//

#include <stdio.h>
#include <string.h>

#include "doomstat.h"
#include "m_misc.h"

#include "dsda/args.h"
#include "dsda/episode.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo/doom.h"
#include "dsda/mapinfo/legacy.h"

#include "mapinfo.h"

__thread map_info_t map_info;

int dsda_NameToMap(const char* name, int* episode, int* map) {
  int found;

  if (dsda_DoomNameToMap(&found, name, episode, map))
    return found;


  dsda_LegacyNameToMap(&found, name, episode, map);

  return found;
}

void dsda_FirstMap(int* episode, int* map) {
  if (dsda_DoomFirstMap(episode, map))
    return;


  dsda_LegacyFirstMap(episode, map);
}

void dsda_NewGameMap(int* episode, int* map) {
  if (dsda_DoomNewGameMap(episode, map))
    return;

  dsda_LegacyNewGameMap(episode, map);
}

void dsda_ResolveWarp(int* args, int arg_count, int* episode, int* map) {
  if (dsda_DoomResolveWarp(args, arg_count, episode, map))
    return;

  dsda_LegacyResolveWarp(args, arg_count, episode, map);
}

void dsda_NextMap(int* episode, int* map) {
  if (dsda_DoomNextMap(episode, map))
    return;

  dsda_LegacyNextMap(episode, map);
}

void dsda_PrevMap(int* episode, int* map) {
  if (dsda_DoomPrevMap(episode, map))
    return;

  dsda_LegacyPrevMap(episode, map);
}

void dsda_ShowNextLocBehaviour(int* behaviour) {
  if (dsda_DoomShowNextLocBehaviour(behaviour))
    return;

  dsda_LegacyShowNextLocBehaviour(behaviour);
}

int dsda_SkipDrawShowNextLoc(void) {
  int skip;

  if (dsda_DoomSkipDrawShowNextLoc(&skip))
    return skip;

  dsda_LegacySkipDrawShowNextLoc(&skip);

  return skip;
}

static fixed_t dsda_Gravity(void) {
  fixed_t gravity;

  if (dsda_DoomGravity(&gravity))
    return gravity;

  dsda_LegacyGravity(&gravity);

  return gravity;
}

static fixed_t dsda_AirControl(void) {
  fixed_t air_control;

  if (dsda_DoomAirControl(&air_control))
    return air_control;

  dsda_LegacyAirControl(&air_control);

  return air_control;
}

static map_info_flags_t dsda_MapFlags(void) {
  map_info_flags_t flags;

  if (dsda_DoomMapFlags(&flags))
    return flags;

  dsda_LegacyMapFlags(&flags);

  return flags;
}

static int dsda_MapColorMap(void) {
  int colormap;

  if (dsda_DoomMapColorMap(&colormap))
    return colormap;

  dsda_LegacyMapColorMap(&colormap);

  return colormap;
}

static void dsda_UpdateMapInfo(void) {
  dsda_DoomUpdateMapInfo();
  dsda_LegacyUpdateMapInfo();

  map_info.flags = dsda_MapFlags();
  map_info.default_colormap = dsda_MapColorMap();
  map_info.gravity = dsda_Gravity();
  map_info.air_control = dsda_AirControl();
  // This formula is based on 256 -> 65536 (no friction) and 65536 -> 0xe800 (normal friction)
  map_info.air_friction = map_info.air_control > 256 ?
                          65560 - FixedMul(map_info.air_control, 6168) :
                          FRACUNIT;
}

void dsda_UpdateGameMap(int episode, int map) {
  gameepisode = episode;
  gamemap = map;
  dsda_UpdateMapInfo();
}

void dsda_ResetAirControl(void) {
  map_info.air_control = dsda_AirControl();
}

void dsda_ResetLeaveData(void) {
  memset(&leave_data, 0, sizeof(leave_data));
}

void dsda_UpdateLeaveData(int map, int position, int flags, angle_t angle) {
  leave_data.map = map;
  leave_data.position = position;
  leave_data.flags = flags;
  leave_data.angle = angle;
}

dboolean dsda_FinaleShortcut(void) {
  return map_format.zdoom && leave_data.map == LEAVE_VICTORY;
}

void dsda_UpdateLastMapInfo(void) {
  dsda_DoomUpdateLastMapInfo();
  dsda_LegacyUpdateLastMapInfo();
}

void dsda_UpdateNextMapInfo(void) {
  dsda_DoomUpdateNextMapInfo();
  dsda_LegacyUpdateNextMapInfo();
}

int dsda_ResolveCLEV(int* episode, int* map) {
  int clev;

  if (dsda_DoomResolveCLEV(&clev, episode, map))
    return clev;

  dsda_LegacyResolveCLEV(&clev, episode, map);

  return clev;
}

int dsda_ResolveINIT(void) {
  int init;

  if (dsda_DoomResolveINIT(&init))
    return init;

  dsda_LegacyResolveINIT(&init);

  return init;
}

int dsda_MusicIndexToLumpNum(int music_index) {
  int lump;

  if (dsda_DoomMusicIndexToLumpNum(&lump, music_index))
    return lump;

  dsda_LegacyMusicIndexToLumpNum(&lump, music_index);

  return lump;
}

void dsda_MapMusic(int* music_index, int* music_lump, int episode, int map) {
  if (dsda_DoomMapMusic(music_index, music_lump, episode, map))
    return;

  dsda_LegacyMapMusic(music_index, music_lump, episode, map);
}

void dsda_IntermissionMusic(int* music_index, int* music_lump) {
  if (dsda_DoomIntermissionMusic(music_index, music_lump))
    return;

  dsda_LegacyIntermissionMusic(music_index, music_lump);
}

void dsda_InterMusic(int* music_index, int* music_lump) {
  if (dsda_DoomInterMusic(music_index, music_lump))
    return;

  dsda_LegacyInterMusic(music_index, music_lump);
}

typedef enum {
  finale_owner_legacy,
  finale_owner_doom,
} finale_owner_t;

static finale_owner_t finale_owner = finale_owner_legacy;

void dsda_StartFinale(void) {
  if (dsda_DoomStartFinale()) {
    finale_owner = finale_owner_doom;
    return;
  }

  dsda_LegacyStartFinale();
  finale_owner = finale_owner_legacy;
}

int dsda_FTicker(void) {
  if (finale_owner == finale_owner_doom) {
    if (!dsda_DoomFTicker())
      finale_owner = finale_owner_legacy;

    return true;
  }
  dsda_LegacyFTicker();
  return false;
}

int dsda_FDrawer(void) {
  if (finale_owner == finale_owner_doom) {
    dsda_DoomFDrawer();

    return true;
  }

  dsda_LegacyFDrawer();
  return false;
}

int dsda_BossAction(mobj_t* mo) {
  if (dsda_DoomBossAction(mo))
    return true;

  dsda_LegacyBossAction(mo);
  return false;
}

const char* dsda_MapLumpName(int episode, int map) {
  const char* name;

  if (dsda_DoomMapLumpName(&name, episode, map))
    return name;

  dsda_LegacyMapLumpName(&name, episode, map);

  return name;
}

void dsda_HUTitle(dsda_string_t* str) {
  if (dsda_DoomHUTitle(str))
    return;

  dsda_LegacyHUTitle(str);
}

const char* dsda_MapAuthor(void) {
  const char* author;

  if (dsda_DoomMapAuthor(&author))
    return author;

  dsda_LegacyMapAuthor(&author);

  return author;
}

int dsda_SkyTexture(void) {
  int sky;

  if (dsda_DoomSkyTexture(&sky))
    return sky;

  dsda_LegacySkyTexture(&sky);

  return sky;
}

void dsda_PrepareInitNew(void) {
  if (dsda_DoomPrepareInitNew())
    return;

  dsda_LegacyPrepareInitNew();
}

void dsda_PrepareIntermission(int* behaviour) {
  if (dsda_DoomPrepareIntermission(behaviour))
    return;

  dsda_LegacyPrepareIntermission(behaviour);
}

void dsda_PrepareFinale(int* behaviour) {
  if (dsda_DoomPrepareFinale(behaviour))
    return;

  dsda_LegacyPrepareFinale(behaviour);
}

void dsda_LoadMapInfo(void) {
  dsda_AddOriginalEpisodes();

  dsda_DoomLoadMapInfo();
  dsda_LegacyLoadMapInfo();

  dsda_AddCustomEpisodes();
}

const char* dsda_ExitPic(void) {
  const char* exit_pic;

  if (dsda_DoomExitPic(&exit_pic))
    return exit_pic;

  dsda_LegacyExitPic(&exit_pic);
  return exit_pic;
}

const char* dsda_EnterPic(void) {
  const char* enter_pic;

  if (dsda_DoomEnterPic(&enter_pic))
    return enter_pic;

  dsda_LegacyEnterPic(&enter_pic);
  return enter_pic;
}

const char* dsda_BorderTexture(void) {
  const char* border_texture;

  if (dsda_DoomBorderTexture(&border_texture))
    return border_texture;

  dsda_LegacyBorderTexture(&border_texture);
  return border_texture;
}

void dsda_PrepareEntering(void) {
  if (dsda_DoomPrepareEntering())
    return;

  dsda_LegacyPrepareEntering();
}

void dsda_PrepareFinished(void) {
  if (dsda_DoomPrepareFinished())
    return;

  dsda_LegacyPrepareFinished();
}

int dsda_MapLightning(void) {
  int lightning;

  if (dsda_DoomMapLightning(&lightning))
    return lightning;

  dsda_LegacyMapLightning(&lightning);

  return lightning;
}

void dsda_ApplyFadeTable(void) {
  if (dsda_DoomApplyFadeTable())
    return;

  dsda_LegacyApplyFadeTable();
}

int dsda_MapCluster(int map) {
  int cluster;

  if (dsda_DoomMapCluster(&cluster, map))
    return cluster;

  dsda_LegacyMapCluster(&cluster, map);

  return cluster;
}

short dsda_Sky1Texture(void) {
  short texture;

  if (dsda_DoomSky1Texture(&texture))
    return texture;

  dsda_LegacySky1Texture(&texture);

  return texture;
}

short dsda_Sky2Texture(void) {
  short texture;

  if (dsda_DoomSky2Texture(&texture))
    return texture;

  dsda_LegacySky2Texture(&texture);

  return texture;
}

void dsda_InitSky(void) {
  if (dsda_DoomInitSky())
    return;

  dsda_LegacyInitSky();
}

//
// Copyright(C) 2020 by Ryan Krafnick
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
//	DSDA Tools
//

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "doomstat.h"
#include "p_inter.h"
#include "p_tick.h"
#include "g_game.h"
#include "sounds.h"

#include "dsda/args.h"
#include "dsda/features.h"
#include "dsda/mouse.h"
#include "dsda/settings.h"
#include "dsda/split_tracker.h"
#include "dsda/tracker.h"
#include "dsda/wad_stats.h"
#include "dsda.h"

#define TELEFRAG_DAMAGE 10000

// command-line toggles
int dsda_track_pacifist;
int dsda_track_100k;
int dsda_track_reality;

int dsda_last_leveltime;
int dsda_last_gamemap;
int dsda_startmap;
int dsda_movie_target;
dboolean dsda_any_map_completed;

// other
int dsda_max_kill_requirement;
static int dsda_session_attempts = 1;

static int turbo_scale;
static int start_in_build_mode;

static int line_activation[2][LINE_ACTIVATION_INDEX_MAX + 1];
static int line_activation_frame;
static int line_activation_index;

static int dsda_time_keys;
static int dsda_time_use;
static int dsda_time_secrets;

dboolean dsda_IsWeapon(mobj_t* thing);
void dsda_DisplayNotification(const char* msg);
void dsda_ResetMapVariables(void);

dboolean dsda_ILComplete(void) {
  return dsda_any_map_completed && dsda_last_gamemap == dsda_startmap && !dsda_movie_target;
}

dboolean dsda_MovieComplete(void) {
  return dsda_any_map_completed && dsda_last_gamemap == dsda_movie_target && dsda_movie_target;
}

void dsda_WatchLineActivation(line_t* line, mobj_t* mo) {
  if (mo && mo->player) {
    if (line_activation_index < LINE_ACTIVATION_INDEX_MAX) {
      line_activation[line_activation_frame][line_activation_index] = line->iLineID;
      ++line_activation_index;
      line_activation[line_activation_frame][line_activation_index] = -1;
    }

    ++line->player_activations;
  }
}

int* dsda_PlayerActivatedLines(void) {
  return line_activation[!line_activation_frame];
}

static void dsda_FlipLineActivationTracker(void) {
  line_activation_frame = !line_activation_frame;
  line_activation_index = 0;

  line_activation[line_activation_frame][line_activation_index] = -1;
}

static void dsda_ResetLineActivationTracker(void) {
  line_activation[0][0] = -1;
  line_activation[1][0] = -1;
}

static void dsda_HandleTurbo(void) {
  dsda_arg_t* arg;

  arg = dsda_Arg(dsda_arg_turbo);

  if (arg->found)
    turbo_scale = arg->value.v_int;

}

int dsda_TurboScale(void) {
  return turbo_scale;
}

static dboolean frozen_mode;

dboolean dsda_FrozenMode(void) {
  return frozen_mode;
}

void dsda_ToggleFrozenMode(void) {
  frozen_mode = !frozen_mode;
}

static void dsda_HandleBuild(void) {
  start_in_build_mode = 0;
}

int dsda_StartInBuildMode(void) {
  return start_in_build_mode;
}

void dsda_ReadCommandLine(void) {
  dsda_arg_t* arg;
  int dsda_time_all;

  dsda_track_pacifist = dsda_Flag(dsda_arg_track_pacifist);
  dsda_track_100k = dsda_Flag(dsda_arg_track_100k);
  dsda_track_reality = dsda_Flag(dsda_arg_track_reality);
  dsda_time_keys = dsda_SimpleIntArg(dsda_arg_time_keys);
  dsda_time_use = dsda_SimpleIntArg(dsda_arg_time_use);
  dsda_time_secrets = dsda_SimpleIntArg(dsda_arg_time_secrets);
  dsda_time_all = dsda_SimpleIntArg(dsda_arg_time_all);

  if ((arg = dsda_Arg(dsda_arg_movie))->found)
    dsda_movie_target = arg->value.v_int;

  if (dsda_time_all) {
    dsda_time_keys = dsda_time_all;
    dsda_time_use = dsda_time_all;
    dsda_time_secrets = dsda_time_all;
  }

  dsda_HandleTurbo();
  dsda_HandleBuild();

  if (dsda_Flag(dsda_arg_tas) || dsda_Flag(dsda_arg_build)) dsda_SetTas();
}

static int dsda_shown_attempt = 0;

int dsda_SessionAttempts(void) {
  return dsda_session_attempts;
}

void dsda_DecomposeILTime(dsda_level_time_t* level_time) {
  level_time->m = dsda_last_leveltime / 35 / 60;
  level_time->s = (dsda_last_leveltime % (60 * 35)) / 35;
  level_time->t = round(100.f * (dsda_last_leveltime % 35) / 35);
}

void dsda_DecomposeMovieTime(dsda_movie_time_t* total_time) {
  extern int totalleveltimes;

  total_time->h = totalleveltimes / 35 / 60 / 60;
  total_time->m = (totalleveltimes % (60 * 60 * 35)) / 35 / 60;
  total_time->s = (totalleveltimes % (60 * 35)) / 35;
}


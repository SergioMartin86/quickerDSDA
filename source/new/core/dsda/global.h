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
//	DSDA Global - define top level globals for doom vs heretic
//

#ifndef __DSDA_GLOBAL__
#define __DSDA_GLOBAL__

#include "doomtype.h"

extern __thread int g_maxplayers;
extern __thread int g_viewheight;
extern __thread int g_numammo;
extern __thread int g_mt_player;
extern __thread int g_mt_tfog;
extern __thread int g_mt_blood;
extern __thread int g_skullpop_mt;
extern __thread int g_s_bloodyskullx1;
extern __thread int g_s_bloodyskullx2;
extern __thread int g_s_play_fdth20;
extern __thread int g_wp_fist;
extern __thread int g_wp_chainsaw;
extern __thread int g_wp_pistol;
extern __thread int g_telefog_height;
extern __thread int g_thrust_factor;
extern __thread int g_fuzzy_aim_shift;
extern __thread int g_jump;
extern __thread int g_s_null;
extern __thread int g_mt_bloodsplatter;
extern __thread int g_bloodsplatter_shift;
extern __thread int g_bloodsplatter_weight;
extern __thread int g_mons_look_range;
extern __thread int g_hide_state;
extern __thread int g_lava_type;
extern __thread int g_mntr_charge_speed;
extern __thread int g_mntr_atk1_sfx;
extern __thread int g_mntr_decide_range;
extern __thread int g_mntr_charge_rng;
extern __thread int g_mntr_fire_rng;
extern __thread int g_mntr_charge_state;
extern __thread int g_mntr_fire_state;
extern __thread int g_mntr_charge_puff;
extern __thread int g_mntr_atk2_sfx;
extern __thread int g_mntr_atk2_dice;
extern __thread int g_mntr_atk2_missile;
extern __thread int g_mntr_atk3_sfx;
extern __thread int g_mntr_atk3_dice;
extern __thread int g_mntr_atk3_missile;
extern __thread int g_mntr_atk3_state;
extern __thread int g_mntr_fire;
extern __thread int g_arti_health;
extern __thread int g_arti_superhealth;
extern __thread int g_arti_fly;
extern __thread int g_arti_limit;
extern __thread int g_sfx_telept;
extern __thread int g_sfx_sawup;
extern __thread int g_sfx_stnmov;
extern __thread int g_sfx_stnmov_plats;
extern __thread int g_sfx_swtchn;
extern __thread int g_sfx_swtchx;
extern __thread int g_sfx_dorcls;
extern __thread int g_sfx_doropn;
extern __thread int g_sfx_dorlnd;
extern __thread int g_sfx_pstart;
extern __thread int g_sfx_pstop;
extern __thread int g_sfx_itemup;
extern __thread int g_sfx_pistol;
extern __thread int g_sfx_oof;
extern __thread int g_sfx_menu;
extern __thread int g_sfx_respawn;
extern __thread int g_sfx_secret;
extern __thread int g_sfx_revive;
extern __thread int g_sfx_console;
extern __thread int g_door_normal;
extern __thread int g_door_raise_in_5_mins;
extern __thread int g_door_open;
extern __thread int g_st_height;
extern __thread int g_border_offset;
extern __thread int g_mf_translucent;
extern __thread int g_mf_shadow;
extern __thread const char* g_skyflatname;

void dsda_InitGlobal(void);

#endif

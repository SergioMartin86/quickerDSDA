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

extern __STORAGE_MODIFIER int g_maxplayers;
extern __STORAGE_MODIFIER int g_viewheight;
extern __STORAGE_MODIFIER int g_numammo;

extern __STORAGE_MODIFIER int g_mt_player;
extern __STORAGE_MODIFIER int g_mt_tfog;
extern __STORAGE_MODIFIER int g_mt_blood;
extern __STORAGE_MODIFIER int g_skullpop_mt;
extern __STORAGE_MODIFIER int g_s_bloodyskullx1;
extern __STORAGE_MODIFIER int g_s_bloodyskullx2;
extern __STORAGE_MODIFIER int g_s_play_fdth20;

extern __STORAGE_MODIFIER int g_wp_fist;
extern __STORAGE_MODIFIER int g_wp_chainsaw;
extern __STORAGE_MODIFIER int g_wp_pistol;

extern __STORAGE_MODIFIER int g_telefog_height;
extern __STORAGE_MODIFIER int g_thrust_factor;
extern __STORAGE_MODIFIER int g_fuzzy_aim_shift;
extern __STORAGE_MODIFIER int g_jump;

extern __STORAGE_MODIFIER int g_s_null;

extern __STORAGE_MODIFIER int g_mt_bloodsplatter;
extern __STORAGE_MODIFIER int g_bloodsplatter_shift;
extern __STORAGE_MODIFIER int g_bloodsplatter_weight;
extern __STORAGE_MODIFIER int g_mons_look_range;
extern __STORAGE_MODIFIER int g_hide_state;
extern __STORAGE_MODIFIER int g_lava_type;

extern __STORAGE_MODIFIER int g_mntr_charge_speed;
extern __STORAGE_MODIFIER int g_mntr_atk1_sfx;
extern __STORAGE_MODIFIER int g_mntr_decide_range;
extern __STORAGE_MODIFIER int g_mntr_charge_rng;
extern __STORAGE_MODIFIER int g_mntr_fire_rng;
extern __STORAGE_MODIFIER int g_mntr_charge_state;
extern __STORAGE_MODIFIER int g_mntr_fire_state;
extern __STORAGE_MODIFIER int g_mntr_charge_puff;
extern __STORAGE_MODIFIER int g_mntr_atk2_sfx;
extern __STORAGE_MODIFIER int g_mntr_atk2_dice;
extern __STORAGE_MODIFIER int g_mntr_atk2_missile;
extern __STORAGE_MODIFIER int g_mntr_atk3_sfx;
extern __STORAGE_MODIFIER int g_mntr_atk3_dice;
extern __STORAGE_MODIFIER int g_mntr_atk3_missile;
extern __STORAGE_MODIFIER int g_mntr_atk3_state;
extern __STORAGE_MODIFIER int g_mntr_fire;

extern __STORAGE_MODIFIER int g_arti_health;
extern __STORAGE_MODIFIER int g_arti_superhealth;
extern __STORAGE_MODIFIER int g_arti_fly;
extern __STORAGE_MODIFIER int g_arti_limit;

extern __STORAGE_MODIFIER int g_sfx_telept;
extern __STORAGE_MODIFIER int g_sfx_sawup;
extern __STORAGE_MODIFIER int g_sfx_stnmov;
extern __STORAGE_MODIFIER int g_sfx_stnmov_plats;
extern __STORAGE_MODIFIER int g_sfx_swtchn;
extern __STORAGE_MODIFIER int g_sfx_swtchx;
extern __STORAGE_MODIFIER int g_sfx_dorcls;
extern __STORAGE_MODIFIER int g_sfx_doropn;
extern __STORAGE_MODIFIER int g_sfx_dorlnd;
extern __STORAGE_MODIFIER int g_sfx_pstart;
extern __STORAGE_MODIFIER int g_sfx_pstop;
extern __STORAGE_MODIFIER int g_sfx_itemup;
extern __STORAGE_MODIFIER int g_sfx_pistol;
extern __STORAGE_MODIFIER int g_sfx_oof;
extern __STORAGE_MODIFIER int g_sfx_menu;
extern __STORAGE_MODIFIER int g_sfx_respawn;
extern __STORAGE_MODIFIER int g_sfx_secret;
extern __STORAGE_MODIFIER int g_sfx_revive;
extern __STORAGE_MODIFIER int g_sfx_console;

extern __STORAGE_MODIFIER int g_door_normal;
extern __STORAGE_MODIFIER int g_door_raise_in_5_mins;
extern __STORAGE_MODIFIER int g_door_open;

extern __STORAGE_MODIFIER int g_st_height;
extern __STORAGE_MODIFIER int g_border_offset;
extern __STORAGE_MODIFIER int g_mf_translucent;
extern __STORAGE_MODIFIER int g_mf_shadow;

extern const char* g_skyflatname;

extern __STORAGE_MODIFIER dboolean heretic;

void dsda_InitGlobal(void);

#endif

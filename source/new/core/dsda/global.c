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

#include <stdlib.h>
#include <string.h>

#include "info.h"
#include "d_items.h"
#include "p_inter.h"
#include "p_spec.h"
#include "p_map.h"
#include "sounds.h"
#include "d_main.h"
#include "v_video.h"

#include "global.h"

#include "dsda/args.h"
#include "dsda/map_format.h"
#include "dsda/mobjinfo.h"
#include "dsda/sprite.h"
#include "dsda/state.h"

#define IGNORE_VALUE -1

const __thread demostate_t (*demostates)[4];
extern __thread const demostate_t doom_demostates[][4];
extern __thread const demostate_t heretic_demostates[][4];
extern __thread const demostate_t hexen_demostates[][4];

__thread weaponinfo_t* weaponinfo;

__thread int g_maxplayers = 4;
__thread int g_viewheight = 41 * FRACUNIT;
__thread int g_numammo;
__thread int g_mt_player;
__thread int g_mt_tfog;
__thread int g_mt_blood;
__thread int g_skullpop_mt;
__thread int g_s_bloodyskullx1;
__thread int g_s_bloodyskullx2;
__thread int g_s_play_fdth20;
__thread int g_wp_fist;
__thread int g_wp_chainsaw;
__thread int g_wp_pistol;
__thread int g_telefog_height;
__thread int g_thrust_factor;
__thread int g_fuzzy_aim_shift;
__thread int g_jump;
__thread int g_s_null;
__thread int g_mt_bloodsplatter;
__thread int g_bloodsplatter_shift;
__thread int g_bloodsplatter_weight;
__thread int g_mons_look_range;
__thread int g_hide_state;
__thread int g_lava_type;
__thread int g_mntr_charge_speed;
__thread int g_mntr_atk1_sfx;
__thread int g_mntr_decide_range;
__thread int g_mntr_charge_rng;
__thread int g_mntr_fire_rng;
__thread int g_mntr_charge_state;
__thread int g_mntr_fire_state;
__thread int g_mntr_charge_puff;
__thread int g_mntr_atk2_sfx;
__thread int g_mntr_atk2_dice;
__thread int g_mntr_atk2_missile;
__thread int g_mntr_atk3_sfx;
__thread int g_mntr_atk3_dice;
__thread int g_mntr_atk3_missile;
__thread int g_mntr_atk3_state;
__thread int g_mntr_fire;
__thread int g_arti_health;
__thread int g_arti_superhealth;
__thread int g_arti_fly;
__thread int g_arti_limit;
__thread int g_sfx_sawup;
__thread int g_sfx_telept;
__thread int g_sfx_stnmov;
__thread int g_sfx_stnmov_plats;
__thread int g_sfx_swtchn;
__thread int g_sfx_swtchx;
__thread int g_sfx_dorcls;
__thread int g_sfx_doropn;
__thread int g_sfx_dorlnd;
__thread int g_sfx_pstart;
__thread int g_sfx_pstop;
__thread int g_sfx_itemup;
__thread int g_sfx_pistol;
__thread int g_sfx_oof;
__thread int g_sfx_menu;
__thread int g_sfx_respawn;
__thread int g_sfx_secret;
__thread int g_sfx_revive;
__thread int g_sfx_console;
__thread int g_door_normal;
__thread int g_door_raise_in_5_mins;
__thread int g_door_open;
__thread int g_st_height;
__thread int g_border_offset;
__thread int g_mf_translucent;
__thread int g_mf_shadow;
__thread const char* g_menu_flat;
__thread int g_menu_save_page_size;
__thread int g_menu_font_spacing;
__thread const char* g_skyflatname;
__thread dboolean hexen = false;

static void dsda_InitDoom(void) {
  int i;
  doom_mobjinfo_t* mobjinfo_p;

  dsda_InitializeMobjInfo(DOOM_MT_ZERO, DOOM_NUMMOBJTYPES, DOOM_NUMMOBJTYPES);
  dsda_InitializeStates(doom_states, DOOM_NUMSTATES);
  dsda_InitializeSprites(doom_sprnames, DOOM_NUMSPRITES);

  demostates = doom_demostates;

  weaponinfo = doom_weaponinfo;

  g_maxplayers = 4;
  g_viewheight = 41 * FRACUNIT;
  g_numammo = DOOM_NUMAMMO;

  g_mt_player = MT_PLAYER;
  g_mt_tfog = MT_TFOG;
  g_mt_blood = MT_BLOOD;
  g_skullpop_mt = MT_NULL;

  g_wp_fist = wp_fist;
  g_wp_chainsaw = wp_chainsaw;
  g_wp_pistol = wp_pistol;

  g_telefog_height = 0;
  g_thrust_factor = 100;
  g_fuzzy_aim_shift = 20;
  g_jump = 8;

  g_s_null = S_NULL;

  g_sfx_sawup = sfx_sawup;
  g_sfx_telept = sfx_telept;
  g_sfx_stnmov = sfx_stnmov;
  g_sfx_stnmov_plats = sfx_stnmov;
  g_sfx_swtchn = sfx_swtchn;
  g_sfx_swtchx = sfx_swtchx;
  g_sfx_dorcls = sfx_dorcls;
  g_sfx_doropn = sfx_doropn;
  g_sfx_dorlnd = sfx_dorcls;
  g_sfx_pstart = sfx_pstart;
  g_sfx_pstop = sfx_pstop;
  g_sfx_itemup = sfx_itemup;
  g_sfx_pistol = sfx_pistol;
  g_sfx_oof = sfx_oof;
  g_sfx_menu = sfx_pstop;
  g_sfx_secret = sfx_secret;
  g_sfx_revive = sfx_slop;
  g_sfx_console = sfx_radio;

  g_door_normal = normal;
  g_door_raise_in_5_mins = waitRaiseDoor;
  g_door_open = openDoor;

  g_st_height = 32;
  g_border_offset = 8;
  g_mf_translucent = MF_TRANSLUCENT;
  g_mf_shadow = MF_SHADOW;

  g_menu_flat = "FLOOR4_6";
  g_menu_save_page_size = 7;
  g_menu_font_spacing = -1;

  g_skyflatname = "F_SKY1";

  // convert doom mobj types to shared type
  for (i = 0; i < DOOM_NUMMOBJTYPES; ++i) {
    mobjinfo_p = &doom_mobjinfo[i];

    mobjinfo[i].doomednum    = mobjinfo_p->doomednum;
    mobjinfo[i].spawnstate   = mobjinfo_p->spawnstate;
    mobjinfo[i].spawnhealth  = mobjinfo_p->spawnhealth;
    mobjinfo[i].seestate     = mobjinfo_p->seestate;
    mobjinfo[i].seesound     = mobjinfo_p->seesound;
    mobjinfo[i].reactiontime = mobjinfo_p->reactiontime;
    mobjinfo[i].attacksound  = mobjinfo_p->attacksound;
    mobjinfo[i].painstate    = mobjinfo_p->painstate;
    mobjinfo[i].painchance   = mobjinfo_p->painchance;
    mobjinfo[i].painsound    = mobjinfo_p->painsound;
    mobjinfo[i].meleestate   = mobjinfo_p->meleestate;
    mobjinfo[i].missilestate = mobjinfo_p->missilestate;
    mobjinfo[i].deathstate   = mobjinfo_p->deathstate;
    mobjinfo[i].xdeathstate  = mobjinfo_p->xdeathstate;
    mobjinfo[i].deathsound   = mobjinfo_p->deathsound;
    mobjinfo[i].speed        = mobjinfo_p->speed;
    mobjinfo[i].radius       = mobjinfo_p->radius;
    mobjinfo[i].height       = mobjinfo_p->height;
    mobjinfo[i].mass         = mobjinfo_p->mass;
    mobjinfo[i].damage       = mobjinfo_p->damage;
    mobjinfo[i].activesound  = mobjinfo_p->activesound;
    mobjinfo[i].flags        = mobjinfo_p->flags;
    mobjinfo[i].raisestate   = mobjinfo_p->raisestate;
    mobjinfo[i].droppeditem  = MT_NULL;
    mobjinfo[i].crashstate   = 0; // not in doom
    mobjinfo[i].flags2       = 0; // not in doom

    // mbf21
    mobjinfo[i].infighting_group = IG_DEFAULT;
    mobjinfo[i].projectile_group = PG_DEFAULT;
    mobjinfo[i].splash_group = SG_DEFAULT;
    mobjinfo[i].ripsound = sfx_None;
    mobjinfo[i].altspeed = NO_ALTSPEED;
    mobjinfo[i].meleerange = MELEERANGE;

    // misc
    mobjinfo[i].bloodcolor = 0; // default
    mobjinfo[i].visibility = VF_DOOM;
  }

  // don't want to reorganize info.c structure for a few tweaks...
  mobjinfo[MT_WOLFSS].droppeditem    = MT_CLIP;
  mobjinfo[MT_POSSESSED].droppeditem = MT_CLIP;
  mobjinfo[MT_SHOTGUY].droppeditem   = MT_SHOTGUN;
  mobjinfo[MT_CHAINGUY].droppeditem  = MT_CHAINGUN;

  mobjinfo[MT_VILE].flags2    = MF2_SHORTMRANGE | MF2_DMGIGNORED | MF2_NOTHRESHOLD;
  mobjinfo[MT_CYBORG].flags2  = MF2_NORADIUSDMG | MF2_HIGHERMPROB | MF2_RANGEHALF |
                                MF2_FULLVOLSOUNDS | MF2_E2M8BOSS | MF2_E4M6BOSS;
  mobjinfo[MT_SPIDER].flags2  = MF2_NORADIUSDMG | MF2_RANGEHALF | MF2_FULLVOLSOUNDS |
                                MF2_E3M8BOSS | MF2_E4M8BOSS;
  mobjinfo[MT_SKULL].flags2   = MF2_RANGEHALF;
  mobjinfo[MT_FATSO].flags2   = MF2_MAP07BOSS1;
  mobjinfo[MT_BABY].flags2    = MF2_MAP07BOSS2;
  mobjinfo[MT_BRUISER].flags2 = MF2_E1M8BOSS;
  mobjinfo[MT_UNDEAD].flags2  = MF2_LONGMELEE | MF2_RANGEHALF;

  mobjinfo[MT_BRUISER].projectile_group = PG_BARON;
  mobjinfo[MT_KNIGHT].projectile_group = PG_BARON;

  mobjinfo[MT_BRUISERSHOT].altspeed = 20 * FRACUNIT;
  mobjinfo[MT_HEADSHOT].altspeed = 20 * FRACUNIT;
  mobjinfo[MT_TROOPSHOT].altspeed = 20 * FRACUNIT;

  for (i = S_SARG_RUN1; i <= S_SARG_PAIN2; ++i)
    states[i].flags |= STATEF_SKILL5FAST;
}

extern void dsda_ResetNullPClass(void);

void dsda_InitGlobal(void) {
    dsda_InitDoom();

  dsda_ResetNullPClass();
}

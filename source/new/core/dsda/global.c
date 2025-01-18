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

const demostate_t (*demostates)[4];
extern const demostate_t doom_demostates[][4];
extern const demostate_t heretic_demostates[][4];
extern const demostate_t hexen_demostates[][4];

weaponinfo_t* weaponinfo;

int g_maxplayers = 4;
int g_viewheight = 41 * FRACUNIT;
int g_numammo;

int g_mt_player;
int g_mt_tfog;
int g_mt_blood;
int g_skullpop_mt;
int g_s_bloodyskullx1;
int g_s_bloodyskullx2;
int g_s_play_fdth20;

int g_wp_fist;
int g_wp_chainsaw;
int g_wp_pistol;

int g_telefog_height;
int g_thrust_factor;
int g_fuzzy_aim_shift;
int g_jump;

int g_s_null;

int g_mt_bloodsplatter;
int g_bloodsplatter_shift;
int g_bloodsplatter_weight;
int g_mons_look_range;
int g_hide_state;
int g_lava_type;

int g_mntr_charge_speed;
int g_mntr_atk1_sfx;
int g_mntr_decide_range;
int g_mntr_charge_rng;
int g_mntr_fire_rng;
int g_mntr_charge_state;
int g_mntr_fire_state;
int g_mntr_charge_puff;
int g_mntr_atk2_sfx;
int g_mntr_atk2_dice;
int g_mntr_atk2_missile;
int g_mntr_atk3_sfx;
int g_mntr_atk3_dice;
int g_mntr_atk3_missile;
int g_mntr_atk3_state;
int g_mntr_fire;

int g_arti_health;
int g_arti_superhealth;
int g_arti_fly;
int g_arti_limit;

int g_sfx_sawup;
int g_sfx_telept;
int g_sfx_stnmov;
int g_sfx_stnmov_plats;
int g_sfx_swtchn;
int g_sfx_swtchx;
int g_sfx_dorcls;
int g_sfx_doropn;
int g_sfx_dorlnd;
int g_sfx_pstart;
int g_sfx_pstop;
int g_sfx_itemup;
int g_sfx_pistol;
int g_sfx_oof;
int g_sfx_menu;
int g_sfx_respawn;
int g_sfx_secret;
int g_sfx_revive;
int g_sfx_console;

int g_door_normal;
int g_door_raise_in_5_mins;
int g_door_open;

int g_st_height;
int g_border_offset;
int g_mf_translucent;
int g_mf_shadow;

const char* g_menu_flat;
int g_menu_save_page_size;
int g_menu_font_spacing;

const char* g_skyflatname;

dboolean hexen = false;

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
uint32_t* headlessGetPallette() { return (uint32_t*) NULL; }
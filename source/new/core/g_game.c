/* Emacs style mode select   -*- C -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2004 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:  none
 *  The original Doom description was none, basically because this file
 *  has everything. This ties up the game logic, linking the menu and
 *  input code to the underlying game by creating & respawning players,
 *  building game tics, calling the underlying thing logic.
 *
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomstat.h"
#include "d_net.h"
#include "f_finale.h"
#include "m_file.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "p_map.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "w_wad.h"
#include "r_main.h"
#include "p_map.h"
#include "sounds.h"
#include "r_data.h"
#include "p_inter.h"
#include "g_game.h"
#include "lprintf.h"
#include "i_main.h"
#include "i_system.h"
#include "e6y.h"//e6y

#include "dsda.h"
#include "dsda/aim.h"
#include "dsda/args.h"
#include "dsda/configuration.h"
#include "dsda/excmd.h"
#include "dsda/features.h"
#include "dsda/mapinfo.h"
#include "dsda/save.h"
#include "dsda/settings.h"
#include "dsda/input.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/mouse.h"
#include "dsda/options.h"
#include "dsda/skill_info.h"
#include "dsda/utility.h"

// Allows use of HELP2 screen for PWADs under DOOM 1
__thread int pwad_help2_check;

struct
{
    int type;   // mobjtype_t
    int speed[2];
} MonsterMissileInfo[] = {
    { HERETIC_MT_IMPBALL, { 10, 20 } },
    { HERETIC_MT_MUMMYFX1, { 9, 18 } },
    { HERETIC_MT_KNIGHTAXE, { 9, 18 } },
    { HERETIC_MT_REDAXE, { 9, 18 } },
    { HERETIC_MT_BEASTBALL, { 12, 20 } },
    { HERETIC_MT_WIZFX1, { 18, 24 } },
    { HERETIC_MT_SNAKEPRO_A, { 14, 20 } },
    { HERETIC_MT_SNAKEPRO_B, { 14, 20 } },
    { HERETIC_MT_HEADFX1, { 13, 20 } },
    { HERETIC_MT_HEADFX3, { 10, 18 } },
    { HERETIC_MT_MNTRFX1, { 20, 26 } },
    { HERETIC_MT_MNTRFX2, { 14, 20 } },
    { HERETIC_MT_SRCRFX1, { 20, 28 } },
    { HERETIC_MT_SOR2FX1, { 20, 28 } },
    { -1, { -1, -1 } }                 // Terminator
};

// e6y
// It is signature for new savegame format with continuous numbering.
// Now it is not necessary to add a new level of compatibility in case
// of need to savegame format change from one minor version to another.
// The old format is still supported.
#define NEWFORMATSIG "\xff\xff\xff\xff"

static __thread const byte *demobuffer;   /* cph - only used for playback */
static __thread int demolength; // check for overrun (missing DEMOMARKER)

__thread dboolean        preventLevelExit;
__thread dboolean        preventGameEnd;
__thread dboolean        reachedLevelExit;
__thread dboolean        reachedGameEnd;
__thread gameaction_t    gameaction;
__thread gamestate_t     gamestate;
__thread dboolean        in_game;
__thread int             gameskill;
__thread int             gameepisode;
 __thread int             gamemap;
 static __thread dboolean forced_loadgame = false;
 static __thread dboolean load_via_cmd = false;
__thread dboolean         timingdemo;    // if true, exit with report on completion
__thread dboolean         fastdemo;      // if true, run at full speed -- killough
__thread dboolean         nodrawers;     // for comparative timing purposes
__thread int             starttime;     // for comparative timing purposes
__thread dboolean         deathmatch;    // only if started as net death
__thread dboolean         netgame;       // only true if packets are broadcast
__thread dboolean         playeringame[MAX_MAXPLAYERS];
__thread player_t        players[MAX_MAXPLAYERS];
__thread int             upmove;
__thread int             consoleplayer; // player taking events and displaying
__thread int             displayplayer; // view being displayed
__thread int             gametic;
__thread int             boom_basetic;       /* killough 9/29/98: for demo sync */
__thread int             true_basetic;
__thread int             totalkills, totallive, totalitems, totalsecret;    // for intermission
__thread dboolean         demorecording;
__thread wbstartstruct_t wminfo;               // parms for world map / intermission
__thread dboolean         haswolflevels = false;// jff 4/18/98 wolf levels present
__thread int             totalleveltimes;      // CPhipps - total time for all completed levels
__thread int             levels_completed;
__thread int             longtics;
__thread dboolean coop_spawns;
__thread int shorttics;
static __thread int     turnheld;       // for accelerative turning
static __thread int next_weapon = 0;

//
// controls (have defaults)
//

#define MAXPLMOVE   (forwardmove[1])
#define SLOWTURNTICS  6
#define QUICKREVERSE (short)32768 // 180 degree reverse                    // phares

fixed_t forwardmove[2] = {0x19, 0x32};
fixed_t sidemove[2]    = {0x18, 0x28};
fixed_t angleturn[3]   = {640, 1280, 320};  // + slow turn
fixed_t flyspeed[2]    = {1*256, 3*256};


static __thread const struct
{
  weapontype_t weapon;
  weapontype_t weapon_num;
} weapon_order_table[] = {
  { wp_fist,         wp_fist },
  { wp_chainsaw,     wp_fist },
  { wp_pistol,       wp_pistol },
  { wp_shotgun,      wp_shotgun },
  { wp_supershotgun, wp_shotgun },
  { wp_chaingun,     wp_chaingun },
  { wp_missile,      wp_missile },
  { wp_plasma,       wp_plasma },
  { wp_bfg,          wp_bfg }
};

// HERETIC_TODO: dynamically set these
// static const struct
// {
//     weapontype_t weapon;
//     weapontype_t weapon_num;
// } heretic_weapon_order_table[] = {
//     { wp_staff,       wp_staff },
//     { wp_gauntlets,   wp_staff },
//     { wp_goldwand,    wp_goldwand },
//     { wp_crossbow,    wp_crossbow },
//     { wp_blaster,     wp_blaster },
//     { wp_skullrod,    wp_skullrod },
//     { wp_phoenixrod,  wp_phoenixrod },
//     { wp_mace,        wp_mace },
//     { wp_beak,        wp_beak },
// };

// mouse values are used once
static __thread  int   mousex;
static __thread  int   mousey;
static __thread  int   dclicktime;
static __thread  int   dclickstate;
static __thread  int   dclicks;
static __thread  int   dclicktime2;
static __thread  int   dclickstate2;
static __thread  int   dclicks2;
static __thread  int left_analog_x;
static __thread  int left_analog_y;

// Game events info
static __thread buttoncode_t special_event; // Event triggered by local player, to send
static __thread int   savegameslot;         // Slot to load if gameaction == ga_loadgame
__thread char         savedescription[SAVEDESCLEN];  // Description to save in savegame if gameaction == ga_savegame

// heretic
#include "p_user.h"

__thread int lookheld;

static dboolean InventoryMoveLeft(void);
static dboolean InventoryMoveRight(void);
// end heretic

// hexen

// Position indicator for cooperative net-play reborn
__thread int RebornPosition;

__thread leave_data_t leave_data;

void G_DoTeleportNewMap(void);
static void Hexen_G_DoReborn(int playernum);
// end hexen

typedef enum
{
  carry_vertmouse,
  carry_mousex,
  carry_mousey,
  NUMDOUBLECARRY
} double_carry_t;

static __thread double double_carry[NUMDOUBLECARRY];

static int G_CarryDouble(double_carry_t c, double value)
{
  int truncated_result;
  double true_result;

  true_result = double_carry[c] + value;
  truncated_result = (int) true_result;
  double_carry[c] = true_result - truncated_result;

  return truncated_result;
}

static void G_DoSaveGame(dboolean via_cmd);

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
static inline signed char fudgef(signed char b)
{
/*e6y
  static int c;
  if (!b || !demo_compatibility || longtics) return b;
  if (++c & 0x1f) return b;
  b |= 1; if (b>2) b-=2;*/
  return b;
}

void G_SetSpeed(dboolean reset)
{
  dsda_pclass_t *player_class;
  static dsda_pclass_t *last_player_class = NULL;

  player_class = &pclass;

  if (last_player_class == player_class && !reset)
    return;

  last_player_class = player_class;

  forwardmove[0] = player_class->forwardmove[0];
  forwardmove[1] = player_class->forwardmove[1];

  if(dsda_AlwaysSR50())
  {
    sidemove[0] = player_class->forwardmove[0];
    sidemove[1] = player_class->forwardmove[1];
  }
  else
  {
    sidemove[0] = player_class->sidemove[0];
    sidemove[1] = player_class->sidemove[1];
  }

  {
    int turbo_scale = dsda_TurboScale();

    if (turbo_scale)
    {
      forwardmove[0] = player_class->forwardmove[0] * turbo_scale / 100;
      forwardmove[1] = player_class->forwardmove[1] * turbo_scale / 100;
      sidemove[0] = sidemove[0] * turbo_scale / 100;
      sidemove[1] = sidemove[1] * turbo_scale / 100;

      if (forwardmove[0] > 127)
        forwardmove[0] = 127;

      if (forwardmove[1] > 127)
        forwardmove[1] = 127;

      if (sidemove[0] > 127)
        sidemove[0] = 127;

      if (sidemove[1] > 127)
        sidemove[1] = 127;
    }
  }
}

static dboolean WeaponSelectable(weapontype_t weapon)
{
  if (gamemode == shareware)
  {
    if (weapon == wp_plasma || weapon == wp_bfg)
      return false;
  }

  // Can't select the super shotgun in Doom 1.
  if (weapon == wp_supershotgun && gamemission == doom)
  {
    return false;
  }

  // Can't select a weapon if we don't own it.
  if (!players[consoleplayer].weaponowned[weapon])
  {
    return false;
  }

  return true;
}

static int G_NextWeapon(int direction)
{
  weapontype_t weapon;
  int start_i, i, arrlen;

  // Find index in the table.
  if (players[consoleplayer].pendingweapon == wp_nochange)
  {
    weapon = players[consoleplayer].readyweapon;
  }
  else
  {
    weapon = players[consoleplayer].pendingweapon;
  }

  arrlen = sizeof(weapon_order_table) / sizeof(*weapon_order_table);
  for (i = 0; i < arrlen; i++)
  {
    if (weapon_order_table[i].weapon == weapon)
    {
      break;
    }
  }

  // Switch weapon. Don't loop forever.
  start_i = i;
  do
  {
    i += direction;
    i = (i + arrlen) % arrlen;
  }
  while (i != start_i && !WeaponSelectable(weapon_order_table[i].weapon));

  return weapon_order_table[i].weapon_num;
}

static __thread double mouse_sensitivity_horiz;
static __thread double mouse_sensitivity_vert;
static __thread double mouse_sensitivity_mlook;
static __thread double mouse_strafe_divisor;

void G_UpdateMouseSensitivity(void)
{
  double horizontal_sensitivity, fine_sensitivity;

  horizontal_sensitivity = dsda_IntConfig(dsda_config_mouse_sensitivity_horiz);
  fine_sensitivity = dsda_IntConfig(dsda_config_fine_sensitivity);

  mouse_sensitivity_horiz = horizontal_sensitivity + fine_sensitivity / 100;
  mouse_sensitivity_vert = dsda_IntConfig(dsda_config_mouse_sensitivity_vert);
  mouse_sensitivity_mlook = dsda_IntConfig(dsda_config_mouse_sensitivity_mlook);
  mouse_strafe_divisor = dsda_IntConfig(dsda_config_movement_mousestrafedivisor);
}

void G_ResetMotion(void)
{
  mousex = mousey = 0;
  left_analog_x = left_analog_y = 0;
}

static void G_ConvertAnalogMotion(int speed, int *forward, int *side)
{
  if (left_analog_x || left_analog_y)
  {
    if (left_analog_x > sidemove[speed])
      left_analog_x = sidemove[speed];
    else if (left_analog_x < -sidemove[speed])
      left_analog_x = -sidemove[speed];

    if (left_analog_y > forwardmove[speed])
      left_analog_y = forwardmove[speed];
    else if (left_analog_y < -forwardmove[speed])
      left_analog_y = -forwardmove[speed];

    *forward += left_analog_y;
    *side += left_analog_x;
  }
}

void G_BuildTiccmd(ticcmd_t* cmd)
{
  int strafe;
  int bstrafe;
  int speed;
  int tspeed;
  int forward;
  int side;
  int newweapon;                                          // phares
  dboolean strict_input;

  dsda_pclass_t *player_class = &pclass;

  strict_input = dsda_StrictMode();

  G_SetSpeed(false);

  memset(cmd, 0, sizeof(*cmd));

  if (demorecording)
  {
    G_ResetMotion();
    return;
  }

  strafe = dsda_InputActive(dsda_input_strafe);
  //e6y: the "RUN" key inverts the autorun state
  speed = (dsda_InputActive(dsda_input_speed) ? !dsda_AutoRun() : dsda_AutoRun()); // phares

  forward = side = 0;

  // use two stage accelerative turning on the keyboard
  if (dsda_InputActive(dsda_input_turnright) || dsda_InputActive(dsda_input_turnleft))
    ++turnheld;
  else
    turnheld = 0;

  if (turnheld < SLOWTURNTICS)
    tspeed = 2;             // slow turn
  else
    tspeed = speed;

  // turn 180 degrees in one keystroke?
  if (dsda_InputTickActivated(dsda_input_reverse))
  {
    if (!strafe) {
      if (strict_input) {}
      else
        cmd->angleturn += QUICKREVERSE;
    }
  }

  // let movement keys cancel each other out

  if (strafe)
  {
    if (dsda_InputActive(dsda_input_turnright))
      side += sidemove[speed];
    if (dsda_InputActive(dsda_input_turnleft))
      side -= sidemove[speed];
  }
  else
  {
    if (dsda_InputActive(dsda_input_turnright))
      cmd->angleturn -= angleturn[tspeed];
    if (dsda_InputActive(dsda_input_turnleft))
      cmd->angleturn += angleturn[tspeed];
  }

  if (dsda_InputActive(dsda_input_forward))
    forward += forwardmove[speed];
  if (dsda_InputActive(dsda_input_backward))
    forward -= forwardmove[speed];
  if (dsda_InputActive(dsda_input_straferight))
    side += sidemove[speed];
  if (dsda_InputActive(dsda_input_strafeleft))
    side -= sidemove[speed];

  if (dsda_AllowJumping())
  {
    if ( dsda_InputActive(dsda_input_jump))
    {
      dsda_QueueExCmdJump();
    }
  }

  if (players[consoleplayer].mo && players[consoleplayer].mo->pitch && !dsda_MouseLook())
    dsda_QueueExCmdLook(XC_LOOK_RESET);

  if (dsda_InputActive(dsda_input_fire))
    cmd->buttons |= BT_ATTACK;

  if (dsda_InputActive(dsda_input_use) || dsda_InputTickActivated(dsda_input_use))
  {
    cmd->buttons |= BT_USE;
    // clear double clicks if hit use button
    dclicks = 0;
  }

  {
    extern dboolean boom_weapon_state_injection;
    static dboolean done_autoswitch = false;

    if (!players[consoleplayer].attackdown)
    {
      done_autoswitch = false;
    }

    // Toggle between the top 2 favorite weapons.                   // phares
    // If not currently aiming one of these, switch to              // phares
    // the favorite. Only switch if you possess the weapon.         // phares

    // killough 3/22/98:
    //
    // Perform automatic weapons switch here rather than in p_pspr.c,
    // except in demo_compatibility mode.
    //
    // killough 3/26/98, 4/2/98: fix autoswitch when no weapons are left

    // Make Boom insert only a single weapon change command on autoswitch.
    if (
      (
        !demo_compatibility &&
        players[consoleplayer].attackdown && // killough
        !P_CheckAmmo(&players[consoleplayer]) &&
        (
          (
            (
              dsda_SwitchWhenAmmoRunsOut() ||
              boom_weapon_state_injection
            ) &&
            !done_autoswitch
          ) || (
            cmd->buttons & BT_ATTACK &&
            players[consoleplayer].pendingweapon == wp_nochange
          )
        )
      ) || (dsda_InputActive(dsda_input_toggleweapon))
    )
    {
      done_autoswitch = true;
      boom_weapon_state_injection = false;
      newweapon = P_SwitchWeapon(&players[consoleplayer]);           // phares
    }
    else
    {                                 // phares 02/26/98: Added gamemode checks
      if (next_weapon && players[consoleplayer].morphTics == 0)
      {
        newweapon = G_NextWeapon(next_weapon);
      }
      else
      {
        // HERETIC_TODO: fix this
        newweapon =
          dsda_InputActive(dsda_input_weapon1) ? wp_fist :    // killough 5/2/98: reformatted
          dsda_InputActive(dsda_input_weapon2) ? wp_pistol :
          dsda_InputActive(dsda_input_weapon3) ? wp_shotgun :
          dsda_InputActive(dsda_input_weapon4) ? wp_chaingun :
          dsda_InputActive(dsda_input_weapon5) ? wp_missile :
          dsda_InputActive(dsda_input_weapon6) && gamemode != shareware ? wp_plasma :
          dsda_InputActive(dsda_input_weapon7) && gamemode != shareware ? wp_bfg :
          dsda_InputActive(dsda_input_weapon8) ? wp_chainsaw :
          (!demo_compatibility && dsda_InputActive(dsda_input_weapon9) && gamemode == commercial) ? wp_supershotgun :
          wp_nochange;
      }

      // killough 3/22/98: For network and demo consistency with the
      // new weapons preferences, we must do the weapons switches here
      // instead of in p_user.c. But for old demos we must do it in
      // p_user.c according to the old rules. Therefore demo_compatibility
      // determines where the weapons switch is made.

      // killough 2/8/98:
      // Allow user to switch to fist even if they have chainsaw.
      // Switch to fist or chainsaw based on preferences.
      // Switch to shotgun or SSG based on preferences.

      if (!demo_compatibility)
      {
        const player_t *player = &players[consoleplayer];

        // only select chainsaw from '1' if it's owned, it's
        // not already in use, and the player prefers it or
        // the fist is already in use, or the player does not
        // have the berserker strength.

        if (newweapon==wp_fist && player->weaponowned[wp_chainsaw] &&
            player->readyweapon!=wp_chainsaw &&
            (player->readyweapon==wp_fist ||
             !player->powers[pw_strength] ||
             P_WeaponPreferred(wp_chainsaw, wp_fist)))
          newweapon = wp_chainsaw;

        // Select SSG from '3' only if it's owned and the player
        // does not have a shotgun, or if the shotgun is already
        // in use, or if the SSG is not already in use and the
        // player prefers it.

        if (newweapon == wp_shotgun && gamemode == commercial &&
            player->weaponowned[wp_supershotgun] &&
            (!player->weaponowned[wp_shotgun] ||
             player->readyweapon == wp_shotgun ||
             (player->readyweapon != wp_supershotgun &&
              P_WeaponPreferred(wp_supershotgun, wp_shotgun))))
          newweapon = wp_supershotgun;
      }
    }
  }

  next_weapon = 0;

  if (newweapon != wp_nochange && players[consoleplayer].chickenTics == 0)
  {
    cmd->buttons |= BT_CHANGE;
    cmd->buttons |= newweapon<<BT_WEAPONSHIFT;
  }

  // mouse

  if (dsda_IntConfig(dsda_config_mouse_doubleclick_as_use)) {//e6y
    // forward double click
    if (dsda_InputMouseBActive(dsda_input_forward) != dclickstate && dclicktime > 1 )
    {
      dclickstate = dsda_InputMouseBActive(dsda_input_forward);
      if (dclickstate)
        dclicks++;
      if (dclicks == 2)
        {
          cmd->buttons |= BT_USE;
          dclicks = 0;
        }
      else
        dclicktime = 0;
    }
    else
      if (++dclicktime > 20)
      {
        dclicks = 0;
        dclickstate = 0;
      }

    // strafe double click
    bstrafe = dsda_InputMouseBActive(dsda_input_strafe) || dsda_InputJoyBActive(dsda_input_strafe);
    if (bstrafe != dclickstate2 && dclicktime2 > 1 )
    {
      dclickstate2 = bstrafe;
      if (dclickstate2)
        dclicks2++;
      if (dclicks2 == 2)
        {
          cmd->buttons |= BT_USE;
          dclicks2 = 0;
        }
      else
        dclicktime2 = 0;
    }
    else
      if (++dclicktime2 > 20)
      {
        dclicks2 = 0;
        dclickstate2 = 0;
      }
  }

  if (dsda_VertMouse())
  {
    forward += mousey;
  }

  dsda_ApplyQuickstartMouseCache(&mousex);

  if (strafe)
  {
    static double mousestrafe_carry = 0;
    int delta;
    double true_delta;

    true_delta = mousestrafe_carry +
                 (double) mousex / mouse_strafe_divisor;

    delta = (int) true_delta;
    delta = (delta / 2) * 2;
    mousestrafe_carry = true_delta - delta;

    side += delta;
    side = (side / 2) * 2; // only even values are possible
  }
  else
    cmd->angleturn -= mousex; /* mead now have enough dynamic range 2-10-00 */

  G_ConvertAnalogMotion(speed, &forward, &side);

  if (!walkcamera.type) //e6y
    G_ResetMotion();

  if (forward > MAXPLMOVE)
    forward = MAXPLMOVE;
  else if (forward < -MAXPLMOVE)
    forward = -MAXPLMOVE;

  if (side > MAXPLMOVE)
    side = MAXPLMOVE;
  else if (side < -MAXPLMOVE)
    side = -MAXPLMOVE;

  //e6y
  if (dsda_AlwaysSR50())
  {
    if (!speed)
    {
      if (side > player_class->forwardmove[0])
        side = player_class->forwardmove[0];
      else if (side < -player_class->forwardmove[0])
        side = -player_class->forwardmove[0];
    }
    else if (!dsda_IntConfig(dsda_config_movement_strafe50onturns) && !strafe && cmd->angleturn)
    {
      if (side > player_class->sidemove[1])
        side = player_class->sidemove[1];
      else if (side < -player_class->sidemove[1])
        side = -player_class->sidemove[1];
    }
  }

  if (players[consoleplayer].powers[pw_speed] && !players[consoleplayer].morphTics)
  {                           // Adjust for a player with a speed artifact
      forward = (3 * forward) >> 1;
      side = (3 * side) >> 1;
  }

  if (stroller) side = 0;

  cmd->forwardmove += fudgef((signed char)forward);
  cmd->sidemove += side;

  if ((demorecording && !longtics) || shorttics)
  {
    // Chocolate Doom Mouse Behaviour
    // Don't discard mouse delta even if value is too small to
    // turn the player this tic
    if (dsda_IntConfig(dsda_config_mouse_carrytics)) {
      static signed short carry = 0;
      signed short desired_angleturn = cmd->angleturn + carry;
      cmd->angleturn = (desired_angleturn + 128) & 0xff00;
      carry = desired_angleturn - cmd->angleturn;
    }

    cmd->angleturn = (((cmd->angleturn + 128) >> 8) << 8);
  }

  upmove = 0;
  if (dsda_InputActive(dsda_input_flyup))
    upmove += flyspeed[speed];
  if (dsda_InputActive(dsda_input_flydown))
    upmove -= flyspeed[speed];

  // CPhipps - special events (game new/load/save/pause)
  if (special_event & BT_SPECIAL) {
    cmd->buttons = special_event;
    special_event = 0;
  }

  dsda_PopExCmdQueue(cmd);

  if (!dsda_StrictMode()) {
    if (leveltime == 0 && totalleveltimes == 0) {
      dsda_arg_t* arg;

      arg = dsda_Arg(dsda_arg_first_input);
      if (arg->found) {
        dsda_TrackFeature(uf_buildzero);

        cmd->forwardmove = (signed char) arg->value.v_int_array[0];
        cmd->sidemove = (signed char) arg->value.v_int_array[1];
        cmd->angleturn = (signed short) (arg->value.v_int_array[2] << 8);
      }
    }
  }
}

static void G_SetInitialHealth(player_t *p)
{
  p->health = initial_health;  // Ty 03/12/98 - use dehacked values
}

static void G_SetInitialInventory(player_t *p)
{
  int i;

    p->readyweapon = p->pendingweapon = g_wp_pistol;
    p->weaponowned[g_wp_fist] = true;
    p->weaponowned[g_wp_pistol] = true;
      p->ammo[am_clip] = initial_bullets; // Ty 03/12/98 - use dehacked values

  for (i = 0; i < NUMAMMO; i++)
    p->maxammo[i] = maxammo[i];
}

static void G_ResetHealth(player_t *p)
{
  G_SetInitialHealth(p);
}

static void G_ResetInventory(player_t *p)
{
  p->armorpoints = 0;
  p->armortype = 0;
  memset(p->powers, 0, sizeof(p->powers));
  memset(p->cards, 0, sizeof(p->cards));
  p->backpack = false;
  memset(p->weaponowned, 0, sizeof(p->weaponowned));
  memset(p->ammo, 0, sizeof(p->ammo));

  // readyweapon, pendingweapon, maxammo handled here
  G_SetInitialInventory(p);
}

//
// G_DoLoadLevel
//
int __thread skyflatnum;

static void G_DoLoadLevel (void)
{
  int i;

  // Set the sky map.
  // First thing, we have a dummy sky texture name,
  //  a flat. The data is in the WAD only because
  //  we look for an actual index, instead of simply
  //  setting one.

  skyflatnum = R_FlatNumForName(g_skyflatname);

  levelstarttic = gametic;        // for time calculation

  if (!demo_compatibility && !mbf_features)   // killough 9/29/98
    boom_basetic = gametic;

  if (wipegamestate == GS_LEVEL)
    wipegamestate = -1;             // force a wipe

  gamestate = GS_LEVEL;

  for (i = 0; i < g_maxplayers; i++)
  {
    if (playeringame[i])
    {
      if (players[i].playerstate == PST_DEAD)
        players[i].playerstate = PST_REBORN;
      else
      {
        if (map_info.flags & MI_RESET_HEALTH)
          G_ResetHealth(&players[i]);

        if (map_info.flags & MI_RESET_INVENTORY)
          G_ResetInventory(&players[i]);
      }
    }
    memset(players[i].frags, 0, sizeof(players[i].frags));
  }

  // automatic pistol start when advancing from one level to the next
  if (dsda_Flag(dsda_arg_pistolstart) || dsda_IntConfig(dsda_config_pistol_start))
      G_PlayerReborn(0);

  // initialize the msecnode_t freelist.                     phares 3/25/98
  // any nodes in the freelist are gone by now, cleared
  // by Z_FreeTag() when the previous level ended or player
  // died.
  P_FreeSecNodeList();


  P_SetupLevel (gameepisode, gamemap, 0, gameskill);
    displayplayer = consoleplayer;    // view the guy you are playing
  gameaction = ga_nothing;

  // clear cmd building stuff
  dsda_InputFlush();
  G_ResetMotion();
  mlooky = 0;//e6y
  special_event = 0;
  dsda_ResetExCmdQueue();

}

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//

dboolean G_Responder (event_t* ev)
{
  if (
    gamestate == GS_LEVEL 
  ) return true;

  // allow spy mode changes even during the demo
  // killough 2/22/98: even during DM demo
  //
  // killough 11/98: don't autorepeat spy mode switch
  if (dsda_InputActivated(dsda_input_spy) &&
      netgame && ( !deathmatch) &&
      gamestate == GS_LEVEL)
  {
    do                                          // spy mode
      if (++displayplayer >= g_maxplayers)
        displayplayer = 0;
    while (!playeringame[displayplayer] && displayplayer!=consoleplayer);

    return true;
  }


  if (gamestate == GS_FINALE && F_Responder(ev))
    return true;  // finale ate the event

  // If the next/previous weapon keys are pressed, set the next_weapon
  // variable to change weapons when the next ticcmd is generated.
  if (dsda_InputActivated(dsda_input_prevweapon))
  {
    next_weapon = -1;
  }
  else if (dsda_InputActivated(dsda_input_nextweapon))
  {
    next_weapon = 1;
  }

  if (dsda_InputActivated(dsda_input_invleft))
  {
    return InventoryMoveLeft();
  }
  if (dsda_InputActivated(dsda_input_invright))
  {
    return InventoryMoveRight();
  }

  if (dsda_InputActivated(dsda_input_pause))
  {
    special_event = BT_SPECIAL | (BT_PAUSE & BT_SPECIALMASK);
    return true;
  }

  // Events that make it here should reach into the game logic
  dsda_InputTrackGameEvent(ev);

  switch (ev->type)
  {
    case ev_keydown:
      return true;    // eat key down events

    case ev_mousemotion:
    {
      double value;

      dsda_WatchMouseEvent();

      value = mouse_sensitivity_horiz * AccelerateMouse(ev->data1.i);
      mousex += G_CarryDouble(carry_mousex, value);
      if (dsda_MouseLook())
      {
        value = mouse_sensitivity_mlook * AccelerateMouse(ev->data2.i);
        if (dsda_IntConfig(dsda_config_movement_mouseinvert))
          mlooky += G_CarryDouble(carry_mousey, value);
        else
          mlooky -= G_CarryDouble(carry_mousey, value);
      }
      else
      {
        value = mouse_sensitivity_vert * AccelerateMouse(ev->data2.i) / 8;
        mousey += G_CarryDouble(carry_vertmouse, value);
      }

      return true;    // eat events
    }

    case ev_move_analog:
      dsda_WatchGameControllerEvent();

      left_analog_x = ev->data1.f;
      left_analog_y = ev->data2.f;
      return true;    // eat events

    case ev_look_analog:
      dsda_WatchGameControllerEvent();

      mousex += AccelerateAnalog(ev->data1.f);
      if (dsda_MouseLook())
      {
        if (dsda_IntConfig(dsda_config_invert_analog_look))
          mlooky += AccelerateAnalog(ev->data2.f);
        else
          mlooky -= AccelerateAnalog(ev->data2.f);
      }
      return true;    // eat events

    default:
      break;
  }
  return false;
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//

dboolean dsda_AdvanceFrame(void) {
 return true;

}


void G_Ticker (void)
{
  int i;
  int entry_leveltime;
  int pause_mask;
  dboolean advance_frame = false;
  static gamestate_t prevgamestate;

  entry_leveltime = leveltime;

  P_MapStart();
  // do player reborns if needed
  for (i = 0; i < g_maxplayers; i++)
    if (playeringame[i] && players[i].playerstate == PST_REBORN)
      G_DoReborn(i);
  P_MapEnd();


  // Resetting level/game ending indication flags
  reachedLevelExit = 0;
  reachedGameEnd = 0;

  // Checking for level ending condition (and prevent the transition if necessary)
  if (gameaction == ga_completed)  
  {
    reachedLevelExit = 1;
    if (preventLevelExit == true) gameaction = ga_nothing; 
  }
  
  // Checking for game ending condition (and prevent the transition if necessary)
  if ((gameaction == ga_completed && gamemode == commercial && gamemap == 30) ||
      (gameaction == ga_completed && gamemode == retail && gamemap == 8) ||
      (preventGameEnd == true && gameaction == ga_victory))
  {
    reachedGameEnd = 1;
    if (preventGameEnd == true) gameaction = ga_nothing;
  }

  // do things to change the game state
  while (gameaction != ga_nothing)
  {
    switch (gameaction)
    {
      case ga_loadlevel:
        // force players to be initialized on level reload
          for (i = 0; i < g_maxplayers; i++)
            players[i].playerstate = PST_REBORN;
        G_DoLoadLevel();
        break;
      case ga_newgame:
        G_DoNewGame();
        break;
      case ga_loadgame:
        G_DoLoadGame();
        break;
      case ga_completed:
        G_DoCompleted();
        break;
      case ga_victory:
        F_StartFinale();
        break;
      case ga_worlddone:
        G_DoWorldDone();
        break;
      case ga_leavemap:
        G_DoTeleportNewMap();
        break;
      case ga_nothing:
        break;
    }
  }

  if (dsda_AdvanceFrame())
  {
    advance_frame = true;
  }

 {
    int buf = gametic % BACKUPTICS;

    dsda_UpdateAutoSaves();

    for (i = 0; i < g_maxplayers; i++)
    {
      if (playeringame[i])
      {
        ticcmd_t *cmd = &players[i].cmd;

        memcpy(cmd, &local_cmds[i], sizeof *cmd);
      }
    }

    dsda_InputFlushTick();

    // check for special buttons
    for (i = 0; i < g_maxplayers; i++)
    {
      if (playeringame[i])
      {
        if (players[i].cmd.buttons & BT_SPECIAL)
        {
          players[i].cmd.buttons = 0;
        }

        if (dsda_AllowExCmd())
        {
          excmd_t *ex = &players[i].cmd.ex;

          if (ex->actions & XC_SAVE)
          {
            savegameslot = ex->save_slot;
            G_DoSaveGame(true);
          }

          if (ex->actions & XC_LOAD)
          {
            savegameslot = ex->load_slot;
            gameaction = ga_loadgame;
            forced_loadgame = true;
            load_via_cmd = true;
          }

          if (ex->actions & XC_LOOK && ex->look != XC_LOOK_RESET && !dsda_MouseLook())
          {
            dsda_UpdateIntConfig(dsda_config_freelook, 1, false);
          }
        }
      }
    }

  }

  // cph - if the gamestate changed, we may need to clean up the old gamestate
  if (gamestate != prevgamestate) {
    switch (prevgamestate) {
      case GS_LEVEL:
        break;
      case GS_INTERMISSION:
        WI_End();
      default:
        break;
    }
    prevgamestate = gamestate;
  }

  // do main actions
  switch (gamestate)
  {
    case GS_LEVEL:
      P_Ticker();
      mlooky = 0;
      break;

    case GS_INTERMISSION:
      WI_Ticker();
      break;

    case GS_FINALE:
      F_Ticker();
      break;

    case GS_DEMOSCREEN:
      D_PageTicker();
      break;
  }

}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//

typedef struct {
  int flight_carryover;
  dboolean set_one_artifact;
  int use_flight_artifact;
  int use_flight_count;
  dboolean remove_cards;
} finish_level_behaviour_t;

static void G_FinishLevelBehaviour(finish_level_behaviour_t *flb, player_t *p)
{
  dboolean different_cluster;

  different_cluster = (dsda_MapCluster(gamemap) != dsda_MapCluster(leave_data.map));

    flb->flight_carryover = 0;

  flb->set_one_artifact = 0;

    flb->use_flight_artifact = arti_none;
    flb->use_flight_count = 0;

  flb->remove_cards = 1;
}

static void G_PlayerFinishLevel(int player)
{
  int i;
  player_t *p = &players[player];
  finish_level_behaviour_t flb;

  G_FinishLevelBehaviour(&flb, p);

  if (flb.set_one_artifact)
  {
    for (i = 0; i < p->inventorySlotNum; i++)
    {
      p->inventory[i].count = 1;
    }
    p->artifactCount = p->inventorySlotNum;
  }

  p->lookdir = 0;
  p->rain1 = NULL;
  p->rain2 = NULL;

  memset(p->powers, 0, sizeof p->powers);
  if (flb.flight_carryover)
    p->powers[pw_flight] = flb.flight_carryover;

  if (flb.remove_cards)
    memset(p->cards, 0, sizeof p->cards);

  // TODO: need to understand the life cycle of p->mo in hexen
    p->mo = NULL;           // cph - this is zone-allocated so it's gone

  p->extralight = 0;      // cancel gun flashes
  p->fixedcolormap = 0;   // cancel ir gogles
  p->damagecount = 0;     // no palette changes
  p->bonuscount = 0;
  p->poisoncount = 0;
}

// CPhipps - G_SetPlayerColour
// Player colours stuff
//
// G_SetPlayerColour

void G_ChangedPlayerColour(int pn, int cl)
{
}

//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//

void G_PlayerReborn (int player)
{
  player_t *p;
  int frags[MAX_MAXPLAYERS];
  int killcount;
  int itemcount;
  int secretcount;
  int maxkilldiscount; //e6y
  unsigned int worldTimer;

  memcpy (frags, players[player].frags, sizeof frags);
  killcount = players[player].killcount;
  itemcount = players[player].itemcount;
  secretcount = players[player].secretcount;
  maxkilldiscount = players[player].maxkilldiscount; //e6y
  worldTimer = players[player].worldTimer;

  p = &players[player];

  // killough 3/10/98,3/21/98: preserve cheats across idclev
  {
    int cheats = p->cheats;
    memset (p, 0, sizeof(*p));
    p->cheats = cheats;
  }

  memcpy(players[player].frags, frags, sizeof(players[player].frags));
  players[player].killcount = killcount;
  players[player].itemcount = itemcount;
  players[player].secretcount = secretcount;
  players[player].maxkilldiscount = maxkilldiscount; //e6y
  players[player].worldTimer = worldTimer;

  p->usedown = p->attackdown = true;  // don't do anything immediately
  p->playerstate = PST_LIVE;

  G_SetInitialHealth(p);
  G_SetInitialInventory(p);

  p->lookdir = 0;

  levels_completed = 0;
}

//
// G_CheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//

static dboolean G_CheckSpot(int playernum, mapthing_t *mthing)
{
  fixed_t     x,y;
  sector_t *sec;
  int         i;

  if (!players[playernum].mo)
    {
      // first spawn of level, before corpses
      for (i=0 ; i<playernum ; i++)
        if (players[i].mo->x == mthing->x && players[i].mo->y == mthing->y)
          return false;
      return true;
    }

  x = mthing->x;
  y = mthing->y;

  // killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
  // corpse to detect collisions with other players in DM starts
  //
  // Old code:
  // if (!P_CheckPosition (players[playernum].mo, x, y))
  //    return false;

  players[playernum].mo->flags |=  MF_SOLID;
  i = P_CheckPosition(players[playernum].mo, x, y);
  players[playernum].mo->flags &= ~MF_SOLID;
  if (!i)
    return false;

  // spawn a teleport fog
  sec = R_PointInSector (x,y);
  { // Teleport fog at respawn point
    fixed_t xa,ya;
    int an;
    mobj_t      *mo;

/* BUG: an can end up negative, because mthing->angle is (signed) short.
 * We have to emulate original Doom's behaviour, deferencing past the start
 * of the array, into the previous array (finetangent) */
    an = ( ANG45 * ((signed)mthing->angle/45) ) >> ANGLETOFINESHIFT;
    xa = finecosine[an];
    ya = finesine[an];

    if (compatibility_level <= finaldoom_compatibility || compatibility_level == prboom_4_compatibility)
      switch (an) {
      case -4096: xa = finetangent[2048];   // finecosine[-4096]
            ya = finetangent[0];      // finesine[-4096]
            break;
      case -3072: xa = finetangent[3072];   // finecosine[-3072]
            ya = finetangent[1024];   // finesine[-3072]
            break;
      case -2048: xa = finesine[0];   // finecosine[-2048]
            ya = finetangent[2048];   // finesine[-2048]
            break;
      case -1024:  xa = finesine[1024];     // finecosine[-1024]
            ya = finetangent[3072];  // finesine[-1024]
            break;
      case 1024:
      case 2048:
      case 3072:
      case 4096:
      case 0:  break; /* correct angles set above */
      default:  I_Error("G_CheckSpot: unexpected angle %d\n",an);
      }

    mo = P_SpawnMobj(x+20*xa, y+20*ya, sec->floorheight, MT_TFOG);

  }

  return true;
}


// G_DeathMatchSpawnPlayer
// Spawns a player at one of the random death match spots
// called at level load and each death
//
void G_DeathMatchSpawnPlayer (int playernum)
{
  int j, selections = deathmatch_p - deathmatchstarts;

  if (selections < g_maxplayers)
    I_Error("G_DeathMatchSpawnPlayer: Only %i deathmatch spots, %d required",
    selections, g_maxplayers);

  for (j=0 ; j<20 ; j++)
    {
      int i = P_Random(pr_dmspawn) % selections;
      if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
        {
          deathmatchstarts[i].type = playernum+1;
          P_SpawnPlayer (playernum, &deathmatchstarts[i]);
          return;
        }
    }

  // no good spot, so the player will probably get stuck
  P_SpawnPlayer (playernum, &playerstarts[0][playernum]);
}

//
// G_DoReborn
//

void G_DoReborn (int playernum)
{
  if (!netgame && !(map_info.flags & MI_ALLOW_RESPAWN) && !(skill_info.flags & SI_PLAYER_RESPAWN))
    gameaction = ga_loadlevel;      // reload the level from scratch
  else
    {                               // respawn at the start
      int i;

      // first dissasociate the corpse
      players[playernum].mo->player = NULL;

      // spawn at random spot if in death match
      if (deathmatch)
        {
          G_DeathMatchSpawnPlayer (playernum);
          return;
        }

      if (G_CheckSpot (playernum, &playerstarts[0][playernum]) )
        {
          P_SpawnPlayer (playernum, &playerstarts[0][playernum]);
          return;
        }

      // try to spawn at one of the other players spots
      for (i = 0; i < g_maxplayers; i++)
        {
          if (G_CheckSpot (playernum, &playerstarts[0][i]) )
            {
              P_SpawnPlayer (playernum, &playerstarts[0][i]);
              return;
            }
          // he's going to be inside something.  Too bad.
        }
      P_SpawnPlayer (playernum, &playerstarts[0][playernum]);
    }
}

// DOOM Par Times
__thread int pars[5][10] = {
  {0},
  {0,30,75,120,90,165,180,180,30,165},
  {0,90,90,90,120,90,360,240,30,170},
  {0,90,45,90,150,90,90,165,30,135},
  // from Doom 3 BFG Edition
  {0,165,255,135,150,180,390,135,360,180}
};

// DOOM II Par Times
__thread  int cpars[34] = {
  30,90,120,120,90,150,120,120,270,90,  //  1-10
  210,150,150,150,210,150,420,150,210,150,  // 11-20
  240,150,180,150,150,300,330,420,300,180,  // 21-30
  120,30,30,30          // 31-34
};

__thread dboolean secretexit;

void G_ExitLevel(int position)
{
  secretexit = false;
  gameaction = ga_completed;
  dsda_UpdateLeaveData(0, position, 0, 0);
}

// Here's for the german edition.
// IF NO WOLF3D LEVELS, NO SECRET EXIT!

void G_SecretExitLevel(int position)
{
  if (gamemode!=commercial || haswolflevels)
    secretexit = true;
  else
    secretexit = false;
  gameaction = ga_completed;
  dsda_UpdateLeaveData(0, position, 0, 0);
}

//
// G_DoCompleted
//

void G_DoCompleted (void)
{
  int i;
  int completed_behaviour;

  R_ResetColorMap();

    totalleveltimes += leveltime - leveltime % 35;
  ++levels_completed;

  gameaction = ga_nothing;

  for (i = 0; i < g_maxplayers; i++)
    if (playeringame[i])
      G_PlayerFinishLevel(i);        // take away cards and stuff

  e6y_G_DoCompleted();

  wminfo.nextep = wminfo.epsd = gameepisode -1;
  wminfo.last = gamemap -1;

  dsda_UpdateLastMapInfo();

  dsda_PrepareIntermission(&completed_behaviour);

  if (completed_behaviour & DC_VICTORY)
  {
    gameaction = ga_victory;
    return;
  }

  dsda_UpdateNextMapInfo();
  wminfo.maxkills = totalkills;
  wminfo.maxitems = totalitems;
  wminfo.maxsecret = totalsecret;
  wminfo.maxfrags = 0;
  wminfo.pnum = consoleplayer;

  for (i = 0; i < g_maxplayers; i++)
  {
    wminfo.plyr[i].in = playeringame[i];
    wminfo.plyr[i].skills = players[i].killcount;
    wminfo.plyr[i].sitems = players[i].itemcount;
    wminfo.plyr[i].ssecret = players[i].secretcount;
    wminfo.plyr[i].stime = leveltime;
    memcpy (wminfo.plyr[i].frags, players[i].frags,
            sizeof(wminfo.plyr[i].frags));
  }

  wminfo.totaltimes = totalleveltimes;

  gamestate = GS_INTERMISSION;

  // lmpwatch.pl engine-side demo testing support
  // print "FINISHED: <mapname>" when the player exits the current map
  if (nodrawers && (timingdemo))
    lprintf(LO_INFO, "FINISHED: %s\n", dsda_MapLumpName(gameepisode, gamemap));

  if (!(map_info.flags & MI_INTERMISSION))
  {
    G_WorldDone();
  }
  else
  {
    WI_Start (&wminfo);
  }
}

//
// G_WorldDone
//

void G_WorldDone (void)
{
  int done_behaviour;

  gameaction = ga_worlddone;

  if (secretexit)
    players[consoleplayer].didsecret = true;

  dsda_PrepareFinale(&done_behaviour);

  if (done_behaviour & WD_VICTORY)
  {
    if (dsda_Flag(dsda_arg_chain_episodes))
    {
      int epi, map;

      dsda_NextMap(&epi, &map);

      if (epi != 1 || map != 1)
      {
        int i;

        for (i = 0; i < g_maxplayers; ++i)
          if (playeringame[i])
            players[i].playerstate = PST_DEAD;

        wminfo.nextep = epi - 1;
        wminfo.next = map - 1;

        return;
      }
    }

    gameaction = ga_victory;

    return;
  }

  if (done_behaviour & WD_START_FINALE)
  {
    F_StartFinale();

    return;
  }
}

void G_DoWorldDone (void)
{
  gamestate = GS_LEVEL;
  dsda_UpdateGameMap(wminfo.nextep + 1, wminfo.next + 1);
  G_DoLoadLevel();
  gameaction = ga_nothing;
}

extern dboolean setsizeneeded;

//CPhipps - savename variable redundant

/* killough 12/98:
 * This function returns a signature for the current wad.
 * It is used to distinguish between wads, for the purposes
 * of savegame compatibility warnings, and options lookups.
 */

static uint64_t G_UpdateSignature(uint64_t s, const char *name)
{
  int i, lump = W_CheckNumForName(name);
  if (lump != LUMP_NOT_FOUND && (i = lump+10) < numlumps)
    do
      {
  int size = W_LumpLength(i);
  const byte *p = W_LumpByNum(i);
  while (size--)
    s <<= 1, s += *p++;
      }
    while (--i > lump);
  return s;
}

static uint64_t G_Signature(void)
{
  static uint64_t s = 0;
  static dboolean computed = false;
  int episode, map;

  if (!computed) {
   computed = true;
   if (gamemode == commercial)
    for (map = haswolflevels ? 32 : 30; map; map--)
      s = G_UpdateSignature(s, dsda_MapLumpName(1, map));
   else
    for (episode = gamemode==retail ? 4 :
     gamemode==shareware ? 1 : 3; episode; episode--)
      for (map = 9; map; map--)
        s = G_UpdateSignature(s, dsda_MapLumpName(episode, map));
  }
  return s;
}

//
// killough 5/15/98: add forced loadgames, which allow user to override checks
//

void G_ForcedLoadGame(void)
{
  // CPhipps - net loadgames are always forced, so we only reach here
  //  in single player
  gameaction = ga_loadgame;
  forced_loadgame = true;
}

// killough 3/16/98: add slot info
void G_LoadGame(int slot)
{
  if (demorecording)
  {
    dsda_QueueExCmdLoad(slot);
    return;
  }

    forced_loadgame = false;
    // Don't stay in netgame state if loading single player save
    // while watching multiplayer demo
    netgame = false;

  gameaction = ga_loadgame;
  savegameslot = slot;
  load_via_cmd = false;
}

// killough 5/15/98:
// Consistency Error when attempting to load savegame.

static void G_LoadGameErr(const char *msg)
{
  P_FreeSaveBuffer();
}

__thread const char * comp_lev_str[MAX_COMPATIBILITY_LEVEL] =
{ "Doom v1.2", "Doom v1.666", "Doom/Doom2 v1.9", "Ultimate Doom/Doom95", "Final Doom",
  "early DosDoom", "TASDoom", "\"boom compatibility\"", "boom v2.01", "boom v2.02", "lxdoom v1.3.2+",
  "MBF", "PrBoom 2.03beta", "PrBoom v2.1.0-2.1.1", "PrBoom v2.1.2-v2.2.6",
  "PrBoom v2.3.x", "PrBoom 2.4.0", "Current PrBoom", "", "", "", "MBF21" };

//==========================================================================
//
// RecalculateDrawnSubsectors
//
// In case the subsector data is unusable this function tries to reconstruct
// if from the linedefs' ML_MAPPED info.
//
//==========================================================================

void RecalculateDrawnSubsectors(void)
{
  int i, j;

  for (i = 0; i < numsubsectors; i++)
  {
    subsector_t *sub = &subsectors[i];
    seg_t *seg = &segs[sub->firstline];
    for (j = 0; j < sub->numlines; j++, seg++)
    {
      if (seg->linedef && seg->linedef->flags & ML_MAPPED)
      {
        map_subsectors[i] = 1;
      }
    }
  }

  #ifdef __ENABLE_OPENGL_
  gld_ResetTexturedAutomap();
  #endif
}

void G_AfterLoad(void)
{
  RecalculateDrawnSubsectors();


  if (setsizeneeded)
    R_ExecuteSetViewSize ();


}

void G_DoLoadGame(void)
{
}

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//

void G_SaveGame(int slot, const char *description)
{
  strcpy(savedescription, description);

  if (demorecording && dsda_AllowCasualExCmdFeatures())
  {
    dsda_QueueExCmdSave(slot);
  }
  else
  {
    savegameslot = slot;
    G_DoSaveGame(false);
  }
}

static void G_DoSaveGame(dboolean via_cmd)
{

}

static __thread int     d_skill;
static __thread int     d_episode;
static __thread int     d_map;

void G_DeferedInitNew(int skill, int episode, int map)
{
  d_skill = skill;
  d_episode = episode;
  d_map = map;
  gameaction = ga_newgame;

}

/* cph -
 * G_Compatibility
 *
 * Initialises the comp[] array based on the compatibility_level
 * For reference, MBF did:
 * for (i=0; i < COMP_TOTAL; i++)
 *   comp[i] = compatibility;
 *
 * Instead, we have a lookup table showing at what version a fix was
 *  introduced, and made optional (replaces comp_options_by_version)
 */

void G_Compatibility(void)
{
  static const struct {
    complevel_t fix; // level at which fix/change was introduced
    complevel_t opt; // level at which fix/change was made optional
  } levels[] = {
    // comp_telefrag - monsters used to telefrag only on MAP30, now they do it for spawners only
    { mbf_compatibility, mbf_compatibility },
    // comp_dropoff - MBF encourages things to drop off of overhangs
    { mbf_compatibility, mbf_compatibility },
    // comp_vile - original Doom archville bugs like ghosts
    { boom_compatibility, mbf_compatibility },
    // comp_pain - original Doom limits Pain Elementals from spawning too many skulls
    { boom_compatibility, mbf_compatibility },
    // comp_skull - original Doom let skulls be spit through walls by Pain Elementals
    { boom_compatibility, mbf_compatibility },
    // comp_blazing - original Doom duplicated blazing door sound
    { boom_compatibility, mbf_compatibility },
    // e6y: "Tagged doors don't trigger special lighting" handled wrong
    // http://sourceforge.net/tracker/index.php?func=detail&aid=1411400&group_id=148658&atid=772943
    // comp_doorlight - MBF made door lighting changes more gradual
    { boom_compatibility, mbf_compatibility },
    // comp_model - improvements to the game physics
    { boom_compatibility, mbf_compatibility },
    // comp_god - fixes to God mode
    { boom_compatibility, mbf_compatibility },
    // comp_falloff - MBF encourages things to drop off of overhangs
    { mbf_compatibility, mbf_compatibility },
    // comp_floors - fixes for moving floors bugs
    { boom_compatibility_compatibility, mbf_compatibility },
    // comp_skymap
    { mbf_compatibility, mbf_compatibility },
    // comp_pursuit - MBF AI change, limited pursuit?
    { mbf_compatibility, mbf_compatibility },
    // comp_doorstuck - monsters stuck in doors fix
    { boom_202_compatibility, mbf_compatibility },
    // comp_staylift - MBF AI change, monsters try to stay on lifts
    { mbf_compatibility, mbf_compatibility },
    // comp_zombie - prevent dead players triggering stuff
    { lxdoom_1_compatibility, mbf_compatibility },
    // comp_stairs - see p_floor.c
    { boom_202_compatibility, mbf_compatibility },
    // comp_infcheat - FIXME
    { mbf_compatibility, mbf_compatibility },
    // comp_zerotags - allow zero tags in wads */
    { boom_compatibility, mbf_compatibility },
    // comp_moveblock - enables keygrab and mancubi shots going thru walls
    { lxdoom_1_compatibility, prboom_2_compatibility },
    // comp_respawn - objects which aren't on the map at game start respawn at (0,0)
    { prboom_2_compatibility, prboom_2_compatibility },
    // comp_sound - see s_sound.c
    { boom_compatibility_compatibility, prboom_3_compatibility },
    // comp_666 - emulate pre-Ultimate BossDeath behaviour
    { ultdoom_compatibility, prboom_4_compatibility },
    // comp_soul - enables lost souls bouncing (see P_ZMovement)
    { prboom_4_compatibility, prboom_4_compatibility },
    // comp_maskedanim - 2s mid textures don't animate
    { doom_1666_compatibility, prboom_4_compatibility },
    //e6y
    // comp_ouchface - Use Doom's buggy "Ouch" face code
    { prboom_1_compatibility, prboom_6_compatibility },
    // comp_maxhealth - Max Health in DEH applies only to potions
    { boom_compatibility_compatibility, prboom_6_compatibility },
    // comp_translucency - No predefined translucency for some things
    { boom_compatibility_compatibility, prboom_6_compatibility },
    // comp_ledgeblock - ground monsters are blocked by ledges
    { boom_compatibility, mbf21_compatibility },
    // comp_friendlyspawn - A_Spawn new mobj inherits friendliness
    { prboom_1_compatibility, mbf21_compatibility },
    // comp_voodooscroller - Voodoo dolls on slow scrollers move too slowly
    { mbf21_compatibility, mbf21_compatibility },
    // comp_reservedlineflag - ML_RESERVED clears extended flags
    { mbf21_compatibility, mbf21_compatibility }
  };
  unsigned int i;

  if (sizeof(levels)/sizeof(*levels) != MBF_COMP_TOTAL)
    I_Error("G_Compatibility: consistency error");

  for (i = 0; i < sizeof(levels)/sizeof(*levels); i++)
    if (compatibility_level < levels[i].opt)
      comp[i] = (compatibility_level < levels[i].fix);

  // These options were deoptionalized in mbf21
  if (mbf21)
  {
    comp[comp_moveblock] = 0;
    comp[comp_sound] = 0;
    comp[comp_666] = 0;
    comp[comp_maskedanim] = 0;
    comp[comp_ouchface] = 0;
    comp[comp_maxhealth] = 0;
    comp[comp_translucency] = 0;
  }

  e6y_G_Compatibility();//e6y

  if (!mbf_features) {
    monster_infighting = 1;
    monster_backing = 0;
    monster_avoid_hazards = 0;
    monster_friction = 0;
    help_friends = 0;

    dogs = 0;
    dog_jumping = 0;

    monkeys = 0;
  }
}

// killough 3/1/98: function to reload all the default parameter
// settings before a new game begins

void G_ReloadDefaults(void)
{
  const dsda_options_t* options;

  compatibility_level = dsda_IntConfig(dsda_config_default_complevel);
  {
    int l;
    l = dsda_CompatibilityLevel();
    if (l != UNSPECIFIED_COMPLEVEL)
      compatibility_level = l;
  }
  if (compatibility_level == -1)
    compatibility_level = best_compatibility;

  // killough 3/1/98: Initialize options based on config file
  // (allows functions above to load different values for demos
  // and savegames without messing up defaults).

  // Allows PWAD HELP2 screen for DOOM 1 wads.
  // there's no easy way to set it only to complevel 0-2, so
  // I just allowed it for complevel 3 if HELP2 is present
  if ((compatibility_level <= 3) && (gamemode != commercial) && (gamemode != shareware))
    pwad_help2_check = W_PWADLumpNameExists("HELP2");

  options = dsda_Options();

  weapon_recoil = options->weapon_recoil;    // weapon recoil

  player_bobbing = 1;  // whether player bobs or not

  variable_friction = 1;
  allow_pushers     = 1;
  monsters_remember = options->monsters_remember; // remember former enemies

  monster_infighting = options->monster_infighting; // killough 7/19/98

  dogs = netgame ? 0 : options->player_helpers; // killough 7/19/98
  dog_jumping = options->dog_jumping;

  distfriend = options->friend_distance; // killough 8/8/98

  monster_backing = options->monster_backing; // killough 9/8/98

  monster_avoid_hazards = options->monster_avoid_hazards; // killough 9/9/98

  monster_friction = options->monster_friction; // killough 10/98

  help_friends = options->help_friends; // killough 9/9/98

  monkeys = options->monkeys;

  // jff 1/24/98 reset play mode to command line spec'd version
  // killough 3/1/98: moved to here
  respawnparm = clrespawnparm;
  fastparm = clfastparm;
  nomonsters = clnomonsters;

  // killough 2/21/98:
  memset(playeringame + 1, 0, sizeof(*playeringame) * (MAX_MAXPLAYERS - 1));

  consoleplayer = 0;

  // MBF introduced configurable compatibility settings
  if (mbf_features)
  {
    comp[comp_telefrag] = options->comp_telefrag;
    comp[comp_dropoff] = options->comp_dropoff;
    comp[comp_vile] = options->comp_vile;
    comp[comp_pain] = options->comp_pain;
    comp[comp_skull] = options->comp_skull;
    comp[comp_blazing] = options->comp_blazing;
    comp[comp_doorlight] = options->comp_doorlight;
    comp[comp_model] = options->comp_model;
    comp[comp_god] = options->comp_god;
    comp[comp_falloff] = options->comp_falloff;
    comp[comp_floors] = options->comp_floors;
    comp[comp_skymap] = options->comp_skymap;
    comp[comp_pursuit] = options->comp_pursuit;
    comp[comp_doorstuck] = options->comp_doorstuck;
    comp[comp_staylift] = options->comp_staylift;
    comp[comp_zombie] = options->comp_zombie;
    comp[comp_stairs] = options->comp_stairs;
    comp[comp_infcheat] = options->comp_infcheat;
    comp[comp_zerotags] = options->comp_zerotags;

    comp[comp_moveblock] = options->comp_moveblock;
    comp[comp_respawn] = options->comp_respawn;
    comp[comp_sound] = options->comp_sound;
    comp[comp_666] = options->comp_666;
    comp[comp_soul] = options->comp_soul;
    comp[comp_maskedanim] = options->comp_maskedanim;
    comp[comp_ouchface] = options->comp_ouchface;
    comp[comp_maxhealth] = options->comp_maxhealth;
    comp[comp_translucency] = options->comp_translucency;
    comp[comp_ledgeblock] = options->comp_ledgeblock;
    comp[comp_friendlyspawn] = options->comp_friendlyspawn;
    comp[comp_voodooscroller] = options->comp_voodooscroller;
    comp[comp_reservedlineflag] = options->comp_reservedlineflag;
  }

  G_Compatibility();

  // killough 3/31/98, 4/5/98: demo sync insurance
  demo_insurance = 0;

  rngseed += I_GetRandomTimeSeed() + gametic; // CPhipps
}

void G_DoNewGame (void)
{
  int realMap = d_map;
  int realEpisode = d_episode;

  G_ReloadDefaults();            // killough 3/1/98
  netgame = solo_net;
  deathmatch = false;

  dsda_NewGameMap(&realEpisode, &realMap);
  dsda_ResetLeaveData();

  G_InitNew (d_skill, realEpisode, realMap, true);
  gameaction = ga_nothing;

  //jff 4/26/98 wake up the status bar in case were coming out of a DM demo
  walkcamera.type=0; //e6y
}

// killough 4/10/98: New function to fix bug which caused Doom
// lockups when idclev was used in conjunction with -fast.

void G_RefreshFastMonsters(void)
{
  static int fast = 0;            // remembers fast state
  int i;
  int fast_pending;

  fast_pending = !!(skill_info.flags & SI_FAST_MONSTERS);


  if (fast != fast_pending) {     /* only change if necessary */
    for (i = 0; i < num_mobj_types; ++i)
      if (mobjinfo[i].altspeed != NO_ALTSPEED)
      {
        int swap = mobjinfo[i].speed;
        mobjinfo[i].speed = mobjinfo[i].altspeed;
        mobjinfo[i].altspeed = swap;
      }

    if ((fast = fast_pending))
    {
      for (i = 0; i < num_states; i++)
        if (states[i].flags & STATEF_SKILL5FAST && (states[i].tics != 1 || demo_compatibility))
          states[i].tics >>= 1;  // don't change 1->0 since it causes cycles
    }
    else
    {
      for (i = 0; i < num_states; i++)
        if (states[i].flags & STATEF_SKILL5FAST)
          states[i].tics <<= 1;
    }
  }
}

int G_ValidateMapName(const char *mapname, int *pEpi, int *pMap)
{
  // Check if the given map name can be expressed as a gameepisode/gamemap pair and be reconstructed from it.
  char lumpname[9], mapuname[9];
  int epi = -1, map = -1;

  if (strlen(mapname) > 8) return 0;
  strncpy(mapuname, mapname, 8);
  mapuname[8] = 0;
  M_Strupr(mapuname);

  if (gamemode != commercial)
  {
    if (sscanf(mapuname, "E%dM%d", &epi, &map) != 2) return 0;
    snprintf(lumpname, sizeof(lumpname), "E%dM%d", epi, map);
  }
  else
  {
    if (sscanf(mapuname, "MAP%d", &map) != 1) return 0;
    snprintf(lumpname, sizeof(lumpname), "MAP%02d", map);
    epi = 1;
  }
  if (pEpi) *pEpi = epi;
  if (pMap) *pMap = map;
  return !strcmp(mapuname, lumpname);
}

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//

extern __thread int EpiCustom;

void G_InitNew(int skill, int episode, int map, dboolean prepare)
{
  int i;

  // e6y
  // This variable is for correct checking for upper limit of episode.
  // Ultimate Doom, Final Doom and Doom95 have
  // "if (episode == 0) episode = 3/4" check instead of
  // "if (episode > 3/4) episode = 3/4"
  dboolean fake_episode_check =
    compatibility_level == ultdoom_compatibility ||
    compatibility_level == finaldoom_compatibility;

  in_game = true;

  if (prepare)
    dsda_PrepareInitNew();


  if (episode < 1)
    episode = 1;

  if (!W_LumpNameExists(dsda_MapLumpName(episode, map)))
  {
   if (map_format.map99)
    {
      if (map < 1)
        map = 1;
      if (map > 99)
        map = 99;
    }
    else
    {
      //e6y: We need to remove the fourth episode for pre-ultimate complevels.
      if (compatibility_level < ultdoom_compatibility && episode > 3)
      {
        episode = 3;
      }

      //e6y: DosDoom has only this check
      if (compatibility_level == dosdoom_compatibility)
      {
        if (gamemode == shareware)
          episode = 1; // only start episode 1 on shareware
      }
      else
      if (gamemode == retail)
        {
          // e6y: Ability to play any episode with Ultimate Doom,
          // Final Doom or Doom95 compatibility and -warp command line switch
          // E5M1 from 2002ado.wad is an example.
          // Now you can play it with "-warp 5 1 -complevel 3".
          // 'Vanilla' Ultimate Doom executable also allows it.
          if (fake_episode_check ? episode == 0 : episode > 4)
            episode = 4;
        }
      else
        if (gamemode == shareware)
          {
            if (episode > 1)
              episode = 1; // only start episode 1 on shareware
          }
        else
          // e6y: Ability to play any episode with Ultimate Doom,
          // Final Doom or Doom95 compatibility and -warp command line switch
          if (fake_episode_check ? episode == 0 : episode > 3)
            episode = 3;

      if (map < 1)
        map = 1;
      if (map > 9 && gamemode != commercial)
        map = 9;
    }
  }

  {
    extern __thread int dsda_startmap;

    dsda_startmap = map;
  }

  M_ClearRandom();

  // force players to be initialized upon first level load
  for (i = 0; i < g_maxplayers; i++)
  {
    players[i].playerstate = PST_REBORN;
    players[i].worldTimer = 0;
  }

  dsda_UpdateGameSkill(skill);
  dsda_UpdateGameMap(episode, map);

  totalleveltimes = 0; // cph
  levels_completed = 0;

  dsda_InitSky();

  G_DoLoadLevel ();
}

//
// DEMO RECORDING
//

#define DEMOHEADER_RESPAWN    0x20
#define DEMOHEADER_LONGTICS   0x10
#define DEMOHEADER_NOMONSTERS 0x02

void G_ReadOneTick(ticcmd_t* cmd, const byte **data_p)
{
  unsigned char at = 0; // e6y: for tasdoom demo format

  cmd->forwardmove = (signed char)(*(*data_p)++);
  cmd->sidemove = (signed char)(*(*data_p)++);
  if (!longtics)
  {
    cmd->angleturn = ((unsigned char)(at = *(*data_p)++))<<8;
  }
  else
  {
    unsigned int lowbyte = (unsigned char)(*(*data_p)++);
    cmd->angleturn = (((signed int)(*(*data_p)++))<<8) + lowbyte;
  }
  cmd->buttons = (unsigned char)(*(*data_p)++);

  // e6y: ability to play tasdoom demos directly
  if (compatibility_level == tasdoom_compatibility)
  {
    signed char tmp = cmd->forwardmove;
    cmd->forwardmove = cmd->sidemove;
    cmd->sidemove = (signed char)at;
    cmd->angleturn = ((unsigned char)cmd->buttons)<<8;
    cmd->buttons = (byte)tmp;
  }

  dsda_ReadExCmd(cmd, data_p);
}

/* Demo limits removed -- killough
 * cph - record straight to file
 */
void G_WriteDemoTiccmd (ticcmd_t* cmd)
{
  char buf[10];
  char *p = buf;

  if (compatibility_level == tasdoom_compatibility)
  {
    *p++ = cmd->buttons;
    *p++ = cmd->forwardmove;
    *p++ = cmd->sidemove;
    *p++ = (cmd->angleturn+128)>>8;
  }
  else
  {
    *p++ = cmd->forwardmove;
    *p++ = cmd->sidemove;
    if (!longtics) {
      *p++ = (cmd->angleturn+128)>>8;
    } else {
      signed short a = cmd->angleturn;
      *p++ = a & 0xff;
      *p++ = (a >> 8) & 0xff;
    }
    *p++ = cmd->buttons;

  }

  dsda_WriteExCmd(&p, cmd);

  p = buf; // make SURE it is exactly the same
  G_ReadOneTick(cmd, (const byte **) &p);
}

// These functions are used to read and write game-specific options in demos
// and savegames so that demo sync is preserved and savegame restoration is
// complete. Not all options (for example "compatibility"), however, should
// be loaded and saved here. It is extremely important to use the same
// positions as before for the variables, so if one becomes obsolete, the
// byte(s) should still be skipped over or padded with 0's.
// Lee Killough 3/1/98

byte *G_WriteOptions(byte *demo_p)
{
  byte *target;

  if (mbf21)
  {
    return dsda_WriteOptions21(demo_p);
  }

  target = demo_p + dsda_GameOptionSize();

  *demo_p++ = monsters_remember;  // part of monster AI

  *demo_p++ = variable_friction;  // ice & mud

  *demo_p++ = weapon_recoil;      // weapon recoil

  *demo_p++ = allow_pushers;      // MT_PUSH Things

  *demo_p++ = 0;

  *demo_p++ = player_bobbing;  // whether player bobs or not

  // killough 3/6/98: add parameters to savegame, move around some in demos
  *demo_p++ = respawnparm;
  *demo_p++ = fastparm;
  *demo_p++ = nomonsters;

  *demo_p++ = demo_insurance;        // killough 3/31/98

  // killough 3/26/98: Added rngseed. 3/31/98: moved here
  *demo_p++ = (byte)((rngseed >> 24) & 0xff);
  *demo_p++ = (byte)((rngseed >> 16) & 0xff);
  *demo_p++ = (byte)((rngseed >>  8) & 0xff);
  *demo_p++ = (byte)( rngseed        & 0xff);

  // Options new to v2.03 begin here

  *demo_p++ = monster_infighting;   // killough 7/19/98

  *demo_p++ = dogs;                 // killough 7/19/98

  *demo_p++ = 0;
  *demo_p++ = 0;

  *demo_p++ = (distfriend >> 8) & 0xff;  // killough 8/8/98
  *demo_p++ =  distfriend       & 0xff;  // killough 8/8/98

  *demo_p++ = monster_backing;         // killough 9/8/98

  *demo_p++ = monster_avoid_hazards;    // killough 9/9/98

  *demo_p++ = monster_friction;         // killough 10/98

  *demo_p++ = help_friends;             // killough 9/9/98

  *demo_p++ = dog_jumping;

  *demo_p++ = monkeys;

  {   // killough 10/98: a compatibility vector now
    int i;
    for (i = 0; i < MBF_COMP_TOTAL; i++)
      *demo_p++ = comp[i] != 0;
  }

  // unused forceOldBsp
  *demo_p++ = 0;

  //----------------
  // Padding at end
  //----------------
  while (demo_p < target)
    *demo_p++ = 0;

  if (demo_p != target)
    I_Error("G_WriteOptions: dsda_GameOptionSize is too small");

  return target;
}

/* Same, but read instead of write
 * cph - const byte*'s
 */

const byte *G_ReadOptions(const byte *demo_p)
{
  const byte *target;

  if (mbf21)
  {
    return dsda_ReadOptions21(demo_p);
  }

  target = demo_p + dsda_GameOptionSize();

  monsters_remember = *demo_p++;

  variable_friction = *demo_p;  // ice & mud
  demo_p++;

  weapon_recoil = *demo_p;       // weapon recoil
  demo_p++;

  allow_pushers = *demo_p;      // MT_PUSH Things
  demo_p++;

  demo_p++;

  player_bobbing = *demo_p;     // whether player bobs or not
  demo_p++;

  // killough 3/6/98: add parameters to savegame, move from demo
  respawnparm = *demo_p++;
  fastparm = *demo_p++;
  nomonsters = *demo_p++;

  demo_insurance = *demo_p++;              // killough 3/31/98

  // killough 3/26/98: Added rngseed to demos; 3/31/98: moved here

  rngseed  = *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;

  // Options new to v2.03
  if (mbf_features)
  {
    monster_infighting = *demo_p++;   // killough 7/19/98

    dogs = *demo_p++;                 // killough 7/19/98

    demo_p += 2;

    distfriend = *demo_p++ << 8;      // killough 8/8/98
    distfriend+= *demo_p++;

    monster_backing = *demo_p++;     // killough 9/8/98

    monster_avoid_hazards = *demo_p++; // killough 9/9/98

    monster_friction = *demo_p++;      // killough 10/98

    help_friends = *demo_p++;          // killough 9/9/98

    dog_jumping = *demo_p++;           // killough 10/98

    monkeys = *demo_p++;

    {   // killough 10/98: a compatibility vector now
      int i;
      for (i = 0; i < MBF_COMP_TOTAL; i++)
        comp[i] = *demo_p++;
    }

    // unused forceOldBsp
    demo_p++;
  }
  else  /* defaults for versions <= 2.02 */
  {
    /* G_Compatibility will set these */
  }

  G_Compatibility();

  return target;
}

void G_BeginRecording (void)
{
}

//
// G_PlayDemo
//

static __thread const char *defdemoname;

void G_DeferedPlayDemo (const char* name)
{
  defdemoname = name;
  gameaction = ga_playdemo;
}

static int G_GetOriginalDoomCompatLevel(int ver)
{
  int level;

  level = dsda_CompatibilityLevel();
  if (level >= 0) return level;

  if (ver == 110) return tasdoom_compatibility;
  if (ver < 107) return doom_1666_compatibility;
  if (gamemode == retail) return ultdoom_compatibility;
  if (gamemission == pack_tnt || gamemission == pack_plut) return finaldoom_compatibility;
  return doom2_19_compatibility;
}

//e6y: Check for overrun
static dboolean CheckForOverrun(const byte *start_p, const byte *current_p, size_t maxsize, size_t size, dboolean failonerror)
{
  size_t pos = current_p - start_p;
  if (pos + size > maxsize)
  {
    if (failonerror)
      I_Error("G_ReadDemoHeader: wrong demo header\n");
    else
      return true;
  }
  return false;
}

const byte* G_ReadDemoHeaderEx(const byte *demo_p, size_t size, unsigned int params)
{
  return demo_p;
}

void G_StartDemoPlayback(const byte *buffer, int length, int behaviour)
{
}

static int LoadDemo(const char *name, const byte **buffer, int *length)
{
  return 0;
}

void P_ResetWalkcam(void)
{
}

void P_SyncWalkcam(dboolean sync_coords, dboolean sync_sight)
{
}

//e6y
void G_ContinueDemo(const char *playback_name)
{
  if (LoadDemo(playback_name, &demobuffer, &demolength))
  {
    G_BeginRecording();
  }
}

// heretic

static dboolean InventoryMoveLeft(void)
{
    return true;
}

static dboolean InventoryMoveRight(void)
{
    return true;
}

// hexen

void G_Completed(int map, int position, int flags, angle_t angle)
{
    secretexit = false;
    gameaction = ga_completed;
    dsda_UpdateLeaveData(map, position, flags, angle);
}

void G_DoTeleportNewMap(void)
{
    gamestate = GS_LEVEL;
    gameaction = ga_nothing;
    RebornPosition = leave_data.position;
}

void Hexen_G_DoReborn(int playernum)
{
    int i;
    dboolean oldWeaponowned[HEXEN_NUMWEAPONS];
    dboolean oldKeys[NUMCARDS];
    int oldPieces;
    dboolean foundSpot;
    int bestWeapon;

    if (!netgame)
    {
        gameaction = ga_loadlevel;
    }
    else
    {                           // Net-game
        players[playernum].mo->player = NULL;   // Dissassociate the corpse

        if (deathmatch)
        {                       // Spawn at random spot if in death match
            G_DeathMatchSpawnPlayer(playernum);
            return;
        }

        // Cooperative net-play, retain keys and weapons
        for (i = 0; i < NUMCARDS; ++i)
          oldKeys[i] = players[playernum].cards[i];
        oldPieces = players[playernum].pieces;
        for (i = 0; i < HEXEN_NUMWEAPONS; i++)
        {
            oldWeaponowned[i] = players[playernum].weaponowned[i];
        }

        foundSpot = false;
        if (G_CheckSpot(playernum, &playerstarts[RebornPosition][playernum]))
        {                       // Appropriate player start spot is open
            P_SpawnPlayer(playernum, &playerstarts[RebornPosition][playernum]);
            foundSpot = true;
        }
        else
        {
            // Try to spawn at one of the other player start spots
            for (i = 0; i < g_maxplayers; i++)
            {
                if (G_CheckSpot(playernum, &playerstarts[RebornPosition][i]))
                {               // Found an open start spot

                    // Fake as other player
                    playerstarts[RebornPosition][i].type = playernum + 1;
                    P_SpawnPlayer(playernum, &playerstarts[RebornPosition][i]);

                    // Restore proper player type
                    playerstarts[RebornPosition][i].type = i + 1;

                    foundSpot = true;
                    break;
                }
            }
        }

        if (foundSpot == false)
        {                       // Player's going to be inside something
            P_SpawnPlayer(playernum, &playerstarts[RebornPosition][playernum]);
        }

        // Restore keys and weapons
        for (i = 0; i < NUMCARDS; ++i)
          players[playernum].cards[i] = oldKeys[i];
        players[playernum].pieces = oldPieces;
        for (bestWeapon = 0, i = 0; i < HEXEN_NUMWEAPONS; i++)
        {
            if (oldWeaponowned[i])
            {
                bestWeapon = i;
                players[playernum].weaponowned[i] = true;
            }
        }
        players[playernum].ammo[MANA_1] = 25;
        players[playernum].ammo[MANA_2] = 25;
        if (bestWeapon)
        {                       // Bring up the best weapon
            players[playernum].pendingweapon = bestWeapon;
        }
    }
}

// Headless functions

void headlessGetMapName(char* outString)
{
  sprintf(outString, "%s", dsda_MapLumpName(gameepisode, gamemap));
}
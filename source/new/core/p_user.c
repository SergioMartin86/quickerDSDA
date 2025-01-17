/* Emacs style mode select   -*- C -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
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
 * DESCRIPTION:
 *      Player related stuff.
 *      Bobbing POV/weapon, movement.
 *      Pending weapon.
 *
 *-----------------------------------------------------------------------------*/

#include <math.h>

#include "doomstat.h"
#include "d_event.h"
#include "r_main.h"
#include "lprintf.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_enemy.h"
#include "p_spec.h"
#include "p_user.h"
#include "smooth.h"
#include "g_game.h"
#include "p_tick.h"
#include "e6y.h"//e6y

#include "dsda/aim.h"
#include "dsda/death.h"
#include "dsda/excmd.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/settings.h"

// heretic needs
#include "sounds.h"
#include "p_inter.h"
#include "m_random.h"

//
// Movement.
//

// 16 pixels of bob

#define MAXBOB  0x100000

dboolean onground; // whether player is on ground or in air

// heretic
int newtorch;      // used in the torch flicker effect.
int newtorchdelta;

fixed_t P_PlayerSpeed(player_t* player)
{
  double vx, vy;

  vx = (double) player->mo->momx / FRACUNIT;
  vy = (double) player->mo->momy / FRACUNIT;

  return (fixed_t) (sqrt(vx * vx + vy * vy) * FRACUNIT);
}

//
// P_Thrust
// Moves the given origin along a given angle.
//

void P_CompatiblePlayerThrust(player_t* player, angle_t angle, fixed_t move)
{
  player->mo->momx += FixedMul(move, finecosine[angle]);
  player->mo->momy += FixedMul(move, finesine[angle]);
}

void P_HereticPlayerThrust(player_t* player, angle_t angle, fixed_t move)
{
  if (player->powers[pw_flight] && !(player->mo->z <= player->mo->floorz))
  {
    player->mo->momx += FixedMul(move, finecosine[angle]);
    player->mo->momy += FixedMul(move, finesine[angle]);
  }
  else if (player->mo->subsector->sector->special == 15)
  {
    player->mo->momx += FixedMul(move >> 2, finecosine[angle]);
    player->mo->momy += FixedMul(move >> 2, finesine[angle]);
  }
  else
  {
    player->mo->momx += FixedMul(move, finecosine[angle]);
    player->mo->momy += FixedMul(move, finesine[angle]);
  }
}

void P_HexenPlayerThrust(player_t* player, angle_t angle, fixed_t move)
{
  if (player->powers[pw_flight] && !(player->mo->z <= player->mo->floorz))
  {
    player->mo->momx += FixedMul(move, finecosine[angle]);
    player->mo->momy += FixedMul(move, finesine[angle]);
  }
  else if (P_GetThingFloorType(player->mo) == FLOOR_ICE) // Friction_Low
  {
    player->mo->momx += FixedMul(move >> 1, finecosine[angle]);
    player->mo->momy += FixedMul(move >> 1, finesine[angle]);
  }
  else
  {
    player->mo->momx += FixedMul(move, finecosine[angle]);
    player->mo->momy += FixedMul(move, finesine[angle]);
  }
}

// In doom, P_Thrust is always player-originated
// In heretic / hexen P_Thrust can come from effects
// Need to differentiate the two because of the flight cheat
void P_ForwardThrust(player_t* player,angle_t angle,fixed_t move)
{
  angle >>= ANGLETOFINESHIFT;

  if ((player->mo->flags & MF_FLY) && player->mo->pitch != 0)
  {
    angle_t pitch = player->mo->pitch >> ANGLETOFINESHIFT;
    fixed_t zpush = FixedMul(move, finesine[pitch]);
    player->mo->momz -= zpush;
    move = FixedMul(move, finecosine[pitch]);
  }

  map_format.player_thrust(player, angle, move);
}

void P_Thrust(player_t* player,angle_t angle,fixed_t move)
{
  angle >>= ANGLETOFINESHIFT;

  map_format.player_thrust(player, angle, move);
}

/*
 * P_Bob
 * Same as P_Thrust, but only affects bobbing.
 *
 * killough 10/98: We apply thrust separately between the real physical player
 * and the part which affects bobbing. This way, bobbing only comes from player
 * motion, nothing external, avoiding many problems, e.g. bobbing should not
 * occur on conveyors, unless the player walks on one, and bobbing should be
 * reduced at a regular rate, even on ice (where the player coasts).
 */

static void P_Bob(player_t *player, angle_t angle, fixed_t move)
{
  //e6y
  if (!mbf_features && !prboom_comp[PC_PRBOOM_FRICTION].state)
    return;

  player->momx += FixedMul(move,finecosine[angle >>= ANGLETOFINESHIFT]);
  player->momy += FixedMul(move,finesine[angle]);
}

//
// P_CalcHeight
// Calculate the walking / running height adjustment
//

void P_CalcHeight (player_t* player)
{
  int     angle;
  fixed_t bob;

  // Regular movement bobbing
  // (needs to be calculated for gun swing
  // even if not on ground)
  // OPTIMIZE: tablify angle
  // Note: a LUT allows for effects
  //  like a ramp with low health.


  /* killough 10/98: Make bobbing depend only on player-applied motion.
   *
   * Note: don't reduce bobbing here if on ice: if you reduce bobbing here,
   * it causes bobbing jerkiness when the player moves from ice to non-ice,
   * and vice-versa.
   */

  player->bob = 0;

  if ((player->mo->flags & MF_FLY) && !onground)
  {
    player->bob = FRACUNIT / 2;
  }

  if (mbf_features)
  {
    if (player_bobbing)
    {
      player->bob = (FixedMul(player->momx, player->momx) +
                     FixedMul(player->momy, player->momy)) >> 2;
    }
  }
  else
  {
    if (demo_compatibility || player_bobbing || prboom_comp[PC_FORCE_INCORRECT_BOBBING_IN_BOOM].state)
    {
      player->bob = (FixedMul(player->mo->momx, player->mo->momx) +
        FixedMul(player->mo->momy, player->mo->momy)) >> 2;
    }
  }

  //e6y
  if (!prboom_comp[PC_PRBOOM_FRICTION].state &&
      compatibility_level >= boom_202_compatibility &&
      compatibility_level <= lxdoom_1_compatibility &&
      player->mo->friction > ORIG_FRICTION) // ice?
  {
    if (player->bob > (MAXBOB >> 2))
      player->bob = MAXBOB >> 2;
  }
  else
  {
    if (player->bob > MAXBOB)
      player->bob = MAXBOB;
  }

  if (player->mo->flags2 & MF2_FLY && !onground)
  {
    player->bob = FRACUNIT / 2;
  }

  if (!onground)
  {
    player->viewz = player->mo->z + g_viewheight;

    if (player->viewz > player->mo->ceilingz - 4 * FRACUNIT)
      player->viewz = player->mo->ceilingz - 4 * FRACUNIT;

    return;
  }

  angle = (FINEANGLES / 20 * leveltime) & FINEMASK;
  bob = dsda_ViewBob() ? FixedMul(player->bob / 2, finesine[angle]) : 0;

  // move viewheight

  if (player->playerstate == PST_LIVE)
  {
    player->viewheight += player->deltaviewheight;

    if (player->viewheight > g_viewheight)
    {
      player->viewheight = g_viewheight;
      player->deltaviewheight = 0;
    }

    if (player->viewheight < g_viewheight / 2)
    {
      player->viewheight = g_viewheight / 2;
      if (player->deltaviewheight <= 0)
        player->deltaviewheight = 1;
    }

    if (player->deltaviewheight)
    {
      player->deltaviewheight += FRACUNIT / 4;
      if (!player->deltaviewheight)
        player->deltaviewheight = 1;
    }
  }

  if (player->chickenTics || player->morphTics)
  {
    player->viewz = player->mo->z + player->viewheight - (20 * FRACUNIT);
  }
  else
  {
    player->viewz = player->mo->z + player->viewheight + bob;
  }

  if (player->playerstate != PST_DEAD && player->mo->z <= player->mo->floorz)
  {
    if (player->mo->floorclip)
      player->viewz -= player->mo->floorclip;
    else if (player->mo->flags2 & MF2_FEETARECLIPPED)
      player->viewz -= FOOTCLIPSIZE;
  }

  if (player->viewz > player->mo->ceilingz - 4 * FRACUNIT)
    player->viewz = player->mo->ceilingz - 4 * FRACUNIT;

}

//
// P_MovePlayer
//
// Adds momentum if the player is not in the air
//
// killough 10/98: simplified

void P_HandleExCmdLook(player_t* player)
{
  int look;

  look = player->cmd.ex.look;
  if (look)
  {
    if (look == XC_LOOK_RESET)
    {
      player->mo->pitch = 0;
    }
    else
    {
      player->mo->pitch += look << 16;
      CheckPitch((signed int *) &player->mo->pitch);
    }
  }
}

void P_MovePlayer (player_t* player)
{
  ticcmd_t *cmd;
  mobj_t *mo;

  P_HandleExCmdLook(player);

  cmd = &player->cmd;
  mo = player->mo;
  mo->angle += cmd->angleturn << 16;

  if (demo_smoothturns && player == &players[displayplayer])
  {
    R_SmoothPlaying_Add(cmd->angleturn << 16);
  }

  onground = (mo->z <= mo->floorz || mo->flags2 & MF2_ONMOBJ);

  if ((player->mo->flags & MF_FLY) && player == &players[consoleplayer] && upmove != 0)
  {
    mo->momz = upmove << 8;
  }

  // killough 10/98:
  //
  // We must apply thrust to the player and bobbing separately, to avoid
  // anomalies. The thrust applied to bobbing is always the same strength on
  // ice, because the player still "works just as hard" to move, while the
  // thrust applied to the movement varies with 'movefactor'.

  //e6y
  if ((!demo_compatibility && !mbf_features && !prboom_comp[PC_PRBOOM_FRICTION].state) ||
    (cmd->forwardmove | cmd->sidemove)) // killough 10/98
    {
      if (onground || mo->flags & MF_BOUNCES || (mo->flags & MF_FLY)) // killough 8/9/98
      {
        int friction, movefactor = P_GetMoveFactor(mo, &friction);

        // killough 11/98:
        // On sludge, make bobbing depend on efficiency.
        // On ice, make it depend on effort.

        int bobfactor =
          friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;

        if (map_format.zdoom && !movefactor)
          bobfactor = movefactor;

        if (cmd->forwardmove)
        {
          P_Bob(player,mo->angle,cmd->forwardmove*bobfactor);
          P_ForwardThrust(player,mo->angle,cmd->forwardmove*movefactor);
        }

        if (cmd->sidemove)
        {
          P_Bob(player,mo->angle-ANG90,cmd->sidemove*bobfactor);
          P_Thrust(player,mo->angle-ANG90,cmd->sidemove*movefactor);
        }
      }
      else if (map_info.air_control)
      {
        int friction, movefactor = P_GetMoveFactor(mo, &friction);

        movefactor = FixedMul(movefactor, map_info.air_control);

        if (cmd->forwardmove)
        {
          P_Bob(player, mo->angle, cmd->forwardmove * movefactor);
          P_Thrust(player, player->mo->angle, cmd->forwardmove * movefactor);
        }

        if (cmd->sidemove)
        {
          P_Bob(player, mo->angle - ANG90, cmd->sidemove * movefactor);
          P_Thrust(player, player->mo->angle - ANG90, cmd->sidemove * movefactor);
        }
      }
      if (mo->state == states+S_PLAY)
        P_SetMobjState(mo,S_PLAY_RUN1);
    }
}

#define ANG5 (ANG90/18)

//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//

void P_DeathThink (player_t* player)
{
  angle_t angle;
  angle_t delta;

  P_MovePsprites(player);

  // fall to the ground

  onground = (player->mo->z <= player->mo->floorz);
  if (player->mo->type == g_skullpop_mt)
  {
    // Flying bloody skull
    player->viewheight = 6*FRACUNIT;
    player->deltaviewheight = 0;
    if (onground)
    {
if ((int)player->mo->pitch > -(int)ANG1*19)
      {
        player->mo->pitch -= ((int)ANG1*19 - player->mo->pitch) / 8;
      }
    }
  }
  else if (!(player->mo->flags2 & MF2_ICEDAMAGE))
  {
    if (player->viewheight > 6*FRACUNIT)
      player->viewheight -= FRACUNIT;

    if (player->viewheight < 6*FRACUNIT)
      player->viewheight = 6*FRACUNIT;

    player->deltaviewheight = 0;

    if (dsda_FreeAim())
    {
      const int delta = dsda_LookDirToPitch(-6);

      if ((int) player->mo->pitch > 0)
      {
         player->mo->pitch -= delta;
      }
      else if ((int) player->mo->pitch < 0)
      {
         player->mo->pitch += delta;
      }

      if (abs((int) player->mo->pitch) < delta)
      {
         player->mo->pitch = 0;
      }
    }
    else
    {
      if (player->lookdir > 0)
      {
          player->lookdir -= 6;
      }
      else if (player->lookdir < 0)
      {
          player->lookdir += 6;
      }
      if (abs(player->lookdir) < 6)
      {
          player->lookdir = 0;
      }
    }
  }

  P_CalcHeight(player);

  if (player->attacker && player->attacker != player->mo)
  {
      angle = R_PointToAngle2(player->mo->x,
                              player->mo->y,
                              player->attacker->x,
                              player->attacker->y);

      delta = angle - player->mo->angle;

      if (delta < ANG5 || delta > (unsigned)-ANG5)
      {
        // Looking at killer,
        //  so fade damage flash down.

        player->mo->angle = angle;

        if (player->damagecount)
          player->damagecount--;
      }
      else if (delta < ANG180)
        player->mo->angle += ANG5;
      else
        player->mo->angle -= ANG5;
  }
  else if (player->damagecount || player->poisoncount)
  {
    if (player->damagecount)
      player->damagecount--;
    else
      player->poisoncount--;
  }

  if (player->cmd.buttons & BT_USE)
  {
    dsda_DeathUse(player);
  }

  R_SmoothPlaying_Reset(player); // e6y
}

void P_PlayerEndFlight(player_t * player)
{
  if (player->mo->z != player->mo->floorz)
  {
    player->centering = true;
  }

  player->mo->flags2 &= ~MF2_FLY;
  player->mo->flags &= ~MF_NOGRAVITY;
}

//
// P_PlayerThink
//

void P_PlayerThink (player_t* player)
{
  ticcmd_t*    cmd;
  weapontype_t newweapon;
  int floorType;

  // killough 2/8/98, 3/21/98:
  if (player->cheats & CF_NOCLIP)
    player->mo->flags |= MF_NOCLIP;
  else
    player->mo->flags &= ~MF_NOCLIP;

  // chain saw run forward

  cmd = &player->cmd;
  if (player->mo->flags & MF_JUSTATTACKED)
  {
    cmd->angleturn = 0;
    cmd->forwardmove = 0xc800 / 512;
    cmd->sidemove = 0;
    player->mo->flags &= ~MF_JUSTATTACKED;
  }

  if (player->playerstate == PST_DEAD)
  {
    P_DeathThink(player);
    return;
  }

  if (player->jumpTics)
  {
    player->jumpTics--;
  }

  // Move around.
  // Reactiontime is used to prevent movement
  //  for a bit after a teleport.

  if (player->mo->reactiontime)
    player->mo->reactiontime--;
  else
  {
    P_MovePlayer(player);

  }

  P_CalcHeight (player); // Determines view height and bobbing

  // Determine if there's anything about the sector you're in that's
  // going to affect you, like painful floors.

  if (P_IsSpecialSector(player->mo->subsector->sector))
    P_PlayerInSpecialSector(player);

  if (dsda_AllowExCmd())
  {
    if (cmd->ex.actions & XC_JUMP && onground && !player->jumpTics)
    {
      player->mo->momz = g_jump * FRACUNIT;
      player->mo->flags2 &= ~MF2_ONMOBJ;
      player->jumpTics = 18;
    }
  }

  // Check for weapon change.
  if (cmd->buttons & BT_CHANGE && !player->morphTics)
  {
    // The actual changing of the weapon is done
    //  when the weapon psprite can do it
    //  (read: not in the middle of an attack).

    newweapon = (cmd->buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT;

    // killough 3/22/98: For demo compatibility we must perform the fist
    // and SSG weapons switches here, rather than in G_BuildTiccmd(). For
    // other games which rely on user preferences, we must use the latter.

    if (demo_compatibility)
    { // compatibility mode -- required for old demos -- killough
      //e6y
      if (!prboom_comp[PC_ALLOW_SSG_DIRECT].state)
        newweapon = (cmd->buttons & BT_WEAPONMASK_OLD)>>BT_WEAPONSHIFT;

        if (
          newweapon == g_wp_fist && player->weaponowned[g_wp_chainsaw]
          && (
            player->readyweapon != g_wp_chainsaw ||
            (!player->powers[pw_strength])
          )
        )
          newweapon = g_wp_chainsaw;

        if (
            gamemode == commercial &&
            newweapon == wp_shotgun &&
            player->weaponowned[wp_supershotgun] &&
            player->readyweapon != wp_supershotgun)
          newweapon = wp_supershotgun;
    }

    // killough 2/8/98, 3/22/98 -- end of weapon selection changes

    if (player->weaponowned[newweapon] && newweapon != player->readyweapon)

      // Do not go to plasma or BFG in shareware,
      //  even if cheated.

      // heretic_note: ignoring this...not sure it's worth worrying about
      if ((newweapon != wp_plasma && newweapon != wp_bfg)
          || (gamemode != shareware) )
        player->pendingweapon = newweapon;
  }

  // check for use

  if (cmd->buttons & BT_USE)
  {
    if (!player->usedown)
    {
      P_UseLines(player);
      player->usedown = true;
    }
  }
  else
    player->usedown = false;

  // cycle psprites
  P_MovePsprites (player);

  // Counters, time dependent power ups.

  // Strength counts up to diminish fade.

  if (player->powers[pw_strength])
    player->powers[pw_strength]++;

  // killough 1/98: Make idbeholdx toggle:

  if (player->powers[pw_invulnerability] > 0) // killough
  {
    if (player->pclass == PCLASS_CLERIC)
    {
      if (!(leveltime & 7) && player->mo->flags & MF_SHADOW
          && !(player->mo->flags2 & MF2_DONTDRAW))
      {
        player->mo->flags &= ~MF_SHADOW;
        if (!(player->mo->flags & MF_ALTSHADOW))
        {
          player->mo->flags2 |= MF2_DONTDRAW | MF2_NONSHOOTABLE;
        }
      }
      if (!(leveltime & 31))
      {
        if (player->mo->flags2 & MF2_DONTDRAW)
        {
          if (!(player->mo->flags & MF_SHADOW))
          {
            player->mo->flags |= MF_SHADOW | MF_ALTSHADOW;
          }
          else
          {
            player->mo->flags2 &=
                ~(MF2_DONTDRAW | MF2_NONSHOOTABLE);
          }
        }
        else
        {
          player->mo->flags |= MF_SHADOW;
          player->mo->flags &= ~MF_ALTSHADOW;
        }
      }
    }

    if (!(--player->powers[pw_invulnerability]))
    {
      player->mo->flags2 &= ~(MF2_INVULNERABLE | MF2_REFLECTIVE);
      if (player->pclass == PCLASS_CLERIC)
      {
        player->mo->flags2 &= ~(MF2_DONTDRAW | MF2_NONSHOOTABLE);
        player->mo->flags &= ~(MF_SHADOW | MF_ALTSHADOW);
      }
    }
  }

  if (player->powers[pw_minotaur])
    player->powers[pw_minotaur]--;

  if (player->powers[pw_speed])
    player->powers[pw_speed]--;

  if (player->powers[pw_invisibility] > 0)    // killough
    if (! --player->powers[pw_invisibility] )
      player->mo->flags &= ~MF_SHADOW;

  if (player->powers[pw_infrared] > 0)        // killough
    player->powers[pw_infrared]--;

  if (player->powers[pw_ironfeet] > 0)        // killough
    player->powers[pw_ironfeet]--;

  if (player->powers[pw_flight])
  {
    if (!--player->powers[pw_flight])
    {
      P_PlayerEndFlight(player);
    }
  }

  if (player->powers[pw_weaponlevel2])
  {
    if (!--player->powers[pw_weaponlevel2])
    {
      if ((player->readyweapon == wp_phoenixrod)
          && (player->psprites[ps_weapon].state
              != &states[HERETIC_S_PHOENIXREADY])
          && (player->psprites[ps_weapon].state
              != &states[HERETIC_S_PHOENIXUP]))
      {
        P_SetPsprite(player, ps_weapon, HERETIC_S_PHOENIXREADY);
        player->ammo[am_phoenixrod] -= USE_PHRD_AMMO_2;
        player->refire = 0;
      }
      else if ((player->readyweapon == wp_gauntlets)
               || (player->readyweapon == wp_staff))
      {
        player->pendingweapon = player->readyweapon;
      }
    }
  }

  if (player->damagecount)
    player->damagecount--;

  if (player->bonuscount)
    player->bonuscount--;

  if (player->hazardcount)
  {
    player->hazardcount--;
    if (!(leveltime % player->hazardinterval) && player->hazardcount > 16 * TICRATE)
      P_DamageMobj(player->mo, NULL, NULL, 5);
  }

  // Handling colormaps.
  // killough 3/20/98: reformat to terse C syntax
    player->fixedcolormap = dsda_PowerPalette() &&
      (player->powers[pw_invulnerability] > 4*32 ||
      player->powers[pw_invulnerability] & 8) ? INVERSECOLORMAP :
      player->powers[pw_infrared] > 4*32 || player->powers[pw_infrared] & 8;
}

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
 *      Handling interactions (i.e., collisions).
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "m_random.h"
#include "r_main.h"
#include "smooth.h"
#include "sounds.h"
#include "p_tick.h"
#include "lprintf.h"

#include "p_inter.h"
#include "p_enemy.h"
#include "p_spec.h"
#include "p_pspr.h"
#include "p_user.h"

#include "p_inter.h"
#include "e6y.h"//e6y

#include "dsda.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/skill_info.h"

// Ty 03/07/98 - add deh externals
// Maximums and such were hardcoded values.  Need to externalize those for
// dehacked support (and future flexibility).  Most var names came from the key
// strings used in dehacked.

int initial_health = 100;
int initial_bullets = 50;
int maxhealth = 100; // was MAXHEALTH as a #define, used only in this module
int maxhealthbonus = 200;
int max_armor = 200;
int green_armor_class = 1;  // these are involved with armortype below
int blue_armor_class = 2;
int max_soul = 200;
int soul_health = 100;
int mega_health = 200;
int god_health = 100;   // these are used in cheats (see st_stuff.c)
int idfa_armor = 200;
int idfa_armor_class = 2;
// not actually used due to pairing of cheat_k and cheat_fa
int idkfa_armor = 200;
int idkfa_armor_class = 2;

int bfgcells = 40;      // used in p_pspr.c
int monsters_infight = 0; // e6y: Dehacked support - monsters infight
// Ty 03/07/98 - end deh externals

// a weapon is found with two clip loads,
// a big item has five clip loads
int maxammo[NUMAMMO]  = {200, 50, 300, 50, 0, 0}; // heretic +2 ammo types
int clipammo[NUMAMMO] = { 10,  4,  20,  1, 0, 0}; // heretic +2 ammo types

//
// GET STUFF
//

// heretic
static weapontype_t GetAmmoChange[] = {
    wp_goldwand,
    wp_crossbow,
    wp_blaster,
    wp_skullrod,
    wp_phoenixrod,
    wp_mace
};

//
// P_AutoSwitchWeapon
// Autoswitches player to a weapon,
// Based on config and other conditions.
//

static void P_AutoSwitchWeapon(player_t *player, weapontype_t weapon)
{
  player->pendingweapon = weapon;
}

//
// P_GiveAmmo
// Num is the number of clip loads,
// not the individual count (0= 1/2 clip).
// Returns false if the ammo can't be picked up at all
//

static dboolean P_GiveAmmoAutoSwitch(player_t *player, ammotype_t ammo, int oldammo)
{
  int i;

  if (
    weaponinfo[player->readyweapon].flags & WPF_AUTOSWITCHFROM &&
    weaponinfo[player->readyweapon].ammo != ammo
  )
  {
    for (i = NUMWEAPONS - 1; i > player->readyweapon; --i)
    {
      if (
        player->weaponowned[i] &&
        !(weaponinfo[i].flags & WPF_NOAUTOSWITCHTO) &&
        weaponinfo[i].ammo == ammo &&
        weaponinfo[i].ammopershot > oldammo &&
        weaponinfo[i].ammopershot <= player->ammo[ammo]
      )
      {
        P_AutoSwitchWeapon(player, i);
        break;
      }
    }
  }

  return true;
}

static dboolean P_GiveAmmo(player_t *player, ammotype_t ammo, int num)
{
  int oldammo;

  if (ammo == am_noammo)
    return false;

#ifdef RANGECHECK
  if (ammo < 0 || ammo > NUMAMMO)
    I_Error ("P_GiveAmmo: bad type %i", ammo);
#endif

  if ( player->ammo[ammo] == player->maxammo[ammo]  )
    return false;

  if (num)
    num *= clipammo[ammo];
  else
    num = clipammo[ammo]/2;

  if (skill_info.ammo_factor)
    num = FixedMul(num, skill_info.ammo_factor);

  oldammo = player->ammo[ammo];
  player->ammo[ammo] += num;

  if (player->ammo[ammo] > player->maxammo[ammo])
    player->ammo[ammo] = player->maxammo[ammo];

  if (mbf21)
    return P_GiveAmmoAutoSwitch(player, ammo, oldammo);

  // If non zero ammo, don't change up weapons, player was lower on purpose.
  if (oldammo)
    return true;

  // We were down to zero, so select a new weapon.
  // Preferences are not user selectable.

  switch (ammo)
    {
    case am_clip:
      if (player->readyweapon == wp_fist) {
        if (player->weaponowned[wp_chaingun])
          P_AutoSwitchWeapon(player, wp_chaingun);
        else
          P_AutoSwitchWeapon(player, wp_pistol);
      }
      break;

    case am_shell:
      if (player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
        if (player->weaponowned[wp_shotgun])
          P_AutoSwitchWeapon(player, wp_shotgun);
      break;

      case am_cell:
        if (player->readyweapon == wp_fist || player->readyweapon == wp_pistol)
          if (player->weaponowned[wp_plasma])
            P_AutoSwitchWeapon(player, wp_plasma);
        break;

      case am_misl:
        if (player->readyweapon == wp_fist)
          if (player->weaponowned[wp_missile])
            P_AutoSwitchWeapon(player, wp_missile);
    default:
      break;
    }
  return true;
}

//
// P_GiveWeapon
// The weapon name may have a MF_DROPPED flag ored in.
//

dboolean P_GiveWeapon(player_t *player, weapontype_t weapon, dboolean dropped)
{
  dboolean gaveammo;
  dboolean gaveweapon;

  if (netgame && deathmatch!=2 && !dropped)
    {
      // leave placed weapons forever on net games
      if (player->weaponowned[weapon])
        return false;

      player->bonuscount += BONUSADD;
      player->weaponowned[weapon] = true;

      P_GiveAmmo(player, weaponinfo[weapon].ammo, deathmatch ? 5 : 2);
      P_AutoSwitchWeapon(player, weapon);
      /* cph 20028/10 - for old-school DM addicts, allow old behavior
       * where only consoleplayer's pickup sounds are heard */
      // displayplayer, not consoleplayer, for viewing multiplayer demos
      return false;
    }

  if (weaponinfo[weapon].ammo != am_noammo)
    {
      // give one clip with a dropped weapon,
      // two clips with a found weapon
      gaveammo = P_GiveAmmo (player, weaponinfo[weapon].ammo, dropped ? 1 : 2);
    }
  else
    gaveammo = false;

  if (player->weaponowned[weapon])
    gaveweapon = false;
  else
    {
      gaveweapon = true;
      player->weaponowned[weapon] = true;
      P_AutoSwitchWeapon(player, weapon);
    }
  return gaveweapon || gaveammo;
}

int P_PlayerHealthIncrease(int value)
{
  if (skill_info.health_factor)
    value = FixedMul(value, skill_info.health_factor);

  return value;
}

int P_PlayerArmorIncrease(int value)
{
  if (skill_info.armor_factor)
    value = FixedMul(value, skill_info.armor_factor);

  return value;
}

//
// P_GiveBody
// Returns false if the body isn't needed at all
//

dboolean P_GiveBody(player_t * player, int num)
{
    int max;

    max = maxhealth;
    if (player->chickenTics)
    {
        max = MAXCHICKENHEALTH;
    }
    if (player->morphTics)
    {
        max = MAXMORPHHEALTH;
    }
    if (player->health >= max)
    {
        return (false);
    }
    player->health += P_PlayerHealthIncrease(num);
    if (player->health > max)
    {
        player->health = max;
    }
    player->mo->health = player->health;
    return (true);
}

void P_HealMobj(mobj_t *mo, int num)
{
  player_t *player = mo->player;

  if (mo->health <= 0 || (player && player->playerstate == PST_DEAD))
    return;

  if (player)
  {
    P_GiveBody(player, num);
    return;
  }
  else
  {
    int max = P_MobjSpawnHealth(mo);

    mo->health += num;
    if (mo->health > max)
      mo->health = max;
  }
}

//
// P_GiveArmor
// Returns false if the armor is worse
// than the current armor.
//

static dboolean P_GiveArmor(player_t *player, int armortype)
{
  int hits = P_PlayerArmorIncrease(armortype * 100);
  if (player->armorpoints[ARMOR_ARMOR] >= hits)
    return false;   // don't pick up
  player->armortype = armortype;
  player->armorpoints[ARMOR_ARMOR] = hits;
  return true;
}

//
// P_GiveCard
//

void P_GiveCard(player_t *player, card_t card)
{
  if (player->cards[card])
    return;
  player->bonuscount = BONUSADD;
  player->cards[card] = 1;
}

//
// P_GivePower
//
// Rewritten by Lee Killough
//

dboolean P_GivePower(player_t *player, int power)
{
  static const int tics[NUMPOWERS] = {
    INVULNTICS, 1 /* strength */, INVISTICS,
    IRONTICS, 1 /* allmap */, INFRATICS,
    WPNLEV2TICS, FLIGHTTICS, 1 /* shield */, 1 /* health2 */,
    SPEEDTICS, MAULATORTICS
   };


  switch (power)
  {
    case pw_invulnerability:
      break;
    case pw_invisibility:
      player->mo->flags |= MF_SHADOW;
      break;
    case pw_allmap:
      if (player->powers[pw_allmap])
        return false;
      break;
    case pw_strength:
      P_GiveBody(player,100);
      break;
    case pw_flight:
      player->mo->flags2 |= MF2_FLY;
      player->mo->flags |= MF_NOGRAVITY;
      if (player->mo->z <= player->mo->floorz)
      {
          player->flyheight = 10;     // thrust the player in the air a bit
      }
      break;
  }

  // Unless player has infinite duration cheat, set duration (killough)

  if (player->powers[power] >= 0)
    player->powers[power] = tics[power];
  return true;
}

//
// P_TouchSpecialThing
//

void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher)
{
  player_t *player;
  int      i;
  int      sound;
  fixed_t  delta = special->z - toucher->z;

  if (delta > toucher->height || delta < -8*FRACUNIT)
    return;        // out of reach

  sound = sfx_itemup;
  player = toucher->player;

  // Dead thing touching.
  // Can happen with a sliding player corpse.
  if (toucher->health <= 0)
    return;

    // Identify by sprite.
  switch (special->sprite)
    {
      // armor
    case SPR_ARM1:
      if (!P_GiveArmor (player, green_armor_class))
        return;
      break;

    case SPR_ARM2:
      if (!P_GiveArmor (player, blue_armor_class))
        return;
      break;

        // bonus items
    case SPR_BON1:
      // can go over 100%
      player->health += P_PlayerHealthIncrease(1);
      if (player->health > (maxhealthbonus))//e6y
        player->health = (maxhealthbonus);//e6y
      player->mo->health = player->health;
      break;

    case SPR_BON2:
      // can go over 100%
      player->armorpoints[ARMOR_ARMOR] += P_PlayerArmorIncrease(1);
      // e6y
      // Doom 1.2 does not do check of armor points on overflow.
      // If you set the "IDKFA Armor" to MAX_INT (DWORD at 0x00064B5A -> FFFFFF7F)
      // and pick up one or more armor bonuses, your armor becomes negative
      // and you will die after reception of any damage since this moment.
      // It happens because the taken health damage depends from armor points
      // if they are present and becomes equal to very large value in this case
      if (player->armorpoints[ARMOR_ARMOR] > max_armor && compatibility_level != doom_12_compatibility)
        player->armorpoints[ARMOR_ARMOR] = max_armor;
      // e6y
      // We always give armor type 1 for the armor bonuses;
      // dehacked only affects the GreenArmor.
      if (!player->armortype)
        player->armortype =
         ((!demo_compatibility || prboom_comp[PC_APPLY_GREEN_ARMOR_CLASS_TO_ARMOR_BONUSES].state) ?
          green_armor_class : 1);
      break;

    case SPR_SOUL:
      player->health += P_PlayerHealthIncrease(soul_health);
      if (player->health > max_soul)
        player->health = max_soul;
      player->mo->health = player->health;
      sound = sfx_getpow;
      break;

    case SPR_MEGA:
      if (gamemode != commercial)
        return;
      player->health = mega_health;
      player->mo->health = player->health;
      // e6y
      // We always give armor type 2 for the megasphere;
      // dehacked only affects the MegaArmor.
      P_GiveArmor (player,
         ((!demo_compatibility || prboom_comp[PC_APPLY_BLUE_ARMOR_CLASS_TO_MEGASPHERE].state) ?
          blue_armor_class : 2));
      sound = sfx_getpow;
      break;

        // cards
        // leave cards for everyone
    case SPR_BKEY:
      if (!player->cards[it_bluecard])
      P_GiveCard (player, it_bluecard);
      if (!netgame)
        break;
      return;

    case SPR_YKEY:
      if (!player->cards[it_yellowcard])
      P_GiveCard (player, it_yellowcard);
      if (!netgame)
        break;
      return;

    case SPR_RKEY:
      if (!player->cards[it_redcard])
      P_GiveCard (player, it_redcard);
      if (!netgame)
        break;
      return;

    case SPR_BSKU:
      if (!player->cards[it_blueskull])
      P_GiveCard (player, it_blueskull);
      if (!netgame)
        break;
      return;

    case SPR_YSKU:
      if (!player->cards[it_yellowskull])
      P_GiveCard (player, it_yellowskull);
      if (!netgame)
        break;
      return;

    case SPR_RSKU:
      if (!player->cards[it_redskull])
      P_GiveCard (player, it_redskull);
      if (!netgame)
        break;
      return;

      // medikits, heals
    case SPR_STIM:
      if (!P_GiveBody (player, 10))
        return;
      break;

    case SPR_MEDI:
      if (!P_GiveBody (player, 25))
        return;
      break;


      // power ups
    case SPR_PINV:
      if (!P_GivePower (player, pw_invulnerability))
        return;
      sound = sfx_getpow;
      break;

    case SPR_PSTR:
      if (!P_GivePower (player, pw_strength))
        return;
      if (player->readyweapon != wp_fist)
        P_AutoSwitchWeapon(player, wp_fist);
      sound = sfx_getpow;
      break;

    case SPR_PINS:
      if (!P_GivePower (player, pw_invisibility))
        return;
      sound = sfx_getpow;
      break;

    case SPR_SUIT:
      if (!P_GivePower (player, pw_ironfeet))
        return;
      sound = sfx_getpow;
      break;

    case SPR_PMAP:
      if (!P_GivePower (player, pw_allmap))
        return;
      sound = sfx_getpow;
      break;

    case SPR_PVIS:
      if (!P_GivePower (player, pw_infrared))
        return;
      sound = sfx_getpow;
      break;

      // ammo
    case SPR_CLIP:
      if (special->flags & MF_DROPPED)
        {
          if (!P_GiveAmmo (player,am_clip,0))
            return;
        }
      else
        {
          if (!P_GiveAmmo (player,am_clip,1))
            return;
        }
      break;

    case SPR_AMMO:
      if (!P_GiveAmmo (player, am_clip,5))
        return;
      break;

    case SPR_ROCK:
      if (!P_GiveAmmo (player, am_misl,1))
        return;
      break;

    case SPR_BROK:
      if (!P_GiveAmmo (player, am_misl,5))
        return;
      break;

    case SPR_CELL:
      if (!P_GiveAmmo (player, am_cell,1))
        return;
      break;

    case SPR_CELP:
      if (!P_GiveAmmo (player, am_cell,5))
        return;
      break;

    case SPR_SHEL:
      if (!P_GiveAmmo (player, am_shell,1))
        return;
      break;

    case SPR_SBOX:
      if (!P_GiveAmmo (player, am_shell,5))
        return;
      break;

    case SPR_BPAK:
      if (!player->backpack)
        {
          for (i=0 ; i<NUMAMMO ; i++)
            player->maxammo[i] *= 2;
          player->backpack = true;
        }
      for (i=0 ; i<NUMAMMO ; i++)
        P_GiveAmmo (player, i, 1);
      break;

        // weapons
    case SPR_BFUG:
      if (!P_GiveWeapon (player, wp_bfg, false) )
        return;
      sound = sfx_wpnup;
      break;

    case SPR_MGUN:
      if (!P_GiveWeapon (player, wp_chaingun, (special->flags&MF_DROPPED)!=0) )
        return;
      sound = sfx_wpnup;
      break;

    case SPR_CSAW:
      if (!P_GiveWeapon (player, wp_chainsaw, false) )
        return;
      sound = sfx_wpnup;
      break;

    case SPR_LAUN:
      if (!P_GiveWeapon (player, wp_missile, false) )
        return;
      sound = sfx_wpnup;
      break;

    case SPR_PLAS:
      if (!P_GiveWeapon (player, wp_plasma, false) )
        return;
      sound = sfx_wpnup;
      break;

    case SPR_SHOT:
      if (!P_GiveWeapon (player, wp_shotgun, (special->flags&MF_DROPPED)!=0 ) )
        return;
      sound = sfx_wpnup;
      break;

    case SPR_SGN2:
      if (!P_GiveWeapon(player, wp_supershotgun, (special->flags&MF_DROPPED)!=0))
        return;
      sound = sfx_wpnup;
      break;

    default:
      I_Error ("P_SpecialThing: Unknown gettable thing");
    }

  if (special->special)
  {
    map_format.execute_line_special(special->special, special->special_args, NULL, 0, player->mo);
    special->special = 0;
  }

  if (special->flags & MF_COUNTITEM)
    player->itemcount++;

  if (special->flags2 & MF2_COUNTSECRET)
    P_PlayerCollectSecret(player);

  P_RemoveMobj (special);
  player->bonuscount += BONUSADD;

}

//
// KillMobj
//

static mobj_t *ActiveMinotaur(player_t * master);

// killough 11/98: make static
static void P_KillMobj(mobj_t *source, mobj_t *target)
{
  mobjtype_t item;
  mobj_t     *mo;
  int xdeath_limit;

  target->flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);

  if (target->type != MT_SKULL)
    target->flags &= ~MF_NOGRAVITY;

  target->flags |= MF_CORPSE|MF_DROPOFF;
  target->height >>= 2;

  // heretic
  target->flags2 &= ~MF2_PASSMOBJ;

  if (
    mbf21 || (
      compatibility_level == mbf_compatibility &&
      !prboom_comp[PC_MBF_REMOVE_THINKER_IN_KILLMOBJ].state
    )
  )
  {
    // killough 8/29/98: remove from threaded list
    P_UpdateThinker(&target->thinker);
  }

  if (!((target->flags ^ MF_COUNTKILL) & (MF_FRIEND | MF_COUNTKILL)))
    totallive--;

  if (map_format.hexen && target->special)
  {
    map_format.execute_line_special(
      target->special, target->special_args, NULL, 0,
      map_info.flags & MI_ACTIVATE_OWN_DEATH_SPECIALS ? target : source
    );
  }

  if (source && source->player)
  {
    if (target->player)
    {
      source->player->frags[target->player-players]++;

    }
  }
  else
    if (target->flags & MF_COUNTKILL) { /* Add to kills tally */
      if ((compatibility_level < lxdoom_1_compatibility) || !netgame) {
        if (!netgame)
        {
        }
        else
        {
          if (!deathmatch) {
            if (target->lastenemy && target->lastenemy->health > 0 && target->lastenemy->player)
            {
            }
            else
            {
              unsigned int player;
              for (player = 0; player < g_maxplayers; player++)
              {
                if (playeringame[player])
                {
                  break;
                }
              }
            }
          }
        }
      }
      else
        if (!deathmatch) {
          // try and find a player to give the kill to, otherwise give the
          // kill to a random player.  this fixes the missing monsters bug
          // in coop - rain
          // CPhipps - not a bug as such, but certainly an inconsistency.
          if (target->lastenemy && target->lastenemy->health > 0 && target->lastenemy->player) // Fighting a player
          {
          }
          else {
            // cph - randomely choose a player in the game to be credited
            //  and do it uniformly between the active players
            unsigned int activeplayers = 0, player, i;

            for (player = 0; player < g_maxplayers; player++)
              if (playeringame[player])
                activeplayers++;

            if (activeplayers) {
              player = P_Random(pr_friends) % activeplayers;

              for (i = 0; i < g_maxplayers; i++)
                if (playeringame[i])
                  if (!player--)
                  {
                  }
            }
          }
        }
    }

  if (target->player)
  {
    // count environment kills against you
    if (!source)
      target->player->frags[target->player-players]++;

    target->flags &= ~MF_SOLID;

    // heretic
    target->flags2 &= ~MF2_FLY;
    target->player->powers[pw_flight] = 0;
    target->player->powers[pw_weaponlevel2] = 0;

    target->player->playerstate = PST_DEAD;
    P_DropWeapon (target->player);
  }

    xdeath_limit = P_MobjSpawnHealth(target);
    if (target->health < -xdeath_limit && target->info->xdeathstate)
      P_SetMobjState (target, target->info->xdeathstate);
    else
      P_SetMobjState (target, target->info->deathstate);

  target->tics -= P_Random(pr_killtics)&3;

  if (target->tics < 1)
    target->tics = 1;

  // In Chex Quest, monsters don't drop items.
  if (gamemission == chex)
  {
    return;
  }

  // Drop stuff.
  // This determines the kind of object spawned
  // during the death frame of a thing.

  if (target->info->droppeditem != MT_NULL)
  {
    item = target->info->droppeditem;
  }
  else return;

  mo = P_SpawnMobj (target->x,target->y,ONFLOORZ, item);
  mo->flags |= MF_DROPPED;    // special versions of items

  if (target->momx == 0 && target->momy == 0)
  {
    target->flags |= MF_FOREGROUND;
  }
}

//
// P_DamageMobj
// Damages both enemies and players
// "inflictor" is the thing that caused the damage
//  creature or missile, can be NULL (slime, etc)
// "source" is the thing to target after taking damage
//  creature or NULL
// Source and inflictor are the same for melee attacks.
// Source can be NULL for slime, barrel explosions
// and other environmental stuff.
//

static dboolean P_InfightingImmune(mobj_t *target, mobj_t *source)
{
  return // not default behaviour, and same group
    mobjinfo[target->type].infighting_group != IG_DEFAULT &&
    mobjinfo[target->type].infighting_group == mobjinfo[source->type].infighting_group;
}

static dboolean P_MorphMonster(mobj_t * actor);

void P_DamageMobj(mobj_t *target,mobj_t *inflictor, mobj_t *source, int damage)
{
  player_t *player;
  dboolean justhit = false;          /* killough 11/98 */

  /* killough 8/31/98: allow bouncers to take damage */
  if (!(target->flags & (MF_SHOOTABLE | MF_BOUNCES)))
    return; // shouldn't happen...

  if (target->health <= 0)
  {
    // hexen
    if (inflictor && inflictor->flags2 & MF2_ICEDAMAGE)
    {
        return;
    }
    else if (target->flags & MF_ICECORPSE)  // frozen
    {
        target->tics = 1;
        target->momx = target->momy = 0;
    }
    return;
  }

  if (target->flags & MF_SKULLFLY)
  {
    target->momx = target->momy = target->momz = 0;
  }

  if (target->flags2 & MF2_DORMANT)
  {
    // Invulnerable, and won't wake up
    return;
  }

  player = target->player;
  if (player && skill_info.damage_factor)
    damage = FixedMul(damage, skill_info.damage_factor);

  // Some close combat weapons should not
  // inflict thrust and push the victim out of reach,
  // thus kick away unless using the chainsaw.

  if (
    inflictor &&
    !(target->flags & MF_NOCLIP) && // hexen_note: not done in hexen, does it matter?
    !(
      source &&
      source->player &&
      (weaponinfo[source->player->readyweapon].flags & WPF_NOTHRUST)
    ) &&
    !(inflictor->flags2 & MF2_NODMGTHRUST)
  )
  {
    unsigned ang = R_PointToAngle2 (inflictor->x, inflictor->y,
                                    target->x,    target->y);

    fixed_t thrust = damage * (FRACUNIT >> 3) * g_thrust_factor / target->info->mass;

    // make fall forwards sometimes
    if ( damage < 40 && damage > target->health
         && target->z - inflictor->z > 64*FRACUNIT
         && P_Random(pr_damagemobj) & 1)
    {
      ang += ANG180;
      thrust *= 4;
    }

    ang >>= ANGLETOFINESHIFT;

    if (source && source->player && (source == inflictor)
        && source->player->powers[pw_weaponlevel2]
        && source->player->readyweapon == wp_staff)
    {
      // Staff power level 2
      target->momx += FixedMul(10 * FRACUNIT, finecosine[ang]);
      target->momy += FixedMul(10 * FRACUNIT, finesine[ang]);
      if (!(target->flags & MF_NOGRAVITY))
      {
          target->momz += 5 * FRACUNIT;
      }
    }
    else
    {
      target->momx += FixedMul(thrust, finecosine[ang]);
      target->momy += FixedMul(thrust, finesine[ang]);
    }

    /* killough 11/98: thrust objects hanging off ledges */
    if (target->intflags & MIF_FALLING && target->gear >= MAXGEAR)
      target->gear = 0;
  }

  // player specific
  if (player)
  {
    // end of game hell hack
    if (target->subsector->sector->special == 11 && damage >= target->health)
      damage = target->health - 1;

    // Below certain threshold,
    // ignore damage in GOD mode, or with INVUL power.
    // killough 3/26/98: make god mode 100% god mode in non-compat mode

    if (
      (damage < 1000 || (!comp[comp_god] && (player->cheats & CF_GODMODE))) &&
      (player->cheats & CF_GODMODE || player->powers[pw_invulnerability])
    )
      return;

    {
      if (player->armortype)
      {
        int saved;

          saved = player->armortype == 1 ? damage / 3 : damage / 2;

        if (player->armorpoints[ARMOR_ARMOR] <= saved)
        {
          // armor is used up
          saved = player->armorpoints[ARMOR_ARMOR];
          player->armortype = 0;
        }
        player->armorpoints[ARMOR_ARMOR] -= saved;
        damage -= saved;
      }
    }

    player->health -= damage;       // mirror mobj health here for Dave
    if (player->health < 0)
      player->health = 0;

    player->attacker = source;
    player->damagecount += damage;  // add damage after armor / invuln

    if (player->damagecount > 100)
      player->damagecount = 100;  // teleport stomp does 10k points...
  }

  // do the damage
  target->health -= damage;
  if (target->health <= 0)
  {

    P_KillMobj (source, target);
    return;
  }

  // killough 9/7/98: keep track of targets so that friends can help friends
  if (mbf_features)
  {
    /* If target is a player, set player's target to source,
     * so that a friend can tell who's hurting a player
     */
    if (player) P_SetTarget(&target->target, source);

    /* killough 9/8/98:
     * If target's health is less than 50%, move it to the front of its list.
     * This will slightly increase the chances that enemies will choose to
     * "finish it off", but its main purpose is to alert friends of danger.
     */
    if (target->health*2 < P_MobjSpawnHealth(target))
    {
      thinker_t *cap = &thinkerclasscap[target->flags & MF_FRIEND ?
               th_friends : th_enemies];
      (target->thinker.cprev->cnext = target->thinker.cnext)->cprev =
        target->thinker.cprev;
      (target->thinker.cnext = cap->cnext)->cprev = &target->thinker;
      (target->thinker.cprev = cap)->cnext = &target->thinker;
    }
  }

  if (!(skill_info.flags & SI_NO_PAIN) &&
      P_Random(pr_painchance) < target->info->painchance &&
      !(target->flags & MF_SKULLFLY)) //killough 11/98: see below
  {
      if (mbf_features)
        justhit = true;
      else
        target->flags |= MF_JUSTHIT;    // fight back!

      P_SetMobjState(target, target->info->painstate);
  }

  target->reactiontime = 0;           // we're awake now...

  /* killough 9/9/98: cleaned up, made more consistent: */
  //e6y: Monsters could commit suicide in Doom v1.2 if they damaged themselves by exploding a barrel
  if (
    source &&
    (source != target || compatibility_level == doom_12_compatibility) &&
    !(source->flags2 & MF2_DMGIGNORED) &&
    (!target->threshold || target->flags2 & MF2_NOTHRESHOLD) &&
    ((source->flags ^ target->flags) & MF_FRIEND || monster_infighting || !mbf_features) &&
    !P_InfightingImmune(target, source)
  )
  {
    /* if not intent on another player, chase after this one
     *
     * killough 2/15/98: remember last enemy, to prevent
     * sleeping early; 2/21/98: Place priority on players
     * killough 9/9/98: cleaned up, made more consistent:
     */

    if (
      !target->lastenemy ||
      target->lastenemy->health <= 0 ||
      (
        !mbf_features ?
        !target->lastenemy->player :
        !((target->flags ^ target->lastenemy->flags) & MF_FRIEND) && target->target != source
      )
    ) // remember last enemy - killough
      P_SetTarget(&target->lastenemy, target->target);

    P_SetTarget(&target->target, source);       // killough 11/98
    target->threshold = BASETHRESHOLD;
    if (target->state == &states[target->info->spawnstate]
        && target->info->seestate != g_s_null)
      P_SetMobjState (target, target->info->seestate);
  }

  /* killough 11/98: Don't attack a friend, unless hit by that friend.
   * cph 2006/04/01 - implicitly this is only if mbf_features */
  if(!demo_compatibility) //e6y
    if (justhit && (target->target == source || !target->target ||
        !(target->flags & target->target->flags & MF_FRIEND)))
      target->flags |= MF_JUSTHIT;    // fight back!
}

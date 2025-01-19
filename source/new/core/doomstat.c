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
 *      Put all global state variables here.
 *
 *-----------------------------------------------------------------------------
 */

#include "doomstat.h"

#include "dsda/map_format.h"

// Game Mode - identify IWAD as shareware, retail etc.
__thread GameMode_t gamemode = indetermined;
__thread GameMission_t   gamemission = doom;

// Language.
__thread Language_t   language = english;

// Set if homebrew PWAD stuff has been added.
__thread dboolean modifiedgame;

//-----------------------------------------------------------------------------

// CPhipps - compatibility vars
__thread complevel_t compatibility_level;

// e6y
// it's required for demos recorded in "demo compatibility" mode by boom201 for example
__thread int demover;

__thread int comp[MBF_COMP_TOTAL];    // killough 10/98
__thread int default_comperr[COMPERR_NUM];

__thread int demo_insurance;        // killough 1/16/98

__thread int  allow_pushers = 1;      // MT_PUSH Things              // phares 3/10/98

__thread int  variable_friction = 1;      // ice & mud               // phares 3/10/98

__thread int  weapon_recoil = 0;              // weapon recoil                   // phares

__thread int player_bobbing = 1;  // whether player bobs or not          // phares 2/25/98

__thread int monsters_remember = 1;          // killough 3/1/98

__thread int monster_infighting=1;       // killough 7/19/98: monster<=>monster attacks

__thread int monster_friction=1;       // killough 10/98: monsters affected by friction

__thread int dogs;         // killough 7/19/98: Marine's best friend :)
__thread int dog_jumping;   // killough 10/98

// killough 8/8/98: distance friends tend to move towards players
__thread int distfriend = 128;

// killough 9/8/98: whether monsters are allowed to strafe or retreat
__thread int monster_backing;

// killough 9/9/98: whether monsters are able to avoid hazards (e.g. crushers)
__thread int monster_avoid_hazards;

// killough 9/9/98: whether monsters help friends
__thread int help_friends;

__thread int monkeys;

char *VANILLA_MAP_LUMP_NAME(int e, int m)
{
  static char name[9];

  if (gamemode == commercial || map_format.map99)
    snprintf(name, sizeof(name), "MAP%02d", m);
  else
    snprintf(name, sizeof(name), "E%dM%d", e, m);

  return name;
}

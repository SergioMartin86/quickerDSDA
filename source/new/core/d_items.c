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
 *  Something to do with weapon sprite frames. Don't ask me.
 *
 *-----------------------------------------------------------------------------
 */

// We are referring to sprite numbers.
#include "doomtype.h"
#include "info.h"

#include "d_items.h"


//
// PSPRITE ACTIONS for waepons.
// This struct controls the weapon animations.
//
// Each entry is:
//  ammo/amunition type
//  upstate
//  downstate
//  readystate
//  atkstate, i.e. attack/fire/hit frame
//  flashstate, muzzle flash
//
weaponinfo_t __thread doom_weaponinfo[NUMWEAPONS+2] =
{
  {
    // fist
    am_noammo,
    S_PUNCHUP,
    S_PUNCHDOWN,
    S_PUNCH,
    S_PUNCH1,
    S_NULL,
    S_NULL,
    1,
    0,
    WPF_FLEEMELEE | WPF_AUTOSWITCHFROM | WPF_NOAUTOSWITCHTO
  },
  {
    // pistol
    am_clip,
    S_PISTOLUP,
    S_PISTOLDOWN,
    S_PISTOL,
    S_PISTOL1,
    S_NULL,
    S_PISTOLFLASH,
    1,
    0,
    WPF_AUTOSWITCHFROM
  },
  {
    // shotgun
    am_shell,
    S_SGUNUP,
    S_SGUNDOWN,
    S_SGUN,
    S_SGUN1,
    S_NULL,
    S_SGUNFLASH1,
    1,
    0,
    WPF_NOFLAG
  },
  {
    // chaingun
    am_clip,
    S_CHAINUP,
    S_CHAINDOWN,
    S_CHAIN,
    S_CHAIN1,
    S_NULL,
    S_CHAINFLASH1,
    1,
    0,
    WPF_NOFLAG
  },
  {
    // missile launcher
    am_misl,
    S_MISSILEUP,
    S_MISSILEDOWN,
    S_MISSILE,
    S_MISSILE1,
    S_NULL,
    S_MISSILEFLASH1,
    1,
    0,
    WPF_NOAUTOFIRE
  },
  {
    // plasma rifle
    am_cell,
    S_PLASMAUP,
    S_PLASMADOWN,
    S_PLASMA,
    S_PLASMA1,
    S_NULL,
    S_PLASMAFLASH1,
    1,
    0,
    WPF_NOFLAG
  },
  {
    // bfg 9000
    am_cell,
    S_BFGUP,
    S_BFGDOWN,
    S_BFG,
    S_BFG1,
    S_NULL,
    S_BFGFLASH1,
    40,
    0,
    WPF_NOAUTOFIRE
  },
  {
    // chainsaw
    am_noammo,
    S_SAWUP,
    S_SAWDOWN,
    S_SAW,
    S_SAW1,
    S_NULL,
    S_NULL,
    1,
    0,
    WPF_NOTHRUST | WPF_FLEEMELEE | WPF_NOAUTOSWITCHTO
  },
  {
    // super shotgun
    am_shell,
    S_DSGUNUP,
    S_DSGUNDOWN,
    S_DSGUN,
    S_DSGUN1,
    S_NULL,
    S_DSGUNFLASH1,
    2,
    0,
    WPF_NOFLAG
  },

  // dseg03:00082D90                 weaponinfo_t <5, 46h, 45h, 43h, 47h, 0>
  // dseg03:00082D90                 weaponinfo_t <1, 22h, 21h, 20h, 23h, 2Fh>
  // dseg03:00082E68 animdefs        dd 0                    ; istexture
  // dseg03:00082E68                 db 'N', 'U', 'K', 'A', 'G', 'E', '3', 2 dup(0); endname
  // dseg03:00082E68                 db 'N', 'U', 'K', 'A', 'G', 'E', '1', 2 dup(0); startname
  // dseg03:00082E68                 dd 8                    ; speed
  // dseg03:00082E68                 dd 0                    ; istexture
  {
    // ololo weapon
    0,
    S_NULL, // states are not used for emulation of weaponinfo overrun
    S_NULL,
    S_NULL,
    S_NULL,
    S_NULL,
    S_NULL,
    0,
    0,
    WPF_NOFLAG
  },
  {
    // preved medved weapon
    0,
    S_NULL,
    S_NULL,
    S_NULL,
    S_NULL,
    S_NULL,
    S_NULL,
    0,
    0,
    WPF_NOFLAG
  },
};


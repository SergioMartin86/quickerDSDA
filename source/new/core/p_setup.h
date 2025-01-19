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
 *   Setup a game, startup stuff.
 *
 *-----------------------------------------------------------------------------*/

#ifndef __P_SETUP__
#define __P_SETUP__

#include "p_mobj.h"

void P_SetupLevel(int episode, int map, int playermask, int skill);
void P_Init(void);               /* Called by startup code. */

extern __thread const byte *rejectmatrix;   /* for fast sight rejection -  cph - const* */
extern __thread int      *blockmaplump;   /* offsets in blockmap are from here */
extern __thread int      *blockmap;
extern __thread int      bmapwidth;
extern __thread int      bmapheight;      /* in mapblocks */
extern __thread fixed_t  bmaporgx;
extern __thread fixed_t  bmaporgy;        /* origin of block map */
extern __thread mobj_t   **blocklinks;    /* for thing chains */

extern __thread dboolean skipblstart; // MaxW: Skip initial blocklist short

// MAES: extensions to support 512x512 blockmaps.
extern __thread int blockmapxneg;
extern __thread int blockmapyneg;

typedef struct
{
  int width;
  int height;
  fixed_t orgx;
  fixed_t orgy;
} blockmap_t;

extern __thread blockmap_t original_blockmap;

void P_RestoreOriginalBlockMap(void);

typedef struct
{
  void (*load_vertexes)(int lump);
  void (*load_sectors)(int lump);
  void (*load_things)(int lump);
  void (*load_linedefs)(int lump);
  void (*allocate_sidedefs)(int lump);
  void (*load_sidedefs)(int lump);
  void (*update_level_components)(int lumpnum);
  void (*po_load_things)(int lump);
} map_loader_t;

extern __thread map_loader_t map_loader;


#endif

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
 *      Here is a core component: drawing the floors and ceilings,
 *       while maintaining a per column clipping list only.
 *      Moreover, the sky areas have to be determined.
 *
 * MAXVISPLANES is no longer a limit on the number of visplanes,
 * but a limit on the number of hash slots; larger numbers mean
 * better performance usually but after a point they are wasted,
 * and memory and time overheads creep in.
 *
 * For more information on visplanes, see:
 *
 * http://classicgaming.com/doom/editing/
 *
 * Lee Killough
 *
 *-----------------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "z_zone.h"  /* memory allocation wrappers -- killough */

#include "doomstat.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_plane.h"
#include "r_main.h"
#include "v_video.h"
#include "lprintf.h"

#include "dsda/map_format.h"
#include "dsda/render_stats.h"

int Sky1Texture;
int Sky2Texture;
fixed_t Sky1ColumnOffset;
fixed_t Sky2ColumnOffset;
dboolean DoubleSky;

#define MAXVISPLANES 256    /* must be a power of 2 */

static visplane_t *visplanes[MAXVISPLANES];   // killough
static visplane_t *freetail;                  // killough
static visplane_t **freehead = &freetail;     // killough
visplane_t *floorplane, *ceilingplane;

// killough -- hash function for visplanes
// Empirically verified to be fairly uniform:

#define visplane_hash(picnum,lightlevel,height) \
  ((unsigned)((picnum)*3+(lightlevel)+(height)*7) & (MAXVISPLANES-1))

size_t maxopenings;
int *openings,*lastopening; // dropoff overflow

// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1

// dropoff overflow
// e6y: resolution limitation is removed
int *floorclip = NULL;
int *ceilingclip = NULL;

// spanstart holds the start of a plane span; initialized to 0 at start

// e6y: resolution limitation is removed
static int *spanstart = NULL;                // killough 2/8/98

//
// texture mapping
//

// killough 2/8/98: make variables static

static fixed_t *cachedheight = NULL;

// e6y: resolution limitation is removed
fixed_t *yslope = NULL;
fixed_t *distscale = NULL;

void R_InitPlanesRes(void)
{
  if (floorclip) Z_Free(floorclip);
  if (ceilingclip) Z_Free(ceilingclip);
  if (spanstart) Z_Free(spanstart);

  if (cachedheight) Z_Free(cachedheight);

  if (yslope) Z_Free(yslope);
  if (distscale) Z_Free(distscale);

  floorclip = Z_Calloc(1, SCREENWIDTH * sizeof(*floorclip));
  ceilingclip = Z_Calloc(1, SCREENWIDTH * sizeof(*ceilingclip));
  spanstart = Z_Calloc(1, SCREENHEIGHT * sizeof(*spanstart));

  cachedheight = Z_Calloc(1, SCREENHEIGHT * sizeof(*cachedheight));

  yslope = Z_Calloc(1, SCREENHEIGHT * sizeof(*yslope));
  distscale = Z_Calloc(1, SCREENWIDTH * sizeof(*distscale));
}

void R_InitVisplanesRes(void)
{
  int i;

  freetail = NULL;
  freehead = &freetail;

  for (i = 0; i < MAXVISPLANES; i++)
  {
    visplanes[i] = 0;
  }
}

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
{
}

//
// RDrawPlanes
// At the end of each frame.
//

void R_DrawPlanes (void)
{
}

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
 *      Refresh/render internal state variables (global).
 *
 *-----------------------------------------------------------------------------*/


#ifndef __R_STATE__
#define __R_STATE__

// Need data structure definitions.
#include "d_player.h"
#include "r_data.h"

//
// Refresh internal data structures,
//  for rendering.
//

// needed for texture pegging
extern __STORAGE_MODIFIER fixed_t *textureheight;

extern __STORAGE_MODIFIER int firstflat, numflats;

// for global animation
extern __STORAGE_MODIFIER int *flattranslation;
extern __STORAGE_MODIFIER int *texturetranslation;

// Sprite....
extern __STORAGE_MODIFIER int firstspritelump;
extern __STORAGE_MODIFIER int lastspritelump;
extern __STORAGE_MODIFIER int numspritelumps;

//
// Lookup tables for map data.
//
extern __STORAGE_MODIFIER spritedef_t *sprites;

extern __STORAGE_MODIFIER int numvertexes;
extern __STORAGE_MODIFIER vertex_t *vertexes;

extern __STORAGE_MODIFIER int numsegs;
extern __STORAGE_MODIFIER seg_t *segs;

extern __STORAGE_MODIFIER int numsectors;
extern __STORAGE_MODIFIER sector_t *sectors;

extern __STORAGE_MODIFIER int numsubsectors;
extern __STORAGE_MODIFIER subsector_t *subsectors;

extern __STORAGE_MODIFIER int numnodes;
extern __STORAGE_MODIFIER node_t *nodes;

extern __STORAGE_MODIFIER int numlines;
extern __STORAGE_MODIFIER line_t *lines;

extern __STORAGE_MODIFIER int numsides;
extern __STORAGE_MODIFIER side_t *sides;

extern __STORAGE_MODIFIER int *sslines_indexes;
extern __STORAGE_MODIFIER ssline_t *sslines;

extern byte             *map_subsectors;

//
// POV data.
//
extern __STORAGE_MODIFIER fixed_t viewx;
extern __STORAGE_MODIFIER fixed_t viewy;
extern __STORAGE_MODIFIER fixed_t viewz;
extern __STORAGE_MODIFIER angle_t viewangle;
extern __STORAGE_MODIFIER player_t *viewplayer;
extern __STORAGE_MODIFIER angle_t clipangle;
extern __STORAGE_MODIFIER int viewangletox[FINEANGLES/2];

// e6y: resolution limitation is removed
extern __STORAGE_MODIFIER angle_t *xtoviewangle;  // killough 2/8/98

extern __STORAGE_MODIFIER int FieldOfView;

extern __STORAGE_MODIFIER fixed_t rw_distance;
extern __STORAGE_MODIFIER angle_t rw_normalangle;

// angle to line origin
extern __STORAGE_MODIFIER int rw_angle1;

extern __STORAGE_MODIFIER visplane_t       *floorplane;
extern __STORAGE_MODIFIER visplane_t       *ceilingplane;

#endif

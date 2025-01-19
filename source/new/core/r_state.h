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
extern __thread fixed_t *textureheight;

extern __thread int firstflat, numflats;

// for global animation
extern __thread int *flattranslation;
extern __thread int *texturetranslation;

// Sprite....
extern __thread int firstspritelump;
extern __thread int lastspritelump;
extern __thread int numspritelumps;

//
// Lookup tables for map data.
//
extern __thread spritedef_t      *sprites;

extern __thread int              numvertexes;
extern __thread vertex_t         *vertexes;

extern __thread int              numsegs;
extern __thread seg_t            *segs;

extern __thread int              numsectors;
extern __thread sector_t         *sectors;

extern __thread int              numsubsectors;
extern __thread subsector_t      *subsectors;

extern __thread int              numnodes;
extern __thread node_t           *nodes;

extern __thread int              numlines;
extern __thread line_t           *lines;

extern __thread int              numsides;
extern __thread side_t           *sides;

extern __thread int              *sslines_indexes;
extern __thread ssline_t         *sslines;

extern __thread byte             *map_subsectors;

//
// POV data.
//
extern __thread fixed_t          viewx;
extern __thread fixed_t          viewy;
extern __thread fixed_t          viewz;
extern __thread angle_t          viewangle;
extern __thread player_t         *viewplayer;
extern __thread angle_t          clipangle;
extern __thread int              viewangletox[FINEANGLES/2];

// e6y: resolution limitation is removed
extern __thread angle_t          *xtoviewangle;  // killough 2/8/98

extern __thread int              FieldOfView;

extern __thread fixed_t          rw_distance;
extern __thread angle_t          rw_normalangle;

// angle to line origin
extern __thread int              rw_angle1;

extern __thread visplane_t       *floorplane;
extern __thread visplane_t       *ceilingplane;

#endif

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
 *      Map functions
 *
 *-----------------------------------------------------------------------------*/

#ifndef __P_MAP__
#define __P_MAP__

#include "r_defs.h"
#include "d_player.h"

#define USERANGE        (64*FRACUNIT)
#define MELEERANGE      (64*FRACUNIT)
#define MISSILERANGE    (32*64*FRACUNIT)

// a couple of explicit constants for non-melee things that used to use MELEERANGE
#define WAKEUPRANGE     (64*FRACUNIT)
#define SNEAKRANGE      (128*FRACUNIT)

// MAXRADIUS is for precalculated sector block boxes the spider demon
// is larger, but we do not have any moving sectors nearby
#define MAXRADIUS       (32*FRACUNIT)

//e6y
#define STAIRS_UNINITIALIZED_CRUSH_FIELD_VALUE -2

#define BF_DAMAGESOURCE 0x01
#define BF_HORIZONTAL   0x02

typedef struct
{
  mobj_t *source;
  mobj_t *spot;
  int damage;
  int distance;
  int flags;
} bomb_t;

// killough 3/15/98: add fourth argument to P_TryMove
dboolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, dboolean dropoff);

// killough 8/9/98: extra argument for telefragging
dboolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y,dboolean boss);
void    P_UnqualifiedMove(mobj_t *thing, fixed_t x, fixed_t y);
void    P_SlideMove(mobj_t *mo);
dboolean P_CheckSight(mobj_t *t1, mobj_t *t2);
dboolean P_CheckFov(mobj_t *t1, mobj_t *t2, angle_t fov);
void    P_UseLines(player_t *player);

typedef dboolean (*CrossSubsectorFunc)(int num);
extern __thread CrossSubsectorFunc P_CrossSubsector;
dboolean P_CrossSubsector_Doom(int num);
dboolean P_CrossSubsector_Boom(int num);
dboolean P_CrossSubsector_PrBoom(int num);

// killough 8/2/98: add 'mask' argument to prevent friends autoaiming at others
fixed_t P_AimLineAttack(mobj_t *t1,angle_t angle,fixed_t distance, uint64_t mask);

void    P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance,
                     fixed_t slope, int damage );
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance, int flags);
dboolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);

typedef struct
{
  msecnode_t *node;
  sector_t *sector;
} mobj_in_sector_t;

void P_InitSectorSearch(mobj_in_sector_t *data, sector_t *sector);
mobj_t *P_FindMobjInSector(mobj_in_sector_t *data);

//jff 3/19/98 P_CheckSector(): new routine to replace P_ChangeSector()
dboolean P_ChangeSector(sector_t *sector, int crunch);
dboolean P_CheckSector(sector_t *sector, int crunch);
void    P_DelSeclist(msecnode_t*);                          // phares 3/16/98
void    P_FreeSecNodeList(void);                            // sf
void    P_CreateSecNodeList(mobj_t*,fixed_t,fixed_t);       // phares 3/14/98
dboolean Check_Sides(mobj_t *, int, int);                    // phares

int     P_GetMoveFactor(mobj_t *mo, int *friction);   // killough 8/28/98
int     P_GetFriction(const mobj_t *mo, int *factor);       // killough 8/28/98
void    P_ApplyTorque(mobj_t *mo);                          // killough 9/12/98

/* cphipps 2004/08/30 */
void	P_MapStart(void);
void	P_MapEnd(void);

// If "floatok" true, move would be ok if within "tmfloorz - tmceilingz".
extern __thread dboolean floatok;
extern __thread dboolean felldown;   // killough 11/98: indicates object pushed off ledge
extern __thread fixed_t tmfloorz;
extern __thread fixed_t tmceilingz;
extern __thread line_t *ceilingline;
extern __thread line_t *floorline;      // killough 8/23/98
extern __thread mobj_t *linetarget;     // who got hit (or NULL)
extern __thread mobj_t *crosshair_target;
extern __thread msecnode_t *sector_list;                             // phares 3/16/98
extern __thread fixed_t tmbbox[4];         // phares 3/20/98
extern __thread line_t *blockline;   // killough 8/11/98

// heretic

dboolean P_TestMobjLocation(mobj_t * mobj);
mobj_t *P_CheckOnmobj(mobj_t * thing);
void P_FakeZMovement(mobj_t * mo);

void P_AppendSpecHit(line_t * ld);

// hexen

extern __thread int tmfloorpic;
extern __thread mobj_t *BlockingMobj;

void P_BounceWall(mobj_t * mo);
dboolean P_UsePuzzleItem(player_t * player, int itemType);
void PIT_ThrustSpike(mobj_t * actor);

// zdoom

dboolean P_MoveThing(mobj_t *thing, fixed_t x, fixed_t y, fixed_t z, dboolean fog);
int P_SplashDamage(fixed_t dist);
void P_AdjustZLimits(mobj_t *thing);

#endif // __P_MAP__

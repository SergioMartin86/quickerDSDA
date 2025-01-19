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
 *      Archiving: SaveGame I/O.
 *
 *-----------------------------------------------------------------------------*/

#include <stdint.h>

#include "doomstat.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_saveg.h"
#include "m_random.h"
#include "p_enemy.h"
#include "lprintf.h"
#include "e6y.h"//e6y

#include "dsda/map_format.h"
#include "dsda/scroll.h"
#include "dsda/utility.h"

#define MARKED_FOR_DELETION -2

#define SAVEGAMESIZE 0x20000

byte *save_p;
byte *savebuffer;
static int savegamesize;

// Check for overrun and realloc if necessary -- Lee Killough 1/22/98
void CheckSaveGame(size_t size)
{
  size_t offset = save_p - savebuffer;

  if (offset + size <= savegamesize)
  {
    return;
  }

  while (offset + size > savegamesize)
  {
    savegamesize *= 2;
  }

  savebuffer = Z_Realloc(savebuffer, savegamesize);
  save_p = savebuffer + offset;
}

void P_InitSaveBuffer(void)
{
  savegamesize = SAVEGAMESIZE;
  save_p = savebuffer = Z_Malloc(savegamesize);
}

void P_ForgetSaveBuffer(void)
{
  save_p = savebuffer = NULL;
}

void P_FreeSaveBuffer(void)
{
  Z_Free(savebuffer);
  P_ForgetSaveBuffer();
}

//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
  int i;

  for (i = 0; i < g_maxplayers; i++)
    if (playeringame[i])
      {
        int      j;
        player_t *dest;

        P_SAVE_TYPE_REF(&players[i], dest, player_t);
        for (j=0; j<NUMPSPRITES; j++)
          if (dest->psprites[j].state)
            dest->psprites[j].state =
              (state_t *)(dest->psprites[j].state-states);
      }
}

//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
  int i;

  for (i = 0; i < g_maxplayers; i++)
    if (playeringame[i])
      {
        int j;

        P_LOAD_X(players[i]);

        // will be set when unarc thinker
        players[i].mo = NULL;
        players[i].attacker = NULL;
        // HERETIC_TODO: does the rain need to be remembered?
        players[i].rain1 = NULL;
        players[i].rain2 = NULL;

        // hexen_note: poisoner not reloaded
        players[i].poisoner = NULL;

        for (j=0 ; j<NUMPSPRITES ; j++)
          if (players[i]. psprites[j].state)
            players[i]. psprites[j].state =
              &states[ (size_t)players[i].psprites[j].state ];
      }
}


//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
  int            i;
  const sector_t *sec;
  const line_t   *li;
  const side_t   *si;

  for (i = 0, sec = sectors; i < numsectors; i++, sec++)
  {
    P_SAVE_X(sec->floorheight);
    P_SAVE_X(sec->ceilingheight);
    P_SAVE_X(sec->floorpic);
    P_SAVE_X(sec->ceilingpic);
    P_SAVE_X(sec->lightlevel);
    P_SAVE_X(sec->special);
    P_SAVE_X(sec->tag);
    P_SAVE_X(sec->flags);
  }

  for (i = 0, li = lines; i < numlines; i++, li++)
  {
    int j;

    P_SAVE_X(li->flags);
    P_SAVE_X(li->special);
    P_SAVE_X(li->tag);
    P_SAVE_BYTE(li->player_activations);
    P_SAVE_ARRAY(li->special_args);
  }
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
  int          i;
  sector_t     *sec;
  line_t       *li;

  for (i = 0, sec = sectors; i < numsectors; i++, sec++)
  {
    P_LOAD_X(sec->floorheight);
    P_LOAD_X(sec->ceilingheight);
    P_LOAD_X(sec->floorpic);
    P_LOAD_X(sec->ceilingpic);
    P_LOAD_X(sec->lightlevel);
    P_LOAD_X(sec->special);
    P_LOAD_X(sec->tag);
    P_LOAD_X(sec->flags);

    sec->ceilingdata = 0; //jff 2/22/98 now three thinker fields, not two
    sec->floordata = 0;
    sec->lightingdata = 0;
    sec->soundtarget = 0;
  }

  // do lines
  for (i = 0, li = lines; i < numlines; i++, li++)
  {
    int j;

    P_LOAD_X(li->flags);
    P_LOAD_X(li->special);
    P_LOAD_X(li->tag);
    P_LOAD_BYTE(li->player_activations);
    P_LOAD_ARRAY(li->special_args);
  }
}

//
// Thinkers
//

// phares 9/13/98: Moved this code outside of P_ArchiveThinkers so the
// thinker indices could be used by the code that saves sector info.

static int number_of_thinkers;

static dboolean P_IsMobjThinker(thinker_t* thinker)
{
  return thinker->function == P_MobjThinker ||
         thinker->function == P_BlasterMobjThinker ||
         (thinker->function == P_RemoveThinkerDelayed && thinker->references);
}

static void P_ReplaceMobjWithIndex(mobj_t **mobj)
{
  if (*mobj)
  {
    *mobj = P_IsMobjThinker(&(*mobj)->thinker) ?
            (mobj_t *) (*mobj)->thinker.prev   :
            NULL;
  }
}

/*
 * killough 11/98
 *
 * Same as P_SetTarget() in p_tick.c, except that the target is nullified
 * first, so that no old target's reference count is decreased (when loading
 * savegames, old targets are indices, not really pointers to targets).
 */

static void P_SetNewTarget(mobj_t **mop, mobj_t *targ)
{
  *mop = NULL;
  P_SetTarget(mop, targ);
}

// savegame file stores ints in the corresponding * field; this function
// safely casts them back to int.
int P_GetMobj(mobj_t* mi, size_t s)
{
  size_t i = (size_t)mi;
  if (i >= s)
    I_Error("Corrupt savegame");
  return i;
}

static void P_ReplaceIndexWithMobj(mobj_t **mobj, mobj_t **mobj_p, int mobj_count)
{
  P_SetNewTarget(
    mobj,
    mobj_p[
      P_GetMobj(*mobj, mobj_count + 1)
    ]
  );
}

void P_ThinkerToIndex(void)
{
  thinker_t *th;

  // killough 2/14/98:
  // count the number of thinkers, and mark each one with its index, using
  // the prev field as a placeholder, since it can be restored later.

  number_of_thinkers = 0;
  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    if (P_IsMobjThinker(th))
      th->prev = (thinker_t *)(intptr_t) ++number_of_thinkers;
}

// phares 9/13/98: Moved this code outside of P_ArchiveThinkers so the
// thinker indices could be used by the code that saves sector info.

void P_IndexToThinker(void)
{
  // killough 2/14/98: restore prev pointers
  thinker_t *th;
  thinker_t *prev = &thinkercap;

  for (th = thinkercap.next ; th != &thinkercap ; prev=th, th=th->next)
    th->prev = prev;
}

// killough 2/16/98: save/restore random number generator state information

void P_ArchiveRNG(void)
{
  P_SAVE_X(rng);
}

void P_UnArchiveRNG(void)
{
  P_LOAD_X(rng);
}

// killough 2/22/98: Save/restore automap state
// killough 2/22/98: Save/restore automap state
void P_ArchiveMap(void)
{
}

void P_UnArchiveMap(void)
{
}

void P_ArchiveThinkerSubclass(th_class class)
{
  int count;
  thinker_t *cap, *th;

  count = 0;
  cap = &thinkerclasscap[class];
  for (th = cap->cnext; th != cap; th = th->cnext)
    count++;

  P_SAVE_X(count);

  for (th = cap->cnext; th != cap; th = th->cnext)
  {
    P_SAVE_X(th->prev);
  }
}

void P_ArchiveThinkerSubclasses(void)
{
  // Other subclass ordering is not relevant
  P_ArchiveThinkerSubclass(th_friends);
  P_ArchiveThinkerSubclass(th_enemies);
}

void P_UnArchiveThinkerSubclass(th_class class, mobj_t** mobj_p, int mobj_count)
{
  int i;
  int count;

  // Reset thinker subclass list
  thinkerclasscap[class].cprev->cnext = thinkerclasscap[class].cnext;
  thinkerclasscap[class].cnext->cprev = thinkerclasscap[class].cprev;
  thinkerclasscap[class].cprev =
    thinkerclasscap[class].cnext = &thinkerclasscap[class];

  P_LOAD_X(count);

  for (i = 0; i < count; ++i)
  {
    thinker_t* th;
    mobj_t* mobj;

    P_LOAD_X(mobj);

    mobj = mobj_p[P_GetMobj(mobj, mobj_count + 1)];

    if (mobj)
    {
      // remove mobj from current subclass list
      th = mobj->thinker.cnext;
      if (th != NULL)
      {
        th->cprev = mobj->thinker.cprev;
        th->cprev->cnext = th;
      }

      th = &thinkerclasscap[class];
      th->cprev->cnext = &mobj->thinker;
      mobj->thinker.cnext = th;
      mobj->thinker.cprev = th->cprev;
      th->cprev = &mobj->thinker;
    }
    else
    {
      I_Error("P_UnArchiveThinkerSubclass: mobj does not exist!\n");
    }
  }
}

void P_UnArchiveThinkerSubclasses(mobj_t** mobj_p, int mobj_count)
{
  P_UnArchiveThinkerSubclass(th_friends, mobj_p, mobj_count);
  P_UnArchiveThinkerSubclass(th_enemies, mobj_p, mobj_count);
}

extern mobj_t** blocklinks;
extern int      blocklinks_count;
extern int      bmapwidth;
extern int      bmapheight;

void P_ArchiveBlockLinks(void)
{
  int i;

  for (i = 0; i < blocklinks_count; ++i)
  {
    int count = 0;
    mobj_t*  mobj;

    mobj = blocklinks[i];
    while (mobj)
    {
      ++count;
      mobj = mobj->bnext;
    }

    P_SAVE_X(count);

    mobj = blocklinks[i];
    while (mobj)
    {
      P_SAVE_X(mobj->thinker.prev);
      mobj = mobj->bnext;
    }
  }
}

void P_UnArchiveBlockLinks(mobj_t** mobj_p, int mobj_count)
{
  int i;
  int size;

  size = bmapwidth * bmapheight;

  for (i = 0; i < size; ++i)
  {
    int j;
    int count;
    mobj_t* mobj;
    mobj_t** bprev;

    P_LOAD_X(count);

    blocklinks[i] = NULL;
    bprev = &blocklinks[i];
    for (j = 0; j < count; ++j)
    {
      P_LOAD_X(mobj);

      mobj = mobj_p[P_GetMobj(mobj, mobj_count + 1)];

      if (mobj)
      {
        *bprev = mobj;
        mobj->bprev = bprev;
        mobj->bnext = NULL;
        bprev = &mobj->bnext;
      }
      else
      {
        I_Error("P_UnArchiveBlockLinks: mobj does not exist!\n");
      }
    }
  }
}

static dboolean P_IsPolyObjThinker(thinker_t *th)
{
  return 0;
}

// dsda - fix save / load synchronization
// merges thinkerclass_t and specials_e
typedef enum {
  tc_mobj,
  tc_ceiling,
  tc_door,
  tc_floor,
  tc_plat,
  tc_flash,
  tc_strobe,
  tc_glow,
  tc_zdoom_glow,
  tc_elevator,
  tc_scroll_side,
  tc_scroll_side_control,
  tc_scroll_floor,
  tc_scroll_floor_control,
  tc_scroll_ceiling,
  tc_scroll_ceiling_control,
  tc_scroll_floor_carry,
  tc_scroll_floor_carry_control,
  tc_zdoom_scroll_floor,
  tc_zdoom_scroll_ceiling,
  tc_thrust,
  tc_pusher,
  tc_flicker,
  tc_zdoom_flicker,
  tc_friction,
  tc_light,
  tc_phase,
  tc_acs,
  tc_pillar,
  tc_floor_waggle,
  tc_ceiling_waggle,
  tc_poly_rotate,
  tc_poly_move,
  tc_poly_door,
  tc_quake,
  tc_ambient_source,
  tc_end
} true_thinkerclass_t;

// dsda - fix save / load synchronization
// merges P_ArchiveThinkers & P_ArchiveSpecials
void P_ArchiveThinkers(void) {
  thinker_t *th;

  P_SAVE_X(brain);

  // save off the current thinkers
  for (th = thinkercap.next ; th != &thinkercap ; th=th->next) {
    if (!th->function)
    {
      platlist_t *pl;
      ceilinglist_t *cl;    //jff 2/22/98 add iter variable for ceilings

      // killough 2/8/98: fix plat original height bug.
      // Since acv==NULL, this could be a plat in stasis.
      // so check the active plats list, and save this
      // plat (jff: or ceiling) even if it is in stasis.

      for (pl=activeplats; pl; pl=pl->next)
        if (pl->plat == (plat_t *) th)      // killough 2/14/98
          goto plat;

      for (cl=activeceilings; cl; cl=cl->next)
        if (cl->ceiling == (ceiling_t *) th)      //jff 2/22/98
          goto ceiling;

      continue;
    }

    if (th->function == T_MoveCeiling)
    {
      ceiling_t *ceiling;
    ceiling:                               // killough 2/14/98
      P_SAVE_BYTE(tc_ceiling);
      P_SAVE_TYPE_REF(th, ceiling, ceiling_t);
      ceiling->sector = (sector_t *)(intptr_t)(ceiling->sector->iSectorID);
      continue;
    }

    if (th->function == T_VerticalDoor)
    {
      vldoor_t *door;
      P_SAVE_BYTE(tc_door);
      P_SAVE_TYPE_REF(th, door, vldoor_t);
      door->sector = (sector_t *)(intptr_t)(door->sector->iSectorID);
      //jff 1/31/98 archive line remembered by door as well
      door->line = (line_t *) (door->line ? door->line-lines : -1);
      continue;
    }

    if (th->function == T_MoveFloor)
    {
      floormove_t *floor;
      P_SAVE_BYTE(tc_floor);
      P_SAVE_TYPE_REF(th, floor, floormove_t);
      floor->sector = (sector_t *)(intptr_t)(floor->sector->iSectorID);
      continue;
    }

    if (th->function == T_PlatRaise)
    {
      plat_t *plat;
    plat:   // killough 2/14/98: added fix for original plat height above
      P_SAVE_BYTE(tc_plat);
      P_SAVE_TYPE_REF(th, plat, plat_t);
      plat->sector = (sector_t *)(intptr_t)(plat->sector->iSectorID);
      continue;
    }

    if (th->function == T_LightFlash)
    {
      lightflash_t *flash;
      P_SAVE_BYTE(tc_flash);
      P_SAVE_TYPE_REF(th, flash, lightflash_t);
      flash->sector = (sector_t *)(intptr_t)(flash->sector->iSectorID);
      continue;
    }

    if (th->function == T_StrobeFlash)
    {
      strobe_t *strobe;
      P_SAVE_BYTE(tc_strobe);
      P_SAVE_TYPE_REF(th, strobe, strobe_t);
      strobe->sector = (sector_t *)(intptr_t)(strobe->sector->iSectorID);
      continue;
    }

    if (th->function == T_Glow)
    {
      glow_t *glow;
      P_SAVE_BYTE(tc_glow);
      P_SAVE_TYPE_REF(th, glow, glow_t);
      glow->sector = (sector_t *)(intptr_t)(glow->sector->iSectorID);
      continue;
    }

    if (th->function == T_ZDoom_Glow)
    {
      zdoom_glow_t *glow;
      P_SAVE_BYTE(tc_zdoom_glow);
      P_SAVE_TYPE_REF(th, glow, zdoom_glow_t);
      glow->sector = (sector_t *)(intptr_t)(glow->sector->iSectorID);
      continue;
    }

    // killough 10/4/98: save flickers
    if (th->function == T_FireFlicker)
    {
      fireflicker_t *flicker;
      P_SAVE_BYTE(tc_flicker);
      P_SAVE_TYPE_REF(th, flicker, fireflicker_t);
      flicker->sector = (sector_t *)(intptr_t)(flicker->sector->iSectorID);
      continue;
    }

    if (th->function == T_ZDoom_Flicker)
    {
      zdoom_flicker_t *flicker;
      P_SAVE_BYTE(tc_zdoom_flicker);
      P_SAVE_TYPE_REF(th, flicker, zdoom_flicker_t);
      flicker->sector = (sector_t *)(intptr_t)(flicker->sector->iSectorID);
      continue;
    }

    //jff 2/22/98 new case for elevators
    if (th->function == T_MoveElevator)
    {
      elevator_t *elevator;         //jff 2/22/98
      P_SAVE_BYTE(tc_elevator);
      P_SAVE_TYPE_REF(th, elevator, elevator_t);
      elevator->sector = (sector_t *)(intptr_t)(elevator->sector->iSectorID);
      continue;
    }

    if (th->function == dsda_UpdateSideScroller)
    {
      P_SAVE_BYTE(tc_scroll_side);
      P_SAVE_TYPE(th, scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateFloorScroller)
    {
      P_SAVE_BYTE(tc_scroll_floor);
      P_SAVE_TYPE(th, scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateCeilingScroller)
    {
      P_SAVE_BYTE(tc_scroll_ceiling);
      P_SAVE_TYPE(th, scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateFloorCarryScroller)
    {
      P_SAVE_BYTE(tc_scroll_floor_carry);
      P_SAVE_TYPE(th, scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateZDoomFloorScroller)
    {
      P_SAVE_BYTE(tc_zdoom_scroll_floor);
      P_SAVE_TYPE(th, scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateZDoomCeilingScroller)
    {
      P_SAVE_BYTE(tc_zdoom_scroll_ceiling);
      P_SAVE_TYPE(th, scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateThruster)
    {
      P_SAVE_BYTE(tc_thrust);
      P_SAVE_TYPE(th, scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateControlSideScroller)
    {
      P_SAVE_BYTE(tc_scroll_side_control);
      P_SAVE_TYPE(th, control_scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateControlFloorScroller)
    {
      P_SAVE_BYTE(tc_scroll_floor_control);
      P_SAVE_TYPE(th, control_scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateControlCeilingScroller)
    {
      P_SAVE_BYTE(tc_scroll_ceiling_control);
      P_SAVE_TYPE(th, control_scroll_t);
      continue;
    }

    if (th->function == dsda_UpdateControlFloorCarryScroller)
    {
      P_SAVE_BYTE(tc_scroll_floor_carry_control);
      P_SAVE_TYPE(th, control_scroll_t);
      continue;
    }

    // phares 3/22/98: Push/Pull effect thinkers

    if (th->function == T_Pusher)
    {
      P_SAVE_BYTE(tc_pusher);
      P_SAVE_TYPE(th, pusher_t);
      continue;
    }

    if (th->function == T_Friction)
    {
      P_SAVE_BYTE(tc_friction);
      P_SAVE_TYPE(th, friction_t);
      continue;
    }

    if (th->function == T_Light)
    {
      light_t *light;
      P_SAVE_BYTE(tc_light);
      P_SAVE_TYPE_REF(th, light, light_t);
      light->sector = (sector_t *)(intptr_t)(light->sector->iSectorID);
      continue;
    }

    if (th->function == T_Phase)
    {
      phase_t *phase;
      P_SAVE_BYTE(tc_phase);
      P_SAVE_TYPE_REF(th, phase, phase_t);
      phase->sector = (sector_t *)(intptr_t)(phase->sector->iSectorID);
      continue;
    }

    if (th->function == T_BuildPillar)
    {
      pillar_t *pillar;
      P_SAVE_BYTE(tc_pillar);
      P_SAVE_TYPE_REF(th, pillar, pillar_t);
      pillar->sector = (sector_t *)(intptr_t)(pillar->sector->iSectorID);
      continue;
    }

    if (P_IsMobjThinker(th))
    {
      mobj_t *mobj;

      P_SAVE_BYTE(tc_mobj);
      P_SAVE_TYPE_REF(th, mobj, mobj_t);

      mobj->state = (state_t *)(mobj->state - states);

      // Example:
      // - Archvile is attacking a lost soul
      // - The lost soul dies before the attack hits
      // - The lost soul is marked for deletion
      // - The archvile will still attack the spot where the lost soul was
      // - We need to save such objects and remember they are marked for deletion
      if (mobj->thinker.function == P_RemoveThinkerDelayed)
        mobj->index = MARKED_FOR_DELETION;

      // killough 2/14/98: convert pointers into indices.
      // Fixes many savegame problems, by properly saving
      // target and tracer fields. Note: we store NULL if
      // the thinker pointed to by these fields is not a
      // mobj thinker.

      P_ReplaceMobjWithIndex(&mobj->target);
      P_ReplaceMobjWithIndex(&mobj->tracer);

      // killough 2/14/98: new field: save last known enemy. Prevents
      // monsters from going to sleep after killing monsters and not
      // seeing player anymore.

      P_ReplaceMobjWithIndex(&mobj->lastenemy);

      // killough 2/14/98: end changes

      if (mobj->player)
        mobj->player = (player_t *)((mobj->player-players) + 1);
    }
  }

  // add a terminating marker
  P_SAVE_BYTE(tc_end);

  // killough 9/14/98: save soundtargets
  {
    int i;
    for (i = 0; i < numsectors; i++)
    {
      mobj_t *target = sectors[i].soundtarget;
      // Fix crash on reload when a soundtarget points to a removed corpse
      // (prboom bug #1590350)
      P_ReplaceMobjWithIndex(&target);
      P_SAVE_X(target);
    }
  }

  P_ArchiveBlockLinks();
  P_ArchiveThinkerSubclasses();
}

// dsda - fix save / load synchronization
// merges P_UnArchiveThinkers & P_UnArchiveSpecials
void P_UnArchiveThinkers(void) {
  thinker_t *th;
  mobj_t    **mobj_p;    // killough 2/14/98: Translation table
  int    mobj_count;        // killough 2/14/98: size of or index into table
  true_thinkerclass_t tc;

  totallive = 0;

  // killough 3/26/98: Load boss brain state
  P_LOAD_X(brain);

  // remove all the current thinkers
  for (th = thinkercap.next; th != &thinkercap; )
  {
    thinker_t *next = th->next;
    if (P_IsMobjThinker(th))
    {
      P_RemoveMobj ((mobj_t *) th);
      P_RemoveThinkerDelayed(th); // fix mobj leak
    }
    else
      Z_Free (th);
    th = next;
  }
  P_InitThinkers ();

  // killough 2/14/98: count number of thinkers by skipping through them
  {
    byte *sp;     // save pointer and skip header

    sp = save_p;
    mobj_count = 0;

    while (true)
    {
      P_LOAD_BYTE(tc);
      if (tc == tc_end)
        break;

      if (tc == tc_mobj) mobj_count++;
      save_p +=
        tc == tc_ceiling        ? sizeof(ceiling_t)        :
        tc == tc_door           ? sizeof(vldoor_t)         :
        tc == tc_floor          ? sizeof(floormove_t)      :
        tc == tc_plat           ? sizeof(plat_t)           :
        tc == tc_flash          ? sizeof(lightflash_t)     :
        tc == tc_strobe         ? sizeof(strobe_t)         :
        tc == tc_glow           ? sizeof(glow_t)           :
        tc == tc_zdoom_glow     ? sizeof(zdoom_glow_t)     :
        tc == tc_elevator       ? sizeof(elevator_t)       :
        tc == tc_scroll_side                ? sizeof(scroll_t)         :
        tc == tc_scroll_floor               ? sizeof(scroll_t)         :
        tc == tc_scroll_ceiling             ? sizeof(scroll_t)         :
        tc == tc_scroll_floor_carry         ? sizeof(scroll_t)         :
        tc == tc_zdoom_scroll_floor         ? sizeof(scroll_t)         :
        tc == tc_zdoom_scroll_ceiling       ? sizeof(scroll_t)         :
        tc == tc_thrust                     ? sizeof(scroll_t)         :
        tc == tc_scroll_side_control        ? sizeof(control_scroll_t) :
        tc == tc_scroll_floor_control       ? sizeof(control_scroll_t) :
        tc == tc_scroll_ceiling_control     ? sizeof(control_scroll_t) :
        tc == tc_scroll_floor_carry_control ? sizeof(control_scroll_t) :
        tc == tc_pusher         ? sizeof(pusher_t)         :
        tc == tc_flicker        ? sizeof(fireflicker_t)    :
        tc == tc_zdoom_flicker  ? sizeof(zdoom_flicker_t)  :
        tc == tc_friction       ? sizeof(friction_t)       :
        tc == tc_light          ? sizeof(light_t)          :
        tc == tc_phase          ? sizeof(phase_t)          :
        tc == tc_pillar         ? sizeof(pillar_t)         :
        tc == tc_floor_waggle   ? sizeof(planeWaggle_t)    :
        tc == tc_ceiling_waggle ? sizeof(planeWaggle_t)    :
        tc == tc_quake          ? sizeof(quake_t)          :
        tc == tc_mobj           ? sizeof(mobj_t)           :
      0;
    }

    if (*--save_p != tc_end)
      I_Error ("P_UnArchiveThinkers: Unknown tc %i in size calculation", *save_p);

    // first table entry special: 0 maps to NULL
    *(mobj_p = Z_Malloc((mobj_count + 1) * sizeof *mobj_p)) = 0;   // table of pointers
    save_p = sp;           // restore save pointer
  }

  // read in saved thinkers
  mobj_count = 0;
  while (true)
  {
    P_LOAD_BYTE(tc);
    if (tc == tc_end)
      break;

    switch (tc) {
      case tc_ceiling:
        {
          ceiling_t *ceiling = Z_MallocLevel (sizeof(*ceiling));
          P_LOAD_P(ceiling);
          ceiling->sector = &sectors[(size_t)ceiling->sector];
          ceiling->sector->ceilingdata = ceiling; //jff 2/22/98

          if (ceiling->thinker.function)
            ceiling->thinker.function = T_MoveCeiling;

          P_AddThinker (&ceiling->thinker);
          P_AddActiveCeiling(ceiling);
          break;
        }

      case tc_door:
        {
          vldoor_t *door = Z_MallocLevel (sizeof(*door));
          P_LOAD_P(door);
          door->sector = &sectors[(size_t)door->sector];

          //jff 1/31/98 unarchive line remembered by door as well
          door->line = (intptr_t)door->line!=-1? &lines[(size_t)door->line] : NULL;

          door->sector->ceilingdata = door;       //jff 2/22/98
          door->thinker.function = T_VerticalDoor;
          P_AddThinker (&door->thinker);
          break;
        }

      case tc_floor:
        {
          floormove_t *floor = Z_MallocLevel (sizeof(*floor));
          P_LOAD_P(floor);
          floor->sector = &sectors[(size_t)floor->sector];
          floor->sector->floordata = floor; //jff 2/22/98
          floor->thinker.function = T_MoveFloor;
          P_AddThinker (&floor->thinker);
          break;
        }

      case tc_plat:
        {
          plat_t *plat = Z_MallocLevel (sizeof(*plat));
          P_LOAD_P(plat);
          plat->sector = &sectors[(size_t)plat->sector];
          plat->sector->floordata = plat; //jff 2/22/98

          if (plat->thinker.function)
            plat->thinker.function = T_PlatRaise;

          P_AddThinker (&plat->thinker);
          P_AddActivePlat(plat);
          break;
        }

      case tc_flash:
        {
          lightflash_t *flash = Z_MallocLevel (sizeof(*flash));
          P_LOAD_P(flash);
          flash->sector = &sectors[(size_t)flash->sector];
          flash->sector->lightingdata = flash;
          flash->thinker.function = T_LightFlash;
          P_AddThinker (&flash->thinker);
          break;
        }

      case tc_strobe:
        {
          strobe_t *strobe = Z_MallocLevel (sizeof(*strobe));
          P_LOAD_P(strobe);
          strobe->sector = &sectors[(size_t)strobe->sector];
          strobe->sector->lightingdata = strobe;
          strobe->thinker.function = T_StrobeFlash;
          P_AddThinker (&strobe->thinker);
          break;
        }

      case tc_glow:
        {
          glow_t *glow = Z_MallocLevel (sizeof(*glow));
          P_LOAD_P(glow);
          glow->sector = &sectors[(size_t)glow->sector];
          glow->sector->lightingdata = glow;
          glow->thinker.function = T_Glow;
          P_AddThinker (&glow->thinker);
          break;
        }

      case tc_zdoom_glow:
        {
          zdoom_glow_t *glow = Z_MallocLevel (sizeof(*glow));
          P_LOAD_P(glow);
          glow->sector = &sectors[(size_t)glow->sector];
          glow->sector->lightingdata = glow;
          glow->thinker.function = T_ZDoom_Glow;
          P_AddThinker (&glow->thinker);
          break;
        }

      case tc_flicker:           // killough 10/4/98
        {
          fireflicker_t *flicker = Z_MallocLevel (sizeof(*flicker));
          P_LOAD_P(flicker);
          flicker->sector = &sectors[(size_t)flicker->sector];
          flicker->sector->lightingdata = flicker;
          flicker->thinker.function = T_FireFlicker;
          P_AddThinker (&flicker->thinker);
          break;
        }

      case tc_zdoom_flicker:
        {
          zdoom_flicker_t *flicker = Z_MallocLevel (sizeof(*flicker));
          P_LOAD_P(flicker);
          flicker->sector = &sectors[(size_t)flicker->sector];
          flicker->sector->lightingdata = flicker;
          flicker->thinker.function = T_ZDoom_Flicker;
          P_AddThinker (&flicker->thinker);
          break;
        }

        //jff 2/22/98 new case for elevators
      case tc_elevator:
        {
          elevator_t *elevator = Z_MallocLevel (sizeof(*elevator));
          P_LOAD_P(elevator);
          elevator->sector = &sectors[(size_t)elevator->sector];
          elevator->sector->floordata = elevator; //jff 2/22/98
          elevator->sector->ceilingdata = elevator; //jff 2/22/98
          elevator->thinker.function = T_MoveElevator;
          P_AddThinker (&elevator->thinker);
          break;
        }

      case tc_scroll_side:
        {
          scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->thinker.function = dsda_UpdateSideScroller;
          P_AddThinker(&scroll->thinker);
          break;
        }

      case tc_scroll_floor:
        {
          scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->thinker.function = dsda_UpdateFloorScroller;
          P_AddThinker(&scroll->thinker);
          break;
        }

      case tc_scroll_ceiling:
        {
          scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->thinker.function = dsda_UpdateCeilingScroller;
          P_AddThinker(&scroll->thinker);
          break;
        }

      case tc_scroll_floor_carry:
        {
          scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->thinker.function = dsda_UpdateFloorCarryScroller;
          P_AddThinker(&scroll->thinker);
          break;
        }

      case tc_zdoom_scroll_floor:
        {
          scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->thinker.function = dsda_UpdateZDoomFloorScroller;
          P_AddThinker(&scroll->thinker);
          break;
        }

      case tc_zdoom_scroll_ceiling:
        {
          scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->thinker.function = dsda_UpdateZDoomCeilingScroller;
          P_AddThinker(&scroll->thinker);
          break;
        }

      case tc_thrust:
        {
          scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->thinker.function = dsda_UpdateThruster;
          P_AddThinker(&scroll->thinker);
          break;
        }

      case tc_scroll_side_control:
        {
          control_scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->scroll.thinker.function = dsda_UpdateControlSideScroller;
          P_AddThinker(&scroll->scroll.thinker);
          break;
        }

      case tc_scroll_floor_control:
        {
          control_scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->scroll.thinker.function = dsda_UpdateControlFloorScroller;
          P_AddThinker(&scroll->scroll.thinker);
          break;
        }

      case tc_scroll_ceiling_control:
        {
          control_scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->scroll.thinker.function = dsda_UpdateControlCeilingScroller;
          P_AddThinker(&scroll->scroll.thinker);
          break;
        }

      case tc_scroll_floor_carry_control:
        {
          control_scroll_t *scroll = Z_MallocLevel (sizeof(*scroll));
          P_LOAD_P(scroll);
          scroll->scroll.thinker.function = dsda_UpdateControlFloorCarryScroller;
          P_AddThinker(&scroll->scroll.thinker);
          break;
        }

      case tc_pusher:   // phares 3/22/98: new Push/Pull effect thinkers
        {
          pusher_t *pusher = Z_MallocLevel (sizeof(pusher_t));
          P_LOAD_P(pusher);
          pusher->thinker.function = T_Pusher;
          pusher->source = P_GetPushThing(pusher->affectee);
          P_AddThinker(&pusher->thinker);
          break;
        }

      case tc_friction:
        {
          friction_t *friction = Z_MallocLevel (sizeof(friction_t));
          P_LOAD_P(friction);
          friction->thinker.function = T_Friction;
          P_AddThinker(&friction->thinker);
          break;
        }

      case tc_light:
        {
          light_t *light = Z_MallocLevel(sizeof(*light));
          P_LOAD_P(light);
          light->sector = &sectors[(size_t)light->sector];
          light->thinker.function = T_Light;
          P_AddThinker(&light->thinker);
          break;
        }

      case tc_phase:
        {
          phase_t *phase = Z_MallocLevel(sizeof(*phase));
          P_LOAD_P(phase);
          phase->sector = &sectors[(size_t)phase->sector];
          phase->sector->lightingdata = phase;
          phase->thinker.function = T_Phase;
          P_AddThinker(&phase->thinker);
          break;
        }

      case tc_pillar:
        {
          pillar_t *pillar = Z_MallocLevel(sizeof(*pillar));
          P_LOAD_P(pillar);
          pillar->sector = &sectors[(size_t)pillar->sector];
          pillar->sector->floordata = pillar;
          pillar->thinker.function = T_BuildPillar;
          P_AddThinker(&pillar->thinker);
          break;
        }

      case tc_mobj:
        {
          mobj_t *mobj = Z_MallocLevel(sizeof(mobj_t));

          // killough 2/14/98 -- insert pointers to thinkers into table, in order:
          mobj_count++;
          mobj_p[mobj_count] = mobj;

          P_LOAD_P(mobj);

          mobj->state = states + (intptr_t) mobj->state;

          if (mobj->player)
            (mobj->player = &players[(size_t) mobj->player - 1]) -> mo = mobj;

          mobj->info = &mobjinfo[mobj->type];

          // Don't place objects marked for deletion
          if (mobj->index == MARKED_FOR_DELETION)
          {
            mobj->thinker.function = P_RemoveThinkerDelayed;
            P_AddThinker(&mobj->thinker);

            // The references value must be nonzero to reach the target code
            mobj->thinker.references = 1;
            break;
          }

          P_SetThingPosition (mobj, 0);

          // killough 2/28/98:
          // Fix for falling down into a wall after savegame loaded:
          //      mobj->floorz = mobj->subsector->sector->floorheight;
          //      mobj->ceilingz = mobj->subsector->sector->ceilingheight;

          mobj->thinker.function = P_MobjThinker;
          P_AddThinker (&mobj->thinker);

          if (!((mobj->flags ^ MF_COUNTKILL) & (MF_FRIEND | MF_COUNTKILL | MF_CORPSE)))
            totallive++;
          break;
        }

      default:
        I_Error("P_UnarchiveSpecials: Unknown tc %i in extraction", tc);
    }
  }

  // killough 2/14/98: adjust target and tracer fields, plus
  // lastenemy field, to correctly point to mobj thinkers.
  // NULL entries automatically handled by first table entry.
  //
  // killough 11/98: use P_SetNewTarget() to set fields

  for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
  {
    if (P_IsMobjThinker(th))
    {
      P_ReplaceIndexWithMobj(&((mobj_t *) th)->target, mobj_p, mobj_count);
      P_ReplaceIndexWithMobj(&((mobj_t *) th)->tracer, mobj_p, mobj_count);
      P_ReplaceIndexWithMobj(&((mobj_t *) th)->lastenemy, mobj_p, mobj_count);

      // restore references now that targets are set
      if (((mobj_t *) th)->index == MARKED_FOR_DELETION)
      {
        ((mobj_t *) th)->index = -1;
        th->references--;
      }
    }
  }

  {  // killough 9/14/98: restore soundtargets
    int i;
    for (i = 0; i < numsectors; i++)
    {
      P_LOAD_X(sectors[i].soundtarget);
      // Must verify soundtarget. See P_ArchiveThinkers.
      P_ReplaceIndexWithMobj(&sectors[i].soundtarget, mobj_p, mobj_count);
    }
  }

  P_UnArchiveBlockLinks(mobj_p, mobj_count);
  P_UnArchiveThinkerSubclasses(mobj_p, mobj_count);

  Z_Free(mobj_p);    // free translation table

  // TODO: not in sync, need to save and load existing order
  if (map_format.thing_id)
  {
    map_format.build_mobj_thing_id_list();
  }

  // killough 3/26/98: Spawn icon landings:
  if (gamemode == commercial)
  {
    // P_SpawnBrainTargets overwrites brain.targeton and brain.easy with zero.
    struct brain_s brain_tmp = brain; // saving

    P_SpawnBrainTargets();

    // old demos with save/load tics should not be affected by this fix
    if (!prboom_comp[PC_RESET_MONSTERSPAWNER_PARAMS_AFTER_LOADING].state)
    {
      brain = brain_tmp; // restoring
    }
  }
}

// hexen

void P_ArchiveACS(void)
{
}

void P_UnArchiveACS(void)
{
}

static void P_ArchiveVertex(vertex_t *v)
{
  P_SAVE_X(v->x);

  P_SAVE_X(v->y);
}

static void P_UnArchiveVertex(vertex_t *v)
{
  P_LOAD_X(v->x);

  P_LOAD_X(v->y);
}

void P_ArchivePolyobjs(void)
{
}

void P_UnArchivePolyobjs(void)
{
}

void P_ArchiveScripts(void)
{
}

void P_UnArchiveScripts(void)
{
}

void P_ArchiveSounds(void)
{
}

void P_UnArchiveSounds(void)
{
}

void P_ArchiveAmbientSound(void)
{
}

void P_UnArchiveAmbientSound(void)
{
}

void P_ArchiveMisc(void)
{


}

void P_UnArchiveMisc(void)
{

}


/// Headless functions

void headlessSetSaveStatePointer(void* savePtr, int saveStateSize)
{ 
  save_p = savePtr;
  savebuffer = savePtr;
  savegamesize = saveStateSize;
}

size_t headlessGetEffectiveSaveSize()
{ 
  return (size_t)save_p - (size_t)savebuffer;
}
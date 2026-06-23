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
 *      Zone Memory Allocation. Neat.
 *
 * Neat enough to be rewritten by Lee Killough...
 *
 * Must not have been real neat :)
 *
 * Made faster and more general, and added wrappers for all of Doom's
 * memory allocation functions, including malloc() and similar functions.
 * Added line and file numbers, in case of error. Added performance
 * statistics and tunables.
 *-----------------------------------------------------------------------------
 */


// use config.h if autoconf made one -- josh
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include "z_zone.h"
#include "doomstat.h"
#include "v_video.h"
#include "g_game.h"
#include "lprintf.h"

#ifdef DJGPP
#include <dpmi.h>
#endif

#define ZONE_SIGNATURE 0x931d4a11

enum {
  ZONE_STATIC,
  ZONE_LEVEL,
  ZONE_MAX
};

typedef struct memblock {
  unsigned signature;
  struct memblock *next,*prev;
  size_t size;
  unsigned char tag;
} memblock_t;

static const size_t HEADER_SIZE = sizeof(memblock_t);

static __STORAGE_MODIFIER memblock_t *blockbytag[ZONE_MAX];

/* ---- Increment 3: contiguous per-thread "thinker" arena ----
 * Dynamic level objects (mobjs + the special thinkers) are bump-allocated from
 * a single contiguous per-thread slab with a segregated (per exact 16-byte size
 * class) free list, so the whole live dynamic set occupies one contiguous
 * region. That contiguity is the foundation for the bulk zone-snapshot save/load
 * path (a single memcpy + pointer relocation in place of the per-thinker field
 * walk). Routing is gated by dsda_thinker_arena_active, flipped on in
 * P_SetupLevel *after* the one-time map geometry is loaded and off before it,
 * so only the dynamic objects (never the static geometry) land in the slab.
 * Blocks keep the ordinary memblock_t header and blockbytag linkage, so the
 * existing pointer-swizzling archive path works unchanged on top of this. */
#ifndef DSDA_THINKER_ARENA_BYTES
#define DSDA_THINKER_ARENA_BYTES ((size_t)32 * 1024 * 1024)
#endif
#define TA_ALIGN       16u
#define TA_MAX_CLASSES 2048u   /* arena-eligible up to TA_MAX_CLASSES*TA_ALIGN = 32KB; larger falls back to malloc */

__STORAGE_MODIFIER int dsda_thinker_arena_active;       /* routing flag (set in p_setup.c) */
static __STORAGE_MODIFIER char       *ta_base;          /* slab base (malloc'd once per thread) */
static __STORAGE_MODIFIER size_t      ta_high;          /* bump high-water mark (bytes used) */
static __STORAGE_MODIFIER size_t      ta_peak;          /* max high-water seen (for reporting/sizing) */
static __STORAGE_MODIFIER memblock_t *ta_free[TA_MAX_CLASSES]; /* segregated free lists, keyed by size class */

/* Reclaim the whole slab in O(1): drop the bump pointer and all free lists. The
 * caller must have already cleared every pointer into the slab (thinker list,
 * sector/blockmap heads, block-zone pools). Used by the bulk state-load teardown
 * to abandon the previous state's objects without walking them. The routing flag
 * is left untouched so subsequent allocations keep using the slab. */
void Z_ResetThinkerArena(void)
{
  ta_high = 0;
  memset(ta_free, 0, sizeof(ta_free));
}

/* Reset the slab to empty and start routing dynamic allocations into it. Called
 * after the geometry load, when no thinker objects are live. */
void Z_BeginThinkerArena(void)
{
  Z_ResetThinkerArena();
  dsda_thinker_arena_active = 1;
}

/* Stop routing into the slab (e.g. while (re)loading static geometry). */
void Z_EndThinkerArena(void)
{
  dsda_thinker_arena_active = 0;
}

size_t Z_ThinkerArenaUsed(void) { return ta_high; }
size_t Z_ThinkerArenaPeak(void) { return ta_peak; }

static memblock_t *Z_ArenaCarve(size_t total, unsigned cls)
{
  memblock_t *b;
  if (ta_free[cls])                 /* exact-size reuse: perfect for fixed-size structs */
  {
    b = ta_free[cls];
    ta_free[cls] = b->next;
    return b;
  }
  if (!ta_base)
  {
    ta_base = malloc(DSDA_THINKER_ARENA_BYTES);
    if (!ta_base)
      I_Error("Z_ArenaCarve: failed to reserve %lu-byte thinker arena", (unsigned long) DSDA_THINKER_ARENA_BYTES);
  }
  if (ta_high + total > DSDA_THINKER_ARENA_BYTES)
    I_Error("Z_ArenaCarve: thinker arena exhausted (need %lu)", (unsigned long)(ta_high + total));
  b = (memblock_t *)(ta_base + ta_high);
  ta_high += total;
  if (ta_high > ta_peak) ta_peak = ta_high;
  return b;
}

static inline int Z_InArena(const void *block)
{
  return ta_base && (const char *)block >= ta_base && (const char *)block < ta_base + ta_high;
}

/* Z_Malloc
 * cph - the algorithm here was a very simple first-fit round-robin
 *  one - just keep looping around, freeing everything we can until
 *  we get a large enough space
 *
 * This has been changed now; we still do the round-robin first-fit,
 * but we only free the blocks we actually end up using; we don't
 * free all the stuff we just pass on the way.
 */

static void *Z_MallocTag(size_t size, int tag)
{
  memblock_t *block = NULL;
  size_t blocksize = size;

  if (!size)
    return NULL; // malloc(0) returns NULL

  // Increment 3: route dynamic level objects into the contiguous thinker arena.
  if (tag == ZONE_LEVEL && dsda_thinker_arena_active)
  {
    size_t rounded = (size + (TA_ALIGN - 1)) & ~(size_t)(TA_ALIGN - 1);
    unsigned cls = (unsigned)(rounded / TA_ALIGN);
    if (cls < TA_MAX_CLASSES)
    {
      block = Z_ArenaCarve(rounded + HEADER_SIZE, cls);
      blocksize = rounded;   // capacity == class size, so re-free maps back to the same class
    }
  }

  if (!block && !(block = malloc(blocksize + HEADER_SIZE)))
  {
    I_Error ("Z_Malloc: Failure trying to allocate %lu bytes", (unsigned long) size);
  }

  if (!blockbytag[tag])
  {
    blockbytag[tag] = block;
    block->next = block->prev = block;
  }
  else
  {
    blockbytag[tag]->prev->next = block;
    block->prev = blockbytag[tag]->prev;
    block->next = blockbytag[tag];
    blockbytag[tag]->prev = block;
  }

  block->size = blocksize;
  block->signature = ZONE_SIGNATURE;
  block->tag = tag;           // tag
  block = (memblock_t *)((char *) block + HEADER_SIZE);

  return block;
}

void Z_Free(void *p)
{
  memblock_t *block = (memblock_t *)((char *) p - HEADER_SIZE);

  if (!p)
    return;

  if (block->signature != ZONE_SIGNATURE)
  {
    fprintf(stderr,"Z_Free: freed a non-zone pointer");
    abort();
  }
  block->signature = 0;       // Nullify signature so another free fails

  if (block == block->next)
    blockbytag[block->tag] = NULL;
  else
    if (blockbytag[block->tag] == block)
      blockbytag[block->tag] = block->next;
  block->prev->next = block->next;
  block->next->prev = block->prev;

  // Increment 3: arena blocks return to their size-class free list, not the heap.
  if (Z_InArena(block))
  {
    unsigned cls = (unsigned)(block->size / TA_ALIGN);
    block->next = ta_free[cls];
    ta_free[cls] = block;
    return;
  }

  free(block);
}

static void Z_FreeTag(int tag)
{
  memblock_t *block, *end_block;

  if (tag < 0 || tag >= ZONE_MAX)
    I_Error("Z_FreeTag: Tag %i does not exist", tag);

  block = blockbytag[tag];
  if (!block)
    return;
  end_block = block->prev;
  while (1)
  {
    memblock_t *next = block->next;
    Z_Free((char *) block + HEADER_SIZE);
    if (block == end_block)
      break;
    block = next;               // Advance to next block
  }
}

static void *Z_ReallocTag(void *ptr, size_t n, int tag)
{
  void *p = Z_MallocTag(n, tag);
  if (ptr)
    {
      memblock_t *block = (memblock_t *)((char *) ptr - HEADER_SIZE);
      memcpy(p, ptr, n <= block->size ? n : block->size);
      Z_Free(ptr);
    }
  return p;
}

static void *Z_CallocTag(size_t n1, size_t n2, int tag)
{
  return
    (n1*=n2) ? memset(Z_MallocTag(n1, tag), 0, n1) : NULL;
}

static char *Z_StrdupTag(const char *s, int tag)
{
  return strcpy(Z_MallocTag(strlen(s)+1, tag), s);
}

void *Z_Malloc(size_t size)
{
  return Z_MallocTag(size, ZONE_STATIC);
}

void *Z_Calloc(size_t n, size_t n2)
{
  return Z_CallocTag(n, n2, ZONE_STATIC);
}

void *Z_Realloc(void *p, size_t n)
{
  return Z_ReallocTag(p, n, ZONE_STATIC);
}

char *Z_Strdup(const char *s)
{
  return Z_StrdupTag(s, ZONE_STATIC);
}

void Z_FreeLevel(void)
{
  return Z_FreeTag(ZONE_LEVEL);
}

void *Z_MallocLevel(size_t size)
{
  return Z_MallocTag(size, ZONE_LEVEL);
}

void *Z_CallocLevel(size_t n, size_t n2)
{
  return Z_CallocTag(n, n2, ZONE_LEVEL);
}

void *Z_ReallocLevel(void *p, size_t n)
{
  return Z_ReallocTag(p, n, ZONE_LEVEL);
}

char *Z_StrdupLevel(const char *s)
{
  return Z_StrdupTag(s, ZONE_LEVEL);
}

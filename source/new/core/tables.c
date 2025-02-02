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
 *      Lookup tables.
 *      Do not try to look them up :-).
 *      In the order of appearance:
 *
 *      int finetangent[4096]   - Tangens LUT.
 *       Should work with BAM fairly well (12 of 16bit,
 *      effectively, by shifting).
 *
 *      int finesine[10240]             - Sine lookup.
 *       Guess what, serves as cosine, too.
 *       Remarkable thing is, how to use BAMs with this?
 *
 *      int tantoangle[2049]    - ArcTan LUT,
 *        maps tan(angle) to angle fast. Gotta search.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include "w_wad.h"
#include "tables.h"

// killough 5/3/98: reformatted

int SlopeDiv(unsigned int num, unsigned int den)
{
  unsigned ans;

  if (den < 512)
    return SLOPERANGE;
  ans = (num<<3)/(den>>8);
  return ans <= SLOPERANGE ? ans : SLOPERANGE;
}

// [crispy] catch SlopeDiv overflows, only used in rendering
int SlopeDivEx(unsigned int num, unsigned int den)
{
  uint64_t ans;
  if (den < 512)
    return SLOPERANGE;
  ans = ((uint64_t)num<<3)/(den>>8);
  return ans <= SLOPERANGE ? (int)ans : SLOPERANGE;
}

fixed_t __thread finetangent[4096];

//const fixed_t *const finecosine = &finesine[FINEANGLES/4];

fixed_t finesine[10240];

angle_t __thread tantoangle[2049];

#include "m_swap.h"
#include "lprintf.h"

// R_LoadTrigTables
// Load trig tables from a wad file lump
// CPhipps 24/12/98 - fix endianness (!)
//
void R_LoadTrigTables(void)
{
  int lump;
  {
    lump = W_CheckNumForName2("SINETABL",ns_prboom);
    if (lump == LUMP_NOT_FOUND)
      I_Error("Failed to locate trig tables");
    if (W_LumpLength(lump) != sizeof(finesine))
      I_Error("R_LoadTrigTables: Invalid SINETABL");
    W_ReadLump(lump,(unsigned char*)finesine);
  }
  {
    lump = W_CheckNumForName2("TANGTABL",ns_prboom);
    if (lump == LUMP_NOT_FOUND)
      I_Error("Failed to locate trig tables");
    if (W_LumpLength(lump) != sizeof(finetangent))
      I_Error("R_LoadTrigTables: Invalid TANGTABL");
    W_ReadLump(lump,(unsigned char*)finetangent);
  }
  {
    lump = W_CheckNumForName2("TANTOANG",ns_prboom);
    if (lump == LUMP_NOT_FOUND)
      I_Error("Failed to locate trig tables");
    if (W_LumpLength(lump) != sizeof(tantoangle))
      I_Error("R_LoadTrigTables: Invalid TANTOANG");
    W_ReadLump(lump,(unsigned char*)tantoangle);
  }
  // Endianness correction - might still be non-portable, but is fast where possible
  {
    size_t n;
    lprintf(LO_DEBUG, "Endianness...");

    // This test doesn't assume the endianness of the tables, but deduces them from
    // en entry. I hope this is portable.
    if ((10 < finesine[1]) && (finesine[1] < 100)) {
      lprintf(LO_DEBUG, "ok.");
      return; // Endianness is correct
    }

    // Must correct endianness of every long loaded (!)
#define CORRECT_TABLE_ENDIAN(tbl) \
    for (n = 0; n<sizeof(tbl)/sizeof(tbl[0]); n++) tbl[n] = doom_swap_l(tbl[n])

    CORRECT_TABLE_ENDIAN(finesine);
    CORRECT_TABLE_ENDIAN(finetangent);
    CORRECT_TABLE_ENDIAN(tantoangle);
    lprintf(LO_DEBUG, "corrected.");
  }
}

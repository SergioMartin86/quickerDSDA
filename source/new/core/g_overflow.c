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
 *
 *---------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "g_overflow.h"

#include "doomstat.h"
#include "lprintf.h"
#include "m_misc.h"
#include "w_wad.h"
#include "e6y.h"

#include "dsda/args.h"

__thread int overflows_enabled = true;

__thread overrun_param_t overflows[OVERFLOW_MAX];
__thread const char *overflow_cfgname[OVERFLOW_MAX] =
{
  "overrun_spechit_emulate",
  "overrun_reject_emulate",
  "overrun_intercept_emulate",
  "overrun_playeringame_emulate",
  "overrun_donut_emulate",
  "overrun_missedbackside_emulate"
};

void ResetOverruns(void)
{
  overrun_list_t overflow;
  for (overflow = 0; overflow < OVERFLOW_MAX; ++overflow)
  {
    overflows[overflow].happened = false;
  }
}

static void ShowOverflowWarning(overrun_list_t overflow, int fatal, const char *params, ...)
{
  overflows[overflow].happened = true;

  if (overflows[overflow].warn && !overflows[overflow].promted)
  {
    va_list argptr;
    char buffer[1024];

    static const char *name[OVERFLOW_MAX] = {
      "SPECHIT", "REJECT", "INTERCEPT", "PLYERINGAME", "DONUT", "MISSEDBACKSIDE"};

    static const char str1[] =
      "Too big or not supported %s overflow has been detected. "
      "Desync or crash can occur soon "
      "or during playback with the vanilla engine in case you're recording demo.%s%s";

    static const char str2[] =
      "%s overflow has been detected.%s%s";

    static const char str3[] =
      "%s overflow has been detected. "
      "The option responsible for emulation of this overflow is switched off "
      "hence desync or crash can occur soon "
      "or during playback with the vanilla engine in case you're recording demo.%s%s";

    overflows[overflow].promted = true;

    sprintf(buffer,
      (fatal ? str1 : (EMULATE(overflow) ? str2 : str3)),
      name[overflow],
      "\nYou can change PrBoom behaviour for this overflow through in-game menu.",
      params);

    va_start(argptr, params);
    I_vWarning(buffer, argptr);
    va_end(argptr);
  }
}

// e6y
//
// Intercepts Overrun emulation
// See more information on:
// doomworld.com/vb/doom-speed-demos/35214-spechits-reject-and-intercepts-overflow-lists
//
// Thanks to Simon Howard (fraggle) for refactor the intercepts
// overrun code so that it should work properly on big endian machines
// as well as little endian machines.

// Overwrite a specific memory location with a value.
static void InterceptsMemoryOverrun(int location, int value)
{
  int i, offset;
  int index;
  void *addr;
  void *addr2;

  i = 0;
  offset = 0;

  // Search down the array until we find the right entry

  while (intercepts_overrun[i].len != 0)
  {
    if (offset + intercepts_overrun[i].len > location)
    {
      addr = intercepts_overrun[i].addr;
      addr2 = intercepts_overrun[i].addr2;

      // Write the value to the memory location.
      // 16-bit and 32-bit values are written differently.

      if (addr != NULL)
      {
        // 2 shorts
        if (intercepts_overrun[i].addr2)
        {
          *((short *) addr) = value & 0xffff;
          *((short *) addr2) = (value >> 16) & 0xffff;
        }
        else
        {
          index = (location - offset) / 4;
          ((int *) addr)[index] = value;
        }
      }

      break;
    }

    offset += intercepts_overrun[i].len;
    ++i;
  }
}

void InterceptsOverrun(int num_intercepts, intercept_t *intercept)
{
  void P_MustRebuildBlockmap(void);

  if (num_intercepts > MAXINTERCEPTS_ORIGINAL && demo_compatibility && PROCESS(OVERFLOW_INTERCEPT))
  {
    ShowOverflowWarning(OVERFLOW_INTERCEPT, false, "");

    if (EMULATE(OVERFLOW_INTERCEPT))
    {
      int location = (num_intercepts - MAXINTERCEPTS_ORIGINAL - 1) * 12;

      // Overwrite memory that is overwritten in Vanilla Doom, using
      // the values from the intercept structure.
      //
      // Note: the ->d.{thing,line} member should really have its
      // address translated into the correct address value for
      // Vanilla Doom.

      InterceptsMemoryOverrun(location, intercept->frac);
      InterceptsMemoryOverrun(location + 4, intercept->isaline);
      InterceptsMemoryOverrun(location + 8, (int)(intptr_t) intercept->d.thing);

      P_MustRebuildBlockmap();
    }
  }
}

// e6y
// playeringame overrun emulation
// it detects and emulates overflows on vex6d.wad\bug_wald(toke).lmp, etc.
// http://www.doom2.net/doom2/research/runningbody.zip

int PlayeringameOverrun(const mapthing_t* mthing)
{
  if (mthing->type == 0 && PROCESS(OVERFLOW_PLAYERINGAME))
  {
    // playeringame[-1] == players[3].didsecret
    ShowOverflowWarning(OVERFLOW_PLAYERINGAME, (players + 3)->didsecret, "");

    if (EMULATE(OVERFLOW_PLAYERINGAME))
    {
      return true;
    }
  }
  return false;
}

//
// spechit overrun emulation
//

__thread unsigned int spechit_baseaddr = 0;

// e6y
// Code to emulate the behavior of Vanilla Doom when encountering an overrun
// of the spechit array.
// No more desyncs on compet-n\hr.wad\hr18*.lmp, all strain.wad\map07 demos etc.
// http://www.doomworld.com/vb/showthread.php?s=&threadid=35214
// See more information on:
// doomworld.com/vb/doom-speed-demos/35214-spechits-reject-and-intercepts-overflow-lists
void SpechitOverrun(spechit_overrun_param_t *params)
{
}

//
// reject overrun emulation
//

// padding the reject table if it is too short
// totallines must be the number returned by P_GroupLines()
// an underflow will be padded with zeroes, or a doom.exe z_zone header
//
// e6y
// reject overrun emulation code
// It's emulated successfully if the size of overflow no more than 16 bytes.
// No more desync on teeth-32.wad\teeth-32.lmp.
// http://www.doomworld.com/vb/showthread.php?s=&threadid=35214

void RejectOverrun(unsigned int length, const byte **rejectmatrix, int totallines)
{
}

//
// Read Access Violation emulation.
//

// C:\>debug
// -d 0:0
//
// DOS 6.22:
// 0000:0000  (57 92 19 00) F4 06 70 00-(16 00)
// DOS 7.1:
// 0000:0000  (9E 0F C9 00) 65 04 70 00-(16 00)
// Win98:
// 0000:0000  (9E 0F C9 00) 65 04 70 00-(16 00)
// DOSBox under XP:
// 0000:0000  (00 00 00 F1) ?? ?? ?? 00-(07 00)

#define DOS_MEM_DUMP_SIZE 10

unsigned char mem_dump_dos622[DOS_MEM_DUMP_SIZE] = {
  0x57, 0x92, 0x19, 0x00, 0xF4, 0x06, 0x70, 0x00, 0x16, 0x00};
unsigned char mem_dump_win98[DOS_MEM_DUMP_SIZE] = {
  0x9E, 0x0F, 0xC9, 0x00, 0x65, 0x04, 0x70, 0x00, 0x16, 0x00};
unsigned char mem_dump_dosbox[DOS_MEM_DUMP_SIZE] = {
  0x00, 0x00, 0x00, 0xF1, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00};

unsigned char *dos_mem_dump = mem_dump_dos622;

static int GetMemoryValue(unsigned int offset, void *value, int size)
{
  static int firsttime = true;

  if (firsttime)
  {
    int i, val;
    dsda_arg_t *arg;

    firsttime = false;
    i = 0;

    arg = dsda_Arg(dsda_arg_setmem);
    if (arg->found)
    {
      if (!strcasecmp(arg->value.v_string_array[0], "dos622"))
        dos_mem_dump = mem_dump_dos622;
      if (!strcasecmp(arg->value.v_string_array[0], "dos71"))
        dos_mem_dump = mem_dump_win98;
      else if (!strcasecmp(arg->value.v_string_array[0], "dosbox"))
        dos_mem_dump = mem_dump_dosbox;
      else
      {
        while (i < DOS_MEM_DUMP_SIZE)
        {
          M_StrToInt(arg->value.v_string_array[i], &val);
          dos_mem_dump[i++] = (unsigned char)val;
        }
      }
    }
  }

  if (value)
  {
    switch (size)
    {
    case 1:
      *((unsigned char*)value) = *((unsigned char*)(&dos_mem_dump[offset]));
      return true;
    case 2:
      *((unsigned short*)value) = *((unsigned short*)(&dos_mem_dump[offset]));
      return true;
    case 4:
      *((unsigned int*)value) = *((unsigned int*)(&dos_mem_dump[offset]));
      return true;
    }
  }

  return false;
}

//
// donut overrun emulation (linedef action #9)
//

#define DONUT_FLOORPIC_DEFAULT 0x16
int DonutOverrun(fixed_t *pfloorheight, short *pfloorpic)
{
  return false;
}


int MissedBackSideOverrun(line_t *line)
{
  return false;
}

//
// GetSectorAtNullAddress
//
sector_t* GetSectorAtNullAddress(void)
{
  static int null_sector_is_initialized = false;
  static sector_t null_sector;

  if (demo_compatibility && EMULATE(OVERFLOW_MISSEDBACKSIDE))
  {
    if (!null_sector_is_initialized)
    {
      memset(&null_sector, 0, sizeof(null_sector));
      null_sector.flags = NULL_SECTOR;
      GetMemoryValue(0, &null_sector.floorheight, 4);
      GetMemoryValue(4, &null_sector.ceilingheight, 4);
      null_sector_is_initialized = true;
    }

    return &null_sector;
  }

  return 0;
}

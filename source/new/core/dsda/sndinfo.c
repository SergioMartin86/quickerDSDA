//
// Copyright(C) 2022 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA SndInfo
//

#include <stddef.h>

#include "m_misc.h"
#include "sc_man.h"
#include "sounds.h"

#include "dsda/map_format.h"

#include "sndinfo.h"

static char song_lump[99][10];

static void ReadMapSongLumpName(int map, char* lump_name) {
  if (map < 1 || map > 98)
    return;

  M_StringCopy(song_lump[map], lump_name, sizeof(song_lump[map]));
}

const char* dsda_SndInfoMapSongLumpName(int map) {
  if (map < 1 || map > 98)
    return song_lump[0];

  return song_lump[map];
}

void dsda_LoadSndInfo(void) {
  int i;

    return;
}

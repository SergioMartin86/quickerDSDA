//
// Copyright(C) 2021 by Ryan Krafnick
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
//	DSDA Extended Cmd
//

#include <stdlib.h>
#include <string.h>

#include "doomstat.h"

#include "dsda/configuration.h"
#include "dsda/mapinfo.h"

#include "excmd.h"

static __thread dboolean excmd_enabled;
static __thread dboolean casual_excmd_features;

void dsda_EnableExCmd(void) {
  excmd_enabled = true;
}

void dsda_DisableExCmd(void) {
  excmd_enabled = false;
}

dboolean dsda_AllowExCmd(void) {
  return excmd_enabled;
}

// If we are reading a demo header, it might not be in playback mode yet
dboolean dsda_ExCmdDemo(void) {
  return excmd_enabled;
}

void dsda_EnableCasualExCmdFeatures(void) {
  void dsda_ResetAirControl(void);

  casual_excmd_features = true;

  dsda_ResetAirControl();
}

dboolean dsda_AllowCasualExCmdFeatures(void) {
  return casual_excmd_features;
}

dboolean dsda_AllowJumping(void) {
  return (dsda_IntConfig(dsda_config_allow_jumping))
         || map_info.flags & MI_ALLOW_JUMP
         || dsda_AllowCasualExCmdFeatures();
}

dboolean dsda_FreeAim(void) {
  return (dsda_IntConfig(dsda_config_freelook))
         || map_info.flags & MI_ALLOW_FREE_LOOK;
}

dboolean dsda_AllowFreeLook(void) {
  return dsda_FreeAim() || dsda_AllowCasualExCmdFeatures();
}

void dsda_ReadExCmd(ticcmd_t* cmd, const byte** p) {
  const byte* demo_p = *p;

  if (!dsda_AllowExCmd()) return;

  cmd->ex.actions = *demo_p++;
  if (cmd->ex.actions & XC_SAVE)
    cmd->ex.save_slot = *demo_p++;
  if (cmd->ex.actions & XC_LOAD)
    cmd->ex.load_slot = *demo_p++;

  if (cmd->ex.actions & XC_LOOK) {
    signed short lowbyte = *demo_p++;
    cmd->ex.look = ((signed short) (*demo_p++) << 8) + lowbyte;
  }
  else
    cmd->ex.look = 0;

  *p = demo_p;
}

void dsda_WriteExCmd(char** p, ticcmd_t* cmd) {
  char* demo_p = *p;

  if (!dsda_AllowExCmd()) return;

  *demo_p++ = cmd->ex.actions;
  if (cmd->ex.actions & XC_SAVE)
    *demo_p++ = cmd->ex.save_slot;
  if (cmd->ex.actions & XC_LOAD)
    *demo_p++ = cmd->ex.load_slot;

  if (cmd->ex.actions & XC_LOOK) {
    *demo_p++ = cmd->ex.look & 0xff;
    *demo_p++ = (cmd->ex.look >> 8) & 0xff;
  }

  *p = demo_p;
}

static __thread excmd_t excmd_queue;

void dsda_ResetExCmdQueue(void) {
  memset(&excmd_queue, 0, sizeof(excmd_queue));
}

void dsda_PopExCmdQueue(ticcmd_t* cmd) {
  cmd->ex = excmd_queue;
  memset(&excmd_queue, 0, sizeof(excmd_queue));
}

void dsda_QueueExCmdJump(void) {
  excmd_queue.actions |= XC_JUMP;
}

void dsda_QueueExCmdLook(short look) {
  excmd_queue.actions |= XC_LOOK;
  excmd_queue.look = look;
}

void dsda_QueueExCmdSave(int slot) {
  excmd_queue.actions |= XC_SAVE;
  excmd_queue.save_slot = slot;
}

void dsda_QueueExCmdLoad(int slot) {
  excmd_queue.actions |= XC_LOAD;
  excmd_queue.load_slot = slot;
}

void dsda_QueueExCmdGod(void) {
  excmd_queue.actions |= XC_GOD;
}

void dsda_QueueExCmdNoClip(void) {
  excmd_queue.actions |= XC_NOCLIP;
}

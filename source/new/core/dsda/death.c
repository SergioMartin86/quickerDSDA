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
//	DSDA Death
//

#include "doomstat.h"
#include "d_player.h"
#include "v_video.h"

#include "dsda/configuration.h"
#include "dsda/excmd.h"
#include "dsda/mapinfo.h"
#include "dsda/save.h"
#include "dsda/skill_info.h"

#include "death.h"

extern int inv_ptr;
extern int curpos;
extern int newtorch;
extern int newtorchdelta;

typedef enum {
  death_use_default,
  death_use_nothing,
  death_use_reload,
} death_use_action_t;

static int dsda_DeathUseAction(void)
{
  if (demorecording ||
      demoplayback ||
      map_info.flags & MI_ALLOW_RESPAWN ||
      skill_info.flags & SI_PLAYER_RESPAWN)
    return death_use_default;

  return dsda_IntConfig(dsda_config_death_use_action);
}

void dsda_DeathUse(player_t* player) {
  switch (dsda_DeathUseAction())
  {
    case death_use_default:
    default:

      player->playerstate = PST_REBORN;
      break;
    case death_use_nothing:
      break;
    case death_use_reload:
      {
        int slot = dsda_LastSaveSlot();
        static int last_load_tic;

        if (slot >= 0 && gametic > last_load_tic + 1) {
          last_load_tic = gametic;
          dsda_QueueExCmdLoad(slot);
        }
      }
      break;
  }
}

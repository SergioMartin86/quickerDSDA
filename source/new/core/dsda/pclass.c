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
//	DSDA Player Class
//

#include "info.h"

#include "pclass.h"

dsda_pclass_t __thread pclass = {
    .armor_increment = 0 ,
    .auto_armor_save = 0,
    .armor_max = 0,

    .forwardmove = { 0x19, 0x32 },
    .sidemove = { 0x18, 0x28 },
    .stroller_threshold = 0x19,
    .turbo_threshold = 0x32,
};

void dsda_ResetNullPClass(void) {
    pclass.normal_state = S_PLAY;
    pclass.run_state = S_PLAY_RUN1;
    pclass.fire_weapon_state = S_PLAY_ATK1;
    pclass.attack_state = S_PLAY_ATK1;
    pclass.attack_end_state = S_PLAY_ATK2;
}

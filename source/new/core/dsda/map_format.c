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
//	DSDA Map Format
//

#include "doomstat.h"
#include "lprintf.h"
#include "p_spec.h"
#include "r_main.h"
#include "r_state.h"
#include "w_wad.h"

#include "dsda/args.h"

#include "map_format.h"

__thread map_format_t map_format;

typedef enum {
  door_type_none = -1,
  door_type_red,
  door_type_blue,
  door_type_yellow,
  door_type_unknown = door_type_yellow,
  door_type_multiple
} door_type_t;

int dsda_DoorType(int index) {
  int special = lines[index].special;


  if (GenLockedBase <= special && special < GenDoorBase) {
    special -= GenLockedBase;
    special = (special & LockedKey) >> LockedKeyShift;
    if (!special || special == 7)
      return door_type_multiple;
    else
      return (special - 1) % 3;
  }

  switch (special) {
    case 26:
    case 32:
    case 99:
    case 133:
      return door_type_blue;
    case 27:
    case 34:
    case 136:
    case 137:
      return door_type_yellow;
    case 28:
    case 33:
    case 134:
    case 135:
      return door_type_red;
    default:
      return door_type_none;
  }
}

dboolean dsda_IsExitLine(int index) {
  int special = lines[index].special;

  return special == 11  ||
         special == 52  ||
         special == 197 ||
         special == 51  ||
         special == 124 ||
         special == 198;
}

dboolean dsda_IsTeleportLine(int index) {
  int special = lines[index].special;

  return special == 39  ||
         special == 97  ||
         special == 125 ||
         special == 126;
}

extern void P_SpawnCompatibleSectorSpecial(sector_t *sector, int i);
extern void P_PlayerInCompatibleSector(player_t *player, sector_t *sector);
extern void P_SpawnCompatibleScroller(line_t *l, int i);
extern void P_SpawnCompatibleFriction(line_t *l);
extern void P_SpawnCompatiblePusher(line_t *l);
extern void P_SpawnCompatibleExtra(line_t *l, int i);
extern void P_CrossCompatibleSpecialLine(line_t *line, int side, mobj_t *thing, dboolean bossaction);
extern void P_ShootCompatibleSpecialLine(mobj_t *thing, line_t *line);
extern void P_PostProcessCompatibleLineSpecial(line_t *ld);
extern void P_PostProcessCompatibleSidedefSpecial(side_t *sd, const mapsidedef_t *msd, sector_t *sec, int i);
extern void P_CheckCompatibleImpact(mobj_t *);
extern void P_TranslateCompatibleLineFlags(unsigned int *, line_activation_t *);
extern void P_ApplyCompatibleSectorMovementSpecial(mobj_t *, int);
extern dboolean P_MobjInCompatibleSector(mobj_t *);
extern void P_CompatiblePlayerThrust(player_t* player, angle_t angle, fixed_t move);
extern void T_VerticalCompatibleDoor(vldoor_t *door);
extern void T_MoveCompatibleFloor(floormove_t *);

void T_MoveCompatibleCeiling(ceiling_t * ceiling);
int EV_CompatibleTeleport(short thing_id, int tag, line_t *line, int side, mobj_t *thing, int flags);
void T_CompatiblePlatRaise(plat_t * plat);

void P_CreateTIDList(void);
void dsda_BuildMobjThingIDList(void);

void P_InsertMobjIntoTIDList(mobj_t * mobj, short tid);
void dsda_AddMobjThingID(mobj_t* mo, short thing_id);

void P_RemoveMobjFromTIDList(mobj_t * mobj);
void dsda_RemoveMobjThingID(mobj_t* mo);

void P_IterateCompatibleSpecHit(mobj_t *thing, fixed_t oldx, fixed_t oldy);

static const map_format_t doom_map_format = {
  .generalized_mask = ~31,
  .switch_activation = 0, // not used
  .init_sector_special = P_SpawnCompatibleSectorSpecial,
  .player_in_special_sector = P_PlayerInCompatibleSector,
  .mobj_in_special_sector = P_MobjInCompatibleSector,
  .spawn_scroller = P_SpawnCompatibleScroller,
  .spawn_friction = P_SpawnCompatibleFriction,
  .spawn_pusher = P_SpawnCompatiblePusher,
  .spawn_extra = P_SpawnCompatibleExtra,
  .cross_special_line = P_CrossCompatibleSpecialLine,
  .shoot_special_line = P_ShootCompatibleSpecialLine,
  .test_activate_line = NULL, // not used
  .execute_line_special = NULL, // not used
  .post_process_line_special = P_PostProcessCompatibleLineSpecial,
  .post_process_sidedef_special = P_PostProcessCompatibleSidedefSpecial,
  .animate_surfaces = NULL,
  .check_impact = P_CheckCompatibleImpact,
  .translate_line_flags = P_TranslateCompatibleLineFlags,
  .apply_sector_movement_special = P_ApplyCompatibleSectorMovementSpecial,
  .t_vertical_door = T_VerticalCompatibleDoor,
  .t_move_floor = T_MoveCompatibleFloor,
  .t_move_ceiling = T_MoveCompatibleCeiling,
  .t_build_pillar = NULL, // not used
  .t_plat_raise = T_CompatiblePlatRaise,
  .ev_teleport = EV_CompatibleTeleport,
  .player_thrust = P_CompatiblePlayerThrust,
  .build_mobj_thing_id_list = NULL, // not used
  .add_mobj_thing_id = NULL, // not used
  .remove_mobj_thing_id = NULL, // not used
  .iterate_spechit = P_IterateCompatibleSpecHit,
  .point_on_side = R_CompatiblePointOnSide,
  .point_on_seg_side = R_CompatiblePointOnSegSide,
  .point_on_line_side = P_CompatiblePointOnLineSide,
  .point_on_divline_side = P_CompatiblePointOnDivlineSide,
  .mapthing_size = sizeof(doom_mapthing_t),
  .maplinedef_size = sizeof(doom_maplinedef_t),
  .mt_push = MT_PUSH,
  .mt_pull = MT_PULL,
  .dn_polyanchor = -1,
  .dn_polyspawn_start = -1,
  .dn_polyspawn_hurt = -1,
  .dn_polyspawn_end = -1,
  .visibility = VF_DOOM,
};

static void dsda_ApplyMapPrecision(void) {
  R_PointOnSide = map_format.point_on_side;
  R_PointOnSegSide = map_format.point_on_seg_side;
  P_PointOnLineSide = map_format.point_on_line_side;
  P_PointOnDivlineSide = map_format.point_on_divline_side;
}

void dsda_ApplyDefaultMapFormat(void) {
    map_format = doom_map_format;

  dsda_ApplyMapPrecision();
}

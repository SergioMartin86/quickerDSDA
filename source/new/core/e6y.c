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
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <winreg.h>
#endif

#include "doomtype.h"
#include "doomstat.h"
#include "d_main.h"
#include "i_system.h"
#include "i_main.h"
#include "i_sound.h"
#include "lprintf.h"
#include "m_file.h"
#include "i_system.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_setup.h"
#include "i_video.h"
#include "info.h"
#include "r_main.h"
#include "r_things.h"
#include "r_sky.h"
#include "dsda.h"
#include "dsda/settings.h"
#include "g_game.h"
#include "e6y.h"
#include "m_file.h"

#include "dsda/args.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/stretch.h"
#include <math.h>

dboolean wasWiped = false;

int secretfound;
int demo_playerscount;
int demo_tics_count;
char demo_len_st[80];

int mouse_handler;
int gl_render_fov = 90;

camera_t walkcamera;

angle_t viewpitch;
float skyscale;
float screen_skybox_zplane;
float tan_pitch;
float skyUpAngle;
float skyUpShift;
float skyXShift;
float skyYShift;

#ifdef _WIN32
const char* WINError(void)
{
  static char *WinEBuff = NULL;
  DWORD err = GetLastError();
  char *ch;

  if (WinEBuff)
  {
    LocalFree(WinEBuff);
  }

  if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
    NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR) &WinEBuff, 0, NULL) == 0)
  {
    return "Unknown error";
  }

  if ((ch = strchr(WinEBuff, '\n')) != 0)
    *ch = 0;
  if ((ch = strchr(WinEBuff, '\r')) != 0)
    *ch = 0;

  return WinEBuff;
}
#endif

//--------------------------------------------------

/* ParamsMatchingCheck
 * Conflicting command-line parameters could cause the engine to be confused
 * in some cases. Added checks to prevent this.
 * Example: dsda-doom.exe -record mydemo -playdemo demoname
 */
void ParamsMatchingCheck()
{
  dboolean recording_attempt =
    dsda_Flag(dsda_arg_record) ||
    dsda_Flag(dsda_arg_recordfromto);

  dboolean playbacking_attempt =
    dsda_Flag(dsda_arg_playdemo) ||
    dsda_Flag(dsda_arg_timedemo) ||
    dsda_Flag(dsda_arg_fastdemo);

  if (recording_attempt && playbacking_attempt)
    I_Error("Params are not matching: Can not being played back and recorded at the same time.");
}

prboom_comp_t prboom_comp[PC_MAX] = {
  {0xffffffff, 0x02020615, 0, dsda_arg_force_monster_avoid_hazards},
  {0x00000000, 0x02040601, 0, dsda_arg_force_remove_slime_trails},
  {0x02020200, 0x02040801, 0, dsda_arg_force_no_dropoff},
  {0x00000000, 0x02040801, 0, dsda_arg_force_truncated_sector_specials},
  {0x00000000, 0x02040802, 0, dsda_arg_force_boom_brainawake},
  {0x00000000, 0x02040802, 0, dsda_arg_force_prboom_friction},
  {0x02020500, 0x02040000, 0, dsda_arg_reject_pad_with_ff},
  {0xffffffff, 0x02040802, 0, dsda_arg_force_lxdoom_demo_compatibility},
  {0x00000000, 0x0202061b, 0, dsda_arg_allow_ssg_direct},
  {0x00000000, 0x02040601, 0, dsda_arg_treat_no_clipping_things_as_not_blocking},
  {0x00000000, 0x02040803, 0, dsda_arg_force_incorrect_processing_of_respawn_frame_entry},
  {0x00000000, 0x02040601, 0, dsda_arg_force_correct_code_for_3_keys_doors_in_mbf},
  {0x00000000, 0x02040601, 0, dsda_arg_uninitialize_crush_field_for_stairs},
  {0x00000000, 0x02040802, 0, dsda_arg_force_boom_findnexthighestfloor},
  {0x00000000, 0x02040802, 0, dsda_arg_allow_sky_transfer_in_boom},
  {0x00000000, 0x02040803, 0, dsda_arg_apply_green_armor_class_to_armor_bonuses},
  {0x00000000, 0x02040803, 0, dsda_arg_apply_blue_armor_class_to_megasphere},
  {0x02020200, 0x02050003, 0, dsda_arg_force_incorrect_bobbing_in_boom},
  {0xffffffff, 0x00000000, 0, dsda_arg_boom_deh_parser},
  {0x00000000, 0x02050007, 0, dsda_arg_mbf_remove_thinker_in_killmobj},
  {0x00000000, 0x02050007, 0, dsda_arg_do_not_inherit_friendlyness_flag_on_spawn},
  {0x00000000, 0x02050007, 0, dsda_arg_do_not_use_misc12_frame_parameters_in_a_mushroom},
  {0x00000000, 0x02050102, 0, dsda_arg_apply_mbf_codepointers_to_any_complevel},
  {0x00000000, 0x02050104, 0, dsda_arg_reset_monsterspawner_params_after_loading},
};

void M_ChangeShorttics(void)
{
  shorttics = dsda_Flag(dsda_arg_shorttics);
}

void e6y_InitCommandLine(void)
{
  stats_level = dsda_Flag(dsda_arg_levelstat);

  if ((stroller = dsda_Flag(dsda_arg_stroller)))
    dsda_UpdateIntArg(dsda_arg_turbo, "50");

  dsda_ReadCommandLine();

  M_ChangeShorttics();
}

int G_ReloadLevel(void)
{
  int result = false;

  if ((gamestate == GS_LEVEL || gamestate == GS_INTERMISSION))
  {
    G_DeferedInitNew(gameskill, gameepisode, gamemap);
    result = true;
  }

  return result;
}

int G_GotoNextLevel(void)
{
  int epsd, map;
  int changed = false;

  dsda_NextMap(&epsd, &map);

  if ((gamestate == GS_LEVEL) )
  {
    G_DeferedInitNew(gameskill, epsd, map);
    changed = true;
  }

  return changed;
}

int G_GotoPrevLevel(void)
{
  int epsd, map;
  int changed = false;

  dsda_PrevMap(&epsd, &map);

  if ((gamestate == GS_LEVEL)  )
  {
    G_DeferedInitNew(gameskill, epsd, map);
    changed = true;
  }

  return changed;
}

void M_ChangeSpeed(void)
{
  G_SetSpeed(true);
}

void M_ChangeSkyMode(void)
{
  #ifdef __ENABLE_OPENGL_
  int gl_skymode;

  viewpitch = 0;

  R_InitSkyMap();

  gl_skymode = dsda_IntConfig(dsda_config_gl_skymode);

  if (gl_skymode == skytype_auto)
    gl_drawskys = (dsda_MouseLook() ? skytype_skydome : skytype_standard);
  else
    gl_drawskys = gl_skymode;
  #endif
}

static const int upViewPitchLimit = -ANG90 + (1 << ANGLETOFINESHIFT);
static const int downViewPitchLimit = ANG90 - (1 << ANGLETOFINESHIFT);

void M_ChangeScreenMultipleFactor(void)
{
  V_ChangeScreenResolution();
}

dboolean HaveMouseLook(void)
{
  return (viewpitch != 0);
}

void CheckPitch(signed int *pitch)
{
  if (*pitch < upViewPitchLimit)
    *pitch = upViewPitchLimit;

  if (*pitch > downViewPitchLimit)
    *pitch = downViewPitchLimit;

  (*pitch) >>= 16;
  (*pitch) <<= 16;
}

float gl_render_ratio;
float gl_render_fovratio;
float gl_render_fovy = FOV90;
float gl_render_multiplier;

void M_ChangeAspectRatio(void)
{
  M_ChangeFOV();

  R_SetViewSize();
}

void M_ChangeStretch(void)
{
  R_SetViewSize();
}

void M_ChangeFOV(void)
{
  float f1, f2;
  dsda_arg_t* arg;
  int gl_render_aspect_width, gl_render_aspect_height;

  arg = dsda_Arg(dsda_arg_aspect);
  if (
    arg->found &&
    sscanf(arg->value.v_string, "%dx%d", &gl_render_aspect_width, &gl_render_aspect_height) == 2
  )
  {
    SetRatio(SCREENWIDTH, SCREENHEIGHT);
    gl_render_fovratio = (float)gl_render_aspect_width / (float)gl_render_aspect_height;
    gl_render_ratio = RMUL * gl_render_fovratio;
    gl_render_multiplier = 64.0f / gl_render_fovratio / RMUL;
  }
  else
  {
    SetRatio(SCREENWIDTH, SCREENHEIGHT);
    gl_render_ratio = gl_ratio;
    gl_render_multiplier = (float)ratio_multiplier;
    if (!tallscreen)
    {
      gl_render_fovratio = 1.6f;
    }
    else
    {
      gl_render_fovratio = gl_render_ratio;
    }
  }

  gl_render_fovy = (float)(2 * RAD2DEG(atan(tan(DEG2RAD(gl_render_fov) / 2) / gl_render_fovratio)));

  screen_skybox_zplane = 320.0f/2.0f/(float)tan(DEG2RAD(gl_render_fov/2));

  f1 = (float)(320.0f / 200.0f * (float)gl_render_fov / (float)FOV90 - 0.2f);
  f2 = (float)tan(DEG2RAD(gl_render_fovy)/2.0f);
  if (f1-f2<1)
    skyUpAngle = (float)-RAD2DEG(asin(f1-f2));
  else
    skyUpAngle = -90.0f;

  skyUpShift = (float)tan(DEG2RAD(gl_render_fovy)/2.0f);

  skyscale = 1.0f / (float)tan(DEG2RAD(gl_render_fov / 2));
}

float viewPitch;

int StepwiseSum(int value, int direction, int minval, int maxval, int defval)
{
  int newvalue;
  int val = (direction > 0 ? value : value - 1);

  if (direction == 0)
    return defval;

  direction = (direction > 0 ? 1 : -1);

  {
    int exp = 1;
    while (exp * 10 <= val)
      exp *= 10;
    newvalue = direction * (val < exp * 5 && exp > 1 ? exp / 2 : exp);
    newvalue = (value + newvalue) / newvalue * newvalue;
  }

  if (newvalue > maxval) newvalue = maxval;
  if (newvalue < minval) newvalue = minval;

  if ((value < defval && newvalue > defval) || (value > defval && newvalue < defval))
    newvalue = defval;

  return newvalue;
}

void I_vWarning(const char *message, va_list argList)
{
  char msg[1024];
  vsnprintf(msg,sizeof(msg),message,argList);
  lprintf(LO_ERROR, "%s\n", msg);
#ifdef _WIN32
  I_MessageBox(msg, PRB_MB_OK);
#endif
}

int I_MessageBox(const char* text, unsigned int type)
{
#ifdef _WIN32
  int result = PRB_IDCANCEL;

  if (!dsda_Flag(dsda_arg_no_message_box))
  {
    HWND current_hwnd = GetForegroundWindow();
    wchar_t *wtext = ConvertUtf8ToWide(text);
    wchar_t *wpackage = ConvertUtf8ToWide(PACKAGE_NAME);
    result = MessageBoxW(GetDesktopWindow(), wtext, wpackage, type|MB_TASKMODAL|MB_TOPMOST);
    Z_Free(wtext);
    Z_Free(wpackage);
    I_SwitchToWindow(current_hwnd);
    return result;
  }
#endif

  return PRB_IDCANCEL;
}

int stats_level;
int stroller;
int numlevels = 0;
int levels_max = 0;
timetable_t *stats = NULL;

void e6y_G_DoCompleted(void)
{
}

typedef struct tmpdata_s
{
  char kill[200];
  char item[200];
  char secret[200];
} tmpdata_t;

void e6y_WriteStats(void)
{
}

//--------------------------------------------------

static double mouse_accelfactor;
static double analog_accelfactor;

void AccelChanging(void)
{
}

int AccelerateMouse(int val)
{
  return 0;
}

int AccelerateAnalog(float val)
{
  return 0;
}

int mlooky = 0;

void e6y_G_Compatibility(void)
{

  P_CrossSubsector = P_CrossSubsector_PrBoom;
  if (!prboom_comp[PC_FORCE_LXDOOM_DEMO_COMPATIBILITY].state)
  {
    if (demo_compatibility)
      P_CrossSubsector = P_CrossSubsector_Doom;

    switch (compatibility_level)
    {
    case boom_compatibility_compatibility:
    case boom_201_compatibility:
    case boom_202_compatibility:
    case mbf_compatibility:
    case mbf21_compatibility:
      P_CrossSubsector = P_CrossSubsector_Boom;
    break;
    }
  }
}

const char* PathFindFileName(const char* pPath)
{
  const char* pT = pPath;

  if (pPath)
  {
    for ( ; *pPath; pPath++)
    {
      if ((pPath[0] == '\\' || pPath[0] == ':' || pPath[0] == '/')
        && pPath[1] &&  pPath[1] != '\\'  &&   pPath[1] != '/')
        pT = pPath + 1;
    }
  }

  return pT;
}

int levelstarttic;

int force_singletics_to = 0;

int HU_DrawDemoProgress(int force)
{

  return true;
}

#ifdef _WIN32
int GetFullPath(const char* FileName, const char* ext, char *Buffer, size_t BufferLength)
{
  int i, Result;
  char *p;
  char dir[PATH_MAX];

  for (i=0; i<3; i++)
  {
    switch(i)
    {
    case 0:
      M_getcwd(dir, sizeof(dir));
      break;
    case 1:
      if (!M_getenv("DOOMWADDIR"))
        continue;
      strcpy(dir, M_getenv("DOOMWADDIR"));
      break;
    case 2:
      strcpy(dir, I_ConfigDir());
      break;
    }

    Result = SearchPath(dir,FileName,ext,BufferLength,Buffer,&p);
    if (Result)
      return Result;
  }

  return false;
}
#endif

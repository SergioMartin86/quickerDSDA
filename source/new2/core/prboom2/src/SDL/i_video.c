/* Emacs style mode select   -*- C -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2006 by
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
 *  DOOM graphics stuff for SDL
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#endif // _WIN32

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "doomstat.h"
#include "doomdef.h"
#include "doomtype.h"
#include "v_video.h"
#include "r_draw.h"
#include "r_things.h"
#include "r_plane.h"
#include "r_main.h"
#include "f_wipe.h"
#include "d_main.h"
#include "d_event.h"
#include "d_deh.h"
#include "i_video.h"
#include "i_capture.h"
#include "z_zone.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "st_stuff.h"
#include "am_map.h"
#include "g_game.h"
#include "lprintf.h"
#include "i_system.h"
#include "gl_struct.h"

#include "e6y.h"//e6y
#include "i_main.h"

#include "dsda/args.h"
#include "dsda/configuration.h"
#include "dsda/game_controller.h"
#include "dsda/palette.h"
#include "dsda/pause.h"
#include "dsda/settings.h"
#include "dsda/time.h"
#include "dsda/gl/render_scale.h"

dboolean window_focused;

// Window resize state.
static void ActivateMouse(void);
static void DeactivateMouse(void);
//static int AccelerateMouse(int val);
static void I_ReadMouse(void);
static dboolean MouseShouldBeGrabbed();
static void UpdateFocus(void);

extern const int gl_colorbuffer_bits;
extern const int gl_depthbuffer_bits;

extern void M_QuitDOOM(int choice);
int desired_fullscreen;
int exclusive_fullscreen;
unsigned int windowid = 0;


dboolean I_WindowFocused(void)
{
  return window_focused;
}

/////////////////////////////////////////////////////////////////////////////////
// Main input code

/* cph - pulled out common button code logic */
//e6y static
int I_SDLtoDoomMouseState(uint32_t buttonstate)
{
  return 0     ;
}

static void I_GetEvent(void)
{
}

//
// I_StartTic
//

void I_StartTic (void)
{
  I_GetEvent();

  if (dsda_AllowMouse())
    I_ReadMouse();

  if (dsda_AllowGameController())
    dsda_PollGameController();
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
}

static void I_FlushMousePosition(void)
{
}

void I_InitMouse(void)
{
}

//
// I_InitInputs
//

static void I_InitInputs(void)
{
  AccelChanging();
  I_InitMouse();
  dsda_InitGameController();
}

///////////////////////////////////////////////////////////
// Palette stuff.
//
static void I_UploadNewPalette(int pal, int force)
{
  // This is used to replace the current 256 colour cmap with a new one
  // Used by 256 colour PseudoColor modes

  static int cachedgamma;
  static size_t num_pals;
  dsda_playpal_t* playpal_data;

  if (V_IsOpenGLMode())
    return;

  playpal_data = dsda_PlayPalData();

  if ((playpal_data->colours == NULL) || (cachedgamma != usegamma) || force) {
    int pplump;
    int gtlump;
    register const byte * palette;
    register const byte * gtable;
    register int i;

    pplump = W_GetNumForName(playpal_data->lump_name);
    gtlump = W_CheckNumForName2("GAMMATBL", ns_prboom);
    palette = (const byte*) W_LumpByNum(pplump);
    gtable = (const byte*) W_LumpByNum(gtlump) + 256 * (cachedgamma = usegamma);

    num_pals = W_LumpLength(pplump) / (3 * 256);
    num_pals *= 256;

    if (!playpal_data->colours) {
      // First call - allocate and prepare colour array
      playpal_data->colours =
        (SDL_Color*) Z_Malloc(sizeof(*playpal_data->colours) * num_pals);
    }

    // set the colormap entries
    for (i = 0; (size_t) i < num_pals; i++) {
      playpal_data->colours[i].r = gtable[palette[0]];
      playpal_data->colours[i].g = gtable[palette[1]];
      playpal_data->colours[i].b = gtable[palette[2]];
      palette += 3;
    }

    num_pals /= 256;
  }

}

//////////////////////////////////////////////////////////////////////////////
// Graphics API

void I_ShutdownGraphics(void)
{
}

static dboolean queue_frame_capture;
static dboolean queue_screenshot;

void I_QueueFrameCapture(void)
{
  queue_frame_capture = true;
}

void I_QueueScreenshot(void)
{
  queue_screenshot = true;
}

void I_HandleCapture(void)
{
  if (queue_frame_capture)
  {
    I_CaptureFrame();
    queue_frame_capture = false;
  }

  if (queue_screenshot)
  {
    M_ScreenShot();
    queue_screenshot = false;
  }
}

//
// I_FinishUpdate
//
static int newpal = 0;
#define NO_PALETTE_CHANGE 1000

void I_FinishUpdate (void)
{
  /* Update the display buffer (flipping video pages if supported)
   * If we need to change palette, that implicitely does a flip */
  if (newpal != NO_PALETTE_CHANGE) {
    I_UploadNewPalette(newpal, false);
    newpal = NO_PALETTE_CHANGE;
  }
}

//
// I_ScreenShot - moved to i_sshot.c
//

//
// I_SetPalette
//
void I_SetPalette (int pal)
{
  newpal = pal;
}

// I_PreInitGraphics

static void I_ShutdownSDL(void)
{
  return;
}

void I_PreInitGraphics(void)
{
}

// e6y: resolution limitation is removed
void I_InitBuffersRes(void)
{
  R_InitMeltRes();
  R_InitSpritesRes();
  R_InitBuffersRes();
  R_InitPlanesRes();
  R_InitVisplanesRes();
}

#define MAX_RESOLUTIONS_COUNT 128
const char *screen_resolutions_list[MAX_RESOLUTIONS_COUNT] = {NULL};

//
// I_GetScreenResolution
// Get current resolution from the config variable (WIDTHxHEIGHT format)
// 640x480 if screen_resolution variable has wrong data
//
void I_GetScreenResolution(void)
{
  int width, height;
  const char *screen_resolution;

  desired_screenwidth = 640;
  desired_screenheight = 480;

  screen_resolution = dsda_StringConfig(dsda_config_screen_resolution);

  if (screen_resolution)
  {
    if (sscanf(screen_resolution, "%dx%d", &width, &height) == 2)
    {
      desired_screenwidth = width;
      desired_screenheight = height;
    }
  }
}

// make sure the canonical resolutions are always available
static const struct {
  const int w, h;
} canonicals[] = {
  { 640, 480}, // Doom 95
  { 320, 240}, // Doom 95
  {1120, 400}, // 21:9
  { 854, 400}, // 16:9
  { 768, 400}, // 16:10
  { 640, 400}, // MBF
  { 560, 200}, // 21:9
  { 426, 200}, // 16:9
  { 384, 200}, // 16:10
  { 320, 200}, // Vanilla Doom
};
static const int num_canonicals = sizeof(canonicals)/sizeof(*canonicals);

// [FG] sort resolutions by width first and height second
static int cmp_resolutions (const void *a, const void *b)
{
    const char *const *sa = (const char *const *) a;
    const char *const *sb = (const char *const *) b;

    int wa, wb, ha, hb;

    if (sscanf(*sa, "%dx%d", &wa, &ha) != 2) wa = ha = 0;
    if (sscanf(*sb, "%dx%d", &wb, &hb) != 2) wb = hb = 0;

    return (wa == wb) ? ha - hb : wa - wb;
}

//
// I_FillScreenResolutionsList
// Get all the supported screen resolutions
// and fill the list with them
//
static void I_FillScreenResolutionsList(void)
{
}

// e6y
// Function for trying to set the closest supported resolution if the requested mode can't be set correctly.
// For example dsda-doom.exe -geom 1025x768 -nowindow will set 1024x768.
// It should be used only for fullscreen modes.
static void I_ClosestResolution (int *width, int *height)
{
}

// e6y
// It is a simple test of CPU cache misses.
unsigned int I_TestCPUCacheMisses(int width, int height, unsigned int mintime)
{
  return 0;
}

// CPhipps -
// I_CalculateRes
// Calculates the screen resolution, possibly using the supplied guide
void I_CalculateRes(int width, int height)
{
  if (desired_fullscreen && exclusive_fullscreen)
  {
    I_ClosestResolution(&width, &height);
  }

#ifdef __ENABLE_OPENGL_
  if (V_IsOpenGLMode()) {
    SCREENWIDTH = width;
    SCREENHEIGHT = height;
    SCREENPITCH = SCREENWIDTH;
  } else
  #endif
   {
    unsigned int count1, count2;
    int pitch1, pitch2;

    SCREENWIDTH = width;//(width+15) & ~15;
    SCREENHEIGHT = height;

    // e6y
    // Trying to optimise screen pitch for reducing of CPU cache misses.
    // It is extremally important for wiping in software.
    // I have ~20x improvement in speed with using 1056 instead of 1024 on Pentium4
    // and only ~10% for Core2Duo
    if (nodrawers)
    {
      SCREENPITCH = ((width + 15) & ~15) + 32;
    }
    else
    {
      unsigned int mintime = 100;
      int w = (width+15) & ~15;
      pitch1 = w;
      pitch2 = w + 32;

      count1 = I_TestCPUCacheMisses(pitch1, SCREENHEIGHT, mintime);
      count2 = I_TestCPUCacheMisses(pitch2, SCREENHEIGHT, mintime);

      lprintf(LO_DEBUG, "I_CalculateRes: trying to optimize screen pitch\n");
      lprintf(LO_DEBUG, " test case for pitch=%d is processed %d times for %d msec\n", pitch1, count1, mintime);
      lprintf(LO_DEBUG, " test case for pitch=%d is processed %d times for %d msec\n", pitch2, count2, mintime);

      SCREENPITCH = (count2 > count1 ? pitch2 : pitch1);

      lprintf(LO_DEBUG, " optimized screen pitch is %d\n", SCREENPITCH);
    }
  }
}

static video_mode_t I_GetModeFromString(const char *modestr)
{
  video_mode_t mode;

  if (!stricmp(modestr,"gl")) {
    mode = VID_MODEGL;
  } else if (!stricmp(modestr,"OpenGL")) {
    mode = VID_MODEGL;
  } else {
    mode = VID_MODESW;
  }

  return mode;
}

static video_mode_t I_DesiredVideoMode(void) {
  dsda_arg_t *arg;
  video_mode_t mode;

  arg = dsda_Arg(dsda_arg_vidmode);
  if (arg->found)
    mode = I_GetModeFromString(arg->value.v_string);
  else
    mode = I_GetModeFromString(dsda_StringConfig(dsda_config_videomode));

  return mode;
}

// CPhipps -
// I_InitScreenResolution
// Sets the screen resolution
void I_InitScreenResolution(void)
{
  int i, w, h;
  char c, x;
  dsda_arg_t *arg;
  video_mode_t mode;

  I_GetScreenResolution();

  desired_fullscreen = dsda_IntConfig(dsda_config_use_fullscreen);

  if (dsda_Flag(dsda_arg_fullscreen))
    desired_fullscreen = 1;

  if (dsda_Flag(dsda_arg_window))
  desired_fullscreen = 0;
  w = desired_screenwidth;
  h = desired_screenheight;

  mode = I_DesiredVideoMode();

  V_InitMode(mode);

  I_CalculateRes(w, h);
  V_FreeScreens();

  // set first three to standard values
  for (i=0; i<3; i++) {
    screens[i].width = SCREENWIDTH;
    screens[i].height = SCREENHEIGHT;
    screens[i].pitch = SCREENPITCH;
  }

  // statusbar
  screens[4].width = SCREENWIDTH;
  screens[4].height = SCREENHEIGHT;
  screens[4].pitch = SCREENPITCH;

  I_InitBuffersRes();

  lprintf(LO_DEBUG, "I_InitScreenResolution: Using resolution %dx%d\n", SCREENWIDTH, SCREENHEIGHT);
}

//
// Set the window caption
//

void I_SetWindowCaption(void)
{
}

//
// Set the application icon
//

#include "icon.c"

void I_SetWindowIcon(void)
{
}

void I_InitGraphics(void)
{
  static int    firsttime=1;

  if (firsttime)
  {
    firsttime = 0;

    I_AtExit(I_ShutdownGraphics, true, "I_ShutdownGraphics", exit_priority_normal);
    lprintf(LO_DEBUG, "I_InitGraphics: %dx%d\n", SCREENWIDTH, SCREENHEIGHT);

    /* Set the video mode */
    I_UpdateVideoMode();

    //e6y: setup the window title
    I_SetWindowCaption();

    //e6y: set the application icon
    I_SetWindowIcon();

    /* Initialize the input system */
    I_InitInputs();

    //e6y: new mouse code
    UpdateFocus();
    UpdateGrab();
  }
}

void I_UpdateVideoMode(void)
{
  int screen_multiply;
  int actualheight;
  int render_vsync;
  // int integer_scaling;
  // const char *sdl_video_window_pos;
  const dboolean novsync = dsda_Flag(dsda_arg_timedemo) ||
                           dsda_Flag(dsda_arg_fastdemo);

  exclusive_fullscreen = dsda_IntConfig(dsda_config_exclusive_fullscreen) &&
                         I_DesiredVideoMode() == VID_MODESW;
  render_vsync = dsda_IntConfig(dsda_config_render_vsync) && !novsync;
  // sdl_video_window_pos = dsda_StringConfig(dsda_config_sdl_video_window_pos);
  screen_multiply = dsda_IntConfig(dsda_config_render_screen_multiply);
  // integer_scaling = dsda_IntConfig(dsda_config_integer_scaling);

  
  // [FG] aspect ratio correction for the canonical video modes
  if (SCREENHEIGHT == 200 || SCREENHEIGHT == 400)
  {
    actualheight = 6*SCREENHEIGHT/5;
  }
  else
  {
    actualheight = SCREENHEIGHT;
  }

  if (V_IsSoftwareMode())
  {
    // Get the info needed to render to the display
    screens[0].not_on_heap = true;
    screens[0].data = (unsigned char *) malloc(SCREENWIDTH*SCREENHEIGHT);
    screens[0].pitch = SCREENWIDTH;

    V_AllocScreens();

    R_InitBuffer(SCREENWIDTH, SCREENHEIGHT);
  }

  // e6y: wide-res
  // Need some initialisations before level precache
  R_ExecuteSetViewSize();

  V_SetPalette(0);
  I_UploadNewPalette(0, true);

  ST_SetResolution();
  AM_SetResolution();
}

static void ActivateMouse(void)
{
}

static void DeactivateMouse(void)
{
}

// Interpolates mouse input to mitigate stuttering
static void CorrectMouseStutter(int *x, int *y)
{
  static int x_remainder_old, y_remainder_old;
  int x_remainder, y_remainder;
  fixed_t fractic, correction_factor;

  if (!dsda_IntConfig(dsda_config_mouse_stutter_correction))
  {
    return;
  }

  fractic = dsda_TickElapsedTime();

  *x += x_remainder_old;
  *y += y_remainder_old;

  correction_factor = FixedDiv(fractic, fractic + 1000000 / TICRATE);

  x_remainder = FixedMul(*x, correction_factor);
  *x -= x_remainder;
  x_remainder_old = x_remainder;

  y_remainder = FixedMul(*y, correction_factor);
  *y -= y_remainder;
  y_remainder_old = y_remainder;
}

//
// Read the change in mouse state to generate mouse motion events
//
// This is to combine all mouse movement for a tic into one mouse
// motion event.
static void I_ReadMouse(void)
{
}

static dboolean MouseShouldBeGrabbed()
{
  return 0;
}

// Update the value of window_focused when we get a focus event
//
// We try to make ourselves be well-behaved: the grab on the mouse
// is removed if we lose focus (such as a popup window appearing),
// and we dont move the mouse around if we aren't focused either.
static void UpdateFocus(void)
{
}

void UpdateGrab(void)
{
}


/////////// Headless function

void* headlessGetVideoBuffer() { return screens[0].data; }
int headlessGetVideoPitch() { return screens[0].pitch; }
int headlessGetVideoWidth() { return screens[0].width; }
int headlessGetVideoHeight() { return screens[0].height; }
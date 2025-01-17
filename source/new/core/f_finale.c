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
 *      Game completion, final screen animation.
 *
 *-----------------------------------------------------------------------------
 */

#include "doomstat.h"
#include "d_event.h"
#include "g_game.h"
#include "lprintf.h"
#include "v_video.h"
#include "w_wad.h"
#include "sounds.h"

#include "heretic/f_finale.h"

#include "dsda/font.h"
#include "dsda/mapinfo.h"

#include "f_finale.h" // CPhipps - hmm...

// defines for the end mission display text                     // phares

#define TEXTSPEED    3     // original value                    // phares
#define TEXTWAIT     250   // original value                    // phares
#define NEWTEXTSPEED 0.01f // new value                         // phares
#define NEWTEXTWAIT  1000  // new value                         // phares

// Stage of animation:
//  0 = text, 1 = art screen, 2 = character cast
int finalestage;
int finalecount;
const char*   finaletext;
const char*   finaleflat;
const char*   finalepatch;

// defines for the end mission display text                     // phares

// CPhipps - removed the old finale screen text message strings;
// they were commented out for ages already
// Ty 03/22/98 - ... the new s_WHATEVER extern variables are used
// in the code below instead.

void    F_CastTicker (void);
dboolean F_CastResponder (event_t *ev);
void    F_CastDrawer (void);

void WI_checkForAccelerate(void);    // killough 3/28/98: used to
extern int acceleratestage;          // accelerate intermission screens
int midstage;                 // whether we're in "mid-stage"

//
// F_StartFinale
//
void F_StartFinale (void)
{
  int mnum;
  int muslump;

  gameaction = ga_nothing;
  gamestate = GS_FINALE;

  // killough 3/28/98: clear accelerative text flags
  acceleratestage = midstage = 0;

  finaletext = NULL;
  finaleflat = NULL;
  finalepatch = NULL;

  dsda_InterMusic(&mnum, &muslump);

  dsda_StartFinale();

  finalestage = 0;
  finalecount = 0;
}



dboolean F_Responder (event_t *event)
{
  if (finalestage == 2)
    return F_CastResponder (event);

  return false;
}

// Get_TextSpeed() returns the value of the text display speed  // phares
// Rewritten to allow user-directed acceleration -- killough 3/28/98

float Get_TextSpeed(void)
{
  return midstage ? NEWTEXTSPEED : (midstage=acceleratestage) ?
    acceleratestage=0, NEWTEXTSPEED : TEXTSPEED;
}


//
// F_Ticker
//
// killough 3/28/98: almost totally rewritten, to use
// player-directed acceleration instead of constant delays.
// Now the player can accelerate the text display by using
// the fire/use keys while it is being printed. The delay
// automatically responds to the user, and gives enough
// time to read.
//
// killough 5/10/98: add back v1.9 demo compatibility
//

static dboolean F_ShowCast(void)
{
  return gamemap == 30 ||
         (gamemission == pack_nerve && allow_incompatibility && gamemap == 8) ||
         dsda_FinaleShortcut();
}

void F_Ticker(void)
{
  int i;

  if (dsda_FTicker())
  {
    return;
  }

  if (!demo_compatibility)
    WI_checkForAccelerate();  // killough 3/28/98: check for acceleration
  else
    if (gamemode == commercial && finalecount > 50) // check for skipping
      for (i = 0; i < g_maxplayers; i++)
        if (players[i].cmd.buttons)
          goto next_level;      // go on to the next level

  // advance animation
  finalecount++;

  if (finalestage == 2)
    F_CastTicker();

  if (!finalestage)
    {
      float speed = demo_compatibility ? TEXTSPEED : Get_TextSpeed();
      /* killough 2/28/98: changed to allow acceleration */
      if (finalecount > 200*speed +
          (midstage ? NEWTEXTWAIT : TEXTWAIT) ||
          (midstage && acceleratestage)) {
        if (gamemode != commercial)       // Doom 1 / Ultimate Doom episode end
          {                               // with enough time, it's automatic
            if (gameepisode == 3)
              F_StartScroll(NULL, NULL, NULL, true);
            else
              F_StartPostFinale();
          }
        else   // you must press a button to continue in Doom 2
          if (!demo_compatibility && midstage)
            {
            next_level:
              if (F_ShowCast())
                F_StartCast(NULL, NULL, true); // cast of Doom 2 characters
              else
                gameaction = ga_worlddone;  // next level, e.g. MAP07
            }
      }
    }
}

//
// F_TextWrite
//
// This program displays the background and text at end-mission     // phares
// text time. It draws both repeatedly so that other displays,      //   |
// like the main menu, can be drawn over it dynamically and         //   V
// erased dynamically. The TEXTSPEED constant is changed into
// the Get_TextSpeed function so that the speed of writing the      //   ^
// text can be increased, and there's still time to read what's     //   |
// written.                                                         // phares
// CPhipps - reformatted

#include "hu_stuff.h"

void F_TextWrite (void)
{
  if (finalepatch)
  {
    V_ClearBorder();
    V_DrawNamePatch(0, 0, 0, finalepatch, CR_DEFAULT, VPT_STRETCH);
  }
  else
    V_DrawBackground(finaleflat, 0);

  { // draw some of the text onto the screen
    int         cx = 10;
    int         cy = 10;
    const char* ch = finaletext; // CPhipps - const
    int         count = (int)((float)(finalecount - 10)/Get_TextSpeed()); // phares
    int         w;

    if (count < 0)
      count = 0;

    for ( ; count ; count-- ) {
      int       c = *ch++;

      if (!c)
        break;

      if (c == '\n') {
        cx = 10;
        cy += 11;
        continue;
      }

      c = toupper(c) - HU_FONTSTART;
      if (c < 0 || c> HU_FONTSIZE) {
        cx += 4;
        continue;
      }

      w = hud_font.font[c].width;
      if (cx+w > SCREENWIDTH)
        break;

      // CPhipps - patch drawing updated
      V_DrawNumPatch(cx, cy, 0, hud_font.font[c].lumpnum, CR_DEFAULT, VPT_STRETCH);
      cx+=w;
    }
  }
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//
typedef struct
{
  const char **name; // CPhipps - const**
  mobjtype_t   type;
} castinfo_t;

static int castnum;
static int casttics;
static state_t* caststate;
static dboolean castdeath;
static int castframes;
static int castonmelee;
static dboolean castattacking;
static const char *castbackground;

//
// F_StartCast
//

static void F_StartCastMusic(const char* music, dboolean loop_music)
{
}

void F_StartCast (const char* background, const char* music, dboolean loop_music)
{
}

//
// F_CastTicker
//
void F_CastTicker (void)
{
  int st;
  int sfx;

  if (--casttics > 0)
    return;                 // not time to change state yet
}


//
// F_CastResponder
//

dboolean F_CastResponder (event_t* ev)
{

  return true;
}


static void F_CastPrint (const char* text) // CPhipps - static, const char*
{
  const char* ch; // CPhipps - const
  int         c;
  int         cx;
  int         w;
  int         width;

  // find width
  ch = text;
  width = 0;

  while (ch)
  {
    c = *ch++;
    if (!c)
      break;
    c = toupper(c) - HU_FONTSTART;
    if (c < 0 || c> HU_FONTSIZE)
    {
      width += 4;
      continue;
    }

    w = hud_font.font[c].width;
    width += w;
  }

  // draw it
  cx = 160-width/2;
  ch = text;
  while (ch)
  {
    c = *ch++;
    if (!c)
      break;
    c = toupper(c) - HU_FONTSTART;
    if (c < 0 || c> HU_FONTSIZE)
    {
      cx += 4;
      continue;
    }

    w = hud_font.font[c].width;
    // CPhipps - patch drawing updated
    V_DrawNumPatch(cx, 180, 0, hud_font.font[c].lumpnum, CR_DEFAULT, VPT_STRETCH);
    cx+=w;
  }
}


//
// F_CastDrawer
//

void F_CastDrawer (void)
{
}

//
// F_BunnyScroll
//
static const char* pfub1 = "PFUB1";
static const char* pfub2 = "PFUB2";

static const char* scrollpic1;
static const char* scrollpic2;

static void F_StartScrollMusic(const char* music, dboolean loop_music)
{
}

static dboolean end_patches_exist;

void F_StartScroll (const char* right, const char* left, const char* music, dboolean loop_music)
{
  wipegamestate = -1; // force a wipe
  scrollpic1 = right ? right : pfub1;
  scrollpic2 = left ? left : pfub2;
  finalecount = 0;
  finalestage = 1;

  end_patches_exist = W_CheckNumForName("END0") != LUMP_NOT_FOUND &&
                      W_CheckNumForName("END1") != LUMP_NOT_FOUND &&
                      W_CheckNumForName("END2") != LUMP_NOT_FOUND &&
                      W_CheckNumForName("END3") != LUMP_NOT_FOUND &&
                      W_CheckNumForName("END4") != LUMP_NOT_FOUND &&
                      W_CheckNumForName("END5") != LUMP_NOT_FOUND &&
                      W_CheckNumForName("END6") != LUMP_NOT_FOUND;

  F_StartScrollMusic(music, loop_music);
}

void F_BunnyScroll (void)
{
  char        name[10];
  int         stage;
  static int  laststage;
  static int  p1offset, p2width;

  if (finalecount == 0)
  {
    const rpatch_t *p1, *p2;
    p1 = R_PatchByName(scrollpic1);
    p2 = R_PatchByName(scrollpic2);

    p2width = p2->width;
    if (p1->width == 320)
    {
      // Unity or original PFUBs.
      p1offset = (p2width - 320) / 2;
    }
    else
    {
      // Widescreen mod PFUBs.
      p1offset = 0;
    }
  }

  {
    int scrolled = 320 - (finalecount-230)/2;
    if (scrolled <= 0) {
      V_DrawNamePatch(0, 0, 0, scrollpic2, CR_DEFAULT, VPT_STRETCH);
    } else if (scrolled >= 320) {
      V_DrawNamePatch(p1offset, 0, 0, scrollpic1, CR_DEFAULT, VPT_STRETCH);
      if (p1offset > 0)
        V_DrawNamePatch(-320, 0, 0, scrollpic2, CR_DEFAULT, VPT_STRETCH);
    } else {
      V_DrawNamePatch(p1offset + 320 - scrolled, 0, 0, scrollpic1, CR_DEFAULT, VPT_STRETCH);
      V_DrawNamePatch(-scrolled, 0, 0, scrollpic2, CR_DEFAULT, VPT_STRETCH);
    }
    if (p2width == 320)
      V_ClearBorder();
  }

  if (!end_patches_exist)
    return;

  if (finalecount < 1130)
    return;
  if (finalecount < 1180)
  {
    // CPhipps - patch drawing updated
    V_DrawNamePatch((320-13*8)/2, (200-8*8)/2,0, "END0", CR_DEFAULT, VPT_STRETCH);
    laststage = 0;
    return;
  }

  stage = (finalecount-1180) / 5;
  if (stage > 6)
    stage = 6;
  if (stage > laststage)
  {
    laststage = stage;
  }

  // CPhipps - patch drawing updated
  V_DrawNamePatch((320-13*8)/2, (200-8*8)/2, 0, name, CR_DEFAULT, VPT_STRETCH);
}

void F_StartPostFinale (void)
{
  finalecount = 0;
  finalestage = 1;
  wipegamestate = -1; // force a wipe
}

//
// F_Drawer
//
void F_Drawer (void)
{
  if (dsda_FDrawer())
  {
    return;
  }

  if (finalestage == 2)
  {
    F_CastDrawer ();
    return;
  }

  if (!finalestage)
    F_TextWrite ();
  else
  {
    // e6y: wide-res
    V_ClearBorder();

    switch (gameepisode)
    {
      // CPhipps - patch drawing updated
      case 1:
           if ( (gamemode == retail && !pwad_help2_check) || gamemode == commercial )
             V_DrawNamePatch(0, 0, 0, "CREDIT", CR_DEFAULT, VPT_STRETCH);
           else
             V_DrawNamePatch(0, 0, 0, "HELP2", CR_DEFAULT, VPT_STRETCH);
           break;
      case 2:
           V_DrawNamePatch(0, 0, 0, "VICTORY2", CR_DEFAULT, VPT_STRETCH);
           break;
      case 3:
           F_BunnyScroll ();
           break;
      case 4:
           V_DrawNamePatch(0, 0, 0, "ENDPIC", CR_DEFAULT, VPT_STRETCH);
           break;
    }
  }
}

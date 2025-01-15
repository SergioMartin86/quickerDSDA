//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
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
/*
========================
=
= IN_lude.c
=
========================
*/

#include "am_map.h"
#include "doomstat.h"
#include "d_event.h"
#include "s_sound.h"
#include "sounds.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "lprintf.h"
#include "w_wad.h"
#include "g_game.h"

#include "dsda/exhud.h"

#include "heretic/def.h"
#include "heretic/dstrings.h"
#include "heretic/sb_bar.h"

#include "in_lude.h"


typedef enum
{
    SINGLE,
    COOPERATIVE,
    DEATHMATCH
} gametype_t;

static void IN_WaitStop(void);
static void IN_Stop(void);
static void IN_CheckForSkip(void);
static void IN_InitStats(void);
static void IN_DrawOldLevel(void);
static void IN_DrawYAH(void);
static void IN_DrawStatBack(void);
static void IN_DrawSingleStats(void);
static void IN_DrawCoopStats(void);
static void IN_DrawDMStats(void);
static void IN_DrawNumber(int val, int x, int y, int digits);
static void IN_DrawTime(int x, int y, int h, int m, int s);
static void IN_DrTextB(const char *text, int x, int y);

// contains information passed into intermission
static wbstartstruct_t* wbs;

static int prevmap;
static int nextmap;
static dboolean intermission;
static dboolean skipintermission;
static dboolean finalintermission;
static int interstate = 0;
static int intertime = -1;
static int oldintertime = 0;
static gametype_t gametype;

static int cnt;

static int hours;
static int minutes;
static int seconds;

// [crispy] Show total time on intermission
static int totalHours;
static int totalMinutes;
static int totalSeconds;

static int slaughterboy;        // in DM, the player with the most kills

static int killPercent[MAX_MAXPLAYERS];
static int bonusPercent[MAX_MAXPLAYERS];
static int secretPercent[MAX_MAXPLAYERS];

static int FontBNumbers[10];

static int FontBLump;
static int patchFaceOkayBase;
static int patchFaceDeadBase;

static signed int totalFrags[MAX_MAXPLAYERS];
static fixed_t dSlideX[MAX_MAXPLAYERS];
static fixed_t dSlideY[MAX_MAXPLAYERS];

static const char *KillersText[] = { "K", "I", "L", "L", "E", "R", "S" };

extern const char *LevelNames[];

typedef struct
{
    int x;
    int y;
} yahpt_t;

static yahpt_t YAHspot[3][9] = {
    {
     {172, 78},
     {86, 90},
     {73, 66},
     {159, 95},
     {148, 126},
     {132, 54},
     {131, 74},
     {208, 138},
     {52, 101}
     },
    {
     {218, 57},
     {137, 81},
     {155, 124},
     {171, 68},
     {250, 86},
     {136, 98},
     {203, 90},
     {220, 140},
     {279, 106}
     },
    {
     {86, 99},
     {124, 103},
     {154, 79},
     {202, 83},
     {178, 59},
     {142, 58},
     {219, 66},
     {247, 57},
     {107, 80}
     }
};

static const char *NameForMap(int map)
{
    const char *name = LevelNames[(gameepisode - 1) * 9 + map - 1];

    if (strlen(name) < 7)
    {
        return "";
    }
    return name + 7;
}

static void IN_DrawInterpic(void)
{
  char name[9];

  if (gameepisode < 1 || gameepisode > 3) return;

  snprintf(name, sizeof(name), "MAPE%d", gameepisode);

  // e6y: wide-res
  V_ClearBorder();
  V_DrawNamePatch(0, 0, 0, name, CR_DEFAULT, VPT_STRETCH);
}

static void IN_DrawBeenThere(int i)
{
  V_DrawNamePatch(
    YAHspot[gameepisode - 1][i].x, YAHspot[gameepisode - 1][i].y, 0,
    "IN_X", CR_DEFAULT, VPT_STRETCH
  );
}

static void IN_DrawGoingThere(int i)
{
  V_DrawNamePatch(
    YAHspot[gameepisode - 1][i].x, YAHspot[gameepisode - 1][i].y, 0,
    "IN_YAH", CR_DEFAULT, VPT_STRETCH
  );
}

static void IN_InitLumps(void)
{
  int i, base;

  base = W_GetNumForName("FONTB16");
  for (i = 0; i < 10; i++)
  {
      FontBNumbers[i] = base + i;
  }

  FontBLump = W_GetNumForName("FONTB_S") + 1;
  patchFaceOkayBase = W_GetNumForName("FACEA0");
  patchFaceDeadBase = W_GetNumForName("FACEB0");
}

static void IN_InitVariables(wbstartstruct_t* wbstartstruct)
{
  wbs = wbstartstruct;
  prevmap = wbs->last + 1;
  nextmap = wbs->next + 1;

  finalintermission = (prevmap == 8);
}

//========================================================================
//
// IN_Start
//
//========================================================================

void IN_Start(wbstartstruct_t* wbstartstruct)
{
    V_SetPalette(0);
    IN_InitVariables(wbstartstruct);
    IN_InitLumps();
    IN_InitStats();
    intermission = true;
    interstate = -1;
    skipintermission = false;
    intertime = 0;
    oldintertime = 0;
    AM_Stop(false);
    S_ChangeMusic(heretic_mus_intr, true);
}

//========================================================================
//
// IN_WaitStop
//
//========================================================================

void IN_WaitStop(void)
{
    if (!--cnt)
    {
        IN_Stop();
        G_WorldDone();
    }
}

//========================================================================
//
// IN_Stop
//
//========================================================================

void IN_Stop(void)
{
    intermission = false;
    SB_Start();
}

//========================================================================
//
// IN_InitStats
//
//      Initializes the stats for single player mode
//========================================================================

void IN_InitStats(void)
{
    int i;
    int j;
    signed int slaughterfrags;
    int posnum;
    int slaughtercount;
    int playercount;
    int count;

    if (!netgame)
    {
        gametype = SINGLE;
        count = leveltime / 35;
        hours = count / 3600;
        count -= hours * 3600;
        minutes = count / 60;
        count -= minutes * 60;
        seconds = count;

        // [crispy] Show total time on intermission
        count = wbs->totaltimes / 35;
        totalHours = count / 3600;
        count -= totalHours * 3600;
        totalMinutes = count / 60;
        count -= totalMinutes * 60;
        totalSeconds = count;
    }
    else if (netgame && !deathmatch)
    {
        gametype = COOPERATIVE;
        memset(killPercent, 0, MAX_MAXPLAYERS * sizeof(int));
        memset(bonusPercent, 0, MAX_MAXPLAYERS * sizeof(int));
        memset(secretPercent, 0, MAX_MAXPLAYERS * sizeof(int));
        for (i = 0; i < g_maxplayers; i++)
        {
            if (playeringame[i])
            {
                if (totalkills)
                {
                    killPercent[i] = players[i].killcount * 100 / totalkills;
                }
                if (totalitems)
                {
                    bonusPercent[i] = players[i].itemcount * 100 / totalitems;
                }
                if (totalsecret)
                {
                    secretPercent[i] =
                        players[i].secretcount * 100 / totalsecret;
                }
            }
        }
    }
    else
    {
        gametype = DEATHMATCH;
        slaughterboy = 0;
        slaughterfrags = -9999;
        posnum = 0;
        playercount = 0;
        slaughtercount = 0;
        for (i = 0; i < g_maxplayers; i++)
        {
            totalFrags[i] = 0;
            if (playeringame[i])
            {
                playercount++;
                for (j = 0; j < g_maxplayers; j++)
                {
                    if (playeringame[j])
                    {
                        totalFrags[i] += players[i].frags[j];
                    }
                }
                dSlideX[i] = (43 * posnum * FRACUNIT) / 20;
                dSlideY[i] = (36 * posnum * FRACUNIT) / 20;
                posnum++;
            }
            if (totalFrags[i] > slaughterfrags)
            {
                slaughterboy = 1 << i;
                slaughterfrags = totalFrags[i];
                slaughtercount = 1;
            }
            else if (totalFrags[i] == slaughterfrags)
            {
                slaughterboy |= 1 << i;
                slaughtercount++;
            }
        }
        if (playercount == slaughtercount)
        {                       // don't do the slaughter stuff if everyone is equal
            slaughterboy = 0;
        }
    }
}

//========================================================================
//
// IN_Ticker
//
//========================================================================

void IN_Ticker(void)
{
    if (!intermission)
    {
        return;
    }
    if (interstate == 3)
    {
        IN_WaitStop();
        return;
    }
    IN_CheckForSkip();
    intertime++;
    if (oldintertime < intertime)
    {
        interstate++;

        // [crispy] skip "now entering" if it's the final intermission
        if (interstate >= 1 && finalintermission)
        {
            IN_Stop();
            G_WorldDone();
            return;
        }

        if (gameepisode > 3 && interstate >= 1)
        {                       // Extended Wad levels:  skip directly to the next level
            interstate = 3;
        }
        switch (interstate)
        {
            case 0:
                oldintertime = intertime + 300;
                if (gameepisode > 3)
                {
                    oldintertime = intertime + 1200;
                }
                break;
            case 1:
                oldintertime = intertime + 200;
                break;
            case 2:
                oldintertime = INT_MAX;
                break;
            case 3:
                cnt = 10;
                break;
            default:
                break;
        }
    }
    if (skipintermission)
    {
        if (interstate == 0 && intertime < 150)
        {
            intertime = 150;
            skipintermission = false;
            return;
        }
        // [crispy] skip "now entering" if it's the final intermission
        else if (finalintermission)
        {
            IN_Stop();
            G_WorldDone();
            return;
        }
        else if (interstate < 2 && gameepisode < 4)
        {
            interstate = 2;
            skipintermission = false;
            S_StartVoidSound(heretic_sfx_dorcls);
            return;
        }
        interstate = 3;
        cnt = 10;
        skipintermission = false;
        S_StartVoidSound(heretic_sfx_dorcls);
    }
}

//========================================================================
//
// IN_CheckForSkip
//
//      Check to see if any player hit a key
//========================================================================

void IN_CheckForSkip(void)
{
    int i;
    player_t *player;

    for (i = 0, player = players; i < g_maxplayers; i++, player++)
    {
        if (playeringame[i])
        {
            if (player->cmd.buttons & BT_ATTACK)
            {
                if (!player->attackdown)
                {
                    skipintermission = 1;
                }
                player->attackdown = true;
            }
            else
            {
                player->attackdown = false;
            }
            if (player->cmd.buttons & BT_USE)
            {
                if (!player->usedown)
                {
                    skipintermission = 1;
                }
                player->usedown = true;
            }
            else
            {
                player->usedown = false;
            }
        }
    }
}

//========================================================================
//
// IN_Drawer
//
//========================================================================

void IN_Drawer(void)
{
    static int oldinterstate;

    if (!intermission)
    {
        return;
    }
    if (interstate == 3)
    {
        return;
    }

    if (oldinterstate != 2 && interstate == 2)
    {
        S_StartVoidSound(heretic_sfx_pstop);
    }
    oldinterstate = interstate;
    switch (interstate)
    {
        case -1:
        case 0:                // draw stats
            IN_DrawStatBack();
            switch (gametype)
            {
                case SINGLE:
                    IN_DrawSingleStats();
                    break;
                case COOPERATIVE:
                    IN_DrawCoopStats();
                    break;
                case DEATHMATCH:
                    IN_DrawDMStats();
                    break;
            }
            break;
        case 1:                // leaving old level
            if (gameepisode < 4)
            {
                IN_DrawInterpic();
                IN_DrawOldLevel();
            }
            break;
        case 2:                // going to the next level
            if (gameepisode < 4)
            {
                IN_DrawInterpic();
                IN_DrawYAH();
            }
            break;
        case 3:                // waiting before going to the next level
            if (gameepisode < 4)
            {
                IN_DrawInterpic();
            }
            break;
        default:
            I_Error("IN_lude:  Intermission state out of range.\n");
            break;
    }
}

//========================================================================
//
// IN_DrawStatBack
//
//========================================================================

void IN_DrawStatBack(void)
{
    // e6y: wide-res
    V_ClearBorder();
    V_DrawBackground("FLOOR16", 0);
}

//========================================================================
//
// IN_DrawOldLevel
//
//========================================================================

void IN_DrawOldLevel(void)
{
}

//========================================================================
//
// IN_DrawYAH
//
//========================================================================

void IN_DrawYAH(void)
{
}

//========================================================================
//
// IN_DrawSingleStats
//
//========================================================================

void IN_DrawSingleStats(void)
{
}

//========================================================================
//
// IN_DrawCoopStats
//
//========================================================================

void IN_DrawCoopStats(void)
{
}

//========================================================================
//
// IN_DrawDMStats
//
//========================================================================

void IN_DrawDMStats(void)
{
}

//========================================================================
//
// IN_DrawTime
//
//========================================================================

// [crispy] always draw seconds; don't 0-pad minutes with no hour
void IN_DrawTime(int x, int y, int h, int m, int s)
{
}

//========================================================================
//
// IN_DrawNumber
//
//========================================================================

void IN_DrawNumber(int val, int x, int y, int digits)
{
}

//========================================================================
//
// IN_DrTextB
//
//========================================================================

void IN_DrTextB(const char *text, int x, int y)
{
}

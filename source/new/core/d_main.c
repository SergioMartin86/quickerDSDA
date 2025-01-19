/* Emacs style mode select   -*- C -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2004 by
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
 *  DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
 *  plus functions to determine game mode (shareware, registered),
 *  parse command line parameters, configure game parameters (turbo),
 *  and call the startup functions.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _MSC_VER
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <sys/types.h>

#include "doomdef.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_net.h"
#include "sounds.h"
#include "z_zone.h"
#include "w_wad.h"
#include "v_video.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "m_file.h"
#include "m_misc.h"
#include "i_main.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "g_game.h"
#include "wi_stuff.h"
#include "p_setup.h"
#include "r_main.h"
#include "d_main.h"
#include "lprintf.h"  // jff 08/03/98 - declaration of lprintf
#include "e6y.h"

#include "dsda/args.h"
#include "dsda/configuration.h"
#include "dsda/features.h"
#include "dsda/global.h"
#include "dsda/save.h"
#include "dsda/data_organizer.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/mobjinfo.h"
#include "dsda/options.h"
#include "dsda/pause.h"
#include "dsda/preferences.h"
#include "dsda/settings.h"
#include "dsda/skill_info.h"
#include "dsda/utility.h"
#include "dsda/wad_stats.h"

// NSM
#include "i_capture.h"

#include "i_glob.h"

static void D_PageDrawer(void);

// jff 1/24/98 add new versions of these variables to remember command line
dboolean clnomonsters;   // checkparm of -nomonsters
dboolean clrespawnparm;  // checkparm of -respawn
dboolean clfastparm;     // checkparm of -fast
// jff 1/24/98 end definition of command line version of play mode switches

dboolean nomonsters;     // working -nomonsters
dboolean respawnparm;    // working -respawn
dboolean fastparm;       // working -fast

dboolean randomclass;

dboolean singletics = false; // debug flag to cancel adaptiveness

//jff 1/22/98 parms for disabling music and sound
dboolean nosfxparm;
dboolean nomusicparm;

//jff 4/18/98
extern dboolean inhelpscreens;
extern dboolean BorderNeedRefresh;

extern dboolean enableOutput;

int     startskill;
int     startepisode;
int     startmap;
dboolean autostart;
FILE    *debugfile;

dboolean advancedemo;

//jff 4/19/98 list of standard IWAD names
const char *const standard_iwads[]=
{
  "doom2f.wad",
  "doom2.wad",
  "plutonia.wad",
  "tnt.wad",

  "doom.wad",
  "doom1.wad",
  "doomu.wad", /* CPhipps - alow doomu.wad */

  "freedoom2.wad", /* wart@kobold.org:  added freedoom for Fedora Extras */
  "freedoom1.wad",
  "freedm.wad",

  "hacx.wad",
  "chex.wad",
  "rekkrsa.wad",

  "bfgdoom2.wad",
  "bfgdoom.wad",

  "heretic.wad",
  "hexen.wad",

  "heretic1.wad"
};
//e6y static
const int nstandard_iwads = sizeof standard_iwads/sizeof*standard_iwads;

/*
 * D_PostEvent - Event handling
 *
 * Called by I/O functions when an event is received.
 * Try event handlers for each code area in turn.
 * cph - in the true spirit of the Boom source, let the
 *  short ciruit operator madness begin!
 */

void D_PostEvent(event_t *ev)
{
  dsda_InputTrackEvent(ev);

    G_Responder(ev);
}

//
// D_Wipe
//
// CPhipps - moved the screen wipe code from D_Display to here
// The screens to wipe between are already stored, this just does the timing
// and screen updating

static void D_Wipe(void)
{
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t    wipegamestate = GS_DEMOSCREEN;
extern dboolean setsizeneeded;

static void D_DrawPause(void)
{
  if (dsda_PauseMode(PAUSE_BUILDMODE))
    return;
}

static dboolean must_fill_back_screen;

void D_MustFillBackScreen(void)
{
  must_fill_back_screen = true;
}

void D_Display (fixed_t frac)
{
}


//
//  DEMO LOOP
//

static int  demosequence;         // killough 5/2/98: made static
static int  pagetic;
static const char *pagename; // CPhipps - const
dboolean bfgedition = 0;

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker(void)
{
  if (--pagetic < 0)
    D_AdvanceDemo();
}

//
// D_PageDrawer
//
static void D_PageDrawer(void)
{
  // Allows use of PWAD HELP2 screen in demosequence
  if (demosequence == 4 && pwad_help2_check)
    pagename = "HELP2";

  // proff/nicolas 09/14/98 -- now stretchs bitmaps to fullscreen!
  // CPhipps - updated for new patch drawing
  // proff - added M_DrawCredits
  if (pagename)
  {
    // e6y: wide-res
    V_ClearBorder();
    V_DrawNamePatch(0, 0, 0, pagename, CR_DEFAULT, VPT_STRETCH);
  }
}

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo (void)
{
  advancedemo = true;
}

/* killough 11/98: functions to perform demo sequences
 * cphipps 10/99: constness fixes
 */

static void D_SetPageName(const char *name)
{
  if ((bfgedition) && name && !strncmp(name,"TITLEPIC",8))
    pagename = "DMENUPIC";
  else
    pagename = name;
}

void D_SetPage(const char* name, int tics, int music)
{
  if (tics)
    pagetic = tics;

  D_SetPageName(name);
}

static void D_DrawTitle1(const char *name)
{
  D_SetPage(name, TICRATE * 170 / 35, mus_intro);
}

static void D_DrawTitle2(const char *name)
{
  D_SetPage(name, 0, mus_dm2ttl);
}

/* killough 11/98: tabulate demo sequences
 */

extern const demostate_t (*demostates)[4];

const demostate_t doom_demostates[][4] =
{
  {
    {D_DrawTitle1, "TITLEPIC"},
    {D_DrawTitle1, "TITLEPIC"},
    {D_DrawTitle2, "TITLEPIC"},
    {D_DrawTitle1, "TITLEPIC"},
  },

  {
    {G_DeferedPlayDemo, "demo1"},
    {G_DeferedPlayDemo, "demo1"},
    {G_DeferedPlayDemo, "demo1"},
    {G_DeferedPlayDemo, "demo1"},
  },

  {
    {D_SetPageName, NULL},
    {D_SetPageName, NULL},
    {D_SetPageName, NULL},
    {D_SetPageName, NULL},
  },

  {
    {G_DeferedPlayDemo, "demo2"},
    {G_DeferedPlayDemo, "demo2"},
    {G_DeferedPlayDemo, "demo2"},
    {G_DeferedPlayDemo, "demo2"},
  },

  {
    {D_SetPageName, "HELP2"},
    {D_SetPageName, "HELP2"},
    {D_SetPageName, "CREDIT"},
    {D_DrawTitle1,  "TITLEPIC"},
  },

  {
    {G_DeferedPlayDemo, "demo3"},
    {G_DeferedPlayDemo, "demo3"},
    {G_DeferedPlayDemo, "demo3"},
    {G_DeferedPlayDemo, "demo3"},
  },

  {
    {NULL},
    {NULL},
    // e6y
    // Both Plutonia and TNT are commercial like Doom2,
    // but in difference from  Doom2, they have demo4 in demo cycle.
    {G_DeferedPlayDemo, "demo4"},
    {D_SetPageName, "CREDIT"},
  },

  {
    {NULL},
    {NULL},
    {NULL},
    {G_DeferedPlayDemo, "demo4"},
  },

  {
    {NULL},
    {NULL},
    {NULL},
    {NULL},
  }
};

/*
 * This cycles through the demo sequences.
 * killough 11/98: made table-driven
 */

void D_DoAdvanceDemo(void)
{
  players[consoleplayer].playerstate = PST_LIVE;  /* not reborn */
  advancedemo = false;
  dsda_ResetPauseMode();
  gameaction = ga_nothing;

  pagetic = TICRATE * 11;         /* killough 11/98: default behavior */
  gamestate = GS_DEMOSCREEN;

  if (netgame )
    demosequence = 0;
  else if (!demostates[++demosequence][gamemode].func)
    demosequence = 0;

  // do not even attempt to play DEMO4 if it is not available
  if (demosequence == 6 && gamemode == commercial && !W_LumpNameExists("demo4"))
    demosequence = 0;

  demostates[demosequence][gamemode].func(demostates[demosequence][gamemode].name);
}

//
// D_StartTitle
//
void D_StartTitle (void)
{
  gameaction = ga_nothing;
  in_game = false;
  demosequence = -1;
  D_AdvanceDemo();
}

//
// D_AddFile
//
// Rewritten by Lee Killough
//
// Ty 08/29/98 - add source parm to indicate where this came from
// CPhipps - static, const char* parameter
//         - source is an enum
//         - modified to allocate & use new wadfiles array
void D_AddFile (const char *file, wad_source_t source)
{
  int len;

  // There can only be one iwad source!
  if (source == source_iwad)
  {
    int i;

    for (i = 0; i < numwadfiles; ++i)
      if (wadfiles[i].src == source_iwad)
        wadfiles[i].src = source_skip;
  }

  wadfiles = Z_Realloc(wadfiles, sizeof(*wadfiles)*(numwadfiles+1));
  wadfiles[numwadfiles].name =
    AddDefaultExtension(strcpy(Z_Malloc(strlen(file)+5), file), ".wad");
  wadfiles[numwadfiles].src = source; // Ty 08/29/98
  wadfiles[numwadfiles].handle = 0;

  // No Rest For The Living
  len=strlen(wadfiles[numwadfiles].name);
  if (len>=9 && !strnicmp(wadfiles[numwadfiles].name+len-9,"nerve.wad",9))
    gamemission = pack_nerve;

  numwadfiles++;
}

// killough 10/98: support -dehout filename
// cph - made const, don't cache results
//e6y static
const char *D_dehout(void)
{
  dsda_arg_t* arg;

  arg = dsda_Arg(dsda_arg_dehout);

  return arg->found ? arg->value.v_string : NULL;
}

//
// CheckIWAD
//
// Verify a file is indeed tagged as an IWAD
// Scan its lumps for levelnames and return gamemode as indicated
// Detect missing wolf levels in DOOM II
//
// The filename to check is passed in iwadname, the gamemode detected is
// returned in gmode, hassec returns the presence of secret levels
//
// jff 4/19/98 Add routine to test IWAD for validity and determine
// the gamemode from it. Also note if DOOM II, whether secret levels exist
// CPhipps - const char* for iwadname, made static
//e6y static
void CheckIWAD(const char *iwadname,GameMode_t *gmode,dboolean *hassec)
{
  if (M_ReadAccess(iwadname))
  {
    int ud=0,rg=0,sw=0,cm=0,sc=0,hx=0;
    dboolean dmenupic = false;
    dboolean large_titlepic = false;
    FILE* fp;

    // Identify IWAD correctly
    if ((fp = M_OpenFile(iwadname, "rb")))
    {
      wadinfo_t header;

      // read IWAD header
      if (fread(&header, sizeof(header), 1, fp) == 1)
      {
        size_t length;
        filelump_t *fileinfo;

        if (strncmp(header.identification, "IWAD", 4)) // missing IWAD tag in header
        {
          lprintf(LO_WARN,"CheckIWAD: IWAD tag %s not present\n", iwadname);
        }

        // read IWAD directory
        header.numlumps = LittleLong(header.numlumps);
        header.infotableofs = LittleLong(header.infotableofs);
        length = header.numlumps;
        fileinfo = Z_Malloc(length*sizeof(filelump_t));
        if (fseek (fp, header.infotableofs, SEEK_SET) ||
            fread (fileinfo, sizeof(filelump_t), length, fp) != length)
        {
          fclose(fp);
          I_Error("CheckIWAD: failed to read directory %s",iwadname);
        }

        // scan directory for levelname lumps
        while (length--)
        {
          if (fileinfo[length].name[0] == 'E' &&
              fileinfo[length].name[2] == 'M' &&
              fileinfo[length].name[4] == 0)
          {
            if (fileinfo[length].name[1] == '4')
              ++ud;
            else if (fileinfo[length].name[1] == '3')
              ++rg;
            else if (fileinfo[length].name[1] == '2')
              ++rg;
            else if (fileinfo[length].name[1] == '1')
              ++sw;
          }
          else if (fileinfo[length].name[0] == 'M' &&
                    fileinfo[length].name[1] == 'A' &&
                    fileinfo[length].name[2] == 'P' &&
                    fileinfo[length].name[5] == 0)
          {
            ++cm;
            if (fileinfo[length].name[3] == '3')
              if (fileinfo[length].name[4] == '1' ||
                  fileinfo[length].name[4] == '2')
                ++sc;
          }

          if (!strncmp(fileinfo[length].name,"DMENUPIC",8))
            dmenupic = true;
          if (!strncmp(fileinfo[length].name,"TITLEPIC",8) && fileinfo[length].size > 68168)
            large_titlepic = true;
          if (!strncmp(fileinfo[length].name,"HACX",4))
            hx++;
        }
        Z_Free(fileinfo);

      }

      fclose(fp);
    }
    else // error from open call
      I_Error("CheckIWAD: Can't open IWAD %s", iwadname);

    // unity iwad has dmenupic and a large titlepic
    if (dmenupic && !large_titlepic)
      bfgedition++;

    // Determine game mode from levels present
    // Must be a full set for whichever mode is present
    // Lack of wolf-3d levels also detected here

    *gmode = indetermined;
    *hassec = false;
    if (cm>=30 || (cm>=20 && hx))
    {
      *gmode = commercial;
      *hassec = sc>=2;
    }
    else if (ud>=9)
      *gmode = retail;
    else if (rg>=18)
      *gmode = registered;
    else if (sw>=9)
      *gmode = shareware;
  }
  else // error from access call
    I_Error("CheckIWAD: IWAD %s not readable", iwadname);
}

//
// AddIWAD
//
void AddIWAD(const char *iwad)
{
  size_t i;

  if (!(iwad && *iwad))
    return;

  //jff 9/3/98 use logical output routine
  lprintf(LO_DEBUG, "IWAD found: %s\n", iwad); //jff 4/20/98 print only if found
  CheckIWAD(iwad,&gamemode,&haswolflevels);

  /* jff 8/23/98 set gamemission global appropriately in all cases
  * cphipps 12/1999 - no version output here, leave that to the caller
  */
  i = strlen(iwad);

  if (i >= 11 && !strnicmp(iwad + i - 11, "heretic.wad", 11))
  {
    if (!dsda_Flag(dsda_arg_heretic))
      dsda_UpdateFlag(dsda_arg_heretic, true);
  }

  if (i >= 9 && !strnicmp(iwad + i - 9, "hexen.wad", 9))
  {
    if (!dsda_Flag(dsda_arg_hexen))
      dsda_UpdateFlag(dsda_arg_hexen, true);

    gamemode = commercial;
    haswolflevels = false;
  }

  if (i >= 12 && !strnicmp(iwad + i - 12, "heretic1.wad", 12))
  {
    if (!dsda_Flag(dsda_arg_heretic))
      dsda_UpdateFlag(dsda_arg_heretic, true);

    gamemode = shareware;
  }

  switch(gamemode)
  {
    case retail:
    case registered:
    case shareware:
      gamemission = doom;
      if (i>=8 && !strnicmp(iwad+i-8,"chex.wad",8))
        gamemission = chex;
      break;
    case commercial:
      gamemission = doom2;
      if (i>=10 && !strnicmp(iwad+i-10,"doom2f.wad",10))
        language=french;
      else if (i>=7 && !strnicmp(iwad+i-7,"tnt.wad",7))
        gamemission = pack_tnt;
      else if (i>=12 && !strnicmp(iwad+i-12,"plutonia.wad",12))
        gamemission = pack_plut;
      else if (i>=8 && !strnicmp(iwad+i-8,"hacx.wad",8))
        gamemission = hacx;
      break;
    default:
      gamemission = none;
      break;
  }
  if (gamemode == indetermined)
    //jff 9/3/98 use logical output routine
    lprintf(LO_WARN,"Unknown Game Version, may not work\n");
  D_AddFile(iwad,source_iwad);
}

/*
 * FindIWADFIle
 *
 * Search for one of the standard IWADs
 * CPhipps  - static, proper prototype
 *    - 12/1999 - rewritten to use I_FindFile
 */
static inline dboolean CheckExeSuffix(const char *suffix)
{
  extern __thread char **dsda_argv;

  char *dash;

  if ((dash = strrchr(dsda_argv[0], '-')))
    if (!strnicmp(dash, suffix, strlen(suffix)))
      return true;

  return false;
}

static char *FindIWADFile(void)
{
  int   i;
  dsda_arg_t* arg;
  char  * iwad  = NULL;

  if (CheckExeSuffix("-heretic"))
  {
    if (!dsda_Flag(dsda_arg_heretic))
      dsda_UpdateFlag(dsda_arg_heretic, true);
  }
  else if (CheckExeSuffix("-hexen"))
  {
    if (!dsda_Flag(dsda_arg_hexen))
      dsda_UpdateFlag(dsda_arg_hexen, true);
  }

  arg = dsda_Arg(dsda_arg_iwad);
  if (arg->found)
  {
    iwad = I_FindWad(arg->value.v_string);
  }
  else
  {
    if (dsda_Flag(dsda_arg_heretic))
      return I_FindWad("heretic.wad");
    else if (dsda_Flag(dsda_arg_hexen))
      return I_FindWad("hexen.wad");

    for (i=0; !iwad && i<nstandard_iwads; i++)
      iwad = I_FindWad(standard_iwads[i]);
  }
  return iwad;
}

static dboolean FileMatchesIWAD(const char *name)
{
  int i;
  int name_length;

  name_length = strlen(name);
  for (i = 0; i < nstandard_iwads; ++i)
  {
    int iwad_length;

    iwad_length = strlen(standard_iwads[i]);
    if (
      name_length >= iwad_length &&
      !stricmp(name + name_length - iwad_length, standard_iwads[i])
    )
      return true;
  }

  return false;
}

//
// IdentifyVersion
//
// Set the location of the defaults file and the savegame root
// Locate and validate an IWAD file
// Determine gamemode from the IWAD
//
// supports IWADs with custom names. Also allows the -iwad parameter to
// specify which iwad is being searched for if several exist in one dir.
// The -iwad parm may specify:
//
// 1) a specific pathname, which must exist (.wad optional)
// 2) or a directory, which must contain a standard IWAD,
// 3) or a filename, which must be found in one of the standard places:
//   a) current dir,
//   b) exe dir
//   c) $DOOMWADDIR
//   d) or $HOME
//
// jff 4/19/98 rewritten to use a more advanced search algorithm

static void IdentifyVersion (void)
{
  char *iwad;

  // why is this here?
  dsda_InitDataDir();
  dsda_InitSaveDir();

  // locate the IWAD and determine game mode from it

  iwad = FindIWADFile();

  if (iwad && *iwad)
  {
    AddIWAD(iwad);
    Z_Free(iwad);
  }
  else
  {
    I_Error("IdentifyVersion: IWAD not found\n\n"
            "Make sure your IWADs are in a folder that dsda-doom searches on\n"
            "For example: %s", I_ConfigDir());
  }
}

//
// DoLooseFiles
//
// Take any file names on the command line before the first switch parm
// and insert the appropriate -file, -deh or -playdemo switch in front
// of them.
//
// e6y
// Fixed crash if numbers of wads/lmps/dehs is greater than 100
// Fixed bug when length of argname is smaller than 3
// Refactoring of the code to avoid use the static arrays
// The logic of DoLooseFiles has been rewritten in more optimized style
// MAXARGVS has been removed.

static void DoLooseFiles(void)
{
  extern __thread int dsda_argc;
  extern __thread char **dsda_argv;

  int i, k;
  const int loose_wad_index = 0;

  struct {
    const char *ext;
    dsda_arg_identifier_t arg_id;
  } looses[] = {
    { ".wad", dsda_arg_file },
    { ".zip", dsda_arg_file },
    { ".lmp", dsda_arg_playdemo },
    { ".deh", dsda_arg_deh },
    { ".bex", dsda_arg_deh },
    // assume wad if no extension or length of the extention is not equal to 3
    // must be last entry
    { "", dsda_arg_file },
    { 0 }
  };

  for (i = 1; i < dsda_argc; i++)
  {
    size_t arglen, extlen;

    if (*dsda_argv[i] == '-') break;  // quit at first switch

    // so now we must have a loose file.  Find out what kind and store it.
    arglen = strlen(dsda_argv[i]);

    for (k = 0; looses[k].ext; ++k)
    {
      extlen = strlen(looses[k].ext);
      if (arglen >= extlen && !stricmp(&dsda_argv[i][arglen - extlen], looses[k].ext))
      {
        // If a wad is an iwad, we don't want to send it to -file
        if (k == loose_wad_index && FileMatchesIWAD(dsda_argv[i]))
        {
          dsda_UpdateStringArg(dsda_arg_iwad, dsda_argv[i]);
          break;
        }

        dsda_AppendStringArg(looses[k].arg_id, dsda_argv[i]);
        break;
      }
    }
  }
}

const char *port_wad_file;

// CPhipps - misc screen stuff
int desired_screenwidth, desired_screenheight;

// Calculate the path to the directory for autoloaded WADs/DEHs.
// Creates the directory as necessary.

static char *autoload_path = NULL;

static char *GetAutoloadDir(const char *iwadname, dboolean createdir)
{
    char *result;
    int len;

    if (autoload_path == NULL)
    {
        const char* configdir = I_ConfigDir();
        len = snprintf(NULL, 0, "%s/autoload", configdir);
        autoload_path = Z_Malloc(len+1);
        snprintf(autoload_path, len+1, "%s/autoload", configdir);
    }

    M_MakeDir(autoload_path, false);

    len = snprintf(NULL, 0, "%s/%s", autoload_path, iwadname);
    result = Z_Malloc(len+1);
    snprintf(result, len+1, "%s/%s", autoload_path, iwadname);

    if (createdir)
    {
      M_MakeDir(result, false);
    }

    return result;
}

const char *IWADBaseName(void)
{
  int i;

  for (i = 0; i < numwadfiles; i++)
  {
    if (wadfiles[i].src == source_iwad)
      break;
  }

  if (i == numwadfiles)
    I_Error("IWADBaseName: IWAD not found\n");

  return dsda_BaseName(wadfiles[i].name);
}

// Load all WAD files from the given directory.

static void LoadWADsAtPath(const char *path, wad_source_t source)
{
    glob_t *glob;
    const char *filename;

    glob = I_StartMultiGlob(path, GLOB_FLAG_NOCASE|GLOB_FLAG_SORTED,
                            "*.wad", "*.lmp", NULL);
    for (;;)
    {
        filename = I_NextGlob(glob);
        if (filename == NULL)
        {
            break;
        }
        D_AddFile(filename, source);
    }

    I_EndGlob(glob);
}


static const char *D_AutoLoadGameBase()
{
  return "doom-all";
}

#define ALL_AUTOLOAD "all-all"

int warpepisode = -1;
int warpmap = -1;

static void HandleWarp(void)
{
  dsda_arg_t* arg;

  arg = dsda_Arg(dsda_arg_warp);

  if (arg->found)
  {
    autostart = true; // Ty 08/29/98 - move outside the decision tree

    dsda_ResolveWarp(arg->value.v_int_array, arg->count, &warpepisode, &warpmap);

    if (warpmap == -1)
      dsda_FirstMap(&warpepisode, &warpmap);

    startmap = warpmap;
    startepisode = warpepisode;
  }
}

static void HandleClass(void)
{
  return;
}

const char* doomverstr = "Unknown";

static void EvaluateDoomVerStr(void)
{
    switch ( gamemode )
    {
      case retail:
        switch (gamemission)
        {
          case chex:
            doomverstr = "Chex(R) Quest";
            break;
          default:
            doomverstr = "The Ultimate DOOM";
            break;
        }
        break;
      case shareware:
        doomverstr = "DOOM Shareware";
        break;
      case registered:
        doomverstr = "DOOM Registered";
        break;
      case commercial:  // Ty 08/27/98 - fixed gamemode vs gamemission
        switch (gamemission)
        {
          case pack_plut:
            doomverstr = "Final DOOM - The Plutonia Experiment";
            break;
          case pack_tnt:
            doomverstr = "Final DOOM - TNT: Evilution";
            break;
          case hacx:
            doomverstr = "HACX - Twitch 'n Kill";
            break;
          default:
            doomverstr = "DOOM 2: Hell on Earth";
            break;
        }
        break;
      default:
        doomverstr = "Public DOOM";
        break;
    }

  if (bfgedition)
  {
    char *tempverstr;
    const char bfgverstr[]=" (BFG Edition)";
    tempverstr = Z_Malloc(sizeof(char) * (strlen(doomverstr)+strlen(bfgverstr)+1));
    strcpy (tempverstr, doomverstr);
    strcat (tempverstr, bfgverstr);
    doomverstr = Z_Strdup (tempverstr);
    Z_Free (tempverstr);
  }

  /* cphipps - the main display. This shows the copyright and game type */
  lprintf(LO_INFO,
          "%s is released under the GNU General Public license v2.0.\n"
          "You are welcome to redistribute it under certain conditions.\n"
          "It comes with ABSOLUTELY NO WARRANTY. See the file COPYING for details.\n\n",
          PACKAGE_NAME);

  lprintf(LO_INFO, "Playing: %s\n", doomverstr);
}

//
// D_DoomMainSetup
//
// CPhipps - the old contents of D_DoomMain, but moved out of the main
//  line of execution so its stack space can be freed
extern dboolean preventLevelExit;
extern dboolean preventGameEnd;
extern dboolean reachedLevelExit;
extern dboolean reachedGameEnd;


void D_DoomMainSetup(void)
{
  int p;
  dsda_arg_t *arg;
  dboolean autoload;
  enableOutput = 0;

  setbuf(stdout,NULL);

  if (dsda_Flag(dsda_arg_help))
  {
    dsda_PrintArgHelp();
  }

  DoLooseFiles();  // Ty 08/29/98 - handle "loose" files on command line

  IdentifyVersion();

  dsda_InitGlobal();

  // e6y: DEH files preloaded in wrong order
  // http://sourceforge.net/tracker/index.php?func=detail&aid=1418158&group_id=148658&atid=772943
  // The dachaked stuff has been moved below an autoload

  // jff 1/24/98 set both working and command line value of play parms
  nomonsters = clnomonsters = dsda_Flag(dsda_arg_nomonsters);
  respawnparm = clrespawnparm = dsda_Flag(dsda_arg_respawn);
  fastparm = clfastparm = dsda_Flag(dsda_arg_fast);
  // jff 1/24/98 end of set to both working and command line value

  if (dsda_Flag(dsda_arg_altdeath))
    deathmatch = 2;
  else if (dsda_Flag(dsda_arg_deathmatch))
    deathmatch = 1;

  modifiedgame = false;

  preventLevelExit = 0;
  preventGameEnd = 0;
  reachedLevelExit = 0;
  reachedGameEnd = 0;

  // get skill / episode / map from parms

  startskill = dsda_IntConfig(dsda_config_default_skill) - 1;
  startepisode = 1;
  startmap = 1;
  autostart = false;

  arg = dsda_Arg(dsda_arg_skill);
  if (arg->found)
  {
    startskill = arg->value.v_int - 1;
    autostart = true;
  }

  arg = dsda_Arg(dsda_arg_episode);
  if (arg->found)
  {
    startepisode = arg->value.v_int;
    startmap = 1;
    autostart = true;
  }

  HandleClass();

  arg = dsda_Arg(dsda_arg_timer);
  if (arg->found && deathmatch)
  {
    int time = arg->value.v_int;
    //jff 9/3/98 use logical output routine
    lprintf(LO_INFO,"Levels will end after %d minute%s.\n", time, time>1 ? "s" : "");
  }

  //jff 1/22/98 add command line parms to disable sound and music
  {
    int nosound = dsda_Flag(dsda_arg_nosound);
    nomusicparm = nosound || dsda_Flag(dsda_arg_nomusic);
    nosfxparm   = nosound || dsda_Flag(dsda_arg_nosfx);
  }
  //jff end of sound/music command line parms

  // killough 3/2/98: allow -nodraw generally
  nodrawers = dsda_Flag(dsda_arg_nodraw);

  // init subsystems

  G_ReloadDefaults();    // killough 3/4/98: set defaults just loaded.
  // jff 3/24/98 this sets startskill if it was -1

  // proff 04/05/2000: for GL-specific switches
  #ifdef __ENABLE_OPENGL_
  gld_InitCommandLine();
  #endif

  //jff 9/3/98 use logical output routine
  lprintf(LO_DEBUG, "V_Init: allocate screens.\n");
  V_Init();

  //e6y: some stuff from command-line should be initialised before ProcessDehFile()
  e6y_InitCommandLine();

  // CPhipps - autoloading of wads
  autoload = !dsda_Flag(dsda_arg_noautoload);

  D_AddFile(port_wad_file, source_auto_load);

  EvaluateDoomVerStr(); // must come after HandlePlayback (may change iwad)

  // add any files specified on the command line with -file wadfile
  // to the wad list

  if ((arg = dsda_Arg(dsda_arg_file))->found)
  {
    int file_i;
    // the parms after p are wadfile/lump names,
    // until end of parms or another - preceded parm
    modifiedgame = true;            // homebrew levels

    for (file_i = 0; file_i < arg->count; ++file_i)
    {
      const char* file_name;
      char *file = NULL;

      file_name = arg->value.v_string_array[file_i];

      if (!dsda_FileExtension(file_name))
      {
        const char *extensions[] = { ".wad", ".lmp", ".zip", ".deh", ".bex", NULL };

        file = I_RequireAnyFile(file_name, extensions);
        file_name = file;
      }

      if (dsda_HasFileExt(file_name, ".deh") || dsda_HasFileExt(file_name, ".bex"))
      {
        dsda_AppendStringArg(dsda_arg_deh, file_name);
      }
      else if (dsda_HasFileExt(file_name, ".wad") || dsda_HasFileExt(file_name, ".lmp"))
      {
        if (!file)
          file = I_RequireWad(file_name);

        D_AddFile(file, source_pwad);
      }
      else
      {
        I_Error("File type \"%s\" is not supported", dsda_FileExtension(file_name));
      }

      Z_Free(file);
    }
  }

  D_InitFakeNetGame();

  //jff 9/3/98 use logical output routine
  lprintf(LO_DEBUG, "W_Init: Init WADfiles.\n");
  W_Init(); // CPhipps - handling of wadfiles init changed

  lprintf(LO_DEBUG, "G_ReloadDefaults: Checking OPTIONS.\n");
  dsda_ParseOptionsLump();
  G_ReloadDefaults();

  // Load command line dehacked patches after WAD dehacked patches

  // e6y: DEH files preloaded in wrong order
  // http://sourceforge.net/tracker/index.php?func=detail&aid=1418158&group_id=148658&atid=772943

  // ty 03/09/98 do dehacked stuff
  // Using -deh in BOOM, others use -dehacked.
  // Ty 03/18/98 also allow .bex extension.  .bex overrides if both exist.

  dsda_AppendZDoomMobjInfo();
  dsda_ApplyDefaultMapFormat();

  lprintf(LO_DEBUG, "dsda_InitWadStats: Setting up wad stats.\n");
  dsda_InitWadStats();

  lprintf(LO_INFO, "\n"); // Separator after file loading

  V_InitColorTranslation(); //jff 4/24/98 load color translation lumps

  //jff 9/3/98 use logical output routine
  lprintf(LO_DEBUG, "M_Init: Init miscellaneous info.\n");


  //jff 9/3/98 use logical output routine
  lprintf(LO_DEBUG, "R_Init: Init DOOM refresh daemon - ");
  R_Init();

  dsda_LoadWadPreferences();
  dsda_LoadMapInfo();
  dsda_InitSkills();

  //jff 9/3/98 use logical output routine
  lprintf(LO_DEBUG, "\nP_Init: Init Playloop state.\n");
  P_Init();

  // Must be after P_Init
  HandleWarp();

  //jff 9/3/98 use logical output routine
  lprintf(LO_DEBUG, "dsda_InitFont: Loading the hud fonts.\n");

  // NSM
  arg = dsda_Arg(dsda_arg_viddump);
  if (arg->found)
  {
    I_CapturePrep(arg->value.v_string);
  }

  // start the appropriate game based on parms

      G_InitNew(startskill, startepisode, startmap, true);

  lprintf(LO_DEBUG, "\n"); // Separator after setup
}

//
// D_DoomMain
//

void D_DoomMain(void)
{
  D_DoomMainSetup(); // CPhipps - setup out of main execution stack
}


/// Functions for headless execution

void headlessRunSingleTick(void)
{
  G_Ticker ();
  gametic++;
}

void headlessUpdateSounds(void)
{
}

void headlessUpdateVideo(void)
{
}

void headlessEnableRendering()
 {
    nodrawers = 0;
    nomusicparm = 0;
    nosfxparm = 0;
 }

void headlessDisableRendering()
{
   nodrawers = 1;
   nomusicparm = 1;
   nosfxparm = 1;
}

/// Headless functions

void headlessClearTickCommand() { memset(local_cmds, 0, sizeof(ticcmd_t) * MAX_MAXPLAYERS); }
void headlessSetTickCommand(int playerId, int forwardSpeed, int strafingSpeed, int turningSpeed, int fire, int action, int weapon, int altWeapon)
{
  local_cmds[playerId].forwardmove = forwardSpeed;
  local_cmds[playerId].sidemove    = strafingSpeed;
  local_cmds[playerId].angleturn   = turningSpeed << 8;

  if (fire == 1)    local_cmds[playerId].buttons |= 0b00000001;
  if (action == 1)  local_cmds[playerId].buttons |= 0b00000010;

  if (weapon == 0)  local_cmds[playerId].buttons |= 0b00000000;
  if (weapon == 1)  local_cmds[playerId].buttons |= 0b00000100;
  if (weapon == 2)  local_cmds[playerId].buttons |= 0b00001000;
  if (weapon == 3)  local_cmds[playerId].buttons |= 0b00001100;
  if (weapon == 4)  local_cmds[playerId].buttons |= 0b00010000;
  if (weapon == 5)  local_cmds[playerId].buttons |= 0b00010100;
  if (weapon == 6)  local_cmds[playerId].buttons |= 0b00011000;
  if (weapon == 7)  local_cmds[playerId].buttons |= 0b00011100;

  if (altWeapon == 1)  local_cmds[playerId].buttons |= 0b00100000;

  // printf("ForwardSpeed: %d - sideMove:     %d - angleTurn:    %d - buttons: %u\n", forwardSpeed, strafingSpeed, turningSpeed, local_cmds[playerId].buttons);
}

//int main(int argc, const char * const * argv)
// Headless main does not initialize SDL
int headlessMain(int argc, char **argv)
{
  dsda_ParseCommandLineArgs(argc, argv);

  // e6y: Check for conflicts.
  // Conflicting command-line parameters could cause the engine to be confused
  // in some cases. Added checks to prevent this.
  // Example: dsda-doom.exe -record mydemo -playdemo demoname
  ParamsMatchingCheck();

  // e6y: was moved from D_DoomMainSetup
  // init subsystems
  //jff 9/3/98 use logical output routine
  lprintf(LO_DEBUG, "M_LoadDefaults: Load system defaults.\n");
  M_LoadDefaults();              // load before initing other systems
  lprintf(LO_DEBUG, "\n");

  D_DoomMainSetup();
  return 0;
}

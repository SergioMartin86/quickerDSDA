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
 *      All the clipping: columns, horizontal spans, sky columns.
 *
 *-----------------------------------------------------------------------------*/
//
// 4/25/98, 5/2/98 killough: reformatted, beautified

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "doomstat.h"
#include "p_spec.h"
#include "r_main.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "r_plane.h"
#include "r_draw.h"
#include "w_wad.h"
#include "v_video.h"
#include "lprintf.h"

#include "dsda/mapinfo.h"
#include "dsda/render_stats.h"

// OPTIMIZE: closed two sided lines as single sided

// killough 1/6/98: replaced globals with statics where appropriate

// True if any of the segs textures might be visible.
static dboolean  segtextured;
static dboolean  markfloor;      // False if the back side is the same plane.
static dboolean  markceiling;
static dboolean  maskedtexture;
static int      toptexture;
static int      bottomtexture;
static int      midtexture;

static fixed_t  toptexheight, midtexheight, bottomtexheight; // cph

angle_t         rw_normalangle; // angle to line origin
int             rw_angle1;
fixed_t         rw_distance;
const lighttable_t    **walllights;

//
// regular wall
//
static int      rw_x;
static int      rw_stopx;
static angle_t  rw_centerangle;
static fixed_t  rw_offset;
static fixed_t  rw_scale;
static fixed_t  rw_scalestep;
static fixed_t  rw_midtexturemid;
static fixed_t  rw_toptexturemid;
static fixed_t  rw_bottomtexturemid;
static int      rw_lightlevel;
static int      worldtop;
static int      worldbottom;
static int      worldhigh;
static int      worldlow;
static int64_t  pixhigh; // R_WiggleFix
static int64_t  pixlow; // R_WiggleFix
static fixed_t  pixhighstep;
static fixed_t  pixlowstep;
static int64_t  topfrac; // R_WiggleFix
static fixed_t  topstep;
static int64_t  bottomfrac; // R_WiggleFix
static fixed_t  bottomstep;
static int      *maskedtexturecol; // dropoff overflow

static int	max_rwscale = 64 * FRACUNIT;
static int	HEIGHTBITS = 12;
static int	HEIGHTUNIT = (1 << 12);
static int	invhgtbits = 4;

/* cph - allow crappy fake contrast to be disabled */
fake_contrast_mode_t fake_contrast_mode;

//
// R_FixWiggle()
// Dynamic wall/texture rescaler, AKA "WiggleHack II"
//  by Kurt "kb1" Baumgardner ("kb")
//
//  [kb] When the rendered view is positioned, such that the viewer is
//   looking almost parallel down a wall, the result of the scale
//   calculation in R_ScaleFromGlobalAngle becomes very large. And, the
//   taller the wall, the larger that value becomes. If these large
//   values were used as-is, subsequent calculations would overflow
//   and crash the program.
//
//  Therefore, vanilla Doom clamps this scale calculation, preventing it
//   from becoming larger than 0x400000 (64*FRACUNIT). This number was
//   chosen carefully, to allow reasonably-tight angles, with reasonably
//   tall sectors to be rendered, within the limits of the fixed-point
//   math system being used. When the scale gets clamped, Doom cannot
//   properly render the wall, causing an undesirable wall-bending
//   effect that I call "floor wiggle".
//
//  Modern source ports offer higher video resolutions, which worsens
//   the issue. And, Doom is simply not adjusted for the taller walls
//   found in many PWADs.
//
//  WiggleHack II attempts to correct these issues, by dynamically
//   adjusting the fixed-point math, and the maximum scale clamp,
//   on a wall-by-wall basis. This has 2 effects:
//
//  1. Floor wiggle is greatly reduced and/or eliminated.
//  2. Overflow is not longer possible, even in levels with maximum
//     height sectors.
//
//  It is not perfect across all situations. Some floor wiggle can be
//   seen, and some texture strips may be slight misaligned in extreme
//   cases. These effects cannot be corrected without increasing the
//   precision of various renderer variables, and, possibly, suffering
//   a performance penalty.
//

void R_FixWiggle(sector_t *sec)
{
  static int  lastheight = 0;

  static const struct
  {
    int clamp;
    int heightbits;
  } scale_values[9] = {
    {2048 * FRACUNIT, 12}, {1024 * FRACUNIT, 12}, {1024 * FRACUNIT, 11},
    { 512 * FRACUNIT, 11}, { 512 * FRACUNIT, 10}, { 256 * FRACUNIT, 10},
    { 256 * FRACUNIT, 9},  { 128 * FRACUNIT, 9},  {  64 * FRACUNIT, 9},
  };

  int height = (sec->ceilingheight - sec->floorheight) >> FRACBITS;

  // disallow negative heights, force cache initialization
  if (height < 1)
    height = 1;

  // early out?
  if (height != lastheight)
  {
    lastheight = height;

    // initialize, or handle moving sector
    if (height != sec->cachedheight)
    {
      frontsector->cachedheight = height;
      frontsector->scaleindex = 0;
      height >>= 7;
      // calculate adjustment
      while ((height >>= 1))
        frontsector->scaleindex++;
    }

    // fine-tune renderer for this wall
    max_rwscale = scale_values[frontsector->scaleindex].clamp;
    HEIGHTBITS = scale_values[frontsector->scaleindex].heightbits;
    HEIGHTUNIT = 1 << HEIGHTBITS;
    invhgtbits = 16 - HEIGHTBITS;
  }
}

//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
// killough 5/2/98: reformatted, cleaned up
// CPhipps - moved here from r_main.c

static fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
  int anglea = ANG90 + (visangle - viewangle);
  int angleb = ANG90 + (visangle - rw_normalangle);
  int den = FixedMul(rw_distance, finesine[anglea >> ANGLETOFINESHIFT]);
  // proff 11/06/98: Changed for high-res
  fixed_t num = FixedMul(projectiony, finesine[angleb >> ANGLETOFINESHIFT]);
  fixed_t scale;

  if (den > (num >> 16))
  {
    scale = FixedDiv(num, den);

    // [kb] use R_WiggleFix clamp
    if (scale > max_rwscale)
      scale = max_rwscale;
    else if (scale < 256)
      scale = 256;
  }
  else
    scale = max_rwscale;

  return scale;
}

const int fake_contrast_value = 16;

static dboolean R_FakeContrast(seg_t *seg)
{
  return fake_contrast_mode != FAKE_CONTRAST_MODE_OFF &&
         !(map_info.flags & MI_EVEN_LIGHTING) &&
         seg && !(seg->sidedef->flags & SF_NOFAKECONTRAST);
}

static dboolean R_SmoothLighting(seg_t *seg)
{
  return fake_contrast_mode == FAKE_CONTRAST_MODE_SMOOTH ||
         map_info.flags & MI_SMOOTH_LIGHTING ||
         seg->sidedef->flags & SF_SMOOTHLIGHTING;
}

void R_AddContrast(seg_t *seg, int *base_lightlevel)
{
  /* cph - ...what is this for? adding contrast to rooms?
   * It looks crap in outdoor areas */
  if (R_FakeContrast(seg))
  {
    if (seg->linedef->dy == 0)
    {
      *base_lightlevel -= fake_contrast_value;
    }
    else if (seg->linedef->dx == 0)
    {
      *base_lightlevel += fake_contrast_value;
    }
    else if (R_SmoothLighting(seg))
    {
      double dx, dy;

      dx = (double) seg->linedef->dx / FRACUNIT;
      dy = (double) seg->linedef->dy / FRACUNIT;

      *base_lightlevel +=
        lround(fabs(atan(dy / dx) * 2 / M_PI) * (2 * fake_contrast_value) - fake_contrast_value);
    }
  };
}

const lighttable_t** GetLightTable(int lightlevel)
{
  int lightnum;

  R_AddContrast(curline, &lightlevel);

  lightnum = (lightlevel >> LIGHTSEGSHIFT) + (extralight * LIGHTBRIGHT);

  return scalelight[BETWEEN(0, LIGHTLEVELS - 1, lightnum)];
}

static void R_UpdateWallLights(int lightlevel)
{
  walllights = GetLightTable(lightlevel);
}

static int R_SideLightLevel(side_t *side, int base_lightlevel)
{
  return side->lightlevel +
         ((side->flags & SF_LIGHTABSOLUTE) ? 0 : base_lightlevel);
}

int R_TopLightLevel(side_t *side, int base_lightlevel)
{
  return side->lightlevel_top +
         ((side->flags & SF_LIGHTABSOLUTETOP) ? 0 : R_SideLightLevel(side, base_lightlevel));
}

int R_MidLightLevel(side_t *side, int base_lightlevel)
{
  return side->lightlevel_mid +
         ((side->flags & SF_LIGHTABSOLUTEMID) ? 0 : R_SideLightLevel(side, base_lightlevel));
}

int R_BottomLightLevel(side_t *side, int base_lightlevel)
{
  return side->lightlevel_bottom +
         ((side->flags & SF_LIGHTABSOLUTEBOTTOM) ? 0 : R_SideLightLevel(side, base_lightlevel));
}

static void R_ApplyTopLight(side_t *side)
{
  int lightlevel;

  lightlevel = R_TopLightLevel(side, rw_lightlevel);

  R_UpdateWallLights(lightlevel);
}

static void R_ApplyMidLight(side_t *side)
{
  int lightlevel;

  lightlevel = R_MidLightLevel(side, rw_lightlevel);

  R_UpdateWallLights(lightlevel);
}

static void R_ApplyBottomLight(side_t *side)
{
  int lightlevel;

  lightlevel = R_BottomLightLevel(side, rw_lightlevel);

  R_UpdateWallLights(lightlevel);
}

static void R_ApplyLightColormap(draw_column_vars_t *dcvars, fixed_t scale)
{
  if (!fixedcolormap)
  {
    int index = (int)(((int64_t) scale * 160 / wide_centerx) >> LIGHTSCALESHIFT);
    if (index >= MAXLIGHTSCALE)
        index = MAXLIGHTSCALE - 1;

    dcvars->colormap = walllights[index];
  }
  else
  {
    dcvars->colormap = fixedcolormap;
  }
}

//
// R_RenderMaskedSegRange
//

void R_RenderMaskedSegRange(drawseg_t *ds, int x1, int x2)
{
}

//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// CALLED: CORE LOOPING ROUTINE.
//

static int didsolidcol; /* True if at least one column was marked solid */

static void R_RenderSegLoop (void)
{
  const rpatch_t *tex_patch;
  R_DrawColumn_f colfunc = R_GetDrawColumnFunc(RDC_PIPELINE_STANDARD, RDRAW_FILTER_POINT);
  draw_column_vars_t dcvars;
  fixed_t texturecolumn = 0;
  fixed_t specific_texturecolumn = 0;

  R_SetDefaultDrawColumnVars(&dcvars);

  dsda_RecordDrawSeg();

  for ( ; rw_x < rw_stopx ; rw_x++)
  {
    // mark floor / ceiling areas
    int yh = (int)(bottomfrac>>HEIGHTBITS);
    int yl = (int)((topfrac+HEIGHTUNIT-1)>>HEIGHTBITS);

    // no space above wall?
    int bottom,top = ceilingclip[rw_x]+1;

    if (yl < top)
      yl = top;

    if (markceiling)
    {
      bottom = yl-1;

      if (bottom >= floorclip[rw_x])
        bottom = floorclip[rw_x]-1;

      if (top <= bottom)
      {
        ceilingplane->top[rw_x] = top;
        ceilingplane->bottom[rw_x] = bottom;
      }
      // SoM: this should be set here
      ceilingclip[rw_x] = bottom;
    }

    bottom = floorclip[rw_x]-1;
    if (yh > bottom)
      yh = bottom;

    if (markfloor)
    {

      top  = yh < ceilingclip[rw_x] ? ceilingclip[rw_x] : yh;

      if (++top <= bottom)
      {
        floorplane->top[rw_x] = top;
        floorplane->bottom[rw_x] = bottom;
      }
      // SoM: This should be set here to prevent overdraw
      floorclip[rw_x] = top;
    }

    // texturecolumn and lighting are independent of wall tiers
    if (segtextured)
    {
      // calculate texture offset
      angle_t angle =(rw_centerangle+xtoviewangle[rw_x])>>ANGLETOFINESHIFT;

      texturecolumn = rw_offset-FixedMul(finetangent[angle],rw_distance);
      texturecolumn >>= FRACBITS;

      dcvars.x = rw_x;
      dcvars.iscale = 0xffffffffu / (unsigned)rw_scale;
    }

    // draw the wall tiers
    if (midtexture)
    {
      specific_texturecolumn = texturecolumn +
                               (curline->sidedef->textureoffset_mid >> FRACBITS);

      dcvars.yl = yl;     // single sided line
      dcvars.yh = yh;
      dcvars.texturemid = rw_midtexturemid;
      tex_patch = R_TextureCompositePatchByNum(midtexture);
      dcvars.source = R_GetTextureColumn(tex_patch, specific_texturecolumn);
      dcvars.prevsource = R_GetTextureColumn(tex_patch, specific_texturecolumn-1);
      dcvars.nextsource = R_GetTextureColumn(tex_patch, specific_texturecolumn+1);
      dcvars.texheight = midtexheight;
      if (!fixedcolormap)
        R_ApplyMidLight(curline->sidedef);
      R_ApplyLightColormap(&dcvars, rw_scale);
      colfunc(&dcvars);
      tex_patch = NULL;
      ceilingclip[rw_x] = viewheight;
      floorclip[rw_x] = -1;
    }
    else
    {
      // two sided line
      if (toptexture)
      {
        // top wall
        int mid = (int)(pixhigh>>HEIGHTBITS);
        pixhigh += pixhighstep;

        if (mid >= floorclip[rw_x])
          mid = floorclip[rw_x]-1;

        if (mid >= yl)
        {
          specific_texturecolumn = texturecolumn +
                                   (curline->sidedef->textureoffset_top >> FRACBITS);

          dcvars.yl = yl;
          dcvars.yh = mid;
          dcvars.texturemid = rw_toptexturemid;
          tex_patch = R_TextureCompositePatchByNum(toptexture);
          dcvars.source = R_GetTextureColumn(tex_patch,specific_texturecolumn);
          dcvars.prevsource = R_GetTextureColumn(tex_patch,specific_texturecolumn-1);
          dcvars.nextsource = R_GetTextureColumn(tex_patch,specific_texturecolumn+1);
          dcvars.texheight = toptexheight;
          if (!fixedcolormap)
            R_ApplyTopLight(curline->sidedef);
          R_ApplyLightColormap(&dcvars, rw_scale);
          colfunc(&dcvars);
          tex_patch = NULL;
          ceilingclip[rw_x] = mid;
        }
        else
          ceilingclip[rw_x] = yl-1;
      }
      else  // no top wall
      {
        if (markceiling)
          ceilingclip[rw_x] = yl-1;
      }

      if (bottomtexture)          // bottom wall
      {
        int mid = (int)((pixlow+HEIGHTUNIT-1)>>HEIGHTBITS);
        pixlow += pixlowstep;

        // no space above wall?
        if (mid <= ceilingclip[rw_x])
          mid = ceilingclip[rw_x]+1;

        if (mid <= yh)
        {
          specific_texturecolumn = texturecolumn +
                                   (curline->sidedef->textureoffset_bottom >> FRACBITS);

          dcvars.yl = mid;
          dcvars.yh = yh;
          dcvars.texturemid = rw_bottomtexturemid;
          tex_patch = R_TextureCompositePatchByNum(bottomtexture);
          dcvars.source = R_GetTextureColumn(tex_patch, specific_texturecolumn);
          dcvars.prevsource = R_GetTextureColumn(tex_patch, specific_texturecolumn-1);
          dcvars.nextsource = R_GetTextureColumn(tex_patch, specific_texturecolumn+1);
          dcvars.texheight = bottomtexheight;
          if (!fixedcolormap)
            R_ApplyBottomLight(curline->sidedef);
          R_ApplyLightColormap(&dcvars, rw_scale);
          colfunc(&dcvars);
          tex_patch = NULL;
          floorclip[rw_x] = mid;
        }
        else
          floorclip[rw_x] = yh+1;
      }
      else        // no bottom wall
      {
        if (markfloor)
          floorclip[rw_x] = yh+1;
      }

      // cph - if we completely blocked further sight through this column,
      // add this info to the solid columns array for r_bsp.c
      if ((markceiling || markfloor) && (floorclip[rw_x] <= ceilingclip[rw_x] + 1))
      {
        solidcol[rw_x] = 1; didsolidcol = 1;
      }

      // save texturecol for backdrawing of masked mid texture
      if (maskedtexture)
        maskedtexturecol[rw_x] = texturecolumn;
    }

    rw_scale += rw_scalestep;
    topfrac += topstep;
    bottomfrac += bottomstep;
  }
}

//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void R_StoreWallRange(const int start, const int stop)
{
}

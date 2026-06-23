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
 *   Thanks Roman "Vortex" Marchenko
 *---------------------------------------------------------------------
 */

#ifndef _GL_OPENGL_H
#define _GL_OPENGL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomtype.h"

#if !defined(GL_DEPTH_STENCIL_EXT)
#define GL_DEPTH_STENCIL_EXT              0x84F9
#endif

#define isExtensionSupported(ext) strstr(extensions, ext)

extern int gl_max_texture_size;

extern dboolean gl_ext_texture_filter_anisotropic;
extern dboolean gl_arb_texture_compression;
extern dboolean gl_ext_framebuffer_object;
extern dboolean gl_ext_packed_depth_stencil;
extern dboolean gl_ext_blend_color;
extern dboolean gl_use_stencil;
extern dboolean gl_ext_arb_vertex_buffer_object;
extern dboolean gl_arb_pixel_buffer_object;
extern dboolean gl_arb_shader_objects;

void gld_InitOpenGL(void);

//states

typedef enum
{
  TMF_MASKBIT = 1,
  TMF_OPAQUEBIT = 2,
  TMF_INVERTBIT = 4,

  TM_MODULATE = 0,
  TM_MASK = TMF_MASKBIT,
  TM_OPAQUE = TMF_OPAQUEBIT,
  TM_INVERT = TMF_INVERTBIT,
  //TM_INVERTMASK = TMF_MASKBIT | TMF_INVERTBIT
  TM_INVERTOPAQUE = TMF_INVERTBIT | TMF_OPAQUEBIT,
} tex_mode_e;
void SetTextureMode(tex_mode_e type);

#endif // _GL_OPENGL_H

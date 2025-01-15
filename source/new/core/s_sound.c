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
 * DESCRIPTION:  Platform-independent sound code
 *
 *-----------------------------------------------------------------------------*/

// killough 3/7/98: modified to allow arbitrary listeners in spy mode
// killough 5/2/98: reindented, removed useless code, beautified

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomstat.h"
#include "s_sound.h"
#include "s_advsound.h"
#include "i_sound.h"
#include "i_system.h"
#include "d_main.h"
#include "r_main.h"
#include "m_random.h"
#include "w_wad.h"
#include "lprintf.h"
#include "p_maputl.h"
#include "p_setup.h"
#include "e6y.h"

#include "hexen/sn_sonix.h"

#include "dsda/configuration.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/memory.h"
#include "dsda/music.h"
#include "dsda/settings.h"
#include "dsda/sfx.h"
#include "dsda/skip.h"

// Adjustable by menu.
#define NORM_PITCH 128
#define NORM_PRIORITY 64
#define NORM_SEP 128
#define S_STEREO_SWING (96<<FRACBITS)

const int channel_not_found = -1;

typedef struct
{
  sfxinfo_t *sfxinfo;  // sound information (if null, channel avail.)
  void *origin;        // origin of sound
  int handle;          // handle of the sound being played
  int pitch;

  // heretic
  int priority;

  // hexen
  int volume;

  dboolean active;
  dboolean ambient;
  float attenuation;
  float volume_factor;
  dboolean loop;
  int loop_timeout;
  sfx_class_t sfx_class;
} channel_t;

// the set of channels available
static channel_t channels[MAX_CHANNELS];
static degenmobj_t sobjs[MAX_CHANNELS];

// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
int snd_SfxVolume;

// Derived value (not saved, accounts for muted sfx)
static int sfx_volume;

// Maximum volume of music.
int snd_MusicVolume = 15;

// whether songs are mus_paused
static dboolean mus_paused;

// music currently being played
musicinfo_t *mus_playing;

// music currently should play
static int musicnum_current;

// number of channels available
int numChannels;

//jff 3/17/98 to keep track of last IDMUS specified music num
int idmusnum;

//
// Internals.
//

void S_StopChannel(int cnum);

int S_AdjustSoundParams(mobj_t *listener, mobj_t *source, channel_t *channel, sfx_params_t *params);

static int S_getChannel(void *origin, sfxinfo_t *sfxinfo, sfx_params_t *params);


// heretic
int max_snd_dist = 1600;
int dist_adjust = 160;

static byte* soundCurve;
static int AmbChan = -1;

static mobj_t* GetSoundListener(void);
static void Heretic_S_StopSound(void *_origin);
static void Raven_S_StartSoundAtVolume(void *_origin, int sound_id, int volume, int loop_timeout);

void S_ResetSfxVolume(void)
{
  snd_SfxVolume = dsda_IntConfig(dsda_config_sfx_volume);

  if (nosfxparm)
    return;

  if (dsda_MuteSfx())
    sfx_volume = 0;
  else
    sfx_volume = snd_SfxVolume;
}

void S_ResetVolume(void)
{
  void I_ResetMusicVolume(void);

  S_ResetSfxVolume();
}

// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void S_Init(void)
{
  idmusnum = -1; //jff 3/17/98 insure idmus number is blank

  S_Stop();

  numChannels = dsda_IntConfig(dsda_config_snd_channels);

  //jff 1/22/98 skip sound init if sound not enabled
  if (!nosfxparm)
  {
    static dboolean first_s_init = true;
    S_ResetSfxVolume();

    // Reset channel memory
    memset(channels, 0, sizeof(channels));
    memset(sobjs, 0, sizeof(sobjs));

    if (first_s_init)
    {
      int i;
      int snd_curve_lump;

      first_s_init = false;

      for (i = 1; i < num_sfx; i++)
        S_sfx[i].lumpnum = -1;

      dsda_CacheSoundLumps();

      // {
      //   int i;
      //   const int snd_curve_length = 1200;
      //   const int flat_curve_length = 160;
      //   byte* buffer = Z_Malloc(snd_curve_length);
      //   for (i = 0; i < snd_curve_length; ++i)
      //   {
      //     if (i < flat_curve_length)
      //       buffer[i] = 127;
      //     else
      //       buffer[i] = 127 * (snd_curve_length - i) / (snd_curve_length - flat_curve_length);

      //     if (!buffer[i])
      //       buffer[i] = 1;
      //   }
      //   M_WriteFile("sndcurve.lmp", buffer, snd_curve_length);
      // }

      snd_curve_lump = W_GetNumForName("SNDCURVE");
      max_snd_dist = W_LumpLength(snd_curve_lump);

      dist_adjust = max_snd_dist / 10;

      soundCurve = Z_Malloc(max_snd_dist);
      memcpy(soundCurve, (const byte *) W_LumpByNum(snd_curve_lump), max_snd_dist);
    }
  }

  // CPhipps - music init reformatted
  if (!nomusicparm) {
    // no sounds are playing, and they are not mus_paused
    mus_paused = 0;
  }
}

void S_Stop(void)
{
  int cnum;

  // heretic
  AmbChan = -1;

  //jff 1/22/98 skip sound init if sound not enabled
  if (!nosfxparm)
    for (cnum=0 ; cnum<numChannels ; cnum++)
      if (channels[cnum].active)
        S_StopChannel(cnum);
}

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//

void S_Start(void)
{
  int mnum;
  int muslump;

  // kill all playing sounds at start of level
  //  (trust me - a good idea)

  S_Stop();

  // start new music for the level
  mus_paused = 0;

  dsda_MapMusic(&mnum, &muslump, gameepisode, gamemap);

  if (muslump >= 0)
  {
    musinfo.items[0] = muslump;
  }

  if (musinfo.items[0] != -1)
  {
    if (!dsda_StartQueuedMusic())
      S_ChangeMusInfoMusic(musinfo.items[0], true);
  }
  else
  {
    if (!dsda_StartQueuedMusic())
      S_ChangeMusic(mnum, true);
  }
}

static float adjust_attenuation;
static float adjust_volume;

void S_AdjustAttenuation(float attenuation) {
  adjust_attenuation = attenuation;
}

void S_AdjustVolume(float volume) {
  adjust_volume = volume;
}

void S_ResetAdjustments(void) {
  adjust_attenuation = 0;
  adjust_volume = 0;
}

void S_StartSoundAtVolume(void *origin_p, int sfx_id, int volume, int loop_timeout)
{
  int cnum;
  sfx_params_t params;
  sfxinfo_t *sfx;
  mobj_t *origin;
  mobj_t *listener;

  origin = (mobj_t *) origin_p;
  listener = GetSoundListener();

  //jff 1/22/98 return if sound is not enabled
  if (nosfxparm)
    return;

  // killough 4/25/98
  if (sfx_id == g_sfx_secret)
    params.sfx_class = sfx_class_secret;
  else if (sfx_id & PICKUP_SOUND ||
      sfx_id == sfx_oof ||
      (compatibility_level >= prboom_2_compatibility && sfx_id == sfx_noway))
    params.sfx_class = sfx_class_important;
  else
    params.sfx_class = sfx_class_none;

  params.ambient = false;
  params.attenuation = adjust_attenuation;
  params.volume_factor = adjust_volume;
  params.loop = loop_timeout > 0;
  params.loop_timeout = loop_timeout;

  sfx_id &= ~PICKUP_SOUND;

  if (sfx_id == sfx_None)
    return;

  // check for bogus sound #
  if (sfx_id < 1 || sfx_id > num_sfx)
    I_Error("S_StartSoundAtVolume: Bad sfx #: %d", sfx_id);

  sfx = &S_sfx[sfx_id];

  // Initialize sound parameters
  params.priority = 128 - sfx->priority;
  if (params.priority <= 0)
    params.priority = 1;
  if (sfx->pitch < 0)
    params.pitch = NORM_PITCH;
  else
    params.pitch = sfx->pitch;
  params.volume = volume;

  // Check to see if it is audible, modify the params
  // killough 3/7/98, 4/25/98: code rearranged slightly

  if (!origin || origin == listener) {
    params.separation = NORM_SEP;
    params.volume *= 8;
    params.priority *= 10;
  } else
    if (!S_AdjustSoundParams(listener, origin, NULL, &params))
      return;
    else if (origin->x == listener->x && origin->y == listener->y)
      params.separation = NORM_SEP;

  if (dsda_BlockSFX(sfx)) return;

  // hacks to vary the sfx pitches
  if (sfx_id >= sfx_sawup && sfx_id <= sfx_sawhit)
    params.pitch += 8 - (M_Random()&15);
  else
    if (sfx_id != sfx_itemup && sfx_id != sfx_tink)
      params.pitch += 16 - (M_Random()&31);

  if (params.pitch < 0)
    params.pitch = 0;

  if (params.pitch > 255)
    params.pitch = 255;

  // try to find a channel
  cnum = S_getChannel(origin, sfx, &params);

  if (cnum == channel_not_found)
    return;
}

void S_StartSectorSound(sector_t *sector, int sfx_id)
{
  if (sector->flags & SECF_SILENT)
    return;

  S_StartSound((mobj_t *) &sector->soundorg, sfx_id);
}

void S_LoopSectorSound(sector_t *sector, int sfx_id, int timeout)
{
  if (sector->flags & SECF_SILENT)
    return;

  S_LoopSound((mobj_t *) &sector->soundorg, sfx_id, timeout);
}

void S_StartMobjSound(mobj_t *mobj, int sfx_id)
{
  if (mobj && mobj->subsector && mobj->subsector->sector->flags & SECF_SILENT)
    return;

  S_StartSound(mobj, sfx_id);
}

void S_LoopMobjSound(mobj_t *mobj, int sfx_id, int timeout)
{
  if (mobj && mobj->subsector && mobj->subsector->sector->flags & SECF_SILENT)
    return;

  S_LoopSound(mobj, sfx_id, timeout);
}

void S_StartVoidSound(int sfx_id)
{
  S_StartSound(NULL, sfx_id);
}

void S_LoopVoidSound(int sfx_id, int timeout)
{
  S_LoopSound(NULL, sfx_id, timeout);
}

void S_StartLineSound(line_t *line, degenmobj_t *soundorg, int sfx_id)
{
  if (line && line->frontsector && line->frontsector->flags & SECF_SILENT)
    return;

  S_StartSound((mobj_t *) soundorg, sfx_id);
}

void S_StartSound(void *origin, int sfx_id)
{
  S_StartSoundAtVolume(origin, sfx_id, sfx_volume, 0);
}

void S_LoopSound(void *origin, int sfx_id, int timeout)
{
  S_StartSoundAtVolume(origin, sfx_id,  sfx_volume, timeout);
}

void S_StopSound(void *origin)
{
  int cnum;

  //jff 1/22/98 return if sound is not enabled
  if (nosfxparm)
    return;

  for (cnum=0 ; cnum<numChannels ; cnum++)
    if (channels[cnum].active && channels[cnum].origin == origin)
      {
        S_StopChannel(cnum);
        break;
      }
}

void S_StopSoundLoops(void)
{
  int cnum;

  if (nosfxparm)
    return;

  for (cnum = 0; cnum < numChannels; ++cnum)
    if (channels[cnum].active && channels[cnum].loop)
      S_StopChannel(cnum);
}

// [FG] disable sound cutoffs
int full_sounds;

void S_UnlinkSound(void *origin)
{
  int cnum;

  //jff 1/22/98 return if sound is not enabled
  if (nosfxparm)
    return;

  if (origin)
  {
    for (cnum = 0; cnum < numChannels; cnum++)
    {
      if (channels[cnum].active && channels[cnum].origin == origin)
      {
        degenmobj_t *const sobj = &sobjs[cnum];
        const mobj_t *const mobj = (mobj_t *) origin;
        sobj->x = mobj->x;
        sobj->y = mobj->y;
        sobj->z = mobj->z;
        channels[cnum].origin = (mobj_t *) sobj;
        break;
      }
    }
  }
}

//
// Stop and resume music, during game PAUSE.
//
void S_PauseSound(void)
{
  //jff 1/22/98 return if music is not enabled
  if (nomusicparm)
    return;

  if (mus_playing && !mus_paused)
    {
      mus_paused = true;
    }
}

void S_ResumeSound(void)
{
  //jff 1/22/98 return if music is not enabled
  if (nomusicparm)
    return;

  if (mus_playing && mus_paused)
    {
      mus_paused = false;
    }
}


//
// Updates music & sounds
//
void S_UpdateSounds(void){}

// Starts some music with the music id found in sounds.h.
//
void S_StartMusic(int m_id)
{
}

dboolean S_ChangeMusicByName(const char *name, dboolean looping)
{
    return false;
}

void S_ChangeMusic(int musicnum, int looping)
{
}

void S_RestartMusic(void)
{
}

void S_ChangeMusInfoMusic(int lumpnum, int looping)
{
}

void S_StopMusic(void)
{
}



void S_StopChannel(int cnum)
{
}

//
// Changes volume, stereo-separation, and pitch variables
//  from the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//

int S_AdjustSoundParams(mobj_t *listener, mobj_t *source, channel_t *channel, sfx_params_t *params){}

//
// S_getChannel :
//   If none available, return -1.  Otherwise channel #.
//

static int S_ChannelScore(channel_t *channel)
{
  return channel->priority;
}

static int S_LowestScoreChannel(void)
{
  int cnum;
  int lowest_score = INT_MAX;
  int lowest_cnum = channel_not_found;

  for (cnum = 0; cnum < numChannels; ++cnum)
  {
    int score = S_ChannelScore(&channels[cnum]);

    if (score < lowest_score)
    {
      lowest_score = score;
      lowest_cnum = cnum;
    }
  }

  return lowest_cnum;
}

static int S_getChannel(void *origin, sfxinfo_t *sfxinfo, sfx_params_t *params)
{
  // channel number to use
  int cnum;
  channel_t *c;

  //jff 1/22/98 return if sound is not enabled
  if (nosfxparm)
    return channel_not_found;

  // Only allow one sound per origin
  // Preserve the secret revealed sound, unless a new one is called
  for (cnum = 0; cnum < numChannels; cnum++)
    if (channels[cnum].active && channels[cnum].origin == origin &&
        (comp[comp_sound] || channels[cnum].sfx_class == params->sfx_class) &&
        (channels[cnum].sfx_class != sfx_class_secret || params->sfx_class == sfx_class_secret))
    {
      // The sound is already playing
      if (channels[cnum].sfxinfo == sfxinfo && channels[cnum].loop && params->loop) {
        channels[cnum].loop_timeout = params->loop_timeout;

        return channel_not_found;
      }

      S_StopChannel(cnum);
      break;
    }

  // Find an open channel
  for (cnum = 0; cnum < numChannels; cnum++)
    if (!channels[cnum].active)
      break;

  // None available
  if (cnum == numChannels)
  {      // Look for lower priority
    channel_t temp_channel;

    memset(&temp_channel, 0, sizeof(temp_channel));
    temp_channel.priority = params->priority;
    temp_channel.volume = params->volume;

    cnum = S_LowestScoreChannel();

    if (cnum == channel_not_found)
      return channel_not_found;

    if (S_ChannelScore(&temp_channel) > S_ChannelScore(&channels[cnum]))
      S_StopChannel(cnum);
    else
      return channel_not_found;
  }

  c = &channels[cnum];              // channel is decided to be cnum.
  c->sfxinfo = sfxinfo;
  c->origin = origin;
  c->sfx_class = params->sfx_class;
  return cnum;
}

// heretic

static dboolean S_StopSoundInfo(sfxinfo_t* sfx, sfx_params_t *params)
{
  int i;
  int priority;
  int least_priority;
  int found;

  if (sfx->numchannels == -1)
    return true;

  priority = params->priority;
  least_priority = -1;
  found = 0;

  for (i = 0; i < numChannels; i++)
  {
    if (channels[i].active && channels[i].sfxinfo == sfx && channels[i].origin)
    {
      found++;            //found one.  Now, should we replace it??
      if (priority >= channels[i].priority)
      {                   // if we're gonna kill one, then this'll be it
        if (!channels[i].loop || priority > channels[i].priority)
        {
          least_priority = i;
          priority = channels[i].priority;
        }
      }
    }
  }

  if (found < sfx->numchannels)
    return true;

  if (least_priority >= 0)
  {
    S_StopChannel(least_priority);

    return true;
  }

  return false; // don't replace any sounds
}

static int Raven_S_getChannel(mobj_t *listener, mobj_t *origin, sfxinfo_t *sfx, sfx_params_t *params)
{
  int i;
  static int sndcount = 0;

  for (i = 0; i < numChannels; i++)
  {
    // The sound is already playing
    if (channels[i].active &&
        channels[i].sfxinfo == sfx &&
        channels[i].origin == origin &&
        channels[i].loop && params->loop)
    {
      channels[i].loop_timeout = params->loop_timeout;

      return channel_not_found;
    }
  }

  if (!S_StopSoundInfo(sfx, params))
    return channel_not_found; // other sounds have greater priority

  for (i = 0; i < numChannels; i++)
  {
    if (gamestate != GS_LEVEL || origin == listener)
    {
      i = numChannels;
      break;              // let the player have more than one sound.
    }
    if (origin == channels[i].origin)
    {                       // only allow other mobjs one sound
      S_StopSound(channels[i].origin);
      break;
    }
  }

  if (i >= numChannels)
  {
    // TODO: can ambient sounds even reach this flow?
    if (params->ambient)
    {
      if (AmbChan != -1 && sfx->priority <= channels[AmbChan].sfxinfo->priority)
        return channel_not_found;         //ambient channel already in use

      AmbChan = -1;
    }

    for (i = 0; i < numChannels; i++)
      if (!channels[i].active)
        break;

    if (i >= numChannels)
    {
      int chan;

      //look for a lower priority sound to replace.
      sndcount++;
      if (sndcount >= numChannels)
        sndcount = 0;

      for (chan = 0; chan < numChannels; chan++)
      {
        i = (sndcount + chan) % numChannels;
        if (params->priority >= channels[i].priority)
        {
          chan = -1;  //denote that sound should be replaced.
          break;
        }
      }

      if (chan != -1)
        return channel_not_found;  //no free channels.

      S_StopChannel(i);
    }
  }

  return i;
}

static mobj_t* GetSoundListener(void)
{
  static degenmobj_t dummy_listener;

  // If we are at the title screen, the display player doesn't have an
  // object yet, so return a pointer to a static dummy listener instead.

  if (players[displayplayer].mo != NULL)
  {
    if (walkcamera.type > 1)
    {
      static mobj_t walkcamera_listener;

      walkcamera_listener.x = walkcamera.x;
      walkcamera_listener.y = walkcamera.y;
      walkcamera_listener.z = walkcamera.z;
      walkcamera_listener.angle = walkcamera.angle;

      return &walkcamera_listener;
    }

    return players[displayplayer].mo;
  }
  else
  {
    dummy_listener.x = 0;
    dummy_listener.y = 0;
    dummy_listener.z = 0;

    return (mobj_t *) &dummy_listener;
  }
}

static void Raven_S_StartSoundAtVolume(void *_origin, int sound_id, int volume, int loop_timeout)
{
  sfxinfo_t *sfx;
  mobj_t *origin;
  mobj_t *listener;
  sfx_params_t params;
  int dist;
  int cnum;
  angle_t angle;
  fixed_t absx;
  fixed_t absy;

  origin = (mobj_t *)_origin;
  listener = GetSoundListener();

  //jff 1/22/98 return if sound is not enabled
  if (nosfxparm)
    return;

  if (sound_id == heretic_sfx_None)
    return;

  if (origin == NULL)
    origin = listener;

  sfx = &S_sfx[sound_id];

  params.ambient = 0;
  params.attenuation = 0;
  params.volume_factor = 0;
  params.loop = loop_timeout > 0;
  params.loop_timeout = loop_timeout;

  // calculate the distance before other stuff so that we can throw out
  // sounds that are beyond the hearing range.
  absx = abs(origin->x - listener->x);
  absy = abs(origin->y - listener->y);
  dist = P_AproxDistance(absx, absy);
  dist >>= FRACBITS;

  if (dist >= max_snd_dist)
    return; //sound is beyond the hearing range...
  if (dist < 0)
    dist = 0;

  params.priority = sfx->priority;
  params.priority *= (10 - (dist / dist_adjust));

  params.sfx_class = sfx_class_none;

  cnum = Raven_S_getChannel(listener, origin, sfx, &params);
  if (cnum == channel_not_found)
    return;

  params.volume = (soundCurve[dist] * volume * sfx_volume * 8) >> 14;

  if (origin == listener)
    params.separation = 128;
  else
  {
    angle = R_PointToAngle2(listener->x, listener->y, origin->x, origin->y);
    if (angle <= listener->angle)
      angle += 0xffffffff;
    angle -= listener->angle;
    angle >>= ANGLETOFINESHIFT;

    // stereo separation
    params.separation = 128 - (FixedMul(S_STEREO_SWING,finesine[angle])>>FRACBITS);
  }

  if (!hexen || sfx->pitch)
  {
    params.pitch = (byte) (NORM_PITCH + (M_Random() & 7) - (M_Random() & 7));
  }
  else
  {
    params.pitch = NORM_PITCH;
  }

  channels[cnum].pitch = params.pitch;
  channels[cnum].origin = origin;
  channels[cnum].sfxinfo = sfx;
  channels[cnum].priority = params.priority;
  channels[cnum].volume = volume; // original volume, not attenuated volume
  channels[cnum].ambient = params.ambient;
  channels[cnum].attenuation = params.attenuation;
  channels[cnum].volume_factor = params.volume_factor;
  channels[cnum].loop = params.loop;
  channels[cnum].loop_timeout = params.loop_timeout;
  channels[cnum].active = true;
  if (channels[cnum].ambient) // TODO: can ambient sounds even reach this flow?
    AmbChan = cnum;
}

void S_StartAmbientSound(void *_origin, int sound_id, int volume)
{
  sfxinfo_t *sfx;
  sfx_params_t params;
  mobj_t *origin;
  mobj_t *listener;
  int i;

  origin = (mobj_t *)_origin;
  listener = GetSoundListener();

  if (nosfxparm)
    return;

  if (sound_id == heretic_sfx_None || volume == 0)
    return;

  if (origin == NULL)
    origin = listener;

  sfx = &S_sfx[sound_id];

  if (sfx_volume > 0)
    params.volume = (volume * (sfx_volume + 1) * 8) >> 7;
  else
    params.volume = 0;

  params.pitch = (byte) (NORM_PITCH - (M_Random() & 3) + (M_Random() & 3));
  params.priority = 1; // super low priority
  params.separation = 128;
  params.sfx_class = sfx_class_none;
  params.ambient = true;
  params.attenuation = 0;
  params.volume_factor = 0;
  params.loop = false;
  params.loop_timeout = 0;

  // no priority checking, as ambient sounds would be the LOWEST.
  for (i = 0; i < numChannels; i++)
    if (channels[i].origin == NULL)
      break;

  if (i >= numChannels)
    return;


  channels[i].pitch = params.pitch;
  channels[i].origin = origin;
  channels[i].sfxinfo = sfx;
  channels[i].priority = params.priority;
  channels[i].ambient = params.ambient;
  channels[i].attenuation = params.attenuation;
  channels[i].volume_factor = params.volume_factor;
  channels[i].loop = params.loop;
  channels[i].loop_timeout = params.loop_timeout;
  channels[i].active = true;
}

static void Heretic_S_StopSound(void *_origin)
{
  mobj_t *origin = _origin;
  int i;

  //jff 1/22/98 return if sound is not enabled
  if (nosfxparm)
    return;

  for (i = 0; i < numChannels; i++)
  {
    if (channels[i].active && channels[i].origin == origin)
    {
      S_StopChannel(i);
    }
  }
}

// hexen

dboolean S_GetSoundPlayingInfo(void * origin, int sound_id)
{
    int i;
    sfxinfo_t *sfx;

    return false;
}

int S_GetSoundID(const char *name)
{
    int i;

    for (i = 0; i < num_sfx; i++)
    {
        if (!strcmp(S_sfx[i].tagname, name))
        {
            return i;
        }
    }
    return 0;
}

void S_StartSongName(const char *songLump, dboolean loop)
{
    int musicnum;

    // lazy shortcut hack - this is a unique character
    switch (songLump[1])
    {
      case 'e':
        musicnum = hexen_mus_hexen;
        break;
      case 'u':
        musicnum = hexen_mus_hub;
        break;
      case 'a':
        musicnum = hexen_mus_hall;
        break;
      case 'r':
        musicnum = hexen_mus_orb;
        break;
      case 'h':
        musicnum = hexen_mus_chess;
        break;
      default:
        musicnum = hexen_mus_hub;
        break;
    }

    S_ChangeMusic(musicnum, loop);
}

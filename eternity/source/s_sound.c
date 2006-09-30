// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// DESCRIPTION:  Platform-independent sound code
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: s_sound.c,v 1.11 1998/05/03 22:57:06 killough Exp $";

// killough 3/7/98: modified to allow arbitrary listeners in spy mode
// killough 5/2/98: reindented, removed useless code, beautified

#include "z_zone.h"
#include "doomstat.h"
#include "d_main.h"
#include "s_sound.h"
#include "i_sound.h"
#include "r_main.h"
#include "m_random.h"
#include "w_wad.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "p_info.h"
#include "d_io.h" // SoM 3/14/2002: strncasecmp
#include "d_gi.h"
#include "a_small.h"
#include "e_sound.h"
#include "m_queue.h"
#include "p_spec.h"

// haleyjd 07/13/05: redefined to use sound-specific attenuation params
#define S_ATTENUATOR ((sfx->clipping_dist - sfx->close_dist) >> FRACBITS)

// Adjustable by menu.
#define NORM_PITCH 128
#define NORM_PRIORITY 64
#define NORM_SEP 128
#define S_STEREO_SWING (96<<FRACBITS)

// sf: sound/music hashing
#define SOUND_HASHSLOTS 257
// use sound_hash for music hash too

static d_inline int sound_hash(const char *s)
{
   int i = 0, hash;
   const char *t = s;

   hash = toupper(*t);
   t++;

   while(*t && i < 8)
   {
      hash = hash * 3 + toupper(*t);
      i++; t++;
   }

   return hash % SOUND_HASHSLOTS;
}

//jff 1/22/98 make sound enabling variables readable here
extern int snd_card, mus_card;
extern boolean nosfxparm, nomusicparm;
//jff end sound enabling variables readable here

typedef struct channel_s
{
  sfxinfo_t *sfxinfo;      // sound information (if null, channel avail.)
  const mobj_t *origin;    // origin of sound
  int volume;              // volume scale value for effect -- haleyjd 05/29/06
  soundattn_e attenuation; // attenuation type -- haleyjd 05/29/06
  int pitch;               // pitch modifier -- haleyjd 06/03/06
  int handle;              // handle of the sound being played
  int o_priority;          // haleyjd 09/27/06: stored priority value
  int priority;            // current priority value
  int singularity;         // haleyjd 09/27/06: stored singularity value
  int idnum;               // haleyjd 09/30/06: unique id num for sound event
} channel_t;

// the set of channels available
static channel_t *channels;

// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
int snd_SfxVolume = 15;

// Maximum volume of music.
int snd_MusicVolume = 15;

// precache sounds ?
int s_precache = 1;

// whether songs are mus_paused
static boolean mus_paused;

// music currently being played
static musicinfo_t *mus_playing;

// following is set
//  by the defaults code in M_misc:
// number of channels available
int numChannels;
int default_numChannels;  // killough 9/98

//jff 3/17/98 to keep track of last IDMUS specified music num
int idmusnum;

// sf:
// haleyjd: sound hashing is now kept up by EDF
//sfxinfo_t *sfxinfos[SOUND_HASHSLOTS];
musicinfo_t *musicinfos[SOUND_HASHSLOTS];

//
// Internals.
//

//
// S_StopChannel
//
// Stops a sound channel.
//
static void S_StopChannel(int cnum)
{
   if(channels[cnum].sfxinfo)
   {
      if(I_SoundIsPlaying(channels[cnum].handle))
         I_StopSound(channels[cnum].handle);      // stop the sound playing
      
      // haleyjd 09/27/06: clear the entire channel
      memset(&channels[cnum], 0, sizeof(channel_t));
   }
}

static boolean S_CheckSectorKill(camera_t *listener, const mobj_t *source)
{
   // haleyjd 05/29/06: moved up to here and fixed a major bug
   // are we in a killed-sound sector?   
   if(gamestate == GS_LEVEL)
   { 
      if(listener && 
         R_PointInSubsector(listener->x, listener->y)->sector->special & SF_KILLSOUND)
         return true;
      
      // source in a killed-sound sector?
      if(source &&
         R_PointInSubsector(source->x, source->y)->sector->special & SF_KILLSOUND)
         return true;
   }

   return false;
}

//
// S_AdjustSoundParams
//
// Alters a playing sound's volume and stereo separation to account for
// the position and angle of the listener relative to the source.
//
// sf: listener now a camera_t for external cameras
// haleyjd: added sfx parameter for custom attenuation data
// haleyjd: added channel volume scale value
// haleyjd: added priority scaling
//
static int S_AdjustSoundParams(camera_t *listener, const mobj_t *source,
                               int chanvol, int chanattn, int *vol, int *sep,
                               int *pitch, int *pri, sfxinfo_t *sfx)
{
   fixed_t adx = 0, ady = 0, dist = 0;
   angle_t angle;
   fixed_t sx, sy;
   int attenuator, basevolume;        // haleyjd
   fixed_t close_dist, clipping_dist; // haleyjd

   // haleyjd 08/12/04: we cannot adjust a sound for a NULL listener.
   if(!listener)
      return 1;

   // haleyjd 05/29/06: this function isn't supposed to be called for NULL sources
#ifdef RANGECHECK
   if(!source)
      I_Error("S_AdjustSoundParams: NULL source\n");
#endif
   
   // calculate the distance to sound origin
   //  and clip it if necessary
   //
   // killough 11/98: scale coordinates down before calculations start
   // killough 12/98: use exact distance formula instead of approximation

   sx = source->x;
   sy = source->y;
      
#ifdef R_LINKEDPORTALS
   if(useportalgroups && listener->groupid != R_NOGROUP && 
      source->groupid != R_NOGROUP && listener->groupid != source->groupid)
   {
      // The listener and the source are not in the same subspace, so offset
      // the sound origin so it will sound correct to the player.
      linkoffset_t *link = P_GetLinkOffset(listener->groupid, source->groupid);
      if(link)
      {
         sx += link->x;
         sy += link->y;
      }
   }
#endif

   adx = D_abs((listener->x >> FRACBITS) - (sx >> FRACBITS));
   ady = D_abs((listener->y >> FRACBITS) - (sy >> FRACBITS));
   
   if(ady > adx)
      dist = adx, adx = ady, ady = dist;

   dist = adx ? FixedDiv(adx, finesine[(tantoangle[FixedDiv(ady,adx) >> DBITS]
                                        + ANG90) >> ANGLETOFINESHIFT]) : 0;   
   
   // haleyjd 05/29/06: allow per-channel volume scaling
   //basevolume = (snd_SfxVolume * chanvol) / 127;
   basevolume = (snd_SfxVolume * chanvol) / 15;

   // haleyjd 05/30/06: allow per-channel attenuation behavior
   switch(chanattn)
   {
   case ATTN_NORMAL: // use sfxinfo_t data
      attenuator = S_ATTENUATOR;
      close_dist = sfx->close_dist;
      clipping_dist = sfx->clipping_dist;
      break;
   case ATTN_IDLE: // use DOOM's original values
      attenuator = S_CLIPPING_DIST_I - S_CLOSE_DIST_I;
      close_dist = S_CLOSE_DIST;
      clipping_dist = S_CLIPPING_DIST;
      break;
   case ATTN_STATIC: // fade fast
      attenuator = 448;
      close_dist = 64 << FRACBITS;
      clipping_dist = 512 << FRACBITS;
      break;
   case ATTN_NONE: // Moooo! Useful for ambient sounds.
      attenuator = close_dist = clipping_dist = 0;
      break;
   }

   // killough 11/98:   handle zero-distance as special case
   // haleyjd 07/13/05: handle case of zero-or-less attenuation as well
   if(!dist || attenuator <= 0)
   {
      *sep = NORM_SEP;
      *vol = basevolume; // haleyjd 05/29/06: use scaled basevolume
      return *vol > 0;
   }

   if(dist > clipping_dist >> FRACBITS)
      return 0;

   // angle of source to listener
   // sf: use listenx, listeny
   
   angle = R_PointToAngle2(listener->x, listener->y, sx, sy);

   if(angle <= listener->angle)
      angle += 0xffffffff;
   angle -= listener->angle;
   angle >>= ANGLETOFINESHIFT;

   // stereo separation
   *sep = NORM_SEP - FixedMul(S_STEREO_SWING >> FRACBITS, finesine[angle]);
   
   // volume calculation
   *vol = dist < close_dist >> FRACBITS ? basevolume :
      basevolume * ((clipping_dist >> FRACBITS) - dist) / attenuator;

   // haleyjd 09/27/06: decrease priority with volume attenuation
   *pri = *pri + (127 - *vol);
   
   if(*pri > 255) // cap to 255
      *pri = 255;

   return *vol > 0;
}

//
// S_getChannel :
//
//   If none available, return -1.  Otherwise channel #.
//   haleyjd 09/27/06: fixed priority/singularity bugs
//   Note that a higher priority number means lower priority!
//
static int S_getChannel(const mobj_t *origin, sfxinfo_t *sfxinfo,
                        int priority, int singularity)
{
   // channel number to use
   int cnum;
   int lowestpriority = -1; // haleyjd
   int lpcnum = -1;

   // haleyjd 09/28/06: moved this here. If we kill a sound already
   // being played, we can use that channel. There is no need to
   // search for a free one again because we already know of one.

   // kill old sound
   // killough 12/98: replace is_pickup hack with singularity flag
   for(cnum = 0; cnum < numChannels; ++cnum)
   {
      if(channels[cnum].sfxinfo &&
         channels[cnum].singularity == singularity &&
         channels[cnum].origin == origin)
      {
         S_StopChannel(cnum);
         break;
      }
   }
   
   // Find an open channel
   if(cnum == numChannels)
   {
      // haleyjd 09/28/06: it isn't necessary to look for playing sounds in
      // the same singularity class again, as we just did that above. Here
      // we are looking for an open channel. We will also keep track of the
      // channel found with the lowest sound priority while doing this.
      for(cnum = 0; cnum < numChannels && channels[cnum].sfxinfo; ++cnum)
      {
         if(channels[cnum].priority > lowestpriority)
         {
            lowestpriority = channels[cnum].priority;
            lpcnum = cnum;
         }
      }
   }

   // None available?
   if(cnum == numChannels)
   {
      // Look for lower priority
      // haleyjd: we have stored the channel found with the lowest priority
      // in the loop above
      if(priority > lowestpriority)
         return -1;                  // No lower priority.  Sorry, Charlie.
      else
      {
         S_StopChannel(lpcnum);      // Otherwise, kick out lowest priority.
         cnum = lpcnum;
      }
   }
   
   return cnum;
}

//
// S_StartSfxInfo
//
// The main sound starting function.
// haleyjd 05/29/06: added volume scaling value. Allows sounds to be
// started and to persist at differing volume levels. volumeScale should
// range from 0 to 127. Also added customizable attenuation types.
// haleyjd 06/03/06: added ability to loop sound samples
//
void S_StartSfxInfo(const mobj_t *origin, sfxinfo_t *sfx, 
                    int volumeScale, soundattn_e attenuation, boolean loop)
{
   int sep, pitch, o_priority, priority, singularity, cnum, handle;
   int volume = snd_SfxVolume;
   boolean extcamera = false;
   camera_t playercam;

   // haleyjd 09/03/03: allow NULL sounds to fall through
   if(!sfx)
      return;
   
   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;

   // haleyjd 09/24/06: Sound aliases. These are similar to links, but we skip
   // through them now, up here, instead of below. This allows aliases to simply
   // serve as alternate names for the same sounds, in contrast to links which
   // provide a way of playing the same sound with different parameters.

   while(sfx->alias)
      sfx = sfx->alias;

   // haleyjd:  we must weed out degenmobj_t's before trying to 
   // dereference these fields -- a thinker check perhaps?

   // haleyjd: monster skins don't support sound replacements

   if(sfx->skinsound) // check for skin sounds
   {
      char *sndname = "";

      if(origin && 
         origin->thinker.function == P_MobjThinker &&       // haleyjd
         origin->skin && origin->skin->type == SKIN_PLAYER) // haleyjd
      {         
         sndname = origin->skin->sounds[sfx->skinsound - 1];
         sfx = S_SfxInfoForName(sndname);
      }
      
      if(!sfx)
      {
         doom_printf(FC_ERROR "S_StartSfxInfo: skin sound %s not found\n",
                     sndname);
         return;
      }
   }

   // Initialize sound parameters
   if(sfx->link)
   {
      pitch        = sfx->pitch;
      volumeScale += sfx->volume;   // haleyjd: now modifies volumeScale
   }
   else
      pitch = NORM_PITCH;

   // haleyjd 09/29/06: rangecheck volumeScale now!
   if(volumeScale < 0)
      volumeScale = 0;
   else if(volumeScale > 127)
      volumeScale = 127;
   
   // haleyjd: modified so that priority value is always used
   // haleyjd: also modified to get and store proper singularity value
   o_priority = priority = sfx->priority;
   singularity = sfx->singularity;

   // haleyjd: setup playercam
   if(gamestate == GS_LEVEL)
   {     
      if(camera) // an external camera is active
      {
         playercam = *camera; // assign directly
         extcamera = true;
      }
      else
      {
         mobj_t *mo = players[displayplayer].mo;
         playercam.x = mo->x; 
         playercam.y = mo->y; 
         playercam.z = mo->z;
         playercam.angle = mo->angle;
#ifdef R_LINKEDPORTALS
         playercam.groupid = mo->groupid;
#endif
      }
   }

   // haleyjd 09/29/06: check for sector sound kill here.
   if(S_CheckSectorKill(&playercam, origin))
      return;

   // Check to see if it is audible, modify the params
   // killough 3/7/98, 4/25/98: code rearranged slightly
   // haleyjd 08/12/04: add extcamera check
   
   if(!origin || (!extcamera && origin == players[displayplayer].mo))
   {
      sep = NORM_SEP;
      volume = (volume * volumeScale) / 15; // haleyjd 05/29/06: scale volume
      if(volume < 1)
         return;
      if(volume > 127)
         volume = 127;
   }
   else
   {     
      // use an external cam?
      if(!S_AdjustSoundParams(&playercam, origin, volumeScale, attenuation,
                              &volume, &sep, &pitch, &priority, sfx))
         return;
      else if(origin->x == playercam.x && origin->y == playercam.y)
         sep = NORM_SEP;
   }
  
   if(pitched_sounds)
   {
      switch(sfx->pitch_type)
      {
      case pitch_doom:
         pitch += 16 - (M_Random() & 31);
         break;
      case pitch_doomsaw:
         pitch += 8 - (M_Random() & 15);
         break;
      case pitch_heretic:
         pitch += M_Random() & 31;
         pitch -= M_Random() & 31;
         break;
      case pitch_hticamb:
         pitch += M_Random() & 15;
         pitch -= M_Random() & 15;
         break;
      case pitch_none:
      default:
         break;
      }

      if(pitch < 0)
         pitch = 0;
      
      if(pitch > 255)
         pitch = 255;
   }

   // try to find a channel
   if((cnum = S_getChannel(origin, sfx, priority, singularity)) < 0)
      return;

   channels[cnum].sfxinfo = sfx;
   channels[cnum].origin  = origin;

   while(sfx->link)
      sfx = sfx->link;     // sf: skip thru link(s)

   // Assigns the handle to one of the channels in the mix/output buffer.
   handle = I_StartSound(sfx, cnum, volume, sep, pitch, priority, loop);

   // haleyjd: check to see if the sound was started
   if(handle >= 0)
   {
      channels[cnum].handle = handle;
      
      // haleyjd 05/29/06: record volume scale value and attenuation type
      // haleyjd 06/03/06: record pitch too (wtf is going on here??)
      // haleyjd 09/27/06: store priority and singularity values (!!!)
      channels[cnum].volume      = volumeScale;
      channels[cnum].attenuation = attenuation;
      channels[cnum].pitch       = pitch;
      channels[cnum].o_priority  = o_priority;  // original priority
      channels[cnum].priority    = priority;    // scaled priority
      channels[cnum].singularity = singularity;
      
      channels[cnum].idnum = I_SoundID(handle);
   }
   else // haleyjd: the sound didn't start, so clear the channel info
      memset(&channels[cnum], 0, sizeof(channel_t));
}

//
// S_StartSoundAtVolume
//
// haleyjd 05/29/06: Actually, DOOM had a routine named this, but it was
// removed, apparently by the BOOM team, because it was never used for 
// anything useful (it was always called with snd_SfxVolume...).
//
void S_StartSoundAtVolume(const mobj_t *origin, int sfx_id, 
                          int volume, soundattn_e attn)
{
   // haleyjd: changed to use EDF DeHackEd number hashing,
   // to enable full use of dynamically defined sounds ^_^
   sfxinfo_t *sfx = E_SoundForDEHNum(sfx_id);

   // ignore any invalid sounds
   if(!sfx)
      return;

   S_StartSfxInfo(origin, sfx, volume, attn, false);
}

//
// S_StartSound
//
// The classic DOOM sound routine, which now accepts an EDF-defined sound
// DeHackEd number. This allows it to extend to all sounds seamlessly yet
// retains full compatibility.
// haleyjd 05/29/06: reimplemented in terms of the above function.
//
void S_StartSound(const mobj_t *origin, int sfx_id)
{
   S_StartSoundAtVolume(origin, sfx_id, 127, ATTN_NORMAL);
}

//
// S_StartSoundNameAtVolume
//
// haleyjd 05/29/06: as below, but allows volume scaling.
//
void S_StartSoundNameAtVolume(const mobj_t *origin, char *name, 
                              int volume, soundattn_e attn)
{
   sfxinfo_t *sfx;
   
   // haleyjd 03/17/03: allow NULL sound names to fall through
   if(!name)
      return;

   if((sfx = S_SfxInfoForName(name)))
      S_StartSfxInfo(origin, sfx, volume, attn, false);
}

//
// S_StartSoundName
//
// Starts a sound by its EDF mnemonic. This is used by some newer
// engine features, particularly ones that use implicitly defined
// WAD sounds.
// haleyjd 05/29/06: reimplemented in terms of the above function.
//
void S_StartSoundName(const mobj_t *origin, char *name)
{
   S_StartSoundNameAtVolume(origin, name, 127, ATTN_NORMAL);
}

//
// S_StartSoundLooped
//
// haleyjd 06/03/06: support playing looped sounds.
//
void S_StartSoundLooped(const mobj_t *origin, char *name, int volume, 
                        soundattn_e attn)
{
   sfxinfo_t *sfx;
   
   if(!name)
      return;

   if((sfx = S_SfxInfoForName(name)))
      S_StartSfxInfo(origin, sfx, volume, attn, true);
}

//
// S_StopSound
//
void S_StopSound(const mobj_t *origin)
{
   int cnum;
   
   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;

   for(cnum = 0; cnum < numChannels; ++cnum)
   {
      if(channels[cnum].sfxinfo && channels[cnum].origin == origin &&
         channels[cnum].idnum == I_SoundID(channels[cnum].handle))
      {
         S_StopChannel(cnum);
         break;
      }
   }
}

//
// S_PauseSound
//
// Stop music, during game PAUSE.
//
void S_PauseSound(void)
{
   if(mus_playing && !mus_paused)
   {
      I_PauseSong(mus_playing->handle);
      mus_paused = true;
   }
}

//
// S_ResumeSound
//
// Start music back up.
//
void S_ResumeSound(void)
{
   if(mus_playing && mus_paused)
   {
      I_ResumeSong(mus_playing->handle);
      mus_paused = false;
   }
}

//
// S_UpdateSounds
//
// Called from main loop. Updates digital sound positional effects.
//
void S_UpdateSounds(const mobj_t *listener)
{
   int cnum;
   camera_t playercam; // sf: a camera_t holding the information about the player

   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;

   if(listener)
   {
      // haleyjd 08/12/04: fix possible bugs with external cameras
      
      if(camera) // an external camera is active
      {
         playercam = *camera; // assign directly
      }
      else
      {
         playercam.x = listener->x;
         playercam.y = listener->y;
         playercam.z = listener->z;
         playercam.angle = listener->angle;
#ifdef R_LINKEDPORTALS
         playercam.groupid = listener->groupid;
#endif
      }      
   }

   // now update each individual channel
   for(cnum = 0; cnum < numChannels; ++cnum)
   {
      channel_t *c = &channels[cnum];
      sfxinfo_t *sfx = c->sfxinfo;

      if(c->idnum != I_SoundID(c->handle))
      {
         memset(c, 0, sizeof(channel_t));
         continue;
      }

      if(sfx)
      {
         if(I_SoundIsPlaying(c->handle))
         {
            // initialize parameters
            int volume = snd_SfxVolume; // haleyjd: this gets scaled below.
            int pitch = c->pitch; // haleyjd 06/03/06: use channel's pitch!
            int sep = NORM_SEP;
            int pri = c->o_priority; // haleyjd 09/27/06: priority

            // check non-local sounds for distance clipping
            // or modify their params
            
            // sf again: use external camera if there is one
            // fix afterglows bug: segv because of NULL listener

            // haleyjd 09/29/06: major bug fix. fraggle's change to remove the
            // listener != origin check here causes player sounds to be adjusted
            // inappropriately. The only reason he changed this was to get to
            // the code in S_AdjustSoundParams that checks for sector sound
            // killing. We do that here now instead.
            if(listener && S_CheckSectorKill(&playercam, c->origin))
               S_StopChannel(cnum);
            else if(c->origin && listener != c->origin) // killough 3/20/98
            {
               // haleyjd 05/29/06: allow per-channel volume scaling
               // and attenuation type selection
               if(!S_AdjustSoundParams(listener ? &playercam : NULL,
                                       c->origin,
                                       c->volume,
                                       c->attenuation,
                                       &volume, &sep, &pitch, &pri, sfx))
                  S_StopChannel(cnum);
               else
               {
                  I_UpdateSoundParams(c->handle, volume, sep, pitch);
                  c->priority = pri; // haleyjd
               }
            }
         }
         else   // if channel is allocated but sound has stopped, free it
            S_StopChannel(cnum);
      }
   }
}

void S_SetMusicVolume(int volume)
{
   //jff 1/22/98 return if music is not enabled
   if(!mus_card || nomusicparm)
      return;

#ifdef RANGECHECK
   if(volume < 0 || volume > 16)
      I_Error("Attempt to set music volume at %d", volume);
#endif

   // haleyjd: I don't think it should do this in SDL
#ifndef _SDL_VER
   I_SetMusicVolume(127);
#endif

   I_SetMusicVolume(volume);
   snd_MusicVolume = volume;
}

void S_SetSfxVolume(int volume)
{
   //jff 1/22/98 return if sound is not enabled
   if (!snd_card || nosfxparm)
      return;

#ifdef RANGECHECK
   if(volume < 0 || volume > 127)
      I_Error("Attempt to set sfx volume at %d", volume);
#endif

   snd_SfxVolume = volume;
}

// sf: created changemusicnum, not limited to original musics
// change by music number
// removed mus_new

void S_ChangeMusicNum(int musnum, int looping)
{
   musicinfo_t *music;
   
   if(musnum <= gameModeInfo->musMin || 
      musnum >= gameModeInfo->numMusic)
   {
      doom_printf(FC_ERROR "Bad music number %d\n", musnum);
      return;
   }
   
   music = &(gameModeInfo->s_music[musnum]);
   
   S_ChangeMusic(music, looping);
}

// change by name
void S_ChangeMusicName(char *name, int looping)
{
   musicinfo_t *music;
   
   music = S_MusicForName(name);
   
   if(music)
      S_ChangeMusic(music, looping);
   else
   {
      C_Printf(FC_ERROR "music not found: %s\n", name);
      S_StopMusic(); // stop music anyway
   }
}

void S_ChangeMusic(musicinfo_t *music, int looping)
{
   int lumpnum;
   char namebuf[16];

   //jff 1/22/98 return if music is not enabled
   if(!mus_card || nomusicparm)
      return;

   // same as the one playing ?
   if(mus_playing == music)
      return;  

   // shutdown old music
   S_StopMusic();

   psnprintf(namebuf, sizeof(namebuf), "%s%s", 
             gameModeInfo->musPrefix, music->name);

   lumpnum = W_CheckNumForName(namebuf);
   if(lumpnum == -1)
   {
      doom_printf(FC_ERROR"bad music name '%s'\n",
                  music->name);
      return;
   }

   // load & register it
   // haleyjd: changed to PU_STATIC
   music->data = W_CacheLumpNum(lumpnum, PU_STATIC);
   // julian: added lump length
   music->handle = I_RegisterSong(music->data, W_LumpLength(lumpnum));

   // play it
   I_PlaySong(music->handle, looping);
   
   mus_playing = music;
}

//
// Starts some music with the music id found in sounds.h.
//
void S_StartMusic(int m_id)
{
   S_ChangeMusicNum(m_id, false);
}

void S_StopMusic(void)
{
   if(!mus_playing)
      return;

   if(mus_paused)
      I_ResumeSong(mus_playing->handle);

   I_StopSong(mus_playing->handle);
   I_UnRegisterSong(mus_playing->handle);
   Z_ChangeTag(mus_playing->data, PU_CACHE);
   
   mus_playing->data = NULL;
   mus_playing = NULL;
}

void S_StopSounds(void)
{
   int cnum;
   // kill all playing sounds at start of level
   //  (trust me - a good idea)

   //jff 1/22/98 skip sound init if sound not enabled
   if(snd_card && !nosfxparm)
      for(cnum = 0; cnum < numChannels; ++cnum)
         if(channels[cnum].sfxinfo)
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
   
   S_StopSounds();
   
   //jff 1/22/98 return if music is not enabled
   if(!mus_card || nomusicparm)
      return;
   
   // start new music for the level
   mus_paused = 0;
   
   if(!*LevelInfo.musicName && gamemap == 0)
   {
      // dont know what music to play
      // we need a default
      LevelInfo.musicName = (gamemode == commercial ? "runnin" : "e1m1");
   }
   
   // sf: replacement music
   if(*LevelInfo.musicName)
   {
      S_ChangeMusicName(LevelInfo.musicName, true);
   }
   else
   {
      if(idmusnum != -1)
      {
         mnum = idmusnum; //jff 3/17/98 reload IDMUS music if not -1
      }
      else if(gameModeInfo->type == Game_Heretic)
      {
         int gep = gameepisode;
         int gmp = gamemap;

         // ensure bounds just for safety
         if(gep < 1) gep = 1;
         if(gep > 6) gep = 6;

         if(gmp < 1) gmp = 1;
         if(gmp > 9) gmp = 9;

         mnum = H_Mus_Matrix[gep - 1][gmp - 1];
      }
      else
      {
         if(gamemode == commercial)
         {
            mnum = mus_runnin + gamemap - 1;
         }
         else
         {
            static const int spmus[] =     // Song - Who? - Where?
            {
               mus_e3m4,     // American     e4m1
               mus_e3m2,     // Romero       e4m2
               mus_e3m3,     // Shawn        e4m3
               mus_e1m5,     // American     e4m4
               mus_e2m7,     // Tim          e4m5
               mus_e2m4,     // Romero       e4m6
               mus_e2m6,     // J.Anderson   e4m7 CHIRON.WAD
               mus_e2m5,     // Shawn        e4m8
               mus_e1m9      // Tim          e4m9
            };
            
            // sf: simplified
            mnum = gameepisode < 4 ?
               mus_e1m1 + (gameepisode-1)*9 + gamemap-1 :
               spmus[gamemap-1];
         }
      }
         
      // start music
      S_ChangeMusicNum(mnum, true);
   }
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void S_Init(int sfxVolume, int musicVolume)
{
   // haleyjd 09/03/03: the sound hash is now maintained by
   // EDF

   //jff 1/22/98 skip sound init if sound not enabled
   if(snd_card && !nosfxparm)
   {
      usermsg("\tdefault sfx volume %d", sfxVolume);  // killough 8/8/98

      // haleyjd 05/30/02
      I_SetChannels();

      S_SetSfxVolume(sfxVolume);

      // Allocating the internal channels for mixing
      // (the maximum numer of sounds rendered
      // simultaneously) within zone memory.

      // killough 10/98:
      channels = calloc(numChannels = default_numChannels, sizeof(channel_t));
   }

   if(s_precache)        // sf: option to precache sounds
   {
      E_PreCacheSounds();
      usermsg("\tprecached all sounds.");
   }
   else
      usermsg("\tsounds to be cached dynamically.");

   if(!mus_card || nomusicparm)
      return;

   S_SetMusicVolume(musicVolume);
   
   // no sounds are playing, and they are not mus_paused
   mus_paused = 0;
}

/////////////////////////////////////////////////////////////////////////
//
// Sound Hashing
//

sfxinfo_t *S_SfxInfoForName(char *name)
{   
   // haleyjd 09/03/03: now calls down to master EDF sound hash   
   return E_SoundForName(name);
}

//
// S_Chgun
//
// Delinks the chgun sound effect when a DSCHGUN lump has been
// detected. This allows the sound to be used separately without 
// use of EDF.
//
void S_Chgun(void)
{
   sfxinfo_t *s_chgun = E_SoundForName("chgun");

   if(!s_chgun)
      return;

   s_chgun->link = NULL;
   s_chgun->pitch = -1;
   s_chgun->volume = -1;
}

// free sound and reload
// also check to see if a new sound name has been found
// (ie. not one in the original game). If so, we create
// a new sfxinfo_t and hook it into the hashtable for use
// by scripting and skins

// NOTE: LUMPNUM NOT SOUNDNUM
void S_UpdateSound(int lumpnum)
{
   // haleyjd 09/03/03: now calls down to master EDF sound hash
   E_NewWadSound(lumpinfo[lumpnum]->name);
}

typedef struct squeueitem_s
{
   mqueueitem_t mqitem; // this must be first
   char lumpname[9];
} squeueitem_t;

static mqueue_t defsndqueue;
static boolean snd_queue_init = false;

static void S_InitDefSndQueue(void)
{
   M_QueueInit(&defsndqueue);
   snd_queue_init = true;
}

//
// S_UpdateSoundDeferred
//
// haleyjd 09/13/03: We need to defer the updating of new wad sounds
// found from W_InitMultipleFiles, otherwise EDF doesn't get a chance
// at them first.
//
void S_UpdateSoundDeferred(int lumpnum)
{
   squeueitem_t *newsq;

   if(!snd_queue_init)
      S_InitDefSndQueue();

   newsq = malloc(sizeof(squeueitem_t));
   memset(newsq, 0, sizeof(squeueitem_t));

   strncpy(newsq->lumpname, lumpinfo[lumpnum]->name, 9);

   M_QueueInsert(&(newsq->mqitem), &defsndqueue);
}

void S_ProcDeferredSounds(void)
{
   mqueueitem_t *rover;

   while((rover = M_QueueIterator(&defsndqueue)))
   {
      squeueitem_t *sqitem = (squeueitem_t *)rover;

      E_NewWadSound(sqitem->lumpname);
   }

   // free the queue
   M_QueueFree(&defsndqueue);
   snd_queue_init = false;
}

///////////////////////////////////////////////////////////////////////////
//
// Music Hashing
//

static boolean mushash_created = false;

static void S_HookMusic(musicinfo_t *music)
{
   int hashslot;
   
   if(!music || !music->name) return;
   
   hashslot = sound_hash(music->name);
   
   music->next = musicinfos[hashslot];
   musicinfos[hashslot] = music;
}

static void S_CreateMusicHashTable()
{
   int i;
   
   // only build once
   
   if(mushash_created)
      return;
   else
      mushash_created = true;

   for(i = 0; i < SOUND_HASHSLOTS; i++)
      musicinfos[i] = NULL;
   
   // hook in all musics
   for(i = 0; i < gameModeInfo->numMusic; i++)
   {
      S_HookMusic(&(gameModeInfo->s_music[i]));
   }
}

musicinfo_t *S_MusicForName(char *name)
{
   int hashnum;
   musicinfo_t *mus;

   if(!name)
      return NULL;

   hashnum = sound_hash(name);
   
   if(!mushash_created)
      S_CreateMusicHashTable();
   
   for(mus = musicinfos[hashnum]; mus; mus = mus->next)
   {
      if(!strcasecmp(name, mus->name))
         return mus;
   }
   
   return NULL;
}

void S_UpdateMusic(int lumpnum)
{
   musicinfo_t *music;
   char sndname[16];

   strncpy(sndname, lumpinfo[lumpnum]->name + 2, 6);
   
   // check if one already in the table first
   
   music = S_MusicForName(sndname);
   
   if(!music)         // not found in list
   {
      // build a new musicinfo_t
      music = Z_Malloc(sizeof(*music), PU_STATIC, 0);
      music->name = Z_Strdup(sndname, PU_STATIC, 0);
      
      // hook into hash list
      S_HookMusic(music);
   }
}

//
// S_CheckSoundPlaying
//
// haleyjd: rudimentary sound checking function
//
boolean S_CheckSoundPlaying(mobj_t *mo, sfxinfo_t *sfx)
{
   int cnum;

   if(mo && sfx)
   {   
      for(cnum = 0; cnum < numChannels; ++cnum)
      {
         if(channels[cnum].origin == mo && channels[cnum].sfxinfo == sfx)
         {
            if(I_SoundIsPlaying(channels[cnum].handle))
               return true;
         }
      }
   }
   
   return false;
}

///////////////////////////////////////////////////////////////////////////
//
// Console Commands
//

VARIABLE_BOOLEAN(s_precache,      NULL, onoff);
VARIABLE_BOOLEAN(pitched_sounds,  NULL, onoff);
VARIABLE_INT(default_numChannels, NULL, 1, 128, NULL);
VARIABLE_INT(snd_SfxVolume,       NULL, 0, 15,  NULL);
VARIABLE_INT(snd_MusicVolume,     NULL, 0, 15,  NULL);

VARIABLE_BOOLEAN(forceFlipPan,    NULL, onoff);

CONSOLE_VARIABLE(s_precache, s_precache, 0) {}
CONSOLE_VARIABLE(s_pitched, pitched_sounds, 0) {}
CONSOLE_VARIABLE(snd_channels, default_numChannels, 0) {}
CONSOLE_VARIABLE(sfx_volume, snd_SfxVolume, 0)
{
  S_SetSfxVolume(snd_SfxVolume);
}
CONSOLE_VARIABLE(music_volume, snd_MusicVolume, 0)
{
  S_SetMusicVolume(snd_MusicVolume);
}

// haleyjd 12/08/01: allow user to force reversal of audio channels
CONSOLE_VARIABLE(s_flippan, forceFlipPan, 0) {}

void S_AddCommands()
{
  C_AddCommand(s_pitched);
  C_AddCommand(s_precache);
  C_AddCommand(snd_channels);
  C_AddCommand(sfx_volume);
  C_AddCommand(music_volume);
  C_AddCommand(s_flippan);
}

//
// Small native functions
//

static cell AMX_NATIVE_CALL sm_globalsound(AMX *amx, cell *params)
{
   int err;
   char *sndname;

   // get sound name
   if((err = A_GetSmallString(amx, &sndname, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return 0;
   }

   S_StartSoundName(NULL, sndname);

   Z_Free(sndname);

   return 0;
}

static cell AMX_NATIVE_CALL sm_glblsoundnum(AMX *amx, cell *params)
{
   int soundnum = (int)params[1];

   S_StartSound(NULL, soundnum);

   return 0;
}

static cell AMX_NATIVE_CALL sm_sectorsound(AMX *amx, cell *params)
{
   int err, tag, secnum = -1;
   char *sndname;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   if((err = A_GetSmallString(amx, &sndname, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return 0;
   }

   tag = params[2];

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sector_t *sector = &sectors[secnum];
      
      S_StartSoundName((mobj_t *)&sector->soundorg, sndname);
   }

   Z_Free(sndname);

   return 0;
}

static cell AMX_NATIVE_CALL sm_sectorsoundnum(AMX *amx, cell *params)
{
   int tag, tagtype, sndnum;

   sndnum  = params[1];
   tag     = params[2];
   tagtype = params[3];

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   if(!tagtype) // 0 == find sector by tag
   {
      int secnum = -1;

      while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
      {
         sector_t *sector = &sectors[secnum];
         
         S_StartSound((mobj_t *)&sector->soundorg, sndnum);
      }
   }

   // TODO: find sector by ExtraData SID

   return 0;
}

AMX_NATIVE_INFO sound_Natives[] =
{
   { "_SoundGlobal",    sm_globalsound },
   { "_SoundGlobalNum", sm_glblsoundnum },
   { "_SectorSound",    sm_sectorsound },
   { "_SectorSoundNum", sm_sectorsoundnum },
   { NULL, NULL }
};

//----------------------------------------------------------------------------
//
// $Log: s_sound.c,v $
// Revision 1.11  1998/05/03  22:57:06  killough
// beautification, #include fix
//
// Revision 1.10  1998/04/27  01:47:28  killough
// Fix pickups silencing player weapons
//
// Revision 1.9  1998/03/23  03:39:12  killough
// Fix spy-mode sound effects
//
// Revision 1.8  1998/03/17  20:44:25  jim
// fixed idmus non-restore, space bug
//
// Revision 1.7  1998/03/09  07:32:57  killough
// ATTEMPT to support hearing with displayplayer's hears
//
// Revision 1.6  1998/03/04  07:46:10  killough
// Remove full-volume sound hack from MAP08
//
// Revision 1.5  1998/03/02  11:45:02  killough
// Make missing sounds non-fatal
//
// Revision 1.4  1998/02/02  13:18:48  killough
// stop incorrect looping of music (e.g. bunny scroll)
//
// Revision 1.3  1998/01/26  19:24:52  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/23  01:50:49  jim
// Added music/sound options, and enables
//
// Revision 1.1.1.1  1998/01/19  14:03:04  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

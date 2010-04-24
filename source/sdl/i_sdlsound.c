// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley, Stephen McGranahan, Julian Aubourg, et al.
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//      SDL sound system interface
//
//-----------------------------------------------------------------------------

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_thread.h"
#include "SDL_mixer.h"

#include "../z_zone.h"
#include "../d_io.h"
#include "../c_runcmd.h"
#include "../c_io.h"
#include "../doomstat.h"
#include "../i_sound.h"
#include "../i_system.h"
#include "../w_wad.h"
#include "../g_game.h"     //jff 1/21/98 added to use dprintf in I_RegisterSong
#include "../d_main.h"
#include "../v_misc.h"
#include "../m_argv.h"
#include "../d_gi.h"
#include "../s_sound.h"
#include "../mn_engin.h"

extern boolean snd_init;

// Needed for calling the actual sound output.
static int SAMPLECOUNT = 512;
#define MAX_CHANNELS 32

// MWM 2000-01-08: Sample rate in samples/second
// haleyjd 10/28/05: updated for Julian's music code, need full quality now
static int snd_samplerate = 44100;

typedef struct channel_info_s
{
  // SFX id of the playing sound effect.
  // Used to catch duplicates (like chainsaw).
  sfxinfo_t *id;
  // The channel step amount...
  unsigned int step;
  // ... and a 0.16 bit remainder of last step.
  unsigned int stepremainder;
  unsigned int samplerate;
  // The channel data pointers, start and end.
  unsigned char *data;
  unsigned char *startdata; // haleyjd
  unsigned char *enddata;
  // Hardware left and right channel volume lookup.
  int *leftvol_lookup;
  int *rightvol_lookup;
  // haleyjd 10/02/08: channel is waiting to be stopped
  boolean stopChannel;
  // haleyjd 06/03/06: looping
  int loop;
  unsigned int idnum;

  // haleyjd 10/02/08: SDL semaphore to protect channel
  SDL_sem *semaphore;

} channel_info_t;

static channel_info_t channelinfo[MAX_CHANNELS+1];

// Pitch to stepping lookup, unused.
static int steptable[256];

// Volume lookups.
static int vol_lookup[128*256];

static int I_GetSfxLumpNum(sfxinfo_t *sfx);

//
// stopchan
//
// cph 
// Stops a sound, unlocks the data 
//
static void stopchan(int handle)
{
   int cnum;
   boolean freeSound = true;
   sfxinfo_t *sfx;
   
#ifdef RANGECHECK
   // haleyjd 02/18/05: bounds checking
   if(handle < 0 || handle >= MAX_CHANNELS)
      return;
#endif
   
   // haleyjd 10/02/08: critical section
   if(SDL_SemWait(channelinfo[handle].semaphore) == 0)
   {
      // haleyjd 06/07/09: store this here so that we can release the
      // semaphore as quickly as possible.
      sfx = channelinfo[handle].id;

      channelinfo[handle].stopChannel = false;

      if(channelinfo[handle].data)
      {
         // haleyjd 06/07/09: bug fix!
         // this channel isn't interested in the sound any more, 
         // even if we didn't free it. This prevented some sounds from
         // getting freed unnecessarily.
         channelinfo[handle].id    = NULL;
         channelinfo[handle].data  = NULL;
         channelinfo[handle].idnum = 0;
      }

      // haleyjd 06/07/09: release the semaphore now. The faster the better.
      SDL_SemPost(channelinfo[handle].semaphore);
         
      if(sfx)
      {
         // haleyjd 06/03/06: see if we can free the sound
         for(cnum = 0; cnum < MAX_CHANNELS; ++cnum)
         {
            if(cnum == handle)
               continue;
            if(channelinfo[cnum].id &&
               channelinfo[cnum].id->data == sfx->data)
            {
               freeSound = false; // still being used by some channel
               break;
            }
         }
         
         // set sample to PU_CACHE level
         if(freeSound)
            Z_ChangeTag(sfx->data, PU_CACHE);
      }
   }
}

#define SOUNDHDRSIZE 8

//
// addsfx
//
// This function adds a sound to the
//  list of currently active sounds,
//  which is maintained as a given number
//  (eight, usually) of internal channels.
// Returns a handle.
//
// haleyjd: needs to take a sfxinfo_t ptr, not a sound id num
// haleyjd 06/03/06: changed to return boolean for failure or success
//
static boolean addsfx(sfxinfo_t *sfx, int channel, int loop, unsigned int id)
{
   size_t lumplen;
   int lump;

#ifdef RANGECHECK
   if(channel < 0 || channel >= MAX_CHANNELS)
      I_Error("addsfx: channel out of range!\n");
#endif

   // haleyjd 02/18/05: null ptr check
   if(!snd_init || !sfx)
      return false;

   stopchan(channel);
   
   // We will handle the new SFX.
   // Set pointer to raw data.

   // haleyjd: Eternity sfxinfo_t does not have a lumpnum field
   lump = I_GetSfxLumpNum(sfx);
   
   // replace missing sounds with a reasonable default
   if(lump == -1)
      lump = W_GetNumForName(GameModeInfo->defSoundName);
   
   lumplen = W_LumpLength(lump);
   
   // haleyjd 10/08/04: do not play zero-length sound lumps
   if(lumplen <= SOUNDHDRSIZE)
      return false;

   // haleyjd 06/03/06: rewrote again to make sound data properly freeable
   if(sfx->data == NULL)
   {   
      byte *data;
      Uint32 samplerate, samplelen;

      // haleyjd: this should always be called (if lump is already loaded,
      // W_CacheLumpNum handles that for us).
      data = (byte *)W_CacheLumpNum(lump, PU_STATIC);

      // Check the header, and ensure this is a valid sound
      if(data[0] != 0x03 || data[1] != 0x00)
      {
         Z_ChangeTag(data, PU_CACHE);
         return false;
      }

      samplerate = (data[3] << 8) | data[2];
      samplelen  = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

      // don't play sounds that think they're longer than they really are
      if(samplelen > lumplen - SOUNDHDRSIZE)
      {
         Z_ChangeTag(data, PU_CACHE);
         return false;
      }

      sfx->alen = (Uint32)(((Uint64)samplelen * snd_samplerate) / samplerate);
      sfx->data = Z_Malloc(sfx->alen, PU_STATIC, &sfx->data);

      // haleyjd 04/23/08: Convert sound to target samplerate
      if(sfx->alen != samplelen)
      {  
         unsigned int i;
         byte *dest = (byte *)sfx->data;
         byte *src  = data + SOUNDHDRSIZE;
         
         unsigned int step = (samplerate << 16) / snd_samplerate;
         unsigned int stepremainder = 0, j = 0;

         // do linear filtering operation
         for(i = 0; i < sfx->alen && j < samplelen - 1; ++i)
         {
            int d = (((unsigned int)src[j  ] * (0x10000 - stepremainder)) +
                     ((unsigned int)src[j+1] * stepremainder)) >> 16;

            if(d > 255)
               dest[i] = 255;
            else if(d < 0)
               dest[i] = 0;
            else
               dest[i] = (byte)d;

            stepremainder += step;
            j += (stepremainder >> 16);

            stepremainder &= 0xffff;
         }
         // fill remainder (if any) with final sample byte
         for(; i < sfx->alen; ++i)
            dest[i] = src[j];
      }
      else
      {
         // sound is already at target samplerate, copy data
         memcpy(sfx->data, data + SOUNDHDRSIZE, samplelen);
      }

      // haleyjd 06/03/06: don't need original lump data any more
      Z_ChangeTag(data, PU_CACHE);
   }
   else
      Z_ChangeTag(sfx->data, PU_STATIC); // reset to static cache level

   // haleyjd 10/02/08: critical section
   if(SDL_SemWait(channelinfo[channel].semaphore) == 0)
   {
      channelinfo[channel].data = sfx->data;
      
      // Set pointer to end of raw data.
      channelinfo[channel].enddata = (byte *)sfx->data + sfx->alen - 1;
      
      // haleyjd 06/03/06: keep track of start of sound
      channelinfo[channel].startdata = channelinfo[channel].data;
      
      channelinfo[channel].stepremainder = 0;
      
      // Preserve sound SFX id
      channelinfo[channel].id = sfx;
      
      // Set looping
      channelinfo[channel].loop = loop;
      
      // Set instance ID
      channelinfo[channel].idnum = id;

      SDL_SemPost(channelinfo[channel].semaphore);

      return true;
   }
   else
      return false; // acquisition failed
}

//
// updateSoundParams
//
// Changes sound parameters in response to stereo panning and relative location
// change.
//
static void updateSoundParams(int handle, int volume, int separation, int pitch)
{
   int slot = handle;
   int rightvol;
   int leftvol;
   int step = steptable[pitch];
   
   if(!snd_init)
      return;

#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_UpdateSoundParams: handle out of range");
#endif
   
   // Separation, that is, orientation/stereo.
   //  range is: 1 - 256
   separation += 1;

   // SoM 7/1/02: forceFlipPan accounted for here
   if(forceFlipPan)
      separation = 257 - separation;
   
   // Per left/right channel.
   //  x^2 separation,
   //  adjust volume properly.
   //volume *= 8;

   leftvol = volume - ((volume*separation*separation) >> 16);
   separation = separation - 257;
   rightvol= volume - ((volume*separation*separation) >> 16);  

   // Sanity check, clamp volume.
   if(rightvol < 0 || rightvol > 127)
      I_Error("rightvol out of bounds");
   
   if(leftvol < 0 || leftvol > 127)
      I_Error("leftvol out of bounds");

   // haleyjd 06/07/09: critical section is not needed here because this data
   // can be out of sync without affecting the sound update loop. This may cause
   // momentary out-of-sync volume between the left and right sound channels, but
   // the impact would be practically unnoticeable.

   // Set stepping
   // MWM 2000-12-24: Calculates proportion of channel samplerate
   // to global samplerate for mixing purposes.
   // Patched to shift left *then* divide, to minimize roundoff errors
   // as well as to use SAMPLERATE as defined above, not to assume 11025 Hz
   if(pitched_sounds)
      channelinfo[slot].step = step;
   else
      channelinfo[slot].step = 1 << 16;
   
   // Get the proper lookup table piece
   //  for this volume level???
   channelinfo[slot].leftvol_lookup  = &vol_lookup[leftvol*256];
   channelinfo[slot].rightvol_lookup = &vol_lookup[rightvol*256];
}

//=============================================================================
//
// Three-Band Equalization
//

typedef struct EQSTATE_s
{
  // Filter #1 (Low band)

  double  lf;       // Frequency
  double  f1p0;     // Poles ...
  double  f1p1;    
  double  f1p2;
  double  f1p3;

  // Filter #2 (High band)

  double  hf;       // Frequency
  double  f2p0;     // Poles ...
  double  f2p1;
  double  f2p2;
  double  f2p3;

  // Sample history buffer

  double  sdm1;     // Sample data minus 1
  double  sdm2;     //                   2
  double  sdm3;     //                   3

  // Gain Controls

  double  lg;       // low  gain
  double  mg;       // mid  gain
  double  hg;       // high gain
  
} EQSTATE;  

// haleyjd 04/21/10: equalizers for each stereo channel
static EQSTATE eqstate[2];

//
// rational_tanh
//
// cschueler
// http://www.musicdsp.org/showone.php?id=238
//
// Notes :
// This is a rational function to approximate a tanh-like soft clipper. It is
// based on the pade-approximation of the tanh function with tweaked 
// coefficients.
// The function is in the range x=-3..3 and outputs the range y=-1..1. Beyond
// this range the output must be clamped to -1..1.
// The first two derivatives of the function vanish at -3 and 3, so the 
// transition to the hard clipped region is C2-continuous.
//
static double rational_tanh(double x)
{
   if(x < -3)
      return -1;
   else if(x > 3)
      return 1;
   else
      return x * ( 27 + x * x ) / ( 27 + 9 * x * x );
}

//
// do_3band
//
// EQ.C - Main Source file for 3 band EQ
// http://www.musicdsp.org/showone.php?id=236
//
// (c) Neil C / Etanza Systems / 2K6
// Shouts / Loves / Moans = etanza at lycos dot co dot uk
//
// This work is hereby placed in the public domain for all purposes, including
// use in commercial applications.
// The author assumes NO RESPONSIBILITY for any problems caused by the use of
// this software.
//
static double do_3band(EQSTATE *es, double sample)
{
   //static double vsa = (1.0 / 4294967295.0);

   // Locals
   double l, m, h;    // Low / Mid / High - Sample Values

   // Filter #1 (lowpass)
   es->f1p0  += (es->lf * (sample   - es->f1p0)); // + vsa;
   es->f1p1  += (es->lf * (es->f1p0 - es->f1p1));
   es->f1p2  += (es->lf * (es->f1p1 - es->f1p2));
   es->f1p3  += (es->lf * (es->f1p2 - es->f1p3));

   l          = es->f1p3;

   // Filter #2 (highpass)
   es->f2p0  += (es->hf * (sample   - es->f2p0)); // + vsa;
   es->f2p1  += (es->hf * (es->f2p0 - es->f2p1));
   es->f2p2  += (es->hf * (es->f2p1 - es->f2p2));
   es->f2p3  += (es->hf * (es->f2p2 - es->f2p3));

   h          = es->sdm3 - es->f2p3;

   // Calculate midrange (signal - (low + high))
   //m          = es->sdm3 - (h + l);
   m          = sample - (h + l);

   // Scale, Combine and store
   l         *= es->lg;
   m         *= es->mg;
   h         *= es->hg;

   // Shuffle history buffer
   es->sdm3   = es->sdm2;
   es->sdm2   = es->sdm1;
   es->sdm1   = sample;                

   // Return result
   // haleyjd: use rational_tanh for soft clipping
   return rational_tanh(l + m + h);
}



//
// End Equalizer Code
//
//============================================================================

#define SND_PI 3.14159265

static double preampmul;

//
// I_SetChannels
//
// Init internal lookups (raw data, mixing buffer, channels).
// This function sets up internal lookups used during
//  the mixing process. 
//
static void I_SetChannels(void)
{
   int i;
   int j;
   
   int *steptablemid = steptable + 128;
   
   // Okay, reset internal mixing channels to zero.
   for(i = 0; i < MAX_CHANNELS; ++i)
      memset(&channelinfo[i], 0, sizeof(channel_info_t));
   
   // This table provides step widths for pitch parameters.
   for(i=-128 ; i<128 ; i++)
      steptablemid[i] = (int)(pow(1.2, ((double)i/(64.0)))*FPFRACUNIT);
   
   // Generates volume lookup tables
   //  which also turn the unsigned samples
   //  into signed samples.
   for(i = 0; i < 128; i++)
   {
      for(j = 0; j < 256; j++)
      {
         // proff - made this a little bit softer, because with
         // full volume the sound clipped badly (191 was 127)
         vol_lookup[i*256+j] = (i*(j-128)*256)/191;
      }
   }

   // haleyjd 10/02/08: create semaphores
   for(i = 0; i < MAX_CHANNELS; ++i)
   {
      channelinfo[i].semaphore = SDL_CreateSemaphore(1);

      if(!channelinfo[i].semaphore)
         I_Error("I_SetChannels: failed to create semaphore for channel %d\n", i);
   }

   // haleyjd 04/21/10: initialize equalizers

   // Set Low/Mid/High gains 
   // TODO: make configurable, add more parameters
   eqstate[0].lg = eqstate[1].lg = s_lowgain;
   eqstate[0].mg = eqstate[1].mg = s_midgain;
   eqstate[0].hg = eqstate[1].hg = s_highgain;

   // Calculate filter cutoff frequencies
   eqstate[0].lf = eqstate[1].lf = 2 * sin(SND_PI * (s_lowfreq  / (double)snd_samplerate));
   eqstate[0].hf = eqstate[1].hf = 2 * sin(SND_PI * (s_highfreq / (double)snd_samplerate));

   // Calculate preamplification factor
   preampmul = s_eqpreamp / 32767.0;
}

//
// I_GetSfxLumpNum
//
// Retrieve the raw data lump index
//  for a given SFX name.
//
static int I_GetSfxLumpNum(sfxinfo_t *sfx)
{
   char namebuf[16];

   memset(namebuf, 0, sizeof(namebuf));

   // haleyjd 09/03/03: determine whether to apply DS prefix to
   // name or not using new prefix flag
   if(sfx->prefix)
      psnprintf(namebuf, sizeof(namebuf), "ds%s", sfx->name);
   else
      strcpy(namebuf, sfx->name);

   return W_CheckNumForName(namebuf);
}

//=============================================================================
// 
// Driver Routines
//

//
// I_SDLUpdateEQParams
//
// haleyjd 04/21/10
//
static void I_SDLUpdateEQParams(void)
{
   // flush out state of equalizers
   memset(eqstate, 0, sizeof(eqstate));

   // Set Low/Mid/High gains 
   eqstate[0].lg = eqstate[1].lg = s_lowgain;
   eqstate[0].mg = eqstate[1].mg = s_midgain;
   eqstate[0].hg = eqstate[1].hg = s_highgain;

   // Calculate filter cutoff frequencies
   eqstate[0].lf = eqstate[1].lf = 2 * sin(SND_PI * (s_lowfreq  / (double)snd_samplerate));
   eqstate[0].hf = eqstate[1].hf = 2 * sin(SND_PI * (s_highfreq / (double)snd_samplerate));

   // Calculate preamp factor
   preampmul = s_eqpreamp / 32767.0;
}

//
// I_SDLUpdateSoundParams
//
// Update the sound parameters. Used to control volume,
// pan, and pitch changes such as when a player turns.
//
static void I_SDLUpdateSoundParams(int handle, int vol, int sep, int pitch)
{
   updateSoundParams(handle, vol, sep, pitch);
}

//
// I_SDLStartSound
//
static int I_SDLStartSound(sfxinfo_t *sound, int cnum, int vol, int sep, 
                           int pitch, int pri, int loop)
{
   static unsigned int id = 1;
   int handle;
   
   // haleyjd: turns out this is too simplistic. see below.
   /*
   // SoM: reimplement hardware channel wrap-around
   if(++handle >= MAX_CHANNELS)
      handle = 0;
   */

   // haleyjd 06/03/06: look for an unused hardware channel
   for(handle = 0; handle < MAX_CHANNELS; ++handle)
   {
      if(channelinfo[handle].data == NULL)
         break;
   }

   // all used? don't play the sound. It's preferable to miss a sound
   // than it is to cut off one already playing, which sounds weird.
   if(handle == MAX_CHANNELS)
      return -1;
 
   if(addsfx(sound, handle, loop, id))
   {
      updateSoundParams(handle, vol, sep, pitch);
      ++id; // increment id to keep each sound instance unique
   }
   else
      handle = -1;
   
   return handle;
}

//
// I_SDLStopSound
//
// Stop the sound. Necessary to prevent runaway chainsaw,
// and to stop rocket launches when an explosion occurs.
//
static void I_SDLStopSound(int handle)
{
#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SDLStopSound: handle out of range");
#endif
   
   stopchan(handle);
}

//
// I_SDLSoundIsPlaying
//
// haleyjd: wow, this can actually do something in the Windows version :P
//
static int I_SDLSoundIsPlaying(int handle)
{
#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SDLSoundIsPlaying: handle out of range");
#endif
 
   return (channelinfo[handle].data != NULL);
}

//
// I_SDLSoundID
//
// haleyjd: returns the unique id number assigned to a specific instance
// of a sound playing on a given channel. This is required to make sure
// that the higher-level sound code doesn't start updating sounds that have
// been displaced without it noticing.
//
static int I_SDLSoundID(int handle)
{
#ifdef RANGECHECK
   if(handle < 0 || handle >= MAX_CHANNELS)
      I_Error("I_SDLSoundID: handle out of range\n");
#endif

   return channelinfo[handle].idnum;
}

//
// I_SDLUpdateSound
//
// haleyjd 10/02/08: This function is now responsible for stopping channels
// that have expired. The I_SDLUpdateSound function does sound updating, but
// cannot be allowed to modify the zone heap due to being dispatched from a
// separate thread.
// 
static void I_SDLUpdateSound(void)
{
   int chan;

   for(chan = 0; chan < MAX_CHANNELS; ++chan)
   {
      if(channelinfo[chan].stopChannel == true)
         stopchan(chan);
   }
}

// size of a single sample
#define SAMPLESIZE sizeof(Sint16) 

// step to next stereo sample pair (2 samples)
#define STEP 2

//
// I_SDLUpdateSound
//
// SDL_mixer postmix callback routine. Possibly dispatched asynchronously.
// We do our own mixing on up to 32 digital sound channels.
//
static void I_SDLUpdateSoundCB(void *userdata, Uint8 *stream, int len)
{
   boolean wrotesound = false;

   // Pointers in audio stream, left, right, end.
   Sint16 *leftout, *rightout, *leftend;
   
   // haleyjd: channel pointer for speed++
   channel_info_t *chan;
         
   // Determine end, for left channel only
   //  (right channel is implicit).
   leftend = (Sint16 *)(stream + len);

   // Love thy L2 cache - made this a loop.
   // Now more channels could be set at compile time
   //  as well. Thus loop those channels.
   for(chan = channelinfo; chan != &channelinfo[MAX_CHANNELS]; ++chan)
   {
      // fast rejection before semaphore lock
      if(!chan->data || chan->stopChannel)
         continue;
         
      // try to acquire semaphore, but do not block; if the main thread is using
      // this channel we'll just skip it for now - safer and faster.
      if(SDL_SemTryWait(chan->semaphore) != 0)
         continue;
      
      // Left and right channel
      //  are in audio stream, alternating.
      leftout  = (Sint16 *)stream;
      rightout = leftout + 1;
      
      // Lost before semaphore acquired? (very unlikely, but must check for 
      // safety). BTW, don't move this up or you'll chew major CPU whenever this
      // does happen.
      if(!chan->data)
      {
         SDL_SemPost(chan->semaphore);
         continue;
      }
      
      // Mix sounds into the mixing buffer.
      // Loop over step*SAMPLECOUNT,
      //  that is 512 values for two channels.
      while(leftout != leftend)
      {
         // Mix current sound data.
         // Data, from raw sound, for right and left.
         Uint8  sample;
         Sint32 dl, dr;

         // Get the raw data from the channel. 
         // Sounds are now prefiltered.
         sample = *(chan->data);

         // Reset left/right value. 
         // Add left and right part
         //  for this channel (sound)
         //  to the current data.
         // Adjust volume accordingly.
         dl = (Sint32)(*leftout)  + chan->leftvol_lookup[sample];
         dr = (Sint32)(*rightout) + chan->rightvol_lookup[sample];
                  
         // Clamp to range. Left hardware channel.
         if(dl > SHRT_MAX)
            *leftout = SHRT_MAX;
         else if(dl < SHRT_MIN)
            *leftout = SHRT_MIN;
         else
            *leftout = (Sint16)dl;
            
         // Same for right hardware channel.
         if(dr > SHRT_MAX)
            *rightout = SHRT_MAX;
         else if(dr < SHRT_MIN)
            *rightout = SHRT_MIN;
         else
            *rightout = (Sint16)dr;

         // Increment current pointers in stream
         leftout  += STEP;
         rightout += STEP;
         
         // Increment index
         chan->stepremainder += chan->step;
         
         // MSB is next sample
         chan->data += chan->stepremainder >> 16;
         
         // Limit to LSB
         chan->stepremainder &= 0xffff;
         
         // Check whether we are done
         if(chan->data >= chan->enddata)
         {
            if(chan->loop && !paused && 
               ((!menuactive && !consoleactive) || demoplayback || netgame))
            {
               // haleyjd 06/03/06: restart a looping sample if not paused
               chan->data = chan->startdata;
               chan->stepremainder = 0;
            }
            else
            {
               // flag the channel to be stopped by the main thread ASAP
               chan->stopChannel = true;
               break;
            }
         }
      }
      
      wrotesound = true;

      // release semaphore and move on to the next channel
      SDL_SemPost(chan->semaphore);
   }

   // haleyjd 04/21/10: equalization pass
   if(s_equalizer && wrotesound)
   {
      leftout  = (Sint16 *)stream;
      rightout = leftout + 1;

      while(leftout != leftend)
      {
         double sl = (double)*leftout  * preampmul;
         double sr = (double)*rightout * preampmul;

         sl = do_3band(&eqstate[0], sl);
         sr = do_3band(&eqstate[1], sr);

         *leftout  = (Sint16)(sl * 32767.0);
         *rightout = (Sint16)(sr * 32767.0);

         leftout  += STEP;
         rightout += STEP;
      }
   }
}

//
// I_SDLSubmitSound
//
static void I_SDLSubmitSound(void)
{
   // haleyjd: whatever it is, SDL doesn't need it either
}

//
// I_SDLShutdownSound
//
// atexit handler.
//
static void I_SDLShutdownSound(void)
{
   Mix_CloseAudio();
}

//
// I_SDLCacheSound
//
// haleyjd 11/05/03: fixed for SDL sound engine
// haleyjd 09/24/06: added sound aliases
//
static void I_SDLCacheSound(sfxinfo_t *sound)
{
   int lump = I_GetSfxLumpNum(sound);
   
   // replace missing sounds with a reasonable default
   if(lump == -1)
      lump = W_GetNumForName(GameModeInfo->defSoundName);
   
   W_CacheLumpNum(lump, PU_CACHE);
}

//
// I_SDLInitSound
//
// SoM 9/14/02: Rewrite. code taken from prboom to use SDL_Mixer
//
static int I_SDLInitSound(void)
{
   int success = 0;
   int audio_buffers;

   /* Initialize variables */
   audio_buffers = SAMPLECOUNT * snd_samplerate / 11025;
   
   // haleyjd: the docs say we should do this
   if(SDL_InitSubSystem(SDL_INIT_AUDIO))
   {
      printf("Couldn't initialize SDL audio.\n");
      nosfxparm   = true;
      nomusicparm = true;
      return 0;
   }
   
   if(Mix_OpenAudio(snd_samplerate, MIX_DEFAULT_FORMAT, 2, audio_buffers) < 0)
   {
      printf("Couldn't open audio with desired format.\n");
      nosfxparm   = true;
      nomusicparm = true;
      return 0;
   }
   
   // haleyjd 10/02/08: this must be done as early as possible.
   I_SetChannels();

   SAMPLECOUNT = audio_buffers;
   Mix_SetPostMix(I_SDLUpdateSoundCB, NULL);
   printf("Configured audio device with %d samples/slice.\n", SAMPLECOUNT);

   return 1;
}

//
// SDL Sound Driver Object
//
i_sounddriver_t i_sdlsound_driver =
{
   I_SDLInitSound,         // InitSound
   I_SDLCacheSound,        // CacheSound
   I_SDLUpdateSound,       // UpdateSound
   I_SDLSubmitSound,       // SubmitSound
   I_SDLShutdownSound,     // ShutdownSound
   I_SDLStartSound,        // StartSound
   I_SDLSoundID,           // SoundID
   I_SDLStopSound,         // StopSound
   I_SDLSoundIsPlaying,    // SoundIsPlaying
   I_SDLUpdateSoundParams, // UpdateSoundParams
   I_SDLUpdateEQParams,    // UpdateEQParams
};

// EOF


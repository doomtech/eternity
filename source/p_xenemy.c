// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2007 James Haley
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
// DESCRIPTION:
//
// Hexen enemy-related functions
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "a_small.h"
#include "doomstat.h"
#include "p_xenemy.h"
#include "p_inter.h"
#include "p_maputl.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "e_sound.h"
#include "s_sound.h"

//
// T_QuakeThinker
//
// Earthquake effect thinker.
//
void T_QuakeThinker(quakethinker_t *qt)
{
   int i, tics;
   mobj_t *soundorg = (mobj_t *)&qt->origin;
   sfxinfo_t *quakesound;
   
   // quake is finished?
   if(qt->duration == 0)
   {
      S_StopSound(soundorg, CHAN_ALL);
      P_RemoveThinker(&(qt->origin.thinker));
      return;
   }
   
   quakesound = E_SoundForName("Earthquake");

   // loop quake sound
   if(quakesound && !S_CheckSoundPlaying(soundorg, quakesound))
      S_StartSfxInfo(soundorg, quakesound, 127, ATTN_NORMAL, true, CHAN_AUTO);

   tics = qt->duration--;

   // do some rumbling
   for(i = 0; i < MAXPLAYERS; ++i)
   {
      if(playeringame[i])
      {
         player_t *p  = &players[i];
         mobj_t   *mo = p->mo;
         fixed_t  dst = P_AproxDistance(qt->origin.x - mo->x, 
                                        qt->origin.y - mo->y);

         // test if player is in quake radius
         // haleyjd 04/16/07: only set p->quake when qt->intensity is greater;
         // this way, the strongest quake in effect always wins out
         if(dst < qt->quakeRadius && p->quake < qt->intensity)
            p->quake = qt->intensity;

         // every 2 tics, the player may be damaged
         if(!(tics & 1))
         {
            angle_t  thrustangle;   
            
            // test if in damage radius and on floor
            if(dst < qt->damageRadius && mo->z <= mo->floorz)
            {
               if(P_Random(pr_quake) < 50)
               {
                  P_DamageMobj(mo, NULL, NULL, P_Random(pr_quakedmg) % 8 + 1,
                               MOD_QUAKE);
               }
               thrustangle = (359 * P_Random(pr_quake) / 255) * ANGLE_1;
               P_ThrustMobj(mo, thrustangle, qt->intensity * FRACUNIT / 2);
            }
         }
      }
   }
}

//
// P_StartQuake
//
// Starts an earthquake at each object with the tid in args[4]
//
boolean P_StartQuake(int *args)
{
   mobj_t *mo  = NULL;
   boolean ret = false;

   while((mo = P_FindMobjFromTID(args[4], mo, NULL)))
   {
      quakethinker_t *qt;
      ret = true;

      qt = Z_Calloc(1, sizeof(*qt), PU_LEVSPEC, NULL);
      qt->origin.thinker.function = T_QuakeThinker;

      P_AddThinker(&(qt->origin.thinker));

      qt->intensity    = args[0];
      qt->duration     = args[1];
      qt->damageRadius = args[2] * (64 * FRACUNIT);
      qt->quakeRadius  = args[3] * (64 * FRACUNIT);

      qt->origin.x       = mo->x;
      qt->origin.y       = mo->y;
      qt->origin.z       = mo->z;
      qt->origin.groupid = mo->groupid;
   }

   return ret;
}

// EOF


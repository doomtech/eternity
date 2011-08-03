// Emacs style mode select   -*- C -*- vi:ts=3 sw=3:
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
// DESCRIPTION:
//      Thinker, Ticker.
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "c_runcmd.h"
#include "c_io.h"
#include "d_main.h"
#include "p_user.h"
#include "p_chase.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_anim.h"  // haleyjd
#include "p_partcl.h"
#include "polyobj.h"
#include "s_sndseq.h"

// [CG] Added.
#include "e_things.h"
#include "cs_main.h"
#include "cs_position.h"
#include "cl_main.h"
#include "sv_main.h"

int leveltime;
boolean reset_viewz;

//
// THINKERS
// All thinkers should be allocated by Z_Malloc
// so they can be operated on uniformly.
// The actual structures will vary in size,
// but the first element must be thinker_t.
//

// Both the head and tail of the thinker list.
thinker_t thinkercap;

// killough 8/29/98: we maintain several separate threads, each containing
// a special class of thinkers, to allow more efficient searches. 

thinker_t thinkerclasscap[NUMTHCLASS];

//
// P_InitThinkers
//
void P_InitThinkers(void)
{
   int i;
   
   for(i = 0; i < NUMTHCLASS; i++)  // killough 8/29/98: initialize threaded lists
      thinkerclasscap[i].cprev = thinkerclasscap[i].cnext = &thinkerclasscap[i];
   
   thinkercap.prev = thinkercap.next  = &thinkercap;
}

//
// P_UpdateThinker
//
// killough 8/29/98:
// 
// We maintain separate threads of friends and enemies, to permit more
// efficient searches.
//
// haleyjd 11/09/06: Added bug fixes from PrBoom, including addition of the
// th_delete thinker class to rectify problems with infinite loops in the AI
// code due to corruption of the th_enemies/th_friends lists when monsters get
// removed at an inopportune moment.
//
void P_UpdateThinker(thinker_t *thinker)
{
   register thinker_t *th;
   int tclass = th_misc; // haleyjd 07/12/03: don't use "class" as a variable name
   
   // find the class to which the thinker belongs
   if(thinker->function == P_RemoveThinkerDelayed)
      tclass = th_delete;
   else if(thinker->function == P_MobjThinker)
   {
      mobj_t *mo = (mobj_t *)thinker;

      if(mo->health > 0 && 
         (mo->flags & MF_COUNTKILL || mo->flags3 & MF3_KILLABLE))
      {
         if(mo->flags & MF_FRIEND)
            tclass = th_friends;
         else
            tclass = th_enemies;
      }
   }

   // Remove from current thread, if in one -- haleyjd: from PrBoom
   if((th = thinker->cnext) != NULL)
      (th->cprev = thinker->cprev)->cnext = th;
  
   // Add to appropriate thread
   th = &thinkerclasscap[tclass];
   th->cprev->cnext = thinker;
   thinker->cnext = th;
   thinker->cprev = th->cprev;
   th->cprev = thinker;
}

//
// P_AddThinker
//
// Adds a new thinker at the end of the list.
//
void P_AddThinker(thinker_t* thinker)
{
   thinkercap.prev->next = thinker;
   thinker->next = &thinkercap;
   thinker->prev = thinkercap.prev;
   thinkercap.prev = thinker;
   
   thinker->references = 0;    // killough 11/98: init reference counter to 0
   
   // killough 8/29/98: set sentinel pointers, and then add to appropriate list
   thinker->cnext = thinker->cprev = thinker;
   P_UpdateThinker(thinker);
}

//
// killough 11/98:
//
// Make currentthinker external, so that P_RemoveThinkerDelayed
// can adjust currentthinker when thinkers self-remove.

static thinker_t *currentthinker;

//
// P_RemoveThinkerDelayed()
//
// Called automatically as part of the thinker loop in P_RunThinkers(),
// on nodes which are pending deletion.
//
// If this thinker has no more pointers referencing it indirectly,
// remove it, and set currentthinker to one node preceeding it, so
// that the next step in P_RunThinkers() will get its successor.
//
void P_RemoveThinkerDelayed(thinker_t *thinker)
{
   if(!thinker->references)
   {
      thinker_t *next = thinker->next;
      (next->prev = currentthinker = thinker->prev)->next = next;
      
      // haleyjd 11/09/06: remove from threaded list now
      (thinker->cnext->cprev = thinker->cprev)->cnext = thinker->cnext;
      
      Z_Free(thinker);
   }
}

//
// P_RemoveThinker
//
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
// killough 4/25/98:
//
// Instead of marking the function with -1 value cast to a function pointer,
// set the function to P_RemoveThinkerDelayed(), so that later, it will be
// removed automatically as part of the thinker process.
//
void P_RemoveThinker(thinker_t *thinker)
{
   thinker->function = P_RemoveThinkerDelayed;
   
   // killough 8/29/98: remove immediately from threaded list
   
   // haleyjd 11/09/06: NO! Doing this here was always suspect to me, and
   // sure enough: if a monster's removed at the wrong time, it gets put
   // back into the list improperly and starts causing an infinite loop in
   // the AI code. We'll follow PrBoom's lead and create a th_delete class
   // for thinkers awaiting deferred removal.

   // Old code:
   //(thinker->cnext->cprev = thinker->cprev)->cnext = thinker->cnext;

   // Move to th_delete class.
   P_UpdateThinker(thinker);
}

//
// P_SetTarget
//
// This function is used to keep track of pointer references to mobj thinkers.
// In Doom, objects such as lost souls could sometimes be removed despite 
// their still being referenced. In Boom, 'target' mobj fields were tested
// during each gametic, and any objects pointed to by them would be prevented
// from being removed. But this was incomplete, and was slow (every mobj was
// checked during every gametic). Now, we keep a count of the number of
// references, and delay removal until the count is 0.
//
void P_SetTarget(mobj_t **mop, mobj_t *targ)
{
   if(*mop)             // If there was a target already, decrease its refcount
      (*mop)->thinker.references--;
   if((*mop = targ))    // Set new target and if non-NULL, increase its counter
      targ->thinker.references++;
}

//
// P_RunThinkers
//
// killough 4/25/98:
//
// Fix deallocator to stop using "next" pointer after node has been freed
// (a Doom bug).
//
// Process each thinker. For thinkers which are marked deleted, we must
// load the "next" pointer prior to freeing the node. In Doom, the "next"
// pointer was loaded AFTER the thinker was freed, which could have caused
// crashes.
//
// But if we are not deleting the thinker, we should reload the "next"
// pointer after calling the function, in case additional thinkers are
// added at the end of the list.
//
// killough 11/98:
//
// Rewritten to delete nodes implicitly, by making currentthinker
// external and using P_RemoveThinkerDelayed() implicitly.
//
static void P_RunThinkers(void)
{
   for(currentthinker = thinkercap.next;
       currentthinker != &thinkercap;
       currentthinker = currentthinker->next)
   {
      if(!currentthinker->function)
      {
         continue;
      }

      if(!clientserver)
      {
         currentthinker->function(currentthinker);
         continue;
      }

      if(currentthinker->function != P_MobjThinker)
      {
         currentthinker->function(currentthinker);
         continue;
      }

      // [CG] In c/s, a player's thinker is called separately from this, so
      //      only run the thinker if the actor isn't a player.
      if(((mobj_t *)currentthinker)->player == NULL)
      {
         currentthinker->function(currentthinker);
      }
   }
}

//
// P_Ticker
//
void P_Ticker(void)
{
   int i;
   
   // pause if in menu and at least one tic has been run
   //
   // killough 9/29/98: note that this ties in with basetic,
   // since G_Ticker does the pausing during recording or
   // playback, and compensates by incrementing basetic.
   // 
   // All of this complicated mess is used to preserve demo sync.
   // [CG] Never pause in c/s.
   if(!clientserver && (
         paused || (
            (menuactive || consoleactive) &&
            !demoplayback &&
            !netgame &&
            players[consoleplayer].viewz != 1)))
      return;
   
   P_ParticleThinker(); // haleyjd: think for particles

   S_RunSequences(); // haleyjd 06/06/06

   // not if this is an intermission screen
   // haleyjd: players don't think during cinematic pauses
   if(gamestate == GS_LEVEL && !cinema_pause)
   {
      if(!clientserver)
      {
         for(i = 0; i < VANILLA_MAXPLAYERS; i++)
         {
            if(playeringame[i])
            {
               P_PlayerThink(&players[i]);
            }
         }
      }
      else
      {
         for(i = 0; i < MAXPLAYERS; i++)
         {
            if(!playeringame[i])
            {
               continue;
            }

            if(clientserver)
            {
               if(i != 0 || CS_SERVER)
               {
                  // [CG] Don't tick on the server's spectator actor unless
                  //      we're serverside.
                  CS_PlayerTicker(i);
               }
            }
            else
            {
               P_PlayerThink(&players[i]);
            }
         }
      }
   }

   reset_viewz = false;  // sf

   P_RunThinkers();
   P_UpdateSpecials();
   P_RespawnSpecials();
   if(demo_version >= 329)
   {
      P_AnimateSurfaces(); // haleyjd 04/14/99
   }
   
   leveltime++;                       // for par times

   // sf: on original doom, sometimes if you activated a hyperlift
   // while standing on it, your viewz was left behind and appeared
   // to "jump". code in p_floor.c detects if a hyperlift has been
   // activated and viewz is reset appropriately here.
   
   if(demo_version >= 303 && reset_viewz && gamestate == GS_LEVEL)
   {
      // Determines view height and bobbing
      P_CalcHeight(&players[displayplayer]);
   }
   
   P_RunEffects(); // haleyjd: run particle effects
}

//----------------------------------------------------------------------------
//
// $Log: p_tick.c,v $
// Revision 1.7  1998/05/15  00:37:56  killough
// Remove unnecessary crash hack, fix demo sync
//
// Revision 1.6  1998/05/13  22:57:59  killough
// Restore Doom bug compatibility for demos
//
// Revision 1.5  1998/05/03  22:49:01  killough
// Get minimal includes at top
//
// Revision 1.4  1998/04/29  16:19:16  killough
// Fix typo causing game to not pause correctly
//
// Revision 1.3  1998/04/27  01:59:58  killough
// Fix crashes caused by thinkers being used after freed
//
// Revision 1.2  1998/01/26  19:24:32  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------


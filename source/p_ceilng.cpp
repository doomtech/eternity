// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3: 
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
//   Ceiling aninmation (lowering, crushing, raising)
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "r_main.h"
#include "p_info.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_state.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "sounds.h"

#include "cs_netid.h" // [CG] 09/18/11
#include "cs_main.h"  // [CG] 09/18/11
#include "cl_main.h"  // [CG] 09/18/11
#include "sv_main.h"  // [CG] 09/18/11

// the list of ceilings moving currently, including crushers
ceilinglist_t *activeceilings;

//
// P_CeilingSequence
//
// haleyjd 09/27/06: Starts the appropriate sound sequence for a ceiling action.
//
void P_CeilingSequence(sector_t *s, int noiseLevel)
{
   if(silentmove(s))
      return;

   if(s->sndSeqID >= 0)
      S_StartSectorSequence(s, SEQ_CEILING);
   else
   {
      switch(noiseLevel)
      {
      case CNOISE_NORMAL:
         S_StartSectorSequenceName(s, "EECeilingNormal", true);
         break;
      case CNOISE_SEMISILENT:
         S_StartSectorSequenceName(s, "EECeilingSemiSilent", true);
         break;
      case CNOISE_SILENT:
         S_StartSectorSequenceName(s, "EECeilingSilent", true);
         break;
      }
   }
}

//
// P_SetSectorCeilingPic
//
// haleyjd 08/30/09: Call this routine to set a sector's ceiling pic.
//
void P_SetSectorCeilingPic(sector_t *sector, int pic)
{
   // clear sky flag
   sector->intflags &= ~SIF_SKY;

   sector->ceilingpic = pic;

   // reset the sky flag
   if(pic == skyflatnum || pic == sky2flatnum)
      sector->intflags |= SIF_SKY;
}

/////////////////////////////////////////////////////////////////
//
// Ceiling action routine and linedef type handler
//
/////////////////////////////////////////////////////////////////

IMPLEMENT_THINKER_TYPE(CeilingThinker)

//
// T_MoveCeiling
//
// Action routine that moves ceilings. Called once per tick.
//
// Passed a CeilingThinker structure that contains all the info about the move.
// see P_SPEC.H for fields. No return value.
//
// jff 02/08/98 all cases with labels beginning with gen added to support 
// generalized line type behaviors.
//
void CeilingThinker::Think()
{
   result_e  res;

   if(inStasis)
      return;

   switch(direction)
   {
   case plat_stop:
      // If ceiling in stasis, do nothing
      break;

   case plat_up:
      // Ceiling is moving up
      res = T_MovePlane(sector, speed, topheight, -1, 1, direction);

      // if not a silent crusher, make moving sound
      // haleyjd: now handled through sound sequences

      // handle reaching destination height
      if(res == pastdest)
      {
         switch(type)
         {
            // plain movers are just removed
         case raiseToHighest:
         case genCeiling:
            if(CS_CLIENT)
               this->inactive = cl_current_world_index;
            else
               P_RemoveActiveCeiling(this);
            break;

            // movers with texture change, change the texture then get removed
         case genCeilingChgT:
         case genCeilingChg0:
            //jff 3/14/98 transfer old special field as well
            P_TransferSectorSpecial(sector, &special);
         case genCeilingChg:
            P_SetSectorCeilingPic(sector, texture);
            if(CS_CLIENT)
               this->inactive = cl_current_world_index;
            else
               P_RemoveActiveCeiling(this);
            break;

            // crushers reverse direction at the top
         case silentCrushAndRaise:
            // haleyjd: if not playing a looping sequence, start one
            if(!S_CheckSectorSequenceLoop(sector, true))
               P_CeilingSequence(sector, CNOISE_SEMISILENT);
         case genSilentCrusher:
         case genCrusher:
         case fastCrushAndRaise:
         case crushAndRaise:
            direction = plat_down;
            break;
            
         default:
            break;
         }
      }
      break;
  
   case plat_down:
      // Ceiling moving down
      res = T_MovePlane(sector, speed, bottomheight, crush, 1, direction);

      // if not silent crusher type make moving sound
      // haleyjd: now handled through sound sequences

      // handle reaching destination height
      if(res == pastdest)
      {
         switch(this->type)
         {
            // 02/09/98 jff change slow crushers' speed back to normal
            // start back up
         case genSilentCrusher:
         case genCrusher:
            if(oldspeed < CEILSPEED*3)
               speed = this->oldspeed;
            direction = plat_up; //jff 2/22/98 make it go back up!
            break;
            
            // make platform stop at bottom of all crusher strokes
            // except generalized ones, reset speed, start back up
         case silentCrushAndRaise:
            // haleyjd: if not playing a looping sequence, start one
            if(!S_CheckSectorSequenceLoop(sector, true))
               P_CeilingSequence(sector, CNOISE_SEMISILENT);
         case crushAndRaise: 
            speed = CEILSPEED;
         case fastCrushAndRaise:
            direction = plat_up;
            break;
            
            // in the case of ceiling mover/changer, change the texture
            // then remove the active ceiling
         case genCeilingChgT:
         case genCeilingChg0:
            //jff add to fix bug in special transfers from changes
            P_TransferSectorSpecial(sector, &special);
         case genCeilingChg:
            P_SetSectorCeilingPic(sector, texture);
            if(CS_CLIENT)
               this->inactive = cl_current_world_index;
            else
               P_RemoveActiveCeiling(this);
            break;

            // all other case, just remove the active ceiling
         case lowerAndCrush:
         case lowerToFloor:
         case lowerToLowest:
         case lowerToMaxFloor:
         case genCeiling:
            if(CS_CLIENT)
               this->inactive = cl_current_world_index;
            else
               P_RemoveActiveCeiling(this);
            break;
            
         default:
            break;
         }
      }
      else // ( res != pastdest )
      {
         // handle the crusher encountering an obstacle
         if(res == crushed)
         {
            switch(type)
            {
               //jff 02/08/98 slow down slow crushers on obstacle
            case genCrusher:  
            case genSilentCrusher:
               if(oldspeed < CEILSPEED*3)
                  speed = CEILSPEED / 8;
               break;
            case silentCrushAndRaise:
            case crushAndRaise:
            case lowerAndCrush:
               speed = CEILSPEED / 8;
               break;
               
            default:
               break;
            }
         }
      } // end else
      break;
   } // end switch
}

//
// CeilingThinker::serialize
//
// Saves and loads CeilingThinker thinkers.
//
void CeilingThinker::serialize(SaveArchive &arc)
{
   Thinker::serialize(arc);

   arc << type << sector << bottomheight << topheight << speed << oldspeed
       << crush << special << texture << direction << inStasis << tag 
       << olddirection;

   if(arc.isLoading())
   {
      // Reattach to sector, and to active ceilings list
      sector->ceilingdata = this;
      P_AddActiveCeiling(this);
   }
}

CeilingThinker* P_SpawnCeiling(line_t *line, sector_t *sec, ceiling_e type)
{
   int noise = CNOISE_NORMAL; // haleyjd 09/28/06
   CeilingThinker *ceiling = new CeilingThinker;

   ceiling->addThinker();
   sec->ceilingdata = ceiling; //jff 2/22/98
   ceiling->sector = sec;
   ceiling->crush = -1;

   // setup ceiling structure according to type of function
   switch(type)
   {
   case fastCrushAndRaise:
      ceiling->crush = 10;
      ceiling->topheight = sec->ceilingheight;
      ceiling->bottomheight = sec->floorheight + (8*FRACUNIT);
      ceiling->direction = plat_down;
      ceiling->speed = CEILSPEED * 2;
      break;

   case silentCrushAndRaise:
      noise = CNOISE_SEMISILENT;
   case crushAndRaise:
      ceiling->crush = 10;
      ceiling->topheight = sec->ceilingheight;
   case lowerAndCrush:
   case lowerToFloor:
      ceiling->bottomheight = sec->floorheight;
      if(type != lowerToFloor)
         ceiling->bottomheight += 8*FRACUNIT;
      ceiling->direction = plat_down;
      ceiling->speed = CEILSPEED;
      break;

   case raiseToHighest:
      ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
      ceiling->direction = plat_up;
      ceiling->speed = CEILSPEED;
      break;
      
   case lowerToLowest:
      ceiling->bottomheight = P_FindLowestCeilingSurrounding(sec);
      ceiling->direction = plat_down;
      ceiling->speed = CEILSPEED;
      break;
      
   case lowerToMaxFloor:
      ceiling->bottomheight = P_FindHighestFloorSurrounding(sec);
      ceiling->direction = plat_down;
      ceiling->speed = CEILSPEED;
      break;
      
   default:
      break;
   }
    
   // add the ceiling to the active list
   ceiling->tag = sec->tag;
   ceiling->type = type;

   // haleyjd 09/28/06: sound sequences
   P_CeilingSequence(ceiling->sector, noise);

   if(serverside)
   {
      P_AddActiveCeiling(ceiling);
      if(CS_SERVER)
      {
         SV_BroadcastMapSpecialSpawned(
            ceiling, NULL, line, ceiling->sector, ms_ceiling
         );
      }
   }

   return ceiling;
}

//
// EV_DoCeiling
//
// Move a ceiling up/down or start a crusher
//
// Passed the linedef activating the function and the type of function desired
// returns true if a thinker started
//
int EV_DoCeiling(line_t *line, ceiling_e type)
{
   int       secnum = -1;
   int       rtn = 0;
   int       noise = CNOISE_NORMAL; // haleyjd 09/28/06
   sector_t  *sec;
   CeilingThinker *ceiling;
      
   // Reactivate in-stasis ceilings...for certain types.
   // This restarts a crusher after it has been stopped
   switch(type)
   {
   case fastCrushAndRaise:
   case silentCrushAndRaise:
   case crushAndRaise:
      //jff 4/5/98 return if activated
      rtn = P_ActivateInStasisCeiling(line);
   default:
      break;
   }
  
   // [CG] Clients don't spawn ceilings themselves when lines are activated,
   //      but we have to inform the caller whether or not a thinker would have
   //      been created regardless.
   if(CS_CLIENT && rtn != 0)
      return rtn;

   // affects all sectors with the same tag as the linedef
   while((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
   {
      // if ceiling already moving, don't start a second function on it
      if(P_SectorActive(ceiling_special, &sectors[secnum]))  //jff 2/22/98
         continue;
      else if(CS_CLIENT)
         return 1;

      // create a new ceiling thinker
      rtn = 1;

      if(serverside)
         ceiling = P_SpawnCeiling(line, &sectors[secnum], type);

   }

   if(CS_CLIENT)
      return 0;

   return 1;
}

//////////////////////////////////////////////////////////////////////
//
// Active ceiling list primitives
//
/////////////////////////////////////////////////////////////////////

// jff 2/22/98 - modified Lee's plat code to work for ceilings
//
// The following were all rewritten by Lee Killough
// to use the new structure which places no limits
// on active ceilings. It also avoids spending as much
// time searching for active ceilings. Previously a 
// fixed-size array was used, with NULL indicating
// empty entries, while now a doubly-linked list
// is used.

//
// P_ActivateInStasisCeiling()
//
// Reactivates all stopped crushers with the right tag
//
// Passed the line reactivating the crusher
// Returns true if a ceiling reactivated
//
//jff 4/5/98 return if activated
//
int P_ActivateInStasisCeiling(line_t *line)
{
   std::map<uint32_t, CeilingThinker*>::iterator it;
   int rtn = 0, noise;

   for(it = NetCeilings.begin(); it != NetCeilings.end(); it++)
   {
      CeilingThinker *ceiling = (*it).second;
      if(ceiling->tag == line->tag && ceiling->direction == 0)
      {
         ceiling->direction = ceiling->olddirection;
         ceiling->inStasis = false;

         // haleyjd: restart sound sequence
         switch(ceiling->type)
         {
         case silentCrushAndRaise:
            noise = CNOISE_SEMISILENT;
            break;
         case genSilentCrusher:
            noise = CNOISE_SILENT;
            break;
         default:
            noise = CNOISE_NORMAL;
            break;
         }
         P_CeilingSequence(ceiling->sector, noise);

         //jff 4/5/98 return if activated
         rtn = 1;
      }
   }
   return rtn;
}

//
// EV_CeilingCrushStop()
//
// Stops all active ceilings with the right tag
//
// Passed the linedef stopping the ceilings
// Returns true if a ceiling put in stasis
//
int EV_CeilingCrushStop(line_t* line)
{
   int rtn = 0;
   std::map<uint32_t, CeilingThinker*>::iterator it;

   for(it = NetCeilings.begin(); it != NetCeilings.end(); it++)
   {
      CeilingThinker *ceiling = (*it).second;
      if(ceiling->direction != plat_stop && ceiling->tag == line->tag)
      {
         ceiling->olddirection = ceiling->direction;
         ceiling->direction = plat_stop;
         ceiling->inStasis = true;
         S_StopSectorSequence(ceiling->sector, true); // haleyjd 09/28/06
         rtn = 1;
      }
   }
   return rtn;
}

//
// P_AddActiveCeiling()
//
// Adds a ceiling to the head of the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_AddActiveCeiling(CeilingThinker *ceiling)
{
   NetCeilings.add(ceiling);
}

//
// P_RemoveActiveCeiling()
//
// Removes a ceiling from the list of active ceilings
//
// Passed the ceiling motion structure
// Returns nothing
//
void P_RemoveActiveCeiling(CeilingThinker* ceiling)
{
   ceiling->sector->ceilingdata = NULL;   //jff 2/22/98
   S_StopSectorSequence(ceiling->sector, true); // haleyjd 09/28/06
   ceiling->removeThinker();
   if(CS_SERVER)
      SV_BroadcastMapSpecialRemoved(ceiling->net_id, ms_ceiling);
   NetCeilings.remove(ceiling);
}

//
// P_RemoveAllActiveCeilings()
//
// Removes all ceilings from the active ceiling list
//
// Passed nothing, returns nothing
//
void P_RemoveAllActiveCeilings(void)
{
   NetCeilings.clear();
}

//
// P_ChangeCeilingTex
//
// Changes the ceiling flat of all tagged sectors.
//
void P_ChangeCeilingTex(const char *name, int tag)
{
   int flatnum;
   int secnum = -1;

   if((flatnum = R_CheckForFlat(name)) == -1)
      return;

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
      P_SetSectorCeilingPic(&sectors[secnum], flatnum);
}

//----------------------------------------------------------------------------
//
// $Log: p_ceilng.c,v $
// Revision 1.14  1998/05/09  10:58:10  jim
// formatted/documented p_ceilng
//
// Revision 1.13  1998/05/03  23:07:43  killough
// Fix #includes at the top, nothing else
//
// Revision 1.12  1998/04/05  13:54:17  jim
// fixed switch change on second activation
//
// Revision 1.11  1998/03/15  14:40:26  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.10  1998/02/23  23:46:35  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.9  1998/02/23  00:41:31  jim
// Implemented elevators
//
// Revision 1.7  1998/02/13  03:28:22  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.5  1998/02/08  05:35:18  jim
// Added generalized linedef types
//
// Revision 1.4  1998/01/30  14:44:12  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.2  1998/01/26  19:23:56  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------


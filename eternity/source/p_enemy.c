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
// DESCRIPTION:
//      Enemy thinking, AI.
//      Action Pointer Functions
//      that are associated with states/frames.
//
//-----------------------------------------------------------------------------

#include "d_io.h"
#include "doomstat.h"
#include "m_random.h"
#include "r_main.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_map3d.h"
#include "p_setup.h"
#include "p_spec.h"
#include "s_sound.h"
#include "sounds.h"
#include "p_inter.h"
#include "g_game.h"
#include "p_enemy.h"
#include "p_tick.h"
#include "m_bbox.h"
#include "p_anim.h" // haleyjd
#include "c_io.h"
#include "c_runcmd.h"
#include "w_wad.h"
#include "p_partcl.h"
#include "p_info.h"
#include "a_small.h"
#include "e_states.h"
#include "e_things.h"
#include "e_ttypes.h"
#include "d_gi.h"
#include "r_main.h"
#include "e_lib.h"
#include "e_sound.h"

extern fixed_t FloatBobOffsets[64]; // haleyjd: Float Bobbing

static mobj_t *current_actor;

typedef enum 
{
   DI_EAST,
   DI_NORTHEAST,
   DI_NORTH,
   DI_NORTHWEST,
   DI_WEST,
   DI_SOUTHWEST,
   DI_SOUTH,
   DI_SOUTHEAST,
   DI_NODIR,
   NUMDIRS
} dirtype_t;

void A_Fall(mobj_t *actor);
void A_FaceTarget(mobj_t *actor);
static void P_NewChaseDir(mobj_t *actor);

//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//

//
// P_RecursiveSound
//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//
// killough 5/5/98: reformatted, cleaned up
//
static void P_RecursiveSound(sector_t *sec, int soundblocks,
                             mobj_t *soundtarget)
{
   int i;
   
   // wake up all monsters in this sector
   if(sec->validcount == validcount &&
      sec->soundtraversed <= soundblocks+1)
      return;             // already flooded

   sec->validcount = validcount;
   sec->soundtraversed = soundblocks+1;
   P_SetTarget(&sec->soundtarget, soundtarget);    // killough 11/98

#ifdef R_LINKEDPORTALS
   if(R_LinkedFloorActive(sec))
   {
      // Ok, because the same portal can be used on many sectors and even
      // lines, the portal structure won't tell you what sector is on the
      // other side of the portal. SO
      sector_t *other;
      line_t *check = sec->lines[0];

      other = 
         R_PointInSubsector(((check->v1->x + check->v2->x) >> 1) 
                             - R_FPLink(sec)->deltax,
                            ((check->v1->y + check->v2->y) >> 1) 
                             - R_FPLink(sec)->deltay)->sector;

      P_RecursiveSound(other, soundblocks, soundtarget);
   }
   
   if(R_LinkedCeilingActive(sec))
   {
      // Ok, because the same portal can be used on many sectors and even lines, the portal
      // structure won't tell you what sector is on the other side of the portal. SO
      sector_t *other;
      line_t *check = sec->lines[0];

      other = 
      R_PointInSubsector(((check->v1->x + check->v2->x) >> 1) - R_CPLink(sec)->deltax,
                         ((check->v1->y + check->v2->y) >> 1) - R_CPLink(sec)->deltay)->sector;

      P_RecursiveSound(other, soundblocks, soundtarget);
   }
#endif

   for(i=0; i<sec->linecount; i++)
   {
      sector_t *other;
      line_t *check = sec->lines[i];
      
#ifdef R_LINKEDPORTALS
      if(R_LinkedLineActive(check))
      {
         sector_t *other;

         other = 
         R_PointInSubsector(((check->v1->x + check->v2->x) >> 1) - check->portal->data.link.deltax,
                            ((check->v1->y + check->v2->y) >> 1) - check->portal->data.link.deltay)->sector;

         P_RecursiveSound(other, soundblocks, soundtarget);
      }
#endif
      if(!(check->flags & ML_TWOSIDED))
         continue;

      P_LineOpening(check, NULL);
      
      if(tm->openrange <= 0)
         continue;       // closed door

      other=sides[check->sidenum[sides[check->sidenum[0]].sector==sec]].sector;
      
      if(!(check->flags & ML_SOUNDBLOCK))
         P_RecursiveSound(other, soundblocks, soundtarget);
      else if(!soundblocks)
         P_RecursiveSound(other, 1, soundtarget);
   }
}

//
// P_NoiseAlert
//
// If a monster yells at a player,
// it will alert other monsters to the player.
//
void P_NoiseAlert(mobj_t *target, mobj_t *emitter)
{
   validcount++;
   P_RecursiveSound(emitter->subsector->sector, 0, target);
}

//
// P_CheckMeleeRange
//
boolean P_CheckMeleeRange(mobj_t *actor)
{
   mobj_t *pl = actor->target;
   
   // haleyjd 02/15/02: revision of joel's fix for z height check
   if(pl && !comp[comp_overunder])
   {
      if(pl->z > actor->z + actor->height || // pl is too far above
         actor->z > pl->z + pl->height)      // pl is too far below
         return false;
   }

   return  // killough 7/18/98: friendly monsters don't attack other friends
      pl && !(actor->flags & pl->flags & MF_FRIEND) &&
      (P_AproxDistance(pl->x-actor->x, pl->y-actor->y) <
       MELEERANGE - 20*FRACUNIT + pl->info->radius) &&
      P_CheckSight(actor, actor->target);
}

//
// P_HitFriend()
//
// killough 12/98
// This function tries to prevent shooting at friends
//
// haleyjd 09/22/02: reformatted
// haleyjd 09/22/09: rewrote to de-Killoughify ;)
//                   also added ignoring of things with NOFRIENDDMG flag
//
static boolean P_HitFriend(mobj_t *actor)
{
   boolean hitfriend = false;

   if(actor->target)
   {
      angle_t angle;
      fixed_t dist, tx, ty;

#ifdef R_LINKEDPORTALS
      tx = getTargetX(actor);
      ty = getTargetY(actor);
#else
      tx = actor->target->x;
      ty = actor->target->y;
#endif
      angle = R_PointToAngle2(actor->x, actor->y, tx, ty);
      dist  = P_AproxDistance(actor->x - tx, actor->y - ty);

      P_AimLineAttack(actor, angle, dist, 0);

      if(tm->linetarget 
         && tm->linetarget != actor->target 
         && !((tm->linetarget->flags ^ actor->flags) & MF_FRIEND) 
         && !(tm->linetarget->flags3 & MF3_NOFRIENDDMG))
      {
         hitfriend = true;
      }
   }

   return hitfriend;
}

//
// P_CheckMissileRange
//
static boolean P_CheckMissileRange(mobj_t *actor)
{
   fixed_t dist;
   
   if(!P_CheckSight(actor, actor->target))
      return false;
   
   if (actor->flags & MF_JUSTHIT)
   {      // the target just hit the enemy, so fight back!
      actor->flags &= ~MF_JUSTHIT;

      // killough 7/18/98: no friendly fire at corpses
      // killough 11/98: prevent too much infighting among friends

      return 
         !(actor->flags & MF_FRIEND) || 
         (actor->target->health > 0 &&
          (!(actor->target->flags & MF_FRIEND) ||
          (actor->target->player ? 
           monster_infighting || P_Random(pr_defect) >128 :
           !(actor->target->flags & MF_JUSTHIT) && P_Random(pr_defect) >128)));
   }

   // killough 7/18/98: friendly monsters don't attack other friendly
   // monsters or players (except when attacked, and then only once)
   if(actor->flags & actor->target->flags & MF_FRIEND)
      return false;
   
   if(actor->reactiontime)
      return false;       // do not attack yet

   // OPTIMIZE: get this from a global checksight
#ifdef R_LINKEDPORTALS
   dist = P_AproxDistance(actor->x - getTargetX(actor),
                          actor->y - getTargetY(actor)) - 64*FRACUNIT;
#else
   dist = P_AproxDistance(actor->x - actor->target->x,
                          actor->y - actor->target->y) - 64*FRACUNIT;
#endif

   if(actor->info->meleestate == NullStateNum)
      dist -= 128*FRACUNIT;       // no melee attack, so fire more

   dist >>= FRACBITS;

   // haleyjd 01/09/02: various changes made below to move
   // thing-type-specific differences in AI into flags

   if(actor->flags2 & MF2_SHORTMRANGE)
   {
      if(dist > 14*64)
         return false;     // too far away
   }

   if(actor->flags2 & MF2_LONGMELEE)
   {
      if (dist < 196)
         return false;   // close for fist attack
      // dist >>= 1;  -- this is now handled below
   }

   if(actor->flags2 & MF2_RANGEHALF)
      dist >>= 1;

   if(dist > 200)
      dist = 200;

   if((actor->flags2 & MF2_HIGHERMPROB) && dist > 160)
      dist = 160;

   if(P_Random(pr_missrange) < dist)
      return false;
  
   if((actor->flags & MF_FRIEND) && P_HitFriend(actor))
      return false;

   return true;
}

//
// P_IsOnLift
//
// killough 9/9/98:
//
// Returns true if the object is on a lift. Used for AI,
// since it may indicate the need for crowded conditions,
// or that a monster should stay on the lift for a while
// while it goes up or down.
//
static boolean P_IsOnLift(const mobj_t *actor)
{
   const sector_t *sec = actor->subsector->sector;
   line_t line;
   int l;

   // Short-circuit: it's on a lift which is active.
   if(sec->floordata &&
      ((thinker_t *) sec->floordata)->function == T_PlatRaise)
      return true;

   // Check to see if it's in a sector which can be 
   // activated as a lift.
   if((line.tag = sec->tag))
   {
      for(l = -1; (l = P_FindLineFromLineTag(&line, l)) >= 0;)
      {
         switch(lines[l].special)
         {
         case  10: case  14: case  15: case  20: case  21: case  22:
         case  47: case  53: case  62: case  66: case  67: case  68:
         case  87: case  88: case  95: case 120: case 121: case 122:
         case 123: case 143: case 162: case 163: case 181: case 182:
         case 144: case 148: case 149: case 211: case 227: case 228:
         case 231: case 232: case 235: case 236:
            return true;
         }
      }
   }
   
   return false;
}

//
// P_IsUnderDamage
//
// killough 9/9/98:
//
// Returns nonzero if the object is under damage based on
// their current position. Returns 1 if the damage is moderate,
// -1 if it is serious. Used for AI.
//
static int P_IsUnderDamage(mobj_t *actor)
{ 
   const struct msecnode_s *seclist;
   const ceiling_t *cl;             // Crushing ceiling
   int dir = 0;

   for(seclist=actor->touching_sectorlist; seclist; seclist=seclist->m_tnext)
   {
      if((cl = seclist->m_sector->ceilingdata) && 
         cl->thinker.function == T_MoveCeiling)
         dir |= cl->direction;
   }

   return dir;
}

static fixed_t xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
static fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

// 1/11/98 killough: Limit removed on special lines crossed
extern  line_t **spechit;          // New code -- killough
extern  int    numspechit;

//
// P_Move
//
// Move in the current direction; returns false if the move is blocked.
//
static boolean P_Move(mobj_t *actor, boolean dropoff) // killough 9/12/98
{
   fixed_t tryx, tryy, deltax, deltay;
   boolean try_ok;
   int movefactor = ORIG_FRICTION_FACTOR;    // killough 10/98
   int friction = ORIG_FRICTION;
   int speed;

   if(actor->movedir == DI_NODIR)
      return false;

   // haleyjd: OVER_UNDER:
   // [RH] Instead of yanking non-floating monsters to the ground,
   // let gravity drop them down, unless they're moving down a step.
   if(!comp[comp_overunder])
   {
      if(!(actor->flags & MF_NOGRAVITY) && actor->z > actor->floorz && 
         !(actor->intflags & MIF_ONMOBJ))
      {
         if (actor->z > actor->floorz + 24*FRACUNIT)
            return false;
         else
            actor->z = actor->floorz;
      }
   }

#ifdef RANGECHECK
   if((unsigned)actor->movedir >= 8)
      I_Error ("Weird actor->movedir!");
#endif
   
   // killough 10/98: make monsters get affected by ice and sludge too:

   if(monster_friction)
      movefactor = P_GetMoveFactor(actor, &friction);

   speed = actor->info->speed;

   if(friction < ORIG_FRICTION &&     // sludge
      !(speed = ((ORIG_FRICTION_FACTOR - (ORIG_FRICTION_FACTOR-movefactor)/2)
        * speed) / ORIG_FRICTION_FACTOR))
   {
      speed = 1;      // always give the monster a little bit of speed
   }

   tryx = actor->x + (deltax = speed * xspeed[actor->movedir]);
   tryy = actor->y + (deltay = speed * yspeed[actor->movedir]);

   // killough 12/98: rearrange, fix potential for stickiness on ice

   if(friction <= ORIG_FRICTION)
      try_ok = P_TryMove(actor, tryx, tryy, dropoff);
   else
   {
      fixed_t x = actor->x;
      fixed_t y = actor->y;
      fixed_t floorz = actor->floorz;
      fixed_t ceilingz = actor->ceilingz;
      fixed_t dropoffz = actor->dropoffz;
      
      try_ok = P_TryMove(actor, tryx, tryy, dropoff);
      
      // killough 10/98:
      // Let normal momentum carry them, instead of steptoeing them across ice.

      if(try_ok)
      {
         P_UnsetThingPosition(actor);
         actor->x = x;
         actor->y = y;
         actor->floorz = floorz;
         actor->ceilingz = ceilingz;
         actor->dropoffz = dropoffz;
         P_SetThingPosition(actor);
         movefactor *= FRACUNIT / ORIG_FRICTION_FACTOR / 4;
         actor->momx += FixedMul(deltax, movefactor);
         actor->momy += FixedMul(deltay, movefactor);
      }
   }
   
   if(!try_ok)
   {      // open any specials
      int good;
      
      if(actor->flags & MF_FLOAT && tm->floatok)
      {
         fixed_t savedz = actor->z;

         if(actor->z < tm->floorz)          // must adjust height
            actor->z += FLOATSPEED;
         else
            actor->z -= FLOATSPEED;

         // haleyjd: OVER_UNDER:
         // [RH] Check to make sure there's nothing in the way of the float
         if(!comp[comp_overunder])
         {
            if(P_TestMobjZ(actor))
            {
               actor->flags |= MF_INFLOAT;
               return true;
            }
            actor->z = savedz;
         }
         else
         {
            actor->flags |= MF_INFLOAT;            
            return true;
         }
      }

      if(!tm->numspechit)
         return false;

#ifdef RANGECHECK
      // haleyjd 01/09/07: SPECHIT_DEBUG
      if(tm->numspechit < 0)
         I_Error("P_Move: numspechit == %d\n", tm->numspechit);
#endif

      actor->movedir = DI_NODIR;
      
      // if the special is not a door that can be opened, return false
      //
      // killough 8/9/98: this is what caused monsters to get stuck in
      // doortracks, because it thought that the monster freed itself
      // by opening a door, even if it was moving towards the doortrack,
      // and not the door itself.
      //
      // killough 9/9/98: If a line blocking the monster is activated,
      // return true 90% of the time. If a line blocking the monster is
      // not activated, but some other line is, return false 90% of the
      // time. A bit of randomness is needed to ensure it's free from
      // lockups, but for most cases, it returns the correct result.
      //
      // Do NOT simply return false 1/4th of the time (causes monsters to
      // back out when they shouldn't, and creates secondary stickiness).

      for(good = false; tm->numspechit--; )
      {
         if(P_UseSpecialLine(actor, tm->spechit[tm->numspechit], 0))
            good |= (tm->spechit[tm->numspechit] == tm->blockline ? 1 : 2);
      }

      // haleyjd 01/09/07: do not leave numspechit == -1
      tm->numspechit = 0;

      return good && (demo_version < 203 || comp[comp_doorstuck] ||
                      (P_Random(pr_opendoor) >= 230) ^ (good & 1));
   }
   else
   {
      actor->flags &= ~MF_INFLOAT;
   }

   // killough 11/98: fall more slowly, under gravity, if felldown==true
   // haleyjd: OVER_UNDER: not while in 3D clipping mode
   if(comp[comp_overunder])
   {
      if(!(actor->flags & MF_FLOAT) && (!tm->felldown || demo_version < 203))
      {
         fixed_t oldz = actor->z;
         actor->z = actor->floorz;
         
         if(actor->z < oldz)
            E_HitFloor(actor);
      }
   }
   return true;
}

//
// P_SmartMove
//
// killough 9/12/98: Same as P_Move, except smarter
//
static boolean P_SmartMove(mobj_t *actor)
{
   mobj_t *target = actor->target;
   int on_lift, dropoff = false, under_damage;

   // killough 9/12/98: Stay on a lift if target is on one
   on_lift = !comp[comp_staylift] && target && target->health > 0 &&
      target->subsector->sector->tag==actor->subsector->sector->tag &&
      P_IsOnLift(actor);

   under_damage = monster_avoid_hazards && P_IsUnderDamage(actor);
   
   // killough 10/98: allow dogs to drop off of taller ledges sometimes.
   // dropoff==1 means always allow it, dropoff==2 means only up to 128 high,
   // and only if the target is immediately on the other side of the line.

   // haleyjd: allow things of HelperType to jump down,
   // as well as any thing that has the MF2_JUMPDOWN flag
   // (includes DOGS)

   if((actor->flags2 & MF2_JUMPDOWN || (actor->type == HelperThing)) &&
      target && dog_jumping &&
      !((target->flags ^ actor->flags) & MF_FRIEND) &&
#ifdef R_LINKEDPORTALS
      P_AproxDistance(actor->x - getTargetX(actor),
                      actor->y - getTargetY(actor)) < FRACUNIT*144 &&
#else
      P_AproxDistance(actor->x - target->x,
                      actor->y - target->y) < FRACUNIT*144 &&
#endif      
      P_Random(pr_dropoff) < 235)
   {
      dropoff = 2;
   }

   if(!P_Move(actor, dropoff))
      return false;

   // killough 9/9/98: avoid crushing ceilings or other damaging areas
   if(
      (on_lift && P_Random(pr_stayonlift) < 230 &&      // Stay on lift
       !P_IsOnLift(actor))
      ||
      (monster_avoid_hazards && !under_damage &&  // Get away from damage
       (under_damage = P_IsUnderDamage(actor)) &&
       (under_damage < 0 || P_Random(pr_avoidcrush) < 200))
     )
   {
      actor->movedir = DI_NODIR;    // avoid the area (most of the time anyway)
   }
   
   return true;
}

//
// P_TryWalk
//
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
static boolean P_TryWalk(mobj_t *actor)
{
   if(!P_SmartMove(actor))
      return false;
   actor->movecount = P_Random(pr_trywalk)&15;
   return true;
}

//
// P_DoNewChaseDir
//
// killough 9/8/98:
//
// Most of P_NewChaseDir(), except for what
// determines the new direction to take
//
static void P_DoNewChaseDir(mobj_t *actor, fixed_t deltax, fixed_t deltay)
{
   dirtype_t xdir, ydir, tdir;
   dirtype_t olddir = actor->movedir;
   dirtype_t turnaround = olddir;
   
   if(turnaround != DI_NODIR)         // find reverse direction
   {
      turnaround ^= 4;
   }

   {
      xdir = 
         (deltax >  10*FRACUNIT ? DI_EAST :
          deltax < -10*FRACUNIT ? DI_WEST : DI_NODIR);
   
      ydir =
         (deltay < -10*FRACUNIT ? DI_SOUTH :
          deltay >  10*FRACUNIT ? DI_NORTH : DI_NODIR);
   }

   // try direct route
   if(xdir != DI_NODIR && ydir != DI_NODIR && turnaround != 
      (actor->movedir = deltay < 0 ? deltax > 0 ? DI_SOUTHEAST : DI_SOUTHWEST :
       deltax > 0 ? DI_NORTHEAST : DI_NORTHWEST) && P_TryWalk(actor))
   {
      return;
   }

   // try other directions
   if(P_Random(pr_newchase) > 200 || D_abs(deltay)>abs(deltax))
      tdir = xdir, xdir = ydir, ydir = tdir;

   if((xdir == turnaround ? xdir = DI_NODIR : xdir) != DI_NODIR &&
      (actor->movedir = xdir, P_TryWalk(actor)))
      return;         // either moved forward or attacked

   if((ydir == turnaround ? ydir = DI_NODIR : ydir) != DI_NODIR &&
      (actor->movedir = ydir, P_TryWalk(actor)))
      return;

   // there is no direct path to the player, so pick another direction.
   if(olddir != DI_NODIR && (actor->movedir = olddir, P_TryWalk(actor)))
      return;

   // randomly determine direction of search
   if(P_Random(pr_newchasedir) & 1)
   {
      for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; tdir++)
      {
         if(tdir != turnaround &&
            (actor->movedir = tdir, P_TryWalk(actor)))
            return;
      }
   }
   else
   {
      for (tdir = DI_SOUTHEAST; tdir != DI_EAST-1; tdir--)
      {
         if(tdir != turnaround &&
            (actor->movedir = tdir, P_TryWalk(actor)))
            return;
      }
   }
  
   if((actor->movedir = turnaround) != DI_NODIR && !P_TryWalk(actor))
      actor->movedir = DI_NODIR;
}

//
// killough 11/98:
//
// Monsters try to move away from tall dropoffs.
//
// In Doom, they were never allowed to hang over dropoffs,
// and would remain stuck if involuntarily forced over one.
// This logic, combined with p_map.c (P_TryMove), allows
// monsters to free themselves without making them tend to
// hang over dropoffs.

static fixed_t dropoff_deltax, dropoff_deltay, floorz;

static boolean PIT_AvoidDropoff(line_t *line)
{
   if(line->backsector                          && // Ignore one-sided linedefs
      tm->bbox[BOXRIGHT]  > line->bbox[BOXLEFT]   &&
      tm->bbox[BOXLEFT]   < line->bbox[BOXRIGHT]  &&
      tm->bbox[BOXTOP]    > line->bbox[BOXBOTTOM] && // Linedef must be contacted
      tm->bbox[BOXBOTTOM] < line->bbox[BOXTOP]    &&
      P_BoxOnLineSide(tm->bbox, line) == -1)
   {
      fixed_t front = line->frontsector->floorheight;
      fixed_t back  = line->backsector->floorheight;
      angle_t angle;

      // The monster must contact one of the two floors,
      // and the other must be a tall dropoff (more than 24).

      if(back == floorz && front < floorz - FRACUNIT*24)
      {
         // front side dropoff
         angle = R_PointToAngle2(0,0,line->dx,line->dy);
      }
      else
      {
         // back side dropoff
         if(front == floorz && back < floorz - FRACUNIT*24)
            angle = R_PointToAngle2(line->dx,line->dy,0,0);
         else
            return true;
      }

      // Move away from dropoff at a standard speed.
      // Multiple contacted linedefs are cumulative (e.g. hanging over corner)
      dropoff_deltax -= finesine[angle >> ANGLETOFINESHIFT]*32;
      dropoff_deltay += finecosine[angle >> ANGLETOFINESHIFT]*32;
   }

   return true;
}

//
// P_AvoidDropoff
//
// Driver for above
//
static fixed_t P_AvoidDropoff(mobj_t *actor)
{
   int yh=((tm->bbox[BOXTOP]   = actor->y+actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
   int yl=((tm->bbox[BOXBOTTOM]= actor->y-actor->radius)-bmaporgy)>>MAPBLOCKSHIFT;
   int xh=((tm->bbox[BOXRIGHT] = actor->x+actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
   int xl=((tm->bbox[BOXLEFT]  = actor->x-actor->radius)-bmaporgx)>>MAPBLOCKSHIFT;
   int bx, by;

   floorz = actor->z;            // remember floor height

   dropoff_deltax = dropoff_deltay = 0;

   // check lines

   validcount++;
   for(bx = xl; bx <= xh; ++bx)
   {
      // all contacted lines
      for(by = yl; by <= yh; ++by)
         P_BlockLinesIterator(bx, by, PIT_AvoidDropoff);
   }
   
   // Non-zero if movement prescribed
   return dropoff_deltax | dropoff_deltay;
}

//
// P_NewChaseDir
//
// killough 9/8/98: Split into two functions
//
static void P_NewChaseDir(mobj_t *actor)
{
   mobj_t *target = actor->target;
#ifdef R_LINKEDPORTALS
   fixed_t deltax = getTargetX(actor) - actor->x;
   fixed_t deltay = getTargetY(actor) - actor->y;
#else
   fixed_t deltax = target->x - actor->x;
   fixed_t deltay = target->y - actor->y;
#endif

   // killough 8/8/98: sometimes move away from target, keeping distance
   //
   // 1) Stay a certain distance away from a friend, to avoid being in their way
   // 2) Take advantage over an enemy without missiles, by keeping distance

   actor->strafecount = 0;

   if(demo_version >= 203)
   {
      if(actor->floorz - actor->dropoffz > FRACUNIT*24 &&
         actor->z <= actor->floorz &&
         !(actor->flags & (MF_DROPOFF|MF_FLOAT)) &&
         (comp[comp_overunder] || 
          !(actor->intflags & MIF_ONMOBJ)) && // haleyjd: OVER_UNDER
         !comp[comp_dropoff] && P_AvoidDropoff(actor)) // Move away from dropoff
      {
         P_DoNewChaseDir(actor, dropoff_deltax, dropoff_deltay);
         
         // If moving away from dropoff, set movecount to 1 so that 
         // small steps are taken to get monster away from dropoff.
         
         actor->movecount = 1;
         return;
      }
      else
      {
         fixed_t dist = P_AproxDistance(deltax, deltay);
         
         // Move away from friends when too close, except
         // in certain situations (e.g. a crowded lift)
         
         if(actor->flags & target->flags & MF_FRIEND &&
            distfriend << FRACBITS > dist && 
            !P_IsOnLift(target) && !P_IsUnderDamage(actor))
         {
            deltax = -deltax, deltay = -deltay;
         }
         else
         {
            if(target->health > 0 &&
               (actor->flags ^ target->flags) & MF_FRIEND)
            {   
               // Live enemy target
               if(monster_backing &&
                  actor->info->missilestate != NullStateNum && 
                  !(actor->flags2 & MF2_NOSTRAFE) &&
                  ((target->info->missilestate == NullStateNum && dist < MELEERANGE*2) ||
                   (target->player && dist < MELEERANGE*3 &&
                    P_GetReadyWeapon(target->player)->flags & WPF_FLEEMELEE)))
               {
                  // Back away from melee attacker
                  actor->strafecount = P_Random(pr_enemystrafe) & 15;
                  deltax = -deltax, deltay = -deltay;
               }
            }
         }
      }
   }

   P_DoNewChaseDir(actor, deltax, deltay);

   // If strafing, set movecount to strafecount so that old Doom
   // logic still works the same, except in the strafing part
   
   if(actor->strafecount)
      actor->movecount = actor->strafecount;
}

//
// P_IsVisible
//
// killough 9/9/98: whether a target is visible to a monster
//
static boolean P_IsVisible(mobj_t *actor, mobj_t *mo, boolean allaround)
{
   if(mo->flags2 & MF2_DONTDRAW)
      return false;  // haleyjd: total invisibility!

   // haleyjd 11/14/02: Heretic ghost effects
   if(mo->flags3 & MF3_GHOST)
   {
      if(P_AproxDistance(mo->x - actor->x, mo->y - actor->y) > 2*MELEERANGE 
         && P_AproxDistance(mo->momx, mo->momy) < 5*FRACUNIT)
      {
         // when distant and moving slow, target is considered
         // to be "sneaking"
         return false;
      }
      if(P_Random(pr_ghostsneak) < 225)
         return false;
   }

   if(!allaround)
   {
      angle_t an = R_PointToAngle2(actor->x, actor->y, 
                                   mo->x, mo->y) - actor->angle;
      if(an > ANG90 && an < ANG270 &&
         P_AproxDistance(mo->x-actor->x, mo->y-actor->y) > MELEERANGE)
         return false;
   }

   return P_CheckSight(actor, mo);
}


static int current_allaround;

//
// PIT_FindTarget
//
// killough 9/5/98
//
// Finds monster targets for other monsters
//
static boolean PIT_FindTarget(mobj_t *mo)
{
   mobj_t *actor = current_actor;

   if(!((mo->flags ^ actor->flags) & MF_FRIEND && // Invalid target
      mo->health > 0 &&
      (mo->flags & MF_COUNTKILL || mo->flags3 & MF3_KILLABLE)))
      return true;

   // If the monster is already engaged in a one-on-one attack
   // with a healthy friend, don't attack around 60% the time
   {
      const mobj_t *targ = mo->target;
      if(targ && targ->target == mo &&
         P_Random(pr_skiptarget) > 100 &&
         (targ->flags ^ mo->flags) & MF_FRIEND &&
         targ->health*2 >= targ->info->spawnhealth)
         return true;
   }

   if(!P_IsVisible(actor, mo, current_allaround))
      return true;

   P_SetTarget(&actor->lastenemy, actor->target);  // Remember previous target
   P_SetTarget(&actor->target, mo);                // Found target

   // Move the selected monster to the end of its associated
   // list, so that it gets searched last next time.

   {
      thinker_t *cap = &thinkerclasscap[mo->flags & MF_FRIEND ?
                                        th_friends : th_enemies];
      (mo->thinker.cprev->cnext = mo->thinker.cnext)->cprev =
         mo->thinker.cprev;
      (mo->thinker.cprev = cap->cprev)->cnext = &mo->thinker;
      (mo->thinker.cnext = cap)->cprev = &mo->thinker;
   }
   
   return false;
}

//
// P_HereticMadMelee
//
// haleyjd 07/30/04: This function is used to make monsters start
// battling like mad when the player dies in Heretic. Who knows why
// Raven added that "feature," but it's fun ^_^
//
static boolean P_HereticMadMelee(mobj_t *actor)
{
   mobj_t *mo;
   thinker_t *th;

   // only monsters within sight of the player will go crazy
   if(!P_CheckSight(players[0].mo, actor))
      return false;

   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(th->function != P_MobjThinker)
         continue;

      mo = (mobj_t *)th;

      // Must be:
      // * killable
      // * not self (will fight same type, however)
      // * alive
      if(!(mo->flags & MF_COUNTKILL || mo->flags3 & MF3_KILLABLE) || 
         mo == actor || 
         mo->health <= 0)
         continue;

      // skip some at random
      if(P_Random(pr_madmelee) < 16)
         continue;

      // must be within sight
      if(!P_CheckSight(actor, mo))
         continue;

      // got one
      P_SetTarget(&actor->target, mo);
      return true;
   }

   return false;
}

//
// P_LookForPlayers
//
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
static boolean P_LookForPlayers(mobj_t *actor, boolean allaround)
{
   player_t *player;
   int stop, stopc, c;
   
   if(actor->flags & MF_FRIEND)
   {  // killough 9/9/98: friendly monsters go about players differently
      int anyone;

#if 0
      if(!allaround) // If you want friendly monsters not to awaken unprovoked
         return false;
#endif

      // Go back to a player, no matter whether it's visible or not
      for(anyone=0; anyone<=1; anyone++)
      {
         for(c=0; c<MAXPLAYERS; c++)
         {
            if(playeringame[c] && players[c].playerstate==PST_LIVE &&
               (anyone || P_IsVisible(actor, players[c].mo, allaround)))
            {
               P_SetTarget(&actor->target, players[c].mo);

               // killough 12/98:
               // get out of refiring loop, to avoid hitting player accidentally

               if(actor->info->missilestate != NullStateNum)
               {
                  P_SetMobjState(actor, actor->info->seestate);
                  actor->flags &= ~MF_JUSTHIT;
               }
               
               return true;
            }
         }
      }
      
      return false;
   }

   if(GameModeInfo->flags & GIF_HASMADMELEE && GameType == gt_single &&
      players[0].health <= 0)
   {
      // Heretic monsters go mad when player dies
      return P_HereticMadMelee(actor);
   }

   // Change mask of 3 to (MAXPLAYERS-1) -- killough 2/15/98:
   stop = (actor->lastlook-1)&(MAXPLAYERS-1);

   c = 0;

   stopc = demo_version < 203 && !demo_compatibility && monsters_remember ?
           MAXPLAYERS : 2;       // killough 9/9/98

   for(;; actor->lastlook = (actor->lastlook+1)&(MAXPLAYERS-1))
   {
      if(!playeringame[actor->lastlook])
         continue;

      // killough 2/15/98, 9/9/98:
      if(c++ == stopc || actor->lastlook == stop)  // done looking
         return false;
      
      player = &players[actor->lastlook];
      
      if(player->health <= 0)
         continue;               // dead
      
      if(!P_IsVisible(actor, player->mo, allaround))
         continue;
      
      P_SetTarget(&actor->target, player->mo);

      // killough 9/9/98: give monsters a threshold towards getting players
      // (we don't want it to be too easy for a player with dogs :)
      if(demo_version >= 203 && !comp[comp_pursuit])
         actor->threshold = 60;
      
      return true;
   }
}

//
// P_LookForMonsters
// 
// Friendly monsters, by Lee Killough 7/18/98
//
// Friendly monsters go after other monsters first, but 
// also return to owner if they cannot find any targets.
// A marine's best friend :)  killough 7/18/98, 9/98
//
static boolean P_LookForMonsters(mobj_t *actor, boolean allaround)
{
   thinker_t *cap, *th;
   
   if(demo_compatibility)
      return false;

   if(actor->lastenemy && actor->lastenemy->health > 0 && monsters_remember &&
      !(actor->lastenemy->flags & actor->flags & MF_FRIEND)) // not friends
   {
      P_SetTarget(&actor->target, actor->lastenemy);
      P_SetTarget(&actor->lastenemy, NULL);
      return true;
   }

   if(demo_version < 203)  // Old demos do not support monster-seeking bots
      return false;

   // Search the threaded list corresponding to this object's potential targets
   cap = &thinkerclasscap[actor->flags & MF_FRIEND ? th_enemies : th_friends];

   // Search for new enemy

   if(cap->cnext != cap)        // Empty list? bail out early
   {
      int x = (actor->x - bmaporgx)>>MAPBLOCKSHIFT;
      int y = (actor->y - bmaporgy)>>MAPBLOCKSHIFT;
      int d;

      current_actor = actor;
      current_allaround = allaround;
      
      // Search first in the immediate vicinity.
      
      if(!P_BlockThingsIterator(x, y, PIT_FindTarget))
         return true;

      for(d = 1; d < 5; ++d)
      {
         int i = 1 - d;
         do
         {
            if(!P_BlockThingsIterator(x+i, y-d, PIT_FindTarget) ||
               !P_BlockThingsIterator(x+i, y+d, PIT_FindTarget))
               return true;
         }
         while(++i < d);
         do
         {
            if(!P_BlockThingsIterator(x-d, y+i, PIT_FindTarget) ||
               !P_BlockThingsIterator(x+d, y+i, PIT_FindTarget))
               return true;
         }
         while(--i + d >= 0);
      }

      {   // Random number of monsters, to prevent patterns from forming
         int n = (P_Random(pr_friends) & 31) + 15;

         for(th = cap->cnext; th != cap; th = th->cnext)
         {
            if(--n < 0)
            { 
               // Only a subset of the monsters were searched. Move all of
               // the ones which were searched so far to the end of the list.
               
               (cap->cnext->cprev = cap->cprev)->cnext = cap->cnext;
               (cap->cprev = th->cprev)->cnext = cap;
               (th->cprev = cap)->cnext = th;
               break;
            }
            else if(!PIT_FindTarget((mobj_t *) th))
               // If target sighted
               return true;
         }
      }
   }
   
   return false;  // No monster found
}

//
// P_LookForTargets
//
// killough 9/5/98: look for targets to go after, depending on kind of monster
//
static boolean P_LookForTargets(mobj_t *actor, int allaround)
{
   return actor->flags & MF_FRIEND ?
      P_LookForMonsters(actor, allaround) || P_LookForPlayers (actor, allaround):
      P_LookForPlayers (actor, allaround) || P_LookForMonsters(actor, allaround);
}

//
// P_HelpFriend
//
// killough 9/8/98: Help friends in danger of dying
//
static boolean P_HelpFriend(mobj_t *actor)
{
   thinker_t *cap, *th;

   // If less than 33% health, self-preservation rules
   if(actor->health*3 < actor->info->spawnhealth)
      return false;

   current_actor = actor;
   current_allaround = true;

   // Possibly help a friend under 50% health
   cap = &thinkerclasscap[actor->flags & MF_FRIEND ? th_friends : th_enemies];

   for (th = cap->cnext; th != cap; th = th->cnext)
   {
      if(((mobj_t *) th)->health*2 >=
         ((mobj_t *) th)->info->spawnhealth)
      {
         if(P_Random(pr_helpfriend) < 180)
            break;
      }
      else
         if(((mobj_t *) th)->flags & MF_JUSTHIT &&
            ((mobj_t *) th)->target && 
            ((mobj_t *) th)->target != actor->target &&
            !PIT_FindTarget(((mobj_t *) th)->target))
         {
            // Ignore any attacking monsters, while searching for 
            // friend
            actor->threshold = BASETHRESHOLD;
            return true;
         }
   }
   
   return false;
}

//
// ACTION ROUTINES
//

//
// A_Look
//
// Stay in state until a player is sighted.
//
void A_Look(mobj_t *actor)
{
   static boolean recursion;
   mobj_t *targ;
   
   // haleyjd 1/25/00:  isolated assignment of targ to test earlier
   //  for "deaf" enemies seeing totally invisible players after the
   //  sector soundtarget is activated, which looks crazy and should
   //  NOT happen.  The if below is simply deplorable to try to read!
   
   targ = actor->subsector->sector->soundtarget;

   if(targ && 
      (targ->flags2 & MF2_DONTDRAW) && 
      (actor->flags & MF_AMBUSH))
      return;

   // killough 7/18/98:
   // Friendly monsters go after other monsters first, but 
   // also return to player, without attacking them, if they
   // cannot find any targets. A marine's best friend :)
   
   actor->threshold = actor->pursuecount = 0;
   if(!(actor->flags & MF_FRIEND && P_LookForTargets(actor, false)) &&
      !((targ) &&  // haleyjd: removed assignment here
        targ->flags & MF_SHOOTABLE &&
        (P_SetTarget(&actor->target, targ),
         !(actor->flags & MF_AMBUSH) || P_CheckSight(actor, targ))) &&
      (actor->flags & MF_FRIEND || !P_LookForTargets(actor, false)))
         return;

   // go into chase state
   
   if(actor->info->seesound)
   {
      int sound;

      // EDF_FIXME: needs to be generalized like in zdoom
      switch (actor->info->seesound)
      {
      case sfx_posit1:
      case sfx_posit2:
      case sfx_posit3:
         sound = sfx_posit1 + P_Random(pr_see) % 3;
         break;
         
      case sfx_bgsit1:
      case sfx_bgsit2:
         sound = sfx_bgsit1 + P_Random(pr_see) % 2;
         break;
                  
      default:
         sound = actor->info->seesound;
         break;
      }
      
      // haleyjd: generalize to all bosses
      if(actor->flags2 & MF2_BOSS)
         S_StartSound(NULL, sound);          // full volume
      else
         S_StartSound(actor, sound);
   }

   // haleyjd 09/21/09: guard against A_Look recursion
   if(recursion)
   {
      doom_printf("Warning: Look recursion detected");
      return;
   }
   
   recursion = true;
   P_SetMobjState(actor, actor->info->seestate);
   recursion = false;
}

//
// A_KeepChasing
//
// killough 10/98:
// Allows monsters to continue movement while attacking
//
void A_KeepChasing(mobj_t *actor)
{
   /*
   if(actor->movecount)
   {
      actor->movecount--;
      if(actor->strafecount)
         actor->strafecount--;
      P_SmartMove(actor);
   }
   */
   P_SmartMove(actor);
}

//
// P_SuperFriend
//
// haleyjd 01/11/04: returns true if this thing is a "super friend"
// and is going to attack a friend
//
static boolean P_SuperFriend(mobj_t *actor)
{
   mobj_t *target = actor->target;

   return ((actor->flags3 & MF3_SUPERFRIEND) && // thing is a super friend,
           target &&                            // target is valid, and
           (actor->flags & target->flags & MF_FRIEND)); // both are friends
}

//
// P_MakeActiveSound
//
// haleyjd 06/15/05: isolated from A_Chase.
//
static void P_MakeActiveSound(mobj_t *actor)
{
   if(actor->info->activesound && P_Random(pr_see) < 3)
   {
      int sound = actor->info->activesound;
      int attn  = ATTN_NORMAL;

      // haleyjd: some heretic enemies use their seesound on
      // occasion, so I've made this a general feature
      if(actor->flags3 & MF3_ACTSEESOUND)
      {
         if(P_Random(pr_lookact) < 128 && actor->info->seesound)
            sound = actor->info->seesound;
      }

      // haleyjd: some heretic enemies snort at full volume :)
      // haleyjd: but when they do so, they do not cut off a sound
      // they are already playing.
      if(actor->flags3 & MF3_LOUDACTIVE)
      {
         sfxinfo_t *sfx = E_SoundForDEHNum(sound);
         if(sfx && S_CheckSoundPlaying(actor, sfx))
            return;
         attn = ATTN_NONE;
      }

      S_StartSoundAtVolume(actor, sound, 127, attn, CHAN_AUTO);
   }
}

//
// A_Chase
//
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void A_Chase(mobj_t *actor)
{
   boolean superfriend = false;

   if(actor->reactiontime)
      actor->reactiontime--;

   // modify target threshold
   if(actor->threshold)
   {
      if(!actor->target || actor->target->health <= 0)
         actor->threshold = 0;
      else
         actor->threshold--;
   }

   // turn towards movement direction if not there yet
   // killough 9/7/98: keep facing towards target if strafing or backing out

   if(actor->strafecount)
      A_FaceTarget(actor);
   else
   {
      if(actor->movedir < 8)
      {
         int delta = (actor->angle &= (7<<29)) - (actor->movedir << 29);

         if(delta > 0)
            actor->angle -= ANG90/2;
         else if (delta < 0)
            actor->angle += ANG90/2;
      }
   }

   if(!actor->target || !(actor->target->flags & MF_SHOOTABLE))
   {
      // haleyjd 07/26/04: Detect and prevent infinite recursion if
      // Chase is called from a thing's spawnstate.
      static boolean recursion = false;

      // if recursion is true at this point, P_SetMobjState sent
      // us back here -- print an error message and return
      if(recursion)
      {
         doom_printf("Warning: Chase recursion detected\n");
         return;
      }

      // set the flag true before calling P_SetMobjState
      recursion = true;

      if(!P_LookForTargets(actor,true)) // look for a new target
         P_SetMobjState(actor, actor->info->spawnstate); // no new target

      // clear the flag after the state set occurs
      recursion = false;

      return;
   }

   // do not attack twice in a row
   if(actor->flags & MF_JUSTATTACKED)
   {
      // haleyjd 05/22/06: ALWAYSFAST flag
      actor->flags &= ~MF_JUSTATTACKED;
      if(gameskill != sk_nightmare && !fastparm && !(actor->flags3 & MF3_ALWAYSFAST))
         P_NewChaseDir(actor);
      return;
   }

   // haleyjd 01/11/04: check superfriend status
   if(demo_version >= 331)
      superfriend = P_SuperFriend(actor);

  // check for melee attack
   if(actor->info->meleestate != NullStateNum && P_CheckMeleeRange(actor) && 
      !superfriend)
   {
      // haleyjd 05/01/05: Detect and prevent infinite recursion if
      // Chase is called from a thing's attack state
      static boolean recursion = false;

      if(recursion)
      {
         doom_printf("Warning: Chase recursion detected\n");
         return;
      }

      S_StartSound(actor, actor->info->attacksound);
      
      recursion = true;

      P_SetMobjState(actor, actor->info->meleestate);

      recursion = false;
      
      if(actor->info->missilestate == NullStateNum)
         actor->flags |= MF_JUSTHIT; // killough 8/98: remember an attack
      return;
   }

   // check for missile attack
   if(actor->info->missilestate != NullStateNum && !superfriend)
   {
      // haleyjd 05/01/05: Detect and prevent infinite recursion if
      // Chase is called from a thing's attack state
      static boolean recursion = false;

      if(recursion)
      {
         doom_printf("Warning: Chase recursion detected\n");
         return;
      }

      // haleyjd 05/22/06: ALWAYSFAST flag
      if(!actor->movecount || gameskill >= sk_nightmare || fastparm || 
         (actor->flags3 & MF3_ALWAYSFAST))
      {
         if(P_CheckMissileRange(actor))
         {
            recursion = true;
            P_SetMobjState(actor, actor->info->missilestate);
            recursion = false;
            actor->flags |= MF_JUSTATTACKED;
            return;
         }
      }
   }

   if (!actor->threshold)
   {
      if(demo_version < 203)
      {
         // killough 9/9/98: for backward demo compatibility
         if(netgame && !P_CheckSight(actor, actor->target) &&
            P_LookForPlayers(actor, true))
            return;  
      }
      else  // killough 7/18/98, 9/9/98: new monster AI
      {
         if(help_friends && P_HelpFriend(actor))
         {
            return;      // killough 9/8/98: Help friends in need
         }
         else  // Look for new targets if current one is bad or is out of view
         {
            if (actor->pursuecount)
               actor->pursuecount--;
            else
            {
               actor->pursuecount = BASETHRESHOLD;
               
               // If current target is bad and a new one is found, return:

               if(!(actor->target && actor->target->health > 0 &&
                   ((comp[comp_pursuit] && !netgame) || 
                    (((actor->target->flags ^ actor->flags) & MF_FRIEND ||
                      (!(actor->flags & MF_FRIEND) && monster_infighting)) &&
                    P_CheckSight(actor, actor->target)))) &&
                    P_LookForTargets(actor, true))
               {
                  return;
               }
              
               // (Current target was good, or no new target was found)
               //
               // If monster is a missile-less friend, give up pursuit 
               // and return to player, if no attacks have occurred 
               // recently.
               
               if(actor->info->missilestate == NullStateNum && 
                  actor->flags & MF_FRIEND)
               {
                  if(actor->flags & MF_JUSTHIT)        // if recent action,
                     actor->flags &= ~MF_JUSTHIT;      // keep fighting
                  else
                     if(P_LookForPlayers(actor, true)) // else return to player
                        return;
               }
            }
         }
      }
   }
   
   if(actor->strafecount)
      actor->strafecount--;
   
   // chase towards player
   if(--actor->movecount<0 || !P_SmartMove(actor))
      P_NewChaseDir(actor);

   // make active sound
   P_MakeActiveSound(actor);   
}

//
// A_RandomWalk
//
// haleyjd 06/15/05: Makes an object walk in random directions without
// following or attacking any target.
//
void A_RandomWalk(mobj_t *actor)
{
   int i, checkdirs[NUMDIRS];

   for(i = 0; i < NUMDIRS; ++i)
      checkdirs[i] = 0;

   // turn toward movement direction
   if(actor->movedir < 8)
   {
      int delta = (actor->angle &= (7 << 29)) - (actor->movedir << 29);

      if(delta > 0)
         actor->angle -= ANG90 / 2;
      else if(delta < 0)
         actor->angle += ANG90 / 2;
   }
   
   // time to move?
   if(--actor->movecount < 0 || !P_Move(actor, 0))
   {
      dirtype_t tdir;
      dirtype_t turnaround = actor->movedir;
      boolean dirfound = false;

      if(P_Random(pr_rndwspawn) < 24)
      {
         P_SetMobjState(actor, actor->info->spawnstate);
         return;
      }
   
      if(turnaround != DI_NODIR) // find reverse direction
         turnaround ^= 4;

      // try a completely random direction
      tdir = P_Random(pr_rndwnewdir) & 7;
      if(tdir != turnaround && 
         (actor->movedir = tdir, P_Move(actor, 0)))
      {
         checkdirs[tdir] = 1;
         dirfound = true;
      }

      // randomly determine search direction
      if(!dirfound)
      {
         if(tdir & 1)
         {
            for(tdir = DI_EAST; tdir <= DI_SOUTHEAST; ++tdir)
            {
               // don't try the one we already tried before
               if(checkdirs[tdir])
                  continue;
               
               if(tdir != turnaround && 
                  (actor->movedir = tdir, P_Move(actor, 0)))
               {
                  dirfound = true;
                  break;
               }
            }
         }
         else
         {
            for(tdir = DI_SOUTHEAST; tdir != DI_EAST - 1; --tdir)
            {
               // don't try the one we already tried before
               if(checkdirs[tdir])
                  continue;
               
               if(tdir != turnaround && 
                  (actor->movedir = tdir, P_Move(actor, 0)))
               {
                  dirfound = true;
                  break;
               }
            }
         }
      }

      // if didn't find a direction, try the opposite direction
      if(!dirfound)
      {
         if((actor->movedir = turnaround) != DI_NODIR && !P_Move(actor, 0))
            actor->movedir = DI_NODIR;
         else
            dirfound = true;
      }
      
      // if moving, reset movecount
      if(dirfound)
         actor->movecount = P_Random(pr_rndwmovect) & 15;
   }

   // make active sound
   P_MakeActiveSound(actor);
}

//
// A_FaceTarget
//
void A_FaceTarget(mobj_t *actor)
{
   if(!actor->target)
      return;

   // haleyjd: special feature for player thunking
   if(actor->intflags & MIF_NOFACE)
      return;

   actor->flags &= ~MF_AMBUSH;
   actor->angle = R_PointToAngle2(actor->x, actor->y,
#ifdef R_LINKEDPORTALS
                                  getTargetX(actor), getTargetY(actor));
#else
                                  actor->target->x, actor->target->y);
#endif
   if(actor->target->flags & MF_SHADOW ||
      actor->target->flags2 & MF2_DONTDRAW || // haleyjd
      actor->target->flags3 & MF3_GHOST)      // haleyjd
   {
      actor->angle += P_SubRandom(pr_facetarget) << 21;
   }
}

//
// A_PosAttack
//
void A_PosAttack(mobj_t *actor)
{
   int angle, damage, slope;
   
   if(!actor->target)
      return;

   A_FaceTarget(actor);
   angle = actor->angle;
   slope = P_AimLineAttack(actor, angle, MISSILERANGE, 0); // killough 8/2/98
   S_StartSound(actor, sfx_pistol);
   
   // haleyjd 08/05/04: use new function
   angle += P_SubRandom(pr_posattack) << 20;

   damage = (P_Random(pr_posattack) % 5 + 1) * 3;
   P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void A_SPosAttack(mobj_t* actor)
{
   int i, bangle, slope;
   
   if (!actor->target)
      return;
   
   S_StartSound(actor, sfx_shotgn);
   A_FaceTarget(actor);
   
   bangle = actor->angle;
   slope = P_AimLineAttack(actor, bangle, MISSILERANGE, 0); // killough 8/2/98
   
   for(i = 0; i < 3; ++i)
   {  
      // haleyjd 08/05/04: use new function
      int angle = bangle + (P_SubRandom(pr_sposattack) << 20);
      int damage = ((P_Random(pr_sposattack) % 5) + 1) * 3;
      P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
   }
}

void A_CPosAttack(mobj_t *actor)
{
   int angle, bangle, damage, slope;
   
   if (!actor->target)
      return;

   // haleyjd: restored to normal
   S_StartSound(actor, sfx_shotgn);
   A_FaceTarget(actor);
   
   bangle = actor->angle;
   slope = P_AimLineAttack(actor, bangle, MISSILERANGE, 0); // killough 8/2/98
   
   // haleyjd 08/05/04: use new function
   angle = bangle + (P_SubRandom(pr_cposattack) << 20);
   damage = ((P_Random(pr_cposattack) % 5) + 1) * 3;
   P_LineAttack(actor, angle, MISSILERANGE, slope, damage);
}

void A_CPosRefire(mobj_t *actor)
{
   // keep firing unless target got out of sight
   A_FaceTarget(actor);
   
   // killough 12/98: Stop firing if a friend has gotten in the way
   if(actor->flags & MF_FRIEND && P_HitFriend(actor))
   {
      P_SetMobjState(actor, actor->info->seestate);
      return;
   }
   
   // killough 11/98: prevent refiring on friends continuously
   if(P_Random(pr_cposrefire) < 40)
   {
      if(actor->target && (actor->flags & actor->target->flags & MF_FRIEND))
         P_SetMobjState(actor, actor->info->seestate);
      return;
   }

   if(!actor->target || actor->target->health <= 0 ||
      !P_CheckSight(actor, actor->target))
   {
      P_SetMobjState(actor, actor->info->seestate);
   }
}

void A_SpidRefire(mobj_t* actor)
{
   // keep firing unless target got out of sight
   A_FaceTarget(actor);
   
   // killough 12/98: Stop firing if a friend has gotten in the way
   if(actor->flags & MF_FRIEND && P_HitFriend(actor))
   {
      P_SetMobjState(actor, actor->info->seestate);
      return;
   }
   
   if(P_Random(pr_spidrefire) < 10)
      return;

   // killough 11/98: prevent refiring on friends continuously
   if(!actor->target || actor->target->health <= 0    ||
      actor->flags & actor->target->flags & MF_FRIEND ||
      !P_CheckSight(actor, actor->target))
   {
      P_SetMobjState(actor, actor->info->seestate);
   }
}

void A_BspiAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   // launch a missile
   P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_ARACHPLAZ), 
                  actor->z + DEFAULTMISSILEZ);
}

//
// A_TroopAttack
//
void A_TroopAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   if(P_CheckMeleeRange(actor))
   {
      int damage;
      S_StartSound(actor, sfx_claw);
      damage = (P_Random(pr_troopattack)%8+1)*3;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
      return;
   }
   // launch a missile
   P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_TROOPSHOT),
                  actor->z + DEFAULTMISSILEZ);
}

void A_SargAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   if(P_CheckMeleeRange(actor))
   {
      int damage = ((P_Random(pr_sargattack)%10)+1)*4;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
   }
}

void A_HeadAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget (actor);
   if(P_CheckMeleeRange(actor))
   {
      int damage = (P_Random(pr_headattack)%6+1)*10;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
      return;
   }
   // launch a missile
   P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_HEADSHOT),
                  actor->z + DEFAULTMISSILEZ);
}

void A_CyberAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   P_SpawnMissile(actor, actor->target, 
                  E_SafeThingType(MT_ROCKET),
                  actor->z + DEFAULTMISSILEZ);   
}

void A_BruisAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   if(P_CheckMeleeRange(actor))
   {
      int damage;
      S_StartSound(actor, sfx_claw);
      damage = (P_Random(pr_bruisattack)%8+1)*10;
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
      return;
   }
   P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_BRUISERSHOT),
                  actor->z + DEFAULTMISSILEZ);  // launch a missile
}

//
// A_SkelMissile
//
void A_SkelMissile(mobj_t *actor)
{
   mobj_t *mo;
   
   if(!actor->target)
      return;
   
   A_FaceTarget (actor);
   actor->z += 16*FRACUNIT;      // so missile spawns higher
   mo = P_SpawnMissile(actor, actor->target, E_SafeThingType(MT_TRACER),
                       actor->z + DEFAULTMISSILEZ);
   actor->z -= 16*FRACUNIT;      // back to normal
   
   mo->x += mo->momx;
   mo->y += mo->momy;
   P_SetTarget(&mo->tracer, actor->target);  // killough 11/98
}

#define TRACEANGLE 0xc000000   /* killough 9/9/98: change to #define */

void A_Tracer(mobj_t *actor)
{
   angle_t       exact;
   fixed_t       dist;
   fixed_t       slope;
   mobj_t        *dest;
   mobj_t        *th;

   // killough 1/18/98: this is why some missiles do not have smoke
   // and some do. Also, internal demos start at random gametics, 
   // thus the bug in which revenants cause internal demos to go out 
   // of sync.
   //
   // killough 3/6/98: fix revenant internal demo bug by subtracting
   // levelstarttic from gametic.
   //
   // killough 9/29/98: use new "basetic" so that demos stay in sync
   // during pauses and menu activations, while retaining old demo 
   // sync.
   //
   // leveltime would have been better to use to start with in Doom, 
   // but since old demos were recorded using gametic, we must stick 
   // with it, and improvise around it (using leveltime causes desync 
   // across levels).

   if((gametic-basetic) & 3)
      return;

   // spawn a puff of smoke behind the rocket
   P_SpawnPuff(actor->x, actor->y, actor->z, 0, 3, false);
   
   th = P_SpawnMobj(actor->x - actor->momx,
                    actor->y - actor->momy,
                    actor->z, E_SafeThingType(MT_SMOKE));
  
   th->momz = FRACUNIT;
   th->tics -= P_Random(pr_tracer) & 3;
   if(th->tics < 1)
      th->tics = 1;
  
   // adjust direction
   dest = actor->tracer;
   
   if(!dest || dest->health <= 0)
      return;

   // change angle
   exact = R_PointToAngle2(actor->x, actor->y, dest->x, dest->y);

   if(exact != actor->angle)
   {
      if(exact - actor->angle > 0x80000000)
      {
         actor->angle -= TRACEANGLE;
         if(exact - actor->angle < 0x80000000)
            actor->angle = exact;
      }
      else
      {
         actor->angle += TRACEANGLE;
         if(exact - actor->angle > 0x80000000)
            actor->angle = exact;
      }
   }

   exact = actor->angle>>ANGLETOFINESHIFT;
   actor->momx = FixedMul(actor->info->speed, finecosine[exact]);
   actor->momy = FixedMul(actor->info->speed, finesine[exact]);

   // change slope
   dist = P_AproxDistance(dest->x - actor->x, dest->y - actor->y);
   
   dist = dist / actor->info->speed;

   if(dist < 1)
      dist = 1;

   slope = (dest->z + 40*FRACUNIT - actor->z) / dist;
   
   if(slope < actor->momz)
      actor->momz -= FRACUNIT/8;
   else
      actor->momz += FRACUNIT/8;
}

void A_SkelWhoosh(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   S_StartSound(actor, sfx_skeswg);
}

void A_SkelFist(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   if(P_CheckMeleeRange(actor))
   {
      int damage = ((P_Random(pr_skelfist) % 10) + 1) * 6;
      S_StartSound(actor, sfx_skepch);
      P_DamageMobj(actor->target, actor, actor, damage, MOD_HIT);
   }
}

//
// PIT_VileCheck
// Detect a corpse that could be raised.
//

mobj_t* corpsehit;
mobj_t* vileobj;
fixed_t viletryx;
fixed_t viletryy;

boolean PIT_VileCheck(mobj_t *thing)
{
   int maxdist;
   boolean check;
   static int vileType = -1;
   
   if(vileType == -1)
      vileType = E_SafeThingType(MT_VILE);
   
   if(!(thing->flags & MF_CORPSE))
      return true;        // not a monster
   
   if(thing->tics != -1)
      return true;        // not lying still yet
   
   if(thing->info->raisestate == NullStateNum)
      return true;        // monster doesn't have a raise state
   
   maxdist = thing->info->radius + mobjinfo[vileType].radius;
   
   if(D_abs(thing->x-viletryx) > maxdist ||
      D_abs(thing->y-viletryy) > maxdist)
      return true;                // not actually touching

// Check to see if the radius and height are zero. If they are      // phares
// then this is a crushed monster that has been turned into a       //   |
// gib. One of the options may be to ignore this guy.               //   V

// Option 1: the original, buggy method, -> ghost (compatibility)
// Option 2: resurrect the monster, but not as a ghost
// Option 3: ignore the gib

//    if (Option3)                                                  //   ^
//        if ((thing->height == 0) && (thing->radius == 0))         //   |
//            return true;                                          // phares

   corpsehit = thing;
   corpsehit->momx = corpsehit->momy = 0;
   if(comp[comp_vile])
   {                                                              // phares
      corpsehit->height <<= 2;                                    //   V
      
      // haleyjd 11/11/04: this is also broken by Lee's change to
      // PIT_CheckThing when not in demo_compatibility.
      if(demo_version >= 331)
         corpsehit->flags |= MF_SOLID;

      check = P_CheckPosition(corpsehit, corpsehit->x, corpsehit->y);

      if(demo_version >= 331)
         corpsehit->flags &= ~MF_SOLID;
      
      corpsehit->height >>= 2;
   }
   else
   {
      int height,radius;
      
      height = corpsehit->height; // save temporarily
      radius = corpsehit->radius; // save temporarily
      corpsehit->height = P_ThingInfoHeight(corpsehit->info);
      corpsehit->radius = corpsehit->info->radius;
      corpsehit->flags |= MF_SOLID;
      check = P_CheckPosition(corpsehit,corpsehit->x,corpsehit->y);
      corpsehit->height = height; // restore
      corpsehit->radius = radius; // restore                      //   ^
      corpsehit->flags &= ~MF_SOLID;
   }                                                              //   |
                                                                  // phares
   if(!check)
      return true;              // doesn't fit here
   return false;               // got one, so stop checking
}

//
// A_VileChase
//
// Check for ressurecting a body
//
void A_VileChase(mobj_t *actor)
{
   int xl, xh;
   int yl, yh;
   int bx, by;

   if(actor->movedir != DI_NODIR)
   {
      // check for corpses to raise
      viletryx =
         actor->x + actor->info->speed*xspeed[actor->movedir];
      viletryy =
         actor->y + actor->info->speed*yspeed[actor->movedir];
      
      xl = (viletryx - bmaporgx - MAXRADIUS*2)>>MAPBLOCKSHIFT;
      xh = (viletryx - bmaporgx + MAXRADIUS*2)>>MAPBLOCKSHIFT;
      yl = (viletryy - bmaporgy - MAXRADIUS*2)>>MAPBLOCKSHIFT;
      yh = (viletryy - bmaporgy + MAXRADIUS*2)>>MAPBLOCKSHIFT;

      vileobj = actor;
      for(bx = xl; bx <= xh; ++bx)
      {
         for(by = yl; by <= yh; ++by)
         {
            // Call PIT_VileCheck to check
            // whether object is a corpse
            // that can be raised.
            if(!P_BlockThingsIterator(bx, by, PIT_VileCheck))
            {
               mobjinfo_t *info;
               
               // got one!
               mobj_t *temp = actor->target;
               actor->target = corpsehit;
               A_FaceTarget(actor);
               actor->target = temp;

               P_SetMobjState(actor, E_SafeState(S_VILE_HEAL1));
               S_StartSound(corpsehit, sfx_slop);
               info = corpsehit->info;

               // haleyjd 09/26/04: need to restore monster skins here
               // in case they were cleared by the thing being crushed
               if(info->altsprite != NUMSPRITES)
                  corpsehit->skin = P_GetMonsterSkin(info->altsprite);
               
               P_SetMobjState(corpsehit, info->raisestate);
               
               if(comp[comp_vile])
                  corpsehit->height <<= 2;                        // phares
               else                                               //   V
               {
                  // fix Ghost bug
                  corpsehit->height = P_ThingInfoHeight(info);
                  corpsehit->radius = info->radius;
               }                                                  // phares
               
               // killough 7/18/98: 
               // friendliness is transferred from AV to raised corpse
               corpsehit->flags = 
                  (info->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);
               
               corpsehit->health = info->spawnhealth;
               P_SetTarget(&corpsehit->target, NULL);  // killough 11/98

               if(demo_version >= 203)
               {         // kilough 9/9/98
                  P_SetTarget(&corpsehit->lastenemy, NULL);
                  corpsehit->flags &= ~MF_JUSTHIT;
               }
               
               // killough 8/29/98: add to appropriate thread
               P_UpdateThinker(&corpsehit->thinker);
               
               return;
            }
         }
      }
   }
   A_Chase(actor);  // Return to normal attack.
}

//
// A_VileStart
//

void A_VileStart(mobj_t *actor)
{
   S_StartSound(actor, sfx_vilatk);
}

//
// A_Fire
// Keep fire in front of player unless out of sight
//

void A_Fire(mobj_t *actor);

void A_StartFire(mobj_t *actor)
{
   S_StartSound(actor,sfx_flamst);
   A_Fire(actor);
}

void A_FireCrackle(mobj_t* actor)
{
   S_StartSound(actor,sfx_flame);
   A_Fire(actor);
}

void A_Fire(mobj_t *actor)
{
   unsigned an;
   mobj_t *dest = actor->tracer;
   
   if(!dest)
      return;
   
   // don't move it if the vile lost sight
   if(!P_CheckSight(actor->target, dest) )
      return;
   
   an = dest->angle >> ANGLETOFINESHIFT;
   
   P_UnsetThingPosition(actor);
   actor->x = dest->x + FixedMul(24*FRACUNIT, finecosine[an]);
   actor->y = dest->y + FixedMul(24*FRACUNIT, finesine[an]);
   actor->z = dest->z;
   P_SetThingPosition(actor);
}

//
// A_VileTarget
// Spawn the hellfire
//

void A_VileTarget(mobj_t *actor)
{
   mobj_t *fog;
   
   if(!actor->target)
      return;

   A_FaceTarget(actor);
   
   // killough 12/98: fix Vile fog coordinates
   fog = P_SpawnMobj(actor->target->x,
                     demo_version < 203 ? actor->target->x : actor->target->y,
                     actor->target->z,E_SafeThingType(MT_FIRE));
   
   P_SetTarget(&actor->tracer, fog);   // killough 11/98
   P_SetTarget(&fog->target, actor);
   P_SetTarget(&fog->tracer, actor->target);
   A_Fire(fog);
}

//
// A_VileAttack
//

void A_VileAttack(mobj_t *actor)
{
   mobj_t *fire;
   int    an;
   
   if(!actor->target)
      return;

   A_FaceTarget(actor);
   
   if(!P_CheckSight(actor, actor->target))
      return;

   S_StartSound(actor, sfx_barexp);
   P_DamageMobj(actor->target, actor, actor, 20, actor->info->mod);
   actor->target->momz = 1000*FRACUNIT/actor->target->info->mass;

   an = actor->angle >> ANGLETOFINESHIFT;
   
   fire = actor->tracer;
   
   if(!fire)
      return;

   // move the fire between the vile and the player
   fire->x = actor->target->x - FixedMul (24*FRACUNIT, finecosine[an]);
   fire->y = actor->target->y - FixedMul (24*FRACUNIT, finesine[an]);
   P_RadiusAttack(fire, actor, 70, actor->info->mod);
}

//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it.              haleyjd: weird BOOM
//                                             comment #345932

#define FATSPREAD       (ANG90/8)

void A_FatRaise(mobj_t *actor)
{
   A_FaceTarget(actor);
   S_StartSound(actor, sfx_manatk);
}

static int FatShotType = -1;

void A_FatAttack1(mobj_t *actor)
{
   mobj_t *mo;
   int    an;
   fixed_t z = actor->z + DEFAULTMISSILEZ;
   
   // haleyjd: no crashing
   if(!actor->target)
      return;

   if(FatShotType == -1)
      FatShotType = E_SafeThingType(MT_FATSHOT);
   
   A_FaceTarget(actor);

   // Change direction  to ...
   actor->angle += FATSPREAD;
   
   P_SpawnMissile(actor, actor->target, FatShotType, z);
   
   mo = P_SpawnMissile(actor, actor->target, FatShotType, z);
   mo->angle += FATSPREAD;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

void A_FatAttack2(mobj_t *actor)
{
   mobj_t *mo;
   int    an;
   fixed_t z = actor->z + DEFAULTMISSILEZ;

   // haleyjd: no crashing
   if(!actor->target)
      return;
   
   if(FatShotType == -1)
      FatShotType = E_SafeThingType(MT_FATSHOT);

   A_FaceTarget(actor);
   // Now here choose opposite deviation.
   actor->angle -= FATSPREAD;
   P_SpawnMissile(actor, actor->target, FatShotType, z);
   
   mo = P_SpawnMissile(actor, actor->target, FatShotType, z);
   mo->angle -= FATSPREAD*2;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

void A_FatAttack3(mobj_t *actor)
{
   mobj_t *mo;
   int    an;
   fixed_t z = actor->z + DEFAULTMISSILEZ;

   // haleyjd: no crashing
   if(!actor->target)
      return;
   
   if(FatShotType == -1)
      FatShotType = E_SafeThingType(MT_FATSHOT);
   
   A_FaceTarget(actor);
   
   mo = P_SpawnMissile(actor, actor->target, FatShotType, z);
   mo->angle -= FATSPREAD/2;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
   
   mo = P_SpawnMissile(actor, actor->target, FatShotType, z);
   mo->angle += FATSPREAD/2;
   an = mo->angle >> ANGLETOFINESHIFT;
   mo->momx = FixedMul(mo->info->speed, finecosine[an]);
   mo->momy = FixedMul(mo->info->speed, finesine[an]);
}

//
// P_SkullFly
//
// haleyjd 08/07/04: generalized code to make a thing skullfly.
// actor->target must be valid.
//
void P_SkullFly(mobj_t *actor, fixed_t speed)
{
   mobj_t *dest;
   angle_t an;
   int     dist;

   dest = actor->target;
   actor->flags |= MF_SKULLFLY;

   A_FaceTarget(actor);
   an = actor->angle >> ANGLETOFINESHIFT;
   actor->momx = FixedMul(speed, finecosine[an]);
   actor->momy = FixedMul(speed, finesine[an]);
   
   dist = P_AproxDistance(getTargetX(actor) - actor->x, 
                          getTargetY(actor) - actor->y);
   dist = dist / speed;
   if(dist < 1)
      dist = 1;

   actor->momz = (getTargetZ(actor)+ (dest->height >> 1) - actor->z) / dist;
}

#define SKULLSPEED              (20*FRACUNIT)

//
// A_SkullAttack
//
// Fly at the player like a missile.
//
void A_SkullAttack(mobj_t *actor)
{   
   if(!actor->target)
      return;
   
   S_StartSound(actor, actor->info->attacksound);

   // haleyjd 08/07/04: use new P_SkullFly function
   P_SkullFly(actor, SKULLSPEED);
}

// sf: removed beta lost soul

void A_Stop(mobj_t *actor)
{
   actor->momx = actor->momy = actor->momz = 0;
}

//
// A_PainShootSkull
//
// Spawn a lost soul and launch it at the target
//
void A_PainShootSkull(mobj_t *actor, angle_t angle)
{
   fixed_t       x,y,z;
   mobj_t        *newmobj;
   angle_t       an;
   int           prestep;
   static int    skullType = -1;
      
   if(skullType == -1)
      skullType = E_SafeThingType(MT_SKULL);

   // The original code checked for 20 skulls on the level,    // phares
   // and wouldn't spit another one if there were. If not in   // phares
   // compatibility mode, we remove the limit.                 // phares

   if(comp[comp_pain])  // killough 10/98: compatibility-optioned
   {
      // count total number of skulls currently on the level
      int count = 20;
      thinker_t *currentthinker;
      for(currentthinker = thinkercap.next;
          currentthinker != &thinkercap;
          currentthinker = currentthinker->next)
      {
         if((currentthinker->function == P_MobjThinker) &&
            ((mobj_t *)currentthinker)->type == skullType)
         {
            if(--count < 0)         // killough 8/29/98: early exit
               return;
         }
      }
   }

   // okay, there's room for another one
   
   an = angle >> ANGLETOFINESHIFT;
   
   prestep = 4*FRACUNIT + 3*(actor->info->radius + mobjinfo[skullType].radius)/2;

   x = actor->x + FixedMul(prestep, finecosine[an]);
   y = actor->y + FixedMul(prestep, finesine[an]);
   z = actor->z + 8*FRACUNIT;
   
   if(comp[comp_skull])   // killough 10/98: compatibility-optioned
      newmobj = P_SpawnMobj(x, y, z, skullType);                    // phares
   else                                                             //   V
   {
      // Check whether the Lost Soul is being fired through a 1-sided
      // wall or an impassible line, or a "monsters can't cross" line.
      // If it is, then we don't allow the spawn. This is a bug fix, 
      // but it should be considered an enhancement, since it may 
      // disturb existing demos, so don't do it in compatibility mode.
      
      if (Check_Sides(actor,x,y))
         return;
      
      newmobj = P_SpawnMobj(x, y, z, skullType);
      
      // Check to see if the new Lost Soul's z value is above the
      // ceiling of its new sector, or below the floor. If so, kill it.
      
      if((newmobj->z >
         (newmobj->subsector->sector->ceilingheight - newmobj->height)) ||
         (newmobj->z < newmobj->subsector->sector->floorheight))
      {
         // kill it immediately
         P_DamageMobj(newmobj,actor,actor,10000,MOD_UNKNOWN);
         return;                                                    //   ^
      }                                                             //   |
   }                                                                // phares

   // killough 7/20/98: PEs shoot lost souls with the same friendliness
   newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (actor->flags & MF_FRIEND);

   // killough 8/29/98: add to appropriate thread
   P_UpdateThinker(&newmobj->thinker);

   // Check for movements.
   // killough 3/15/98: don't jump over dropoffs:

   if(!P_TryMove(newmobj, newmobj->x, newmobj->y, false))
   {
      // kill it immediately
      P_DamageMobj(newmobj, actor, actor, 10000, MOD_UNKNOWN);
      return;
   }
   
   P_SetTarget(&newmobj->target, actor->target);
   A_SkullAttack(newmobj);
}

//
// A_PainAttack
// Spawn a lost soul and launch it at the target
//

void A_PainAttack(mobj_t *actor)
{
   if(!actor->target)
      return;
   A_FaceTarget(actor);
   A_PainShootSkull(actor, actor->angle);
}

void A_PainDie(mobj_t *actor)
{
   A_Fall(actor);
   A_PainShootSkull(actor, actor->angle + ANG90);
   A_PainShootSkull(actor, actor->angle + ANG180);
   A_PainShootSkull(actor, actor->angle + ANG270);
}

void A_Scream(mobj_t *actor)
{
   int sound;
   
   switch(actor->info->deathsound)
   {
   case 0:
      return;

   case sfx_podth1:
   case sfx_podth2:
   case sfx_podth3:
      sound = sfx_podth1 + P_Random(pr_scream)%3;
      break;
      
   case sfx_bgdth1:
   case sfx_bgdth2:
      sound = sfx_bgdth1 + P_Random(pr_scream)%2;
      break;
      
   default:
      sound = actor->info->deathsound;
      break;
   }

   // Check for bosses.
   // haleyjd: generalize to all bosses
   if(actor->flags2 & MF2_BOSS)
      S_StartSound(NULL, sound); // full volume
   else
      S_StartSound(actor, sound);
}

void A_XScream(mobj_t *actor)
{
   int sound = GameModeInfo->playerSounds[sk_slop];
   
   // haleyjd: falling damage
   if(!comp[comp_fallingdmg] && demo_version >= 329)
   {
      if(actor->player && actor->intflags & MIF_DIEDFALLING)
         sound = GameModeInfo->playerSounds[sk_fallht];
   }
   
   S_StartSound(actor, sound);
}

void A_Pain(mobj_t *actor)
{
   S_StartSound(actor, actor->info->painsound);
}

void A_Fall(mobj_t *actor)
{
   // actor is on ground, it can be walked over
   actor->flags &= ~MF_SOLID;
}

// killough 11/98: kill an object
void A_Die(mobj_t *actor)
{
   actor->flags2 &= ~MF2_INVULNERABLE;  // haleyjd: just in case
   P_DamageMobj(actor, NULL, NULL, actor->health, MOD_UNKNOWN);
}

//
// A_Explode
//
void A_Explode(mobj_t *thingy)
{
   P_RadiusAttack(thingy, thingy->target, 128, thingy->info->mod);

   if(thingy->z <= thingy->secfloorz + 128*FRACUNIT)
      E_HitWater(thingy, thingy->subsector->sector);
}

void A_Nailbomb(mobj_t *thing)
{
   int i;
   
   P_RadiusAttack(thing, thing->target, 128, thing->info->mod);

   // haleyjd: added here as of 3.31b3 -- was overlooked
   if(demo_version >= 331 && thing->z <= thing->secfloorz + 128*FRACUNIT)
      E_HitWater(thing, thing->subsector->sector);

   for(i = 0; i < 30; ++i)
      P_LineAttack(thing, i*(ANG180/15), MISSILERANGE, 0, 10);
}


//
// A_Detonate
// killough 8/9/98: same as A_Explode, except that the damage is variable
//

void A_Detonate(mobj_t *mo)
{
   P_RadiusAttack(mo, mo->target, mo->damage, mo->info->mod);

   // haleyjd: added here as of 3.31b3 -- was overlooked
   if(demo_version >= 331 && mo->z <= mo->secfloorz + mo->damage*FRACUNIT)
      E_HitWater(mo, mo->subsector->sector);
}

//
// killough 9/98: a mushroom explosion effect, sorta :)
// Original idea: Linguica
//

void A_Mushroom(mobj_t *actor)
{
   int i, j, n = actor->damage;
   int arg0, ShotType;
   
   // Mushroom parameters are part of code pointer's state
   fixed_t misc1 = 
      actor->state->misc1 ? actor->state->misc1 : FRACUNIT*4;
   fixed_t misc2 = 
      actor->state->misc2 ? actor->state->misc2 : FRACUNIT/2;

   if(FatShotType == -1)
      FatShotType = E_SafeThingType(MT_FATSHOT);

   // haleyjd: extended parameter support requested by Mordeth:
   // allow specification of thing type in args[0]   
   if((arg0 = actor->state->args[0]))
      ShotType = E_SafeThingType(arg0);
   else
      ShotType = FatShotType;
   
   A_Explode(actor);               // make normal explosion

   for(i = -n; i <= n; i += 8)    // launch mushroom cloud
   {
      for(j = -n; j <= n; j += 8)
      {
         mobj_t target = *actor, *mo;
         target.x += i << FRACBITS;    // Aim in many directions from source
         target.y += j << FRACBITS;
         target.z += P_AproxDistance(i,j) * misc1;         // Aim fairly high
         mo = P_SpawnMissile(actor, &target, ShotType,
                             actor->z + DEFAULTMISSILEZ);  // Launch fireball
         mo->momx = FixedMul(mo->momx, misc2);
         mo->momy = FixedMul(mo->momy, misc2);             // Slow down a bit
         mo->momz = FixedMul(mo->momz, misc2);
         mo->flags &= ~MF_NOGRAVITY;   // Make debris fall under gravity
      }
   }
}

typedef struct
{
   unsigned int thing_flag;
   unsigned int level_flag;
} boss_spec_t;

#define NUM_BOSS_SPECS 7

static boss_spec_t boss_specs[NUM_BOSS_SPECS] =
{
   { MF2_MAP07BOSS1, BSPEC_MAP07_1 },
   { MF2_MAP07BOSS2, BSPEC_MAP07_2 },
   { MF2_E1M8BOSS,   BSPEC_E1M8 },
   { MF2_E2M8BOSS,   BSPEC_E2M8 },
   { MF2_E3M8BOSS,   BSPEC_E3M8 },
   { MF2_E4M6BOSS,   BSPEC_E4M6 },
   { MF2_E4M8BOSS,   BSPEC_E4M8 },
};

//
// A_BossDeath
//
// Possibly trigger special effects if on boss level
//
// haleyjd: enhanced to allow user specification of the thing types
//          allowed to trigger each special effect.
// haleyjd: 03/14/05 -- enhanced to allow actions on any map.
//
void A_BossDeath(mobj_t *mo)
{
   thinker_t *th;
   line_t    junk;
   int       i;

   // make sure there is a player alive for victory
   for(i = 0; i < MAXPLAYERS; ++i)
   {
      if(playeringame[i] && players[i].health > 0)
         break;
   }
   
   // no one left alive, so do not end game
   if(i == MAXPLAYERS)
      return;

   for(i = 0; i < NUM_BOSS_SPECS; ++i)
   {
      // to activate a special, the thing must be a boss that triggers
      // it, and the map must have the special enabled.
      if((mo->flags2 & boss_specs[i].thing_flag) &&
         (LevelInfo.bossSpecs & boss_specs[i].level_flag))
      {
         // scan the remaining thinkers to see if all bosses are dead
         for(th = thinkercap.next; th != &thinkercap; th = th->next)
         {
            if(th->function == P_MobjThinker)
            {
               mobj_t *mo2 = (mobj_t *)th;
               if(mo2 != mo && 
                  (mo2->flags2 & boss_specs[i].thing_flag) && 
                  mo2->health > 0)
                  return;         // other boss not dead
            }
         }

         // victory!
         switch(boss_specs[i].level_flag)
         {
         case BSPEC_E1M8:
         case BSPEC_E4M8:
         case BSPEC_MAP07_1:
            // lower floors tagged 666 to lowest neighbor
            junk.tag = 666;
            EV_DoFloor(&junk, lowerFloorToLowest);
            break;
         case BSPEC_MAP07_2:
            // raise floors tagged 667 by shortest lower texture
            junk.tag = 667;
            EV_DoFloor(&junk, raiseToTexture);
            break;
         case BSPEC_E2M8:
         case BSPEC_E3M8:
            // exit map -- no use processing any further after this
            G_ExitLevel();
            return;
         case BSPEC_E4M6:
            // open sectors tagged 666 as blazing doors
            junk.tag = 666;
            EV_DoDoor(&junk, blazeOpen);
            break;
         default:
            break;
         } // end switch
      } // end if
   } // end for
}

void A_Hoof(mobj_t* mo)
{
   S_StartSound(mo, sfx_hoof);
   A_Chase(mo);
}

void A_Metal(mobj_t *mo)
{
   S_StartSound(mo, sfx_metal);
   A_Chase(mo);
}

void A_BabyMetal(mobj_t *mo)
{
   S_StartSound(mo, sfx_bspwlk);
   A_Chase(mo);
}

// killough 2/7/98: Remove limit on icon landings:
// haleyjd 07/30/04: use new MobjCollection

MobjCollection braintargets;

struct brain_s brain;   // killough 3/26/98: global state of boss brain

// killough 3/26/98: initialize icon landings at level startup,
// rather than at boss wakeup, to prevent savegame-related crashes

void P_SpawnBrainTargets(void)  // killough 3/26/98: renamed old function
{
   int BrainSpotType = E_ThingNumForDEHNum(MT_BOSSTARGET);

   // find all the target spots
   P_ReInitMobjCollection(&braintargets, BrainSpotType);

   brain.easy = 0;   // killough 3/26/98: always init easy to 0

   if(BrainSpotType == NUMMOBJTYPES)
      return;

   P_CollectThings(&braintargets);
}

// haleyjd 07/30/04: P_CollectThings moved to p_mobj.c

void A_BrainAwake(mobj_t *mo)
{
   S_StartSound(NULL,sfx_bossit); // killough 3/26/98: only generates sound now
}

void A_BrainPain(mobj_t *mo)
{
   S_StartSound(NULL,sfx_bospn);
}

void A_BrainScream(mobj_t *mo)
{
   int x;
   static int rocketType = -1;
   
   if(rocketType == -1)
      rocketType = E_SafeThingType(MT_ROCKET);

   for(x=mo->x - 196*FRACUNIT ; x< mo->x + 320*FRACUNIT ; x+= FRACUNIT*8)
   {
      int y = mo->y - 320*FRACUNIT;
      int z = 128 + P_Random(pr_brainscream)*2*FRACUNIT;
      mobj_t *th = P_SpawnMobj (x,y,z, rocketType);
      // haleyjd 02/21/05: disable particle events/effects for this thing
      th->intflags |= MIF_NOPTCLEVTS;
      th->effects = 0;
      th->momz = P_Random(pr_brainscream)*512;
      P_SetMobjState(th, E_SafeState(S_BRAINEXPLODE1));
      th->tics -= P_Random(pr_brainscream)&7;
      if(th->tics < 1)
         th->tics = 1;
   }
   S_StartSound(NULL,sfx_bosdth);
}

void A_BrainExplode(mobj_t *mo)
{  
   // haleyjd 08/05/04: use new function
   int x = mo->x + P_SubRandom(pr_brainexp)*2048;
   int y = mo->y;
   int z = 128 + P_Random(pr_brainexp)*2*FRACUNIT;

   mobj_t *th = P_SpawnMobj(x, y, z, E_SafeThingType(MT_ROCKET));
   th->momz = P_Random(pr_brainexp)*512;
   // haleyjd 02/21/05: disable particle events/effects for this thing
   th->intflags |= MIF_NOPTCLEVTS;
   th->effects = 0;
   P_SetMobjState(th, E_SafeState(S_BRAINEXPLODE1));
   
   th->tics -= P_Random(pr_brainexp) & 7;
   if(th->tics < 1)
      th->tics = 1;
}

void A_BrainDie(mobj_t *mo)
{
   G_ExitLevel();
}

void A_BrainSpit(mobj_t *mo)
{
   mobj_t *targ, *newmobj;
   static int SpawnShotType = -1;
   
   if(SpawnShotType == -1)
      SpawnShotType = E_SafeThingType(MT_SPAWNSHOT);
   
    // killough 4/1/98: ignore if no targets
   if(P_CollectionIsEmpty(&braintargets))
      return;

   brain.easy ^= 1;          // killough 3/26/98: use brain struct
   if(gameskill <= sk_easy && !brain.easy)
      return;

   // shoot a cube at current target
   targ = P_CollectionWrapIterator(&braintargets);

   // spawn brain missile
   newmobj = P_SpawnMissile(mo, targ, SpawnShotType, 
                            mo->z + DEFAULTMISSILEZ);
   P_SetTarget(&newmobj->target, targ);
   newmobj->reactiontime = (short)(((targ->y-mo->y)/newmobj->momy)/newmobj->state->tics);

   // killough 7/18/98: brain friendliness is transferred
   newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);
   
   // killough 8/29/98: add to appropriate thread
   P_UpdateThinker(&newmobj->thinker);
   
   S_StartSound(NULL, sfx_bospit);
}

void A_SpawnFly(mobj_t *mo);

// travelling cube sound
void A_SpawnSound(mobj_t *mo)
{
   S_StartSound(mo,sfx_boscub);
   A_SpawnFly(mo);
}

// haleyjd 07/13/03: editable boss brain spawn types
// schepe: removed 11-type limit
int NumBossTypes;
int *BossSpawnTypes;
int *BossSpawnProbs;

void A_SpawnFly(mobj_t *mo)
{
   int    i;         // schepe 
   mobj_t *newmobj;  // killough 8/9/98
   int    r;
   mobjtype_t type = 0;
   static int fireType = -1;
      
   mobj_t *fog;
   mobj_t *targ;

   // haleyjd 05/31/06: allow 0 boss types
   if(NumBossTypes == 0)
      return;

   if(fireType == -1)
      fireType = E_SafeThingType(MT_SPAWNFIRE);

   if(--mo->reactiontime)
      return;     // still flying

   // haleyjd: do not crash if target is null
   if(!(targ = mo->target))
      return;
   
   // First spawn teleport fog.
   fog = P_SpawnMobj(targ->x, targ->y, targ->z, fireType);
   
   S_StartSound(fog, sfx_telept);
   
   // Randomly select monster to spawn.
   r = P_Random(pr_spawnfly);

   // Probability distribution (kind of :), decreasing likelihood.
   // schepe:
   for(i = 0; i < NumBossTypes; ++i)
   {
      if(r < BossSpawnProbs[i])
      {
         type = BossSpawnTypes[i];
         break;
      }
   }

   newmobj = P_SpawnMobj(targ->x, targ->y, targ->z, type);
   
   // killough 7/18/98: brain friendliness is transferred
   newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);

   // killough 8/29/98: add to appropriate thread
   P_UpdateThinker(&newmobj->thinker);
   
   if(P_LookForTargets(newmobj,true))      // killough 9/4/98
      P_SetMobjState(newmobj, newmobj->info->seestate);
   
   // telefrag anything in this spot
   P_TeleportMove(newmobj, newmobj->x, newmobj->y, true); // killough 8/9/98
   
   // remove self (i.e., cube).
   P_RemoveMobj(mo);
}

void A_PlayerScream(mobj_t *mo)
{
   int sound;

   if(mo->player && strcasecmp(mo->player->skin->sounds[sk_plwdth], "none") &&
      mo->intflags & MIF_WIMPYDEATH)
   {
      // haleyjd 09/29/07: wimpy death, if supported
      sound = sk_plwdth;
   }
   else if(GameModeInfo->id == shareware || mo->health >= -50)
   {
      // Default death sound
      sound = sk_pldeth; 
   }
   else
   {
      // IF THE PLAYER DIES LESS THAN -50% WITHOUT GIBBING
      sound = sk_pdiehi;
   }

   // if died falling, gross falling death sound
   if(!comp[comp_fallingdmg] && demo_version >= 329 &&
      mo->intflags & MIF_DIEDFALLING)
      sound = sk_fallht;
      
   S_StartSound(mo, GameModeInfo->playerSounds[sound]);
}

//
// A_KeenDie
//
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie(mobj_t* mo)
{
   thinker_t *th;
   line_t   junk;
   
   A_Fall(mo);

   // scan the remaining thinkers to see if all Keens are dead
   
   for(th = thinkercap.next; th != &thinkercap; th = th->next)
   {
      if(th->function == P_MobjThinker)
      {
         mobj_t *mo2 = (mobj_t *) th;
         if(mo2 != mo && mo2->type == mo->type && mo2->health > 0)
            return;                           // other Keen not dead
      }
   }

   junk.tag = 666;
   EV_DoDoor(&junk,doorOpen);
}

//
// killough 11/98
//
// The following were inspired by Len Pitre
//
// A small set of highly-sought-after code pointers
//

void A_Spawn(mobj_t *mo)
{
   if(mo->state->misc1)
   {
      mobj_t *newmobj;

      // haleyjd 03/06/03 -- added error check
      //         07/05/03 -- adjusted for EDF
      int thingtype = E_SafeThingType((int)(mo->state->misc1));
      
      newmobj = 
         P_SpawnMobj(mo->x, mo->y, 
                     (mo->state->misc2 << FRACBITS) + mo->z,
                     thingtype);
      if(newmobj)
         newmobj->flags = (newmobj->flags & ~MF_FRIEND) | (mo->flags & MF_FRIEND);
   }
}

void A_Turn(mobj_t *mo)
{
   mo->angle += (angle_t)(((uint64_t) mo->state->misc1 << 32) / 360);
}

void A_Face(mobj_t *mo)
{
   mo->angle = (angle_t)(((uint64_t) mo->state->misc1 << 32) / 360);
}

E_Keyword_t kwds_A_Scratch[] =
{
   { "usemisc1",           0  },
   { "usedamage",          1  },
   { "usecounter",         2  },
   { NULL }
};

//
// A_Scratch
//
// Parameterized melee attack.
// * misc1 == constant damage amount
// * misc2 == optional sound deh num to play
//
// haleyjd 08/02/04: extended parameters:
// * args[0] == special mode select
//              * 0 == compatibility (use misc1 like normal)
//              * 1 == use mo->damage
//              * 2 == use counter specified in args[1]
// * args[1] == counter number for mode 2
//
void A_Scratch(mobj_t *mo)
{
   int damage, cnum;

   // haleyjd: demystified
   if(!mo->target)
      return;

   // haleyjd 08/02/04: extensions to get damage from multiple sources
   switch(mo->state->args[0])
   {
   default:
   case 0: // default, compatibility mode
      damage = mo->state->misc1;
      break;
   case 1: // use mo->damage
      damage = mo->damage;
      break;
   case 2: // use a counter
      cnum = mo->state->args[1];

      if(cnum < 0 || cnum >= NUMMOBJCOUNTERS)
         return; // invalid

      damage = mo->counters[cnum];
      break;
   }

   A_FaceTarget(mo);

   if(P_CheckMeleeRange(mo))
   {
      if(mo->state->misc2)
         S_StartSound(mo, mo->state->misc2);

      P_DamageMobj(mo->target, mo, mo, damage, MOD_HIT);
   }
}

E_Keyword_t kwds_A_PlaySound[] =
{
   { "normal",             0  },
   { "fullvolume",         1  },
   { NULL }
};

void A_PlaySound(mobj_t *mo)
{
   S_StartSound(mo->state->misc2 ? NULL : mo, mo->state->misc1);
}

void A_RandomJump(mobj_t *mo)
{
   // haleyjd 03/06/03: rewrote to be failsafe
   //         07/05/03: adjusted for EDF
   int statenum = mo->state->misc1;

   statenum = E_StateNumForDEHNum(statenum);
   if(statenum == NUMSTATES)
      return;

   if(P_Random(pr_randomjump) < mo->state->misc2)
      P_SetMobjState(mo, statenum);
}

//
// This allows linedef effects to be activated inside deh frames.
//

void A_LineEffect(mobj_t *mo)
{
   // haleyjd 05/02/04: bug fix:
   // This line can end up being referred to long after this
   // function returns, thus it must be made static or memory
   // corruption is possible.
   static line_t junk;

   if(!(mo->intflags & MIF_LINEDONE))                // Unless already used up
   {
      junk = *lines;                                 // Fake linedef set to 1st
      if((junk.special = (short)mo->state->misc1))   // Linedef type
      {
         player_t player, *oldplayer = mo->player;   // Remember player status
         mo->player = &player;                       // Fake player
         player.health = 100;                        // Alive player
         junk.tag = (short)mo->state->misc2;         // Sector tag for linedef
         if(!P_UseSpecialLine(mo, &junk, 0))         // Try using it
            P_CrossSpecialLine(&junk, 0, mo);        // Try crossing it
         if(!junk.special)                           // If type cleared,
            mo->intflags |= MIF_LINEDONE;            // no more for this thing
         mo->player = oldplayer;                     // Restore player status
      }
   }
}

//
// haleyjd: Start Eternity Engine action functions
//

//
// A_SetFlags
//
// A parameterized codepointer that turns on thing flags
//
// args[0] == 1, 2, 3, 4 -- flags field to affect
// args[1] == flags value to OR with thing flags
//
void A_SetFlags(mobj_t *actor)
{
   int flagfield = actor->state->args[0];
   unsigned int flags = (unsigned int)(actor->state->args[1]);

   switch(flagfield)
   {
   case 1:
      actor->flags |= flags;
      break;
   case 2:
      actor->flags2 |= flags;
      break;
   case 3:
      actor->flags3 |= flags;
      break;
   case 4:
      actor->flags4 |= flags;
      break;
   }
}

//
// A_UnSetFlags
//
// A parameterized codepointer that turns off thing flags
//
// args[0] == 1, 2, 3, 4 -- flags field to affect
// args[1] == flags value to inverse AND with thing flags
//
void A_UnSetFlags(mobj_t *actor)
{
   int flagfield = actor->state->args[0];
   unsigned int flags = (unsigned int)(actor->state->args[1]);

   switch(flagfield)
   {
   case 1:
      actor->flags &= ~flags;
      break;
   case 2:
      actor->flags2 &= ~flags;
      break;
   case 3:
      actor->flags3 &= ~flags;
      break;
   case 4:
      actor->flags4 &= ~flags;
      break;
   }
}

// haleyjd: add back for mbf dehacked patch compatibility
//          might be a useful function to someone, anyway

//
// A_BetaSkullAttack()
//
// killough 10/98: this emulates the beta version's lost soul attacks
//
void A_BetaSkullAttack(mobj_t *actor)
{
   int damage;
   
   // haleyjd: changed to check if objects are the SAME type, not
   // for hard-coded lost soul
   if(!actor->target || actor->target->type == actor->type)
      return;
   
   S_StartSound(actor, actor->info->attacksound);
   
   A_FaceTarget(actor);
   damage = (P_Random(pr_skullfly)%8+1)*actor->damage;
   P_DamageMobj(actor->target, actor, actor, damage, actor->info->mod);
}

E_Keyword_t kwds_A_StartScript[] =
{
   { "gamescript",         0  },
   { "levelscript",        1  },
   { NULL }
};

//
// A_StartScript
//
// Parameterized codepointer for starting Small scripts
//
// args[0] - script number to start
// args[1] - select vm (0 == gamescript, 1 == levelscript)
// args[2-4] - parameters to script (must accept 3 params)
//
void A_StartScript(mobj_t *actor)
{
   SmallContext_t *rootContext, *useContext;
   SmallContext_t newContext;
   int scriptnum = (int)(actor->state->args[0]);
   int selectvm  = (int)(actor->state->args[1]);

   cell params[3] =
   {
      (cell)(actor->state->args[2]),
      (cell)(actor->state->args[3]),
      (cell)(actor->state->args[4]),
   };

   // determine root context to use
   switch(selectvm)
   {
   default:
   case 0: // game script
      if(!gameScriptLoaded)
         return;
      rootContext = curGSContext;
      break;
   case 1: // level script
      if(!levelScriptLoaded)
         return;
      rootContext = curLSContext;
      break;
   }

   // possibly create a child context for the selected VM
   useContext = SM_CreateChildContext(rootContext, &newContext);

   // set invocation data
   useContext->invocationData.invokeType = SC_INVOKE_THING;
   useContext->invocationData.trigger = actor;

   // execute
   SM_ExecScriptByNum(&useContext->smallAMX, scriptnum, 3, params);

   // clear invocation data
   SM_ClearInvocation(useContext);

   // destroy any child context that might have been created
   SM_DestroyChildContext(useContext);
}

E_Keyword_t kwds_A_PlayerStartScript[] =
{
   { "gamescript",         0  },
   { "levelscript",        1  },
   { NULL }
};

//
// A_PlayerStartScript
//
// Parameterized player gun frame codepointer for starting
// Small scripts
//
// args[0] - script number to start
// args[1] - select vm (0 == gamescript, 1 == levelscript)
// args[2-4] - parameters to script (must accept 3 params)
//
void A_PlayerStartScript(mobj_t *mo)
{
   SmallContext_t *rootContext, *useContext;
   SmallContext_t newContext;
   int scriptnum, selectvm;
   player_t *player;
   pspdef_t *psp;
   cell params[3];

   if(!(player = mo->player))
      return;

   psp = &player->psprites[player->curpsprite];

   scriptnum =  (int)(psp->state->args[0]);
   selectvm  =  (int)(psp->state->args[1]);
   params[0] = (cell)(psp->state->args[2]);
   params[1] = (cell)(psp->state->args[3]);
   params[2] = (cell)(psp->state->args[4]);

   // determine root context to use
   switch(selectvm)
   {
   default:
   case 0: // game script
      if(!gameScriptLoaded)
         return;
      rootContext = curGSContext;
      break;
   case 1: // level script
      if(!levelScriptLoaded)
         return;
      rootContext = curLSContext;
      break;
   }

   // possibly create a child context for the selected VM
   useContext = SM_CreateChildContext(rootContext, &newContext);

   // set invocation data
   useContext->invocationData.invokeType = SC_INVOKE_PLAYER;
   useContext->invocationData.playernum = (int)(player - players);
   useContext->invocationData.trigger = mo;

   // execute
   SM_ExecScriptByNum(&useContext->smallAMX, scriptnum, 3, params);

   // clear invocation data
   SM_ClearInvocation(useContext);

   // destroy any child context that might have been created
   SM_DestroyChildContext(useContext);
}

//
// A_FaceMoveDir
//
// Face a walking object in the direction it is moving.
// haleyjd TODO: this is not documented or available in BEX yet.
//
void A_FaceMoveDir(mobj_t *actor)
{
   angle_t moveangles[NUMDIRS] = { 0, 32, 64, 96, 128, 160, 192, 224 };

   if(actor->movedir != DI_NODIR)
      actor->angle = moveangles[actor->movedir] << 24;
}

//
// A_GenRefire
//
// Generalized refire check pointer, requested by Kate.
//
// args[0] : state to branch to when ceasing to fire
// args[1] : random chance of still firing if target out of sight
//
void A_GenRefire(mobj_t *actor)
{
   int statenum;

   statenum = E_SafeState(actor->state->args[0]);

   A_FaceTarget(actor);

   // check for friends in the way
   if(actor->flags & MF_FRIEND && P_HitFriend(actor))
   {
      P_SetMobjState(actor, statenum);
      return;
   }

   // random chance of continuing to fire
   if(P_Random(pr_genrefire) < actor->state->args[1])
      return;

   if(!actor->target || actor->target->health <= 0 ||
      (actor->flags & actor->target->flags & MF_FRIEND) ||
      !P_CheckSight(actor, actor->target))
   {
      P_SetMobjState(actor, statenum);
   }
}


//
// haleyjd: Start Eternity TC Action Functions
// TODO: possibly eliminate some of these
//

//
// A_FogSpawn
//
// The slightly-more-complicated-than-you-thought Hexen fog cloud generator
// Modified to use random initializer values as opposed to actor->args[]
// set in mapthing data fields
//
#define FOGSPEED 2
#define FOGFREQUENCY 8

void A_FogSpawn(mobj_t *actor)
{
   mobj_t *mo = NULL;
   angle_t delta;

   if(actor->counters[0]-- > 0)
     return;

   actor->counters[0] = FOGFREQUENCY; // reset frequency

   switch(P_Random(pr_cloudpick)%3)
   {
   case 0:
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, 
         E_SafeThingType(MT_FOGPATCHS));
      break;
   case 1:
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, 
         E_SafeThingType(MT_FOGPATCHM));
      break;
   case 2:
      mo = P_SpawnMobj(actor->x, actor->y, actor->z, 
         E_SafeThingType(MT_FOGPATCHL));
      break;
   }
   if(mo)
   {
      delta = (P_Random(pr_fogangle)&0x7f)+1;
      mo->angle = actor->angle + (((P_Random(pr_fogangle)%delta)-(delta>>1))<<24);
      mo->target = actor;
      mo->args[0] = (P_Random(pr_fogangle)%FOGSPEED)+1; // haleyjd: narrowed range
      mo->args[3] = (P_Random(pr_fogcount)&0x7f)+1;
      mo->args[4] = 1;
      mo->counters[1] = P_Random(pr_fogfloat)&63;
   }
}

//
// A_FogMove
//

void A_FogMove(mobj_t *actor)
{
   int speed = actor->args[0]<<FRACBITS;
   angle_t angle;
   int weaveindex;

   if(!(actor->args[4]))
     return;

   if(actor->args[3]-- <= 0)
   {
      P_SetMobjStateNF(actor, actor->info->deathstate);
      return;
   }

   if((actor->args[3]%4) == 0)
   {
      weaveindex = actor->counters[1];
      actor->z += FloatBobOffsets[weaveindex]>>1;
      actor->counters[1] = (weaveindex+1)&63;
   }
   angle = actor->angle>>ANGLETOFINESHIFT;
   actor->momx = FixedMul(speed, finecosine[angle]);
   actor->momy = FixedMul(speed, finesine[angle]);
}

#if 0
void P_ClericTeleport(mobj_t *actor)
{
   bossteleport_t bt;

   if(actor->flags2&MF2_INVULNERABLE)
     P_ClericSparkle(actor);

   bt.mc        = &braintargets;            // use braintargets
   bt.rngNum    = pr_clr2choose;            // use this rng
   bt.boss      = actor;                    // teleport leader cleric
   bt.state     = E_SafeState(S_LCLER_TELE1); // put cleric in this state
   bt.fxtype    = E_SafeThingType(MT_IFOG); // spawn item fog for fx
   bt.zpamt     = 24*FRACUNIT;              // add 24 units to fog z 
   bt.hereThere = BOSSTELE_BOTH;            // spawn fx @ origin & dest.
   bt.soundNum  = sfx_itmbk;                // use item respawn sound

   P_BossTeleport(&bt);
}
#endif

void A_DwarfAlterEgoChase(mobj_t *actor)
{
   if(actor->counters[0])
      actor->counters[0]--;
   else
   {
      A_Die(actor);
      return;
   }
   A_Chase(actor);
}

//=============================================================================
//
// Console Commands for Things
//

// haleyjd 07/05/03: new console commands that can use
// EDF thing type names instead of internal type numbers

extern int *deh_ParseFlagsCombined(const char *strval);

static void P_ConsoleSummon(int type, angle_t an, int flagsmode, const char *flags)
{
   static int fountainType = -1;
   static int dripType = -1;
   static int ambienceType = -1;
   static int enviroType = -1;
   static int vileFireType = -1;
   static int spawnSpotType = -1;

   fixed_t  x, y, z;
   mobj_t   *newmobj;
   int      prestep;
   player_t *plyr = &players[consoleplayer];

   // resolve EDF types (done once for efficiency)
   if(fountainType == -1)
   {
      fountainType  = E_ThingNumForName("EEParticleFountain");
      dripType      = E_ThingNumForName("EEParticleDrip");
      ambienceType  = E_ThingNumForName("EEAmbience");
      enviroType    = E_ThingNumForName("EEEnviroSequence");
      vileFireType  = E_ThingNumForName("VileFire");
      spawnSpotType = E_ThingNumForName("BossSpawnSpot");
   }

   // if it's a missile, shoot it
   if(mobjinfo[type].flags & MF_MISSILE)
   {
      newmobj = P_SpawnPlayerMissile(plyr->mo, type);

      // set the tracer target in case it is a homing missile
      P_BulletSlope(plyr->mo);
      if(tm->linetarget)
         P_SetTarget(&newmobj->tracer, tm->linetarget);
   }
   else
   {
      // do a good old Pain-Elemental style summoning
      an = (plyr->mo->angle + an) >> ANGLETOFINESHIFT;
      prestep = 4*FRACUNIT + 3*(plyr->mo->info->radius + mobjinfo[type].radius)/2;
      
      x = plyr->mo->x + FixedMul(prestep, finecosine[an]);
      y = plyr->mo->y + FixedMul(prestep, finesine[an]);
      
      z = (mobjinfo[type].flags & MF_SPAWNCEILING) ? ONCEILINGZ : ONFLOORZ;
      
      if(Check_Sides(plyr->mo, x, y))
         return;
      
      newmobj = P_SpawnMobj(x, y, z, type);
      
      newmobj->angle = plyr->mo->angle;
   }

   // tweak the object's flags
   if(flagsmode != -1)
   {
      int *res = deh_ParseFlagsCombined(flags);

      switch(flagsmode)
      {
      case 0: // set flags
         newmobj->flags  = res[0];
         newmobj->flags2 = res[1];
         newmobj->flags3 = res[2];
         newmobj->flags4 = res[3];
         break;
      case 1: // add flags
         newmobj->flags  |= res[0];
         newmobj->flags2 |= res[1];
         newmobj->flags3 |= res[2];
         newmobj->flags4 |= res[3];
         break;
      case 2: // rem flags
         newmobj->flags  &= ~res[0];
         newmobj->flags2 &= ~res[1];
         newmobj->flags3 &= ~res[2];
         newmobj->flags4 &= ~res[3];
         break;
      default:
         break;
      }
   }

   // code to tweak interesting objects
   if(type == vileFireType)
   {
      P_BulletSlope(plyr->mo);
      if(tm->linetarget)
      {
         P_SetTarget(&newmobj->target, plyr->mo);
         P_SetTarget(&newmobj->tracer, tm->linetarget);
         A_Fire(newmobj);
      }
   }

   // code to make spawning parameterized objects more useful
      
   // fountain: random color
   if(type == fountainType)
   {
      int ft = 9027 + M_Random() % 7;
      
      newmobj->effects |= (ft - 9026) << FX_FOUNTAINSHIFT;
   }

   // drip: random parameters
   if(type == dripType)
   {
      newmobj->args[0] = M_Random();
      newmobj->args[1] = (M_Random() % 8) + 1;
      newmobj->args[2] = M_Random() + 35;
      newmobj->args[3] = 1;
   }

   // ambience: cycle through first 64 ambience #'s
   if(type == ambienceType)
   {
      static int ambnum;

      newmobj->args[0] = ambnum++;

      if(ambnum == 64)
         ambnum = 0;
   }

   if(type == enviroType)
   {
      static int envnum;

      newmobj->args[0] = envnum++;

      if(envnum == 64)
         envnum = 0;
   }

   if(type == spawnSpotType)
      P_SpawnBrainTargets();

   // adjust count* flags to avoid messing up the map

   if(newmobj->flags & MF_COUNTKILL)
   {
     newmobj->flags &= ~MF_COUNTKILL;
     newmobj->flags3 |= MF3_KILLABLE;
   }
   
   if(newmobj->flags & MF_COUNTITEM)
      newmobj->flags &= ~MF_COUNTITEM;
   
   // 06/09/02: set player field for voodoo dolls
   if(E_IsPlayerClassThingType(newmobj->type))
      newmobj->player = plyr;
   
   // killough 8/29/98: add to appropriate thread
   P_UpdateThinker(&newmobj->thinker);
}

CONSOLE_COMMAND(summon, cf_notnet|cf_level|cf_hidden)
{
   int type;
   int flagsmode = -1;
   const char *flags = NULL;

   if(!c_argc)
   {
      C_Printf("usage: summon thingtype flags mode\n");
      return;
   }

   if(c_argc >= 2)
   {
      flagsmode = 1;
      flags = c_argv[1];
   }

   if(c_argc >= 3)
   {
      if(!strcasecmp(c_argv[2], "set"))
         flagsmode = 0; // set
      else if(!strcasecmp(c_argv[2], "remove"))
         flagsmode = 2; // remove
   }

   if((type = E_ThingNumForName(c_argv[0])) == NUMMOBJTYPES)
   {
      C_Printf("unknown thing type\n");
      return;
   }

   P_ConsoleSummon(type, 0, flagsmode, flags);
}

CONSOLE_COMMAND(viles, cf_notnet|cf_level|cf_hidden)
{
   // only in DOOM II ;)
   if(GameModeInfo->id == commercial)
   {
      int vileType = E_ThingNumForName("Archvile");
      
      S_ChangeMusicNum(mus_stalks, true);
      
      P_ConsoleSummon(vileType,  0,     1, "FRIEND");
      P_ConsoleSummon(vileType,  ANG45, 1, "FRIEND");
      P_ConsoleSummon(vileType, -ANG45, 1, "FRIEND");
   }
}

CONSOLE_COMMAND(give, cf_notnet|cf_level)
{
   int i;
   int itemnum;
   int thingnum;
   player_t *plyr = &players[consoleplayer];

   if(!c_argc)
      return;
   
   thingnum = E_ThingNumForName(c_argv[0]);
   if(thingnum == NUMMOBJTYPES)
   {
      C_Printf("unknown thing type\n");
      return;
   }
   if(!(mobjinfo[thingnum].flags & MF_SPECIAL))
   {
      C_Printf("thing type is not a special\n");
      return;
   }
   itemnum = (c_argc >= 2) ? atoi(c_argv[1]) : 1;

   for(i = 0; i < itemnum; i++)
   {
      mobj_t *mo = P_SpawnMobj(plyr->mo->x, plyr->mo->y, plyr->mo->z,
                               thingnum);
      mo->flags &= ~(MF_SPECIAL|MF_COUNTITEM);

      P_TouchSpecialThing(mo, plyr->mo);

      // if it wasn't picked up, remove it
      if(mo->thinker.function == P_MobjThinker)
         P_RemoveMobj(mo);
   }
}

//
// whistle
//
// This command lets the player "whistle" to teleport a friend
// of the given type to his location.
//
CONSOLE_COMMAND(whistle, cf_notnet|cf_level)
{
   int thingnum;
   player_t *plyr = &players[consoleplayer];

   if(!c_argc)
      return;
   
   thingnum = E_ThingNumForName(c_argv[0]);
   if(thingnum == NUMMOBJTYPES)
   {
      C_Printf("unknown thing type\n");
      return;
   }

   P_Whistle(plyr->mo, thingnum);
}

CONSOLE_COMMAND(mdk, cf_notnet|cf_level)
{
   player_t *plyr = &players[consoleplayer];
   fixed_t slope;
   int damage = 10000;

   slope = P_AimLineAttack(plyr->mo, plyr->mo->angle, MISSILERANGE, 0);

   if(tm->linetarget)
      damage = tm->linetarget->health;

   P_LineAttack(plyr->mo, plyr->mo->angle, MISSILERANGE, slope, damage);
}

CONSOLE_COMMAND(mdkbomb, cf_notnet|cf_level)
{
   player_t *plyr = &players[consoleplayer];
   int i;
   fixed_t slope;
   int damage = 10000;

   for(i = 0; i < 60; ++i)  // offset angles from its attack angle
   {
      angle_t an = (ANG360/60)*i;
      
      slope = P_AimLineAttack(plyr->mo, an, MISSILERANGE,0);

      if(tm->linetarget)
         damage = tm->linetarget->health;

      P_LineAttack(plyr->mo, an, MISSILERANGE, slope, damage);
   }
}

CONSOLE_COMMAND(banish, cf_notnet|cf_level)
{
   player_t *plyr = &players[consoleplayer];

   P_AimLineAttack(plyr->mo, plyr->mo->angle, MISSILERANGE, 0);

   if(tm->linetarget)
      P_RemoveMobj(tm->linetarget);
}

CONSOLE_COMMAND(vilehit, cf_notnet|cf_level)
{
   player_t *plyr = &players[consoleplayer];

   P_BulletSlope(plyr->mo);
   if(!tm->linetarget)
      return;
   
   S_StartSound(plyr->mo, sfx_barexp);
   P_DamageMobj(tm->linetarget, plyr->mo, plyr->mo, 20, MOD_UNKNOWN);
   tm->linetarget->momz = 1000*FRACUNIT/tm->linetarget->info->mass;

   P_RadiusAttack(tm->linetarget, plyr->mo, 70, MOD_UNKNOWN);
}

void P_SpawnPlayer(mapthing_t* mthing);

static void P_ResurrectPlayer(void)
{
   player_t *p = &players[consoleplayer];

   if(p->health <= 0 || p->mo->health <= 0 || p->playerstate == PST_DEAD)
   {
      mapthing_t mthing;
      mobj_t *oldmo = p->mo;

      memset(&mthing, 0, sizeof(mapthing_t));

      mthing.x     = (short)(p->mo->x >> FRACBITS);
      mthing.y     = (short)(p->mo->y >> FRACBITS);
      mthing.angle = (short)(p->mo->angle / ANGLE_1);
      mthing.type  = (p - players) + 1;

      p->health = 100;
      P_SpawnPlayer(&mthing);
      oldmo->player = NULL;
      P_TeleportMove(p->mo, p->mo->x, p->mo->y, true);
   }
}

CONSOLE_COMMAND(resurrect, cf_notnet|cf_level)
{
   P_ResurrectPlayer();
}

void PE_AddCommands(void)
{
   C_AddCommand(summon);
   C_AddCommand(give);
   C_AddCommand(viles);
   C_AddCommand(whistle);
   C_AddCommand(mdk);
   C_AddCommand(mdkbomb);
   C_AddCommand(banish);
   C_AddCommand(vilehit);
   C_AddCommand(resurrect);
}

//----------------------------------------------------------------------------
//
// $Log: p_enemy.c,v $
// Revision 1.22  1998/05/12  12:47:10  phares
// Removed OVER UNDER code
//
// Revision 1.21  1998/05/07  00:50:55  killough
// beautification, remove dependence on evaluation order
//
// Revision 1.20  1998/05/03  22:28:02  killough
// beautification, move declarations and includes around
//
// Revision 1.19  1998/04/01  12:58:44  killough
// Disable boss brain if no targets
//
// Revision 1.18  1998/03/28  17:57:05  killough
// Fix boss spawn savegame bug
//
// Revision 1.17  1998/03/23  15:18:03  phares
// Repaired AV ghosts stuck together bug
//
// Revision 1.16  1998/03/16  12:33:12  killough
// Use new P_TryMove()
//
// Revision 1.15  1998/03/09  07:17:58  killough
// Fix revenant tracer bug
//
// Revision 1.14  1998/03/02  11:40:52  killough
// Use separate monsters_remember flag instead of bitmask
//
// Revision 1.13  1998/02/24  08:46:12  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.12  1998/02/23  04:43:44  killough
// Add revenant p_atracer, optioned monster ai_vengence
//
// Revision 1.11  1998/02/17  06:04:55  killough
// Change RNG calling sequences
// Fix minor icon landing bug
// Use lastenemy to make monsters remember former targets, and fix player look
//
// Revision 1.10  1998/02/09  03:05:22  killough
// Remove icon landing limit
//
// Revision 1.9  1998/02/05  12:15:39  phares
// tighten lost soul wall fix to compatibility
//
// Revision 1.8  1998/02/02  13:42:54  killough
// Relax lost soul wall fix to demo_compatibility
//
// Revision 1.7  1998/01/28  13:21:01  phares
// corrected Option3 in AV bug
//
// Revision 1.6  1998/01/28  12:22:17  phares
// AV bug fix and Lost Soul trajectory bug fix
//
// Revision 1.5  1998/01/26  19:24:00  phares
// First rev with no ^Ms
//
// Revision 1.4  1998/01/23  14:51:51  phares
// No content change. Put ^Ms back.
//
// Revision 1.3  1998/01/23  14:42:14  phares
// No content change. Removed ^Ms for experimental checkin.
//
// Revision 1.2  1998/01/19  14:45:01  rand
// Temporary line for checking checkins
//
// Revision 1.1.1.1  1998/01/19  14:02:59  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------


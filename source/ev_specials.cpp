// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Generalized line action system
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "acs_intr.h"
#include "d_gi.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "e_hash.h"
#include "ev_actions.h"
#include "ev_macros.h"
#include "ev_specials.h"
#include "ev_bindings.h"
#include "g_game.h"
#include "p_info.h"
#include "p_mobj.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_xenemy.h"
#include "polyobj.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_state.h"
#include "s_sound.h"


//=============================================================================
//
// Utilities
//

//
// P_ClearSwitchOnFail
//
// haleyjd 08/29/09: Replaces demo_compatibility checks for clearing 
// W1/S1/G1 line actions on action failure, because it makes some maps
// unplayable if it is disabled unconditionally outside of demos.
//
inline static bool P_ClearSwitchOnFail(void)
{
   return demo_compatibility || (demo_version >= 335 && comp[comp_special]);
}

inline static unsigned int EV_CompositeActionFlags(ev_action_t *action)
{
   return (action->type->flags | action->flags);
}

//
// EV_Check3DMidTexSwitch
//
// haleyjd 12/02/12: split off 3DMidTex switch range testing into its own
// independent routine.
//
static bool EV_Check3DMidTexSwitch(line_t *line, Mobj *thing, int side)
{
   int     sidenum = line->sidenum[side];
   side_t *sidedef = NULL;

   if(demo_version < 331)
      return true; // 3DMidTex don't exist in old demos

   if(sidenum != -1)
      sidedef = &sides[sidenum];

   // SoM: only allow switch specials on 3d sides to be triggered if 
   // the mobj is within range of the side.
   // haleyjd 05/02/06: ONLY on two-sided lines.
   if((line->flags & ML_3DMIDTEX) && line->backsector && 
      sidedef && sidedef->midtexture)
   {
      fixed_t opentop, openbottom, textop, texbot;

      opentop = line->frontsector->ceilingheight < line->backsector->ceilingheight ?
                line->frontsector->ceilingheight : line->backsector->ceilingheight;
      
      openbottom = line->frontsector->floorheight > line->backsector->floorheight ?
                   line->frontsector->floorheight : line->backsector->floorheight;

      if(line->flags & ML_DONTPEGBOTTOM)
      {
         texbot = sidedef->rowoffset + openbottom;
         textop = texbot + textures[sidedef->midtexture]->heightfrac;
      }
      else
      {
         textop = opentop + sidedef->rowoffset;
         texbot = textop - textures[sidedef->midtexture]->heightfrac;
      }

      if(thing->z > textop || thing->z + thing->height < texbot)
         return false;
   }

   return true;
}

//=============================================================================
// 
// Null Activation Helpers - They do nothing.
//

static bool EV_NullPreCrossLine(ev_action_t *action, ev_instance_t *instance)
{
   return false;
}

static bool EV_NullPostCrossLine(ev_action_t *action, bool result, 
                                 ev_instance_t *instance)
{
   return false;
}

//=============================================================================
//
// DOOM Activation Helpers - Preambles and Post-Actions
//

//
// EV_DOOMPreCrossLine
//
// This function is used as the preamble to all DOOM cross-type line specials.
//
static bool EV_DOOMPreCrossLine(ev_action_t *action, ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   // DOOM-style specials require an actor and a line
   REQUIRE_ACTOR(instance->actor);
   REQUIRE_LINE(instance->line);

   // Things that should never trigger lines
   if(!instance->actor->player)
   {
      // haleyjd: changed to check against MF2_NOCROSS flag instead of 
      // switching on type.
      if(instance->actor->flags2 & MF2_NOCROSS)
         return false;
   }

   // Check if action only allows player
   // jff 3/5/98 add ability of monsters etc. to use teleporters
   if(!instance->actor->player && !(flags & EV_PREALLOWMONSTERS))
      return false;

   // jff 2/27/98 disallow zero tag on some types
   // killough 11/98: compatibility option:
   if(!(instance->tag || comp[comp_zerotags] || (flags & EV_PREALLOWZEROTAG)))
      return false;

   // check for first-side-only instance
   if(instance->side && (flags & EV_PREFIRSTSIDEONLY))
      return false;

   // line type is *only* for monsters?
   if(instance->actor->player && (flags & EV_PREMONSTERSONLY))
      return false;

   return true;
}

//
// EV_DOOMPostCrossLine
//
// This function is used as the post-action for all DOOM cross-type line
// specials.
//
static bool EV_DOOMPostCrossLine(ev_action_t *action, bool result,
                                 ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   if(flags & EV_POSTCLEARSPECIAL)
   {
      bool clearSpecial = (result || P_ClearSwitchOnFail());

      if(clearSpecial || (flags & EV_POSTCLEARALWAYS))
         instance->line->special = 0;
   }

   return result;
}

//
// EV_DOOMPreUseLine
//
// Preamble for DOOM-type use (switch or manual) line types.
//
static bool EV_DOOMPreUseLine(ev_action_t *action, ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   Mobj   *thing = instance->actor;
   line_t *line  = instance->line;

   // actor and line are required
   REQUIRE_ACTOR(thing);
   REQUIRE_LINE(line);

   // All DOOM-style use specials only support activation from the first side
   if(instance->side) 
      return false;

   // Check for 3DMidTex range restrictions
   if(!EV_Check3DMidTexSwitch(line, thing, instance->side))
      return false;

   if(!thing->player)
   {
      // Monsters never activate use specials on secret lines
      if(line->flags & ML_SECRET)
         return false;

      // Otherwise, check the special flags
      if(!(flags & EV_PREALLOWMONSTERS))
         return false;
   }

   // check for zero tag
   if(!(instance->tag || comp[comp_zerotags] || (flags & EV_PREALLOWZEROTAG)))
      return false;

   return true;
}

//
// EV_DOOMPostUseLine
//
// Post-activation semantics for DOOM-style use line actions.
//
static bool EV_DOOMPostUseLine(ev_action_t *action, bool result, 
                               ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   // check for switch texture changes
   if(flags & EV_POSTCHANGESWITCH)
   {
      if(result || (flags & EV_POSTCHANGEALWAYS))
      {
         int useAgain   = !(flags & EV_POSTCLEARSPECIAL);
         int changeSide = (flags & EV_POSTCHANGESIDED) ? instance->side : 0;
         P_ChangeSwitchTexture(instance->line, useAgain, changeSide);
      }
   }

   // FIXME/TODO: ideally we return result; the vanilla code always returns true
   // if the line action was *attempted*, not if it succeeded. Fixing this will
   // require a new comp flag.
   
   //return result;
   return true; // temporary
}

//
// EV_DOOMPreShootLine
//
// Pre-activation semantics for DOOM gun-type actions
//
static bool EV_DOOMPreShootLine(ev_action_t *action, ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   Mobj   *thing = instance->actor;
   line_t *line  = instance->line;

   // actor and line are required
   REQUIRE_ACTOR(thing);
   REQUIRE_LINE(line);

   if(!thing->player)
   {
      // Check if special allows monsters
      if(!(flags & EV_PREALLOWMONSTERS))
         return false;
   }

   // check for zero tag
   if(!(instance->tag || comp[comp_zerotags] || (flags & EV_PREALLOWZEROTAG)))
      return false;

   return true;
}

//
// EV_DOOMPostShootLine
//
// Post-activation semantics for DOOM-style use line actions.
//
static bool EV_DOOMPostShootLine(ev_action_t *action, bool result, 
                                 ev_instance_t *instance)
{
   unsigned int flags = EV_CompositeActionFlags(action);

   // check for switch texture changes
   if(flags & EV_POSTCHANGESWITCH)
   {
      // Non-BOOM gun line types may clear their special even if they fail
      if(result || (flags & EV_POSTCHANGEALWAYS) || 
         (action->minversion < 200 && P_ClearSwitchOnFail()))
      {
         int useAgain   = !(flags & EV_POSTCLEARSPECIAL);
         int changeSide = (flags & EV_POSTCHANGESIDED) ? instance->side : 0;
         P_ChangeSwitchTexture(instance->line, useAgain, changeSide);
      }
   }

   return result;
}

//=============================================================================
//
// BOOM Generalized Pre- and Post-Actions
//
// You'd think these could all be very simply combined into each other, but,
// thanks to a reckless implementation by the BOOM team, not quite.
//

//
// EV_BOOMGenPreActivate
//
// Pre-activation logic for BOOM generalized line types
//
static bool EV_BOOMGenPreActivate(ev_action_t *action, ev_instance_t *instance)
{
   Mobj   *thing = instance->actor;
   line_t *line  = instance->line;
   
   REQUIRE_ACTOR(thing);
   REQUIRE_LINE(line);

   // check against zero tags
   if(!line->tag)
   {
      switch(instance->genspac)
      {
      case WalkOnce:
      case WalkMany:
         // jff 2/27/98 all walk generalized types require tag
         // haleyjd 12/01/12: except not, Jim, because you forgot locked doors -_-
         if(instance->gentype != GenTypeLocked)
            return false;
         break;
      case SwitchOnce:
      case SwitchMany:
      case PushOnce:
      case PushMany:
         // jff 3/2/98 all non-manual generalized types require tag
         if((line->special & 6) != 6)
            return false;
         break;
      case GunOnce:
      case GunMany:
         // jff 2/27/98 all gun generalized types require tag
         // haleyjd 12/01/12: except this time you forgot lifts :/
         if(instance->gentype != GenTypeLift)
            return false;
         break;
      default:
         break;
      }
   }

   // check whether monsters are allowed or not
   if(!thing->player)
   {
      switch(instance->gentype)
      {
      case GenTypeFloor:
         // FloorModel is "Allow Monsters" if FloorChange is 0
         if((line->special & FloorChange) || !(line->special & FloorModel))
            return false;
         break;
      case GenTypeCeiling:
         // CeilingModel is "Allow Monsters" if CeilingChange is 0
         if((line->special & CeilingChange) || !(line->special & CeilingModel))
            return false; 
         break;
      case GenTypeDoor:
         if(!(line->special & DoorMonster))
            return false;            // monsters disallowed from this door
         if(line->flags & ML_SECRET) // they can't open secret doors either
            return false;
         break;
      case GenTypeLocked:
         return false; // monsters disallowed from unlocking doors
      case GenTypeLift:
         if(!(line->special & LiftMonster))
            return false; // monsters disallowed
         break;
      case GenTypeStairs:
         if(!(line->special & StairMonster))
            return false; // monsters disallowed
         break;
      case GenTypeCrusher:
         if(!(line->special & CrusherMonster))
            return false; // monsters disallowed
         break;
      default:
         break;
      }
   }

   // check each range of generalized linedefs (special checks)
   switch(instance->gentype)
   {
   case GenTypeLocked:
      if(thing->player && !P_CanUnlockGenDoor(line, thing->player))
         return false;
      break;
   case GenTypeCrusher:
      // haleyjd 06/09/09: This was completely forgotten in BOOM, disabling
      // all generalized walk-over crusher types!
      if((instance->genspac == WalkOnce || instance->genspac == WalkMany)
         && demo_version < 335)
         return false;
      break;
   default:
      break;
   }

   // check for line side?
   // NB: BOOM code checked specially for Push types, but, that check was
   // redundant to one at the top of P_UseSpecialLine that applies to 
   // everything.
   if(instance->side)
   {
      switch(instance->genspac)
      {
      case PushOnce:
      case PushMany:
      case SwitchOnce:
      case SwitchMany:
         return false; // activate from first side only.
      default:
         break;
      }
   }

   return true;
}

//
// EV_BOOMGenPostActivate
//
// Post-activation logic for BOOM generalized line types
//
static bool EV_BOOMGenPostActivate(ev_action_t *action, bool result,
                                   ev_instance_t *instance)
{
   if(result)
   {
      switch(instance->genspac)
      {
      case GunOnce:
      case SwitchOnce:
         P_ChangeSwitchTexture(instance->line, 0, 0);
         break;
      case GunMany:
      case SwitchMany:
         P_ChangeSwitchTexture(instance->line, 1, 0);
         break;
      case WalkOnce:
      case PushOnce:
         instance->line->special = 0;
         break;
      default:
         break;
      }
   }

   // FIXME/TODO: P_Cross and P_Shoot have always been void, and P_UseSpecialLine
   // just returns true if an action is attempted, not if it succeeds. Until a
   // comp var is added, we return true from here instead of forwarding on the
   // result.
   
   //return result;
   return true; // temporary
}

//=============================================================================
//
// Parameterized Pre- and Post-Actions
//

//
// EV_ParamPreActivate
//
// There is nothing to do here. All logic was already taken care of by the 
// EV_checkSpac function.
//
static bool EV_ParamPreActivate(ev_action_t *action, ev_instance_t *instance)
{
   return true;
}

//
// EV_ParamPostActivate
//
// If the action was successful, and has a source linedef, check for
// reusability and for switch texture changes, depending on the spac.
//
static bool EV_ParamPostActivate(ev_action_t *action, bool result, 
                                 ev_instance_t *instance)
{
   // check for switch texture change and special clear
   if(result && instance->line)
   {
      int reuse = (instance->line->extflags & EX_ML_REPEAT);

      if(!reuse)
         instance->line->special = 0;

      if(instance->spac == SPAC_USE || instance->spac == SPAC_IMPACT)
         P_ChangeSwitchTexture(instance->line, reuse, instance->side);
   }

   return result;
}

//=============================================================================
//
// Action Types
//

// The Null action type doesn't do anything.
ev_actiontype_t NullActionType =
{
   -1,
   EV_NullPreCrossLine,
   EV_NullPostCrossLine,
   0
};

// DOOM-Style Action Types

// WR-Type lines may be crossed multiple times
ev_actiontype_t WRActionType =
{
   SPAC_CROSS,           // line must be crossed
   EV_DOOMPreCrossLine,  // pre-activation callback
   EV_DOOMPostCrossLine, // post-activation callback
   0                     // no default flags
};

// W1-Type lines may be activated once. Semantics are implemented in the 
// post-cross callback to implement compatibility behaviors regarding the 
// premature clearing of specials crossed from the wrong side or without
// successful activation having occurred.
ev_actiontype_t W1ActionType =
{
   SPAC_CROSS,           // line must be crossed
   EV_DOOMPreCrossLine,  // pre-activation callback
   EV_DOOMPostCrossLine, // post-activation callback
   EV_POSTCLEARSPECIAL   // special will be cleared after activation
};

// SR-Type lines may be activated multiple times by using them.
ev_actiontype_t SRActionType =
{
   SPAC_USE,             // line must be used
   EV_DOOMPreUseLine,    // pre-activation callback
   EV_DOOMPostUseLine,   // post-activation callback
   EV_POSTCHANGESWITCH   // change switch texture
};

// S1-Type lines may be activated once, by using them.
ev_actiontype_t S1ActionType =
{
   SPAC_USE,             // line must be used
   EV_DOOMPreUseLine,    // pre-activation callback
   EV_DOOMPostUseLine,   // post-activation callback
   EV_POSTCHANGESWITCH | EV_POSTCLEARSPECIAL // change switch and clear special
};

// DR-Type lines may be activated repeatedly by pushing; the main distinctions
// from switch-types are that switch textures are not changed and the sector
// target is always the line's backsector. Therefore the tag can be zero, and
// when not zero, is recycled by BOOM to allow a special lighting effect.
ev_actiontype_t DRActionType =
{
   SPAC_USE,             // line must be used
   EV_DOOMPreUseLine,    // pre-activation callback
   EV_DOOMPostUseLine,   // post-activation callback
   EV_PREALLOWZEROTAG    // tags are never used for activation purposes
};

// GR-Type lines may be activated multiple times by hitscan attacks
// FIXME/TODO: In Raven and Rogue games, projectiles activate them as well.
ev_actiontype_t GRActionType =
{
   SPAC_IMPACT,          // line must be hit
   EV_DOOMPreShootLine,  // pre-activation callback
   EV_DOOMPostShootLine, // post-activation callback (same as use)
   EV_POSTCHANGESWITCH   // change switch texture
};

// G1-Type lines may be activated once, by a hitscan attack.
ev_actiontype_t G1ActionType = 
{
   SPAC_IMPACT,          // line must be hit
   EV_DOOMPreShootLine,  // pre-activation callback
   EV_DOOMPostShootLine, // post-activation callback
   EV_POSTCHANGESWITCH | EV_POSTCLEARSPECIAL // change switch and clear special
};

// BOOM Generalized Action Type (there is but one)

ev_actiontype_t BoomGenActionType =
{
   -1,                     // SPAC is determined by the line special
   EV_BOOMGenPreActivate,  // pre-activation callback
   EV_BOOMGenPostActivate, // post-activation callback
   0                       // flags are not used by this type
};

// Parameterized Action Type (there is but one)

ev_actiontype_t ParamActionType =
{
   -1,                    // SPAC is determined by line flags
   EV_ParamPreActivate,   // pre-activation callback
   EV_ParamPostActivate,  // post-activation callback
   EV_PARAMLINESPEC       // is parameterized
};

//=============================================================================
//
// Special Resolution
//
// Lookup an action by number.
//

// Special hash table type
typedef EHashTable<ev_binding_t, EIntHashKey, &ev_binding_t::actionNumber,
                   &ev_binding_t::links> EV_SpecHash;

// By-name hash table type
typedef EHashTable<ev_binding_t, ENCStringHashKey, &ev_binding_t::name,
                   &ev_binding_t::namelinks> EV_SpecNameHash;

// Hash tables for binding arrays
static EV_SpecHash DOOMSpecHash;
static EV_SpecHash HereticSpecHash;
static EV_SpecHash HexenSpecHash;

static EV_SpecNameHash DOOMSpecNameHash;
static EV_SpecNameHash HexenSpecNameHash;

//
// EV_initSpecHash
//
// Initialize a special hash table.
//
static void EV_initSpecHash(EV_SpecHash &hash, ev_binding_t *bindings, 
                            size_t numBindings)
{
   hash.initialize((unsigned int)numBindings);

   for(size_t i = 0; i < numBindings; i++)
      hash.addObject(bindings[i]);
}

//
// EV_initSpecNameHash
//
// Initialize a by-name lookup hash table.
//
static void EV_initSpecNameHash(EV_SpecNameHash &hash, ev_binding_t *bindings,
                                size_t numBindings)
{
   hash.initialize((unsigned int)numBindings);

   for(size_t i = 0; i < numBindings; i++)
   {
      if(bindings[i].name)
         hash.addObject(bindings[i]);
   }
}

//
// EV_initDoomSpecHash
//
// Initializes the DOOM special bindings hash table the first time it is 
// called.
//
static void EV_initDoomSpecHash()
{
   if(DOOMSpecHash.isInitialized())
      return;

   EV_initSpecHash    (DOOMSpecHash,     DOOMBindings, DOOMBindingsLen);
   EV_initSpecNameHash(DOOMSpecNameHash, DOOMBindings, DOOMBindingsLen);
}

//
// EV_initHereticSpecHash
//
// Initializes the Heretic special bindings hash table the first time it is 
// called. Also inits DOOM bindings.
//
static void EV_initHereticSpecHash()
{
   if(HereticSpecHash.isInitialized())
      return;

   EV_initSpecHash(HereticSpecHash, HereticBindings, HereticBindingsLen);
   EV_initDoomSpecHash(); // initialize DOOM bindings as well
}

//
// EV_initHexenSpecHash
//
// Initializes the Hexen special bindings hash table the first time it is
// called.
//
static void EV_initHexenSpecHash()
{
   if(HexenSpecHash.isInitialized())
      return;

   EV_initSpecHash    (HexenSpecHash,     HexenBindings, HexenBindingsLen);
   EV_initSpecNameHash(HexenSpecNameHash, HexenBindings, HexenBindingsLen);
}

//
// EV_DOOMBindingForSpecial
//
// Returns a special binding from the DOOM gamemode's bindings array, 
// regardless of the current gamemode or map format. Returns NULL if
// the special is not bound to an action.
//
ev_binding_t *EV_DOOMBindingForSpecial(int special)
{
   EV_initDoomSpecHash(); // ensure table is initialized

   return DOOMSpecHash.objectForKey(special);
}

//
// EV_DOOMBindingForName
//
// Returns a special binding from the DOOM gamemode's bindings array
// by name.
//
ev_binding_t *EV_DOOMBindingForName(const char *name)
{
   EV_initDoomSpecHash();

   return DOOMSpecNameHash.objectForKey(name);
}

//
// EV_DOOMActionForSpecial
//
// Returns an action from the DOOM gamemode's bindings array, regardless
// of the current gamemode or map format. Returns NULL if the special is
// not bound to an action.
//
ev_action_t *EV_DOOMActionForSpecial(int special)
{
   ev_binding_t *bind = EV_DOOMBindingForSpecial(special);

   return bind ? bind->action : NULL;
}

//
// EV_HereticBindingForSpecial
//
// Returns a special binding from the Heretic gamemode's bindings array,
// regardless of the current gamemode or map format. Returns NULL if
// the special is not bound to an action.
//
ev_binding_t *EV_HereticBindingForSpecial(int special)
{
   ev_binding_t *bind;

   EV_initHereticSpecHash(); // ensure table is initialized

   // Try Heretic's bindings first. If nothing is found, defer to DOOM's 
   // bindings.
   if(!(bind = HereticSpecHash.objectForKey(special)))
      bind = DOOMSpecHash.objectForKey(special);

   return bind;
}

//
// EV_HereticActionForSpecial
//
// Returns an action from the Heretic gamemode's bindings array,
// regardless of the current gamemode or map format. Returns NULL if
// the special is not bound to an action.
//
ev_action_t *EV_HereticActionForSpecial(int special)
{
   ev_binding_t *bind = EV_HereticBindingForSpecial(special);

   return bind ? bind->action : NULL;
}

//
// EV_HexenBindingForSpecial
//
// Returns a special binding from the Hexen gamemode's bindings array,
// regardless of the current gamemode or map format. Returns NULL if
// the special is not bound to an action.
//
ev_binding_t *EV_HexenBindingForSpecial(int special)
{
   EV_initHexenSpecHash(); // ensure table is initialized

   return HexenSpecHash.objectForKey(special);
}

//
// EV_HexenBindingForName
//
// Returns a special binding from the Hexen gamemode's bindings array
// by name.
//
ev_binding_t *EV_HexenBindingForName(const char *name)
{
   EV_initHexenSpecHash();

   return HexenSpecNameHash.objectForKey(name);
}

//
// EV_HexenActionForSpecial
//
// Returns a special binding from the Hexen gamemode's bindings array,
// regardless of the current gamemode or map format. Returns NULL if
// the special is not bound to an action.
//
ev_action_t *EV_HexenActionForSpecial(int special)
{
   ev_binding_t *bind = EV_HexenBindingForSpecial(special);

   return bind ? bind->action : NULL;
}

//
// EV_StrifeActionForSpecial
//
// TODO
//
ev_action_t *EV_StrifeActionForSpecial(int special)
{
   return NULL;
}

//
// EV_BindingForName
//
// Look up a binding by name depending on the current map format and gamemode.
//
ev_binding_t *EV_BindingForName(const char *name)
{
   if(LevelInfo.mapFormat == LEVEL_FORMAT_HEXEN)
      return EV_HexenBindingForName(name);
   else
      return EV_DOOMBindingForName(name);
}

//
// EV_GenTypeForSpecial
//
// Get a GenType enumeration value given a line special
//
static int EV_GenTypeForSpecial(int special)
{
   // Floors
   if(special >= GenFloorBase)
      return GenTypeFloor;

   // Ceilings
   if(special >= GenCeilingBase)
      return GenTypeCeiling;

   // Doors
   if(special >= GenDoorBase)
      return GenTypeDoor;

   // Locked Doors
   if(special >= GenLockedBase)
      return GenTypeLocked;

   // Lifts
   if(special >= GenLiftBase)
      return GenTypeLift;

   // Stairs
   if(special >= GenStairsBase)
      return GenTypeStairs;

   // Crushers
   if(special >= GenCrusherBase)
      return GenTypeCrusher;

   // not a generalized line.
   return -1;
}

//
// EV_GenActivationType
//
// Extract the activation type from a generalized line special.
//
static int EV_GenActivationType(int special)
{
   return (special & TriggerType) >> TriggerTypeShift;
}

//
// EV_ActionForSpecial
//
ev_action_t *EV_ActionForSpecial(int special)
{
   if(LevelInfo.mapFormat == LEVEL_FORMAT_HEXEN)
      return EV_HexenActionForSpecial(special);
   else
   {
      switch(LevelInfo.levelType)
      {
      case LI_TYPE_DOOM:
      default:
         return EV_DOOMActionForSpecial(special);
      case LI_TYPE_HERETIC:
      case LI_TYPE_HEXEN:
         return EV_HereticActionForSpecial(special);
      case LI_TYPE_STRIFE:
         return EV_StrifeActionForSpecial(special);
      }
   }
}

//
// EV_ActionForInstance
//
// Given an instance, obtain the corresponding ev_action_t structure,
// within the currently defined set of bindings.
//
ev_action_t *EV_ActionForInstance(ev_instance_t &instance)
{
   // check if it is a generalized type 
   instance.gentype = EV_GenTypeForSpecial(instance.special);

   if(instance.gentype >= GenTypeFloor)
   {
      // This is a BOOM generalized special type

      // set trigger type
      instance.genspac = EV_GenActivationType(instance.special);

      return &BoomGenAction;
   }

   return EV_ActionForSpecial(instance.special);
}

//=============================================================================
//
// Activation
//

//
// EV_checkSpac
//
// Checks against the instance characteristics of the action to see if this
// method of activating the line is allowed.
//
static bool EV_checkSpac(ev_action_t *action, ev_instance_t *instance)
{
   if(action->type->activation >= 0) // specific type for this action?
   {
      return action->type->activation == instance->spac;
   }
   else if(instance->gentype >= GenTypeFloor) // generalized line?
   {
      switch(instance->genspac)
      {
      case WalkOnce:
      case WalkMany:
         return instance->spac == SPAC_CROSS;
      case GunOnce:
      case GunMany:
         return instance->spac == SPAC_IMPACT;
      case SwitchOnce:
      case SwitchMany:
      case PushOnce:
      case PushMany:
         return instance->spac == SPAC_USE;
      default:
         return false; // should be unreachable.
      }
   }
   else // activation ability is determined by the linedef's flags
   {
      Mobj   *thing = instance->actor;
      line_t *line  = instance->line;
      int     flags = 0;

      REQUIRE_ACTOR(thing);
      REQUIRE_LINE(line);

      // check player / monster / missile enable flags
      if(thing->player)                   // treat as player?
         flags |= EX_ML_PLAYER;
      if(thing->flags3 & MF3_SPACMISSILE) // treat as missile?
         flags |= EX_ML_MISSILE;
      if(thing->flags3 & MF3_SPACMONSTER) // treat as monster?
         flags |= EX_ML_MONSTER;

      if(!(line->extflags & flags))
         return false;

      // check 1S only flag -- if set, must be activated from first side
      if((line->extflags & EX_ML_1SONLY) && instance->side != 0)
         return false;

      // check activation flags -- can we activate this line this way?
      switch(instance->spac)
      {
      case SPAC_CROSS:
         flags = EX_ML_CROSS;
         break;
      case SPAC_USE:
         flags = EX_ML_USE;
         break;
      case SPAC_IMPACT:
         flags = EX_ML_IMPACT;
         break;
      case SPAC_PUSH:
         flags = EX_ML_PUSH;
         break;
      }

      return (line->extflags & flags) != 0;
   }
}

//
// EV_ActivateSpecial
//
// Shared logic for all types of line activation
//
bool EV_ActivateSpecial(ev_action_t *action, ev_instance_t *instance)
{
   // execute pre-amble routine
   if(!action->type->pre(action, instance))
      return false;

   bool result = action->action(action, instance);

   // execute the post-action routine
   return action->type->post(action, result, instance);
}

//
// EV_ActivateSpecialLineWithSpac
//
// Populates the ev_instance_t from separate arguments and then activates the
// special.
//
bool EV_ActivateSpecialLineWithSpac(line_t *line, int side, Mobj *thing, int spac)
{
   ev_action_t *action;
   INIT_STRUCT(ev_instance_t, instance);

   // setup instance
   instance.actor   = thing;
   instance.args    = line->args;
   instance.line    = line;
   instance.special = line->special;
   instance.side    = side;
   instance.spac    = spac;
   instance.tag     = line->tag;

   // get action
   if(!(action = EV_ActionForInstance(instance)))
      return false;

   // check for parameterized special behavior with tags
   if(EV_CompositeActionFlags(action) & EV_PARAMLINESPEC)
      instance.tag = instance.args[0];

   // check for special instance
   if(!EV_checkSpac(action, &instance))
      return false;

   return EV_ActivateSpecial(action, &instance);
}

//
// EV_ActivateSpecialNum
//
// Activate a special without a source linedef. Only some specials support
// this; ones which don't will return false in their preamble.
//
bool EV_ActivateSpecialNum(int special, int *args, Mobj *thing)
{
   ev_action_t *action;
   INIT_STRUCT(ev_instance_t, instance);

   // setup instance
   instance.actor   = thing;
   instance.args    = args;
   instance.special = special;
   instance.side    = 0;
   instance.spac    = SPAC_CROSS;
   instance.tag     = args[0];

   // get action
   if(!(action = EV_ActionForInstance(instance)))
      return false;

   return EV_ActivateSpecial(action, &instance);
}

//
// EV_ActivateAction
//
// Activate an action that has been looked up elsewhere.
//
bool EV_ActivateAction(ev_action_t *action, int *args, Mobj *thing)
{
   if(!action)
      return false;
   
   INIT_STRUCT(ev_instance_t, instance);

   // setup instance
   instance.actor   = thing;
   instance.args    = args;
   instance.side    = 0;
   instance.spac    = SPAC_CROSS;
   instance.tag     = args[0];

   return EV_ActivateSpecial(action, &instance);
}

//
// EV_IsParamLineSpec
//
// Query the currently active binding set for whether or not a given special
// number is a parameterized line special.
//
bool EV_IsParamLineSpec(int special)
{
   bool result = false;
   ev_action_t *action;

   if((action = EV_ActionForSpecial(special)))
   {
      unsigned int flags = EV_CompositeActionFlags(action);

      result = ((flags & EV_PARAMLINESPEC) == EV_PARAMLINESPEC);
   }

   return result;
}

// EOF


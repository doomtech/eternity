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

#ifndef EV_SPECIALS_H__
#define EV_SPECIALS_H__

#include "m_dllist.h"

struct ev_action_t;
struct line_t;
class  Mobj;

// Action flags
enum EVActionFlags
{
   EV_PREALLOWMONSTERS = 0x00000001, // Preamble should allow non-players
   EV_PREMONSTERSONLY  = 0x00000002, // Preamble should only allow monsters
   EV_PREALLOWZEROTAG  = 0x00000004, // Preamble should allow zero tag
   EV_PREFIRSTSIDEONLY = 0x00000008, // Disallow activation from back side
   
   EV_POSTCLEARSPECIAL = 0x00000010, // Clear special after activation
   EV_POSTCLEARALWAYS  = 0x00000020, // Always clear special
   EV_POSTCHANGESWITCH = 0x00000040, // Changes switch texture if successful
   EV_POSTCHANGEALWAYS = 0x00000080, // Changes switch texture even if fails
   EV_POSTCHANGESIDED  = 0x00000100, // Switch texture changes on proper side of line

   EV_PARAMLINESPEC    = 0x00000200, // Is a parameterized line special
};

// Data related to an instance of a special activation.
struct ev_instance_t
{
   Mobj   *actor;   // actor, if any
   line_t *line;    // line, if any
   int     special; // special to activate (may == line->special)
   int    *args;    // arguments (may point to line->args)
   int     tag;     // tag (may == line->tag or line->args[0])
   int     side;    // side of activation
   int     spac;    // special activation type
   int     gentype; // generalized type, if is generalized (-1 otherwise)
   int     genspac; // generalized activation type, if generalized
};

//
// EVPreFunc
//
// Before a special will be activated, it will be checked for validity of the
// activation. This allows common prologue functionality to occur before the
// special is activated. Most arguments are optional and must be checked for
// validity.
//
typedef bool (*EVPreFunc)(ev_action_t *, ev_instance_t *);

//
// EVPostFunc
//
// This function will be executed after the special and can react to its
// execution. The result code of the EVActionFunc is passed as the first
// parameter. Most other parameters are optional and must be checked for
// validity.
//
typedef bool (*EVPostFunc)(ev_action_t *, bool, ev_instance_t *);

//
// EVActionFunc
//
// All actions must adhere to this call signature. Most members of the 
// specialactivation structure are optional and must be checked for validity.
//
typedef bool (*EVActionFunc)(ev_action_t *, ev_instance_t *);

//
// ev_actiontype_t
//
// This structure represents a distinct type of action, for example a DOOM
// cross-type action.
//
struct ev_actiontype_t
{
   int           activation; // activation type of this special, if restricted
   EVPreFunc     pre;        // pre-action callback
   EVPostFunc    post;       // post-action callback
   unsigned int  flags;      // flags; the action may add additional flags.
   const char   *name;       // name for display purposes
};

struct ev_action_t
{
   ev_actiontype_t *type;       // actiontype structure
   EVActionFunc     action;     // action function
   unsigned int     flags;      // action flags
   int              minversion; // minimum demo version
   const char      *name;       // name for display purposes
};

// Binds a line special action to a specific action number.
struct ev_binding_t
{
   int actionNumber;    // line action number
   ev_action_t *action; // the actual action to execute
   const char *name;    // name, if this binding has one

   DLListItem<ev_binding_t> links;     // hash links by number
   DLListItem<ev_binding_t> namelinks; // hash links by name
};

//
// EV_CompositeActionFlags
//
// Returns the composition of the action's flags with the flags imposed by the
// action's type. Always call this rather than directly accessing action->flags,
// because all flags that are implied by an action's type will only be set by
// the type and never by the action. For example, W1ActionType sets 
// EV_POSTCLEARSPECIAL for all DOOM-style W1 actions.
//
inline static unsigned int EV_CompositeActionFlags(ev_action_t *action)
{
   return (action->type->flags | action->flags);
}

// Action Types
extern ev_actiontype_t NullActionType;
extern ev_actiontype_t WRActionType;
extern ev_actiontype_t W1ActionType;
extern ev_actiontype_t SRActionType;
extern ev_actiontype_t S1ActionType;
extern ev_actiontype_t DRActionType;
extern ev_actiontype_t GRActionType;
extern ev_actiontype_t G1ActionType;
extern ev_actiontype_t BoomGenActionType;
extern ev_actiontype_t ParamActionType;

// Interface

// Binding for Special
ev_binding_t *EV_DOOMBindingForSpecial(int special);
ev_binding_t *EV_HereticBindingForSpecial(int special);
ev_binding_t *EV_HexenBindingForSpecial(int special);

// Binding for Name
ev_binding_t *EV_DOOMBindingForName(const char *name);
ev_binding_t *EV_HexenBindingForName(const char *name);
ev_binding_t *EV_BindingForName(const char *name);

// Action for Special 
ev_action_t  *EV_DOOMActionForSpecial(int special);
ev_action_t  *EV_HereticActionForSpecial(int special);
ev_action_t  *EV_HexenActionForSpecial(int special);
ev_action_t  *EV_ActionForSpecial(int special);

// Lockdef ID for Special
int EV_LockDefIDForSpecial(int special);

// Testing
bool EV_IsParamLineSpec(int special);

// Activation
bool EV_ActivateSpecialLineWithSpac(line_t *line, int side, Mobj *thing, int spac);
bool EV_ActivateSpecialNum(int special, int *args, Mobj *thing);
bool EV_ActivateACSSpecial(line_t *line, int special, int *args, int side, Mobj *thing);
bool EV_ActivateAction(ev_action_t *action, int *args, Mobj *thing);

//
// Static Init Line Types
//

enum
{
   // Enumerator                               DOOM/BOOM/MBF/EE Line Special
   EV_STATIC_NULL,                          // No-op placeholder
   EV_STATIC_SCROLL_LINE_LEFT,              // 48
   EV_STATIC_SCROLL_LINE_RIGHT,             // 85
   EV_STATIC_LIGHT_TRANSFER_FLOOR,          // 213
   EV_STATIC_SCROLL_ACCEL_CEILING,          // 214
   EV_STATIC_SCROLL_ACCEL_FLOOR,            // 215
   EV_STATIC_CARRY_ACCEL_FLOOR,             // 216
   EV_STATIC_SCROLL_CARRY_ACCEL_FLOOR,      // 217
   EV_STATIC_SCROLL_ACCEL_WALL,             // 218
   EV_STATIC_FRICTION_TRANSFER,             // 223
   EV_STATIC_WIND_CONTROL,                  // 224
   EV_STATIC_CURRENT_CONTROL,               // 225
   EV_STATIC_PUSHPULL_CONTROL,              // 226
   EV_STATIC_TRANSFER_HEIGHTS,              // 242
   EV_STATIC_SCROLL_DISPLACE_CEILING,       // 245
   EV_STATIC_SCROLL_DISPLACE_FLOOR,         // 246
   EV_STATIC_CARRY_DISPLACE_FLOOR,          // 247
   EV_STATIC_SCROLL_CARRY_DISPLACE_FLOOR,   // 248
   EV_STATIC_SCROLL_DISPLACE_WALL,          // 249
   EV_STATIC_SCROLL_CEILING,                // 250
   EV_STATIC_SCROLL_FLOOR,                  // 251
   EV_STATIC_CARRY_FLOOR,                   // 252
   EV_STATIC_SCROLL_CARRY_FLOOR,            // 253
   EV_STATIC_SCROLL_WALL_WITH,              // 254
   EV_STATIC_SCROLL_BY_OFFSETS,             // 255
   EV_STATIC_TRANSLUCENT,                   // 260
   EV_STATIC_LIGHT_TRANSFER_CEILING,        // 261
   EV_STATIC_EXTRADATA_LINEDEF,             // 270
   EV_STATIC_SKY_TRANSFER,                  // 271
   EV_STATIC_SKY_TRANSFER_FLIPPED,          // 272
   EV_STATIC_3DMIDTEX_ATTACH_FLOOR,         // 281
   EV_STATIC_3DMIDTEX_ATTACH_CEILING,       // 282
   EV_STATIC_PORTAL_PLANE_CEILING,          // 283
   EV_STATIC_PORTAL_PLANE_FLOOR,            // 284
   EV_STATIC_PORTAL_PLANE_CEILING_FLOOR,    // 285
   EV_STATIC_PORTAL_HORIZON_CEILING,        // 286
   EV_STATIC_PORTAL_HORIZON_FLOOR,          // 287
   EV_STATIC_PORTAL_HORIZON_CEILING_FLOOR,  // 288
   EV_STATIC_PORTAL_LINE,                   // 289
   EV_STATIC_PORTAL_SKYBOX_CEILING,         // 290
   EV_STATIC_PORTAL_SKYBOX_FLOOR,           // 291
   EV_STATIC_PORTAL_SKYBOX_CEILING_FLOOR,   // 292
   EV_STATIC_HERETIC_WIND,                  // 293
   EV_STATIC_HERETIC_CURRENT,               // 294
   EV_STATIC_PORTAL_ANCHORED_CEILING,       // 295
   EV_STATIC_PORTAL_ANCHORED_FLOOR,         // 296
   EV_STATIC_PORTAL_ANCHORED_CEILING_FLOOR, // 297
   EV_STATIC_PORTAL_ANCHOR,                 // 298
   EV_STATIC_PORTAL_ANCHOR_FLOOR,           // 299
   EV_STATIC_PORTAL_TWOWAY_CEILING,         // 344
   EV_STATIC_PORTAL_TWOWAY_FLOOR,           // 345
   EV_STATIC_PORTAL_TWOWAY_ANCHOR,          // 346
   EV_STATIC_PORTAL_TWOWAY_ANCHOR_FLOOR,    // 347
   EV_STATIC_POLYOBJ_START_LINE,            // 348
   EV_STATIC_POLYOBJ_EXPLICIT_LINE,         // 349
   EV_STATIC_PORTAL_LINKED_CEILING,         // 358
   EV_STATIC_PORTAL_LINKED_FLOOR,           // 359
   EV_STATIC_PORTAL_LINKED_ANCHOR,          // 360
   EV_STATIC_PORTAL_LINKED_ANCHOR_FLOOR,    // 361
   EV_STATIC_PORTAL_LINKED_LINE2LINE,       // 376
   EV_STATIC_PORTAL_LINKED_L2L_ANCHOR,      // 377
   EV_STATIC_LINE_SET_IDENTIFICATION,       // 378
   EV_STATIC_ATTACH_SET_CEILING_CONTROL,    // 379
   EV_STATIC_ATTACH_SET_FLOOR_CONTROL,      // 380
   EV_STATIC_ATTACH_FLOOR_TO_CONTROL,       // 381
   EV_STATIC_ATTACH_CEILING_TO_CONTROL,     // 382
   EV_STATIC_ATTACH_MIRROR_FLOOR,           // 383
   EV_STATIC_ATTACH_MIRROR_CEILING,         // 384
   EV_STATIC_PORTAL_APPLY_FRONTSECTOR,      // 385
   EV_STATIC_SLOPE_FSEC_FLOOR,              // 386
   EV_STATIC_SLOPE_FSEC_CEILING,            // 387
   EV_STATIC_SLOPE_FSEC_FLOOR_CEILING,      // 388
   EV_STATIC_SLOPE_BSEC_FLOOR,              // 389
   EV_STATIC_SLOPE_BSEC_CEILING,            // 390
   EV_STATIC_SLOPE_BSEC_FLOOR_CEILING,      // 391
   EV_STATIC_SLOPE_BACKFLOOR_FRONTCEILING,  // 392
   EV_STATIC_SLOPE_FRONTFLOOR_BACKCEILING,  // 393
   EV_STATIC_SLOPE_FRONTFLOOR_TAG,          // 394
   EV_STATIC_SLOPE_FRONTCEILING_TAG,        // 395
   EV_STATIC_SLOPE_FRONTFLOORCEILING_TAG,   // 396
   EV_STATIC_EXTRADATA_SECTOR,              // 401
   EV_STATIC_SCROLL_LEFT_PARAM,             // 406
   EV_STATIC_SCROLL_RIGHT_PARAM,            // 407
   EV_STATIC_SCROLL_UP_PARAM,               // 408
   EV_STATIC_SCROLL_DOWN_PARAM,             // 409

   EV_STATIC_MAX
};

// Binds a line special number to a static init function
struct ev_static_t
{
   int actionNumber; // numeric line special
   int staticFn;     // static init function enumeration value

   DLListItem<ev_static_t> staticLinks; // hash links for static->special
   DLListItem<ev_static_t> actionLinks; // hash links for special->static
};

// Interface

int  EV_SpecialForStaticInit(int staticFn);
int  EV_StaticInitForSpecial(int special);
int  EV_SpecialForStaticInitName(const char *name);
bool EV_IsParamStaticInit(int special);

#endif

// EOF


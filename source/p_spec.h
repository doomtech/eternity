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
// DESCRIPTION:  definitions, declarations and prototypes for specials
//
//-----------------------------------------------------------------------------

#ifndef P_SPEC_H__
#define P_SPEC_H__

// HEADER_FIXME: Needs to be broken up, too much intermixed functionality.

#include <map>

#include "m_fixed.h" // [CG] 09/23/11 Requred for fixed_t.

// Required for: Thinker
#include "p_tick.h"

struct line_t;
class  Mobj;
struct player_t;
class  SaveArchive;
struct sector_t;
struct side_t;

//      Define values for map objects
#define MO_TELEPORTMAN  14

// p_floor

#define ELEVATORSPEED (FRACUNIT*4)
#define FLOORSPEED     FRACUNIT

// p_ceilng

#define CEILSPEED   FRACUNIT
#define CEILWAIT    150

// p_doors

#define VDOORSPEED  (FRACUNIT*2)
#define VDOORWAIT   150

// p_plats

#define PLATWAIT    3

#define PLATSPEED   FRACUNIT

// p_switch

// 4 players, 4 buttons each at once, max.
// killough 2/14/98: redefine in terms of MAXPLAYERS
// #define MAXBUTTONS    (MAXPLAYERS*4)

// 1 second, in ticks. 
#define BUTTONTIME  TICRATE

// p_lights

#define GLOWSPEED       8
#define STROBEBRIGHT    5
#define FASTDARK        15
#define SLOWDARK        35

//jff 3/14/98 add bits and shifts for generalized sector types

#define DAMAGE_MASK     0x60
#define DAMAGE_SHIFT    5
#define SECRET_MASK     0x80
#define SECRET_SHIFT    7
#define FRICTION_MASK   0x100
#define FRICTION_SHIFT  8
#define PUSH_MASK       0x200
#define PUSH_SHIFT      9
#define KILLSOUND_MASK  0x400
#define KILLSOUND_SHIFT 10
#define MOVESOUND_MASK  0x800
#define MOVESOUND_SHIFT 11

// haleyjd 12/28/08: mask used to get generalized special bits that are now
// part of the sector flags
#define GENSECTOFLAGSMASK \
   (SECRET_MASK|FRICTION_MASK|PUSH_MASK|KILLSOUND_MASK|MOVESOUND_MASK)

//jff 02/04/98 Define masks, shifts, for fields in 
// generalized linedef types

#define GenFloorBase          0x6000
#define GenCeilingBase        0x4000
#define GenDoorBase           0x3c00
#define GenLockedBase         0x3800
#define GenLiftBase           0x3400
#define GenStairsBase         0x3000
#define GenCrusherBase        0x2F80

#define TriggerType           0x0007
#define TriggerTypeShift      0

// define masks and shifts for the floor type fields

#define FloorCrush            0x1000
#define FloorChange           0x0c00
#define FloorTarget           0x0380
#define FloorDirection        0x0040
#define FloorModel            0x0020
#define FloorSpeed            0x0018

#define FloorCrushShift           12
#define FloorChangeShift          10
#define FloorTargetShift           7
#define FloorDirectionShift        6
#define FloorModelShift            5
#define FloorSpeedShift            3
                               
// define masks and shifts for the ceiling type fields

#define CeilingCrush          0x1000
#define CeilingChange         0x0c00
#define CeilingTarget         0x0380
#define CeilingDirection      0x0040
#define CeilingModel          0x0020
#define CeilingSpeed          0x0018

#define CeilingCrushShift         12
#define CeilingChangeShift        10
#define CeilingTargetShift         7
#define CeilingDirectionShift      6
#define CeilingModelShift          5
#define CeilingSpeedShift          3

// define masks and shifts for the lift type fields

#define LiftTarget            0x0300
#define LiftDelay             0x00c0
#define LiftMonster           0x0020
#define LiftSpeed             0x0018

#define LiftTargetShift            8
#define LiftDelayShift             6
#define LiftMonsterShift           5
#define LiftSpeedShift             3

// define masks and shifts for the stairs type fields

#define StairIgnore           0x0200
#define StairDirection        0x0100
#define StairStep             0x00c0
#define StairMonster          0x0020
#define StairSpeed            0x0018

#define StairIgnoreShift           9
#define StairDirectionShift        8
#define StairStepShift             6
#define StairMonsterShift          5
#define StairSpeedShift            3

// define masks and shifts for the crusher type fields

#define CrusherSilent         0x0040
#define CrusherMonster        0x0020
#define CrusherSpeed          0x0018

#define CrusherSilentShift         6
#define CrusherMonsterShift        5
#define CrusherSpeedShift          3

// define masks and shifts for the door type fields

#define DoorDelay             0x0300
#define DoorMonster           0x0080
#define DoorKind              0x0060
#define DoorSpeed             0x0018

#define DoorDelayShift             8
#define DoorMonsterShift           7
#define DoorKindShift              5
#define DoorSpeedShift             3

// define masks and shifts for the locked door type fields

#define LockedNKeys           0x0200
#define LockedKey             0x01c0
#define LockedKind            0x0020
#define LockedSpeed           0x0018

#define LockedNKeysShift           9
#define LockedKeyShift             6
#define LockedKindShift            5
#define LockedSpeedShift           3

// define names for the TriggerType field of the general linedefs

typedef enum
{
   WalkOnce,
   WalkMany,
   SwitchOnce,
   SwitchMany,
   GunOnce,
   GunMany,
   PushOnce,
   PushMany,
} triggertype_e;

// define names for the Speed field of the general linedefs

typedef enum
{
   SpeedSlow,
   SpeedNormal,
   SpeedFast,
   SpeedTurbo,  
   SpeedParam, // haleyjd 05/04/04: parameterized extension 
} motionspeed_e;

// define names for the Target field of the general floor

typedef enum
{
   FtoHnF,
   FtoLnF,
   FtoNnF,
   FtoLnC,
   FtoC,
   FbyST,
   Fby24,
   Fby32,
  
   FbyParam, // haleyjd 05/07/04: parameterized extensions
   FtoAbs,
   FInst,
} floortarget_e;

// define names for the Changer Type field of the general floor

typedef enum
{
   FNoChg,
   FChgZero,
   FChgTxt,
   FChgTyp,
} floorchange_e;

// define names for the Change Model field of the general floor

typedef enum
{
   FTriggerModel,
   FNumericModel,
} floormodel_t;

// define names for the Target field of the general ceiling

typedef enum
{
   CtoHnC,
   CtoLnC,
   CtoNnC,
   CtoHnF,
   CtoF,
   CbyST,
   Cby24,
   Cby32,

   CbyParam, // haleyjd 05/07/04: parameterized extensions
   CtoAbs,
   CInst,
} ceilingtarget_e;

// define names for the Changer Type field of the general ceiling

typedef enum
{
   CNoChg,
   CChgZero,
   CChgTxt,
   CChgTyp,
} ceilingchange_e;

// define names for the Change Model field of the general ceiling

typedef enum
{
   CTriggerModel,
   CNumericModel,
} ceilingmodel_t;

// define names for the Target field of the general lift

typedef enum
{
   F2LnF,
   F2NnF,
   F2LnC,
   LnF2HnF,
} lifttarget_e;

// haleyjd 10/06/05: defines for generalized stair step sizes
 
typedef enum
{
   StepSize4,
   StepSize8,
   StepSize16,
   StepSize24,

   StepSizeParam, // haleyjd 10/06/05: parameterized extension
} genstairsize_e;

// define names for the door Kind field of the general ceiling

typedef enum
{
   OdCDoor,
   ODoor,
   CdODoor,
   CDoor,
  
   // haleyjd 03/01/05: new param types with initial delays
   pDOdCDoor,
   pDCDoor,
} doorkind_e;

// define names for the locked door Kind field of the general ceiling

typedef enum
{
   AnyKey,
   RCard,
   BCard,
   YCard,
   RSkull,
   BSkull,
   YSkull,
   AllKeys,
} keykind_e;

//////////////////////////////////////////////////////////////////
//
// enums for classes of linedef triggers
//
//////////////////////////////////////////////////////////////////

//jff 2/23/98 identify the special classes that can share sectors

typedef enum
{
   floor_special,
   ceiling_special,
   lighting_special,
} special_e;

//jff 3/15/98 pure texture/type change for better generalized support
typedef enum
{
   trigChangeOnly,
   numChangeOnly,
} change_e;

// p_plats

typedef enum
{
   up,
   down,
   waiting,
   in_stasis
} plat_e;

typedef enum
{
   perpetualRaise,
   downWaitUpStay,
   raiseAndChange,
   raiseToNearestAndChange,
   blazeDWUS,
   genLift,      //jff added to support generalized Plat types
   genPerpetual, 
   toggleUpDn,   //jff 3/14/98 added to support instant toggle type

} plattype_e;

// p_doors

// haleyjd 03/17/02: the names normal, close, and open are too
// common to be used unprefixed in a global enumeration, so they
// have been changed

typedef enum
{
   doorNormal,
   close30ThenOpen,
   doorClose,
   doorOpen,
   raiseIn5Mins,
   blazeRaise,
   blazeOpen,
   blazeClose,

   //jff 02/05/98 add generalize door types
   genRaise,
   genBlazeRaise,
   genOpen,
   genBlazeOpen,
   genClose,
   genBlazeClose,
   genCdO,
   genBlazeCdO,

   // haleyjd 03/01/05: exclusively param door types
   paramRaiseIn,
   paramBlazeRaiseIn,
   paramCloseIn,
   paramBlazeCloseIn,
} vldoor_e;

// haleyjd 05/04/04: door wait types
typedef enum
{
   doorWaitOneSec,
   doorWaitStd,
   doorWaitStd2x,
   doorWaitStd7x,
   doorWaitParam,
} doorwait_e;

// p_ceilng

typedef enum
{
   lowerToFloor,
   raiseToHighest,
   lowerToLowest,
   lowerToMaxFloor,
   lowerAndCrush,
   crushAndRaise,
   fastCrushAndRaise,
   silentCrushAndRaise,

   //jff 02/04/98 add types for generalized ceiling mover
   genCeiling,
   genCeilingChg,
   genCeilingChg0,
   genCeilingChgT,

   //jff 02/05/98 add types for generalized ceiling mover
   genCrusher,
   genSilentCrusher,
} ceiling_e;

// p_floor

typedef enum
{
   // lower floor to highest surrounding floor
   lowerFloor,
  
   // lower floor to lowest surrounding floor
   lowerFloorToLowest,
  
   // lower floor to highest surrounding floor VERY FAST
   turboLower,
  
   // raise floor to lowest surrounding CEILING
   raiseFloor,
  
   // raise floor to next highest surrounding floor
   raiseFloorToNearest,

   //jff 02/03/98 lower floor to next lowest neighbor
   lowerFloorToNearest,

   //jff 02/03/98 lower floor 24 absolute
   lowerFloor24,

   //jff 02/03/98 lower floor 32 absolute
   lowerFloor32Turbo,

   // raise floor to shortest height texture around it
   raiseToTexture,
  
   // lower floor to lowest surrounding floor
   //  and change floorpic
   lowerAndChange,

   raiseFloor24,

   //jff 02/03/98 raise floor 32 absolute
   raiseFloor32Turbo,

   raiseFloor24AndChange,
   raiseFloorCrush,

   // raise to next highest floor, turbo-speed
   raiseFloorTurbo,       
   donutRaise,
   raiseFloor512,

   //jff 02/04/98  add types for generalized floor mover
   genFloor,
   genFloorChg,
   genFloorChg0,
   genFloorChgT,

   //new types for stair builders
   buildStair,
   genBuildStair,
   genWaitStair,  // haleyjd 10/10/05: stair resetting
   genDelayStair, // haleyjd 10/13/05: delayed stair
   genResetStair, 
} floor_e;

typedef enum
{
   build8, // slowly build by 8
   turbo16 // quickly build by 16    
} stair_e;

typedef enum
{
   elevateUp,
   elevateDown,
   elevateCurrent,
} elevator_e;

// haleyjd 01/09/07: p_lights

typedef enum
{
   setlight_set, // set light to given level
   setlight_add, // add to light level
   setlight_sub, // subtract from light level
} setlight_e;

typedef enum
{
   fade_once, // just a normal fade effect
   fade_glow, // glow effect
} lightfade_e;

//////////////////////////////////////////////////////////////////
//
// general enums
//
//////////////////////////////////////////////////////////////////

// texture type enum
typedef enum
{
   top,
   middle,
   bottom      
} bwhere_e;

// crush check returns
typedef enum
{
   ok,
   crushed,
   pastdest
} result_e;

//////////////////////////////////////////////////////////////////
//
// linedef and sector special data types
//
//////////////////////////////////////////////////////////////////

// p_switch

// switch animation structure type

//jff 3/23/98 pack to read from memory
#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(push, 1)
#endif

struct switchlist_s
{
  char    name1[9];
  char    name2[9];
  int16_t episode;
}; 

typedef struct switchlist_s switchlist_t;

#if defined(_MSC_VER) || defined(__GNUC__)
#pragma pack(pop)
#endif

typedef struct button_s
{
   int      line;
   int      side;
   int      where;
   int      btexture;
   int      btimer;
   bool     dopopout;
} button_t;

// haleyjd 04/17/08: made buttonlist/numbuttonsalloc external for savegames
extern button_t *buttonlist;
extern int numbuttonsalloc;

// p_lights

class FireFlickerThinker : public Thinker
{
protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "FireFlickerThinker"; }

   // Data Members
   sector_t *sector;
   int count;
   int maxlight;
   int minlight;
};

class LightFlashThinker : public Thinker
{
protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "LightFlashThinker"; }
   
   // Data Members
   sector_t *sector;
   int count;
   int maxlight;
   int minlight;
   int maxtime;
   int mintime;
};

class StrobeThinker : public Thinker
{
protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "StrobeThinker"; }

   // Data Members
   sector_t *sector;
   int count;
   int minlight;
   int maxlight;
   int darktime;
   int brighttime;
};

class GlowThinker : public Thinker
{
protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "GlowThinker"; }

   // Data Members
   sector_t *sector;
   int minlight;
   int maxlight;
   int direction;
};

// sf 13/10/99
// haleyjd 01/10/06: revised for parameterized line specs

class LightFadeThinker : public Thinker
{
protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "LightFadeThinker"; }

   // Data Members
   sector_t *sector;
   fixed_t lightlevel;
   fixed_t destlevel;
   fixed_t step;
   fixed_t glowmin;
   fixed_t glowmax;
   int     glowspeed;
   int     type;
};

// p_plats

class PlatThinker : public Thinker
{
protected:
   void Think();

public:
   // [CG] 09/22/11: Status for sending over the network.
#pragma pack(push, 1)
   typedef struct
   {
      uint32_t net_id;
      int32_t type;
      fixed_t speed;
      fixed_t low;
      fixed_t high;
      int32_t wait;
      int32_t count;
      int32_t status;
      int32_t old_status;
      int32_t crush;
      int32_t tag;
   } status_t;
#pragma pack(pop)

   std::map<uint32_t, status_t*> statuses;

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "PlatThinker"; }

   status_t* getStatus(void)
   {
      status_t *status = (status_t *)(malloc(sizeof(status_t)));

      status->net_id     = this->net_id;
      status->type       = this->type;
      status->speed      = this->speed;
      status->low        = this->low;
      status->high       = this->high;
      status->wait       = this->wait;
      status->count      = this->count;
      status->status     = this->status;
      status->old_status = this->oldstatus;
      status->crush      = this->crush;
      status->tag        = this->tag;

      return status;
   }

   void getStatus(status_t *status)
   {
      status->net_id     = this->net_id;
      status->type       = this->type;
      status->speed      = this->speed;
      status->low        = this->low;
      status->high       = this->high;
      status->wait       = this->wait;
      status->count      = this->count;
      status->status     = this->status;
      status->old_status = this->oldstatus;
      status->crush      = this->crush;
      status->tag        = this->tag;
   }

   void setStatus(status_t *status)
   {
      this->net_id     = status->net_id;
      this->type       = status->type;
      this->speed      = status->speed;
      this->low        = status->low;
      this->high       = status->high;
      this->wait       = status->wait;
      this->count      = status->count;
      this->status     = status->status;
      this->oldstatus  = status->old_status;
      this->crush      = status->crush;
      this->tag        = status->tag;
   }

   // Data Members
   sector_t *sector;
   fixed_t speed;
   fixed_t low;
   fixed_t high;
   int wait;
   int count;
   int status;
   int oldstatus;
   int crush;
   int tag;
   int type;
   struct platlist *list;   // killough
   uint32_t net_id;   // [CG] 09/18/11
   uint32_t inactive; // [CG] 09/18/11
};

// New limit-free plat structure -- killough

typedef struct platlist {
  PlatThinker *plat; 
  struct platlist *next,**prev;
} platlist_t;

// p_ceilng

// haleyjd: ceiling noise levels
enum
{
   CNOISE_NORMAL,     // plays plat move sound
   CNOISE_SEMISILENT, // plays plat stop at end of strokes
   CNOISE_SILENT,     // plays silence sequence (not same as silent flag!)
};

class VerticalDoorThinker : public Thinker
{
protected:
   void Think();

public:
   // [CG] 09/22/11: Status for sending over the network.
#pragma pack(push, 1)
   typedef struct
   {
      uint32_t net_id;
      int32_t type;
      fixed_t top_height;
      fixed_t speed;
      int32_t direction;
      int32_t top_wait;
      int32_t top_countdown;
   } status_t;
#pragma pack(pop)

   std::map<uint32_t, status_t*> statuses;

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "VerticalDoorThinker"; }

   status_t* getStatus(void)
   {
      status_t *status = (status_t *)(malloc(sizeof(status_t)));

      status->net_id        = this->net_id;
      status->type          = this->type;
      status->top_height    = this->topheight;
      status->speed         = this->speed;
      status->direction     = this->direction;
      status->top_wait      = this->topwait;
      status->top_countdown = this->topcountdown;

      return status;
   }

   void getStatus(status_t *status)
   {
      status->net_id        = this->net_id;
      status->type          = this->type;
      status->top_height    = this->topheight;
      status->speed         = this->speed;
      status->direction     = this->direction;
      status->top_wait      = this->topwait;
      status->top_countdown = this->topcountdown;
   }

   void setStatus(status_t *status)
   {
      this->net_id       = status->net_id;
      this->type         = status->type;
      this->topheight    = status->top_height;
      this->speed        = status->speed;
      this->direction    = status->direction;
      this->topwait      = status->top_wait;
      this->topcountdown = status->top_countdown;
   }

   // Data Members
   int type;
   sector_t *sector;
   fixed_t topheight;
   fixed_t speed;

   // 1 = up, 0 = waiting at top, -1 = down
   int direction;

   // tics to wait at the top
   int topwait;
   // (keep in case a door going down is reset)
   // when it reaches 0, start going down
   int topcountdown;

   //jff 1/31/98 keep track of line door is triggered by
   line_t *line;

   int lighttag; //killough 10/98: sector tag for gradual lighting effects

   uint32_t net_id;   // [CG] 09/18/11
   uint32_t inactive; // [CG] 09/18/11
};

// haleyjd 05/04/04: extended data struct for gen/param doors
typedef struct doordata_s
{
   // generalized values
   int delay_type;
   int kind;
   int speed_type;
   int trigger_type;

   // parameterized values
   fixed_t speed_value;
   int     delay_value;
   int     altlighttag;
   bool    usealtlighttag;
   int     topcountdown;
} doordata_t;

// haleyjd 09/06/07: sector special transfer structure

struct spectransfer_t
{
   int newspecial;
   unsigned int flags;
   int damage;
   int damagemask;
   int damagemod;
   int damageflags;
};

// p_doors

class CeilingThinker : public Thinker
{
protected:
   void Think();

public:
   // [CG] 09/22/11: Status for sending over the network.
#pragma pack(push, 1)
   typedef struct
   {
      uint32_t net_id;
      int32_t type;
      fixed_t bottom_height;
      fixed_t top_height;
      fixed_t speed;
      fixed_t old_speed;
      int32_t crush;
      int32_t texture;
      int8_t direction;
      int32_t tag;
      int32_t old_direction;
   } status_t;
#pragma pack(pop)

   std::map<uint32_t, status_t*> statuses;

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "CeilingThinker"; }

   status_t* getStatus(void)
   {
      status_t *status = (status_t *)(malloc(sizeof(status_t)));

      status->net_id        = this->net_id;
      status->type          = this->type;
      status->bottom_height = this->bottomheight;
      status->top_height    = this->topheight;
      status->speed         = this->speed;
      status->old_speed     = this->oldspeed;
      status->crush         = this->crush;
      status->texture       = this->texture;
      status->direction     = this->direction;
      status->tag           = this->tag;
      status->old_direction = this->olddirection;

      return status;
   }
   
   void getStatus(status_t *status)
   {
      status->net_id        = this->net_id;
      status->type          = this->type;
      status->bottom_height = this->bottomheight;
      status->top_height    = this->topheight;
      status->speed         = this->speed;
      status->old_speed     = this->oldspeed;
      status->crush         = this->crush;
      status->texture       = this->texture;
      status->direction     = this->direction;
      status->tag           = this->tag;
      status->old_direction = this->olddirection;
   }

   void setStatus(status_t *status)
   {
      this->net_id       = status->net_id;
      this->type         = status->type;
      this->bottomheight = status->bottom_height;
      this->topheight    = status->top_height;
      this->speed        = status->speed;
      this->oldspeed     = status->old_speed;
      this->crush        = status->crush;
      this->texture      = status->texture;
      this->direction    = status->direction;
      this->tag          = status->tag;
      this->olddirection = status->old_direction;
   }

   // Data Members
   int type;
   sector_t *sector;
   fixed_t bottomheight;
   fixed_t topheight;
   fixed_t speed;
   fixed_t oldspeed;
   int crush;

   //jff 02/04/98 add these to support ceiling changers
   //jff 3/14/98 add to fix bug in change transfers
   spectransfer_t special; // haleyjd 09/06/07: spectransfer
   int texture;

   // 1 = up, 0 = waiting, -1 = down
   int direction;

   // haleyjd: stasis
   bool inStasis;

   // ID
   int tag;                   
   int olddirection;
   struct ceilinglist *list;   // jff 2/22/98 copied from killough's plats
   uint32_t net_id;   // [CG] 09/18/11
   uint32_t inactive; // [CG] 09/18/11
};

typedef struct ceilinglist 
{
  CeilingThinker *ceiling; 
  struct ceilinglist *next,**prev;
} ceilinglist_t;

// haleyjd 10/05/05: extended data struct for parameterized ceilings
typedef struct ceilingdata_s
{   
   // generalized values
   int trigger_type;
   int crush;
   int direction;
   int speed_type;
   int change_type;
   int change_model;
   int target_type;

   // parameterized values
   fixed_t height_value;
   fixed_t speed_value;
} ceilingdata_t;


// p_floor

class FloorMoveThinker : public Thinker
{
protected:
   void Think();

public:
   // [CG] 09/22/11: Status for sending over the network.
#pragma pack(push, 1)
   typedef struct
   {
      uint32_t net_id;
      int32_t type;
      int32_t crush;
      int8_t direction;
      int32_t new_special;
      uint32_t flags;
      int32_t damage;
      int32_t damage_mask;
      int32_t damage_mod;
      int32_t damage_flags;
      int16_t texture;
      fixed_t floor_dest_height;
      fixed_t speed;
      int32_t reset_time;
      fixed_t reset_height;
      int32_t step_raise_time;
      int32_t delay_time;
      int32_t delay_timer;
   } status_t;
#pragma pack(pop)

   std::map<uint32_t, status_t*> statuses;

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "FloorMoveThinker"; }

   status_t* getStatus(void)
   {
      status_t *status = (status_t *)(malloc(sizeof(status_t)));

      status->net_id            = this->net_id;
      status->type              = this->type;
      status->crush             = this->crush;
      status->direction         = this->direction;
      status->new_special       = this->special.newspecial;
      status->flags             = this->special.flags;
      status->damage            = this->special.damage;
      status->damage_mask       = this->special.damagemask;
      status->damage_mod        = this->special.damagemod;
      status->damage_flags      = this->special.damageflags;
      status->texture           = this->texture;
      status->floor_dest_height = this->floordestheight;
      status->speed             = this->speed;
      status->reset_time        = this->resetTime;
      status->reset_height      = this->resetHeight;
      status->step_raise_time   = this->stepRaiseTime;
      status->delay_time        = this->delayTime;
      status->delay_timer       = this->delayTimer;

      return status;
   }

   void getStatus(status_t *status)
   {
      status->net_id            = this->net_id;
      status->type              = this->type;
      status->crush             = this->crush;
      status->direction         = this->direction;
      status->new_special       = this->special.newspecial;
      status->flags             = this->special.flags;
      status->damage            = this->special.damage;
      status->damage_mask       = this->special.damagemask;
      status->damage_mod        = this->special.damagemod;
      status->damage_flags      = this->special.damageflags;
      status->texture           = this->texture;
      status->floor_dest_height = this->floordestheight;
      status->speed             = this->speed;
      status->reset_time        = this->resetTime;
      status->reset_height      = this->resetHeight;
      status->step_raise_time   = this->stepRaiseTime;
      status->delay_time        = this->delayTime;
      status->delay_timer       = this->delayTimer;
   }

   void setStatus(status_t *status)
   {
      this->net_id              = status->net_id;
      this->type                = status->type;
      this->crush               = status->crush;
      this->direction           = status->direction;
      this->special.newspecial  = status->new_special;
      this->special.flags       = status->flags;
      this->special.damage      = status->damage;
      this->special.damagemask  = status->damage_mask;
      this->special.damagemod   = status->damage_mod;
      this->special.damageflags = status->damage_flags;
      this->texture             = status->texture;
      this->floordestheight     = status->floor_dest_height;
      this->speed               = status->speed;
      this->resetTime           = status->reset_time;
      this->resetHeight         = status->reset_height;
      this->stepRaiseTime       = status->step_raise_time;
      this->delayTime           = status->delay_time;
      this->delayTimer          = status->delay_timer;
   }

   // Data Members
   int type;
   int crush;
   sector_t *sector;
   int direction;

   // jff 3/14/98 add to fix bug in change transfers
   // haleyjd 09/06/07: spectransfer
   spectransfer_t special;

   int16_t texture;
   fixed_t floordestheight;
   fixed_t speed;
   int resetTime;       // haleyjd 10/13/05: resetting stairs
   fixed_t resetHeight;
   int stepRaiseTime;   // haleyjd 10/13/05: delayed stairs
   int delayTime;       
   int delayTimer;
   uint32_t net_id;   // [CG] 09/18/11
   uint32_t inactive; // [CG] 09/18/11
};

// haleyjd 05/07/04: extended data struct for parameterized floors
typedef struct floordata_s
{   
   // generalized values
   int trigger_type;
   int crush;
   int direction;
   int speed_type;
   int change_type;
   int change_model;
   int target_type;

   // parameterized values
   fixed_t height_value;
   fixed_t speed_value;
} floordata_t;

// haleyjd 10/06/05: extended data struct for parameterized stairs
typedef struct stairdata_s
{   
   // generalized values
   int trigger_type;
   int ignore;
   int direction;
   int stepsize_type;
   int speed_type;

   // parameterized values
   fixed_t stepsize_value;
   fixed_t speed_value;
   int delay_value;
   int reset_value;
   int sync_value;
} stairdata_t;

class ElevatorThinker : public Thinker
{
protected:
   void Think();

public:
   // [CG] 09/22/11: Status for sending over the network.
#pragma pack(push, 1)
   typedef struct
   {
      uint32_t net_id;
      int32_t type;
      int8_t direction;
      fixed_t floor_dest_height;
      fixed_t ceiling_dest_height;
      fixed_t speed;
   } status_t;
#pragma pack(pop)

   std::map<uint32_t, status_t*> statuses;

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "ElevatorThinker"; }

   status_t* getStatus(void)
   {
      status_t *status = (status_t *)(malloc(sizeof(status_t)));

      status->net_id              = this->net_id;
      status->type                = this->type;
      status->direction           = this->direction;
      status->floor_dest_height   = this->floordestheight;
      status->ceiling_dest_height = this->ceilingdestheight;
      status->speed               = this->speed;

      return status;
   }

   void getStatus(status_t *status)
   {
      status->net_id              = this->net_id;
      status->type                = this->type;
      status->direction           = this->direction;
      status->floor_dest_height   = this->floordestheight;
      status->ceiling_dest_height = this->ceilingdestheight;
      status->speed               = this->speed;
   }

   void setStatus(status_t *status)
   {
      this->net_id            = status->net_id;
      this->type              = status->type;
      this->direction         = status->direction;
      this->floordestheight   = status->floor_dest_height;
      this->ceilingdestheight = status->ceiling_dest_height;
      this->speed             = status->speed;

   }

   // Data Members
   int type;
   sector_t *sector;
   int direction;
   fixed_t floordestheight;
   fixed_t ceilingdestheight;
   fixed_t speed;
   uint32_t net_id;   // [CG] 09/18/11
   uint32_t inactive; // [CG] 09/18/11
};

// joek: pillars
class PillarThinker : public Thinker
{
protected:
   void Think();

public:
   // [CG] 09/22/11: Status for sending over the network.
#pragma pack(push, 1)
   typedef struct
   {
      uint32_t net_id;
      int32_t ceiling_speed;
      int32_t floor_speed;
      int32_t floor_dest;
      int32_t ceiling_dest;
      int8_t direction;
      int32_t crush;
   } status_t;
#pragma pack(pop)

   std::map<uint32_t, status_t*> statuses;

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "PillarThinker"; }

   status_t* getStatus(void)
   {
      status_t *status = (status_t *)(malloc(sizeof(status_t)));

      status->net_id        = this->net_id;
      status->ceiling_speed = this->ceilingSpeed;
      status->floor_speed   = this->floorSpeed;
      status->floor_dest    = this->floordest;
      status->ceiling_dest  = this->ceilingdest;
      status->direction     = this->direction;
      status->crush         = this->crush;

      return status;
   }

   void getStatus(status_t *status)
   {
      status->net_id        = this->net_id;
      status->ceiling_speed = this->ceilingSpeed;
      status->floor_speed   = this->floorSpeed;
      status->floor_dest    = this->floordest;
      status->ceiling_dest  = this->ceilingdest;
      status->direction     = this->direction;
      status->crush         = this->crush;
   }

   void setStatus(status_t *status)
   {
      this->net_id       = status->net_id;
      this->ceilingSpeed = status->ceiling_speed;
      this->floorSpeed   = status->floor_speed;
      this->floordest    = status->floor_dest;
      this->ceilingdest  = status->ceiling_dest;
      this->direction    = status->direction;
      this->crush        = status->crush;
   }

   // Data Members
   sector_t *sector;
   int ceilingSpeed;
   int floorSpeed;
   int floordest;
   int ceilingdest;
   int direction;
   int crush;
   uint32_t net_id;   // [CG] 09/18/11
   uint32_t inactive; // [CG] 09/18/11
};

// haleyjd 10/21/06: data struct for param pillars
typedef struct pillardata_s
{
   fixed_t speed;  // speed of furthest moving surface
   fixed_t fdist;  // for open, how far to open floor
   fixed_t cdist;  // for open, how far to open ceiling
   fixed_t height; // for close, where to meet
   int     crush;  // amount of crushing damage
   int     tag;    // tag
} pillardata_t;

// haleyjd 06/30/09: waggle floors
class FloorWaggleThinker : public Thinker
{
protected:
   void Think();

public:
   // [CG] 09/22/11: Status for sending over the network.
#pragma pack(push, 1)
   typedef struct
   {
      uint32_t net_id;
      fixed_t original_height;
      fixed_t accumulator;
      fixed_t acc_delta;
      fixed_t target_scale;
      fixed_t scale;
      fixed_t scale_delta;
      int32_t ticker;
      int32_t state;
   } status_t;
#pragma pack(pop)

   std::map<uint32_t, status_t*> statuses;

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "FloorWaggleThinker"; }

   status_t* getStatus(void)
   {
      status_t *status = (status_t *)(malloc(sizeof(status_t)));

      status->net_id          = this->net_id;
      status->original_height = this->originalHeight;
      status->accumulator     = this->accumulator;
      status->acc_delta       = this->accDelta;
      status->target_scale    = this->targetScale;
      status->scale           = this->scale;
      status->scale_delta     = this->scaleDelta;
      status->ticker          = this->ticker;
      status->state           = this->state;

      return status;
   }

   void getStatus(status_t *status)
   {
      status->net_id          = this->net_id;
      status->original_height = this->originalHeight;
      status->accumulator     = this->accumulator;
      status->acc_delta       = this->accDelta;
      status->target_scale    = this->targetScale;
      status->scale           = this->scale;
      status->scale_delta     = this->scaleDelta;
      status->ticker          = this->ticker;
      status->state           = this->state;
   }

   void setStatus(status_t *status)
   {
      this->net_id         = status->net_id;
      this->originalHeight = status->original_height;
      this->accumulator    = status->accumulator;
      this->accDelta       = status->acc_delta;
      this->targetScale    = status->target_scale;
      this->scale          = status->scale;
      this->scaleDelta     = status->scale_delta;
      this->ticker         = status->ticker;
      this->state          = status->state;
   }

   // Data Members
   sector_t *sector;
   fixed_t originalHeight;
   fixed_t accumulator;
   fixed_t accDelta;
   fixed_t targetScale;
   fixed_t scale;
   fixed_t scaleDelta;
   int ticker;
   int state;
   uint32_t net_id;   // [CG] 09/18/11
   uint32_t inactive; // [CG] 09/18/11
};

// p_spec

// killough 3/7/98: Add generalized scroll effects

class ScrollThinker : public Thinker
{
protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "ScrollThinker"; }

   // Data Members
   fixed_t dx, dy;      // (dx,dy) scroll speeds
   int affectee;        // Number of affected sidedef, sector, tag, or whatever
   int control;         // Control sector (-1 if none) used to control scrolling
   fixed_t last_height; // Last known height of control sector
   fixed_t vdx, vdy;    // Accumulated velocity if accelerative
   int accel;           // Whether it's accelerative
   enum
   {
      sc_side,
      sc_floor,
      sc_ceiling,
      sc_carry,
      sc_carry_ceiling,  // killough 4/11/98: carry objects hanging on ceilings
   };
   int type;              // Type of scroll effect
};

// haleyjd 04/11/10: FrictionThinker restored
// phares 3/12/98: added new model of friction for ice/sludge effects

class FrictionThinker : public Thinker
{
protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "FrictionThinker"; }

   // Data Members
   int friction;      // friction value (E800 = normal)
   int movefactor;    // inertia factor when adding to momentum
   int affectee;      // Number of affected sector
};

// phares 3/20/98: added new model of Pushers for push/pull effects

class PushThinker : public Thinker
{
protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual const char *getClassName() const { return "PushThinker"; }

   // Data Members
   enum
   {
      p_push,
      p_pull,
      p_wind,
      p_current,
   }; 
   int type;
   Mobj *source;      // Point source if point pusher
   int x_mag;           // X Strength
   int y_mag;           // Y Strength
   int magnitude;       // Vector strength for point pusher
   int radius;          // Effective radius for point pusher
   int x;               // X of point source if point pusher
   int y;               // Y of point source if point pusher
   int affectee;        // Number of affected sector
};

// sf: direction plat moving

enum
{
  plat_stop     = 0,
  plat_up       = 1,
  plat_down     = -1,
  plat_special  = 2,  // haleyjd 02/24/05
};


//////////////////////////////////////////////////////////////////
//
// external data declarations
//
//////////////////////////////////////////////////////////////////

//
// End-level timer (-TIMER option)
// frags limit (-frags)
//

extern int             levelTimeLimit;
extern int             levelFragLimit;
extern int             levelScoreLimit; // [CG] 09/23/11.

extern platlist_t *activeplats;        // killough 2/14/98

extern ceilinglist_t *activeceilings;  // jff 2/22/98

////////////////////////////////////////////////////////////////
//
// Linedef and sector special utility function prototypes
//
////////////////////////////////////////////////////////////////

int twoSided(int sector, int line);

sector_t *getSector(int currentSector, int line, int side);

side_t *getSide(int currentSector, int line, int side);

fixed_t P_FindLowestFloorSurrounding(sector_t *sec);

fixed_t P_FindHighestFloorSurrounding(sector_t *sec);

fixed_t P_FindNextHighestFloor(sector_t *sec, int currentheight);

fixed_t P_FindNextLowestFloor(sector_t *sec, int currentheight);

fixed_t P_FindLowestCeilingSurrounding(sector_t *sec); // jff 2/04/98

fixed_t P_FindHighestCeilingSurrounding(sector_t *sec); // jff 2/04/98

fixed_t P_FindNextLowestCeiling(sector_t *sec, int currentheight); // jff 2/04/98

fixed_t P_FindNextHighestCeiling(sector_t *sec, int currentheight); // jff 2/04/98

fixed_t P_FindShortestTextureAround(int secnum); // jff 2/04/98

fixed_t P_FindShortestUpperAround(int secnum); // jff 2/04/98

sector_t *P_FindModelFloorSector(fixed_t floordestheight, int secnum); //jff 02/04/98

sector_t *P_FindModelCeilingSector(fixed_t ceildestheight, int secnum); //jff 02/04/98 

int P_FindSectorFromLineTag(const line_t *line, int start); // killough 4/17/98

int P_FindLineFromLineTag(const line_t *line, int start);   // killough 4/17/98

int P_FindSectorFromTag(const int tag, int start);        // sf

int P_FindMinSurroundingLight(sector_t *sector, int max);

sector_t *getNextSector(line_t *line, sector_t *sec);

int P_CheckTag(line_t *line); // jff 2/27/98

bool P_CanUnlockGenDoor(line_t *line, player_t *player);

int P_SectorActive(special_e t, sector_t *s);

bool P_IsSecret(sector_t *sec);

bool P_WasSecret(sector_t *sec);

void P_ChangeSwitchTexture(line_t *line, int useAgain, int side);

void P_ConvertHexenLineSpec(int16_t *special, int *args);

////////////////////////////////////////////////////////////////
//
// Linedef and sector special action function prototypes
//
////////////////////////////////////////////////////////////////

// p_floor

result_e T_MovePlane(sector_t *sector, fixed_t speed, fixed_t dest,
                     int crush, int floorOrCeiling, int direction);

////////////////////////////////////////////////////////////////
//
// Linedef and sector special handler prototypes
//
////////////////////////////////////////////////////////////////

// p_telept

int EV_Teleport(line_t *line, int side, Mobj *thing);

// killough 2/14/98: Add silent teleporter
int EV_SilentTeleport(line_t *line, int side, Mobj *thing);

// killough 1/31/98: Add silent line teleporter
int EV_SilentLineTeleport(line_t *line, int side, 
			              Mobj *thing, bool reverse);

// p_floor

int EV_DoElevator(line_t *line, elevator_e type);

int EV_BuildStairs(line_t *line, stair_e type);

int EV_DoFloor(line_t *line, floor_e floortype);

// p_ceilng

int EV_DoCeiling(line_t *line, ceiling_e type);

int EV_CeilingCrushStop(line_t *line);

void P_ChangeCeilingTex(const char *name, int tag);

// p_doors

int EV_VerticalDoor(line_t *line, Mobj *thing);

int EV_DoDoor(line_t *line, vldoor_e type);

int EV_DoLockedDoor(line_t *line, vldoor_e type, Mobj *thing);

void EV_OpenDoor(int sectag, int speed, int wait_time);

void EV_CloseDoor(int sectag, int speed);

// p_lights

int EV_StartLightStrobing(line_t *line);

int EV_TurnTagLightsOff(line_t *line);

int EV_LightTurnOn(line_t *line, int bright);

int EV_LightTurnOnPartway(int tag, fixed_t level);  // killough 10/10/98

int EV_SetLight(line_t *, int tag, setlight_e type, int lvl); // haleyjd 01/09/07

int EV_FadeLight(line_t *, int tag, int destvalue, int speed); // haleyjd 01/10/07

// haleyjd 01/10/07:
int EV_GlowLight(line_t *, int tag, int maxval, int minval, int speed); 

// haleyjd 01/16/07:
int EV_StrobeLight(line_t *, int tag, int maxval, int minval, int maxtime, int mintime);

int EV_FlickerLight(line_t *, int tag, int maxval, int minval);

// p_floor

int EV_DoChange(line_t *line, change_e changetype);

int EV_DoDonut(line_t *line);

int EV_PillarBuild(line_t *line, pillardata_t *pd);	// joek: pillars

int EV_PillarOpen(line_t *line, pillardata_t *pd);

int EV_StartFloorWaggle(line_t *line, int tag, int height, int speed, 
                        int offset, int timer);

void P_ChangeFloorTex(const char *name, int tag);

// p_plats

int EV_DoPlat(line_t *line, plattype_e type, int amount);

int EV_StopPlat(line_t *line);

// p_genlin

int EV_DoGenFloor(line_t *line);

int EV_DoGenCeiling(line_t *line);

int EV_DoGenLift(line_t *line);

int EV_DoGenStairs(line_t *line);

int EV_DoGenCrusher(line_t *line);

int EV_DoGenDoor(line_t *line);

int EV_DoGenLockedDoor(line_t *line);

void P_ChangeLineTex(const char *texture, int pos, int side, int tag, bool usetag);

// haleyjd 02/23/04
extern Mobj *genDoorThing;

// p_things

int EV_ThingProjectile(int *args, bool gravity);
int EV_ThingSpawn(int *args, bool fog);
int EV_ThingActivate(int tid);
int EV_ThingDeactivate(int tid);


////////////////////////////////////////////////////////////////
//
// Linedef and sector special thinker spawning
//
////////////////////////////////////////////////////////////////

// at game start
void P_InitPicAnims(void);

void P_InitSwitchList(void);

// at map load
void P_SpawnSpecials(int);

// 
// P_SpawnDeferredSpecials
//
// SoM: Specials that copy slopes, ect., need to be collected in a separate 
// pass
void P_SpawnDeferredSpecials(int mapformat);

// every tic
void P_UpdateSpecials(void);

// when needed
bool P_UseSpecialLine(Mobj *thing, line_t *line, int side);

void P_ShootSpecialLine(Mobj *thing, line_t *line, int side);

void P_CrossSpecialLine(line_t *, int side, Mobj *thing); // killough 11/98

void P_PlayerInSpecialSector(player_t *player);
void P_PlayerOnSpecialFlat(player_t *player);

// p_switch

// haleyjd 10/16/05: moved all button code into p_switch.c
void P_ClearButtons(void);
void P_RunButtons(void);

// p_lights

void P_SpawnFireFlicker(sector_t *sector);

void P_SpawnLightFlash(sector_t *sector);

void P_SpawnStrobeFlash(sector_t *sector, int fastOrSlow, int inSync);

void P_SpawnGlowingLight(sector_t *sector);


// p_plats

PlatThinker* P_SpawnPlatform(line_t *line, sector_t *sec, int amount,
                             plattype_e type);

PlatThinker* P_SpawnGenPlatform(line_t *line, sector_t *sec);

void P_AddActivePlat(PlatThinker *plat);

void P_RemoveActivePlat(PlatThinker *plat);

void P_RemoveAllActivePlats(void);    // killough

void P_ActivateInStasis(int tag);

void P_PlatSequence(sector_t *s, const char *seqname);

// p_doors

void P_RemoveDoor(VerticalDoorThinker *door);

VerticalDoorThinker* P_SpawnTaggedDoor(line_t *line, sector_t *sec,
                                       vldoor_e type);

VerticalDoorThinker* P_SpawnManualDoor(line_t *line, sector_t *sec);

VerticalDoorThinker* P_SpawnDoorCloseIn30(sector_t *sec);

VerticalDoorThinker* P_SpawnDoorRaiseIn5Mins(sector_t *sec);

VerticalDoorThinker* P_SpawnParamDoor(line_t *line, sector_t *sector,
                                      doordata_t *dd);

void P_DoorSequence(bool raise, bool turbo, bool bounced, sector_t *s); // haleyjd

// p_floor
void P_FloorSequence(sector_t *s);

void P_RemoveFloor(FloorMoveThinker *floor);

void P_RemoveElevator(ElevatorThinker *elevator);

void P_RemovePillar(PillarThinker *pillar);

void P_RemoveFloorWaggle(FloorWaggleThinker *floorwaggle);

FloorMoveThinker* P_SpawnFloor(line_t *line, sector_t *sec, floor_e type);

void P_SpawnStairs(line_t *line, sector_t *sec, stair_e type);

FloorMoveThinker* P_SpawnDonut(line_t *line, sector_t *sec, int16_t texture,
                               fixed_t floordestheight);

FloorMoveThinker* P_SpawnDonutHole(line_t *line, sector_t *sec,
                                   fixed_t floordestheight);

ElevatorThinker* P_SpawnElevator(line_t *line, sector_t *sec,
                                 elevator_e elevtype);

PillarThinker* P_SpawnBuildPillar(line_t *line, sector_t *sector,
                                  fixed_t height, fixed_t speed, int crush);

PillarThinker* P_SpawnOpenPillar(line_t *line, sector_t *sector, fixed_t speed,
                                 fixed_t fdist, fixed_t cdist);

FloorWaggleThinker* P_SpawnFloorWaggle(line_t *line, sector_t *sector,
                                       int height, int speed, int offset,
                                       int timer);

FloorMoveThinker* P_SpawnParamFloor(line_t *line, sector_t *sector,
                                    floordata_t *fd);

// p_ceilng

CeilingThinker* P_SpawnCeiling(line_t *line, sector_t *sec, ceiling_e type);

CeilingThinker* P_SpawnParamCeiling(line_t *line, sector_t *sector,
                                    ceilingdata_t *cd);

void P_SetSectorCeilingPic(sector_t *sector, int pic); // haleyjd 08/30/09

void P_RemoveActiveCeiling(CeilingThinker *ceiling);  //jff 2/22/98

void P_RemoveAllActiveCeilings(void);                //jff 2/22/98

void P_AddActiveCeiling(CeilingThinker *c);

void P_RemoveActiveCeiling(CeilingThinker *c);

int P_ActivateInStasisCeiling(line_t *line); 

void P_CeilingSequence(sector_t *s, int noiseLevel);

Mobj *P_GetPushThing(int);                                // phares 3/23/98

// SoM 9/19/02: 3dside movement. :)
void P_AttachLines(line_t *cline, bool ceiling);
bool P_MoveAttached(sector_t *sector, bool ceiling, fixed_t delta, int crush);
void P_AttachSectors(line_t *line);

bool P_Scroll3DSides(sector_t *sector, bool ceiling, fixed_t delta, int crush);

line_t *P_FindLine(int tag, int *searchPosition);

// haleyjd: sector special transfers
void P_SetupSpecialTransfer(sector_t *, spectransfer_t *);

void P_ZeroSpecialTransfer(spectransfer_t *);

void P_TransferSectorSpecial(sector_t *, spectransfer_t *);

void P_DirectTransferSectorSpecial(sector_t *, sector_t *);

void P_ZeroSectorSpecial(sector_t *);

void P_SetLineID(line_t *line, int id);

// haleyjd: parameterized lines

// param special activation types
enum
{
   SPAC_CROSS,
   SPAC_USE,
   SPAC_IMPACT,
   SPAC_PUSH,
};

bool P_ActivateParamLine(line_t *line, Mobj *thing, int side, int spac);
bool P_ExecParamLineSpec(line_t *line, Mobj *thing, int16_t special, 
                         int *args, int side, int spac, bool reuse);

#endif

//----------------------------------------------------------------------------
//
// $Log: p_spec.h,v $
// Revision 1.30  1998/05/04  02:22:23  jim
// formatted p_specs, moved a coupla routines to p_floor
//
// Revision 1.28  1998/04/17  10:25:04  killough
// Add P_FindLineFromLineTag()
//
// Revision 1.27  1998/04/14  18:49:50  jim
// Added monster only and reverse teleports
//
// Revision 1.26  1998/04/12  02:05:54  killough
// Add ceiling light setting, start ceiling carriers
//
// Revision 1.25  1998/04/05  13:54:03  jim
// fixed switch change on second activation
//
// Revision 1.24  1998/03/31  16:52:09  jim
// Fixed uninited type field in stair builders
//
// Revision 1.23  1998/03/23  18:38:39  jim
// Switch and animation tables now lumps
//
// Revision 1.22  1998/03/23  15:24:47  phares
// Changed pushers to linedef control
//
// Revision 1.21  1998/03/20  14:24:48  jim
// Gen ceiling target now shortest UPPER texture
//
// Revision 1.20  1998/03/20  00:30:27  phares
// Changed friction to linedef control
//
// Revision 1.19  1998/03/19  16:49:00  jim
// change sector bits to combine ice and sludge
//
// Revision 1.18  1998/03/16  12:39:08  killough
// Add accelerative scrollers
//
// Revision 1.17  1998/03/15  14:39:39  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.16  1998/03/14  17:18:56  jim
// Added instant toggle floor type
//
// Revision 1.15  1998/03/09  07:24:40  killough
// Add ScrollThinker for generalized scrollers
//
// Revision 1.14  1998/03/02  12:11:35  killough
// Add scroll_effect_offset declaration
//
// Revision 1.13  1998/02/27  19:20:42  jim
// Fixed 0 tag trigger activation
//
// Revision 1.12  1998/02/23  23:47:15  jim
// Compatibility flagged multiple thinker support
//
// Revision 1.11  1998/02/23  00:42:12  jim
// Implemented elevators
//
// Revision 1.9  1998/02/17  06:20:32  killough
// Add prototypes, redefine MAXBUTTONS
//
// Revision 1.8  1998/02/13  03:28:17  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.7  1998/02/09  03:09:37  killough
// Remove limit on switches
//
// Revision 1.6  1998/02/08  05:35:48  jim
// Added generalized linedef types
//
// Revision 1.4  1998/02/02  13:43:55  killough
// Add silent teleporter
//
// Revision 1.3  1998/01/30  14:44:03  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.2  1998/01/26  19:27:29  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

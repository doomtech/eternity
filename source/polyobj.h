// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
// Polyobjects
//
// Movable segs like in Hexen, but more flexible due to application of
// dynamic binary space partitioning theory.
//
// haleyjd 02/16/06
//
//----------------------------------------------------------------------------

#ifndef POLYOBJ_H__
#define POLYOBJ_H__

// Required for: DLListItem, PointThinker (FIXME?), vertex_t
#include "m_dllist.h"
#include "p_mobj.h"
#include "r_defs.h"

//
// Defines
//

// haleyjd: use zdoom-compatible doomednums

#define POLYOBJ_ANCHOR_DOOMEDNUM      9300
#define POLYOBJ_SPAWN_DOOMEDNUM       9301
#define POLYOBJ_SPAWNCRUSH_DOOMEDNUM  9302
#define POLYOBJ_SPAWNDAMAGE_DOOMEDNUM 9303

#define POLYOBJ_START_LINE    348
#define POLYOBJ_EXPLICIT_LINE 349

//
// Polyobject Flags
//
enum
{
   POF_ATTACHED = 0x01, // is attached to the world for rendering (has dynasegs)
   POF_LINKED   = 0x02, // is attached to the world for clipping
   POF_ISBAD    = 0x04, // is bad; should not be attached/linked/moved/rendered
   POF_DAMAGING = 0x08, // does damage just by touching objects
};

struct polymaplink_t;

//
// Polyobject Structure
//
typedef struct polyobj_s
{
   int id;    // numeric id
   int first; // for hashing: index of first polyobject in this hash chain
   int next;  // for hashing: next polyobject in this hash chain

   int mirror; // numeric id of a mirroring polyobject

   subsector_t **dynaSubsecs;  // list of subsectors holding fragments
   int numDSS;                 // number of subsector pointers
   int numDSSAlloc;            // number of subsector pointers allocated

   int numVertices;            // number of vertices (generally == segCount)
   int numVerticesAlloc;       // number of vertices allocated
   vertex_t *origVerts;        // original positions relative to spawn spot
   vertex_t *tmpVerts;         // temporary vertex backups for rotation
   vertex_t **vertices;        // vertices this polyobject must move   
   
   int numLines;               // number of linedefs
   int numLinesAlloc;          // number of linedefs allocated
   line_t **lines;             // linedefs this polyobject must move

   Mobj *spawnSpotMobj;        // for use during init only!
   PointThinker spawnSpot;     // location of spawn spot
   vertex_t centerPt;          // center point
   fixed_t zdist;              // viewz distance for sorting
   angle_t angle;              // for rotation

   fixed_t blockbox[4];        // bounding box for clipping
   polymaplink_t *linkhead;    // haleyjd 05/18/06: unlink optimization
   int validcount;             // for clipping: prevents multiple checks
   int damage;                 // damage to inflict on stuck things
   fixed_t thrust;             // amount of thrust to put on blocking objects

   int seqId;                  // 10/17/06: sound sequence id

   Thinker *thinker;           // pointer to a thinker affecting this polyobj

   unsigned int flags;         // 09/11/09: polyobject flags

} polyobj_t;

//
// Polyobject Blockmap Link Structure
//
struct polymaplink_t
{
   DLListItem<polymaplink_t> link; // for blockmap links
   polyobj_t *po;                   // pointer to polyobject
   polymaplink_t *po_next;          // haleyjd 05/18/06: unlink optimization
};

//
// Polyobject Special Thinkers
//

class SaveArchive;

class PolyRotateThinker : public Thinker
{
   DECLARE_THINKER_TYPE(PolyRotateThinker, Thinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int polyObjNum;    // numeric id of polyobject (avoid C pointers here)
   int speed;         // speed of movement per frame
   int distance;      // distance to move
};

class PolyMoveThinker : public Thinker
{
   DECLARE_THINKER_TYPE(PolyMoveThinker, Thinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int polyObjNum;     // numeric id of polyobject
   int speed;          // resultant velocity
   int momx;           // x component of speed along angle
   int momy;           // y component of speed along angle
   int distance;       // total distance to move
   unsigned int angle; // angle along which to move
};

class PolySlideDoorThinker : public Thinker
{
   DECLARE_THINKER_TYPE(PolySlideDoorThinker, Thinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int polyObjNum;         // numeric id of affected polyobject
   int delay;              // delay time
   int delayCount;         // delay counter
   int initSpeed;          // initial speed 
   int speed;              // speed of motion
   int initDistance;       // initial distance to travel
   int distance;           // current distance to travel
   unsigned int initAngle; // intial angle
   unsigned int angle;     // angle of motion
   unsigned int revAngle;  // reversed angle to avoid roundoff error
   int momx;               // x component of speed along angle
   int momy;               // y component of speed along angle
   bool closing;           // if true, is closing
};

class PolySwingDoorThinker : public Thinker
{
   DECLARE_THINKER_TYPE(PolySwingDoorThinker, Thinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   int polyObjNum;    // numeric id of affected polyobject
   int delay;         // delay time
   int delayCount;    // delay counter
   int initSpeed;     // initial speed
   int speed;         // speed of rotation
   int initDistance;  // initial distance to travel
   int distance;      // current distance to travel
   bool closing;      // if true, is closing
};

//
// Line Activation Data Structures
//

typedef struct polyrotdata_s
{
   int polyObjNum;   // numeric id of polyobject to affect
   int direction;    // direction of rotation
   int speed;        // angular speed
   int distance;     // distance to move
   bool overRide;    // if true, will override any action on the object
} polyrotdata_t;

typedef struct polymovedata_s
{
   int polyObjNum;     // numeric id of polyobject to affect
   fixed_t distance;   // distance to move
   fixed_t speed;      // linear speed
   unsigned int angle; // angle of movement
   bool overRide;      // if true, will override any action on the object
} polymovedata_t;

// polyobject door types
typedef enum
{
   POLY_DOOR_SLIDE,
   POLY_DOOR_SWING,
} polydoor_e;

typedef struct polydoordata_s
{
   int polyObjNum;     // numeric id of polyobject to affect
   int doorType;       // polyobj door type
   int speed;          // linear or angular speed
   unsigned int angle; // for slide door only, angle of motion
   int distance;       // distance to move
   int delay;          // delay time after opening
} polydoordata_t;

//
// Functions
//

polyobj_t *Polyobj_GetForNum(int id);
void Polyobj_InitLevel(void);
void Polyobj_MoveOnLoad(polyobj_t *po, angle_t angle, fixed_t x, fixed_t y);

int EV_DoPolyDoor(polydoordata_t *);
int EV_DoPolyObjMove(polymovedata_t *);
int EV_DoPolyObjRotate(polyrotdata_t *);


//
// External Variables
//

extern polyobj_t *PolyObjects;
extern int numPolyObjects;
extern DLListItem<polymaplink_t> **polyblocklinks; // polyobject blockmap

#endif

// EOF


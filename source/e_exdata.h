// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
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
// ExtraData
//
// The be-all, end-all extension to the DOOM map format. Uses the
// libConfuse library like EDF.
// 
// ExtraData can extend mapthings, lines, and sectors with an
// arbitrary number of fields, with data provided in more or less
// any format. The use of a textual input language will forever
// remove any future problems caused by binary format limitations.
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_EXDATA_H__
#define E_EXDATA_H__

// Required for:
// * maplinedef_t
// * NUMLINEARGS, etc.
#include "doomdata.h"
#include "r_defs.h"

// defines

// the ExtraData control object has doomednum 5004
#define ED_CTRL_DOOMEDNUM 5004

// the ExtraData line special has number 270
#define ED_LINE_SPECIAL 270

// ExtraData mapthing structure

// haleyjd 03/03/07: usurped by new unified mapthing_t in doomdata.h

// Extended line special flags
// Because these go into a new field used only by parameterized
// line specials, I don't face the issue of running out of line
// flags anytime soon. This provides another full word for
// future expansion.
enum
{
   EX_ML_CROSS    = 0x00000001, // crossable
   EX_ML_USE      = 0x00000002, // usable
   EX_ML_IMPACT   = 0x00000004, // shootable
   EX_ML_PUSH     = 0x00000008, // reserved for future use
   EX_ML_PLAYER   = 0x00000010, // enabled for players
   EX_ML_MONSTER  = 0x00000020, // enabled for monsters
   EX_ML_MISSILE  = 0x00000040, // enabled for missiles
   EX_ML_REPEAT   = 0x00000080, // can be used multiple times
   EX_ML_1SONLY   = 0x00000100, // only activated from first side
   EX_ML_ADDITIVE = 0x00000200, // uses additive blending
   EX_ML_BLOCKALL = 0x00000400  // line blocks everything
};

// ExtraData line structure

typedef struct maplinedefext_s
{
   // standard fields
   maplinedef_t stdfields;

   // extended fields
   int   extflags;
   int   args[NUMLINEARGS];
   int   id;
   float alpha;

   // internal fields (used by ExtraData only)
   int recordnum;
   int next;

} maplinedefext_t;

// ExtraData sector structure

typedef struct ETerrain_s *eterrainptr;

typedef struct mapsectorext_s
{
   // extended fields
   double floor_xoffs;
   double floor_yoffs;
   double ceiling_xoffs;
   double ceiling_yoffs;
   double floorangle;
   double ceilingangle;
   int    flags;
   int    flagsadd;
   int    flagsrem;
   int    topmap;
   int    midmap;
   int    bottommap;
   int    damage;
   int    damagemask;
   int    damagemod;
   int    damageflags;
   int    damageflagsadd;
   int    damageflagsrem;

   unsigned int f_pflags;
   unsigned int c_pflags;
   unsigned int f_alpha;
   unsigned int c_alpha;

   eterrainptr floorterrain;
   eterrainptr ceilingterrain;

   // internal fields (used by ExtraData only)
   bool hasflags;
   bool hasdamageflags;
   int recordnum;
   int next;

} mapsectorext_t;

// Globals

void    E_LoadExtraData(void);
Mobj   *E_SpawnMapThingExt(mapthing_t *mt);
void    E_LoadLineDefExt(line_t *line, bool applySpecial);
bool    E_IsParamSpecial(int16_t special);
void    E_GetEDMapThings(mapthing_t **things, int *numthings);
void    E_GetEDLines(maplinedefext_t **lines, int *numlines);
int16_t E_LineSpecForName(const char *name);
void    E_LoadSectorExt(line_t *line);

#endif

// EOF


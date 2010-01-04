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
//  Do all the WAD I/O, get map description,
//  set up initial state and misc. LUTs.
//
//-----------------------------------------------------------------------------

#include "c_io.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "d_main.h"
#include "hu_stuff.h"
#include "doomstat.h"
#include "hu_frags.h"
#include "m_bbox.h"
#include "m_argv.h"
#include "g_game.h"
#include "w_wad.h"
#include "p_hubs.h"
#include "r_main.h"
#include "r_things.h"
#include "r_sky.h"
#include "p_chase.h"
#include "p_maputl.h"
#include "p_map.h"
#include "p_setup.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_tick.h"
#include "p_enemy.h"
#include "p_info.h"
#include "r_defs.h"
#include "s_sound.h"
#include "p_anim.h"  // haleyjd: lightning
#include "p_partcl.h"
#include "d_dialog.h"
#include "d_io.h" // SoM 3/14/2002: strncasecmp
#include "in_lude.h"
#include "a_small.h"
#include "acs_intr.h"
#include "e_exdata.h" // haleyjd: ExtraData!
#include "e_ttypes.h"
#include "polyobj.h"
#include "s_sndseq.h"
#include "r_dynseg.h"
#include "p_slopes.h"
//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//

boolean  newlevel = false;
int      doom1level = false;    // doom 1 level running under doom 2
char     levelmapname[10];

int      numvertexes;
vertex_t *vertexes;

int      numsegs;
seg_t    *segs;

int      numsectors;
sector_t *sectors;

int      numsubsectors;
subsector_t *subsectors;

int      numnodes;
node_t   *nodes;

int      numlines;
line_t   *lines;

int      numsides;
side_t   *sides;

int      numthings;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.

int       bmapwidth, bmapheight;  // size in mapblocks

// killough 3/1/98: remove blockmap limit internally:
int       *blockmap;              // was short -- killough

// offsets in blockmap are from here
int       *blockmaplump;          // was short -- killough

fixed_t   bmaporgx, bmaporgy;     // origin of block map

mobj_t    **blocklinks;           // for thing chains

//
// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without the special effect, this could
// be used as a PVS lookup as well.
//

byte *rejectmatrix;

// Maintain single and multi player starting spots.

// 1/11/98 killough: Remove limit on deathmatch starts
mapthing_t *deathmatchstarts;      // killough
size_t     num_deathmatchstarts;   // killough

mapthing_t *deathmatch_p;
mapthing_t playerstarts[MAXPLAYERS];

// haleyjd 12/28/08: made module-global
static int mapformat;

//
// ShortToLong
//
// haleyjd 06/19/06: Happy birthday to me ^_^
// Inline routine to convert a short value to a long, preserving the value
// -1 but treating any other negative value as unsigned.
//
d_inline static int ShortToLong(short value)
{
   return (value == -1) ? -1l : (int)value & 0xffff;
}

//
// SafeUintIndex
//
// haleyjd 12/04/08: Inline routine to convert an effectively unsigned short
// index into a long index, safely checking against the provided upper bound
// and substituting the value of 0 in the event of an overflow.
//
d_inline static int SafeUintIndex(short input, int limit, const char *func,
                                  const char *item)
{
   int ret = (int)(SwapShort(input)) & 0xffff;

   if(ret >= limit)
   {
      C_Printf(FC_ERROR "%s error: %s #%d out of range\a\n", func, item, ret);
      ret = 0;
   }

   return ret;
}

//
// P_LoadVertexes
//
// killough 5/3/98: reformatted, cleaned up
//
void P_LoadVertexes(int lump)
{
   byte *data;
   int i;
   
   // Determine number of vertexes:
   //  total lump length / vertex record length.
   numvertexes = W_LumpLength(lump) / sizeof(mapvertex_t);

   // Allocate zone memory for buffer.
   vertexes = Z_Calloc(numvertexes, sizeof(vertex_t), PU_LEVEL, 0);
   
   // Load data into cache.
   data = W_CacheLumpNum(lump, PU_STATIC);
   
   // Copy and convert vertex coordinates, internal representation as fixed.
   for(i = 0; i < numvertexes; ++i)
   {
      vertexes[i].x = SwapShort(((mapvertex_t *) data)[i].x) << FRACBITS;
      vertexes[i].y = SwapShort(((mapvertex_t *) data)[i].y) << FRACBITS;
      
      // SoM: Cardboard stores float versions of vertices.
      vertexes[i].fx = M_FixedToFloat(vertexes[i].x);
      vertexes[i].fy = M_FixedToFloat(vertexes[i].y);
   }

   // Free buffer memory.
   Z_Free(data);
}

//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up
//
void P_LoadSegs(int lump)
{
   int  i;
   byte *data;
   
   numsegs = W_LumpLength(lump) / sizeof(mapseg_t);
   segs = Z_Calloc(numsegs, sizeof(seg_t), PU_LEVEL, NULL);
   data = W_CacheLumpNum(lump, PU_STATIC);
   
   for(i = 0; i < numsegs; ++i)
   {
      seg_t *li = segs + i;
      mapseg_t *ml = (mapseg_t *)data + i;
      
      int side, linedef;
      line_t *ldef;

      // haleyjd 06/19/06: convert indices to unsigned
      li->v1 = &vertexes[SafeUintIndex(ml->v1, numvertexes, "vertex", "vertex")];
      li->v2 = &vertexes[SafeUintIndex(ml->v2, numvertexes, "vertex", "vertex")];

      //li->angle  = (SwapShort(ml->angle))  << 16;
      li->offset = (SwapShort(ml->offset)) << 16;

      // haleyjd 06/19/06: convert indices to unsigned
      linedef = SafeUintIndex(ml->linedef, numlines, "vertex", "line");
      ldef = &lines[linedef];
      li->linedef = ldef;
      side = SwapShort(ml->side);
      li->sidedef = &sides[ldef->sidenum[side]];
      li->frontsector = sides[ldef->sidenum[side]].sector;

      // haleyjd 05/24/08: link segs into parent linedef;
      // allows fast reverse searches
      li->linenext = ldef->segs;
      ldef->segs   = li;

      // killough 5/3/98: ignore 2s flag if second sidedef missing:
      if(ldef->flags & ML_TWOSIDED && ldef->sidenum[side^1]!=-1)
         li->backsector = sides[ldef->sidenum[side^1]].sector;
      else
         li->backsector = NULL;

      P_CalcSegLength(li);
   }
   
   Z_Free(data);
}


// SoM 5/13/09: calculate seg length
void P_CalcSegLength(seg_t *seg)
{
   float dx, dy;

   dx = seg->v2->fx - seg->v1->fx;
   dy = seg->v2->fy - seg->v1->fy;
   seg->len = (float)sqrt(dx * dx + dy * dy);
}

//
// P_LoadSubsectors
//
// killough 5/3/98: reformatted, cleaned up
//
void P_LoadSubsectors(int lump)
{
   mapsubsector_t *mss;
   byte *data;
   int  i;
   
   numsubsectors = W_LumpLength(lump) / sizeof(mapsubsector_t);
   subsectors = Z_Calloc(numsubsectors, sizeof(subsector_t), PU_LEVEL, 0);
   data = W_CacheLumpNum(lump, PU_STATIC);
   
   for(i = 0; i < numsubsectors; ++i)
   {
      mss = &(((mapsubsector_t *)data)[i]);

      // haleyjd 06/19/06: convert indices to unsigned
      subsectors[i].numlines =
         (int)SwapShort(mss->numsegs) & 0xffff;
      subsectors[i].firstline =
         (int)SwapShort(mss->firstseg) & 0xffff;
   }
   
   Z_Free(data);
}

//
// P_LoadSectors
//
// killough 5/3/98: reformatted, cleaned up
//
void P_LoadSectors(int lump)
{
   byte *data;
   int  i;
   int  defaultSndSeq;
   
   numsectors = W_LumpLength(lump) / sizeof(mapsector_t);
   sectors    = Z_Calloc(numsectors, sizeof(sector_t), PU_LEVEL, 0);
   data       = W_CacheLumpNum(lump, PU_STATIC);

   // haleyjd 09/24/06: determine what the default sound sequence is
   defaultSndSeq = LevelInfo.noAutoSequences ? 0 : -1;

   for(i = 0; i < numsectors; ++i)
   {
      sector_t *ss = sectors + i;
      const mapsector_t *ms = (mapsector_t *)data + i;
      
      ss->floorheight        = SwapShort(ms->floorheight)   << FRACBITS;
      ss->ceilingheight      = SwapShort(ms->ceilingheight) << FRACBITS;      
      ss->floorpic           = R_FlatNumForName(ms->floorpic);
      ss->lightlevel         = SwapShort(ms->lightlevel);
      ss->special            = SwapShort(ms->special);
      ss->tag                = SwapShort(ms->tag);
      ss->thinglist          = NULL;
      ss->touching_thinglist = NULL;            // phares 3/14/98

      // haleyjd 08/30/09: set ceiling pic using function
      P_SetSectorCeilingPic(ss, R_FlatNumForName(ms->ceilingpic));      

      ss->nextsec = -1; //jff 2/26/98 add fields to support locking out
      ss->prevsec = -1; // stair retriggering until build completes

      // killough 3/7/98:
      ss->heightsec     = -1;   // sector used to get floor and ceiling height
      ss->floorlightsec = -1;   // sector used to get floor lighting
      // killough 3/7/98: end changes

      // killough 4/11/98 sector used to get ceiling lighting:
      ss->ceilinglightsec = -1;
      
      // killough 4/4/98: colormaps:
      // haleyjd 03/04/07: modifications for per-sector colormap logic
      ss->bottommap = ss->midmap = ss->topmap =
         ((ss->intflags & SIF_SKY) ? global_fog_index : global_cmap_index);
            
      // SoM 9/19/02: Initialize the attached sector list for 3dsides
      ss->c_attached = ss->f_attached = NULL;
      // SoM 11/9/04: 
      ss->c_attsectors = ss->f_attsectors = NULL;

      // SoM 10/14/07:
      ss->c_asurfaces = ss->f_asurfaces = NULL;

      // SoM: init portals
      ss->c_portal = ss->f_portal = NULL;
#ifdef R_LINKEDPORTALS
      ss->groupid = R_NOGROUP;
#endif

      // SoM: Slopes!
      ss->c_slope = ss->f_slope = NULL;

      // SoM: These are kept current with floorheight and ceilingheight now
      ss->floorheightf = M_FixedToFloat(ss->floorheight);
      ss->ceilingheightf = M_FixedToFloat(ss->ceilingheight);

      ss->ptcllist = NULL; // haleyjd 02/20/04: particle list

      // haleyjd 09/24/06: sound sequences -- set default
      ss->sndSeqID = defaultSndSeq;

      // haleyjd 12/28/08: convert BOOM generalized sector types into sector flags
      //         12/31/08: convert BOOM generalized damage
      if(mapformat == LEVEL_FORMAT_DOOM && LevelInfo.levelType == LI_TYPE_DOOM)
      {
         int damagetype;

         // convert special bits into flags (correspondence is direct by design)
         ss->flags |= (ss->special & GENSECTOFLAGSMASK) >> SECRET_SHIFT;

         // convert damage
         damagetype = (ss->special & DAMAGE_MASK) >> DAMAGE_SHIFT;
         switch(damagetype)
         {
         case 1:
            ss->damage     = 5;
            ss->damagemask = 0x1f;
            ss->damagemod  = MOD_SLIME;
            break;
         case 2:
            ss->damage     = 10;
            ss->damagemask = 0x1f;
            ss->damagemod  = MOD_SLIME;
            break;
         case 3:
            ss->damage       = 20;
            ss->damagemask   = 0x1f;
            ss->damagemod    = MOD_SLIME;
            ss->damageflags |= SDMG_LEAKYSUIT;
            break;
         default:
            break;
         }
      }
   }

   Z_Free(data);
}


//
// P_LoadNodes
//
// killough 5/3/98: reformatted, cleaned up
//
void P_LoadNodes(int lump)
{
   byte *data;
   int  i;
   
   numnodes = W_LumpLength(lump) / sizeof(mapnode_t);

   // haleyjd 09/01/02:
   // Long-needed fix: bomb out on zero-length nodes
   if(!numnodes)
      I_Error("P_LoadNodes: no nodes defined for level");

   nodes = Z_Malloc(numnodes * sizeof(node_t), PU_LEVEL, 0);
   data  = W_CacheLumpNum(lump, PU_STATIC);

   for(i = 0; i < numnodes; ++i)
   {
      node_t *no = nodes + i;
      mapnode_t *mn = (mapnode_t *)data + i;
      int j;

      no->x  = SwapShort(mn->x);
      no->y  = SwapShort(mn->y);
      no->dx = SwapShort(mn->dx);
      no->dy = SwapShort(mn->dy);

      // haleyjd 05/16/08: keep floating point versions as well for dynamic
      // seg splitting operations
      no->fx  = (double)no->x;
      no->fy  = (double)no->y;
      no->fdx = (double)no->dx;
      no->fdy = (double)no->dy;

      // haleyjd 05/20/08: precalculate general line equation coefficients
      no->a   = -no->fdy;
      no->b   =  no->fdx;
      no->c   =  no->fdy * no->fx - no->fdx * no->fy;
      no->len = sqrt(no->fdx * no->fdx + no->fdy * no->fdy);

      no->x  <<= FRACBITS;
      no->y  <<= FRACBITS;
      no->dx <<= FRACBITS;
      no->dy <<= FRACBITS;

      for(j = 0; j < 2; ++j)
      {
         int k;
         no->children[j] = SwapShort(mn->children[j]);
         for(k = 0; k < 4; ++k)
            no->bbox[j][k] = SwapShort(mn->bbox[j][k]) << FRACBITS;
      }
   }
   
   Z_Free(data);
}

static void P_ConvertHereticThing(mapthing_t *mthing);

//
// P_LoadThings
//
// killough 5/3/98: reformatted, cleaned up
//
// sf: added spawnedthings for scripting
//
// haleyjd: added missing player start detection
//
void P_LoadThings(int lump)
{
   int  i;
   byte *data = W_CacheLumpNum(lump, PU_STATIC);
   mapthing_t *mapthings;
   
   numthings = W_LumpLength(lump) / sizeof(mapthingdoom_t); //sf: use global

   // haleyjd 03/03/07: allocate full mapthings
   mapthings = calloc(numthings, sizeof(mapthing_t));
   
   for(i = 0; i < numthings; ++i)
   {
      mapthingdoom_t *mt = (mapthingdoom_t *)data + i;
      mapthing_t     *ft = &mapthings[i];
      
      // haleyjd 09/11/06: wow, this should be up here.
      ft->type = SwapShort(mt->type);

      // Do not spawn cool, new monsters if !commercial
      // haleyjd: removing this for Heretic and DeHackEd
      if(demo_version < 331 && GameModeInfo->type == Game_DOOM &&
         GameModeInfo->id != commercial)
      {
         switch(ft->type)
         {
         case 68:  // Arachnotron
         case 64:  // Archvile
         case 88:  // Boss Brain
         case 89:  // Boss Shooter
         case 69:  // Hell Knight
         case 67:  // Mancubus
         case 71:  // Pain Elemental
         case 65:  // Former Human Commando
         case 66:  // Revenant
         case 84:  // Wolf SS
            continue;
         }
      }
      
      // Do spawn all other stuff.
      ft->x       = SwapShort(mt->x);
      ft->y       = SwapShort(mt->y);
      ft->angle   = SwapShort(mt->angle);      
      ft->options = SwapShort(mt->options);

      // haleyjd 10/05/05: convert heretic things
      if(LevelInfo.levelType == LI_TYPE_HERETIC)
         P_ConvertHereticThing(ft);
      
      P_SpawnMapThing(ft);
   }
   
   // haleyjd: all player things for players in this game should now be valid
   if(GameType != gt_dm)
   {
      for(i = 0; i < MAXPLAYERS; ++i)
      {
         if(playeringame[i] && !players[i].mo)
            I_Error("P_LoadThings: Missing required player start %i\n", i+1);
      }
   }

   Z_Free(data);
   Z_Free(mapthings);
}

//
// P_LoadHexenThings
//
// haleyjd: Loads a Hexen-format THINGS lump.
//
void P_LoadHexenThings(int lump)
{
   int  i;
   byte *data = W_CacheLumpNum(lump, PU_STATIC);
   mapthing_t *mapthings;
   
   numthings = W_LumpLength(lump) / sizeof(mapthinghexen_t);

   // haleyjd 03/03/07: allocate full mapthings
   mapthings = calloc(numthings, sizeof(mapthing_t));
   
   for(i = 0; i < numthings; ++i)
   {
      mapthinghexen_t *mt = (mapthinghexen_t *)data + i;
      mapthing_t      *ft = &mapthings[i];
      
      ft->tid     = SwapShort(mt->tid);
      ft->x       = SwapShort(mt->x);
      ft->y       = SwapShort(mt->y);
      ft->height  = SwapShort(mt->height);
      ft->angle   = SwapShort(mt->angle);
      ft->type    = SwapShort(mt->type);
      ft->options = SwapShort(mt->options);

      // note: args are already in order since they're just bytes

      // haleyjd 10/05/05: convert heretic things
      if(LevelInfo.levelType == LI_TYPE_HERETIC)
         P_ConvertHereticThing(ft);
      
      P_SpawnMapThing(ft);
   }
   
   // haleyjd: all player things for players in this game
   //          should now be valid in SP or co-op
   if(GameType != gt_dm)
   {
      for(i = 0; i < MAXPLAYERS; ++i)
      {
         if(playeringame[i] && !players[i].mo)
            I_Error("P_LoadThings: Missing required player start %i", i+1);
      }
   }

   Z_Free(data);
   Z_Free(mapthings);
}

//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//        ^^^
// ??? killough ???
// Does this mean secrets used to be linedef-based, rather than sector-based?
//
// killough 4/4/98: split into two functions, to allow sidedef overloading
// killough 5/3/98: reformatted, cleaned up
// haleyjd 2/26/05: ExtraData extensions
//
void P_LoadLineDefs(int lump)
{
   byte *data;
   int  i;

   numlines = W_LumpLength(lump) / sizeof(maplinedef_t);
   lines    = Z_Calloc(numlines, sizeof(line_t), PU_LEVEL, 0);
   data     = W_CacheLumpNum(lump, PU_STATIC);

   for(i = 0; i < numlines; ++i)
   {
      maplinedef_t *mld = (maplinedef_t *)data + i;
      line_t *ld = lines + i;
      vertex_t *v1, *v2;

      ld->flags   = SwapShort(mld->flags);
      ld->special = SwapShort(mld->special);
      ld->tag     = SwapShort(mld->tag);

      // haleyjd 06/19/06: convert indices to unsigned
      v1 = ld->v1 = &vertexes[SafeUintIndex(mld->v1, numvertexes, "line", "vertex")];
      v2 = ld->v2 = &vertexes[SafeUintIndex(mld->v2, numvertexes, "line", "vertex")];
      ld->dx = v2->x - v1->x;
      ld->dy = v2->y - v1->y;

      // SoM: Calculate line normal
      P_MakeLineNormal(ld);

      ld->tranlump = -1;   // killough 4/11/98: no translucency by default

      ld->slopetype = !ld->dx ? ST_VERTICAL : !ld->dy ? ST_HORIZONTAL :
         FixedDiv(ld->dy, ld->dx) > 0 ? ST_POSITIVE : ST_NEGATIVE;

      if(v1->x < v2->x)
      {
         ld->bbox[BOXLEFT]  = v1->x;
         ld->bbox[BOXRIGHT] = v2->x;
      }
      else
      {
         ld->bbox[BOXLEFT]  = v2->x;
         ld->bbox[BOXRIGHT] = v1->x;
      }

      if(v1->y < v2->y)
      {
         ld->bbox[BOXBOTTOM] = v1->y;
         ld->bbox[BOXTOP]    = v2->y;
      }
      else
      {
         ld->bbox[BOXBOTTOM] = v2->y;
         ld->bbox[BOXTOP]    = v1->y;
      }

      // haleyjd 06/19/06: convert indices, except -1, to unsigned
      ld->sidenum[0] = ShortToLong(SwapShort(mld->sidenum[0]));
      ld->sidenum[1] = ShortToLong(SwapShort(mld->sidenum[1]));

      // haleyjd 12/04/08: rangechecking for safety
      if(ld->sidenum[0] >= numsides)
      {
         C_Printf(FC_ERROR "line error: bad side 0 #%d\a\n", ld->sidenum[0]);
         ld->sidenum[0] = 0;
      }
      if(ld->sidenum[1] >= numsides)
      {
         C_Printf(FC_ERROR "line error: bad side 1 #%d\a\n", ld->sidenum[1]);
         ld->sidenum[1] = 0;
      }

      // killough 4/4/98: support special sidedef interpretation below
      if(ld->sidenum[0] != -1 && ld->special)
         sides[*ld->sidenum].special = ld->special;

      // haleyjd 04/19/09: position line sound origin
      ld->soundorg.x       = ld->v1->x + ld->dx / 2;
      ld->soundorg.y       = ld->v1->y + ld->dy / 2;
      ld->soundorg.groupid = R_NOGROUP;

      // haleyjd 02/26/05: ExtraData
      // haleyjd 04/20/08: Implicit ExtraData lines
      if(ld->special == ED_LINE_SPECIAL)
         E_LoadLineDefExt(ld, true);
      else if(E_IsParamSpecial(ld->special))
         E_LoadLineDefExt(ld, false);
   }
   Z_Free(data);
}

// these flags are shared with Hexen in the normal flags fields
#define HX_SHAREDFLAGS \
   (ML_BLOCKING|ML_BLOCKMONSTERS|ML_TWOSIDED|ML_DONTPEGTOP|ML_DONTPEGBOTTOM| \
    ML_SECRET|ML_SOUNDBLOCK|ML_DONTDRAW|ML_MAPPED)

// line flags belonging only to Hexen
#define HX_ML_REPEAT_SPECIAL 0x0200

// special activation crap -- definitely the only place this will be used
#define HX_SPAC_SHIFT 10
#define HX_SPAC_MASK  0x1c00
#define HX_GET_SPAC(flags) ((flags & HX_SPAC_MASK) >> HX_SPAC_SHIFT)

enum
{
   HX_SPAC_CROSS,  // when player crosses line
   HX_SPAC_USE,    // when player uses line
   HX_SPAC_MCROSS, // when monster crosses line
   HX_SPAC_IMPACT, // when projectile hits line
   HX_SPAC_PUSH,   // when player/monster pushes line
   HX_SPAC_PCROSS, // when projectile crosses line
   HX_SPAC_NUMSPAC
};

// haleyjd 02/28/07: tablified
static int spac_flags_tlate[HX_SPAC_NUMSPAC] =
{
   EX_ML_CROSS  | EX_ML_PLAYER,                   // SPAC_CROSS
   EX_ML_USE    | EX_ML_PLAYER,                   // SPAC_USE
   EX_ML_CROSS  | EX_ML_MONSTER,                  // SPAC_MCROSS
   EX_ML_IMPACT | EX_ML_MISSILE,                  // SPAC_IMPACT
   EX_ML_PUSH   | EX_ML_PLAYER   | EX_ML_MONSTER, // SPAC_PUSH
   EX_ML_CROSS  | EX_ML_MISSILE                   // SPAC_PCROSS
};

//
// P_ConvertHexenLineFlags
//
// Uses the defines above to fiddle with the Hexen map format's line flags
// and turn them into something meaningful to Eternity. The values get split
// between line->flags and line->extflags.
//
static void P_ConvertHexenLineFlags(line_t *line)
{
   // extract Hexen special activation information
   short spac = HX_GET_SPAC(line->flags);

   // translate into ExtraData extended line flags
   line->extflags = spac_flags_tlate[spac];

   // check for repeat flag
   if(line->flags & HX_ML_REPEAT_SPECIAL)
      line->extflags |= EX_ML_REPEAT;

   // FIXME/TODO: set 1SONLY line flag here, or elsewhere? Depends on special...

   // clear line flags to those that are shared with DOOM
   line->flags &= HX_SHAREDFLAGS;

   // FIXME/TODO: how to support Eternity's extended normal flags in Hexen?
   // We want Hexen to be able to use stuff like 3DMidTex also.
}

//
// P_LoadHexenLineDefs
//
// haleyjd: Loads a Hexen-format LINEDEFS lump.
//
void P_LoadHexenLineDefs(int lump)
{
   byte *data;
   int  i;

   numlines = W_LumpLength(lump) / sizeof(maplinedefhexen_t);
   lines = Z_Calloc(numlines, sizeof(line_t), PU_LEVEL, 0);
   data = W_CacheLumpNum(lump, PU_STATIC);

   for(i = 0; i < numlines; ++i)
   {
      maplinedefhexen_t *mld = (maplinedefhexen_t *)data + i;
      line_t *ld = lines + i;
      vertex_t *v1, *v2;
      int argnum;

      ld->flags = SwapShort(mld->flags);

      ld->special = mld->special;
      
      for(argnum = 0; argnum < NUMHXLINEARGS; ++argnum)
         ld->args[argnum] = mld->args[argnum];

      // Do Hexen special translation, unconditionally
      // TODO: support more than one mode (ie, strict Hexen, non-strict, etc)
      P_ConvertHexenLineSpec(&(ld->special), ld->args);

      // Convert line flags after setting special?
      P_ConvertHexenLineFlags(ld);

      ld->tag = -1; // haleyjd 02/27/07

      // haleyjd 06/19/06: convert indices to unsigned
      v1 = ld->v1 = &vertexes[(int)SwapShort(mld->v1) & 0xffff];
      v2 = ld->v2 = &vertexes[(int)SwapShort(mld->v2) & 0xffff];
      ld->dx = v2->x - v1->x;
      ld->dy = v2->y - v1->y;

      // SoM: Calculate line normal
      P_MakeLineNormal(ld);

      ld->tranlump = -1;   // killough 4/11/98: no translucency by default

      ld->slopetype = !ld->dx ? ST_VERTICAL : !ld->dy ? ST_HORIZONTAL :
         FixedDiv(ld->dy, ld->dx) > 0 ? ST_POSITIVE : ST_NEGATIVE;

      if(v1->x < v2->x)
      {
         ld->bbox[BOXLEFT] = v1->x;
         ld->bbox[BOXRIGHT] = v2->x;
      }
      else
      {
         ld->bbox[BOXLEFT] = v2->x;
         ld->bbox[BOXRIGHT] = v1->x;
      }

      if(v1->y < v2->y)
      {
         ld->bbox[BOXBOTTOM] = v1->y;
         ld->bbox[BOXTOP] = v2->y;
      }
      else
      {
         ld->bbox[BOXBOTTOM] = v2->y;
         ld->bbox[BOXTOP] = v1->y;
      }

      // haleyjd 06/19/06: convert indices, except -1, to unsigned
      ld->sidenum[0] = ShortToLong(SwapShort(mld->sidenum[0]));
      ld->sidenum[1] = ShortToLong(SwapShort(mld->sidenum[1]));

      // killough 4/4/98: support special sidedef interpretation below
      if(ld->sidenum[0] != -1 && ld->special)
         sides[*ld->sidenum].special = ld->special;

      // haleyjd 04/19/09: position line sound origin
      ld->soundorg.x       = ld->v1->x + ld->dx / 2;
      ld->soundorg.y       = ld->v1->y + ld->dy / 2;
      ld->soundorg.groupid = R_NOGROUP;

      // haleyjd 02/26/05: ExtraData
      // FIXME/TODO: how to support ExtraData via Hexen??
#if 0
      if(ld->special == ED_LINE_SPECIAL)
         E_LoadLineDefExt(ld);
#endif
   }
   Z_Free(data);
}


// killough 4/4/98: delay using sidedefs until they are loaded
// killough 5/3/98: reformatted, cleaned up

void P_LoadLineDefs2(void)
{
   int i = numlines;
   register line_t *ld = lines;

   for(; i--; ld++)
   {
      // killough 11/98: fix common wad errors (missing sidedefs):
      
      if(ld->sidenum[0] == -1)
      {
         C_Printf(FC_ERROR "line error: missing first sidedef replaced\a\n");
         ld->sidenum[0] = 0;  // Substitute dummy sidedef for missing right side
      }

      if(ld->sidenum[1] == -1)
         ld->flags &= ~ML_TWOSIDED;  // Clear 2s flag for missing left side

      // haleyjd 05/02/06: Reserved line flag. If set, we must clear all
      // BOOM or later extended line flags. This is necessitated by E2M7.
      if(ld->flags & ML_RESERVED)
         ld->flags &= ML_BLOCKING|ML_BLOCKMONSTERS|ML_TWOSIDED|ML_DONTPEGTOP|
                      ML_DONTPEGBOTTOM|ML_SECRET|ML_SOUNDBLOCK|ML_DONTDRAW|
                      ML_MAPPED;

      // haleyjd 03/13/05: removed redundant -1 check for first side
      ld->frontsector = sides[ld->sidenum[0]].sector;
      ld->backsector  = ld->sidenum[1] != -1 ? sides[ld->sidenum[1]].sector : 0;
      
      switch(ld->special)
      {                       // killough 4/11/98: handle special types
         int lump, j;
      case 260:               // killough 4/11/98: translucent 2s textures
         lump = sides[*ld->sidenum].special; // translucency from sidedef
         if(!ld->tag)                        // if tag == 0,
            ld->tranlump = lump;             // affect this linedef only
         else
         {
            for(j = 0; j < numlines; ++j)    // if tag != 0,
            {
               if(lines[j].tag == ld->tag)   // affect all matching linedefs
                  lines[j].tranlump = lump;
            }
         }
         break;
      }
   } // end for
}

//
// P_LoadSideDefs
//
// killough 4/4/98: split into two functions
//
void P_LoadSideDefs(int lump)
{
   numsides = W_LumpLength(lump) / sizeof(mapsidedef_t);
   sides = Z_Calloc(numsides, sizeof(side_t), PU_LEVEL, 0);
}

// killough 4/4/98: delay using texture names until
// after linedefs are loaded, to allow overloading.
// killough 5/3/98: reformatted, cleaned up

void P_LoadSideDefs2(int lump)
{
   byte *data = W_CacheLumpNum(lump, PU_STATIC);
   int  i;

   for(i = 0; i < numsides; ++i)
   {
      register mapsidedef_t *msd = (mapsidedef_t *)data + i;
      register side_t *sd = sides + i;
      register sector_t *sec;
      int cmap;

      sd->textureoffset = SwapShort(msd->textureoffset) << FRACBITS;
      sd->rowoffset     = SwapShort(msd->rowoffset)     << FRACBITS;

      // killough 4/4/98: allow sidedef texture names to be overloaded
      // killough 4/11/98: refined to allow colormaps to work as wall
      // textures if invalid as colormaps but valid as textures.

      // haleyjd 06/19/06: convert indices to unsigned
      sd->sector = sec = &sectors[SafeUintIndex(msd->sector, numsectors, "side", "sector")];

      switch(sd->special)
      {
      case 242:                  // variable colormap via 242 linedef
         if((cmap = R_ColormapNumForName(msd->bottomtexture)) < 0)
            sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
         else
         {
            sec->bottommap = cmap;
            sd->bottomtexture = 0;
         }
         if((cmap = R_ColormapNumForName(msd->midtexture)) < 0)
            sd->midtexture = R_TextureNumForName(msd->midtexture);
         else
         {
            sec->midmap = cmap;
            sd->midtexture = 0;
         }
         if((cmap = R_ColormapNumForName(msd->toptexture)) < 0)
            sd->toptexture = R_TextureNumForName(msd->toptexture);
         else
         {
            sec->topmap = cmap;
            sd->toptexture = 0;
         }
         break;

      case 260: // killough 4/11/98: apply translucency to 2s normal texture
         sd->midtexture = strncasecmp("TRANMAP", msd->midtexture, 8) ?
            (sd->special = W_CheckNumForName(msd->midtexture)) < 0 ||
            W_LumpLength(sd->special) != 65536 ?
            sd->special=0, R_TextureNumForName(msd->midtexture) :
               (sd->special++, 0) : (sd->special=0);
         sd->toptexture = R_TextureNumForName(msd->toptexture);
         sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
         break;

      default:                        // normal cases
         sd->midtexture    = R_TextureNumForName(msd->midtexture);
         sd->toptexture    = R_TextureNumForName(msd->toptexture);
         sd->bottomtexture = R_TextureNumForName(msd->bottomtexture);
         break;
      }
   }

   Z_Free(data);
}

//
// killough 10/98:
//
// Rewritten to use faster algorithm.
//
// New procedure uses Bresenham-like algorithm on the linedefs, adding the
// linedef to each block visited from the beginning to the end of the linedef.
//
// The algorithm's complexity is on the order of nlines*total_linedef_length.
//
// Please note: This section of code is not interchangable with TeamTNT's
// code which attempts to fix the same problem.

static void P_CreateBlockMap(void)
{
   register unsigned int i;
   fixed_t minx = INT_MAX, miny = INT_MAX,
           maxx = INT_MIN, maxy = INT_MIN;

   // First find limits of map
   
   for(i = 0; i < (unsigned)numvertexes; ++i)
   {
      if((vertexes[i].x >> FRACBITS) < minx)
         minx = vertexes[i].x >> FRACBITS;
      else if((vertexes[i].x >> FRACBITS) > maxx)
         maxx = vertexes[i].x >> FRACBITS;

      if((vertexes[i].y >> FRACBITS) < miny)
         miny = vertexes[i].y >> FRACBITS;
      else if((vertexes[i].y >> FRACBITS) > maxy)
         maxy = vertexes[i].y >> FRACBITS;
   }

   // Save blockmap parameters
   
   bmaporgx   = minx << FRACBITS;
   bmaporgy   = miny << FRACBITS;
   bmapwidth  = ((maxx - minx) >> MAPBTOFRAC) + 1;
   bmapheight = ((maxy - miny) >> MAPBTOFRAC) + 1;

   // Compute blockmap, which is stored as a 2d array of variable-sized 
   // lists.
   //
   // Pseudocode:
   //
   // For each linedef:
   //
   //   Map the starting and ending vertices to blocks.
   //
   //   Starting in the starting vertex's block, do:
   //
   //     Add linedef to current block's list, dynamically resizing it.
   //
   //     If current block is the same as the ending vertex's block,
   //     exit loop.
   //
   //     Move to an adjacent block by moving towards the ending block
   //     in either the x or y direction, to the block which contains 
   //     the linedef.

   {
      typedef struct { int n, nalloc, *list; } bmap_t;  // blocklist structure
      unsigned tot = bmapwidth * bmapheight;            // size of blockmap
      bmap_t *bmap = calloc(sizeof *bmap, tot);         // array of blocklists

      for(i = 0; i < (unsigned)numlines; ++i)
      {
         // starting coordinates
         int x = (lines[i].v1->x >> FRACBITS) - minx;
         int y = (lines[i].v1->y >> FRACBITS) - miny;
         
         // x-y deltas
         int adx = lines[i].dx >> FRACBITS, dx = adx < 0 ? -1 : 1;
         int ady = lines[i].dy >> FRACBITS, dy = ady < 0 ? -1 : 1; 

         // difference in preferring to move across y (>0) 
         // instead of x (<0)
         int diff = !adx ? 1 : !ady ? -1 :
          (((x >> MAPBTOFRAC) << MAPBTOFRAC) + 
           (dx > 0 ? MAPBLOCKUNITS-1 : 0) - x) * (ady = D_abs(ady)) * dx -
          (((y >> MAPBTOFRAC) << MAPBTOFRAC) + 
           (dy > 0 ? MAPBLOCKUNITS-1 : 0) - y) * (adx = D_abs(adx)) * dy;

         // starting block, and pointer to its blocklist structure
         int b = (y >> MAPBTOFRAC) * bmapwidth + (x >> MAPBTOFRAC);

         // ending block
         int bend = (((lines[i].v2->y >> FRACBITS) - miny) >> MAPBTOFRAC) *
            bmapwidth + (((lines[i].v2->x >> FRACBITS) - minx) >> MAPBTOFRAC);

         // delta for pointer when moving across y
         dy *= bmapwidth;

         // deltas for diff inside the loop
         adx <<= MAPBTOFRAC;
         ady <<= MAPBTOFRAC;

         // Now we simply iterate block-by-block until we reach the end block.
         while((unsigned) b < tot)    // failsafe -- should ALWAYS be true
         {
            // Increase size of allocated list if necessary
            if(bmap[b].n >= bmap[b].nalloc)
            {
               bmap[b].list = 
                  realloc(bmap[b].list,
                          (bmap[b].nalloc = bmap[b].nalloc ? 
                           bmap[b].nalloc*2 : 8)*sizeof*bmap->list);
            }

            // Add linedef to end of list
            bmap[b].list[bmap[b].n++] = i;

            // If we have reached the last block, exit
            if(b == bend)
               break;

            // Move in either the x or y direction to the next block
            if(diff < 0) 
               diff += ady, b += dx;
            else
               diff -= adx, b += dy;
         }
      }

      // Compute the total size of the blockmap.
      //
      // Compression of empty blocks is performed by reserving two 
      // offset words at tot and tot+1.
      //
      // 4 words, unused if this routine is called, are reserved at 
      // the start.

      {
         // we need at least 1 word per block, plus reserved's
         int count = tot + 6;

         for(i = 0; i < tot; ++i)
         {
            // 1 header word + 1 trailer word + blocklist
            if(bmap[i].n)
               count += bmap[i].n + 2; 
         }

         // Allocate blockmap lump with computed count
         blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count, 
                                 PU_LEVEL, 0);
      }

      // Now compress the blockmap.
      {
         int ndx = tot += 4;      // Advance index to start of linedef lists
         bmap_t *bp = bmap;       // Start of uncompressed blockmap

         blockmaplump[ndx++] = 0;  // Store an empty blockmap list at start
         blockmaplump[ndx++] = -1; // (Used for compression)

         for(i = 4; i < tot; i++, bp++)
         {
            if(bp->n)                          // Non-empty blocklist
            {
               blockmaplump[blockmaplump[i] = ndx++] = 0;  // Store index & header
               do
                  blockmaplump[ndx++] = bp->list[--bp->n]; // Copy linedef list
               while (bp->n);
               blockmaplump[ndx++] = -1;                   // Store trailer
               free(bp->list);                             // Free linedef list
            }
            else     // Empty blocklist: point to reserved empty blocklist
               blockmaplump[i] = tot;
         }
         
         free(bmap);    // Free uncompressed blockmap
      }
   }
}

//
// P_LoadBlockMap
//
// killough 3/1/98: substantially modified to work
// towards removing blockmap limit (a wad limitation)
//
// killough 3/30/98: Rewritten to remove blockmap limit
//
void P_LoadBlockMap(int lump)
{
   int count;
   
   // sf: -blockmap checkparm made into variable
   // also checking for levels without blockmaps (0 length)
   if(r_blockmap || W_LumpLength(lump) == 0 || 
      (count = W_LumpLength(lump) / 2) >= 0x10000)
   {
      P_CreateBlockMap();
   }
   else
   {
      int i;
      short *wadblockmaplump = W_CacheLumpNum (lump, PU_LEVEL);
      blockmaplump = Z_Malloc(sizeof(*blockmaplump) * count,
                              PU_LEVEL, NULL);

      // killough 3/1/98: Expand wad blockmap into larger internal one,
      // by treating all offsets except -1 as unsigned and zero-extending
      // them. This potentially doubles the size of blockmaps allowed,
      // because Doom originally considered the offsets as always signed.

      blockmaplump[0] = SwapShort(wadblockmaplump[0]);
      blockmaplump[1] = SwapShort(wadblockmaplump[1]);
      blockmaplump[2] = (int)(SwapShort(wadblockmaplump[2])) & 0xffff;
      blockmaplump[3] = (int)(SwapShort(wadblockmaplump[3])) & 0xffff;

      for(i = 4; i < count; i++)
      {
         short t = SwapShort(wadblockmaplump[i]);          // killough 3/1/98
         blockmaplump[i] = t == -1 ? -1l : (int) t & 0xffff;
      }

      Z_Free(wadblockmaplump);

      bmaporgx   = blockmaplump[0] << FRACBITS;
      bmaporgy   = blockmaplump[1] << FRACBITS;
      bmapwidth  = blockmaplump[2];
      bmapheight = blockmaplump[3];
   }

   // clear out mobj chains
   count = sizeof(*blocklinks) * bmapwidth * bmapheight;
   blocklinks = Z_Calloc(1, count, PU_LEVEL, NULL);
   blockmap = blockmaplump + 4;

   // haleyjd 2/22/06: setup polyobject blockmap
   count = sizeof(*polyblocklinks) * bmapwidth * bmapheight;
   polyblocklinks = Z_Calloc(1, count, PU_LEVEL, NULL);
}


//
// AddLineToSector
//
// Utility function for P_GroupLines below.
//
static void AddLineToSector(sector_t *s, line_t *l)
{
   M_AddToBox(s->blockbox, l->v1->x, l->v1->y);
   M_AddToBox(s->blockbox, l->v2->x, l->v2->y);
   *s->lines++ = l;
}

//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
// killough 5/3/98: reformatted, cleaned up
// killough 8/24/98: rewrote to use faster algorithm
//
void P_GroupLines(void)
{
   int i, total;
   line_t **linebuffer;

   // look up sector number for each subsector
   for(i = 0; i < numsubsectors; ++i)
      subsectors[i].sector = segs[subsectors[i].firstline].sidedef->sector;

   // count number of lines in each sector
   for(i = 0; i < numlines; ++i)
   {
      lines[i].frontsector->linecount++;
      if(lines[i].backsector && 
         lines[i].backsector != lines[i].frontsector)
      {
         lines[i].backsector->linecount++;
      }
   }

   // compute total number of lines and clear bounding boxes
   for(total = 0, i = 0; i < numsectors; ++i)
   {
      total += sectors[i].linecount;
      M_ClearBox(sectors[i].blockbox);
   }

   // build line tables for each sector
   linebuffer = Z_Malloc(total * sizeof(*linebuffer), PU_LEVEL, 0);

   for(i = 0; i < numsectors; ++i)
   {
      sectors[i].lines = linebuffer;
      linebuffer += sectors[i].linecount;
   }
  
   for(i = 0; i < numlines; ++i)
   {
      AddLineToSector(lines[i].frontsector, &lines[i]);
      if(lines[i].backsector && 
         lines[i].backsector != lines[i].frontsector)
      {
         AddLineToSector(lines[i].backsector, &lines[i]);
      }
   }

   for(i = 0; i < numsectors; ++i)
   {
      sector_t *sector = sectors+i;
      int block;

      // adjust pointers to point back to the beginning of each list
      sector->lines -= sector->linecount;
      
      // set the degenmobj_t to the middle of the bounding box
      sector->soundorg.x = (sector->blockbox[BOXRIGHT] + 
                            sector->blockbox[BOXLEFT])/2;
      sector->soundorg.y = (sector->blockbox[BOXTOP] + 
                            sector->blockbox[BOXBOTTOM])/2;
#ifdef R_LINKEDPORTALS
      // SoM: same for group id.
      // haleyjd: note - groups have not been built yet, so this is just for
      // initialization.
      sector->soundorg.groupid = sector->groupid;
#endif

      // haleyjd 10/16/06: copy all properties to ceiling origin
      sector->csoundorg = sector->soundorg;

      // adjust bounding box to map blocks
      block = (sector->blockbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block >= bmapheight ? bmapheight-1 : block;
      sector->blockbox[BOXTOP]=block;

      block = (sector->blockbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block < 0 ? 0 : block;
      sector->blockbox[BOXBOTTOM]=block;

      block = (sector->blockbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block >= bmapwidth ? bmapwidth-1 : block;
      sector->blockbox[BOXRIGHT]=block;

      block = (sector->blockbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
      block = block < 0 ? 0 : block;
      sector->blockbox[BOXLEFT]=block;
   }
}

//
// killough 10/98
//
// Remove slime trails.
//
// Slime trails are inherent to Doom's coordinate system -- i.e. there is
// nothing that a node builder can do to prevent slime trails ALL of the time,
// because it's a product of the integer coordinate system, and just because
// two lines pass through exact integer coordinates, doesn't necessarily mean
// that they will intersect at integer coordinates. Thus we must allow for
// fractional coordinates if we are to be able to split segs with node lines,
// as a node builder must do when creating a BSP tree.
//
// A wad file does not allow fractional coordinates, so node builders are out
// of luck except that they can try to limit the number of splits (they might
// also be able to detect the degree of roundoff error and try to avoid splits
// with a high degree of roundoff error). But we can use fractional coordinates
// here, inside the engine. It's like the difference between square inches and
// square miles, in terms of granularity.
//
// For each vertex of every seg, check to see whether it's also a vertex of
// the linedef associated with the seg (i.e, it's an endpoint). If it's not
// an endpoint, and it wasn't already moved, move the vertex towards the
// linedef by projecting it using the law of cosines. Formula:
//
//      2        2                         2        2
//    dx  x0 + dy  x1 + dx dy (y0 - y1)  dy  y0 + dx  y1 + dx dy (x0 - x1)
//   {---------------------------------, ---------------------------------}
//                  2     2                            2     2
//                dx  + dy                           dx  + dy
//
// (x0,y0) is the vertex being moved, and (x1,y1)-(x1+dx,y1+dy) is the
// reference linedef.
//
// Segs corresponding to orthogonal linedefs (exactly vertical or horizontal
// linedefs), which comprise at least half of all linedefs in most wads, don't
// need to be considered, because they almost never contribute to slime trails
// (because then any roundoff error is parallel to the linedef, which doesn't
// cause slime). Skipping simple orthogonal lines lets the code finish quicker.
//
// Please note: This section of code is not interchangable with TeamTNT's
// code which attempts to fix the same problem.
//
// Firelines (TM) is a Rezistered Trademark of MBF Productions
//

void P_RemoveSlimeTrails(void)             // killough 10/98
{
   byte *hit = calloc(1, numvertexes);     // Hitlist for vertices
   int i;
   
   for(i=0; i<numsegs; i++)                // Go through each seg
   {
      const line_t *l = segs[i].linedef;   // The parent linedef
      if (l->dx && l->dy)                  // We can ignore orthogonal lines
      {
         vertex_t *v = segs[i].v1;
         do
         {
            if(!hit[v - vertexes])         // If we haven't processed vertex
            {
               hit[v - vertexes] = 1;        // Mark this vertex as processed
               if(v != l->v1 && v != l->v2)  // Exclude endpoints of linedefs
               { // Project the vertex back onto the parent linedef
                  int64_t dx2 = (l->dx >> FRACBITS) * (l->dx >> FRACBITS);
                  int64_t dy2 = (l->dy >> FRACBITS) * (l->dy >> FRACBITS);
                  int64_t dxy = (l->dx >> FRACBITS) * (l->dy >> FRACBITS);
                  int64_t s = dx2 + dy2;
                  int x0 = v->x, y0 = v->y, x1 = l->v1->x, y1 = l->v1->y;
                  v->x = (int)((dx2 * x0 + dy2 * x1 + dxy * (y0 - y1)) / s);
                  v->y = (int)((dy2 * y0 + dx2 * y1 + dxy * (x0 - x1)) / s);
                  // Cardboard store float versions of vertices.
                  v->fx = M_FixedToFloat(v->x);
                  v->fy = M_FixedToFloat(v->y);
               }
            }  // Obfuscated C contest entry:   :)
         }
         while((v != segs[i].v2) && (v = segs[i].v2));
      }
   }
   free(hit);
}

//
// P_LoadReject
//
// haleyjd 01/26/04: Although DOOM accepted them due to differing
// Z_Malloc behavior, Eternity was crashing on levels with zero-
// length reject lumps. This function will test to see if the reject
// lump is zero in size, and if so, will generate a reject with all
// zeroes. This is preferable to adding checks to see if a reject
// matrix exists, in my opinion. This could be improved by actually
// generating a meaningful reject, but that will have to wait.
//
static void P_LoadReject(int lump)
{
   int size;
   int expectedsize;

   size = W_LumpLength(lump);

   // haleyjd: round numsectors^2 to next higher multiple of 8, then divide by
   // 8 to get the expected reject size for this level
   expectedsize = (((numsectors * numsectors) + 7) & ~7) / 8;

   // 12/04/08: be super-smart about REJECT lumps:
   // 1. if size >= expectedsize, all is good.
   // 2. if size <  expectedsize, allocate a zero-filled buffer and copy
   //    in whatever exists from the actual lump.
   if(size >= expectedsize)
      rejectmatrix = W_CacheLumpNum(lump, PU_LEVEL);
   else
   {
      // set to all zeroes so that the reject has no effect
      rejectmatrix = Z_Calloc(1, expectedsize, PU_LEVEL, NULL);

      if(size > 0)
      {
         byte *temp = W_CacheLumpNum(lump, PU_CACHE);
         memcpy(rejectmatrix, temp, size);
      }
   }

   // warn on too-large rejects, but do nothing special.
   if(size > expectedsize)
      C_Printf(FC_ERROR "P_LoadReject warning: reject is too large\a\n");
}

//
// Map lumps table
//
// For Doom- and Hexen-format maps.
//
static const char *levellumps[] =
{
   "label",    // ML_LABEL,    A separator, name, ExMx or MAPxx
   "THINGS",   // ML_THINGS,   Monsters, items..
   "LINEDEFS", // ML_LINEDEFS, LineDefs, from editing
   "SIDEDEFS", // ML_SIDEDEFS, SideDefs, from editing
   "VERTEXES", // ML_VERTEXES, Vertices, edited and BSP splits generated
   "SEGS",     // ML_SEGS,     LineSegs, from LineDefs split by BSP
   "SSECTORS", // ML_SSECTORS, SubSectors, list of LineSegs
   "NODES",    // ML_NODES,    BSP nodes
   "SECTORS",  // ML_SECTORS,  Sectors, from editing
   "REJECT",   // ML_REJECT,   LUT, sector-sector visibility
   "BLOCKMAP", // ML_BLOCKMAP  LUT, motion clipping, walls/grid element
   "BEHAVIOR"  // ML_BEHAVIOR  haleyjd: ACS bytecode; used to id hexen maps
};

//
// P_CheckLevel
//
// sf 11/9/99
// we need to do this now because we no longer have to
// conform to the MAPxy or ExMy standard previously
// imposed
//
int P_CheckLevel(int lumpnum)
{
   int i, ln;
   
   for(i = ML_THINGS; i <= ML_BEHAVIOR; ++i)
   {
      ln = lumpnum + i;
      if(ln >= w_GlobalDir.numlumps ||     // past the last lump?
         strncmp(w_GlobalDir.lumpinfo[ln]->name, levellumps[i], 8))
      {
         // If "BEHAVIOR" wasn't found, we assume we are dealing with
         // a DOOM-format map, and it is not an error; any other missing
         // lump means the map is invalid.

         if(i == ML_BEHAVIOR)
            return LEVEL_FORMAT_DOOM;
         else
            return LEVEL_FORMAT_INVALID;
      }
   }

   // if we got here, we're dealing with a Hexen-format map

   return LEVEL_FORMAT_HEXEN;
}

void P_ConvertHereticSpecials(void); // haleyjd

void P_LoadOlo(void);
extern int level_error;

void P_InitThingLists(void); // haleyjd

//
// P_NewLevelMsg
//
// Called when loading a new map.
// haleyjd 06/04/05: moved here and renamed from HU_NewLevel
//
static void P_NewLevelMsg(void)
{   
   C_Printf("\n");
   C_Separator();
   C_Printf("%c  %s\n\n", 128+CR_GRAY, LevelInfo.levelName);
   C_InstaPopup();       // put console away
}

//
// P_SetupLevel
//
// killough 5/3/98: reformatted, cleaned up
//
void P_SetupLevel(char *mapname, int playermask, skill_t skill)
{
   int i, lumpnum;
   
   totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
   wminfo.partime = 180;
   c_showprompt = false;      // kill console prompt as nothing can
                              // be typed at the moment

   for(i = 0; i < MAXPLAYERS; ++i)
      players[i].killcount = players[i].secretcount = players[i].itemcount = 0;

   // Initial height of PointOfView will be set by player think.
   players[consoleplayer].viewz = 1;

   // haleyjd 03/15/03: clear levelscript callbacks
   // haleyjd 01/07/07: reset ACS interpreter state
#ifndef EE_NO_SMALL_SUPPORT
   SM_RemoveCallbacks(SC_VM_LEVELSCRIPT);
#endif
   ACS_InitLevel();

   // get the map name lump number
   if((lumpnum = W_CheckNumForName(mapname)) == -1)
   {
      C_Printf(FC_ERROR"Map not found: '%s'\n", mapname);
      C_SetConsole();
      return;
   }

   // determine map format; if invalid, abort
   if((mapformat = P_CheckLevel(lumpnum)) == LEVEL_FORMAT_INVALID)
   {
      C_Printf(FC_ERROR "Not a level: '%s'\n", mapname);
      C_SetConsole();
      return;
   }

   // haleyjd 07/22/04: moved up
   newlevel = (w_GlobalDir.lumpinfo[lumpnum]->file != iwadhandle);
   doom1level = false;

   strncpy(levelmapname, mapname, 8);
   leveltime = 0;
   
   // Make sure all sounds are stopped before Z_FreeTags. - sf: why?
   //  sf: s_start split into s_start, s_stopsounds because of this requirement
   S_StopSounds();

   // haleyjd 05/16/08: must clear dynamic segs before Z_FreeTags
   R_ClearDynaSegs();
   
   // free the old level
   Z_FreeTags(PU_LEVEL, PU_LEVSPEC);

   // haleyjd: explicitly nullify old player object pointers here
   for(i = 0; i < MAXPLAYERS; ++i)
      players[i].mo = NULL;

   P_FreeSecNodeList(); // sf: free the psecnode_t linked list in p_map.c
   P_InitThinkers();   
   P_InitTIDHash();     // haleyjd 02/02/04 -- clear the TID hash table

   P_InitPortals(); // SoM: I can't believe I forgot to call this!

   P_LoadLevelInfo(lumpnum);  // load MapInfo
   E_LoadExtraData();         // haleyjd 10/08/03: load ExtraData

   // haleyjd: set global colormap -- see r_data.c
   R_SetGlobalLevelColormap();

#if 0
   // when loading a hub level, display a 'loading' box
   if(hub_changelevel)
      V_SetLoading(4, "Loading");
#endif

   P_NewLevelMsg();
   
   // wake up heads-up display
   HU_Start();
   
   // must be after p_loadlevelinfo as the music lump name is gotten there
   S_Start();
   
   // load the sky
   R_StartSky();

   // note: most of this ordering is important
   
   // killough 3/1/98: P_LoadBlockMap call moved down to below
   // killough 4/4/98: split load of sidedefs into two parts,
   // to allow texture names to be used in special linedefs

   level_error = false;  // reset
   
   P_LoadVertexes(lumpnum + ML_VERTEXES);
   P_LoadSectors (lumpnum + ML_SECTORS);
   P_LoadSideDefs(lumpnum + ML_SIDEDEFS); // killough 4/4/98

   // haleyjd 10/03/05: handle multiple map formats
   switch(mapformat)
   {
   case LEVEL_FORMAT_DOOM:
      P_LoadLineDefs(lumpnum + ML_LINEDEFS);
      break;
   case LEVEL_FORMAT_HEXEN:
      P_LoadHexenLineDefs(lumpnum + ML_LINEDEFS);
      break;
   }

#if 0
   V_LoadingIncrease();  // update
#endif

   P_LoadSideDefs2(lumpnum + ML_SIDEDEFS);
   P_LoadLineDefs2();                     // killough 4/4/98

   if(level_error)       // drop to the console
   {             
      C_SetConsole();
      return;
   }

   P_LoadBlockMap  (lumpnum + ML_BLOCKMAP);     // killough 3/1/98
   P_LoadSubsectors(lumpnum + ML_SSECTORS);
   P_LoadNodes     (lumpnum + ML_NODES);
   P_LoadSegs      (lumpnum + ML_SEGS);

#if 0
   V_LoadingIncrease();
#endif

   // haleyjd 01/26/04: call new P_LoadReject
   P_LoadReject(lumpnum + ML_REJECT);
   P_GroupLines();

   P_RemoveSlimeTrails(); // killough 10/98: remove slime trails from wad

   // Note: you don't need to clear player queue slots --
   // a much simpler fix is in g_game.c -- killough 10/98   
   bodyqueslot = 0;
   deathmatch_p = deathmatchstarts;

   // haleyjd 10/03/05: handle multiple map formats
   switch(mapformat)
   {
   case LEVEL_FORMAT_DOOM:
      P_LoadThings(lumpnum + ML_THINGS);
      break;
   case LEVEL_FORMAT_HEXEN:
      P_LoadHexenThings(lumpnum + ML_THINGS);
      break;
   }

   // if deathmatch, randomly spawn the active players
   if(GameType == gt_dm)
   {
      for(i = 0; i < MAXPLAYERS; ++i)
      {
         if(playeringame[i])
         {
            players[i].mo = NULL;
            G_DeathMatchSpawnPlayer(i);
         }
      }
   }

   // haleyjd: init all thing lists (boss brain spots, etc)
   P_InitThingLists(); 

   // clear special respawning que
   iquehead = iquetail = 0;
   
   // haleyjd 10/05/05: convert heretic specials
   if(mapformat == LEVEL_FORMAT_DOOM && LevelInfo.levelType == LI_TYPE_HERETIC)
      P_ConvertHereticSpecials();
   
   // set up world state
   P_SpawnSpecials(mapformat);

   // SoM: Deferred specials that need to be spawned after P_SpawnSpecials
   P_SpawnDeferredSpecials(mapformat);

   // haleyjd
   P_InitLightning();

#if 0
   V_LoadingIncrease();
#endif

   // preload graphics
   if(precache)
      R_PrecacheLevel();

   R_SetViewSize(screenSize+3, c_detailshift); //sf

#if 0
   V_LoadingIncrease();
#endif

   // haleyjd: keep the chasecam on between levels
   if(camera == &chasecam)
      P_ResetChasecam();
   else
      camera = NULL;        // camera off

#ifndef EE_NO_SMALL_SUPPORT
   // haleyjd 03/15/03: load and initialize any level scripts
   SM_InitLevelScript();
#endif

   // haleyjd 01/07/07: initialize ACS for Hexen maps
   if(mapformat == LEVEL_FORMAT_HEXEN)
      ACS_LoadLevelScript(lumpnum + ML_BEHAVIOR);
}

//
// P_InitThingLists
//
// haleyjd 11/19/02: Sets up all dynamically allocated thing lists.
//
void P_InitThingLists(void)
{
   // haleyjd: allow to work in any game mode
   // killough 3/26/98: Spawn icon landings:
   if(GameModeInfo->id == commercial || demo_version >= 331)
      P_SpawnBrainTargets();

   // haleyjd: spawn D'Sparil teleport spots
   P_SpawnSorcSpots();

   // haleyjd 04/08/03: spawn camera spots
   IN_AddCameras();

   // haleyjd 06/06/06: initialize environmental sequences
   S_InitEnviroSpots();
}

//
// P_Init
//
void P_Init(void)
{
   P_InitParticleEffects();  // haleyjd 09/30/01
   P_InitSwitchList();
   P_InitPicAnims();
   R_InitSprites(spritelist);
   P_InitHubs();
   E_InitTerrainTypes();     // haleyjd 07/03/99
}

//
// OLO Support. - sf
//
// OLOs were something I came up with a while ago when making my 'onslaunch'
// launcher. They are lumps which hold information about the lump: which
// deathmatch type they are best played on etc. which was read by onslaunch
// which adjusted its launch settings appropriately. More importantly,
// they hold the level names which I use here..
//

#if 0
olo_t olo;
int olo_loaded = false;

void P_LoadOlo(void)
{
   int lumpnum;
   char *lump;
   
   if((lumpnum = W_CheckNumForName("OLO")) == -1)
      return;
   
   lump = W_CacheLumpNum(lumpnum, PU_CACHE);
   
   if(strncmp(lump, "OLO", 3))
      return;
   
   memcpy(&olo, lump, sizeof(olo_t));
   
   olo_loaded = true;
}
#endif

//
// P_ConvertHereticThing
//
// haleyjd: Converts Heretic doomednums into an Eternity-compatible range.
//
// 05/30/06: made much more specific to avoid translating things that don't
// need to be translated.
//
static void P_ConvertHereticThing(mapthing_t *mthing)
{
   // null, player starts, teleport destination are common
   if(mthing->type <= 4 || mthing->type == 11 || mthing->type == 14)
      return;
   
   // handle ordinary heretic things -- all are less than 100
   if(mthing->type < 100)
   {
      // add 7000 to normal doomednum
      mthing->type += 7000;
   }
   else if(mthing->type > 2000 && mthing->type <= 2035)
   {
      // handle items numbered > 2000
      mthing->type = (mthing->type - 2000) + 7200;
   }
}

//----------------------------------------------------------------------------
//
// $Log: p_setup.c,v $
// Revision 1.16  1998/05/07  00:56:49  killough
// Ignore translucency lumps that are not exactly 64K long
//
// Revision 1.15  1998/05/03  23:04:01  killough
// beautification
//
// Revision 1.14  1998/04/12  02:06:46  killough
// Improve 242 colomap handling, add translucent walls
//
// Revision 1.13  1998/04/06  04:47:05  killough
// Add support for overloading sidedefs for special uses
//
// Revision 1.12  1998/03/31  10:40:42  killough
// Remove blockmap limit
//
// Revision 1.11  1998/03/28  18:02:51  killough
// Fix boss spawner savegame crash bug
//
// Revision 1.10  1998/03/20  00:30:17  phares
// Changed friction to linedef control
//
// Revision 1.9  1998/03/16  12:35:36  killough
// Default floor light level is sector's
//
// Revision 1.8  1998/03/09  07:21:48  killough
// Remove use of FP for point/line queries and add new sector fields
//
// Revision 1.7  1998/03/02  11:46:10  killough
// Double blockmap limit, prepare for when it's unlimited
//
// Revision 1.6  1998/02/27  11:51:05  jim
// Fixes for stairs
//
// Revision 1.5  1998/02/17  22:58:35  jim
// Fixed bug of vanishinb secret sectors in automap
//
// Revision 1.4  1998/02/02  13:38:48  killough
// Comment out obsolete reload hack
//
// Revision 1.3  1998/01/26  19:24:22  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:02:21  killough
// Generalize and simplify level name generation
//
// Revision 1.1.1.1  1998/01/19  14:03:00  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------


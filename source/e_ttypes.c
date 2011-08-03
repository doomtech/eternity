// Emacs style mode select   -*- C -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
//   New dynamic TerrainTypes system. Inspired heavily by zdoom, but
//   all-original code.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_dehtbl.h"
#include "d_mod.h"
#include "doomstat.h"
#include "i_system.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_mobj.h"
#include "p_tick.h"
#include "r_data.h"
#include "r_main.h"
#include "s_sound.h"
#include "w_wad.h"

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"
#include "e_lib.h"
#include "e_edf.h"
#include "e_ttypes.h"
#include "e_things.h"
#include "e_mod.h"

//
// Static Variables
//

// Splashes
#define NUMSPLASHCHAINS 37
static ETerrainSplash *SplashChains[NUMSPLASHCHAINS];

// Terrain
#define NUMTERRAINCHAINS 37
static ETerrain *TerrainChains[NUMTERRAINCHAINS];

// Floors
#define NUMFLOORCHAINS 37
static EFloor *FloorChains[NUMFLOORCHAINS];

static int numsplashes;
static int numterrains;
static int numfloors;

//
// libConfuse Stuff
//

// Splash Keywords
#define ITEM_SPLASH_SMALLCLASS "smallclass"
#define ITEM_SPLASH_SMALLCLIP  "smallclip"
#define ITEM_SPLASH_SMALLSOUND "smallsound"
#define ITEM_SPLASH_BASECLASS  "baseclass"
#define ITEM_SPLASH_CHUNKCLASS "chunkclass"
#define ITEM_SPLASH_XVELSHIFT  "chunkxvelshift"
#define ITEM_SPLASH_YVELSHIFT  "chunkyvelshift"
#define ITEM_SPLASH_ZVELSHIFT  "chunkzvelshift"
#define ITEM_SPLASH_BASEZVEL   "chunkbasezvel"
#define ITEM_SPLASH_SOUND      "sound"

// Splash Options
cfg_opt_t edf_splash_opts[] =
{
   CFG_STR(ITEM_SPLASH_SMALLCLASS, "",        CFGF_NONE),
   CFG_INT(ITEM_SPLASH_SMALLCLIP,  0,         CFGF_NONE),
   CFG_STR(ITEM_SPLASH_SMALLSOUND, "none",    CFGF_NONE),
   CFG_STR(ITEM_SPLASH_BASECLASS,  "",        CFGF_NONE),
   CFG_STR(ITEM_SPLASH_CHUNKCLASS, "",        CFGF_NONE),
   CFG_INT(ITEM_SPLASH_XVELSHIFT,  -1,        CFGF_NONE),
   CFG_INT(ITEM_SPLASH_YVELSHIFT,  -1,        CFGF_NONE),
   CFG_INT(ITEM_SPLASH_ZVELSHIFT,  -1,        CFGF_NONE),
   CFG_INT(ITEM_SPLASH_BASEZVEL,   0,         CFGF_NONE),
   CFG_STR(ITEM_SPLASH_SOUND,      "none",    CFGF_NONE),
   CFG_END()
};

// Terrain Keywords
#define ITEM_TERRAIN_SPLASH   "splash"
#define ITEM_TERRAIN_DMGAMT   "damageamount"
#define ITEM_TERRAIN_DMGTYPE  "damagetype"
#define ITEM_TERRAIN_DMGMASK  "damagetimemask"
#define ITEM_TERRAIN_FOOTCLIP "footclip"
#define ITEM_TERRAIN_LIQUID   "liquid"
#define ITEM_TERRAIN_SPALERT  "splashalert"
#define ITEM_TERRAIN_USECOLS  "useptclcolors"
#define ITEM_TERRAIN_COL1     "ptclcolor1"
#define ITEM_TERRAIN_COL2     "ptclcolor2"
#define ITEM_TERRAIN_MINVER   "minversion"

#define ITEM_TERDELTA_NAME "name"

// Terrain Options
cfg_opt_t edf_terrn_opts[] =
{
   CFG_STR(ITEM_TERRAIN_SPLASH,   "",        CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_DMGAMT,   0,         CFGF_NONE),
   CFG_STR(ITEM_TERRAIN_DMGTYPE,  "UNKNOWN", CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_DMGMASK,  0,         CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_FOOTCLIP, 0,         CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_LIQUID,  cfg_false, CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_SPALERT, cfg_false, CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_USECOLS, cfg_false, CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_MINVER,   0,         CFGF_NONE),
   
   CFG_INT_CB(ITEM_TERRAIN_COL1,  0,         CFGF_NONE, E_ColorStrCB),
   CFG_INT_CB(ITEM_TERRAIN_COL2,  0,         CFGF_NONE, E_ColorStrCB),
   
   CFG_END()
};

cfg_opt_t edf_terdelta_opts[] =
{
   CFG_STR(ITEM_TERDELTA_NAME,    NULL,      CFGF_NONE),

   CFG_STR(ITEM_TERRAIN_SPLASH,   "",        CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_DMGAMT,   0,         CFGF_NONE),
   CFG_STR(ITEM_TERRAIN_DMGTYPE,  "UNKNOWN", CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_DMGMASK,  0,         CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_FOOTCLIP, 0,         CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_LIQUID,  cfg_false, CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_SPALERT, cfg_false, CFGF_NONE),
   CFG_BOOL(ITEM_TERRAIN_USECOLS, cfg_false, CFGF_NONE),
   CFG_INT(ITEM_TERRAIN_MINVER,   0,         CFGF_NONE),
   
   CFG_INT_CB(ITEM_TERRAIN_COL1,  0,         CFGF_NONE, E_ColorStrCB),
   CFG_INT_CB(ITEM_TERRAIN_COL2,  0,         CFGF_NONE, E_ColorStrCB),
   
   CFG_END()
};

// Floor Keywords
#define ITEM_FLOOR_FLAT    "flat"
#define ITEM_FLOOR_TERRAIN "terrain"

// Floor Options
cfg_opt_t edf_floor_opts[] =
{
   CFG_STR(ITEM_FLOOR_FLAT,    "none",   CFGF_NONE),
   CFG_STR(ITEM_FLOOR_TERRAIN, "Solid",  CFGF_NONE),
   CFG_END()
};

//
// E_SplashForName
//
// Returns a terrain splash object for a name.
// Returns NULL if no such splash exists.
//
static ETerrainSplash *E_SplashForName(const char *name)
{
   int key = D_HashTableKey(name) % NUMSPLASHCHAINS;
   ETerrainSplash *splash = SplashChains[key];

   while(splash && strcasecmp(splash->name, name))
      splash = splash->next;

   return splash;
}

//
// E_AddSplashToHash
//
// Adds a terrain splash object to the hash table.
//
static void E_AddSplashToHash(ETerrainSplash *splash)
{
   int key = D_HashTableKey(splash->name) % NUMSPLASHCHAINS;

   splash->next = SplashChains[key];
   SplashChains[key] = splash;

   // keep track of how many there are
   ++numsplashes;
}

//
// E_ProcessSplash
//
// Processes a single splash object from the section data held
// in the cfg parameter. If a splash object already exists by
// the mnemonic, that splash will be edited instead of having a
// new one created. This will allow terrain lumps to have additive
// behavior over EDF.
//
static void E_ProcessSplash(cfg_t *cfg)
{
   const char *tempstr;
   ETerrainSplash *newSplash;
   boolean newsp = false;

   // init name and add to hash table
   tempstr = cfg_title(cfg);
   if(strlen(tempstr) > 32)
   {
      E_EDFLoggedErr(3, "E_ProcessSplash: invalid splash mnemonic '%s'\n",
                     tempstr);
   }

   // If one already exists, modify it. Otherwise, allocate a new
   // splash and add it to the splash hash table.
   if(!(newSplash = E_SplashForName(tempstr)))
   {
      newSplash = calloc(1, sizeof(ETerrainSplash));
      strncpy(newSplash->name, tempstr, 33);
      E_AddSplashToHash(newSplash);
      newsp = true;
   }

   // process smallclass
   tempstr = cfg_getstr(cfg, ITEM_SPLASH_SMALLCLASS);
   newSplash->smallclass = E_ThingNumForName(tempstr);

   // process smallclip
   newSplash->smallclip = cfg_getint(cfg, ITEM_SPLASH_SMALLCLIP) * FRACUNIT;

   // process smallsound
   tempstr = cfg_getstr(cfg, ITEM_SPLASH_SMALLSOUND);
   if(strlen(tempstr) > 32)
   {
      E_EDFLoggedErr(3, "E_ProcessSplash: invalid sound mnemonic '%s'\n",
                     tempstr);
   }
   strncpy(newSplash->smallsound, tempstr, 33);

   // process baseclass
   tempstr = cfg_getstr(cfg, ITEM_SPLASH_BASECLASS);
   newSplash->baseclass = E_ThingNumForName(tempstr);
   
   // process chunkclass
   tempstr = cfg_getstr(cfg, ITEM_SPLASH_CHUNKCLASS);
   newSplash->chunkclass = E_ThingNumForName(tempstr);
   
   // process chunkxvelshift, yvelshift, zvelshift
   newSplash->chunkxvelshift = cfg_getint(cfg, ITEM_SPLASH_XVELSHIFT);
   newSplash->chunkyvelshift = cfg_getint(cfg, ITEM_SPLASH_YVELSHIFT);
   newSplash->chunkzvelshift = cfg_getint(cfg, ITEM_SPLASH_ZVELSHIFT);
  
   // process chunkbasezvel
   newSplash->chunkbasezvel = 
      cfg_getint(cfg, ITEM_SPLASH_BASEZVEL) * FRACUNIT;
   
   // process sound
   tempstr = cfg_getstr(cfg, ITEM_SPLASH_SOUND);
   if(strlen(tempstr) > 32)
   {
      E_EDFLoggedErr(3, "E_ProcessSplash: invalid sound mnemonic '%s'\n",
                     tempstr);
   }
   strncpy(newSplash->sound, tempstr, 33);

   E_EDFLogPrintf("\t\t\t%s splash '%s'\n", 
                  newsp ? "Finished" : "Modified", 
                  newSplash->name);
}

static void E_ProcessSplashes(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numSplashes = cfg_size(cfg, EDF_SEC_SPLASH);

   E_EDFLogPrintf("\t\t* Processing splashes\n"
                  "\t\t\t%d splash(es) defined\n", numSplashes);

   for(i = 0; i < numSplashes; ++i)
   {
      cfg_t *splashsec = cfg_getnsec(cfg, EDF_SEC_SPLASH, i);
      E_ProcessSplash(splashsec);
   }
}

//
// E_TerrainForName
//
// Returns a terrain object for a name.
// Returns NULL if no such terrain exists.
//
static ETerrain *E_TerrainForName(const char *name)
{
   int key = D_HashTableKey(name) % NUMTERRAINCHAINS;
   ETerrain *terrain = TerrainChains[key];

   while(terrain && strcasecmp(terrain->name, name))
      terrain = terrain->next;

   return terrain;
}

//
// E_AddTerrainToHash
//
// Adds a terrain object to the hash table.
//
static void E_AddTerrainToHash(ETerrain *terrain)
{
   int key = D_HashTableKey(terrain->name) % NUMTERRAINCHAINS;

   terrain->next = TerrainChains[key];
   TerrainChains[key] = terrain;

   // keep track of how many there are
   ++numterrains;
}

#define IS_SET(name) (def || cfg_size(cfg, (name)) > 0)

static void E_ProcessTerrain(cfg_t *cfg, boolean def)
{
   const char *tempstr;
   ETerrain *newTerrain;
   boolean newtr = false;

   // init name and add to hash table
   if(def)
   {
      // definition:
      tempstr = cfg_title(cfg);
      if(strlen(tempstr) > 32)
      {
         E_EDFLoggedErr(3, "E_ProcessTerrain: invalid terrain mnemonic '%s'\n",
            tempstr);
      }

      // If one already exists, modify it. Otherwise, allocate a new
      // terrain and add it to the terrain hash table.
      if(!(newTerrain = E_TerrainForName(tempstr)))
      {
         newTerrain = calloc(1, sizeof(ETerrain));
         strncpy(newTerrain->name, tempstr, 33);
         E_AddTerrainToHash(newTerrain);
         newtr = true;
      }
   }
   else
   {
      // delta:
      tempstr = cfg_getstr(cfg, ITEM_TERDELTA_NAME);
      if(!tempstr)
      {
         E_EDFLoggedErr(3, 
            "E_ProcessTerrain: terrain delta requires name field!\n");
      }

      if(!(newTerrain = E_TerrainForName(tempstr)))
      {
         E_EDFLoggedWarning(3, "Warning: terrain '%s' doesn't exist\n",
                            tempstr);
         return;
      }
   }

   // process splash -- may be null
   if(IS_SET(ITEM_TERRAIN_SPLASH))
   {
      tempstr = cfg_getstr(cfg, ITEM_TERRAIN_SPLASH);
      newTerrain->splash = E_SplashForName(tempstr);
   }

   // process damageamount
   if(IS_SET(ITEM_TERRAIN_DMGAMT))
      newTerrain->damageamount = cfg_getint(cfg, ITEM_TERRAIN_DMGAMT);

   // process damagetype
   if(IS_SET(ITEM_TERRAIN_DMGTYPE))
   {
      tempstr = cfg_getstr(cfg, ITEM_TERRAIN_DMGTYPE);
      newTerrain->damagetype = E_DamageTypeForName(tempstr)->num;
   }

   // process damagetimemask
   if(IS_SET(ITEM_TERRAIN_DMGMASK))
      newTerrain->damagetimemask = cfg_getint(cfg, ITEM_TERRAIN_DMGMASK);

   // process footclip
   if(IS_SET(ITEM_TERRAIN_FOOTCLIP))
   {
      newTerrain->footclip = 
         cfg_getint(cfg, ITEM_TERRAIN_FOOTCLIP) * FRACUNIT;
   }

   // process liquid
   if(IS_SET(ITEM_TERRAIN_LIQUID))
   {
      newTerrain->liquid = 
         (cfg_getbool(cfg, ITEM_TERRAIN_LIQUID) == cfg_true);
   }

   // process splashalert
   if(IS_SET(ITEM_TERRAIN_SPALERT))
   {
      newTerrain->splashalert =
         (cfg_getbool(cfg, ITEM_TERRAIN_SPALERT) == cfg_true);
   }

   // process usepcolors
   if(IS_SET(ITEM_TERRAIN_USECOLS))
   {
      newTerrain->usepcolors =
         (cfg_getbool(cfg, ITEM_TERRAIN_USECOLS) == cfg_true);
   }

   // process particle colors
   if(IS_SET(ITEM_TERRAIN_COL1))
      newTerrain->pcolor_1 = (byte)cfg_getint(cfg, ITEM_TERRAIN_COL1);

   if(IS_SET(ITEM_TERRAIN_COL2))
      newTerrain->pcolor_2 = (byte)cfg_getint(cfg, ITEM_TERRAIN_COL2);

   if(IS_SET(ITEM_TERRAIN_MINVER))
      newTerrain->minversion = cfg_getint(cfg, ITEM_TERRAIN_MINVER);

   // games other than DOOM have always had terrain types
   if(GameModeInfo->type != Game_DOOM)
      newTerrain->minversion = 0;

   if(def)
   {
      E_EDFLogPrintf("\t\t\t%s terrain '%s'\n", 
                     newtr ? "Finished" : "Modified",
                     newTerrain->name);
   }
   else
      E_EDFLogPrintf("\t\t\tApplied terraindelta to terrain '%s'\n", newTerrain->name);
}

// The 'Solid' terrain object.
static ETerrain solid;

//
// E_AddSolidTerrain
//
// Adds the default 'Solid' TerrainType to the terrain hash. This
// is done before user TerrainTypes are added, so if the user
// creates a type named Solid, it will override this one by modifying
// its properties. This default type has no special properties.
//
static void E_AddSolidTerrain(void)
{
   static boolean solidinit;

   if(!solidinit)
   {
      E_EDFLogPuts("\t\t\tCreating Solid terrain...\n");
      strncpy(solid.name, "Solid", 33);
      E_AddTerrainToHash(&solid);
      solidinit = true;
      --numterrains; // do not count the solid terrain 
   }
}

static void E_ProcessTerrains(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numTerrains = cfg_size(cfg, EDF_SEC_TERRAIN);

   E_EDFLogPrintf("\t\t* Processing terrain\n"
                  "\t\t\t%d terrain(s) defined\n", numTerrains);

   E_AddSolidTerrain();

   for(i = 0; i < numTerrains; ++i)
   {
      cfg_t *terrainsec = cfg_getnsec(cfg, EDF_SEC_TERRAIN, i);
      E_ProcessTerrain(terrainsec, true);
   }
}

static void E_ProcessTerrainDeltas(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numTerrains = cfg_size(cfg, EDF_SEC_TERDELTA);

   E_EDFLogPrintf("\t\t* Processing terrain deltas\n"
                  "\t\t\t%d terrain delta(s) defined\n", numTerrains);

   for(i = 0; i < numTerrains; ++i)
   {
      cfg_t *terrainsec = cfg_getnsec(cfg, EDF_SEC_TERDELTA, i);
      E_ProcessTerrain(terrainsec, false);
   }
}

//
// E_FloorForName
//
// Returns a floor object for a flat name.
// Returns NULL if no such floor exists.
//
static EFloor *E_FloorForName(const char *name)
{
   int key = D_HashTableKey(name) % NUMFLOORCHAINS;
   EFloor *floor = FloorChains[key];

   while(floor && strcasecmp(floor->name, name))
      floor = floor->next;

   return floor;
}

//
// E_AddFloorToHash
//
// Adds a floor object to the hash table.
//
static void E_AddFloorToHash(EFloor *floor)
{
   int key = D_HashTableKey(floor->name) % NUMFLOORCHAINS;

   floor->next = FloorChains[key];
   FloorChains[key] = floor;

   // keep track of how many there are
   ++numfloors;
}

//
// E_ProcessFloor
//
// Creates or modifies a floor object.
//
static void E_ProcessFloor(cfg_t *cfg)
{
   const char *tempstr;
   EFloor *newFloor;

   // init flat name and add to hash table
   tempstr = cfg_getstr(cfg, ITEM_FLOOR_FLAT);
   if(strlen(tempstr) > 8)
   {
      E_EDFLoggedErr(3, "E_ProcessFloor: invalid flat name '%s'\n",
                     tempstr);
   }

   // If one already exists, modify it. Otherwise, allocate a new
   // terrain and add it to the terrain hash table.
   if(!(newFloor = E_FloorForName(tempstr)))
   {
      newFloor = calloc(1, sizeof(EFloor));
      strncpy(newFloor->name, tempstr, 9);
      E_AddFloorToHash(newFloor);
   }

   // process terrain
   tempstr = cfg_getstr(cfg, ITEM_FLOOR_TERRAIN);
   if(!(newFloor->terrain = E_TerrainForName(tempstr)))
   {
      E_EDFLoggedWarning(3, "Warning: Flat '%s' uses bad terrain '%s'\n",
                         newFloor->name, tempstr);
      newFloor->terrain = &solid;
   }

   E_EDFLogPrintf("\t\t\tFlat '%s' = Terrain '%s'\n",
                  newFloor->name, newFloor->terrain->name);
}

//
// E_ProcessFloors
//
// Processes all floor sections in the provided cfg object. Also
// parses the deprecated binary TERTYPES lump to add definitions for
// legacy projects.
//
static void E_ProcessFloors(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numFloors = cfg_size(cfg, EDF_SEC_FLOOR);

   E_EDFLogPrintf("\t\t* Processing floors\n"
                  "\t\t\t%d floor(s) defined\n", numFloors);

   for(i = 0; i < numFloors; ++i)
   {
      cfg_t *floorsec = cfg_getnsec(cfg, EDF_SEC_FLOOR, i);
      E_ProcessFloor(floorsec);
   }
}

//
// E_ProcessTerrainTypes
//
// Performs all TerrainTypes processing for EDF.
//
void E_ProcessTerrainTypes(cfg_t *cfg)
{
   E_EDFLogPuts("\t* Processing TerrainTypes\n");

   // First, process splashes
   E_ProcessSplashes(cfg);

   // Second, process terrains
   E_ProcessTerrains(cfg);

   // Third, process terrain deltas
   E_ProcessTerrainDeltas(cfg);

   // Last, process floors
   E_ProcessFloors(cfg);
}

// TerrainTypes lookup array
static ETerrain **TerrainTypes = NULL;

//
// E_InitTerrainTypes
//
void E_InitTerrainTypes(void)
{
   int numf, size, i;

   // if TerrainTypes already exists, free it
   if(TerrainTypes)
      free(TerrainTypes);

   // allocate the TerrainTypes lookup
   numf = (texturecount + 1);
   size = numf * sizeof(ETerrain *);
   TerrainTypes = (ETerrain **)malloc(size);

   // initialize all flats to Solid terrain
   for(i = 0; i < numf; ++i)
      TerrainTypes[i] = &solid;

   // run down each chain of the Floor hash table and assign each
   // Floor object to the proper TerrainType
   for(i = 0; i < NUMFLOORCHAINS; ++i)
   {
      EFloor *floor = FloorChains[i];

      while(floor)
      {
         int tnum = R_CheckForFlat(floor->name);

         if(tnum != -1)
            TerrainTypes[tnum] = floor->terrain;

         floor = floor->next;
      }
   }
}

//
// E_GetThingFloorType
//
// Note: this returns the floor type of the thing's subsector
// floorpic, not necessarily the floor the thing is standing on.
//
ETerrain *E_GetThingFloorType(mobj_t *thing)
{
   ETerrain *terrain = TerrainTypes[thing->subsector->sector->floorpic];
   
   // [CG] FIXME: Terrain types are disabled in c/s mode due to desyncs.
   if(demo_version < terrain->minversion || comp[comp_terrain] || clientserver)
      terrain = &solid;

   return terrain;
}

//
// E_GetTerrainTypeForPt
//
// haleyjd 06/21/02: function to get TerrainType from a point
//
ETerrain *E_GetTerrainTypeForPt(fixed_t x, fixed_t y, int position)
{
   subsector_t *subsec = R_PointInSubsector(x, y);

   // can retrieve a TerrainType for either the floor or the
   // ceiling
   switch(position)
   {
   case 0:
      return TerrainTypes[subsec->sector->floorpic];
   case 1:
      return TerrainTypes[subsec->sector->ceilingpic];
   default:
      return &solid;
   }
}

//
// E_SectorFloorClip
//
// Returns the amount of floorclip a sector should impart upon
// objects standing inside it.
//
fixed_t E_SectorFloorClip(sector_t *sector)
{
   ETerrain *terrain = TerrainTypes[sector->floorpic];

   return (demo_version >= terrain->minversion) ? terrain->footclip : 0;
}

//
// E_PtclTerrainHit
//
// Executes particle terrain hits.
//
void E_PtclTerrainHit(particle_t *p)
{
   ETerrain *terrain;
   ETerrainSplash *splash;
   mobj_t *mo = NULL;
   fixed_t x, y, z;

   // particles could never hit terrain before v3.33
   // [CG] FIXME: Terrain types are disabled in c/s mode due to desyncs.
   if(demo_version < 333 || comp[comp_terrain] || clientserver)
      return;

   // no particle hits during netgames or demos;
   // this is necessary because particles are not only client
   // specific, but they also use M_Random
   if(netgame || demoplayback || demorecording)
      return;

   terrain = TerrainTypes[p->subsector->sector->floorpic];

   // some terrains didn't exist before a certain version
   if(demo_version < terrain->minversion)
      return;

   if(!(splash = terrain->splash))
      return;

   x = p->x;
   y = p->y;
   z = p->z;

   // low mass splash -- always when possible.
   if(splash->smallclass != NUMMOBJTYPES)
   {
      mo = P_SpawnMobj(x, y, z, splash->smallclass);
      mo->floorclip += splash->smallclip;      
   }
   else if(splash->baseclass != NUMMOBJTYPES)
   {
      // spawn only a splash base otherwise
      mo = P_SpawnMobj(x, y, z, splash->baseclass);
   }
   
   if(mo)
      S_StartSoundName(mo, splash->smallsound);
}

//
// E_TerrainHit
//
// Executes mobj terrain effects.
//
static void E_TerrainHit(ETerrain *terrain, mobj_t *thing, fixed_t z)
{
   ETerrainSplash *splash = terrain->splash;
   mobj_t *mo = NULL;
   boolean lowmass = (thing->info->mass < 10);   

   if(!splash)
      return;

   // low mass splash?
   // note: small splash didn't exist before version 3.33
   if(demo_version >= 333 && 
      lowmass && splash->smallclass != NUMMOBJTYPES)
   {
      mo = P_SpawnMobj(thing->x, thing->y, z, splash->smallclass);
      mo->floorclip += splash->smallclip;
   }
   else
   {
      if(splash->baseclass != NUMMOBJTYPES)
         mo = P_SpawnMobj(thing->x, thing->y, z, splash->baseclass);

      if(splash->chunkclass != NUMMOBJTYPES)
      {
         mo = P_SpawnMobj(thing->x, thing->y, z, splash->chunkclass);
         P_SetTarget(&mo->target, thing);
         
         if(splash->chunkxvelshift != -1)
            mo->momx = P_SubRandom(pr_splash) << splash->chunkxvelshift;
         if(splash->chunkyvelshift != -1)
            mo->momy = P_SubRandom(pr_splash) << splash->chunkyvelshift;
         mo->momz = splash->chunkbasezvel;
         if(splash->chunkzvelshift != -1)
            mo->momz += P_SubRandom(pr_splash) << splash->chunkzvelshift;
      }

      // some terrains may awaken enemies when hit
      if(!lowmass && terrain->splashalert && thing->player)
         P_NoiseAlert(thing, thing);
   }

   // make a sound
   // use the splash object as the origin if possible, 
   // else the thing that hit the terrain
   S_StartSoundName(mo ? mo : thing, 
                    lowmass ? splash->smallsound : splash->sound);
}

//
// E_HitWater
//
// Called when a thing hits a floor or passes a deep water plane.
//
boolean E_HitWater(mobj_t *thing, sector_t *sector)
{
   fixed_t z;
   ETerrain *terrain;

   terrain = TerrainTypes[sector->floorpic];

   // no TerrainTypes in old demos or if comp enabled
   // [CG] FIXME: Terrain types are disabled in c/s mode due to desyncs.
   if(demo_version < terrain->minversion || comp[comp_terrain] || clientserver)
      terrain = &solid;

   // some things don't cause splashes
   if(thing->flags2 & MF2_NOSPLASH || thing->flags2 & MF2_FLOATBOB)
      terrain = &solid;

   z = sector->heightsec != -1 ? 
         sectors[sector->heightsec].floorheight :
         sector->floorheight;

   E_TerrainHit(terrain, thing, z);

   return terrain->liquid;
}

//
// E_HitFloor
//
// Called when a thing hits a floor.
//
boolean E_HitFloor(mobj_t *thing)
{
   msecnode_t  *m;

   // determine what touched sector the thing is standing on
   for(m = thing->touching_sectorlist; m; m = m->m_tnext)
   {
      if(thing->z == m->m_sector->floorheight)
         break;
   }

   // not on a floor or dealing with deep water, return solid
   // deep water splashes are handled in P_MobjThinker now
   if(!m || m->m_sector->heightsec != -1)         
      return false;

   return E_HitWater(thing, m->m_sector);
}

// EOF


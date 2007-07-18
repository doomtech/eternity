// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// EDF Thing Types Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"
#include "acs_intr.h"
#include "i_system.h"
#include "w_wad.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_dehtbl.h"
#include "d_mod.h"
#include "g_game.h"
#include "info.h"
#include "m_cheat.h"
#include "p_inter.h"
#include "p_partcl.h"
#include "r_draw.h"

#include "Confuse/confuse.h"

#include "e_lib.h"
#include "e_edf.h"
#include "e_states.h"
#include "e_things.h"
#include "e_sound.h"

// 7/24/05: This is now global, for efficiency's sake

// The "Unknown" thing type, which is required, has its type
// number resolved in E_CollectThings
int UnknownThingType;

// Thing Keywords
#define ITEM_TNG_INHERITS "inherits"
#define ITEM_TNG_DOOMEDNUM "doomednum"
#define ITEM_TNG_SPAWNSTATE "spawnstate"
#define ITEM_TNG_SPAWNHEALTH "spawnhealth"
#define ITEM_TNG_SEESTATE "seestate"
#define ITEM_TNG_SEESOUND "seesound"
#define ITEM_TNG_REACTTIME "reactiontime"
#define ITEM_TNG_ATKSOUND "attacksound"
#define ITEM_TNG_PAINSTATE "painstate"
#define ITEM_TNG_PAINCHANCE "painchance"
#define ITEM_TNG_PAINSOUND "painsound"
#define ITEM_TNG_MELEESTATE "meleestate"
#define ITEM_TNG_MISSILESTATE "missilestate"
#define ITEM_TNG_DEATHSTATE "deathstate"
#define ITEM_TNG_XDEATHSTATE "xdeathstate"
#define ITEM_TNG_DEATHSOUND "deathsound"
#define ITEM_TNG_SPEED "speed"
#define ITEM_TNG_RADIUS "radius"
#define ITEM_TNG_HEIGHT "height"
#define ITEM_TNG_MASS "mass"
#define ITEM_TNG_DAMAGE "damage"
#define ITEM_TNG_ACTIVESOUND "activesound"
#define ITEM_TNG_FLAGS "flags"
#define ITEM_TNG_FLAGS2 "flags2"
#define ITEM_TNG_RAISESTATE "raisestate"
#define ITEM_TNG_TRANSLUC "translucency"
#define ITEM_TNG_FLAGS3 "flags3"
#define ITEM_TNG_BLOODCOLOR "bloodcolor"
#define ITEM_TNG_FASTSPEED "fastspeed"
#define ITEM_TNG_NUKESPEC  "nukespecial"
#define ITEM_TNG_PARTICLEFX "particlefx"
#define ITEM_TNG_DROPTYPE "droptype"
#define ITEM_TNG_MOD      "mod"
#define ITEM_TNG_OBIT1  "obituary_normal"
#define ITEM_TNG_OBIT2  "obituary_melee"
#define ITEM_TNG_COLOR  "translation"
#define ITEM_TNG_CFLAGS "cflags"
#define ITEM_TNG_ADDFLAGS "addflags"
#define ITEM_TNG_REMFLAGS "remflags"
#define ITEM_TNG_DMGSPECIAL "dmgspecial"
#define ITEM_TNG_CRASHSTATE "crashstate"
#define ITEM_TNG_SKINSPRITE "skinsprite"
#define ITEM_TNG_C3DHEIGHT "correct_height"
#define ITEM_TNG_BASICTYPE "basictype"
#define ITEM_TNG_TOPDAMAGE "topdamage"
#define ITEM_TNG_TOPDMGMASK "topdamagemask"
#define ITEM_TNG_ACS_SPAWN "acs_spawndata"
#define ITEM_TNG_ACS_NUM "num"
#define ITEM_TNG_ACS_MODES "modes"
#define ITEM_TNG_DEHNUM "dehackednum"

// Thing Delta Keywords
#define ITEM_DELTA_NAME "name"

//
// Field-Specific Data
//

// Particle effects flags

static dehflags_t particlefx[] =
{
   { "ROCKET",         FX_ROCKET },
   { "GRENADE",        FX_GRENADE },
   { "FLIES",          FX_FLIES },
   { "BFG",            FX_BFG },
   { "FLIESONDEATH",   FX_FLIESONDEATH },
   { "DRIP",           FX_DRIP },
   { "REDFOUNTAIN",    FX_REDFOUNTAIN },
   { "GREENFOUNTAIN",  FX_GREENFOUNTAIN },
   { "BLUEFOUNTAIN",   FX_BLUEFOUNTAIN },
   { "YELLOWFOUNTAIN", FX_YELLOWFOUNTAIN },
   { "PURPLEFOUNTAIN", FX_PURPLEFOUNTAIN },
   { "BLACKFOUNTAIN",  FX_BLACKFOUNTAIN },
   { "WHITEFOUNTAIN",  FX_WHITEFOUNTAIN },
   { NULL,             0 }
};

static dehflagset_t particle_flags =
{
   particlefx,  // flaglist
   0,           // mode
};

// ACS game mode flags

static dehflags_t acs_gamemodes[] =
{
   { "doom",    ACS_MODE_DOOM },
   { "heretic", ACS_MODE_HTIC },
   { "all",     ACS_MODE_ALL  },
   { NULL,      0             }
};

static dehflagset_t acs_gamemode_flags =
{
   acs_gamemodes, // flaglist
   0,             // mode
};

// table to translate from gameModeInfo->type to acs gamemode flag
static int gameTypeToACSFlags[NumGameModeTypes] =
{
   ACS_MODE_DOOM, // Game_DOOM,
   ACS_MODE_HTIC, // Game_Heretic
};

// Special damage inflictor types
// currently searched linearly
// matching enum values in p_inter.h

static const char *inflictorTypes[INFLICTOR_NUMTYPES] =
{
   "none",
   "MinotaurCharge",
   "Whirlwind",
};

// haleyjd 07/05/06: Basic types for things. These determine a number of
// alternate "defaults" for the thingtype that will make it behave in a
// typical manner for things of its basic type (ie: monster, projectile).
// The nice thing about basic types is that they can change, and things
// dependent upon them will be automatically updated for new versions of
// the engine.

static const char *BasicTypeNames[] =
{
   "Monster",           // normal walking monster with no fancy features
   "FlyingMonster",     // normal flying monster
   "FriendlyHelper",    // a thing with max options for helping player
   "Projectile",        // normal projectile
   "PlayerProjectile",  // player projectile (activates param linespecs)
   "Seeker",            // homing projectile
   "SolidDecor",        // solid decoration
   "HangingDecor",      // hanging decoration
   "SolidHangingDecor", // solid hanging decoration
   "ShootableDecor",    // shootable decoration
   "Fog",               // fog, like telefog, item fog, or bullet puffs
   "Item",              // collectable item
   "ItemCount",         // collectable item that counts for item %
   "TerrainBase",       // base of a terrain effect
   "TerrainChunk",      // chunk of a terrain effect
   "ControlPoint",      // a control point
   "ControlPointGrav",  // a control point with gravity
};

#define NUMBASICTYPES (sizeof(BasicTypeNames) / sizeof(char *))

typedef struct basicttype_s
{
   unsigned long flags;    // goes to: mi->flags
   unsigned long flags2;   //        : mi->flags2
   unsigned long flags3;   //        : mi->flags3
   const char *spawnstate; //        : mi->spawnstate
} basicttype_t;

static basicttype_t BasicThingTypes[] =
{
   // Monster
   {
      MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL, // flags
      MF2_FOOTCLIP,                       // flags2
      MF3_SPACMONSTER|MF3_PASSMOBJ,       // flags3
   },
   // FlyingMonster
   {
      MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_NOGRAVITY|MF_FLOAT, // flags
      0,                                                        // flags2
      MF3_SPACMONSTER|MF3_PASSMOBJ,                             // flags3
   },
   // FriendlyHelper
   { 
      MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL|MF_FRIEND,                // flags
      MF2_JUMPDOWN|MF2_FOOTCLIP,                                   // flags2
      MF3_WINDTHRUST|MF3_SUPERFRIEND|MF3_SPACMONSTER|MF3_PASSMOBJ, // flags3
   },
   // Projectile
   {
      MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE, // flags
      MF2_NOCROSS,                                      // flags2
      0,                                                // flags3
   },
   // PlayerProjectile
   {
      MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE, // flags
      MF2_NOCROSS,                                      // flags2
      MF3_SPACMISSILE,                                  // flags3
   },
   // Seeker
   {
      MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE, // flags
      MF2_NOCROSS|MF2_SEEKERMISSILE,                    // flags2
      0,                                                // flags3
   },
   // SolidDecor
   {
      MF_SOLID, // flags
      0,        // flags2
      0,        // flags3
   },
   // HangingDecor
   {
      MF_SPAWNCEILING|MF_NOGRAVITY, // flags
      0,                            // flags2
      0,                            // flags3
   },
   // SolidHangingDecor
   {
      MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY, // flags
      0,                                     // flags2
      0,                                     // flags3
   },
   // ShootableDecor
   {
      MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD, // flags
      0,                                // flags2
      0,                                // flags3
   },
   // Fog
   {
      MF_NOBLOCKMAP|MF_NOGRAVITY|MF_TRANSLUCENT, // flags
      MF2_NOSPLASH,                              // flags2
      0,                                         // flags3
   },
   // Item
   {
      MF_SPECIAL, // flags
      0,          // flags2
      0,          // flags3
   },
   // ItemCount
   {
      MF_SPECIAL|MF_COUNTITEM, // flags
      0,                       // flags2
      0,                       // flags3
   },
   // TerrainBase
   {
      MF_NOBLOCKMAP|MF_NOGRAVITY, // flags
      MF2_NOSPLASH,               // flags2
      0,                          // flags3
   },
   // TerrainChunk
   {
      MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE, // flags
      MF2_LOGRAV|MF2_NOSPLASH|MF2_NOCROSS, // flags2
      MF3_CANNOTPUSH,                      // flags3
   },
   // ControlPoint
   {
      MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY, // flags
      0,                                      // flags2
      0,                                      // flags3
      "S_TNT1",                               // spawnstate
   },
   // ControlPointGrav
   {
      0,                         // flags
      MF2_DONTDRAW|MF2_NOSPLASH, // flags2
      0,                         // flags3
      "S_TNT1",                  // spawnstate
   },
};

//
// Thing Type Options
//

// acs spawndata sub-section
static cfg_opt_t acs_data[] =
{
   CFG_INT(ITEM_TNG_ACS_NUM,   -1,    CFGF_NONE),
   CFG_STR(ITEM_TNG_ACS_MODES, "all", CFGF_NONE),
   CFG_END()
};

// translation value-parsing callback
static int E_ColorCB(cfg_t *, cfg_opt_t *, const char *, void *);

// MOD value-parsing callback
static int E_ModCB(cfg_t *, cfg_opt_t *, const char *, void *);

#define THINGTYPE_FIELDS \
   CFG_INT(ITEM_TNG_DOOMEDNUM,    -1,       CFGF_NONE), \
   CFG_STR(ITEM_TNG_SPAWNSTATE,   "S_NULL", CFGF_NONE), \
   CFG_INT(ITEM_TNG_SPAWNHEALTH,  1000,     CFGF_NONE), \
   CFG_STR(ITEM_TNG_SEESTATE,     "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_TNG_SEESOUND,     "none",   CFGF_NONE), \
   CFG_INT(ITEM_TNG_REACTTIME,    8,        CFGF_NONE), \
   CFG_STR(ITEM_TNG_ATKSOUND,     "none",   CFGF_NONE), \
   CFG_STR(ITEM_TNG_PAINSTATE,    "S_NULL", CFGF_NONE), \
   CFG_INT(ITEM_TNG_PAINCHANCE,   0,        CFGF_NONE), \
   CFG_STR(ITEM_TNG_PAINSOUND,    "none",   CFGF_NONE), \
   CFG_STR(ITEM_TNG_MELEESTATE,   "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_TNG_MISSILESTATE, "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_TNG_DEATHSTATE,   "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_TNG_XDEATHSTATE,  "S_NULL", CFGF_NONE), \
   CFG_STR(ITEM_TNG_DEATHSOUND,   "none",   CFGF_NONE), \
   CFG_INT_CB(ITEM_TNG_SPEED,     0,        CFGF_NONE, E_IntOrFixedCB), \
   CFG_FLOAT(ITEM_TNG_RADIUS,     20.0f,    CFGF_NONE), \
   CFG_FLOAT(ITEM_TNG_HEIGHT,     16.0f,    CFGF_NONE), \
   CFG_INT(ITEM_TNG_MASS,         100,      CFGF_NONE), \
   CFG_INT(ITEM_TNG_DAMAGE,       0,        CFGF_NONE), \
   CFG_STR(ITEM_TNG_ACTIVESOUND,  "none",   CFGF_NONE), \
   CFG_STR(ITEM_TNG_FLAGS,        "",       CFGF_NONE), \
   CFG_STR(ITEM_TNG_FLAGS2,       "",       CFGF_NONE), \
   CFG_STR(ITEM_TNG_RAISESTATE,   "S_NULL", CFGF_NONE), \
   CFG_INT_CB(ITEM_TNG_TRANSLUC,  65536,    CFGF_NONE, E_TranslucCB), \
   CFG_STR(ITEM_TNG_FLAGS3,       "",       CFGF_NONE), \
   CFG_INT(ITEM_TNG_BLOODCOLOR,   0,        CFGF_NONE), \
   CFG_INT_CB(ITEM_TNG_FASTSPEED, 0,        CFGF_NONE, E_IntOrFixedCB), \
   CFG_STR(ITEM_TNG_NUKESPEC,     "NULL",   CFGF_NONE), \
   CFG_STR(ITEM_TNG_PARTICLEFX,   "",       CFGF_NONE), \
   CFG_STR(ITEM_TNG_DROPTYPE,     "NONE",   CFGF_NONE), \
   CFG_INT_CB(ITEM_TNG_MOD,       0,        CFGF_NONE, E_ModCB), \
   CFG_STR(ITEM_TNG_OBIT1,        "NONE",   CFGF_NONE), \
   CFG_STR(ITEM_TNG_OBIT2,        "NONE",   CFGF_NONE), \
   CFG_INT_CB(ITEM_TNG_COLOR,     0,        CFGF_NONE, E_ColorCB), \
   CFG_STR(ITEM_TNG_CFLAGS,       "",       CFGF_NONE), \
   CFG_STR(ITEM_TNG_ADDFLAGS,     "",       CFGF_NONE), \
   CFG_STR(ITEM_TNG_REMFLAGS,     "",       CFGF_NONE), \
   CFG_STR(ITEM_TNG_DMGSPECIAL,   "NONE",   CFGF_NONE), \
   CFG_STR(ITEM_TNG_CRASHSTATE,   "S_NULL", CFGF_NONE), \
   CFG_INT(ITEM_TNG_DEHNUM,       -1,       CFGF_NONE), \
   CFG_STR(ITEM_TNG_SKINSPRITE,   "noskin", CFGF_NONE), \
   CFG_FLOAT(ITEM_TNG_C3DHEIGHT,  0.0f,     CFGF_NONE), \
   CFG_STR(ITEM_TNG_BASICTYPE,    "",       CFGF_NONE), \
   CFG_INT(ITEM_TNG_TOPDAMAGE,    0,        CFGF_NONE), \
   CFG_INT(ITEM_TNG_TOPDMGMASK,   0,        CFGF_NONE), \
   CFG_SEC(ITEM_TNG_ACS_SPAWN,    acs_data, CFGF_NOCASE), \
   CFG_END()

cfg_opt_t edf_thing_opts[] =
{
   CFG_STR(ITEM_TNG_INHERITS,  0, CFGF_NONE),
   THINGTYPE_FIELDS
};

cfg_opt_t edf_tdelta_opts[] =
{
   CFG_STR(ITEM_DELTA_NAME, 0, CFGF_NONE),
   THINGTYPE_FIELDS
};

//
// Thing Type Hash Lookup Functions
//

// Thing hash tables
// Note: Keep some buffer space between this prime number and the
// number of default mobj types defined, so that users can add a
// number of types without causing significant hash collisions. I
// do not recommend changing the hash table to be dynamically allocated.

#define NUMTHINGCHAINS 307
static int thing_namechains[NUMTHINGCHAINS];
static int thing_dehchains[NUMTHINGCHAINS];

//
// E_ThingNumForDEHNum
//
// As with states, things need to store their DeHackEd number now.
// Returns NUMMOBJTYPES if a thing type is not found. This is used
// extensively by parameterized codepointers.
//
int E_ThingNumForDEHNum(int dehnum)
{
   int thingnum;
   int thingkey = dehnum % NUMTHINGCHAINS;

   if(dehnum < 0)
      return NUMMOBJTYPES;

   thingnum = thing_dehchains[thingkey];
   while(thingnum != NUMMOBJTYPES && 
         mobjinfo[thingnum].dehnum != dehnum)
   {
      thingnum = mobjinfo[thingnum].dehnext;
   }

   return thingnum;
}

//
// E_GetThingNumForDEHNum
//
// As above, but causes a fatal error if a thing type is not found.
//
int E_GetThingNumForDEHNum(int dehnum)
{
   int thingnum = E_ThingNumForDEHNum(dehnum);

   if(thingnum == NUMMOBJTYPES)
      I_Error("E_GetThingNumForDEHNum: invalid deh num %d\n", dehnum);

   return thingnum;
}

//
// E_SafeThingType
//
// As above, but returns the 'Unknown' type if the requested
// one was not found.
//
int E_SafeThingType(int dehnum)
{
   int thingnum = E_ThingNumForDEHNum(dehnum);

   if(thingnum == NUMMOBJTYPES)
      thingnum = UnknownThingType;

   return thingnum;
}

//
// E_SafeThingName
//
// As above, but for names
//
int E_SafeThingName(const char *name)
{
   int thingnum = E_ThingNumForName(name);

   if(thingnum == NUMMOBJTYPES)
      thingnum = UnknownThingType;

   return thingnum;
}

//
// E_ThingNumForName
//
// Returns a thing type index given its name. Returns NUMMOBJTYPES
// if a thing type is not found.
//
int E_ThingNumForName(const char *name)
{
   int thingnum;
   unsigned int thingkey = D_HashTableKey(name) % NUMTHINGCHAINS;
   
   thingnum = thing_namechains[thingkey];
   while(thingnum != NUMMOBJTYPES && 
         strcasecmp(name, mobjinfo[thingnum].name))
   {
      thingnum = mobjinfo[thingnum].namenext;
   }
   
   return thingnum;
}

//
// E_GetThingNumForName
//
// As above, but causes a fatal error if the thing type isn't found.
//
int E_GetThingNumForName(const char *name)
{
   int thingnum = E_ThingNumForName(name);

   if(thingnum == NUMMOBJTYPES)
      I_Error("E_GetThingNumForName: bad thing type %s\n", name);

   return thingnum;
}

// haleyjd 03/22/06: automatic dehnum allocation
//
// Automatic allocation of dehacked numbers allows things to be used with
// parameterized codepointers without having had a DeHackEd number explicitly
// assigned to them by the EDF author. This was requested by several users
// after v3.33.02.
//

// allocation starts at D_MAXINT and works toward 0
static int edf_alloc_thing_dehnum = D_MAXINT;

boolean E_AutoAllocThingDEHNum(int thingnum)
{
   unsigned int key;
   int dehnum;
   mobjinfo_t *mi = &mobjinfo[thingnum];

#ifdef RANGECHECK
   if(mi->dehnum != -1)
      I_Error("E_AutoAllocThingDEHNum: called for thing with valid dehnum\n");
#endif

   // cannot assign because we're out of dehnums?
   if(edf_alloc_thing_dehnum < 0)
      return false;

   do
   {
      dehnum = edf_alloc_thing_dehnum--;
   } 
   while(dehnum >= 0 && E_ThingNumForDEHNum(dehnum) != NUMMOBJTYPES);

   // ran out while looking for an unused number?
   if(dehnum < 0)
      return false;

   // assign it!
   mi->dehnum = dehnum;
   key = dehnum % NUMTHINGCHAINS;
   mi->dehnext = thing_dehchains[key];
   thing_dehchains[key] = thingnum;

   return true;
}

// 
// Processing Functions
//

//
// E_CollectThings
//
// Pre-creates and hashes by name the thingtypes, for purpose 
// of mutual and forward references.
//
void E_CollectThings(cfg_t *tcfg)
{
   int i;

   // allocate array
   mobjinfo = Z_Malloc(sizeof(mobjinfo_t)*NUMMOBJTYPES, PU_STATIC, NULL);

   // initialize hash slots
   for(i = 0; i < NUMTHINGCHAINS; ++i)
      thing_namechains[i] = thing_dehchains[i] = NUMMOBJTYPES;

   // build hash tables
   E_EDFLogPuts("\t* Building thing hash tables\n");

   for(i = 0; i < NUMMOBJTYPES; ++i)
   {
      unsigned int key;
      cfg_t *thingcfg = cfg_getnsec(tcfg, EDF_SEC_THING, i);
      const char *name = cfg_title(thingcfg);
      int tempint;

      // verify length
      if(strlen(name) > 40)
      {
         E_EDFLoggedErr(2, 
            "E_CollectThings: invalid thing mnemonic '%s'\n", name);
      }

      // copy it to the thing
      memset(mobjinfo[i].name, 0, 41);
      strncpy(mobjinfo[i].name, name, 41);

      // hash it
      key = D_HashTableKey(name) % NUMTHINGCHAINS;
      mobjinfo[i].namenext = thing_namechains[key];
      thing_namechains[key] = i;

      // process dehackednum and add thing to dehacked hash table,
      // if appropriate
      tempint = cfg_getint(thingcfg, ITEM_TNG_DEHNUM);
      mobjinfo[i].dehnum = tempint;
      if(tempint != -1)
      {
         int dehkey = tempint % NUMTHINGCHAINS;
         int cnum;
         
         // make sure it doesn't exist yet
         if((cnum = E_ThingNumForDEHNum(tempint)) != NUMMOBJTYPES)
         {
            E_EDFLoggedErr(2, 
               "E_CollectThings: thing '%s' reuses dehackednum %d\n"
               "                 Conflicting thing: '%s'\n",
               mobjinfo[i].name, tempint, mobjinfo[cnum].name);
         }
   
         mobjinfo[i].dehnext = thing_dehchains[dehkey];
         thing_dehchains[dehkey] = i;
      }
   }

   // verify the existance of the Unknown thing type
   UnknownThingType = E_ThingNumForName("Unknown");
   if(UnknownThingType == NUMMOBJTYPES)
      E_EDFLoggedErr(2, "E_CollectThings: 'Unknown' thingtype must be defined!\n");
}

//
// E_ThingSound
//
// Does sound name lookup & verification and then stores the resulting
// sound DeHackEd number into *target.
//
static void E_ThingSound(const char *data, const char *fieldname, 
                         int thingnum, int *target)
{
   sfxinfo_t *sfx;

   if((sfx = E_EDFSoundForName(data)) == NULL)
   {
      // haleyjd 05/31/06: relaxed to warning
      E_EDFLogPrintf("\t\tWarning: thing '%s': invalid %s '%s'\n",
                     mobjinfo[thingnum].name, fieldname, data);
      sfx = &S_sfx[0];
   }
   
   // haleyjd 03/22/06: support auto-allocation of dehnums where possible
   if(sfx->dehackednum != -1 || E_AutoAllocSoundDEHNum(sfx))
      *target = sfx->dehackednum;
   else
   {
      E_EDFLogPrintf("\t\tWarning: failed to auto-allocate DeHackEd number "
                     "for sound %s\n", sfx->mnemonic);
      *target = 0;
   }
}

//
// E_ThingFrame
//
// Does frame name lookup & verification and then stores the resulting
// frame index into *target.
//
static void E_ThingFrame(const char *data, const char *fieldname,
                         int thingnum, int *target)
{
   int index;

   if((index = E_StateNumForName(data)) == NUMSTATES)
   {
      E_EDFLoggedErr(2, "E_ThingFrame: thing '%s': invalid %s '%s'\n",
                     mobjinfo[thingnum].name, fieldname, data);
   }
   *target = index;
}

//
// E_ColorCB
//
// libConfuse value-parsing callback for the thingtype translation
// field. Can accept an integer value which indicates one of the 14
// builtin player translations, or a lump name, which will be looked
// up within the translations namespace (T_START/T_END), allowing for
// custom sprite translations.
//
static int E_ColorCB(cfg_t *cfg, cfg_opt_t *opt, const char *value,
                     void *result)
{
   int num;
   char *endptr;

   num = strtol(value, &endptr, 0);

   // try lump name
   if(*endptr != '\0')
   {
      int markernum = W_GetNumForName("T_START");
      int lumpnum   = (W_CheckNumForName)(value, ns_translations);

      if(lumpnum == -1)
      {
         if(cfg)
         {
            cfg_error(cfg, "bad translation lump '%s'", value);
         }
         return -1;
      }
         
      *(long *)result = lumpnum - markernum + TRANSLATIONCOLOURS;
   }
   else
   {
      *(long *)result = num % TRANSLATIONCOLOURS;
   }

   return 0;
}


static int E_ModCB(cfg_t *cfg, cfg_opt_t *opt, const char *value, 
                   void *result)
{
   long num;
   char *endptr;

   num = strtol(value, &endptr, 0);

   // try mod name
   if(*endptr != '\0')
   {
      num = E_StrToNumLinear(MODNames, NUM_MOD_TYPES, value);

      if(num == NUM_MOD_TYPES)
         num = MOD_UNKNOWN;

      *(long *)result = num;
   }
   else
      *(long *)result = num;

   return 0;
}

// Thing type inheritance code -- 01/27/04

// thing_hitlist: keeps track of what thingtypes are initialized
static byte *thing_hitlist = NULL;

// thing_pstack: used by recursive E_ProcessThing to track inheritance
static int  *thing_pstack  = NULL;
static int  thing_pindex   = 0;

//
// E_CheckThingInherit
//
// Makes sure the thing type being inherited from has not already
// been inherited during the current inheritance chain. Returns
// false if the check fails, and true if it succeeds.
//
static boolean E_CheckThingInherit(int pnum)
{
   int i;

   for(i = 0; i < NUMMOBJTYPES; ++i)
   {
      // circular inheritance
      if(thing_pstack[i] == pnum)
         return false;

      // found end of list
      if(thing_pstack[i] == -1)
         break;
   }

   return true;
}

//
// E_AddThingToPStack
//
// Adds a type number to the inheritance stack.
//
static void E_AddThingToPStack(int num)
{
   // Overflow shouldn't happen since it would require cyclic
   // inheritance as well, but I'll guard against it anyways.
   
   if(thing_pindex >= NUMMOBJTYPES)
      E_EDFLoggedErr(2, "E_AddThingToPStack: max inheritance depth exceeded\n");

   thing_pstack[thing_pindex++] = num;
}

//
// E_ResetThingPStack
//
// Resets the thingtype inheritance stack, setting all the pstack
// values to -1, and setting pindex back to zero.
//
static void E_ResetThingPStack(void)
{
   int i;

   for(i = 0; i < NUMMOBJTYPES; ++i)
      thing_pstack[i] = -1;

   thing_pindex = 0;
}

//
// E_CopyThing
//
// Copies one thingtype into another.
//
static void E_CopyThing(int num, int pnum)
{
   char name[41];
   mobjinfo_t *this_mi;
   int dehnum, dehnext, namenext;
   
   this_mi = &mobjinfo[num];

   // must save the following fields in the destination thing
   
   dehnum   = this_mi->dehnum;
   dehnext  = this_mi->dehnext;
   namenext = this_mi->namenext;
   memcpy(name, this_mi->name, 41);

   // copy from source to destination
   memcpy(this_mi, &mobjinfo[pnum], sizeof(mobjinfo_t));

   // normalize special fields

   // must duplicate obituaries if they exist
   if(this_mi->obituary)
      this_mi->obituary = strdup(this_mi->obituary);
   if(this_mi->meleeobit)
      this_mi->meleeobit = strdup(this_mi->meleeobit);

   // copy speedset if one exists for parent type
   G_CopySpeedSet(num, pnum);

   // copy nukespec if one exists for parent type
   M_CopyNukeSpec(num, pnum);

   // must restore name and dehacked num data
   this_mi->dehnum   = dehnum;
   this_mi->dehnext  = dehnext;
   this_mi->namenext = namenext;
   memcpy(this_mi->name, name, 41);

   // other fields not inherited:

   // force doomednum of inheriting type to -1
   this_mi->doomednum = -1;
}

// IS_SET: this macro tests whether or not a particular field should
// be set. When applying deltas, we should not retrieve defaults.
// 01/27/04: Also, if inheritance is taking place, we should not
// retrieve defaults.

#undef  IS_SET
#define IS_SET(name) ((def && !inherits) || cfg_size(thingsec, (name)) > 0)

// Same as above, but also considers if the basictype has set fields in the thing.
// In that case, fields which are affected by basictype don't set defaults unless
// they are explicitly specified.

#define IS_SET_BT(name) \
   ((def && !inherits && !hasbtype) || cfg_size(thingsec, (name)) > 0)

//
// E_ProcessThing
//
// Generalized code to process the data for a single thing type
// structure. Doubles as code for thingtype and thingdelta.
//
void E_ProcessThing(int i, cfg_t *thingsec, cfg_t *pcfg, boolean def)
{
   double tempfloat;
   int tempint;
   const char *tempstr;
   boolean inherits = false;
   boolean cflags   = false;
   boolean hasbtype = false;

   // 01/27/04: added inheritance -- not in deltas
   if(def)
   {
      // if this thingtype is already processed via recursion due to
      // inheritance, don't process it again
      if(thing_hitlist[i])
         return;
      
      if(cfg_size(thingsec, ITEM_TNG_INHERITS) > 0)
      {
         cfg_t *parent_tngsec;
         
         // resolve parent thingtype
         int pnum = E_GetThingNumForName(cfg_getstr(thingsec, ITEM_TNG_INHERITS));

         // check against cyclic inheritance
         if(!E_CheckThingInherit(pnum))
         {
            E_EDFLoggedErr(2, 
               "E_ProcessThing: cyclic inheritance detected in thing '%s'\n",
               mobjinfo[i].name);
         }
         
         // add to inheritance stack
         E_AddThingToPStack(pnum);

         // process parent recursively
         parent_tngsec = cfg_getnsec(pcfg, EDF_SEC_THING, pnum);
         E_ProcessThing(pnum, parent_tngsec, pcfg, true);
         
         // copy parent to this thing
         E_CopyThing(i, pnum);
         
         // we inherit, so treat defaults as no value
         inherits = true;
      }

      // mark this thingtype as processed
      thing_hitlist[i] = 1;
   }

   // haleyjd 07/05/06: process basictype
   // Note that when basictype is present, the default handling of all fields
   // affected by the basictype will be changed.
   if(IS_SET(ITEM_TNG_BASICTYPE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_BASICTYPE);
      tempint = E_StrToNumLinear(BasicTypeNames, NUMBASICTYPES, tempstr);
      if(tempint != NUMBASICTYPES)
      {
         basicttype_t *basicType = &BasicThingTypes[tempint];

         mobjinfo[i].flags  = basicType->flags;
         mobjinfo[i].flags2 = basicType->flags2;
         mobjinfo[i].flags3 = basicType->flags3;

         if(basicType->spawnstate &&
            (tempint = E_StateNumForName(basicType->spawnstate)) != NUMSTATES)
            mobjinfo[i].spawnstate = tempint;
         else if(!inherits) // don't init to default if the thingtype inherits
            mobjinfo[i].spawnstate = NullStateNum;

         hasbtype = true; // mobjinfo has a basictype set
      }
   }

   // process doomednum
   if(IS_SET(ITEM_TNG_DOOMEDNUM))
      mobjinfo[i].doomednum = cfg_getint(thingsec, ITEM_TNG_DOOMEDNUM);

   // process spawnstate
   if(IS_SET_BT(ITEM_TNG_SPAWNSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_SPAWNSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_SPAWNSTATE, i, 
                   &(mobjinfo[i].spawnstate));
   }

   // process spawnhealth
   if(IS_SET(ITEM_TNG_SPAWNHEALTH))
      mobjinfo[i].spawnhealth = cfg_getint(thingsec, ITEM_TNG_SPAWNHEALTH);

   // process seestate
   if(IS_SET(ITEM_TNG_SEESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_SEESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_SEESTATE, i,
                   &(mobjinfo[i].seestate));
   }

   // process seesound
   if(IS_SET(ITEM_TNG_SEESOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_SEESOUND);
      E_ThingSound(tempstr, ITEM_TNG_SEESOUND, i,
                   &(mobjinfo[i].seesound));
   }

   // process reactiontime
   if(IS_SET(ITEM_TNG_REACTTIME))
      mobjinfo[i].reactiontime = cfg_getint(thingsec, ITEM_TNG_REACTTIME);

   // process attacksound
   if(IS_SET(ITEM_TNG_ATKSOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_ATKSOUND);
      E_ThingSound(tempstr, ITEM_TNG_ATKSOUND, i,
                   &(mobjinfo[i].attacksound));
   }

   // process painstate
   if(IS_SET(ITEM_TNG_PAINSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_PAINSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_PAINSTATE, i,
                   &(mobjinfo[i].painstate));
   }

   // process painchance
   if(IS_SET(ITEM_TNG_PAINCHANCE))
      mobjinfo[i].painchance = cfg_getint(thingsec, ITEM_TNG_PAINCHANCE);

   // process painsound
   if(IS_SET(ITEM_TNG_PAINSOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_PAINSOUND);
      E_ThingSound(tempstr, ITEM_TNG_PAINSOUND, i,
                   &(mobjinfo[i].painsound));
   }

   // process meleestate
   if(IS_SET(ITEM_TNG_MELEESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_MELEESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_MELEESTATE, i,
                   &(mobjinfo[i].meleestate));
   }

   // process missilestate
   if(IS_SET(ITEM_TNG_MISSILESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_MISSILESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_MISSILESTATE, i,
                   &(mobjinfo[i].missilestate));
   }

   // process deathstate
   if(IS_SET(ITEM_TNG_DEATHSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DEATHSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_DEATHSTATE, i,
                   &(mobjinfo[i].deathstate));
   }

   // process xdeathstate
   if(IS_SET(ITEM_TNG_XDEATHSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_XDEATHSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_XDEATHSTATE, i,
                   &(mobjinfo[i].xdeathstate));
   }

   // process deathsound
   if(IS_SET(ITEM_TNG_DEATHSOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DEATHSOUND);
      E_ThingSound(tempstr, ITEM_TNG_DEATHSOUND, i,
                   &(mobjinfo[i].deathsound));
   }

   // process speed
   if(IS_SET(ITEM_TNG_SPEED))
      mobjinfo[i].speed = cfg_getint(thingsec, ITEM_TNG_SPEED);

   // process radius
   if(IS_SET(ITEM_TNG_RADIUS))
   {
      tempfloat = cfg_getfloat(thingsec, ITEM_TNG_RADIUS);
      mobjinfo[i].radius = (int)(tempfloat * FRACUNIT);
   }

   // process height
   if(IS_SET(ITEM_TNG_HEIGHT))
   {
      tempfloat = cfg_getfloat(thingsec, ITEM_TNG_HEIGHT);
      mobjinfo[i].height = (int)(tempfloat * FRACUNIT);
   }

   // process mass
   if(IS_SET(ITEM_TNG_MASS))
      mobjinfo[i].mass = cfg_getint(thingsec, ITEM_TNG_MASS);

   // process damage
   if(IS_SET(ITEM_TNG_DAMAGE))
      mobjinfo[i].damage = cfg_getint(thingsec, ITEM_TNG_DAMAGE);

   // process activesound
   if(IS_SET(ITEM_TNG_ACTIVESOUND))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_ACTIVESOUND);
      E_ThingSound(tempstr, ITEM_TNG_ACTIVESOUND, i,
                   &(mobjinfo[i].activesound));
   }

   // 02/19/04: process combined flags first
   if(IS_SET_BT(ITEM_TNG_CFLAGS))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_CFLAGS);
      if(*tempstr == '\0')
      {
         mobjinfo[i].flags = mobjinfo[i].flags2 = mobjinfo[i].flags3 = 0;
      }
      else
      {
         long *results = deh_ParseFlagsCombined(tempstr);

         mobjinfo[i].flags  = results[0];
         mobjinfo[i].flags2 = results[1];
         mobjinfo[i].flags3 = results[2];
         
         cflags = true; // values were set from cflags
      }
   }

   if(!cflags) // skip if cflags are defined
   {
      // process flags
      if(IS_SET_BT(ITEM_TNG_FLAGS))
      {
         tempstr = cfg_getstr(thingsec, ITEM_TNG_FLAGS);
         if(*tempstr == '\0')
            mobjinfo[i].flags = 0;
         else
            mobjinfo[i].flags = deh_ParseFlagsSingle(tempstr, DEHFLAGS_MODE1);
      }
      
      // process flags2
      if(IS_SET_BT(ITEM_TNG_FLAGS2))
      {
         tempstr = cfg_getstr(thingsec, ITEM_TNG_FLAGS2);
         if(*tempstr == '\0')
            mobjinfo[i].flags2 = 0;
         else
            mobjinfo[i].flags2 = deh_ParseFlagsSingle(tempstr, DEHFLAGS_MODE2);
      }

      // process flags3
      if(IS_SET_BT(ITEM_TNG_FLAGS3))
      {
         tempstr = cfg_getstr(thingsec, ITEM_TNG_FLAGS3);
         if(*tempstr == '\0')
            mobjinfo[i].flags3 = 0;
         else
            mobjinfo[i].flags3 = deh_ParseFlagsSingle(tempstr, DEHFLAGS_MODE3);
      }
   }

   // process addflags and remflags modifiers

   if(cfg_size(thingsec, ITEM_TNG_ADDFLAGS) > 0)
   {
      long *results;

      tempstr = cfg_getstr(thingsec, ITEM_TNG_ADDFLAGS);
         
      results = deh_ParseFlagsCombined(tempstr);

      mobjinfo[i].flags  |= results[0];
      mobjinfo[i].flags2 |= results[1];
      mobjinfo[i].flags3 |= results[2];
   }

   if(cfg_size(thingsec, ITEM_TNG_REMFLAGS) > 0)
   {
      long *results;

      tempstr = cfg_getstr(thingsec, ITEM_TNG_REMFLAGS);

      results = deh_ParseFlagsCombined(tempstr);

      mobjinfo[i].flags  &= ~(results[0]);
      mobjinfo[i].flags2 &= ~(results[1]);
      mobjinfo[i].flags3 &= ~(results[2]);
   }

   // process raisestate
   if(IS_SET(ITEM_TNG_RAISESTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_RAISESTATE);
      E_ThingFrame(tempstr, ITEM_TNG_RAISESTATE, i,
                   &(mobjinfo[i].raisestate));
   }

   // process translucency
   if(IS_SET(ITEM_TNG_TRANSLUC))
      mobjinfo[i].translucency = cfg_getint(thingsec, ITEM_TNG_TRANSLUC);

   // process bloodcolor
   if(IS_SET(ITEM_TNG_BLOODCOLOR))
      mobjinfo[i].bloodcolor = cfg_getint(thingsec, ITEM_TNG_BLOODCOLOR);

   // 07/13/03: process fastspeed
   // get the fastspeed and, if non-zero, add the thing
   // to the speedset list in g_game.c

   if(IS_SET(ITEM_TNG_FASTSPEED))
   {
      tempint = cfg_getint(thingsec, ITEM_TNG_FASTSPEED);         
      if(tempint)
         G_SpeedSetAddThing(i, mobjinfo[i].speed, tempint);
   }

   // 07/13/03: process nukespecial
   // get the nukespecial, and if not NULL, add the thing
   // to the nukespec hash table in m_cheat.c

   if(IS_SET(ITEM_TNG_NUKESPEC))
   {
      deh_bexptr *dp;

      tempstr = cfg_getstr(thingsec, ITEM_TNG_NUKESPEC);
      
      if(!(dp = D_GetBexPtr(tempstr)))
      {
         E_EDFLoggedErr(2, 
            "E_ProcessThing: thing '%s': bad nukespecial '%s'\n",
            mobjinfo[i].name, tempstr);
      }
      
      if(dp->cptr != NULL)
         M_AddNukeSpec(i, (void (*)(mobj_t*))(dp->cptr)); // yuck!
   }

   // 07/13/03: process particlefx
   if(IS_SET(ITEM_TNG_PARTICLEFX))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_PARTICLEFX);
      if(*tempstr == '\0')
         mobjinfo[i].particlefx = 0;
      else
         mobjinfo[i].particlefx = E_ParseFlags(tempstr, &particle_flags);
   }
      
   // 07/13/03: process droptype
   if(IS_SET(ITEM_TNG_DROPTYPE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DROPTYPE);
      if(!strcasecmp(tempstr, "NONE"))
         mobjinfo[i].droptype = -1;
      else
      {
         tempint = E_ThingNumForName(tempstr);

         if(tempint == NUMMOBJTYPES)
         {
            // haleyjd 05/31/06: demoted to warning
            E_EDFLogPrintf("\t\tWarning: thing '%s': bad drop type '%s'\n",
                           mobjinfo[i].name, tempstr);
            tempint = UnknownThingType;
         }

         mobjinfo[i].droptype = tempint;
      }
   }

   // 07/13/03: process mod
   if(IS_SET(ITEM_TNG_MOD))
      mobjinfo[i].mod = cfg_getint(thingsec, ITEM_TNG_MOD);

   // 07/13/03: process obituaries
   if(IS_SET(ITEM_TNG_OBIT1))
   {
      // if this is a delta or the thing type inherited obits
      // from its parent, we need to free any old obituary
      if((!def || inherits) && mobjinfo[i].obituary)
         free(mobjinfo[i].obituary);

      tempstr = cfg_getstr(thingsec, ITEM_TNG_OBIT1);
      if(strcasecmp(tempstr, "NONE"))
         mobjinfo[i].obituary = strdup(tempstr);
      else
         mobjinfo[i].obituary = NULL;
   }

   if(IS_SET(ITEM_TNG_OBIT2))
   {
      // if this is a delta or the thing type inherited obits
      // from its parent, we need to free any old obituary
      if((!def || inherits) && mobjinfo[i].meleeobit)
         free(mobjinfo[i].meleeobit);

      tempstr = cfg_getstr(thingsec, ITEM_TNG_OBIT2);
      if(strcasecmp(tempstr, "NONE"))
         mobjinfo[i].meleeobit = strdup(tempstr);
      else
         mobjinfo[i].meleeobit = NULL;
   }

   // 01/12/04: process translation
   if(IS_SET(ITEM_TNG_COLOR))
      mobjinfo[i].colour = cfg_getint(thingsec, ITEM_TNG_COLOR);

   // 08/01/04: process dmgspecial
   if(IS_SET(ITEM_TNG_DMGSPECIAL))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_DMGSPECIAL);
      
      // find the proper dmgspecial number (linear search)
      tempint = E_StrToNumLinear(inflictorTypes, INFLICTOR_NUMTYPES, tempstr);

      if(tempint == INFLICTOR_NUMTYPES)
      {
         E_EDFLoggedErr(2, 
            "E_ProcessThing: thing '%s': invalid dmgspecial '%s'\n",
            mobjinfo[i].name, tempstr);
      }

      mobjinfo[i].dmgspecial = tempint;
   }

   // 08/07/04: process crashstate
   if(IS_SET(ITEM_TNG_CRASHSTATE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_CRASHSTATE);
      E_ThingFrame(tempstr, ITEM_TNG_CRASHSTATE, i,
                   &(mobjinfo[i].crashstate));
   }

   // 09/26/04: process alternate sprite
   if(IS_SET(ITEM_TNG_SKINSPRITE))
   {
      tempstr = cfg_getstr(thingsec, ITEM_TNG_SKINSPRITE);
      mobjinfo[i].altsprite = E_SpriteNumForName(tempstr);
   }

   // 07/06/05: process correct 3D thing height
   if(IS_SET(ITEM_TNG_C3DHEIGHT))
   {
      tempfloat = cfg_getfloat(thingsec, ITEM_TNG_C3DHEIGHT);
      mobjinfo[i].c3dheight = (int)(tempfloat * FRACUNIT);
   }

   // 09/22/06: process topdamage 
   if(IS_SET(ITEM_TNG_TOPDAMAGE))
      mobjinfo[i].topdamage = cfg_getint(thingsec, ITEM_TNG_TOPDAMAGE);

   // 09/23/06: process topdamagemask
   if(IS_SET(ITEM_TNG_TOPDMGMASK))
      mobjinfo[i].topdamagemask = cfg_getint(thingsec, ITEM_TNG_TOPDMGMASK);

   // 01/17/07: process acs_spawndata
   if(cfg_size(thingsec, ITEM_TNG_ACS_SPAWN) > 0)
   {
      cfg_t *acs_sec = cfg_getsec(thingsec, ITEM_TNG_ACS_SPAWN);

      // get ACS spawn number
      tempint = cfg_getint(acs_sec, ITEM_TNG_ACS_NUM);

      // rangecheck number
      if(tempint >= ACS_NUM_THINGTYPES)
      {
         E_EDFLogPrintf("\t\tWarning: invalid ACS spawn number %d\n", tempint);
      }
      else if(tempint >= 0) // negative numbers mean no spawn number
      {
         long flags;

         // get mode flags
         tempstr = cfg_getstr(acs_sec, ITEM_TNG_ACS_MODES);

         flags = E_ParseFlags(tempstr, &acs_gamemode_flags);

         if(flags & gameTypeToACSFlags[gameModeInfo->type])
            ACS_thingtypes[tempint] = i;
      }
   }

   // output end message if processing a definition
   if(def)
      E_EDFLogPrintf("\t\tFinished thing %s(#%d)\n", mobjinfo[i].name, i);
}

//
// E_ProcessThings
//
// Resolves and loads all information for the mobjinfo_t structures.
//
void E_ProcessThings(cfg_t *cfg)
{
   int i;

   E_EDFLogPuts("\t* Processing thing data\n");

   // allocate inheritance stack and hitlist
   thing_hitlist = Z_Calloc(NUMMOBJTYPES, sizeof(byte), PU_STATIC, 0);
   thing_pstack  = Z_Malloc(NUMMOBJTYPES * sizeof(int), PU_STATIC, 0);

   // 01/17/07: initialize ACS thingtypes array
   for(i = 0; i < ACS_NUM_THINGTYPES; ++i)
      ACS_thingtypes[i] = UnknownThingType;

   for(i = 0; i < NUMMOBJTYPES; ++i)
   {
      cfg_t *thingsec = cfg_getnsec(cfg, EDF_SEC_THING, i);

      // reset the inheritance stack
      E_ResetThingPStack();

      // add this thing to the stack
      E_AddThingToPStack(i);

      E_ProcessThing(i, thingsec, cfg, true);
   }

   // free tables
   Z_Free(thing_hitlist);
   Z_Free(thing_pstack);
}

//
// E_ProcessThingDeltas
//
// Does processing for thingdelta sections, which allow cascading
// editing of existing things. The thingdelta shares most of its
// fields and processing code with the thingtype section.
//
void E_ProcessThingDeltas(cfg_t *cfg)
{
   int i, numdeltas;

   E_EDFLogPuts("\t* Processing thing deltas\n");

   numdeltas = cfg_size(cfg, EDF_SEC_TNGDELTA);

   E_EDFLogPrintf("\t\t%d thingdelta(s) defined\n", numdeltas);

   for(i = 0; i < numdeltas; i++)
   {
      const char *tempstr;
      int mobjType;
      cfg_t *deltasec = cfg_getnsec(cfg, EDF_SEC_TNGDELTA, i);

      // get thingtype to edit
      if(!cfg_size(deltasec, ITEM_DELTA_NAME))
         E_EDFLoggedErr(2, "E_ProcessThingDeltas: thingdelta requires name field\n");

      tempstr = cfg_getstr(deltasec, ITEM_DELTA_NAME);
      mobjType = E_GetThingNumForName(tempstr);

      E_ProcessThing(mobjType, deltasec, cfg, false);

      E_EDFLogPrintf("\t\tApplied thingdelta #%d to %s(#%d)\n",
                     i, mobjinfo[mobjType].name, mobjType);
   }
}

// EOF


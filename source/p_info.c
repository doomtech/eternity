// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley, Simon Howard, et al.
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
// Level info.
//
// Under SMMU, level info is stored in the level marker: ie. "mapxx"
// or "exmx" lump. This contains new info such as: the level name, music
// lump to be played, par time etc.
//
// By Simon Howard
//
// Eternity Modifications:
// -----------------------
// As of 07/22/04, Eternity can now support global MapInfo lumps in
// addition to MapInfo placed in level headers. Header MapInfo always
// overrides any global lump data. The global lumps cascade in such
// a way that a map will get its global MapInfo from the newest 
// "EMAPINFO" lump that contains a block with the map's name.
//
// I have also moved much of the initialization code from many modules
// into this file, making the LevelInfo structure the default place to
// get map information, rather than an alternative to many other
// sources as it was previously. This simplifies code outside this
// module and encapsulates more level-dependent decisions here.
//
// -- haleyjd
//
//-----------------------------------------------------------------------------

/* includes ************************/

#include "z_zone.h"
#include "d_io.h"
#include "doomstat.h"
#include "doomdef.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "f_finale.h"
#include "w_wad.h"
#include "p_setup.h"
#include "p_info.h"
#include "p_mobj.h"
#include "sounds.h"
#include "d_gi.h"
#include "d_deh.h"
#include "e_sound.h"
#include "g_game.h"
#include "m_qstr.h"
#include "m_misc.h"
#include "m_dllist.h"
#include "e_lib.h"
#include "e_hash.h"

extern char gamemapname[9];

//
// LevelInfoProto
//
// haleyjd 06/21/10: This structure is a prototype for LevelInfo. Information
// will be stored here from sources of global information such as EMAPINFO or
// Hexen MAPINFO lumps, and then copied to the normal LevelInfo structure at
// the start of each level.
//
// A single separate prototype object serves as the read destination for the
// current pass of parsing. This is so that the levelvars array can reference
// the fields of a static object without overwriting the LevelInfo object 
// itself.
//
typedef struct LevelInfoProto_s
{
   mdllistitem_t links;                      // links for hashing
   char          mapname[9];                 // name of map to which this belongs
   LevelInfo_t info;                         // the LevelInfo object
   boolean     modified[LI_FIELD_NUMFIELDS]; // array of bools to track modified fields
} LevelInfoProto_t;

// haleyjd: moved everything into the LevelInfo struct

LevelInfo_t LevelInfo;

static void P_ParseLevelInfo(int lumpnum);

static int  P_ParseInfoCmd(qstring_t *line);
static void P_ParseLevelVar(qstring_t *cmd);

static void P_ClearLevelVars(void);
static void P_InitWeapons(void);

// post-processing routine prototypes
static void P_LoadInterTextLump(void);
static void P_SetSky2Texture(void);
static void P_SetParTime(void);
static void P_SetInfoSoundNames(void);
static void P_SetOutdoorFog(void);

static char *P_openWadTemplate(const char *wadfile, int *len);
static char *P_findTextInTemplate(char *text, int len, int titleOrAuthor);

// haleyjd 05/30/10: struct for info read from a metadata file
typedef struct metainfo_s
{
   int level;             // level to use on
   const char *levelname; // name
   int partime;           // par time
   const char *musname;   // music name
   int nextlevel;         // next level #, only used if non-0
   int nextsecret;        // next secret #, only used if non-0
   boolean finale;        // if true, sets LevelInfo.endOfGame
   const char *intertext; // only used if finale is true
} metainfo_t;

static int nummetainfo, nummetainfoalloc;
static metainfo_t *metainfo;
static metainfo_t *curmetainfo;

static metainfo_t *P_GetMetaInfoForLevel(int mapnum);

static enum lireadtype_e
{
   RT_LEVELINFO,
   RT_OTHER,
} readtype;

static enum limode_e
{
   LI_MODE_GLOBAL, // global: we're reading a global MapInfo lump
   LI_MODE_LEVEL,  // level:  we're reading a level header
   LI_NUMMODES
} limode;

static boolean foundGlobalMap;

// haleyjd: flag set for boss specials
static dehflags_t boss_spec_flags[] =
{
   { "MAP07_1", BSPEC_MAP07_1 },
   { "MAP07_2", BSPEC_MAP07_2 },
   { "E1M8",    BSPEC_E1M8 },
   { "E2M8",    BSPEC_E2M8 },
   { "E3M8",    BSPEC_E3M8 },
   { "E4M6",    BSPEC_E4M6 },
   { "E4M8",    BSPEC_E4M8 },
   { "E5M8",    BSPEC_E5M8 },
   { NULL,      0 }
};

static dehflagset_t boss_flagset =
{
   boss_spec_flags, // flaglist
   0,               // mode: single flags word
};

typedef struct textvals_s
{
   const char **vals;
   int numvals;
   int defaultval;
} textvals_t;

static const char *ltstrs[] =
{
   "doom",
   "heretic",
};

static textvals_t levelTypeVals =
{
   ltstrs,
   2,
   0
};

static const char *finaleTypeStrs[] =
{
   "text",
   "doom_credits",
   "doom_deimos",
   "doom_bunny",
   "doom_marine",
   "htic_credits",
   "htic_water",
   "htic_demon",
};

static textvals_t finaleTypeVals =
{
   finaleTypeStrs,
   FINALE_NUMFINALES,
   0
};

//
// P_LoadLevelInfo
//
// Parses global MapInfo lumps looking for the first entry that
// matches this map, then parses the map's own header MapInfo if
// it has any. This is the main external function of this module.
// Called from P_SetupLevel.
//
// lvname, if non-null, is the filepath of the wad that contains
// the level when it has been loaded from a managed wad directory.
//
void P_LoadLevelInfo(int lumpnum, const char *lvname)
{
   lumpinfo_t *lump;
   int glumpnum;

   // set all the level defaults
   P_ClearLevelVars();

   // override with info from text file?
   if(lvname)
   {
      int len = 0;
      char *text = P_openWadTemplate(lvname, &len);
      char *str;

      if(text && len > 0)
      {
         // look for title
         if((str = P_findTextInTemplate(text, len, 0)))
            LevelInfo.levelName = str;

         // look for author
         if((str = P_findTextInTemplate(text, len, 1)))
            LevelInfo.creator = str;
      }
   }

   // parse global lumps
   limode = LI_MODE_GLOBAL;
   foundGlobalMap = false;

   // run down the hash chain for EMAPINFO
   lump = w_GlobalDir.lumpinfo[W_LumpNameHash("EMAPINFO") % 
                               (unsigned int)w_GlobalDir.numlumps];   
   
   for(glumpnum = lump->index; glumpnum >= 0; glumpnum = lump->next)
   {
      lump = w_GlobalDir.lumpinfo[glumpnum];

      if(!strncasecmp(lump->name, "EMAPINFO", 8) &&
         lump->li_namespace == ns_global)
      {
         // reset the parser state         
         readtype = RT_OTHER;
         P_ParseLevelInfo(glumpnum);
         if(foundGlobalMap) // parsed an entry for this map, so stop
            break;
      }
   }
   
   // parse level lump
   limode   = LI_MODE_LEVEL;
   readtype = RT_OTHER;
   P_ParseLevelInfo(lumpnum);
   
   // haleyjd: call post-processing routines
   P_LoadInterTextLump();
   P_SetSky2Texture();
   P_SetParTime();
   P_SetInfoSoundNames();
   P_SetOutdoorFog();

   // haleyjd 03/15/03: handle level scripts
   if(LevelInfo.scriptLump)
      LevelInfo.hasScripts = true;

   P_InitWeapons();
}

//=============================================================================
//
// LevelInfo Prototypes Implementation
//
// haleyjd 06/21/10
//

static LevelInfoProto_t **levelInfoPrototypes; // reallocating array of pointers
static int numPrototypes;
static int numPrototypesAlloc;
static ehash_t protoHash; // hash table for prototype objects

// key retrieval function for prototype hash table
E_KEYFUNC(LevelInfoProto_t, mapname)

//
// P_addLevelInfoPrototype
//
// haleyjd 06/21/10: Adds a LevelInfo prototype object to the reallocating
// pointer list and to the hash table.
//
static LevelInfoProto_t *P_addLevelInfoPrototype(const char *mapname)
{
   LevelInfoProto_t *newProto = calloc(1, sizeof(LevelInfoProto_t));

   // reallocate prototype pointers array if necessary
   if(numPrototypes >= numPrototypesAlloc)
   {
      numPrototypesAlloc = numPrototypesAlloc ? numPrototypesAlloc * 2 : 40;

      levelInfoPrototypes = realloc(levelInfoPrototypes,
                                    numPrototypesAlloc * sizeof(LevelInfoProto_t *));
   }

   // add it to the pointer array
   levelInfoPrototypes[numPrototypes++] = newProto;

   // initialize name
   strncpy(newProto->mapname, mapname, 8);

   // initialize hash table first time if necessary
   if(!protoHash.isinit)
   {
      E_NCStrHashInit(&protoHash, numPrototypesAlloc, 
                      E_KEYFUNCNAME(LevelInfoProto_t, mapname), NULL);
   }

   // add it to the hash table
   E_HashAddObject(&protoHash, newProto);
}

//
// P_clearLevelInfoPrototypes
//
// haleyjd 06/21/10: Deletes all existing LevelInfo prototypes in the event that
// global MAPINFO sources are being reparsed.
//
static void P_clearLevelInfoPrototypes(void)
{
   int i;

   // destroy the hash table
   E_HashDestroy(&protoHash);

   // free all the LevelInfo objects
   for(i = 0; i < numPrototypes; ++i)
      free(levelInfoPrototypes[i]);

   // free the pointer array
   free(levelInfoPrototypes);
   levelInfoPrototypes = NULL;
   numPrototypes = numPrototypesAlloc = 0;
}

//
// P_getLevelInfoPrototype
//
// haleyjd 06/21/10: Returns a LevelInfoProto object for the given map name,
// if such exists. Returns NULL otherwise.
//
static LevelInfoProto_t *P_getLevelInfoPrototype(const char *mapname)
{
   return E_HashObjectForKey(&protoHash, &mapname);
}

//
// P_LoadGlobalLevelInfo
//
// haleyjd 06/21/10: This function is now responsible for loading and caching
// global level info into LevelInfoProto objects. If this routine is called
// more than once (for example, for runtime wad loading), the hive will be
// dumped and all EMAPINFO lumps will be parsed again.
//
void P_LoadGlobalLevelInfo(void)
{
   // if any prototypes exist, delete them
   if(numPrototypes)
      P_clearLevelInfoPrototypes();
}

//
// P_ParseLevelInfo
//
// Parses one individual MapInfo lump.
//
static void P_ParseLevelInfo(int lumpnum)
{
   qstring_t line;
   char *lump, *rover;
   int  size;

   // haleyjd 03/12/05: seriously restructured to eliminate last line
   // problem and to use qstring_t to buffer lines
   
   // if lump is zero size, we are done
   if(!(size = w_GlobalDir.lumpinfo[lumpnum]->size))
      return;

   // allocate lump buffer with size + 2 to allow for termination
   size += 2;
   lump = Z_Malloc(size, PU_STATIC, NULL);
   W_ReadLump(lumpnum, lump);

   // terminate lump data with a line break and null character;
   // this makes uniform parsing much easier
   // haleyjd 04/08/07: buffer overrun repaired!
   lump[size - 2] = '\n';
   lump[size - 1] = '\0';

   rover = lump;

   // create the line buffer
   M_QStrInitCreate(&line);
   
   while(*rover)
   {
      if(*rover == '\n') // end of line
      {
         // hack for global MapInfo: if P_ParseInfoCmd returns -1,
         // we can break out of parsing early
         if(P_ParseInfoCmd(&line) == -1)
            break;
         M_QStrClear(&line); // clear line buffer
      }
      else
         M_QStrPutc(&line, *rover); // add char to line buffer

      ++rover;
   }

   // free the line buffer
   M_QStrFree(&line);

   // free the lump
   Z_Free(lump);
}

//
// P_ParseInfoCmd
//
// Parses a single line of a MapInfo lump.
//
static int P_ParseInfoCmd(qstring_t *line)
{
   unsigned int len;
   const char *label = NULL;

   M_QStrReplace(line, "\t\r\n", ' '); // erase any control characters
   M_QStrLwr(line);                    // make everything lowercase
   M_QStrLStrip(line, ' ');            // strip spaces at beginning
   M_QStrRStrip(line, ' ');            // strip spaces at end
   
   if(!(len = M_QStrLen(line)))                // ignore totally empty lines
      return 0;

   // detect comments at beginning
   if(line->buffer[0] == '#' || line->buffer[0] == ';' || 
      (len > 1 && line->buffer[0] == '/' && line->buffer[1] == '/'))
      return 0;
     
   if((label = M_QStrChr(line, '[')))  // a new section separator
   {
      ++label;

      if(limode == LI_MODE_GLOBAL)
      {
         // when in global mode, returning -1 will make
         // P_ParseLevelInfo break out early, saving time
         if(foundGlobalMap)
            return -1;

         if(!strncasecmp(label, gamemapname, strlen(gamemapname)))
         {
            foundGlobalMap = true;
            readtype = RT_LEVELINFO;
         }
      }
      else
      {
         if(!strncmp(label, "level info", 10))
            readtype = RT_LEVELINFO;
      }

      // go to next line immediately
      return 0;
   }
  
   switch(readtype)
   {
   case RT_LEVELINFO:
      P_ParseLevelVar(line);
      break;

   case RT_OTHER:
      break;
   }

   return 0;
}

//
//  Level vars: level variables in the [level info] section.
//
//  Takes the form:
//     [variable name] = [value]
//
//  '=' sign is optional
//

enum
{
   IVT_STRING,
   IVT_STRNUM,
   IVT_INT,
   IVT_BOOLEAN,
   IVT_FLAGS,
   IVT_END
};

typedef struct levelvar_s
{
   int   type;
   char *name;
   int   fieldenum;
   void *variable;
   void *extra;
} levelvar_t;

#define LI_STRING(name, enumval, field) \
   { IVT_STRING, name, LI_FIELD_ ## enumval, (void *)(&(field)) }
#define LI_STRNUM(name, enumval, field, extra) \
   { IVT_STRNUM, name, LI_FIELD_ ## enumval, (void *)(&(field)), &(extra) }
#define LI_INTEGR(name, enumval, field) \
   { IVT_INT, name, LI_FIELD_ ## enumval, &(field) }
#define LI_BOOLNF(name, enumval, field) \
   { IVT_BOOLEAN, name, LI_FIELD_ ## enumval, &(field) }
#define LI_FLAGSF(name, enumval, field, extra) \
   { IVT_FLAGS, name, LI_FIELD_ ## enumval, &(field), &(extra) }
#define LI_END() \
   { IVT_END, NULL, LI_FIELD_NUMFIELDS }

levelvar_t levelvars[]=
{
   LI_STRING("altskyname",      ALTSKYNAME,       LevelInfo.altSkyName),
   LI_FLAGSF("boss-specials",   BOSSSPECS,        LevelInfo.bossSpecs,        boss_flagset),
   LI_STRING("colormap",        COLORMAP,         LevelInfo.colorMap),
   LI_STRING("creator",         CREATOR,          LevelInfo.creator),
   LI_BOOLNF("doublesky",       DOUBLESKY,        LevelInfo.doubleSky),
   LI_BOOLNF("edf_intername",   USEEDFINTERNAME,  LevelInfo.useEDFInterName),
   LI_BOOLNF("endofgame",       ENDOFGAME,        LevelInfo.endOfGame),
   LI_STRING("extradata",       EXTRADATA,        LevelInfo.extraData),
   LI_BOOLNF("finale-secret",   FINALESECRETONLY, LevelInfo.finaleSecretOnly), 
   LI_STRNUM("finaletype",      FINALETYPE,       LevelInfo.finaleType,       finaleTypeVals),
   LI_BOOLNF("fullbright",      USEFULLBRIGHT,    LevelInfo.useFullBright),
   LI_INTEGR("gravity",         GRAVITY,          LevelInfo.gravity),
   LI_STRING("inter-backdrop",  BACKDROP,         LevelInfo.backDrop),
   LI_STRING("intermusic",      INTERMUSIC,       LevelInfo.interMusic),
   LI_STRING("interpic",        INTERPIC,         LevelInfo.interPic),
   LI_STRING("intertext",       INTERTEXTLUMP,    LevelInfo.interTextLump),
   LI_BOOLNF("killfinale",      KILLFINALE,       LevelInfo.killFinale),
   LI_BOOLNF("killstats",       KILLSTATS,        LevelInfo.killStats),
   LI_STRING("levelname",       LEVELNAME,        LevelInfo.levelName),
   LI_STRING("levelpic",        LEVELPIC,         LevelInfo.levelPic),
   LI_STRING("levelpicnext",    NEXTLEVELPIC,     LevelInfo.nextLevelPic),
   LI_STRING("levelpicsecret",  NEXTSECRETPIC,    LevelInfo.nextSecretPic),
   LI_STRING("levelscript",     SCRIPTLUMP,       LevelInfo.scriptLump),
   LI_STRNUM("leveltype",       LEVELTYPE,        LevelInfo.levelType,        levelTypeVals),
   LI_BOOLNF("lightning",       HASLIGHTNING,     LevelInfo.hasLightning),
   LI_STRING("music",           MUSICNAME,        LevelInfo.musicName),
   LI_STRING("nextlevel",       NEXTLEVEL,        LevelInfo.nextLevel),
   LI_STRING("nextsecret",      NEXTSECRET,       LevelInfo.nextSecret),
   LI_BOOLNF("noautosequences", NOAUTOSEQUENCES,  LevelInfo.noAutoSequences),
   LI_STRING("outdoorfog",      OUTDOORFOG,       LevelInfo.outdoorFog),
   LI_INTEGR("partime",         PARTIME,          LevelInfo.partime),
   LI_INTEGR("skydelta",        SKYDELTA,         LevelInfo.skyDelta),
   LI_INTEGR("sky2delta",       SKY2DELTA,        LevelInfo.sky2Delta),
   LI_STRING("skyname",         SKYNAME,          LevelInfo.skyName),
   LI_STRING("sky2name",        SKY2NAME,         LevelInfo.sky2Name),
   LI_STRING("sound-swtchn",    SOUNDSWTCHN,      LevelInfo.sound_swtchn),
   LI_STRING("sound-swtchx",    SOUNDSWTCHX,      LevelInfo.sound_swtchx),
   LI_STRING("sound-stnmov",    SOUNDSTNMOV,      LevelInfo.sound_stnmov),
   LI_STRING("sound-pstop",     SOUNDPSTOP,       LevelInfo.sound_pstop),
   LI_STRING("sound-bdcls",     SOUNDBDCLS,       LevelInfo.sound_bdcls),
   LI_STRING("sound-bdopn",     SOUNDBDOPN,       LevelInfo.sound_bdopn),
   LI_STRING("sound-dorcls",    SOUNDDORCLS,      LevelInfo.sound_dorcls),
   LI_STRING("sound-doropn",    SOUNDDOROPN,      LevelInfo.sound_doropn),
   LI_STRING("sound-pstart",    SOUNDPSTART,      LevelInfo.sound_pstart),
   LI_STRING("sound-fcmove",    SOUNDFCMOVE,      LevelInfo.sound_fcmove),
   LI_BOOLNF("unevenlight",     UNEVENLIGHT,      LevelInfo.unevenLight),

   //{ IVT_STRING,  "defaultweapons", &info_weapons },
   
   LI_END() // must be last
};

// lexer state enumeration for P_ParseLevelVar
enum
{
   STATE_VAR,
   STATE_BETWEEN,
   STATE_VAL
};

//
// P_ParseLevelVar
//
// Tokenizes the line parsed by P_ParseInfoCmd and then sets
// any appropriate matching MapInfo variable to the retrieved
// value.
//
static void P_ParseLevelVar(qstring_t *cmd)
{
   int state = 0;
   qstring_t var, value;
   char c;
   const char *rover = cmd->buffer;
   levelvar_t *current = levelvars;

   // haleyjd 03/12/05: seriously restructured to remove possible
   // overflow of static buffer and bad kludges used to separate
   // the variable and value tokens -- now uses qstring_t.

   // create qstrings to hold the tokens
   M_QStrInitCreate(&var);
   M_QStrInitCreate(&value);

   while((c = *rover++))
   {
      switch(state)
      {
      case STATE_VAR: // in variable -- read until whitespace or = is hit
         if(c == ' ' || c == '=')
            state = STATE_BETWEEN;
         else
            M_QStrPutc(&var, c);
         continue;
      case STATE_BETWEEN: // between -- skip whitespace or =
         if(c != ' ' && c != '=')
         {
            state = STATE_VAL;
            M_QStrPutc(&value, c);
         }
         continue;
      case STATE_VAL: // value -- everything else goes here
         M_QStrPutc(&value, c);
         continue;
      default:
         I_Error("P_ParseLevelVar: undefined state in lexer\n");
      }
   }

   // detect some syntax errors
   if(state != STATE_VAL || !M_QStrLen(&var) || !M_QStrLen(&value))
   {
      M_QStrFree(&var);
      M_QStrFree(&value);
      return;
   }

   // TODO: improve linear search? fixed small set, so may not matter
   while(current->type != IVT_END)
   {
      if(!M_QStrCmp(&var, current->name))
      {
         switch(current->type)
         {
         case IVT_STRING:
            *(char**)current->variable = M_QStrCDup(&value, PU_LEVEL);
            break;

            // haleyjd 10/05/05: named value support
         case IVT_STRNUM:
            {
               textvals_t *tv = (textvals_t *)current->extra;
               int val = E_StrToNumLinear(tv->vals, tv->numvals, value.buffer);

               if(val >= tv->numvals)
                  val = tv->defaultval;

               *(int *)current->variable = val;
            }
            break;
            
         case IVT_INT:
            *(int*)current->variable = M_QStrAtoi(&value);
            break;
            
            // haleyjd 03/15/03: boolean support
         case IVT_BOOLEAN:
            *(boolean *)current->variable = 
               !M_QStrCaseCmp(&value, "true") ? true : false;
            break;

            // haleyjd 03/14/05: flags support
         case IVT_FLAGS:
            {
               dehflagset_t *flagset = (dehflagset_t *)current->extra;
               
               *(int *)current->variable = E_ParseFlags(value.buffer, flagset);
            }
            break;
         default:
            I_Error("P_ParseLevelVar: unknown level variable type\n");
         }
      }
      current++;
   }

   // free the qstrings
   M_QStrFree(&var);
   M_QStrFree(&value);
}

//
// Default Setup and Post-Processing Routines
//

// automap name macros (moved from hu_stuff.c)

#define HU_TITLE  (mapnames[(gameepisode-1)*9+gamemap-1])
#define HU_TITLE2 (mapnames2[gamemap-1])
#define HU_TITLEP (mapnamesp[gamemap-1])
#define HU_TITLET (mapnamest[gamemap-1])
#define HU_TITLEH (mapnamesh[(gameepisode-1)*9+gamemap-1])

//
// SynthLevelName
//
// Makes up a level name for new maps and Heretic secrets.
// Moved here from hu_stuff.c
//
// secret == true  -> Heretic hidden map
// secret == false -> Just a plain new level
//
static void SynthLevelName(boolean secret)
{
   // haleyjd 12/14/01: halved size of this string, max length
   // is deterministic since gamemapname is 8 chars long
   static char newlevelstr[25];

   sprintf(newlevelstr, 
           secret ? "%s: hidden level" : "%s: new level", 
           gamemapname);
   
   LevelInfo.levelName = newlevelstr;
}

//
// P_InfoDefaultLevelName
//
// Figures out the name to use for this map.
// Moved here from hu_stuff.c
//
static void P_InfoDefaultLevelName(void)
{
   const char *bexname  = NULL;
   boolean deh_modified = false;
   boolean synth_type   = false;
   missioninfo_t *missionInfo = GameModeInfo->missionInfo;

   // if we have a current metainfo, use its level name
   if(curmetainfo)
   {
      LevelInfo.levelName = curmetainfo->levelname;
      return;
   }

   if(isMAPxy(gamemapname) && gamemap > 0 && gamemap <= 32)
   {
      // DOOM II
      bexname = missionInfo->id == pack_tnt  ? HU_TITLET :
                missionInfo->id == pack_plut ? HU_TITLEP : HU_TITLE2;
   }
   else if(isExMy(gamemapname) &&
           gameepisode > 0 && gameepisode <= GameModeInfo->numEpisodes &&
           gamemap > 0 && gamemap <= 9)
   {
      if(GameModeInfo->type == Game_Heretic) // Heretic
      {
         int maxEpisode = GameModeInfo->numEpisodes;
         
         // For Heretic, the last episode isn't "real" and contains
         // "secret" levels -- a name is synthesized for those
         // 10/10/05: don't catch shareware here
         if(maxEpisode == 1 || gameepisode < maxEpisode)
            bexname = HU_TITLEH;            
         else
            synth_type = true; // put "hidden level"
      }
      else // DOOM
         bexname = HU_TITLE;         
   }

   if(bexname)
      deh_modified = DEH_StringChanged(bexname);

   if((newlevel && !deh_modified) || !bexname)
      SynthLevelName(synth_type);
   else
      LevelInfo.levelName = DEH_String(bexname);
}

#define NUMMAPINFOSOUNDS 10

static char *DefSoundNames[NUMMAPINFOSOUNDS] =
{
   "EE_DoorOpen",
   "EE_DoorClose",
   "EE_BDoorOpen",
   "EE_BDoorClose",
   "EE_SwitchOn",
   "EE_SwitchEx",
   "EE_PlatStart",
   "EE_PlatStop",
   "EE_PlatMove",
   "EE_FCMove",
};

static sfxinfo_t *DefSoundAliases[NUMMAPINFOSOUNDS][2];

// Yes, an array of pointers-to-pointers. Your eyes do not deceive you.
// It's just easiest to do it this way.

static const char **infoSoundPtrs[NUMMAPINFOSOUNDS] =
{
   &LevelInfo.sound_doropn,
   &LevelInfo.sound_dorcls,
   &LevelInfo.sound_bdopn,
   &LevelInfo.sound_bdcls,
   &LevelInfo.sound_swtchn,
   &LevelInfo.sound_swtchx,
   &LevelInfo.sound_pstart,
   &LevelInfo.sound_pstop,
   &LevelInfo.sound_stnmov,
   &LevelInfo.sound_fcmove,
};

//
// P_InfoDefaultSoundNames
//
// Restores the alias fields of the sounds used by the sound sequence engine
// for default effects.
//
static void P_InfoDefaultSoundNames(void)
{
   static boolean firsttime = true;
   int i;

   // if first time, save pointers to the sounds and their aliases
   if(firsttime)
   {
      firsttime = false;

      for(i = 0; i < NUMMAPINFOSOUNDS; ++i)
      {
         sfxinfo_t *sfx = E_SoundForName(DefSoundNames[i]);

         DefSoundAliases[i][0] = sfx;
         DefSoundAliases[i][1] = sfx ? sfx->alias : NULL;
      }
   }
   else
   {
      // restore defaults
      for(i = 0; i < NUMMAPINFOSOUNDS; ++i)
      {
         if(DefSoundAliases[i][0])
            DefSoundAliases[i][0]->alias = DefSoundAliases[i][1];
      }
   }

   // set sound names to defaults
   for(i = 0; i < NUMMAPINFOSOUNDS; ++i)
   {
      if(DefSoundAliases[i][0] && DefSoundAliases[i][0]->alias)
         *infoSoundPtrs[i] = DefSoundAliases[i][0]->alias->mnemonic;
      else
         *infoSoundPtrs[i] = "none";
   }
}

//
// P_SetInfoSoundNames
//
// Post-processing routine.
//
// Changes aliases in sounds used by the sound sequence engine for the default
// sequences to point at the sounds defined by MapInfo.
//
static void P_SetInfoSoundNames(void)
{
   int i;

   for(i = 0; i < NUMMAPINFOSOUNDS; ++i)
   {
      const char *name = *infoSoundPtrs[i];

      if(DefSoundAliases[i][0])
         DefSoundAliases[i][0]->alias = E_SoundForName(name);
   }
}

//
// P_LoadInterTextLump
//
// Post-processing routine.
//
// If the intertext lump name was set via MapInfo, this loads the
// lump and sets LevelInfo.interText to it.
// Moved here from f_finale.c
//
static void P_LoadInterTextLump(void)
{
   if(LevelInfo.interTextLump)
   {
      int lumpNum, lumpLen;
      char *str;
            
      lumpNum = W_GetNumForName(LevelInfo.interTextLump);
      lumpLen = W_LumpLength(lumpNum);
      
      str = Z_Malloc(lumpLen + 1, PU_LEVEL, 0);
      
      W_ReadLump(lumpNum, str);
      
      // null-terminate the string
      str[lumpLen] = '\0';

      LevelInfo.interText = str;
   }
}

//
// P_InfoDefaultFinale
//
// Sets up default MapInfo values related to f_finale.c code.
// Moved here from f_finale.c and altered for new features, etc.
//
// haleyjd: rewritten 07/03/09 to tablify info through GameModeInfo.
// haleyjd: merged finaleType here 01/09/10
//
static void P_InfoDefaultFinale(void)
{
   finaledata_t *fdata   = GameModeInfo->finaleData;
   finalerule_t *rule    = fdata->rules;
   finalerule_t *theRule = NULL;

   // universal defaults
   LevelInfo.interTextLump    = NULL;
   LevelInfo.finaleSecretOnly = false;
   LevelInfo.killFinale       = false;
   LevelInfo.killStats        = false;
   LevelInfo.endOfGame        = false;
   LevelInfo.useEDFInterName  = false;

   // set data from the finaledata object
   if(fdata->musicnum != mus_None)
      LevelInfo.interMusic = (GameModeInfo->s_music[fdata->musicnum]).name;
   else
      LevelInfo.interMusic = NULL;

   // check killStats flag - a bit of a hack, this is for Heretic's "hidden"
   // levels in episode 6, which have no statistics intermission.
   if(fdata->killStatsHack)
   {
      if(!(GameModeInfo->flags & GIF_SHAREWARE) && 
         gameepisode >= GameModeInfo->numEpisodes)
         LevelInfo.killStats = true;
   }

   // look for a rule that applies to this level in particular
   for(; rule->gameepisode != -2; ++rule)
   {
      if((rule->gameepisode == -1 || rule->gameepisode == gameepisode) &&
         (rule->gamemap == -1 || rule->gamemap == gamemap))
      {
         theRule = rule;
         break;
      }
   }

   if(theRule)
   {
      // set backdrop graphic, intertext, and finale type
      LevelInfo.backDrop   = DEH_String(rule->backDrop);
      LevelInfo.interText  = DEH_String(rule->interText);
      LevelInfo.finaleType = rule->finaleType;

      // check for endOfGame flag
      if(rule->endOfGame)
         LevelInfo.endOfGame = true;

      // check for secretOnly flag
      if(rule->secretOnly)
         LevelInfo.finaleSecretOnly = true;

      // allow metainfo overrides
      if(curmetainfo)
      {
         if(curmetainfo->finale)
         {
            LevelInfo.interText = curmetainfo->intertext;
            LevelInfo.endOfGame = true;
         }
         else
            LevelInfo.interText = NULL; // disable other levels
      }
   }
   else
   {
      // This shouldn't really happen but I'll be proactive about it.
      // The finale data sets all have rules to cover defaulted maps.

      // haleyjd: note -- this cannot use F_SKY1 like it did in BOOM
      // because heretic.wad contains an invalid F_SKY1 flat. This 
      // caused crashes during development of Heretic support, so now
      // it uses the F_SKY2 flat which is provided in eternity.wad.
      LevelInfo.backDrop   = "F_SKY2";
      LevelInfo.interText  = NULL;
      LevelInfo.finaleType = FINALE_TEXT;
   }
}


//
// P_InfoDefaultSky
//
// Sets the default sky texture for the level.
// Moved here from r_sky.c
//
static void P_InfoDefaultSky(void)
{
   skydata_t *sd      = GameModeInfo->skyData;
   skyrule_t *rule    = sd->rules;
   skyrule_t *theRule = NULL;

   // DOOM determines the sky texture to be used
   // depending on the current episode, and the game version.
   // haleyjd 01/07/10: use GameModeInfo ruleset

   for(; rule->data != -2; ++rule)
   {
      switch(sd->testtype)
      {
      case GI_SKY_IFEPISODE:
         if(rule->data == gameepisode || rule->data == -1)
            theRule = rule;
         break;
      case GI_SKY_IFMAPLESSTHAN:
         if(gamemap < rule->data || rule->data == -1)
            theRule = rule;
         break;
      default:
         I_Error("P_InfoDefaultSky: bad microcode %d in skyrule set\n", sd->testtype);
      }

      if(theRule) // break out if we've found one
         break;
   }

   // we MUST have a default sky rule
   if(theRule)
      LevelInfo.skyName = theRule->skyTexture;
   else
      I_Error("P_InfoDefaultSky: no default rule in skyrule set\n");

   // set sky2Name to NULL now, we'll test it later
   LevelInfo.sky2Name = NULL;
   LevelInfo.doubleSky = false; // double skies off by default

   // sky deltas for Hexen-style double skies - default to 0
   LevelInfo.skyDelta  = 0;
   LevelInfo.sky2Delta = 0;

   // altSkyName -- this is used for lightning flashes --
   // starts out NULL to indicate none.
   LevelInfo.altSkyName = NULL;
}

//
// P_InfoDefaultBossSpecials
//
// haleyjd 03/14/05: Sets the default boss specials for DOOM, DOOM II,
// and Heretic. These are the actions triggered for various maps by the
// BossDeath and HticBossDeath codepointers. They can now be enabled for
// any map by using the boss-specials variable.
//
static void P_InfoDefaultBossSpecials(void)
{
   bspecrule_t *rule = GameModeInfo->bossRules;

   // most maps have no specials, so set to zero here
   LevelInfo.bossSpecs = 0;

   // look for a level-specific default
   for(; rule->episode != -1; ++rule)
   {
      if(gameepisode == rule->episode && gamemap == rule->map)
      {
         LevelInfo.bossSpecs = rule->flags;
         return;
      }
   }
}

//
// P_SetSky2Texture
//
// Post-processing routine.
//
// Sets the sky2Name, if it is still NULL, to whatever 
// skyName has ended up as. This may be the default, or 
// the value from MapInfo.
//
static void P_SetSky2Texture(void)
{
   if(!LevelInfo.sky2Name)
      LevelInfo.sky2Name = LevelInfo.skyName;
}

//
// P_SetParTime
//
// Post-processing routine.
//
// Sets the map's par time, allowing for DeHackEd replacement and
// status as a "new" level. Moved here from g_game.c
//
static void P_SetParTime(void)
{
   if(LevelInfo.partime == -1) // if not set via MapInfo already
   {
      if(!newlevel || deh_pars) // if not a new map, OR deh pars loaded
      {
         switch(GameModeInfo->id)
         {
         case shareware:
         case retail:
         case registered:
            if(gameepisode >= 1 && gameepisode <= 3 &&
               gamemap >= 1 && gamemap <= 9)
               LevelInfo.partime = TICRATE * pars[gameepisode][gamemap];
            break;
         case commercial:
            if(gamemap >= 1 && gamemap <= 34)
               LevelInfo.partime = TICRATE * cpars[gamemap - 1];
            break;
         case hereticsw:
         case hereticreg:
         default:
            LevelInfo.partime = 0; // no par times in Heretic
            break;
         }
      }
   }
   else
      LevelInfo.partime *= TICRATE; // multiply MapInfo value by TICRATE
}

//
// P_SetOutdoorFog
//
// Post-processing routine
//
// If the global fog map has not been set, set it the same as the global
// colormap for the level.
//
static void P_SetOutdoorFog(void)
{
   if(LevelInfo.outdoorFog == NULL)
      LevelInfo.outdoorFog = LevelInfo.colorMap;
}

//
// P_ClearLevelVars
//
// Clears all the level variables so that none are left over from a
// previous level. Calls all the default construction functions
// defined above. Post-processing routines are called *after* the
// MapInfo processing is complete, and thus some of the values set
// here are used to detect whether or not MapInfo set a value.
//
static void P_ClearLevelVars(void)
{
   static char nextlevel[10];
   static char nextsecret[10];

   // find a metainfo for the level if one exists
   curmetainfo = P_GetMetaInfoForLevel(gamemap);

   // set default level type depending on game mode
   switch(GameModeInfo->type)
   {
   case Game_DOOM:
      LevelInfo.levelType = LI_TYPE_DOOM;
      break;
   case Game_Heretic:
      LevelInfo.levelType = LI_TYPE_HERETIC;
      break;
   }

   LevelInfo.levelPic        = NULL;
   LevelInfo.nextLevelPic    = NULL;
   LevelInfo.nextSecretPic   = NULL;
   LevelInfo.musicName       = curmetainfo ? curmetainfo->musname : "";
   LevelInfo.creator         = "unknown";
   LevelInfo.interPic        = GameModeInfo->interPic;
   LevelInfo.partime         = curmetainfo ? curmetainfo->partime : -1;

   LevelInfo.colorMap        = "COLORMAP";
   LevelInfo.outdoorFog      = NULL;
   LevelInfo.useFullBright   = true;
   LevelInfo.unevenLight     = true;
   
   LevelInfo.hasLightning    = false;
   LevelInfo.nextSecret      = "";
   //info_weapons            = "";
   LevelInfo.gravity         = DEFAULTGRAVITY;
   LevelInfo.hasScripts      = false;
   LevelInfo.scriptLump      = NULL;
   LevelInfo.extraData       = NULL;
   
   // Hexen TODO: will be true for Hexen maps by default
   LevelInfo.noAutoSequences = false;

   // haleyjd: construct defaults
   P_InfoDefaultLevelName();
   P_InfoDefaultSoundNames();
   P_InfoDefaultFinale();
   P_InfoDefaultSky();
   P_InfoDefaultBossSpecials();
   
   // special handling for ExMy maps under DOOM II
   if(GameModeInfo->id == commercial && isExMy(levelmapname))
   {
      LevelInfo.nextLevel = nextlevel;
      
      // set the next episode
      strcpy(nextlevel, levelmapname);
      nextlevel[3]++;
      if(nextlevel[3] > '9')  // next episode
      {
         nextlevel[3] = '1';
         nextlevel[1]++;
      }
      LevelInfo.musicName = levelmapname;
   }
   else
   {
      // allow metainfo override for nextlevel
      if(curmetainfo && curmetainfo->nextlevel)
      {
         memset(nextlevel, 0, sizeof(nextlevel));
         psnprintf(nextlevel, sizeof(nextlevel), "MAP%02d", curmetainfo->nextlevel);
         LevelInfo.nextLevel = nextlevel;
      }
      else
         LevelInfo.nextLevel = "";
   }

   // allow metainfo override for nextsecret
   if(curmetainfo && curmetainfo->nextsecret)
   {
      memset(nextsecret, 0, sizeof(nextsecret));
      psnprintf(nextsecret, sizeof(nextsecret), "MAP%02d", curmetainfo->nextsecret);
      LevelInfo.nextSecret = nextsecret;
   }
}

boolean default_weaponowned[NUMWEAPONS];

// haleyjd: note -- this is considered deprecated and is a
// candidate for replacement/rewrite
// WEAPON_FIXME: mapinfo weapons.

static void P_InitWeapons(void)
{
#if 0
   char *s;
#endif
   
   memset(default_weaponowned, 0, sizeof(default_weaponowned));
#if 0   
   s = info_weapons;
   
   while(*s)
   {
      switch(*s)
      {
      case '3': default_weaponowned[wp_shotgun] = true; break;
      case '4': default_weaponowned[wp_chaingun] = true; break;
      case '5': default_weaponowned[wp_missile] = true; break;
      case '6': default_weaponowned[wp_plasma] = true; break;
      case '7': default_weaponowned[wp_bfg] = true; break;
      case '8': default_weaponowned[wp_supershotgun] = true; break;
      default: break;
      }
      s++;
   }
#endif
}

//=============================================================================
//
// Meta Info
//
// Meta info is only loaded from diskfiles and may override LevelInfo defaults.
// 

//
// P_GetMetaInfoForLevel
//
// Finds a metainfo object for the given map number, if one exists (returns 
// NULL otherwise).
//
static metainfo_t *P_GetMetaInfoForLevel(int mapnum)
{
   int i;
   metainfo_t *mi = NULL;

   for(i = 0; i < nummetainfo; ++i)
   {
      if(metainfo[i].level == mapnum)
      {
         mi = &metainfo[i];
         break;
      }
   }

   return mi;
}

//
// P_CreateMetaInfo
//
// Creates a metainfo object for a given map with all of the various data that
// can be defined in metadata.txt files. This is called from some code in 
// d_main.c that deals with special ".disk" files that contain an IWAD and
// possible PWAD(s) that originate from certain console versions of DOOM.
//
void P_CreateMetaInfo(int map, const char *levelname, int par, const char *mus, 
                      int next, int secr, boolean finale, const char *intertext)
{
   metainfo_t *mi;

   if(nummetainfo >= nummetainfoalloc)
   {
      nummetainfoalloc = nummetainfoalloc ? nummetainfoalloc * 2 : 10;
      metainfo = realloc(metainfo, nummetainfoalloc * sizeof(metainfo_t));
   }

   mi = &metainfo[nummetainfo];

   mi->level      = map;
   mi->levelname  = levelname;
   mi->partime    = par;
   mi->musname    = mus;
   mi->nextlevel  = next;
   mi->nextsecret = secr;
   mi->finale     = finale;
   mi->intertext  = intertext;

   ++nummetainfo;
}

//=============================================================================
//
// Wad Template Parsing
//
// haleyjd 06/16/10: For situations where we can get information from a wad
// file's corresponding template text file.
//

//
// P_openWadTemplate
//
// Input:  The filepath to the wad file for the current level.
// Output: The buffered text from the corresponding wad template file, if the
//         file was found. NULL otherwise.
//
static char *P_openWadTemplate(const char *wadfile, int *len)
{
   char *fn = strdup(wadfile);
   char *dotloc = NULL;
   char *buffer = NULL;

   // find an extension if it has one, and see that it is ".wad"
   if((dotloc = strrchr(fn, '.')) && !strcasecmp(dotloc, ".wad"))
   {
      strcpy(dotloc, ".txt");    // try replacing .wad with .txt

      if(access(fn, R_OK))       // no?
      {
         strcpy(dotloc, ".TXT"); // try with .TXT (for You-neeks systems 9_9)
         if(access(fn, R_OK))
            return NULL;         // oh well, tough titties.
      }
   }

   return (*len = M_ReadFile(fn, &buffer)) < 0 ? NULL : buffer;
}

// template parsing states
enum
{
   TMPL_STATE_START,   // at beginning
   TMPL_STATE_QUOTE,   // saw a quote first off (for Sverre levels)
   TMPL_STATE_TITLE,   // reading in "Title" identifier
   TMPL_STATE_SPACE,   // skipping spaces and/or ':' characters
   TMPL_STATE_INTITLE, // parsing out the title
   TMPL_STATE_DONE     // finished successfully
};

typedef struct tmplpstate_s
{
   char *text;
   int i;
   int len;
   int state;
   qstring_t *tokenbuf;
   int spacecount;
   int titleOrAuthor; // if non-zero, looking for Author, not title
} tmplpstate_t;

static void TmplStateStart(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case '"':
      if(!state->titleOrAuthor) // only when looking for title...
      {
         state->spacecount = 0;
         state->state = TMPL_STATE_QUOTE; // this is a hack for Sverre levels
      }
      break;
   case 'T':
   case 't':
      if(state->titleOrAuthor)
         break;
      M_QStrPutc(state->tokenbuf, c);
      state->state = TMPL_STATE_TITLE; // start reading out "Title"
      break;
   case 'A':
   case 'a':
      if(state->titleOrAuthor)
      {
         M_QStrPutc(state->tokenbuf, c);
         state->state = TMPL_STATE_TITLE; // start reading out "Title"
      }
      break;
   default:
      break; // stay in same state
   }
}

static void TmplStateQuote(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case ' ':
      if(++state->spacecount == 2)
      {
         M_QStrPutc(state->tokenbuf, ' ');
         state->spacecount = 0;
      }
      break;
   case '\n':
   case '\r':
   case '"': // done
      state->state = TMPL_STATE_DONE;
      break;
   default:
      state->spacecount = 0;
      M_QStrPutc(state->tokenbuf, c);
      break;
   }
}

static void TmplStateTitle(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case ' ':
   case '\n':
   case '\r':
   case '\t':
   case ':':
      // whitespace - check to see if we have "Title" or "Author" in the token buffer
      if(!M_QStrCaseCmp(state->tokenbuf, state->titleOrAuthor ? "Author" : "Title"))
      {
         M_QStrClear(state->tokenbuf);
         state->state = TMPL_STATE_SPACE;
      }
      else
      {
         M_QStrClear(state->tokenbuf);
         state->state = TMPL_STATE_START; // start over
      }
      break;
   default:
      M_QStrPutc(state->tokenbuf, c);
      break;
   }
}

static void TmplStateSpace(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case ' ':
   case ':':
   case '\n':
   case '\r':
   case '\t':
   case '"':
      break; // stay in same state
   default:
      // should be start of title
      M_QStrPutc(state->tokenbuf, c);
      state->state = TMPL_STATE_INTITLE;
      break;
   }
}

static void TmplStateInTitle(tmplpstate_t *state)
{
   char c = state->text[state->i];

   switch(c)
   {
   case '"':
      if(state->titleOrAuthor) // for authors, quotes are allowed
      {
         M_QStrPutc(state->tokenbuf, c);
         break;
      }
      // intentional fall-through for titles (end of title)
   case '\n':
   case '\r':
      // done
      state->state = TMPL_STATE_DONE;
      break;
   default:
      M_QStrPutc(state->tokenbuf, c);
      break;
   }
}

typedef void (*tmplstatefunc_t)(tmplpstate_t *state);

// state functions
static tmplstatefunc_t statefuncs[] =
{
   TmplStateStart,  // TMPL_STATE_START
   TmplStateQuote,  // TMPL_STATE_QUOTE
   TmplStateTitle,  // TMPL_STATE_TITLE
   TmplStateSpace,  // TMPL_STATE_SPACE
   TmplStateInTitle // TMPL_STATE_INTITLE
};

//
// P_findTextInTemplate
//
// haleyjd 06/16/10: Find the title or author of the wad in the template.
//
static char *P_findTextInTemplate(char *text, int len, int titleOrAuthor)
{
   tmplpstate_t state;
   qstring_t tokenbuffer;
   char *ret = NULL;

   M_QStrInitCreate(&tokenbuffer);

   state.text          = text;
   state.len           = len;
   state.i             = 0;
   state.state         = TMPL_STATE_START;
   state.tokenbuf      = &tokenbuffer;
   state.titleOrAuthor = titleOrAuthor;

   while(state.i != len && state.state != TMPL_STATE_DONE)
   {
      statefuncs[state.state](&state);
      state.i++;
   }

   // valid termination states are DONE or INTITLE (though it's pretty unlikely
   // that we would hit EOF in the title string, I'll allow for it)
   if(state.state == TMPL_STATE_DONE || state.state == TMPL_STATE_INTITLE)
      ret = M_QStrCDup(&tokenbuffer, PU_LEVEL);

   M_QStrFree(&tokenbuffer);

   return ret;
}

// EOF


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
#include "e_lib.h"

extern char gamemapname[9];

// haleyjd: moved everything into the LevelInfo struct

LevelInfo_t LevelInfo;

static void P_ParseLevelInfo(int lumpnum);

static void P_StripSpaces(char *line);
static void P_RemoveEqualses(char *line);
static void P_CleanLine(char *line);

static int  P_ParseInfoCmd(char *line);
static void P_ParseLevelVar(char *cmd);

static void P_ClearLevelVars(void);
static void P_InitWeapons(void);

// post-processing routine prototypes
static void P_LoadInterTextLump(void);
static void P_SetSky2Texture(void);
static void P_SetParTime(void);
static void P_SetInfoSoundNames(void);
static void P_SetOutdoorFog(void);

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

typedef struct
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
void P_LoadLevelInfo(int lumpnum)
{
   lumpinfo_t *lump;
   int glumpnum;

   // set all the level defaults
   P_ClearLevelVars();

   // parse global lumps
   limode = LI_MODE_GLOBAL;
   foundGlobalMap = false;

   // run down the hash chain for EMAPINFO
   lump = lumpinfo[W_LumpNameHash("EMAPINFO") % (unsigned)numlumps];   
   
   for(glumpnum = lump->index; glumpnum >= 0; glumpnum = lump->next)
   {
      lump = lumpinfo[glumpnum];

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
   if(!(size = lumpinfo[lumpnum]->size))
      return;

   // allocate lump buffer with size + 2 to allow for termination
   size += 2;
   lump = Z_Malloc(size, PU_STATIC, NULL);
   W_ReadLump(lumpnum, lump);

   // terminate lump data with a line break and null character;
   // this makes uniform parsing much easier
   lump[size] = '\n';
   lump[size + 1] = '\0';

   rover = lump;

   // create the line buffer
   M_QStrInitCreate(&line);
   
   while(*rover)
   {
      if(*rover == '\n') // end of line
      {
         // hack for global MapInfo: if P_ParseInfoCmd returns -1,
         // we can break out of parsing early
         if(P_ParseInfoCmd(M_QStrBuffer(&line)) == -1)
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
static int P_ParseInfoCmd(char *line)
{
   P_CleanLine(line);
   P_StripSpaces(line);
   M_Strlwr(line);
   
   while(*line == ' ') 
      line++;
   
   if(!*line) 
      return 0;
   
   if((line[0] == '/' && line[1] == '/') ||     // comment
       line[0] == '#' || line[0] == ';') 
      return 0;
  
   if(*line == '[')                // a new section seperator
   {
      ++line;

      if(limode == LI_MODE_GLOBAL)
      {
         // when in global mode, returning -1 will make
         // P_ParseLevelInfo break out early, saving time
         if(foundGlobalMap)
            return -1;

         if(!strncasecmp(line, gamemapname, strlen(gamemapname)))
         {
            foundGlobalMap = true;
            readtype = RT_LEVELINFO;
         }
      }
      else
      {
         if(!strncmp(line, "level info", 10))
            readtype = RT_LEVELINFO;
      }
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
//  '=' sign is optional: all equals signs are internally turned to spaces
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

typedef struct
{
   int type;
   char *name;
   void *variable;
   void *extra;
} levelvar_t;

levelvar_t levelvars[]=
{
   { IVT_STRING,  "altskyname",      (void *)&LevelInfo.altSkyName },
   { IVT_FLAGS,   "boss-specials",   &LevelInfo.bossSpecs,  &boss_flagset }, // haleyjd 03/14/05
   { IVT_STRING,  "colormap",        (void *)&LevelInfo.colorMap },
   { IVT_STRING,  "creator",         (void *)&LevelInfo.creator },
   { IVT_BOOLEAN, "doublesky",       &LevelInfo.doubleSky },
   { IVT_BOOLEAN, "edf-intername",   &LevelInfo.useEDFInterName },
   { IVT_BOOLEAN, "endofgame",       &LevelInfo.endOfGame },
   { IVT_STRING,  "extradata",       &LevelInfo.extraData },     // haleyjd 04/02/03
   { IVT_BOOLEAN, "finale-secret",   &LevelInfo.finaleSecretOnly },
   { IVT_STRNUM,  "finaletype",      &LevelInfo.finaleType, &finaleTypeVals },
   { IVT_BOOLEAN, "fullbright",      &LevelInfo.useFullBright },
   { IVT_INT,     "gravity",         &LevelInfo.gravity },
   { IVT_STRING,  "inter-backdrop",  (void *)&LevelInfo.backDrop },
   { IVT_STRING,  "intermusic",      (void *)&LevelInfo.interMusic },
   { IVT_STRING,  "interpic",        (void *)&LevelInfo.interPic },
   { IVT_STRING,  "intertext",       (void *)&LevelInfo.interTextLump }, // haleyjd 12/13/01
   { IVT_BOOLEAN, "killfinale",      &LevelInfo.killFinale },
   { IVT_BOOLEAN, "killstats",       &LevelInfo.killStats },     // haleyjd 03/24/05
   { IVT_STRING,  "levelname",       (void *)&LevelInfo.levelName },
   { IVT_STRING,  "levelpic",        (void *)&LevelInfo.levelPic },
   { IVT_STRING,  "levelpicnext",    (void *)&LevelInfo.nextLevelPic },
   { IVT_STRING,  "levelpicsecret",  (void *)&LevelInfo.nextSecretPic },
   { IVT_STRING,  "levelscript",     (void *)&LevelInfo.scriptLump },    // haleyjd
   { IVT_STRNUM,  "leveltype",       (void *)&LevelInfo.levelType,  &levelTypeVals  },
   { IVT_BOOLEAN, "lightning",       &LevelInfo.hasLightning },
   { IVT_STRING,  "music",           (void *)&LevelInfo.musicName },
   { IVT_STRING,  "nextlevel",       (void *)&LevelInfo.nextLevel },
   { IVT_STRING,  "nextsecret",      (void *)&LevelInfo.nextSecret },
   { IVT_BOOLEAN, "noautosequences", &LevelInfo.noAutoSequences }, // haleyjd 09/24/06
   { IVT_STRING,  "outdoorfog",      (void *)&LevelInfo.outdoorFog }, // haleyjd 03/04/07
   { IVT_INT,     "partime",         &LevelInfo.partime },
   { IVT_INT,     "skydelta",        &LevelInfo.skyDelta },
   { IVT_INT,     "sky2delta",       &LevelInfo.sky2Delta },
   { IVT_STRING,  "skyname",         (void *)&LevelInfo.skyName },
   { IVT_STRING,  "sky2name",        (void *)&LevelInfo.sky2Name },
   { IVT_STRING,  "sound-swtchn",    (void *)&LevelInfo.sound_swtchn },
   { IVT_STRING,  "sound-swtchx",    (void *)&LevelInfo.sound_swtchx },
   { IVT_STRING,  "sound-stnmov",    (void *)&LevelInfo.sound_stnmov },
   { IVT_STRING,  "sound-pstop",     (void *)&LevelInfo.sound_pstop },
   { IVT_STRING,  "sound-bdcls",     (void *)&LevelInfo.sound_bdcls },
   { IVT_STRING,  "sound-bdopn",     (void *)&LevelInfo.sound_bdopn },
   { IVT_STRING,  "sound-dorcls",    (void *)&LevelInfo.sound_dorcls },
   { IVT_STRING,  "sound-doropn",    (void *)&LevelInfo.sound_doropn },
   { IVT_STRING,  "sound-pstart",    (void *)&LevelInfo.sound_pstart },
   { IVT_STRING,  "sound-fcmove",    (void *)&LevelInfo.sound_fcmove },
   { IVT_BOOLEAN, "unevenlight",     (void *)&LevelInfo.unevenLight },

   //{ IVT_STRING,  "defaultweapons", &info_weapons },
   
   { IVT_END,     0,                0 } // must be last
};

//
// P_ParseLevelVar
//
// Tokenizes the line parsed by P_ParseInfoCmd and then sets
// any appropriate matching MapInfo variable to the retrieved
// value.
//
static void P_ParseLevelVar(char *cmd)
{
   int state = 0;
   qstring_t var, value;
   char c, *varname, *equals, *rover = cmd;
   levelvar_t *current = levelvars;

   // haleyjd 03/12/05: seriously restructured to remove possible
   // overflow of static buffer and bad kludges used to separate
   // the variable and value tokens -- now uses qstring_t.

   if(!*cmd)
      return;

   P_RemoveEqualses(cmd);

   // create qstrings to hold the tokens
   M_QStrInitCreate(&var);
   M_QStrInitCreate(&value);

   while((c = *rover++))
   {
      switch(state)
      {
      case 0: // beginning: variable -- read until whitespace is hit
         if(c == ' ')
            state = 1;
         else
            M_QStrPutc(&var, c);
         continue;
      case 1: // between: skip whitespace -- read until non-whitespace
         if(c != ' ')
         {
            state = 2;
            M_QStrPutc(&value, c);
         }
         continue;
      case 2: // end: value -- everything else goes into the value
         M_QStrPutc(&value, c);
         continue;
      }
   }

   // get pointers to qstring buffers to do some C lib stuff below
   varname = M_QStrBuffer(&var);
   equals  = M_QStrBuffer(&value);

   // TODO: improve linear search? fixed small set, so may not matter
   while(current->type != IVT_END)
   {
      if(!strcmp(current->name, varname))
      {
         switch(current->type)
         {
         case IVT_STRING:
            *(char**)current->variable = Z_Strdup(equals, PU_LEVEL, NULL);
            break;

            // haleyjd 10/05/05: named value support
         case IVT_STRNUM:
            {
               textvals_t *tv = (textvals_t *)current->extra;
               int val = E_StrToNumLinear(tv->vals, tv->numvals, equals);

               if(val >= tv->numvals)
                  val = tv->defaultval;

               *(int *)current->variable = val;
            }
            break;
            
         case IVT_INT:
            *(int*)current->variable = atoi(equals);
            break;
            
            // haleyjd 03/15/03: boolean support
         case IVT_BOOLEAN:
            *(boolean *)current->variable = 
               !strcasecmp(equals, "true") ? true : false;
            break;

            // haleyjd 03/14/05: flags support
         case IVT_FLAGS:
            {
               dehflagset_t *flagset = (dehflagset_t *)current->extra;
               
               *(long *)current->variable = E_ParseFlags(equals, flagset);
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
   const char *bexname = NULL;
   boolean deh_modified = false;
   boolean synth_type   = false;

   if(isMAPxy(gamemapname) && gamemap > 0 && gamemap <= 32)
   {
      // DOOM II
      bexname = gamemission == pack_tnt ? HU_TITLET :
                gamemission == pack_plut ? HU_TITLEP : HU_TITLE2;
   }
   else if(isExMy(gamemapname) &&
           gameepisode > 0 && gameepisode <= gameModeInfo->numEpisodes &&
           gamemap > 0 && gamemap <= 9)
   {
      if(gameModeInfo->type == Game_Heretic) // Heretic
      {
         int maxEpisode = gameModeInfo->numEpisodes;
         
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
// Note: The newer finale type is handled somewhat differently and is thus
// in a different function below.
//
static void P_InfoDefaultFinale(void)
{
   // set lump to NULL
   LevelInfo.interTextLump = NULL;
   LevelInfo.finaleSecretOnly = false;
   LevelInfo.killFinale = false;
   LevelInfo.killStats = false;
   LevelInfo.endOfGame = false;
   LevelInfo.useEDFInterName = false;

   switch(gamemode)
   {
   case shareware:
   case retail:
   case registered:
      // DOOM modes
      LevelInfo.interMusic = S_music[mus_victor].name;
      
      if((gameepisode >= 1 && gameepisode <= 4) && gamemap == 8)
      {
         switch(gameepisode)
         {
         case 1:
            LevelInfo.backDrop  = bgflatE1;
            LevelInfo.interText = DEH_String("E1TEXT");
            break;
         case 2:
            LevelInfo.backDrop  = bgflatE2;
            LevelInfo.interText = DEH_String("E2TEXT");
            break;
         case 3:
            LevelInfo.backDrop  = bgflatE3;
            LevelInfo.interText = DEH_String("E3TEXT");
            break;
         case 4:
            LevelInfo.backDrop  = bgflatE4;
            LevelInfo.interText = DEH_String("E4TEXT");
            break;
         }
      }
      else
      {
         LevelInfo.backDrop  = bgflatE1;
         LevelInfo.interText = NULL;
      }
      break;
   
   case commercial:
      // DOOM II modes
      LevelInfo.interMusic = S_music[mus_read_m].name;

      switch(gamemap)
      {
      case 6:
         LevelInfo.backDrop  = bgflat06;
         LevelInfo.interText = 
            gamemission == pack_tnt  ? DEH_String("T1TEXT") :
            gamemission == pack_plut ? DEH_String("P1TEXT") : 
                                       DEH_String("C1TEXT");
         break;
      case 11:
         LevelInfo.backDrop  = bgflat11;
         LevelInfo.interText =
            gamemission == pack_tnt  ? DEH_String("T2TEXT") :
            gamemission == pack_plut ? DEH_String("P2TEXT") : 
                                       DEH_String("C2TEXT");
         break;
      case 20:
         LevelInfo.backDrop  = bgflat20;
         LevelInfo.interText =
            gamemission == pack_tnt  ? DEH_String("T3TEXT") :
            gamemission == pack_plut ? DEH_String("P3TEXT") : 
                                       DEH_String("C3TEXT");
         break;
      case 30:
         LevelInfo.backDrop  = bgflat30;
         LevelInfo.interText =
            gamemission == pack_tnt  ? DEH_String("T4TEXT") :
            gamemission == pack_plut ? DEH_String("P4TEXT") : 
                                       DEH_String("C4TEXT");
         LevelInfo.endOfGame = true;
         break;
      case 15:
         LevelInfo.backDrop  = bgflat15;
         LevelInfo.interText =
            gamemission == pack_tnt  ? DEH_String("T5TEXT") :
            gamemission == pack_plut ? DEH_String("P5TEXT") : 
                                       DEH_String("C5TEXT");
         LevelInfo.finaleSecretOnly = true; // only after secret exit
         break;
      case 31:
         LevelInfo.backDrop  = bgflat31;
         LevelInfo.interText =
            gamemission == pack_tnt  ? DEH_String("T6TEXT") :
            gamemission == pack_plut ? DEH_String("P6TEXT") : 
                                       DEH_String("C6TEXT");
         LevelInfo.finaleSecretOnly = true; // only after secret exit
         break;
      default:
         LevelInfo.backDrop  = bgflat06;
         LevelInfo.interText = NULL;
         break;
      }
      break;

   case hereticsw:
   case hereticreg:
      // Heretic modes
      LevelInfo.interMusic = H_music[hmus_cptd].name;

      // "hidden" levels have no statistics intermission
      // 10/10/05: do not catch shareware here
      if(gamemode != hereticsw &&
         gameepisode >= gameModeInfo->numEpisodes)
         LevelInfo.killStats = true;

      if((gameepisode >= 1 && gameepisode <= 5) && gamemap == 8)
      {
         switch(gameepisode)
         {
         case 1:
            LevelInfo.backDrop  = bgflathE1;
            LevelInfo.interText = DEH_String("H1TEXT");
            break;
         case 2:
            LevelInfo.backDrop  = bgflathE2;
            LevelInfo.interText = DEH_String("H2TEXT");
            break;
         case 3:
            LevelInfo.backDrop  = bgflathE3;
            LevelInfo.interText = DEH_String("H3TEXT");
            break;
         case 4:
            LevelInfo.backDrop  = bgflathE4;
            LevelInfo.interText = DEH_String("H4TEXT");
            break;
         case 5:
            LevelInfo.backDrop  = bgflathE5;
            LevelInfo.interText = DEH_String("H5TEXT");
            break;
         }
      }
      else
      {
         LevelInfo.backDrop  = bgflathE1;
         LevelInfo.interText = NULL;
      }
      break;

   default:
      LevelInfo.interMusic = NULL;
      // haleyjd: note -- this cannot use F_SKY1 like it did in BOOM
      // because heretic.wad contains an invalid F_SKY1 flat. This 
      // caused crashes during development of Heretic support, so now
      // it uses the F_SKY2 flat which is provided in eternity.wad.
      LevelInfo.backDrop   = "F_SKY2";
      LevelInfo.interText  = NULL;
      break;
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
   // DOOM determines the sky texture to be used
   // depending on the current episode, and the game version.

   if(gamemode == commercial)
   {      
      if(gamemap < 12)
         LevelInfo.skyName = "SKY1";
      else if(gamemap < 21) 
         LevelInfo.skyName = "SKY2";
      else
         LevelInfo.skyName = "SKY3";
   }
   else if(gameModeInfo->type == Game_Heretic)
   {
      // haleyjd: heretic skies
      switch(gameepisode)
      {
      default: // haleyjd: episode 6, and default to avoid NULL
      case 1:
      case 4:
         LevelInfo.skyName = "SKY1";
         break;
      case 2:
         LevelInfo.skyName = "SKY2";
         break;
      case 3:
      case 5:
         LevelInfo.skyName = "SKY3";
         break;
      }
   }
   else //jff 3/27/98 and lets not forget about DOOM and Ultimate DOOM huh?
   {
      switch(gameepisode)
      {
      case 1:
         LevelInfo.skyName = "SKY1";
         break;
      case 2:
         LevelInfo.skyName = "SKY2";
         break;
      case 3:
         LevelInfo.skyName = "SKY3";
         break;
      case 4:  // Special Edition sky
      default: // haleyjd: don't let sky name end up NULL
         LevelInfo.skyName = "SKY4";
         break;
      }//jff 3/27/98 end sky setting fix
   }

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
   // most maps have no specials, so set to zero here
   LevelInfo.bossSpecs = 0;

   if(gamemode == commercial)
   {
      // DOOM II -- MAP07 has two specials
      if(gamemap == 7)
         LevelInfo.bossSpecs = BSPEC_MAP07_1 | BSPEC_MAP07_2;
   }
   else
   {
      // Ultimate DOOM and Heretic can use the same settings,
      // since the monsters use different codepointers.
      // E4M6 isn't triggered by HticBossDeath, and E5M8 isn't
      // triggered by BossDeath.
      if(gamemap == 8 || (gameepisode == 4 && gamemap == 6))
      {
         switch(gameepisode)
         {
         case 1:
            LevelInfo.bossSpecs = BSPEC_E1M8;
            break;
         case 2:
            LevelInfo.bossSpecs = BSPEC_E2M8;
            break;
         case 3:
            LevelInfo.bossSpecs = BSPEC_E3M8;
            break;
         case 4:
            switch(gamemap)
            {
            case 6:
               LevelInfo.bossSpecs = BSPEC_E4M6;
               break;
            case 8:
               LevelInfo.bossSpecs = BSPEC_E4M8;
               break;
            }
            break;
         case 5:
            LevelInfo.bossSpecs = BSPEC_E5M8;
            break;
         } // end switch
      } // end if
   } // end else
}

//
// P_InfoDefaultFinaleType
//
// haleyjd 05/26/06: sets the default finale type for concerned maps in
// DOOM, DOOM II, and Heretic.
//
static void P_InfoDefaultFinaleType(void)
{
   switch(gamemode)
   {
   case shareware:    // DOOM 1
   case registered:
   case retail:
      if(gamemap == 8)
      {
         switch(gameepisode)
         {
         default:
         case 1:
            LevelInfo.finaleType = FINALE_DOOM_CREDITS;
            break;
         case 2:
            LevelInfo.finaleType = FINALE_DOOM_DEIMOS;
            break;
         case 3:
            LevelInfo.finaleType = FINALE_DOOM_BUNNY;
            break;
         case 4:
            LevelInfo.finaleType = FINALE_DOOM_MARINE;
            break;
         }
      }
      else
         LevelInfo.finaleType = FINALE_TEXT; // for non-ExM8, default to just text
      break;
   case hereticsw:    // Heretic
   case hereticreg:
      if(gamemap == 8)
      {
         switch(gameepisode)
         {
         default: // note: includes E4, E5 for SoSR
         case 1:
            LevelInfo.finaleType = FINALE_HTIC_CREDITS;
            break;
         case 2:
            LevelInfo.finaleType = FINALE_HTIC_WATER;
            break;
         case 3:
            LevelInfo.finaleType = FINALE_HTIC_DEMON;
            break;
         }
      }
      else
         LevelInfo.finaleType = FINALE_TEXT;
      break;
   default:
      LevelInfo.finaleType = FINALE_TEXT; // default for most maps, inc. DOOM II
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
         switch(gamemode)
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
      LevelInfo.outdoorFog = "COLORMAP"; //LevelInfo.colorMap;
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
   // set default level type depending on game mode
   switch(gameModeInfo->type)
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
   LevelInfo.musicName       = "";
   LevelInfo.creator         = "unknown";
   LevelInfo.interPic        = "INTERPIC";
   LevelInfo.partime         = -1;

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
   P_InfoDefaultFinaleType();
   
   // special handling for ExMy maps under DOOM II
   if(gamemode == commercial && isExMy(levelmapname))
   {
      static char nextlevel[10];
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
      LevelInfo.nextLevel = "";
}

//
// Parsing Utility Functions
//

//
// P_CleanLine
//
// Gets rid of control characters such as \t or \n.
// 
static void P_CleanLine(char *line)
{
   char *temp;
   
   for(temp = line; *temp; ++temp)
      *temp = *temp < 32 ? ' ' : *temp;
}

//
// P_StripSpaces
//
// Strips whitespace from the end of the line.
//
static void P_StripSpaces(char *line)
{
   char *temp;
   
   temp = line + strlen(line) - 1;
   
   while(*temp == ' ')
   {
      *temp = 0;
      temp--;
   }
}

//
// P_RemoveEqualses
//
// Weirdly-named routine to turn optional = chars into spaces.
//
static void P_RemoveEqualses(char *line)
{
   char *temp;
   
   temp = line;
   
   while(*temp)
   {
      if(*temp == '=')
      {
         *temp = ' ';
      }
      temp++;
   }
}

boolean default_weaponowned[NUMWEAPONS];

// haleyjd: note -- this is considered deprecated and is a
// candidate for replacement/rewrite

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

// EOF


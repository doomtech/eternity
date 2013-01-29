// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2002 James Haley
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
// Gamemode information
//
// Stores all gamemode-dependent information in one location and 
// lends more structure to other modules.
//
// James Haley
//
//----------------------------------------------------------------------------

#ifndef D_GI_H__
#define D_GI_H__

#include "m_fixed.h"

struct default_or_t;
struct interfns_t;
struct menu_t;
struct musicinfo_t;
struct stbarfns_t;

// inspired by, but not taken from, zdoom

// Menu cursor -- a 2 patch alternating graphic
typedef struct gimenucursor_s
{
   const char *patch1;
   const char *patch2;
} gimenucursor_t;

// Screen border used to fill backscreen for small screen sizes
typedef struct giborder_s
{
   int offset;
   int size;
   const char *c_tl;
   const char *top;
   const char *c_tr;
   const char *left;
   const char *right;
   const char *c_bl;
   const char *bottom;
   const char *c_br;
} giborder_t;

typedef struct giftextpos_s
{
   int x, y;   // initial coordinates (for finale)
} giftextpos_t;

//
// enum for menu sounds
//
enum
{
   MN_SND_ACTIVATE,
   MN_SND_DEACTIVATE,
   MN_SND_KEYUPDOWN,
   MN_SND_COMMAND,
   MN_SND_PREVIOUS,
   MN_SND_KEYLEFTRIGHT,
   MN_SND_NUMSOUNDS,
};

// haleyjd 07/02/09: moved demostate_t to header for GameModeInfo

typedef void (*dsfunc_t)(const char *);

typedef struct demostate_s
{
   dsfunc_t func;
   const char *name;
} demostate_t;

extern const demostate_t demostates_doom[];
extern const demostate_t demostates_doom2[];
extern const demostate_t demostates_udoom[];
extern const demostate_t demostates_hsw[];
extern const demostate_t demostates_hreg[];

//
// Exit Rule Sets
//

typedef struct exitrule_s
{
   int gameepisode; // current episode (1 for games like DOOM 2, 
                    //   -1 if doesn't matter, -2 to terminate array)
   int gamemap;     // current map the game is on (1-based, -1 if doesn't matter)   
   int destmap;     // destination map (0-based for wminfo)
   bool isSecret;   // this rule applies for secret exits   
} exitrule_t;

//
// MapInfo-related Stuff
//

// Default finale data

typedef struct finalerule_s
{
   int gameepisode;       // episode where applies, or -1 to match all; -2 ends.
   int gamemap;           // map where applies, or -1 to match all
   const char *backDrop;  // BEX mnemonic of background graphic string
   const char *interText; // BEX mnemonic of intertext string
   int finaleType;        // transferred to LevelInfo.finaleType
   bool endOfGame;        // if true, LevelInfo.endOfGame is set
   bool secretOnly;       // if true, LevelInfo.finaleSecretOnly is set
} finalerule_t;

typedef struct finaledata_s
{
   int musicnum;        // index into gamemodeinfo_t::s_music
   bool killStatsHack;  // kill stats if !GIF_SHAREWARE && episode >= numEpisodes
   finalerule_t *rules; // rules array
} finaledata_t;

// Default sky data

// sky ops
enum
{
   GI_SKY_IFEPISODE,     // rules test episode #'s
   GI_SKY_IFMAPLESSTHAN  // rules test map #'s with <
};

typedef struct skyrule_s
{
   int data;               // data for rule; -1 == match any; -2 == end
   const char *skyTexture; // name of default sky texture
} skyrule_t;

typedef struct skydata_s
{
   int testtype;     // if tests maps or episodes
   skyrule_t *rules; // rules array
} skydata_t;

// Default boss specs

typedef struct bspecrule_s
{
   int episode;
   int map;
   unsigned int flags;
} bspecrule_t;

//
// Game Mode Flags
//
enum
{
   GIF_HASDISK        = 0x00000001, // has flashing io disk
   GIF_SHAREWARE      = 0x00000002, // shareware game (no -file)
   GIF_MNBIGFONT      = 0x00000004, // uses big font for menu titles
   GIF_MAPXY          = 0x00000008, // gamemode uses MAPxy maps by default
   GIF_SAVESOUND      = 0x00000010, // makes a sound in save & load menus
   GIF_HASADVISORY    = 0x00000020, // displays advisory popup on title screen
   GIF_SHADOWTITLES   = 0x00000040, // shadows titles in menus
   GIF_HASMADMELEE    = 0x00000080, // has mad melee when player dies in SP
   GIF_HASEXITSOUNDS  = 0x00000100, // has sounds at exit
   GIF_WOLFHACK       = 0x00000200, // is subject to German-edition restriction
   GIF_SETENDOFGAME   = 0x00000400, // Teleport_EndGame sets LevelInfo.endOfGame
   GIF_CLASSICMENUS   = 0x00000800, // supports classic/traditional menu emulation
   GIF_SKILL5RESPAWN  = 0x00001000, // monsters respawn by default on skill 5
   GIF_SKILL5WARNING  = 0x00002000, // display menu warning for skill 5
   GIF_HUDSTATBARNAME = 0x00004000, // HUD positions level name above status bar
   GIF_CENTERHUDMSG   = 0x00008000, // HUD messages are centered by default
};

// Game mode handling - identify IWAD version
//  to handle IWAD dependent animations etc.
typedef enum 
{
  shareware,    // DOOM 1 shareware, E1, M9
  registered,   // DOOM 1 registered, E3, M27
  commercial,   // DOOM 2 retail, E1, M34
  retail,       // DOOM 1 retail, E4, M36
  hereticsw,    // Heretic shareware
  hereticreg,   // Heretic full
  indetermined, // Incomplete or corrupted IWAD
  NumGameModes
} GameMode_t;

// Mission packs
typedef enum
{
  doom,         // DOOM 1
  doom2,        // DOOM 2
  pack_tnt,     // TNT mission pack
  pack_plut,    // Plutonia pack
  pack_disk,    // Disk version
  pack_hacx,    // HacX stand-alone IWAD
  heretic,      // Heretic
  hticsosr,     // Heretic - Shadow of the Serpent Riders
  none,
  NumGameMissions
} GameMission_t;

//
// Game Mode Types
//
// Some things have to vary only on the game-type being played, be it
// Doom or Heretic (or eventually, Hexen or Strife). These are directly
// tied to the state of the Enable system in EDF.
//
enum
{
   Game_DOOM,
   Game_Heretic,
   NumGameModeTypes
};

// haleyjd 10/05/05: level translation types
enum
{
   LI_TYPE_DOOM,
   LI_TYPE_HERETIC,
   LI_TYPE_HEXEN,
   LI_TYPE_STRIFE,
   LI_TYPE_NUMTYPES
};

// mission flags
enum
{
   MI_DEMOIFDEMO4  = 0x00000001, // use demoStates override iff DEMO4 exists
   MI_CONBACKTITLE = 0x00000002, // use console backdrop instead of titlepic
};

//
// missioninfo_t
//
// This structure holds mission-dependent information for the
// gamemodeinfo_t structure. The gamemode info structure must be
// assigned a missioninfo_t object during startup in d_main.c:IdentifyVersion.
//
typedef struct missioninfo_s
{
   GameMission_t  id;            // mission id - replaces "gamemission" variable
   unsigned int   flags;         // missioninfo flags
   const char    *gamePathName;  // name of base/game folder used for this mission
   
   // override data - information here overrides that contained in the
   // gamemodeinfo_t that uses this missioninfo object.

   const char   *versionNameOR;      // if not NULL, overrides name of the gamemode
   const char   *startupBannerOR;    // if not NULL, overrides the startup banner 
   int           numEpisodesOR;      // if not    0, overrides number of episodes
   char        **iwadPathOR;         // if not NULL, overrides iwadPath
   finaledata_t *finaleDataOR;       // if not NULL, overrides finaleData
   const char   *menuBackgroundOR;   // if not NULL, overrides menuBackground
   const char   *creditBackgroundOR; // if not NULL, overrides creditBackground
   const char   *consoleBackOR;      // if not NULL, overrides consoleBack
   const demostate_t *demoStatesOR;  // if not NULL, overrides demostates
   const char   *interPicOR;         // if not NULL, overrides interPic
   exitrule_t   *exitRulesOR;        // if not NULL, overrides exitRules
} missioninfo_t;

//
// gamemodeinfo_t
//
// This structure holds just about all the gamemode-pertinent
// information that could easily be pulled out of the rest of
// the source. This approach, as mentioned above, was inspired
// by zdoom, but I've taken it and really run with it.
//
typedef struct gamemodeinfo_s
{
   GameMode_t id;             // id      - replaces "gamemode" variable
   int type;                  // main game mode type: doom, heretic, etc.
   int flags;                 // game mode flags
   
   // startup stuff
   const char *versionName;       // descriptive version name
   const char *freeVerName;       // FreeDoom override name, if such exists
   const char *bfgEditionName;    // BFG Edition override name, if exists
   const char *startupBanner;     // startup banner text   
   char **iwadPath;               // iwad path variable
   
   // demo state information
   const demostate_t *demoStates; // demostates table
   int titleTics;                 // length of time to show title
   int advisorTics;               // for Heretic, len. to show advisory
   int pageTics;                  // length of general demo state pages
   int titleMusNum;               // music number to use for title

   // menu stuff
   const char *menuBackground;   // name of menu background flat
   const char *creditBackground; // name of dynamic credit bg flat
   int   creditY;                // y coord for credit text
   int   creditTitleStep;        // step-down from credit title
   gimenucursor_t *menuCursor;   // pointer to the big menu cursor
   menu_t *mainMenu;             // pointer to main menu structure
   menu_t *saveMenu;             // pointer to save menu structure
   menu_t *loadMenu;             // pointer to load menu structure
   menu_t *newGameMenu;          // pointer to new game menu structure
   int *menuSounds;              // menu sound indices
   int transFrame;               // frame DEH # used on video menu
   int skvAtkSound;              // skin viewer attack sound
   int unselectColor;            // color of unselected menu item text
   int selectColor;              // color of selected menu item text
   int variableColor;            // color of variable text
   int titleColor;               // color of title strings
   int infoColor;                // color of it_info items
   int menuOffset;               // an amount to subtract from menu y coords

   // border stuff
   const char *borderFlat;    // name of flat to fill backscreen
   giborder_t *border;        // pointer to a game border

   // HU / Video / Console stuff
   int defTextTrans;          // default text color rng for msgs
   int colorNormal;           // color # for normal messages
   int colorHigh;             // color # for highlighted messages
   int colorError;            // color # for error messages
   int c_numCharsPerLine;     // number of chars per line in console
   int c_BellSound;           // sound used for \a in console
   int c_ChatSound;           // sound used by say command
   const char *consoleBack;   // lump to use for default console backdrop
   unsigned char blackIndex;  // palette index for black {0,0,0}
   unsigned char whiteIndex;  // palette index for white {255,255,255}
   int numHUDKeys;            // number of keys to show in HUD

   // Status bar
   stbarfns_t *StatusBar;     // status bar function set

   // Automap
   const char *markNumFmt;    // automap mark number format string

   // Miscellaneous graphics
   const char *pausePatch;    // name of patch to show when paused

   // Game interaction stuff
   int numEpisodes;           // number of game episodes
   exitrule_t *exitRules;     // exit rule set
   int teleFogType;           // DeHackEd number of telefog object
   fixed_t teleFogHeight;     // amount to add to telefog z coord
   int teleSound;             // sound id for teleportation
   int16_t thrustFactor;      // damage thrust factor
   const char *defPClassName; // default playerclass name
   const char *defTranslate;  // default translation for AUTOTRANSLATE
   bspecrule_t *bossRules;    // default boss specials
   int levelType;             // level translation type

   // Intermission and Finale stuff
   const char *interPic;      // default intermission backdrop
   int interMusNum;           // intermission music number
   giftextpos_t *fTextPos;    // finale text info
   interfns_t *interfuncs;    // intermission function pointers
   int teleEndGameFinaleType; // Teleport_EndGame causes this finale by default
   finaledata_t *finaleData;  // Default finale data for MapInfo

   // Sound
   musicinfo_t *s_music;      // pointer to musicinfo_t (sounds.h)
   int (*MusicForMap)(void);  // pointer to S_MusicForMap* routine
   int musMin;                // smallest music index value (0)
   int numMusic;              // maximum music index value
   const char *musPrefix;     // "D_" for DOOM, "MUS_" for Heretic
   const char *defMusName;    // default music name
   const char *defSoundName;  // default sound if one is missing
   const char **skinSounds;   // default skin sound mnemonics array
   int *playerSounds;         // player sound dehnum indirection

   // Renderer stuff
   int switchEpisode;         // "episode" number for switch texture defs
   skydata_t *skyData;        // default sky data for MapInfo

   // Configuration
   default_or_t *defaultORs;  // default overrides for configuration file

   // Miscellaneous stuff
   const char *endTextName;   // name of end text resource
   int *exitSounds;           // exit sounds array, if GIF_HASEXITSOUNDS is set

   // Internal fields - these are set at runtime, so keep them last.
   missioninfo_t *missionInfo; // gamemission-dependent info

} gamemodeinfo_t;

extern missioninfo_t  *MissionInfoObjects[NumGameMissions];
extern gamemodeinfo_t *GameModeInfoObjects[NumGameModes];

extern gamemodeinfo_t *GameModeInfo;

// set by system config:
extern char *gi_path_doomsw;
extern char *gi_path_doomreg;
extern char *gi_path_doomu;
extern char *gi_path_doom2;
extern char *gi_path_bfgdoom2;
extern char *gi_path_tnt;
extern char *gi_path_plut;
extern char *gi_path_hacx;
extern char *gi_path_hticsw;
extern char *gi_path_hticreg;
extern char *gi_path_sosr;
extern char *gi_path_fdoom;
extern char *gi_path_fdoomu;
extern char *gi_path_freedm;


void D_SetGameModeInfo(GameMode_t, GameMission_t);
void D_InitGMIPostWads(void);

#endif

// EOF

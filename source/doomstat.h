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
//   All the global variables that store the internal state.
//   Theoretically speaking, the internal state of the engine
//    should be found by looking at the variables collected
//    here, and every relevant module will have to include
//    this header file.
//   In practice, things are a bit messy.
//
//-----------------------------------------------------------------------------

#ifndef D_STATE_H__
#define D_STATE_H__

// We need globally shared data structures,
//  for defining the global state variables.

// We need the player data structure as well.
#include "d_player.h"
#include "p_tick.h"
#include "tables.h"

struct doomcom_t;
struct doomdata_t;
struct mapthing_t;

typedef enum
{
  bfg_normal,
  bfg_classic,
  bfg_11k,
  bfg_bouncing, // haleyjd
  bfg_burst,    // haleyjd
} bfg_t;

extern int use_doom_config;

// ------------------------
// Command line parameters.
//

extern bool in_textmode;

extern bool nomonsters;  // checkparm of -nomonsters
extern bool respawnparm; // checkparm of -respawn
extern bool fastparm;    // checkparm of -fast
extern bool devparm;     // DEBUG: launched with -devparm

                // sf: screenblocks removed, replaced w/screenSize
extern  int screenSize;     // killough 11/98

// -----------------------------------------------------
// Game Mode - identify IWAD as shareware, retail etc.
//

//extern GameMode_t gamemode;
//extern GameMission_t  gamemission;

// Set if homebrew PWAD stuff has been added.
extern bool modifiedgame;

// compatibility with old engines (monster behavior, metrics, etc.)
extern int compatibility, default_compatibility;          // killough 1/31/98

extern int demo_version;           // killough 7/19/98: Version of demo
extern int demo_subversion;

// Only true when playing back an old demo -- used only in "corner cases"
// which break playback but are otherwise unnoticable or are just desirable:

#define demo_compatibility (demo_version < 200) /* killough 11/98: macroized */
#define ancient_demo       (demo_version < 5)   /* haleyjd  03/17: for old demos */

// haleyjd 10/16/10: full version macros
#define make_full_version(v, sv) ((v << 8) | sv)
#define full_demo_version        make_full_version(demo_version, demo_subversion)


// killough 7/19/98: whether monsters should fight against each other
extern int monster_infighting, default_monster_infighting;

extern int monkeys, default_monkeys;

// v1.1-like pitched sounds
extern int pitched_sounds;

extern int general_translucency;
extern int tran_filter_pct;
extern int demo_insurance, default_demo_insurance;      // killough 4/5/98

// haleyjd 04/13/04 -- macro to get a flex tran level roughly
// equivalent to the current tran_filter_pct value.
#define FTRANLEVEL ((FRACUNIT * tran_filter_pct) / 100)

// -------------------------------------------
// killough 10/98: compatibility vector

enum {
  comp_telefrag,
  comp_dropoff,
  comp_vile,
  comp_pain,
  comp_skull,
  comp_blazing,
  comp_doorlight,
  comp_model,
  comp_god,
  comp_falloff,
  comp_floors,
  comp_skymap,
  comp_pursuit,
  comp_doorstuck,
  comp_staylift,
  comp_zombie,
  comp_stairs,
  comp_infcheat,
  comp_zerotags,
  comp_terrain,     // haleyjd 07/04/99: TerrainTypes toggle (#19)
  comp_respawnfix,  // haleyjd 08/09/00: compat. option for nm respawn fix
  comp_fallingdmg,  //         08/09/00: falling damage
  comp_soul,        //         03/23/03: lost soul bounce
  comp_theights,    //         07/06/05: thing heights fix
  comp_overunder,   //         10/19/02: thing z clipping
  comp_planeshoot,  //         09/22/07: ability to shoot floor/ceiling
  comp_special,     //         08/29/09: special failure behavior
  comp_ninja,       //         04/18/10: ninja spawn in G_CheckSpot
  COMP_TOTAL=32  // Some extra room for additional variables
};

extern int comp[COMP_TOTAL], default_comp[COMP_TOTAL];

// -------------------------------------------
// Language.
extern  Language_t   language;

// -------------------------------------------
// Selected skill type, map etc.
//

// Defaults for menu, methinks.
extern  skill_t startskill;
extern  int     startepisode;
extern  int     startmap;

extern  bool    autostart;

// Selected by user.
extern  skill_t gameskill;
extern  int     gameepisode;
extern  int     gamemap;

// Nightmare mode flag, single player.
extern  bool    respawnmonsters;

// Netgame? Only true if >1 player.
extern  bool netgame;

// Flag: true only if started as net deathmatch.
// An enum might handle altdeath/cooperative better.
extern  bool deathmatch;

// ------------------------------------------
// Internal parameters for sound rendering.
// These have been taken from the DOS version,
//  but are not (yet) supported with Linux
//  (e.g. no sound volume adjustment with menu.

// These are not used, but should be (menu).
// From m_menu.c:
//  Sound FX volume has default, 0 - 15
//  Music volume has default, 0 - 15
// These are multiplied by 8.
extern int snd_SfxVolume;      // maximum volume for sound
extern int snd_MusicVolume;    // maximum volume for music

// Current music/sfx card - index useless
//  w/o a reference LUT in a sound module.
// Ideally, this would use indices found
//  in: /usr/include/linux/soundcard.h
extern int snd_MusicDevice;
extern int snd_SfxDevice;
// Config file? Same disclaimer as above.
extern int snd_DesiredMusicDevice;
extern int snd_DesiredSfxDevice;


// -------------------------
// Status flags for refresh.
//

// Depending on view size - no status bar?
// Note that there is no way to disable the
//  status bar explicitely.
extern  bool statusbaractive;

extern  bool automapactive; // In AutoMap mode?
extern  bool menuactive;    // Menu overlayed?
extern  int  paused;        // Game Pause?
extern  int  hud_active;    //jff 2/17/98 toggles heads-up status display
extern  bool viewactive;
extern  bool nodrawers;
extern  bool noblit;
extern  int  lefthanded; //sf

// This one is related to the 3-screen display mode.
// ANG90 = left side, ANG270 = right
extern  int viewangleoffset;

// Player taking events, and displaying.
extern  int consoleplayer;
extern  int displayplayer;

// -------------------------------------
// Scores, rating.
// Statistics on a given map, for intermission.
//
extern  int totalkills;
extern  int totalitems;
extern  int totalsecret;

// Timer, for scores.
extern  int levelstarttic;  // gametic at level start
extern  int basetic;    // killough 9/29/98: levelstarttic, adjusted
extern  int leveltime;  // tics in game play for par
// --------------------------------------
// DEMO playback/recording related stuff.

extern  bool usergame;
extern  bool demoplayback;
extern  bool demorecording;

// Quit after playing a demo from cmdline.
extern  bool singledemo;
// Print timing information after quitting.  killough
extern  bool timingdemo;
// Run tick clock at fastest speed possible while playing demo.  killough
extern  bool fastdemo;

extern  gamestate_t  gamestate;

//-----------------------------
// Internal parameters, fixed.
// These are set by the engine, and not changed
//  according to user inputs. Partly load from
//  WAD, partly set at startup time.

extern  int   gametic;


// Bookkeeping on players - state.
extern  player_t  players[MAXPLAYERS];

// Alive? Disconnected?
extern  bool playeringame[];

extern  mapthing_t *deathmatchstarts;     // killough
extern  size_t     num_deathmatchstarts; // killough

extern  mapthing_t *deathmatch_p;

// Player spawn spots.
extern  mapthing_t playerstarts[];

// Intermission stats.
// Parameters for world map / intermission.
extern wbstartstruct_t wminfo;

// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern int maxammo[];

//extern angle_t consoleangle;

//-----------------------------------------
// Internal parameters, used for engine.
//

// File handling stuff.
extern  char   *basedefault;
extern  char   *basepath;
extern  char   *userpath;
extern  char   *basegamepath;
extern  char   *usergamepath;

// if true, load all graphics at level load
extern  bool precache;

// wipegamestate can be set to -1
//  to force a wipe on the next draw
extern  gamestate_t     wipegamestate;

extern  double          mouseSensitivity_horiz; // killough
extern  double          mouseSensitivity_vert;
extern  bool            mouseSensitivity_vanilla; // [CG] 01/20/12

// SoM: 
extern  int             mouseAccel_type;

// [CG] 01/20/12
extern  int             mouseAccel_threshold;
extern  double          mouseAccel_value;

// debug flag to cancel adaptiveness
extern  bool            singletics;

extern  int             bodyqueslot;

// Needed to store the number of the dummy sky flat.
// Used for rendering, as well as tracking projectiles etc.

extern int    skyflatnum;

// Netgame stuff (buffers and pointers, i.e. indices).
extern  doomcom_t  *doomcom;
extern  doomdata_t *netbuffer;  // This points inside doomcom.

extern  int        rndindex;

extern  int        maketic;

extern  int        ticdup;

extern Thinker thinkercap;  // Both the head and tail of the thinker list

//-----------------------------------------------------------------------------

// v1.1-like pitched sounds
extern int pitched_sounds, default_pitched_sounds;     // killough 2/21/98

extern int allow_pushers;         // PUSH Things    // phares 3/10/98
extern int default_allow_pushers;

extern int variable_friction;  // ice & mud            // phares 3/10/98
extern int default_variable_friction;

extern int monsters_remember;                          // killough 3/1/98
extern int default_monsters_remember;

extern int weapon_recoil;          // weapon recoil    // phares
extern int default_weapon_recoil;

extern int player_bobbing;  // whether player bobs or not   // phares 2/25/98
extern int default_player_bobbing;  // killough 3/1/98: make local to each game

// killough 7/19/98: Classic Pre-Beta BFG
extern bfg_t bfgtype, default_bfgtype;

extern int dogs, default_dogs;     // killough 7/19/98: Marine's best friend :)
extern int dog_jumping, default_dog_jumping;   // killough 10/98

// killough 8/8/98: distance friendly monsters tend to stay from player
extern int distfriend, default_distfriend;

// killough 9/8/98: whether monsters are allowed to strafe or retreat
extern int monster_backing, default_monster_backing;

// killough 9/9/98: whether monsters intelligently avoid hazards
extern int monster_avoid_hazards, default_monster_avoid_hazards;

// killough 10/98: whether monsters are affected by friction
extern int monster_friction, default_monster_friction;

// killough 9/9/98: whether monsters help friends
extern int help_friends, default_help_friends;

extern int autoaim, default_autoaim;

extern int allowmlook, default_allowmlook; // haleyjd

extern int flashing_hom; // killough 10/98

extern int doom_weapon_toggles;   // killough 10/98

//=======================================================
//
// haleyjd: Eternity Stuff
//
//=======================================================

extern int HelperThing;          // type of thing to use for helper

extern int sky2flatnum;          // flat num of F_SKY2

extern fixed_t Sky1ColumnOffset, Sky2ColumnOffset;

extern bool cinema_pause;
extern int drawparticles;
extern int bloodsplat_particle;
extern int bulletpuff_particle;
extern int drawrockettrails;
extern int drawgrenadetrails;
extern int drawbfgcloud;

extern int forceFlipPan;

extern int startOnNewMap;

// haleyjd 04/10/03: game type, replaces limiting mess of netgame
// and deathmatch variables being used to mean multiple things
// haleyjd 04/14/03: deathmatch type is now controlled via dmflags

typedef enum
{
   gt_single,
   gt_coop,
   gt_dm,
} gametype_t;

extern gametype_t GameType, DefaultGameType;

extern int markUnknowns;

#endif

//----------------------------------------------------------------------------
//
// $Log: doomstat.h,v $
// Revision 1.13  1998/05/12  12:47:28  phares
// Removed OVER_UNDER code
//
// Revision 1.12  1998/05/06  16:05:34  jim
// formatting and documenting
//
// Revision 1.11  1998/05/05  16:28:51  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.10  1998/05/03  23:12:52  killough
// beautify, move most global switch variable decls here
//
// Revision 1.9  1998/04/06  04:54:55  killough
// Add demo_insurance
//
// Revision 1.8  1998/03/02  11:26:25  killough
// Remove now-dead monster_ai mask idea
//
// Revision 1.7  1998/02/23  04:17:38  killough
// fix bad translucency flag
//
// Revision 1.5  1998/02/20  21:56:29  phares
// Preliminarey sprite translucency
//
// Revision 1.4  1998/02/19  16:55:30  jim
// Optimized HUD and made more configurable
//
// Revision 1.3  1998/02/18  00:58:54  jim
// Addition of HUD
//
// Revision 1.2  1998/01/26  19:26:41  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:09  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

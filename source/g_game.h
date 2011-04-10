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

#ifndef __G_GAME__
#define __G_GAME__

#include "doomdef.h"
#include "d_event.h"
#include "d_ticcmd.h"
#include "d_player.h"

//
// GAME
//
class WadDirectory;

char *G_GetNameForMap(int episode, int map);
int G_GetMapForName(const char *name);

bool G_Responder(event_t *ev);
bool G_CheckDemoStatus(void);
bool G_CheckDemoStatus(void);
void G_DeathMatchSpawnPlayer(int playernum);
void G_DeferedInitNewNum(skill_t skill, int episode, int map);
void G_DeferedInitNew(skill_t skill, const char *levelname);
void G_DeferedPlayDemo(const char *demo);
void G_TimeDemo(const char *name, bool showmenu);
void G_LoadGame(char *name, int slot, bool is_command); // killough 5/15/98
void G_ForcedLoadGame(void);           // killough 5/15/98: forced loadgames
void G_SaveGame(int slot, char *description); // Called by M_Responder.
void G_RecordDemo(char *name);              // Only called by startup code.
void G_SetOldDemoOptions(void);
void G_BeginRecording(void);
void G_PlayDemo(char *name);
void G_StopDemo();
void G_ScrambleRand();
void G_ExitLevel(void);
void G_SecretExitLevel(void);
void G_WorldDone(void);
void G_ForceFinale(void);
void G_Ticker(void);
void G_ScreenShot(void);
void G_ReloadDefaults(void);     // killough 3/1/98: loads game defaults
void G_SaveGameName(char *,size_t,int); // killough 3/22/98: sets savegame filename
void G_SetFastParms(int);        // killough 4/10/98: sets -fast parameters
void G_DoNewGame(void);
void G_DoReborn(int playernum);
void G_DoLoadLevel(void);
byte *G_ReadOptions(byte *demoptr);         // killough 3/1/98
byte *G_WriteOptions(byte *demoptr);        // killough 3/1/98
void G_PlayerReborn(int player);
void G_InitNewNum(skill_t skill, int episode, int map);
void G_InitNew(skill_t skill, char*);
void G_DoVictory(void);
void G_SetGameMapName(const char *s); // haleyjd
void G_SetGameMap(void);
void G_SpeedSetAddThing(int thingtype, int nspeed, int fspeed); // haleyjd
uint64_t G_Signature(WadDirectory *dir);
void G_DoPlayDemo(void);

void R_InitPortals();

// killough 1/18/98: Doom-style printf;   killough 4/25/98: add gcc attributes
void doom_printf(const char *, ...) __attribute__((format(printf,1,2)));

        // sf: player_printf
void player_printf(player_t *player, const char *s, ...);

// killough 5/2/98: moved from m_misc.c:

extern int  key_escape;                                             // phares
extern int  key_autorun;
extern int  key_chat;
extern int  key_help;
extern int  key_spy;
extern int  key_pause;
extern int  destination_keys[MAXPLAYERS];
extern int  autorun;           // always running?                   // phares
extern int  runiswalk;
extern int  automlook;
extern int  invert_mouse;
extern int  bfglook;

extern angle_t consoleangle;

extern int  defaultskill;     // jff 3/24/98 default skill
extern bool haswolflevels;    // jff 4/18/98 wolf levels present
extern bool demorecording;    // killough 12/98
extern bool forced_loadgame;
extern bool command_loadgame;
extern char gamemapname[9];

extern int  bodyquesize, default_bodyquesize; // killough 2/8/98, 10/98
extern int  animscreenshot;       // animated screenshots

// killough 5/2/98: moved from d_deh.c:
// Par times (new item with BOOM) - from g_game.c
extern int pars[][10];  // hardcoded array size
extern int cpars[];     // hardcoded array size

#define NUMKEYS   256

extern int cooldemo;
extern bool hub_changelevel;

extern bool scriptSecret;   // haleyjd

extern bool sendpause;

extern int novert; // haleyjd

#define VERSIONSIZE   16

// killough 2/22/98: version id string format for savegames
#define VERSIONID "MBF %d"

extern WadDirectory *g_dir;
extern WadDirectory *d_dir;

// killough 2/28/98: A ridiculously large number
// of players, the most you'll ever need in a demo
// or savegame. This is used to prevent problems, in
// case more players in a game are supported later.

#define MIN_MAXPLAYERS 32

#endif

//----------------------------------------------------------------------------
//
// $Log: g_game.h,v $
// Revision 1.10  1998/05/16  09:17:02  killough
// Make loadgame checksum friendlier
//
// Revision 1.9  1998/05/06  15:15:59  jim
// Documented IWAD routines
//
// Revision 1.8  1998/05/03  22:15:50  killough
// Add all external declarations in g_game.c
//
// Revision 1.7  1998/04/27  02:00:53  killough
// Add gcc __attribute__ to check doom_printf() format string
//
// Revision 1.6  1998/04/10  06:34:35  killough
// Fix -fast parameter bugs
//
// Revision 1.5  1998/03/23  03:15:02  killough
// Add G_SaveGameName()
//
// Revision 1.4  1998/03/16  12:29:53  killough
// Remember savegame slot when loading
//
// Revision 1.3  1998/03/02  11:28:46  killough
// Add G_ReloadDefaults() prototype
//
// Revision 1.2  1998/01/26  19:26:51  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:55  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

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
//  DOOM main program (D_DoomMain) and game loop, plus functions to
//  determine game mode (shareware, registered), parse command line
//  parameters, configure game parameters (turbo), and call the startup
//  functions.
//
//-----------------------------------------------------------------------------

static const char rcsid[] = "$Id: d_main.c,v 1.47 1998/05/16 09:16:51 killough Exp $";

#ifndef LINUX
#include <sys/types.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>

// haleyjd 10/28/04: Win32-specific repair for D_DoomExeDir
#ifdef _MSC_VER
#include "Win32/i_fnames.h"
#endif

#include "z_zone.h"

#include "d_io.h"  // SoM 3/12/2002: moved unistd stuff into d_io.h
#include "doomdef.h"
#include "doomstat.h"
#include "dstrings.h"
#include "sounds.h"
#include "c_runcmd.h"
#include "c_io.h"
#include "c_net.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_misc.h"
#include "mn_engin.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "p_setup.h"
#include "p_chase.h"
#include "r_draw.h"
#include "r_main.h"
#include "d_main.h"
#include "d_deh.h"  // Ty 04/08/98 - Externalizations
#include "g_bind.h" // haleyjd
#include "d_dialog.h"
#include "d_gi.h"
#include "in_lude.h"
#include "a_small.h"
#include "g_gfs.h"
#include "d_dehtbl.h"
#include "g_dmflag.h"
#include "e_edf.h"

char **wadfiles;

// killough 10/98: preloaded files
#define MAXLOADFILES 2
char *wad_files[MAXLOADFILES], *deh_files[MAXLOADFILES];
// haleyjd: allow two auto-loaded console scripts
char *csc_files[MAXLOADFILES];

int textmode_startup = 0;  // sf: textmode_startup for old-fashioned people
int use_startmap = -1;     // default to -1 for asking in menu
boolean devparm;           // started game with -devparm

// jff 1/24/98 add new versions of these variables to remember command line
boolean clnomonsters;   // checkparm of -nomonsters
boolean clrespawnparm;  // checkparm of -respawn
boolean clfastparm;     // checkparm of -fast
// jff 1/24/98 end definition of command line version of play mode switches

int r_blockmap = false;       // -blockmap command line

boolean nomonsters;     // working -nomonsters
boolean respawnparm;    // working -respawn
boolean fastparm;       // working -fast

boolean singletics = false; // debug flag to cancel adaptiveness

//jff 1/22/98 parms for disabling music and sound
boolean nosfxparm;
boolean nomusicparm;

//jff 4/18/98
extern boolean inhelpscreens;

skill_t startskill;
int     startepisode;
int     startmap;
char    *startlevel;
boolean autostart;
FILE    *debugfile;

boolean advancedemo;

extern boolean timingdemo, singledemo, demoplayback, fastdemo; // killough

char    wadfile[PATH_MAX+1];       // primary wad file
char    mapdir[PATH_MAX+1];        // directory of development maps
char    basedefault[PATH_MAX+1];   // default file
char    baseiwad[PATH_MAX+1];      // jff 3/23/98: iwad directory
char    basesavegame[PATH_MAX+1];  // killough 2/16/98: savegame directory

// set from iwad: level to start new games from
char firstlevel[9] = "";     

//jff 4/19/98 list of standard IWAD names
const char *const standard_iwads[]=
{
   "/doom2f.wad",   // DOOM II, French Version
   "/doom2.wad",    // DOOM II
   "/plutonia.wad", // Final DOOM: Plutonia
   "/tnt.wad",      // Final DOOM: TNT
   "/doom.wad",     // Registered/Ultimate DOOM
   "/doom1.wad",    // Shareware DOOM
   "/doomu.wad",    // CPhipps - allow doomu.wad
   "/freedoom.wad", // Freedoom -- haleyjd 01/31/03
   "/heretic.wad",  // Heretic  -- haleyjd 10/10/05
   "/heretic1.wad", // Shareware Heretic
};

static const int nstandard_iwads = sizeof standard_iwads/sizeof*standard_iwads;

void D_CheckNetGame(void);
void D_ProcessEvents(void);
void G_BuildTiccmd(ticcmd_t* cmd);
void D_DoAdvanceDemo(void);

//
// EVENT HANDLING
//
// Events are asynchronous inputs generally generated by the game user.
// Events can be discarded if no responder claims them
//

event_t events[MAXEVENTS];
int eventhead, eventtail;

//
// D_PostEvent
// Called by the I/O functions when input is detected
//
void D_PostEvent(event_t *ev)
{
   events[eventhead++] = *ev;
   eventhead &= MAXEVENTS-1;
}

//
// D_ProcessEvents
// Send all the events of the given timestamp down the responder chain
//
void D_ProcessEvents(void)
{
   // IF STORE DEMO, DO NOT ACCEPT INPUT
   // sf: I don't think SMMU is going to be played in any store any
   //     time soon =)
   // if (gamemode != commercial || W_CheckNumForName("map01") >= 0)

   for(; eventtail != eventhead; eventtail = (eventtail+1) & (MAXEVENTS-1))
   {
      event_t *evt = events + eventtail;
      
      if(!MN_Responder(evt))
         if(!C_Responder(evt))
            G_Responder(evt);
   }
}

//
// D_Display
//  draw current display, possibly wiping it from the previous
//

// wipegamestate can be set to -1 to force a wipe on the next draw

gamestate_t    oldgamestate = -1;  // sf: globaled
gamestate_t    wipegamestate = GS_DEMOSCREEN;
void           R_ExecuteSetViewSize(void);
camera_t       *camera;
extern boolean setsizeneeded;
boolean        redrawsbar;      // sf: globaled
boolean        redrawborder;    // sf: cleaned up border redraw

void D_Display(void)
{
   if(nodrawers)                // for comparative timing / profiling
      return;

   if(setsizeneeded)            // change the view size if needed
   {
      R_ExecuteSetViewSize();
      R_FillBackScreen();       // redraw backscreen
   }

   // save the current screen if about to wipe
   // no melting consoles
   if(gamestate != wipegamestate && wipegamestate != GS_CONSOLE)
   {
      Wipe_StartScreen();
   }

   if(inwipe || c_moving || menuactive)
      redrawsbar = redrawborder = true;   // redraw status bar and border

   // haleyjd: optimization for fullscreen menu drawing -- no
   // need to do all this if the menus are going to cover it up :)
   if(!MN_CheckFullScreen())
   {
      switch(gamestate)                // do buffered drawing
      {
      case GS_LEVEL:
         // see if the border needs to be initially drawn
         if(oldgamestate != GS_LEVEL)
            R_FillBackScreen();    // draw the pattern into the back screen
         HU_Erase();

         if(automapactive)
         {
            AM_Drawer();
         }
         else
         {
            // see if the border needs to be updated to the screen
            if(redrawborder)
               R_DrawViewBorder();    // redraw border
            R_RenderPlayerView (&players[displayplayer], camera);
         }

         ST_Drawer(scaledviewheight == 200, redrawsbar);  // killough 11/98
         HU_Drawer();
         if(currentdialog)
            DLG_Drawer();
         break;
      case GS_INTERMISSION:
         IN_Drawer();
         break;
      case GS_FINALE:
         F_Drawer();
         break;
      case GS_DEMOSCREEN:
         D_PageDrawer();
         break;
      case GS_CONSOLE:
         break;
      }
  
      redrawsbar = false; // reset this now
      redrawborder = false;
      
      // clean up border stuff
      if(gamestate != oldgamestate && gamestate != GS_LEVEL)
         I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
      
      oldgamestate = wipegamestate = gamestate;
      
      // draw pause pic
      if(paused && !walkcam_active) // sf: not if walkcam active for
      {                             // frads taking screenshots
         char *lumpname = gameModeInfo->pausePatch; // haleyjd 03/12/03

         // in heretic, and with user pause patches
         patch_t *patch = (patch_t *)W_CacheLumpName(lumpname, PU_CACHE);
         int width = SHORT(patch->width);
         int x = (SCREENWIDTH - width) / 2 + patch->leftoffset;
         // SoM 2-4-04: ANYRES
         int y = 4 + (automapactive ? 0 : scaledwindowy);

         V_DrawPatch(x, y, &vbscreen, patch);
      }
      
      if(inwipe)
         Wipe_Drawer();
      
      C_Drawer();

   } // if(!MN_CheckFullScreen())

   // menus go directly to the screen
   MN_Drawer();         // menu is drawn even on top of everything
   NetUpdate();         // send out any new accumulation
   
   //sf : now system independent
   if(v_ticker)
      V_FPSDrawer();
   
   // sf: wipe changed: runs alongside the rest of the game rather
   //     than in its own loop
   
   I_FinishUpdate();              // page flip or blit buffer
}

//
//  DEMO LOOP
//

static int demosequence;         // killough 5/2/98: made static
static int pagetic;
static char *pagename;

//
// D_PageTicker
// Handles timing for warped projection
//
void D_PageTicker(void)
{
   // killough 12/98: don't advance internal demos if a single one is 
   // being played. The only time this matters is when using -loadgame with
   // -fastdemo, -playdemo, or -timedemo, and a consistency error occurs.
   
   if (/*!singledemo &&*/ --pagetic < 0)
      D_AdvanceDemo();
}

void D_640PageDrawer(const char *key);

        // titlepic checksums
#define DOOM1TITLEPIC 382248766
#define DOOM2TITLEPIC 176650962

//
// D_PageDrawer
//
// killough 11/98: add credits screen
//
void D_PageDrawer(void)
{
   int l;
   byte *t;

   if(pagename && (l = W_CheckNumForName(pagename)) != -1)
   {      
      t = W_CacheLumpNum(l, PU_CACHE);

      // haleyjd 08/15/02: handle Heretic pages
      // haleyjd 04/22/06: use lump size of 64000 to distinguish raw pages.
      // Valid fullscreen patch graphics should be larger than this.

      if(W_LumpLength(l) == 64000)
         V_DrawBlock(0, 0, &vbscreen, SCREENWIDTH, SCREENHEIGHT, t);
      else
         V_DrawPatch(0, 0, &vbscreen, (patch_t *)t);

      if(gameModeInfo->hasAdvisory && demosequence == 1)
      {
         l = W_GetNumForName("ADVISOR");
         t = W_CacheLumpNum(l, PU_CACHE);
         V_DrawPatch(4, 160, &vbscreen, (patch_t *)t);
      }
              
      // SoM 2-4-04: ANYRES 
      // TODO/FIXME: reimplement higher resolution titlescreen?
      /*if(hires) // check for original title screen
      {
         long checksum = W_LumpCheckSum(l);
         if(checksum == DOOM1TITLEPIC)
         {
            D_640PageDrawer("UDTTL");
            return;
         }
         else if(checksum == DOOM2TITLEPIC)
         {
            D_640PageDrawer("D2TTL");
            return;
         }
      }*/
   }
   else
      MN_DrawCredits();  
}

        // sf: 640x400 title screens at satori's request
void D_640PageDrawer(const char *key)
{
   char tempstr[10];

   // draw the patches
  
   sprintf(tempstr, "%s00", key);
   V_DrawPatchUnscaled(0, 0, 0, W_CacheLumpName(tempstr, PU_CACHE));
  
   sprintf(tempstr, "%s01", key);
   V_DrawPatchUnscaled(0, SCREENHEIGHT, 0,
                       W_CacheLumpName(tempstr, PU_CACHE));
  
   sprintf(tempstr, "%s10", key);
   V_DrawPatchUnscaled(SCREENWIDTH, 0, 0,
                       W_CacheLumpName(tempstr, PU_CACHE));
  
   sprintf(tempstr, "%s11", key);
   V_DrawPatchUnscaled(SCREENWIDTH, SCREENHEIGHT, 0,
                       W_CacheLumpName(tempstr, PU_CACHE));
}

//
// D_AdvanceDemo
// Called after each demo or intro demosequence finishes
//
void D_AdvanceDemo(void)
{
   advancedemo = true;
}

// killough 11/98: functions to perform demo sequences

static void D_SetPageName(char *name)
{
   pagename = name;
}

static void D_DrawTitle(char *name)
{
   S_StartMusic(gameModeInfo->titleMusNum);
   pagetic = gameModeInfo->titleTics;
   D_SetPageName(name);
}

static void D_DrawTitleA(char *name)
{
   pagetic = gameModeInfo->advisorTics;
   D_SetPageName(name);
}

// killough 11/98: tabulate demo sequences

static struct 
{
   void (*func)(char *);
   char *name;
} const demostates[][6] =
  {
    {
      {D_DrawTitle, "TITLEPIC"}, // shareware
      {D_DrawTitle, "TITLEPIC"}, // registerd
      {D_DrawTitle, "TITLEPIC"}, // retail
      {D_DrawTitle, "TITLEPIC"}, // commercial
      {D_DrawTitle, "TITLE"},    // heretic shareware
      {D_DrawTitle, "TITLE"},    // heretic registered/sosr
    },

    {
      {G_DeferedPlayDemo, "demo1"},
      {G_DeferedPlayDemo, "demo1"},
      {G_DeferedPlayDemo, "demo1"},
      {G_DeferedPlayDemo, "demo1"},
      {D_DrawTitleA, "TITLE"},
      {D_DrawTitleA, "TITLE"},
    },

    {
      {D_SetPageName, NULL},
      {D_SetPageName, NULL},
      {D_SetPageName, NULL},
      {D_SetPageName, NULL},
      {G_DeferedPlayDemo, "demo1"},
      {G_DeferedPlayDemo, "demo1"},
    },

    {
      {G_DeferedPlayDemo, "demo2"},
      {G_DeferedPlayDemo, "demo2"},
      {G_DeferedPlayDemo, "demo2"},
      {G_DeferedPlayDemo, "demo2"},
      {D_SetPageName, "ORDER"},
      {D_SetPageName, "CREDIT"},
    },

    {
      {D_SetPageName, "HELP2"},
      {D_SetPageName, "HELP2"},
      {D_SetPageName, "CREDIT"},
      {D_DrawTitle,   "TITLEPIC"},
      {G_DeferedPlayDemo, "demo2"},
      {G_DeferedPlayDemo, "demo2"},
    },

    {
      {G_DeferedPlayDemo, "demo3"},
      {G_DeferedPlayDemo, "demo3"},
      {G_DeferedPlayDemo, "demo3"},
      {G_DeferedPlayDemo, "demo3"},
      {D_SetPageName, NULL},
      {D_SetPageName, NULL},
    },

    {
      {NULL},
      {NULL},
      {NULL},
      {D_SetPageName, "CREDIT"},
      {G_DeferedPlayDemo, "demo3"},
      {G_DeferedPlayDemo, "demo3"},
    },

    {
      {NULL},
      {NULL},
      {NULL},
      {G_DeferedPlayDemo, "demo4"},
      {D_SetPageName, "CREDIT"},
      {D_SetPageName, "CREDIT"},
    },

    {
      {NULL},
      {NULL},
      {NULL},
      {NULL},
      {NULL},
      {NULL},
    }
  };

//
// This cycles through the demo sequences.
//
// killough 11/98: made table-driven

void D_DoAdvanceDemo(void)
{
   players[consoleplayer].playerstate = PST_LIVE;  // not reborn
   advancedemo = usergame = paused = false;
   gameaction = ga_nothing;
   
   pagetic = gameModeInfo->pageTics;
   gamestate = GS_DEMOSCREEN;
   
   if (!demostates[++demosequence][gamemode].func)
      demosequence = 0;
   demostates[demosequence][gamemode].func
      (demostates[demosequence][gamemode].name);

   C_InstaPopup();       // make console go away
}

//
// D_StartTitle
//
void D_StartTitle(void)
{
   gameaction = ga_nothing;
   demosequence = -1;
   D_AdvanceDemo();
   C_InstaPopup();       // pop up the console straight away
}

//
// D_AddFile
//
// Rewritten by Lee Killough
//
// killough 11/98: remove limit on number of files
//

static int numwadfiles, numwadfiles_alloc;

void D_AddFile(char *file)
{
   // sf: allocate for +2 for safety
   if(numwadfiles+2 >= numwadfiles_alloc)
      wadfiles = realloc(wadfiles, (numwadfiles_alloc = numwadfiles_alloc ?
				    numwadfiles_alloc * 2 : 8)*sizeof*wadfiles);
   wadfiles[numwadfiles] = strdup(file); //sf: always NULL at end
   wadfiles[numwadfiles+1] = NULL;
   numwadfiles++;
}

        //sf: console command to list loaded files
void D_ListWads(void)
{
   int i;
   C_Printf(FC_HI "Loaded WADs:\n");
   
   for(i = 0; i < numwadfiles; i++)
      C_Printf("%s\n", wadfiles[i]);
}

// Return the path where the executable lies -- Lee Killough
char *D_DoomExeDir(void)
{
   static char *base = NULL;

   if(!base) // cache multiple requests
   {
#ifndef _MSC_VER

      size_t len = strlen(myargv[0]) + 1;
      
      base = malloc(len);
      
      // haleyjd 03/09/03: generalized
      M_GetFilePath(myargv[0], base, len);
#else
      // haleyjd 10/28/04: the above is not sufficient for all versions
      // of Windows. There is an API function which takes care of this,
      // however.  See i_fnames.c in the Win32 subdirectory.
      base = malloc(PATH_MAX + 1);

      WIN_GetExeDir(base, PATH_MAX + 1);
#endif
   }

   return base;
}

// killough 10/98: return the name of the program the exe was invoked as
char *D_DoomExeName(void)
{
   static char *name;    // cache multiple requests

   if(!name)
   {
      char *p = myargv[0] + strlen(myargv[0]);
      int i = 0;

      while(p > myargv[0] && p[-1] != '/' && p[-1] != '\\' && p[-1] != ':')
         p--;
      while(p[i] && p[i] != '.')
         i++;

      name = malloc(i + 1);
      memset(name, 0, i + 1);
      
      strncpy(name, p, i);
   }
   return name;
}

#define isIWAD(name) \
   ((name)[0] == 'I' && (name)[1] == 'W' && \
    (name)[2] == 'A' && (name)[3] == 'D')

#define isMapExMy(name) \
   ((name)[0] == 'E' && (name)[2] == 'M' && !(name)[4])

#define isMapMAPxy(name) \
   ((name)[0] == 'M' && (name)[1] == 'A' && (name)[2] == 'P' && !(name)[5])

#define isCAV(name) \
   ((name)[0] == 'C' && (name)[1] == 'A' && (name)[2] == 'V' && !(name)[7])

#define isMC(name) \
   ((name)[0] == 'M' && (name)[1] == 'C' && !(name)[3])

#define isADVISOR(name) \
   ((name)[0] == 'A' && (name)[1] == 'D' && (name)[2] == 'V' && \
    (name)[3] == 'I' && (name)[4] == 'S' && (name)[5] == 'O' && \
    (name)[6] == 'R' && !(name)[7])

#define isTINTTAB(name) \
   ((name)[0] == 'T' && (name)[1] == 'I' && (name)[2] == 'N' && \
    (name)[3] == 'T' && (name)[4] == 'T' && (name)[5] == 'A' && \
    (name)[6] == 'B' && !(name)[7])

#define isSNDCURVE(name) \
   ((name)[0] == 'S' && (name)[1] == 'N' && (name)[2] == 'D' && \
    (name)[3] == 'C' && (name)[4] == 'U' && (name)[5] == 'R' && \
    (name)[6] == 'V' && (name)[7] == 'E')

#define isEXTENDED(name) \
   ((name)[0] == 'E' && (name)[1] == 'X' && (name)[2] == 'T' && \
    (name)[3] == 'E' && (name)[4] == 'N' && (name)[5] == 'D' && \
    (name)[6] == 'E' && (name)[7] == 'D')

#define isFREEDOOM(name) \
   ((name)[0] == 'F' && (name)[1] == 'R' && (name)[2] == 'E' && \
    (name)[3] == 'E' && (name)[4] == 'D' && (name)[5] == 'O' && \
    (name)[6] == 'O' && (name)[7] == 'M')

// haleyjd 10/13/05: special stuff for FreeDOOM :)
static boolean freedoom = false;

//
// CheckIWAD
//
// Verify a file is indeed tagged as an IWAD
// Scan its lumps for levelnames and return gamemode as indicated
// Detect missing wolf levels in DOOM II
//
// The filename to check is passed in iwadname, the gamemode detected is
// returned in gmode, hassec returns the presence of secret levels
//
// jff 4/19/98 Add routine to test IWAD for validity and determine
// the gamemode from it. Also note if DOOM II, whether secret levels exist
//
// killough 11/98:
// Rewritten to considerably simplify
// Added Final Doom support (thanks to Joel Murdoch)
//
static void CheckIWAD(const char *iwadname,
		      GameMode_t *gmode,
		      GameMission_t *gmission,  // joel 10/17/98 Final DOOM fix
		      boolean *hassec)
{
   FILE *fp;
   int ud = 0, rg = 0, sw = 0, cm = 0, sc = 0, tnt = 0, plut = 0;
   int raven = 0, sosr = 0;
   filelump_t lump;
   wadinfo_t header;
   const char *n = lump.name;

   if(!(fp = fopen(iwadname, "rb")))
      I_Error("Can't open IWAD: %s\n",iwadname);

   // read IWAD header
   if(fread(&header, 1, sizeof header, fp) != sizeof header ||
      !isIWAD(header.identification))
      I_Error("IWAD tag not present: %s\n", iwadname);

   fseek(fp, LONG(header.infotableofs), SEEK_SET);

   // Determine game mode from levels present
   // Must be a full set for whichever mode is present
   // Lack of wolf-3d levels also detected here

   header.numlumps = LONG(header.numlumps);

   for(; header.numlumps; header.numlumps--)
   {
      if(!fread(&lump, sizeof(lump), 1, fp))
         break;

      if(isMapExMy(n))
      {
         if(n[1] == '4')
            ++ud;
         else if(n[1] == '3' || n[1] == '2')
            ++rg;
         else if(n[1] == '1')
            ++sw;
      }
      else if(isMapMAPxy(n))
      {
         ++cm;
         sc += (n[3] == '3' && (n[4] == '1' || n[4] == '2'));
      }
      else if(isCAV(n))
         ++tnt;
      else if(isMC(n))
         ++plut;
      else if(isADVISOR(n) || isTINTTAB(n) || isSNDCURVE(n))
         ++raven;
      else if(isEXTENDED(n))
         ++sosr;
      else if(isFREEDOOM(n))
         freedoom = true;
   }

   fclose(fp);

   *hassec = false;

   // haleyjd 10/09/05: "Raven mode" detection
   if(raven == 3)
   {
      // TODO: Hexen     
      *gmission = heretic;

      if(rg >= 18)
      {
         // require both E4 and EXTENDED lump for SoSR
         if(sosr && ud >= 9)
            *gmission = hticsosr;
         *gmode = hereticreg;
      }
      else if(sw >= 9)
         *gmode = hereticsw;
      else
         *gmode = indetermined;
   }
   else
   {
      *gmission = doom;
      
      if(cm >= 30)
      {
         if(tnt >= 4)
            *gmission = pack_tnt;
         else if(plut >= 8)
            *gmission = pack_plut;
         else
            *gmission = doom2;
         *hassec = (sc >= 2);
         *gmode = commercial;
      }
      else if(ud >= 9)
         *gmode = retail;
      else if(rg >= 18)
         *gmode = registered;
      else if(sw >= 9)
         *gmode = shareware;
      else
         *gmode = indetermined;   
   }
}

// jff 4/19/98 Add routine to check a pathname for existence as
// a file or directory. If neither append .wad and check if it
// exists as a file then. Else return non-existent.

boolean WadFileStatus(char *filename,boolean *isdir)
{
   struct stat sbuf;
   int i;
   
   *isdir = false;                //default is directory to false
   if(!filename || !*filename)    //if path NULL or empty, doesn't exist
      return false;

   if(!stat(filename,&sbuf))      //check for existence
   {
      *isdir=S_ISDIR(sbuf.st_mode); //if it does, set whether a dir or not
      return true;                  //return does exist
   }

   i = strlen(filename);          //get length of path
   if(i >= 4)
      if(!strnicmp(filename + i - 4, ".wad", 4))
         return false;            //if already ends in .wad, not found

   strcat(filename,".wad");       //try it with .wad added
   if(!stat(filename,&sbuf))      //if it exists then
   {
      if(S_ISDIR(sbuf.st_mode))   //but is a dir, then say we didn't find it
         return false;
      return true;                //otherwise return file found, w/ .wad added
   }
   filename[i] = 0;               //remove .wad
   return false;                  //and report doesn't exist
}

//
// FindIWADFile
//
// Search in all the usual places until an IWAD is found.
//
// The global baseiwad contains either a full IWAD file specification
// or a directory to look for an IWAD in, or the name of the IWAD desired.
//
// The global standard_iwads lists the standard IWAD names
//
// The result of search is returned in baseiwad, or set blank if none found
//
// IWAD search algorithm:
//
// Set customiwad blank
// If -iwad present set baseiwad to normalized path from -iwad parameter
//  If baseiwad is an existing file, thats it
//  If baseiwad is an existing dir, try appending all standard iwads
//  If haven't found it, and no : or / is in baseiwad,
//   append .wad if missing and set customiwad to baseiwad
//
// Look in . for customiwad if set, else all standard iwads
//
// Look in DoomExeDir. for customiwad if set, else all standard iwads
//
// If $DOOMWADDIR is an existing file
//  If customiwad is not set, thats it
//  else replace filename with customiwad, if exists thats it
// If $DOOMWADDIR is existing dir, try customiwad if set, else standard iwads
//
// If $HOME is an existing file
//  If customiwad is not set, thats it
//  else replace filename with customiwad, if exists thats it
// If $HOME is an existing dir, try customiwad if set, else standard iwads
//
// IWAD not found
//
// jff 4/19/98 Add routine to search for a standard or custom IWAD in one
// of the standard places. Returns a blank string if not found.
//
// killough 11/98: simplified, removed error-prone cut-n-pasted code
//

char *FindIWADFile(void)
{
   static const char *envvars[] = {"DOOMWADDIR", "HOME"};
   static char iwad[PATH_MAX+1], customiwad[PATH_MAX+1];
   boolean isdir=false;
   int i,j;
   char *p;
   const char *basename = NULL;
   
   *iwad = 0;       // default return filename to empty
   *customiwad = 0; // customiwad is blank

   //jff 3/24/98 get -iwad parm if specified else use .
   if((i = M_CheckParm("-iwad")) && i < myargc-1)
      basename = myargv[i+1];
   else
      basename = G_GFSCheckIWAD(); // haleyjd 04/16/03: GFS support

   //jff 3/24/98 get -iwad parm if specified else use .
   if(basename)
   {
      NormalizeSlashes(strcpy(baseiwad,basename));
      if(WadFileStatus(strcpy(iwad,baseiwad),&isdir))
      {
         if(!isdir)
            return iwad;
         else
            for(i = 0; i < nstandard_iwads; i++)
            {
               int n = strlen(iwad);
               strcat(iwad,standard_iwads[i]);
               if(WadFileStatus(iwad,&isdir) && !isdir)
                  return iwad;
               iwad[n] = 0; // reset iwad length to former
            }
      }
      else if(!strchr(iwad,':') && !strchr(iwad,'/'))
         AddDefaultExtension(strcat(strcpy(customiwad, "/"), iwad), ".wad");
   }

   for(j = 0; j < 2; j++)
   {
      strcpy(iwad, j ? D_DoomExeDir() : ".");
      NormalizeSlashes(iwad);
      
      if(devparm)       // sf: only show 'looking in' for devparm
         printf("Looking in %s\n",iwad);   // killough 8/8/98
      if(*customiwad)
      {
         strcat(iwad,customiwad);
         if (WadFileStatus(iwad,&isdir) && !isdir)
            return iwad;
      }
      else
      {
         for(i = 0; i < nstandard_iwads; i++)
         {
            int n = strlen(iwad);
            strcat(iwad,standard_iwads[i]);
            if(WadFileStatus(iwad,&isdir) && !isdir)
               return iwad;
            iwad[n] = 0; // reset iwad length to former
         }
      }
   }

   for(i=0; i<sizeof envvars/sizeof *envvars;i++)
   {
      if((p = getenv(envvars[i])))
      {
         NormalizeSlashes(strcpy(iwad,p));
         if(WadFileStatus(iwad,&isdir))
         {
            if (!isdir)
            {
               if(!*customiwad)
                  return printf("Looking for %s\n",iwad), iwad; // killough 8/8/98
               else if((p = strrchr(iwad,'/')))
               {
                  *p=0;
                  strcat(iwad,customiwad);
                  printf("Looking for %s\n",iwad);  // killough 8/8/98
                  if(WadFileStatus(iwad,&isdir) && !isdir)
                     return iwad;
               }
            }
            else
            {
               if(devparm)       // sf: devparm only
                  printf("Looking in %s\n",iwad);  // killough 8/8/98
               if(*customiwad)
               {
                  if (WadFileStatus(strcat(iwad,customiwad),&isdir) && !isdir)
                     return iwad;
               }
               else
               {
                  for(i = 0; i < nstandard_iwads; i++)
                  {
                     int n = strlen(iwad);
                     strcat(iwad,standard_iwads[i]);
                     if(WadFileStatus(iwad,&isdir) && !isdir)
                        return iwad;
                     iwad[n] = 0; // reset iwad length to former
                  }
               } // end else (!*customiwad)
            } // end else (isdir)
         } // end if(WadFileStatus(...))
      } // end if((p = getenv(...)))
   } // end for

   *iwad = 0;
   return iwad;
}

//
// D_LoadResourceWad
//
// haleyjd 03/10/03: moved eternity.wad loading to this function
//
static void D_LoadResourceWad(void)
{
   char filestr[PATH_MAX+1];
   
   // get smmu.wad from the same directory as smmu.exe
   // 25/10/99: use same name as exe
   
   // haleyjd 06/04/02: lengthened filestr, memset to 0
   memset(filestr, 0, PATH_MAX+1);
   
   psnprintf(filestr, sizeof(filestr), gameModeInfo->resourceFmt,
             D_DoomExeDir(), D_DoomExeName());
   
   NormalizeSlashes(filestr);
   D_AddFile(filestr);
   
   modifiedgame = false;         // reset, ignoring smmu.wad etc.
}

//
// IdentifyVersion
//
// Set the location of the defaults file and the savegame root
// Locate and validate an IWAD file
// Determine gamemode from the IWAD
//
// supports IWADs with custom names. Also allows the -iwad parameter to
// specify which iwad is being searched for if several exist in one dir.
// The -iwad parm may specify:
//
// 1) a specific pathname, which must exist (.wad optional)
// 2) or a directory, which must contain a standard IWAD,
// 3) or a filename, which must be found in one of the standard places:
//   a) current dir,
//   b) exe dir
//   c) $DOOMWADDIR
//   d) or $HOME
//
// jff 4/19/98 rewritten to use a more advanced search algorithm

const char *game_name = "unknown game";      // description of iwad

void IdentifyVersion(void)
{
   int         i;    //jff 3/24/98 index of args on commandline
   struct stat sbuf; //jff 3/24/98 used to test save path for existence
   char *iwad;

   // get config file from same directory as executable
   // killough 10/98
   // haleyjd 07/04/02: fix for when doom exe dir is ""
   
   psnprintf(basedefault, sizeof(basedefault),
             "%s/%s.cfg", D_DoomExeDir(), D_DoomExeName());

   // set save path to -save parm or current dir

   strcpy(basesavegame,".");       //jff 3/27/98 default to current dir
   if((i=M_CheckParm("-save")) && i<myargc-1) //jff 3/24/98 if -save present
   {
      if(!stat(myargv[i+1],&sbuf) && S_ISDIR(sbuf.st_mode)) // and is a dir
         strcpy(basesavegame,myargv[i+1]);  //jff 3/24/98 use that for savegame
      else
         puts("Error: -save path does not exist, using current dir");  // killough 8/8/98
   }

   // locate the IWAD and determine game mode from it
   
   iwad = FindIWADFile();
   
   if(iwad && *iwad)
   {
      printf("IWAD found: %s\n",iwad); //jff 4/20/98 print only if found

      CheckIWAD(iwad,
		&gamemode,
		&gamemission,   // joel 10/16/98 gamemission added
		&haswolflevels);

      switch(gamemode)
      {
      case retail:
         gameModeInfo = &giDoomReg;
         gameModeInfo->numEpisodes = 4;       // haleyjd 05/21/06: patch this here
         game_name = "Ultimate DOOM version"; // killough 8/8/98
         break;

      case registered:
         gameModeInfo = &giDoomReg;
         game_name = "DOOM Registered version";
         break;
         
      case shareware:
         gameModeInfo = &giDoomReg;
         gameModeInfo->numEpisodes = 1;
         gameModeInfo->flags |= GIF_SHAREWARE; // haleyjd 05/21/06: patch these here
         game_name = "DOOM Shareware version";
         break;

      case commercial:
         gameModeInfo = &giDoomCommercial;
         // joel 10/16/98 Final DOOM fix
         switch (gamemission)
         {
         case pack_tnt:
            game_name = "Final DOOM: TNT - Evilution version";
            break;

         case pack_plut:
            game_name = "Final DOOM: The Plutonia Experiment version";
            break;
            
         case doom2:
         default:

            i = strlen(iwad);
            if(i >= 10 && !strnicmp(iwad+i-10, "doom2f.wad", 10))
            {
               language = french;
               game_name = "DOOM II version, French language";
            }
            else
            {
               game_name = haswolflevels ? "DOOM II version" :
                  "DOOM II version, German edition, no Wolf levels";
            }
            break;
         }
         // joel 10/16/98 end Final DOOM fix

         // haleyjd 10/13/05: freedoom override :)
         if(freedoom)
            game_name = "DOOM II, Freedoom version";
         break;

      case hereticsw:
         gameModeInfo = &giHereticSW;
         game_name = "Heretic Shareware version";
         break;

      case hereticreg:
         gameModeInfo = &giHereticReg;
         // haleyjd: patch # of episodes here if appropriate
         if(gamemission == hticsosr)
         {
            gameModeInfo->numEpisodes = 6;
            game_name = "Heretic: Shadow of the Serpent Riders version";
         }
         else
            game_name = "Heretic Registered version";
         break;

      default:
         break;
      }

      puts(game_name);
      
      if(gamemode == indetermined)
      {
         gameModeInfo = &giDoomReg;
         puts("Unknown Game Version, may not work");  // killough 8/8/98
      }

      // haleyjd 03/10/03: add eternity.wad before the IWAD, at
      // request of fraggle -- this allows better compatibility
      // with new IWADs
      D_LoadResourceWad();

      D_AddFile(iwad);
   }
   else
      I_Error("IWAD not found\n");
}

// MAXARGVS: a reasonable(?) limit on response file arguments

#define MAXARGVS 100

//
//
// FindResponseFile
//
// Find a Response File, identified by an "@" argument.
//
// haleyjd 04/17/03: copied, slightly modified prboom's code to
// allow quoted LFNs in response files.
//
void FindResponseFile(void)
{
   int i;

   for(i = 1; i < myargc; i++)
   {
      if(myargv[i][0] == '@')
      {
         int size, index, indexinfile;
         char *file = NULL, *firstargv;
         char **moreargs = malloc(myargc * sizeof(char *));
         char **newargv;
         char fname[PATH_MAX + 1];

         strncpy(fname, &myargv[i][1], PATH_MAX + 1);
         AddDefaultExtension(fname, ".rsp");

         // read the response file into memory
         if((size = M_ReadFile(fname, (byte **)&file)) < 0)
         {
            I_Error("No such response file: %s\n", fname);
         }

         printf("Found response file %s\n", fname);

         // proff 04/05/2000: Added check for empty rsp file
         if(!size)
         {
            int k;
            printf("\nResponse file empty!\n");
            
            newargv = calloc(sizeof(char *), MAXARGVS);
            newargv[0] = myargv[0];
            for(k = 1, index = 1; k < myargc; k++)
            {
               if(i != k)
                  newargv[index++] = myargv[k];
            }
            myargc = index; myargv = newargv;
            return;
         }

         // keep all cmdline args following @responsefile arg
         memcpy((void *)moreargs,&myargv[i+1],(index = myargc - i - 1) * sizeof(myargv[0]));

         firstargv = myargv[0];
         newargv = calloc(sizeof(char *),MAXARGVS);
         newargv[0] = firstargv;

         {
            char *infile = file;
            indexinfile = 0;
            indexinfile++;  // skip past argv[0] (keep it)
            do
            {
               while(size > 0 && isspace(*infile))
               { 
                  infile++;
                  size--; 
               }
               
               if(size > 0)
               {
                  char *s = malloc(size+1);
                  char *p = s;
                  int quoted = 0; 

                  while (size > 0)
                  {
                     // Whitespace terminates the token unless quoted
                     if(!quoted && isspace(*infile))
                        break;
                     if(*infile == '\"')
                     {
                        // Quotes are removed but remembered
                        infile++; size--; quoted ^= 1; 
                     } 
                     else 
                     {
                        *p++ = *infile++; size--;
                     }
                  }
                  if(quoted) 
                     I_Error("Runaway quoted string in response file");

                  // Terminate string, realloc and add to argv
                  *p = 0;
                  newargv[indexinfile++] = realloc(s,strlen(s)+1);
               }
            } while(size > 0);
         }
         free(file);
         
         memcpy((void *)&newargv[indexinfile],moreargs,index*sizeof(moreargs[0]));
         free((void *)moreargs);
         
         myargc = indexinfile+index; myargv = newargv;

         // display args
         printf("%d command-line args:\n", myargc);
         
         for(index = 1; index < myargc; index++)            
            printf("%s\n", myargv[index]);
         
         break;
      }
   }
}

// killough 10/98: moved code to separate function

static void D_ProcessDehCommandLine(void)
{
   // ty 03/09/98 do dehacked stuff
   // Note: do this before any other since it is expected by
   // the deh patch author that this is actually part of the EXE itself
   // Using -deh in BOOM, others use -dehacked.
   // Ty 03/18/98 also allow .bex extension.  .bex overrides if both exist.
   // killough 11/98: also allow -bex

   int p = M_CheckParm ("-deh");
   if(p || (p = M_CheckParm("-bex")))
   {
      // the parms after p are deh/bex file names,
      // until end of parms or another - preceded parm
      // Ty 04/11/98 - Allow multiple -deh files in a row
      // killough 11/98: allow multiple -deh parameters

      boolean deh = true;
      while(++p < myargc)
      {
         if(*myargv[p] == '-')
            deh = !strcasecmp(myargv[p],"-deh") || !strcasecmp(myargv[p],"-bex");
         else
         {
            if(deh)
            {
               char file[PATH_MAX+1];      // killough
               AddDefaultExtension(strcpy(file, myargv[p]), ".bex");
               if(access(file, F_OK))  // nope
               {
                  AddDefaultExtension(strcpy(file, myargv[p]), ".deh");
                  if(access(file, F_OK))  // still nope
                     I_Error("Cannot find .deh or .bex file named %s",
                             myargv[p]);
               }
               // during the beta we have debug output to dehout.txt
               // (apparently, this was never removed after Boom beta-killough)
               //ProcessDehFile(file, D_dehout(), 0);  // killough 10/98
               // haleyjd: queue the file, process it later
               D_QueueDEH(file, 0);
            }
         }
      }
   }
   // ty 03/09/98 end of do dehacked stuff
}

// killough 10/98: support preloaded wads

static void D_ProcessWadPreincludes(void)
{
   if (!M_CheckParm ("-noload"))
   {
      int i;
      char *s;
      for(i = 0; i < MAXLOADFILES; ++i)
         if((s = wad_files[i]))
         {
            while(isspace(*s))
               s++;
            if(*s)
            {
               char file[PATH_MAX+1];
               AddDefaultExtension(strcpy(file, s), ".wad");
               if(!access(file, R_OK))
                  D_AddFile(file);
               else
                  printf("\nWarning: could not open %s\n", file);
            }
         }
   }
}

// killough 10/98: support preloaded deh/bex files

static void D_ProcessDehPreincludes(void)
{
   if(!M_CheckParm ("-noload"))
   {
      int i;
      char *s;
      for(i = 0; i < MAXLOADFILES; i++)
      {
         if((s = deh_files[i]))
         {
            while(isspace(*s))
               s++;
            if(*s)
            {
               char file[PATH_MAX+1];
               AddDefaultExtension(strcpy(file, s), ".bex");
               if(!access(file, R_OK))
                  D_QueueDEH(file, 0); // haleyjd: queue it
               else
               {
                  AddDefaultExtension(strcpy(file, s), ".deh");
                  if(!access(file, R_OK))
                     D_QueueDEH(file, 0); // haleyjd: queue it
                  else
                     printf("\nWarning: could not open %s .deh or .bex\n", s);
               }
            } // end if(*s)
         } // end if((s = deh_files[i]))
      } // end for
   } // end if
}

// haleyjd: auto-executed console scripts

static void D_AutoExecScripts(void)
{
   if(!M_CheckParm("-nocscload")) // separate param from above
   {
      int i;
      char *s;
      for(i = 0; i < MAXLOADFILES; ++i)
         if((s = csc_files[i]))
         {
            while(isspace(*s))
               s++;
            if(*s)
            {
               char file[PATH_MAX+1];
               AddDefaultExtension(strcpy(file, s), ".csc");
               if(!access(file, R_OK))
                  C_RunScriptFromFile(file);
               else
                  usermsg("\nWarning: could not open console script %s\n", s);
            }
         }
   }
}

// killough 10/98: support .deh from wads
//
// A lump named DEHACKED is treated as plaintext of a .deh file embedded in
// a wad (more portable than reading/writing info.c data directly in a wad).
//
// If there are multiple instances of "DEHACKED", we process each, in first
// to last order (we must reverse the order since they will be stored in
// last to first order in the chain). Passing NULL as first argument to
// ProcessDehFile() indicates that the data comes from the lump number
// indicated by the third argument, instead of from a file.

// haleyjd 10/20/03: restored to MBF semantics in order to support
// queueing of dehacked lumps without trouble because of reordering
// of the wad directory by W_InitMultipleFiles.

static void D_ProcessDehInWad(int i)
{
   if(i >= 0)
   {
      D_ProcessDehInWad(lumpinfo[i]->next);
      if(!strncasecmp(lumpinfo[i]->name, "dehacked", 8) &&
         lumpinfo[i]->li_namespace == ns_global)
         D_QueueDEH(NULL, i); // haleyjd: queue it
   }
}

static void D_ProcessDehInWads(void)
{
   // haleyjd: start at the top of the hash chain
   lumpinfo_t *root = 
      lumpinfo[W_LumpNameHash("dehacked") % (unsigned)numlumps]; 
   
   D_ProcessDehInWad(root->index);
}

// haleyjd 03/10/03: GFS functions

static void D_ProcessGFSDeh(gfs_t *gfs)
{
   int i;
   char filename[PATH_MAX + 1];

   for(i = 0; i < gfs->numdehs; ++i)
   {
      memset(filename, 0, PATH_MAX + 1);

      psnprintf(filename, sizeof(filename),
                "%s/%s", gfs->filepath, gfs->dehnames[i]);
      NormalizeSlashes(filename);

      if(access(filename, F_OK))
         I_Error("Couldn't open .deh or .bex %s\n", filename);

      D_QueueDEH(filename, 0); // haleyjd: queue it
   }
}

static void D_ProcessGFSWads(gfs_t *gfs)
{
   int i;
   char filename[PATH_MAX + 1];

   // haleyjd 06/21/04: GFS should mark modified game when
   // wads are added!
   if(gfs->numwads > 0)
      modifiedgame = true;

   for(i = 0; i < gfs->numwads; ++i)
   {
      memset(filename, 0, PATH_MAX + 1);

      psnprintf(filename, sizeof(filename),
                "%s/%s", gfs->filepath, gfs->wadnames[i]);
      NormalizeSlashes(filename);

      if(access(filename, F_OK))
         I_Error("Couldn't open WAD file %s\n", filename);

      D_AddFile(filename);
   }
}

static void D_ProcessGFSCsc(gfs_t *gfs)
{
   int i;
   char filename[PATH_MAX + 1];

   for(i = 0; i < gfs->numcsc; ++i)
   {
      memset(filename, 0, PATH_MAX + 1);

      psnprintf(filename, sizeof(filename),
                "%s/%s", gfs->filepath, gfs->cscnames[i]);
      NormalizeSlashes(filename);

      if(access(filename, F_OK))
         I_Error("Couldn't open CSC file %s\n", filename);
      
      C_RunScriptFromFile(filename);
   }
}

//
// D_LooseEDF
//
// Looks for a loose EDF file on the command line, to support
// drag-and-drop.
//
static boolean D_LooseEDF(char *buffer)
{
   int i;
   const char *dot;

   for(i = 1; i < myargc; ++i)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check extension
      if(!dot || strncasecmp(dot, ".edf", 4))
         continue;

      strncpy(buffer, myargv[i], PATH_MAX + 1);
      return true; // process only the first EDF found
   }

   return false;
}

//
// D_LoadEDF
//
// Identifies the root EDF file, and then calls E_ProcessEDF.
//
static void D_LoadEDF(gfs_t *gfs)
{
   int i;
   char edfname[PATH_MAX + 1];
   const char *shortname = NULL;

   // command line takes utmost precedence
   if((i = M_CheckParm("-edf")) && i < myargc - 1)
   {
      // command-line EDF file found
      strncpy(edfname, myargv[i+1], sizeof(edfname));
   }
   else if(gfs && (shortname = G_GFSCheckEDF()))
   {
      // GFS specified an EDF file
      psnprintf(edfname, sizeof(edfname), "%s/%s", gfs->filepath, shortname);
   }
   else
   {
      // use default
      if(!D_LooseEDF(edfname)) // check for loose files (drag and drop)
      {
         psnprintf(edfname, sizeof(edfname), "%s/%s", 
                   D_DoomExeDir(), "root.edf");

         // disable other game modes' definitions implicitly ONLY
         // when using the default root.edf
         // also, allow command line toggle
         if(!M_CheckParm("-edfenables"))
         {
            if(gameModeInfo->type == Game_Heretic)
               E_EDFSetEnableValue("DOOM", 0);
            else
               E_EDFSetEnableValue("HERETIC", 0);
         }
      }
   }
   
   NormalizeSlashes(edfname);
   
   E_ProcessEDF(edfname);

   // haleyjd FIXME: temporary hacks
   D_InitGameInfo();
   D_InitWeaponInfo();
}

// loose file support functions -- these enable drag-and-drop support
// for Windows and possibly other OSes

static void D_LooseWads(void)
{
   int i;
   const char *dot;
   char filename[PATH_MAX + 1];

   for(i = 1; i < myargc; ++i)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check extension
      if(!dot || strncasecmp(dot, ".wad", 4))
         continue;

      // add it
      strncpy(filename, myargv[i], PATH_MAX + 1);
      NormalizeSlashes(filename);
      modifiedgame = true;
      D_AddFile(filename);
   }
}

static void D_LooseDehs(void)
{
   int i;
   const char *dot;
   char filename[PATH_MAX + 1];

   for(i = 1; i < myargc; ++i)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check extension
      if(!dot || (strncasecmp(dot, ".deh", 4) &&
                  strncasecmp(dot, ".bex", 4))) 
         continue;

      // add it
      strncpy(filename, myargv[i], PATH_MAX + 1);
      NormalizeSlashes(filename);
      D_QueueDEH(filename, 0);
   }
}

static gfs_t *D_LooseGFS(void)
{
   int i;
   const char *dot;

   for(i = 1; i < myargc; ++i)
   {
      // stop at first param with '-' or '@'
      if(myargv[i][0] == '-' || myargv[i][0] == '@')
         break;

      // get extension (search from right end)
      dot = strrchr(myargv[i], '.');

      // check extension
      if(!dot || strncasecmp(dot, ".gfs", 4))
         continue;

      printf("Found loose GFS file %s\n", myargv[i]);
      
      // process only the first GFS found
      return G_LoadGFS(myargv[i]);
   }

   return NULL;
}

        //sf:
void startupmsg(char *func, char *desc)
{
   // add colours in console mode
   usermsg(in_textmode ? "%s: %s" : FC_HI "%s: " FC_NORMAL "%s",
           func, desc);
}

// sf: this is really part of D_DoomMain but I made it into
// a seperate function
// this stuff needs to be kept together

void D_SetGraphicsMode(void)
{
   int p, mode;

   DEBUGMSG("** set graphics mode\n");

   // haleyjd 04/07/04: v_mode command-line override
   if((p = M_CheckParm("-v_mode")) && p < myargc - 1)
   {
      mode = atoi(myargv[p + 1]);

      if(mode >= 0 && mode < V_NumModes())
         v_mode = mode;
   }
   
   // set graphics mode
   I_InitGraphics();
   
   DEBUGMSG("done\n");
   
   // set up the console to display startup messages
   gamestate = GS_CONSOLE; 
   current_height = SCREENHEIGHT;
   c_showprompt = false;
   
   C_Puts(game_name);    // display description of gamemode
   D_ListWads();         // list wads to the console
   C_Printf("\n");       // leave a gap
}

#ifdef GAMEBAR
// print title for every printed line
static char title[128];
#endif

extern int levelTimeLimit;
extern int levelFragLimit;

static void D_StartupMessage(void)
{
   puts("The Eternity Engine\n"
        "Copyright 2006 James Haley and Steven McGranahan\n"
        "http://www.doomworld.com/eternity\n"
        "\n"
        "This program is free software distributed under the terms of\n"
        "the GNU General Public License. See the file \"COPYING\" for\n"
        "full details. Commercial sale or distribution of this product\n"
        "without its license, source code, and copyright notices is an\n"
        "infringement of US and international copyright laws.\n");
}

// haleyjd 11/12/05: in cdrom mode?
boolean cdrom_mode = false;

//
// D_DoomInit
//
// Broke D_DoomMain into two functions in order to keep 
// initialization stuff off the main line of execution.
//
static void D_DoomInit(void)
{
   int p, slot;
   char file[PATH_MAX+1];      // killough 3/22/98
   int dmtype = 0;             // haleyjd 04/14/03
   boolean haveGFS = false;    // haleyjd 03/10/03
   gfs_t *gfs = NULL;

   D_StartupMessage();

#ifdef GAMEBAR
   // haleyjd 01/17/05: I think this is only needed for the "gamebar" option
   setbuf(stdout, NULL);
#endif

   FindResponseFile(); // Append response file arguments to command-line

   // haleyjd 11/12/05: moved -cdrom check up and made status global
#ifdef EE_CDROM_SUPPORT
   if(M_CheckParm("-cdrom"))
      cdrom_mode = true;
#endif
   
   // haleyjd 03/10/03: GFS support
   // haleyjd 11/22/03: support loose GFS on the command line too
   if((p = M_CheckParm("-gfs")) && p < myargc - 1)
   {
      char fn[PATH_MAX + 1]; 
      
      // haleyjd 01/19/05: corrected use of AddDefaultExtension
      AddDefaultExtension(strcpy(fn, myargv[p + 1]), ".gfs");
      if(access(fn, F_OK))
         I_Error("GFS file %s not found\n", fn);
      
      printf("Parsing GFS file %s\n", fn);
      
      gfs = G_LoadGFS(fn);
      haveGFS = true;
   }
   else
   {
      // look for a loose GFS for drag-and-drop support
      if((gfs = D_LooseGFS()))
         haveGFS = true;
   }

   // haleyjd: init the dehacked queue (only necessary the first time)
   D_DEHQueueInit();

   // haleyjd 11/22/03: look for loose DEH files (drag and drop)
   D_LooseDehs();

   // killough 10/98: process all command-line DEH's first
   // haleyjd  09/03: this just queues them now
   D_ProcessDehCommandLine();
   
   // haleyjd 09/11/03: queue GFS DEH's
   if(haveGFS)
   {
      D_ProcessGFSDeh(gfs);
   }

   // killough 10/98: set default savename based on executable's name
   // haleyjd 08/28/03: must be done BEFORE bex hash chain init!
   savegamename = Z_Malloc(16, PU_STATIC, NULL);
   psnprintf(savegamename, 16, "%.4ssav", D_DoomExeName());
   
   devparm = !!M_CheckParm ("-devparm");         //sf: move up here
      
   IdentifyVersion();
   printf("\n"); // gap
   
   modifiedgame = false;
   
   // jff 1/24/98 set both working and command line value of play parms
   // sf: make boolean for console
   nomonsters = clnomonsters = !!M_CheckParm ("-nomonsters");
   respawnparm = clrespawnparm = !!M_CheckParm ("-respawn");
   fastparm = clfastparm = !!M_CheckParm ("-fast");
   // jff 1/24/98 end of set to both working and command line value

   DefaultGameType = gt_single;

   if(M_CheckParm("-deathmatch"))
   {
      DefaultGameType = gt_dm;
      dmtype = 1;
   }
   if(M_CheckParm("-altdeath"))
   {
      DefaultGameType = gt_dm;
      dmtype = 2;
   }
   if(M_CheckParm("-trideath"))  // deathmatch 3.0!
   {
      DefaultGameType = gt_dm;
      dmtype = 3;
   }

   GameType = DefaultGameType;
   G_SetDefaultDMFlags(dmtype, true);

#ifdef GAMEBAR
   switch(gamemode)
   {
   case retail:
      psnprintf(title, sizeof(title), "The Ultimate DOOM Startup");
      break;
   case shareware:
      psnprintf(title, sizeof(title), "DOOM Shareware Startup");
      break;	
   case registered:
      psnprintf(title, sizeof(title), "DOOM Registered Startup");
      break;	
   case commercial:
      switch(gamemission)      // joel 10/16/98 Final DOOM fix
      {
      case pack_plut:
         psnprintf(title, sizeof(title), "DOOM 2: Plutonia Experiment");
         break;
         
      case pack_tnt:
         psnprintf(title, sizeof(title), "DOOM 2: TNT - Evilution");
         break;
         
      case doom2:
      default:
         psnprintf(title, sizeof(title), "DOOM 2: Hell on Earth");
         break;
      }
      break;
      // joel 10/16/98 end Final DOOM fix

   case hereticsw:
      psnprintf(title, sizeof(title), "Heretic Shareware Startup");
      break;

   case hereticreg:
      switch(gamemission)
      {
      default:
      case heretic:
         psnprintf(title, sizeof(title), "Heretic Registered Startup");
         break;
      case hticsosr:
         psnprintf(title, sizeof(title), "Heretic: Shadow of the Serpent Riders");
         break;
      }
      break;
	
   default:
      psnprintf(title, sizeof(title), "Public DOOM");
      break;
   }
   printf("%s\n", title);
   printf("%s\nBuilt on %s at %s\n", title, version_date, 
          version_time);    // killough 2/1/98
#else
   // haleyjd: always provide version date/time
   printf("Built on %s at %s\n", version_date, version_time);
#endif /* GAMEBAR */

   if(devparm)
   {
      printf(D_DEVSTR);
      v_ticker = true;  // turn on the fps ticker
   }

#ifdef EE_CDROM_SUPPORT
   // sf: ok then, this is broken under linux.
   // haleyjd: FIXME
   if(cdrom_mode)
   {
#ifndef DJGPP
      mkdir("c:/doomdata");
#else
      mkdir("c:/doomdata", 0);
#endif      
      // killough 10/98:
      psnprintf(basedefault, sizeof(basedefault),
                "c:/doomdata/%s.cfg", D_DoomExeName());
   }
#endif

   // turbo option
   if((p = M_CheckParm("-turbo")))
   {
      extern int turbo_scale;
      extern int forwardmove[2];
      extern int sidemove[2];
      
      if(p < myargc - 1)
         turbo_scale = atoi(myargv[p + 1]);
      if(turbo_scale < 10)
         turbo_scale = 10;
      if(turbo_scale > 400)
         turbo_scale = 400;
      printf ("turbo scale: %i%%\n",turbo_scale);
      forwardmove[0] = forwardmove[0] * turbo_scale / 100;
      forwardmove[1] = forwardmove[1] * turbo_scale / 100;
      sidemove[0]    =    sidemove[0] * turbo_scale / 100;
      sidemove[1]    =    sidemove[1] * turbo_scale / 100;
   }

   // haleyjd 03/10/03: Load GFS Wads
   // 08/08/03: moved first, so that command line overrides
   if(haveGFS)
   {
      D_ProcessGFSWads(gfs);
   }

   // haleyjd 11/22/03: look for loose wads (drag and drop)
   D_LooseWads();

   // add any files specified on the command line with -file wadfile
   // to the wad list

   // killough 1/31/98, 5/2/98: reload hack removed, -wart same as -warp now.

   if((p = M_CheckParm("-file")))
   {
      // the parms after p are wadfile/lump names,
      // until end of parms or another - preceded parm
      // killough 11/98: allow multiple -file parameters
      
      boolean file = modifiedgame = true; // homebrew levels
      while(++p < myargc)
      {
         if(*myargv[p] == '-')
         {
            file = !strcasecmp(myargv[p],"-file");
         }
         else
         {
            if(file)
               D_AddFile(myargv[p]);
         }
      }
   }

   if(!(p = M_CheckParm("-playdemo")) || p >= myargc-1)   // killough
   {
      if((p = M_CheckParm("-fastdemo")) && p < myargc-1)  // killough
         fastdemo = true;            // run at fastest speed possible
      else
         p = M_CheckParm("-timedemo");
   }

   if(p && p < myargc - 1)
   {
      strcpy(file,myargv[p + 1]);
      AddDefaultExtension(file,".lmp");     // killough
      D_AddFile(file);
      usermsg("Playing demo %s\n",file);
   }

   // get skill / episode / map from parms

   // jff 3/24/98 was sk_medium, just note not picked
   startskill = sk_none;
   startepisode = 1;
   startmap = 1;
   autostart = false;
   
   if((p = M_CheckParm("-skill")) && p < myargc - 1)
   {
      startskill = myargv[p+1][0]-'1';
      autostart = true;
   }

   if((p = M_CheckParm("-episode")) && p < myargc - 1)
   {
      startepisode = myargv[p+1][0]-'0';
      startmap = 1;
      autostart = true;
   }

   if((p = M_CheckParm("-timer")) && p < myargc-1 && (GameType == gt_dm))
   {
      int time = atoi(myargv[p+1]);
      
      usermsg("Levels will end after %d minute%s.\n", 
              time, time > 1 ? "s" : "");
      levelTimeLimit = time;
   }

   // sf: moved from p_spec.c
   // See if -frags has been used

   p = M_CheckParm("-frags");
   if(p && GameType == gt_dm)
   {
      int frags = atoi(myargv[p+1]);
      
      if (frags <= 0) frags = 10;  // default 10 if no count provided
      levelFragLimit = frags;
   }

   if((p = M_CheckParm("-avg")) && p < myargc-1 && GameType == gt_dm)
   {  
      levelTimeLimit = 20 * 60 * TICRATE;
      puts("Austin Virtual Gaming: Levels will end after 20 minutes");
   }

   if(((p = M_CheckParm ("-warp")) ||      // killough 5/2/98
       (p = M_CheckParm ("-wart"))) && p < myargc - 1)
   {
      // 1/25/98 killough: fix -warp xxx from crashing Doom 1 / UD
      if(gamemode == commercial)
      {
         startmap = atoi(myargv[p + 1]);
         autostart = true;
      }
      else if(p < myargc - 2)
      {         
         startepisode = atoi(myargv[++p]);
         startmap = atoi(myargv[p + 1]);
         autostart = true;
      }
   }

   //jff 1/22/98 add command line parms to disable sound and music
   {
      boolean nosound = !!M_CheckParm("-nosound");
      nomusicparm = nosound || M_CheckParm("-nomusic");
      nosfxparm   = nosound || M_CheckParm("-nosfx");
   }
   //jff end of sound/music command line parms

   // killough 3/2/98: allow -nodraw -noblit generally
   nodrawers = !!M_CheckParm ("-nodraw");
   noblit = !!M_CheckParm ("-noblit");
   
   if(M_CheckParm("-debugfile")) // sf: debugfile check earlier
   {
      char filename[20];
      psnprintf(filename, sizeof(filename),
                "debug%i.txt", consoleplayer);
      usermsg("debug output to: %s\n", filename);
      debugfile = fopen(filename, "w");
   }

   if(M_CheckParm("-debugstd"))
   {
      usermsg("debug output to stdout\n");
      debugfile = stdout;
   }
   
   // haleyjd: need to do this before M_LoadDefaults
   C_InitPlayerName();

   startupmsg("M_LoadDefaults", "Load system defaults.");
   M_LoadDefaults();              // load before initing other systems
   
   bodyquesize = default_bodyquesize; // killough 10/98
   
   G_ReloadDefaults();    // killough 3/4/98: set defaults just loaded.
   // jff 3/24/98 this sets startskill if it was -1

   // 1/18/98 killough: Z_Init call moved to i_main.c
   
   // init subsystems
   startupmsg("V_Init", "Allocate screens.");    // killough 11/98: moved down to here
   V_Init();
   
   D_ProcessWadPreincludes(); // killough 10/98: add preincluded wads at the end
      
   startupmsg("W_Init", "Init WADfiles.");
   W_InitMultipleFiles(wadfiles);
   usermsg("");  // gap

   // Check for -file in shareware
   //
   // haleyjd 03/22/03: there's no point in trying to detect
   // fake IWADs, especially after user wads have already been
   // linked in, so I've removed that kludge
   if(modifiedgame && (gameModeInfo->flags & GIF_SHAREWARE))
      I_Error("\nYou cannot -file with the shareware version. Register!");

   // haleyjd 10/20/03: use D_ProcessDehInWads again
   D_ProcessDehInWads();

   // killough 10/98: process preincluded .deh files
   // haleyjd  09/03: this just queues them now
   D_ProcessDehPreincludes();

   // haleyjd 09/11/03: All EDF and DeHackEd processing is now
   // centralized here, in order to allow EDF to load from wads.
   // As noted in comments, the other DEH functions above now add
   // their files or lumps to the queue in d_dehtbl.c -- the queue
   // is processed here to parse all files/lumps at once.
   
   // Init bex hash chaining before EDF
   D_BuildBEXHashChains();

   // Identify root EDF file and process EDF
   D_LoadEDF(gfs);

   // Build BEX tables (some are EDF-dependent)
   D_BuildBEXTables();

   // Process the DeHackEd queue, then free it
   D_ProcessDEHQueue();

   // Process the deferred wad sounds queue, after the above stuff.
   S_ProcDeferredSounds();

   V_InitColorTranslation(); //jff 4/24/98 load color translation lumps

   // killough 2/22/98: copyright / "modified game" / SPA banners removed

   // haleyjd 11/13/05: moved down CD-ROM mode notice and changed to
   // use BEX string like it really always should have. Because it was
   // previously before the point where DEH/BEX was loaded, it couldn't
   // be done.
   if(cdrom_mode)
      puts(s_D_CDROM);
   
   // Ty 04/08/98 - Add 5 lines of misc. data, only if nonblank
   // The expectation is that these will be set in a .bex file
   
   // haleyjd: in order for these to play the appropriate role, they
   //  should appear in the console if not in text mode startup
   
   if(textmode_startup)
   {
      if(*startup1) puts(startup1);
      if(*startup2) puts(startup2);
      if(*startup3) puts(startup3);
      if(*startup4) puts(startup4);
      if(*startup5) puts(startup5);
   }
   // End new startup strings

   startupmsg("C_Init","Init console.");
   C_Init();
   
   startupmsg("V_InitMisc","Init miscellaneous video patches.");
   V_InitMisc();
   
   startupmsg("I_Init","Setting up machine state.");
   I_Init();

   // devparm override of early set graphics mode
   if(!textmode_startup && !devparm)
   {
      startupmsg("D_SetGraphicsMode", "Set graphics mode");
      D_SetGraphicsMode();
   }
  
   startupmsg("R_Init","Init DOOM refresh daemon");
   R_Init();
   
   startupmsg("P_Init","Init Playloop state.");
   P_Init();
   
   startupmsg("HU_Init","Setting up heads up display.");
   HU_Init();
   
   startupmsg("ST_Init","Init status bar.");
   ST_Init();

   startupmsg("MN_Init","Init menu.");
   MN_Init();
   
   startupmsg("S_Init","Setting up sound.");
   S_Init(snd_SfxVolume, snd_MusicVolume);

   //
   // NETCODE_FIXME: Netgame check.
   //
   
   startupmsg("D_CheckNetGame","Check netgame status.");
   D_CheckNetGame();

   // haleyjd 04/10/03
   if(netgame && GameType == gt_single)
   {
      GameType = DefaultGameType = gt_coop;
      G_SetDefaultDMFlags(0, true);
   }

   // check for command-line override of dmflags
   if((p = M_CheckParm("-dmflags")) && p < myargc-1)
   {
      dmflags = default_dmflags = (unsigned long)atoi(myargv[p+1]);
   }

   // haleyjd: this SHOULD be late enough...
   startupmsg("G_LoadDefaults", "Init keybindings.");
   G_LoadDefaults();

   //
   // CONSOLE_FIXME: This may not be the best time for scripts.
   // Reconsider this as is appropriate.
   //
   
   // haleyjd: AFTER keybindings for overrides
   startupmsg("D_AutoExecScripts", "Executing console scripts.");
   D_AutoExecScripts();

   // haleyjd 03/10/03: GFS csc's
   if(haveGFS)
   {
      D_ProcessGFSCsc(gfs);
      
      // this is the last GFS action, so free the gfs now
      G_FreeGFS(gfs);
      haveGFS = false;
   }

   // haleyjd: GFS is no longer valid from here!
   
   if(devparm) // we wait if in devparm so the user can see the messages
   {
      printf("devparm: press a key..\n");
      getchar();
   }

   ///////////////////////////////////////////////////////////////////
   //
   // Must be in Graphics mode by now!
   //
   
   // check
   
   if(in_textmode)
      D_SetGraphicsMode();

   // initialize Small, load game scripts
   A_InitSmall();
   A_InitGameScript();

   // haleyjd: updated for eternity
   C_Printf("\n");
   C_Separator();
   C_Printf("\n"
            FC_HI "The Eternity Engine\n"
            FC_NORMAL "By James Haley and Stephen McGranahan\n"
            "http://doomworld.com/eternity/ \n"
            "Version %i.%02i.%02i '%s' \n\n",
            version/100, version%100, SUBVERSION, version_name);

   // haleyjd: if we didn't do textmode startup, these didn't show up
   //  earlier, so now is a cool time to show them :)
   // haleyjd: altered to prevent printf attacks
   if(!textmode_startup)
   {
      if(*startup1) C_Printf("%s", startup1);
      if(*startup2) C_Printf("%s", startup2);
      if(*startup3) C_Printf("%s", startup3);
      if(*startup4) C_Printf("%s", startup4);
      if(*startup5) C_Printf("%s", startup5);
   }
   
   if(!textmode_startup && !devparm)
      C_Update();

   idmusnum = -1; //jff 3/17/98 insure idmus number is blank

#if 0
   // check for a driver that wants intermission stats
   if((p = M_CheckParm("-statcopy")) && p<myargc-1)
   {
      // for statistics driver
      extern void *statcopy;
      
      // killough 5/2/98: this takes a memory
      // address as an integer on the command line!
      
      statcopy = (void *)atoi(myargv[p+1]);
      usermsg("External statistics registered.");
   }
#endif

   // sf: -blockmap option as a variable now
   if(M_CheckParm("-blockmap")) r_blockmap = true;
   
   // start the appropriate game based on parms
   
   // killough 12/98: 
   // Support -loadgame with -record and reimplement -recordfrom.
   if((slot = M_CheckParm("-recordfrom")) && (p = slot+2) < myargc)
      G_RecordDemo(myargv[p]);
   else
   {
      slot = M_CheckParm("-loadgame");
      if((p = M_CheckParm("-record")) && ++p < myargc)
      {
         autostart = true;
         G_RecordDemo(myargv[p]);
      }
   }

   if((p = M_CheckParm ("-fastdemo")) && ++p < myargc)
   {                                 // killough
      fastdemo = true;                // run at fastest speed possible
      timingdemo = true;              // show stats after quit
      G_DeferedPlayDemo(myargv[p]);
      singledemo = true;              // quit after one demo
   }
   else if((p = M_CheckParm("-timedemo")) && ++p < myargc)
   {
      G_TimeDemo(myargv[p], false);
   }
   else if((p = M_CheckParm("-playdemo")) && ++p < myargc)
   {
      G_DeferedPlayDemo(myargv[p]);
      singledemo = true;          // quit after one demo
   }

   DEBUGMSG("start gamestate: title screen etc.\n");

   startlevel = Z_Strdup(G_GetNameForMap(startepisode, startmap), PU_STATIC, 0);

   if(slot && ++slot < myargc)
   {
      slot = atoi(myargv[slot]);        // killough 3/16/98: add slot info
      G_SaveGameName(file, sizeof(file), slot); // killough 3/22/98
      G_LoadGame(file, slot, true);     // killough 5/15/98: add command flag
   }
   else if(!singledemo)                    // killough 12/98
   {
      if(netgame)
      {
         //
         // NETCODE_FIXME: C_SendNetData.
         //
         C_SendNetData();
         
         if(demorecording)
            G_BeginRecording();
      }
      else if(autostart)
      {
         G_InitNew(startskill, startlevel);
         
         if(demorecording)
            G_BeginRecording();
      }
      else
      {
         D_StartTitle();                 // start up intro loop
      }
   }
}


//
// D_DoomMain
//
void D_DoomMain(void)
{
   D_DoomInit();
   
   DEBUGMSG("start main loop\n");
   
   oldgamestate = wipegamestate = gamestate;

   // haleyjd 02/23/04: fix problems with -warp
   if(autostart)
   {
      oldgamestate = -1;
      redrawborder = true;
   }

   // killough 12/98: inlined D_DoomLoop

   while(1)
   {
      // frame synchronous IO operations
      I_StartFrame();
      
      DEBUGMSG("tics\n");
      TryRunTics(); // will run at least one tic
      
      DEBUGMSG("sound\n");
      
      // killough 3/16/98: change consoleplayer to displayplayer
      S_UpdateSounds(players[displayplayer].mo);// move positional sounds
      
      DEBUGMSG("display\n");
      
      // Update display, next frame, with current state.
      D_Display();
      
      DEBUGMSG("more sound\n");
      
      // Sound mixing for the buffer is synchronous.
      I_UpdateSound();
      
      // Synchronous sound output is explicitly called.
      // Update sound output.
      I_SubmitSound();
   }
}

// re init everything after loading a wad

void D_ReInitWadfiles(void)
{
   R_FreeData();
   E_ProcessEDFLumps(); // haleyjd 07/24/05: reproc. optional EDF lumps
   D_ProcessDEHQueue(); // haleyjd 09/12/03: run any queued DEHs
   R_Init();
   P_Init();
}

// FIXME: various parts of this routine need tightening up
void D_NewWadLumps(int handle, int sound_update_type)
{
   int i, format;
   char wad_firstlevel[9];
   
   memset(wad_firstlevel, 0, 9);
  
   for(i = 0; i < numlumps; ++i)
   {
      if(lumpinfo[i]->handle != handle)
         continue;
      
      // haleyjd: changed check for "THINGS" lump to a fullblown
      // P_CheckLevel call -- this should fix some problems with
      // some crappy wads that have partial levels sitting around
      
      if((format = P_CheckLevel(i)) != LEVEL_FORMAT_INVALID) // a level
      {
         char *name = lumpinfo[i]->name;

         // ignore ones called 'start' as these are checked
         // elsewhere (m_menu.c)
         if((!*wad_firstlevel && strcmp(name, "START")) ||
            strncmp(name, wad_firstlevel, 8) < 0)
            strncpy(wad_firstlevel, name, 8);

         // haleyjd: move up to the level's last lump
         i += (format == LEVEL_FORMAT_HEXEN ? 11 : 10);
         continue;
      }
      
      // new sound
      if(!strncmp(lumpinfo[i]->name, "DSCHGUN",8)) // chaingun sound
      {
         S_Chgun();
         continue;
      }

      if(!strncmp(lumpinfo[i]->name, "DS", 2))
      {
         switch(sound_update_type)
         {
         case 0: // called during startup, defer processing
            S_UpdateSoundDeferred(i);
            break;
         case 1: // called during gameplay
            S_UpdateSound(i);
            break;
         }
         continue;
      }
      
      // new music
      if(!strncmp(lumpinfo[i]->name, gameModeInfo->musPrefix,
                  strlen(gameModeInfo->musPrefix)))
      {
         S_UpdateMusic(i);
         continue;
      }

      // skins
      if(!strncmp(lumpinfo[i]->name, "S_SKIN", 6))
      {
         P_ParseSkin(i);
         continue;
      }
   } 
  
   if(*wad_firstlevel && (!*firstlevel ||
      strncmp(wad_firstlevel, firstlevel, 8) < 0)) // a new first level?
      strcpy(firstlevel, wad_firstlevel);
}

void usermsg(const char *s, ...)
{
  static char msg[1024];
  va_list v;

  va_start(v,s);
  pvsnprintf(msg, sizeof(msg), s, v); // print message in buffer
  va_end(v);

  if(in_textmode)
  {
     puts(msg);
  }
  else
  {
     C_Puts(msg);
     C_Update();
  }
}

// add a new .wad file
// returns true if successfully loaded

boolean D_AddNewFile(char *s)
{
  c_showprompt = false;
  if(W_AddNewFile(s)) 
     return false;
  modifiedgame = true;
  D_AddFile(s);   // add to the list of wads
  C_SetConsole();
  D_ReInitWadfiles();
  return true;
}

//----------------------------------------------------------------------------
//
// $Log: d_main.c,v $
// Revision 1.47  1998/05/16  09:16:51  killough
// Make loadgame checksum friendlier
//
// Revision 1.46  1998/05/12  10:32:42  jim
// remove LEESFIXES from d_main
//
// Revision 1.45  1998/05/06  15:15:46  jim
// Documented IWAD routines
//
// Revision 1.44  1998/05/03  22:26:31  killough
// beautification, declarations, headers
//
// Revision 1.43  1998/04/24  08:08:13  jim
// Make text translate tables lumps
//
// Revision 1.42  1998/04/21  23:46:01  jim
// Predefined lump dumper option
//
// Revision 1.39  1998/04/20  11:06:42  jim
// Fixed print of IWAD found
//
// Revision 1.37  1998/04/19  01:12:19  killough
// Fix registered check to work with new lump namespaces
//
// Revision 1.36  1998/04/16  18:12:50  jim
// Fixed leak
//
// Revision 1.35  1998/04/14  08:14:18  killough
// Remove obsolete adaptive_gametics code
//
// Revision 1.34  1998/04/12  22:54:41  phares
// Remaining 3 Setup screens
//
// Revision 1.33  1998/04/11  14:49:15  thldrmn
// Allow multiple deh/bex files
//
// Revision 1.32  1998/04/10  06:31:50  killough
// Add adaptive gametic timer
//
// Revision 1.31  1998/04/09  09:18:17  thldrmn
// Added generic startup strings for BEX use
//
// Revision 1.30  1998/04/06  04:52:29  killough
// Allow demo_insurance=2, fix fps regression wrt redrawsbar
//
// Revision 1.29  1998/03/31  01:08:11  phares
// Initial Setup screens and Extended HELP screens
//
// Revision 1.28  1998/03/28  15:49:37  jim
// Fixed merge glitches in d_main.c and g_game.c
//
// Revision 1.27  1998/03/27  21:26:16  jim
// Default save dir offically . now
//
// Revision 1.26  1998/03/25  18:14:21  jim
// Fixed duplicate IWAD search in .
//
// Revision 1.25  1998/03/24  16:16:00  jim
// Fixed looking for wads message
//
// Revision 1.23  1998/03/24  03:16:51  jim
// added -iwad and -save parms to command line
//
// Revision 1.22  1998/03/23  03:07:44  killough
// Use G_SaveGameName, fix some remaining default.cfg's
//
// Revision 1.21  1998/03/18  23:13:54  jim
// Deh text additions
//
// Revision 1.19  1998/03/16  12:27:44  killough
// Remember savegame slot when loading
//
// Revision 1.18  1998/03/10  07:14:58  jim
// Initial DEH support added, minus text
//
// Revision 1.17  1998/03/09  07:07:45  killough
// print newline after wad files
//
// Revision 1.16  1998/03/04  08:12:05  killough
// Correctly set defaults before recording demos
//
// Revision 1.15  1998/03/02  11:24:25  killough
// make -nodraw -noblit work generally, fix ENDOOM
//
// Revision 1.14  1998/02/23  04:13:55  killough
// My own fix for m_misc.c warning, plus lots more (Rand's can wait)
//
// Revision 1.11  1998/02/20  21:56:41  phares
// Preliminarey sprite translucency
//
// Revision 1.10  1998/02/20  00:09:00  killough
// change iwad search path order
//
// Revision 1.9  1998/02/17  06:09:35  killough
// Cache D_DoomExeDir and support basesavegame
//
// Revision 1.8  1998/02/02  13:20:03  killough
// Ultimate Doom, -fastdemo -nodraw -noblit support, default_compatibility
//
// Revision 1.7  1998/01/30  18:48:15  phares
// Changed textspeed and textwait to functions
//
// Revision 1.6  1998/01/30  16:08:59  phares
// Faster end-mission text display
//
// Revision 1.5  1998/01/26  19:23:04  phares
// First rev with no ^Ms
//
// Revision 1.4  1998/01/26  05:40:12  killough
// Fix Doom 1 crashes on -warp with too few args
//
// Revision 1.3  1998/01/24  21:03:04  jim
// Fixed disappearence of nomonsters, respawn, or fast mode after demo play or IDCLEV
//
// Revision 1.1.1.1  1998/01/19  14:02:53  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

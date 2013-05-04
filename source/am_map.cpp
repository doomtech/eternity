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
//   the automap code
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "am_map.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"    // Ty 03/27/98 - externalizations
#include "d_dehtbl.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_main.h"
#include "doomstat.h"
#include "dstrings.h"
#include "g_bind.h"
#include "p_maputl.h"
#include "p_portal.h"
#include "p_setup.h"
#include "p_spec.h"
#include "st_stuff.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_portal.h"
#include "r_state.h"
#include "v_block.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"


//jff 1/7/98 default acolors added
int mapcolor_back;    // map background
int mapcolor_grid;    // grid lines color
int mapcolor_wall;    // normal 1s wall color
int mapcolor_fchg;    // line at floor height change color
int mapcolor_cchg;    // line at ceiling height change color
int mapcolor_clsd;    // line at sector with floor=ceiling color
int mapcolor_rkey;    // red key color
int mapcolor_bkey;    // blue key color
int mapcolor_ykey;    // yellow key color
int mapcolor_rdor;    // red door color  (diff from keys to allow option)
int mapcolor_bdor;    // blue door color (of enabling one but not other )
int mapcolor_ydor;    // yellow door color
int mapcolor_tele;    // teleporter line color
int mapcolor_secr;    // secret sector boundary color
int mapcolor_exit;    // jff 4/23/98 add exit line color
int mapcolor_unsn;    // computer map unseen line color
int mapcolor_flat;    // line with no floor/ceiling changes
int mapcolor_sprt;    // general sprite color
int mapcolor_hair;    // crosshair color
int mapcolor_sngl;    // single player arrow color
int mapcolor_plyr[4]; // colors for player arrows in multiplayer
int mapcolor_frnd;    // colors for friends of player
int mapcolor_prtl;    // SoM: color of lines not in the current portal group

// SoM: map mode. True means the portal groups are overlayed (the group the 
// player is in being displayed in color and the other groups being grayed 
// out and underneath) and false means the map is not modified.
int mapportal_overlay;

//jff 3/9/98 add option to not show secret sectors until entered
int map_secret_after;

//jff 4/3/98 add symbols for "no-color" for disable and "black color" for black
#define NC 0
#define BC 247

// drawing stuff
#define FB    0

// haleyjd 05/17/08: ability to draw node lines on map
bool map_draw_nodelines;

// haleyjd 07/07/04: removed key_map* variables

// scale on entry
#define INITSCALEMTOF (0.2)
// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels in 1 second
#define F_PANINC  4.0
// how much zoom-in per tic
// goes to 2x in 1 second
#define M_ZOOMIN        (1.02)
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
#define M_ZOOMOUT       (1.0/1.02)

// translates between frame-buffer and map distances
#define FTOM(x) ((x) * scale_ftom)
#define MTOF(x) ((x) * scale_mtof)
// translates between frame-buffer and map coordinates
#define CXMTOF(x)  (f_x + (int)(MTOF((x) - m_x)))
#define CYMTOF(y)  (f_y + (f_h - (int)(MTOF((y) - m_y))))

typedef struct fpoint_s
{
   int x, y;
} fpoint_t;

typedef struct fline_s
{
   fpoint_t a, b;
} fline_t;

typedef struct mline_s
{
   mpoint_t a, b;
} mline_t;

typedef struct islope_s
{
   double slp, islp;
} islope_t;

// haleyjd: moved here, as this is the only place it is used
#define PLAYERRADIUS    (16.0)

//
// The vector graphics for the automap.
//  A line drawing of the player pointing right,
//   starting from the middle.
//
#define R ((8*PLAYERRADIUS)/7)
mline_t player_arrow[] =
{
  { { -R+R/8,   0 }, {  R,      0   } }, // -----
  { {  R,       0 }, {  R-R/2,  R/4 } }, // ----->
  { {  R,       0 }, {  R-R/2, -R/4 } },
  { { -R+R/8,   0 }, { -R-R/8,  R/4 } }, // >---->
  { { -R+R/8,   0 }, { -R-R/8, -R/4 } },
  { { -R+3*R/8, 0 }, { -R+R/8,  R/4 } }, // >>--->
  { { -R+3*R/8, 0 }, { -R+R/8, -R/4 } }
};
#undef R
#define NUMPLYRLINES (sizeof(player_arrow)/sizeof(mline_t))

#define R ((8*PLAYERRADIUS)/7)
mline_t cheat_player_arrow[] =
{ // killough 3/22/98: He's alive, Jim :)
  { { -R+R/8,         0   }, {  R,             0   } }, // -----
  { {  R,             0   }, {  R-R/2,         R/4 } }, // ----->
  { {  R,             0   }, {  R-R/2,        -R/4 } },
  { { -R+R/8,         0   }, { -R-R/8,         R/4 } }, // >---->
  { { -R+R/8,         0   }, { -R-R/8,        -R/4 } },
  { { -R+3*R/8,       0   }, { -R+R/8,         R/4 } }, // >>--->
  { { -R+3*R/8,       0   }, { -R+R/8,        -R/4 } },
  { { -R/10-R/6,      R/4 }, { -R/10-R/6,     -R/4 } }, // J
  { { -R/10-R/6,     -R/4 }, { -R/10-R/6-R/8, -R/4 } },
  { { -R/10-R/6-R/8, -R/4 }, { -R/10-R/6-R/8, -R/8 } },
  { { -R/10,          R/4 }, { -R/10,         -R/4 } }, // F
  { { -R/10,          R/4 }, { -R/10+R/8,      R/4 } },
  { { -R/10+R/4,      R/4 }, { -R/10+R/4,     -R/4 } }, // F
  { { -R/10+R/4,      R/4 }, { -R/10+R/4+R/8,  R/4 } },
};
#undef R
#define NUMCHEATPLYRLINES (sizeof(cheat_player_arrow)/sizeof(mline_t))

mline_t triangle_guy[] =
{
  { { -0.867, -0.5 }, {  0.867, -0.5 } },
  { {  0.867, -0.5 }, {  0.0,    1.0 } },
  { {  0.0,    1.0 }, { -0.867, -0.5 } }
};
#define NUMTRIANGLEGUYLINES (sizeof(triangle_guy)/sizeof(mline_t))

//jff 1/5/98 new symbol for keys on automap
mline_t cross_mark[] =
{
  { { -1.0,  0.0 }, { 1.0, 0.0 } },
  { {  0.0, -1.0 }, { 0.0, 1.0 } },
};
#define NUMCROSSMARKLINES (sizeof(cross_mark)/sizeof(mline_t))
//jff 1/5/98 end of new symbol

mline_t thintriangle_guy[] =
{
  { { -0.5, -0.7 }, {  1.0,  0.0 } },
  { {  1.0,  0.0 }, { -0.5,  0.7 } },
  { { -0.5,  0.7 }, { -0.5, -0.7 } }
};
#define NUMTHINTRIANGLEGUYLINES (sizeof(thintriangle_guy)/sizeof(mline_t))

int ddt_cheating = 0;         // killough 2/7/98: make global, rename to ddt_*

int automap_grid = 0;

bool automapactive = false;

// location of window on screen
static int  f_x;
static int  f_y;

// size of window on screen
static int  f_w;
static int  f_h;

// SoM: This should NOT be used anymore.
//static byte*  fb;            // pseudo-frame buffer
static int  lightlev;        // used for funky strobing effect
static int  amclock;

static mpoint_t m_paninc;    // how far the window pans each tic (map coords)
static double mtof_zoommul; // how far the window zooms each tic (map coords)
static double ftom_zoommul; // how far the window zooms each tic (fb coords)

static double m_x,  m_y;    // LL x,y window location on the map (map coords)
static double m_x2, m_y2;   // UR x,y window location on the map (map coords)

//
// width/height of window on map (map coords)
//
static double  m_w;
static double  m_h;

// based on level size
static double  min_x;
static double  min_y; 
static double  max_x;
static double  max_y;

static double  max_w;          // max_x-min_x,
static double  max_h;          // max_y-min_y

// based on player size
static double  min_w;
static double  min_h;

static double  min_scale_mtof; // used to tell when to stop zooming out
static double  max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static double old_m_w, old_m_h;
static double old_m_x, old_m_y;

// old location used by the Follower routine
static mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
static double scale_mtof = INITSCALEMTOF;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static double scale_ftom;

static player_t *plr;           // the player represented by an arrow

static patch_t *marknums[10];   // numbers used for marking by the automap

// killough 2/22/98: Remove limit on automap marks,
// and make variables external for use in savegames.

mpoint_t *markpoints = NULL;    // where the points are
int markpointnum = 0; // next point to be assigned (also number of points now)
int markpointnum_max = 0;       // killough 2/22/98
int followplayer = 1; // specifies whether to follow the player around

static bool stopped = true;
static bool am_needbackscreen; // haleyjd 05/03/13

// haleyjd 12/22/02: Heretic stuff

// backdrop
static byte *am_backdrop = NULL;
static bool am_usebackdrop = false;

// haleyjd 08/01/09: this function is unused
#if 0
//
// AM_getIslope()
//
// Calculates the slope and slope according to the x-axis of a line
// segment in map coordinates (with the upright y-axis n' all) so
// that it can be used with the brain-dead drawing stuff.
//
// Passed the line slope is desired for and an islope_t structure for return
// Returns nothing
//
static void AM_getIslope(mline_t *ml, islope_t *is )
{
   double dx, dy;
   
   dy = ml->a.y - ml->b.y;
   dx = ml->b.x - ml->a.x;
   if(dy == 0.0)
      is->islp = (dx < 0 ? -D_MAXINT : D_MAXINT);
   else
      is->islp = dx / dy;
   if(dx == 0.0)
      is->slp = (dy < 0 ? -D_MAXINT : D_MAXINT);
   else
      is->slp = dy / dx;
}
#endif

//
// AM_activateNewScale()
//
// Changes the map scale after zooming or translating
//
// Passed nothing, returns nothing
//
static void AM_activateNewScale(void)
{
   m_x += m_w / 2.0;
   m_y += m_h / 2.0;
   m_w  = FTOM(f_w);
   m_h  = FTOM(f_h);
   m_x -= m_w / 2.0;
   m_y -= m_h / 2.0;
   m_x2 = m_x + m_w;
   m_y2 = m_y + m_h;
}

//
// AM_saveScaleAndLoc()
//
// Saves the current center and zoom
// Affects the variables that remember old scale and loc
//
// Passed nothing, returns nothing
//
static void AM_saveScaleAndLoc(void)
{
   old_m_x = m_x;
   old_m_y = m_y;
   old_m_w = m_w;
   old_m_h = m_h;
}

//
// AM_restoreScaleAndLoc()
//
// restores the center and zoom from locally saved values
// Affects global variables for location and scale
//
// Passed nothing, returns nothing
//
static void AM_restoreScaleAndLoc(void)
{
   m_w = old_m_w;
   m_h = old_m_h;

   if(!followplayer)
   {
      m_x = old_m_x;
      m_y = old_m_y;
   }
   else
   {
      linkoffset_t *link;

      if(mapportal_overlay && useportalgroups && plr->mo->groupid > 0 && 
         (link = P_GetLinkOffset(0, plr->mo->groupid)))
      {
         m_x = M_FixedToDouble(plr->mo->x + link->x) - m_w/2;
         m_y = M_FixedToDouble(plr->mo->y + link->y) - m_h/2;
      }
      else
      {
         m_x = M_FixedToDouble(plr->mo->x) - m_w/2;
         m_y = M_FixedToDouble(plr->mo->y) - m_h/2;
      }
   }
   m_x2 = m_x + m_w;
   m_y2 = m_y + m_h;
   
   // Change the scaling multipliers
   scale_mtof = (double)f_w / m_w;
   scale_ftom = 1.0 / scale_mtof;
}

//
// AM_addMark()
//
// Adds a marker at the current location
// Affects global variables for marked points
//
// Passed nothing, returns nothing
//
static void AM_addMark(void)
{
   // killough 2/22/98:
   // remove limit on automap marks
   
   if(markpointnum >= markpointnum_max)
      markpoints = erealloc(mpoint_t *, markpoints,
                            (markpointnum_max = markpointnum_max ? 
                             markpointnum_max*2 : 16) * sizeof(*markpoints));

   markpoints[markpointnum].x = m_x + m_w/2;
   markpoints[markpointnum].y = m_y + m_h/2;
   markpointnum++;
}

//
// AM_findMinMaxBoundaries()
//
// Determines bounding box of all vertices,
// sets global variables controlling zoom range.
//
// Passed nothing, returns nothing
//
static void AM_findMinMaxBoundaries(void)
{
   int i;
   double a, b;
   
   min_x = min_y =  DBL_MAX;
   max_x = max_y = -DBL_MAX;
   
   // haleyjd: rewritten to work by line so as to have access to portal groups
   for(i = 0; i < numlines; ++i)
   {
      double x1, x2, y1, y2;

      x1 = lines[i].v1->fx;
      y1 = lines[i].v1->fy;
      x2 = lines[i].v2->fx;
      y2 = lines[i].v2->fy;

      if(mapportal_overlay && useportalgroups &&
         lines[i].frontsector->groupid > 0)
      {
         linkoffset_t *link;

         if((link = P_GetLinkOffset(0, lines[i].frontsector->groupid)))
         {
            x1 += M_FixedToDouble(link->x);
            y1 += M_FixedToDouble(link->y);
            x2 += M_FixedToDouble(link->x);
            y2 += M_FixedToDouble(link->y);
         }
      }

      if(x1 < min_x)
         min_x = x1;
      else if(x1 > max_x)
         max_x = x1;

      if(x2 < min_x)
         min_x = x2;
      else if(x2 > max_x)
         max_x = x2;

      if(y1 < min_y)
         min_y = y1;
      else if(y1 > max_y)
         max_y = y1;

      if(y2 < min_y)
         min_y = y2;
      else if(y2 > max_y)
         max_y = y2;
   }
     
   max_w = max_x - min_x;
   max_h = max_y - min_y;
   
   min_w = 2.0 * PLAYERRADIUS; // const? never changed?
   min_h = 2.0 * PLAYERRADIUS;
   
   a = (double)f_w / max_w;
   b = (double)f_h / max_h;
   
   min_scale_mtof = a < b ? a : b;
   max_scale_mtof = (double)f_h / (2.0 * PLAYERRADIUS);
}

//
// AM_changeWindowLoc()
//
// Moves the map window by the global variables m_paninc.x, m_paninc.y
//
// Passed nothing, returns nothing
//
static void AM_changeWindowLoc(void)
{
   if(m_paninc.x != 0.0 || m_paninc.y != 0.0)
   {
      followplayer = 0;
      f_oldloc.x = D_MAXINT;
   }
   
   m_x += m_paninc.x;
   m_y += m_paninc.y;
   
   if(m_x + m_w/2 > max_x)
      m_x = max_x - m_w/2;
   else if(m_x + m_w/2 < min_x)
      m_x = min_x - m_w/2;
   
   if(m_y + m_h/2 > max_y)
      m_y = max_y - m_h/2;
   else if(m_y + m_h/2 < min_y)
      m_y = min_y - m_h/2;
   
   m_x2 = m_x + m_w;
   m_y2 = m_y + m_h;
}

extern void ST_AutomapEvent(int type);

//
// AM_initVariables()
//
// Initialize the variables for the automap
//
// Affects the automap global variables
// Status bar is notified that the automap has been entered
// Passed nothing, returns nothing
//
static void AM_initVariables()
{
   int pnum;   
   
   automapactive = true;

   // haleyjd: need to redraw the backscreen?
   am_needbackscreen = (vbscreen.getVirtualAspectRatio() > 4*FRACUNIT/3);

   f_oldloc.x = D_MAXINT;
   amclock = 0;
   lightlev = 0;
   
   m_paninc.x = m_paninc.y = 0;
   ftom_zoommul = 1.0;
   mtof_zoommul = 1.0;
   
   m_w = FTOM(f_w);
   m_h = FTOM(f_h);
   
   // find player to center on initially
   if(!playeringame[pnum = consoleplayer])
   {
      for(pnum = 0; pnum < MAXPLAYERS; ++pnum)
         if(playeringame[pnum])
            break;
   }
         
   plr = &players[pnum];

   {
      linkoffset_t *link;

      if(mapportal_overlay && useportalgroups && plr->mo->groupid > 0 && 
         (link = P_GetLinkOffset(0, plr->mo->groupid)))
      {
         m_x = M_FixedToDouble(plr->mo->x + link->x) - m_w/2;
         m_y = M_FixedToDouble(plr->mo->y + link->y) - m_h/2;
      }
      else
      {
         m_x = M_FixedToDouble(plr->mo->x) - m_w/2;
         m_y = M_FixedToDouble(plr->mo->y) - m_h/2;
      }
   }

   AM_changeWindowLoc();
         
   // for saving & restoring
   old_m_x = m_x;
   old_m_y = m_y;
   old_m_w = m_w;
   old_m_h = m_h;
   
   // inform the status bar of the change
   ST_AutomapEvent(AM_MSGENTERED);
}

//
// AM_loadPics()
// 
// Load the patches for the mark numbers
//
// Sets the marknums[i] variables to the patches for each digit
// Passed nothing, returns nothing;
//
static void AM_loadPics(void)
{
   int i, lumpnum;
   char namebuf[9];
   
   // haleyjd 10/09/05: get format string from GameModeInfo
   for(i = 0; i < 10; ++i)
   {
      sprintf(namebuf, GameModeInfo->markNumFmt, i);
      marknums[i] = PatchLoader::CacheName(wGlobalDir, namebuf, PU_STATIC);
   }

   // haleyjd 12/22/02: automap background support (raw format)
   if((lumpnum = W_CheckNumForName("AUTOPAGE")) != -1)
   {
      int size = W_LumpLength(lumpnum);
      byte *autopage = (byte *)(wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC));
      int height = size / SCREENWIDTH;

      // allocate backdrop
      if(!am_backdrop)
         am_backdrop = (byte *)(Z_Malloc(SCREENWIDTH*SCREENHEIGHT, PU_STATIC, NULL));

      // must be at least 100 tall
      if(height < 100 || height > SCREENHEIGHT)
         I_Error("AM_loadPics: bad AUTOPAGE size\n");

      // use V_CacheBlock to construct an unscaled screen buffer
      V_CacheBlock(0, 0, SCREENWIDTH, height, autopage, am_backdrop);

      // background is allowed to be shorter than 200 pixels, so
      // tile the graphic through the difference
      if(height < SCREENHEIGHT)
      {
         V_CacheBlock(0, height, SCREENWIDTH, SCREENHEIGHT-height,
                      autopage, am_backdrop);
      }

      // set lump purgable
      Z_ChangeTag(autopage, PU_CACHE);

      am_usebackdrop = true;
   }
}

//
// AM_unloadPics()
//
// Makes the mark patches purgable
//
// Passed nothing, returns nothing
//
static void AM_unloadPics(void)
{
   int i;
   
   for(i = 0; i < 10; ++i)
      Z_ChangeTag(marknums[i], PU_CACHE);
   
   // haleyjd 12/22/02: backdrop support
   if(am_backdrop)
   {
      Z_Free(am_backdrop);
      am_backdrop = NULL;
      am_usebackdrop = false;
   }
}

//
// AM_clearMarks()
//
// Sets the number of marks to 0, thereby clearing them from the display
//
// Affects the global variable markpointnum
// Passed nothing, returns nothing
//
void AM_clearMarks()
{
   markpointnum = 0;
}

//
// AM_LevelInit()
//
// Initialize the automap at the start of a new level
// should be called at the start of every level
//
// Passed nothing, returns nothing
// Affects automap's global variables
//
static void AM_LevelInit()
{
   f_x = f_y = 0;
   
   // killough 2/7/98: get rid of finit_ vars
   // to allow runtime setting of width/height
   //
   
   // SoM 2-4-04: ANYRES
   f_w = video.width;
   f_h = video.height - ((GameModeInfo->StatusBar->height * video.yscale) >> FRACBITS);

   AM_findMinMaxBoundaries();
   scale_mtof = min_scale_mtof / 0.7;
   if(scale_mtof > max_scale_mtof)
      scale_mtof = min_scale_mtof;
   scale_ftom = 1.0 / scale_mtof;
}

//
// AM_Stop()
//
// Cease automap operations, unload patches, notify status bar
//
// Passed nothing, returns nothing
//
void AM_Stop()
{  
   AM_unloadPics();
   automapactive = false;
   am_needbackscreen = false;
   ST_AutomapEvent(AM_MSGEXITED);
   stopped = true;
   setsizeneeded = true; // haleyjd
}

//
// AM_Start
// 
// Start up automap operations, 
//  if a new level, or game start, (re)initialize level variables
//  init map variables
//  load mark patches
//
// Passed nothing, returns nothing
//
void AM_Start()
{
   static int lastlevel = -1, lastepisode = -1, 
              last_width = -1, last_height = -1,
              last_overlay = -1;
   
   if(!stopped)
      AM_Stop();

   stopped = false;
   
   // SoM: ANYRES
   // haleyjd 06/10/09: added portal overlay
   if(lastlevel != gamemap || lastepisode != gameepisode || 
      last_width != video.width || last_height != video.height ||
      last_overlay != mapportal_overlay)
   {
      last_width = video.width;
      last_height = video.height;
      last_overlay = mapportal_overlay;

      AM_LevelInit();
      
      lastlevel = gamemap;
      lastepisode = gameepisode;
   }
   AM_initVariables();
   AM_loadPics();
}

//
// AM_minOutWindowScale()
//
// Set the window scale to the maximum size
//
// Passed nothing, returns nothing
//
static void AM_minOutWindowScale()
{
   scale_mtof = min_scale_mtof;
   scale_ftom = 1.0 / scale_mtof;
   AM_activateNewScale();
}

//
// AM_maxOutWindowScale(void)
//
// Set the window scale to the minimum size
//
// Passed nothing, returns nothing
//
static void AM_maxOutWindowScale(void)
{
   scale_mtof = max_scale_mtof;
   scale_ftom = 1.0 / scale_mtof;
   AM_activateNewScale();
}

static bool am_key_handled;

//
// AM_Responder()
//
// Handle events (user inputs) in automap mode
//
// Passed an input event, returns true if its handled
//
// haleyjd 07/07/04: rewritten to support new keybindings
//
bool AM_Responder(event_t *ev)
{
   static int bigstate = 0;
  
   // haleyjd 07/07/04: dynamic bindings
   int action = G_KeyResponder(ev, kac_map);

   if(!automapactive)
   {
      if(ev->type != ev_keydown)
         return false;

      switch(action)
      {
      case ka_map_toggle: // activate automap
         AM_Start();
         return true;
      default:
         return false;
      }
   }
   else
   {
      // handle end of pan or zoom on keyup
      if(ev->type == ev_keyup)
      {
         switch(action)
         {
         case ka_map_right:   // stop pan left or right
         case ka_map_left:
            if(!followplayer)
            {
               m_paninc.x = 0;
               return true;
            }
            return false;

         case ka_map_up:      // stop pan up or down
         case ka_map_down:
            if(!followplayer)
            {
               m_paninc.y = 0;
               return true;
            }
            return false;

         case ka_map_zoomin:  // stop zoom in or out
         case ka_map_zoomout:
            mtof_zoommul = 1.0;
            ftom_zoommul = 1.0;
            return true;
         }
      }

      // all other events are keydown only
      if(ev->type != ev_keydown)
         return false;

      switch(action)
      {
      case ka_map_right: // pan right
         if(!followplayer)
         {
            m_paninc.x = FTOM(F_PANINC);
            return true;
         }
         return false;

      case ka_map_left: // pan left
         if(!followplayer)
         {
            m_paninc.x = -FTOM(F_PANINC);
            return true;
         }
         return false;

      case ka_map_up: // pan up
         if(!followplayer)
         {
            m_paninc.y = FTOM(F_PANINC);
            return true;
         }
         return false;

      case ka_map_down: // pan down
         if(!followplayer)
         {
            m_paninc.y = -FTOM(F_PANINC);
            return true;
         }
         return false;

      case ka_map_zoomout: // zoom out
         mtof_zoommul = M_ZOOMOUT;
         ftom_zoommul = M_ZOOMIN;
         return true;

      case ka_map_zoomin: // zoom in
         mtof_zoommul = M_ZOOMIN;
         ftom_zoommul = M_ZOOMOUT;
         return true;

      case ka_map_toggle: // deactivate map
         bigstate = 0;
         AM_Stop();
         return true;

      case ka_map_gobig: // "go big"
         bigstate = !bigstate;
         if(bigstate)
         {
            AM_saveScaleAndLoc();
            AM_minOutWindowScale();
         }
         else
            AM_restoreScaleAndLoc();
         return true;

      case ka_map_follow: // toggle follow mode
         followplayer = !followplayer;
         f_oldloc.x = D_MAXINT;
         // Ty 03/27/98 - externalized
         doom_printf("%s", DEH_String(followplayer ? "AMSTR_FOLLOWON"
                                                   : "AMSTR_FOLLOWOFF"));
         return true;

      case ka_map_grid: // toggle grid
         automap_grid = !automap_grid; // killough 2/28/98
         // Ty 03/27/98 - *not* externalized
         doom_printf("%s", DEH_String(automap_grid ? "AMSTR_GRIDON" 
                                                   : "AMSTR_GRIDOFF"));
         return true;

      case ka_map_mark: // mark a spot
         // Ty 03/27/98 - *not* externalized     
         // sf: fixed this (buffer at start, presumably from an old sprintf
         doom_printf("%s %d", DEH_String("AMSTR_MARKEDSPOT"), markpointnum);
         AM_addMark();
         return true;

      case ka_map_clear: // clear all marked spots
         AM_clearMarks();  // Ty 03/27/98 - *not* externalized
         doom_printf("%s", DEH_String("AMSTR_MARKSCLEARED"));
         return true;

      default:
         return false;
      }
   }
}

//
// AM_changeWindowScale()
//
// Automap zooming
//
// Passed nothing, returns nothing
//
static void AM_changeWindowScale()
{
   // Change the scaling multipliers
   scale_mtof = scale_mtof * mtof_zoommul;
   scale_ftom = 1.0 / scale_mtof;
   
   if(scale_mtof < min_scale_mtof)
      AM_minOutWindowScale();
   else if(scale_mtof > max_scale_mtof)
      AM_maxOutWindowScale();
   else
      AM_activateNewScale();
}

//
// AM_doFollowPlayer()
//
// Turn on follow mode - the map scrolls opposite to player motion
//
// Passed nothing, returns nothing
//
static void AM_doFollowPlayer(void)
{
   if(f_oldloc.x != plr->mo->x || f_oldloc.y != plr->mo->y)
   {
      linkoffset_t *link;
      if(mapportal_overlay && useportalgroups && plr->mo->groupid > 0 && 
         (link = P_GetLinkOffset(0, plr->mo->groupid)))
      {
         m_x = FTOM(MTOF(M_FixedToDouble(plr->mo->x + link->x))) - m_w/2;
         m_y = FTOM(MTOF(M_FixedToDouble(plr->mo->y + link->y))) - m_h/2;
      }
      else
      {
         m_x = FTOM(MTOF(M_FixedToDouble(plr->mo->x))) - m_w/2;
         m_y = FTOM(MTOF(M_FixedToDouble(plr->mo->y))) - m_h/2;
      }
      m_x2 = m_x + m_w;
      m_y2 = m_y + m_h;
      f_oldloc.x = M_FixedToDouble(plr->mo->x);
      f_oldloc.y = M_FixedToDouble(plr->mo->y);
   }
}

//
// killough 10/98: return coordinates, to allow use of a non-follow-mode
// pointer. Allows map inspection without moving player to the location.
//

// haleyjd 07/01/02: reformatted for readability

int map_point_coordinates;

void AM_Coordinates(const Mobj *mo, fixed_t &x, fixed_t &y, fixed_t &z)
{
   if(followplayer || !map_point_coordinates)
   {
      x = mo->x;
      y = mo->y;
      z = mo->z;
   }
   else
   {
      x = M_DoubleToFixed(m_x + m_w / 2);
      y = M_DoubleToFixed(m_y + m_h / 2);
      z = R_PointInSubsector(x, y)->sector->floorheight;
   }
}

//
// AM_Ticker()
//
// Updates on gametic - enter follow mode, zoom, or change map location
//
// Passed nothing, returns nothing
//
void AM_Ticker(void)
{
   if(!automapactive)
      return;
   
   amclock++;
   
   if(followplayer)
      AM_doFollowPlayer();
   
   // Change the zoom if necessary
   if(ftom_zoommul != 1.0)
      AM_changeWindowScale();
   
   // Change x,y location
   if(m_paninc.x != 0.0 || m_paninc.y != 0.0)
      AM_changeWindowLoc();
}


//
// Clear automap frame buffer.
//
static void AM_clearFB(int color)
{
   // haleyjd 05/03/13: redraw the backscreen if needed, so that wings can
   // be filled in for widescreen modes even if the player's screensize setting
   // has been fullscreen since startup.
   if(am_needbackscreen)
   {
      rrect_t temprect;
      temprect.scaledFromScreenBlocks(10); // use stat-bar-up screensize
      R_FillBackScreen(temprect);
      D_DrawWings();
      am_needbackscreen = false;
   }

   // haleyjd 12/22/02: backdrop support
   if(am_usebackdrop && am_backdrop)
   {
      // SoM 2-4-04: ANYRES
      V_DrawBlock(0, 0, &vbscreen, 
                  SCREENWIDTH, (f_h << FRACBITS) / video.yscale, 
                  am_backdrop);
   }
   else
      V_ColorBlock(&vbscreen, (unsigned char)color, 0, 0, f_w, f_h);
}

//
// AM_clipMline()
//
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes. If the speed is needed,
// use a hash algorithm to handle the common cases.
//
// Passed the line's coordinates on map and in the frame buffer performs
// clipping on them in the lines frame coordinates.
// Returns true if any part of line was not clipped
//
static bool AM_clipMline(mline_t *ml, fline_t *fl)
{
   enum
   {
      LEFT   = 1,
      RIGHT  = 2,
      BOTTOM = 4,
      TOP    = 8
   };
   
   register int outcode1 = 0;
   register int outcode2 = 0;
   register int outside;

   fpoint_t tmp = { 0, 0 };
   int   dx;
   int   dy;

    
#define DOOUTCODE(oc, mx, my) \
   (oc) = 0; \
   if ((my) < 0) (oc) |= TOP; \
   else if ((my) >= f_h) (oc) |= BOTTOM; \
   if ((mx) < 0) (oc) |= LEFT; \
   else if ((mx) >= f_w) (oc) |= RIGHT;

    
   // do trivial rejects and outcodes
   if(ml->a.y > m_y2)
      outcode1 = TOP;
   else if(ml->a.y < m_y)
      outcode1 = BOTTOM;

   if(ml->b.y > m_y2)
      outcode2 = TOP;
   else if(ml->b.y < m_y)
      outcode2 = BOTTOM;

   if(outcode1 & outcode2)
      return false; // trivially outside

   if(ml->a.x < m_x)
      outcode1 |= LEFT;
   else if(ml->a.x > m_x2)
      outcode1 |= RIGHT;

   if(ml->b.x < m_x)
      outcode2 |= LEFT;
   else if(ml->b.x > m_x2)
      outcode2 |= RIGHT;

   if(outcode1 & outcode2)
      return false; // trivially outside

   // transform to frame-buffer coordinates.
   fl->a.x = CXMTOF(ml->a.x);
   fl->a.y = CYMTOF(ml->a.y);
   fl->b.x = CXMTOF(ml->b.x);
   fl->b.y = CYMTOF(ml->b.y);
   
   DOOUTCODE(outcode1, fl->a.x, fl->a.y);
   DOOUTCODE(outcode2, fl->b.x, fl->b.y);

   if(outcode1 & outcode2)
      return false;

   while(outcode1 | outcode2)
   {
      // may be partially inside box
      // find an outside point
      if(outcode1)
         outside = outcode1;
      else
         outside = outcode2;

      // clip to each side
      if(outside & TOP)
      {
         dy = fl->a.y - fl->b.y;
         dx = fl->b.x - fl->a.x;
         tmp.x = fl->a.x + (dx*(fl->a.y))/dy;
         tmp.y = 0;
      }
      else if(outside & BOTTOM)
      {
         dy = fl->a.y - fl->b.y;
         dx = fl->b.x - fl->a.x;
         tmp.x = fl->a.x + (dx*(fl->a.y-f_h))/dy;
         tmp.y = f_h-1;
      }
      else if(outside & RIGHT)
      {
         dy = fl->b.y - fl->a.y;
         dx = fl->b.x - fl->a.x;
         tmp.y = fl->a.y + (dy*(f_w-1 - fl->a.x))/dx;
         tmp.x = f_w-1;
      }
      else if(outside & LEFT)
      {
         dy = fl->b.y - fl->a.y;
         dx = fl->b.x - fl->a.x;
         tmp.y = fl->a.y + (dy*(-fl->a.x))/dx;
         tmp.x = 0;
      }

      if(outside == outcode1)
      {
         fl->a = tmp;
         DOOUTCODE(outcode1, fl->a.x, fl->a.y);
      }
      else
      {
         fl->b = tmp;
         DOOUTCODE(outcode2, fl->b.x, fl->b.y);
      }
      
      if(outcode1 & outcode2)
         return false; // trivially outside
   }
   
   return true;
}
#undef DOOUTCODE

// haleyjd 06/12/09: this macro is now shared by Bresenham and Wu
#define PUTDOT(xx,yy,cc) *(vbscreen.ylut[(yy)] + vbscreen.xlut[(xx)]) = (cc)

//
// AM_drawFline()
//
// Draw a line in the frame buffer.
// Classic Bresenham w/ whatever optimizations needed for speed
//
// Passed the frame coordinates of line, and the color to be drawn
// Returns nothing
//
static void AM_drawFline(fline_t *fl, int color )
{
   register int x;
   register int y;
   register int dx;
   register int dy;
   register int sx;
   register int sy;
   register int ax;
   register int ay;
   register int d;

#ifdef RANGECHECK         // killough 2/22/98    
   //static int fuck = 0;
   
   // For debugging only
   if(   fl->a.x < 0 || fl->a.x >= f_w
      || fl->a.y < 0 || fl->a.y >= f_h
      || fl->b.x < 0 || fl->b.x >= f_w
      || fl->b.y < 0 || fl->b.y >= f_h
     )
   {
      //fprintf(stderr, "fuck %d \r", fuck++);
      return;
   }
#endif

   dx = fl->b.x - fl->a.x;
   ax = 2 * (dx < 0 ? -dx : dx);
   sx = dx < 0 ? -1 : 1;
   
   dy = fl->b.y - fl->a.y;
   ay = 2 * (dy < 0 ? -dy : dy);
   sy = dy < 0 ? -1 : 1;

   x = fl->a.x;
   y = fl->a.y;
   
   if(ax > ay)
   {
      d = ay - ax/2;
      while(1)
      {
         PUTDOT(x, y, color);
         if(x == fl->b.x) return;
         if(d >= 0)
         {
            y += sy;
            d -= ax;
         }
         x += sx;
         d += ay;
      }
   }
   else
   {
      d = ax - ay/2;
      while(1)
      {
         PUTDOT(x, y, color);
         if(y == fl->b.y) return;
         if(d >= 0)
         {
            x += sx;
            d -= ay;
         }
         y += sy;
         d += ax;
      }
   }
}

//
// AM_putWuDot
//
// haleyjd 06/13/09: Pixel plotter for Wu line drawing.
//
static void AM_putWuDot(int x, int y, int color, int weight)
{
   byte *dest = vbscreen.ylut[y] + vbscreen.xlut[x];
   unsigned int *fg2rgb = Col2RGB8[weight];
   unsigned int *bg2rgb = Col2RGB8[64 - weight];
   unsigned int fg, bg;

   fg = fg2rgb[color];
   bg = bg2rgb[*dest];
   fg = (fg + bg) | 0x1f07c1f;
   *dest = RGB32k[0][0][fg & (fg >> 15)];
}


// Given 65536, we need 2048; 65536 / 2048 == 32 == 2^5
// Why 2048? ANG90 == 0x40000000 which >> 19 == 0x800 == 2048.
// The trigonometric correction is based on an angle from 0 to 90.
#define wu_fineshift 5

// Given 64 levels in the Col2RGB8 table, 65536 / 64 == 1024 == 2^10
#define wu_fixedshift 10

//
// AM_drawFlineWu
//
// haleyjd 06/12/09: Wu line drawing for the automap, with trigonometric
// brightness correction by SoM. I call this the Wu-McGranahan line drawing
// algorithm.
//
static void AM_drawFlineWu(fline_t *fl, int color)
{
   int dx, dy, xdir = 1;
   int x, y;   

   // swap end points if necessary
   if(fl->a.y > fl->b.y)
   {
      fpoint_t tmp = fl->a;

      fl->a = fl->b;
      fl->b = tmp;
   }

   // determine change in x, y and direction of travel
   dx = fl->b.x - fl->a.x;
   dy = fl->b.y - fl->a.y;

   if(dx < 0)
   {
      dx   = -dx;
      xdir = -xdir;      
   }   

   // detect special cases -- horizontal, vertical, and 45 degrees;
   // revert to Bresenham
   if(dx == 0 || dy == 0 || dx == dy)
   {
      AM_drawFline(fl, color);
      return;
   }

   // draw first pixel
   PUTDOT(fl->a.x, fl->a.y, color);

   x = fl->a.x;
   y = fl->a.y;

   if(dy > dx)
   {
      // line is y-axis major.
      uint16_t erroracc = 0, 
         erroradj = (uint16_t)(((uint32_t)dx << 16) / (uint32_t)dy);

      while(--dy)
      {
         uint16_t erroracctmp = erroracc;

         erroracc += erroradj;

         // if error has overflown, advance x coordinate
         if(erroracc <= erroracctmp)
            x += xdir;
         
         y += 1; // advance y

         // the trick is in the trig!
         AM_putWuDot(x, y, color, 
                     finecosine[erroracc >> wu_fineshift] >> wu_fixedshift);
         AM_putWuDot(x + xdir, y, color, 
                     finesine[erroracc >> wu_fineshift] >> wu_fixedshift);
      }
   }
   else
   {
      // line is x-axis major.
      uint16_t erroracc = 0, 
         erroradj = (uint16_t)(((uint32_t)dy << 16) / (uint32_t)dx);

      while(--dx)
      {
         uint16_t erroracctmp = erroracc;

         erroracc += erroradj;

         // if error has overflown, advance y coordinate
         if(erroracc <= erroracctmp)
            y += 1;
         
         x += xdir; // advance x

         // the trick is in the trig!
         AM_putWuDot(x, y, color, 
                     finecosine[erroracc >> wu_fineshift] >> wu_fixedshift);
         AM_putWuDot(x, y + 1, color, 
                     finesine[erroracc >> wu_fineshift] >> wu_fixedshift);
      }
   }

   // draw last pixel
   PUTDOT(fl->b.x, fl->b.y, color);
}

//
// AM_drawMline()
//
// Clip lines, draw visible parts of lines.
//
// Passed the map coordinates of the line, and the color to draw it
// Color -1 is special and prevents drawing. Color 247 is special and
// is translated to black, allowing Color 0 to represent feature disable
// in the defaults file.
// Returns nothing.
//
static void AM_drawMline(mline_t *ml, int color)
{
   static fline_t fl;
   
   if(color == -1)  // jff 4/3/98 allow not drawing any sort of line
      return;       // by setting its color to -1
   if(color == 247) // jff 4/3/98 if color is 247 (xparent), use black
      color=0;
   
   /*
   if(AM_clipMline(ml, &fl))
      AM_drawFline(&fl, color); // draws it on frame buffer using fb coords
   */
   // TEST:
   if(AM_clipMline(ml, &fl))
      AM_drawFlineWu(&fl, color);
}

//
// AM_drawGrid()
//
// Draws blockmap aligned grid lines.
//
// Passed the color to draw the grid lines
// Returns nothing
//
static void AM_drawGrid(int color)
{
   fixed_t x, y;
   fixed_t start, end;
   mline_t ml;
   
   // Figure out start of vertical gridlines
   start = M_DoubleToFixed(m_x);
   if((start - bmaporgx) % (MAPBLOCKUNITS << FRACBITS))
      start -= ((start - bmaporgx) % (MAPBLOCKUNITS << FRACBITS));
   end = M_DoubleToFixed(m_x + m_w);

   // draw vertical gridlines
   ml.a.y = m_y;
   ml.b.y = m_y + m_h;
   for(x = start; x < end; x += (MAPBLOCKUNITS << FRACBITS))
   {
      ml.a.x = M_FixedToDouble(x);
      ml.b.x = M_FixedToDouble(x);
      AM_drawMline(&ml, color);
   }

   // Figure out start of horizontal gridlines
   start = M_DoubleToFixed(m_y);
   if((start - bmaporgy) % (MAPBLOCKUNITS << FRACBITS))
      start -= ((start - bmaporgy) % (MAPBLOCKUNITS << FRACBITS));
   end = M_DoubleToFixed(m_y + m_h);

   // draw horizontal gridlines
   ml.a.x = m_x;
   ml.b.x = m_x + m_w;
   for(y = start; y < end; y += (MAPBLOCKUNITS << FRACBITS))
   {
      ml.a.y = M_FixedToDouble(y);
      ml.b.y = M_FixedToDouble(y);
      AM_drawMline(&ml, color);
   }
}

//
// AM_DoorColor()
//
// Returns the 'color' or key needed for a door linedef type
//
// Passed the type of linedef, returns:
//   -1 if not a keyed door
//    0 if a red key required
//    1 if a blue key required
//    2 if a yellow key required
//    3 if a multiple keys required
//
// jff 4/3/98 add routine to get color of generalized keyed door
//
static int AM_DoorColor(int type)
{
   if(GenLockedBase <= type && type < GenDoorBase)
   {
      type -= GenLockedBase;
      type = (type & LockedKey) >> LockedKeyShift;
      if(!type || type==7)
         return 3;  //any or all keys
      else 
         return (type - 1) % 3;
   }

   switch(type)  // closed keyed door
   {
   case 26: case 32: case 99: case 133:
      /*bluekey*/
      return 1;
   case 27: case 34: case 136: case 137:
      /*yellowkey*/
      return 2;
   case 28: case 33: case 134: case 135:
      /*redkey*/
      return 0;
   default:
      return -1; // not a keyed door
   }
   return -1;    // not a keyed door
}

// haleyjd 07/07/04: Support routines to clean up the horrible
// mess that was AM_drawWalls. Go look in the beta 7 or earlier
// source if you want to see what I now consider one of the largest
// messes in all +120k lines of this program's code. Some of the
// code in these functions was actually repeated many times over.

//
// AM_drawAsExitLine
//
// Returns true if line is an exit and the exit map color is
// defined; returns false otherwise.
//
inline static bool AM_drawAsExitLine(line_t *line)
{
   return (mapcolor_exit &&
           (line->special==11  ||
            line->special==52  ||
            line->special==197 ||
            line->special==51  ||
            line->special==124 ||
            line->special==198));
}

//
// AM_drawAs1sSecret
//
// Returns true if a 1S line is or was secret and the secret line
// map color is defined; returns false otherwise.
//
inline static bool AM_drawAs1sSecret(line_t *line)
{
   return (mapcolor_secr &&
           ((map_secret_after &&
             P_WasSecret(line->frontsector) &&
             !P_IsSecret(line->frontsector)) ||
           (!map_secret_after &&
            P_WasSecret(line->frontsector))));
}

//
// AM_drawAs2sSecret
//
// Returns true if a 2S line is or was secret and the secret line
// map color is defined; returns false otherwise.
//
inline static bool AM_drawAs2sSecret(line_t *line)
{
   //jff 2/16/98 fixed bug: special was cleared after getting it
   
   //jff 3/9/98 add logic to not show secret til after entered 
   // if map_secret_after is true

   // haleyjd: this is STILL horrible, but oh well.
   
   return (mapcolor_secr &&       
           ((map_secret_after &&
             ((P_WasSecret(line->frontsector) && 
               !P_IsSecret(line->frontsector)) || 
              (P_WasSecret(line->backsector) && 
               !P_IsSecret(line->backsector)))) ||  
           (!map_secret_after &&
            (P_WasSecret(line->frontsector) ||
             P_WasSecret(line->backsector)))));
}

//
// AM_drawAsTeleporter
//
// Returns true if a line is a teleporter and the teleporter map
// color is defined; returns false otherwise.
//
inline static bool AM_drawAsTeleporter(line_t *line)
{
   return (mapcolor_tele && !(line->flags & ML_SECRET) && 
           (line->special == 39  || line->special == 97 ||
            line->special == 125 || line->special == 126));
}

//
// AM_drawAsLockedDoor
//
// Returns true if a line is a locked door for which the corresponding
// map color is defined; returns false otherwise.
//
// FIXME / HTIC_TODO: Heretic support
//
inline static bool AM_drawAsLockedDoor(line_t *line)
{
   return ((mapcolor_bdor || mapcolor_ydor || mapcolor_rdor) &&
           ((line->special >=  26 && line->special <=  28) ||
            (line->special >=  32 && line->special <=  34) ||
            (line->special >= 133 && line->special <= 137) ||
            line->special == 99 ||
            (line->special >= GenLockedBase && 
             line->special <  GenDoorBase)));
}

//
// AM_isDoorClosed
//
// Returns true if a door is closed, false otherwise.
//
inline static bool AM_isDoorClosed(line_t *line)
{
   return ((line->backsector->floorheight  == line->backsector->ceilingheight) ||
           (line->frontsector->floorheight == line->frontsector->ceilingheight));
}

//
// AM_drawAsClosedDoor
//
// Returns true if a door is closed, not secret, and closed door
// map color is defined; returns false otherwise.
//
inline static bool AM_drawAsClosedDoor(line_t *line)
{
   return (mapcolor_clsd &&  
           !(line->flags & ML_SECRET) &&    // non-secret closed door
           AM_isDoorClosed(line));
}

//
// Determines visible lines, draws them.
// This is LineDef based, not LineSeg based.
//
// jff 1/5/98 many changes in this routine
// backward compatibility not needed, so just changes, no ifs
// addition of clauses for:
//    doors opening, keyed door id, secret sectors,
//    teleports, exit lines, key things
// ability to suppress any of added features or lines with no height changes
//
// support for gamma correction in automap abandoned
//
// jff 4/3/98 changed mapcolor_xxxx=0 as control to disable feature
// jff 4/3/98 changed mapcolor_xxxx=-1 to disable drawing line completely
//
static void AM_drawWalls(void)
{
   int i;
   static mline_t l;
   
   int plrgroup = plr->mo->groupid;

   // Draw overlay lines first so they will not obscure the (more important)
   // normal map lines
   if(mapportal_overlay && useportalgroups)
   {
      for(i = 0; i < numlines; ++i)
      {
         line_t *line = &lines[i];

         if(line->frontsector->groupid == plrgroup)
            continue;

         l.a.x = line->v1->fx;
         l.a.y = line->v1->fy;
         l.b.x = line->v2->fx;
         l.b.y = line->v2->fy;

         if(line->frontsector->groupid > 0)
         {
            linkoffset_t *link;

            if((link = P_GetLinkOffset(0, line->frontsector->groupid)))
            {
               l.a.x += M_FixedToDouble(link->x);
               l.a.y += M_FixedToDouble(link->y);
               l.b.x += M_FixedToDouble(link->x);
               l.b.y += M_FixedToDouble(link->y);
            }
         }
         // if line has been seen or IDDT has been used
         if(ddt_cheating || (line->flags & ML_MAPPED))
         {
            // check for DONTDRAW flag; those lines are only visible
            // if using the IDDT cheat.
            if((line->flags & ML_DONTDRAW) && !ddt_cheating)
               continue;

            if(!line->backsector ||
               line->backsector->floorheight != line->frontsector->floorheight ||
               line->backsector->ceilingheight != line->frontsector->ceilingheight)
            {
               AM_drawMline(&l, mapcolor_prtl);
            }
         }
         else if(plr->powers[pw_allmap]) // computermap visible lines
         {
            // now draw the lines only visible because the player has computermap
            if(!(line->flags & ML_DONTDRAW)) // invisible flag lines do not show
            {
               if(!line->backsector ||
                  line->backsector->floorheight != line->frontsector->floorheight ||
                  line->backsector->ceilingheight != line->frontsector->ceilingheight)
               {
                  AM_drawMline(&l, mapcolor_prtl);
               }
            }
         } // end else if
      
      }
   }

   // draw the unclipped visible portions of all lines
   for(i = 0; i < numlines; ++i)
   {
      line_t *line = &lines[i];

      l.a.x = line->v1->fx;
      l.a.y = line->v1->fy;
      l.b.x = line->v2->fx;
      l.b.y = line->v2->fy;

      if(mapportal_overlay && useportalgroups)
      {
         if(line->frontsector && line->frontsector->groupid != plrgroup)
            continue;

         if(line->frontsector && line->frontsector->groupid > 0)
         {
            linkoffset_t *link;

            if((link = P_GetLinkOffset(0, line->frontsector->groupid)))
            {
               l.a.x += M_FixedToDouble(link->x);
               l.a.y += M_FixedToDouble(link->y);
               l.b.x += M_FixedToDouble(link->x);
               l.b.y += M_FixedToDouble(link->y);
            }
         }
      }

      // if line has been seen or IDDT has been used
      if(ddt_cheating || (line->flags & ML_MAPPED))
      {
         // check for DONTDRAW flag; those lines are only visible
         // if using the IDDT cheat.
         if((line->flags & ML_DONTDRAW) && !ddt_cheating)
            continue;

         if(!line->backsector) // 1S lines
         {            
            if(AM_drawAsExitLine(line))
            {
               //jff 4/23/98 add exit lines to automap
               AM_drawMline(&l, mapcolor_exit); // exit line
            }            
            else if(AM_drawAs1sSecret(line))
            {
               // jff 1/10/98 add new color for 1S secret sector boundary
               AM_drawMline(&l, mapcolor_secr); // line bounding secret sector
            }
            else                                //jff 2/16/98 fixed bug
               AM_drawMline(&l, mapcolor_wall); // special was cleared
         }
         else // 2S lines
         {
            // jff 1/10/98 add color change for all teleporter types
            if(AM_drawAsTeleporter(line))
            { 
               // teleporters
               AM_drawMline(&l, mapcolor_tele);
            }
            else if(AM_drawAsExitLine(line))
            {
               //jff 4/23/98 add exit lines to automap
               AM_drawMline(&l, mapcolor_exit);
            }
            else if(AM_drawAsLockedDoor(line))
            {
               //jff 1/5/98 this clause implements showing keyed doors
               if(AM_isDoorClosed(line))
               {
                  switch(AM_DoorColor(line->special)) // closed keyed door
                  {
                  case 1:
                     /*bluekey*/
                     AM_drawMline(&l,
                        mapcolor_bdor ? mapcolor_bdor : mapcolor_cchg);
                     break;
                  case 2:
                     /*yellowkey*/
                     AM_drawMline(&l,
                        mapcolor_ydor ? mapcolor_ydor : mapcolor_cchg);
                     break;
                  case 0:
                     /*redkey*/
                     AM_drawMline(&l,
                        mapcolor_rdor ? mapcolor_rdor : mapcolor_cchg);
                     break;
                  case 3:
                     /*any or all*/
                     AM_drawMline(&l,
                        mapcolor_clsd ? mapcolor_clsd : mapcolor_cchg);
                     break;
                  }
               }
               else
                  AM_drawMline(&l, mapcolor_cchg); // open keyed door
            }
            else if(line->flags & ML_SECRET)    // secret door
            {
               AM_drawMline(&l, mapcolor_wall);      // wall color
            }
            else if(AM_drawAsClosedDoor(line))
            {
               AM_drawMline(&l, mapcolor_clsd); // non-secret closed door
            } 
            else if(AM_drawAs2sSecret(line))
            {
               AM_drawMline(&l, mapcolor_secr); // line bounding secret sector
            } 
            else if(line->backsector->floorheight !=
                    line->frontsector->floorheight)
            {
               AM_drawMline(&l, mapcolor_fchg); // floor level change
            }
            else if(line->backsector->ceilingheight !=
                    line->frontsector->ceilingheight)
            {
               AM_drawMline(&l, mapcolor_cchg); // ceiling level change
            }
            else if(mapcolor_flat && ddt_cheating)
            { 
               AM_drawMline(&l, mapcolor_flat); // 2S lines that appear only in IDDT
            }
         }
      } 
      else if(plr->powers[pw_allmap]) // computermap visible lines
      {
         // now draw the lines only visible because the player has computermap
         if(!(line->flags & ML_DONTDRAW)) // invisible flag lines do not show
         {
            if(mapcolor_flat ||
               !line->backsector ||
               line->backsector->floorheight != line->frontsector->floorheight ||
               line->backsector->ceilingheight != line->frontsector->ceilingheight
              )
            {
               AM_drawMline(&l, mapcolor_unsn);
            }
         }
      } // end else if
   } // end for
}


//
// AM_drawNodeLines
//
// haleyjd 05/17/08: Draws node partition lines on the automap as a debugging
// aid or for the interest of the curious.
//
static void AM_drawNodeLines(int bspnum)
{
   mline_t l;

   while(!(bspnum & NF_SUBSECTOR))
   {
      node_t  *bsp   = &nodes[bspnum];
      fnode_t *fnode = &fnodes[bspnum];

      l.a.x = fnode->fx;
      l.a.y = fnode->fy;
      l.b.x = fnode->fx + fnode->fdx;
      l.b.y = fnode->fy + fnode->fdy;

      AM_drawMline(&l, mapcolor_frnd);

      AM_drawNodeLines(bsp->children[1]);

      bspnum = bsp->children[0];
   }
}

//
// AM_rotate()
//
// Rotation in 2D.
// Used to rotate player arrow line character.
//
// Passed the coordinates of a point, and an angle
// Returns the coordinates rotated by the angle
//
// haleyjd 01/24/03: made static
//
static void AM_rotate(double &x, double &y, angle_t a)
{
   double tmpx;

   // a little magic: a * (PI / ANG180) converts angle_t to radians
   double angle = (double)a * 1.4629180792671596811e-9; 

   tmpx = x * cos(angle) - y * sin(angle);
   y    = x * sin(angle) + y * cos(angle);
   x = tmpx;
}

//
// AM_drawLineCharacter()
//
// Draws a vector graphic according to numerous parameters
//
// Passed the structure defining the vector graphic shape, the number
// of vectors in it, the scale to draw it at, the angle to draw it at,
// the color to draw it with, and the map coordinates to draw it at.
// Returns nothing
//
static void AM_drawLineCharacter(mline_t *lineguy, int lineguylines, 
                                 double scale, angle_t angle, int color,
                                 fixed_t x, fixed_t y)
{
   int   i;
   mline_t l;
   double fx, fy;

   fx = M_FixedToDouble(x);
   fy = M_FixedToDouble(y);
   
   for(i = 0; i < lineguylines; ++i)
   {
      l.a.x = lineguy[i].a.x;
      l.a.y = lineguy[i].a.y;

      if(scale != 0.0)
      {
         l.a.x = scale * l.a.x;
         l.a.y = scale * l.a.y;
      }

      if(angle)
         AM_rotate(l.a.x, l.a.y, angle);

      l.a.x += fx;
      l.a.y += fy;
      
      l.b.x = lineguy[i].b.x;
      l.b.y = lineguy[i].b.y;
      
      if(scale)
      {
         l.b.x = scale * l.b.x;
         l.b.y = scale * l.b.y;
      }
      
      if(angle)
         AM_rotate(l.b.x, l.b.y, angle);

      l.b.x += fx;
      l.b.y += fy;
      
      AM_drawMline(&l, color);
   }
}

//
// AM_drawPlayers()
//
// Draws the player arrow in single player, 
// or all the player arrows in a netgame.
//
// Passed nothing, returns nothing
//
static void AM_drawPlayers(void)
{
   int   i;
   player_t* p;
   int   their_color = -1;
   int   color;
   // SoM: player x and y
   fixed_t px, py;
   linkoffset_t *link;

   if(!netgame)
   {
      if(mapportal_overlay && useportalgroups && plr->mo->groupid > 0 && 
         (link = P_GetLinkOffset(0, plr->mo->groupid)))
      {
         px = plr->mo->x + link->x;
         py = plr->mo->y + link->y;
      }
      else
      {
         px = plr->mo->x;
         py = plr->mo->y;
      }

      if(ddt_cheating)
      {
         AM_drawLineCharacter
          (
            cheat_player_arrow,
            NUMCHEATPLYRLINES,
            0.0,
            plr->mo->angle,
            mapcolor_sngl,      //jff color
            px,
            py
          );
      }
      else
      {
         AM_drawLineCharacter
          (
            player_arrow,
            NUMPLYRLINES,
            0.0,
            plr->mo->angle,
            mapcolor_sngl,      //jff color
            px,
            py
          );
      }
      return;
   }
  
   for(i = 0; i < MAXPLAYERS; ++i)
   {
      their_color = players[i].colormap;
      p = &players[i];

      // killough 9/29/98: use !demoplayback so internal demos are no different
      if((GameType == gt_dm && !demoplayback) && p != plr)
         continue;

      if(!playeringame[i])
         continue;
      
      if(mapportal_overlay && useportalgroups && plr->mo->groupid > 0 && 
         (link = P_GetLinkOffset(0, plr->mo->groupid)))
      {
         px = p->mo->x + link->x;
         py = p->mo->y + link->y;
      }
      else
      {
         px = p->mo->x;
         py = p->mo->y;
      }

      // haleyjd: add total invisibility
      
      if(p->powers[pw_invisibility] || p->powers[pw_totalinvis])
         color = 246; // *close* to black
      else
      {
         // sf: extended colour range
#define GREEN 112
         if(their_color == 0)
         {
            color = GREEN;
         }
         else
         {
            // haleyjd 01/12/04: rewritten
            byte *transtbl = translationtables[their_color - 1];

            color = transtbl[GREEN];
         }
      }
      
      AM_drawLineCharacter
       (
         player_arrow,
         NUMPLYRLINES,
         0.0,
         p->mo->angle,
         color,
         px,
         py
       );
   }
}

//
// AM_drawThings()
//
// Draws the things on the automap in double IDDT cheat mode
//
// Passed colors and colorrange, no longer used
// Returns nothing
//
static void AM_drawThings(int colors, int colorrange)
{
   int     i;
   Mobj *t;
   fixed_t tx, ty; // SoM: Moved thing coords to variables for linked portals
   
   // for all sectors
   for(i = 0; i < numsectors; ++i)
   {
      t = sectors[i].thinglist;

      while(t) // for all things in that sector
      {
         tx = t->x;
         ty = t->y;

         if(mapportal_overlay && useportalgroups && t->subsector->sector->groupid > 0)
         {
            linkoffset_t *link;

            if((link = P_GetLinkOffset(0, t->subsector->sector->groupid)))
            {
               tx += link->x;
               ty += link->y;
            }
         }
         // FIXME / HTIC_TODO: Heretic support and EDF editing?

         //jff 1/5/98 case over doomednum of thing being drawn
         if(mapcolor_rkey || mapcolor_ykey || mapcolor_bkey)
         {
            switch(t->info->doomednum)
            {
               //jff 1/5/98 treat keys special
            case 38: case 13: //jff  red key
               AM_drawLineCharacter
                  (
                   cross_mark,
                   NUMCROSSMARKLINES,
                   16.0,
                   t->angle,
                   mapcolor_rkey!=-1? mapcolor_rkey : mapcolor_sprt,
                   tx,
                   ty
                  );
               t = t->snext;
               continue;
            case 39: case 6: //jff yellow key
               AM_drawLineCharacter
                  (
                   cross_mark,
                   NUMCROSSMARKLINES,
                   16.0,
                   t->angle,
                   mapcolor_ykey!=-1? mapcolor_ykey : mapcolor_sprt,
                   tx,
                   ty
                  );
               t = t->snext;
               continue;
            case 40: case 5: //jff blue key
               AM_drawLineCharacter
                  (
                   cross_mark,
                   NUMCROSSMARKLINES,
                   16.0,
                   t->angle,
                   mapcolor_bkey!=-1? mapcolor_bkey : mapcolor_sprt,
                   tx,
                   ty
                  );
               t = t->snext;
               continue;
            default:
               break;
            }
         }

         //jff 1/5/98 end added code for keys
         //jff previously entire code
         AM_drawLineCharacter
            (
             thintriangle_guy,
             NUMTHINTRIANGLEGUYLINES,
             16.0,
             t->angle,
             // killough 8/8/98: mark friends specially
             t->flags & MF_FRIEND && !t->player ? mapcolor_frnd : mapcolor_sprt,
             tx,
             ty
            );
         t = t->snext;
      } // end if
   } // end for
}

//
// AM_drawMarks()
//
// Draw the marked locations on the automap
//
// Passed nothing, returns nothing
//
// killough 2/22/98:
// Rewrote AM_drawMarks(). Removed limit on marks.
//
// SoM: ANYRES support
//
static void AM_drawMarks(void)
{
   int i;
   for(i = 0; i < markpointnum; ++i) // killough 2/22/98: remove automap mark limit
   {
      if(markpoints[i].x != -1)
      {
         int w  = (5 * video.xscale) >> FRACBITS;
         int h  = (6 * video.yscale) >> FRACBITS;
         int fx = CXMTOF(markpoints[i].x);
         int fy = CYMTOF(markpoints[i].y);
         int j  = i;
         
         do
         {
            int d = j % 10;
            
            if(d == 1)          // killough 2/22/98: less spacing for '1'
               fx += (video.xscale >> FRACBITS);
            
            if(fx >= f_x && fx < f_w - w && fy >= f_y && fy < f_h - h)
            {
               V_DrawPatch((fx<<FRACBITS)/video.xscale, 
                           (fy<<FRACBITS)/video.yscale, 
                           &vbscreen, 
                           marknums[d]);
            }
            
            fx -= w - (video.xscale >> FRACBITS); // killough 2/22/98: 1 space backwards
            
            j /= 10;

         } while(j > 0);
      }
   }
}

//
// AM_drawCrosshair()
//
// Draw the single point crosshair representing map center
//
// Passed the color to draw the pixel with
// Returns nothing
// haleyjd: made inline static
//
inline static void AM_drawCrosshair(int color)
{
   vbscreen.data[(vbscreen.pitch * ((f_h + 1) >> 1)) + (vbscreen.width >> 1)] =
      color; // single point for now
}

//
// AM_Drawer()
//
// Draws the entire automap
//
// Passed nothing, returns nothing
//
void AM_Drawer(void)
{
   if(!automapactive)
      return;

   AM_clearFB(mapcolor_back);       //jff 1/5/98 background default color
   
   if(automap_grid)                 // killough 2/28/98: change var name
      AM_drawGrid(mapcolor_grid);   //jff 1/7/98 grid default color
   
   AM_drawWalls();

   // haleyjd 05/17/08:
   if(map_draw_nodelines)
      AM_drawNodeLines(numnodes - 1);

   AM_drawPlayers();
   
   if(ddt_cheating == 2)
      AM_drawThings(mapcolor_sprt, 0); //jff 1/5/98 default double IDDT sprite

   AM_drawCrosshair(mapcolor_hair); //jff 1/7/98 default crosshair color   
   AM_drawMarks();
}

//----------------------------------------------------------------------------
//
// $Log: am_map.c,v $
// Revision 1.24  1998/05/10  12:05:24  jim
// formatted/documented am_map
//
// Revision 1.23  1998/05/03  22:13:49  killough
// Provide minimal headers at top; no other changes
//
// Revision 1.22  1998/04/23  13:06:53  jim
// Add exit line to automap
//
// Revision 1.21  1998/04/16  16:16:56  jim
// Fixed disappearing marks after new level
//
// Revision 1.20  1998/04/03  14:45:17  jim
// Fixed automap disables at 0, mouse sens unbounded
//
// Revision 1.19  1998/03/28  05:31:40  jim
// Text enabling changes for DEH
//
// Revision 1.18  1998/03/23  03:06:22  killough
// I wonder
//
// Revision 1.17  1998/03/15  14:36:46  jim
// fixed secrets transfer bug in automap
//
// Revision 1.16  1998/03/10  07:06:21  jim
// Added secrets on automap after found only option
//
// Revision 1.15  1998/03/09  18:29:22  phares
// Created separately bound automap and menu keys
//
// Revision 1.14  1998/03/02  11:22:30  killough
// change grid to automap_grid and make external
//
// Revision 1.13  1998/02/23  04:08:11  killough
// Remove limit on automap marks, save them in savegame
//
// Revision 1.12  1998/02/17  22:58:40  jim
// Fixed bug of vanishinb secret sectors in automap
//
// Revision 1.11  1998/02/15  03:12:42  phares
// Jim's previous comment: Fixed bug in automap from mistaking framebuffer index for mark color
//
// Revision 1.10  1998/02/15  02:47:33  phares
// User-defined keys
//
// Revision 1.8  1998/02/09  02:50:13  killough
// move ddt cheat to st_stuff.c and some cleanup
//
// Revision 1.7  1998/02/02  22:16:31  jim
// Fixed bug in automap that showed secret lines
//
// Revision 1.6  1998/01/26  20:57:54  phares
// Second test of checkin/checkout
//
// Revision 1.5  1998/01/26  20:28:15  phares
// First checkin/checkout script test
//
// Revision 1.4  1998/01/26  19:23:00  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/24  11:21:25  jim
// Changed disables in automap to -1 and -2 (nodraw)
//
// Revision 1.1.1.1  1998/01/19  14:02:53  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------


// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//  Hardware Abstraction Layer for Video
//  haleyjd 05/08/11
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"   /* memory allocation wrappers -- killough */

// Need platform defines
#include "i_platform.h"

#include "../am_map.h"
#include "../c_runcmd.h"
#include "../d_gi.h"
#include "../d_main.h"
#include "../doomstat.h"
#include "../f_wipe.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../in_lude.h"
#include "../m_argv.h"
#include "../m_misc.h"
#include "../m_qstr.h"
#include "../r_main.h"
#include "../st_stuff.h"
#include "../v_misc.h"
#include "../v_video.h"

// Platform-Specific Video Drivers:
#ifdef _SDL_VER
#include "../sdl/i_sdlvideo.h"
#ifdef EE_FEATURE_OPENGL
#include "../sdl/i_sdlgl2d.h"
#endif
#endif

//=============================================================================
//
// Video Driver Object Pointer
//

HALVideoDriver *i_video_driver = NULL;

//=============================================================================
//
// Video Driver Table
//

// Config variable to select backend video driver module
int i_videodriverid = -1;

struct haldriveritem_t
{
   int id;                 // unique ID # assigned here by the HAL
   const char *name;       // descriptive name of this driver
   HALVideoDriver *driver; // driver object
};

// Help string for system.cfg:
const char *const i_videohelpstr =
  "Select video backend (-1 = default"
#ifdef _SDL_VER
  ", 0 = SDL Software"
#ifdef EE_FEATURE_OPENGL
  ", 1 = SDL GL2D"
#endif
#endif
  ")";

// Driver table
static haldriveritem_t halVideoDriverTable[VDR_MAXDRIVERS] =
{
   // SDL Software Driver
   {
      VDR_SDLSOFT,
      "SDL Software",
#ifdef _SDL_VER
      &i_sdlvideodriver
#else
      NULL
#endif
   },

   // SDL GL2D Driver
   {
      VDR_SDLGL2D,
      "SDL GL2D",
#if defined(_SDL_VER) && defined(EE_FEATURE_OPENGL)
      &i_sdlgl2dvideodriver
#else
      NULL
#endif
   }
};

//
// I_DefaultVideoDriver
//
// Chooses the default video driver based on user specifications, or on preset
// platform-specific defaults when there is no user specification.
//
static haldriveritem_t *I_DefaultVideoDriver()
{
   haldriveritem_t *item = NULL;

   if(i_videodriverid < 0 || i_videodriverid >= VDR_MAXDRIVERS ||
      halVideoDriverTable[i_videodriverid].driver == NULL)
   {
      // Default or plain invalid setting, or unsupported driver on current
      // compile. Find the lowest-numbered valid driver and use it.
      for(int i = 0; i < VDR_MAXDRIVERS; i++)
      {
         if(halVideoDriverTable[i].driver)
         {
            item = &halVideoDriverTable[i];
            break;
         }
      }

      // Nothing?! Somebody borked up their configure/makefile.
      if(!item)
        I_Error("I_DefaultVideoDriver: no valid drivers for this platform!\n");
   }
   else
      item = &halVideoDriverTable[i_videodriverid];

   return item;
}

//=============================================================================
//
// WM-related stuff (see i_input.c)
//

extern int  usejoystick;
extern int  grabmouse;
extern int  usemouse;   // killough 10/98
extern bool fullscreen;

void I_InitKeyboard();
void I_InitMouse();

//=============================================================================
//
// Graphics Code
//

int  use_vsync;     // killough 2/8/98: controls whether vsync is called
bool noblit;

bool in_graphics_mode;

// haleyjd 07/15/09
char *i_default_videomode;
char *i_videomode;

// haleyjd 06/17/11: software-mode bitdepth setting (for better -8in32)
int i_softbitdepth;

//
// I_FinishUpdate
//
void I_FinishUpdate()
{
   if(!noblit && in_graphics_mode)
      i_video_driver->FinishUpdate();
}

//
// I_ReadScreen
//
void I_ReadScreen(byte *scr)
{
   i_video_driver->ReadScreen(scr);
}

int disk_icon;

//
// killough 10/98: init disk icon
//
static void I_InitDiskFlash()
{
   // haleyjd 05/21/06: no disk in some game modes...
   if(GameModeInfo->flags & GIF_HASDISK)
      i_video_driver->InitDiskFlash();
}

//
// killough 10/98: draw disk icon
//
void I_BeginRead()
{
   if(disk_icon && in_graphics_mode)
      i_video_driver->BeginRead();
}

//
// killough 10/98: erase disk icon
//
void I_EndRead()
{
   if(disk_icon && in_graphics_mode)
      i_video_driver->EndRead();
}

//
// I_SetPalette
//
void I_SetPalette(byte *palette)
{
   if(in_graphics_mode)             // killough 8/11/98
      i_video_driver->SetPalette(palette);
}


void I_UnsetPrimaryBuffer()
{
   i_video_driver->UnsetPrimaryBuffer();
}


void I_SetPrimaryBuffer()
{
   i_video_driver->SetPrimaryBuffer();
}

void I_ShutdownGraphics()
{
   if(in_graphics_mode)
   {
      i_video_driver->ShutdownGraphics();
      in_graphics_mode = false;
      in_textmode = true;
   }
}

#define BADVID "video mode not supported"

extern bool setsizeneeded;

// states for geometry parser
enum
{
   STATE_WIDTH,
   STATE_HEIGHT,
   STATE_FLAGS
};

//
// I_ParseGeom
//
// Function to parse geometry description strings in the form [wwww]x[hhhh][f].
// This is now the primary way in which Eternity stores its video mode setting.
//
void I_ParseGeom(const char *geom, int *w, int *h, bool *fs, bool *vs, bool *hw,
                 bool *wf)
{
   const char *c = geom;
   int state = STATE_WIDTH;
   int tmpwidth = 320, tmpheight = 200;
   qstring qstr;
   bool errorflag = false;

   while(*c)
   {
      switch(state)
      {
      case STATE_WIDTH:
         if(*c >= '0' && *c <= '9')
            qstr += *c;
         else
         {
            int width = qstr.toInt();
            if(width < 320 || width > MAX_SCREENWIDTH)
            {
               state = STATE_FLAGS;
               errorflag = true;
            }
            else
            {
               tmpwidth = width;
               qstr.clear();
               state = STATE_HEIGHT;
            }
         }
         break;
      case STATE_HEIGHT:
         if(*c >= '0' && *c <= '9')
            qstr += *c;
         else
         {
            int height = qstr.toInt();
            if(height < 200 || height > MAX_SCREENHEIGHT)
            {
               state = STATE_FLAGS;
               errorflag = true;
            }
            else
            {
               tmpheight = height;
               state = STATE_FLAGS;
               continue; // don't increment the pointer
            }
         }
         break;
      case STATE_FLAGS:
         switch(tolower(*c))
         {
         case 'w': // window
            *fs = false;
            break;
         case 'f': // fullscreen
            *fs = true;
            break;
         case 'a': // async update
            *vs = false;
            break;
         case 'v': // vsync update
            *vs = true;
            break;
         case 's': // software
            *hw = false;
            break;
         case 'h': // hardware
            *hw = true;
            break;
         case 'n': // noframe
            *wf = false;
            break;
         default:
            break;
         }
         break;
      }
      ++c;
   }

   // handle termination of loop during STATE_HEIGHT (no flags)
   if(state == STATE_HEIGHT)
   {
      int height = qstr.toInt();

      if(height < 200 || height > MAX_SCREENHEIGHT)
         errorflag = true;
      else
         tmpheight = height;
   }

   // if an error occurs setting w/h, we default.
   if(errorflag)
   {
      tmpwidth  = 640;
      tmpheight = 480;
   }

   *w = tmpwidth;
   *h = tmpheight;
}

//
// I_CheckVideoCmds
//
// Checks for all video-mode-related command-line parameters in one
// convenient location. Though called from I_InitGraphicsMode, this
// function will only run once at startup. Resolution changes made at
// runtime want to use the precise settings specified through the UI
// instead.
//
void I_CheckVideoCmds(int *w, int *h, bool *fs, bool *vs, bool *hw, bool *wf)
{
   static bool firsttime = true;
   int p;

   if(firsttime)
   {
      firsttime = false;

      if((p = M_CheckParm("-geom")) && p < myargc - 1)
         I_ParseGeom(myargv[p + 1], w, h, fs, vs, hw, wf);

      if((p = M_CheckParm("-vwidth")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 320 && p <= MAX_SCREENWIDTH)
         *w = p;

      if((p = M_CheckParm("-vheight")) && p < myargc - 1 &&
         (p = atoi(myargv[p + 1])) >= 200 && p <= MAX_SCREENHEIGHT)
         *h = p;

      if(M_CheckParm("-fullscreen"))
         *fs = true;
      if(M_CheckParm("-nofullscreen") || M_CheckParm("-window"))
         *fs = false;

      if(M_CheckParm("-vsync"))
         *vs = true;
      if(M_CheckParm("-novsync"))
         *vs = false;

      if(M_CheckParm("-hardware"))
         *hw = true;
      if(M_CheckParm("-software"))
         *hw = false;

      if(M_CheckParm("-frame"))
         *wf = true;
      if(M_CheckParm("-noframe"))
         *wf = false;
   }
}

#ifdef _MSC_VER
extern void I_DisableSysMenu(void);
#endif

//
// I_InitGraphicsMode
//
// killough 11/98: New routine, for setting hires and page flipping
// sf: now returns true if an error occurred
//
static bool I_InitGraphicsMode(void)
{
   bool result;

   if(!i_default_videomode)
      i_default_videomode = estrdup("640x480w");

   if(!i_videomode)
      i_videomode = estrdup(i_default_videomode);

   // A false return value from HALVideoDriver::InitGraphicsMode means that no
   // errors have occured and we should continue with initialization.
   if(!(result = i_video_driver->InitGraphicsMode()))
   {
      // Reset renderer field of view
      R_ResetFOV(video.width, video.height);

#ifdef _MSC_VER
      // Win32 specific hack: disable system menu
      I_DisableSysMenu();
#endif

      V_Init();                 // initialize high-level video

      in_graphics_mode = true;  // now in graphics mode
      in_textmode      = false; // no longer in text mode
      setsizeneeded    = true;  // should initialize screen size

      I_InitDiskFlash();        // initialize disk flasher
   }

   return result;
}

//
// I_ResetScreen
//
// Changes the resolution by getting out of the old mode and into the new one,
// and then waking up any interested game-code modules that need to know about
// the screen resolution.
//
static void I_ResetScreen(void)
{
   int old_disk_icon = disk_icon;

   // [CG] Set disk_icon to 0, so disk reads don't try to flash the disk when
   //      there is no surface.
   disk_icon = 0;

   // Switch out of old graphics mode
   if(in_graphics_mode)
   {
      i_video_driver->ShutdownGraphicsPartway(); // haleyjd 10/15/05: WOOPS!
      in_graphics_mode = false;
      in_textmode = true;
   }

   // Switch to new graphics mode
   // check for errors -- we may be setting to a different mode instead
   if(I_InitGraphicsMode())
   {
      disk_icon = old_disk_icon; // [CG] Reset disk icon to its original value.
      return;
   }

   // reset other modules

   // Reset automap dimensions
   if(automapactive)
      AM_Start();

   // Reset palette
   ST_Start();

   // haleyjd: reset wipe engine
   Wipe_ScreenReset();

   // A LOT of heap activity just happened, so check it.
   Z_CheckHeap();

   disk_icon = old_disk_icon; // [CG] Reset disk icon to its original value.
}

void I_InitGraphics(void)
{
   static int firsttime = true;
   haldriveritem_t *driveritem = NULL;

   if(!firsttime)
      return;

   firsttime = false;

   // Select video driver based on configuration (out of those available in
   // the current compile), or get the default driver if unspecified
   if(!(driveritem = I_DefaultVideoDriver()))
   {
      I_Error("I_InitGraphics: invalid video driver %d\n", i_videodriverid);
   }
   else
   {
      i_video_driver  = driveritem->driver;
      i_videodriverid = driveritem->id;
      usermsg(" (using video driver '%s')", driveritem->name);
   }

   // haleyjd: not a good idea for SDL :(
   // if(nodrawers) // killough 3/2/98: possibly avoid gfx mode
   //    return;

   // init keyboard
   I_InitKeyboard();

   // haleyjd 05/10/11: init mouse
   I_InitMouse();

   //
   // enter graphics mode
   //

   atexit(I_ShutdownGraphics);

   V_ResetMode();

   Z_CheckHeap();
}

void I_SetMode(int i)
{
   static int firsttime = true;    // the first time to set mode

   if(firsttime)
      I_InitGraphicsMode();
   else
      I_ResetScreen();

   firsttime = false;
}

/************************
        CONSOLE COMMANDS
 ************************/

VARIABLE_BOOLEAN(use_vsync, NULL,  yesno);
VARIABLE_BOOLEAN(disk_icon, NULL,  onoff);

CONSOLE_VARIABLE(v_diskicon, disk_icon, 0) {}
CONSOLE_VARIABLE(v_retrace, use_vsync, 0)
{
   V_ResetMode();
}

VARIABLE_BOOLEAN(usemouse,    NULL, yesno);
VARIABLE_BOOLEAN(usejoystick, NULL, yesno);

CONSOLE_VARIABLE(i_usemouse, usemouse, 0) {}
CONSOLE_VARIABLE(i_usejoystick, usejoystick, 0) {}

// haleyjd 04/15/02: joystick sensitivity variables
VARIABLE_INT(joystickSens_x, NULL, -32768, 32767, NULL);
VARIABLE_INT(joystickSens_y, NULL, -32768, 32767, NULL);

CONSOLE_VARIABLE(joySens_x, joystickSens_x, 0) {}
CONSOLE_VARIABLE(joySens_y, joystickSens_y, 0) {}

// haleyjd 03/27/06: mouse grabbing
VARIABLE_BOOLEAN(grabmouse, NULL, yesno);
CONSOLE_VARIABLE(i_grabmouse, grabmouse, 0) {}

VARIABLE_STRING(i_videomode, NULL, UL);
CONSOLE_VARIABLE(i_videomode, i_videomode, cf_buffered)
{
   V_ResetMode();

   if(i_default_videomode)
      efree(i_default_videomode);

   i_default_videomode = estrdup(i_videomode);
}

CONSOLE_COMMAND(i_default_videomode, 0)
{
   /*
   if(i_default_videomode)
      efree(i_default_videomode);

   i_default_videomode = estrdup(i_videomode);
   */
}

static const char *i_videodrivernames[] =
{
   "default",
   "SDL Software",
   "SDL GL2D"
};

VARIABLE_INT(i_videodriverid, NULL, -1, VDR_MAXDRIVERS-1, i_videodrivernames);
CONSOLE_VARIABLE(i_videodriverid, i_videodriverid, 0) {}

VARIABLE_INT(i_softbitdepth, NULL, 8, 32, NULL);
CONSOLE_VARIABLE(i_softbitdepth, i_softbitdepth, 0) {}

void I_Video_AddCommands(void)
{
   C_AddCommand(i_usemouse);
   C_AddCommand(i_usejoystick);

   C_AddCommand(v_diskicon);
   C_AddCommand(v_retrace);

   C_AddCommand(joySens_x);
   C_AddCommand(joySens_y);

   C_AddCommand(i_grabmouse);

   C_AddCommand(i_videomode);
   C_AddCommand(i_default_videomode);

   C_AddCommand(i_videodriverid);

   C_AddCommand(i_softbitdepth);
}

// EOF
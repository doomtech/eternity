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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//      Main program, simply calls D_DoomMain high level loop.
//
//-----------------------------------------------------------------------------


#include "SDL.h"

#include "../z_zone.h"
#include "../doomdef.h"
#include "../m_argv.h"
#include "../d_main.h"
#include "../i_system.h"

// SoM 3/13/2001: Use SDL's signal handler

void I_Quit(void);

// SoM 3/11/2002: Disable the parachute for debugging.
// haleyjd 07/06/04: changed to a macro to eliminate local variable
// note: sound init is handled separately in i_sound.c

#define BASE_INIT_FLAGS (SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)

#ifdef _DEBUG
#define INIT_FLAGS (BASE_INIT_FLAGS | SDL_INIT_NOPARACHUTE)
#else
#define INIT_FLAGS BASE_INIT_FLAGS
#endif

int main(int argc, char **argv)
{
   myargc = argc;
   myargv = argv;
   
   // SoM: From CHOCODOOM Thank you fraggle!!
#ifdef _WIN32

   // Allow -gdi as a shortcut for using the windib driver.
   
   //!
   // @category video 
   // @platform windows
   //
   // Use the Windows GDI driver instead of DirectX.
   //
   
   // From the SDL 1.2.10 release notes: 
   //
   // > The "windib" video driver is the default now, to prevent 
   // > problems with certain laptops, 64-bit Windows, and Windows 
   // > Vista. 
   //
   // The hell with that.
   
   // SoM: the gdi interface is much faster for windowed modes which are more
   // commonly used. Thus, GDI is default.
   if(M_CheckParm("-directx"))
      putenv("SDL_VIDEODRIVER=directx");
   else if(M_CheckParm("-gdi") > 0 || getenv("SDL_VIDEODRIVER") == NULL)
      putenv("SDL_VIDEODRIVER=windib");
#endif

   // haleyjd 04/15/02: added check for failure
   if(SDL_Init(INIT_FLAGS) == -1)
   {
      puts("Failed to initialize SDL library.\n");
      return -1;
   }

   // haleyjd 02/23/04: ignore mouse events at startup
   //SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
   
   SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY/2, SDL_DEFAULT_REPEAT_INTERVAL*4);
   
   Z_Init();
   atexit(I_Quit);
   
   D_DoomMain();
   
   return 0;
}


//----------------------------------------------------------------------------
//
// $Log: i_main.c,v $
//
//----------------------------------------------------------------------------

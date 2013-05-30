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
//      System specific interface stuff.
//
//-----------------------------------------------------------------------------

#ifndef I_SYSTEM_H__
#define I_SYSTEM_H__

struct ticcmd_t;

// Called by DoomMain.
void I_Init(void);

// Called by D_DoomLoop,
// returns current time in tics.
// int I_GetTime (void);

extern int (*I_GetTime)();           // killough
int I_GetTime_RealTime();     // killough
int I_GetTime_Adaptive(void); // killough 4/10/98
void I_SetTime(int newtime);
extern int GetTime_Scale;

unsigned int I_GetTicks(void);

//
// Called by D_DoomLoop,
// called before processing any tics in a frame
// (just after displaying a frame).
// Time consuming syncronous operations
// are performed here (joystick reading).
// Can call D_PostEvent.
//

void I_StartFrame (void);

//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick syncronous operations are performed here.
// Can call D_PostEvent.

void I_StartTic(void);

// Asynchronous interrupt functions should maintain private queues
// that are read by the synchronous functions
// to be converted into events.

// Either returns a null ticcmd,
// or calls a loadable driver to build it.
// This ticcmd will then be modified by the gameloop
// for normal input.

ticcmd_t* I_BaseTiccmd (void);

// atexit handler -- killough

void I_Quit(void);

#define I_Tactile(on, off, total)

// haleyjd 05/21/10: error codes for I_FatalError
enum
{
   I_ERR_KILL,  // exit and do not perform shutdown actions
   I_ERR_ABORT  // call abort()
};

// haleyjd 06/05/10
void I_ExitWithMessage(const char *msg, ...);

#ifdef __GNUC__
// haleyjd 05/21/10
void I_FatalError(int code, const char *error, ...) __attribute__((noreturn, format(printf,2,3)));

// killough 3/20/98: add const
// killough 4/25/98: add gcc attributes
void I_Error(const char *error, ...) __attribute__((noreturn, format(printf,1,2)));

void I_ErrorVA(const char *error, va_list args) __attribute__((noreturn));
#else
// haleyjd 05/21/10
void I_FatalError(int code, const char *error, ...);

//SoM 3/14/2002: vc++ 
void I_Error(const char *error, ...);

void I_ErrorVA(const char *error, va_list args);
#endif

extern int mousepresent;                // killough

#ifdef DJGPP
extern int leds_always_off;   // killough 10/98
void I_ResetLEDs(void);       // killough 10/98
#endif

void I_EndDoom(void);         // killough 2/22/98: endgame screen

// SoM 3/14/2002: Gyah!
#ifdef DJGPP
// killough 3/21/98: keyboard queue

#define KQSIZE 256

extern struct keyboard_queue_s {
  volatile int head,tail,queue[KQSIZE];
} keyboard_queue;
#endif

int I_CheckAbort();

// haleyjd 04/15/02: SDL joystick stuff

#ifdef _SDL_VER

typedef struct jsdata_s
{
   char *description;
} jsdata_t;

extern jsdata_t *joysticks;

#endif

// haleyjd 06/16/09: I_Sleep prototype
void I_Sleep(int ms);

#endif

//----------------------------------------------------------------------------
//
// $Log: i_system.h,v $
// Revision 1.7  1998/05/03  22:33:43  killough
// beautification, remove unnecessary #includes
//
// Revision 1.6  1998/04/27  01:52:47  killough
// Add __attribute__ to I_Error for gcc checking
//
// Revision 1.5  1998/04/10  06:34:07  killough
// Add adaptive gametic timer
//
// Revision 1.4  1998/03/23  03:17:19  killough
// Add keyboard FIFO queue and make I_Error arg const
//
// Revision 1.3  1998/02/23  04:28:30  killough
// Add ENDOOM support
//
// Revision 1.2  1998/01/26  19:26:59  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:10  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

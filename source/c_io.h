// Emacs style mode select -*- C -*-
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

#ifndef C_IO_H__
#define C_IO_H__

#include "doomstat.h"
#include "d_event.h"
        // for text colours:
#include "v_video.h"
#include "m_qstr.h"

#define INPUTLENGTH 512
#define LINELENGTH  128

void    C_Init(void);
void    C_Ticker(void);
void    C_Drawer(void);
boolean C_Responder(event_t* ev);
void    C_Update(void);

void C_Printf(const char *s, ...);
void C_Puts(const char *s);
void C_Separator(void);

void C_SetConsole(void);
void C_Popup(void);
void C_InstaPopup(void);

// haleyjd
void C_OpenConsoleLog(qstring_t *filename);
void C_CloseConsoleLog(void);
void C_DumpMessages(qstring_t *filename);

// sf 9/99: made a #define
#define consoleactive (Console.current_height || gamestate == GS_CONSOLE)
#define c_moving      (Console.current_height != Console.current_target)

extern int c_height;     // the height of the console
extern int c_speed;       // pixels/tic it moves

extern const char *c_fontname;

#endif

// EOF


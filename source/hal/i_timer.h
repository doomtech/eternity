// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:  
//    Hardware Abstraction Layer for Timing
//
//-----------------------------------------------------------------------------

#ifndef I_TIMER_H__
#define I_TIMER_H__

//sf: made a #define, changed to 16
#define CLOCK_BITS 16
#define CLOCK_UNIT (1<<CLOCK_BITS)
extern int     realtic_clock_rate;
extern int64_t I_GetTime_Scale;

typedef void         (*HAL_TimerInitFunc)();
typedef void         (*HAL_ChangeClockRateFunc)();

typedef int          (*HAL_GetTimeFunc)();
typedef unsigned int (*HAL_GetTicksFunc)();
typedef void         (*HAL_SleepFunc)(int);

//
// HALTimer
//
// Unlike most HAL objects, HALTimer is a POD structure with function pointers,
// in order to avoid any virtual dispatch overhead. There is only one instance
// of the structure, and implementing layers initialize it with their function
// pointer values.
//
struct HALTimer
{
   HAL_GetTimeFunc         GetTime;         // get time in gametics, possibly scaled
   HAL_GetTimeFunc         GetRealTime;     // get time in gametics regardless of scaling
   HAL_GetTicksFunc        GetTicks;        // get time in milliseconds
   HAL_SleepFunc           Sleep;           // sleep for time in milliseconds
};

extern HALTimer i_haltimer;

void I_InitHALTimer();

#endif

// EOF


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
//   Scrollers, carriers, and related effects
//
//-----------------------------------------------------------------------------

#ifndef P_SCROLL_H__
#define P_SCROLL_H__

#include "m_fixed.h" // for fixed_t
#include "p_tick.h"  // for Thinker

class SaveArchive;

// killough 3/7/98: Add generalized scroll effects

class ScrollThinker : public Thinker
{
   DECLARE_THINKER_TYPE(ScrollThinker, Thinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   
   // Data Members
   fixed_t dx, dy;      // (dx,dy) scroll speeds
   int affectee;        // Number of affected sidedef, sector, tag, or whatever
   int control;         // Control sector (-1 if none) used to control scrolling
   fixed_t last_height; // Last known height of control sector
   fixed_t vdx, vdy;    // Accumulated velocity if accelerative
   int accel;           // Whether it's accelerative
   enum
   {
      sc_side,
      sc_floor,
      sc_ceiling,
      sc_carry,
      sc_carry_ceiling,  // killough 4/11/98: carry objects hanging on ceilings
   };
   int type;              // Type of scroll effect
};

void Add_Scroller(int type, fixed_t dx, fixed_t dy,
                  int control, int affectee, int accel);

void P_SpawnScrollers();

#endif

// EOF


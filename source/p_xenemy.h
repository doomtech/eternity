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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//
// Hexen-inspired action functions
//
// DISCLAIMER: None of this code was taken from Hexen. Any
// resemblence is purely coincidental or is the result of work from
// a common base source.
//
//-----------------------------------------------------------------------------

#ifndef P_XENEMY_H__
#define P_XENEMY_H__

#include "p_mobj.h"

// Earthquakes

class QuakeThinker : public PointThinker
{
   DECLARE_THINKER_TYPE(QuakeThinker, PointThinker)

protected:
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);

   // Data Members
   int intensity;        // richter scale (hardly realistic)
   int duration;         // how long it lasts
   fixed_t quakeRadius;  // radius of shaking effects
   fixed_t damageRadius; // radius of damage effects (if any)
};

bool P_StartQuake(int *args);

#endif

// EOF


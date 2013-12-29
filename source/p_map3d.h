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
//      Map functions
//
//-----------------------------------------------------------------------------

#ifndef P_MAP3D_H__
#define P_MAP3D_H__

class Mobj;
struct sector_t;

bool  P_Use3DClipping();
bool  P_TestMobjZ(Mobj *mo);
bool  P_CheckPosition3D(Mobj *thing, fixed_t x, fixed_t y);
bool  P_CheckPositionExt(Mobj *mo, fixed_t x, fixed_t y);
bool  P_ChangeSector3D(sector_t *sector, int crunch, int amt, int floorOrCeil);
Mobj *P_GetThingUnder(Mobj *mo);

#endif 

// EOF


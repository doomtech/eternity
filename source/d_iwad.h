// Emacs style mode select   -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//    Routines for IWAD location, version identification, and loading.
//    Split out of d_main.cpp 08/12/12.
//
//-----------------------------------------------------------------------------

#ifndef D_IWAD_H__
#define D_IWAD_H__

extern bool freedoom;

char *D_FindInDoomWadPath(const char *filename, const char *extension);
void D_IdentifyVersion();

#endif

// EOF


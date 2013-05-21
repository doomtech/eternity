// Emacs style mode select -*- C++ -*- vi:ts=3:sw=3:set et:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//    Stuff that will only be needed until Eternity has converted up to the
//    C++11 standard.
//
//-----------------------------------------------------------------------------

#ifndef PRECPP11_H__
#define PRECPP11_H__

namespace eeprestd
{
  //
  // remove_pointer
  //
  // This series of partially-specialized templates allows the "pointerism" of
  // template parameters to be discarded, retrieving the base type for access
  // to static methods and members, amongst other possible uses.
  //
  // Originally defined by the Boost library and then absorbed into C++11 as
  // std::remove_pointer in the <type_traits> header.
  //
  template<typename T> struct remove_pointer           { typedef T type; };
  template<typename T> struct remove_pointer<T *>      { typedef T type; };
  template<typename T> struct remove_pointer<T *const> { typedef T type; };
}

#endif

// EOF

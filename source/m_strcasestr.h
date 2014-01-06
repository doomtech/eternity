// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2005-2007 Free Software Foundation, Inc.
// Written by Bruno Haible <bruno@clisp.org>, 2005.
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
// GNUlib strcasestr implementation for case-insensitive substring detection.
//
//-----------------------------------------------------------------------------

#ifndef M_STRCASESTR_H__
#define M_STRCASESTR_H__

const char *M_StrCaseStr(const char *haystack, const char *needle);
      char *M_StrCaseStr(      char *haystack, const char *needle);

#endif

// EOF


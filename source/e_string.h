// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// EDF Strings Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_STRING_H__
#define E_STRING_H__

#include "m_dllist.h"

struct edf_string_t
{
   DLListItem<edf_string_t> numlinks; // next, prev links for numeric hash chain
   edf_string_t *next;                 // next in mnemonic hash chain

   char *string;       // string value
   char key[129];      // mnemonic for hashing
   int  numkey;        // number for hashing
};

edf_string_t *E_CreateString(const char *value, const char *key, int num);
edf_string_t *E_StringForName(const char *key);
edf_string_t *E_GetStringForName(const char *key);
edf_string_t *E_StringForNum(int num);
edf_string_t *E_GetStringForNum(int num);

#ifdef NEED_EDF_DEFINITIONS
#define EDF_SEC_STRING "string"
extern cfg_opt_t edf_string_opts[];

void E_ProcessStrings(cfg_t *cfg);
#endif

#endif

// EOF


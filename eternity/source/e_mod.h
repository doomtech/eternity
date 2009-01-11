// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley
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
//    Custom damage types, or "Means of Death" flags.
//
//-----------------------------------------------------------------------------

#ifndef E_MOD_H__
#define E_MOD_H__

#include "doomtype.h"
#include "m_dllist.h"

//
// emod structure
//
// Holds the info for a damage type.
//
typedef struct emod_s
{
   mdllistitem_t numlinks; // for numeric hash

   const char *name;
   int num;
   char *obituary;
   char *selfobituary;
   boolean obitIsBexString;
   boolean selfObitIsBexString;
   boolean sourceless;

   struct emod_s *nextname; // next by name
} emod_t;

emod_t *E_DamageTypeForName(const char *name);
emod_t *E_DamageTypeForNum(int num);
int     E_DamageTypeNumForName(const char *name);

// EDF-only stuff
#ifdef NEED_EDF_DEFINITIONS

#define EDF_SEC_MOD "damagetype"

extern cfg_opt_t edf_dmgtype_opts[];

void E_ProcessDamageTypes(cfg_t *cfg);

#endif // NEED_EDF_DEFINITIONS

#endif

// EOF


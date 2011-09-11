// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley, Stephen McGranahan, et al.
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
//      Inline linked portal predicates.
//      SoM created 12/23/07
//
//-----------------------------------------------------------------------------

#ifndef R_PORTALCHECK_H__
#define R_PORTALCHECK_H__

#include "r_portal.h"
#include "r_defs.h"

extern int demo_version;

//
// R_FPCam
//
// haleyjd 3/17/08: Convenience routine to clean some shit up.
//
d_inline static linkdata_t *R_FPLink(sector_t *s)
{
   return &(s->f_portal->data.link);
}

//
// R_CPCam
//
// ditto
//
d_inline static linkdata_t *R_CPLink(sector_t *s)
{
   return &(s->c_portal->data.link);
}


#endif

//----------------------------------------------------------------------------
//
// $Log: r_pcheck.h,v $
//
//----------------------------------------------------------------------------

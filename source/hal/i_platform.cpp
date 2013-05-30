// Emacs style mode select   -*- C++ -*- 
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
//    Platform Defines
//
//-----------------------------------------------------------------------------

#include "i_platform.h"

int ee_current_platform = EE_CURRENT_PLATFORM;
int ee_current_compiler = EE_CURRENT_COMPILER;

int ee_platform_flags[EE_PLATFORM_MAX] =
{
   0,             // Windows
   EE_PLATF_CSFS, // Linux
   EE_PLATF_CSFS, // FreeBSD
   0,             // MacOS X
   0              // Unknown
};

// EOF


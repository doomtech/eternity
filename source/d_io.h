// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//  IO. Deals with the differences between VC++ and other languages.
//
//-----------------------------------------------------------------------------

#ifndef __D_IO_H__
#define __D_IO_H__

// SoM 3/12/2002: Take all of this out of the source files themselves

#ifdef _MSC_VER

   #include <direct.h>
   #include <io.h>
   #define F_OK 0
   #define W_OK 2
   #define R_OK 4
   #define S_ISDIR(x) (((sbuf.st_mode & S_IFDIR)==S_IFDIR)?1:0)
   #ifndef PATH_MAX
      #define PATH_MAX _MAX_PATH
   #endif

#else
#include <unistd.h>
#endif

#ifdef LINUX
   #ifdef __FreeBSD__ // [Kate] Yeeaah...
   #include <limits.h>
   #else
   #include <linux/limits.h>
   #endif
#endif

#endif

//----------------------------------------------------------------------------
//
// $Log: d_io.h,v $
//
//
//----------------------------------------------------------------------------

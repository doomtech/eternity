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
//      Savegame I/O, archiving, persistence.
//
//-----------------------------------------------------------------------------

#ifndef __P_SAVEG__
#define __P_SAVEG__

// Persistent storage/archiving.
// These are the load / save game routines.

// [CG] GCC needs stdlib for size_t and NULL.
#ifdef __GNUC__
#include <stdlib.h>
#endif

class  Thinker;
class  Mobj;
class  OutBuffer;
class  InBuffer;
struct spectransfer_t;
struct mapthing_t;
struct sector_t;
struct line_t;

class SaveArchive
{
protected:
   OutBuffer *savefile;        // valid when saving
   InBuffer  *loadfile;        // valid when loading

public:
   SaveArchive(OutBuffer *pSaveFile);
   SaveArchive(InBuffer  *pLoadFile);

   // Accessors
   bool isSaving()  const   { return (savefile != NULL); }
   bool isLoading() const   { return (loadfile != NULL); }
   OutBuffer *getSaveFile() { return savefile; }
   InBuffer  *getLoadFile() { return loadfile; }

   // Methods
   void ArchiveCString(char *str,  size_t maxLen);
   void ArchiveLString(char *&str, size_t &len);
   
   // WriteLString is valid during saving only. This is to accomodate const
   // char *'s which must be saved, and are read into temporary buffers 
   // during loading.
   void WriteLString(const char *str, size_t len = 0);

   // Operators
   // Similar to ZDoom's FArchive class, these are symmetric - they are used
   // both for reading and writing.
   // Basic types:
   SaveArchive &operator << (int32_t  &x);
   SaveArchive &operator << (uint32_t &x);
   SaveArchive &operator << (int16_t  &x);
   SaveArchive &operator << (uint16_t &x);
   SaveArchive &operator << (int8_t   &x);
   SaveArchive &operator << (uint8_t  &x); 
   SaveArchive &operator << (bool     &x);
   SaveArchive &operator << (float    &x);
   SaveArchive &operator << (double   &x);
   // Pointers:
   SaveArchive &operator << (sector_t *&s);
   SaveArchive &operator << (line_t   *&ln);
   // Structures:
   SaveArchive &operator << (spectransfer_t &st);
   SaveArchive &operator << (mapthing_t     &mt);
};

// Global template functions for SaveArchive

//
// P_ArchiveArray
//
// Archives an array. The base element of the array must have an
// operator << overload for SaveArchive.
//
template <typename T>
void P_ArchiveArray(SaveArchive &arc, T *ptrArray, int numElements)
{
   for(int i = 0; i < numElements; i++)
      arc << ptrArray[i];
}

// haleyjd: These utilities are now needed for external serialization support
// in Thinker-derived classes.
unsigned int P_NumForThinker(Thinker *th);
Thinker *P_ThinkerForNum(unsigned int n);
void P_SetNewTarget(Mobj **mop, Mobj *targ);

void P_SaveCurrentLevel(char *filename, char *description);
void P_LoadGame(const char *filename);

#endif

//----------------------------------------------------------------------------
//
// $Log: p_saveg.h,v $
// Revision 1.5  1998/05/03  23:10:40  killough
// beautification
//
// Revision 1.4  1998/02/23  04:50:09  killough
// Add automap marks and properties to saved state
//
// Revision 1.3  1998/02/17  06:26:04  killough
// Remove unnecessary plat archive/unarchive funcs
//
// Revision 1.2  1998/01/26  19:27:26  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:06  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

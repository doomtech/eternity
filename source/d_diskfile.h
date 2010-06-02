// Emacs style mode select   -*- C -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//    Code to deal with disk files.
//    
//-----------------------------------------------------------------------------

#ifndef D_DISK_H__
#define D_DISK_H__

//
// diskfile
//
// Represents a specific type of "disk" file format which has 72-byte entries
// in the header for each file, and 2 words of other information including the
// number of files. This is an opaque type, similar to a FILE *.
//
typedef struct diskfile_s
{
   void *opaque;
} diskfile_t;

//
// diskwad
//
// This struct is returned from D_FindWadInDiskFile and contains information on a
// logical wad file found in the physical disk file.
//
typedef struct diskwad_s
{
   FILE *f;          // file pointer
   size_t offset;    // offset of wad within physical file
   const char *name; // canonical file name
} diskwad_t;

diskfile_t *D_OpenDiskFile(const char *filename);
diskwad_t D_FindWadInDiskFile(diskfile_t *df, const char *filename);
void *D_CacheDiskFileResource(diskfile_t *df, const char *path, boolean text);
void D_CloseDiskFile(diskfile_t *df, boolean closefile);

#endif

// EOF


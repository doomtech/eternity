// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
//----------------------------------------------------------------------------
//
// C/S resource (WAD, DEH, etc.) routines.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

#ifndef __CS_WAD_H__
#define __CS_WAD_H__

#include "doomtype.h"

typedef enum
{
   rt_iwad,
   rt_pwad,
   rt_deh,
} resource_type_t;

typedef struct cs_resource_s
{
   char *name;
   char *path;
   resource_type_t type;
   char sha1_hash[41];
} cs_resource_t;

typedef struct cs_map_s
{
   char *name;
   boolean initialized;
   unsigned int resource_count;
   unsigned int *resource_indices;
} cs_map_t;

extern const char *cs_iwad;
extern char *cs_wad_repository;
extern cs_map_t *cs_maps;
extern unsigned int cs_map_count;
extern cs_resource_t *cs_resources;
extern unsigned int cs_resource_count;
extern unsigned int cs_current_map_index;
extern unsigned int cs_current_map_number;

void CS_ClearMaps(void);
char* CS_DownloadWAD(const char *wad_name);
boolean CS_AddIWAD(const char *resource_name);
boolean CS_AddWAD(const char *resource_name);
boolean CS_AddDeHackEdFile(const char *resource_name);
cs_resource_t* CS_GetResource(const char *resource_name);
boolean CS_CheckResourceHash(const char *resource_name, const char *sha1_hash);
void CS_AddMapAtIndex(const char *name, unsigned int resource_count,
                                        unsigned int *resource_indices,
                                        unsigned int index);
void CS_AddMap(const char *name, unsigned int resource_count,
                                 unsigned int *resource_indices);
boolean CS_LoadMap(void);
void CS_NewGame(void);
void CS_InitNew(void);

#endif


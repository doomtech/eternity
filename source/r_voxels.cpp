// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
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
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomtype.h"
#include "m_swap.h"
#include "w_wad.h"

#include "r_voxels.h"

//
// R_LoadVoxelResource
//
// Loads a .vox-format voxel model into an rvoxelmodel_t structure.
//
rvoxelmodel_t *R_LoadVoxelResource(int lumpnum)
{
   rvoxelmodel_t *model = NULL;
   byte *buffer = NULL, *rover = NULL;
   int lumplen  = W_LumpLength(lumpnum);
   int xsize, ysize, zsize, voxsize;
   int i;

   // minimum size test
   if(lumplen < 12)
      return NULL;

   // cache the lump
   rover = buffer = (byte *)(wGlobalDir.CacheLumpNum(lumpnum, PU_STATIC));

   // get sizes
   xsize = SwapLong(*(int32_t *)rover);
   rover += 4;
   ysize = SwapLong(*(int32_t *)rover);
   rover += 4;
   zsize = SwapLong(*(int32_t *)rover);
   rover += 4;

   voxsize = xsize*ysize*zsize;

   // true size test
   if(lumplen < 12 + voxsize + 768)
   {
      Z_ChangeTag(buffer, PU_CACHE);
      return NULL;
   }

   // create the model and its voxel buffer
   model         = (rvoxelmodel_t *)(Z_Calloc(1,       sizeof(rvoxelmodel_t), PU_RENDERER, NULL));
   model->voxels =          (byte *)(Z_Calloc(voxsize, sizeof(byte),          PU_RENDERER, NULL));

   model->xsize = xsize;
   model->ysize = ysize;
   model->zsize = zsize;

   // get voxel data
   memcpy(model->voxels, rover, voxsize);
   rover += voxsize;

   // get original palette data
   memcpy(model->palette, rover, 768);

   // transform palette data into 0-255 range
   // TODO: verify color component order
   for(i = 0; i < 768; i++)
      model->palette[i] <<= 2;

   // TODO: run V_FindBestColor to create translated palette?

   // done with lump
   Z_ChangeTag(buffer, PU_CACHE);

   return model;
}

// EOF



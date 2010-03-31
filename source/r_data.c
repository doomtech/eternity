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
//      Preparation of data for rendering,
//      generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------

#include <time.h>

#include "z_zone.h"
#include "doomstat.h"
#include "c_io.h"
#include "d_main.h"
#include "w_wad.h"
#include "p_skin.h"
#include "p_setup.h"
#include "r_main.h"
#include "r_sky.h"
#include "v_video.h"
#include "p_info.h" // haleyjd
#include "d_io.h" // SoM 3/14/2002: strncasecmp
#include "m_misc.h"
#include "e_hash.h"


static void R_LoadDoom1();
static int R_Doom1Texture(const char *name);

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//

// ============================================================================
// SoM: Moved textures to r_textur.c

void R_InitTextures(void);

// killough 4/17/98: make firstcolormaplump,lastcolormaplump external
int         firstcolormaplump, lastcolormaplump;      // killough 4/17/98
int         firstspritelump, lastspritelump, numspritelumps;

// needed for pre-rendering
fixed_t     *spritewidth, *spriteoffset, *spritetopoffset;
// SoM: used by cardboard
float       *spriteheight;

//
// R_InitSpriteLumps
//
// Finds the width and hoffset of all sprites in the wad,
// so the sprite does not need to be cached completely
// just for having the header info ready during rendering.
//
void R_InitSpriteLumps(void)
{
   int i;
   patch_t *patch;
   
   firstspritelump = W_GetNumForName("S_START") + 1;
   lastspritelump = W_GetNumForName("S_END") - 1;
   numspritelumps = lastspritelump - firstspritelump + 1;
   
   // killough 4/9/98: make columnd offsets 32-bit;
   // clean up malloc-ing to use sizeof
   
   spritewidth = 
      Z_Malloc(numspritelumps * sizeof(*spritewidth), PU_RENDERER, 0);
   spriteoffset = 
      Z_Malloc(numspritelumps * sizeof(*spriteoffset), PU_RENDERER, 0);
   spritetopoffset =
      Z_Malloc(numspritelumps * sizeof(*spritetopoffset), PU_RENDERER, 0);
   spriteheight = 
      Z_Malloc(numspritelumps * sizeof(float), PU_RENDERER, 0);
   
   for(i = 0; i < numspritelumps; ++i)
   {
      // sf: loading pic
      if(!(i&127))            // killough
         V_LoadingIncrease();
      
      patch = W_CacheLumpNum(firstspritelump + i, PU_CACHE);

      spritewidth[i]     = SwapShort(patch->width) << FRACBITS;
      spriteoffset[i]    = SwapShort(patch->leftoffset) << FRACBITS;
      spritetopoffset[i] = SwapShort(patch->topoffset) << FRACBITS;
      spriteheight[i]    = (float)(SwapShort(patch->height));
   }
}

//
// R_InitColormaps
//
// killough 3/20/98: rewritten to allow dynamic colormaps
// and to remove unnecessary 256-byte alignment
//
// killough 4/4/98: Add support for C_START/C_END markers
//
void R_InitColormaps(void)
{
   int i;

   firstcolormaplump = W_GetNumForName("C_START");
   lastcolormaplump  = W_GetNumForName("C_END");
   numcolormaps      = lastcolormaplump - firstcolormaplump;
   colormaps = Z_Malloc(sizeof(*colormaps) * numcolormaps, PU_RENDERER, 0);
   
   colormaps[0] = W_CacheLumpNum(W_GetNumForName("COLORMAP"), PU_RENDERER);
   
   for(i = 1; i < numcolormaps; ++i)
      colormaps[i] = W_CacheLumpNum(i + firstcolormaplump, PU_RENDERER);
}

// haleyjd: new global colormap system -- simply sets an index to
//   the appropriate colormap and the rendering code checks this
//   instead of assuming it should always use colormap 0 -- much
//   cleaner but requires more modification to the rendering code.
//
// 03/04/07: added outdoor fogmap

int global_cmap_index = 0;
int global_fog_index  = 0;

void R_SetGlobalLevelColormap(void)
{
   global_cmap_index = R_ColormapNumForName(LevelInfo.colorMap);
   global_fog_index  = R_ColormapNumForName(LevelInfo.outdoorFog);

   // 04/15/08: this error can be tolerated; default to COLORMAP.
   if(global_cmap_index < 0)
      global_cmap_index = 0;
   if(global_fog_index < 0)
      global_fog_index = 0;
}

// killough 4/4/98: get colormap number from name
// killough 4/11/98: changed to return -1 for illegal names
// killough 4/17/98: changed to use ns_colormaps tag

int R_ColormapNumForName(const char *name)
{
   register int i = 0;
   if(strncasecmp(name, "COLORMAP", 8))     // COLORMAP predefined to return 0
      if((i = W_CheckNumForNameNS(name, ns_colormaps)) != -1)
         i -= firstcolormaplump;
   return i;
}


int tran_filter_pct = 66;       // filter percent

#define TSC 12        /* number of fixed point digits in filter percent */

// haleyjd 06/09/09: moved outside and added packing pragmas

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

struct trmapcache_s 
{
   char signature[4];          // haleyjd 06/09/09: added
   byte pct;
   byte playpal[768]; // haleyjd 06/09/09: corrected 256->768
} __attribute__((packed));

#ifdef _MSC_VER
#pragma pack(pop)
#endif

// haleyjd 06/09/09: for version control of tranmap.dat
#define TRANMAPSIG "Etm1"

//
// R_InitTranMap
//
// Initialize translucency filter map
//
// By Lee Killough 2/21/98
//
void R_InitTranMap(int progress)
{
   int lump = W_CheckNumForName("TRANMAP");
   
   // If a translucency filter map lump is present, use it
   
   if(lump != -1)  // Set a pointer to the translucency filter maps.
      main_tranmap = W_CacheLumpNum(lump, PU_RENDERER);   // killough 4/11/98
   else
   {
      // Compose a default transparent filter map based on PLAYPAL.
      unsigned char *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
      
      char *fname = NULL;
      unsigned int fnamesize;
      
      struct trmapcache_s cache;
      
      FILE *cachefp;

      // haleyjd 11/23/06: use basegamepath
      // haleyjd 12/06/06: use Z_Alloca for path length limit removal
      fnamesize = M_StringAlloca(&fname, 1, 12, basegamepath);

      psnprintf(fname, fnamesize, "%s/tranmap.dat", basegamepath);
      
      cachefp = fopen(fname, "r+b");

      main_tranmap = Z_Malloc(256*256, PU_RENDERER, 0);  // killough 4/11/98
      
      // Use cached translucency filter if it's available

      if(!cachefp ? cachefp = fopen(fname, "wb") , 1 :
         fread(&cache, 1, sizeof cache, cachefp) != sizeof cache ||
         strncmp(cache.signature, TRANMAPSIG, 4) ||
         cache.pct != tran_filter_pct ||
         memcmp(cache.playpal, playpal, sizeof cache.playpal) ||
         fread(main_tranmap, 256, 256, cachefp) != 256 ) // killough 4/11/98
      {
         int pal[3][256], tot[256], pal_w1[3][256];
         int w1 = ((unsigned int) tran_filter_pct<<TSC)/100;
         int w2 = (1l<<TSC)-w1;

         // First, convert playpal into long int type, and transpose array,
         // for fast inner-loop calculations. Precompute tot array.

         {
            register int i = 255;
            register const unsigned char *p = playpal + 255 * 3;
            do
            {
               register int t,d;
               pal_w1[0][i] = (pal[0][i] = t = p[0]) * w1;
               d = t*t;
               pal_w1[1][i] = (pal[1][i] = t = p[1]) * w1;
               d += t*t;
               pal_w1[2][i] = (pal[2][i] = t = p[2]) * w1;
               d += t*t;
               p -= 3;
               tot[i] = d << (TSC - 1);
            }
            while (--i >= 0);
         }

         // Next, compute all entries using minimum arithmetic.
         
         {
            int i,j;
            byte *tp = main_tranmap;
            for(i = 0; i < 256; ++i)
            {
               int r1 = pal[0][i] * w2;
               int g1 = pal[1][i] * w2;
               int b1 = pal[2][i] * w2;
               
               if(!(i & 31) && progress)
                  V_LoadingIncrease();        //sf 
               
               if(!(~i & 15))
               {
                  if (i & 32)       // killough 10/98: display flashing disk
                     I_EndRead();
                  else
                     I_BeginRead();
               }

               for(j = 0; j < 256; ++j, ++tp)
               {
                  register int color = 255;
                  register int err;
                  int r = pal_w1[0][j] + r1;
                  int g = pal_w1[1][j] + g1;
                  int b = pal_w1[2][j] + b1;
                  int best = LONG_MAX;
                  do
                  {
                     if((err = tot[color] - pal[0][color]*r
                        - pal[1][color]*g - pal[2][color]*b) < best)
                        best = err, *tp = color;
                  }
                  while(--color >= 0);
               }
            }
         }

         if(cachefp)        // write out the cached translucency map
         {
            strncpy(cache.signature, TRANMAPSIG, 4);
            cache.pct = tran_filter_pct;
            memcpy(cache.playpal, playpal, 768); // haleyjd: corrected 256->768
            fseek(cachefp, 0, SEEK_SET);
            fwrite(&cache, 1, sizeof cache, cachefp);
            fwrite(main_tranmap, 256, 256, cachefp);
         }
      }
      else if(progress)
      {
         int i;
         for(i = 0; i < 8; ++i)
            V_LoadingIncrease();    // 8 '.'s
      }
      
      if(cachefp)              // killough 11/98: fix filehandle leak
         fclose(cachefp);

      Z_ChangeTag(playpal, PU_CACHE);
   }
}

//
// R_InitData
//
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData(void)
{
   static boolean firsttime = true;

   P_InitSkins();
   R_InitColormaps();                    // killough 3/20/98
   R_ClearSkyTextures();                 // haleyjd  8/30/02
   R_InitTextures();
   R_InitSpriteLumps();

   if(general_translucency)             // killough 3/1/98, 10/98
   {
      R_InitTranMap(1);          // killough 2/21/98, 3/6/98
   }
   else
   {
      // sf: fill in dots missing from no translucency build
      int i;
      for(i = 0; i < 8; ++i)
         V_LoadingIncrease();    // 8 '.'s
   }

   // haleyjd 11/21/09: first time through here, set DOOM thingtype translucency
   // styles. Why only the first time? We don't need to do this if R_Init is 
   // invoked through adding a new wad file.
   
   // haleyjd 01/28/10: also initialize tantoangle_acc table
   
   if(firsttime)
   {
      Table_InitTanToAngle();
      R_DoomTLStyle();
      firsttime = false;
   }
   
   // process SMMU Doom->Doom2 texture conversion table
   R_LoadDoom1();
}


// ============================================================================
// SoM: Moved texture/flat lookup functions to r_textur.c
// ============================================================================


int r_precache = 1;     //sf: option not to precache the levels

//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
// Totally rewritten by Lee Killough to use less memory,
// to avoid using alloca(), and to improve performance.

void R_PrecacheLevel(void)
{
   register int i;
   register byte *hitlist;
   unsigned int unumtextures = (unsigned int)numtextures;

   if(demoplayback)
      return;
   
   if(!r_precache)
      return;

   {
      size_t size = numflats > numsprites  ? numflats : numsprites;
      hitlist = malloc(unumtextures > size ? unumtextures : size);
   }

   // Precache flats.
   
   memset(hitlist, 0, numflats);
   
   for(i = numsectors; --i >= 0; )
      hitlist[sectors[i].floorpic] = hitlist[sectors[i].ceilingpic] = 1;

   for(i = numflats; --i >= 0; )
   {
      if(hitlist[i])
         W_CacheLumpNum(firstflat + i, PU_CACHE);
   }

   // Precache textures.
   
   memset(hitlist, 0, numtextures);

   for(i = numsides; --i >= 0; )
   {
      hitlist[sides[i].bottomtexture] =
        hitlist[sides[i].toptexture] =
        hitlist[sides[i].midtexture] = 1;
   }

   // Sky texture is always present.
   // Note that F_SKY1 is the name used to
   //  indicate a sky floor/ceiling as a flat,
   //  while the sky texture is stored like
   //  a wall texture, with an episode dependend
   //  name.
   
   hitlist[skytexture] = 1;
   hitlist[sky2texture] = 1; // haleyjd

   for(i = numtextures; --i >= 0; )
   {
      if(hitlist[i])
      {
         texture_t *texture = textures[i];
         int j = texture->patchcount;
         while(--j >= 0)
            W_CacheLumpNum(texture->patches[j].patch, PU_CACHE);
      }
   }

   // Precache sprites.
   memset(hitlist, 0, numsprites);

   {
      thinker_t *th;
      for(th = thinkercap.next ; th != &thinkercap ; th=th->next)
         if(th->function == P_MobjThinker)
            hitlist[((mobj_t *)th)->sprite] = 1;
   }

   for(i = numsprites; --i >= 0; )
   {
      if (hitlist[i])
      {
         int j = sprites[i].numframes;
         
         while (--j >= 0)
         {
            int16_t *sflump = sprites[i].spriteframes[j].lump;
            int k = 7;
            do
               W_CacheLumpNum(firstspritelump + sflump[k], PU_CACHE);
            while(--k >= 0);
         }
      }
   }
   free(hitlist);
}

//
// R_FreeData
//
// Called when adding a new wad file. Frees all data loaded through R_Init.
// R_Init must then be immediately called again.
//
void R_FreeData(void)
{
   // haleyjd: let's harness the power of the zone heap and make this simple.
   Z_FreeTags(PU_RENDERER, PU_RENDERER);
}

/********************************
        Doom I texture conversion
 *********************************/

// convert old doom I levels so they will
// work under doom II

typedef struct doom1text_s
{
   char doom1[9];
   char doom2[9];
} doom1text_t;

doom1text_t *txtrconv;
int numconvs = 0;
int numconvsalloc = 0;

// haleyjd 01/27/09: state table for proper FSA parsing
enum
{
   D1_STATE_COMMENT,
   D1_STATE_SCAN,
   D1_STATE_TEX1,
   D1_STATE_TEX2,
};

//
// R_LoadDoom1
//
// Parses the DOOM -> DOOM II texture conversion table.
// haleyjd: rewritten 01/27/09 to remove heap corruption.
//
static void R_LoadDoom1(void)
{
   char *lump;
   char *rover;
   int lumplen, lumpnum;
   int state = D1_STATE_SCAN;
   int tx1, tx2;

   char texture1[9];
   char texture2[9];
   
   if((lumpnum = W_CheckNumForName("TXTRCONV")) == -1)
      return;

   memset(texture1, 0, sizeof(texture1));
   memset(texture2, 0, sizeof(texture2));
   tx1 = tx2 = 0;

   lumplen = W_LumpLength(lumpnum);
   lump    = calloc(1, lumplen + 1);
   
   W_ReadLump(lumpnum, lump);
   
   rover = lump;
   numconvs = 0;
   
   while(*rover)
   {
      switch(state)
      {
      case D1_STATE_SCAN:
         if(*rover == ';')
            state = D1_STATE_COMMENT;
         else if(isalnum(*rover))
         {
            texture1[tx1++] = *rover;
            state = D1_STATE_TEX1;
         }
         break;

      case D1_STATE_COMMENT:
         if(*rover == 0x0D || *rover == 0x0A)
            state = D1_STATE_SCAN;
         break;

      case D1_STATE_TEX1:
         if(*rover == 0x0D || *rover == 0x0A) // no linebreak during this state
            I_Error("R_LoadDoom1: malformed TXTRCONV lump: bad linebreak\n");

         if(isspace(*rover))
            state = D1_STATE_TEX2;
         else
         {
            if(tx1 >= 8)
               I_Error("R_LoadDoom1: malformed TXTRCONV lump: tx1 >= 8 chars\n");
            texture1[tx1++] = *rover;
         }
         break;

      case D1_STATE_TEX2:
          // finished with this entry?
         if(*rover == 0x0D || *rover == 0x0A || *(rover+1) == '\0')
         {
            // haleyjd 09/07/05: this was wasting tons of memory before with
            // lots of little mallocs; make the whole thing dynamic
            if(numconvs >= numconvsalloc)
            {
               txtrconv = realloc(txtrconv, 
                                  (numconvsalloc = numconvsalloc ?
                                   numconvsalloc*2 : 56) * sizeof *txtrconv);
            }
            
            strncpy(txtrconv[numconvs].doom1, texture1, 9);
            strncpy(txtrconv[numconvs].doom2, texture2, 9);
            
            numconvs++;

            // clear out temps
            memset(texture1, 0, sizeof(texture1));
            memset(texture2, 0, sizeof(texture2));
            tx1 = tx2 = 0;

            state = D1_STATE_SCAN;
         }
         else if(!isspace(*rover)) // skip spaces
         {
            if(tx2 >= 8)
               I_Error("R_LoadDoom1: malformed TXTRCONV lump: tx2 >= 8 chars\n");
            texture2[tx2++] = *rover;
         }
         break;
      }

      rover++;
   }
   
   // finished with lump
   free(lump);
}

static int R_Doom1Texture(const char *name)
{
   int i;
   
   // slow i know; should be hash tabled
   // mind you who cares? it's only going to be
   // used by a few people and only at the start of 
   // the level
   
   for(i = 0; i < numconvs; i++)
   {
      if(!strncasecmp(name, txtrconv[i].doom1, 8))   // found it
      {
         doom1level = true;
         return R_CheckTextureNumForName(txtrconv[i].doom2);
      }
   }
   
   return -1;
}


//-----------------------------------------------------------------------------
//
// $Log: r_data.c,v $
// Revision 1.23  1998/05/23  08:05:57  killough
// Reformatting
//
// Revision 1.21  1998/05/07  00:52:03  killough
// beautification
//
// Revision 1.20  1998/05/03  22:55:15  killough
// fix #includes at top
//
// Revision 1.19  1998/05/01  18:23:06  killough
// Make error messages look neater
//
// Revision 1.18  1998/04/28  22:56:07  killough
// Improve error handling of bad textures
//
// Revision 1.17  1998/04/27  01:58:08  killough
// Program beautification
//
// Revision 1.16  1998/04/17  10:38:58  killough
// Tag lumps with namespace tags to resolve collisions
//
// Revision 1.15  1998/04/16  10:47:40  killough
// Improve missing flats error message
//
// Revision 1.14  1998/04/14  08:12:31  killough
// Fix seg fault
//
// Revision 1.13  1998/04/12  09:52:51  killough
// Fix ?bad merge? causing seg fault
//
// Revision 1.12  1998/04/12  02:03:51  killough
// rename tranmap main_tranmap, better colormap support
//
// Revision 1.11  1998/04/09  13:19:35  killough
// Fix Medusa for transparent middles, and remove 64K composite texture size limit
//
// Revision 1.10  1998/04/06  04:39:58  killough
// Support multiple colormaps and C_START/C_END
//
// Revision 1.9  1998/03/23  03:33:29  killough
// Add support for an arbitrary number of colormaps, e.g. WATERMAP
//
// Revision 1.8  1998/03/09  07:26:03  killough
// Add translucency map caching
//
// Revision 1.7  1998/03/02  11:54:26  killough
// Don't initialize tranmap until needed
//
// Revision 1.6  1998/02/23  04:54:03  killough
// Add automatic translucency filter generator
//
// Revision 1.5  1998/02/02  13:35:36  killough
// Improve hashing algorithm
//
// Revision 1.4  1998/01/26  19:24:38  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/26  06:11:42  killough
// Fix Medusa bug, tune hash function
//
// Revision 1.2  1998/01/22  05:55:56  killough
// Improve hashing algorithm
//
// Revision 1.3  1997/01/29 20:10
// ???
//
//-----------------------------------------------------------------------------

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
//  Gamma correction LUT stuff.
//  Color range translation support
//  Functions to draw patches (by post) directly to screen.
//  Functions to blit a block to the screen.
//
//-----------------------------------------------------------------------------

#include "c_io.h"
#include "doomdef.h"
#include "doomstat.h"
#include "r_main.h"
#include "m_bbox.h"
#include "r_draw.h"
#include "w_wad.h"   /* needed for color translation lump lookup */
#include "v_video.h"
#include "v_patch.h" // haleyjd
#include "i_video.h"

// Each screen is [SCREENWIDTH*SCREENHEIGHT];
// SoM: Moved. See cb_video_t
// byte *screens[5];
int  dirtybox[4];

//jff 2/18/98 palette color ranges for translation
//jff 4/24/98 now pointers set to predefined lumps to allow overloading

char *cr_brick;
char *cr_tan;
char *cr_gray;
char *cr_green;
char *cr_brown;
char *cr_gold;
char *cr_red;
char *cr_blue;
char *cr_blue_status;
char *cr_orange;
char *cr_yellow;

//jff 4/24/98 initialize this at runtime
char *colrngs[10];

// Now where did these came from?
byte gammatable[5][256] =
{
  {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
   17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
   33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
   49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
   65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
   81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,
   97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
   113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
   128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
   144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
   160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
   176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
   192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
   208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
   224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
   240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255},

  {2,4,5,7,8,10,11,12,14,15,16,18,19,20,21,23,24,25,26,27,29,30,31,
   32,33,34,36,37,38,39,40,41,42,44,45,46,47,48,49,50,51,52,54,55,
   56,57,58,59,60,61,62,63,64,65,66,67,69,70,71,72,73,74,75,76,77,
   78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,
   99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,
   115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,129,
   130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,
   146,147,148,148,149,150,151,152,153,154,155,156,157,158,159,160,
   161,162,163,163,164,165,166,167,168,169,170,171,172,173,174,175,
   175,176,177,178,179,180,181,182,183,184,185,186,186,187,188,189,
   190,191,192,193,194,195,196,196,197,198,199,200,201,202,203,204,
   205,205,206,207,208,209,210,211,212,213,214,214,215,216,217,218,
   219,220,221,222,222,223,224,225,226,227,228,229,230,230,231,232,
   233,234,235,236,237,237,238,239,240,241,242,243,244,245,245,246,
   247,248,249,250,251,252,252,253,254,255},

  {4,7,9,11,13,15,17,19,21,22,24,26,27,29,30,32,33,35,36,38,39,40,42,
   43,45,46,47,48,50,51,52,54,55,56,57,59,60,61,62,63,65,66,67,68,69,
   70,72,73,74,75,76,77,78,79,80,82,83,84,85,86,87,88,89,90,91,92,93,
   94,95,96,97,98,100,101,102,103,104,105,106,107,108,109,110,111,112,
   113,114,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
   129,130,131,132,133,133,134,135,136,137,138,139,140,141,142,143,144,
   144,145,146,147,148,149,150,151,152,153,153,154,155,156,157,158,159,
   160,160,161,162,163,164,165,166,166,167,168,169,170,171,172,172,173,
   174,175,176,177,178,178,179,180,181,182,183,183,184,185,186,187,188,
   188,189,190,191,192,193,193,194,195,196,197,197,198,199,200,201,201,
   202,203,204,205,206,206,207,208,209,210,210,211,212,213,213,214,215,
   216,217,217,218,219,220,221,221,222,223,224,224,225,226,227,228,228,
   229,230,231,231,232,233,234,235,235,236,237,238,238,239,240,241,241,
   242,243,244,244,245,246,247,247,248,249,250,251,251,252,253,254,254,
   255},

  {8,12,16,19,22,24,27,29,31,34,36,38,40,41,43,45,47,49,50,52,53,55,
   57,58,60,61,63,64,65,67,68,70,71,72,74,75,76,77,79,80,81,82,84,85,
   86,87,88,90,91,92,93,94,95,96,98,99,100,101,102,103,104,105,106,107,
   108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,
   125,126,127,128,129,130,131,132,133,134,135,135,136,137,138,139,140,
   141,142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,
   155,156,157,158,159,160,160,161,162,163,164,165,165,166,167,168,169,
   169,170,171,172,173,173,174,175,176,176,177,178,179,180,180,181,182,
   183,183,184,185,186,186,187,188,189,189,190,191,192,192,193,194,195,
   195,196,197,197,198,199,200,200,201,202,202,203,204,205,205,206,207,
   207,208,209,210,210,211,212,212,213,214,214,215,216,216,217,218,219,
   219,220,221,221,222,223,223,224,225,225,226,227,227,228,229,229,230,
   231,231,232,233,233,234,235,235,236,237,237,238,238,239,240,240,241,
   242,242,243,244,244,245,246,246,247,247,248,249,249,250,251,251,252,
   253,253,254,254,255},

  {16,23,28,32,36,39,42,45,48,50,53,55,57,60,62,64,66,68,69,71,73,75,76,
   78,80,81,83,84,86,87,89,90,92,93,94,96,97,98,100,101,102,103,105,106,
   107,108,109,110,112,113,114,115,116,117,118,119,120,121,122,123,124,
   125,126,128,128,129,130,131,132,133,134,135,136,137,138,139,140,141,
   142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,155,
   156,157,158,159,159,160,161,162,163,163,164,165,166,166,167,168,169,
   169,170,171,172,172,173,174,175,175,176,177,177,178,179,180,180,181,
   182,182,183,184,184,185,186,187,187,188,189,189,190,191,191,192,193,
   193,194,195,195,196,196,197,198,198,199,200,200,201,202,202,203,203,
   204,205,205,206,207,207,208,208,209,210,210,211,211,212,213,213,214,
   214,215,216,216,217,217,218,219,219,220,220,221,221,222,223,223,224,
   224,225,225,226,227,227,228,228,229,229,230,230,231,232,232,233,233,
   234,234,235,235,236,236,237,237,238,239,239,240,240,241,241,242,242,
   243,243,244,244,245,245,246,246,247,247,248,248,249,249,250,250,251,
   251,252,252,253,254,254,255,255}
};

int usegamma;

//
// V_InitColorTranslation
//
// Loads the color translation tables from predefined lumps at game start
// No return value
//
// Used for translating text colors from the red palette range
// to other colors. The first nine entries can be used to dynamically
// switch the output of text color thru the HUlib_drawText routine
// by embedding ESCn in the text to obtain color n. Symbols for n are
// provided in v_video.h.
//

typedef struct {
  const char *name;
  char **map1, **map2;
} crdef_t;

// killough 5/2/98: table-driven approach
static const crdef_t crdefs[] = {
  {"CRBRICK",  &cr_brick,   &colrngs[CR_BRICK ]},
  {"CRTAN",    &cr_tan,     &colrngs[CR_TAN   ]},
  {"CRGRAY",   &cr_gray,    &colrngs[CR_GRAY  ]},
  {"CRGREEN",  &cr_green,   &colrngs[CR_GREEN ]},
  {"CRBROWN",  &cr_brown,   &colrngs[CR_BROWN ]},
  {"CRGOLD",   &cr_gold,    &colrngs[CR_GOLD  ]},
  {"CRRED",    &cr_red,     &colrngs[CR_RED   ]},
  {"CRBLUE",   &cr_blue,    &colrngs[CR_BLUE  ]},
  {"CRORANGE", &cr_orange,  &colrngs[CR_ORANGE]},
  {"CRYELLOW", &cr_yellow,  &colrngs[CR_YELLOW]},
  {"CRBLUE2",  &cr_blue_status, &cr_blue_status},
  {NULL}
};

// killough 5/2/98: tiny engine driven by table above
void V_InitColorTranslation(void)
{
  register const crdef_t *p;
  for (p=crdefs; p->name; p++)
    *p->map1 = *p->map2 = W_CacheLumpName(p->name, PU_STATIC);
}


//
// V_CopyRect
//
// Copies a source rectangle in a screen buffer to a destination
// rectangle in another screen buffer. Source origin in srcx,srcy,
// destination origin in destx,desty, common size in width and height.
// Source buffer specfified by srcscrn, destination buffer by destscrn.
//
// Marks the destination rectangle on the screen dirty.
//
// No return value.

void V_CopyRect(int srcx, int srcy, VBuffer *src, int width,
		int height, int destx, int desty, VBuffer *dest)
{
  byte *srcp;
  byte *destp;
  int  p1, p2;

   // This stuff is no longer current anyway
#ifdef RANGECHECK
   if(dest->scaled)
   {
      if (srcx<0
           ||srcx+width > dest->scalew
           || srcy<0
           || srcy+height > dest->scaleh
           ||destx<0||destx+width > dest->scalew
           || desty<0
           || desty+height > dest->scaleh
           || !src || !dest)
         I_Error ("Bad V_CopyRect");
   }
   else
   {
      if (srcx<0
           ||srcx+width > src->width
           || srcy<0
           || srcy+height > src->height
           ||destx<0||destx+width > dest->width
           || desty<0
           || desty+height > dest->height
           || !src || !dest)
         I_Error ("Bad V_CopyRect");
   }
#endif

   p1 = src->pitch;
   p2 = dest->pitch;

   if(dest->scaled)
   {
      int realx, realy;

      realx = dest->x1lookup[srcx];
      realy = dest->y1lookup[srcy];

      srcp = src->ylut[realy] + src->xlut[realx];

      realx = video.x1lookup[destx];
      realy = video.y1lookup[desty];
      
      destp = dest->ylut[realy] + dest->xlut[realx];

      // I HOPE this will not extend the array bounds HEHE
      width = video.x2lookup[width + destx - 1] - realx + 1;
      height = video.y2lookup[height + desty - 1] - realy + 1;
   }
   else
   {
      srcp = src->ylut[srcy] + src->xlut[srcx];
      destp = dest->ylut[desty] + src->xlut[destx];
   }

   if(p1 == p2 && width == src->width && width == dest->width)
   {
      memcpy(destp, srcp, p1 * height);
   }
   else
   {
      while(height--)
      {
         memcpy(destp, srcp, width);
         srcp += p1;
         destp += p2;
      }
   }
}

//
// V_DrawPatch
//
// Masks a column based masked pic to the screen.
//
// The patch is drawn at x,y in the buffer selected by scrn
// No return value
//
// V_DrawPatchFlipped
//
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//
// Patch is drawn at x,y in screenbuffer scrn.
// No return value
//
// killough 11/98: Consolidated V_DrawPatch and V_DrawPatchFlipped into one
// haleyjd  04/04: rewritten to use new ANYRES patch system
//
void V_DrawPatchGeneral(int x, int y, VBuffer *buffer, patch_t *patch,
			boolean flipped)
{
   PatchInfo pi;

   pi.x = x;
   pi.y = y;
   pi.patch = patch;
   pi.flipped = flipped;
   pi.drawstyle = PSTYLE_NORMAL;

   V_DrawPatchInt(&pi, buffer);
}



//
// V_DrawPatchTranslated
//
// Masks a column based masked pic to the screen.
// Also translates colors from one palette range to another using
// the color translation lumps loaded in V_InitColorTranslation
//
// The patch is drawn at x,y in the screen buffer scrn. Color translation
// is performed thru the table pointed to by outr. cm is not used.
//
// jff 1/15/98 new routine to translate patch colors
// haleyjd 04/03/04: rewritten for ANYRES patch system
//
void V_DrawPatchTranslated(int x, int y, VBuffer *buffer, patch_t *patch,
                           char *outr, boolean flipped)
{
   PatchInfo pi;
   
   pi.x = x;
   pi.y = y;
   pi.patch = patch;
   pi.flipped = flipped;

   // is the patch really translated?
   if(outr)
   {
      pi.drawstyle = PSTYLE_TLATED;   
      V_SetPatchColrng(outr);
   }
   else
      pi.drawstyle = PSTYLE_NORMAL;

   V_DrawPatchInt(&pi, buffer);
}

//
// V_DrawPatchTL
//
// Masks a column based masked pic to the screen with translucency.
// Also translates colors from one palette range to another using
// the color translation lumps loaded in V_InitColorTranslation.
//
// haleyjd 04/03/04: rewritten for ANYRES patch system
// 
void V_DrawPatchTL(int x, int y, VBuffer *buffer, patch_t *patch,
                  char *outr, int tl)
{
   PatchInfo pi;

   // is invisible?
   if(tl == 0)
      return;

   // if translucency is off or is opaque, fall back to translated
   if(!general_translucency || tl == FRACUNIT)
   {
      V_DrawPatchTranslated(x, y, buffer, patch, outr, false);
      return;
   }

   pi.x = x;
   pi.y = y;
   pi.patch = patch;
   pi.flipped = false; // TODO: these could be flipped too now

   // is the patch translated as well as translucent?
   if(outr)
   {
      pi.drawstyle = PSTYLE_TLTRANSLUC;
      V_SetPatchColrng(outr);
   }
   else
      pi.drawstyle = PSTYLE_TRANSLUC;

   // figure out the RGB tables to use for the tran level
   {
      unsigned int fglevel, bglevel;
      fglevel = tl & ~0x3ff;
      bglevel = FRACUNIT - fglevel;
      V_SetPatchTL(Col2RGB8[fglevel >> 10], Col2RGB8[bglevel >> 10]);
   }

   V_DrawPatchInt(&pi, buffer);
}

//
// V_DrawPatchAdd
//
// Masks a column based masked pic to the screen with additive
// translucency and optional color translation.
//
// haleyjd 02/08/05
// 
void V_DrawPatchAdd(int x, int y, VBuffer *buffer, patch_t *patch,
                    char *outr, int tl)
{
   PatchInfo pi;

   // if translucency is off, fall back to translated
   if(!general_translucency)
   {
      V_DrawPatchTranslated(x, y, buffer, patch, outr, false);
      return;
   }

   pi.x = x;
   pi.y = y;
   pi.patch = patch;
   pi.flipped = false; // TODO: these could be flipped too now

   // is the patch translated as well as translucent?
   if(outr)
   {
      pi.drawstyle = PSTYLE_TLADD;
      V_SetPatchColrng(outr);
   }
   else
      pi.drawstyle = PSTYLE_ADD;

   // figure out the RGB tables to use for the tran level
   {
      unsigned int fglevel, bglevel;
      fglevel = tl & ~0x3ff;    // normal foreground level
      bglevel = FRACUNIT;       // full background level
      V_SetPatchTL(Col2RGB8_LessPrecision[fglevel >> 10], 
                   Col2RGB8_LessPrecision[bglevel >> 10]);
   }

   V_DrawPatchInt(&pi, buffer);
}

//
// V_DrawPatchShadowed
//
// Convenience routine; draws the patch twice, first using colormap 33,
// and then normally.
//
void V_DrawPatchShadowed(int x, int y, VBuffer *buffer, patch_t *patch,
                         char *outr, int tl)
{
   char *cm = (char *)(colormaps[0] + 33*256);

   V_DrawPatchTL(x + 2, y + 2, buffer, patch, cm,   FRACUNIT*2/3);
   V_DrawPatchTL(x,     y,     buffer, patch, outr, tl);
}

//
// V_DrawBlock
//
// Draw a linear block of pixels into the view buffer. 
//
// The bytes at src are copied in linear order to the screen rectangle
// at x,y in screenbuffer scrn, with size width by height.
//
// No return value.
//
// haleyjd 04/08/03: rewritten for ANYRES system -- see v_block.c
// 
void V_DrawBlock(int x, int y, VBuffer *buffer, int width, int height, byte *src)
{
   buffer->BlockDrawer(x, y, buffer, width, height, src);
}

//
// V_DrawMaskedBlockTR
//
// Draw a translated, masked linear block of pixels into the view buffer. 
//
// haleyjd 06/29/08
// 
void V_DrawMaskedBlockTR(int x, int y, VBuffer *buffer, int width, int height,
                         int srcpitch, byte *src, byte *cmap)
{
   buffer->MaskedBlockDrawer(x, y, buffer, width, height, srcpitch, src, cmap);
}


#ifdef DJGPP
//
// V_GetBlock
//
// Gets a linear block of pixels from the view buffer.
//
// The pixels in the rectangle at x,y in screenbuffer scrn with size
// width by height are linearly packed into the buffer dest.
// No return value
//
void V_GetBlock(int x, int y, int scrn, int width, int height, byte *dest)
{
  byte *src;
  int p;

#ifdef RANGECHECK
  if (x<0
      ||x+width > SCREENWIDTH
      || y<0
      || y+height > SCREENHEIGHT
      || (unsigned)scrn>4 )
    I_Error ("Bad V_GetBlock");
#endif

   // SoM 1-30-04: ANYRES
   if(video.scaled)
   {
      width = video.x2lookup[x + width - 1] - video.x1lookup[x] + 1;
      x = video.x1lookup[x];

      height = video.y2lookup[y + height - 1] - video.y1lookup[y] + 1;
      y = video.y1lookup[y];
   }

   p = scrn == 0 ? video.pitch : video.width;

   src = video.screens[scrn] + y * p + x;
   while (height--)
   {
      memcpy (dest, src, width);
      src += p;
      dest += width;
   }
}
#endif

// 
// V_FindBestColor
//
// Adapted from zdoom -- thanks to Randy Heit.
//
// This always assumes a 256-color palette;
// it's intended for use in startup functions to match hard-coded
// color values to the best fit in the game's palette (allows
// cross-game usage among other things).
//
byte V_FindBestColor(const byte *palette, int r, int g, int b)
{
   int i, dr, dg, db;
   int bestcolor, bestdistortion, distortion;

   // use color 0 as a worst-case match for any color
   bestdistortion = 257*257*3;
   bestcolor = 0;

   for(i = 0; i < 256; i++)
   {
      dr = r - *palette++;
      dg = g - *palette++;
      db = b - *palette++;
      
      distortion = dr*dr + dg*dg + db*db;

      if(distortion < bestdistortion)
      {
         // exact match
         if(!distortion)
            return i;
         
         bestdistortion = distortion;
         bestcolor = i;
      }
   }

   return bestcolor;
}

// haleyjd: DOSDoom-style single translucency lookup-up table
// generation code. This code has a 32k (plus a bit more) 
// footprint but allows a much wider range of translucency effects
// than BOOM-style translucency. This will be used for particles, 
// for variable mapthing trans levels, and for screen patches.

// haleyjd: Updated 06/21/08 to use 32k lookup, mainly to fix
// additive translucency. Note this code is included in Odamex and
// so it can be considered GPL as used here, rather than BSD. But,
// I don't care either way. It is effectively dual-licensed I suppose.

boolean flexTranInit = false;
unsigned int  Col2RGB8[65][256];
unsigned int *Col2RGB8_LessPrecision[65];
byte RGB32k[32][32][32];

static unsigned int Col2RGB8_2[63][256];

#define MAKECOLOR(a) (((a)<<3)|((a)>>2))

typedef struct tpalcol_s
{
   unsigned int r, g, b;
} tpalcol_t;

void V_InitFlexTranTable(const byte *palette)
{
   int i, r, g, b, x, y;
   tpalcol_t  *tempRGBpal;
   const byte *palRover;

   // mark that we've initialized the flex tran table
   flexTranInit = true;
   
   tempRGBpal = Z_Malloc(256*sizeof(*tempRGBpal), PU_STATIC, NULL);
   
   for(i = 0, palRover = palette; i < 256; i++, palRover += 3)
   {
      tempRGBpal[i].r = palRover[0];
      tempRGBpal[i].g = palRover[1];
      tempRGBpal[i].b = palRover[2];
   }

   // build RGB table
   for(r = 0; r < 32; ++r)
   {
      for(g = 0; g < 32; ++g)
      {
         for(b = 0; b < 32; ++b)
         {
            RGB32k[r][g][b] = 
               V_FindBestColor(palette, 
                               MAKECOLOR(r), MAKECOLOR(g), MAKECOLOR(b));
         }
      }
   }
   
   // build lookup table
   for(x = 0; x < 65; ++x)
   {
      for(y = 0; y < 256; ++y)
      {
         Col2RGB8[x][y] = (((tempRGBpal[y].r*x)>>4)<<20)  |
                            ((tempRGBpal[y].g*x)>>4) |
                          (((tempRGBpal[y].b*x)>>4)<<10);
      }
   }

   // build a secondary lookup with red and blue lsbs masked out for additive 
   // blending; otherwise, the overflow messes up the calculation and you get 
   // something very ugly.
   for(x = 1; x < 64; ++x)
   {
      Col2RGB8_LessPrecision[x] = Col2RGB8_2[x - 1];
      
      for(y = 0; y < 256; ++y)
         Col2RGB8_2[x-1][y] = Col2RGB8[x][y] & 0x3feffbff;
   }
   Col2RGB8_LessPrecision[0]  = Col2RGB8[0];
   Col2RGB8_LessPrecision[64] = Col2RGB8[64];

   Z_Free(tempRGBpal);
}

//
// V_CacheBlock
//
// haleyjd 12/22/02: 
// Copies a linear block to a memory buffer as if to a 
// low-res screen
//
void V_CacheBlock(int x, int y, int width, int height, byte *src,
                  byte *bdest)
{
   byte *dest = bdest + y*SCREENWIDTH + x;
   
   while(height--)
   {
      memcpy(dest, src, width);
      src += width;
      dest += SCREENWIDTH;
   }
}




//----------------------------------------------------------------------------
//
// $Log: v_video.c,v $
// Revision 1.10  1998/05/06  11:12:48  jim
// Formattted v_video.*
//
// Revision 1.9  1998/05/03  22:53:16  killough
// beautification, simplify translation lookup
//
// Revision 1.8  1998/04/24  08:09:39  jim
// Make text translate tables lumps
//
// Revision 1.7  1998/03/02  11:41:58  killough
// Add cr_blue_status for blue statusbar numbers
//
// Revision 1.6  1998/02/24  01:40:12  jim
// Tuned HUD font
//
// Revision 1.5  1998/02/23  04:58:17  killough
// Fix performance problems
//
// Revision 1.4  1998/02/19  16:55:00  jim
// Optimized HUD and made more configurable
//
// Revision 1.3  1998/02/17  23:00:36  jim
// Added color translation machinery and data
//
// Revision 1.2  1998/01/26  19:25:08  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:05  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------


// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2004 James Haley
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
//  Functions to manipulate linear blocks of graphics data.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "v_video.h"

//==============================================================================
//
// V_DrawBlock Implementors
//
// * V_BlockDrawer   -- unscaled
// * V_BlockDrawerS  -- general scaling
//

static void V_BlockDrawer(int x, int y, VBuffer *buffer, 
                          int width, int height, byte *source)
{
   byte *src, *dest;
   int cx1, cy1, cx2, cy2, cw, ch;
   int dx, dy;

   // clip to screen

   // entirely off-screen?
   if(x + width < 0 || y + height < 0 || x >= buffer->width || y >= buffer->height)
      return;

   cx1 = x >= 0 ? x : 0;
   cy1 = y >= 0 ? y : 0;
   cx2 = x + width - 1;
   cy2 = y + height - 1;

   if(cx2 >= buffer->width)
      cx2 = buffer->width - 1;

   if(cy2 >= buffer->height)
      cy2 = buffer->height - 1;

   // change in origin due to clipping
   dx = cx1 - x;
   dy = cy1 - y;

   // clipped rect width/height
   cw = cx2 - cx1 + 1;
   ch = cy2 - cy1 + 1;

   // zero-size rect?
   if(cw <= 0 || ch <= 0)
      return;

   src  = source + dy * width + dx;
   dest = buffer->ylut[cy1] + buffer->xlut[cx1];

   while(ch--)
   {
      memcpy(dest, src, cw);
      src += width;
      dest += buffer->pitch;
   }
}

static void V_BlockDrawerS(int x, int y, VBuffer *buffer, 
                           int width, int height, byte *source)
{
   byte *src, *dest, *row;
   fixed_t xstep, ystep, xfrac, yfrac;
   int xtex, ytex, w, h, i, realx, realy;
   int cx1, cy1, cx2, cy2, cw, ch;
   int dx, dy;
   
   // clip to screen within scaled coordinate space
   
   // entirely off-screen?
   if(x + width < 0 || y + height < 0 || x >= buffer->unscaledw || y >= buffer->unscaledh)
      return;
   
   cx1 = x >= 0 ? x : 0;
   cy1 = y >= 0 ? y : 0;
   cx2 = x + width - 1;
   cy2 = y + height - 1;

   if(cx2 >= buffer->unscaledw)
      cx2 = buffer->unscaledw - 1;

   if(cy2 >= buffer->unscaledh)
      cy2 = buffer->unscaledh - 1;
   
   // change in origin due to clipping
   dx = cx1 - x;
   dy = cy1 - y;
   
   // clipped rect width/height
   cw = cx2 - cx1 + 1;
   ch = cy2 - cy1 + 1;
   
   // zero-size rect?
   if(cw <= 0 || ch <= 0)
      return;
      
   realx = buffer->x1lookup[cx1];
   realy = buffer->y1lookup[cy1];
   w     = buffer->x2lookup[cx2] - realx + 1;
   h     = buffer->y2lookup[cy2] - realy + 1;
   xstep = buffer->ixscale;
   ystep = buffer->iyscale;
   yfrac = 0;

   src  = source + dy * width + dx;
   dest = buffer->ylut[realy] + buffer->xlut[realx];

#ifdef RANGECHECK
   // sanity check
   if(realx < 0 || realx + w > buffer->width ||
      realy < 0 || realy + h > buffer->height)
   {
      I_Error("V_BlockDrawerS: block exceeds buffer boundaries.\n");
   }
#endif

   while(h--)
   {
      row = dest;
      i = w;
      xfrac = 0;
      ytex = (yfrac >> FRACBITS) * width;
      
      while(i--)
      {
         xtex = (xfrac >> FRACBITS);
         *row++ = src[ytex + xtex];
         xfrac += xstep;
      }

      dest += buffer->pitch;
      yfrac += ystep;
   }
}

//==============================================================================
// 
// Masked block drawers
//
// haleyjd 06/29/08
//

static void V_MaskedBlockDrawer(int x, int y, VBuffer *buffer, 
                                int width, int height, int srcpitch,
                                byte *source, byte *cmap)
{
   byte *src, *dest;
   int cx1, cy1, cx2, cy2, cw, ch;
   int dx, dy, i;

   // clip to screen

   // entirely off-screen?
   if(x + width < 0 || y + height < 0 || x >= buffer->width || y >= buffer->height)
      return;

   cx1 = x >= 0 ? x : 0;
   cy1 = y >= 0 ? y : 0;
   cx2 = x + width - 1;
   cy2 = y + height - 1;

   if(cx2 >= buffer->width)
      cx2 = buffer->width - 1;

   if(cy2 >= buffer->height)
      cy2 = buffer->height - 1;

   // change in origin due to clipping
   dx = cx1 - x;
   dy = cy1 - y;

   // clipped rect width/height
   cw = cx2 - cx1 + 1;
   ch = cy2 - cy1 + 1;

   // zero-size rect?
   if(cw <= 0 || ch <= 0)
      return;

   src  = source + dy * srcpitch + dx;
   dest = buffer->ylut[cy1] + buffer->xlut[cx1];

   while(ch--)
   {
      for(i = 0; i < cw; ++i)
      {
         if(*(src + i))
            *(dest + i) = cmap[*(src + i)];
      }
      src += srcpitch;
      dest += buffer->pitch;
   }
}

static void V_MaskedBlockDrawerS(int x, int y, VBuffer *buffer, 
                                 int width, int height, int srcpitch,
                                 byte *source, byte *cmap)
{
   byte *src, *dest, *row;
   fixed_t xstep, ystep, xfrac, yfrac;
   int xtex, ytex, w, h, i, realx, realy;
   int cx1, cy1, cx2, cy2, cw, ch;
   int dx, dy;
   
   // clip to screen within scaled coordinate space
   
   // entirely off-screen?
   if(x + width < 0 || y + height < 0 || x >= buffer->unscaledw || y >= buffer->unscaledh)
      return;
   
   cx1 = x >= 0 ? x : 0;
   cy1 = y >= 0 ? y : 0;
   cx2 = x + width - 1;
   cy2 = y + height - 1;

   if(cx2 >= buffer->unscaledw)
      cx2 = buffer->unscaledw - 1;

   if(cy2 >= buffer->unscaledh)
      cy2 = buffer->unscaledh - 1;
   
   // change in origin due to clipping
   dx = cx1 - x;
   dy = cy1 - y;
   
   // clipped rect width/height
   cw = cx2 - cx1 + 1;
   ch = cy2 - cy1 + 1;
   
   // zero-size rect?
   if(cw <= 0 || ch <= 0)
      return;

   realx = buffer->x1lookup[cx1];
   realy = buffer->y1lookup[cy1];
   w     = buffer->x2lookup[cx2] - realx + 1;
   h     = buffer->y2lookup[cy2] - realy + 1;
   xstep = buffer->ixscale;
   ystep = buffer->iyscale;
   yfrac = 0;

   src  = source + dy * srcpitch + dx;
   dest = buffer->ylut[realy] + buffer->xlut[realx];

#ifdef RANGECHECK
   // sanity check
   if(realx < 0 || realx + w > buffer->width ||
      realy < 0 || realy + h > buffer->height)
   {
      I_Error("V_BlockDrawerS: block exceeds buffer boundaries.\n");
   }
#endif

   while(h--)
   {
      row = dest;
      i = w;
      xfrac = 0;
      ytex = (yfrac >> FRACBITS) * srcpitch;
      
      while(i--)
      {
         xtex = (xfrac >> FRACBITS);
         if(src[ytex + xtex])
            *row = cmap[src[ytex + xtex]];
         ++row;
         xfrac += xstep;
      }

      dest += buffer->pitch;
      yfrac += ystep;
   }
}

//==============================================================================
//
// Color block drawing
//

void V_ColorBlockScaled(VBuffer *dest, byte color, int x, int y, int w, int h)
{
   byte *d;
   int  i, size;
   int x2, y2;

   x2 = x + w - 1;
   y2 = y + h - 1;

   if(x < 0)
      x = 0;

   if(y < 0)
      y = 0;

   if(x2 >= dest->unscaledw)
      x2 = dest->unscaledw - 1;

   if(y2 >= dest->unscaledh)
      y2 = dest->unscaledh - 1;

   if(x > x2 || y > y2)
      return;

   x = dest->x1lookup[x];
   x2 = dest->x2lookup[x2];

   y = dest->y1lookup[y];
   y2 = dest->y2lookup[y2];

   w = x2 - x + 1;
   h = y2 - y + 1;

   d = dest->ylut[y] + dest->xlut[x];
   size = w;

   for(i = 0; i < h; i++)
   {
      memset(d, color, size);
      d += dest->pitch;
   }
}

void V_ColorBlockTLScaled(VBuffer *dest, byte color, int x, int y, int w, int h, 
                          int tl)
{
   byte *d, *row;
   int  i, tw;
   int x2, y2;
   unsigned int *fg2rgb, *bg2rgb, col;

   // haleyjd 05/06/09: optimizations for opaque and invisible
   if(tl == FRACUNIT)
   {
      V_ColorBlockScaled(dest, color, x, y, w, h);
      return;
   }
   else if(tl == 0)
      return;

   unsigned int fglevel = tl & ~0x3ff;
   unsigned int bglevel = FRACUNIT - fglevel;
   fg2rgb  = Col2RGB8[fglevel >> 10];
   bg2rgb  = Col2RGB8[bglevel >> 10];

   x2 = x + w - 1;
   y2 = y + h - 1;

   if(x < 0)
      x = 0;

   if(y < 0)
      y = 0;

   if(x2 >= dest->unscaledw)
      x2 = dest->unscaledw - 1;

   if(y2 >= dest->unscaledh)
      y2 = dest->unscaledh - 1;

   if(x > x2 || y > y2)
      return;

   x  = dest->x1lookup[x];
   x2 = dest->x2lookup[x2];

   y  = dest->y1lookup[y];
   y2 = dest->y2lookup[y2];

   w = x2 - x + 1;
   h = y2 - y + 1;

   d = dest->ylut[y] + dest->xlut[x];

   for(i = 0; i < h; i++)
   {
      row = d;
      tw = w;

      while(tw--)
      {
         col    = (fg2rgb[color] + bg2rgb[*row]) | 0x1f07c1f;
         *row++ = RGB32k[0][0][col & (col >> 15)];
      }

      d += dest->pitch;
   }
}

//
// V_ColorBlock
//
// Draws a block of solid color.
//
void V_ColorBlock(VBuffer *buffer, byte color, int x, int y, int w, int h)
{
   byte *dest;

#ifdef RANGECHECK
   if(x < 0 || x + w > buffer->width || y < 0 || y + h > buffer->height)
      I_Error("V_ColorBlock: block exceeds buffer boundaries.\n");
#endif

   dest = buffer->ylut[y] + buffer->xlut[x];
   
   while(h--)
   {
      memset(dest, color, w);
      dest += buffer->pitch;
   }
}

//
// V_ColorBlockTL
//
// Draws a block of solid color with alpha blending.
//
void V_ColorBlockTL(VBuffer *buffer, byte color, int x, int y, 
                    int w, int h, int tl)
{
   byte *dest, *row;
   register int tw;
   register unsigned int col;
   unsigned int *fg2rgb, *bg2rgb;

#ifdef RANGECHECK
   if(x < 0 || x + w > buffer->width || y < 0 || y + h > buffer->height)
      I_Error("V_ColorBlockTL: block exceeds buffer boundaries.\n");
#endif

   // haleyjd 10/15/05: optimizations for opaque and invisible
   if(tl == FRACUNIT)
   {
      V_ColorBlock(buffer, color, x, y, w, h);
      return;
   }
   else if(tl == 0)
      return;

   unsigned int fglevel = tl & ~0x3ff;
   unsigned int bglevel = FRACUNIT - fglevel;
   fg2rgb  = Col2RGB8[fglevel >> 10];
   bg2rgb  = Col2RGB8[bglevel >> 10];

   dest = buffer->ylut[y] + buffer->xlut[x];
   
   while(h--)
   { 
      row = dest;
      tw = w;

      while(tw--)
      {
         col    = (fg2rgb[color] + bg2rgb[*row]) | 0x1f07c1f;
         *row++ = RGB32k[0][0][col & (col >> 15)];
      }

      dest += buffer->pitch;
   }
}

//
// V_TileFlat implementors
//

//
// Unscaled
//
// Works for any video mode.
//
static void V_TileBlock64(VBuffer *buffer, byte *src)
{
   int x, y;
   byte *row, *dest = buffer->data;
   int wmod;

   // if width % 64 != 0, we must do some extra copying at the end
   if((wmod = buffer->width & 63))
   {
      for(y = 0; y < buffer->height; y++)
      {
         row = dest;
         for(x = 0; x < buffer->width >> 6; x++)
         {
            memcpy(row, src + ((y & 63) << 6), 64);
            row += 64;
         }
         memcpy(row, src + ((y & 63) << 6), wmod);
         dest += buffer->pitch;
      }
   }
   else
   {
      for(y = 0; y < buffer->height; y++)
      {
         row = dest;         
         for(x = 0; x < buffer->width >> 6; ++x)
         {
            memcpy(row, src + ((y & 63) << 6), 64);
            row += 64;
         }
         dest += buffer->pitch;
      }
   }
}

//
// General scaling
//
static void V_TileBlock64S(VBuffer *buffer, byte *src)
{
   byte *dest, *row;
   fixed_t xstep, ystep, xfrac, yfrac = 0;
   int xtex, ytex, w, h;
   
   w = buffer->width;
   h = buffer->height;
   xstep = buffer->ixscale;
   ystep = buffer->iyscale;
   
   dest = buffer->data;

   while(h--)
   {
      int i = w;
      row = dest;
      xfrac = 0;
      ytex = ((yfrac >> FRACBITS) & 63) << 6;
      
      while(i--)
      {
         xtex = (xfrac >> FRACBITS) & 63;
         *row++ = src[ytex + xtex];
         xfrac += xstep;
      }
      
      yfrac += ystep;
      dest += buffer->pitch;
   }
}

//
// V_SetBlockFuncs
//
// Sets the block drawing function pointers in a VBuffer object
// based on the size of the buffer. Called from V_SetupBufferFuncs.
//
void V_SetBlockFuncs(VBuffer *buffer, int drawtype)
{
   switch(drawtype)
   {
   case DRAWTYPE_UNSCALED:
      buffer->BlockDrawer       = V_BlockDrawer;
      buffer->MaskedBlockDrawer = V_MaskedBlockDrawer;
      buffer->TileBlock64       = V_TileBlock64;
      break;
   case DRAWTYPE_2XSCALED:
   case DRAWTYPE_GENSCALED:
      buffer->BlockDrawer       = V_BlockDrawerS;
      buffer->MaskedBlockDrawer = V_MaskedBlockDrawerS;
      buffer->TileBlock64       = V_TileBlock64S;
      break;
   default:
      break;
   }
}

// EOF


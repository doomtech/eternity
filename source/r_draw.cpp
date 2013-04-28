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
//      The actual column drawing functions.
//      Here find the main potential for optimization,
//       e.g. inline assembly, different algorithms.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "doomstat.h"
#include "d_gi.h"
#include "e_lib.h"
#include "mn_engin.h"
#include "r_draw.h"
#include "r_main.h"
#include "st_stuff.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

// need wad iterator
#include "w_iterator.h"

#define MAXWIDTH  MAX_SCREENWIDTH          /* kilough 2/8/98 */
#define MAXHEIGHT MAX_SCREENHEIGHT

#ifdef DJGPP
#define USEASM /* sf: changed #ifdef DJGPP to #ifdef USEASM */
#endif

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//  and we need only the base address,
//  and the total size == width*height*depth/8.,
//

byte *viewimage; 
int  viewwidth;
int  scaledviewwidth;
int  scaledviewheight;        // killough 11/98
int  viewheight;
int  viewwindowx;
int  viewwindowy; 
// SoM: ANYRES
int  scaledwindowx;
int  scaledwindowy;

byte *ylookup[MAXHEIGHT]; 
int  columnofs[MAXWIDTH]; 
int  linesize = SCREENWIDTH;  // killough 11/98

// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//
 
byte *tranmap;          // translucency filter maps 256x256   // phares 
byte *main_tranmap;     // killough 4/11/98

//
// R_DrawColumn
// Source is the top of the column to scale.
//

// Fuzz stuffs

const int fuzzoffset[FUZZTABLE] = 
{
  1,0,1,0,1,1,0,
  1,1,0,1,1,1,0,
  1,1,1,0,0,0,0,
  1,0,0,1,1,1,1,0,
  1,0,1,1,0,0,1,
  1,0,0,0,0,1,1,
  1,1,0,1,1,0,1 
}; 

int fuzzpos = 0; 

//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
// 

void CB_DrawColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawColumn_8: %i to %i at %i\n", column.y1, column.y2, column.x);
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;

         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;

         do
         {
            *dest = colormap[source[frac>>FRACBITS]];
            dest += linesize;                     // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
   }
}

// Here is the version of R_DrawColumn that deals with translucent  // phares
// textures and sprites. It's identical to R_DrawColumn except      //    |
// for the spot where the color index is stuffed into *dest. At     //    V
// that point, the existing color index and the new color index
// are mapped through the TRANMAP lump filters to get a new color
// index whose RGB values are the average of the existing and new
// colors.
//
// Since we're concerned about performance, the 'translucent or
// opaque' decision is made outside this routine, not down where the
// actual code differences are.

#define SRCPIXEL \
   tranmap[(*dest<<8)+colormap[source[(frac>>FRACBITS) & heightmask]]]

void CB_DrawTLColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawTLColumn_8: %i to %i at %i\n", column.y1, column.y2, column.x);
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   // killough 2/1/98, 2/21/98: more performance tuning
   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;

         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;

         do
         {
            *dest = tranmap[(*dest<<8) + colormap[source[frac>>FRACBITS]]]; // phares
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = SRCPIXEL; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
            *dest = SRCPIXEL; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = SRCPIXEL; // phares
      }
   }
}

#undef SRCPIXEL

#define SRCPIXEL \
   tranmap[(*dest<<8) + colormap[column.translation[source[frac>>FRACBITS]]]]

#define SRCPIXEL_MASK \
   tranmap[(*dest<<8) + \
      colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]]

//
// haleyjd 02/08/05: BOOM TL/Tlated was neglected.
//
void CB_DrawTLTRColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawTLTRColumn_8: %i to %i at %i\n", column.y1, column.y2, column.x);    
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;

         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;

         do
         {
            *dest = SRCPIXEL; // phares
            dest += linesize; // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = SRCPIXEL_MASK; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
            *dest = SRCPIXEL_MASK; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = SRCPIXEL_MASK; // phares
      }
   }
}

#undef SRCPIXEL
#undef SRCPIXEL_MASK


//
// Spectre/Invisibility.
//


// SoM: Fuzz Stuff moved to beginning of the file
//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//

// sf: restored original fuzz effect (changed in mbf)
// sf: changed to use vis->colormap not fullcolormap
//     for coloured lighting and SHADOW now done with
//     flags not NULL colormap

#define SRCPIXEL \
   colormap[6*256+dest[fuzzoffset[fuzzpos] ? linesize : -linesize]]

void CB_DrawFuzzColumn_8(void)
{
   int count;
   register byte *dest;

   // Adjust borders. Low...
   if(!column.y1) 
      column.y1 = 1;
   
   // .. and high.
   if(column.y2 == viewheight - 1) 
      column.y2 = viewheight - 2; 

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawFuzzColumn_8: %i to %i at %i\n", column.y1, column.y2, column.x);    
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];

   {
      register lighttable_t *colormap = column.colormap;
      
      while((count -= 2) >= 0) // texture height is a power of 2 -- killough
      {
         *dest = SRCPIXEL;
         if(++fuzzpos == FUZZTABLE) fuzzpos = 0;
         dest += linesize;   // killough 11/98

         *dest = SRCPIXEL;
         if(++fuzzpos == FUZZTABLE) fuzzpos = 0;
         dest += linesize;   // killough 11/98
      }
      if(count & 1)
      {
         *dest = SRCPIXEL;
         if(++fuzzpos == FUZZTABLE) fuzzpos = 0;
      }
   }
}

#undef SRCPIXEL

//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//

// haleyjd: changed translationtables to byte **
byte **translationtables = NULL;

// haleyjd: new stuff
int firsttranslationlump;
int numtranslations = 0;

#define SRCPIXEL \
   colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]

void CB_DrawTRColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawTRColumn_8: %i to %i at %i\n", column.y1, column.y2, column.x);    
#endif 

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;

         if(frac < 0)
            while((frac += heightmask) <  0);
         else
            while(frac >= heightmask)
               frac -= heightmask;

         do
         {
            *dest = colormap[column.translation[source[frac>>FRACBITS]]]; // phares
            dest += linesize;                     // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            *dest = SRCPIXEL; // phares
            dest += linesize;   // killough 11/98
            frac += fracstep;
            *dest = SRCPIXEL;
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
            *dest = SRCPIXEL;
      }
   }
}

#undef SRCPIXEL

//
// R_DrawFlexColumn
//
// haleyjd 09/01/02: zdoom-style translucency
//
void CB_DrawFlexColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   register unsigned int fg, bg;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawFlexColumn_8: %i to %i at %i\n", column.y1, column.y2, column.x);    
#endif 

   {
      unsigned int fglevel, bglevel;
      
      fglevel = column.translevel & ~0x3ff;
      bglevel = FRACUNIT - fglevel;
      fg2rgb  = Col2RGB8[fglevel >> 10];
      bg2rgb  = Col2RGB8[bglevel >> 10];
   }

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if (column.texheight & heightmask)   // not a power of 2 -- killough
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            fg = colormap[source[frac>>FRACBITS]];
            bg = *dest;

            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0x1f07c1f;
            *dest = RGB32k[0][0][fg & (fg>>15)];
            
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0)   // texture height is a power of 2 -- killough
         {
            fg = colormap[source[(frac>>FRACBITS) & heightmask]];
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0x1f07c1f;

            *dest = RGB32k[0][0][fg & (fg>>15)];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            
            fg = colormap[source[(frac>>FRACBITS) & heightmask]];
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0x1f07c1f;

            *dest = RGB32k[0][0][fg & (fg>>15)];
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
         {
            fg = colormap[source[(frac>>FRACBITS) & heightmask]];
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0x1f07c1f;

            *dest = RGB32k[0][0][fg & (fg>>15)];
         }
      }
   }
}

#define SRCPIXEL \
   colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]

//
// R_DrawFlexTlatedColumn
//
// haleyjd 11/05/02: zdoom-style translucency w/translation, for
// player sprites
//
void CB_DrawFlexTRColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   register unsigned int fg, bg;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawFlexTRColumn_8: %i to %i at %i\n", column.y1, column.y2, column.x);    
#endif 

   {
      unsigned int fglevel, bglevel;
      
      fglevel = column.translevel & ~0x3ff;
      bglevel = FRACUNIT - fglevel;
      fg2rgb  = Col2RGB8[fglevel >> 10];
      bg2rgb  = Col2RGB8[bglevel >> 10];
   }

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            fg = colormap[column.translation[source[frac>>FRACBITS]]];
            bg = *dest;

            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0x1f07c1f;
            *dest = RGB32k[0][0][fg & (fg>>15)];
            
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            fg = SRCPIXEL;
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0x1f07c1f;

            *dest = RGB32k[0][0][fg & (fg>>15)];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            
            fg = SRCPIXEL;
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0x1f07c1f;

            *dest = RGB32k[0][0][fg & (fg>>15)];
            dest += linesize;   // killough 11/98
            frac += fracstep;
         }
         if(count & 1)
         {
            fg = SRCPIXEL;
            bg = *dest;
            fg = fg2rgb[fg];
            bg = bg2rgb[bg];
            fg = (fg+bg) | 0x1f07c1f;

            *dest = RGB32k[0][0][fg & (fg>>15)];
         }
      }
   }
}

#undef SRCPIXEL

#define SRCPIXEL \
   fg2rgb[colormap[source[(frac>>FRACBITS) & heightmask]]]

//
// R_DrawAddColumn
//
// haleyjd 02/08/05: additive translucency
//
void CB_DrawAddColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   unsigned int a, b;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawAddColumn_8: %i to %i at %i\n", column.y1, column.y2, column.x);    
#endif 

   {
      unsigned int fglevel, bglevel;
      
      fglevel = column.translevel & ~0x3ff;
      bglevel = FRACUNIT;
      fg2rgb  = Col2RGB8_LessPrecision[fglevel >> 10];
      bg2rgb  = Col2RGB8_LessPrecision[bglevel >> 10];
   }

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            // mask out LSBs in green and red to allow overflow
            a = fg2rgb[colormap[source[frac>>FRACBITS]]] + bg2rgb[*dest];
            b = a;
            
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
                        
            *dest = RGB32k[0][0][a & (a >> 15)];
            
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0)   // texture height is a power of 2 -- killough
         {
            a = SRCPIXEL + bg2rgb[*dest];
            b = a;

            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;            
            
            *dest = RGB32k[0][0][a & (a >> 15)];
            dest += linesize;   // killough 11/98
            frac += fracstep;

            a = SRCPIXEL + bg2rgb[*dest];
            b = a;
            
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
            
            *dest = RGB32k[0][0][a & (a >> 15)];
            dest += linesize;   // killough 11/98
            frac += fracstep;            
         }
         if(count & 1)
         {
            a = SRCPIXEL + bg2rgb[*dest];
            b = a;
            
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
            
            *dest = RGB32k[0][0][a & (a >> 15)];
         }
      }
   }
}

#undef SRCPIXEL

#define SRCPIXEL \
   fg2rgb[colormap[column.translation[source[frac>>FRACBITS]]]]

#define SRCPIXEL_MASK \
   fg2rgb[colormap[column.translation[source[(frac>>FRACBITS) & heightmask]]]]

//
// R_DrawAddTlatedColumn
//
// haleyjd 02/08/05: additive translucency + translation
// The slowest of all column drawers!
//
void CB_DrawAddTRColumn_8(void)
{
   int count;
   register byte *dest;
   register fixed_t frac;
   fixed_t fracstep;
   unsigned int *fg2rgb, *bg2rgb;
   unsigned int a, b;

   count = column.y2 - column.y1 + 1;
   if(count <= 0) return;

#ifdef RANGECHECK 
   if(column.x  < 0 || column.x  >= video.width || 
      column.y1 < 0 || column.y2 >= video.height)
      I_Error("CB_DrawAddTRColumn_8: %i to %i at %i\n", column.y1, column.y2, column.x);    
#endif 

   {
      unsigned int fglevel, bglevel;
      
      fglevel = column.translevel & ~0x3ff;
      bglevel = FRACUNIT;
      fg2rgb  = Col2RGB8_LessPrecision[fglevel >> 10];
      bg2rgb  = Col2RGB8_LessPrecision[bglevel >> 10];
   }

   dest = ylookup[column.y1] + columnofs[column.x];
   fracstep = column.step;
   frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

   {
      register byte *source = (byte *)(column.source);
      register lighttable_t *colormap = column.colormap;
      register int heightmask = column.texheight - 1;
      
      if(column.texheight & heightmask)
      {
         heightmask++;
         heightmask <<= FRACBITS;
          
         if (frac < 0)
            while ((frac += heightmask) <  0);
         else
            while (frac >= (int)heightmask)
               frac -= heightmask;          

         do
         {
            // mask out LSBs in green and red to allow overflow
            a = SRCPIXEL + bg2rgb[*dest];
            b = a;
            
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
            
            *dest = RGB32k[0][0][a & (a >> 15)];
            
            dest += linesize;          // killough 11/98
            if((frac += fracstep) >= heightmask)
               frac -= heightmask;
         } 
         while(--count);
      }
      else
      {
         while((count -= 2) >= 0) // texture height is a power of 2 -- killough
         {
            a = SRCPIXEL_MASK + bg2rgb[*dest];
            b = a;
            
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
            
            *dest = RGB32k[0][0][a & (a >> 15)];
            dest += linesize;   // killough 11/98
            frac += fracstep;

            a = SRCPIXEL_MASK + bg2rgb[*dest];
            b = a;
            
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
            
            *dest = RGB32k[0][0][a & (a >> 15)];
            dest += linesize;   // killough 11/98
            frac += fracstep;            
         }
         if(count & 1)
         {
            a = SRCPIXEL_MASK + bg2rgb[*dest];
            b = a;
            
            a |= 0x01f07c1f;
            b &= 0x40100400;
            a &= 0x3fffffff;
            b  = b - (b >> 5);
            a |= b;
            
            *dest = RGB32k[0][0][a & (a >> 15)];
         }
      }
   }
}

#undef SRCPIXEL
#undef SRCPIXEL_MASK

//
// Normal Column Drawer Object
// haleyjd 09/04/06
//
columndrawer_t r_normal_drawer =
{
   CB_DrawColumn_8,
   CB_DrawTLColumn_8,
   CB_DrawTRColumn_8,
   CB_DrawTLTRColumn_8,
   CB_DrawFuzzColumn_8,
   CB_DrawFlexColumn_8,
   CB_DrawFlexTRColumn_8,
   CB_DrawAddColumn_8,
   CB_DrawAddTRColumn_8,

   NULL,

   {
      // Normal              Translated
      { CB_DrawColumn_8,     CB_DrawTRColumn_8     }, // NORMAL
      { CB_DrawFuzzColumn_8, CB_DrawFuzzColumn_8   }, // SHADOW
      { CB_DrawFlexColumn_8, CB_DrawFlexTRColumn_8 }, // ALPHA
      { CB_DrawAddColumn_8,  CB_DrawAddTRColumn_8  }, // ADD
      { CB_DrawTLColumn_8,   CB_DrawTLTRColumn_8   }, // TRANMAP
   },
};

//
// R_InitTranslationTables
// Creates the translation tables to map
//  the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//

static const char *translations[TRANSLATIONCOLOURS] =
{
   // Standard Doom Colors:
   /*Indigo*/ "112:127=96:111",
   /*Brown */ "112:127=64:79", 
   /*Red   */ "112:127=32:47", 

   // SMMU-compatible colors, re-designed by ptoing:
   /*Tomato*/ "112:113=171,114=172,115:122=173:187,123:124=188:189,125:126=45:47,127=1",
   /*Dirt  */ "112:117=128:133,118:120=135:137,121:123=139:143,124:125=237:239,126:127=1:2",
   /*Blue  */ "112:121=197:206,122:127=240:245",
   /*Gold  */ "112:113=160,114:119=161:166,120:123=236:239,124:125=1:2,126:127=7:8",
   /*Sea   */ "112=91,113:114=94:95,115:122=152:159,123:126=9:12,127=8",
   /*Black */ "112=101,113:121=103:111,122:125=5:8,126:127=0",
   /*Purple*/ "112:113=4,114:115=170,116:117=250,118:119=251,120:121=252,122:123=253,124:125=254,126:127=46",
   /*Vomit */ "112:119=209:216,120:121=218:220,122:124=69:75,125:127=237:239",
   /*Pink  */ "112:113=16:17,114:117=19:25,118:119=27:28,120:124=30:38,125:126=41:43,127=46",
   /*Cream */ "112=4,113:118=48:63,119=65,120:124=68:76,125:126=77:79,127=1",
   /*White */ "112=4,113:115=80:82,116:117=84:86,118:120=89:93,121:127=96:108",
};

// 
// R_InitTranslationTables
//
// haleyjd 01/12/04: rewritten to support translation lumps
//
void R_InitTranslationTables()
{
   int i;
   WadNamespaceIterator wni(wGlobalDir, lumpinfo_t::ns_translations);
   
   // count number of lumps
   firsttranslationlump = wni.getFirstLump();

   // set numtranslations
   numtranslations = TRANSLATIONCOLOURS + wni.getNumLumps();

   // allocate the array of pointers
   translationtables = ecalloctag(byte **, numtranslations, sizeof(byte *), PU_RENDERER, NULL);
   
   // build the internal player translations
   for(i = 0; i < TRANSLATIONCOLOURS; i++)
      translationtables[i] = E_ParseTranslation(translations[i], PU_RENDERER);

   // read in the lumps, if any
   for(wni.begin(); wni.current(); wni.next(), i++)
      translationtables[i] = (byte *)(wGlobalDir.cacheLumpNum((*wni)->selfindex, PU_RENDERER));
}

//
// R_TranslationNumForName
//
// haleyjd 09/13/09
//
int R_TranslationNumForName(const char *name)
{
   int result  = -1;
   int lumpnum = W_CheckNumForNameNS(name, lumpinfo_t::ns_translations);

   // NB: result is + 1 as zero is assumed to be no translation by code 
   // that uses this function
   if(lumpnum != -1)
      result = (lumpnum - firsttranslationlump + TRANSLATIONCOLOURS) + 1;

   return result;
}

//
// R_GetIdentityMap
//
// haleyjd 09/08/12: Returns a pointer to the identity translation.
// There is one global shared copy of it, which can neither be freed 
// nor have its tag changed. If you really want/need your own copy 
// of it for some reason, get it and then memcpy it into your own 
// buffer.
//
byte *R_GetIdentityMap()
{
   static byte *identityMap;

   if(!identityMap)
   {
      identityMap = emalloctag(byte *, 256, PU_PERMANENT, NULL);
      for(int i = 0; i < 256; i++)
         identityMap[i] = i;
   }

   return identityMap;
}

////////////////////////////////////////////////////////////////
// SoM: moved span drawers to r_span.c
////////////////////////////////////////////////////////////////

//
// R_InitBuffer
//
// Creates lookup tables that avoid multiplies and other hazzles
// for getting the framebuffer address of a pixel to draw.
// SoM: ANYRES
//
void R_InitBuffer(int width, int height)
{ 
   int i; 
   int st_height;
   int tviewwidth = viewwidth;
   
   // SoM: use pitch damn you!
   linesize = video.pitch;    // killough 11/98
   
   // Handle resize,
   //  e.g. smaller view windows
   //  with border and/or status bar.
   scaledwindowx = (SCREENWIDTH - width) >> 1;
   viewwindowx   = video.x1lookup[scaledwindowx];

   // Column offset. For windows.
   for (i = tviewwidth ; i--; )   // killough 11/98
      columnofs[i] = viewwindowx + i;
   
   // Same with base row offset.
   st_height = GameModeInfo->StatusBar->height;
   
   if(tviewwidth == video.width)
      viewwindowy = scaledwindowy = 0;
   else
   {
      scaledwindowy = (SCREENHEIGHT - st_height - height) >> 1;
      viewwindowy = video.y1lookup[scaledwindowy];
   }
   
   // Precalculate all row offsets.
   for(i = viewheight; i--; )
      ylookup[i] = video.screens[0] + (i + viewwindowy) * linesize; // killough 11/98
} 

//
// R_FillBackScreen
//
// Fills the back screen with a pattern for variable screen sizes.
// Also draws a beveled edge.
//
void R_FillBackScreen(void) 
{ 
   // killough 11/98: trick to shadow variables
   // SoM: ANYRES use scaledwindowx and scaledwindowy instead
   int x, y; 
   patch_t *patch;
   giborder_t *border = GameModeInfo->border;

   int offset = border->offset;
   int size   = border->size;

   //if(scaledviewwidth == SCREENWIDTH)
   if(scaledviewheight == SCREENHEIGHT)
      return;

   // haleyjd 08/16/02: some restructuring to use GameModeInfo

   // killough 11/98: use the function in m_menu.c
   V_DrawBackground(GameModeInfo->borderFlat, &backscreen1);

   patch = PatchLoader::CacheName(wGlobalDir, border->top, PU_CACHE);

   for(x = 0; x < scaledviewwidth; x += size)
      V_DrawPatch(scaledwindowx+x,scaledwindowy-offset,&backscreen1,patch);

   patch = PatchLoader::CacheName(wGlobalDir, border->bottom, PU_CACHE);

   for(x = 0; x < scaledviewwidth; x += size)   // killough 11/98:
      V_DrawPatch(scaledwindowx+x,scaledwindowy+scaledviewheight,&backscreen1,patch);

   patch = PatchLoader::CacheName(wGlobalDir, border->left, PU_CACHE);

   for(y = 0; y < scaledviewheight; y += size)  // killough 11/98
      V_DrawPatch(scaledwindowx-offset,scaledwindowy+y,&backscreen1,patch);

   patch = PatchLoader::CacheName(wGlobalDir, border->right, PU_CACHE);

   for(y = 0; y < scaledviewheight; y += size)  // killough 11/98
      V_DrawPatch(scaledwindowx+scaledviewwidth,scaledwindowy+y,&backscreen1,patch);

   // Draw beveled edge. 
   V_DrawPatch(scaledwindowx-offset,
               scaledwindowy-offset,
               &backscreen1,
               PatchLoader::CacheName(wGlobalDir, border->c_tl, PU_CACHE));

   V_DrawPatch(scaledwindowx+scaledviewwidth,
               scaledwindowy-offset,
               &backscreen1,
               PatchLoader::CacheName(wGlobalDir, border->c_tr, PU_CACHE));

   V_DrawPatch(scaledwindowx-offset,
               scaledwindowy+scaledviewheight,    // killough 11/98
               &backscreen1,
               PatchLoader::CacheName(wGlobalDir, border->c_bl, PU_CACHE));

   V_DrawPatch(scaledwindowx+scaledviewwidth,
               scaledwindowy+scaledviewheight,     // killough 11/98
               &backscreen1,
               PatchLoader::CacheName(wGlobalDir, border->c_br, PU_CACHE));
} 

//
// R_VideoErase
//
// Copy a screen buffer.
//
// SoM: why the hell was this written to only take an offset and size parameter?
// this is a much nicer solution which fixes scaling issues in highres modes that 
// aren't perfectly 4/3
//
void R_VideoErase(unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{ 
   // haleyjd 06/08/05: protect against bad erasings
   if(x + w > SCREENWIDTH || y + h > SCREENHEIGHT)
      return;

   // SoM: ANYRES
   // This receives scaled offsets.
   if(video.width != SCREENWIDTH || video.height != SCREENHEIGHT)
   {
      // w = x2 - x1 + 1
      w = video.x2lookup[x + w - 1] - video.x1lookup[x] + 1;
      h = video.y2lookup[y + h - 1] - video.y1lookup[y] + 1;
      x = video.x1lookup[x];
      y = video.y1lookup[y];
   }

   V_BlitVBuffer(&vbscreen, x, y, &backscreen1, x, y, w, h);
}

void R_VideoEraseScaled(unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
   if(x + w > static_cast<unsigned int>(vbscreen.width) || 
      y + h > static_cast<unsigned int>(vbscreen.height))
      return;

   V_BlitVBuffer(&vbscreen, x, y, &backscreen1, x, y, w, h);
}

//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
// SoM: Removed old killough hack and reformatted to use new R_VideoErase
//
void R_DrawViewBorder(void) 
{ 
   int side;
   
   if(scaledviewwidth == SCREENWIDTH) 
      return;

   // copy top
   // SoM: ANYRES
   R_VideoErase(0, 0, SCREENWIDTH, scaledwindowy);

   // copy sides
   side = scaledwindowx;
   R_VideoErase(0, scaledwindowy, side, scaledviewheight);
   R_VideoErase(SCREENWIDTH - side, scaledwindowy, side, scaledviewheight);

   // copy bottom 
   R_VideoErase(0, scaledwindowy + scaledviewheight, SCREENWIDTH, scaledwindowy);
} 

// haleyjd: experimental column drawer for masked sky textures
void R_DrawNewSkyColumn(void) 
{ 
  int              count; 
  register byte    *dest;            // killough
  register fixed_t frac;            // killough
  fixed_t          fracstep;     

  count = column.y2 - column.y1 + 1; 

  if (count <= 0)    // Zero length, column does not exceed a pixel.
    return; 
                                 
#ifdef RANGECHECK 
  if ((unsigned int)column.x >= MAX_SCREENWIDTH
      || column.y1 < 0
      || column.y2 >= MAX_SCREENHEIGHT) 
    I_Error ("R_DrawNewSkyColumn: %i to %i at %i\n", column.y1, column.y2, column.x); 
#endif 

  // Framebuffer destination address.
  // Use ylookup LUT to avoid multiply with ScreenWidth.
  // Use columnofs LUT for subwindows? 

  dest = ylookup[column.y1] + columnofs[column.x];  

  // Determine scaling, which is the only mapping to be done.

  fracstep = column.step; 
  frac = column.texmid + (int)((column.y1 - view.ycenter + 1) * fracstep);

  // Inner loop that does the actual texture mapping,
  //  e.g. a DDA-lile scaling.
  // This is as fast as it gets.       (Yeah, right!!! -- killough)
  //
  // killough 2/1/98: more performance tuning

  {
    register const byte *source = (byte *)(column.source);
    register const lighttable_t *colormap = column.colormap; 
    register int heightmask = column.texheight-1;
    if (column.texheight & heightmask)   // not a power of 2 -- killough
      {
        heightmask++;
        heightmask <<= FRACBITS;
          
        if (frac < 0)
          while ((frac += heightmask) <  0);
        else
          while (frac >= heightmask)
            frac -= heightmask;
          
        do
          {
            // Re-map color indices from wall texture column
            //  using a lighting/special effects LUT.
            
            // heightmask is the Tutti-Frutti fix -- killough

            // haleyjd
            if(source[frac>>FRACBITS])
              *dest = colormap[source[frac>>FRACBITS]];
            dest += linesize;                     // killough 11/98
            if ((frac += fracstep) >= heightmask)
              frac -= heightmask;
          } 
        while (--count);
      }
    else
      {
        while ((count-=2)>=0)   // texture height is a power of 2 -- killough
          {
            if(source[(frac>>FRACBITS) & heightmask])
              *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
            if(source[(frac>>FRACBITS) & heightmask])
              *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
            dest += linesize;   // killough 11/98
            frac += fracstep;
          }
        if ((count & 1) && source[(frac>>FRACBITS) & heightmask])
          *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
      }
  }
} 


//----------------------------------------------------------------------------
//
// $Log: r_draw.c,v $
// Revision 1.16  1998/05/03  22:41:46  killough
// beautification
//
// Revision 1.15  1998/04/19  01:16:48  killough
// Tidy up last fix's code
//
// Revision 1.14  1998/04/17  15:26:55  killough
// fix showstopper
//
// Revision 1.13  1998/04/12  01:57:51  killough
// Add main_tranmap
//
// Revision 1.12  1998/03/23  03:36:28  killough
// Use new 'fullcolormap' for fuzzy columns
//
// Revision 1.11  1998/02/23  04:54:59  killough
// #ifdef out translucency code since its in asm
//
// Revision 1.10  1998/02/20  21:57:04  phares
// Preliminarey sprite translucency
//
// Revision 1.9  1998/02/17  06:23:40  killough
// #ifdef out code duplicated in asm for djgpp targets
//
// Revision 1.8  1998/02/09  03:18:02  killough
// Change MAXWIDTH, MAXHEIGHT defintions
//
// Revision 1.7  1998/02/02  13:17:55  killough
// performance tuning
//
// Revision 1.6  1998/01/27  16:33:59  phares
// more testing
//
// Revision 1.5  1998/01/27  16:32:24  phares
// testing
//
// Revision 1.4  1998/01/27  15:56:58  phares
// Comment about invisibility
//
// Revision 1.3  1998/01/26  19:24:40  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/26  05:05:55  killough
// Use unrolled version of R_DrawSpan
//
// Revision 1.1.1.1  1998/01/19  14:03:02  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

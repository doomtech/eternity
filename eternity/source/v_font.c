// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
// Unified Font Engine
//
// haleyjd 01/14/05
//
// The functions in this module replace code that was previously
// duplicated with special-case constants inserted in at least four
// different modules.  This provides a uniform interface for all fonts
// and makes the addition of more fonts easier.
//
// TODO: Add support for rect-blit fonts stored in a single image.
//       Will require masked block drawing functions.
//
//----------------------------------------------------------------------------

#include "doomstat.h"
#include "c_io.h"
#include "d_gi.h"
#include "v_video.h"
#include "v_font.h"

static boolean fixedColor = false;
static int fixedColNum = 0;
static char *altMap = NULL;
static boolean shadowChar = false;
static boolean absCentered = false; // 03/04/06: every line will be centered


//
// V_FontLineWidth
//
// Finds the width of the string up to the first \n or the end.
//
static int V_FontLineWidth(vfont_t *font, const unsigned char *s)
{
   int length = 0;        // current line width
   unsigned char c;
   patch_t *patch = NULL;
   
   for(; *s; s++)
   {
      c = *s;

      if(c >= 128) // color code
         continue;
      
      if(c == '\n') // newline
         break;

      // normalize character
      if(font->upper)
         c = toupper(c) - font->start;
      else
         c = c - font->start;

      if(c >= font->size || !(patch = font->fontgfx[c]))
         length += font->space;
      else
         length += (SHORT(patch->width) - font->dw);
   }   
   
   return length;
}

//
// V_FontWriteText
//
// Generalized bitmapped font drawing code. A vfont_t object contains
// all the information necessary to format the text. Support for
// translucency and color range translation table codes is present,
// and features including the ability to have case-sensitive fonts and
// fonts which center their characters within uniformly spaced blocks
// have been added or absorbed from other code.
//
void V_FontWriteText(vfont_t *font, const char *s, int x, int y)
{
   patch_t *patch = NULL;   // patch for current character
   int     w;               // width of patch
   
   const unsigned char *ch; // pointer to string
   unsigned int c;          // current character
   int cx, cy, tx;          // current screen position

   char    *color = NULL;     // current color range translation tbl
   boolean tl = false;        // current translucency state
   boolean useAltMap = false; // using alternate colormap source?

   if(font->color)
   {
      if(fixedColor)
      {
         // haleyjd 03/27/03: use fixedColor if it was set
         color = colrngs[fixedColNum];
         fixedColor = false;
      }
      else
      {
         // haleyjd: get default text color from gamemode info
         color = *(gameModeInfo->defTextTrans); // Note: ptr to ptr
      }
   }
   
   // haleyjd 10/04/05: support alternate colormap sources
   if(altMap)
   {
      color = altMap;
      altMap = NULL;
      useAltMap = true;
   }
      
   ch = (const unsigned char *)s;

   // haleyjd 03/29/06: special treatment - if first character is 143
   // (abscenter toggle), then toggle absCentered now.
   if(*ch == 143)
      absCentered ^= true;

   cx = absCentered ? (SCREENWIDTH - V_FontLineWidth(font, s)) >> 1 : x;
   cy = y;
   
   while((c = *ch++))
   {
      // color and translucency codes
      if(c >= 128)
      {
         if(c == 138) // translucency toggle
         {
            tl ^= true;
         }
         else if(c == 142) // shadow toggle
         {
            shadowChar ^= true;
         }
         else if(c == 143) // abscenter toggle
         {
            absCentered ^= true;
         }
         else if(font->color && !useAltMap) // not all fonts support translations
         {
            int colnum;
            
            // haleyjd: allow use of gamemode-dependent defaults
            switch(c)
            {
            case 139:
               colnum = gameModeInfo->colorNormal;
               break;
            case 140:
               colnum = gameModeInfo->colorHigh;
               break;
            case 141:
               colnum = gameModeInfo->colorError;
               break;
            default:
               colnum = c - 128;
               break;
            }

            // check that colrng number is within bounds
            if(colnum < 0 || colnum >= CR_LIMIT)
            {
               C_Printf("V_WriteText: invalid colour %i\n", colnum);
               continue;
            }
            else
               color = colrngs[colnum];
         }
         continue;
      }

      // special characters
      if(c == '\t')
      {
         cx = (cx / 40) + 1;
         cx =  cx * 40;
         continue;
      }
      if(c == '\n')
      {
         cx = absCentered ? (SCREENWIDTH - V_FontLineWidth(font, ch+1)) >> 1 : x;
         cy += font->cy;
         continue;
      }
      
      // normalize character
      if(font->upper)
         c = toupper(c) - font->start;
      else
         c = c - font->start;

      // check if within font bounds, draw a space if not
      if(c >= font->size || !(patch = font->fontgfx[c]))
      {
         cx += font->space;
         continue;
      }

      // check against screen bounds
      // haleyjd 12/29/05: text is now clipped by patch drawing code

      w = SHORT(patch->width);

      // possibly adjust x coordinate for centering
      tx = (font->centered ? cx + (font->cw >> 1) - (w >> 1) : cx);
      
      // draw character
      if(tl)
         V_DrawPatchTL(tx, cy, &vbscreen, patch, color, FTRANLEVEL);
      else
      {
         // haleyjd 10/04/05: text shadowing
         if(shadowChar)
         {
            char *cm = (char *)(colormaps[0] + 31*256);
            V_DrawPatchTL(tx + 2, cy + 2, &vbscreen, patch, cm, 
                          FRACUNIT*2/3);
         }
         V_DrawPatchTranslated(tx, cy, &vbscreen, patch, color, false);
      }
      
      // move over in x direction, applying width delta if appropriate
      cx += (font->centered ? font->cw : (w - font->dw));
   }
   
   // reset text shadowing on exit
   shadowChar = false;

   // reset absCentered on exit
   absCentered = false;
}

//
// V_FontWriteTextColored
//
// Write text in a particular colour.
//
void V_FontWriteTextColored(vfont_t *font, const char *s, int color, int x, int y)
{
   if(color < 0 || color >= CR_LIMIT)
   {
      C_Printf("V_FontWriteTextColored: invalid color %i\n", color);
      return;
   }

   fixedColor  = true;
   fixedColNum = color;

   V_FontWriteText(font, s, x, y);
}

//
// V_FontWriteTextMapped
//
// Write text using a specified colormap.
//
void V_FontWriteTextMapped(vfont_t *font, const char *s, int x, int y, char *map)
{
   altMap = map;
   V_FontWriteText(font, s, x, y);
}

//
// V_FontWriteTextShadowed
//
// Write text with a shadow effect.
//
void V_FontWriteTextShadowed(vfont_t *font, const char *s, int x, int y)
{
   shadowChar = true;
   V_FontWriteText(font, s, x, y);
}

//
// V_FontSetAbsCentered
//
// Sets the next string to be printed with absolute centering.
//
void V_FontSetAbsCentered(void)
{
   absCentered = true;
}

//
// V_FontStringHeight
//
// Finds the tallest height in pixels of the string
//
int V_FontStringHeight(vfont_t *font, const unsigned char *s)
{
   unsigned char c;
   int height;
   
   height = font->cy; // a string is always at least one line high
   
   // add an extra cy for each newline found
   while((c = *s++))
   {
      if(c == '\n')
         height += font->cy;
   }
   
   return height;
}

//
// V_FontStringWidth
//
// Finds the width of the longest line in the string
//
int V_FontStringWidth(vfont_t *font, const unsigned char *s)
{
   int length = 0;        // current line width
   int longest_width = 0; // longest line width so far
   unsigned char c;
   patch_t *patch = NULL;
   
   for(; *s; s++)
   {
      c = *s;

      if(c >= 128) // color code
         continue;
      
      if(c == '\n') // newline
      {
         if(length > longest_width) 
            longest_width = length;
         length = 0; // next line;
         continue;	  
      }

      // normalize character
      if(font->upper)
         c = toupper(c) - font->start;
      else
         c = c - font->start;

      if(c >= font->size || !(patch = font->fontgfx[c]))
         length += font->space;
      else
         length += (SHORT(patch->width) - font->dw);
   }
   
   if(length > longest_width)
      longest_width = length; // check last line
   
   return longest_width;
}

extern vfont_t small_font, hud_font, big_font, big_num_font;

//
// V_FontSelect
//
// haleyjd 10/08/05: allows selection of a font based on a numeric id
// number. This could be extended in the future to allow lookup of user
// fonts by a hashed id number (providing against conflict with native
// font id numbers).
//
vfont_t *V_FontSelect(int fontnum)
{
   switch(fontnum)
   {
   case VFONT_SMALL:      // "small" (normal) font defined in v_misc.c
      return &small_font;
   case VFONT_HUD:        // hud font defined in hu_over.c
      return &hud_font;
   case VFONT_BIG:        // "big" font defined in v_misc.c
      return &big_font;
   case VFONT_BIG_NUM:    // "big" font for spaced numbers, v_misc.c
      return &big_num_font;
   default:
      return NULL;
   }
}

// EOF


// Emacs style mode select   -*- C++ -*- vi:ts=3 sw=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
//    EDF Font Definitions
//
//-----------------------------------------------------------------------------

#define NEED_EDF_DEFINITIONS

#include "Confuse/confuse.h"

#include "z_zone.h"
#include "i_system.h"
#include "c_io.h"
#include "d_io.h"
#include "f_finale.h"
#include "hu_over.h"
#include "hu_stuff.h"
#include "in_lude.h"
#include "mn_engin.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patch.h"
#include "w_wad.h"

#include "e_lib.h"
#include "e_edf.h"
#include "e_fonts.h"

//=============================================================================
//
// Data Tables
//

#define ITEM_FONT_ID     "id"
#define ITEM_FONT_START  "start"
#define ITEM_FONT_END    "end"
#define ITEM_FONT_CY     "linesize"
#define ITEM_FONT_SPACE  "spacesize"
#define ITEM_FONT_DW     "widthdelta"
#define ITEM_FONT_ABSH   "tallestchar"
#define ITEM_FONT_CW     "centerwidth"
#define ITEM_FONT_LFMT   "linearformat"
#define ITEM_FONT_LLUMP  "linearlump"
#define ITEM_FONT_FILTER "filter"
#define ITEM_FONT_COLOR  "colorable"
#define ITEM_FONT_UPPER  "uppercase"
#define ITEM_FONT_CENTER "blockcentered"
#define ITEM_FONT_POFFS  "patchnumoffset"

#define ITEM_FILTER_CHARS "chars"
#define ITEM_FILTER_START "start"
#define ITEM_FILTER_END   "end"
#define ITEM_FILTER_MASK  "mask"

static cfg_opt_t filter_opts[] =
{
   CFG_STR(ITEM_FILTER_CHARS, 0,  CFGF_LIST),
   CFG_STR(ITEM_FILTER_START, "", CFGF_NONE),
   CFG_STR(ITEM_FILTER_END,   "", CFGF_NONE),
   CFG_STR(ITEM_FILTER_MASK,  "", CFGF_NONE),
   CFG_END()
};

cfg_opt_t edf_font_opts[] =
{
   CFG_INT(ITEM_FONT_ID,     -1,          CFGF_NONE),
   CFG_STR(ITEM_FONT_START,  "",          CFGF_NONE),
   CFG_STR(ITEM_FONT_END,    "",          CFGF_NONE),
   CFG_INT(ITEM_FONT_CY,     0,           CFGF_NONE),
   CFG_INT(ITEM_FONT_SPACE,  0,           CFGF_NONE),
   CFG_INT(ITEM_FONT_DW,     0,           CFGF_NONE),
   CFG_INT(ITEM_FONT_ABSH,   0,           CFGF_NONE),
   CFG_INT(ITEM_FONT_CW,     0,           CFGF_NONE),
   CFG_STR(ITEM_FONT_LFMT,   "linear",    CFGF_NONE),
   CFG_STR(ITEM_FONT_LLUMP,  "",          CFGF_NONE),   
   CFG_INT(ITEM_FONT_POFFS,  0,           CFGF_NONE),
   CFG_SEC(ITEM_FONT_FILTER, filter_opts, CFGF_MULTI|CFGF_NOCASE),

   CFG_BOOL(ITEM_FONT_COLOR,  cfg_false,  CFGF_NONE),
   CFG_BOOL(ITEM_FONT_UPPER,  cfg_false,  CFGF_NONE),
   CFG_BOOL(ITEM_FONT_CENTER, cfg_false,  CFGF_NONE),
   
   CFG_END()
};

// linear font formats
enum
{
   FONT_FMT_LINEAR,
   FONT_FMT_PATCH,
   NUM_FONT_FMTS
};

static const char *fontfmts[] =
{
   "linear",  // a linear block of pixel data; the default format.
   "patch"    // a DOOM patch, which will be converted to linear.
};

#define NUMFONTCHAINS 31

static vfont_t              *e_font_namechains[NUMFONTCHAINS];
static CDLListItem<vfont_t> *e_font_numchains[NUMFONTCHAINS];

//=============================================================================
//
// Hashing Routines
//

//
// E_AddFontToNameHash
//
// Puts the vfont_t into the name hash table.
//
static void E_AddFontToNameHash(vfont_t *font)
{
   unsigned int key = D_HashTableKey(font->name) % NUMFONTCHAINS;

   font->namenext = e_font_namechains[key];
   e_font_namechains[key] = font;
}

// need forward declaration for E_AutoAllocFontNum
static void E_AddFontToNumHash(vfont_t *font);

// allocation starts at D_MAXINT and works toward 0
static int edf_alloc_fontnum = D_MAXINT;

//
// E_AutoAllocFontNum
//
// Automatically assigns an unused font number to a font
// which was not given one by the author, to allow reference by name
// anywhere without the chore of number allocation.
//
static boolean E_AutoAllocFontNum(vfont_t *font)
{
   int num;

#ifdef RANGECHECK
   if(font->num > 0)
      I_Error("E_AutoAllocFontNum: called for vfont_t with valid num\n");
#endif

   // cannot assign because we're out of dehnums?
   if(edf_alloc_fontnum < 0)
      return false;

   do
   {
      num = edf_alloc_fontnum--;
   } 
   while(num >= 0 && E_FontForNum(num));

   // ran out while looking for an unused number?
   if(num < 0)
      return false;

   // assign it!
   font->num = num;
   E_AddFontToNumHash(font);

   return true;
}

//
// E_AddFontToNumHash
//
// Puts the vfont_t into the numeric hash table.
//
static void E_AddFontToNumHash(vfont_t *font)
{
   unsigned int key = font->num % NUMFONTCHAINS;

   // Auto-assign a numeric key to all fonts which don't have
   // a valid one explicitly specified.

   if(font->num < 0)
   {
      E_AutoAllocFontNum(font);
      return;
   }

   font->numlinks.insert(font, &e_font_numchains[key]);
}

//
// E_DelFontFromNumHash
//
// Removes the given vfont_t from the numeric hash table.
// For overrides changing the numeric key of an existing font.
//
static void E_DelFontFromNumHash(vfont_t *font)
{
   font->numlinks.remove();
}

//=============================================================================
//
// Resource Disposal
//

//
// E_IsLinearLumpUsed
//
// Runs down font hash chains to see if any other linear font is
// using the resource in question, before freeing it.
// Returns true if the resource is used, and false otherwise.
//
static boolean E_IsLinearLumpUsed(vfont_t *font, byte *data)
{
   int i;

   // run down all hash chains
   for(i = 0; i < NUMFONTCHAINS; ++i)
   {
      vfont_t *rover = e_font_namechains[i];

      // run down the chain
      while(rover)
      {
         if(rover != font && rover->linear && rover->data == data)
            return true; // is used
         rover = rover->namenext;
      }
   }

   // didn't find one, doc
   return false;
}

//
// E_IsPatchUsed
//
// Returns true if some other font is using the patch in question,
// to support proper disposal of shared resources.
//
static boolean E_IsPatchUsed(vfont_t *font, patch_t *p)
{
   int i;

   // run down all hash chains
   for(i = 0; i < NUMFONTCHAINS; ++i)
   {
      vfont_t *rover = e_font_namechains[i];

      // run down the chain
      while(rover)
      {
         if(rover != font && rover->fontgfx)
         {
            unsigned int j;
            
            // run down the font graphics
            for(j = 0; j < font->size; ++j)
               if(font->fontgfx[j] == p)
                  return true; // got a match
         }
         rover = rover->namenext;
      }
   }

   // didn't find one, doc
   return false;
}

//
// E_DisposePatches
//
// Dumps all dumpable patches when overwriting a font.
//
static void E_DisposePatches(vfont_t *font)
{
   unsigned int i;

   if(!font->fontgfx)
      return;

   for(i = 0; i < font->size; ++i)
   {
      if(font->fontgfx[i] && !E_IsPatchUsed(font, font->fontgfx[i]))
         Z_ChangeTag(font->fontgfx[i], PU_CACHE); // make purgable
   }

   // get rid of the patch array
   free(font->fontgfx);
   font->fontgfx = NULL;
}

//=============================================================================
//
// Initialization / Resource Loading
//

//
// E_LoadLinearFont
//
// Populates a pre-allocated vfont_t with information on a linear font 
// graphic.
//
static void E_LoadLinearFont(vfont_t *font, const char *name, int fmt)
{
   byte *lump;
   int w, h, size, i, lumpnum;
   boolean foundsize = false;

   // in case this font was changed from patch to block:
   E_DisposePatches(font);

   // handle disposal of previous font graphics 
   if(font->data && !E_IsLinearLumpUsed(font, font->data))
   {
      if(font->linear_font_format != FONT_FMT_LINEAR)
         free(font->data);
      font->data = NULL;
   }

   lumpnum = W_GetNumForName(name);

   lump = (byte *)(W_CacheLumpNum(lumpnum, PU_STATIC));
   size = W_LumpLength(lumpnum);

   if(fmt == FONT_FMT_PATCH)
   {
      // convert patch to linear
      font->data = V_PatchToLinear((patch_t *)lump, false, 0, &w, &h);

      // done with lump
      Z_ChangeTag(lump, PU_CACHE);

      size = w * h;
   }
   else
      font->data = lump;

   font->linear_font_format = fmt;
   if(font->lumpname != name)
      strncpy(font->lumpname, name, 9);

   // check for proper dimensions
   for(i = 5; i <= 32; ++i)
   {
      if(i * i * 128 == size)
      {
         font->lsize = i;
         foundsize = true;
         break;
      }
   }
   
   if(!foundsize)
      E_EDFLoggedErr(2, "E_LoadLinearFont: invalid lump dimensions\n");

   font->start = 0;
   font->end   = 127;
   font->size  = 128; // full ASCII range is supported

   // set metrics - linear fonts are monospace
   font->cw    = font->lsize;
   font->cy    = font->lsize;
   font->dw    = 0;
   font->absh  = font->lsize;
   font->space = font->lsize;

   // set flags
   font->centered = false; // not block-centered
   font->color    = true;  // supports color translations
   font->linear   = true;  // is linear, obviously ;)
   font->upper    = false; // not all-caps

   // patch array is unused
   font->fontgfx = NULL;
}

//
// E_VerifyFilter
//
// Because it's exceptionally dangerous to allow the specification of format
// strings by the end user, I'm going to make sure it cannot be abused.
//
static void E_VerifyFilter(const char *str)
{
   const char *rover = str;
   boolean inpct = false;
   boolean foundpct = false;

   while(*rover != '\0')
   {
      if(inpct)
      {
         // formatting lengths are ok, and are indeed necessary
         if((*rover >= '0' && *rover <= '9') || *rover == '.')
         {
            ++rover;
            continue;
         }

         switch(*rover)
         {
         // allowed characters:
         case 'c': // char
         case 'i': // general int
         case 'd': // int
         case 'x': // hex
         case 'X': // upper-case hex
         case 'o': // octal
         case 'u': // unsigned
            inpct = false;
            break;
         default: // screw you, hacker boy
            E_EDFLoggedErr(2, 
               "E_VerifyFilter: '%s' has bad format specifier %c\n",
               str, *rover);
         }
      }
      
      if(*rover == '%')
      {
         if(foundpct) // already found one? dirty hackers.
         {
            E_EDFLoggedErr(2, 
               "E_VerifyFilter: '%s' has too many format specifiers\n",
               str);
         }
         foundpct = true;
         inpct = true;
      }

      ++rover; // let's not forget to do this!
   }
}

//
// E_FreeFilterData
//
static void E_FreeFilterData(vfontfilter_t *f)
{
   if(f->chars)
   {
      free(f->chars);
      f->chars = NULL;
   }

   if(f->mask)
   {
      free((void *)f->mask);
      f->mask = NULL;
   }
}

//
// E_FreeFontFilters
//
// Frees all the filters in a font that is being overwritten.
//
static void E_FreeFontFilters(vfont_t *font)
{
   unsigned int i;

   if(!font->filters)
      return;

   for(i = 0; i < font->numfilters; ++i)
      E_FreeFilterData(&(font->filters[i]));

   free(font->filters);
   font->filters = NULL;
}

//
// E_ProcessFontFilter
//
// Patch fonts can specify any number of filter objects (at least one is
// required), which have two methods for specifying the lump that belongs
// to a given character or range of characters. The filters use mask strings
// that are C format strings, but the validity of these are checked at load
// by the routine above.
//
static void E_ProcessFontFilter(cfg_t *sec, vfontfilter_t *f)
{
   unsigned int numchars;
   const char *tempstr;
   char *pos = NULL;
   int tempnum = 0;
   unsigned int i;

   // the filter works in one of two ways:
   // 1. specifies a list of characters to which it applies
   // 2. specifies a range of characters from start to end

   // is it a list?
   if((numchars = cfg_size(sec, ITEM_FILTER_CHARS)) > 0)
   {
      f->chars    = (unsigned int *)(calloc(numchars, sizeof(unsigned int)));
      f->numchars = numchars;

      for(i = 0; i < numchars; ++i)
      {
         pos = NULL;
         tempstr = cfg_getnstr(sec, ITEM_FILTER_CHARS, i);

         if(strlen(tempstr) > 1)
            tempnum = strtol(tempstr, &pos, 0);

         if(pos && *pos == '\0')
            f->chars[i] = tempnum;
         else
            f->chars[i] = *tempstr;
      }
   }
   else
   {
      // get fields
      pos = NULL;
      tempstr = cfg_getstr(sec, ITEM_FILTER_START);
      
      if(strlen(tempstr) > 1)
         tempnum = strtol(tempstr, &pos, 0);

      if(pos && *pos == '\0') // is it a number?
         f->start = tempnum;
      else
         f->start = *tempstr; // interpret as character

      pos = NULL;
      tempstr = cfg_getstr(sec, ITEM_FILTER_END);
      
      if(strlen(tempstr) > 1)
         tempnum = strtol(tempstr, &pos, 0);

      if(pos && *pos == '\0') // is it a number?
         f->end = tempnum;
      else
         f->end = *tempstr; // interpret as character
   }

   // get mask
   f->mask = strdup(cfg_getstr(sec, ITEM_FILTER_MASK));

   // verify mask string to prevent hacks
   E_VerifyFilter(f->mask);
}

//
// E_LoadPatchFont
//
// Creates the fontgfx array and precaches all patches as determined via
// execution of the filter objects in the font.
// 
void E_LoadPatchFont(vfont_t *font)
{
   unsigned int i, j, k, m;
   char lumpname[9];

   // dump any pre-existing patch array
   E_DisposePatches(font);

   // in case this font was changed from block to patch:
   if(font->data && !E_IsLinearLumpUsed(font, font->data))
   {
      free(font->data);
      font->data = NULL;
      font->linear = false;
   }

   // first calculate font size
   font->size = (font->end - font->start + 1);

   if(font->size <= 0)
      E_EDFLoggedErr(2, "E_LoadPatchFont: font %s size <= 0\n", font->name);

   // init all to NULL at beginning
   // [CG] Try PU_RENDERER here.
   // font->fontgfx = (patch_t **)(calloc(font->size, sizeof(patch_t *)));
   Z_Calloc(font->size, sizeof(patch_t *), PU_RENDERER, &font->fontgfx));

   for(i = 0, j = font->start; i < font->size; i++, j++)
   {
      vfontfilter_t *filter, *filtertouse = NULL;

      // run down filters until a match is found
      for(k = 0; k < font->numfilters; ++k)
      {
         filter = &(font->filters[k]);
         if(filter->numchars)
         {
            for(m = 0; m < filter->numchars; ++m)
               if(j == filter->chars[m])
                  filtertouse = filter;
         }
         else if(j >= filter->start && j <= filter->end)
            filtertouse = filter;

         if(filtertouse)
            break;
      }

      if(filtertouse)
      {
         int lnum;

         memset(lumpname, 0, sizeof(lumpname));
         psnprintf(lumpname, sizeof(lumpname), 
                   filtertouse->mask, j - font->patchnumoffset);

         if((lnum = W_CheckNumForName(lumpname)) >= 0) // no errors here.
            font->fontgfx[i] = (patch_t *)(W_CacheLumpNum(lnum, PU_STATIC));
      }
   }
}

//
// E_UpdateFonts
//
// [CG] Updates all fonts, used after a WAD reload.
//
void E_UpdateFonts(void)
{
   vfont_t *font = NULL;
   unsigned int i, j;

   for(i = 0; i < NUMFONTCHAINS; i++)
   {
      font = e_font_namechains[i];

      while(font)
      {
         /*
         for(i = 0; i < font->size; ++i)
         {
            if(font->fontgfx[i])
            {
               Z_ChangeTag(font->fontgfx[i], PU_CACHE); // make purgable
            }
         }
         */

         if(font->linear)
            E_LoadLinearFont(font, font->lumpname, font->linear_font_format);
         else
            E_LoadPatchFont(font);

         font = font->namenext;
      }
   }
}

//=============================================================================
//
// EDF Processing
//

#define IS_SET(sec, name) (def || cfg_size(sec, name) > 0)

//
// E_ProcessFont
//
// Processes a single EDF font object.
//
static void E_ProcessFont(cfg_t *sec)
{
   vfont_t *font;
   const char *title, *tempstr;
   boolean def = true;
   int num, tempnum = 0;

   title = cfg_title(sec);
   num   = cfg_getint(sec, ITEM_FONT_ID);

   // if one exists by this name already, modify it
   if((font = E_FontForName(title)))
   {
      // check numeric key
      if(font->num != num)
      {
         // remove from numeric hash
         E_DelFontFromNumHash(font);

         // change the key
         font->num = num;

         // rehash
         E_AddFontToNumHash(font);
      }

      // not a definition
      def = false;
   }
   else
   {
      font = (vfont_t *)(calloc(1, sizeof(vfont_t)));

      if(strlen(title) > 32)
         E_EDFLoggedErr(2, "E_ProcessFont: mnemonic '%s' is too long\n", title);

      strncpy(font->name, title, 33);
      font->num = num;

      // add to hash tables
      E_AddFontToNameHash(font);
      E_AddFontToNumHash(font);
   }

   // process start
   if(IS_SET(sec, ITEM_FONT_START))
   {
      char *pos = NULL;
      tempstr = cfg_getstr(sec, ITEM_FONT_START);

      if(strlen(tempstr) > 1)
         tempnum = strtol(tempstr, &pos, 0);

      if(pos && *pos == '\0') // it is a number?
         font->start = tempnum; 
      else
         font->start = *tempstr; // interpret as character
   }

   // process end
   if(IS_SET(sec, ITEM_FONT_END))
   {
      char *pos = NULL;
      tempstr = cfg_getstr(sec, ITEM_FONT_END);

      if(strlen(tempstr) > 1)
         tempnum = strtol(tempstr, &pos, 0);

      if(pos && *pos == '\0') // is it a number?
         font->end = tempnum;
      else
         font->end = *tempstr; // interpret as character
   }

   // process linebreak height
   if(IS_SET(sec, ITEM_FONT_CY))
      font->cy = cfg_getint(sec, ITEM_FONT_CY);

   // process space size
   if(IS_SET(sec, ITEM_FONT_SPACE))
      font->space = cfg_getint(sec, ITEM_FONT_SPACE);

   // process width delta
   if(IS_SET(sec, ITEM_FONT_DW))
      font->dw = cfg_getint(sec, ITEM_FONT_DW);

   // process absolute height (tallest character)
   if(IS_SET(sec, ITEM_FONT_ABSH))
      font->absh = cfg_getint(sec, ITEM_FONT_ABSH);

   // process centered width
   if(IS_SET(sec, ITEM_FONT_CW))
      font->cw = cfg_getint(sec, ITEM_FONT_CW);

   // process colorable flag
   if(IS_SET(sec, ITEM_FONT_COLOR))
      font->color = (cfg_getbool(sec, ITEM_FONT_COLOR) == cfg_true);

   // process uppercase flag
   if(IS_SET(sec, ITEM_FONT_UPPER))
      font->upper = (cfg_getbool(sec, ITEM_FONT_UPPER) == cfg_true);

   // process blockcentered flag
   if(IS_SET(sec, ITEM_FONT_CENTER))
      font->centered = (cfg_getbool(sec, ITEM_FONT_CENTER) == cfg_true);

   // process linear lump - if defined, this is a linear font automatically
   if(cfg_size(sec, ITEM_FONT_LLUMP) > 0)
   {
      int format;
      const char *fmtstr;
      tempstr = cfg_getstr(sec, ITEM_FONT_LLUMP);

      // get also the linear format
      fmtstr = cfg_getstr(sec, ITEM_FONT_LFMT);

      format = E_StrToNumLinear(fontfmts, NUM_FONT_FMTS, fmtstr);

      if(format == NUM_FONT_FMTS)
         E_EDFLoggedErr(2, "E_ProcessFont: invalid linear format '%s'\n", fmtstr);

      E_LoadLinearFont(font, tempstr, format);
   }
   else
   {
      unsigned int i;
      unsigned int numfilters;

      // handle disposal of pre-existing filters
      E_FreeFontFilters(font);

      // process font filter objects now
      if((numfilters = cfg_size(sec, ITEM_FONT_FILTER)) <= 0)
         E_EDFLoggedErr(2, "E_ProcessFont: at least one filter is required\n");

      // allocate the font filters
      font->filters    = (vfontfilter_t *)(calloc(numfilters, sizeof(vfontfilter_t)));
      font->numfilters = numfilters;
      
      for(i = 0; i < numfilters; ++i)
      {
         E_ProcessFontFilter(cfg_getnsec(sec, ITEM_FONT_FILTER, i), 
                             &(font->filters[i]));
      }

      font->patchnumoffset = cfg_getint(sec, ITEM_FONT_POFFS);

      // load the font
      E_LoadPatchFont(font);
   }

   E_EDFLogPrintf("\t\t%s font %s\n", 
                  def ? "Defined" : "Modified", font->name);
}

//
// E_ProcessFontVars
//
// Processes setting of native subsystem fonts via EDF globals.
//
static void E_ProcessFontVars(cfg_t *cfg)
{
   // [CG] These need to be free'd first if applicable, otherwise WAD reloads
   //      will leak memory.
   if(hud_fontname)
      free(hud_fontname);
   if(hud_overfontname)
      free(hud_overfontname);
   if(mn_fontname)
      free(mn_fontname);
   if(mn_bigfontname)
      free(mn_bigfontname);
   if(mn_normalfontname)
      free(mn_normalfontname);
   if(f_fontname)
      free(f_fontname);
   if(in_fontname)
      free(in_fontname);
   if(in_bigfontname)
      free(in_bigfontname);
   if(in_bignumfontname)
      free(in_bignumfontname);
   if(c_fontname)
      free(c_fontname);

   // 02/25/09: set native module font names
   hud_fontname      = strdup(cfg_getstr(cfg, ITEM_FONT_HUD));
   hud_overfontname  = strdup(cfg_getstr(cfg, ITEM_FONT_HUDO));
   mn_fontname       = strdup(cfg_getstr(cfg, ITEM_FONT_MENU));
   mn_bigfontname    = strdup(cfg_getstr(cfg, ITEM_FONT_BMENU));
   mn_normalfontname = strdup(cfg_getstr(cfg, ITEM_FONT_NMENU));
   f_fontname        = strdup(cfg_getstr(cfg, ITEM_FONT_FINAL));
   in_fontname       = strdup(cfg_getstr(cfg, ITEM_FONT_INTR));
   in_bigfontname    = strdup(cfg_getstr(cfg, ITEM_FONT_INTRB));
   in_bignumfontname = strdup(cfg_getstr(cfg, ITEM_FONT_INTRBN));
   c_fontname        = strdup(cfg_getstr(cfg, ITEM_FONT_CONS));
}

//=============================================================================
//
// Global Routines
//

//
// E_ProcessFonts
//
// Adds all fonts in the given cfg_t.
//
void E_ProcessFonts(cfg_t *cfg)
{
   unsigned int i;
   unsigned int numfonts = cfg_size(cfg, EDF_SEC_FONT);

   E_EDFLogPrintf("\t* Processing fonts\n"
                  "\t\t%d fonts(s) defined\n", numfonts);

   for(i = 0; i < numfonts; ++i)
      E_ProcessFont(cfg_getnsec(cfg, EDF_SEC_FONT, i));

   E_ProcessFontVars(cfg);
}

//
// E_FontForName
//
// Finds a font for the given name. If the name does not exist,
// NULL is returned.
//
vfont_t *E_FontForName(const char *name)
{
   unsigned int key = D_HashTableKey(name) % NUMFONTCHAINS;
   vfont_t *font = e_font_namechains[key];

   while(font && strcasecmp(font->name, name))
      font = font->namenext;

   return font;
}

//
// E_FontForNum
//
// Finds a font for the given number. If the number is not found,
// NULL is returned.
//
vfont_t *E_FontForNum(int num)
{
   unsigned int key = num % NUMFONTCHAINS;
   CDLListItem<vfont_t> *link = e_font_numchains[key];

   while(link && link->dllObject->num != num)
      link = link->dllNext;

   return link ? link->dllObject : NULL;
}

//
// E_FontNumForName
//
// Given a name, returns the corresponding font number, or -1 if
// the name does not exist.
//
int E_FontNumForName(const char *name)
{ 
   vfont_t *font = E_FontForName(name);

   return font ? font->num : -1;
}

// EOF


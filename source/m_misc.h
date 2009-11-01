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
//    Misc stuff - config file, screenshots
//    
//-----------------------------------------------------------------------------

#ifndef __M_MISC__
#define __M_MISC__

#include "doomtype.h"
#include "doomstat.h" // haleyjd: this was missing

//
// MISC
//

boolean M_WriteFile(const char *name, void *source, unsigned int length);
int     M_ReadFile(const char *name, byte **buffer);
void    M_ScreenShot(void);
void    M_LoadDefaults(void);
void    M_SaveDefaults(void);
int     M_DrawText(int x,int y,boolean direct, char *string);
void    M_LoadOptions(void);                             // killough 11/98

// haleyjd: Portable versions of common non-standard C functions.
// Some of these default to the standard implementation if its
// existence is verifiable (see d_keywds.h)

char *M_Strupr(char *string);
char *M_Strlwr(char *string);
char *M_Itoa(int value, char *string, int radix);

// Misc file routines
// haleyjd: moved a number of these here from w_wad module.

void  M_GetFilePath(const char *fn, char *base, size_t len); // haleyjd
int   M_FileLength(int handle);
void  M_ExtractFileBase(const char *, char *);               // killough
char *M_AddDefaultExtension(char *, const char *);           // killough 1/18/98
void  M_NormalizeSlashes(char *);                            // killough 11/98

int M_StringAlloca(char **str, int numstrs, size_t extra, const char *str1, ...);

extern int screenshot_pcx;                                   // killough 10/98
extern int screenshot_gamma;                                 // haleyjd  03/06

// haleyjd 07/27/09: default file portability fix - separate types for config
// variables

typedef enum
{
   dt_integer,
   dt_string,
   dt_float,
   dt_boolean,
   dt_numtypes
} defaulttype_e;

// phares 4/21/98:
// Moved from m_misc.c so m_menu.c could see it.
//
// killough 11/98: totally restructured

typedef struct default_s
{
   const char *const   name;                 // name
   const defaulttype_e type;                 // type

   void *const location;                     // default variable
   void *const current;                      // possible nondefault variable
   
   int         defaultvalue_i;               // built-in default value
   const char *defaultvalue_s;
   float       defaultvalue_f;
   boolean     defaultvalue_b;

   struct { int min, max; } const limit;       // numerical limits
      
   enum { wad_no, wad_yes } const wad_allowed; // whether it's allowed in wads
   const char *const help;                     // description of parameter
   
   // internal fields (initialized implicitly to 0) follow
   
   struct default_s *first, *next;           // hash table pointers
   int modified;                             // Whether it's been modified
   
   int         orig_default_i;               // Original default, if modified
   const char *orig_default_s;
   float       orig_default_f;
   boolean     orig_default_b;
   
   //struct setup_menu_s *setup_menu;          // Xref to setup menu item, if any
} default_t;

// haleyjd 07/27/09: Macros for defining configuration values.

#define DEFAULT_INT(name, loc, cur, def, min, max, wad, help) \
   { name, dt_integer, loc, cur, def, NULL, 0.0f, false, { min, max }, wad, help }

#define DEFAULT_STR(name, loc, cur, def, wad, help) \
   { name, dt_string, loc, cur, 0, def, 0.0f, false, { 0 }, wad, help }

#define DEFAULT_FLOAT(name, loc, cur, def, min, max, wad, help) \
   { name, dt_float, loc, cur, 0, NULL, def, false, { min, max }, wad, help }

#define DEFAULT_BOOL(name, loc, cur, def, wad, help) \
   { name, dt_boolean, loc, cur, 0, NULL, 0.0f, def, { 0, 1 }, wad, help }

// haleyjd 03/14/09: defaultfile_t structure
typedef struct defaultfile_s
{
   default_t   *defaults;   // array of defaults
   size_t      numdefaults; // length of defaults array
   boolean     hashInit;    // if true, this default file's hash table is setup
   char        *fileName;   // name of corresponding file
   boolean     loaded;      // if true, defaults are loaded
   boolean     helpHeader;  // has help header?
   struct comment_s
   { 
      char *text; 
      int line; 
   } *comments;             // stored comments
   size_t numcomments;      // number of comments stored
   size_t numcommentsalloc; // number of comments allocated
} defaultfile_t;

// haleyjd 06/29/09: default overrides
typedef struct default_or_s
{
   const char *name;
   int defaultvalue;
} default_or_t;

// killough 11/98:
boolean    M_ParseOption(defaultfile_t *df, const char *name, boolean wad);
void       M_LoadDefaultFile(defaultfile_t *df);
void       M_SaveDefaultFile(defaultfile_t *df);

#define UL (-123456789) /* magic number for no min or max for parameter */

// haleyjd 06/24/02: platform-dependent macros for sound/music defaults
#if defined(DJGPP)
  #define SND_DEFAULT -1
  #define SND_MIN     -1
  #define SND_MAX      7
  #define SND_DESCR    "code used by Allegro to select sounds driver, -1 is autodetect"
  #define MUS_DEFAULT -1
  #define MUS_MIN     -1
  #define MUS_MAX      9
  #define MUS_DESCR    "code used by Allegro to select music driver, -1 is autodetect"
#elif defined(_SDL_VER)
  #define SND_DEFAULT -1
  #define SND_MIN     -1
  #define SND_MAX      1
  #define SND_DESCR    "code to select digital sound, -1 is SDL sound, 0 is no sound, 1 is PC speaker emulation"
  #define MUS_DEFAULT -1
  #define MUS_MIN     -1
  #define MUS_MAX      0
  #define MUS_DESCR    "code to select music device, -1 is SDL_mixer, 0 is no music"
#else
  #define SND_DEFAULT  0
  #define SND_MIN      0
  #define SND_MAX      0
  #define SND_DESCR    "no sound driver available for this platform"
  #define MUS_DEFAULT  0
  #define MUS_MIN      0
  #define MUS_MAX      0
  #define MUS_DESCR    "no midi driver available for this platform"
#endif

#endif

//----------------------------------------------------------------------------
//
// $Log: m_misc.h,v $
// Revision 1.4  1998/05/05  19:56:06  phares
// Formatting and Doc changes
//
// Revision 1.3  1998/04/22  13:46:17  phares
// Added Setup screen Reset to Defaults
//
// Revision 1.2  1998/01/26  19:27:12  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:58  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

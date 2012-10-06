// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2012 James Haley
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//    Routines for IWAD location, version identification, and loading.
//    Split out of d_main.cpp 08/12/12.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "hal/i_picker.h"
#include "hal/i_platform.h"

#include "d_diskfile.h"
#include "d_files.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_main.h"
#include "doomstat.h"
#include "g_game.h"
#include "g_gfs.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_collection.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "p_info.h"
#include "sounds.h"
#include "w_wad.h"

// D_FIXME:
extern bool gamepathset;
extern int  gamepathparm;
void D_InitPaths();
void D_CheckGameMusic();


static char *baseiwad;                // jff 3/23/98: iwad directory

//=============================================================================
//
// DOOMWADPATH support
//
// haleyjd 12/31/10: A standard evolved later for DOOMWADPATH, in preference to
// use of DOOMWADDIR, which could only specify a single path. DOOMWADPATH is 
// like the standard system path, except for wads. When looking for a file on
// the DOOMWADPATH, the paths in the variable will be tried in the order they
// are specified.
//

// doomwadpaths is an array of paths, the decomposition of the DOOMWADPATH
// environment variable
static PODCollection<char *> doomwadpaths;

//
// D_addDoomWadPath
//
// Adds a path to doomwadpaths.
//
static void D_addDoomWadPath(const char *path)
{
   doomwadpaths.add(estrdup(path));
}

// haleyjd 01/17/11: Use a different separator on Windows than on POSIX platforms
#if EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS
#define DOOMWADPATHSEP ';'
#else
#define DOOMWADPATHSEP ':'
#endif

//
// D_parseDoomWadPath
//
// Looks for the DOOMWADPATH environment variable. If it is defined, then
// doomwadpaths will consist of the components of the decomposed variable.
//
static void D_parseDoomWadPath(void)
{
   const char *dwp;

   if((dwp = getenv("DOOMWADPATH")))
   {
      char *tempdwp = Z_Strdupa(dwp);
      char *rover   = tempdwp;
      char *currdir = tempdwp;
      int   dirlen  = 0;

      while(*rover)
      {
         // Found the end of a path?
         // Add the string from the prior one (or the beginning) to here as
         // a separate path.
         if(*rover == DOOMWADPATHSEP)
         {
            *rover = '\0'; // replace ; or : with a null terminator
            if(dirlen)
               D_addDoomWadPath(currdir);
            dirlen = 0;
            currdir = rover + 1; // start of next path is the next char
         }
         else
            ++dirlen; // Length is tracked so that we don't add any 0-length paths

         ++rover;
      }

      // Add the last path if necessary (it's either the only one, or the final
      // one, which is probably not followed by a semicolon).
      if(dirlen)
         D_addDoomWadPath(currdir);
   }
}

//
// D_FindInDoomWadPath
//
// Looks for a file in each path extracted from DOOMWADPATH in the order the
// paths were defined. A normalized concatenation of the path and the filename
// will be returned which must be freed by the calling code, if the file is
// found. Otherwise NULL is returned.
//
char *D_FindInDoomWadPath(const char *filename, const char *extension)
{
   qstring qstr;
   char *concat  = NULL;
   char *currext = NULL;
   size_t numpaths = doomwadpaths.getLength();

   for(size_t i = 0; i < numpaths ; i++)
   {
      struct stat sbuf;
      
      qstr = doomwadpaths[i];
      qstr.pathConcatenate(filename);

      // See if the file exists as-is
      if(!stat(qstr.constPtr(), &sbuf)) // check for existence
      {
         if(!S_ISDIR(sbuf.st_mode)) // check that it's NOT a directory
         {
            concat = qstr.duplicate(PU_STATIC);
            break; // done.
         }
      }

      // See if the file could benefit from having the default extension
      // added to it.
      if(extension && (currext = qstr.bufferAt(qstr.length() - 4))) 
      {
         if(strcasecmp(currext, extension)) // Doesn't already have it?
         {
            qstr += extension;

            if(!stat(qstr.constPtr(), &sbuf)) // exists?
            {
               if(!S_ISDIR(sbuf.st_mode)) // not a dir?
               {
                  concat = qstr.duplicate(PU_STATIC);
                  break; // done.
               }
            }
         }
      }
   }

   return concat;
}

//=============================================================================
//
// Disk File Handling
//
// haleyjd 05/28/10
//

// known .disk types
enum
{
   DISK_DOOM,
   DISK_DOOM2
};

static bool havediskfile;    // if true, -disk loaded a file
static bool havediskiwad;    // if true, an IWAD was found in the disk file
static const char *diskpwad; // PWAD name (or substring) to look for
static diskfile_t *diskfile; // diskfile object (see d_diskfile.c)
static diskwad_t   diskiwad; // diskwad object for iwad
static int disktype;         // type of disk file

//
// D_CheckDiskFileParm
//
// haleyjd 05/28/10: Looks for -disk and sets up diskfile loading.
//
static void D_CheckDiskFileParm(void)
{
   int p;
   const char *fn;

   if((p = M_CheckParm("-disk")) && p < myargc - 1)
   {
      havediskfile = true;

      // get diskfile name
      fn = myargv[p + 1];

      // have a pwad name as well?
      if(p < myargc - 2 && *(myargv[p + 2]) != '-')
         diskpwad = myargv[p + 2];

      // open the diskfile
      diskfile = D_OpenDiskFile(fn);
   }
}

//
// D_FindDiskFileIWAD
//
// Finds an IWAD in a disk file.
// If this fails, the disk file will be closed.
//
static void D_FindDiskFileIWAD(void)
{
   diskiwad = D_FindWadInDiskFile(diskfile, "doom");

   if(diskiwad.f)
   {
      havediskiwad = true;

      if(strstr(diskiwad.name, "doom2.wad"))
         disktype = DISK_DOOM2;
      else
         disktype = DISK_DOOM;
   }
   else
   {
      // close it up, we can't use it
      D_CloseDiskFile(diskfile, true);
      diskfile     = NULL;
      diskpwad     = NULL;
      havediskfile = false;
      havediskiwad = false;
   }
}

//
// D_LoadDiskFileIWAD
//
// Loads an IWAD from the disk file.
//
static void D_LoadDiskFileIWAD(void)
{
   if(diskiwad.f)
      D_AddFile(diskiwad.name, lumpinfo_t::ns_global, diskiwad.f, diskiwad.offset, 0);
   else
      I_Error("D_LoadDiskFileIWAD: invalid file pointer\n");
}

//
// D_LoadDiskFilePWAD
//
// Loads a PWAD from the disk file.
//
static void D_LoadDiskFilePWAD(void)
{
   diskwad_t wad = D_FindWadInDiskFile(diskfile, diskpwad);

   if(wad.f)
   {
      if(!strstr(wad.name, "doom")) // do not add doom[2].wad twice
         D_AddFile(wad.name, lumpinfo_t::ns_global, wad.f, wad.offset, 0);
   }
}

//
// D_metaGetLine
//
// Gets a single line of input from the metadata.txt resource.
//
static bool D_metaGetLine(qstring &qstr, const char *input, int *idx)
{
   int i = *idx;

   // if empty at start, we are finished
   if(input[i] == '\0')
      return false;

   qstr.clear();

   while(input[i] != '\n' && input[i] != '\0')
   {
      if(input[i] == '\\' && input[i+1] == 'n')
      {
         // make \n sequence into a \n character
         ++i;
         qstr += '\n';
      }
      else if(input[i] != '\r')
         qstr += input[i];

      ++i;
   }

   if(input[i] == '\n')
      ++i;

   // write back input position
   *idx = i;

   return true;
}

//
// D_DiskMetaData
//
// Handles metadata in the disk file.
//
static void D_DiskMetaData(void)
{
   char *name  = NULL, *metatext = NULL;
   const char *slash = NULL;
   const char *endtext = NULL, *levelname = NULL, *musicname = NULL;
   int slen = 0, index = 0;
   int partime = 0, musicnum = 0;
   int exitreturn = 0, secretlevel = 0, levelnum = 1, linenum = 0;
   diskwad_t wad;
   qstring buffer;

   if(!diskpwad)
      return;

   // find the wad to get the canonical resource name
   wad = D_FindWadInDiskFile(diskfile, diskpwad);

   // return if not found, or if this is metadata for the IWAD
   if(!wad.f || strstr(wad.name, "doom"))
      return;

   // construct the metadata filename
   M_StringAlloca(&name, 2, 1, wad.name, "metadata.txt");
   
   if(!(slash = strrchr(wad.name, '\\')))
      return;

   slen = slash - wad.name;
   ++slen;
   strncpy(name, wad.name, slen);
   strcpy(name + slen, "metadata.txt");

   // load it up
   if(!(metatext = (char *)(D_CacheDiskFileResource(diskfile, name, true))))
      return;

   // parse it

   // get first line, which is an episode id
   D_metaGetLine(buffer, metatext, &index);

   // get episode name
   if(D_metaGetLine(buffer, metatext, &index))
      GameModeInfo->versionName = buffer.duplicate(PU_STATIC);

   // get end text
   if(D_metaGetLine(buffer, metatext, &index))
      endtext = buffer.duplicate(PU_STATIC);

   // get next level after secret
   if(D_metaGetLine(buffer, metatext, &index))
      exitreturn = buffer.toInt();

   // skip next line (wad name)
   D_metaGetLine(buffer, metatext, &index);

   // get secret level
   if(D_metaGetLine(buffer, metatext, &index))
      secretlevel = buffer.toInt();

   // get levels
   while(D_metaGetLine(buffer, metatext, &index))
   {
      switch(linenum)
      {
      case 0: // levelname
         levelname = buffer.duplicate(PU_STATIC);
         break;
      case 1: // music number
         musicnum = mus_runnin + buffer.toInt() - 1;

         if(musicnum > GameModeInfo->musMin && musicnum < GameModeInfo->numMusic)
            musicname = S_music[musicnum].name;
         else
            musicname = "";
         break;
      case 2: // partime (final field)
         partime = buffer.toInt();

         // create a metainfo object for LevelInfo
         P_CreateMetaInfo(levelnum, levelname, partime, musicname, 
                          levelnum == secretlevel ? exitreturn : 0,
                          levelnum == exitreturn - 1 ? secretlevel : 0,
                          levelnum == secretlevel - 1, 
                          (levelnum == secretlevel - 1) ? endtext : NULL);
         break;
      }
      ++linenum;

      if(linenum == 3)
      {
         levelnum++;
         linenum = 0;
      }
   }

   // done with metadata resource
   efree(metatext);
}

//=============================================================================
//
// IWAD Detection / Verification Code
//

int iwad_choice; // haleyjd 03/19/10: remember choice

// variable-for-index lookup for D_DoIWADMenu
static char **iwadVarForNum[NUMPICKIWADS] =
{
   &gi_path_doomsw, &gi_path_doomreg, &gi_path_doomu,  // Doom 1
   &gi_path_doom2,  &gi_path_tnt,     &gi_path_plut,   // Doom 2
   &gi_path_hacx,                                      // HACX
   &gi_path_hticsw, &gi_path_hticreg, &gi_path_sosr,   // Heretic
   &gi_path_fdoom,  &gi_path_fdoomu,  &gi_path_freedm, // FreeDoom
};

//
// D_doIWADMenu
//
// Crazy fancy graphical IWAD choosing menu.
// Paths are stored in the system.cfg file, and only the games with paths
// stored can be picked from the menu.
// This feature is only available for SDL builds.
//
static const char *D_doIWADMenu(void)
{
   const char *iwadToUse = NULL;

#ifdef _SDL_VER
   bool haveIWADs[NUMPICKIWADS];
   int i, choice = -1;
   bool foundone = false;

   // populate haveIWADs array based on system.cfg variables
   for(i = 0; i < NUMPICKIWADS; ++i)
   {
      if((haveIWADs[i] = (**iwadVarForNum[i] != '\0')))
         foundone = true;
   }

   if(foundone) // at least one IWAD must be specified!
   {
      startupmsg("D_DoIWADMenu", "Init IWAD choice subsystem.");
      choice = I_Pick_DoPicker(haveIWADs, iwad_choice);
   }

   if(choice >= 0)
   {
      iwad_choice = choice;               // 03/19/10: remember selection
      iwadToUse = *iwadVarForNum[choice];
   }
#endif

   return iwadToUse;
}

// Match modes for iwadpathmatch 
enum
{
   MATCH_NONE,
   MATCH_GAME,
   MATCH_IWAD
};

//
// iwadpathmatch
//
// This structure is used for finding an IWAD path variable that is the
// best match for a -game or -iwad string. A predefined priority is imposed
// on the IWAD variables so that the "best" version of the game available
// is chosen.
//
struct iwadpathmatch_t
{
   int         mode;         // Mode for this entry: -game, or -iwad
   const char *name;         // The -game or -iwad substring matched
   char      **iwadpaths[3]; // IWAD path variables to check when this matches,
                             // in order of precedence from greatest to least.
};

static iwadpathmatch_t iwadMatchers[] =
{
   // -game matches:
   { MATCH_GAME, "doom2",     { &gi_path_doom2,  &gi_path_fdoom,   NULL            } },
   { MATCH_GAME, "doom",      { &gi_path_doomu,  &gi_path_doomreg, &gi_path_doomsw } },
   { MATCH_GAME, "tnt",       { &gi_path_tnt,    NULL,             NULL            } },
   { MATCH_GAME, "plutonia",  { &gi_path_plut,   NULL,             NULL            } },
   { MATCH_GAME, "hacx",      { &gi_path_hacx,   NULL,             NULL            } },
   { MATCH_GAME, "heretic",   { &gi_path_sosr,   &gi_path_hticreg, &gi_path_hticsw } },

   // -iwad matches 
   { MATCH_IWAD, "doom2f",    { &gi_path_doom2,  &gi_path_fdoom,   NULL            } },
   { MATCH_IWAD, "doom2",     { &gi_path_doom2,  &gi_path_fdoom,   NULL            } },
   { MATCH_IWAD, "doomu",     { &gi_path_doomu,  &gi_path_fdoomu,  NULL            } },
   { MATCH_IWAD, "doom1",     { &gi_path_doomsw, NULL,             NULL            } },
   { MATCH_IWAD, "doom",      { &gi_path_doomu,  &gi_path_doomreg, &gi_path_fdoomu } },
   { MATCH_IWAD, "tnt",       { &gi_path_tnt,    NULL,             NULL            } },
   { MATCH_IWAD, "plutonia",  { &gi_path_plut,   NULL,             NULL            } },
   { MATCH_IWAD, "hacx",      { &gi_path_hacx,   NULL,             NULL            } },
   { MATCH_IWAD, "heretic1",  { &gi_path_hticsw, NULL,             NULL            } },
   { MATCH_IWAD, "heretic",   { &gi_path_sosr,   &gi_path_hticreg, NULL            } },
   { MATCH_IWAD, "freedoom",  { &gi_path_fdoom,  NULL,             NULL            } },
   { MATCH_IWAD, "freedoomu", { &gi_path_fdoomu, NULL,             NULL            } },
   { MATCH_IWAD, "freedm",    { &gi_path_freedm, NULL,             NULL            } },
   
   // Terminating entry
   { MATCH_NONE, NULL,        { NULL,            NULL,             NULL            } }
};

//
// D_IWADPathForGame
//
// haleyjd 12/31/10: Return the best defined IWAD path variable for a 
// -game parameter. Returns NULL if none found.
//
static char *D_IWADPathForGame(const char *game)
{
   iwadpathmatch_t *cur = iwadMatchers;

   while(cur->mode != MATCH_NONE)
   {
      if(cur->mode == MATCH_GAME) // is a -game matcher?
      {
         if(!strcasecmp(cur->name, game)) // should be an exact match
         {
            for(int i = 0; i < 3; ++i) // try each path in order
            {
               if(!cur->iwadpaths[i]) // no more valid paths to try
                  break;

               if(**(cur->iwadpaths[i]) != '\0')
                  return *(cur->iwadpaths[i]); // got one!
            }
         }
      }
      ++cur; // try the next entry
   }

   return NULL; // nothing was found
}

//
// D_IWADPathForIWADParam
//
// haleyjd 12/31/10: Return the best defined IWAD path variable for a 
// -iwad parameter. Returns NULL if none found.
//
static char *D_IWADPathForIWADParam(const char *iwad)
{
   iwadpathmatch_t *cur = iwadMatchers;
   
   // If the name starts with a slash, step forward one
   char *tmpname = Z_Strdupa((*iwad == '/' || *iwad == '\\') ? iwad + 1 : iwad);
   
   // Truncate at any extension
   char *dotpos = strrchr(tmpname, '.');
   if(dotpos)
      *dotpos = '\0';

   while(cur->mode != MATCH_NONE)
   {
      if(cur->mode == MATCH_IWAD) // is a -iwad matcher?
      {
         if(!strcasecmp(cur->name, tmpname)) // should be an exact match
         {
            for(int i = 0; i < 3; ++i) // try each path in order
            {
               if(!cur->iwadpaths[i]) // no more valid paths to try
                  break;

               if(**(cur->iwadpaths[i]) != '\0')
                  return *(cur->iwadpaths[i]); // got one!
            }
         }
      }
      ++cur; // try the next entry
   }

   return NULL; // nothing was found
}

// macros for CheckIWAD

#define isMapExMy(name) \
   ((name)[0] == 'E' && (name)[2] == 'M' && !(name)[4])

#define isMapMAPxy(name) \
   ((name)[0] == 'M' && (name)[1] == 'A' && (name)[2] == 'P' && !(name)[5])

#define isCAV(name) \
   ((name)[0] == 'C' && (name)[1] == 'A' && (name)[2] == 'V' && !(name)[7])

#define isMC(name) \
   ((name)[0] == 'M' && (name)[1] == 'C' && !(name)[3])

// haleyjd 10/13/05: special stuff for FreeDOOM :)
bool freedoom = false;

//
// D_checkIWAD
//
// Verify a file is indeed tagged as an IWAD
// Scan its lumps for levelnames and return gamemode as indicated
// Detect missing wolf levels in DOOM II
//
// The filename to check is passed in iwadname, the gamemode detected is
// returned in gmode, hassec returns the presence of secret levels
//
// jff 4/19/98 Add routine to test IWAD for validity and determine
// the gamemode from it. Also note if DOOM II, whether secret levels exist
//
// killough 11/98:
// Rewritten to considerably simplify
// Added Final Doom support (thanks to Joel Murdoch)
//
// joel 10/17/98 Final DOOM fix: added gmission
//
static void D_checkIWAD(const char *iwadname, 
                        GameMode_t *gmode, GameMission_t *gmission, 
                        bool *hassec)
{
   FILE *fp;
   int ud = 0, rg = 0, sw = 0, cm = 0, sc = 0, tnt = 0, plut = 0, hacx = 0;
   int raven = 0, sosr = 0;
   filelump_t lump;
   wadinfo_t header;
   const char *n = lump.name;

   if(!(fp = fopen(iwadname, "rb")))
      I_Error("Can't open IWAD: %s\n", iwadname);

   // read IWAD header
   if(fread(&header, sizeof header, 1, fp) < 1 ||
      strncmp(header.identification, "IWAD", 4))
   {
      // haleyjd 06/06/09: do not error out here, due to some bad tools
      // resetting peoples' IWADs to PWADs. Only error if it is also 
      // not a PWAD.
      if(strncmp(header.identification, "PWAD", 4))
         I_Error("IWAD or PWAD tag not present: %s\n", iwadname);
      else
         usermsg("Warning: IWAD tag not present: %s\n", iwadname);
   }

   fseek(fp, SwapLong(header.infotableofs), SEEK_SET);

   // Determine game mode from levels present
   // Must be a full set for whichever mode is present
   // Lack of wolf-3d levels also detected here

   header.numlumps = SwapLong(header.numlumps);

   for(; header.numlumps; header.numlumps--)
   {
      if(!fread(&lump, sizeof(lump), 1, fp))
         break;

      if(isMapExMy(n))
      {
         if(n[1] == '4')
            ++ud;
         else if(n[1] == '3' || n[1] == '2')
            ++rg;
         else if(n[1] == '1')
            ++sw;
      }
      else if(isMapMAPxy(n))
      {
         ++cm;
         sc += (n[3] == '3' && (n[4] == '1' || n[4] == '2'));
      }
      else if(isCAV(n))
         ++tnt;
      else if(isMC(n))
         ++plut;
      else if(!strncmp(n, "ADVISOR",  7) || 
              !strncmp(n, "TINTTAB",  7) || 
              !strncmp(n, "SNDCURVE", 8))
      {
         ++raven;
      }
      else if(!strncmp(n, "EXTENDED", 8))
         ++sosr;
      else if(!strncmp(n, "FREEDOOM", 8))
         freedoom = true;
      else if(!strncmp(n, "HACX-R", 6))
         ++hacx;
   }

   fclose(fp);

   *hassec = false;

   // haleyjd 10/09/05: "Raven mode" detection
   if(raven == 3)
   {
      // TODO: Hexen
      *gmission = heretic;

      if(rg >= 18)
      {
         // require both E4 and EXTENDED lump for SoSR
         if(sosr && ud >= 9)
            *gmission = hticsosr;
         *gmode = hereticreg;
      }
      else if(sw >= 9)
         *gmode = hereticsw;
      else
         *gmode = indetermined;
   }
   else
   {
      *gmission = doom;

      if(cm >= 30 || (cm && !rg))
      {
         if(freedoom) // FreeDoom is meant to be Doom II, not TNT
            *gmission = doom2;
         else
         {
            if(tnt >= 4)
               *gmission = pack_tnt;
            else if(plut >= 8)
               *gmission = pack_plut;
            else if(hacx)
               *gmission = pack_hacx;
            else
               *gmission = doom2;
         }
         *hassec = (sc >= 2) || hacx;
         *gmode = commercial;
      }
      else if(ud >= 9)
         *gmode = retail;
      else if(rg >= 18)
         *gmode = registered;
      else if(sw >= 9)
         *gmode = shareware;
      else
         *gmode = indetermined;
   }
}

//
// WadFileStatus
//
// jff 4/19/98 Add routine to check a pathname for existence as
// a file or directory. If neither append .wad and check if it
// exists as a file then. Else return non-existent.
//
static bool WadFileStatus(char *filename, bool *isdir)
{
   struct stat sbuf;
   int i;

   *isdir = false;                //default is directory to false
   if(!filename || !*filename)    //if path NULL or empty, doesn't exist
      return false;

   if(!stat(filename, &sbuf))      //check for existence
   {
      *isdir = S_ISDIR(sbuf.st_mode); //if it does, set whether a dir or not
      return true;                    //return does exist
   }

   i = strlen(filename);          //get length of path
   if(i >= 4)
   {
      if(!strncasecmp(filename + i - 4, ".wad", 4))
         return false;            //if already ends in .wad, not found
   }

   strcat(filename,".wad");       //try it with .wad added
   if(!stat(filename, &sbuf))     //if it exists then
   {
      if(S_ISDIR(sbuf.st_mode))   //but is a dir, then say we didn't find it
         return false;
      return true;                //otherwise return file found, w/ .wad added
   }
   filename[i] = 0;               //remove .wad
   return false;                  //and report doesn't exist
}

// jff 4/19/98 list of standard IWAD names
static const char *const standard_iwads[]=
{
   // Official IWADs
   "/doom2.wad",     // DOOM II
   "/doom2f.wad",    // DOOM II, French Version
   "/plutonia.wad",  // Final DOOM: Plutonia
   "/tnt.wad",       // Final DOOM: TNT
   "/doom.wad",      // Registered/Ultimate DOOM
   "/doomu.wad",     // CPhipps - allow doomu.wad
   "/doom1.wad",     // Shareware DOOM
   "/heretic.wad",   // Heretic  -- haleyjd 10/10/05
   "/heretic1.wad",  // Shareware Heretic

   // Unofficial IWADs
   "/freedoom.wad",  // Freedoom                -- haleyjd 01/31/03
   "/freedoomu.wad", // "Ultimate" Freedoom     -- haleyjd 03/07/10
   "/freedoom1.wad", // Freedoom "Demo"         -- haleyjd 03/07/10
   "/freedm.wad",    // FreeDM IWAD             -- haleyjd 08/28/11
   "/hacx.wad",      // HACX standalone version -- haleyjd 08/19/09
};

static const int nstandard_iwads = sizeof standard_iwads/sizeof*standard_iwads;

//
// D_findIWADFile
//
// Search in all the usual places until an IWAD is found.
//
// The global baseiwad contains either a full IWAD file specification
// or a directory to look for an IWAD in, or the name of the IWAD desired.
//
// The global standard_iwads lists the standard IWAD names
//
// The result of search is returned in baseiwad, or set blank if none found
//
// IWAD search algorithm:
//
// Set customiwad blank
// If -iwad present set baseiwad to normalized path from -iwad parameter
//  If baseiwad is an existing file, thats it
//  If baseiwad is an existing dir, try appending all standard iwads
//  If haven't found it, and no : or / is in baseiwad,
//   append .wad if missing and set customiwad to baseiwad
//
// Look in . for customiwad if set, else all standard iwads
//
// Look in DoomExeDir. for customiwad if set, else all standard iwads
//
// If $DOOMWADDIR is an existing file
//  If customiwad is not set, thats it
//  else replace filename with customiwad, if exists thats it
// If $DOOMWADDIR is existing dir, try customiwad if set, else standard iwads
//
// If $HOME is an existing file
//  If customiwad is not set, thats it
//  else replace filename with customiwad, if exists thats it
// If $HOME is an existing dir, try customiwad if set, else standard iwads
//
// IWAD not found
//
// jff 4/19/98 Add routine to search for a standard or custom IWAD in one
// of the standard places. Returns a blank string if not found.
//
// killough 11/98: simplified, removed error-prone cut-n-pasted code
//
static char *D_findIWADFile()
{
   static const char *envvars[] = { "DOOMWADDIR", "HOME" };
   static char *iwad = NULL;
   char *customiwad = NULL;
   char *gameiwad = NULL;
   bool isdir = false;
   int i, j;
   char *p;
   const char *basename = NULL;

   //jff 3/24/98 get -iwad parm if specified else use .
   if((i = M_CheckParm("-iwad")) && i < myargc - 1)
      basename = myargv[i + 1];
   else
      basename = G_GFSCheckIWAD(); // haleyjd 04/16/03: GFS support

   // haleyjd 08/19/07: if -game was used and neither -iwad nor a GFS iwad
   // specification was used, start off by trying base/game/game.wad
   if(gamepathset && !basename)
   {
      qstring tempGameIWAD;
      
      tempGameIWAD = basegamepath;
      tempGameIWAD.pathConcatenate(myargv[gamepathparm]);
      tempGameIWAD.addDefaultExtension(".wad");
      gameiwad = tempGameIWAD.duplicateAuto();

      if(!access(gameiwad, R_OK)) // only if the file exists do we try to use it.
         basename = gameiwad;
      else                        
      {
         // haleyjd 12/31/10: base/game/game.wad doesn't exist;
         // try matching against appropriate configured IWAD path(s)
         char *cfgpath = D_IWADPathForGame(myargv[gamepathparm]);
         if(cfgpath && !access(cfgpath, R_OK))
            basename = cfgpath;
      }
   }
      
   //jff 3/24/98 get -iwad parm if specified else use .
   if(basename)
   {
      baseiwad = estrdup(basename);
      M_NormalizeSlashes(baseiwad);

      iwad = ecalloc(char *, 1, strlen(baseiwad) + 1024);
      strcpy(iwad, baseiwad);
      
      if(WadFileStatus(iwad, &isdir))
      {
         if(!isdir)
            return iwad;
         else
         {
            for(i = 0; i < nstandard_iwads; i++)
            {
               int n = strlen(iwad);
               strcat(iwad, standard_iwads[i]);
               if(WadFileStatus(iwad, &isdir) && !isdir)
                  return iwad;
               iwad[n] = 0; // reset iwad length to former
            }
         }
      }
      else if(!strchr(iwad, ':') && !strchr(iwad, '/') && !strchr(iwad, '\\'))
      {
         M_StringAlloca(&customiwad, 1, 8, iwad);
         M_AddDefaultExtension(strcat(strcpy(customiwad, "/"), iwad), ".wad");
         M_NormalizeSlashes(customiwad);
      }
   }
   else if(!gamepathset) // try wad picker
   {
      const char *name = D_doIWADMenu();
      if(name && *name)
      {
         baseiwad = estrdup(name);
         M_NormalizeSlashes(baseiwad);
         return baseiwad;
      }
   }

   for(j = 0; j < (gamepathset ? 3 : 2); j++)
   {
      switch(j)
      {
      case 0:
      case 1:
         if(iwad)
            efree(iwad);
         iwad = ecalloc(char *, 1, strlen(D_DoomExeDir()) + 1024);
         strcpy(iwad, j ? D_DoomExeDir() : ".");
         break;
      case 2:
         // haleyjd: try basegamepath too when -game was used
         if(iwad)
            efree(iwad);
         iwad = ecalloc(char *, 1, strlen(basegamepath) + 1024);
         strcpy(iwad, basegamepath);
         break;
      }

      M_NormalizeSlashes(iwad);

       // sf: only show 'looking in' for devparm
      if(devparm)
         printf("Looking in %s\n",iwad);   // killough 8/8/98

      if(customiwad)
      {
         strcat(iwad, customiwad);
         if(WadFileStatus(iwad, &isdir) && !isdir)
            return iwad;
      }
      else
      {
         for(i = 0; i < nstandard_iwads; i++)
         {
            int n = strlen(iwad);
            strcat(iwad, standard_iwads[i]);
            if(WadFileStatus(iwad, &isdir) && !isdir)
               return iwad;
            iwad[n] = 0; // reset iwad length to former
         }
      }
   }

   // haleyjd 12/31/10: Try finding a match amongst configured IWAD paths
   if(customiwad)
   {
      char *cfgpath = D_IWADPathForIWADParam(customiwad);
      if(cfgpath && !access(cfgpath, R_OK))
      {
         if(iwad)
            efree(iwad);
         iwad = estrdup(cfgpath);
         return iwad;
      }
   }

   // haleyjd 01/01/11: support for DOOMWADPATH
   D_parseDoomWadPath();

   if(doomwadpaths.getLength()) // If at least one path is specified...
   {
      if(customiwad) // -iwad was used with a file name?
      {
         if(iwad)
            efree(iwad);
         if((iwad = D_FindInDoomWadPath(customiwad, ".wad")))
            return iwad;
      }
      else
      {
         // Try all the standard iwad names in the normal order
         for(i = 0; i < nstandard_iwads; i++)
         {
            if(iwad)
               efree(iwad);
            if((iwad = D_FindInDoomWadPath(standard_iwads[i], ".wad")))
               return iwad;
         }
      }
   }

   for(i = 0; i < sizeof envvars / sizeof *envvars; i++)
   {
      if((p = getenv(envvars[i])))
      {
         if(iwad)
            efree(iwad);
         iwad = ecalloc(char *, 1, sizeof(p) + 1024);
         M_NormalizeSlashes(strcpy(iwad, p));
         if(WadFileStatus(iwad, &isdir))
         {
            if(!isdir)
            {
               if(!customiwad)
                  return printf("Looking for %s\n", iwad), iwad; // killough 8/8/98
               else if((p = strrchr(iwad, '/')) || (p = strrchr(iwad, '\\')))
               {
                  *p=0;
                  strcat(iwad, customiwad);
                  printf("Looking for %s\n",iwad);  // killough 8/8/98
                  if(WadFileStatus(iwad, &isdir) && !isdir)
                     return iwad;
               }
            }
            else
            {
               if(devparm)       // sf: devparm only
                  printf("Looking in %s\n",iwad);  // killough 8/8/98
               if(customiwad)
               {
                  if(WadFileStatus(strcat(iwad, customiwad), &isdir) && !isdir)
                     return iwad;
               }
               else
               {
                  for(i = 0; i < nstandard_iwads; i++)
                  {
                     int n = strlen(iwad);
                     strcat(iwad, standard_iwads[i]);
                     if(WadFileStatus(iwad, &isdir) && !isdir)
                        return iwad;
                     iwad[n] = 0; // reset iwad length to former
                  }
               } // end else (!*customiwad)
            } // end else (isdir)
         } // end if(WadFileStatus(...))
      } // end if((p = getenv(...)))
   } // end for

   // haleyjd 01/17/11: be sure iwad return string is valid...
   if(!iwad)
      iwad = emalloc(char *, 1);

   *iwad = 0;
   return iwad;
}

//
// D_loadResourceWad
//
// haleyjd 03/10/03: moved eternity.wad loading to this function
//
static void D_loadResourceWad()
{
   char *filestr = NULL;
   size_t len = M_StringAlloca(&filestr, 1, 20, basegamepath);

   psnprintf(filestr, len, "%s/eternity.wad", basegamepath);

   // haleyjd 08/19/07: if not found, fall back to base/doom/eternity.wad
   if(access(filestr, R_OK))
      psnprintf(filestr, len, "%s/doom/eternity.wad", basepath);

   M_NormalizeSlashes(filestr);
   D_AddFile(filestr, lumpinfo_t::ns_global, NULL, 0, 0);

   modifiedgame = false; // reset, ignoring smmu.wad etc.
}

//
// D_identifyDisk
//
// haleyjd 05/31/10: IdentifyVersion subroutine for dealing with disk files.
//
static void D_identifyDisk(void)
{
   GameMode_t    gamemode;
   GameMission_t gamemission;

   printf("IWAD found: %s\n", diskiwad.name);

   // haleyjd: hardcoded for now
   if(disktype == DISK_DOOM2)
   {
      gamemode      = commercial;
      gamemission   = pack_disk;
      haswolflevels = true;
   }
   else
   {
      gamemode    = retail;
      gamemission = doom;
   }

   // setup gameModeInfo
   D_SetGameModeInfo(gamemode, gamemission);

   // haleyjd: load metadata from diskfile
   D_DiskMetaData();

   // set and display version name
   D_SetGameName(NULL);

   // initialize game/data paths
   D_InitPaths();

   // haleyjd 03/10/03: add eternity.wad before the IWAD, at request of
   // fraggle -- this allows better compatibility with new IWADs
   D_loadResourceWad();

   // load disk IWAD
   D_LoadDiskFileIWAD();

   // haleyjd: load disk file pwad here, if one was specified
   if(diskpwad)
      D_LoadDiskFilePWAD();

   // 12/24/11: check for game folder hi-def music
   D_CheckGameMusic();

   // done with the diskfile structure
   D_CloseDiskFile(diskfile, false);
   diskfile = NULL;
}

//
// D_identifyIWAD
//
// haleyjd 05/31/10: IdentifyVersion subroutine for dealing with normal IWADs.
//
static void D_identifyIWAD(void)
{
   char *iwad;
   GameMode_t    gamemode;
   GameMission_t gamemission;

   iwad = D_findIWADFile();

   if(iwad && *iwad)
   {
      printf("IWAD found: %s\n", iwad); //jff 4/20/98 print only if found

      // joel 10/16/98 gamemission added
      D_checkIWAD(iwad, &gamemode, &gamemission, &haswolflevels);

      // setup GameModeInfo
      D_SetGameModeInfo(gamemode, gamemission);

      // set and display version name
      D_SetGameName(iwad);

      // initialize game/data paths
      D_InitPaths();

      // haleyjd 03/10/03: add eternity.wad before the IWAD, at request of
      // fraggle -- this allows better compatibility with new IWADs
      D_loadResourceWad();

      D_AddFile(iwad, lumpinfo_t::ns_global, NULL, 0, 0);

      // 12/24/11: check for game folder hi-def music
      D_CheckGameMusic();

      // done with iwad string
      efree(iwad);
   }
   else
   {
      // haleyjd 08/20/07: improved error message for n00bs
      I_Error("\nIWAD not found!\n"
              "To specify an IWAD, try one of the following:\n"
              "* Configure IWAD file paths in base/system.cfg\n"
              "* Use -iwad\n"
              "* Set the DOOMWADDIR or DOOMWADPATH environment variables.\n"
              "* Place an IWAD in the working directory.\n"
              "* Place an IWAD file under the appropriate game folder of\n"
              "  the base directory and use the -game parameter.\n");
   }
}

//
// D_IdentifyVersion
//
// Set the location of the defaults file and the savegame root
// Locate and validate an IWAD file
// Determine gamemode from the IWAD
//
// supports IWADs with custom names. Also allows the -iwad parameter to
// specify which iwad is being searched for if several exist in one dir.
// The -iwad parm may specify:
//
// 1) a specific pathname, which must exist (.wad optional)
// 2) or a directory, which must contain a standard IWAD,
// 3) or a filename, which must be found in one of the standard places:
//   a) current dir,
//   b) exe dir
//   c) $DOOMWADDIR
//   d) or $HOME
//
// jff 4/19/98 rewritten to use a more advanced search algorithm
//
void D_IdentifyVersion()
{
   // haleyjd 05/28/10: check for -disk parameter
   D_CheckDiskFileParm();

   // if we loaded one, try finding an IWAD in it
   if(havediskfile)
      D_FindDiskFileIWAD();

   // locate the IWAD and determine game mode from it
   if(havediskiwad)
      D_identifyDisk();
   else
      D_identifyIWAD();
}

// EOF


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
//      Handles WAD file header, directory, lump I/O.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"
#include "doomstat.h"
#include "d_io.h"  // SoM 3/12/2002: moved unistd stuff into d_io.h

#include "c_io.h"
#include "m_argv.h"
#include "m_hash.h"
#include "m_misc.h"
#include "m_swap.h"
#include "p_skin.h"
#include "s_sound.h"
#include "v_misc.h"
#include "w_wad.h"
#include "w_hacks.h"

//
// GLOBALS
//

// Location of each lump on disk.
WadDirectory wGlobalDir;

int WadDirectory::source;            // haleyjd 03/18/10: next source ID# to use
int WadDirectory::IWADSource   = -1; // sf: the handle of the main iwad
int WadDirectory::ResWADSource = -1; // haleyjd: track handle of first wad added

//
// haleyjd 07/12/07: structure for transparently manipulating lumps of
// different types
//

typedef struct lumptype_s
{
   size_t (*readLump)(lumpinfo_t *, void *, size_t);
} lumptype_t;

static size_t W_DirectReadLump(lumpinfo_t *, void *, size_t);
static size_t W_MemoryReadLump(lumpinfo_t *, void *, size_t);

static lumptype_t LumpHandlers[lumpinfo_t::lump_numtypes] =
{
   // direct lump
   {
      W_DirectReadLump,
   },

   // memory lump
   {
      W_MemoryReadLump,
   },
};

//
// LUMP BASED ROUTINES.
//

void D_NewWadLumps(FILE *handle);

//
// W_addInfoPtr
//
// haleyjd 06/06/10: I need to track all lumpinfo_t allocations that are
// added to a waddir_t's lumpinfo directory, or else these allocations get
// orphaned when freeing a private wad directory. Oops! ;)
//
void WadDirectory::AddInfoPtr(lumpinfo_t *infoptr)
{
   // reallocate if necessary
   if(numallocs >= numallocsa)
   {
      numallocsa = numallocsa ? numallocsa * 2 : 32;

      infoptrs = (lumpinfo_t **)(realloc(infoptrs, numallocsa * sizeof(lumpinfo_t *)));
   }
   
   // add it
   infoptrs[numallocs] = infoptr;
   ++numallocs;
}

//
// WadDirectory::OpenFile
//
// haleyjd 04/06/11: For normal wad files, the file needs to be found and 
// opened.
//
WadDirectory::openwad_t WadDirectory::OpenFile(const char *name, int filetype)
{
   openwad_t openData;
   char *filename = NULL;
   memset(&openData, 0, sizeof(openData));

   // haleyjd 03/18/10: filename fix
   M_StringAlloca(&filename, 2, 1, name, ".wad");
   strcpy(filename, name);

   // Only normal wad files allow this
   if(filetype == ADDWADFILE)
      M_NormalizeSlashes(M_AddDefaultExtension(filename, ".wad"));  // killough 11/98
   
   // open the file and add to directory
   if((openData.handle = fopen(filename, "rb")) == NULL)
   {
      // If a private file, issue an error message immediately; we do not consider
      // .lmp files.
      if(filetype == ADDPRIVATE)
      {
         C_Printf(FC_ERROR "Couldn't open %s\n", filename);
         openData.filename = filename;
         openData.error    = true;
         openData.errorRet = false;
         return openData;
      }

      if(strlen(name) > 4 && !strcasecmp(name + strlen(name) - 4 , ".lmp" ))
      {
         openData.filename = filename;
         openData.error    = true;
         openData.errorRet = false;
         return openData; // sf: no errors
      }

      // killough 11/98: allow .lmp extension if none existed before
      M_NormalizeSlashes(M_AddDefaultExtension(strcpy(filename, name), ".lmp"));
      
      if((openData.handle = fopen(filename, "rb")) == NULL)
      {
         if(in_textmode)
            I_Error("Error: couldn't open %s\n", name);  // killough
         else
         {
            C_Printf(FC_ERROR "Couldn't open %s\n", name);
            openData.filename = filename;
            openData.error    = true;
            openData.errorRet = true;
            return openData; // error
         }
      }
   }

   openData.filename = filename;
   openData.error = openData.errorRet = false;
   return openData;
}

//
// W_AddFile
//
// All files are optional, but at least one file must be found (PWAD, if all 
// required lumps are present).
// Files with a .wad extension are wadlink files with multiple lumps.
// Other files are single lumps with the base filename for the lump name.
//
// Reload hack removed by Lee Killough
// killough 1/31/98: static, const
//
bool WadDirectory::AddFile(const char *name, int li_namespace, int filetype,
                              FILE *file, size_t baseoffset)
{
   wadinfo_t    header;
   openwad_t    openData;
   lumpinfo_t  *lump_p;
   unsigned     i;
   int          length;
   int          startlump;
   filelump_t  *fileinfo, *fileinfo2free = NULL; //killough
   filelump_t   singleinfo;
   lumpinfo_t  *newlumps;
   bool         isWad;     // haleyjd 05/23/04
   
   // haleyjd 04/07/11
   HashData     wadHash  = HashData(HashData::SHA1);
   bool         showHash = false;
   bool         doHacks  = false;

   switch(filetype)
   {
   case ADDWADFILE: // Normal file being added from the command line or a script
      doHacks = true;
      // fall through
   case ADDPRIVATE: // WAD being loaded into a private directory object
      openData = OpenFile(name, filetype);
      if(openData.error)
         return openData.errorRet; // return immediately if an error occurred
      break;
   case ADDSUBFILE: // WAD file contained inside a larger disk file
      doHacks = false;
      openData.filename = name;
      openData.handle   = file;
      break;
   }
  
   if(filetype != ADDPRIVATE && this->ispublic && in_textmode)
      printf(" adding %s\n", openData.filename);   // killough 8/8/98
   startlump = this->numlumps;

   // haleyjd: seek to baseoffset first when loading a subfile
   if(filetype == ADDSUBFILE)
      fseek(openData.handle, baseoffset, SEEK_SET);

   // -nowadhacks disables all wad directory hacks, in case of unforeseen
   // compatibility problems that would last until the next release
   if(M_CheckParm("-nowadhacks"))
      doHacks = false;

   // -showhashes enables display of the computed SHA-1 hash used to apply
   // wad directory hacks. Quite useful for when a new hack needs to be added.
   if(M_CheckParm("-showhashes"))
      showHash = true;

   // killough:
   if(filetype == ADDWADFILE && // only when adding normal wad files
      (strlen(openData.filename) <= 4 || 
       strcasecmp(openData.filename+strlen(openData.filename)-4, ".wad" )))
   {
      // single lump file
      doHacks = false;
      isWad = false; // haleyjd 05/23/04
      fileinfo = &singleinfo;
      singleinfo.filepos = 0;
      singleinfo.size = SwapLong(M_FileLength(openData.handle));
      M_ExtractFileBase(openData.filename, singleinfo.name);
      this->numlumps++;
   }
   else
   {
      // WAD file
      isWad = true; // haleyjd 05/23/04
      
      if(fread(&header, sizeof(header), 1, openData.handle) < 1)
      {
         if(filetype == ADDPRIVATE)
         {
            fclose(openData.handle);
            C_Printf(FC_ERROR "Failed reading header for wad file %s\n", openData.filename);
            return false;
         }
         else
            I_Error("Failed reading header for wad file %s\n", openData.filename);
      }

      // Feed the wad header data into the hash computation
      if(doHacks || showHash)
         wadHash.addData((const uint8_t *)&header, (uint32_t)sizeof(header));
      
      if(strncmp(header.identification, "IWAD", 4) && 
         strncmp(header.identification, "PWAD", 4))
      {
         if(filetype == ADDPRIVATE) // Error is tolerated for private files
         {
            fclose(openData.handle);
            C_Printf(FC_ERROR "Wad file %s doesn't have IWAD or PWAD id\n", openData.filename);
            return false;
         }
         else
            I_Error("Wad file %s doesn't have IWAD or PWAD id\n", openData.filename);
      }
      
      header.numlumps     = SwapLong(header.numlumps);
      header.infotableofs = SwapLong(header.infotableofs);
      
      length = header.numlumps * sizeof(filelump_t);
      fileinfo2free = fileinfo = (filelump_t *)(malloc(length)); // killough
      
      long info_offset = (long)(header.infotableofs);
      // subfile wads may exist at a positive base offset in the container file
      if(filetype == ADDSUBFILE) 
         info_offset += (long)baseoffset;
      fseek(openData.handle, info_offset, SEEK_SET);

      if(fread(fileinfo, length, 1, openData.handle) < 1)
      {
         if(filetype == ADDPRIVATE) // Error is tolerated for private files
         {
            fclose(openData.handle);
            free(fileinfo2free);
            C_Printf(FC_ERROR "Failed reading directory for wad file %s\n", openData.filename);
            return false;
         }
         else
            I_Error("Failed reading directory for wad file %s\n", openData.filename);
      }

      // Feed the wad directory into the hash computation, wrap it up, and if requested,
      // output it to the console.
      if(doHacks || showHash)
      {
         wadHash.addData((const uint8_t *)fileinfo, (uint32_t)length);
         wadHash.wrapUp();
         if(in_textmode && showHash)
            printf("\thash = %s\n", wadHash.digestToString());
         // haleyjd 04/08/11: apply wad directory hacks as needed
         if(doHacks)
            W_CheckDirectoryHacks(wadHash, fileinfo, header.numlumps);
      }
      
      this->numlumps += header.numlumps;
   }
   
   // Fill in lumpinfo
   this->lumpinfo = (lumpinfo_t **)(realloc(this->lumpinfo, this->numlumps * sizeof(lumpinfo_t *)));

   // space for new lumps
   newlumps = (lumpinfo_t *)(malloc((this->numlumps - startlump) * sizeof(lumpinfo_t)));
   lump_p   = newlumps;

   // haleyjd: keep track of this allocation of lumps
   AddInfoPtr(newlumps);
   
   // update IWAD handle? 
   // haleyjd: Must be a public wad file.
   if(filetype != ADDPRIVATE && isWad && this->ispublic)
   {
      // haleyjd 06/21/04: track handle of first wad added also
      if(ResWADSource == -1)
         ResWADSource = source;

      // haleyjd 07/13/09: only track the first IWAD found
      if(IWADSource < 0 && !strncmp(header.identification, "IWAD", 4))
         IWADSource = source;
   }
  
   for(i = startlump; i < (unsigned int)this->numlumps; i++, lump_p++, fileinfo++)
   {
      this->lumpinfo[i] = lump_p;
      lump_p->type      = lumpinfo_t::lump_direct; // haleyjd
      lump_p->file      = openData.handle;
      lump_p->position  = (size_t)(SwapLong(fileinfo->filepos));
      lump_p->size      = (size_t)(SwapLong(fileinfo->size));
      lump_p->source    = source; // haleyjd

      // for subfiles, add baseoffset to the lump offset
      if(filetype == ADDSUBFILE)
         lump_p->position += baseoffset;
      
      lump_p->data = lump_p->cache = NULL;         // killough 1/31/98
      lump_p->li_namespace = li_namespace;         // killough 4/17/98
      
      memset(lump_p->name, 0, 9);
      strncpy(lump_p->name, fileinfo->name, 8);
   }

   // haleyjd: increment source
   ++source;
   
   if(fileinfo2free)
      free(fileinfo2free); // killough
   
   if(filetype == ADDPRIVATE)
      return true; // no error (note opposite return value semantics 9_9)

   if(this->ispublic)
      D_NewWadLumps(openData.handle);
   
   return false; // no error
}

// jff 1/23/98 Create routines to reorder the master directory
// putting all flats into one marked block, and all sprites into another.
// This will allow loading of sprites and flats from a PWAD with no
// other changes to code, particularly fast hashes of the lumps.
//
// killough 1/24/98 modified routines to be a little faster and smaller

int WadDirectory::IsMarker(const char *marker, const char *name)
{
   return !strncasecmp(name, marker, 8) ||
      (*name == *marker && !strncasecmp(name+1, marker, 7));
}

//
// W_CoalesceMarkedResource
//
// killough 4/17/98: add namespace tags
//
void WadDirectory::CoalesceMarkedResource(const char *start_marker,
                                          const char *end_marker, 
                                          int li_namespace)
{
   lumpinfo_t **marked = ecalloc(lumpinfo_t **, sizeof(*marked), this->numlumps);
   size_t i, num_marked = 0, num_unmarked = 0;
   int is_marked = 0, mark_end = 0;
   lumpinfo_t *lump;
  
   for(i = 0; i < (unsigned)this->numlumps; i++)
   {
      lump = this->lumpinfo[i];
      
      // If this is the first start marker, add start marker to marked lumps
      if(IsMarker(start_marker, lump->name))       // start marker found
      {
         if(!num_marked)
         {
            marked[0] = lump;
            marked[0]->li_namespace = lumpinfo_t::ns_global; // killough 4/17/98
            num_marked = 1;
         }
         is_marked = 1;                            // start marking lumps
      }
      else if(IsMarker(end_marker, lump->name))    // end marker found
      {
         mark_end = 1;                           // add end marker below
         is_marked = 0;                          // stop marking lumps
      }
      else if(is_marked || lump->li_namespace == li_namespace)
      {
         // if we are marking lumps,
         // move lump to marked list
         // sf: check for namespace already set

         // sf 26/10/99:
         // ignore sprite lumps smaller than 8 bytes (the smallest possible)
         // in size -- this was used by some dmadds wads
         // as an 'empty' graphics resource
         
         // SoM: Ignore marker lumps inside F_START and F_END
         if((li_namespace == lumpinfo_t::ns_sprites && lump->size > 8) ||
            (li_namespace == lumpinfo_t::ns_flats && lump->size > 0) ||
            (li_namespace != lumpinfo_t::ns_sprites && li_namespace != lumpinfo_t::ns_flats))
         {
            marked[num_marked] = lump;
            marked[num_marked]->li_namespace = li_namespace;
            num_marked++;
         }
      }
      else
      {
         this->lumpinfo[num_unmarked] = lump;       // else move down THIS list
         num_unmarked++;
      }
   }
   
   // Append marked list to end of unmarked list
   memcpy(this->lumpinfo + num_unmarked, marked, num_marked * sizeof(lumpinfo_t *));

   free(marked);                                    // free marked list
   
   this->numlumps = num_unmarked + num_marked;      // new total number of lumps
   
   if(mark_end)                                     // add end marker
   {
      lumpinfo_t *newlump = ecalloc(lumpinfo_t *, 1, sizeof(lumpinfo_t));
      int lNumLumps = this->numlumps;
      AddInfoPtr(newlump); // haleyjd: track it
      this->lumpinfo[lNumLumps] = newlump;
      this->lumpinfo[lNumLumps]->size = 0;  // killough 3/20/98: force size to be 0
      this->lumpinfo[lNumLumps]->li_namespace = lumpinfo_t::ns_global;   // killough 4/17/98
      strncpy(this->lumpinfo[lNumLumps]->name, end_marker, 8);
      this->numlumps++;
   }
}

//
// W_LumpNameHash
//
// Hash function used for lump names.
// Must be mod'ed with table size.
// Can be used for any 8-character names.
// by Lee Killough
//
unsigned int WadDirectory::LumpNameHash(const char *s)
{
  unsigned int hash;
  (void) ((hash =        toupper(s[0]), s[1]) &&
          (hash = hash*3+toupper(s[1]), s[2]) &&
          (hash = hash*2+toupper(s[2]), s[3]) &&
          (hash = hash*2+toupper(s[3]), s[4]) &&
          (hash = hash*2+toupper(s[4]), s[5]) &&
          (hash = hash*2+toupper(s[5]), s[6]) &&
          (hash = hash*2+toupper(s[6]),
           hash = hash*2+toupper(s[7]))
         );
  return hash;
}

//
// W_CheckNumForName
// Returns -1 if name not found.
//
// Rewritten by Lee Killough to use hash table for performance. Significantly
// cuts down on time -- increases Doom performance over 300%. This is the
// single most important optimization of the original Doom sources, because
// lump name lookup is used so often, and the original Doom used a sequential
// search. For large wads with > 1000 lumps this meant an average of over
// 500 were probed during every search. Now the average is under 2 probes per
// search. There is no significant benefit to packing the names into longwords
// with this new hashing algorithm, because the work to do the packing is
// just as much work as simply doing the string comparisons with the new
// algorithm, which minimizes the expected number of comparisons to under 2.
//
// killough 4/17/98: add namespace parameter to prevent collisions
// between different resources such as flats, sprites, colormaps
//
// haleyjd 03/01/09: added InDir version.
//
int WadDirectory::CheckNumForName(const char *name, int li_namespace)
{
   // Hash function maps the name to one of possibly numlump chains.
   // It has been tuned so that the average chain length never exceeds 2.
   
   register int i = lumpinfo[LumpNameHash(name) % (unsigned int)numlumps]->index;

   // We search along the chain until end, looking for case-insensitive
   // matches which also match a namespace tag. Separate hash tables are
   // not used for each namespace, because the performance benefit is not
   // worth the overhead, considering namespace collisions are rare in
   // Doom wads.

   while(i >= 0 && (strncasecmp(lumpinfo[i]->name, name, 8) ||
         lumpinfo[i]->li_namespace != li_namespace))
      i = lumpinfo[i]->next;

   // Return the matching lump, or -1 if none found.   
   return i;
}

//
// W_CheckNumForName
//
// haleyjd: Now a global directory convenience routine.
//
int W_CheckNumForName(register const char *name)
{
   return wGlobalDir.CheckNumForName(name);
}

//
// W_CheckNumForNameNS
//
// haleyjd: Separated from W_CheckNumForName. Looks in a specific namespace.
//
int W_CheckNumForNameNS(register const char *name, register int li_namespace)
{
   return wGlobalDir.CheckNumForName(name, li_namespace);
}

//
// W_CheckNumForNameNSG
//
// haleyjd 02/15/10: Looks in specified namespace and if not found, then looks
// in the global namespace.
//
int WadDirectory::CheckNumForNameNSG(const char *name, int ns)
{
   int num = -1;
   int curnamespace = ns;

   do
   {
      num = CheckNumForName(name, curnamespace);
   }
   while(num < 0 && curnamespace == ns ? curnamespace = lumpinfo_t::ns_global, 1 : 0);

   return num;
}

//
// W_GetNumForName
//
// Calls W_CheckNumForName, but bombs out if not found.
//
int WadDirectory::GetNumForName(const char* name)     // killough -- const added
{
   int i = CheckNumForName(name);
   if(i == -1)
      I_Error("WadDirectory::GetNumForName: %.8s not found!\n", name); // killough .8 added
   return i;
}

int W_GetNumForName(const char *name)
{
   return wGlobalDir.GetNumForName(name);
}

//
// W_GetLumpNameChainInDir
//
// haleyjd 03/18/10: routine for getting the lumpinfo hash chain for lumps of a
// given name, to replace code segments doing this in several different places.
//
lumpinfo_t *WadDirectory::GetLumpNameChain(const char *name)
{
   return lumpinfo[LumpNameHash(name) % (unsigned int)numlumps];
}

//
// W_GetLumpNameChain
//
// haleyjd 03/18/10: convenience routine to do the above on the global wad
// directory.
//
lumpinfo_t *W_GetLumpNameChain(const char *name)
{
   return wGlobalDir.GetLumpNameChain(name);
}


//
// W_InitLumpHash
//
// killough 1/31/98: Initialize lump hash table
//
void WadDirectory::InitLumpHash()
{
   int i;
   
   for(i = 0; i < numlumps; i++)
   {
      lumpinfo[i]->index     = -1; // mark slots empty
      lumpinfo[i]->selfindex =  i; // haleyjd: record position in array
   }

   // Insert nodes to the beginning of each chain, in first-to-last
   // lump order, so that the last lump of a given name appears first
   // in any chain, observing pwad ordering rules. killough

   for(i = 0; i < numlumps; i++)
   {                                           // hash function:
      unsigned int j;

      j = LumpNameHash(lumpinfo[i]->name) % (unsigned int)numlumps;
      lumpinfo[i]->next = lumpinfo[j]->index;     // Prepend to list
      lumpinfo[j]->index = i;
   }
}

// End of lump hashing -- killough 1/31/98

//
// W_InitResources
//
void WadDirectory::InitResources() // sf
{
   //jff 1/23/98
   // get all the sprites and flats into one marked block each
   // killough 1/24/98: change interface to use M_START/M_END explicitly
   // killough 4/17/98: Add namespace tags to each entry
   
   CoalesceMarkedResource("S_START", "S_END", lumpinfo_t::ns_sprites);
   
   CoalesceMarkedResource("F_START", "F_END", lumpinfo_t::ns_flats);

   // killough 4/4/98: add colormap markers
   CoalesceMarkedResource("C_START", "C_END", lumpinfo_t::ns_colormaps);
   
   // haleyjd 01/12/04: add translation markers
   CoalesceMarkedResource("T_START", "T_END", lumpinfo_t::ns_translations);

   // set up caching
   // sf: caching now done in the lumpinfo_t's
   
   // killough 1/31/98: initialize lump hash table
   InitLumpHash();
}

//
// W_InitMultipleFiles
//
// Pass a null terminated list of files to use.
// All files are optional, but at least one file must be found.
// Files with a .wad extension are idlink files with multiple lumps.
// Other files are single lumps with the base filename for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file overrides earlier ones.
//
void WadDirectory::InitMultipleFiles(wfileadd_t *files)
{
   wfileadd_t *curfile;

   // Basic initialization
   numlumps = 0;
   lumpinfo = NULL;
   ispublic = true;   // Is a public wad directory
   type     = NORMAL; // Not a managed directory
   data     = NULL;   // No special data

   curfile = files;
   
   // open all the files, load headers, and count lumps
   while(curfile->filename)
   {
      // haleyjd 07/11/09: ignore empty filenames
      if((curfile->filename)[0])
      {
         // haleyjd 05/28/10: if there is a non-zero baseoffset, then this is a
         // subfile wad, or a wad contained within a larger file in other words.
         // Open them with a special routine; they can be treated uniformly with
         // other wad files afterward.
         // 04/07/11: Merged AddFile and AddSubFile
         AddFile(curfile->filename, curfile->li_namespace,
                 curfile->baseoffset ? ADDSUBFILE : ADDWADFILE, 
                 curfile->baseoffset ? curfile->f : NULL,
                 curfile->baseoffset);
      }

      ++curfile;
   }
   
   if(!numlumps)
      I_Error("WadDirectory::InitMultipleFiles: no files found\n");
   
   InitResources();
}

int WadDirectory::AddNewFile(const char *filename)
{
   if(AddFile(filename, lumpinfo_t::ns_global, ADDWADFILE))
      return true;
   InitResources();         // reinit lump lookups etc
   return false;
}

int WadDirectory::AddNewPrivateFile(const char *filename)
{
   if(!AddFile(filename, lumpinfo_t::ns_global, ADDPRIVATE))
      return false;

   // there is no resource coalescence on this particular brand of private
   // wad file, so just call W_InitLumpHash.
   InitLumpHash();

   return true;
}

//
// W_LumpLength
//
// Returns the buffer size needed to load the given lump.
//
int WadDirectory::LumpLength(int lump)
{
   if(lump < 0 || lump >= numlumps)
      I_Error("WadDirectory::LumpLength: %i >= numlumps\n", lump);
   return lumpinfo[lump]->size;
}

int W_LumpLength(int lump)
{
   return wGlobalDir.LumpLength(lump);
}

//
// W_ReadLump
//
// Loads the lump into the given buffer, which must be >= W_LumpLength().
//
void WadDirectory::ReadLump(int lump, void *dest, WadLumpLoader *lfmt)
{
   size_t c;
   lumpinfo_t *l;
   
   if(lump < 0 || lump >= numlumps)
      I_Error("WadDirectory::ReadLump: %d >= numlumps\n", lump);

   l = lumpinfo[lump];

   // killough 1/31/98: Reload hack (-wart) removed

   c = LumpHandlers[l->type].readLump(l, dest, l->size);
   if(c < l->size)
   {
      I_Error("WadDirectory::ReadLump: only read %d of %d on lump %d\n", 
              (int)c, (int)l->size, lump);
   }

   // haleyjd 06/26/11: Apply lump formatting/preprocessing if provided
   if(lfmt)
   {
      bool success   = false;
      int  errorMode = lfmt->getErrorMode();

      if(lfmt->verifyData(dest, l->size) && lfmt->formatData(dest, l->size))
         success = true;

      // Lump formatter wants us to handle errors by bombing out?
      if(!success && errorMode == WadLumpLoader::EM_FATAL)
         I_Error("WadDirectory::ReadLump: lump %d is malformed\n", lump);
   }
}

//
// W_ReadLumpHeader
//
// haleyjd 08/30/02: Inspired by DOOM Legacy, this function is for when you
// just need a small piece of data from the beginning of a lump. There's hardly
// any use reading in the whole thing in that case.
//
int WadDirectory::ReadLumpHeader(int lump, void *dest, size_t size)
{
   lumpinfo_t *l;
   void *data;
   
   if(lump < 0 || lump >= numlumps)
      I_Error("WadDirectory::ReadLumpHeader: %d >= numlumps\n", lump);

   l = lumpinfo[lump];

   if(l->size < size || l->size == 0)
      return 0;

   data = CacheLumpNum(lump, PU_CACHE);

   memcpy(dest, data, size);
   
   return size;
}

int W_ReadLumpHeader(int lump, void *dest, size_t size)
{
   return wGlobalDir.ReadLumpHeader(lump, dest, size);
}

//
// W_CacheLumpNum
//
// killough 4/25/98: simplified
//
void *WadDirectory::CacheLumpNum(int lump, int tag, WadLumpLoader *lfmt)
{
   // haleyjd 08/14/02: again, should not be RANGECHECK only
   if(lump < 0 || lump >= numlumps)
      I_Error("WadDirectory::CacheLumpNum: %i >= numlumps\n", lump);
   
   if(!(lumpinfo[lump]->cache))      // read the lump in
   {
      ReadLump(lump, 
               Z_Malloc(LumpLength(lump), tag, &(lumpinfo[lump]->cache)), 
               lfmt);
   }
   else
   {
      // haleyjd: do not lower cache level and cause static users to lose their 
      // data unexpectedly (ie, do not change PU_STATIC into PU_CACHE -- that 
      // must be done using Z_ChangeTag explicitly)
      
      int oldtag = Z_CheckTag(lumpinfo[lump]->cache);

      if(tag < oldtag) 
         Z_ChangeTag(lumpinfo[lump]->cache, tag);
   }
   
   return lumpinfo[lump]->cache;
}

//
// W_CacheLumpName
//
// haleyjd 06/26/11: Restored to status as a method of WadDirectory instead of
// being a macro, as it needs to take an optional parameter now.
//
void *WadDirectory::CacheLumpName(const char *name, int tag, WadLumpLoader *lfmt)
{
   return CacheLumpNum(GetNumForName(name), tag, lfmt);
}

// Predefined lumps removed -- sf

//
// W_LumpCheckSum
//
// sf
// haleyjd 08/27/11: Rewritten to use CRC32 hash algorithm
//
uint32_t W_LumpCheckSum(int lumpnum)
{
   uint8_t  *lump    = (uint8_t *)(wGlobalDir.CacheLumpNum(lumpnum, PU_CACHE));
   uint32_t  lumplen = (uint32_t )(W_LumpLength(lumpnum));

   return HashData(HashData::CRC32, lump, lumplen).getDigestPart(0);
}

//
// W_FreeDirectoryLumps
//
// haleyjd 06/26/09
// Frees all the lumps cached in a private directory.
//
// Note that it's necessary to use Z_Free and not Z_ChangeTag 
// on all resources loaded from a private wad directory if the 
// directory is destroyed. Otherwise the zone heap will maintain
// dangling pointers into the freed wad directory, and heap 
// corruption would occur at a seemingly random time after an 
// arbitrary Z_Malloc call freed the cached resources.
//
void WadDirectory::FreeDirectoryLumps()
{
   int i;
   lumpinfo_t **li = lumpinfo;

   for(i = 0; i < numlumps; ++i)
   {
      if(li[i]->cache)
      {
         Z_Free(li[i]->cache);
         li[i]->cache = NULL;
      }
   }
}

//
// W_FreeDirectoryAllocs
//
// haleyjd 06/06/10
// Frees all lumpinfo_t's allocated for a wad directory.
//
void WadDirectory::FreeDirectoryAllocs()
{
   int i;

   if(!infoptrs)
      return;

   // free each lumpinfo_t allocation
   for(i = 0; i < numallocs; ++i)
      free(infoptrs[i]);

   // free the allocation tracking table
   free(infoptrs);
   infoptrs = NULL;
   numallocs = numallocsa = 0;
}

//
// WadDirectory::Close
//
// haleyjd 03/09/11: Abstracted out of I_Pick_CloseWad and W_delManagedDir.
//
void WadDirectory::Close()
{
   // close the wad file if it is open; public directories can't be closed
   if(lumpinfo && !ispublic)
   {
      // free all resources loaded from the wad
      FreeDirectoryLumps();

      if(lumpinfo[0]->file)
         fclose(lumpinfo[0]->file);

      // free all lumpinfo_t's allocated for the wad
      FreeDirectoryAllocs();

      // free the private wad directory
      Z_Free(lumpinfo);

      lumpinfo = NULL;
   }
}

//=============================================================================
//
// haleyjd 07/12/07: Implementor functions for individual lump types
//

//
// Direct lumps -- lumps that consist of an entire physical file, or a part
// of one. This includes normal files and wad lumps; to the code, it makes no
// difference.
//

static size_t W_DirectReadLump(lumpinfo_t *l, void *dest, size_t size)
{
   size_t ret;

   // killough 10/98: Add flashing disk indicator
   I_BeginRead();
   fseek(l->file, l->position, SEEK_SET);
   ret = fread(dest, 1, size, l->file);
   I_EndRead();

   return ret;
}

//
// Memory lumps -- lumps that are held in a static memory buffer
//

static size_t W_MemoryReadLump(lumpinfo_t *l, void *dest, size_t size)
{
   // killough 1/31/98: predefined lump data
   memcpy(dest, (byte *)(l->data) + l->position, size);

   return size;
}

//----------------------------------------------------------------------------
//
// $Log: w_wad.c,v $
// Revision 1.20  1998/05/06  11:32:00  jim
// Moved predefined lump writer info->w_wad
//
// Revision 1.19  1998/05/03  22:43:09  killough
// beautification, header #includes
//
// Revision 1.18  1998/05/01  14:53:59  killough
// beautification
//
// Revision 1.17  1998/04/27  02:06:41  killough
// Program beautification
//
// Revision 1.16  1998/04/17  10:34:53  killough
// Tag lumps with namespace tags to resolve collisions
//
// Revision 1.15  1998/04/06  04:43:59  killough
// Add C_START/C_END support, remove non-standard C code
//
// Revision 1.14  1998/03/23  03:42:59  killough
// Fix drive-letter bug and force marker lumps to 0-size
//
// Revision 1.12  1998/02/23  04:59:18  killough
// Move TRANMAP init code to r_data.c
//
// Revision 1.11  1998/02/20  23:32:30  phares
// Added external tranmap
//
// Revision 1.10  1998/02/20  22:53:25  phares
// Moved TRANMAP initialization to w_wad.c
//
// Revision 1.9  1998/02/17  06:25:07  killough
// Make numlumps static add #ifdef RANGECHECK for perf
//
// Revision 1.8  1998/02/09  03:20:16  killough
// Fix garbage printed in lump error message
//
// Revision 1.7  1998/02/02  13:21:04  killough
// improve hashing, add predef lumps, fix err handling
//
// Revision 1.6  1998/01/26  19:25:10  phares
// First rev with no ^Ms
//
// Revision 1.5  1998/01/26  06:30:50  killough
// Rewrite merge routine to use simpler, robust algorithm
//
// Revision 1.3  1998/01/23  20:28:11  jim
// Basic sprite/flat functionality in PWAD added
//
// Revision 1.2  1998/01/22  05:55:58  killough
// Improve hashing algorithm
//
//----------------------------------------------------------------------------

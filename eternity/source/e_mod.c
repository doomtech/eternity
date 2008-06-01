// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2008 James Haley
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
//    Custom damage types, or "Means of Death" flags.
//
//-----------------------------------------------------------------------------

#define NEED_EDF_DEFINITIONS

#include "z_zone.h"

#include "Confuse/confuse.h"

#include "e_lib.h"
#include "e_edf.h"
#include "e_mod.h"

#include "d_dehtbl.h"
#include "d_io.h"

//
// damagetype options
//

#define ITEM_DAMAGETYPE_NUM  "num"
#define ITEM_DAMAGETYPE_OBIT "obituary"

cfg_opt_t edf_damagetype_opts[] =
{
   CFG_INT(ITEM_DAMAGETYPE_NUM,  -1,   CFGF_NONE),
   CFG_STR(ITEM_DAMAGETYPE_OBIT, NULL, CFGF_NONE),
   CFG_END()
};

//
// Static Data
//

// hash tables

#define NUMMODCHAINS 67

static emod_t *e_mod_namechains[NUMMODCHAINS];
static emod_t *e_mod_numchains[NUMMODCHAINS];

// default damage type - "Unknown"

static emod_t unknown_mod =
{
   { NULL, NULL }, // numlinks

   "Unknown",      // name
   0,              // num
   "OB_DEFAULT",   // obituary
   true            // obitIsBexString
};

// allocation starts at D_MAXINT and works toward 0
static int edf_alloc_modnum = D_MAXINT;

//
// Static Functions
//

//
// E_AddDamageTypeToNameHash
//
// Puts the emod_t into the name hash table.
//
static void E_AddDamageTypeToNameHash(emod_t *mod)
{
   unsigned int key = D_HashTableKey(mod->name) % NUMMODCHAINS;

   mod->nextname = e_mod_namechains[key];
   e_mod_namechains[key] = mod;
}

//
// E_AddDamageTypeToNumHash
//
// Puts the emod_t into the numeric hash table.
//
static void E_AddDamageTypeToNumHash(emod_t *mod)
{
   unsigned int key = mod->num % NUMMODCHAINS;

   if(mod->num <= 0)
      return;

   M_DLListInsert((mdllistitem_t *)mod, 
                  (mdllistitem_t **)(&e_mod_numchains[key]));
}

//
// E_DelDamageTypeFromNumHash
//
// Removes the given emod_t from the numeric hash table.
// For overrides changing the numeric key of an existing damage type.
//
static void E_DelDamageTypeFromNumHash(emod_t *mod)
{
   M_DLListRemove((mdllistitem_t *)mod);
}

//
// E_ProcessDamageType
//
// Adds a single damage type.
//
static void E_ProcessDamageType(cfg_t *dtsec)
{
   emod_t *mod;
   const char *title, *obituary;
   boolean def = true;
   int num;

   title = cfg_title(dtsec);
   num   = cfg_getint(dtsec, ITEM_DAMAGETYPE_NUM);

   // if one exists by this name already, modify it
   if((mod = E_DamageTypeForName(title)))
   {
      // check numeric key
      if(mod->num != num)
      {
         // if old num > 0, remove from numeric hash
         if(mod->num > 0)
            E_DelDamageTypeFromNumHash(mod);

         // change key
         mod->num = num;

         // if new num > 0, add back to numeric hash
         if(mod->num > 0)
            E_AddDamageTypeToNumHash(mod);
      }

      // not a definition
      def = false;
   }
   else
   {
      // create a new mod
      mod = calloc(1, sizeof(emod_t));

      mod->name = strdup(title);
      mod->num  = num;

      // add to hash tables
      E_AddDamageTypeToNameHash(mod);
      E_AddDamageTypeToNumHash(mod);
   }

   // get obituary
   if(def || cfg_size(dtsec, ITEM_DAMAGETYPE_OBIT) > 0)
   {
      obituary = cfg_getstr(dtsec, ITEM_DAMAGETYPE_OBIT);

      // if modifying, free any that already exists
      if(!def && mod->obituary)
      {
         free(mod->obituary);
         mod->obituary = NULL;
      }

      if(obituary)
      {
         // determine if obituary string is a BEX string
         if(obituary[0] == '$' && strlen(obituary) > 1)
         {
            ++obituary;         
            mod->obitIsBexString = true;
         }
         else
            mod->obitIsBexString = false;

         mod->obituary = strdup(obituary);
      }
   }

   E_EDFLogPrintf("\t\t%s damagetype %s\n", 
                  def ? "Defined" : "Modified", mod->name);
}

//
// Global Functions
//

//
// E_ProcessDamageTypes
//
// Adds all damage types in the given cfg_t.
//
void E_ProcessDamageTypes(cfg_t *cfg)
{
   unsigned int i;
   unsigned int nummods = cfg_size(cfg, EDF_SEC_MOD);

   E_EDFLogPrintf("\t* Processing damagetypes\n"
                  "\t\t%d damagetype(s) defined\n", nummods);

   for(i = 0; i < nummods; ++i)
      E_ProcessDamageType(cfg_getnsec(cfg, EDF_SEC_MOD, i));
}

//
// E_DamageTypeForName
//
// Finds a damage type for the given name. If the name does not exist, the
// default "Unknown" damage type will be returned.
//
emod_t *E_DamageTypeForName(const char *name)
{
   unsigned int key = D_HashTableKey(name) % NUMMODCHAINS;
   emod_t *mod = e_mod_namechains[key];

   while(mod && strcasecmp(mod->name, name))
      mod = mod->nextname;

   if(mod == NULL)
      mod = &unknown_mod;

   return mod;
}

//
// E_DamageTypeForNum
//
// Finds a damage type for the given number. If the number is not found, the
// default "Unknown" damage type will be returned.
//
emod_t *E_DamageTypeForNum(int num)
{
   unsigned int key = num % NUMMODCHAINS;
   emod_t *mod = e_mod_numchains[key];

   while(mod && mod->num != num)
      mod = (emod_t *)(mod->numlinks.next);

   if(mod == NULL)
      mod = &unknown_mod;

   return mod;
}

//
// E_AutoAllocModNum
//
// Automatically assigns an unused damage type number to a damage type
// which was not given one by the author, to allow reference by name
// anywhere without the chore of number allocation.
//
boolean E_AutoAllocModNum(emod_t *mod)
{
   int num;

#ifdef RANGECHECK
   if(mod->num >= 0)
      I_Error("E_AutoAllocModNum: called for emod_t with valid num\n");
#endif

   // cannot assign because we're out of dehnums?
   if(edf_alloc_modnum < 0)
      return false;

   do
   {
      num = edf_alloc_modnum--;
   } 
   while(num > 0 && E_DamageTypeForNum(num) != &unknown_mod);

   // ran out while looking for an unused number?
   if(num <= 0)
      return false;

   // assign it!
   mod->num = num;
   E_AddDamageTypeToNumHash(mod);

   return true;
}

// EOF


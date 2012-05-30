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
//    Inventory and Item Effects
//
//-----------------------------------------------------------------------------

#ifndef P_INVENTORY_H__
#define P_INVENTORY_H__

class  CheckFunctor;
struct inventory_t;
class  Mobj;

class InventoryGeneric
{
public:
   struct pickupdata_t
   {
      Mobj        *collector;   // object acquiring the item
      Mobj        *item;        // object being picked up (may be NULL)
      inventory_t *inv;         // inventory definition
      bool         validPClass; // collector passed pclass restrictions
   };

protected:
   // Statics
   static bool CheckForCollect(pickupdata_t &params);
   static bool IsValidPlayerClass(pickupdata_t &params);

   // Virtuals
   virtual bool canCollect(pickupdata_t &params); // see if can collect

   /*
   // Virtuals
   virtual bool tryCollect(Mobj *collector);      // first chance to pickup
   virtual bool tryCollectAgain(Mobj *collector); // last chance to pickup
   */

   // Data
   const char *const name;
   
   // Friends
   friend class CheckFunctor;

public:
   InventoryGeneric(const char *pName) : name(pName) {}

   // Statics
   static bool GiveItem(Mobj *collector, inventory_t *inv);
   static bool TouchItem(Mobj *collector, Mobj *item);
   static InventoryGeneric *GetInventoryInstance(int classType);

   // Fixed methods
   const char *getName() const { return name; }
};

class InventoryHealth : public InventoryGeneric
{
public:
   InventoryHealth(const char *pName) : InventoryGeneric(pName) {}
};

#endif

// EOF


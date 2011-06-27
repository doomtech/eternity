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

#ifndef P_TICK_H__
#define P_TICK_H__

#include "z_zone.h"

class SaveArchive;

// Doubly linked list of actors.
class Thinker : public ZoneObject
{
private:
   // Private implementation details
   void removeDelayed(); 
   
   // Current position in list during RunThinkers
   static Thinker *currentthinker;

protected:
   // Virtual methods (overridables)
   virtual void Think() {}

   // Methods
   void addToThreadedList(int tclass);

   // Data Members
   bool removed;

   // killough 11/98: count of how many other objects reference
   // this one using pointers. Used for garbage collection.
   unsigned int references;

   // haleyjd 12/22/2010: for savegame enumeration
   unsigned int ordinal;

public:
   // Constructor
   Thinker() 
      : ZoneObject(), removed(false), references(0), ordinal(0), prev(NULL),
        next(NULL), cprev(NULL), cnext(NULL)
   {
      ChangeTag(PU_LEVEL);
   }

   // Static functions
   static void InitThinkers();
   static void RunThinkers();

   // Methods
   void addThinker();
   
   // Accessors
   bool isRemoved() const { return removed; }
   
   // Reference counting
   void addReference() { ++references; }
   void delReference() { --references; }

   // Enumeration 
   // For thinkers needing savegame enumeration.
   void setOrdinal(unsigned int i) { ordinal = shouldSerialize() ? i : 0; }
   unsigned int getOrdinal() const { return ordinal; }

   // Virtual methods (overridables)
   virtual void updateThinker();
   virtual void removeThinker();

   // Serialization
   // When using serialize, always call your parent implementation!
   virtual void serialize(SaveArchive &arc);
   // De-swizzling should restore pointers to other thinkers.
   virtual void deswizzle() {}
   virtual bool shouldSerialize()  const { return !removed;  }
   virtual const char *getClassName() const { return "Thinker"; }

   // Data Members

   Thinker *prev;
   Thinker *next;
  
   // killough 8/29/98: we maintain thinkers in several equivalence classes,
   // according to various criteria, so as to allow quicker searches.

   Thinker *cprev; // Next, previous thinkers in same class
   Thinker *cnext;
};

//
// thinker_cast
//
// Use this dynamic_cast variant to automatically check if something is a valid
// unremoved Thinker subclass instance. This is necessary because the old 
// behavior of checking function pointer values effectively changed the type 
// that a thinker was considered to be when it was set into a deferred removal
// state, and C++ doesn't support that with virtual methods OR RTTI.
//
template<typename T> inline T thinker_cast(Thinker *th)
{
   return (th && !th->isRemoved()) ? dynamic_cast<T>(th) : NULL;
}

// Called by C_Ticker, can call G_PlayerExited.
// Carries out all thinking of monsters and players.
void P_Ticker(void);

extern Thinker thinkercap;  // Both the head and tail of the thinker list

//
// P_SetTarget
//
// killough 11/98
// This function is used to keep track of pointer references to mobj thinkers.
// In Doom, objects such as lost souls could sometimes be removed despite 
// their still being referenced. In Boom, 'target' mobj fields were tested
// during each gametic, and any objects pointed to by them would be prevented
// from being removed. But this was incomplete, and was slow (every mobj was
// checked during every gametic). Now, we keep a count of the number of
// references, and delay removal until the count is 0.
//
template<typename T> void P_SetTarget(T **mop, T *targ)
{
   if(*mop)             // If there was a target already, decrease its refcount
      (*mop)->delReference();
   if((*mop = targ))    // Set new target and if non-NULL, increase its counter
      targ->addReference();
}

// killough 8/29/98: threads of thinkers, for more efficient searches
typedef enum 
{
  th_delete,  // haleyjd 11/09/06: giant bug fix
  th_misc,
  th_friends,
  th_enemies,
  NUMTHCLASS
} th_class;

extern Thinker thinkerclasscap[];

// sf: jumping-viewz-on-hyperlift bug
extern bool reset_viewz;

//
// Thinker Factory
//
// haleyjd 12/07/10: The save game code needs to be able to construct thinkers
// of any class without resort to a gigantic switch statement. This calls for
// a factory pattern.
//
class ThinkerType
{
protected:
   ThinkerType *next;
   const char  *name;

public:
   ThinkerType(const char *pName);

   // newThinker is a pure virtual method that must be overridden by a child 
   // class to construct a specific type of thinker
   virtual Thinker *newThinker() const = 0;

   // FindType is a static method that will find the ThinkerType given the
   // name of a Thinker-descendent class (ie., "FireFlickerThinker"). The returned
   // object can then be used to construct a new instance of that type by 
   // calling the newThinker method. The instance can then be fed to the
   // serialization mechanism.
   static ThinkerType *FindType(const char *pName); // find a type in the list
};

//
// IMPLEMENT_THINKER_TYPE
//
// Use this macro once per Thinker descendant. Best placed near the Think 
// routine.
// Example:
//   IMPLEMENT_THINKER_TYPE(FireFlickerThinker)
//   This defines FireFlickerThinkerType, which constructs a ThinkerType parent
//   with "FireFlickerThinker" as its name member and which returns a new FireFlickerThinker
//   instance via its newThinker virtual method.
// 
#define IMPLEMENT_THINKER_TYPE(name) \
class name ## Type : public ThinkerType \
{ \
protected: \
   static name ## Type global ## name ## Type ; \
public: \
   name ## Type() : ThinkerType( #name ) {} \
   virtual Thinker *newThinker() const { return new name ; } \
}; \
name ## Type name ## Type :: global ## name ## Type;
   

#endif

//----------------------------------------------------------------------------
//
// $Log: p_tick.h,v $
// Revision 1.5  1998/05/15  00:36:22  killough
// Remove unnecessary crash hack
//
// Revision 1.4  1998/05/13  22:58:01  killough
// Restore Doom bug compatibility for demos
//
// Revision 1.3  1998/05/03  22:49:29  killough
// Add external declarations, formerly in p_local.h
//
// Revision 1.2  1998/01/26  19:27:31  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:08  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

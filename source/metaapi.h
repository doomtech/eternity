// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
//
//   Metatables for storage of multiple types of objects in an associative
//   array.
//
//-----------------------------------------------------------------------------

#ifndef METAAPI_H__
#define METAAPI_H__

#include "e_rtti.h"
#include "m_dllist.h"

// METATYPE macro - make a string from a typename
#define METATYPE(t) #t

// enumeration for metaerrno
enum
{
   META_ERR_NOERR,        // 0 is not an error
   META_ERR_NOSUCHOBJECT,
   META_ERR_NOSUCHTYPE,
   META_NUMERRS
};

extern int metaerrno;

class metaTablePimpl;

//
// MetaObject
//
class MetaObject : public RTTIObject
{
   DECLARE_RTTI_TYPE(MetaObject, RTTIObject)

protected:
   DLListItem<MetaObject> links;     // links by key
   DLListItem<MetaObject> typelinks; // links by type
   const char *key;                  // primary hash key
   const char *type;                 // type hash key
   
   char *key_name; // storage pointer for key (alloc'd string)

   friend class metaTablePimpl;

public:
   // Constructors/Destructor
   MetaObject();
   MetaObject(const char *pKey);
   MetaObject(const MetaObject &other);
   virtual ~MetaObject();

   void setType();

   const char *getKey() const { return key_name; }

   // Virtual Methods
   virtual MetaObject *clone() const;
   virtual const char *toString() const;   
};

// MetaObject specializations for basic types

class MetaInteger : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaInteger, MetaObject)

protected:
   int value;

public:
   MetaInteger() : MetaObject(), value(0) {}
   MetaInteger(const char *key, int i);
   MetaInteger(const MetaInteger &other);

   // Virtual Methods
   virtual MetaObject *clone() const;
   virtual const char *toString() const; 

   // Accessors
   int getValue() const { return value; }
   void setValue(int i) { value = i;    }

   friend class MetaTable;
};

class MetaDouble : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaDouble, MetaObject)

protected:
   double value;

public:
   MetaDouble() : MetaObject(), value(0.0) {}
   MetaDouble(const char *key, double d);
   MetaDouble(const MetaDouble &other);

   // Virtual Methods
   virtual MetaObject *clone() const;
   virtual const char *toString() const;

   // Accessors
   double getValue() const { return value; }
   void setValue(double d) { value = d;    }

   friend class MetaTable;
};

class MetaString : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaString, MetaObject)

protected:
   char *value;

public:
   MetaString();
   MetaString(const char *key, const char *s);
   MetaString(const MetaString &other);
   virtual ~MetaString();

   // Virtual Methods
   virtual MetaObject *clone() const;
   virtual const char *toString() const;

   // Accessors
   const char *getValue() const { return value; }
   void setValue(const char *s, char **ret = NULL);

   friend class MetaTable;
};

// MetaTable

class MetaTable : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaTable, MetaObject)

private:
   metaTablePimpl *pImpl;

public:
   MetaTable();
   MetaTable(const char *name);
   MetaTable(const MetaTable &other);
   virtual ~MetaTable();

   // MetaObject overrides
   virtual MetaObject *clone() const;
   virtual const char *toString() const;

   // Search functions. Frankly, it's more efficient to just use the "get" routines :P
   bool hasKey(const char *key);
   bool hasType(const char *type);
   bool hasKeyAndType(const char *key, const char *type);

   // Count functions.
   int countOfKey(const char *key);
   int countOfType(const char *type);
   int countOfKeyAndType(const char *key, const char *type);

   // Add/Remove Objects
   void addObject(MetaObject *object);
   void addObject(MetaObject &object);
   void removeObject(MetaObject *object);
   void removeObject(MetaObject &object);

   // Find objects in the table:
   // * By Key
   MetaObject *getObject(const char *key);
   // * By Type
   MetaObject *getObjectType(const char *type);
   // * By Key AND Type
   MetaObject *getObjectKeyAndType(const char *key, const char *type);
   MetaObject *getObjectKeyAndType(const char *key, MetaObject::Type *rt);

   // Iterators
   MetaObject *getNextObject(MetaObject *object, const char *key);
   MetaObject *getNextType(MetaObject *object, const char *type);
   MetaObject *getNextKeyAndType(MetaObject *object, const char *key, const char *type);
   MetaObject *tableIterator(MetaObject *object) const;

   // Add/Get/Set Convenience Methods for Basic MetaObjects
   
   // Signed integer
   void addInt(const char *key, int value);
   int  getInt(const char *key, int defValue);
   void setInt(const char *key, int newValue);
   int  removeInt(const char *key);

   // Double floating-point
   void   addDouble(const char *key, double value);
   double getDouble(const char *key, double defValue);
   void   setDouble(const char *key, double newValue);
   double removeDouble(const char *key);

   // Managed strings
   void        addString(const char *key, const char *value);
   const char *getString(const char *key, const char *defValue);
   void        setString(const char *key, const char *newValue);
   char       *removeString(const char *key);
   void        removeStringNR(const char *key);

   // Copy routine - clones the entire MetaTable
   void copyTableTo(MetaTable *dest) const;
   void copyTableFrom(const MetaTable *source);

   // Clearing
   void clearTable();
};

#endif

// EOF


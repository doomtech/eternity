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
//    Hash routines for data integrity and cryptographic purposes.
//
//-----------------------------------------------------------------------------

#ifndef M_HASH_H__
#define M_HASH_H__

//#define NUMDIGEST 5

class HashData
{
public:
   enum hashtype_e
   {
      CRC32,
      ADLER32,
      MD5,
      SHA1,
      NUMHASHTYPES
   };

   static const int numdigest;

protected:
   uint32_t digest[5];   // the hash of the message (not all are always used)
   uint32_t messagelen;  // total length of message processed so far
   uint8_t  message[64]; // current 512-bit message block
   int      messageidx;  // current index into message (accumulate until == 64)
   boolean  gonebad;     // an error occurred in the computation
   hashtype_e type;      // algorithm being used for computation   

   friend class CRC32Hash;
   friend class Adler32Hash;
   friend class MD5Hash;
   friend class SHA1Hash;

public:
   void    Initialize(hashtype_e type);
   void    AddData(const uint8_t *data, uint32_t size);
   void    WrapUp();
   boolean compare(const HashData &other) const;
   void    StringToDigest(const char *str);
   char   *DigestToString() const;
};

#endif

// EOF


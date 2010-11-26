// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2010 James Haley
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
//   Buffered file output.
//
//-----------------------------------------------------------------------------

#ifndef M_BUFFER_H__
#define M_BUFFER_H__

class CBufferedFileBase
{
protected:
   FILE *f;      // destination or source file
   byte *buffer; // buffer
   size_t len;   // total buffer length
   size_t idx;   // current index
   int endian;   // endianness indicator
   
   void InitBuffer(size_t pLen, int pEndian);

public:
   long Tell();
   virtual void Close();

   void SwapULong(uint32_t &x);
   void SwapUShort(uint16_t &x);

   // endianness values
   enum
   {
      NENDIAN, // doesn't swap shorts or ints
      LENDIAN, // swaps shorts/ints to little endian
      BENDIAN  // swaps shorts/ints to big endian
   };
};

class COutBuffer : public CBufferedFileBase
{
public:
   boolean CreateFile(const char *filename, size_t pLen, int pEndian);
   boolean Flush();
   void    Close();
   boolean Write(const void *data, size_t size);
   boolean WriteUint32(uint32_t num);
   boolean WriteUint16(uint16_t num);
   boolean WriteUint8(uint8_t num);

};

class CInBuffer : public CBufferedFileBase
{
protected:
   size_t readlen; // amount actually read (may be less than len)
   boolean atEOF;
   boolean ReadFile();

public:
   boolean OpenFile(const char *filename, size_t pLen, int pEndian);
   void    Close();
   boolean Read(void *dest, size_t size);
   boolean ReadUint32(uint32_t &num);
   boolean ReadUint16(uint16_t &num);
   boolean ReadUint8(uint8_t &num);
};

#endif

// EOF



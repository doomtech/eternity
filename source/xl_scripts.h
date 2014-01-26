// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//-----------------------------------------------------------------------------
//
// Hexen Script Lumps
//
//   Parsing and processing for Hexen's data scripts
//
//   By James Haley
//
//-----------------------------------------------------------------------------

#ifndef XL_SCRIPTS_H__
#define XL_SCRIPTS_H__

#include "m_qstr.h"

struct lumpinfo_t;
class  WadDirectory;

//
// XLTokenizer
//
// Tokenizer used by XLParser
//
class XLTokenizer
{
public:
   // Tokenizer states
   enum
   {
      STATE_SCAN,    // scanning for a string token
      STATE_INTOKEN, // in a string token
      STATE_QUOTED,  // in a quoted string
      STATE_COMMENT, // reading out a comment (eat rest of line)
      STATE_DONE     // finished the current token
   };

   // Token types
   enum
   {
      TOKEN_NONE,    // Nothing identified yet
      TOKEN_KEYWORD, // Starts with a $; otherwise, same as a string
      TOKEN_STRING,  // Generic string token; ex: 92 foobar
      TOKEN_EOF,     // End of input
      TOKEN_ERROR    // An unknown token
   };

protected:
   int state;         // state of the scanner
   const char *input; // input string
   int idx;           // current position in input string
   int tokentype;     // type of current token
   qstring token;     // current token value

   void doStateScan();
   void doStateInToken();
   void doStateQuoted();
   void doStateComment();

   // State table declaration
   static void (XLTokenizer::*States[])();

public:
   // Constructor / Destructor
   XLTokenizer(const char *str) 
      : state(STATE_SCAN), input(str), idx(0), tokentype(TOKEN_NONE), token(32)
   { 
   }

   int getNextToken();
   
   // Accessors
   int getTokenType() const { return tokentype; }
   qstring &getToken() { return token; }
};

//
// XLParser
//
// Base class for Hexen lump parsers
//
class XLParser
{
protected:
   // Data
   const char   *lumpname; // Name of lump handled by this parser
   char         *lumpdata; // Cached lump data
   WadDirectory *waddir;   // Current directory

   // Override me!
   virtual void startLump() {} // called at beginning of a new lump
   virtual bool doToken(XLTokenizer &token) { return true; } // called for each token

   void parseLump(WadDirectory &dir, lumpinfo_t *lump);
   void parseLumpRecursive(WadDirectory &dir, lumpinfo_t *curlump);

public:
   // Constructors
   XLParser(const char *pLumpname) : lumpname(pLumpname), lumpdata(NULL) {}

   // Destructor
   virtual ~XLParser() 
   {
      // kill off any lump that might still be cached
      if(lumpdata)
      {
         efree(lumpdata);
         lumpdata = NULL;
      }
   }

   void parseAll(WadDirectory &dir);
   void parseNew(WadDirectory &dir);

   // Accessors
   const char *getLumpName() const { return lumpname; }
   void setLumpName(const char *s) { lumpname = s;    }
};

void XL_ParseHexenScripts();

#endif

// EOF


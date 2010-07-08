// Emacs style mode select   -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 James Haley
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
// Reallocating string structure
//
// What this "class" guarantees:
// * The string will always be null-terminated
// * Indexing functions always check array bounds
// * Insertion functions always reallocate when needed
//
// Of course, using M_QStrBuffer can negate these, so avoid it
// except for passing a char * to read op functions.
//
// By James Haley
//
//-----------------------------------------------------------------------------

#ifndef M_QSTR_H__
#define M_QSTR_H__

typedef struct qstring_s
{
   char *buffer;
   unsigned int index;
   unsigned int size;
} qstring_t;

qstring_t    *M_QStrInitCreate(qstring_t *qstr);
qstring_t    *M_QStrCreateSize(qstring_t *qstr, unsigned int size);
qstring_t    *M_QStrCreate(qstring_t *qstr);
unsigned int  M_QStrLen(qstring_t *qstr);
unsigned int  M_QStrSize(qstring_t *qstr);
char         *M_QStrBuffer(qstring_t *qstr);
qstring_t    *M_QStrGrow(qstring_t *qstr, unsigned int len);
qstring_t    *M_QStrClear(qstring_t *qstr);
void          M_QStrFree(qstring_t *qstr);
char          M_QStrCharAt(qstring_t *qstr, unsigned int idx);
qstring_t    *M_QStrPutc(qstring_t *qstr, char ch);
qstring_t    *M_QStrDelc(qstring_t *qstr);
qstring_t    *M_QStrCat(qstring_t *qstr, const char *str);
int           M_QStrCmp(qstring_t *qstr, const char *str);
int           M_QStrCaseCmp(qstring_t *qstr, const char *str);
qstring_t    *M_QStrCpy(qstring_t *qstr, const char *str);
qstring_t    *M_QStrCopyTo(qstring_t *dest, const qstring_t *src);
qstring_t    *M_QStrUpr(qstring_t *qstr);
qstring_t    *M_QStrLwr(qstring_t *qstr);
unsigned int  M_QStrReplace(qstring_t *qstr, const char *filter, char repl);
unsigned int  M_QStrReplaceNotOf(qstring_t *qstr, const char *filter, char repl);
char         *M_QStrCDup(qstring_t *qstr, int tag);
int           M_QStrAtoi(qstring_t *qstr);
const char   *M_QStrChr(qstring_t *qstr, char c);
const char   *M_QStrRChr(qstring_t *qstr, char c);
qstring_t    *M_QStrLStrip(qstring_t *qstr, char c);
qstring_t    *M_QStrRStrip(qstring_t *qstr, char c);
int           M_QStrPrintf(qstring_t *qstr, unsigned int maxlen, const char *fmt, ...);

#endif

// EOF



// Emacs style mode select   -*- C++ -*- 
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
// DESCRIPTION:  
//    Common utilities for "Extended Feature" modules.
//
//-----------------------------------------------------------------------------

#ifndef E_LIB_H__
#define E_LIB_H__

#include "doomtype.h"

struct dehflagset_t;

typedef struct E_Enable_s
{
   const char *name;
   int enabled;
} E_Enable_t;

#ifdef NEED_EDF_DEFINITIONS

// basic stuff
void E_ErrorCB(cfg_t *cfg, const char *fmt, va_list ap);

// include tracking
int E_CheckRoot(cfg_t *cfg, const char *data, int size);

// function callbacks
int E_Include    (cfg_t *, cfg_opt_t *, int, const char **);
int E_LumpInclude(cfg_t *, cfg_opt_t *, int, const char **);
int E_IncludePrev(cfg_t *, cfg_opt_t *, int, const char **);
int E_StdInclude (cfg_t *, cfg_opt_t *, int, const char **);
int E_UserInclude(cfg_t *, cfg_opt_t *, int, const char **);
int E_Endif      (cfg_t *, cfg_opt_t *, int, const char **);

// value-parsing callbacks
int E_SpriteFrameCB(cfg_t *, cfg_opt_t *, const char *, void *);
int E_IntOrFixedCB (cfg_t *, cfg_opt_t *, const char *, void *);
int E_TranslucCB   (cfg_t *, cfg_opt_t *, const char *, void *);
int E_TranslucCB2  (cfg_t *, cfg_opt_t *, const char *, void *);
int E_ColorStrCB   (cfg_t *, cfg_opt_t *, const char *, void *);

// MetaTable adapter utilities
class  MetaTable;
void E_MetaStringFromCfgString(MetaTable *meta, cfg_t *cfg, const char *prop);

#endif

const char *E_BuildDefaultFn(const char *filename);

// misc utilities
int E_EnableNumForName(const char *name, E_Enable_t *enables);
int E_StrToNumLinear(const char **strings, int numstrings, const char *value);
int E_ParseFlags(const char *str, dehflagset_t *flagset);
const char *E_ExtractPrefix(const char *value, char *prefixbuf, int buflen);
void E_ReplaceString(char *&dest, char *newvalue);
char *E_GetHeredocLine(char **src);
byte *E_ParseTranslation(const char *str, int tag);

#define E_MAXCMDTOKENS 8

//
// This structure is returned by E_ParseTextLine and is used to hold pointers
// to the tokens inside the command string. The pointers may be NULL if the
// corresponding tokens do not exist.
//
struct tempcmd_t
{
   const char *strs[E_MAXCMDTOKENS]; // command and up to 2 arguments
};

tempcmd_t E_ParseTextLine(char *str);

#endif

// EOF



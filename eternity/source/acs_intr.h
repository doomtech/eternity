// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
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
//----------------------------------------------------------------------------
//
// Original 100% GPL ACS Interpreter
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef ACS_INTR_H__
#define ACS_INTR_H__

#include "p_mobj.h"
#include "m_dllist.h"
#include "m_qstr.h"

//
// Defines
//

#define ACS_STACK_LEN 128 
#define ACS_NUMLOCALS 10
#define ACS_NUM_THINGTYPES 256

//
// Misc. Enums
//

// ACS game mode flags for EDF thingtypes

enum
{
   ACS_MODE_DOOM = 0x00000001,
   ACS_MODE_HTIC = 0x00000002,
   ACS_MODE_ALL  = ACS_MODE_DOOM | ACS_MODE_HTIC,
}; 

//
// Structures
//

//
// acscript
//
// An actual script entity as read from the ACS lump's entry table,
// augmented with basic runtime data.
//
typedef struct acscript_s
{
   int number;   // the number of this script in ACS itself
   int numArgs;  // number of arguments this script wants
   int *code;    // bytecode entry point

   struct acsthinker_s *threads;
} acscript_t;

//
// acsthinker
//
// A thinker which runs a script.
//
typedef struct acsthinker_s
{
   thinker_t thinker;         // must be first

   // thread links
   struct acsthinker_s **prev;
   struct acsthinker_s *next;

   // script info
   int vmID;                  // vm id number
   int scriptNum;             // script number in ACS itself
   int internalNum;           // internal script number

   // virtual machine data
   int *ip;                   // instruction pointer
   int stack[ACS_STACK_LEN];  // value stack
   int stp;                   // stack pointer
   int locals[ACS_NUMLOCALS]; // local variables and arguments
   int sreg;                  // state register
   int sdata;                 // special data for state
   
   // info copied from acscript and acsvm
   int  *code;                // entry point
   byte *data;                // base code pointer for jumps
   char **stringtable;        // strings
   qstring_t *printBuffer;    // buffer for message printing
   acscript_t *acscript;      // for convenience of access
   struct acsvm_s *vm;        // for convenience of access

   // misc
   int    delay;              // counter for script delays
   mobj_t *trigger;           // thing that activated
   line_t *line;              // line that activated
   int    lineSide;           // line side of activation
} acsthinker_t;

// deferred action types
enum
{
   ACS_DEFERRED_EXECUTE,
   ACS_DEFERRED_SUSPEND,
   ACS_DEFERRED_TERMINATE,
};

//
// deferredacs_t
//
// This struct keeps track of scripts to be executed on other maps.
//
typedef struct deferredacs_s
{
   mdllistitem_t link;     // list links
   int  scriptNum;         // ACS script number to execute
   int  vmID;              // id # of vm on which to execute the script
   int  targetMap;         // target map number
   int  type;              // type of action to perform...
   long args[NUMLINEARGS]; // additional arguments from linedef
} deferredacs_t;

//
// acsvm
//
// haleyjd 06/24/08: I am rewriting the interpreter to be modular in hopes of
// eventual support for ZDoomish features such as ACS libraries.
//
typedef struct acsvm_s
{
   byte       *data;         // ACS lump; jumps are relative to this
   char       **stringtable; // self-explanatory, yes?
   qstring_t  printBuffer;   // used for message printing
   acscript_t *scripts;      // the scripts...
   int        numScripts;    // ... and how many there are.
   boolean    loaded;        // for static vms, if it's valid or not
   int        id;            // vm id number
} acsvm_t;


// Global function prototypes

void T_ACSThinker(acsthinker_t *script);
void ACS_Init(void);
void ACS_NewGame(void);
void ACS_InitLevel(void);
void ACS_LoadScript(acsvm_t *vm, int lump);
void ACS_LoadLevelScript(int lump);
void ACS_RunDeferredScripts(void);
boolean ACS_StartScriptVM(acsvm_t *vm, int scrnum, int map, long *args, 
                          mobj_t *mo, line_t *line, int side,
                          acsthinker_t **scr, boolean always);
boolean ACS_StartScript(int scrnum, int map, long *args, mobj_t *mo, 
                        line_t *line, int side, acsthinker_t **scr);
boolean ACS_TerminateScript(int srcnum, int mapnum);
boolean ACS_SuspendScript(int scrnum, int mapnum);

// extern vars.

extern int ACS_thingtypes[ACS_NUM_THINGTYPES];

#endif

// EOF


// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2006 James Haley
// Copyright(C) 2012 David Hill
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

#include "m_dllist.h"
#include "p_tick.h"
#include "r_defs.h"

class  qstring;
class  Mobj;
struct line_t;
class  WadDirectory;

//
// Defines
//

#define ACS_STACK_LEN      128 
#define ACS_NUM_LOCALVARS  10
#define ACS_NUM_MAPVARS    32
#define ACS_NUM_WORLDVARS  64
#define ACS_NUM_GLOBALVARS 64
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
// acs_op
//
typedef enum acs_op_e
{
   #define ACS_OP(OP,ARGC) ACS_OP_##OP,
   #include "acs_op.h"
   #undef ACS_OP

   ACS_OPMAX
} acs_op_t;

//
enum
{
   ACS_THINGVAR_X,
   ACS_THINGVAR_Y,
   ACS_THINGVAR_Z,

   ACS_THINGVARMAX
};

//
// Structures
//

class ACSThinker;
class ACSVM;

//
// acs_opdata
//
typedef struct acs_opdata_s
{
   acs_op_t op;
   int args;
} acs_opdata_t;

//
// acscript
//
// An actual script entity as read from the ACS lump's entry table,
// augmented with basic runtime data.
//
typedef struct acscript_s
{
   int number;
   int numArgs;
   int type;

   union
   {
      uint32_t codeIndex;
      int32_t *codePtr;
   };

   ACSThinker *threads;
} acscript_t;

//
// acsthinker
//
// A thinker which runs a script.
//
class ACSThinker : public Thinker
{
   DECLARE_THINKER_TYPE(ACSThinker, Thinker)

protected:
   // Data Members
   unsigned int triggerSwizzle; // Holds the swizzled target during loading

   // Methods
   void Think();

public:
   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual void deSwizzle();
   
   // Data Members
   // thread links
   ACSThinker **prevthread;
   ACSThinker  *nextthread;

   // script info
   uint32_t     vmID;                 // vm id number
   int          scriptNum;            // script number in ACS itself
   unsigned int internalNum;          // internal script number

   // virtual machine data
   int32_t *ip;                       // instruction pointer
   int32_t stack[ACS_STACK_LEN];      // value stack
   int32_t stp;                       // stack pointer
   int32_t locals[ACS_NUM_LOCALVARS]; // local variables and arguments
   int32_t sreg;                      // state register
   int32_t sdata;                     // special data for state

   // info copied from acscript and acsvm
   int32_t    *code;                  // entry point
   int32_t    *data;                  // base code pointer for jumps
   qstring    *printBuffer;           // buffer for message printing
   acscript_t *acscript;              // for convenience of access
   ACSVM      *vm;                    // for convenience of access

   // misc
   int32_t delay;                     // counter for script delays
   Mobj   *trigger;                   // thing that activated
   line_t *line;                      // line that activated
   int     lineSide;                  // line side of activation
};

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
struct deferredacs_t
{
   DLListItem<deferredacs_t> link; // list links
   
   int  scriptNum;         // ACS script number to execute
   int  vmID;              // id # of vm on which to execute the script
   int  targetMap;         // target map number
   int  type;              // type of action to perform...
   int  args[NUMLINEARGS]; // additional arguments from linedef
};

//
// ACSVM
//
// haleyjd 06/24/08: I am rewriting the interpreter to be modular in hopes of
// eventual support for ZDoomish features such as ACS libraries.
//
class ACSVM : public ZoneObject
{
public:
   ACSVM(int tag = PU_STATIC);
   ~ACSVM();

   // Resets the VM to an uninitialized state. Only useful for static VMs.
   void reset();

   // bytecode info
   int32_t     *code;                 // ACS code; jumps are relative to this
   unsigned int numCode;
   unsigned int strings;              // offset into global table
   unsigned int numStrings;
   acscript_t  *scripts;              // the scripts
   unsigned int numScripts;
   bool         loaded;               // for static VMs, if it's valid or not
   uint32_t     id;                   // vm id number
   int          lump;                 // lump bytecode was loaded from

   // interpreter info
   qstring *printBuffer;              // used for message printing
   int32_t *mapvtab[ACS_NUM_MAPVARS]; // pointers into vm mapvars
   int32_t  mapvars[ACS_NUM_MAPVARS];

   // global bytecode info
   static const char **GlobalStrings;
   static unsigned int GlobalNumStrings;
   static const char *GetString(uint32_t strnum)
   {
      return strnum < GlobalNumStrings ? GlobalStrings[strnum] : "";
   }
};


// Global function prototypes

void ACS_Init(void);
void ACS_NewGame(void);
void ACS_InitLevel(void);
void ACS_LoadScript(ACSVM *vm, WadDirectory *dir, int lump);
void ACS_LoadScriptACS0(ACSVM *vm, WadDirectory *dir, int lump, byte *data);
void ACS_LoadLevelScript(WadDirectory *dir, int lump);
void ACS_RunDeferredScripts(void);
bool ACS_StartScriptVM(ACSVM *vm, int scrnum, int map, int *args,
                       Mobj *mo, line_t *line, int side,
                       ACSThinker **scr, bool always);
bool ACS_StartScript(int scrnum, int map, int *args, Mobj *mo,
                     line_t *line, int side, ACSThinker **scr, bool always);
bool ACS_TerminateScript(int srcnum, int mapnum);
bool ACS_SuspendScript(int scrnum, int mapnum);
void ACS_Archive(SaveArchive &arc);
void ACS_RestartSavedScript(ACSThinker *th, unsigned int ipOffset);

// extern vars.

extern acs_opdata_t ACSopdata[ACS_OPMAX];

extern int ACS_thingtypes[ACS_NUM_THINGTYPES];
extern int32_t ACSworldvars[ACS_NUM_WORLDVARS];
extern int32_t ACSglobalvars[ACS_NUM_GLOBALVARS];

#endif

// EOF


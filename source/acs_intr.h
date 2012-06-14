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
#define ACS_NUM_CALLS      16
#define ACS_NUM_LOCALVARS  20
#define ACS_NUM_MAPVARS    32
#define ACS_NUM_MAPARRS    32
#define ACS_NUM_WORLDVARS  64
#define ACS_NUM_WORLDARRS  64
#define ACS_NUM_GLOBALVARS 64
#define ACS_NUM_GLOBALARRS 64
#define ACS_NUM_THINGTYPES 256

// ACS array constants
#define ACS_PAGESIZE 1024
#define ACS_BLOCKSIZE 512
#define ACS_REGIONSIZE 512
#define ACS_ARRDATASIZE 16

#define ACS_FUNCARG ACSThinker *thread, uint32_t argc, const int32_t *args, int32_t *&retn

#define ACS_CHUNKID(A,B,C,D) (((A) << 0) | ((B) << 8) | ((C) << 16) | ((D) << 24))

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

// CALLFUNC indexes.
enum acs_funcnum_t
{
   ACS_FUNC_NOP,
   ACS_FUNC_ActivatorSound,
   ACS_FUNC_AmbientSound,
   ACS_FUNC_AmbientSoundLocal,
   ACS_FUNC_ChangeCeiling,
   ACS_FUNC_ChangeFloor,
   ACS_FUNC_GetSectorCeilingZ,
   ACS_FUNC_GetSectorFloorZ,
   ACS_FUNC_GetSectorLightLevel,
   ACS_FUNC_Random,
   ACS_FUNC_SectorSound,
   ACS_FUNC_SetLineBlocking,
   ACS_FUNC_SetLineMonsterBlocking,
   ACS_FUNC_SetLineSpecial,
   ACS_FUNC_SetLineTexture,
   ACS_FUNC_SetMusic,
   ACS_FUNC_SetMusicLocal,
   ACS_FUNC_SetThingPosition,
   ACS_FUNC_SetThingSpecial,
   ACS_FUNC_SoundSequence,
   ACS_FUNC_SpawnPoint,
   ACS_FUNC_SpawnProjectile,
   ACS_FUNC_SpawnSpot,
   ACS_FUNC_SpawnSpotAngle,
   ACS_FUNC_ThingCount,
   ACS_FUNC_ThingCountName,
   ACS_FUNC_ThingProjectile,
   ACS_FUNC_ThingSound,

   ACS_FUNCMAX
};

// script types
enum acs_stype_t
{
   ACS_STYPE_CLOSED,
   ACS_STYPE_OPEN,

   ACS_STYPEMAX
};

// Level variables.
enum
{
   ACS_LEVELVAR_ParTime        = 0,
   ACS_LEVELVAR_ClusterNumber  = 1,
   ACS_LEVELVAR_LevelNumber    = 2,
   ACS_LEVELVAR_TotalSecrets   = 3,
   ACS_LEVELVAR_FoundSecrets   = 4,
   ACS_LEVELVAR_TotalItems     = 5,
   ACS_LEVELVAR_FoundItems     = 6,
   ACS_LEVELVAR_TotalMonsters  = 7,
   ACS_LEVELVAR_KilledMonsters = 8,
   ACS_LEVELVAR_SuckTime       = 9,

   ACS_LEVELVARMAX
};

// Thing variables.
enum
{
   ACS_THINGVAR_Health       =  0,
   ACS_THINGVAR_Speed        =  1,
   ACS_THINGVAR_Damage       =  2,
   ACS_THINGVAR_Alpha        =  3,
   ACS_THINGVAR_RenderStyle  =  4,
   ACS_THINGVAR_SeeSound     =  5,
   ACS_THINGVAR_AttackSound  =  6,
   ACS_THINGVAR_PainSound    =  7,
   ACS_THINGVAR_DeathSound   =  8,
   ACS_THINGVAR_ActiveSound  =  9,
   ACS_THINGVAR_Ambush       = 10,
   ACS_THINGVAR_Invulnerable = 11,
   ACS_THINGVAR_JumpZ        = 12,
   ACS_THINGVAR_ChaseGoal    = 13,
   ACS_THINGVAR_Frightened   = 14,
   ACS_THINGVAR_Gravity      = 15,
   ACS_THINGVAR_Friendly     = 16,
   ACS_THINGVAR_SpawnHealth  = 17,
   ACS_THINGVAR_Dropped      = 18,
   ACS_THINGVAR_NoTarget     = 19,
   ACS_THINGVAR_Species      = 20,
   ACS_THINGVAR_NameTag      = 21,
   ACS_THINGVAR_Score        = 22,
   ACS_THINGVAR_NoTrigger    = 23,
   ACS_THINGVAR_DamageFactor = 24,
   ACS_THINGVAR_MasterTID    = 25,
   ACS_THINGVAR_TargetTID    = 26,
   ACS_THINGVAR_TracerTID    = 27,
   ACS_THINGVAR_WaterLevel   = 28,
   ACS_THINGVAR_ScaleX       = 29,
   ACS_THINGVAR_ScaleY       = 30,
   ACS_THINGVAR_Dormant      = 31,
   ACS_THINGVAR_Mass         = 32,
   ACS_THINGVAR_Accuracy     = 33,
   ACS_THINGVAR_Stamina      = 34,

   // Unexposed variables.
   ACS_THINGVAR_Angle,
   ACS_THINGVAR_Armor,
   ACS_THINGVAR_CeilingZ,
   ACS_THINGVAR_FloorZ,
   ACS_THINGVAR_Frags,
   ACS_THINGVAR_PlayerNumber,
   ACS_THINGVAR_SigilPieces,
   ACS_THINGVAR_TID,
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

typedef void (*acs_func_t)(ACS_FUNCARG);

//
// ACSArray
//
// Stores an "array" with a logical size of 2^32, but is actually only
// allocated as needed.
//
class ACSArray
{
private:
   typedef int32_t val_t;
   typedef val_t  page_t[ACS_PAGESIZE];
   typedef page_t *block_t[ACS_BLOCKSIZE];
   typedef block_t *region_t[ACS_REGIONSIZE];
   typedef region_t *arrdata_t[ACS_ARRDATASIZE];

   void archiveArrdata(SaveArchive &arc, arrdata_t *arrdata);
   void archiveRegion(SaveArchive &arc, region_t *region);
   void archiveBlock(SaveArchive &arc, block_t *block);
   void archivePage(SaveArchive &arc, page_t *page);
   void archiveVal(SaveArchive &arc, val_t *val);

   arrdata_t &getArrdata() {return arrdata;}
   region_t &getRegion(uint32_t addr);
   block_t &getBlock(uint32_t addr);
   page_t &getPage(uint32_t addr);
   val_t &getVal(uint32_t addr)
   {
      return getPage(addr / ACS_PAGESIZE)[addr % ACS_PAGESIZE];
   }

   arrdata_t arrdata;

public:
   ACSArray() {memset(arrdata, 0, sizeof(arrdata));}
   ~ACSArray() {clear();}

   void clear();

   val_t &at(uint32_t addr) {return getVal(addr);}

   val_t &operator [] (uint32_t addr) {return getVal(addr);}

   void archive(SaveArchive &arc);

   void print(qstring *printBuffer, uint32_t offset, uint32_t length = 0xFFFFFFFF);
};

//
// acs_opdata
//
struct acs_opdata_t
{
   acs_op_t op;
   int args;
};

//
// ACSFunc
//
// Like acscript below, but for user-defined functions.
//
class ACSFunc
{
public:
   uint8_t numArgs; // argument count
   uint8_t numVars; // variable count
   uint8_t numRetn; // return count
   ACSVM *vm;       // VM the function is from

   union
   {
      uint32_t codeIndex;
      int32_t *codePtr;
   };
};

//
// acscript
//
// An actual script entity as read from the ACS lump's entry table,
// augmented with basic runtime data.
//
typedef struct acscript_s
{
   int32_t     number;  // the script's number; negative means named
   uint32_t    numArgs; // expected arguments
   uint16_t    numVars; // local variable count
   uint16_t    flags;   // script flags
   acs_stype_t type;    // script type

   union
   {
      uint32_t codeIndex;
      int32_t *codePtr;
   };

   ACSVM      *vm;      // VM the script is associated with
   ACSThinker *threads;
} acscript_t;

//
// acs_call_t
//
// Stores a call-frame for ACS execution.
//
struct acs_call_t
{
   int32_t *ip;
   int32_t *locals;
   uint32_t numLocals;
   qstring *printBuffer;
   ACSVM   *vm;
};

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
   ACSThinker() : result(0), calls(NULL), callPtr(NULL), numCalls(0),
                  printBuffer(NULL)
   {
   }

   // Methods
   virtual void serialize(SaveArchive &arc);
   virtual void deSwizzle();

   // Data Members
   // thread links
   ACSThinker **prevthread;
   ACSThinker  *nextthread;

   // virtual machine data
   int32_t     result;                // user-defined result of the thread
   int32_t    *ip;                    // instruction pointer
   int32_t     stack[ACS_STACK_LEN];  // value stack
   int32_t     stp;                   // stack pointer
   int32_t    *locals;                // local variables and arguments
   uint32_t    numLocals;             // number of local variables
   int32_t     sreg;                  // state register
   int32_t     sdata;                 // special data for state
   acs_call_t *calls;                 // call frames
   acs_call_t *callPtr;               // current call frame
   uint32_t    numCalls;              // number of call frames
   qstring    *printBuffer;           // print buffer

   // info copied from acscript and acsvm
   acscript_t *acscript;              // the script being executed
   ACSVM      *vm;                    // the current execution environment

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

   ACSFunc *findFunction(const char *name);

   int32_t *findMapVar(const char *name);
   ACSArray *findMapArr(const char *name);

   // Resets the VM to an uninitialized state. Only useful for static VMs.
   void reset();

   // bytecode info
   int32_t     *code;                  // ACS code; jumps are relative to this
   unsigned int numCode;
   unsigned int strings;               // offset into global table
   unsigned int numStrings;
   acscript_t  *scripts;               // the scripts
   unsigned int numScripts;
   const char **scriptNames;           // script names
   unsigned int numScriptNames;
   bool         loaded;                // for static VMs, if it's valid or not
   uint32_t     id;                    // vm id number
   int          lump;                  // lump bytecode was loaded from

   // interpreter info
   ACSFunc **funcptrs;                 // pointers into vm funcdat
   ACSFunc  *funcs;                    // functions local to this vm
   uint32_t  numFuncs;                 // number of functions
   int32_t  *mapvtab[ACS_NUM_MAPVARS]; // pointers into vm mapvars
   int32_t   mapvars[ACS_NUM_MAPVARS]; // map variables local to this vm
   ACSArray *mapatab[ACS_NUM_MAPARRS]; // pointers into vm maparrs
   ACSArray  maparrs[ACS_NUM_MAPARRS]; // map arrays local to this vm

   // loader info (not valid post-loading)
   // should this be a separate struct, freed after loading?
   const char **exports;                  // exported variables
   unsigned int numExports;               // number of exports
   const char **imports;                  // imported lump names
   ACSVM      **importVMs;                // imported VMs
   unsigned int numImports;               // number of imports
   const char **funcNames;                // function names
   unsigned int numFuncNames;             // number of function names
   const char  *mapvnam[ACS_NUM_MAPVARS]; // map variable names
   const char  *mapanam[ACS_NUM_MAPARRS]; // map array names
   uint32_t     mapalen[ACS_NUM_MAPARRS]; // map array lengths
   bool         mapahas[ACS_NUM_MAPARRS]; // if true, index is declared as array

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
ACSVM *ACS_LoadScript(WadDirectory *dir, int lump);
void ACS_LoadScript(ACSVM *vm, WadDirectory *dir, int lump);
void ACS_LoadScriptACS0(ACSVM *vm, WadDirectory *dir, int lump, byte *data);
void ACS_LoadScriptACSE(ACSVM *vm, WadDirectory *dir, int lump, byte *data,
                        uint32_t tableOffset = 4);
void ACS_LoadScriptACSe(ACSVM *vm, WadDirectory *dir, int lump, byte *data,
                        uint32_t tableOffset = 4);
void ACS_LoadScriptCodeACS0(ACSVM *vm, byte *data, uint32_t lumpLength, bool compressed);
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

// extern vars.

extern acs_func_t ACSfunc[ACS_FUNCMAX];
extern acs_opdata_t ACSopdata[ACS_OPMAX];

extern int ACS_thingtypes[ACS_NUM_THINGTYPES];

extern int32_t ACSworldvars[ACS_NUM_WORLDVARS];
extern ACSArray ACSworldarrs[ACS_NUM_WORLDARRS];
extern int32_t ACSglobalvars[ACS_NUM_GLOBALVARS];
extern ACSArray ACSglobalarrs[ACS_NUM_GLOBALARRS];

#endif

// EOF


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
// Improved by David Hill
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "a_small.h"
#include "acs_intr.h"
#include "c_runcmd.h"
#include "doomstat.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "m_collection.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "p_info.h"
#include "p_maputl.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "r_state.h"
#include "v_misc.h"
#include "w_wad.h"

//
// Local Enumerations
//

// script states for sreg
enum
{
   ACS_STATE_STOPPED,    // not running
   ACS_STATE_RUNNING,    // currently running
   ACS_STATE_SUSPEND,    // suspended by instruction
   ACS_STATE_WAITTAG,    // waiting on a tag
   ACS_STATE_WAITSCRIPT, // waiting on a script
   ACS_STATE_WAITPOLY,   // waiting on a polyobject
   ACS_STATE_TERMINATE,  // will be stopped on next thinking turn
};

//
// Static Variables
//

// deferred scripts
static DLListItem<deferredacs_t> *acsDeferred;

// haleyjd 06/24/08: level script vm for ACS
static ACSVM acsLevelScriptVM;

static PODCollection<ACSVM *> acsVMs;


//
// Global Variables
//

acs_opdata_t ACSopdata[ACS_OPMAX] =
{
   #define ACS_OP(OP,ARGC) {ACS_OP_##OP, ARGC},
   #include "acs_op.h"
   #undef ACS_OP
};


const char **ACSVM::GlobalStrings = NULL;
unsigned int ACSVM::GlobalNumStrings = 0;

// ACS_thingtypes:
// This array translates from ACS spawn numbers to internal thingtype indices.
// ACS spawn numbers are specified via EDF and are gamemode-dependent. EDF takes
// responsibility for populating this list.

int ACS_thingtypes[ACS_NUM_THINGTYPES];

// world variables
int32_t ACSworldvars[ACS_NUM_WORLDVARS];
ACSArray ACSworldarrs[ACS_NUM_WORLDARRS];

// global variables
int32_t ACSglobalvars[ACS_NUM_GLOBALVARS];
ACSArray ACSglobalarrs[ACS_NUM_GLOBALARRS];

//
// Static Functions
//

//
// ACS_addVirtualMachine
//
// haleyjd 06/24/08: keeps track of all ACS virtual machines.
//
static void ACS_addVirtualMachine(ACSVM *vm)
{
   vm->id = acsVMs.getLength();
   acsVMs.add(vm);
}

static bool ACS_addDeferredScriptVM(ACSVM *vm, int scrnum, int mapnum, 
                                    int type, int args[5]);

//
// ACS_addThread
//
// Adds a thinker as a thread on the given script.
//
static void ACS_addThread(ACSThinker *script, acscript_t *acscript)
{
   ACSThinker *next = acscript->threads;

   if((script->nextthread = next))
      next->prevthread = &script->nextthread;
   script->prevthread = &acscript->threads;
   acscript->threads = script;
}

//
// ACS_removeThread
//
// Removes a thinker from the acscript thread list.
//
static void ACS_removeThread(ACSThinker *script)
{
   ACSThinker **prev = script->prevthread;
   ACSThinker  *next = script->nextthread;
   
   if((*prev = next))
      next->prevthread = prev;
}

//
// ACS_scriptFinished
//
// Called when a script stops executing. Looks for other scripts in the
// same VM which are waiting on this one. All threads of the specified
// script must be terminated first.
//
static void ACS_scriptFinished(ACSThinker *script)
{
   unsigned int i;
   ACSVM      *vm       = script->vm;
   acscript_t *acscript = script->acscript;
   ACSThinker *th;

   // first check that all threads of the same script have terminated
   if(acscript->threads)
      return; // nope

   // find scripts waiting on this one (same VM only)
   
   // loop on vm scripts array
   for(i = 0; i < vm->numScripts; ++i)
   {
      // loop on threads list for each script
      for(th = vm->scripts[i].threads; th; th = th->nextthread)
      {
         if(th->sreg  == ACS_STATE_WAITSCRIPT &&
            th->sdata == acscript->number)
         {
            // wake up the script
            th->sreg  = ACS_STATE_RUNNING;
            th->sdata = 0;
         }
      }
   }
}

//
// ACS_stopScript
//
// Ultimately terminates the script and removes its thinker.
//
static void ACS_stopScript(ACSThinker *script, acscript_t *acscript)
{
   ACS_removeThread(script);
   
   // notify waiting scripts that this script has ended
   ACS_scriptFinished(script);

   P_SetTarget<Mobj>(&script->trigger, NULL);

   script->removeThinker();
}

//
// ACS_runOpenScript
//
// Starts an open script (a script indicated to start at the beginning of
// the level).
//
static void ACS_runOpenScript(ACSVM *vm, acscript_t *acs)
{
   ACSThinker *newScript = new ACSThinker;

   // open scripts wait one second before running
   newScript->delay = TICRATE;

   // set ip to entry point
   newScript->ip        = acs->codePtr;
   newScript->numLocals = acs->numVars;
   newScript->locals    = estructalloctag(int32_t, newScript->numLocals, PU_LEVEL);

   // copy in some important data
   newScript->acscript = acs;
   newScript->vm       = vm;

   // set up thinker
   newScript->addThinker();

   // mark as running
   newScript->sreg = ACS_STATE_RUNNING;
   ACS_addThread(newScript, acs);
}

//
// ACS_indexForNum
//
// Returns the index of the script in the "scripts" array given its number
// from within the script itself. Returns vm->numScripts if no such script
// exists.
//
// Currently uses a linear search, since the set is small and fixed-size.
//
unsigned int ACS_indexForNum(ACSVM *vm, int num)
{
   unsigned int idx;

   for(idx = 0; idx < vm->numScripts; ++idx)
      if(vm->scripts[idx].number == num)
         break;

   return idx;
}

//
// ACS_execLineSpec
//
// Executes a line special that has been encoded in the script with
// immediate operands or on the stack.
//
static int32_t ACS_execLineSpec(line_t *l, Mobj *mo, int16_t spec, int side,
                                int argc, int *argv)
{
   int args[NUMLINEARGS] = { 0, 0, 0, 0, 0 };
   int i = argc;

   // args follow instruction in the code from first to last
   for(; i > 0; --i)
      args[argc-i] = *argv++;

   // translate line specials & args for Hexen maps
   P_ConvertHexenLineSpec(&spec, args);

   return P_ExecParamLineSpec(l, mo, spec, args, side, SPAC_CROSS, true);
}

//
// ACS_countPlayers
//
// Returns a count of active players.
//
static int ACS_countPlayers(void)
{
   int i, count = 0;

   for(i = 0; i < MAXPLAYERS; ++i)
      if(playeringame[i])
         ++count;

   return count;
}

//
// ACS_getCVar
//
static int32_t ACS_getCVar(const char *name)
{
   command_t *command;
   variable_t *var;

   command = C_GetCmdForName(name);

   if(!command || !(var = command->variable))
      return 0;

   switch(var->type)
   {
   case vt_int: return *(int *)var->variable;
   case vt_float: return M_DoubleToFixed(*(double *)var->variable);
   case vt_string: return 0; // TODO: DynaStrings
   case vt_chararray: return 0; // TODO: DynaStrings
   case vt_toggle: return *(bool *)var->variable;
   }

   return 0;
}

//
// ACS_getLevelVar
//
static int32_t ACS_getLevelVar(uint32_t var)
{
   switch(var)
   {
   case ACS_LEVELVAR_ParTime:        return LevelInfo.partime;
   case ACS_LEVELVAR_ClusterNumber:  return 0;
   case ACS_LEVELVAR_LevelNumber:    return 0;
   case ACS_LEVELVAR_TotalSecrets:   return wminfo.maxsecret;
   case ACS_LEVELVAR_FoundSecrets:   return players[consoleplayer].secretcount;
   case ACS_LEVELVAR_TotalItems:     return wminfo.maxitems;
   case ACS_LEVELVAR_FoundItems:     return players[consoleplayer].itemcount;
   case ACS_LEVELVAR_TotalMonsters:  return wminfo.maxkills;
   case ACS_LEVELVAR_KilledMonsters: return players[consoleplayer].killcount;
   case ACS_LEVELVAR_SuckTime:       return 1;

   default: return 0;
   }
}

//
// ACS_getThingVar
//
static int32_t ACS_getThingVar(Mobj *thing, uint32_t var)
{
   if(!thing) return 0;

   switch(var)
   {
   case ACS_THINGVAR_Health:       return thing->health;
   case ACS_THINGVAR_Speed:        return thing->info->speed;
   case ACS_THINGVAR_Damage:       return thing->damage;
   case ACS_THINGVAR_Alpha:        return thing->translucency;
   case ACS_THINGVAR_RenderStyle:  return 0;
   case ACS_THINGVAR_SeeSound:     return 0;
   case ACS_THINGVAR_AttackSound:  return 0;
   case ACS_THINGVAR_PainSound:    return 0;
   case ACS_THINGVAR_DeathSound:   return 0;
   case ACS_THINGVAR_ActiveSound:  return 0;
   case ACS_THINGVAR_Ambush:       return !!(thing->flags & MF_AMBUSH);
   case ACS_THINGVAR_Invulnerable: return !!(thing->flags2 & MF2_INVULNERABLE);
   case ACS_THINGVAR_JumpZ:        return 0;
   case ACS_THINGVAR_ChaseGoal:    return 0;
   case ACS_THINGVAR_Frightened:   return 0;
   case ACS_THINGVAR_Friendly:     return !!(thing->flags & MF_FRIEND);
   case ACS_THINGVAR_SpawnHealth:  return thing->info->spawnhealth;
   case ACS_THINGVAR_Dropped:      return !!(thing->flags & MF_DROPPED);
   case ACS_THINGVAR_NoTarget:     return 0;
   case ACS_THINGVAR_Species:      return 0;
   case ACS_THINGVAR_NameTag:      return 0;
   case ACS_THINGVAR_Score:        return 0;
   case ACS_THINGVAR_NoTrigger:    return 0;
   case ACS_THINGVAR_DamageFactor: return 0;
   case ACS_THINGVAR_MasterTID:    return 0;
   case ACS_THINGVAR_TargetTID:    return thing->target ? thing->target->tid : 0;
   case ACS_THINGVAR_TracerTID:    return thing->tracer ? thing->tracer->tid : 0;
   case ACS_THINGVAR_WaterLevel:   return 0;
   case ACS_THINGVAR_ScaleX:       return M_FloatToFixed(thing->xscale);
   case ACS_THINGVAR_ScaleY:       return M_FloatToFixed(thing->yscale);
   case ACS_THINGVAR_Dormant:      return !!(thing->flags2 & MF2_DORMANT);
   case ACS_THINGVAR_Mass:         return thing->info->mass;
   case ACS_THINGVAR_Accuracy:     return 0;
   case ACS_THINGVAR_Stamina:      return 0;

   case ACS_THINGVAR_Angle:        return thing->angle >> 16;
   case ACS_THINGVAR_Armor:        return thing->player ? thing->player->armorpoints : 0;
   case ACS_THINGVAR_CeilingZ:     return thing->ceilingz;
   case ACS_THINGVAR_FloorZ:       return thing->floorz;
   case ACS_THINGVAR_Frags:        return thing->player ? thing->player->totalfrags : 0;
   case ACS_THINGVAR_PlayerNumber: return thing->player ? thing->player - players : -1;
   case ACS_THINGVAR_SigilPieces:  return 0;
   case ACS_THINGVAR_TID:          return thing->tid;
   case ACS_THINGVAR_X:            return thing->x;
   case ACS_THINGVAR_Y:            return thing->y;
   case ACS_THINGVAR_Z:            return thing->z;

   default: return 0;
   }
}

//
// ACS_setThingVar
//
static void ACS_setThingVar(Mobj *thing, uint32_t var, int32_t val)
{
   switch(var)
   {
   case ACS_THINGVAR_Health:       thing->health = val; break;
   case ACS_THINGVAR_Speed:        break;
   case ACS_THINGVAR_Damage:       thing->damage = val; break;
   case ACS_THINGVAR_Alpha:        thing->translucency = val; break;
   case ACS_THINGVAR_RenderStyle:  break;
   case ACS_THINGVAR_SeeSound:     break;
   case ACS_THINGVAR_AttackSound:  break;
   case ACS_THINGVAR_PainSound:    break;
   case ACS_THINGVAR_DeathSound:   break;
   case ACS_THINGVAR_ActiveSound:  break;
   case ACS_THINGVAR_Ambush:       if(val) thing->flags |=  MF_AMBUSH;
                                   else    thing->flags &= ~MF_AMBUSH; break;
   case ACS_THINGVAR_Invulnerable: if(val) thing->flags2 |=  MF2_INVULNERABLE;
                                   else    thing->flags2 &= ~MF2_INVULNERABLE; break;
   case ACS_THINGVAR_JumpZ:        break;
   case ACS_THINGVAR_ChaseGoal:    break;
   case ACS_THINGVAR_Frightened:   break;
   case ACS_THINGVAR_Friendly:     if(val) thing->flags |=  MF_FRIEND;
                                   else    thing->flags &= ~MF_FRIEND; break;
   case ACS_THINGVAR_SpawnHealth:  break;
   case ACS_THINGVAR_Dropped:      if(val) thing->flags |=  MF_DROPPED;
                                   else    thing->flags &= ~MF_DROPPED; break;
   case ACS_THINGVAR_NoTarget:     break;
   case ACS_THINGVAR_Species:      break;
   case ACS_THINGVAR_NameTag:      break;
   case ACS_THINGVAR_Score:        break;
   case ACS_THINGVAR_NoTrigger:    break;
   case ACS_THINGVAR_DamageFactor: break;
   case ACS_THINGVAR_MasterTID:    break;
   case ACS_THINGVAR_TargetTID:    P_SetTarget(&thing->target, P_FindMobjFromTID(val, 0, 0)); break;
   case ACS_THINGVAR_TracerTID:    P_SetTarget(&thing->tracer, P_FindMobjFromTID(val, 0, 0)); break;
   case ACS_THINGVAR_WaterLevel:   break;
   case ACS_THINGVAR_ScaleX:       thing->xscale = M_FixedToFloat(val); break;
   case ACS_THINGVAR_ScaleY:       thing->yscale = M_FixedToFloat(val); break;
   case ACS_THINGVAR_Dormant:      if(val) thing->flags2 |=  MF2_DORMANT;
                                   else    thing->flags2 &= ~MF2_DORMANT; break;
   case ACS_THINGVAR_Mass:         break;
   case ACS_THINGVAR_Accuracy:     break;
   case ACS_THINGVAR_Stamina:      break;

   case ACS_THINGVAR_Angle:        thing->angle = val << 16; break;
   case ACS_THINGVAR_Armor:        break;
   case ACS_THINGVAR_CeilingZ:     break;
   case ACS_THINGVAR_FloorZ:       break;
   case ACS_THINGVAR_Frags:        break;
   case ACS_THINGVAR_PlayerNumber: break;
   case ACS_THINGVAR_SigilPieces:  break;
   case ACS_THINGVAR_TID:          P_RemoveThingTID(thing); P_AddThingTID(thing, val); break;
   case ACS_THINGVAR_X:            thing->x = val; break;
   case ACS_THINGVAR_Y:            thing->y = val; break;
   case ACS_THINGVAR_Z:            thing->z = val; break;
   }
}


//
// Global Functions
//


// Interpreter Macros

// This sure would be nice to enable! (But maybe a better check?)
#if defined(__GNUC__)
#define COMPGOTO
#endif

#define PUSH(x)   (*stp++ = (x))
#define POP()     (*--stp)
#define PEEK()    (*(stp-1))
#define DECSTP()  (--stp)
#define INCSTP()  (++stp)

#define IPCURR()  (*ip)
#define IPNEXT()  (*ip++)

#define STACK_AT(x) (*(stp-(x)))

// for binary operations: ++ and -- mess these up
#define ST_BINOP(OP) temp = POP(); STACK_AT(1) = (STACK_AT(1) OP temp)
#define ST_BINOP_EQ(OP) temp = POP(); STACK_AT(1) OP temp

#define AR_BINOP(VAR, OP) temp = POP(); (VAR)[POP()] OP temp

#define DIV_CHECK(VAL) \
   if(!(VAL)) \
   { \
      doom_printf(FC_ERROR "ACS Error: divide by zero\a"); \
      goto action_endscript; \
   } \


#define DIVOP_EQ(VAR, OP) DIV_CHECK(temp = POP()); (VAR) OP temp

#define BRANCH_COUNT() \
   if(++count > 500000) \
   { \
      doom_printf(FC_ERROR "ACS Error: terminated runaway script\a"); \
      goto action_endscript; \
   }

// Uses do...while for convenience of use.
#define BRANCHOP(TARGET) \
   do \
   { \
      ip = vm->code + (TARGET); \
      BRANCH_COUNT(); \
   } \
   while(0)

#ifdef COMPGOTO
#define OPCODE(OP) acs_op_##OP
#define NEXTOP() goto *ops[IPNEXT()]
#else
#define OPCODE(OP) case ACS_OP_##OP
#define NEXTOP() break
#endif

IMPLEMENT_THINKER_TYPE(ACSThinker)

//
// T_ACSThinker
//
// Function for acs thinkers. Runs the script by interpreting its bytecode
// until the script terminates, is suspended, or waits on some condition.
//
void ACSThinker::Think()
{
#ifdef COMPGOTO
   static const void *const ops[ACS_OPMAX] =
   {
      #define ACS_OP(OP,ARGC) &&acs_op_##OP,
      #include "acs_op.h"
      #undef ACS_OP
   };
#endif

   // cache vm data in local vars for efficiency
   register int32_t *ip  = this->ip;
   register int32_t *stp = this->stack + this->stp;
   int count = 0;
   uint32_t opcode;
   int32_t temp;

   // should the script terminate?
   if(this->sreg == ACS_STATE_TERMINATE)
      ACS_stopScript(this, this->acscript);

   // is the script running?
   if(this->sreg != ACS_STATE_RUNNING)
      return;

   // check for delays
   if(this->delay)
   {
      --this->delay;
      return;
   }

   // run opcodes until a terminating instruction is reached
#ifdef COMPGOTO
   NEXTOP();
#else
   for(;;) switch(IPNEXT())
#endif
   {
   OPCODE(NOP):
      NEXTOP();

   OPCODE(KILL):
      doom_printf(FC_ERROR "ACS Error: KILL at %d\a", (int)(ip - vm->code - 1));
      goto action_endscript;

      // Special Commands
   OPCODE(CALLFUNC):
      opcode = IPNEXT(); // read special
      temp = IPNEXT(); // read argcount
      stp -= temp; // consume args
      ACSfunc[opcode](this, temp, stp, stp);
      NEXTOP();
   OPCODE(CALLFUNC_IMM):
      opcode = IPNEXT(); // read special
      temp = IPNEXT(); // read argcount
      ACSfunc[opcode](this, temp, ip, stp);
      ip += temp; // consume args
      NEXTOP();

   OPCODE(LINESPEC):
      opcode = IPNEXT(); // read special
      temp = IPNEXT(); // read argcount
      stp -= temp; // consume args
      ACS_execLineSpec(line, trigger, (int16_t)opcode, lineSide, temp, stp);
      NEXTOP();
   OPCODE(LINESPEC_IMM):
      opcode = IPNEXT(); // read special
      temp = IPNEXT(); // read argcount
      ACS_execLineSpec(line, trigger, (int16_t)opcode, lineSide, temp, ip);
      ip += temp; // consume args
      NEXTOP();
   OPCODE(LINESPEC_RET):
      opcode = IPNEXT(); // read special
      temp = IPNEXT(); // read argcount
      stp -= temp; // consume args
      temp = ACS_execLineSpec(line, trigger, (int16_t)opcode, lineSide, temp, stp);
      PUSH(temp);
      NEXTOP();

      // SET
   OPCODE(SET_RESULT):
      result = POP();
      NEXTOP();
   OPCODE(SET_LOCALVAR):
      this->locals[IPNEXT()] = POP();
      NEXTOP();
   OPCODE(SET_MAPVAR):
      *vm->mapvtab[IPNEXT()] = POP();
      NEXTOP();
   OPCODE(SET_WORLDVAR):
      ACSworldvars[IPNEXT()] = POP();
      NEXTOP();
   OPCODE(SET_GLOBALVAR):
      ACSglobalvars[IPNEXT()] = POP();
      NEXTOP();
   OPCODE(SET_MAPARR):
      AR_BINOP(*vm->mapatab[IPNEXT()], =);
      NEXTOP();
   OPCODE(SET_WORLDARR):
      AR_BINOP(ACSworldarrs[IPNEXT()], =);
      NEXTOP();
   OPCODE(SET_GLOBALARR):
      AR_BINOP(ACSglobalarrs[IPNEXT()], =);
      NEXTOP();

   OPCODE(SET_THINGVAR):
      {
         temp    = POP();
         opcode  = IPNEXT();
         int tid = POP();
         Mobj *mo = NULL;

         while((mo = P_FindMobjFromTID(tid, NULL, trigger)))
            ACS_setThingVar(mo, opcode, temp);
      }
      NEXTOP();
   OPCODE(SET_THINGARR):
      {
         temp    = POP();
         opcode  = POP();
         int tid = POP();
         Mobj *mo = NULL;

         while((mo = P_FindMobjFromTID(tid, NULL, trigger)))
            ACS_setThingVar(mo, opcode, temp);
      }
      NEXTOP();

      // GET
   OPCODE(GET_IMM):
      PUSH(IPNEXT());
      NEXTOP();
   OPCODE(GET_LOCALVAR):
      PUSH(this->locals[IPNEXT()]);
      NEXTOP();
   OPCODE(GET_MAPVAR):
      PUSH(*vm->mapvtab[IPNEXT()]);
      NEXTOP();
   OPCODE(GET_WORLDVAR):
      PUSH(ACSworldvars[IPNEXT()]);
      NEXTOP();
   OPCODE(GET_GLOBALVAR):
      PUSH(ACSglobalvars[IPNEXT()]);
      NEXTOP();
   OPCODE(GET_MAPARR):
      STACK_AT(1) = vm->mapatab[IPNEXT()]->at(STACK_AT(1));
      NEXTOP();
   OPCODE(GET_WORLDARR):
      STACK_AT(1) = ACSworldarrs[IPNEXT()][STACK_AT(1)];
      NEXTOP();
   OPCODE(GET_GLOBALARR):
      STACK_AT(1) = ACSglobalarrs[IPNEXT()][STACK_AT(1)];
      NEXTOP();

   OPCODE(GET_THINGVAR):
      STACK_AT(1) = ACS_getThingVar(P_FindMobjFromTID(STACK_AT(1), NULL, trigger), IPNEXT());
      NEXTOP();
   OPCODE(GET_THINGARR):
      temp = POP();
      STACK_AT(1) = ACS_getThingVar(P_FindMobjFromTID(STACK_AT(1), NULL, trigger), temp);
      NEXTOP();

   OPCODE(GET_LEVELARR):
      STACK_AT(1) = ACS_getLevelVar(STACK_AT(1));
      NEXTOP();

   OPCODE(GET_CVAR):
      STACK_AT(1) = ACS_getCVar(ACSVM::GetString(STACK_AT(1)));
      NEXTOP();

      // GETARR
   OPCODE(GETARR_IMM):
      for(temp = IPNEXT(); temp--;) PUSH(IPNEXT());
      NEXTOP();

      // Binary Ops
      // ADD
   OPCODE(ADD_STACK):
      ST_BINOP_EQ(+=);
      NEXTOP();
   OPCODE(ADD_LOCALVAR):
      this->locals[IPNEXT()] += POP();
      NEXTOP();
   OPCODE(ADD_MAPVAR):
      *vm->mapvtab[IPNEXT()] += POP();
      NEXTOP();
   OPCODE(ADD_WORLDVAR):
      ACSworldvars[IPNEXT()] += POP();
      NEXTOP();
   OPCODE(ADD_GLOBALVAR):
      ACSglobalvars[IPNEXT()] += POP();
      NEXTOP();
   OPCODE(ADD_MAPARR):
      AR_BINOP(*vm->mapatab[IPNEXT()], +=);
      NEXTOP();
   OPCODE(ADD_WORLDARR):
      AR_BINOP(ACSworldarrs[IPNEXT()], +=);
      NEXTOP();
   OPCODE(ADD_GLOBALARR):
      AR_BINOP(ACSglobalarrs[IPNEXT()], +=);
      NEXTOP();

      // AND
   OPCODE(AND_STACK):
      ST_BINOP_EQ(&=);
      NEXTOP();

      // CMP
   OPCODE(CMP_EQ):
      ST_BINOP(==);
      NEXTOP();
   OPCODE(CMP_NE):
      ST_BINOP(!=);
      NEXTOP();
   OPCODE(CMP_LT):
      ST_BINOP(<);
      NEXTOP();
   OPCODE(CMP_GT):
      ST_BINOP(>);
      NEXTOP();
   OPCODE(CMP_LE):
      ST_BINOP(<=);
      NEXTOP();
   OPCODE(CMP_GE):
      ST_BINOP(>=);
      NEXTOP();

      // DEC
   OPCODE(DEC_LOCALVAR):
      --this->locals[IPNEXT()];
      NEXTOP();
   OPCODE(DEC_MAPVAR):
      --*vm->mapvtab[IPNEXT()];
      NEXTOP();
   OPCODE(DEC_WORLDVAR):
      --ACSworldvars[IPNEXT()];
      NEXTOP();
   OPCODE(DEC_GLOBALVAR):
      --ACSglobalvars[IPNEXT()];
      NEXTOP();
   OPCODE(DEC_MAPARR):
      --vm->mapatab[IPNEXT()]->at(POP());
      NEXTOP();
   OPCODE(DEC_WORLDARR):
      --ACSworldarrs[IPNEXT()][POP()];
      NEXTOP();
   OPCODE(DEC_GLOBALARR):
      --ACSglobalarrs[IPNEXT()][POP()];
      NEXTOP();

      // DIV
   OPCODE(DIV_STACK):
      DIVOP_EQ(STACK_AT(1), /=);
      NEXTOP();
   OPCODE(DIV_LOCALVAR):
      DIVOP_EQ(this->locals[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIV_MAPVAR):
      DIVOP_EQ(*vm->mapvtab[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIV_WORLDVAR):
      DIVOP_EQ(ACSworldvars[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIV_GLOBALVAR):
      DIVOP_EQ(ACSglobalvars[IPNEXT()], /=);
      NEXTOP();
   OPCODE(DIV_MAPARR):
      DIVOP_EQ(vm->mapatab[IPNEXT()]->at(POP()), /=);
      NEXTOP();
   OPCODE(DIV_WORLDARR):
      DIVOP_EQ(ACSworldarrs[IPNEXT()][POP()], /=);
      NEXTOP();
   OPCODE(DIV_GLOBALARR):
      DIVOP_EQ(ACSglobalarrs[IPNEXT()][POP()], /=);
      NEXTOP();
   OPCODE(DIVX_STACK):
      DIV_CHECK(temp = POP()); STACK_AT(1) = FixedDiv(STACK_AT(1), temp);
      NEXTOP();

      // INC
   OPCODE(INC_LOCALVAR):
      ++this->locals[IPNEXT()];
      NEXTOP();
   OPCODE(INC_MAPVAR):
      ++*vm->mapvtab[IPNEXT()];
      NEXTOP();
   OPCODE(INC_WORLDVAR):
      ++ACSworldvars[IPNEXT()];
      NEXTOP();
   OPCODE(INC_GLOBALVAR):
      ++ACSglobalvars[IPNEXT()];
      NEXTOP();
   OPCODE(INC_MAPARR):
      ++vm->mapatab[IPNEXT()]->at(POP());
      NEXTOP();
   OPCODE(INC_WORLDARR):
      ++ACSworldarrs[IPNEXT()][POP()];
      NEXTOP();
   OPCODE(INC_GLOBALARR):
      ++ACSglobalarrs[IPNEXT()][POP()];
      NEXTOP();

      // IOR
   OPCODE(IOR_STACK):
      ST_BINOP_EQ(|=);
      NEXTOP();

      // LSH
   OPCODE(LSH_STACK):
      ST_BINOP_EQ(<<=);
      NEXTOP();

      // MOD
   OPCODE(MOD_STACK):
      DIVOP_EQ(STACK_AT(1), %=);
      NEXTOP();
   OPCODE(MOD_LOCALVAR):
      DIVOP_EQ(this->locals[IPNEXT()], %=);
      NEXTOP();
   OPCODE(MOD_MAPVAR):
      DIVOP_EQ(*vm->mapvtab[IPNEXT()], %=);
      NEXTOP();
   OPCODE(MOD_WORLDVAR):
      DIVOP_EQ(ACSworldvars[IPNEXT()], %=);
      NEXTOP();
   OPCODE(MOD_GLOBALVAR):
      DIVOP_EQ(ACSglobalvars[IPNEXT()], %=);
      NEXTOP();
   OPCODE(MOD_MAPARR):
      DIVOP_EQ(vm->mapatab[IPNEXT()]->at(POP()), %=);
      NEXTOP();
   OPCODE(MOD_WORLDARR):
      DIVOP_EQ(ACSworldarrs[IPNEXT()][POP()], %=);
      NEXTOP();
   OPCODE(MOD_GLOBALARR):
      DIVOP_EQ(ACSglobalarrs[IPNEXT()][POP()], %=);
      NEXTOP();

      // MUL
   OPCODE(MUL_STACK):
      ST_BINOP_EQ(*=);
      NEXTOP();
   OPCODE(MUL_LOCALVAR):
      this->locals[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MUL_MAPVAR):
      *vm->mapvtab[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MUL_WORLDVAR):
      ACSworldvars[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MUL_GLOBALVAR):
      ACSglobalvars[IPNEXT()] *= POP();
      NEXTOP();
   OPCODE(MUL_MAPARR):
      AR_BINOP(*vm->mapatab[IPNEXT()], *=);
      NEXTOP();
   OPCODE(MUL_WORLDARR):
      AR_BINOP(ACSworldarrs[IPNEXT()], *=);
      NEXTOP();
   OPCODE(MUL_GLOBALARR):
      AR_BINOP(ACSglobalarrs[IPNEXT()], *=);
      NEXTOP();
   OPCODE(MULX_STACK):
      temp = POP(); STACK_AT(1) = FixedMul(STACK_AT(1), temp);
      NEXTOP();

      // RSH
   OPCODE(RSH_STACK):
      ST_BINOP_EQ(>>=);
      NEXTOP();

      // SUB
   OPCODE(SUB_STACK):
      ST_BINOP_EQ(-=);
      NEXTOP();
   OPCODE(SUB_LOCALVAR):
      this->locals[IPNEXT()] -= POP();
      NEXTOP();
   OPCODE(SUB_MAPVAR):
      *vm->mapvtab[IPNEXT()] -= POP();
      NEXTOP();
   OPCODE(SUB_WORLDVAR):
      ACSworldvars[IPNEXT()] -= POP();
      NEXTOP();
   OPCODE(SUB_GLOBALVAR):
      ACSglobalvars[IPNEXT()] -= POP();
      NEXTOP();
   OPCODE(SUB_MAPARR):
      AR_BINOP(*vm->mapatab[IPNEXT()], -=);
      NEXTOP();
   OPCODE(SUB_WORLDARR):
      AR_BINOP(ACSworldarrs[IPNEXT()], -=);
      NEXTOP();
   OPCODE(SUB_GLOBALARR):
      AR_BINOP(ACSglobalarrs[IPNEXT()], -=);
      NEXTOP();

      // XOR
   OPCODE(XOR_STACK):
      ST_BINOP_EQ(^=);
      NEXTOP();

      // Unary Ops
   OPCODE(NEGATE_STACK):
      STACK_AT(1) = -STACK_AT(1);
      NEXTOP();

      // Logical Ops
   OPCODE(LOGAND_STACK):
      ST_BINOP(&&);
      NEXTOP();
   OPCODE(LOGIOR_STACK):
      ST_BINOP(||);
      NEXTOP();
   OPCODE(LOGNOT_STACK):
      STACK_AT(1) = !STACK_AT(1);
      NEXTOP();

      // Trigonometry Ops
   OPCODE(TRIG_COS):
      STACK_AT(1) = finecosine[(angle_t)(STACK_AT(1) << FRACBITS) >> ANGLETOFINESHIFT];
      NEXTOP();
   OPCODE(TRIG_SIN):
      STACK_AT(1) = finesine[(angle_t)(STACK_AT(1) << FRACBITS) >> ANGLETOFINESHIFT];
      NEXTOP();
   OPCODE(TRIG_VECTORANGLE):
      DECSTP();
      STACK_AT(1) = P_PointToAngle(0, 0, STACK_AT(1), STACK_AT(0));
      NEXTOP();

      // Branching
   OPCODE(BRANCH_CALL):
      {
         ACSFunc *func;
         opcode = IPNEXT();

         BRANCH_COUNT();

         if(opcode >= vm->numFuncs)
         {
            doom_printf(FC_ERROR "ACS Error: CALL out of range: %d\a", (int)opcode);
            goto action_endscript;
         }

         // Ensure there's enough space for the call frame.
         if((size_t)(callPtr - calls) >= numCalls)
         {
            size_t callPtrTemp = callPtr - calls;
            numCalls += ACS_NUM_CALLS;
            calls = (acs_call_t *)Z_Realloc(calls, numCalls * sizeof(acs_call_t),
                                            PU_LEVEL, NULL);
            callPtr = &calls[callPtrTemp];
         }

         func = vm->funcptrs[opcode];

         callPtr->ip          = ip;
         callPtr->numLocals   = numLocals;
         callPtr->locals      = locals;
         callPtr->printBuffer = printBuffer;
         callPtr->vm          = vm;

         ip          = func->codePtr;
         numLocals   = func->numVars + func->numArgs;
         locals      = estructalloc(int32_t, numLocals);
         printBuffer = NULL;
         vm          = func->vm;

         for(temp = func->numArgs; temp--;)
            locals[temp] = POP();

         ++callPtr;
      }
      NEXTOP();
   OPCODE(BRANCH_CASE):
      if(PEEK() == IPNEXT()) // compare top of stack against op+1
      {
         DECSTP(); // take the value off the stack
         BRANCHOP(IPCURR()); // jump to op+2
      }
      else
         ++ip; // increment past offset at op+2, leave value on stack
      NEXTOP();
   OPCODE(BRANCH_CASETABLE):
      {
         // Case data is a series if value-address pairs.
         typedef int32_t case_t[2];
         case_t *caseBegin, *caseEnd, *caseItr;

         temp = IPNEXT(); // Number of cases.

         caseBegin = (case_t *)ip;
         caseEnd = caseBegin + temp;

         ip = (int32_t *)caseEnd;

         // Search for matching case using binary search.
         if(temp) for(;;)
         {
            temp = caseEnd - caseBegin;
            caseItr = caseBegin + (temp / 2);

            if((*caseItr)[0] == PEEK())
            {
               DECSTP();
               BRANCHOP((*caseItr)[1]);
               break;
            }

            // If true, this was the last case to try.
            if(temp == 1) break;

            if((*caseItr)[0] < PEEK())
               caseBegin = caseItr;
            else
               caseEnd = caseItr;
         }
      }
      NEXTOP();
   OPCODE(BRANCH_IMM):
      BRANCHOP(IPCURR());
      NEXTOP();
   OPCODE(BRANCH_NOTZERO):
      if(POP())
         BRANCHOP(IPCURR());
      else
         ++ip;
      NEXTOP();
   OPCODE(BRANCH_RETURN):
      --callPtr;

      efree(locals);
      delete printBuffer;

      ip          = callPtr->ip;
      numLocals   = callPtr->numLocals;
      locals      = callPtr->locals;
      printBuffer = callPtr->printBuffer;
      vm          = callPtr->vm;

      NEXTOP();
   OPCODE(BRANCH_ZERO):
      if(!POP())
         BRANCHOP(IPCURR());
      else
         ++ip;
      NEXTOP();

      // Stack Control
   OPCODE(STACK_COPY):
      STACK_AT(0) = STACK_AT(1);
      INCSTP();
      NEXTOP();
   OPCODE(STACK_DROP):
      DECSTP();
      NEXTOP();
   OPCODE(STACK_SWAP):
      temp = STACK_AT(1);
      STACK_AT(1) = STACK_AT(2);
      STACK_AT(2) = temp;
      NEXTOP();

      // Script Control
   OPCODE(DELAY):
      this->delay = POP();
      goto action_stop;
   OPCODE(DELAY_IMM):
      this->delay = IPNEXT();
      goto action_stop;

   OPCODE(POLYWAIT):
      this->sreg  = ACS_STATE_WAITPOLY;
      this->sdata = POP(); // get poly tag
      goto action_stop;
   OPCODE(POLYWAIT_IMM):
      this->sreg  = ACS_STATE_WAITPOLY;
      this->sdata = IPNEXT(); // get poly tag
      goto action_stop;

   OPCODE(SCRIPT_RESTART):
      ip = acscript->codePtr;
      BRANCH_COUNT();
      NEXTOP();
   OPCODE(SCRIPT_SUSPEND):
      this->sreg = ACS_STATE_SUSPEND;
      goto action_stop;
   OPCODE(SCRIPT_TERMINATE):
      goto action_endscript;

   OPCODE(TAGWAIT):
      this->sreg  = ACS_STATE_WAITTAG;
      this->sdata = POP(); // get sector tag
      goto action_stop;
   OPCODE(TAGWAIT_IMM):
      this->sreg  = ACS_STATE_WAITTAG;
      this->sdata = IPNEXT(); // get sector tag
      goto action_stop;

   OPCODE(SCRIPTWAIT):
      this->sreg  = ACS_STATE_WAITSCRIPT;
      this->sdata = POP(); // get script num
      goto action_stop;
   OPCODE(SCRIPTWAIT_IMM):
      this->sreg  = ACS_STATE_WAITSCRIPT;
      this->sdata = IPNEXT(); // get script num
      goto action_stop;

      // Printing
   OPCODE(STARTPRINT):
      if(!printBuffer)
         printBuffer = new qstring(qstring::basesize, PU_LEVEL);
      else
         printBuffer->clear();
      NEXTOP();
   OPCODE(ENDPRINT):
      if(this->trigger && this->trigger->player)
         player_printf(trigger->player, printBuffer->constPtr());
      else
         player_printf(&players[consoleplayer], printBuffer->constPtr());
      NEXTOP();
   OPCODE(ENDPRINTBOLD):
      HU_CenterMsgTimedColor(printBuffer->constPtr(), FC_GOLD, 20*35);
      NEXTOP();
   OPCODE(ENDPRINTLOG):
      printf("%s\n", printBuffer->constPtr());
      NEXTOP();
   OPCODE(PRINTMAPARRAY):
      stp -= 2;
      vm->mapatab[stp[1]]->print(printBuffer, stp[0]);
      NEXTOP();
   OPCODE(PRINTWORLDARRAY):
      stp -= 2;
      ACSworldarrs[stp[1]].print(printBuffer, stp[0]);
      NEXTOP();
   OPCODE(PRINTGLOBALARRAY):
      stp -= 2;
      ACSglobalarrs[stp[1]].print(printBuffer, stp[0]);
      NEXTOP();
   OPCODE(PRINTCHAR):
      *printBuffer += (char)POP();
      NEXTOP();
   OPCODE(PRINTFIXED):
      {
         // %E worst case: -1.52587e-05 == 12 + NUL
         // %F worst case: -0.000106811 == 12 + NUL
         // %F worst case: -32768.9     ==  8 + NUL
         // %G is probably maximally P+6+1. (Actually, plus longer exponent.)
         char buffer[13];
         sprintf(buffer, "%G", M_FixedToDouble(POP()));
         printBuffer->concat(buffer);
      }
      NEXTOP();
   OPCODE(PRINTINT):
      {
         // %i worst case: -2147483648 == 11 + NUL
         char buffer[12];
         printBuffer->concat(M_Itoa(POP(), buffer, 10));
      }
      NEXTOP();
   OPCODE(PRINTNAME):
      temp = POP();
      switch(temp)
      {
      case 0:
         printBuffer->concat(players[consoleplayer].name);
         break;

      default:
         if(temp > 0 && temp <= MAXPLAYERS)
            printBuffer->concat(players[temp - 1].name);
         break;
      }
      NEXTOP();
   OPCODE(PRINTSTRING):
      printBuffer->concat(ACSVM::GetString(POP()));
      NEXTOP();

      // Miscellaneous

   OPCODE(CLEARLINESPECIAL):
      if(this->line)
         this->line->special = 0;
      NEXTOP();

   OPCODE(GAMESKILL):
      PUSH(gameskill);
      NEXTOP();

   OPCODE(GAMETYPE):
      PUSH(GameType);
      NEXTOP();

   OPCODE(GETSCREENHEIGHT):
      PUSH(video.height);
      NEXTOP();
   OPCODE(GETSCREENWIDTH):
      PUSH(video.width);
      NEXTOP();

   OPCODE(LINEOFFSETY):
      PUSH(line ? sides[line->sidenum[0]].rowoffset >> FRACBITS : 0);
      NEXTOP();
   OPCODE(LINESIDE):
      PUSH(this->lineSide);
      NEXTOP();

   OPCODE(PLAYERCOUNT):
      PUSH(ACS_countPlayers());
      NEXTOP();

   OPCODE(SETGRAVITY):
      LevelInfo.gravity = POP() / 800;
      NEXTOP();
   OPCODE(SETGRAVITY_IMM):
      LevelInfo.gravity = IPNEXT() / 800;
      NEXTOP();

   OPCODE(STRLEN):
      STACK_AT(1) = strlen(ACSVM::GetString(STACK_AT(1)));
      NEXTOP();
   OPCODE(TAGSTRING):
      STACK_AT(1) += vm->strings;
      NEXTOP();

   OPCODE(TIMER):
      PUSH(leveltime);
      NEXTOP();

#ifndef COMPGOTO
   default:
      // unknown opcode, must stop execution
      doom_printf(FC_ERROR "ACS Error: unknown opcode %d\a", opcode);
      goto action_endscript;
#endif
   }

action_endscript:
   // end the script
   ACS_stopScript(this, this->acscript);
   goto function_end;

action_stop:
   // copy fields back into script
   this->ip  = ip;
   this->stp = stp - this->stack;
   goto function_end;

function_end:;
}

//
// ACSArray::clear
//
// Frees all of the associated memory, effectively resetting all values to 0.
//
void ACSArray::clear()
{
   // Iterate over every region in arrdata.
   for(region_t **regionItr = arrdata, **regionEnd = regionItr + ACS_ARRDATASIZE;
       regionItr != regionEnd; ++regionItr)
   {
      // If this region isn't allocated, skip it.
      if(!*regionItr) continue;

      // Iterate over every block in the region. (**regionItr is region_t AKA block_t*[])
      for(block_t **blockItr = **regionItr, **blockEnd = blockItr + ACS_REGIONSIZE;
          blockItr != blockEnd; ++blockItr)
      {
         // If this block isn't allocated, skip it.
         if(!*blockItr) continue;

         // Iterate over every page in the block. (**blockItr is block_t AKA page_t*[])
         for(page_t **pageItr = **blockItr, **pageEnd = pageItr + ACS_BLOCKSIZE;
             pageItr != pageEnd; ++pageItr)
         {
            // If this page is allocated, free it.
            if(*pageItr) Z_Free(*pageItr);
         }

         // Free the block.
         Z_Free(*blockItr);
      }

      // Free the region.
      Z_Free(*regionItr);
      *regionItr = NULL;
   }
}

//
// ACSArray::getRegion
//
ACSArray::region_t &ACSArray::getRegion(uint32_t addr)
{
   // Find the requested region.
   region_t *&region = getArrdata()[addr];

   // If not allocated yet, do so.
   if(!region) region = estructalloc(region_t, 1);

   return *region;
}

//
// ACSArray::getBlock
//
ACSArray::block_t &ACSArray::getBlock(uint32_t addr)
{
   // Find the requested block.
   block_t *&block = getRegion(addr / ACS_REGIONSIZE)[addr % ACS_REGIONSIZE];

   // If not allocated yet, do so.
   if(!block) block = estructalloc(block_t, 1);

   return *block;
}

//
// ACSArray::getPage
//
ACSArray::page_t &ACSArray::getPage(uint32_t addr)
{
   // Find the requested page.
   page_t *&page = getBlock(addr / ACS_BLOCKSIZE)[addr % ACS_BLOCKSIZE];

   // If not allocated yet, do so.
   if(!page) page = estructalloc(page_t, 1);

   return *page;
}

//
// ACSArray::print
//
void ACSArray::print(qstring *printBuffer, uint32_t offset, uint32_t length)
{
   length += offset; // use length as end
   for(char val; offset != length && (val = (char)getVal(offset)); ++offset)
      *printBuffer += val;
}

//
// ACSVM::ACSVM
//
ACSVM::ACSVM(int tag) : ZoneObject()
{
   ChangeTag(tag);
   reset();
}

//
// ACSVM::~ACSVM
//
ACSVM::~ACSVM()
{
}

//
// ACSVM::findFunction
//
ACSFunc *ACSVM::findFunction(const char *name)
{
   for(unsigned int i = numFuncNames; i--;)
   {
      if(i < numFuncs && funcs[i].codeIndex && !strcasecmp(funcNames[i], name))
         return &funcs[i];
   }

   return NULL;
}

//
// ACSVM::findMapVar
//
int32_t *ACSVM::findMapVar(const char *name)
{
   for(unsigned int i = ACS_NUM_MAPVARS; i--;)
   {
      if(mapvnam[i] && !strcasecmp(mapvnam[i], name))
         return &mapvars[i];
   }

   return NULL;
}

//
// ACSVM::findMapArr
//
ACSArray *ACSVM::findMapArr(const char *name)
{
   for(unsigned int i = ACS_NUM_MAPARRS; i--;)
   {
      if(mapanam[i] && !strcasecmp(mapanam[i], name))
         return &maparrs[i];
   }

   return NULL;
}

//
// ACSVM::reset
//
void ACSVM::reset()
{
   for(int i = ACS_NUM_MAPVARS; i--;)
   {
      mapvars[i] = 0;
      mapvtab[i] = &mapvars[i];

      mapvnam[i] = NULL;
   }

   for(int i = ACS_NUM_MAPARRS; i--;)
   {
      maparrs[i].clear();
      mapatab[i] = &maparrs[i];

      mapanam[i] = NULL;
      mapalen[i] = 0;
      mapahas[i] = false;
   }

   code       = NULL;
   numCode    = 0;
   strings    = 0;
   numStrings = 0;
   scripts    = NULL;
   numScripts = 0;
   loaded     = false;
   lump       = -1;

   exports    = NULL;
   numExports = 0;
   imports    = NULL;
   importVMs  = NULL;
   numImports = 0;
}

//
// ACS_Init
//
// Called at startup.
//
void ACS_Init(void)
{
}

//
// ACS_NewGame
//
// Called when a new game is started.
//
void ACS_NewGame(void)
{
   DLListItem<deferredacs_t> *cur, *next;

   // clear out the world variables
   memset(ACSworldvars, 0, sizeof(ACSworldvars));

   // clear out the global variables
   memset(ACSglobalvars, 0, sizeof(ACSglobalvars));

   // clear out deferred scripts
   cur = acsDeferred;

   while(cur)
   {
      next = cur->dllNext;
      cur->remove();      
      efree(cur->dllObject);
      cur = next;
   }

   acsDeferred = NULL;
}

//
// ACS_InitLevel
//
// Called at level start from P_SetupLevel
//
void ACS_InitLevel(void)
{
   acsLevelScriptVM.reset();

   ACSVM::GlobalStrings = NULL;
   ACSVM::GlobalNumStrings = 0;
}

//
// ACS_LoadScript
//
ACSVM *ACS_LoadScript(WadDirectory *dir, int lump)
{
   ACSVM *vm;

   if(lump == -1) return NULL;

   // If the lump has already been loaded, don't reload it.
   for(ACSVM **itr = acsVMs.begin(), **end = acsVMs.end(); itr != end; ++itr)
   {
      if ((*itr)->lump == lump)
         return *itr;
   }

   vm = new ACSVM(PU_LEVEL);

   ACS_LoadScript(vm, dir, lump);

   return vm;
}

//
// ACS_LoadScript
//
void ACS_LoadScript(ACSVM *vm, WadDirectory *dir, int lump)
{
   byte *data;

   ACS_addVirtualMachine(vm);
   vm->strings = ACSVM::GlobalNumStrings;
   vm->lump    = lump;

   // zero length or too-short lump?
   if(dir->lumpLength(lump) < 4)
   {
      vm->code = NULL;
      return;
   }

   // load the lump
   data = (byte *)(dir->cacheLumpNum(lump, PU_LEVEL));

   switch(SwapULong(*(uint32_t *)data))
   {
   case ACS_CHUNKID('A', 'C', 'S', '\0'):
      ACS_LoadScriptACS0(vm, dir, lump, data);
      break;

   case ACS_CHUNKID('A', 'C', 'S', 'E'):
      ACS_LoadScriptACSE(vm, dir, lump, data);
      break;

   case ACS_CHUNKID('A', 'C', 'S', 'e'):
      ACS_LoadScriptACSe(vm, dir, lump, data);
      break;
   }

   // haleyjd 06/30/09: open scripts must be started *here*, not above.
   for(acscript_t *end = vm->scripts+vm->numScripts,
       *itr = vm->scripts; itr != end; ++itr)
   {
      if(itr->type == ACS_STYPE_OPEN)
         ACS_runOpenScript(vm, itr);
   }
}

//
// ACS_loadScripts
//
// Reads lump names out of the lump and loads them as scripts.
//
static void ACS_loadScripts(WadDirectory *dir, int lump)
{
   const char *itr, *end;
   char lumpname[9], *nameItr, *const nameEnd = lumpname+8;

   itr = (const char *)(dir->cacheLumpNum(lump, PU_CACHE));
   end = itr + dir->lumpLength(lump);

   for(;;)
   {
      // Discard any whitespace.
      while(itr != end && isspace(*itr)) ++itr;

      if(itr == end) break;

      // Read a name.
      nameItr = lumpname;
      while(itr != end && nameItr != nameEnd && !isspace(*itr))
         *nameItr++ = *itr++;
      *nameItr = '\0';

      // Discard excess letters.
      while(itr != end && !isspace(*itr)) ++itr;

      if((lump = dir->checkNumForName(lumpname, lumpinfo_t::ns_acs)) != -1)
         ACS_LoadScript(dir, lump);
   }
}

//
// ACS_LoadLevelScript
//
// Loads the level scripts and initializes the levelscript virtual machine.
// Called from P_SetupLevel.
//
void ACS_LoadLevelScript(WadDirectory *dir, int lump)
{
   acsVMs.makeEmpty();

   // load the level script, if any
   if(lump != -1)
      ACS_LoadScript(&acsLevelScriptVM, dir, lump);

   // The rest of the function is LOADACS handling.

   lumpinfo_t **lumpinfo = dir->getLumpInfo();

   lump = dir->getLumpNameChain("LOADACS")->index;
   while(lump != -1)
   {
      if(!strncasecmp(lumpinfo[lump]->name, "LOADACS", 7) &&
         lumpinfo[lump]->li_namespace == lumpinfo_t::ns_global)
      {
         ACS_loadScripts(dir, lump);
      }

      lump = lumpinfo[lump]->next;
   }

}

//
// ACS_addDeferredScriptVM
//
// Adds a deferred script that will be executed when the indicated
// gamemap is reached. Currently supports maps of MAPxy name structure.
//
static bool ACS_addDeferredScriptVM(ACSVM *vm, int scrnum, int mapnum, 
                                    int type, int args[NUMLINEARGS])
{
   DLListItem<deferredacs_t> *cur = acsDeferred;
   deferredacs_t *newdacs;

   // check to make sure the script isn't already scheduled
   while(cur)
   {
      deferredacs_t *dacs = cur->dllObject;

      if(dacs->targetMap == mapnum && dacs->scriptNum == scrnum)
         return false;

      cur = cur->dllNext;
   }

   // allocate a new deferredacs_t
   newdacs = estructalloc(deferredacs_t, 1);

   // set script number
   newdacs->scriptNum = scrnum; 

   // set vm id #
   newdacs->vmID = vm->id;

   // set action type
   newdacs->type = type;

   // set args
   memcpy(newdacs->args, args, 5 * sizeof(int));

   // record target map number
   newdacs->targetMap = mapnum;

   // add it to the linked list
   newdacs->link.insert(newdacs, &acsDeferred);

   return true;
}

//
// ACS_RunDeferredScripts
//
// Runs scripts that have been deferred for the current map
//
void ACS_RunDeferredScripts(void)
{
   DLListItem<deferredacs_t> *cur = acsDeferred, *next;
   ACSThinker *newScript = NULL;
   ACSThinker *rover = NULL;
   unsigned int internalNum;

   while(cur)
   {
      ACSVM *vm;
      deferredacs_t *dacs = cur->dllObject;

      next = cur->dllNext; 

      vm = acsVMs[dacs->vmID];

      if(dacs->targetMap == gamemap)
      {
         switch(dacs->type)
         {
         case ACS_DEFERRED_EXECUTE:
            ACS_StartScriptVM(vm, dacs->scriptNum, 0, dacs->args, NULL, NULL, 0, 
                              &newScript, false);
            if(newScript)
               newScript->delay = TICRATE;
            break;
         case ACS_DEFERRED_SUSPEND:
            if((internalNum = ACS_indexForNum(vm, dacs->scriptNum)) != vm->numScripts)
            {
               acscript_t *script = &(vm->scripts[internalNum]);
               rover = script->threads;

               while(rover)
               {
                  if(rover->sreg != ACS_STATE_STOPPED &&
                     rover->sreg != ACS_STATE_TERMINATE)
                     rover->sreg = ACS_STATE_SUSPEND;

                  rover = rover->nextthread;
               }
            }
            break;
         case ACS_DEFERRED_TERMINATE:
            if((internalNum = ACS_indexForNum(vm, dacs->scriptNum)) != vm->numScripts)
            {
               acscript_t *script = &(vm->scripts[internalNum]);
               rover = script->threads;

               while(rover)
               {
                  if(rover->sreg != ACS_STATE_STOPPED)
                     rover->sreg = ACS_STATE_TERMINATE;

                  rover = rover->nextthread;
               }
            }
            break;
         }

         // unhook and delete this deferred script
         cur->remove();
         efree(dacs);
      }

      cur = next;
   }
}

//
// ACS_StartScriptVM
//
// Standard method for starting an ACS script.
//
bool ACS_StartScriptVM(ACSVM *vm, int scrnum, int map, int *args,
                       Mobj *mo, line_t *line, int side,
                       ACSThinker **scr, bool always)
{
   acscript_t   *scrData;
   ACSThinker *newScript, *rover;
   bool foundScripts = false;
   unsigned int internalNum, i;

   // ACS must be active on the current map or we do nothing
   if(!vm->loaded)
      return false;

   // should the script be deferred to a different map?
   if(map > 0 && map != gamemap)
      return ACS_addDeferredScriptVM(vm, scrnum, map, ACS_DEFERRED_EXECUTE, args);

   // look for the script
   if((internalNum = ACS_indexForNum(vm, scrnum)) == vm->numScripts)
   {
      // tink!
      doom_printf(FC_ERROR "ACS_StartScript: no such script %d\a\n", scrnum);
      return false;
   }

   scrData = &(vm->scripts[internalNum]);

   rover = scrData->threads;

   while(rover)
   {
      // if the script is suspended, restart it
      if(rover->sreg == ACS_STATE_SUSPEND)
      {
         foundScripts = true;
         rover->sreg = ACS_STATE_RUNNING;
      }

      rover = rover->nextthread;
   }

   // if not an always-execute action and we restarted a stopped script,
   // then we return true now.
   // otherwise, return false for failure.
   if(!always && scrData->threads)
      return foundScripts;

   // setup the new script thinker
   newScript = new ACSThinker;

   newScript->ip          = scrData->codePtr;
   newScript->locals      = (int32_t *)Z_Calloc(scrData->numVars, sizeof(int32_t),
                                                PU_LEVEL, NULL);
   newScript->line        = line;
   newScript->lineSide    = side;
   P_SetTarget<Mobj>(&newScript->trigger, mo);

   // copy in some important data
   newScript->acscript    = scrData;
   newScript->vm          = vm;

   // copy arguments into first N local variables
   for(i = 0; i < scrData->numArgs; ++i)
      newScript->locals[i] = args[i];

   // attach the thinker
   newScript->addThinker();

   // add as a thread of the acscript
   ACS_addThread(newScript, scrData);
	
   // mark as running
   newScript->sreg  = ACS_STATE_RUNNING;
   newScript->sdata = 0;

   // return pointer to new script in *scr if not null
   if(scr)
      *scr = newScript;
	
   return true;
}

//
// ACS_StartScript
//
// Convenience routine; starts a script in the levelscript vm.
//
bool ACS_StartScript(int scrnum, int map, int *args, 
                     Mobj *mo, line_t *line, int side,
                     ACSThinker **scr, bool always)
{
   return ACS_StartScriptVM(&acsLevelScriptVM, scrnum, map, args, mo,
                            line, side, scr, always);
}

//
// ACS_TerminateScriptVM
//
// Attempts to terminate the given script. If the mapnum doesn't match the
// current gamemap, the action will be deferred.
//
bool ACS_TerminateScriptVM(ACSVM *vm, int scrnum, int mapnum)
{
   bool ret = false;
   int foo[NUMLINEARGS] = { 0, 0, 0, 0, 0 };

   // ACS must be active on the current map or we do nothing
   if(!vm->loaded)
      return false;

   if(mapnum > 0 && mapnum == gamemap)
   {
      unsigned int internalNum;

      if((internalNum = ACS_indexForNum(vm, scrnum)) != vm->numScripts)
      {
         acscript_t *script = &(vm->scripts[internalNum]);
         ACSThinker *rover = script->threads;

         while(rover)
         {
            if(rover->sreg != ACS_STATE_STOPPED)
            {
               rover->sreg = ACS_STATE_TERMINATE;
               ret = true;
            }
            rover = rover->nextthread;
         }
      }
   }
   else
      ret = ACS_addDeferredScriptVM(vm, scrnum, mapnum, ACS_DEFERRED_TERMINATE, foo);

   return ret;
}

//
// ACS_TerminateScript
//
// Convenience routine; terminates a level script.
//
bool ACS_TerminateScript(int scrnum, int mapnum)
{
   return ACS_TerminateScriptVM(&acsLevelScriptVM, scrnum, mapnum);
}

//
// ACS_SuspendScriptVM
//
// Attempts to suspend the given script. If the mapnum doesn't match the
// current gamemap, the action will be deferred.
//
bool ACS_SuspendScriptVM(ACSVM *vm, int scrnum, int mapnum)
{
   int foo[NUMLINEARGS] = { 0, 0, 0, 0, 0 };
   bool ret = false;

   // ACS must be active on the current map or we do nothing
   if(!vm->loaded)
      return false;

   if(mapnum > 0 && mapnum == gamemap)
   {
      unsigned int internalNum;

      if((internalNum = ACS_indexForNum(vm, scrnum)) != vm->numScripts)
      {
         acscript_t *script = &(vm->scripts[internalNum]);
         ACSThinker *rover = script->threads;

         while(rover)
         {
            if(rover->sreg != ACS_STATE_STOPPED &&
               rover->sreg != ACS_STATE_TERMINATE)
            {
               rover->sreg = ACS_STATE_SUSPEND;
               ret = true;
            }
         }
      }
   }
   else
      ret = ACS_addDeferredScriptVM(vm, scrnum, mapnum, ACS_DEFERRED_SUSPEND, foo);

   return ret;
}

//
// ACS_SuspendScript
//
// Convenience routine; suspends a level script.
//
bool ACS_SuspendScript(int scrnum, int mapnum)
{
   return ACS_SuspendScriptVM(&acsLevelScriptVM, scrnum, mapnum);
}

//=============================================================================
//
// Save/Load Code
//

//
// SaveArchive << ACSVM *
//
static SaveArchive &operator << (SaveArchive &arc, ACSVM *&vm)
{
   uint32_t vmID;

   if(arc.isSaving())
      vmID = vm->id;

   arc << vmID;

   if(arc.isLoading())
      vm = acsVMs[vmID];

   return arc;
}

//
// SaveArchive << ACSArray
//
static SaveArchive &operator << (SaveArchive &arc, ACSArray &arr)
{
   arr.archive(arc);
   return arc;
}

//
// SaveArchive << acs_call_t
//
static SaveArchive &operator << (SaveArchive &arc, acs_call_t &call)
{
   bool hasPrintBuffer;
   uint32_t ipTemp;

   if(arc.isSaving())
   {
      hasPrintBuffer = call.printBuffer != NULL;
      ipTemp = call.ip - call.vm->code;
   }

   arc << ipTemp << call.numLocals << hasPrintBuffer << call.vm;

   call.ip = call.vm->code + ipTemp;

   if(arc.isLoading())
      call.locals = (int32_t *)Z_Malloc(call.numLocals * sizeof(int32_t), PU_LEVEL, NULL);

   P_ArchiveArray(arc, call.locals, call.numLocals);

   // Save the print buffer, if any.
   if(hasPrintBuffer)
   {
      if(arc.isLoading())
         call.printBuffer = new qstring(qstring::basesize, PU_LEVEL);

      call.printBuffer->archive(arc);
   }

   return arc;
}

//
// SaveArchive << deferredacs_t
//
static SaveArchive &operator << (SaveArchive &arc, deferredacs_t &dacs)
{
   arc << dacs.scriptNum << dacs.vmID << dacs.targetMap << dacs.type;
   P_ArchiveArray(arc, dacs.args, NUMLINEARGS);
   return arc;
}

//
// ACSArray::archiveArrdata
//
void ACSArray::archiveArrdata(SaveArchive &arc, arrdata_t *arrdata)
{
   bool hasRegion;

   // Archive every region.
   for(region_t **regionItr = *arrdata, **regionEnd = regionItr + ACS_ARRDATASIZE;
       regionItr != regionEnd; ++regionItr)
   {
      // Determine if there is a region to archive.
      if(arc.isSaving())
         hasRegion = *regionItr != NULL;

      arc << hasRegion;

      // If so, archive it.
      if(hasRegion)
      {
         // If loading, need to allocate the region.
         if(arc.isLoading())
            *regionItr = estructalloc(region_t, 1);

         archiveRegion(arc, *regionItr);
      }
   }
}

//
// ACSArray::archiveRegion
//
void ACSArray::archiveRegion(SaveArchive &arc, region_t *region)
{
   bool hasBlock;

   // Archive every block in the region.
   for(block_t **blockItr = *region, **blockEnd = blockItr + ACS_REGIONSIZE;
       blockItr != blockEnd; ++blockItr)
   {
      // Determine if there is a block to archive.
      if(arc.isSaving())
         hasBlock = *blockItr != NULL;

      arc << hasBlock;

      // If so, archive it.
      if(hasBlock)
      {
         // If loading, need to allocate the block.
         if(arc.isLoading())
            *blockItr = estructalloc(block_t, 1);

         archiveBlock(arc, *blockItr);
      }
   }
}

//
// ACSArray::archiveBlock
//
void ACSArray::archiveBlock(SaveArchive &arc, block_t *block)
{
   bool hasPage;

   // Archive every page in the block.
   for(page_t **pageItr = *block, **pageEnd = pageItr + ACS_BLOCKSIZE;
       pageItr != pageEnd; ++pageItr)
   {
      // Determine if there is a page to archive.
      if(arc.isSaving())
         hasPage = *pageItr != NULL;

      arc << hasPage;

      // If so, archive it.
      if(hasPage)
      {
         // If loading, need to allocate the page first.
         if(arc.isLoading())
            *pageItr = estructalloc(page_t, 1);

         archivePage(arc, *pageItr);
      }
   }
}

//
// ACSArray::archivePage
//
void ACSArray::archivePage(SaveArchive &arc, page_t *page)
{
   for(val_t *valItr = *page, *valEnd = valItr + ACS_PAGESIZE;
       valItr != valEnd; ++valItr)
   {
      archiveVal(arc, valItr);
   }
}

//
// ACSArray::archiveVal
//
void ACSArray::archiveVal(SaveArchive &arc, val_t *val)
{
   arc << *val;
}

//
// ACSArray::archive
//
void ACSArray::archive(SaveArchive &arc)
{
   if(arc.isLoading())
      clear();

   archiveArrdata(arc, &arrdata);
}

//
// ACSThinker::serialize
//
// Saves/loads a ACSThinker.
//
void ACSThinker::serialize(SaveArchive &arc)
{
   uint32_t acscriptIndex, callPtrIndex, ipIndex, lineIndex;

   Super::serialize(arc);

   // Pointer-to-Index
   if(arc.isSaving())
   {
      acscriptIndex = acscript - vm->scripts;

      callPtrIndex = callPtr - calls;

      ipIndex = ip - vm->code;

      lineIndex = line ? line - lines + 1 : 0;

      triggerSwizzle = P_NumForThinker(trigger);
   }

   // Basic properties
   arc << ipIndex << stp << numLocals << sreg << sdata << callPtrIndex
       << acscriptIndex << vm << delay << triggerSwizzle << lineIndex << lineSide;

   // Allocations/Index-to-Pointer
   if(arc.isLoading())
   {
      acscript = vm->scripts + acscriptIndex;

      numCalls = callPtrIndex;
      calls    = estructalloctag(acs_call_t, numCalls, PU_LEVEL);
      callPtr  = calls + callPtrIndex;

      ip = vm->code + ipIndex;

      line = lineIndex ? &lines[lineIndex - 1] : NULL;

      locals = estructalloctag(int32_t, numLocals, PU_LEVEL);
   }

   // Arrays
   P_ArchiveArray(arc, stack, ACS_STACK_LEN);
   P_ArchiveArray(arc, locals, numLocals);
   P_ArchiveArray(arc, calls, callPtrIndex);

   // Post-load insertion into the environment.
   if(arc.isLoading())
   {
      // add the thread
      ACS_addThread(this, acscript);
   }
}

//
// ACSThinker::deSwizzle
//
// Fixes up the trigger reference in a ACSThinker.
//
void ACSThinker::deSwizzle()
{
   Mobj *mo = thinker_cast<Mobj *>(P_ThinkerForNum(triggerSwizzle));
   P_SetNewTarget(&trigger, mo);
   triggerSwizzle = 0;
}

//
// ACS_Archive
//
void ACS_Archive(SaveArchive &arc)
{
   DLListItem<deferredacs_t> *dacs;
   uint32_t size;

   // any OPEN script threads created during ACS_LoadLevelScript have
   // been destroyed, so clear out the threads list of all scripts
   if(arc.isLoading())
   {
      for(ACSVM **vm = acsVMs.begin(), **vmEnd = acsVMs.end(); vm != vmEnd; ++vm)
      {
         for(acscript_t *s = (*vm)->scripts,
             *sEnd = s + (*vm)->numScripts; s != sEnd; ++s)
         {
            s->threads = NULL;
         }
      }
   }

   // Archive map variables.
   for(ACSVM **vm = acsVMs.begin(), **vmEnd = acsVMs.end(); vm != vmEnd; ++vm)
   {
      P_ArchiveArray(arc, (*vm)->mapvars, ACS_NUM_MAPVARS);
      P_ArchiveArray(arc, (*vm)->maparrs, ACS_NUM_MAPARRS);
   }

   // Archive world variables. (TODO: not load on hub transfer?)
   P_ArchiveArray(arc, ACSworldvars, ACS_NUM_WORLDVARS);
   P_ArchiveArray(arc, ACSworldarrs, ACS_NUM_WORLDARRS);

   // Archive global variables.
   P_ArchiveArray(arc, ACSglobalvars, ACS_NUM_GLOBALVARS);
   P_ArchiveArray(arc, ACSglobalarrs, ACS_NUM_GLOBALARRS);

   // Archive deferred scripts. (TODO: not load on hub transfer?)
   if(arc.isSaving())
   {
      for(size = 0, dacs = acsDeferred; dacs; dacs = dacs->dllNext)
         ++size;
   }

   arc << size;

   if(arc.isLoading())
   {
      while(size--)
      {
         deferredacs_t *dacsItem;
         dacsItem = estructalloc(deferredacs_t, 1);
         dacsItem->link.insert(dacsItem, &acsDeferred);
      }
   }

   for(dacs = acsDeferred; dacs; dacs = dacs->dllNext)
      arc << *dacs->dllObject;
}

// EOF


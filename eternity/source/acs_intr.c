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

#include "z_malloc.h"

#include "acs_intr.h"

#ifdef ACS_INTR

//
// Enumerations
//

// script states for T_ACSThinker
enum
{
   ACTION_CONTINUE,
   ACTION_TERMINATE,
   ACTION_SUSPEND,
};

// opcode IDs
enum
{
   /*   0 */ OP_NOP,
   /*   1 */ OP_TERMINATE,
   /*   2 */ OP_SUSPEND,
   /*   3 */ OP_PUSHNUMBER,
   /*   4 */ OP_LINESPEC1,
   /*   5 */ OP_LINESPEC2,
   /*   6 */ OP_LINESPEC3,
   /*   7 */ OP_LINESPEC4,
   /*   8 */ OP_LINESPEC5,
   /*   9 */ OP_LINESPEC1_IMM,
   /*  10 */ OP_LINESPEC2_IMM,
   /*  11 */ OP_LINESPEC3_IMM,
   /*  12 */ OP_LINESPEC4_IMM,
   /*  13 */ OP_LINESPEC5_IMM,
   /*  14 */ OP_ADD,
   /*  15 */ OP_SUB,
   /*  16 */ OP_MUL,
   /*  17 */ OP_DIV,
   /*  18 */ OP_MOD,
   /*  19 */ OP_EQUAL,
   /*  20 */ OP_NOTEQUAL,
   /*  21 */ OP_LESS,
   /*  22 */ OP_GREATER,
   /*  23 */ OP_LESSOREQUAL,
   /*  24 */ OP_GREATEROREQUAL,
   /*  25 */ OP_ASSIGNSCRIPTVAR,
   /*  26 */ OP_ASSIGNMAPVAR,
   /*  27 */ OP_ASSIGNWORLDVAR,
   /*  28 */ OP_PUSHSCRIPTVAR,
   /*  29 */ OP_PUSHMAPVAR,
   /*  30 */ OP_PUSHWORLDVAR,
   /*  31 */ OP_ADDSCRIPTVAR,
   /*  32 */ OP_ADDMAPVAR,
   /*  33 */ OP_ADDWORLDVAR,
   /*  34 */ OP_SUBSCRIPTVAR,
   /*  35 */ OP_SUBMAPVAR,
   /*  36 */ OP_SUBWORLDVAR,
   /*  37 */ OP_MULSCRIPTVAR,
   /*  38 */ OP_MULMAPVAR,
   /*  39 */ OP_MULWORLDVAR,
   /*  40 */ OP_DIVSCRIPTVAR,
   /*  41 */ OP_DIVMAPVAR,
   /*  42 */ OP_DIVWORLDVAR,
   /*  43 */ OP_MODSCRIPTVAR,
   /*  44 */ OP_MODMAPVAR,
   /*  45 */ OP_MODWORLDVAR,
   /*  46 */ OP_INCSCRIPTVAR,
   /*  47 */ OP_INCMAPVAR,
   /*  48 */ OP_INCWORLDVAR,
   /*  49 */ OP_DECSCRIPTVAR,
   /*  50 */ OP_DECMAPVAR,
   /*  51 */ OP_DECWORLDVAR,
   /*  52 */ OP_BRANCH,
   /*  53 */ OP_BRANCHNOTZERO,
   /*  54 */ OP_DECSTP,
   /*  55 */ OP_DELAY,
   /*  56 */ OP_DELAY_IMM,
   /*  57 */ OP_RANDOM,
   /*  58 */ OP_RANDOM_IMM,
   /*  59 */ OP_THINGCOUNT,
   /*  60 */ OP_THINGCOUNT_IMM,
   /*  61 */ OP_TAGWAIT,
   /*  62 */ OP_TAGWAIT_IMM,
   /*  63 */ OP_POLYWAIT,
   /*  64 */ OP_POLYWAIT_IMM,
   /*  65 */ OP_CHANGEFLOOR,
   /*  66 */ OP_CHANGEFLOOR_IMM,
   /*  67 */ OP_CHANGECEILING,
   /*  68 */ OP_CHANGECEILING_IMM,
   /*  69 */ OP_RESTART,
   /*  70 */ OP_LOGICALAND,
   /*  71 */ OP_LOGICALOR,
   /*  72 */ OP_BITWISEAND,
   /*  73 */ OP_BITWISEOR,
   /*  74 */ OP_BITWISEXOR,
   /*  75 */ OP_LOGICALNOT,
   /*  76 */ OP_SHIFTLEFT,
   /*  77 */ OP_SHIFTRIGHT,
   /*  78 */ OP_NEGATE,
   /*  79 */ OP_BRANCHZERO,
   /*  80 */ OP_LINESIDE,
   /*  81 */ OP_SCRIPTWAIT,
   /*  82 */ OP_SCRIPTWAIT_IMM,
   /*  83 */ OP_CLEARLINESPECIAL,
   /*  84 */ OP_CASEJUMP,
   /*  85 */ OP_STARTPRINT,
   /*  86 */ OP_ENDPRINT,
   /*  87 */ OP_PRINTSTRING,
   /*  88 */ OP_PRINTINT,
   /*  89 */ OP_PRINTCHAR,
   /*  90 */ OP_PLAYERCOUNT,
   /*  91 */ OP_GAMETYPE,
   /*  92 */ OP_GAMESKILL,
   /*  93 */ OP_TIMER,
   /*  94 */ OP_SECTORSOUND,
   /*  95 */ OP_AMBIENTSOUND,
   /*  96 */ OP_SOUNDSEQUENCE,
   /*  97 */ OP_SETLINETEXTURE,
   /*  98 */ OP_SETLINEBLOCKING,
   /*  99 */ OP_SETLINESPECIAL,
   /* 100 */ OP_THINGSOUND,
   /* 101 */ OP_ENDPRINTBOLD,
};

//
// Interpreter Macros
//

#define PUSH(x)  stack[stp++] = (x)
#define POP()    stack[--stp]
#define PEEK()   stack[stp-1]
#define DECSTP() --stp

// for binary operations: ++ and -- mess these up
#define ST_OP1      stack[stp-2]
#define ST_OP2      stack[stp-1]
#define ST_BINOP(x) stack[stp-2] = (x); --stp

//
// Global Functions
//

void T_ACSThinker(acsthinker_t *script)
{
   // cache vm data in local vars
   int *ip    = script->ip;
   int stp    = script->stp;
   int *stack = script->stack;
   int action = ACTION_CONTINUE;
   int opcode, temp;

   // TODO: check for:
   // 1. termination
   // 2. not running
   // 3. delay time
  
   // run opcodes
   do
   {
      opcode = *ip++;
      switch(opcode)
      {
      case OP_NOP:
         break;
      case OP_TERMINATE:
         action = ACTION_TERMINATE;
         break;
      case OP_SUSPEND:
         // TODO: write to acscript sreg
         action = ACTION_SUSPEND;
         break;
      case OP_PUSHNUMBER:
         PUSH(*ip++);
         break;
      case OP_LINESPEC1:
         break;
      case OP_LINESPEC2:
         break;
      case OP_LINESPEC3:
         break;
      case OP_LINESPEC4:
         break;
      case OP_LINESPEC5:
         break;
      case OP_LINESPEC1_IMM:
         break;
      case OP_LINESPEC2_IMM:
         break;
      case OP_LINESPEC3_IMM:
         break;
      case OP_LINESPEC4_IMM:
         break;
      case OP_LINESPEC5_IMM:
         break;
      case OP_ADD:
         ST_BINOP(ST_OP1 + ST_OP2);
         break;
      case OP_SUB:
         ST_BINOP(ST_OP1 - ST_OP2);
         break;
      case OP_MUL:
         ST_BINOP(ST_OP1 * ST_OP2);
         break;
      case OP_DIV:
         if(!(temp = ST_OP2))
         {
            action = ACTION_TERMINATE;
            break;
         }
         ST_BINOP(ST_OP1 / temp);
         break;
      case OP_MOD:
         if(!(temp = ST_OP2))
         {
            action = ACTION_TERMINATE;
            break;
         }
         ST_BINOP(ST_OP1 % temp);
         break;
      case OP_EQUAL:
         ST_BINOP(ST_OP1 == ST_OP2);
         break;
      case OP_NOTEQUAL:
         ST_BINOP(ST_OP1 != ST_OP2);
         break;
      case OP_LESS:
         ST_BINOP(ST_OP1 < ST_OP2);
         break;
      case OP_GREATER:
         ST_BINOP(ST_OP1 > ST_OP2);
         break;
      case OP_LESSOREQUAL:
         ST_BINOP(ST_OP1 <= ST_OP2);
         break;
      case OP_GREATEROREQUAL:
         ST_BINOP(ST_OP1 >= ST_OP2);
         break;
      case OP_ASSIGNSCRIPTVAR:
         script->locals[*ip++] = POP();
         break;
      case OP_ASSIGNMAPVAR:
         break;
      case OP_ASSIGNWORLDVAR:
         break;
      case OP_PUSHSCRIPTVAR:
         PUSH(script->locals[*ip++]);
         break;
      case OP_PUSHMAPVAR:
         break;
      case OP_PUSHWORLDVAR:
         break;
      case OP_ADDSCRIPTVAR:
         script->locals[*ip++] += POP();
         break;
      case OP_ADDMAPVAR:
         break;
      case OP_ADDWORLDVAR:
         break;
      case OP_SUBSCRIPTVAR:
         script->locals[*ip++] -= POP();
         break;
      case OP_SUBMAPVAR:
         break;
      case OP_SUBWORLDVAR:
         break;
      case OP_MULSCRIPTVAR:
         script->locals[*ip++] *= POP();
         break;
      case OP_MULMAPVAR:
         break;
      case OP_MULWORLDVAR:
         break;
      case OP_DIVSCRIPTVAR:
         if(!(temp = POP()))
         {
            action = ACTION_TERMINATE;
            break;
         }
         script->locals[*ip++] /= temp;
         break;
      case OP_DIVMAPVAR:
         break;
      case OP_DIVWORLDVAR:
         break;
      case OP_MODSCRIPTVAR:
         if(!(temp = POP()))
         {
            action = ACTION_TERMINATE;
            break;
         }
         script->locals[*ip++] %= temp;
         break;
      case OP_MODMAPVAR:
         break;
      case OP_MODWORLDVAR:
         break;
      case OP_INCSCRIPTVAR:
         script->locals[*ip] += 1;
         ++ip;
         break;
      case OP_INCMAPVAR:
         break;
      case OP_INCWORLDVAR:
         break;
      case OP_DECSCRIPTVAR:
         script->locals[*ip] -= 1;
         ++ip;
         break;
      case OP_DECMAPVAR:
         break;
      case OP_DECWORLDVAR:
         break;
      case OP_BRANCH:
         break;
      case OP_BRANCHNOTZERO:
         break;
      case OP_DECSTP:
         DECSTP();
         break;
      case OP_DELAY:
         break;
      case OP_DELAY_IMM:
         break;
      case OP_RANDOM:
         break;
      case OP_RANDOM_IMM:
         break;
      case OP_THINGCOUNT:
         break;
      case OP_THINGCOUNT_IMM:
         break;
      case OP_TAGWAIT:
         break;
      case OP_TAGWAIT_IMM:
         break;
      case OP_POLYWAIT:
         break;
      case OP_POLYWAIT_IMM:
         break;
      case OP_CHANGEFLOOR:
         break;
      case OP_CHANGEFLOOR_IMM:
         break;
      case OP_CHANGECEILING:
         break;
      case OP_CHANGECEILING_IMM:
         break;
      case OP_RESTART:
         break;
      case OP_LOGICALAND:
         ST_BINOP(ST_OP1 && ST_OP2);
         break;
      case OP_LOGICALOR:
         ST_BINOP(ST_OP1 || ST_OP2);
         break;
      case OP_BITWISEAND:
         ST_BINOP(ST_OP1 & ST_OP2);
         break;
      case OP_BITWISEOR:
         ST_BINOP(ST_OP1 | ST_OP2);
         break;
      case OP_BITWISEXOR:
         ST_BINOP(ST_OP1 ^ ST_OP2);
         break;
      case OP_LOGICALNOT:
         temp = POP();
         PUSH(!temp);
         break;
      case OP_SHIFTLEFT:
         ST_BINOP(ST_OP1 << ST_OP2);
         break;
      case OP_SHIFTRIGHT:
         ST_BINOP(ST_OP1 >> ST_OP2);
         break;
      case OP_NEGATE:
         temp = POP();
         PUSH(-temp);
         break;
      case OP_BRANCHZERO:
         break;
      case OP_LINESIDE:
         break;
      case OP_SCRIPTWAIT:
         break;
      case OP_SCRIPTWAIT_IMM:
         break;
      case OP_CLEARLINESPECIAL:
         break;
      case OP_CASEJUMP:
         break;
      case OP_STARTPRINT:
         break;
      case OP_ENDPRINT:
         break;
      case OP_PRINTSTRING:
         break;
      case OP_PRINTINT:
         break;
      case OP_PRINTCHAR:
         break;
      case OP_PLAYERCOUNT:
         break;
      case OP_GAMETYPE:
         break;
      case OP_GAMESKILL:
         break;
      case OP_TIMER:
         break;
      case OP_SECTORSOUND:
         break;
      case OP_AMBIENTSOUND:
         break;
      case OP_SOUNDSEQUENCE:
         break;
      case OP_SETLINETEXTURE:
         break;
      case OP_SETLINEBLOCKING:
         break;
      case OP_SETLINESPECIAL:
         break;
      case OP_THINGSOUND:
         break;
      case OP_ENDPRINTBOLD:
         break;
      default:
         break;
      }
   } while(action == ACTION_CONTINUE);
}

#endif

// EOF


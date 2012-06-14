// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
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
// ACS0 loader.
//
//----------------------------------------------------------------------------

#include "acs_intr.h"

#include "i_system.h"
#include "m_swap.h"
#include "w_wad.h"
#include "doomstat.h"


//----------------------------------------------------------------------------|
// Types                                                                      |
//

typedef enum acs0_op_e
{
   // ZDoom was first, so their extensions are what will be used for ACS0 lumps.
   #define ACSE_OP(OPDATA,OP,ARGC,COMP) ACS0_OP(OPDATA, OP, ARGC, COMP)
   #define ACS0_OP(OPDATA,OP,ARGC,COMP) ACS0_OP_##OP,
   #include "acs_op.h"
   #undef ACS0_OP

   ACS0_OPMAX
} acs0_op_t;

typedef struct acs0_opdata_s
{
   acs_opdata_t const *opdata;
   acs0_op_t op;
   int args;
   bool compressed;
} acs0_opdata_t;


//----------------------------------------------------------------------------|
// Static Variables                                                           |
//

static acs0_opdata_t const ACS0opdata[ACS0_OPMAX] =
{
   #define ACS0_OP(OPDATA,OP,ARGC,COMP) {&ACSopdata[ACS_OP_##OPDATA], ACS0_OP_##OP, ARGC, COMP},
   #include "acs_op.h"
   #undef ACS0_OP
};


//----------------------------------------------------------------------------|
// Static Functions                                                           |
//

//
// ACS_readStringACS0
//
static const char *ACS_readStringACS0(byte *dataBegin, byte *dataEnd)
{
   char *begin = (char *)dataBegin, *end = (char *)dataEnd, *itr = begin;
   char *str;

   // Find either a null-terminator or the end of the lump.
   while(itr != end && *itr) ++itr;

   // If it's an empty string, don't bother with Z_Malloc.
   if (itr == begin) return "";

   str = (char *)Z_Malloc(itr - begin + 1, PU_LEVEL, NULL);
   memcpy(str, begin, itr - begin);
   str[itr - begin] = 0;

   return str;
}

//
// ACS_touchScriptACS0
//
static bool ACS_touchScriptACS0(bool *begin, bool *end)
{
   bool *itr;

   int32_t touchCount = 0;

   for (itr = begin; itr != end; ++itr)
      touchCount += *itr;

   if(!touchCount)
   {
      for (itr = begin; itr != end; ++itr)
         *itr = true;

      return false;
   }

   return true;
}

//
// ACS_traceScriptACS0
//
static void ACS_traceScriptACS0(ACSVM *vm, uint32_t lumpLength, byte *data,
                                bool *codeTouched, uint32_t index,
                                uint32_t &jumpCount, bool compressed)
{
   uint32_t indexNext, opSize;
   int32_t op;
   acs0_opdata_t const *opdata;

   for(;;)
   {
      if(compressed)
      {
         opSize = 1;
         if((op = data[index]) >= 240)
         {
            ++opSize;
            op = 240 + ((op - 240) << 8) + data[index + 1];
         }
      }
      else
      {
         opSize = 4;
         op = SwapLong(*(int32_t *)(data + index));
      }

      // Invalid opcode terminates tracer.
      if(op >= ACS0_OPMAX || op < 0)
      {
         // But flag it so that a KILL gets generated by the translator.
         ACS_touchScriptACS0(codeTouched + index, codeTouched + index + opSize);
         ++vm->numCode;
         return;
      }

      opdata = &ACS0opdata[op];

      // Some instructions require special handling to find next index.
      switch(op)
      {
      case ACS0_OP_LINESPEC1_IMM_BYTE:
      case ACS0_OP_LINESPEC2_IMM_BYTE:
      case ACS0_OP_LINESPEC3_IMM_BYTE:
      case ACS0_OP_LINESPEC4_IMM_BYTE:
      case ACS0_OP_LINESPEC5_IMM_BYTE:
      case ACS0_OP_GET_IMM_BYTE:
      case ACS0_OP_GET2_IMM_BYTE:
      case ACS0_OP_GET3_IMM_BYTE:
      case ACS0_OP_GET4_IMM_BYTE:
      case ACS0_OP_GET5_IMM_BYTE:
      case ACS0_OP_DELAY_IMM_BYTE:
      case ACS0_OP_RANDOM_IMM_BYTE:
         indexNext = index + opSize + opdata->args;
         break;

      case ACS0_OP_GETARR_IMM_BYTE:
         // Set indexNext to after the op.
         indexNext = index + opSize;

         // Need room for the get count.
         if(indexNext > lumpLength-4) return;

         // Read number of gets.
         indexNext += data[indexNext] + 1;

         break;

      case ACS0_OP_BRANCH_CASETABLE:
         // Set indexNext to after the op plus alignment.
         indexNext = (index + opSize + 3) & ~3;

         // Need room for the case count.
         if(indexNext > lumpLength-4) return;

         // Read the number of cases.
         indexNext += (SwapULong(*(uint32_t *)(data + indexNext)) * 8) + 4;

         break;

      default:
         if(compressed && opdata->compressed)
            indexNext = index + opSize + opdata->args;
         else
            indexNext = index + opSize + (opdata->args * 4);
         break;
      }

      // Leaving the bounds of the lump also terminates the tracer.
      if (indexNext > lumpLength) return;

      // If already touched this instruction, stop tracing.
      if(ACS_touchScriptACS0(codeTouched + index, codeTouched + indexNext))
         return;

      // Determine how many internal codes this counts for.
      switch (op)
      {
      case ACS0_OP_LINESPEC1_IMM:
      case ACS0_OP_LINESPEC2_IMM:
      case ACS0_OP_LINESPEC3_IMM:
      case ACS0_OP_LINESPEC4_IMM:
      case ACS0_OP_LINESPEC5_IMM:
      case ACS0_OP_LINESPEC1_IMM_BYTE:
      case ACS0_OP_LINESPEC2_IMM_BYTE:
      case ACS0_OP_LINESPEC3_IMM_BYTE:
      case ACS0_OP_LINESPEC4_IMM_BYTE:
      case ACS0_OP_LINESPEC5_IMM_BYTE:
      case ACS0_OP_GET2_IMM_BYTE:
      case ACS0_OP_GET3_IMM_BYTE:
      case ACS0_OP_GET4_IMM_BYTE:
      case ACS0_OP_GET5_IMM_BYTE:
      case ACS0_OP_GET_THINGX:
      case ACS0_OP_GET_THINGY:
      case ACS0_OP_GET_THINGZ:
      case ACS0_OP_GET_THINGFLOORZ:
      case ACS0_OP_GET_THINGANGLE:
         vm->numCode += opdata->args + 2;
         break;

      case ACS0_OP_GETARR_IMM_BYTE:
         vm->numCode += *(data + index + opSize) + 2;
         break;

      case ACS0_OP_ACTIVATORHEALTH:
      case ACS0_OP_ACTIVATORARMOR:
      case ACS0_OP_ACTIVATORFRAGS:
      case ACS0_OP_BRANCH_RETURNVOID:
      case ACS0_OP_PLAYERNUMBER:
      case ACS0_OP_ACTIVATORTID:
      case ACS0_OP_SIGILPIECES:
         vm->numCode += opdata->opdata->args + 1 + 2; // GET_IMM 0
         break;

      case ACS0_OP_GAMETYPE_ONEFLAGCTF:
         vm->numCode += 2; // GET_IMM
         break;
      case ACS0_OP_GAMETYPE_SINGLEPLAYER:
         vm->numCode += 4; // GAMETYPE + GET_IMM + CMP_EQ
         break;

      case ACS0_OP_BRANCH_CALLDISCARD:
         vm->numCode += opdata->opdata->args + 1 + 1; // DROP
         break;

      case ACS0_OP_BRANCH_CASETABLE:
         vm->numCode += 2;
         // More alignment stuff.
         vm->numCode += SwapULong(*(uint32_t *)(((uintptr_t)data + index + opSize + 3) & ~3)) * 2;
         break;

      default:
         // Translation to CALLFUNC.
         if(opdata->opdata->op == ACS_OP_CALLFUNC_IMM)
         {
            // Adds the func and argc arguments.
            vm->numCode += opdata->args + 1 + 2;
            break;
         }

         // Direct translation.
#ifdef RANGECHECK
         // This should never happen.
         if(opdata->opdata->args == -1)
            I_Error("Unknown translation for opcode. opcode %i", (int)op);
#endif

         vm->numCode += opdata->opdata->args + 1;
         break;
      }

      // Advance the index past the instruction.
      switch(op)
      {
      case ACS0_OP_SCRIPT_TERMINATE:
      case ACS0_OP_BRANCH_RETURN:
      case ACS0_OP_BRANCH_RETURNVOID:
         return;

      case ACS0_OP_BRANCH_IMM:
         ++jumpCount;
         index = SwapLong(*(int32_t *)(data + index + opSize));
         continue;

      case ACS0_OP_BRANCH_NOTZERO:
      case ACS0_OP_BRANCH_ZERO:
         ++jumpCount;
         ACS_traceScriptACS0(vm, lumpLength, data, codeTouched,
                             SwapLong(*(int32_t *)(data + index + opSize)),
                             jumpCount, compressed);
         break;

      case ACS0_OP_BRANCH_CASE:
         ++jumpCount;
         ACS_traceScriptACS0(vm, lumpLength, data, codeTouched,
                             SwapLong(*(int32_t *)(data + index + opSize + 4)),
                             jumpCount, compressed);
         break;

      case ACS0_OP_BRANCH_CASETABLE:
         {
            uint32_t jumps, *rover;

            rover = (uint32_t *)(data + index + opSize);
            // And alignment again.
            rover = (uint32_t *)(((uintptr_t)rover + 3) & ~3);
            jumps = SwapULong(*rover++);

            jumpCount += jumps;

            // Trace all of the jump targets.
            // Start by incrementing rover to point to address.
            for(++rover; jumps--; rover += 2)
               ACS_traceScriptACS0(vm, lumpLength, data, codeTouched,
                                   SwapULong(*rover), jumpCount, compressed);
         }
         break;
      }

      index = indexNext;
   }
}

//
// ACS_translateFuncACS0
//
// Translates an ACS0 opdata into a CALLFUNC func/argc pair.
//
static void ACS_translateFuncACS0(int32_t *&codePtr, const acs0_opdata_t *opdata)
{
   acs_funcnum_t func;
   uint32_t argc;

   #define CASE(OP,FUNC,ARGC) \
      case ACS0_OP_##OP: func = ACS_FUNC_##FUNC; argc = ARGC; break
   #define CASE_IMM(OP,FUNC,ARGC) \
      case ACS0_OP_##OP##_IMM: CASE(OP, FUNC, ARGC)

   switch(opdata->op)
   {
   CASE(ACTIVATORSOUND,         ActivatorSound,         2);
   CASE(AMBIENTSOUND,           AmbientSound,           2);
   CASE(AMBIENTSOUNDLOCAL,      AmbientSoundLocal,      2);
   CASE(GETSECTORCEILINGZ,      GetSectorCeilingZ,      3);
   CASE(GETSECTORFLOORZ,        GetSectorFloorZ,        3);
   CASE(SECTORSOUND,            SectorSound,            2);
   CASE(SETLINEBLOCKING,        SetLineBlocking,        2);
   CASE(SETLINEMONSTERBLOCKING, SetLineMonsterBlocking, 2);
   CASE(SETLINESPECIAL,         SetLineSpecial,         7);
   CASE(SETLINETEXTURE,         SetLineTexture,         4);
   CASE(SETMUSIC_ST,            SetMusic,               1);
   CASE(SETTHINGSPECIAL,        SetThingSpecial,        7);
   CASE(SOUNDSEQUENCE,          SoundSequence,          1);
   CASE(THINGPROJECTILE,        ThingProjectile,        7);
   CASE(THINGSOUND,             ThingSound,             3);

   CASE_IMM(CHANGECEILING, ChangeCeiling, 2);
   CASE_IMM(CHANGEFLOOR,   ChangeFloor,   2);
   case ACS0_OP_RANDOM_IMM_BYTE:
   CASE_IMM(RANDOM,        Random,        2);
   CASE_IMM(SETMUSIC,      SetMusic,      3);
   CASE_IMM(SETMUSICLOCAL, SetMusicLocal, 3);
   CASE_IMM(SPAWNPOINT,    SpawnPoint,    6);
   CASE_IMM(SPAWNSPOT,     SpawnSpot,     4);
   CASE_IMM(THINGCOUNT,    ThingCount,    2);

   default: opdata = &ACS0opdata[ACS_OP_KILL]; CASE(NOP, NOP, 0);
   }

   #undef CASE_IMM
   #undef CASE

   *codePtr++ = opdata->opdata->op;
   *codePtr++ = func;
   *codePtr++ = argc;
}

//
// ACS_translateThingVarACS0
//
// Translates an ACS0 opdata into a THINGVAR to access.
//
static int32_t ACS_translateThingVarACS0(const acs0_opdata_t *opdata)
{
   #define CASE(OP,VAR) case ACS0_OP_##OP: return ACS_THINGVAR_##VAR

   switch(opdata->op)
   {
   CASE(ACTIVATORARMOR,  Armor);
   CASE(ACTIVATORFRAGS,  Frags);
   CASE(ACTIVATORHEALTH, Health);
   CASE(ACTIVATORTID,    TID);
   CASE(GET_THINGANGLE,  Angle);
   CASE(GET_THINGFLOORZ, FloorZ);
   CASE(GET_THINGX,      X);
   CASE(GET_THINGY,      Y);
   CASE(GET_THINGZ,      Z);
   CASE(PLAYERNUMBER,    PlayerNumber);
   CASE(SIGILPIECES,     SigilPieces);

   default: return ACS_THINGVARMAX;
   }

   #undef CASE
}

//
// ACS_translateScriptACS0
//
static void ACS_translateScriptACS0(ACSVM *vm, uint32_t lumpLength, byte *data,
                                    bool *codeTouched, int32_t *codeIndexMap,
                                    uint32_t jumpCount, bool compressed)
{
   byte *brover;
   int32_t *codePtr = vm->code, *rover;
   uint32_t codeIndex = 1; // Start at 1 so 0 is invalid instruction.
   uint32_t opSize;
   int32_t op, temp;
   acs0_opdata_t const *opdata;
   int32_t **jumps, **jumpItr;

   // This is used to store all of the places that need to have a jump target
   // translated. The count was determined by the tracer.
   jumps = (int32_t **)Z_Malloc(jumpCount * sizeof(int32_t *), PU_STATIC, NULL);
   jumpItr = jumps;

   // Set the first instruction to a KILL.
   *codePtr++ = ACS_OP_KILL;

   while(codeIndex < lumpLength)
   {
      // Search for code.
      if(!codeTouched[codeIndex])
      {
         ++codeIndex;
         continue;
      }

      if(compressed)
      {
         opSize = 1;
         if((op = data[codeIndex]) >= 240)
         {
            ++opSize;
            op = 240 + ((op - 240) << 8) + data[codeIndex + 1];
         }
      }
      else
      {
         opSize = 4;
         op = SwapLong(*(int32_t *)(data + codeIndex));
      }

      rover = (int32_t *)(data + codeIndex + opSize);

      // Unrecognized instructions need to stop execution.
      if(op >= ACS0_OPMAX || op < 0)
      {
         *codePtr++ = ACS_OP_KILL;
         codeIndex += opSize;
         continue;
      }

      opdata = &ACS0opdata[op];

      codeIndexMap[codeIndex] = codePtr - vm->code;

      // Some instructions require special handling to find next index.
      switch(op)
      {
      case ACS0_OP_LINESPEC1_IMM_BYTE:
      case ACS0_OP_LINESPEC2_IMM_BYTE:
      case ACS0_OP_LINESPEC3_IMM_BYTE:
      case ACS0_OP_LINESPEC4_IMM_BYTE:
      case ACS0_OP_LINESPEC5_IMM_BYTE:
      case ACS0_OP_GET_IMM_BYTE:
      case ACS0_OP_GET2_IMM_BYTE:
      case ACS0_OP_GET3_IMM_BYTE:
      case ACS0_OP_GET4_IMM_BYTE:
      case ACS0_OP_GET5_IMM_BYTE:
      case ACS0_OP_DELAY_IMM_BYTE:
      case ACS0_OP_RANDOM_IMM_BYTE:
         codeIndex += opSize + opdata->args;
         break;

      case ACS0_OP_GETARR_IMM_BYTE:
         codeIndex += opSize + *(data + codeIndex + opSize) + 1;
         break;

      case ACS0_OP_BRANCH_CASETABLE:
         // Alignment, as in the equivalent switch above.
         codeIndex = (codeIndex + opSize + 3) & ~3;
         codeIndex = codeIndex + (SwapULong(*(uint32_t *)(data + codeIndex)) * 8) + 4;
         break;

      default:
         if(compressed && opdata->compressed)
            codeIndex += opSize + opdata->args;
         else
            codeIndex += opSize + (opdata->args * 4);
         break;
      }

      switch(op)
      {
      case ACS0_OP_LINESPEC1:
      case ACS0_OP_LINESPEC2:
      case ACS0_OP_LINESPEC3:
      case ACS0_OP_LINESPEC4:
      case ACS0_OP_LINESPEC5:
         temp = op - ACS0_OP_LINESPEC1 + 1;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = SwapLong(*rover++);
         *codePtr++ = temp;
         break;

      case ACS0_OP_LINESPEC1_IMM:
      case ACS0_OP_LINESPEC2_IMM:
      case ACS0_OP_LINESPEC3_IMM:
      case ACS0_OP_LINESPEC4_IMM:
      case ACS0_OP_LINESPEC5_IMM:
         temp = opdata->args - 1;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = SwapLong(*rover++);
         *codePtr++ = temp;
         while(temp--)
            *codePtr++ = SwapLong(*rover++);
         break;

      case ACS0_OP_LINESPEC5_RET:
         temp = op - ACS0_OP_LINESPEC5_RET + 5;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = SwapLong(*rover++);
         *codePtr++ = temp;
         break;

      case ACS0_OP_LINESPEC1_IMM_BYTE:
      case ACS0_OP_LINESPEC2_IMM_BYTE:
      case ACS0_OP_LINESPEC3_IMM_BYTE:
      case ACS0_OP_LINESPEC4_IMM_BYTE:
      case ACS0_OP_LINESPEC5_IMM_BYTE:
         brover = (byte *)rover;
         temp = opdata->args - 1;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = *brover++;
         *codePtr++ = temp;
         while(temp--)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_GET_IMM_BYTE:
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = *(byte *)rover;
         break;

      case ACS0_OP_GET2_IMM_BYTE:
      case ACS0_OP_GET3_IMM_BYTE:
      case ACS0_OP_GET4_IMM_BYTE:
      case ACS0_OP_GET5_IMM_BYTE:
         brover = (byte *)rover;
         temp = op - ACS0_OP_GET2_IMM_BYTE + 2;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = temp;
         while(temp--)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_GET_THINGX:
      case ACS0_OP_GET_THINGY:
      case ACS0_OP_GET_THINGZ:
      case ACS0_OP_GET_THINGFLOORZ:
      case ACS0_OP_GET_THINGANGLE:
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = ACS_translateThingVarACS0(opdata);
         break;

      case ACS0_OP_GETARR_IMM_BYTE:
         brover = (byte *)rover;
         temp = *brover++;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = temp;
         while(temp--)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_BRANCH_IMM:
      case ACS0_OP_BRANCH_NOTZERO:
      case ACS0_OP_BRANCH_ZERO:
         *jumpItr++ = codePtr + 1;
         goto case_direct;

      case ACS0_OP_BRANCH_CASE:
         *jumpItr++ = codePtr + 2;
         goto case_direct;

      case ACS0_OP_CHANGEFLOOR_IMM:
      case ACS0_OP_CHANGECEILING_IMM:
         ACS_translateFuncACS0(codePtr, opdata);
         *codePtr++ = SwapLong(*rover++);
         *codePtr++ = SwapLong(*rover++) + vm->strings; // tag string
         for(int i = opdata->args - 2; i--;)
            *codePtr++ = SwapLong(*rover++);
         break;

      case ACS0_OP_DELAY_IMM_BYTE:
         brover = (byte *)rover;
         *codePtr++ = opdata->opdata->op;
         for(int i = opdata->args; i--;)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_ACTIVATORHEALTH:
      case ACS0_OP_ACTIVATORARMOR:
      case ACS0_OP_ACTIVATORFRAGS:
      case ACS0_OP_PLAYERNUMBER:
      case ACS0_OP_ACTIVATORTID:
         *codePtr++ = ACS_OP_GET_IMM;
         *codePtr++ = 0;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = ACS_translateThingVarACS0(opdata);
         break;

      case ACS0_OP_GAMETYPE_ONEFLAGCTF:
         *codePtr++ = ACS_OP_GET_IMM;
         *codePtr++ = 0;
         break;

      case ACS0_OP_GAMETYPE_SINGLEPLAYER:
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = ACS_OP_GET_IMM;
         *codePtr++ = gt_single;
         *codePtr++ = ACS_OP_CMP_EQ;
         break;

      case ACS0_OP_RANDOM_IMM_BYTE:
         brover = (byte *)rover;
         ACS_translateFuncACS0(codePtr, opdata);
         for(int i = opdata->args; i--;)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_SETMUSIC_IMM:
      case ACS0_OP_SETMUSICLOCAL_IMM:
      case ACS0_OP_SPAWNPOINT_IMM:
      case ACS0_OP_SPAWNSPOT_IMM:
         ACS_translateFuncACS0(codePtr, opdata);
         *codePtr++ = SwapLong(*rover++) + vm->strings; // tag string
         for(int i = opdata->args - 1; i--;)
            *codePtr++ = SwapLong(*rover++);
         break;

      case ACS0_OP_BRANCH_CALLDISCARD:
         *codePtr++ = opdata->opdata->op;
         if(compressed)
            *codePtr++ = *(byte *)rover;
         else
            *codePtr++ = SwapLong(*rover);
         *codePtr++ = ACS_OP_STACK_DROP;
         break;

      case ACS0_OP_BRANCH_RETURNVOID:
         *codePtr++ = ACS_OP_GET_IMM;
         *codePtr++ = 0;
         *codePtr++ = opdata->opdata->op;
         break;

      case ACS0_OP_BRANCH_CASETABLE:
         // Align rover.
         rover = (int32_t *)(((uintptr_t)rover + 3) & ~3);
         temp = SwapLong(*rover++);

         *codePtr++ = opdata->opdata->op;
         *codePtr++ = temp;

         while(temp--)
         {
            *codePtr++ = SwapLong(*rover++);
            *jumpItr++ = codePtr;
            *codePtr++ = SwapLong(*rover++);
         }
         break;

      default: case_direct:
         // Direct translation.
         if(opdata->opdata->op == ACS_OP_CALLFUNC ||
            opdata->opdata->op == ACS_OP_CALLFUNC_IMM)
         {
            ACS_translateFuncACS0(codePtr, opdata);
         }
         else
            *codePtr++ = opdata->opdata->op;

         if(compressed && opdata->compressed)
         {
            brover = (byte *)rover;
            for(int i = opdata->args; i--;)
               *codePtr++ = *brover++;
         }
         else
         {
            for(int i = opdata->args; i--;)
               *codePtr++ = SwapLong(*rover++);
         }

         break;
      }
   }

   // Set the last instruction to a KILL.
   *codePtr++ = ACS_OP_KILL;

#ifdef RANGECHECK
   // If this isn't true, something very wrong has happened internally.
   if (codePtr != vm->code + vm->numCode)
      I_Error("Incorrect code count. %i/%i/%i", (int)(codePtr - vm->code),
              (int)vm->numCode, (int)(lumpLength / 4));

   // Same here, this is just a sanity check.
   if (jumpItr != jumps + jumpCount)
      I_Error("Incorrect jump count. %i/%i", (int)(jumpItr - jumps), (int)jumpCount);
#endif

   // Translate jumps. Has to be done after code in order to jump forward.
   while(jumpItr != jumps)
   {
      codePtr = *--jumpItr;

      if ((uint32_t)*codePtr < lumpLength)
         *codePtr = codeIndexMap[*codePtr];
      else
         *codePtr = 0;
   }
}


//----------------------------------------------------------------------------|
// Global Functions                                                           |
//

//
// ACS_LoadScriptACS0
//
void ACS_LoadScriptACS0(ACSVM *vm, WadDirectory *dir, int lump, byte *data)
{
   uint32_t lumpAvail; // Used in bounds checking.
   uint32_t lumpLength = dir->lumpLength(lump);
   int32_t *rover;
   uint32_t tableIndex;

   // Header + table index + script count + string count = 16 bytes.
   if (lumpLength < 16) return;
   lumpAvail = lumpLength - 16;

   rover = (int32_t *)data + 1;

   // Find script table.
   tableIndex = SwapLong(*rover);

   // Aha, but there might really be an ACSE header here!
   if(tableIndex >= 8 && tableIndex <= lumpLength)
   {
      uint32_t fakeHeader = SwapULong(*(uint32_t *)(data + tableIndex - 4));

      if(fakeHeader == ACS_CHUNKID('A', 'C', 'S', 'E'))
      {
         ACS_LoadScriptACSE(vm, dir, lump, data, tableIndex - 8);
         return;
      }
      else if(fakeHeader == ACS_CHUNKID('A', 'C', 'S', 'e'))
      {
         ACS_LoadScriptACSe(vm, dir, lump, data, tableIndex - 8);
         return;
      }
   }

   // At the index there must be at least the script count and string count.
   // Subtracting from lumpLength instead of adding to tableIndex in case the
   // latter would cause an overflow.
   if (tableIndex > lumpLength - 8) return;

   rover = (int32_t *)(data + tableIndex);

   // Read script count.
   vm->numScripts = SwapLong(*rover++);

   // Verify that there is enough space for the given number of scripts.
   if (vm->numScripts * 12 > lumpAvail) return;
   lumpAvail -= vm->numScripts * 12;

   // Also verify that the string count will be inside the lump.
   if (tableIndex + 8 > lumpLength - (vm->numScripts * 12)) return;
   tableIndex += 8 + (vm->numScripts * 12);

   // Read scripts.
   vm->scripts = estructalloctag(acscript_t, vm->numScripts, PU_LEVEL);

   for(acscript_t *end = vm->scripts + vm->numScripts,
       *itr = vm->scripts; itr != end; ++itr)
   {
      itr->number    = SwapLong(*rover++);
      itr->codeIndex = SwapLong(*rover++);
      itr->numArgs   = SwapLong(*rover++);

      itr->numVars   = ACS_NUM_LOCALVARS;
      itr->vm        = vm;

      if(itr->number >= 1000)
      {
         itr->type = ACS_STYPE_OPEN;
         itr->number -= 1000;
      }
      else
         itr->type = ACS_STYPE_CLOSED;
   }

   // Read string count.
   vm->numStrings = SwapLong(*rover++);

   // Again, verify that there is enough space for the table first.
   if (vm->numStrings * 4 > lumpAvail) return;
   lumpAvail -= vm->numStrings * 4;

   // This time, just verify the end of the table is in bounds.
   if (tableIndex > lumpLength - (vm->numStrings * 4)) return;

   // Read strings.
   ACSVM::GlobalNumStrings += vm->numStrings;
   ACSVM::GlobalStrings = (const char **)Z_Realloc(ACSVM::GlobalStrings,
      ACSVM::GlobalNumStrings * sizeof(const char *), PU_LEVEL, NULL);

   for(const char **itr = ACSVM::GlobalStrings + vm->strings,
       **end = itr + vm->numStrings; itr != end; ++itr)
   {
      tableIndex = SwapLong(*rover++);

      if (tableIndex < lumpLength)
         *itr = ACS_readStringACS0(data + tableIndex, data + lumpLength);
      else
         *itr = "";
   }

   // Read code.
   ACS_LoadScriptCodeACS0(vm, data, lumpLength, false);

   vm->loaded = true;
}

//
// ACS_LoadScriptCodeACS0
//
void ACS_LoadScriptCodeACS0(ACSVM *vm, byte *data, uint32_t lumpLength, bool compressed)
{
   int32_t *codeIndexMap;
   bool *codeTouched;
   uint32_t jumpCount = 0;

   vm->numCode = 1; // Start at 1 so that 0 is an invalid index.

   // Find where there is code by tracing potential execution paths...
   codeTouched = ecalloc(bool *, lumpLength, sizeof(bool));

   // ... from scripts.
   for(acscript_t *itr = vm->scripts, *end = itr + vm->numScripts; itr != end; ++itr)
   {
      ACS_traceScriptACS0(vm, lumpLength, data, codeTouched, itr->codeIndex,
                          jumpCount, compressed);
   }

   // ... from functions.
   for(ACSFunc *itr = vm->funcs, *end = itr + vm->numFuncs; itr!= end; ++itr)
   {
      ACS_traceScriptACS0(vm, lumpLength, data, codeTouched, itr->codeIndex,
                          jumpCount, compressed);
   }

   ++vm->numCode; // Add a KILL to the end as well, to avoid running off the end.

   // Translate all the instructions found.
   vm->code = (int32_t *)Z_Malloc(vm->numCode * sizeof(int32_t), PU_LEVEL, NULL);
   codeIndexMap = ecalloc(int32_t *, lumpLength, sizeof(int32_t));
   ACS_translateScriptACS0(vm, lumpLength, data, codeTouched, codeIndexMap,
                           jumpCount, compressed);

   efree(codeTouched);

   // Process script indexes.
   for(acscript_t *itr = vm->scripts, *end = itr + vm->numScripts; itr != end; ++itr)
   {
      if(itr->codeIndex < lumpLength)
         itr->codePtr = vm->code + codeIndexMap[itr->codeIndex];
      else
         itr->codePtr = vm->code;
   }

   // Process function indexes.
   for(ACSFunc *itr = vm->funcs, *end = itr + vm->numFuncs; itr!= end; ++itr)
   {
      if(itr->codeIndex < lumpLength)
         itr->codePtr = vm->code + codeIndexMap[itr->codeIndex];
      else
         itr->codePtr = vm->code;
   }

   efree(codeIndexMap);
}

// EOF


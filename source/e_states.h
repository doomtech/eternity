// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
//
// EDF States Module
//
// By James Haley
//
//----------------------------------------------------------------------------

#ifndef E_STATES_H__
#define E_STATES_H__

int E_StateNumForDEHNum(int dehnum);        // dehnum lookup
int E_GetStateNumForDEHNum(int dehnum);     //    fatal error version
int E_SafeState(int dehnum);                //    fallback version
int E_SafeStateName(const char *name);      //    fallback by name
int E_StateNumForName(const char *name);    // mnemonic lookup
int E_GetStateNumForName(const char *name); //    fatal error version

extern int NullStateNum;

// EDF-Only Definitions/Declarations
#ifdef NEED_EDF_DEFINITIONS

#define EDF_SEC_FRAME    "frame"
#define EDF_SEC_FRMDELTA "framedelta"

extern cfg_opt_t edf_frame_opts[];
extern cfg_opt_t edf_fdelta_opts[];

void    E_CollectStates(cfg_t *scfg);
void    E_ProcessStates(cfg_t *cfg);
void    E_ProcessStateDeltas(cfg_t *cfg);
boolean E_AutoAllocStateDEHNum(int statenum);

#endif

//
// DECORATE state output structure 1:
//
// DECORATE state. This is a label plus a pointer to the state_t to which it 
// refers (which is already a member of the states array). This can be assigned
// to either a native state or a metastate by the calling code.
//
typedef struct edecstate_s
{
   char    *label; // native state name or metastate key (allocated)
   state_t *state; // pointer to state
} edecstate_t;

//
// DECORATE state output structure 2:
//
// Goto record. This is a goto destination label which requires external
// resolution by the caller because it refers to an ancestral definition
// (super state, or a label not defined within the current state block).
// The data consists of the original jump label text, offset, and a pointer
// to the nextstate field of the state_t which needs to have its nextstate 
// set.
//
typedef struct egoto_s
{
   char       *label;     // label text (allocated)
   int         offset;    // offset, if non-zero
   statenum_t *nextstate; // pointer to field needing resolved value
} egoto_t;

//
// DECORATE state output structure 3:
//
// Kill state. This is a state which needs to be removed from the object
// which is receiving this set of DECORATE states.
//
typedef struct ekillstate_s
{
   char *killname; // native state name or metastate key to nullify/delete
} ekillstate_t;

//
// Aggregate DECORATE state parsing output
//
typedef struct edecstateout_s
{
   edecstate_t  *states;     // states to create
   egoto_t      *gotos;      // gotos to resolve
   ekillstate_t *killstates; // states to kill
   int numstates;            // number of states to add
   int numgotos;             // number of gotos to resolve
   int numkillstates;        // number of states to kill
   int numstatesalloc;       // number of state objects allocated
   int numgotosalloc;        // number of goto objects allocated
   int numkillsalloc;        // number of kill states allocated
} edecstateout_t;

#endif

// EOF


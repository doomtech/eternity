// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
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
// Skin Viewer Widget for the Menu
//
//    Why? Because I can!
//
// haleyjd 04/20/03
//
//-----------------------------------------------------------------------------

#include "doomstat.h"
#include "doomtype.h"
#include "info.h"
#include "mn_engin.h"
#include "r_defs.h"
#include "r_draw.h"
#include "v_video.h"
#include "w_wad.h"
#include "p_pspr.h"
#include "p_skin.h"
#include "s_sound.h"
#include "sounds.h"
#include "m_random.h"
#include "d_gi.h"
#include "e_states.h"
#include "e_things.h"

// player skin sprite states
enum
{
   SKV_WALKING,
   SKV_FIRING,
   SKV_PAIN,
   SKV_DEAD,
};

// static state variables

static state_t *skview_state = NULL;
static int skview_tics = 0;
static int skview_action = SKV_WALKING;
static int skview_rot = 0;
static boolean skview_halfspeed = false;
static int skview_typenum; // 07/12/03

// haleyjd 09/29/07: rewrites for player class engine
static statenum_t skview_atkstate2;

//
// MN_SkinSetState
//
// Sets the skin sprite to a state, and sets its tics appropriately.
// The tics will be set to 2x normal if half speed mode is active.
//
static void MN_SkinSetState(state_t *state)
{
   int tics;
   
   skview_state = state;
   
   tics = skview_state->tics;
   skview_tics = menutime + (skview_halfspeed ? 2*tics : tics);
}

//
// MN_SkinResponder
//
// The skin viewer widget responder function. Sorta long and
// hackish, but cool.
//
static boolean MN_SkinResponder(event_t *ev)
{
   // only interested in keydown events
   if(ev->type != ev_keydown)
      return false;

   if(action_menu_toggle || action_menu_previous)
   {
      action_menu_toggle = action_menu_previous = false;

      // kill the widget
      S_StartSound(NULL, gameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
      current_menuwidget = NULL;
      return true;
   }

   if(action_menu_left)
   {
      action_menu_left = false;

      // rotate sprite left
      if(skview_rot == 7)
         skview_rot = 0;
      else
         skview_rot++;

      return true;
   }

   if(action_menu_right)
   {
      action_menu_right = false;

      // rotate sprite right
      if(skview_rot == 0)
         skview_rot = 7;
      else
         skview_rot--;

      return true;
   }

   switch(ev->data1)
   {
   case KEYD_RCTRL:
      // attack!
      if(skview_action == SKV_WALKING)
      {
         S_StartSound(NULL, gameModeInfo->skvAtkSound);
         MN_SkinSetState(&states[skview_atkstate2]);
         skview_action = SKV_FIRING;
      }
      break;
   case 'p':
   case 'P':
      // act hurt
      if(skview_action == SKV_WALKING)
      {
         S_StartSoundName(NULL,
            players[consoleplayer].skin->sounds[sk_plpain]);
         MN_SkinSetState(&states[mobjinfo[skview_typenum].painstate]);
         skview_action = SKV_PAIN;
      }
      break;
   case 'd':
   case 'D':
      // die normally
      if(skview_action != SKV_DEAD)
      {
         S_StartSoundName(NULL,
            (gamemode == shareware || M_Random() % 2) ? 
               players[consoleplayer].skin->sounds[sk_pldeth] :
               players[consoleplayer].skin->sounds[sk_pdiehi]);
         MN_SkinSetState(&states[mobjinfo[skview_typenum].deathstate]);
         skview_action = SKV_DEAD;
      }
      break;
   case 'x':
   case 'X':
      // gib
      if(skview_action != SKV_DEAD)
      {
         S_StartSoundName(NULL,
            players[consoleplayer].skin->sounds[sk_slop]);
         MN_SkinSetState(&states[mobjinfo[skview_typenum].xdeathstate]);
         skview_action = SKV_DEAD;
      }
      break;
   case 'h':
   case 'H':
      // toggle half-speed animation
      skview_halfspeed ^= true;
      break;
   case ' ':
      // "respawn" the player if dead
      if(skview_action == SKV_DEAD)
      {
         S_StartSound(NULL, gameModeInfo->teleSound);
         MN_SkinSetState(&states[mobjinfo[skview_typenum].seestate]);
         skview_action = SKV_WALKING;
      }
      break;
   default:
      return false;
   }

   return true;
}

// haleyjd 05/29/06: got rid of cascading macros in preference of this
#define INSTR_Y ((SCREENHEIGHT - 1) - (gameModeInfo->vtextinfo->cy * 5))

//
// MN_SkinInstructions
//
// Draws some v_font strings to the screen to tell the user how
// to operate this beast.
//
static void MN_SkinInstructions(void)
{
   char *msg = FC_GOLD "skin viewer";

   void (*textfunc)(const char *, int, int) = 
      gameModeInfo->shadowTitles ? V_WriteTextBigShadowed : V_WriteTextBig;

   // draw a title at the top, too

   textfunc(msg, 160 - V_StringWidthBig(msg)/2, 8);

   // haleyjd 05/29/06: rewrote to be binding neutral and to draw all of
   // it with one call to V_WriteText instead of five.
   V_WriteText("instructions:\n"
               FC_GRAY "left" FC_RED " = rotate left, "
               FC_GRAY "right" FC_RED " = rotate right\n"
               FC_GRAY "ctrl" FC_RED " = fire, "
               FC_GRAY "p" FC_RED " = pain, "
               FC_GRAY "d" FC_RED " = die, "
               FC_GRAY "x" FC_RED " = gib\n"
               FC_GRAY "space" FC_RED " = respawn, "
               FC_GRAY "h" FC_RED " = half-speed\n"
               FC_GRAY "toggle or previous" FC_RED " = exit", 4, INSTR_Y);
}

//
// MN_SkinDrawer
//
// The skin viewer widget drawer function. Basically implements a
// small state machine and sprite drawer all in one function. Since
// state transition timing is done through the drawer and not a ticker,
// it's not absolutely precise, but it's good enough.
//
static void MN_SkinDrawer(void)
{
   spritedef_t *sprdef;
   spriteframe_t *sprframe;
   int lump;
   boolean flip;
   patch_t *patch;

   // draw the normal menu background
   V_DrawBackground(gameModeInfo->menuBackground, &vbscreen);

   // draw instructions and title
   MN_SkinInstructions();

   // get the player skin sprite definition
   sprdef = &sprites[players[consoleplayer].skin->sprite];
   if(!(sprdef->spriteframes))
      return;

   // get the current frame, using the skin state and rotation vars
   sprframe = &sprdef->spriteframes[skview_state->frame&FF_FRAMEMASK];
   if(sprframe->rotate)
   {
      lump = sprframe->lump[skview_rot];
      flip = (boolean)sprframe->flip[skview_rot];
   }
   else
   {
      lump = sprframe->lump[0];
      flip = (boolean)sprframe->flip[0];
   }

   // cache the sprite patch -- watch out for "firstspritelump"!
   patch = W_CacheLumpNum(lump+firstspritelump, PU_CACHE);

   // draw the sprite, with color translation and proper flipping
   // 01/12/04: changed translation handling
   V_DrawPatchTranslated(160, 120, &vbscreen, patch,
      players[consoleplayer].colormap ?
        (char *)translationtables[(players[consoleplayer].colormap - 1)] :
        NULL, 
      flip);
}

//
// MN_SkinTicker
//
// haleyjd 05/29/06: separated out from the drawer and added ticker
// support to widgets to enable precise state transition timing.
//
void MN_SkinTicker(void)
{
   if(skview_tics != -1 && menutime >= skview_tics)
   {
      // hack states: these need special nextstate handling so
      // that the player will start walking again afterward
      if(skview_state->nextstate == mobjinfo[skview_typenum].spawnstate)
      {
         MN_SkinSetState(&states[mobjinfo[skview_typenum].seestate]);
         skview_action = SKV_WALKING;
      }
      else
      {
         // normal state transition
         MN_SkinSetState(&states[skview_state->nextstate]);

         // if the state has -1 tics, reset skview_tics
         if(skview_state->tics == -1)
            skview_tics = -1;
      }
   }
}

// the skinviewer menu widget

menuwidget_t skinviewer = { MN_SkinDrawer, MN_SkinResponder, MN_SkinTicker, true };

//
// MN_InitSkinViewer
//
// Called by the skinviewer console command, this function resets
// all the skview internal state variables to their defaults, and
// activates the skinviewer menu widget.
//
void MN_InitSkinViewer(void)
{
   playerclass_t *pclass = players[consoleplayer].pclass; // haleyjd 09/29/07

   // reset all state variables
   skview_action = SKV_WALKING;
   skview_rot = 0;
   skview_halfspeed = false;
   skview_typenum = pclass->type;

   // haleyjd 09/29/07: save alternate attack state number
   skview_atkstate2 = pclass->altattack;

   MN_SkinSetState(&states[mobjinfo[skview_typenum].seestate]);

   // set the widget
   current_menuwidget = &skinviewer;
}

// EOF


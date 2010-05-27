// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
//
// The heart of DOOM itself -- everything is tied together from
// here. Input, demos, netplay, level loading, exit, savegames --
// there's something pertinent to everything.
//
//-----------------------------------------------------------------------------

#include <time.h>
#include <stdarg.h>
#include <errno.h>

#include "d_io.h"
#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "p_info.h"
#include "doomstat.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_shots.h"
#include "mn_engin.h"
#include "mn_menus.h"
#include "m_random.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "d_main.h"
#include "hu_stuff.h"
#include "hu_frags.h" // haleyjd
#include "st_stuff.h"
#include "am_map.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_draw.h"
#include "r_things.h" // haleyjd
#include "p_map.h"
#include "s_sound.h"
#include "dstrings.h"
#include "sounds.h"
#include "p_chase.h"
#include "r_data.h"
#include "r_sky.h"
#include "d_deh.h"              // Ty 3/27/98 deh declarations
#include "p_inter.h"
#include "g_game.h"
#include "c_net.h"
#include "d_net.h"
#include "p_hubs.h"
#include "g_bind.h"
#include "d_dialog.h"
#include "d_gi.h"
#include "in_lude.h"
#include "a_small.h"
#include "g_dmflag.h"
#include "e_states.h"
#include "s_sndseq.h"
#include "acs_intr.h"
#include "metaapi.h"
#include "p_maputl.h"

#define SAVEGAMESIZE  0x20000
#define SAVESTRINGSIZE  24

// haleyjd: new demo format stuff
static char     eedemosig[] = "ETERN";

static size_t   savegamesize = SAVEGAMESIZE; // killough
static char     *demoname;
static boolean  netdemo;
static byte     *demobuffer;   // made some static -- killough
static size_t   maxdemosize;
static byte     *demo_p;
static int16_t  consistancy[MAXPLAYERS][BACKUPTICS];

gameaction_t    gameaction;
gamestate_t     gamestate;
skill_t         gameskill;
boolean         respawnmonsters;
int             gameepisode;
int             gamemap;
// haleyjd: changed to an array
char            gamemapname[9] = { 0,0,0,0,0,0,0,0,0 }; 
boolean         paused;
boolean         sendpause;     // send a pause event next tic
boolean         sendsave;      // send a save event next tic
boolean         usergame;      // ok to save / end game
boolean         timingdemo;    // if true, exit with report on completion
boolean         fastdemo;      // if true, run at full speed -- killough
boolean         nodrawers;     // for comparative timing purposes
boolean         noblit;        // for comparative timing purposes
int             startgametic;
int             starttime;     // for comparative timing purposes
boolean         deathmatch;    // only if started as net death
boolean         netgame;       // only true if packets are broadcast
boolean         playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];
int             consoleplayer; // player taking events and displaying
int             displayplayer; // view being displayed
int             gametic;
int             levelstarttic; // gametic at level start
int             basetic;       // killough 9/29/98: for demo sync
int             totalkills, totalitems, totalsecret;    // for intermission
boolean         demorecording;
boolean         demoplayback;
/*boolean         timedemo_menuscreen;*/
boolean         singledemo;           // quit after playing a demo from cmdline
boolean         precache = true;      // if true, load all graphics at start
wbstartstruct_t wminfo;               // parms for world map / intermission
boolean         haswolflevels = false;// jff 4/18/98 wolf levels present
byte            *savebuffer;
int             autorun = false;      // always running?          // phares
int             runiswalk = false;    // haleyjd 08/23/09
int             automlook = false;
int             bfglook = 1;
int             smooth_turning = 0;   // sf
int             novert;               // haleyjd

// sf: moved sensitivity here
int             mouseSensitivity_horiz; // has default   //  killough
int             mouseSensitivity_vert;  // has default
int             invert_mouse = true;
int             animscreenshot = 0;       // animated screenshots
int             mouseAccel_type = 0;

//
// controls (have defaults)
//

// haleyjd: these keys are not dynamically rebindable

int key_escape = KEYD_ESCAPE;
int key_chat;
int key_help = KEYD_F1;
int key_spy;
int key_pause;
int destination_keys[MAXPLAYERS];

// haleyjd: mousebfire is now unused -- removed
int mousebstrafe;   // double-clicking either of these buttons
int mousebforward;  // causes a use action, however

// haleyjd: joyb variables are obsolete -- removed

#define MAXPLMOVE      (pc->forwardmove[1])
#define TURBOTHRESHOLD (pc->oforwardmove[1])
#define SLOWTURNTICS   6
#define QUICKREVERSE   32768 // 180 degree reverse                    // phares

boolean gamekeydown[NUMKEYS];
int     turnheld;       // for accelerative turning

boolean mousearray[4];
boolean *mousebuttons = &mousearray[1];    // allow [-1]

// mouse values are used once
int mousex;
int mousey;
int dclicktime;
int dclickstate;
int dclicks;
int dclicktime2;
int dclickstate2;
int dclicks2;

// joystick values are repeated
int joyxmove;
int joyymove;

int  savegameslot;
char savedescription[32];

//jff 3/24/98 declare startskill external, define defaultskill here
extern skill_t startskill;      //note 0-based
int defaultskill;               //note 1-based

// killough 2/8/98: make corpse queue variable in size
int bodyqueslot, bodyquesize, default_bodyquesize; // killough 2/8/98, 10/98

void *statcopy;       // for statistics driver

int keylookspeed = 5;

int cooldemo = false;
int cooldemo_tics;      // number of tics until changing view

void G_CoolViewPoint();

//
// G_BuildTiccmd
//
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
// NETCODE_FIXME: The ticcmd_t structure will probably need to be
// altered to better support command packing.
//
void G_BuildTiccmd(ticcmd_t *cmd)
{
   boolean strafe;
   boolean bstrafe;
   boolean sendcenterview = false;
   int speed;
   int tspeed;
   int forward;
   int side;
   int newweapon;            // phares
   int look = 0; 
   int mlook = 0;
   static int prevmlook = 0;
   ticcmd_t *base;
   int tmousex, tmousey;     // local mousex, mousey
   playerclass_t *pc = players[consoleplayer].pclass;

   base = I_BaseTiccmd();    // empty, or external driver
   memcpy(cmd, base, sizeof(*cmd));

#ifdef CONSHUGE
   // in console mode the whole ticcmd is used
   // to transfer console command chars
   
   if(gamestate == GS_CONSOLE)
   {                         
      int i;
      
      // fill ticcmd with console chars
      for(i = 0; i < sizeof(ticcmd_t); i++)
      {
         ((unsigned char*)cmd)[i] = C_dequeueChatChar();
      }
      return;
   }
#endif

   cmd->consistancy = consistancy[consoleplayer][maketic%BACKUPTICS];

   strafe = action_strafe;
   //speed = autorun || action_speed;
   speed = (autorun ? !(runiswalk && action_speed) : action_speed);
   
   forward = side = 0;

   // use two stage accelerative turning on the keyboard and joystick
   if(joyxmove < 0 || joyxmove > 0 || action_right || action_left)
      turnheld += ticdup;
   else
      turnheld = 0;

   if(turnheld < SLOWTURNTICS)
      tspeed = 2;             // slow turn
   else
      tspeed = speed;

   // turn 180 degrees in one keystroke? -- phares
   if(action_flip)
   {
      cmd->angleturn += (int16_t)QUICKREVERSE;
      action_flip = false;
   }

  // let movement keys cancel each other out
   if(strafe)
   {
      if(action_right)
         side += pc->sidemove[speed];
      if(action_left)
         side -= pc->sidemove[speed];
      if(joyxmove > 0)
         side += pc->sidemove[speed];
      if(joyxmove < 0)
         side -= pc->sidemove[speed];
   }
   else
   {
      if(action_right)
         cmd->angleturn -= (int16_t)pc->angleturn[tspeed];
      if(action_left)
         cmd->angleturn += (int16_t)pc->angleturn[tspeed];
      if(joyxmove > 0)
         cmd->angleturn -= (int16_t)pc->angleturn[tspeed];
      if(joyxmove < 0)
         cmd->angleturn += (int16_t)pc->angleturn[tspeed];
   }

   if(action_forward)
      forward += pc->forwardmove[speed];
   if(action_backward)
      forward -= pc->forwardmove[speed];
   if(joyymove < 0)
      forward += pc->forwardmove[speed];
   if(joyymove > 0)
      forward -= pc->forwardmove[speed];
   if(action_moveright)
      side += pc->sidemove[speed];
   if(action_moveleft)
      side -= pc->sidemove[speed];
   if(action_jump)                    // -- joek 12/22/07
      cmd->actions |= AC_JUMP;
   mlook = allowmlook && (action_mlook || automlook);

   // console commands
   cmd->chatchar = C_dequeueChatChar();

   if(action_attack)
      cmd->buttons |= BT_ATTACK;

   if(action_use)
   {
      cmd->buttons |= BT_USE;
      // clear double clicks if hit use button
      dclicks = 0;
   }

   // phares:
   // Toggle between the top 2 favorite weapons.
   // If not currently aiming one of these, switch to
   // the favorite. Only switch if you possess the weapon.
   
   // killough 3/22/98:
   //
   // Perform automatic weapons switch here rather than in p_pspr.c,
   // except in demo_compatibility mode.
   //
   // killough 3/26/98, 4/2/98: fix autoswitch when no weapons are left

   //
   // NETCODE_FIXME -- WEAPON_FIXME
   //
   // In order to later support dynamic weapons, the way weapon
   // changes are handled is going to have to be different from
   // either of the old systems. Packing weapon changes into the ticcmd
   // isn't going to be sufficient any more, there's not enough space
   // to support more than 16 weapons.
   //
 
   if((!demo_compatibility && players[consoleplayer].attackdown &&
       !P_CheckAmmo(&players[consoleplayer])) || action_nextweapon)
   {
      newweapon = P_SwitchWeapon(&players[consoleplayer]); // phares
   }
   else
   {                                 // phares 02/26/98: Added gamemode checks
      newweapon =
        action_weapon1 ? wp_fist :    // killough 5/2/98: reformatted
        action_weapon2 ? wp_pistol :
        action_weapon3 ? wp_shotgun :
        action_weapon4 ? wp_chaingun :
        action_weapon5 ? wp_missile :
        action_weapon6 && GameModeInfo->id != shareware ? wp_plasma :
        action_weapon7 && GameModeInfo->id != shareware ? wp_bfg :
        action_weapon8 ? wp_chainsaw :
        action_weapon9 && enable_ssg ? wp_supershotgun :
        wp_nochange;

      // killough 3/22/98: For network and demo consistency with the
      // new weapons preferences, we must do the weapons switches here
      // instead of in p_user.c. But for old demos we must do it in
      // p_user.c according to the old rules. Therefore demo_compatibility
      // determines where the weapons switch is made.

      // killough 2/8/98:
      // Allow user to switch to fist even if they have chainsaw.
      // Switch to fist or chainsaw based on preferences.
      // Switch to shotgun or SSG based on preferences.
      //
      // killough 10/98: make SG/SSG and Fist/Chainsaw
      // weapon toggles optional
      
      if(!demo_compatibility && doom_weapon_toggles)
      {
         const player_t *player = &players[consoleplayer];

         // only select chainsaw from '1' if it's owned, it's
         // not already in use, and the player prefers it or
         // the fist is already in use, or the player does not
         // have the berserker strength.

         if(newweapon==wp_fist && player->weaponowned[wp_chainsaw] &&
            player->readyweapon!=wp_chainsaw &&
            (player->readyweapon==wp_fist ||
             !player->powers[pw_strength] ||
             P_WeaponPreferred(wp_chainsaw, wp_fist)))
         {
            newweapon = wp_chainsaw;
         }

         // Select SSG from '3' only if it's owned and the player
         // does not have a shotgun, or if the shotgun is already
         // in use, or if the SSG is not already in use and the
         // player prefers it.

         if(newweapon == wp_shotgun && enable_ssg &&
            player->weaponowned[wp_supershotgun] &&
            (!player->weaponowned[wp_shotgun] ||
             player->readyweapon == wp_shotgun ||
             (player->readyweapon != wp_supershotgun &&
              P_WeaponPreferred(wp_supershotgun, wp_shotgun))))
         {
            newweapon = wp_supershotgun;
         }
      }
      // killough 2/8/98, 3/22/98 -- end of weapon selection changes
   }

   // haleyjd 03/06/09: next/prev weapon actions
   if(action_weaponup)
      newweapon = P_NextWeapon(&players[consoleplayer]);
   else if(action_weapondown)
      newweapon = P_PrevWeapon(&players[consoleplayer]);

   if(newweapon != wp_nochange)
   {
      cmd->buttons |= BT_CHANGE;
      cmd->buttons |= newweapon << BT_WEAPONSHIFT;
   }

   // mouse -- haleyjd: some of this is obsolete now -- removed
  
   // forward double click -- haleyjd: still allow double clicks
   if(mousebuttons[mousebforward] != dclickstate && dclicktime > 1 )
   {
      dclickstate = mousebuttons[mousebforward];
      
      if(dclickstate)
         dclicks++;

      if(dclicks == 2)
      {
         cmd->buttons |= BT_USE;
         dclicks = 0;
      }
      else
         dclicktime = 0;
   }
   else if((dclicktime += ticdup) > 20)
   {
      dclicks = 0;
      dclickstate = 0;
   }

   // strafe double click

   bstrafe = mousebuttons[mousebstrafe]; // haleyjd: removed joybstrafe
   if(bstrafe != dclickstate2 && dclicktime2 > 1 )
   {
      dclickstate2 = bstrafe;

      if(dclickstate2)
         dclicks2++;
      
      if(dclicks2 == 2)
      {
         cmd->buttons |= BT_USE;
         dclicks2 = 0;
      }
      else
         dclicktime2 = 0;
   }
   else if((dclicktime2 += ticdup) > 20)
   {
      dclicks2 = 0;
      dclickstate2 = 0;
   }

   // sf: smooth out the mouse movement
   // change to use tmousex, y   
   // divide by the number of new tics so each gets an equal share

   tmousex = mousex /* / newtics */;
   tmousey = mousey /* / newtics */;

   // we average the mouse movement as well
   // this is most important in smoothing movement
   if(smooth_turning)
   {
      static int oldmousex=0, mousex2;
      static int oldmousey=0, mousey2;

      mousex2 = tmousex; mousey2 = tmousey;
      tmousex = (tmousex + oldmousex)/2;        // average
      oldmousex = mousex2;
      tmousey = (tmousey + oldmousey)/2;        // average
      oldmousey = mousey2;
   }

   // YSHEAR_FIXME: add arrow keylook, joystick look?

   if(mlook)
   {
      // YSHEAR_FIXME: provide separate mlook speed setting?
      int lookval = tmousey * 16 / ticdup;
      if(invert_mouse)
         look -= lookval;
      else
         look += lookval;
   }
   else
   {                // just stopped mlooking?
      // YSHEAR_FIXME: make lookspring configurable
      if(prevmlook)
         sendcenterview = true;

      // haleyjd 10/24/08: novert support
      if(!novert)
         forward += tmousey;
   }

   prevmlook = mlook;
   
   if(action_lookup)
      look += pc->lookspeed[speed];
   if(action_lookdown)
      look -= pc->lookspeed[speed];
   if(action_center)
      sendcenterview = true;

   // haleyjd: special value for view centering
   if(sendcenterview)
      look = -32768;
   else
   {
      if(look > 32767)
         look = 32767;
      else if(look < -32767)
         look = -32767;
   }

   if(strafe)
      side += tmousex*2;
   else
      cmd->angleturn -= tmousex*0x8;

   if(forward > MAXPLMOVE)
      forward = MAXPLMOVE;
   else if(forward < -MAXPLMOVE)
      forward = -MAXPLMOVE;
   
   if(side > MAXPLMOVE)
      side = MAXPLMOVE;
   else if(side < -MAXPLMOVE)
      side = -MAXPLMOVE;

   cmd->forwardmove += forward;
   cmd->sidemove += side;
   cmd->look = look;

   // special buttons
   if(sendpause)
   {
      sendpause = false;
      cmd->buttons = BT_SPECIAL | BTS_PAUSE;
   }

   // killough 10/6/98: suppress savegames in demos
   if(sendsave && !demoplayback)
   {
      sendsave = false;
      cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot << BTS_SAVESHIFT);
   }

   mousex = mousey = 0;
}

//
// G_SetGameMap
//
// sf: from gamemapname, get gamemap and gameepisode
//
void G_SetGameMap(void)
{
   gamemap = G_GetMapForName(gamemapname);
   
   if(!(GameModeInfo->flags & GIF_MAPXY))
   {
      gameepisode = gamemap / 10;
      gamemap = gamemap % 10;
   }
   else
      gameepisode = 1;
   
   if(gameepisode < 1)
      gameepisode = 1;

   // haleyjd: simplified to use gameModeInfo

   // bound to maximum episode for gamemode
   // (only start episode 1 on shareware, etc)
   if(gameepisode > GameModeInfo->numEpisodes)
      gameepisode = GameModeInfo->numEpisodes;   
   
   if(gamemap < 0)
      gamemap = 0;
   if(gamemap > 9 && !(GameModeInfo->flags & GIF_MAPXY))
      gamemap = 9;
}

//
// G_SetGameMapName
//
// haleyjd: set and normalize gamemapname
//
void G_SetGameMapName(const char *s)
{
   strncpy(gamemapname, s, 8);
   M_Strupr(gamemapname);
}

extern gamestate_t wipegamestate;
extern gamestate_t oldgamestate;

extern void R_InitPortals(void);

//
// G_DoLoadLevel
//
static void G_DoLoadLevel(void)
{
   int i;
   
   levelstarttic = gametic; // for time calculation
   
   if(!demo_compatibility && demo_version < 203)   // killough 9/29/98
      basetic = gametic;

   gamestate = GS_LEVEL;

   P_SetupLevel(gamemapname, 0, gameskill);

   if(gamestate != GS_LEVEL)       // level load error
   {
      for(i = 0; i < MAXPLAYERS; ++i)
         players[i].playerstate = PST_LIVE;
      return;
   }

   HU_FragsUpdate();

   if(!netgame || demoplayback)
      consoleplayer = 0;
   
   gameaction = ga_nothing;
   displayplayer = consoleplayer;    // view the guy you are playing
   P_ResetChasecam();    // sf: because displayplayer changed
   Z_CheckHeap();

   // clear cmd building stuff
   memset(gamekeydown, 0, sizeof(gamekeydown));
   joyxmove = joyymove = 0;
   mousex = mousey = 0;
   sendpause = sendsave = paused = false;
   memset(mousebuttons, 0, sizeof(mousebuttons));
   G_ClearKeyStates(); // haleyjd 05/20/05: all bindings off

   // killough: make -timedemo work on multilevel demos
   // Move to end of function to minimize noise -- killough 2/22/98:
   
   //jff 4/26/98 wake up the status bar in case were coming out of a DM demo
   // killough 5/13/98: in case netdemo has consoleplayer other than green
   ST_Start();

   C_Popup();  // pop up the console
   
   // sf: if loading a hub level, restore position relative to sector
   //  for 'seamless' travel between levels
   if(hub_changelevel)
   {
      P_RestorePlayerPosition();
   }
   else
   {  // sf: no screen wipe while changing hub level
      if(wipegamestate == GS_LEVEL)
         wipegamestate = -1;             // force a wipe
   }
}

//
// G_Responder
//
// Get info needed to make ticcmd_ts for the players.
//
boolean G_Responder(event_t* ev)
{
   // allow spy mode changes even during the demo
   // killough 2/22/98: even during DM demo
   //
   // killough 11/98: don't autorepeat spy mode switch

   if(ev->data1 == key_spy && netgame && 
      (demoplayback || GameType != gt_dm) && gamestate == GS_LEVEL)
   {
      if(ev->type == ev_keyup)
         gamekeydown[key_spy] = false;
      
      if(ev->type == ev_keydown && !gamekeydown[key_spy])
      {
         gamekeydown[key_spy] = true;
         do // spy mode
         {
            if(++displayplayer >= MAXPLAYERS)
               displayplayer = 0;
         }
         while(!playeringame[displayplayer] && displayplayer != consoleplayer);
         
         ST_Start();    // killough 3/7/98: switch status bar views too
         HU_Start();
         S_UpdateSounds(players[displayplayer].mo);
         P_ResetChasecam();
      }
      return true;
   }

   // killough 9/29/98: reformatted
   if(gamestate == GS_LEVEL && 
      (HU_Responder(ev) ||  // chat ate the event
       ST_Responder(ev) ||  // status window ate it
       AM_Responder(ev)))
   {
      return true;
   }

   if(G_KeyResponder(ev, kac_cmd))
      return true;

   // any other key pops up menu if in demos
   //
   // killough 8/2/98: enable automap in -timedemo demos
   //
   // killough 9/29/98: make any key pop up menu regardless of
   // which kind of demo, and allow other events during playback

   if(gameaction == ga_nothing && (demoplayback || gamestate == GS_DEMOSCREEN))
   {
      // killough 9/29/98: allow user to pause demos during playback
      if (ev->type == ev_keydown && ev->data1 == key_pause)
      {
         if (paused ^= 2)
            S_PauseSound();
         else
            S_ResumeSound();
         return true;
      }

      // killough 10/98:
      // Don't pop up menu, if paused in middle
      // of demo playback, or if automap active.
      // Don't suck up keys, which may be cheats

      // haleyjd: deobfuscated into an if statement
      // fixed a bug introduced in beta 3 that possibly
      // broke the walkcam

      if(!walkcam_active) // if so, we need to go on below
      {
         if(gamestate == GS_DEMOSCREEN && !(paused & 2) && 
            !automapactive &&
            ((ev->type == ev_keydown) ||
             (ev->type == ev_mouse && ev->data1) ||
             (ev->type == ev_joystick && ev->data1)))
         {
            // popup menu
            MN_StartControlPanel();
            return true;
         }
         else
         {
            return false;
         }
      }
   }

   if(gamestate == GS_FINALE && F_Responder(ev))
      return true;  // finale ate the event

   if(action_autorun)
   {
      action_autorun = 0;
      autorun = !autorun;
   }

   switch(ev->type)
   {
   case ev_keydown:
      if(ev->data1 == key_pause) // phares
      {
         C_RunTextCmd("pause");
      }
      else
      {
         if(ev->data1 < NUMKEYS)
            gamekeydown[ev->data1] = true;         
         G_KeyResponder(ev, kac_game); // haleyjd
      }
      return true;    // eat key down events
      
   case ev_keyup:
      if(ev->data1 < NUMKEYS)
         gamekeydown[ev->data1] = false;
      G_KeyResponder(ev, kac_game);   // haleyjd
      return false;   // always let key up events filter down
      
   case ev_mouse:
      mousebuttons[0] = ev->data1 & 1;
      mousebuttons[1] = ev->data1 & 2;
      mousebuttons[2] = ev->data1 & 4;

      // SoM: this mimics the doom2 behavior better. 
      mousex += (ev->data2 * (mouseSensitivity_horiz + 5) / 10);
      mousey += (ev->data3 * (mouseSensitivity_vert + 5) / 10);
      return true;    // eat events
      
   case ev_joystick:
      // haleyjd: joybuttons now obsolete -- removed
      joyxmove = ev->data2;
      joyymove = ev->data3;
      return true;    // eat events
      
   default:
      break;
   }
   
   return false;
}

//
// DEMO RECORDING
//

// killough 2/28/98: A ridiculously large number
// of players, the most you'll ever need in a demo
// or savegame. This is used to prevent problems, in
// case more players in a game are supported later.

#define MIN_MAXPLAYERS 32

// haleyjd 10/08/06: longtics demo support -- DOOM v1.91 writes 111 into the
// version field of its demos (because it's one more than v1.10 I guess).
#define DOOM_191_VERSION 111

static boolean longtics_demo; // if true, demo playing is longtics format

static const char *defdemoname;

//
// G_DemoStartMessage
//
// haleyjd 04/27/10: prints a nice startup message when playing a demo.
//
static void G_DemoStartMessage(const char *basename)
{
   if(demo_version < 200) // Vanilla demos
   {
      C_Printf("Playing demo '%s'\n"
               FC_HI "\tVersion %d.%d\n",
               basename, demo_version / 100, demo_version % 100);
   }
   else if(demo_version >= 200 && demo_version <= 203) // Boom & MBF demos
   {
      C_Printf("Playing demo '%s'\n"
               FC_HI "\tVersion %d.%02d%s\n",
               basename, demo_version / 100, demo_version % 100,
               demo_version >= 200 && demo_version <= 202 ? 
                  (compatibility ? "; comp=on" : "; comp=off") : "");
   }
   else // Eternity demos
   {
      C_Printf("Playing demo '%s'\n"
               FC_HI "\tVersion %d.%02d.%02d\n",
               basename, demo_version / 100, demo_version % 100, demo_subversion);
   }
}

//
// complevels
//
// haleyjd 04/10/10: compatibility matrix for properly setting comp vars based
// on the current demoversion. Derived from PrBoom.
//
struct complevel_s
{
   int fix; // first version that contained a fix
   int opt; // first version that made the fix an option
} complevels[] =
{
   { 203, 203 }, // comp_telefrag
   { 203, 203 }, // comp_dropoff
   { 201, 203 }, // comp_vile
   { 201, 203 }, // comp_pain
   { 201, 203 }, // comp_skull
   { 201, 203 }, // comp_blazing
   { 201, 203 }, // comp_doorlight
   { 201, 203 }, // comp_model
   { 201, 203 }, // comp_god
   { 203, 203 }, // comp_falloff
   { 200, 203 }, // comp_floors - FIXME
   { 201, 203 }, // comp_skymap
   { 203, 203 }, // comp_pursuit
   { 202, 203 }, // comp_doorstuck
   { 203, 203 }, // comp_staylift
   { 203, 203 }, // comp_zombie
   { 202, 203 }, // comp_stairs
   { 203, 203 }, // comp_infcheat
   { 201, 203 }, // comp_zerotags
   { 329, 329 }, // comp_terrain
   { 329, 329 }, // comp_respawnfix
   { 329, 329 }, // comp_fallingdmg
   { 329, 329 }, // comp_soul
   { 329, 329 }, // comp_theights
   { 329, 329 }, // comp_overunder
   { 329, 329 }, // comp_planeshoot
   { 335, 335 }, // comp_special
   { 337, 337 }, // comp_ninja
   { 0,   0   }
};

//
// G_SetCompatibility
//
// haleyjd 04/10/10
//
static void G_SetCompatibility(void)
{
   int i = 0;

   while(complevels[i].fix > 0)
   {
      if(demo_version < complevels[i].opt)
         comp[i] = (demo_version < complevels[i].fix);

      ++i;
   }
}

//
// NETCODE_FIXME -- DEMO_FIXME
//
// More demo-related stuff here, for playing back demos. Will need more
// version detection to detect the new non-homogeneous demo format.
// Use of G_ReadOptions also impacts the configuration, netcode, 
// console, etc. G_ReadOptions and G_WriteOptions are, as indicated in
// one of Lee's comments, designed to be able to transmit initial
// variable values during netgame arbitration. I don't know if this
// avenue should be pursued but it might be a good idea. The current
// system being used to send them at startup is garbage.
//

static void G_DoPlayDemo(void)
{
   skill_t skill;
   int i, episode, map;
   int demover;
   char basename[9];
   byte *option_p = NULL;      // killough 11/98
   int lumpnum;
  
   if(gameaction != ga_loadgame)      // killough 12/98: support -loadgame
      basetic = gametic;  // killough 9/29/98
      
   M_ExtractFileBase(defdemoname, basename);         // killough
   
   // haleyjd 11/09/09: check ns_demos namespace first, then ns_global
   if((lumpnum = W_CheckNumForNameNSG(basename, ns_demos)) < 0)
   {
      if(singledemo)
         I_Error("G_DoPlayDemo: no such demo %s\n", basename);
      else
      {
         C_Printf(FC_ERROR "G_DoPlayDemo: no such demo %s\n", basename);
         gameaction = ga_nothing;
         D_AdvanceDemo();
      }
      return;
   }

   demobuffer = demo_p = W_CacheLumpNum(lumpnum, PU_STATIC); // killough
   
   // killough 2/22/98, 2/28/98: autodetect old demos and act accordingly.
   // Old demos turn on demo_compatibility => compatibility; new demos load
   // compatibility flag, and other flags as well, as a part of the demo.
   
   // haleyjd: this is the version for DOOM/BOOM/MBF demos; new demos 
   // test the signature and then get the new version number after it
   // if the signature matches the eedemosig array declared above.

   // killough 7/19/98: use the version id stored in demo
   demo_version = demover = *demo_p++;

   // Supported demo formats:
   // * 0.99 - 1.2:  first byte is skill level, 0-5; support is minimal.
   // * 1.4  - 1.10: 13-byte header w/version number; 1.9 supported fully.
   // * 1.11:        DOOM 1.91 longtics demo; supported fully.
   // * 2.00 - 2.02: BOOM demos; support poor for maps using BOOM features.
   // * 2.03:        MBF demos; support should be near perfect.
   // * 2.55:        These are EE demos. True version number is stored later.

   // Note that SMMU demos cannot be supported due to the fact that they
   // contain incorrect version numbers. Demo recording was also broken in
   // several versions of the port anyway.

   if(!((demover >=   0 && demover <= 4  ) || // Doom 1.2 or less
        (demover >= 104 && demover <= 111) || // Doom 1.4 - 1.9, 1.10, 1.11
        (demover >= 200 && demover <= 203) || // BOOM, MBF
        (demover == 255)))                    // Eternity
   {
      if(singledemo)
         I_Error("G_DoPlayDemo: unsupported demo format\n");
      else
      {
         C_Printf(FC_ERROR "Unsupported demo format\n");
         gameaction = ga_nothing;
         Z_ChangeTag(demobuffer, PU_CACHE);
         D_AdvanceDemo();
      }
      return;
   }
   
   if(demover < 200)     // Autodetect old demos
   {
      // haleyjd 10/08/06: longtics support
      longtics_demo = (demover >= DOOM_191_VERSION);

      demo_subversion = 0; // haleyjd: always 0 for old demos

      G_SetCompatibility();

      // haleyjd 03/17/09: in old Heretic demos, some should be false
      if(GameModeInfo->type == Game_Heretic)
      {
         comp[comp_terrain]   = 0; // terrain on
         comp[comp_overunder] = 0; // 3D clipping on
      }

      // killough 3/2/98: force these variables to be 0 in demo_compatibility

      variable_friction = 0;

      weapon_recoil = 0;

      allow_pushers = 0;

      monster_infighting = 1;           // killough 7/19/98

      bfgtype = bfg_normal;                  // killough 7/19/98

      dogs = 0;                         // killough 7/19/98
      dog_jumping = 0;                  // killough 10/98

      monster_backing = 0;              // killough 9/8/98
      
      monster_avoid_hazards = 0;        // killough 9/9/98

      monster_friction = 0;             // killough 10/98
      help_friends = 0;                 // killough 9/9/98
      monkeys = 0;

      // haleyjd 05/23/04: autoaim is sync-critical
      default_autoaim = autoaim;
      autoaim = 1;

      default_allowmlook = allowmlook;
      allowmlook = 0;

      // killough 3/6/98: rearrange to fix savegame bugs (moved fastparm,
      // respawnparm, nomonsters flags to G_LoadOptions()/G_SaveOptions())

      if((skill = demover) >= 100)         // For demos from versions >= 1.4
      {
         skill = *demo_p++;
         episode = *demo_p++;
         map = *demo_p++;
         deathmatch = *demo_p++;
         respawnparm = *demo_p++;
         fastparm = *demo_p++;
         nomonsters = *demo_p++;
         consoleplayer = *demo_p++;
      }
      else
      {
         episode = *demo_p++;
         map = *demo_p++;
         deathmatch = respawnparm = fastparm =
            nomonsters = consoleplayer = 0;
      }
   }
   else    // new versions of demos
   {
      // haleyjd: don't ignore the signature any more -- use it
      // demo_p += 6;               // skip signature;
      if(demo_version == 255 && !strncmp((const char *)demo_p, eedemosig, 5))
      {
         int temp;
         
         demo_p += 6; // increment past signature
         
         // reconstruct full version number and reset it
         temp  =        *demo_p++;         // byte one
         temp |= ((int)(*demo_p++)) <<  8; // byte two
         temp |= ((int)(*demo_p++)) << 16; // byte three
         temp |= ((int)(*demo_p++)) << 24; // byte four
         demo_version = demover = temp;
         
         // get subversion
         demo_subversion = *demo_p++;
      }
      else
      {
         demo_p += 6; // increment past signature
         
         // subversion is always 0 for demo versions < 329
         demo_subversion = 0;
      }
      
      compatibility = *demo_p++;       // load old compatibility flag
      skill = *demo_p++;
      episode = *demo_p++;
      map = *demo_p++;
      deathmatch = *demo_p++;
      consoleplayer = *demo_p++;

      // haleyjd 10/08/06: determine longtics support in new demos
      longtics_demo = (demo_version > 333 || 
                       (demo_version == 333 && demo_subversion >= 50));

      // haleyjd 04/14/03: retrieve dmflags if appropriate version
      if(demo_version >= 331)
      {
         dmflags  = *demo_p++;
         dmflags |= ((unsigned int)(*demo_p++)) << 8;
         dmflags |= ((unsigned int)(*demo_p++)) << 16;
         dmflags |= ((unsigned int)(*demo_p++)) << 24;
      }

      // haleyjd 12/14/01: retrieve gamemapname if in appropriate
      // version
      if(demo_version > 329 || 
         (demo_version == 329 && demo_subversion >= 5))
      {
         int i;
         
         for(i = 0; i < 8; i++)
            gamemapname[i] = *demo_p++;
         
         gamemapname[8] = '\0';
      }

      // killough 11/98: save option pointer for below
      if (demover >= 203)
         option_p = demo_p;

      demo_p = G_ReadOptions(demo_p);  // killough 3/1/98: Read game options
      
      if(demover == 200)        // killough 6/3/98: partially fix v2.00 demos
         demo_p += 256-GAME_OPTION_SIZE;
   }

   if(demo_compatibility)  // only 4 players can exist in old demos
   {
      for(i=0; i<4; i++)  // intentionally hard-coded 4 -- killough
         playeringame[i] = *demo_p++;
      for(;i < MAXPLAYERS; i++)
         playeringame[i] = 0;
   }
   else
   {
      for(i = 0; i < MAXPLAYERS; ++i)
         playeringame[i] = *demo_p++;
      demo_p += MIN_MAXPLAYERS - MAXPLAYERS;
   }

   if(playeringame[1])
      netgame = netdemo = true;

   // haleyjd 04/10/03: determine game type
   if(demo_version < 331)
   {
      // note: do NOT set default_dmflags here
      if(deathmatch)
      {
         GameType = gt_dm;
         G_SetDefaultDMFlags(deathmatch, false);
      }
      else
      {
         GameType = (netgame ? gt_coop : gt_single);
         G_SetDefaultDMFlags(0, false);
      }
   }
   else
   {
      // dmflags was already set above,
      // "deathmatch" now holds the game type
      GameType = deathmatch;
   }
   
   // don't spend a lot of time in loadlevel

   if(gameaction != ga_loadgame)      // killough 12/98: support -loadgame
   {
      // killough 2/22/98:
      // Do it anyway for timing demos, to reduce timing noise
      precache = timingdemo;
      
      // haleyjd: choose appropriate G_InitNew based on version
      if(demo_version > 329 ||
         (demo_version == 329 && demo_subversion >= 5))
      {
         G_InitNew(skill, gamemapname);
      }
      else
         G_InitNewNum(skill, episode, map);

      // killough 11/98: If OPTIONS were loaded from the wad in G_InitNew(),
      // reload any demo sync-critical ones from the demo itself, to be exactly
      // the same as during recording.
      
      if(option_p)
         G_ReadOptions(option_p);
   }

   precache = true;
   usergame = false;
   demoplayback = true;
   
   for(i=0; i<MAXPLAYERS;i++)         // killough 4/24/98
      players[i].cheats = 0;
   
   gameaction = ga_nothing;

   G_DemoStartMessage(basename);
   
   if(timingdemo)
   {
      static int first = 1;
      if(first)
      {
         starttime = I_GetTime_RealTime();
         startgametic = gametic;
         first = 0;
      }
   }
}

#define DEMOMARKER    0x80

//
// NETCODE_FIXME -- DEMO_FIXME
//
// In order to allow console commands and the such in demos, the demo
// reading system will require major modifications. In order to support
// old demos too, it will probably be necessary to split this function
// off into one that handles the old format and one that handles the
// new. Once demos contain things other than just ticcmds, they get
// a lot more complicated. The new demo format will also have to deal
// with ticcmd packing. It will make demos smaller, but will require
// use of the same functions needed by the netcode to pack/unpack
// ticcmds.
//

static void G_ReadDemoTiccmd(ticcmd_t *cmd)
{
   if(*demo_p == DEMOMARKER)
   {
      G_CheckDemoStatus();      // end of demo data stream
   }
   else
   {
      cmd->forwardmove = ((signed char)*demo_p++);
      cmd->sidemove    = ((signed char)*demo_p++);
      
      if(longtics_demo) // haleyjd 10/08/06: longtics support
      {
         cmd->angleturn  =  *demo_p++;
         cmd->angleturn |= (*demo_p++) << 8;
      }
      else
         cmd->angleturn = ((unsigned char)*demo_p++)<<8;
      
      cmd->buttons = (unsigned char)*demo_p++;

      // old Heretic demo?
      if(demo_version <= 4 && GameModeInfo->type == Game_Heretic)
      {
         demo_p++;
         demo_p++;
      }
      
      if(demo_version >= 335)
         cmd->actions = *demo_p++;
      else
         cmd->actions = 0;

      if(demo_version >= 333)
      {
         cmd->look  =  *demo_p++;
         cmd->look |= (*demo_p++) << 8;
      }
      else if(demo_version >= 329)
      {
         // haleyjd: 329 and 331 store updownangle, but we can't use
         // it any longer. Demos recorded with mlook will desync, 
         // but ones without can still play with this here.
         ++demo_p;
         cmd->look = 0;
      }
      else
         cmd->look = 0;
      
      // killough 3/26/98, 10/98: Ignore savegames in demos 
      if(demoplayback && 
         cmd->buttons & BT_SPECIAL && cmd->buttons & BTS_SAVEGAME)
      {
         cmd->buttons &= ~BTS_SAVEGAME;
         doom_printf("Game Saved (Suppressed)");
      }
   }
}

//
// G_WriteDemoTiccmd
//
// Demo limits removed -- killough
//
// NETCODE_FIXME: This will have to change as well, but maintaining
// compatibility is not necessary for demo writing, making this function
// much simpler to handle. Like reading, writing of other types of
// commands and ticcmd packing must be handled here for the new demo
// format. The way the demo buffer grows is also sensitive and will
// have to be changed, otherwise the buffer may be overflowed before
// it checks for another reallocation. zdoom changes this, so I know
// it is an issue.
//
static void G_WriteDemoTiccmd(ticcmd_t *cmd)
{
   unsigned int position = demo_p - demobuffer;
   int i = 0;
   
   demo_p[i++] = cmd->forwardmove;
   demo_p[i++] = cmd->sidemove;

   // haleyjd 10/08/06: longtics support from Choco Doom.
   // If this is a longtics demo, record in higher resolution
   if(longtics_demo)
   {
      demo_p[i++] =  cmd->angleturn & 0xff;
      demo_p[i++] = (cmd->angleturn >> 8) & 0xff;
   }
   else
      demo_p[i++] = (cmd->angleturn + 128) >> 8; 

   demo_p[i++] =  cmd->buttons;

   if(demo_version >= 335)
      demo_p[i++] =  cmd->actions;         //  -- joek 12/22/07

   if(demo_version >= 333)
   {
      demo_p[i++] =  cmd->look & 0xff;
      demo_p[i]   = (cmd->look >> 8) & 0xff;
   }
   
   if(position + 16 > maxdemosize)   // killough 8/23/98
   {
      // no more space
      maxdemosize += 128*1024;   // add another 128K  -- killough
      demobuffer = realloc(demobuffer, maxdemosize);
      demo_p = position + demobuffer;  // back on track
      // end of main demo limit changes -- killough
   }
   
   G_ReadDemoTiccmd(cmd); // make SURE it is exactly the same
}

static boolean secretexit;

// haleyjd: true if a script called exitsecret()
boolean scriptSecret = false; 

void G_ExitLevel(void)
{
   secretexit = scriptSecret = false;
   gameaction = ga_completed;
}

//
// G_SecretExitLevel
//
// Here's for the german edition.
// IF NO WOLF3D LEVELS, NO SECRET EXIT!
// (unless it's a script secret exit)
//
void G_SecretExitLevel(void)
{
   secretexit = 
      !(GameModeInfo->flags & GIF_WOLFHACK) || haswolflevels || scriptSecret;
   gameaction = ga_completed;
}

//
// G_PlayerFinishLevel
//
// Called when a player completes a level.
//
static void G_PlayerFinishLevel(int player)
{
   player_t *p = &players[player];
   memset(p->powers, 0, sizeof p->powers);
   memset(p->cards, 0, sizeof p->cards);
   p->mo->flags &= ~MF_SHADOW;     // cancel invisibility
   p->mo->flags2 &= ~MF2_DONTDRAW; // haleyjd: cancel total invis.
   p->mo->flags3 &= ~MF3_GHOST;    // haleyjd: cancel ghost
   p->extralight = 0;      // cancel gun flashes
   p->fixedcolormap = 0;   // cancel ir gogles
   p->damagecount = 0;     // no palette changes
   p->bonuscount = 0;
}

//
// G_SetNextMap
//
// haleyjd 07/03/09: Replaces gamemode-dependent exit determination
// functions with interpretation of a rule set held in GameModeInfo.
//
static void G_SetNextMap(void)
{
   exitrule_t *exitrule = GameModeInfo->exitRules;
   exitrule_t *theRule = NULL;

   // find a rule
   for(; exitrule->gameepisode != -2; ++exitrule)
   {
      if((exitrule->gameepisode == -1 || exitrule->gameepisode == gameepisode) &&
         (exitrule->gamemap == -1 || exitrule->gamemap == gamemap) &&
         exitrule->isSecret == secretexit)
      {
         theRule = exitrule;
         break;
      }
   }

   if(theRule)
      wminfo.next = theRule->destmap;
   else if(!secretexit)
      wminfo.next = gamemap;
}

//
// G_DoCompleted
//
// Called upon level completion. Figures out what map is next and
// starts the intermission.
//
static void G_DoCompleted(void)
{
   int i;
   
   gameaction = ga_nothing;
   
   for(i = 0; i < MAXPLAYERS; ++i)
   {
      if(playeringame[i])
         G_PlayerFinishLevel(i);        // take away cards and stuff
   }

   // clear hubs now
   P_ClearHubs();
   
   if(automapactive)
      AM_Stop();

   if(!(GameModeInfo->flags & GIF_MAPXY)) // kilough 2/7/98
   {
      switch(gamemap)
      {
      case 8:
         if(!LevelInfo.killFinale) // haleyjd 05/26/10: long-forgotten
            gameaction = ga_victory;
         return;
      case 9:
         for(i = 0; i < MAXPLAYERS; ++i)
            players[i].didsecret = true;
         break;
      }
   }

   wminfo.gotosecret = secretexit; // haleyjd
   wminfo.didsecret = players[consoleplayer].didsecret;
   wminfo.epsd = gameepisode - 1;
   wminfo.last = gamemap - 1;

   // set the next gamemap
   G_SetNextMap();

   // haleyjd: override with MapInfo values
   if(!secretexit)
   {
      if(*LevelInfo.nextLevel) // only for normal exit
      {
         wminfo.next = G_GetMapForName(LevelInfo.nextLevel);
         if(!(GameModeInfo->flags & GIF_MAPXY))
            wminfo.next = wminfo.next % 10;
         wminfo.next--;
      }
   }
   else
   {
      if(*LevelInfo.nextSecret) // only for secret exit
      {
         wminfo.next = G_GetMapForName(LevelInfo.nextSecret);
         if(!(GameModeInfo->flags & GIF_MAPXY))
            wminfo.next = wminfo.next % 10;
         wminfo.next--;
      }
   }

   wminfo.maxkills  = totalkills;
   wminfo.maxitems  = totalitems;
   wminfo.maxsecret = totalsecret;
   wminfo.maxfrags  = 0;

   wminfo.partime = LevelInfo.partime; // haleyjd 07/22/04

   wminfo.pnum = consoleplayer;

   for(i = 0; i < MAXPLAYERS; ++i)
   {
      wminfo.plyr[i].in      = playeringame[i];
      wminfo.plyr[i].skills  = players[i].killcount;
      wminfo.plyr[i].sitems  = players[i].itemcount;
      wminfo.plyr[i].ssecret = players[i].secretcount;
      wminfo.plyr[i].stime   = leveltime;
      memcpy(wminfo.plyr[i].frags, players[i].frags,
             sizeof(wminfo.plyr[i].frags));
   }
  
   gamestate = GS_INTERMISSION;
   automapactive = false;
   
   if(statcopy)
      memcpy(statcopy, &wminfo, sizeof(wminfo));
   
   IN_Start(&wminfo);
}

static void G_DoWorldDone(void)
{
   missioninfo_t *missionInfo = GameModeInfo->missionInfo;

   idmusnum = -1; //jff 3/17/98 allow new level's music to be loaded
   gamestate = GS_LEVEL;
   gamemap = wminfo.next+1;

   // haleyjd: handle heretic hidden levels
   if((missionInfo->id == hticsosr && gameepisode == 6 && gamemap == 4) ||
      (missionInfo->id == heretic  && gameepisode == 4 && gamemap == 2))
   {
      gamemap--; // return to same level
   }
   
   // haleyjd: customizable secret exits
   if(secretexit)
   {
      if(*LevelInfo.nextSecret)
         G_SetGameMapName(LevelInfo.nextSecret);
      else
         G_SetGameMapName(G_GetNameForMap(gameepisode, gamemap));
   }
   else
   {
      // haleyjd 12/14/01: don't use nextlevel for secret exits here either!
      if(*LevelInfo.nextLevel)
         G_SetGameMapName(LevelInfo.nextLevel);
      else
         G_SetGameMapName(G_GetNameForMap(gameepisode, gamemap));
   }
   
   hub_changelevel = false;
   G_DoLoadLevel();
   gameaction = ga_nothing;
   AM_clearMarks(); //jff 4/12/98 clear any marks on the automap
   // haleyjd 01/07/07: run deferred ACS scripts
   ACS_RunDeferredScripts();
}

//
// G_ForceFinale
//
// haleyjd 07/01/09: Used by the Hexen Teleport_EndGame special.
// If the map does not have a finale sequence defined, it will be given
// a default sequence.
//
void G_ForceFinale(void)
{
   // in DOOM 2, we want a cast call
   if(GameModeInfo->flags & GIF_SETENDOFGAME)
      LevelInfo.endOfGame = true;   
   
   if(LevelInfo.finaleType == FINALE_TEXT) // modify finale type?
      LevelInfo.finaleType = GameModeInfo->teleEndGameFinaleType;

   // no text defined? make up something.
   if(!LevelInfo.interText)
      LevelInfo.interText = "You have won.";

   // set other variables for consistency
   LevelInfo.killFinale = false;
   LevelInfo.finaleSecretOnly = false;

   gameaction = ga_victory;
}

#define VERSIONSIZE   16

// killough 2/22/98: version id string format for savegames
#define VERSIONID "MBF %d"

static char *savename;

//
// killough 5/15/98: add forced loadgames, which allow user to override checks
//

static boolean forced_loadgame = false;
static boolean command_loadgame = false;

void G_ForcedLoadGame(void)
{
   gameaction = ga_loadgame;
   forced_loadgame = true;
}

// killough 3/16/98: add slot info
// killough 5/15/98: add command-line

void G_LoadGame(char *name, int slot, boolean command)
{
   if(savename)
      free(savename);
   savename = strdup(name);
   savegameslot = slot;
   gameaction = ga_loadgame;
   forced_loadgame = false;
   command_loadgame = command;
   hub_changelevel = false;
}

// killough 5/15/98:
// Consistency Error when attempting to load savegame.

static void G_LoadGameErr(char *msg)
{
   Z_Free(savebuffer);           // Free the savegame buffer
   MN_ForcedLoadGame(msg);       // Print message asking for 'Y' to force
   if(command_loadgame)          // If this was a command-line -loadgame
   {
      D_StartTitle();            // Start the title screen
      gamestate = GS_DEMOSCREEN; // And set the game state accordingly
   }
}

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//

void G_SaveGame(int slot, char *description)
{
   savegameslot = slot;
   strcpy(savedescription, description);
   sendsave = true;
   hub_changelevel = false;
}

//
// CheckSaveGame
//
// Lee Killough 1/22/98
// Check for overrun and realloc if necessary
//
void CheckSaveGame(size_t size)
{
   size_t pos = save_p - savebuffer;
   size += 1024;  // breathing room
   
   if(pos + size > savegamesize)
   {
      // haleyjd: deobfuscated
      savegamesize += (size + 1023) & ~1023;
      savebuffer = realloc(savebuffer, savegamesize);
      save_p = savebuffer + pos;
   }
}

// killough 3/22/98: form savegame name in one location
// (previously code was scattered around in multiple places)

void G_SaveGameName(char *name, size_t len, int slot)
{
   // Ty 05/04/98 - use savegamename variable (see d_deh.c)
   // killough 12/98: add .7 to truncate savegamename

#ifdef EE_CDROM_SUPPORT
   if(cdrom_mode)
      psnprintf(name, len, "c:/doomdata/%.7s%d.dsg", 
                savegamename, slot);
   else
#endif
      psnprintf(name, len, "%s/%.7s%d.dsg", 
                basesavegame, savegamename, slot);
}

// killough 12/98:
// This function returns a signature for the current wad.
// It is used to distinguish between wads, for the purposes
// of savegame compatibility warnings, and options lookups.
//
// killough 12/98: use faster algorithm which has less IO

uint64_t G_Signature(void)
{
   uint64_t s = 0;
   int lump, i;
   
   // sf: use gamemapname now, not gameepisode and gamemap
   lump = W_CheckNumForName(gamemapname);
   
   if(lump != -1 && (i = lump + 10) < w_GlobalDir.numlumps)
   {
      do
      {
         s = s * 2 + W_LumpLength(i);
      }
      while(--i > lump);
   }
   
   return s;
}

extern wfileadd_t *wadfiles;       // killough 11/98

// sf: split into two functions

void G_SaveCurrentLevel(char *filename, char *description)
{
   int  length, i;
   char name2[VERSIONSIZE];
   
   save_p = savebuffer = malloc(savegamesize);

   // haleyjd: somebody got really lax with checking the savegame
   // buffer size on these first few writes -- granted that the
   // buffer starts out large enough by default to handle them,
   // but it should be done properly for safety.
   
   CheckSaveGame(SAVESTRINGSIZE+VERSIONSIZE+2); // haleyjd: was wrong
   memcpy (save_p, description, SAVESTRINGSIZE);
   save_p += SAVESTRINGSIZE;
   memset (name2, 0, sizeof(name2));
   
   // killough 2/22/98: "proprietary" version string :-)
   sprintf (name2, VERSIONID, version);
   
   memcpy (save_p, name2, VERSIONSIZE);
   save_p += VERSIONSIZE;
   
   // killough 2/14/98: save old compatibility flag:
   *save_p++ = compatibility;

   *save_p++ = gameskill;
   
   // sf: use string rather than episode, map
   {
      int i;
      CheckSaveGame(8); // haleyjd: added 
      for(i=0; i<8; i++)
         *save_p++ = levelmapname[i];
   }
  
   {  // killough 3/16/98, 12/98: store lump name checksum
      uint64_t checksum = G_Signature();

      CheckSaveGame(sizeof checksum); // haleyjd: added
      memcpy(save_p, &checksum, sizeof checksum);
      save_p += sizeof checksum;
   }

   // killough 3/16/98: store pwad filenames in savegame
   {
      wfileadd_t *file = wadfiles;

      for(*save_p = 0; file->filename; ++file)
      {
         const char *fn = file->filename;
         CheckSaveGame(strlen(fn) + 2);
         strcat(strcat((char *)save_p, fn), "\n");
      }
      save_p += strlen((char *)save_p) + 1;
   }

   CheckSaveGame(GAME_OPTION_SIZE+MIN_MAXPLAYERS+11);
   
   for(i=0 ; i<MAXPLAYERS ; i++)
      *save_p++ = playeringame[i];
   
   for(;i<MIN_MAXPLAYERS;i++)         // killough 2/28/98
      *save_p++ = 0;

   // haleyjd: uses 1 (byte 1)
   *save_p++ = idmusnum;               // jff 3/17/98 save idmus state

   // haleyjd 04/14/03: save game type (uses byte 2)
   *save_p++ = GameType;
   
   save_p = G_WriteOptions(save_p);    // killough 3/1/98: save game options
   
   // haleyjd: uses 4 (bytes 3-6)
   memcpy(save_p, &leveltime, sizeof(leveltime)); //killough 11/98: save entire word
   save_p += sizeof(leveltime);
   
   // haleyjd: uses 1 (byte 7)
   // killough 11/98: save revenant tracer state
   *save_p++ = (gametic-basetic) & 255;

   // haleyjd: bytes 8-11
   memcpy(save_p, &dmflags, sizeof(dmflags));
   save_p += sizeof(dmflags);
   
   // killough 3/22/98: add Z_CheckHeap after each call to ensure consistency
   // haleyjd 07/06/09: just Z_CheckHeap after the end. This stuff works by now.
   
   P_NumberObjects();    // turn ptrs to numbers

   P_ArchivePlayers();
   P_ArchiveWorld();
   P_ArchivePolyObjects(); // haleyjd 03/27/06
   P_ArchiveThinkers();
   P_ArchiveSpecials();
   P_ArchiveRNG();    // killough 1/18/98: save RNG information
   P_ArchiveMap();    // killough 1/22/98: save automap information
   P_ArchiveScripts();   // sf: archive scripts
   P_ArchiveSoundSequences();
   P_ArchiveButtons();
   
   P_DeNumberObjects();

   CheckSaveGame(1); // haleyjd

   *save_p++ = 0xe6;   // consistancy marker
   
   length = save_p - savebuffer;
   
   Z_CheckHeap();

   if(!M_WriteFile(filename, savebuffer, length))
   {
      const char *str = 
         errno ? strerror(errno) : FC_ERROR "Could not save game: Error unknown";
      doom_printf("%s", str);
   }
   else if(!hub_changelevel) // sf: no 'game saved' message for hubs
      doom_printf("%s", DEH_String("GGSAVED"));  // Ty 03/27/98 - externalized

   free(savebuffer);  // killough
   savebuffer = save_p = NULL;
}

static void G_DoSaveGame(void)
{
   char *name = NULL;
   size_t len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);
   
   G_SaveGameName(name, len, savegameslot);
   
   G_SaveCurrentLevel(name, savedescription);
   
   gameaction = ga_nothing;
   savedescription[0] = 0;
}

static void G_DoLoadGame(void)
{
   int i;
   char vcheck[VERSIONSIZE];
   uint64_t checksum;

   gameaction = ga_nothing;
   
   // haleyjd 10/24/06: check for failure
   if(M_ReadFile(savename, &savebuffer) == -1)
   {
      C_Printf(FC_ERROR "Failed to load savegame %s\n", savename);
      C_SetConsole();
      return;
   }

   save_p = savebuffer + SAVESTRINGSIZE;
   
   // skip the description field
   
   // killough 2/22/98: "proprietary" version string :-)
   sprintf(vcheck, VERSIONID, version);
   
   // killough 2/22/98: Friendly savegame version difference message
   if(!forced_loadgame && strncmp((const char *)save_p, vcheck, VERSIONSIZE))
   {
      G_LoadGameErr("Different Savegame Version!!!\n\nAre you sure?");
      return;
   }

   save_p += VERSIONSIZE;
   
   // killough 2/14/98: load compatibility mode
   compatibility = *save_p++;
   demo_version = version;     // killough 7/19/98: use this version's id
   demo_subversion = SUBVERSION; // haleyjd 06/17/01   
   
   gameskill = *save_p++;
   
   // sf: use string rather than episode, map

   {
      int i;
      
      for(i = 0; i < 8; i++)
         gamemapname[i] = *save_p++;
      gamemapname[8] = '\0';        // ending NULL
   }

   G_SetGameMap();       // get gameepisode, map
   
   if(!forced_loadgame)
   {  // killough 3/16/98, 12/98: check lump name checksum
      checksum = G_Signature();
      if (memcmp(&checksum, save_p, sizeof checksum))
      {
         char *msg = calloc(1, strlen((const char *)(save_p + sizeof checksum)) + 128);
         strcpy(msg,"Incompatible Savegame!!!\n");
         if(save_p[sizeof checksum])
            strcat(strcat(msg,"Wads expected:\n\n"), (char *)(save_p + sizeof checksum));
         strcat(msg, "\nAre you sure?");
         C_Puts(msg);
         G_LoadGameErr(msg);
         free(msg);
         return;
      }
   }

   save_p += sizeof checksum;
   while(*save_p++);

   for(i = 0; i < MAXPLAYERS; ++i)
      playeringame[i] = *save_p++;
   save_p += MIN_MAXPLAYERS-MAXPLAYERS;         // killough 2/28/98

   // jff 3/17/98 restore idmus music
   // jff 3/18/98 account for unsigned byte
   // killough 11/98: simplify
   idmusnum = *(signed char *) save_p++;

   // haleyjd 04/14/03: game type
   // note: don't set DefaultGameType from save games
   GameType = *save_p++;

   /* cph 2001/05/23 - Must read options before we set up the level */
   G_ReadOptions(save_p);
 
   // load a base level
   // sf: in hubs, use g_doloadlevel instead of g_initnew
   if(hub_changelevel)
      G_DoLoadLevel();
   else
      G_InitNew(gameskill, gamemapname);

   // killough 3/1/98: Read game options
   // killough 11/98: move down to here
   
   // cph - MBF needs to reread the savegame options because 
   // G_InitNew rereads the WAD options. The demo playback code does 
   // this too.
   save_p = G_ReadOptions(save_p);

   // get the times
   // killough 11/98: save entire word
   // haleyjd  08/01/09: try sizeof variable, not sizeof pointer!
   memcpy(&leveltime, save_p, sizeof(leveltime));
   save_p += sizeof(leveltime);

   // killough 11/98: load revenant tracer state
   basetic = gametic - (int) *save_p++;

   // haleyjd 04/14/03: load dmflags
   memcpy(&dmflags, save_p, sizeof(dmflags));
   save_p += sizeof(dmflags);

   // haleyjd 07/06/09: prepare ACS for loading
   ACS_PrepareForLoad();

   // dearchive all the modifications
   P_UnArchivePlayers();
   P_UnArchiveWorld();
   P_UnArchivePolyObjects();    // haleyjd 03/27/06
   P_UnArchiveThinkers();
   P_UnArchiveSpecials();
   P_UnArchiveRNG();            // killough 1/18/98: load RNG information
   P_UnArchiveMap();            // killough 1/22/98: load automap information
   P_UnArchiveScripts();        // sf: scripting
   P_UnArchiveSoundSequences();
   P_UnArchiveButtons();
   P_FreeObjTable();

   if(*save_p != 0xe6)
   {
      C_SetConsole();
      C_Printf(FC_ERROR "bad savegame: offset 0x%x is 0x%x\n",
         save_p-savebuffer, *save_p);
      Z_Free(savebuffer);
      return; 
   }

   // haleyjd: move up Z_CheckHeap to before Z_Free (safer)
   Z_CheckHeap(); 

   // done
   Z_Free(savebuffer);
   
   if (setsizeneeded)
      R_ExecuteSetViewSize();
   
   // draw the pattern into the back screen
   R_FillBackScreen();

   // haleyjd 02/09/10: wake up status bar again
   ST_Start();

   // killough 12/98: support -recordfrom and -loadgame -playdemo
   if(!command_loadgame)
      singledemo = false;         // Clear singledemo flag if loading from menu
   else if(singledemo)
   {
      gameaction = ga_loadgame; // Mark that we're loading a game before demo
      G_DoPlayDemo();           // This will detect it and won't reinit level
   }
   else       // Loading games from menu isn't allowed during demo recordings,
      if(demorecording) // So this can only possibly be a -recordfrom command.
         G_BeginRecording();// Start the -recordfrom, since the game was loaded.

   // sf: if loading a hub level, restore position relative to sector
   //  for 'seamless' travel between levels
   if(hub_changelevel) 
      P_RestorePlayerPosition();

   // haleyjd 01/07/07: run deferred ACS scripts
   ACS_RunDeferredScripts();
}

//
// G_CameraTicker
//
// haleyjd 01/10/2010: SMMU was calling camera tickers too early, so I turned
// it into a separate function to call from below.
//
static void G_CameraTicker(void)
{
   // run special cameras
   if((walkcam_active = (camera == &walkcamera)))
      P_WalkTicker();
   else if((chasecam_active = (camera == &chasecam)))
      P_ChaseTicker();
   else if(camera == &followcam)
      P_FollowCamTicker();

   // cooldemo countdown   
   if(demoplayback && cooldemo)
   {
      // force refresh on death of displayed player
      if(players[displayplayer].health <= 0)
         cooldemo_tics = 0;

      if(cooldemo_tics)
         cooldemo_tics--;
      else
         G_CoolViewPoint();
   }
}

//
// G_Ticker
//
// Make ticcmd_ts for the players.
//
void G_Ticker(void)
{
   int i;

   // do player reborns if needed
   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i] && players[i].playerstate == PST_REBORN)
         G_DoReborn(i);
   }

   // do things to change the game state
   while (gameaction != ga_nothing)
   {
      switch (gameaction)
      {
      case ga_loadlevel:
         G_DoLoadLevel();
         break;
      case ga_newgame:
         G_DoNewGame();
         break;
      case ga_loadgame:
         G_DoLoadGame();
         break;
      case ga_savegame:
         G_DoSaveGame();
         break;
      case ga_playdemo:
         G_DoPlayDemo();
         break;
      case ga_completed:
         G_DoCompleted();
         break;
      case ga_victory:
         F_StartFinale();
         break;
      case ga_worlddone:
         G_DoWorldDone();
         break;
      case ga_screenshot:
         M_ScreenShot();
         gameaction = ga_nothing;
         break;
      default:  // killough 9/29/98
         gameaction = ga_nothing;
         break;
      }
   }

   if(animscreenshot)    // animated screen shots
   {
      if(gametic % 16 == 0)
      {
         animscreenshot--;
         M_ScreenShot();
      }
   }

   // killough 10/6/98: allow games to be saved during demo
   // playback, by the playback user (not by demo itself)
   
   if (demoplayback && sendsave)
   {
      sendsave = false;
      G_DoSaveGame();
   }

   // killough 9/29/98: Skip some commands while pausing during demo
   // playback, or while menu is active.
   //
   // We increment basetic and skip processing if a demo being played
   // back is paused or if the menu is active while a non-net game is
   // being played, to maintain sync while allowing pauses.
   //
   // P_Ticker() does not stop netgames if a menu is activated, so
   // we do not need to stop if a menu is pulled up during netgames.

   if(paused & 2 || (!demoplayback && (menuactive || consoleactive) && !netgame))
   {
      basetic++;  // For revenant tracers and RNG -- we must maintain sync
   }
   else
   {
      // get commands, check consistancy, and build new consistancy check
      int buf = (gametic / ticdup) % BACKUPTICS;
      
      for(i=0; i<MAXPLAYERS; i++)
      {
         if(playeringame[i])
         {
            ticcmd_t *cmd = &players[i].cmd;
            playerclass_t *pc = players[i].pclass;
            
            memcpy(cmd, &netcmds[i][buf], sizeof *cmd);
            
            if(demoplayback)
               G_ReadDemoTiccmd(cmd);
            
            if(demorecording)
               G_WriteDemoTiccmd(cmd);
            
            /*
            if(isconsoletic && netgame)
               continue;
            */
            
            // check for turbo cheats
            // killough 2/14/98, 2/20/98 -- only warn in netgames and demos
            
            if((netgame || demoplayback) && 
               cmd->forwardmove > TURBOTHRESHOLD &&
               !(gametic&31) && ((gametic>>5)&3) == i)
            {
               doom_printf("%s is turbo!", players[i].name); // killough 9/29/98
            }
            
            if(netgame && /*!isconsoletic &&*/ !netdemo && 
               !(gametic % ticdup))
            {
               if(gametic > BACKUPTICS && 
                  consistancy[i][buf] != cmd->consistancy)
               {
                  D_QuitNetGame();
                  C_Printf(FC_ERROR "consistency failure");
                  C_Printf(FC_ERROR "(%i should be %i)",
                              cmd->consistancy, consistancy[i][buf]);
               }
               
               // sf: include y as well as x
               if(players[i].mo)
                  consistancy[i][buf] = (int16_t)(players[i].mo->x + players[i].mo->y);
               else
                  consistancy[i][buf] = 0; // killough 2/14/98
            }
         }
      }
      
      // check for special buttons
      for(i=0; i<MAXPLAYERS; i++)
      {
         if(playeringame[i] && 
            players[i].cmd.buttons & BT_SPECIAL)
         {
            // killough 9/29/98: allow multiple special buttons
            if(players[i].cmd.buttons & BTS_PAUSE)
            {
               if((paused ^= 1))
                  S_PauseSound();
               else
                  S_ResumeSound();
            }
            
            if(players[i].cmd.buttons & BTS_SAVEGAME)
            {
               if(!savedescription[0])
                  strcpy(savedescription, "NET GAME");
               savegameslot =
                  (players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT;
               gameaction = ga_savegame;
            }
         }
      }
   }

   // do main actions
   
   // killough 9/29/98: split up switch statement
   // into pauseable and unpauseable parts.
   
   // call other tickers
   C_NetTicker();        // sf: console network commands
   if(inwipe)
      Wipe_Ticker();

#ifndef EE_NO_SMALL_SUPPORT
   // haleyjd 03/15/03: execute scheduled Small callbacks
   SM_ExecuteCallbacks();
#endif
   
   if(gamestate == GS_LEVEL)
   {
      P_Ticker();
      G_CameraTicker(); // haleyjd: move cameras
      ST_Ticker(); 
      AM_Ticker(); 
      HU_Ticker();
   }
   else if(!(paused & 2)) // haleyjd: refactored
   {
      switch(gamestate)
      {
      case GS_INTERMISSION:
         IN_Ticker();
         break;
      case GS_FINALE:
         F_Ticker();
         break;
      case GS_DEMOSCREEN:
         D_PageTicker();
         break;
      default:
         break;
      }
   }
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerReborn
//
// Called after a player dies
// almost everything is cleared and initialized
//
void G_PlayerReborn(int player)
{
   player_t *p;
   int i;
   int frags[MAXPLAYERS];
   int totalfrags;
   int killcount;
   int itemcount;
   int secretcount;
   char playername[20];
   int playercolour;
   skin_t *playerskin;
   playerclass_t *playerclass;

   memcpy (frags, players[player].frags, sizeof frags);
   killcount = players[player].killcount;
   itemcount = players[player].itemcount;
   secretcount = players[player].secretcount;
   strncpy(playername, players[player].name, 20);
   playercolour = players[player].colormap;
   totalfrags = players[player].totalfrags;
   playerskin = players[player].skin;
   playerclass = players[player].pclass; // haleyjd: playerclass
   
   p = &players[player];

   // killough 3/10/98,3/21/98: preserve cheats across idclev
   {
      int cheats = p->cheats;
      memset(p, 0, sizeof(*p));
      p->cheats = cheats;
   }

   memcpy(players[player].frags, frags, sizeof(players[player].frags));
   players[player].colormap = playercolour;
   strcpy(players[player].name, playername);
   players[player].killcount = killcount;
   players[player].itemcount = itemcount;
   players[player].secretcount = secretcount;
   players[player].totalfrags = totalfrags;
   players[player].skin = playerskin;
   players[player].pclass = playerclass; // haleyjd: playerclass
   
   p->usedown = p->attackdown = true;  // don't do anything immediately
   p->playerstate = PST_LIVE;
   p->health = initial_health;  // Ty 03/12/98 - use dehacked values
   p->quake = 0;                // haleyjd 01/21/07

   // WEAPON_FIXME: default reborn weapon
   // PCLASS_FIXME: default reborn weapon
   p->readyweapon = p->pendingweapon = wp_pistol;

   // WEAPON_FIXME: revive "weaponowned" feature?
   // sf: different weapons owned
   memcpy(p->weaponowned, default_weaponowned, sizeof(p->weaponowned));
   
   // WEAPON_FIXME: always owned weapons
   // PCLASS_FIXME: always owned weapons
   p->weaponowned[wp_fist] = true;     // always fist and pistol
   p->weaponowned[wp_pistol] = true;
   
   // WEAPON_FIXME: default ammo stuff
   // PCLASS_FIXME: default ammo stuff
   p->ammo[am_clip] = initial_bullets; // Ty 03/12/98 - use dehacked values
   
   for(i = 0; i < NUMAMMO; i++)
      p->maxammo[i] = maxammo[i];
}

void P_SpawnPlayer(mapthing_t *mthing);

//
// G_CheckSpot
//
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//
static boolean G_CheckSpot(int playernum, mapthing_t *mthing, mobj_t **fog)
{
   fixed_t     x, y;
   subsector_t *ss;
   unsigned    an;
   mobj_t      *mo;
   int         i;
   fixed_t     mtcos, mtsin;

   if(!players[playernum].mo)
   {
      // first spawn of level, before corpses
      for(i = 0; i < playernum; i++)
      {
         if(players[i].mo->x == mthing->x << FRACBITS &&
            players[i].mo->y == mthing->y << FRACBITS)
            return false;
      }
      return true;
   }

   x = mthing->x << FRACBITS;
   y = mthing->y << FRACBITS;
   
   // killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
   // corpse to detect collisions with other players in DM starts
   //
   // Old code:
   // if (!P_CheckPosition (players[playernum].mo, x, y))
   //    return false;

   players[playernum].mo->flags |=  MF_SOLID;
   i = P_CheckPosition(players[playernum].mo, x, y);
   players[playernum].mo->flags &= ~MF_SOLID;
   if(!i)
      return false;

   // flush an old corpse if needed
   // killough 2/8/98: make corpse queue have an adjustable limit
   // killough 8/1/98: Fix bugs causing strange crashes

   if(bodyquesize > 0)
   {
      static mobj_t **bodyque;
      static size_t queuesize;

      if(queuesize < (unsigned int)bodyquesize)
      {
         bodyque = realloc(bodyque, bodyquesize*sizeof*bodyque);
         memset(bodyque+queuesize, 0, 
                (bodyquesize-queuesize)*sizeof*bodyque);
         queuesize = bodyquesize;
      }
      
      if(bodyqueslot >= bodyquesize) 
         P_RemoveMobj(bodyque[bodyqueslot % bodyquesize]); 
      
      bodyque[bodyqueslot++ % bodyquesize] = players[playernum].mo; 
   } 
   else if(!bodyquesize)
      P_RemoveMobj(players[playernum].mo);

   // spawn a teleport fog
   ss = R_PointInSubsector(x,y);

   // haleyjd: There was a weird bug with this statement:
   //
   // an = (ANG45 * (mthing->angle/45)) >> ANGLETOFINESHIFT;
   //
   // Even though this code stores the result into an unsigned variable, most
   // compilers seem to ignore that fact in the optimizer and use the resulting
   // value directly in a lea instruction. This causes the signed mapthing_t
   // angle value to generate an out-of-bounds access into the fine trig
   // lookups. In vanilla, this accesses the finetangent table and other parts
   // of the finesine table, and the result is what I call the "ninja spawn,"
   // which is missing the fog and sound, as it spawns somewhere out in the
   // far reaches of the void.

   if(!comp[comp_ninja])
   {
      an = ANG45 * (angle_t)(mthing->angle / 45);
      mtcos = finecosine[an >> ANGLETOFINESHIFT];
      mtsin = finesine[an >> ANGLETOFINESHIFT];
   }
   else
   {
      // emulate out-of-bounds access to finecosine / finesine tables
      angle_t mtangle = (angle_t)(mthing->angle / 45);
      
      an = ANG45 * mtangle;

      switch(mtangle)
      {
      case 4: // 180 degrees (0x80000000 >> 19 == -4096)
         mtcos = finetangent[2048];
         mtsin = finetangent[0];
         break;
      case 5: // 225 degrees (0xA0000000 >> 19 == -3072)
         mtcos = finetangent[3072];
         mtsin = finetangent[1024];
         break;
      case 6: // 270 degrees (0xC0000000 >> 19 == -2048)
         mtcos = finesine[0];
         mtsin = finetangent[2048];
         break;
      case 7: // 315 degrees (0xE0000000 >> 19 == -1024)
         mtcos = finesine[1024];
         mtsin = finetangent[3072];
         break;
      default: // everything else works properly
         mtcos = finecosine[an >> ANGLETOFINESHIFT];
         mtsin = finesine[an >> ANGLETOFINESHIFT];
         break;
      }
   }

   mo = P_SpawnMobj(x + 20 * mtcos, 
                    y + 20 * mtsin,
                    ss->sector->floorheight + 
                       GameModeInfo->teleFogHeight, 
                    GameModeInfo->teleFogType);

   // haleyjd: There was a hack here trying to avoid playing the sound on the
   // "first frame"; but if this is done, then you miss your own spawn sound
   // quite often, due to the fact your sound origin hasn't been moved yet.
   // So instead, I'll return the fog in *fog and play the sound at the caller.
   if(fog)
      *fog = mo;

   return true;
}

//
// G_ClosestDMSpot
//
// haleyjd 02/16/10: finds the closest deathmatch start to a given
// location. Returns -1 if a spot wasn't found.
//
// Will not return the spot marked in "notspot" if notspot is >= 0.
//
int G_ClosestDMSpot(fixed_t x, fixed_t y, int notspot)
{
   int j, numspots = deathmatch_p - deathmatchstarts;
   int closestspot = -1;
   fixed_t closestdist = 32767*FRACUNIT;

   if(numspots <= 0)
      return -1;

   for(j = 0; j < numspots; ++j)
   {
      fixed_t dist = P_AproxDistance(x - deathmatchstarts[j].x * FRACUNIT,
                                     y - deathmatchstarts[j].y * FRACUNIT);
      
      if(dist < closestdist && j != notspot)
      {
         closestdist = dist;
         closestspot = j;
      }
   }

   return closestspot;
}

extern const char *level_error;

//
// G_DeathMatchSpawnPlayer
//
// Spawns a player at one of the random death match spots
// called at level load and each death
//
void G_DeathMatchSpawnPlayer(int playernum)
{
   int j, selections = deathmatch_p - deathmatchstarts;
   mobj_t *fog = NULL;
   
   if(selections < MAXPLAYERS)
   {
      static char errormsg[64];
      psnprintf(errormsg, sizeof(errormsg), 
                "Only %d deathmatch spots, %d required", 
                selections, MAXPLAYERS);
      level_error = errormsg;
      return;
   }

   for(j = 0; j < 20; j++)
   {
      int i = P_Random(pr_dmspawn) % selections;
      
      if(G_CheckSpot(playernum, &deathmatchstarts[i], &fog))
      {
         deathmatchstarts[i].type = playernum + 1;
         P_SpawnPlayer(&deathmatchstarts[i]);
         if(fog)
            S_StartSound(fog, GameModeInfo->teleSound);
         return;
      }
   }

   // no good spot, so the player will probably get stuck
   P_SpawnPlayer(&playerstarts[playernum]);
}

//
// G_DoReborn
//
void G_DoReborn(int playernum)
{
   hub_changelevel = false;

   if(GameType == gt_single) // haleyjd 04/10/03
   {
      // reload level from scratch
      // sf: use P_HubReborn, so that if in a hub, we restart from the
      // last time we entered this level
      // normal levels are unaffected
      P_HubReborn();
   }
   else
   {                               // respawn at the start
      int i;
      mobj_t *fog = NULL;
      
      // first dissasociate the corpse
      players[playernum].mo->player = NULL;

      // spawn at random spot if in deathmatch
      if(GameType == gt_dm)
      {
         G_DeathMatchSpawnPlayer(playernum);
         
         // haleyjd: G_DeathMatchSpawnPlayer may set level_error
         if(level_error)
         {
            C_Printf(FC_ERROR "G_DeathMatchSpawnPlayer: %s\a\n", level_error);
            C_SetConsole();
         }

         return;
      }

      if(G_CheckSpot(playernum, &playerstarts[playernum], &fog))
      {
         P_SpawnPlayer(&playerstarts[playernum]);
         if(fog)
            S_StartSound(fog, GameModeInfo->teleSound);
         return;
      }

      // try to spawn at one of the other players spots
      for(i = 0; i < MAXPLAYERS; i++)
      {
         mobj_t *fog = NULL;

         if(G_CheckSpot(playernum, &playerstarts[i], &fog))
         {
            playerstarts[i].type = playernum + 1; // fake as other player
            P_SpawnPlayer(&playerstarts[i]);
            if(fog)
               S_StartSound(fog, GameModeInfo->teleSound);
            playerstarts[i].type = i+1;   // restore
            return;
         }
         // he's going to be inside something.  Too bad.
      }
      P_SpawnPlayer(&playerstarts[playernum]);
   }
}

void G_ScreenShot(void)
{
   gameaction = ga_screenshot;
}

// DOOM Par Times
int pars[4][10] =
{
   {0},
   {0,30,75,120,90,165,180,180,30,165},
   {0,90,90,90,120,90,360,240,30,170},
   {0,90,45,90,150,90,90,165,30,135}
};

// DOOM II Par Times
int cpars[34] = 
{
   30,90,120,120,90,150,120,120,270,90,  //  1-10
   210,150,150,150,210,150,420,150,210,150,  // 11-20
   240,150,180,150,150,300,330,420,300,180,  // 21-30
   120,30,30,30          // 31-34
};

//
// G_WorldDone
//

void G_WorldDone(void)
{
   gameaction = ga_worlddone;
   
   if(secretexit)
      players[consoleplayer].didsecret = true;
   
   if(LevelInfo.interText && !LevelInfo.killFinale &&
      (!LevelInfo.finaleSecretOnly || secretexit))
   {
      F_StartFinale();
   }
}

static skill_t d_skill;
static int     d_episode;
static int     d_map;
static char    d_mapname[10];

int G_GetMapForName(const char *name)
{
   // haleyjd 03/17/02: do not write back into argument!
   char normName[9];
   int episode, map;

   strncpy(normName, name, 9);

   M_Strupr(normName);
   
   if(GameModeInfo->flags & GIF_MAPXY)
   {
      map = isMAPxy(normName) ? 
         10 * (normName[3]-'0') + (normName[4]-'0') : 0;
      return map;
   }
   else
   {
      if(isExMy(normName))
      {
         episode = normName[1] - '0';
         map = normName[3] - '0';
      }
      else
      {
         episode = 1;
         map = 0;
      }
      return (episode*10) + map;
   }
}

char *G_GetNameForMap(int episode, int map)
{
   static char levelname[9];

   memset(levelname, 0, 9);

   if(GameModeInfo->flags & GIF_MAPXY)
   {
      sprintf(levelname, "MAP%02d", map);
   }
   else
   {
      sprintf(levelname, "E%01dM%01d", episode, map);
   }

   return levelname;
}

void G_DeferedInitNewNum(skill_t skill, int episode, int map)
{
   G_DeferedInitNew(skill, G_GetNameForMap(episode, map) );
}

void G_DeferedInitNew(skill_t skill, char *levelname)
{
   strncpy(d_mapname, levelname, 8);
   d_map = G_GetMapForName(levelname);
   
   if(!(GameModeInfo->flags & GIF_MAPXY))
   {
      d_episode = d_map / 10;
      d_map = d_map % 10;
   }
   else
      d_episode = 1;
   
   d_skill = skill;
   
   gameaction = ga_newgame;
}

// killough 7/19/98: Marine's best friend :)
static int G_GetHelpers(void)
{
   int j = M_CheckParm ("-dog");
   
   if(!j)
      j = M_CheckParm ("-dogs");

   return j ? ((j+1 < myargc) ? atoi(myargv[j+1]) : 1) : default_dogs;
}

// killough 3/1/98: function to reload all the default parameter
// settings before a new game begins

void G_ReloadDefaults(void)
{
   // killough 3/1/98: Initialize options based on config file
   // (allows functions above to load different values for demos
   // and savegames without messing up defaults).
   
   weapon_recoil = default_weapon_recoil;    // weapon recoil
   
   player_bobbing = default_player_bobbing;  // haleyjd: readded
   
   variable_friction = allow_pushers = true;
   
   monsters_remember = default_monsters_remember;   // remember former enemies
   
   monster_infighting = default_monster_infighting; // killough 7/19/98
   
   // dogs = netgame ? 0 : G_GetHelpers();             // killough 7/19/98
   if(GameType == gt_single) // haleyjd 04/10/03
      dogs = G_GetHelpers();
   else
      dogs = 0;
   
   dog_jumping = default_dog_jumping;
   
   distfriend = default_distfriend;                 // killough 8/8/98
   
   monster_backing = default_monster_backing;     // killough 9/8/98
   
   monster_avoid_hazards = default_monster_avoid_hazards; // killough 9/9/98
   
   monster_friction = default_monster_friction;     // killough 10/98
   
   help_friends = default_help_friends;             // killough 9/9/98
   
   autoaim = default_autoaim;
   
   allowmlook = default_allowmlook;
   
   monkeys = default_monkeys;
   
   bfgtype = default_bfgtype;               // killough 7/19/98
   
   // jff 1/24/98 reset play mode to command line spec'd version
   // killough 3/1/98: moved to here
   respawnparm = clrespawnparm;
   fastparm = clfastparm;
   nomonsters = clnomonsters;
   
   //jff 3/24/98 set startskill from defaultskill in config file, unless
   // it has already been set by a -skill parameter
   if(startskill == sk_none)
      startskill = (skill_t)(defaultskill - 1);
   
   demoplayback = false;
   singledemo = false; // haleyjd: restore from MBF
   netdemo = false;
   
   // killough 2/21/98:
   memset(playeringame+1, 0, sizeof(*playeringame)*(MAXPLAYERS-1));
   
   consoleplayer = 0;
   
   compatibility = false;     // killough 10/98: replaced by comp[] vector
   memcpy(comp, default_comp, sizeof comp);
   
   demo_version = version;     // killough 7/19/98: use this version's id
   demo_subversion = SUBVERSION; // haleyjd 06/17/01
   
   // killough 3/31/98, 4/5/98: demo sync insurance
   demo_insurance = default_demo_insurance == 1;
   
   G_ScrambleRand();
}

        // sf: seperate function
void G_ScrambleRand()
{                            // killough 3/26/98: shuffle random seed
   // haleyjd: restored MBF code
   // SoM 3/13/2002: New SMMU code actually compiles in VC++
   // sf: simpler
   rngseed = (unsigned int) time(NULL);

}

void G_DoNewGame (void)
{
   //G_StopDemo();
   G_ReloadDefaults();            // killough 3/1/98
   P_ClearHubs();                 // sf: clear hubs when starting new game
   
   netgame  = false;              // killough 3/29/98
   GameType = DefaultGameType;    // haleyjd  4/10/03
   dmflags  = default_dmflags;    // haleyjd  4/15/03
   basetic  = gametic;            // killough 9/29/98
   
   G_InitNew(d_skill, d_mapname);
   gameaction = ga_nothing;
}

// haleyjd 07/13/03: -fast projectile information list

typedef struct speedset_s
{
   metaobject_t parent;     // metatable link
   int mobjType;            // the type this speedset is for
   int normalSpeed;         // the normal speed of this thing type
   int fastSpeed;           // -fast speed
} speedset_t;

void G_SpeedSetAddThing(int thingtype, int nspeed, int fspeed)
{
   static metatype_t metaSpeedSetType;
   metaobject_t *o;
   mobjinfo_t   *mi = &mobjinfo[thingtype];

   // first time, register a metatype for speedsets
   if(!metaSpeedSetType.isinit)
   {
      MetaRegisterTypeEx(&metaSpeedSetType, 
                         METATYPE(speedset_t), sizeof(speedset_t),
                         NULL, NULL, NULL, NULL);
   }

   if((o = MetaGetObjectKeyAndType(mi->meta, "speedset", METATYPE(speedset_t))))
   {
      speedset_t *ss  = o->object;
      ss->normalSpeed = nspeed;
      ss->fastSpeed   = fspeed;
   }
   else
   {
      speedset_t *newSpeedSet = calloc(1, sizeof(speedset_t));

      newSpeedSet->mobjType    = thingtype;
      newSpeedSet->normalSpeed = nspeed;
      newSpeedSet->fastSpeed   = fspeed;

      MetaAddObject(mi->meta, "speedset", &newSpeedSet->parent, newSpeedSet, 
                    METATYPE(speedset_t));
   }
}

// killough 4/10/98: New function to fix bug which caused Doom
// lockups when idclev was used in conjunction with -fast.

void G_SetFastParms(int fast_pending)
{
   static int fast = 0;            // remembers fast state
   int i;
   metaobject_t *o;

   // TODO: Heretic support?
   // EDF FIXME: demon frame speedup difficult to generalize
   int demonRun1  = E_SafeState(S_SARG_RUN1);
   int demonPain2 = E_SafeState(S_SARG_PAIN2);
   
   if(fast != fast_pending)       // only change if necessary
   {
      if((fast = fast_pending))
      {
         for(i = demonRun1; i <= demonPain2; i++)
         {
            // killough 4/10/98
            // don't change 1->0 since it causes cycles
            if(states[i]->tics != 1 || demo_compatibility)
               states[i]->tics >>= 1;  
         }

         for(i = 0; i < NUMMOBJTYPES; ++i)
         {
            if((o = MetaGetObjectKeyAndType(mobjinfo[i].meta, "speedset", 
                                            METATYPE(speedset_t))))
            {
               mobjinfo[i].speed = ((speedset_t *)o->object)->fastSpeed;
            }
         }
      }
      else
      {
         for(i = demonRun1; i <= demonPain2; i++)
            states[i]->tics <<= 1;

         for(i = 0; i < NUMMOBJTYPES; ++i)
         {
            if((o = MetaGetObjectKeyAndType(mobjinfo[i].meta, "speedset", 
                                            METATYPE(speedset_t))))
            {
               mobjinfo[i].speed = ((speedset_t *)o->object)->normalSpeed;
            }
         }
      }
   }
}


void G_InitNewNum(skill_t skill, int episode, int map)
{
   G_InitNew(skill, G_GetNameForMap(episode, map) );
}

//
// G_InitNew
//
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//
void G_InitNew(skill_t skill, char *name)
{
   int i;

   // ACS_FIXME: For now, call ACS_NewGame from here. This may not suffice once
   // hubs are working, but then, pretty much all the level transfer code needs
   // to be rewritten for that to work smoothly anyways.
   ACS_NewGame();

   if(paused)
   {
      paused = false;
      S_ResumeSound();
   }

   hub_changelevel = false;  // sf
   
   if(skill > sk_nightmare)
      skill = sk_nightmare;

   G_SetFastParms(fastparm || skill == sk_nightmare);  // killough 4/10/98

   M_ClearRandom();
   
   respawnmonsters = 
      (GameModeInfo->flags & GIF_SKILL5RESPAWN && skill == sk_nightmare) 
      || respawnparm;

   // force players to be initialized upon first level load
   for(i = 0; i < MAXPLAYERS; i++)
      players[i].playerstate = PST_REBORN;

   usergame = true;                // will be set false if a demo
   paused = false;

   if(demoplayback)
   {
      netgame = false;
      displayplayer = consoleplayer = 0;
      P_ResetChasecam();      // sf: displayplayer changed
   }

   demoplayback = false;
   
   //G_StopDemo();
   
   automapactive = false;
   gameskill = skill;

   G_SetGameMapName(name);

   G_SetGameMap();  // sf
  
   //jff 4/16/98 force marks on automap cleared every new level start
   AM_clearMarks();

   if(demo_version >= 203)
      M_LoadOptions();     // killough 11/98: read OPTIONS lump from wad
  
   //G_StopDemo();
   
   G_DoLoadLevel();
}

//
// G_RecordDemo
//
// NETCODE_FIXME -- DEMO_FIXME: See the comment above where demos
// are read. Some of the same issues may apply here.
//
void G_RecordDemo(char *name)
{
   int i;
   
   demo_insurance = (default_demo_insurance != 0); // killough 12/98
   
   usergame = false;

   if(demoname)
      free(demoname);
   demoname = malloc(strlen(name) + 8);

   M_AddDefaultExtension(strcpy(demoname, name), ".lmp");  // 1/18/98 killough
   
   i = M_CheckParm("-maxdemo");
   
   if(i && i<myargc-1)
      maxdemosize = atoi(myargv[i+1]) * 1024;
   
   if(maxdemosize < 0x20000)  // killough
      maxdemosize = 0x20000;
   
   demobuffer = malloc(maxdemosize); // killough
   
   demorecording = true;
}

// These functions are used to read and write game-specific options in demos
// and savegames so that demo sync is preserved and savegame restoration is
// complete. Not all options (for example "compatibility"), however, should
// be loaded and saved here. It is extremely important to use the same
// positions as before for the variables, so if one becomes obsolete, the
// byte(s) should still be skipped over or padded with 0's.
// Lee Killough 3/1/98

// NETCODE_FIXME -- DEMO_FIXME -- SAVEGAME_FIXME: G_ReadOptions/G_WriteOptions
// These functions are going to be very important. The way they work may
// need to be altered for the new demo format too, although this must be
// done carefully so as to preserve demo compatibility with previous
// versions.

byte *G_WriteOptions(byte *demo_p)
{
   byte *target = demo_p + GAME_OPTION_SIZE;
   
   *demo_p++ = monsters_remember;  // part of monster AI -- byte 1
   
   *demo_p++ = variable_friction;  // ice & mud -- byte 2
   
   *demo_p++ = weapon_recoil;      // weapon recoil -- byte 3
   
   *demo_p++ = allow_pushers;      // PUSH Things -- byte 4
   
   *demo_p++ = 0;                  // ??? unused -- byte 5
   
   *demo_p++ = player_bobbing;     // whether player bobs or not -- byte 6
   
   // killough 3/6/98: add parameters to savegame, move around some in demos
   *demo_p++ = respawnparm; // byte 7
   *demo_p++ = fastparm;    // byte 8
   *demo_p++ = nomonsters;  // byte 9
   
   *demo_p++ = demo_insurance;        // killough 3/31/98 -- byte 10
   
   // killough 3/26/98: Added rngseed. 3/31/98: moved here
   *demo_p++ = (byte)((rngseed >> 24) & 0xff); // byte 11
   *demo_p++ = (byte)((rngseed >> 16) & 0xff); // byte 12
   *demo_p++ = (byte)((rngseed >>  8) & 0xff); // byte 13
   *demo_p++ = (byte)( rngseed        & 0xff); // byte 14
   
   // Options new to v2.03 begin here
   *demo_p++ = monster_infighting;   // killough 7/19/98 -- byte 15
   
   *demo_p++ = dogs;                 // killough 7/19/98 -- byte 16
   
   *demo_p++ = bfgtype;              // killough 7/19/98 -- byte 17
   
   *demo_p++ = 0;                    // unused - (beta mode) -- byte 18
   
   *demo_p++ = (distfriend >> 8) & 0xff;  // killough 8/8/98 -- byte 19  
   *demo_p++ =  distfriend       & 0xff;  // killough 8/8/98 -- byte 20
   
   *demo_p++ = monster_backing;           // killough 9/8/98 -- byte 21
   
   *demo_p++ = monster_avoid_hazards;     // killough 9/9/98 -- byte 22
   
   *demo_p++ = monster_friction;          // killough 10/98  -- byte 23
   
   *demo_p++ = help_friends;              // killough 9/9/98 -- byte 24
   
   *demo_p++ = dog_jumping; // byte 25
   
   *demo_p++ = monkeys;     // byte 26
   
   {   // killough 10/98: a compatibility vector now
      int i;
      for (i=0; i < COMP_TOTAL; i++)
         *demo_p++ = comp[i] != 0;
   }
   // bytes 27 - 58 : comp
   
   // haleyjd 05/23/04: autoaim is sync critical
   *demo_p++ = autoaim; // byte 59

   // haleyjd 04/06/05: allowmlook is sync critical
   *demo_p++ = allowmlook; // byte 60
   
   // CURRENT BYTES LEFT: 3

   //----------------
   // Padding at end
   //----------------
   while(demo_p < target)
      *demo_p++ = 0;
   
   if(demo_p != target)
      I_Error("G_WriteOptions: GAME_OPTION_SIZE is too small\n");
   
   return target;
}

// Same, but read instead of write

byte *G_ReadOptions(byte *demo_p)
{
   byte *target = demo_p + GAME_OPTION_SIZE;

   monsters_remember = *demo_p++;

   variable_friction = *demo_p;  // ice & mud
   demo_p++;

   weapon_recoil = *demo_p;      // weapon recoil
   demo_p++;

   allow_pushers = *demo_p;      // PUSH Things
   demo_p++;

   demo_p++;

   // haleyjd: restored bobbing to proper sync critical status
   player_bobbing = *demo_p;     // whether player bobs or not
   demo_p++;

   // killough 3/6/98: add parameters to savegame, move from demo
   respawnparm = *demo_p++;
   fastparm = *demo_p++;
   nomonsters = *demo_p++;

   demo_insurance = *demo_p++;              // killough 3/31/98

   // killough 3/26/98: Added rngseed to demos; 3/31/98: moved here

   rngseed  = *demo_p++ & 0xff;
   rngseed <<= 8;
   rngseed += *demo_p++ & 0xff;
   rngseed <<= 8;
   rngseed += *demo_p++ & 0xff;
   rngseed <<= 8;
   rngseed += *demo_p++ & 0xff;

   // Options new to v2.03
   if(demo_version >= 203)
   {
      monster_infighting = *demo_p++;   // killough 7/19/98
      
      dogs = *demo_p++;                 // killough 7/19/98
      
      bfgtype = *demo_p++;          // killough 7/19/98
      demo_p ++;        // sf: where beta was
      
      distfriend = *demo_p++ << 8;      // killough 8/8/98
      distfriend+= *demo_p++;
      
      monster_backing = *demo_p++;     // killough 9/8/98
      
      monster_avoid_hazards = *demo_p++; // killough 9/9/98
      
      monster_friction = *demo_p++;      // killough 10/98
      
      help_friends = *demo_p++;          // killough 9/9/98
      
      dog_jumping = *demo_p++;           // killough 10/98
      
      monkeys = *demo_p++;
      
      {   // killough 10/98: a compatibility vector now
         int i;
         for(i = 0; i < COMP_TOTAL; ++i)
            comp[i] = *demo_p++;
      }

      G_SetCompatibility();
     
      // Options new to v2.04, etc.

      // haleyjd 05/23/04: autoaim is sync-critical
      if(demo_version > 331 ||
         (demo_version == 331 && demo_subversion > 7))
      {
         autoaim = *demo_p++;
      }

      if(demo_version >= 333)
      {
         // haleyjd 04/06/05: allowmlook is sync-critical
         allowmlook = *demo_p++;
      }
   }
   else  // defaults for versions <= 2.02
   {
      int i;  // killough 10/98: a compatibility vector now
      for(i = 0; i <= comp_zerotags; ++i)
         comp[i] = compatibility;

      G_SetCompatibility();

      // haleyjd 05/18/06: BOOM fix: allow zombie exits
      if(demo_version >= 200 && demo_version <= 202)
         comp[comp_zombie] = false;
      
      monster_infighting = 1;           // killough 7/19/98
      
      monster_backing = 0;              // killough 9/8/98
      
      monster_avoid_hazards = 0;        // killough 9/9/98
      
      monster_friction = 0;             // killough 10/98
      
      help_friends = 0;                 // killough 9/9/98
      
      bfgtype = bfg_normal;             // killough 7/19/98
      
      dogs = 0;                         // killough 7/19/98
      dog_jumping = 0;                  // killough 10/98
      monkeys = 0;
      
      default_autoaim = autoaim;
      autoaim = 1;

      default_allowmlook = allowmlook;
      allowmlook = 0;
   }
  
   return target;
}

//
// G_SetOldDemoOptions
//
// haleyjd 02/21/10: Configure everything to run for an old demo.
//
static void G_SetOldDemoOptions(void)
{
   int i;

   compatibility = 1;

   for(i = 0; i < COMP_TOTAL; ++i)
      comp[i] = 1;
      
   monsters_remember     = 0;
   variable_friction     = 0;
   weapon_recoil         = 0;
   allow_pushers         = 0;
   player_bobbing        = 1;
   demo_insurance        = 0;
   monster_infighting    = 1;
   monster_backing       = 0;
   monster_avoid_hazards = 0;
   monster_friction      = 0;
   help_friends          = 0;
   bfgtype               = bfg_normal;
   dogs                  = 0;
   dog_jumping           = 0;
   monkeys               = 0;
   default_autoaim       = autoaim;
   autoaim               = 1;
   default_allowmlook    = allowmlook;
   allowmlook            = 0;
}

//
// G_BeginRecordingOld
//
// haleyjd 02/21/10: Support recording of vanilla demos.
//
static void G_BeginRecordingOld(void)
{
   int i;

   // support -longtics when recording vanilla format demos
   longtics_demo = (M_CheckParm("-longtics") != 0);

   // set demo version appropriately
   if(longtics_demo)
      demo_version = DOOM_191_VERSION; // v1.91 (unofficial patch)
   else
      demo_version = 109;              // v1.9

   demo_subversion = 0;

   G_SetOldDemoOptions();

   demo_p = demobuffer;

   *demo_p++ = demo_version;    
   *demo_p++ = gameskill;
   *demo_p++ = gameepisode;
   *demo_p++ = gamemap;
   *demo_p++ = (GameType == gt_dm);
   *demo_p++ = respawnparm;
   *demo_p++ = fastparm;
   *demo_p++ = nomonsters;
   *demo_p++ = consoleplayer;

   for(i = 0; i < MAXPLAYERS; ++i)
      *demo_p++ = playeringame[i];
}

/*
  haleyjd 06/17/01: new demo format changes

  1. The old version field is now always written as 255
  2. The signature has been changed to the null-term'd string ETERN
  3. The version and new subversion are written immediately following
     the signature
  4. cmd->updownangle is now recorded and read back appropriately

  12/14/01:
  5. gamemapname is recorded and will be used on loading demos

  Note that the demo-reading code still handles the "sacred" formats
  for DOOM, BOOM and MBF, so purists don't need to have heart attacks.
  However, only new Eternity-format demos can be written, and these
  will not be compatible with other engines.
*/
// NETCODE_FIXME -- DEMO_FIXME: Yet more demo writing.

void G_BeginRecording(void)
{
   int i;

   // haleyjd 02/21/10: -vanilla will record v1.9-format demos
   if(M_CheckParm("-vanilla"))
   {
      G_BeginRecordingOld();
      return;
   }
   
   demo_p = demobuffer;

   longtics_demo = true;
   
   //*demo_p++ = version;
   // haleyjd 06/17/01: always write 255 for Eternity-format demos,
   // since VERSION is now > 255 -- version setting is now handled
   // immediately after the new signature below.
   *demo_p++ = 255;
   
   // signature -- haleyjd: updated to use new Eternity signature
   *demo_p++ = eedemosig[0]; //0x1d;
   *demo_p++ = eedemosig[1]; //'M';
   *demo_p++ = eedemosig[2]; //'B';
   *demo_p++ = eedemosig[3]; //'F';
   *demo_p++ = eedemosig[4]; //0xe6;
   *demo_p++ = '\0';
   
   // haleyjd: write appropriate version and subversion numbers
   // write the WHOLE version number :P
   *demo_p++ =  version & 255;
   *demo_p++ = (version >> 8 ) & 255;
   *demo_p++ = (version >> 16) & 255;
   *demo_p++ = (version >> 24) & 255;
   
   *demo_p++ = SUBVERSION; // always ranges from 0 to 255
   
   // killough 2/22/98: save compatibility flag in new demos
   *demo_p++ = compatibility;       // killough 2/22/98
   
   demo_version = version;     // killough 7/19/98: use this version's id
   demo_subversion = SUBVERSION; // haleyjd 06/17/01
   
   *demo_p++ = gameskill;
   *demo_p++ = gameepisode;
   *demo_p++ = gamemap;
   *demo_p++ = GameType; // haleyjd 04/10/03
   *demo_p++ = consoleplayer;

   // haleyjd 04/14/03: save dmflags
   *demo_p++ = (unsigned char)(dmflags & 255);
   *demo_p++ = (unsigned char)((dmflags >> 8 ) & 255);
   *demo_p++ = (unsigned char)((dmflags >> 16) & 255);
   *demo_p++ = (unsigned char)((dmflags >> 24) & 255);
   
   // haleyjd 12/14/01: write gamemapname in new demos -- this will
   // enable demos to be recorded for arbitrarily-named levels
   for(i = 0; i < 8; i++)
      *demo_p++ = gamemapname[i];
   
   demo_p = G_WriteOptions(demo_p); // killough 3/1/98: Save game options
   
   for(i = 0; i < MAXPLAYERS; i++)
      *demo_p++ = playeringame[i];
   
   // killough 2/28/98:
   // We always store at least MIN_MAXPLAYERS bytes in demo, to
   // support enhancements later w/o losing demo compatibility
   
   for(; i < MIN_MAXPLAYERS; i++)
      *demo_p++ = 0;
}

//
// G_DeferedPlayDemo
//
void G_DeferedPlayDemo(const char *name)
{
   // haleyjd: removed SMMU cruft in attempt to fix
   defdemoname = name;
   gameaction = ga_playdemo;
}

//
// G_TimeDemo - sf
//
void G_TimeDemo(char *name, boolean showmenu)
{
   // haleyjd 10/19/01: hoo boy this had problems
   // It was using a variable called "name" from who knows where --
   // neither I nor Visual C++ could find a variable called name
   // that was in scope for this function -- now name is a
   // parameter, not s. I've also made some other adjustments.

   if(W_CheckNumForNameNSG(name, ns_demos) == -1)
   {
      C_Printf("%s: demo not found\n", name);
      return;
   }
   
   //G_StopDemo();         // stop any previous demos
   
   defdemoname = name;
   gameaction = ga_playdemo;
   singledemo = true;      // sf: moved from reloaddefaults
   
   singletics = true;
   timingdemo = true;      // show stats after quit   
}

//
// G_CheckDemoStatus
//
// Called after a death or level completion to allow demos to be cleaned up
// Returns true if a new demo loop action will take place
//
boolean G_CheckDemoStatus(void)
{
   if(demorecording)
   {
      demorecording = false;
      *demo_p++ = DEMOMARKER;
      
      if(!M_WriteFile(demoname, demobuffer, demo_p - demobuffer))
      {
         // killough 11/98
         I_Error("Error recording demo %s: %s\n", demoname,
                 errno ? strerror(errno) : "(Unknown Error)");
      }
      
      free(demobuffer);
      demobuffer = NULL;  // killough
      I_Error("Demo %s recorded\n", demoname);
      return false;  // killough
   }

   if(timingdemo)
   {
      int endtime = I_GetTime_RealTime();

      // killough -- added fps information and made it work for longer demos:
      unsigned int realtics = endtime - starttime;
      I_Error("Timed %u gametics in %u realtics = %-.1f frames per second\n",
              (unsigned int)(gametic), realtics,
              (unsigned int)(gametic) * (double) TICRATE / realtics);
   }              

   if(demoplayback)
   {
      if(singledemo)
      {
         demoplayback = false;
         C_SetConsole();
         return false;
      }
      
      Z_ChangeTag(demobuffer, PU_CACHE);
      G_ReloadDefaults();    // killough 3/1/98
      netgame = false;       // killough 3/29/98
      D_AdvanceDemo();
      return true;
   }
   return false;
}

void G_StopDemo(void)
{
   extern boolean advancedemo;
   
   if(!demorecording && !demoplayback)
      return;
   
   G_CheckDemoStatus();
   advancedemo = false;
   C_SetConsole();
}

// killough 1/22/98: this is a "Doom printf" for messages. I've gotten
// tired of using players->message=... and so I've added this doom_printf.
//
// killough 3/6/98: Made limit static to allow z_zone functions to call
// this function, without calling realloc(), which seems to cause problems.

// sf: changed to run console command instead

#define MAX_MESSAGE_SIZE 1024

void doom_printf(const char *s, ...)
{
   static char msg[MAX_MESSAGE_SIZE];
   va_list v;
   
   va_start(v, s);
   pvsnprintf(msg, sizeof(msg), s, v); // print message in buffer
   va_end(v);
   
   C_Puts(msg);  // set new message
   HU_PlayerMsg(msg);
}

//
// player_printf
//
// sf: printf to a particular player only
// to make up for the loss of player->msg = ...
//
void player_printf(player_t *player, const char *s, ...)
{
   static char msg[MAX_MESSAGE_SIZE];
   va_list v;
   
   va_start(v, s);
   pvsnprintf(msg, sizeof(msg), s, v); // print message in buffer
   va_end(v);
   
   if(player == &players[consoleplayer])
   {
      C_Puts(msg);  // set new message
      HU_PlayerMsg(msg);
   }
}

extern camera_t intercam;

//
// G_CoolViewPoint
//
// Change to new viewpoint
//
void G_CoolViewPoint(void)
{
   int viewtype;
   int old_displayplayer = displayplayer;
   int numdmspots = deathmatch_p - deathmatchstarts;

   viewtype = M_Random() % 3;
   
   // pick the next player
   do
   { 
      displayplayer++; 
      if(displayplayer == MAXPLAYERS) 
         displayplayer = 0;
   } while(!playeringame[displayplayer]);
  
   if(displayplayer != old_displayplayer)
   {
      ST_Start();
      HU_Start();
      S_UpdateSounds(players[displayplayer].mo);
      P_ResetChasecam();      // reset the chasecam
   }

   if(players[displayplayer].health <= 0)
      viewtype = 1; // use chasecam when player is dead
  
   // turn off the chasecam?
   if(chasecam_active && viewtype != 1)
   {
      chasecam_active = false;
      P_ChaseEnd();
   }

   // turn off followcam
   P_FollowCamOff();
   if(camera == &followcam)
      camera = NULL;
  
   if(viewtype == 1)  // view from the chasecam
   {
      chasecam_active = true;
      P_ChaseStart();
   }
   else if(viewtype == 2) // camera view
   {
      // sometimes check out the player's enemies
      mobj_t *spot = players[displayplayer].attacker;

      // no enemy? check out the player's current location then.
      if(!spot || spot->health <= 0)
         spot = players[displayplayer].mo;

      P_SetFollowCam(spot->x, spot->y, players[displayplayer].mo);
      
      camera = &followcam;
   }
  
   // pic a random number of tics until changing the viewpoint
   cooldemo_tics = (6 + M_Random() % 4) * TICRATE;
}

#ifndef EE_NO_SMALL_SUPPORT

//
// Small native functions
//


static cell AMX_NATIVE_CALL sm_exitlevel(AMX *amx, cell *params)
{
   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   G_ExitLevel();
   return 0;
}

static cell AMX_NATIVE_CALL sm_exitsecret(AMX *amx, cell *params)
{
   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   scriptSecret = true;
   G_SecretExitLevel();

   return 0;
}

//
// sm_startgame
//
// Implements StartGame(skill, levelname)
//
static cell AMX_NATIVE_CALL sm_startgame(AMX *amx, cell *params)
{
   int err;
   int skill;
   char *levelname;

   // note: user view of skill is 1 - 5, internal view is 0 - 4
   skill = (int)(params[1]) - 1;

   // get level name
   if((err = SM_GetSmallString(amx, &levelname, params[2])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return 0;
   }

   G_DeferedInitNew(skill, levelname);

   Z_Free(levelname);

   return 0;
}

static cell AMX_NATIVE_CALL sm_gameskill(AMX *amx, cell *params)
{
   return gameskill + 1;
}

static cell AMX_NATIVE_CALL sm_gametype(AMX *amx, cell *params)
{
   return (cell)GameType;
}

AMX_NATIVE_INFO game_Natives[] =
{
   { "_ExitLevel",  sm_exitlevel },
   { "_ExitSecret", sm_exitsecret },
   { "_StartGame",  sm_startgame },
   { "_GameSkill",  sm_gameskill },
   { "_GameType",   sm_gametype },
   { NULL, NULL }
};

#endif

//----------------------------------------------------------------------------
//
// $Log: g_game.c,v $
// Revision 1.59  1998/06/03  20:23:10  killough
// fix v2.00 demos
//
// Revision 1.58  1998/05/16  09:16:57  killough
// Make loadgame checksum friendlier
//
// Revision 1.57  1998/05/15  00:32:28  killough
// Remove unnecessary crash hack
//
// Revision 1.56  1998/05/13  22:59:23  killough
// Restore Doom bug compatibility for demos, fix multiplayer status bar
//
// Revision 1.55  1998/05/12  12:46:16  phares
// Removed OVER_UNDER code
//
// Revision 1.54  1998/05/07  00:48:51  killough
// Avoid displaying uncalled for precision
//
// Revision 1.53  1998/05/06  15:32:24  jim
// document g_game.c, change externals
//
// Revision 1.52  1998/05/05  16:29:06  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.51  1998/05/04  22:00:33  thldrmn
// savegamename globalization
//
// Revision 1.50  1998/05/03  22:15:19  killough
// beautification, decls & headers, net consistency fix
//
// Revision 1.49  1998/04/27  17:30:12  jim
// Fix DM demo/newgame status, remove IDK (again)
//
// Revision 1.48  1998/04/25  12:03:44  jim
// Fix secret level fix
//
// Revision 1.46  1998/04/24  12:09:01  killough
// Clear player cheats before demo starts
//
// Revision 1.45  1998/04/19  19:24:19  jim
// Improved IWAD search
//
// Revision 1.44  1998/04/16  16:17:09  jim
// Fixed disappearing marks after new level
//
// Revision 1.43  1998/04/14  10:55:13  phares
// Recoil, Bobbing, Monsters Remember changes in Setup now take effect immediately
//
// Revision 1.42  1998/04/13  21:36:12  phares
// Cemented ESC and F1 in place
//
// Revision 1.41  1998/04/13  10:40:58  stan
// Now synch up all items identified by Lee Killough as essential to
// game synch (including bobbing, recoil, rngseed).  Commented out
// code in g_game.c so rndseed is always set even in netgame.
//
// Revision 1.40  1998/04/13  00:39:29  jim
// Fix automap marks carrying over thru levels
//
// Revision 1.39  1998/04/10  06:33:00  killough
// Fix -fast parameter bugs
//
// Revision 1.38  1998/04/06  04:51:32  killough
// Allow demo_insurance=2
//
// Revision 1.37  1998/04/05  00:50:48  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.36  1998/04/02  16:15:24  killough
// Fix weapons switch
//
// Revision 1.35  1998/04/02  04:04:27  killough
// Fix DM respawn sticking problem
//
// Revision 1.34  1998/04/02  00:47:19  killough
// Fix net consistency errors
//
// Revision 1.33  1998/03/31  10:36:41  killough
// Fix crash caused by last change, add new RNG options
//
// Revision 1.32  1998/03/28  19:15:48  killough
// fix DM spawn bug (Stan's fix)
//
// Revision 1.31  1998/03/28  17:55:06  killough
// Fix weapons switch bug, improve RNG while maintaining sync
//
// Revision 1.30  1998/03/28  15:49:47  jim
// Fixed merge glitches in d_main.c and g_game.c
//
// Revision 1.29  1998/03/28  05:32:00  jim
// Text enabling changes for DEH
//
// Revision 1.28  1998/03/27  21:27:00  jim
// Fixed sky bug for Ultimate DOOM
//
// Revision 1.27  1998/03/27  16:11:43  stan
// (SG) Commented out lines in G_ReloadDefaults that reset netgame and
//      deathmatch to zero.
//
// Revision 1.26  1998/03/25  22:51:25  phares
// Fixed headsecnode bug trashing memory
//
// Revision 1.25  1998/03/24  15:59:17  jim
// Added default_skill parameter to config file
//
// Revision 1.24  1998/03/23  15:23:39  phares
// Changed pushers to linedef control
//
// Revision 1.23  1998/03/23  03:14:27  killough
// Fix savegame checksum, net/demo consistency w.r.t. weapon switch
//
// Revision 1.22  1998/03/20  00:29:39  phares
// Changed friction to linedef control
//
// Revision 1.21  1998/03/18  16:16:47  jim
// Fix to idmusnum handling
//
// Revision 1.20  1998/03/17  20:44:14  jim
// fixed idmus non-restore, space bug
//
// Revision 1.19  1998/03/16  12:29:14  killough
// Add savegame checksum test
//
// Revision 1.18  1998/03/14  17:17:24  jim
// Fixes to deh
//
// Revision 1.17  1998/03/11  17:48:01  phares
// New cheats, clean help code, friction fix
//
// Revision 1.16  1998/03/09  18:29:17  phares
// Created separately bound automap and menu keys
//
// Revision 1.15  1998/03/09  07:09:20  killough
// Avoid realloc() in doom_printf(), fix savegame -nomonsters bug
//
// Revision 1.14  1998/03/02  11:27:45  killough
// Forward and backward demo sync compatibility
//
// Revision 1.13  1998/02/27  08:09:22  phares
// Added gamemode checks to weapon selection
//
// Revision 1.12  1998/02/24  08:45:35  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.11  1998/02/23  04:19:35  killough
// Fix Internal and v1.9 Demo sync problems
//
// Revision 1.10  1998/02/20  22:50:51  killough
// Fix doom_printf for multiplayer games
//
// Revision 1.9  1998/02/20  06:15:08  killough
// Turn turbo messages on in demo playbacks
//
// Revision 1.8  1998/02/17  05:53:41  killough
// Suppress "green is turbo" in non-net games
// Remove dependence on RNG for net consistency (intereferes with RNG)
// Use new RNG calling method, with keys assigned to blocks
// Friendlier savegame version difference message (instead of nothing)
// Remove futile attempt to make Boom v1.9-savegame-compatibile
//
// Revision 1.7  1998/02/15  02:47:41  phares
// User-defined keys
//
// Revision 1.6  1998/02/09  02:57:08  killough
// Make player corpse limit user-configurable
// Fix ExM8 level endings
// Stop 'q' from ending demo recordings
//
// Revision 1.5  1998/02/02  13:44:45  killough
// Fix doom_printf and CheckSaveGame realloc bugs
//
// Revision 1.4  1998/01/26  19:23:18  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/24  21:03:07  jim
// Fixed disappearence of nomonsters, respawn, or fast mode after demo play or IDCLEV
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------

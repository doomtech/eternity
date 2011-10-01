// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3: 
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
// Various structures pertaining to the player.
//
//-----------------------------------------------------------------------------

#ifndef D_PLAYER_H__
#define D_PLAYER_H__

// The player data structure depends on a number
// of other structs: items (internal inventory),
// animation states (closely tied to the sprites
// used to represent them, unfortunately).

#include "p_pspr.h"

// In addition, the player is just a special
// case of the generic moving object/actor.

// Finally, for odd reasons, the player input
// is buffered within the player data struct,
// as commands per game tick.
#include "d_ticcmd.h"

// skins.
// haleyjd: player classes

struct playerclass_t;
struct skin_t;

//
// Player states.
//
enum 
{
  // Playing or camping.
  PST_LIVE,
  // Dead on the ground, view follows killer.
  PST_DEAD,
  // Ready to restart/respawn???
  PST_REBORN            

};

typedef int playerstate_t;

//
// Player internal flags, for cheats and debug.
//
typedef enum
{
  // No clipping, walk through barriers.
  CF_NOCLIP           = 1,
  // No damage, no health loss.
  CF_GODMODE          = 2,
  // Not really a cheat, just a debug aid.
  CF_NOMOMENTUM       = 4,
  // haleyjd 03/18/03: infinite ammo
  CF_INFAMMO          = 8,
  // haleyjd 12/29/10: immortality cheat
  CF_IMMORTAL         = 0x10
} cheat_t;


//
// Extended player object info: player_t
//
struct player_t
{
   Mobj        *mo;
   playerclass_t *pclass;      // haleyjd 09/27/07: player class
   skin_t        *skin;        // skin
   playerstate_t  playerstate; // live, dead, reborn, etc.
   ticcmd_t       cmd;         // current input

   // Determine POV,
   //  including viewpoint bobbing during movement.
  
   fixed_t        viewz;           // Focal origin above r.z  
   fixed_t        viewheight;      // Base height above floor for viewz.  
   fixed_t        deltaviewheight; // Bob/squat speed.
   fixed_t        bob;             // bounded/scaled total momentum.
   fixed_t        pitch;           // haleyjd 04/03/05: true pitch angle

    // killough 10/98: used for realistic bobbing (i.e. not simply overall speed)
   // mo->momx and mo->momy represent true momenta experienced by player.
   // This only represents the thrust that the player applies himself.
   // This avoids anomolies with such things as Boom ice and conveyors.
   fixed_t        momx, momy;      // killough 10/98

   int            health;       // This is only used between levels
   int            armorpoints;
   int            armortype;    // Armor type is 0-2.  
   bool           hereticarmor; // haleyjd 10/10/02: true if heretic armor (FIXME)

   // Power ups. invinc and invis are tic counters.
   int            powers[NUMPOWERS];
   bool           cards[NUMCARDS];
   bool           backpack;
  
   // Frags, kills of other players.
   int            frags[MAXPLAYERS];
   int            totalfrags;
   
   weapontype_t   readyweapon;
   weapontype_t   pendingweapon; // Is wp_nochange if not changing.

   int            weaponowned[NUMWEAPONS];
   int            ammo[NUMAMMO];
   int            maxammo[NUMAMMO];
   int            weaponctrs[NUMWEAPONS][3]; // haleyjd 03/31/06

   int            extralight;    // So gun flashes light up areas.
   
   int            attackdown; // True if button down last tic.
   int            usedown;

   int            cheats;      // Bit flags, for cheats and debug.

   int            refire;      // Refired shots are less accurate.

   // For intermission stats.
   int            killcount;
   int            itemcount;
   int            secretcount;
   bool           didsecret;    // True if secret level has been done.
  
   // For screen flashing (red or bright).
   int            damagecount;
   int            bonuscount;
   int            fixedcolormap; // Current PLAYPAL, for pain etc.

   Mobj          *attacker;      // Who did damage (NULL for floors/ceilings).

   int            colormap;      // colorshift for player sprites

   // Overlay view sprites (gun, etc).
   pspdef_t       psprites[NUMPSPRITES];
   int            curpsprite;    // haleyjd 04/05/07: for codeptr rewrite
  
   int            quake;         // If > 0, player is experiencing an earthquake

   int            jumptime;      // If > 0, player can't jump again yet

   // Player name
   char           name[20];
};


//
// INTERMISSION
// Structure passed e.g. to WI_Start(wb)
//
struct wbplayerstruct_t
{
  bool        in;     // whether the player is in game
    
  // Player stats, kills, collected items etc.
  int         skills;
  int         sitems;
  int         ssecret;
  int         stime; 
  int         frags[4];
  int         score;  // current score on entry, modified on return
  
};

struct wbstartstruct_t
{
  int         epsd;   // episode # (0-2)

  // if true, splash the secret level
  bool        didsecret;

  // haleyjd: if player is going to secret map
  bool        gotosecret;
    
  // previous and next levels, origin 0
  int         last;
  int         next;   
    
  int         maxkills;
  int         maxitems;
  int         maxsecret;
  int         maxfrags;

  // the par time
  int         partime;
    
  // index of this player in game
  int         pnum;   

  wbplayerstruct_t    plyr[MAXPLAYERS];

};


#endif

//----------------------------------------------------------------------------
//
// $Log: d_player.h,v $
// Revision 1.3  1998/05/04  21:34:15  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:26:31  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:07  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

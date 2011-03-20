//----------------------------------------------------------------------------
//
// Copyright(C) 2005 Simon Howard, James Haley
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
// Key Bindings
//
//----------------------------------------------------------------------------

#ifndef G_BIND_H__
#define G_BIND_H__

void G_InitKeyBindings(void);
boolean G_KeyResponder(event_t *ev, int bclass);

void G_ClearKeyStates(void);

typedef void (*binding_handler)(event_t *ev);

void G_EditBinding(const char *action);
const char *G_BoundKeys(const char *action);
const char *G_FirstBoundKey(const char *action);

// default file loading

void G_LoadDefaults(void);
void G_SaveDefaults(void);

void G_Bind_AddCommands(void);

// action variables

extern int action_forward,  action_backward;
extern int action_left,     action_right;
extern int action_moveleft, action_moveright;
extern int action_lookup,   action_lookdown;
extern int action_use;
extern int action_speed;
extern int action_attack;
extern int action_strafe;
extern int action_flip;
extern int action_jump;
extern int action_autorun;

extern int action_mlook;
extern int action_center;

extern int action_weapon1, action_weapon2, action_weapon3;
extern int action_weapon4, action_weapon5, action_weapon6;
extern int action_weapon7, action_weapon8, action_weapon9;
extern int action_nextweapon;
extern int action_weaponup;
extern int action_weapondown;


extern int action_frags;

extern int action_menu_help;
extern int action_menu_toggle;
extern int action_menu_setup;
extern int action_menu_up;
extern int action_menu_down;
extern int action_menu_confirm;
extern int action_menu_previous;
extern int action_menu_left;
extern int action_menu_right;
extern int action_menu_pageup;
extern int action_menu_pagedown;
extern int action_menu_contents;

extern int action_map_toggle;
extern int action_map_gobig;
extern int action_map_follow;
extern int action_map_mark;
extern int action_map_clear;
extern int action_map_grid;

extern int action_console_pageup;
extern int action_console_pagedown;
extern int action_console_toggle;
extern int action_console_tab;
extern int action_console_enter;
extern int action_console_up;
extern int action_console_down;
extern int action_console_backspace;

// haleyjd 07/03/04: key binding classes
enum keyactionclass
{
   kac_game,            // game bindings -- handled by G_BuildTiccmd
   kac_menu,            // menu bindings -- handled by MN_Responder
   kac_map,             // map  bindings -- handled by AM_Responder
   kac_console,         // con. bindings -- handled by C_Repsonder
   kac_hud,             // hud  bindings -- handled by HU_Responder
   kac_cmd,             // command
   NUMKEYACTIONCLASSES
};

enum keyaction_e
{
   ka_forward,
   ka_backward,
   ka_left,      
   ka_right,     
   ka_moveleft,  
   ka_moveright, 
   ka_use,       
   ka_strafe,    
   ka_attack,    
   ka_flip,
   ka_speed,
   ka_jump,
   ka_autorun,
   ka_mlook,     
   ka_lookup,    
   ka_lookdown,  
   ka_center,    
   ka_weapon1,   
   ka_weapon2,   
   ka_weapon3,   
   ka_weapon4,   
   ka_weapon5,   
   ka_weapon6,   
   ka_weapon7,   
   ka_weapon8,   
   ka_weapon9,   
   ka_nextweapon,
   ka_weaponup,
   ka_weapondown,
   ka_frags,   
   ka_menu_toggle,   
   ka_menu_help,     
   ka_menu_setup,    
   ka_menu_up,       
   ka_menu_down,     
   ka_menu_confirm,  
   ka_menu_previous, 
   ka_menu_left,     
   ka_menu_right,    
   ka_menu_pageup,   
   ka_menu_pagedown,
   ka_menu_contents,
   ka_map_right,   
   ka_map_left,    
   ka_map_up,      
   ka_map_down,    
   ka_map_zoomin,  
   ka_map_zoomout, 
   ka_map_toggle,  
   ka_map_gobig,   
   ka_map_follow,  
   ka_map_mark,    
   ka_map_clear,   
   ka_map_grid,
   ka_console_pageup,
   ka_console_pagedown,
   ka_console_toggle,
   ka_console_tab,
   ka_console_enter,
   ka_console_up,
   ka_console_down,
   ka_console_backspace,
   NUMKEYACTIONS
};

#endif

// EOF

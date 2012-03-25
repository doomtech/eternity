// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
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
// Game console commands
//
// console commands controlling the game functions.
//
// By Simon Howard
//
//---------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_batch.h"
#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_gi.h"     // haleyjd: gamemode pertinent info
#include "d_io.h"
#include "d_mod.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_main.h"
#include "e_lib.h"
#include "e_player.h" // haleyjd: turbo cmd must alter playerclass info
#include "f_wipe.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_shots.h"
#include "m_random.h"
#include "m_syscfg.h"
#include "mn_engin.h"
#include "mn_misc.h"  // haleyjd
#include "p_inter.h"
#include "p_partcl.h" // haleyjd: add particle event cmds
#include "p_setup.h"
#include "p_user.h"
#include "s_sound.h"  // haleyjd: restored exit sounds
#include "sounds.h"   // haleyjd: restored exit sounds
#include "v_misc.h"
#include "v_video.h"
#include "w_levels.h"
#include "w_wad.h"

// [CG] Added.
#include "cs_main.h"
#include "cs_demo.h"
#include "cs_wad.h"
#include "cl_net.h"
#include "cl_main.h"
#include "sv_main.h"

extern void I_WaitVBL(int); // haleyjd: restored exit sounds

extern int automlook;
extern int invert_mouse;
extern int keylookspeed;

////////////////////////////////////////////////////////////////////////
//
// Command batches
//

CONSOLE_COMMAND(batch, 0)
{
   if(Console.argc == 1)
   {
      const char *batch = C_GetCommandBatch(Console.argv[0]->constPtr());

      if(!batch)
         doom_printf("No such batch %s.", Console.argv[0]->constPtr());
      else
         doom_printf("%s -> %s", Console.argv[0]->constPtr(), batch);
   }
   else if(Console.argc == 2)
   {
      C_AddCommandBatch(
         Console.argv[0]->constPtr(), Console.argv[1]->constPtr()
      );
   }
   else
      doom_printf("usage: batch \"cmd1;cmd2;cmdN\"");
}

////////////////////////////////////////////////////////////////////////
//
// Game Commands
//

CONSOLE_COMMAND(i_exitwithmessage, 0)
{
   I_ExitWithMessage("%s\n", Console.args.constPtr());
}

CONSOLE_COMMAND(i_fatalerror, 0)
{
   I_FatalError(I_ERR_KILL, "%s\n", Console.args.constPtr());
}

CONSOLE_COMMAND(i_error, 0)
{
   I_Error("%s\n", Console.args.constPtr());
}

CONSOLE_COMMAND(z_print, cf_hidden)
{
   Z_PrintZoneHeap();
}

CONSOLE_COMMAND(z_dumpcore, cf_hidden)
{
   Z_DumpCore();
}

CONSOLE_COMMAND(starttitle, cf_notnet)
{
   // haleyjd 04/18/03
   if(demorecording || singledemo)
      G_CheckDemoStatus();

   MN_ClearMenus();         // put menu away
   D_StartTitle();
}

CONSOLE_COMMAND(endgame, cf_notnet)
{
   C_SetConsole();
}

CONSOLE_COMMAND(pause, cf_server)
{
   sendpause = true;
}

//
// haleyjd: Restoration of original exit behavior
//
void G_QuitDoom(void)
{
   // haleyjd: re-added code for playing random sound before exit
   extern int snd_card;

   if((!netgame || demoplayback) && !nosfxparm && snd_card &&
      GameModeInfo->flags & GIF_HASEXITSOUNDS)
   {
      S_StartSound(NULL, GameModeInfo->exitSounds[(gametic>>2)&7]);
      I_WaitVBL(105);
   }

   exit(0);
}

CONSOLE_COMMAND(quit, 0)
{
   if(CS_CLIENT)
      CL_Disconnect();

   G_QuitDoom();
}

CONSOLE_COMMAND(animshot, 0)
{
   if(!Console.argc)
   {
      C_Printf(
         "animated screenshot.\n"
         "usage: animshot <frames>\n");
      return;
   }
   animscreenshot = Console.argv[0]->toInt();
   C_InstaPopup();    // turn off console
}

CONSOLE_COMMAND(screenshot, 0)
{
   if(Console.cmdtype != c_typed)
      C_InstaPopup();
   G_ScreenShot();
}

// particle stuff

// haleyjd 05/18/02: changed particle vars
// to INT variables with choice of sprite, particle, or both

const char *particle_choices[] = { "sprites", "particles", "both" };

VARIABLE_BOOLEAN(drawparticles, NULL, onoff);
CONSOLE_VARIABLE(draw_particles, drawparticles, 0) {}

VARIABLE_INT(bloodsplat_particle, NULL, 0, 2, particle_choices);
CONSOLE_VARIABLE(bloodsplattype, bloodsplat_particle, 0) {}

VARIABLE_INT(bulletpuff_particle, NULL, 0, 2, particle_choices);
CONSOLE_VARIABLE(bulletpufftype, bulletpuff_particle, 0) {}

VARIABLE_BOOLEAN(drawrockettrails, NULL, onoff);
CONSOLE_VARIABLE(rocket_trails, drawrockettrails, 0) {}

VARIABLE_BOOLEAN(drawgrenadetrails, NULL, onoff);
CONSOLE_VARIABLE(grenade_trails, drawgrenadetrails, 0) {}

VARIABLE_BOOLEAN(drawbfgcloud, NULL, onoff);
CONSOLE_VARIABLE(bfg_cloud, drawbfgcloud, 0) {}

// [CG] 3D radial attack options
double radial_attack_damage = 1.0;
double default_radial_attack_damage = 1.0;
double radial_attack_self_damage = 1.0;
double default_radial_attack_self_damage = 1.0;
double radial_attack_lift = 0.5;
double default_radial_attack_lift = 0.5;
double radial_attack_self_lift = 0.8;
double default_radial_attack_self_lift = 0.8;

VARIABLE_FLOAT(radial_attack_damage, &default_radial_attack_damage, 0.0, 10.0);
CONSOLE_VARIABLE(radial_attack_damage, radial_attack_damage, 0) {}

VARIABLE_FLOAT(
   radial_attack_self_damage, &default_radial_attack_self_damage, 0.0, 10.0
);
CONSOLE_VARIABLE(radial_attack_self_damage, radial_attack_self_damage, 0) {}

VARIABLE_FLOAT(radial_attack_lift, &default_radial_attack_lift, 0.0, 10.0);
CONSOLE_VARIABLE(radial_attack_lift, radial_attack_lift, 0) {}

VARIABLE_FLOAT(
   radial_attack_self_lift, &default_radial_attack_self_lift, 0.0, 10.0
);
CONSOLE_VARIABLE(radial_attack_self_lift, radial_attack_self_lift, 0) {}

// [CG] Turning speed
VARIABLE_INT(turning_speed, &default_turning_speed, 0, 1000, NULL);
CONSOLE_VARIABLE(turning_speed, turning_speed, 0) {}

// always mlook

VARIABLE_BOOLEAN(automlook, NULL,           onoff);
CONSOLE_VARIABLE(alwaysmlook, automlook, 0) {}

// invert mouse

VARIABLE_BOOLEAN(invert_mouse, NULL,        onoff);
CONSOLE_VARIABLE(invertmouse, invert_mouse, 0) {}

// horizontal mouse sensitivity

VARIABLE_FLOAT(mouseSensitivity_horiz, NULL, 0.0, 1024.0);
CONSOLE_VARIABLE(sens_horiz, mouseSensitivity_horiz, 0) {}

// vertical mouse sensitivity

VARIABLE_FLOAT(mouseSensitivity_vert, NULL, 0.0, 1024.0);
CONSOLE_VARIABLE(sens_vert, mouseSensitivity_vert, 0) {}

int mouseSensitivity_c;

// combined sensitivity (for old-style menu only)
VARIABLE_INT(mouseSensitivity_c, NULL, 0, 16, NULL);
CONSOLE_VARIABLE(sens_combined, mouseSensitivity_c, 0)
{
   mouseSensitivity_horiz = mouseSensitivity_vert = mouseSensitivity_c * 4;
}

// [CG] 01/20/12: Create "vanilla" and "raw" mouse sensitivities.
bool default_vanilla_mouse_sensitivity = true;
VARIABLE_TOGGLE(mouseSensitivity_vanilla, &default_vanilla_mouse_sensitivity,
                yesno);
CONSOLE_VARIABLE(sens_vanilla, mouseSensitivity_vanilla, 0) {}

// player bobbing -- haleyjd: altered to read default, use netcmd

VARIABLE_BOOLEAN(player_bobbing, &default_player_bobbing, onoff);
CONSOLE_NETVAR(bobbing, player_bobbing, 0, netcmd_bobbing)
{
   if(clientserver)
   {
      if(CS_CLIENT)
      {
         if(dmflags2 & dmf_allow_movebob_change)
            CL_SendPlayerScalarInfo(ci_bobbing);
         else
            doom_printf("for server only");
      }
   }
   else if(consoleplayer > 0)
      doom_printf("for server only");
}

VARIABLE_FLOAT(bobbing_intensity, &default_bobbing_intensity, 0.0, 1.0);
CONSOLE_VARIABLE(bobbing_intensity, bobbing_intensity, 0)
{
   if(!clientserver)
   {
      doom_printf("C/S mode only.");
      return;
   }

   if(CS_CLIENT)
   {
      if(dmflags2 & dmf_allow_movebob_change)
         CL_SendPlayerScalarInfo(ci_bobbing_intensity);
      else
         doom_printf("Changes to player bobbing disallowed.");
   }

   printf(
      "Bobbing intensity: %g, %g.\n",
      bobbing_intensity,
      default_bobbing_intensity
   );
}

VARIABLE_BOOLEAN(doom_weapon_toggles, NULL, onoff);
CONSOLE_VARIABLE(doom_weapon_toggles, doom_weapon_toggles, 0)
{
   if(CS_CLIENT)
      CL_SendPlayerScalarInfo(ci_weapon_toggle);
}

VARIABLE_TOGGLE(allow_weapon_switch_while_firing,
                 &default_allow_weapon_switch_while_firing, yesno);
CONSOLE_VARIABLE(switch_weapons_while_firing,
                 allow_weapon_switch_while_firing, 0) {}

bool display_target_names = false;
bool default_display_target_names = false;

// [CG] Display target names (if target is a player) beneath crosshair.
VARIABLE_TOGGLE(display_target_names, &default_display_target_names, onoff);
CONSOLE_VARIABLE(display_target_names, display_target_names, 0) {}

// [CG] Weapon switching.

const char *weapon_switch_choices[] = { "vanilla", "pwo", "never" };
const char *ammo_switch_choices[] = { "vanilla", "pwo", "never" };

// [CG] Weapon switch on weapon pickup.

unsigned int weapon_switch_on_pickup = WEAPON_SWITCH_ALWAYS;
unsigned int default_weapon_switch_on_pickup = WEAPON_SWITCH_ALWAYS;

VARIABLE_INT(
   weapon_switch_on_pickup,
   &default_weapon_switch_on_pickup,
   WEAPON_SWITCH_ALWAYS,
   WEAPON_SWITCH_NEVER,
   weapon_switch_choices
);

CONSOLE_VARIABLE(weapon_switch_on_pickup, weapon_switch_on_pickup, 0)
{
   if(CS_CLIENT && net_peer != NULL)
      CL_SendPlayerScalarInfo(ci_wsop);
}

// [CG] Weapon switch on ammo pickup.

unsigned int ammo_switch_on_pickup = AMMO_SWITCH_VANILLA;
unsigned int default_ammo_switch_on_pickup = AMMO_SWITCH_VANILLA;

VARIABLE_INT(
   ammo_switch_on_pickup,
   &default_ammo_switch_on_pickup,
   AMMO_SWITCH_VANILLA,
   AMMO_SWITCH_DISABLED,
   ammo_switch_choices
);

CONSOLE_VARIABLE(ammo_switch_on_pickup, ammo_switch_on_pickup, 0)
{
   if(CS_CLIENT && net_peer != NULL)
      CL_SendPlayerScalarInfo(ci_asop);
}

// turbo scale

int turbo_scale = 100;
VARIABLE_INT(turbo_scale, NULL,         10, 400, NULL);
CONSOLE_VARIABLE(turbo, turbo_scale, 0)
{
   if(clientserver)
   {
      C_Printf("Cannot enable turbo in c/s mode.\n");
      return;
   }
   C_Printf("turbo scale: %i%%\n",turbo_scale);
   E_ApplyTurbo(turbo_scale);
}

CONSOLE_NETCMD(exitlevel, cf_server|cf_level, netcmd_exitlevel)
{
   // haleyjd 09/04/02: prevent exit if dead, unless comp flag on
   player_t *player = &players[Console.cmdsrc];

   if((player->health > 0) || comp[comp_zombie])
      G_ExitLevel();
}

//////////////////////////////////////
//
// C/S Demo Stuff
//

VARIABLE_STRING(cs_demo_folder_path, NULL, 1024);
CONSOLE_VARIABLE(cs_demo_folder_path, cs_demo_folder_path, 0) {}

VARIABLE_INT(
   cs_demo_compression_level, &default_cs_demo_compression_level, 0, 9, NULL
);
CONSOLE_VARIABLE(cs_demo_compression_level, cs_demo_compression_level, 0) {}

CONSOLE_COMMAND(csdemoplay, 0)
{
   if(!clientserver)
   {
      doom_printf("C/S mode only.");
      return;
   }

   if(Console.argc < 1)
   {
      doom_printf("Usage: csdemoplay demourl");
      return;
   }

   CS_PlayDemo(Console.argv[0]->constPtr());
}

CONSOLE_COMMAND(csdemospeedup, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(cs_demo_speed == cs_demo_speed_fastest)
   {
      doom_printf("Demo running at fastest speed.");
      return;
   }

   cs_demo_speed++;
   doom_printf("Demo speed: %s.", cs_demo_speed_names[cs_demo_speed]);
}

CONSOLE_COMMAND(csdemoslowdown, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(cs_demo_speed == cs_demo_speed_slowest)
   {
      doom_printf("Demo running at slowest speed.");
      return;
   }

   cs_demo_speed--;
   doom_printf("Demo speed: %s.", cs_demo_speed_names[cs_demo_speed]);
}

CONSOLE_COMMAND(csdemoprevious, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->hasPrevious())
      doom_printf("Already at the first demo.");
   else if(!cs_demo->playPrevious())
      doom_printf("Error: %s.\n", cs_demo->getError());
}

CONSOLE_COMMAND(csdemonext, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->hasNext())
      doom_printf("Already at the last demo.");
   else if(!cs_demo->playNext())
      doom_printf("Error: %s.\n", cs_demo->getError());
}

CONSOLE_COMMAND(csdemoback15, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->rewind(15 * TICRATE))
      doom_printf("Error rewinding demo: %s.", cs_demo->getError());
}

CONSOLE_COMMAND(csdemoback30, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->rewind(30 * TICRATE))
      doom_printf("Error rewinding demo: %s.", cs_demo->getError());
}

CONSOLE_COMMAND(csdemoback60, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->rewind(60 * TICRATE))
      doom_printf("Error rewinding demo: %s.", cs_demo->getError());
}

CONSOLE_COMMAND(csdemoforward15, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->fastForward(15 * TICRATE))
      doom_printf("Error fast-forwarding demo: %s.", cs_demo->getError());
}

CONSOLE_COMMAND(csdemoforward30, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->fastForward(30 * TICRATE))
      doom_printf("Error fast-forwarding demo: %s.", cs_demo->getError());
}

CONSOLE_COMMAND(csdemoforward60, 0)
{
   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->rewind(60 * TICRATE))
      doom_printf("Error fast-forwarding demo: %s.", cs_demo->getError());
}

CONSOLE_COMMAND(csdemorecord, 0)
{
   if(!clientserver)
   {
      doom_printf("C/S mode only.");
      return;
   }

   if(!net_peer)
   {
      doom_printf("Must be connected.");
      return;
   }

   if(CS_DEMORECORD)
   {
      doom_printf("Already recording.");
      return;
   }

   CS_RecordDemo();
}

CONSOLE_COMMAND(csdemosavecheckpoint, 0)
{
   if(!clientserver)
   {
      doom_printf("C/S mode only.");
      return;
   }

   if(!CS_DEMO)
   {
      doom_printf("No current demo.");
      return;
   }

   if(!cs_demo->saveCheckpoint())
   {
      doom_printf("%s", cs_demo->getError());
      return;
   }

   doom_printf("Checkpoint saved");
}

CONSOLE_COMMAND(csdemopreviouscheckpoint, 0)
{
   if(!clientserver)
   {
      doom_printf("C/S mode only.");
      return;
   }

   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->loadPreviousCheckpoint())
   {
      doom_printf("%s", cs_demo->getError());
      return;
   }

   doom_printf("Loaded previous checkpoint");
}

CONSOLE_COMMAND(csdemonextcheckpoint, 0)
{
   if(!clientserver)
   {
      doom_printf("C/S mode only.");
      return;
   }

   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(!cs_demo->loadNextCheckpoint())
   {
      doom_printf("%s", cs_demo->getError());
      return;
   }

   doom_printf("Loaded next checkpoint");
}

CONSOLE_COMMAND(csdemostop, 0)
{
   if(!clientserver)
   {
      doom_printf("C/S mode only.");
      return;
   }

   if(!CS_DEMO)
   {
      doom_printf("No current demo.");
      return;
   }

   if(!cs_demo->stop())
      doom_printf("Error stopping demo: %s.", cs_demo->getError());
   else if(CS_DEMOPLAY)
      doom_printf("Demo playback stopped.");
   else if(CS_DEMORECORD)
      doom_printf("Demo recording stopped.");
   else
      doom_printf("Demo stopped.");
}

CONSOLE_COMMAND(csdemopauseresume, 0)
{
   if(!clientserver)
   {
      doom_printf("C/S mode only.");
      return;
   }

   if(!CS_DEMO)
   {
      doom_printf("No current demo.");
      return;
   }

   if(!CS_DEMOPLAY)
   {
      doom_printf("No demo playing.");
      return;
   }

   if(cs_demo->isPaused())
   {
      if(!cs_demo->resume())
         doom_printf("Error resuming demo: %s.", cs_demo->getError());
      else
         doom_printf("Demo resumed.");
   }
   else
   {
      if(!cs_demo->pause())
         doom_printf("Error pausing demo: %s.", cs_demo->getError());
      else
         doom_printf("Demo paused.");
   }
}

//////////////////////////////////////
//
// Demo Stuff
//

CONSOLE_COMMAND(playdemo, cf_notnet)
{
   if(Console.argc < 1)
   {
      doom_printf("usage: playdemo demoname\n");
      return;
   }

   // haleyjd 02/15/10: check in both ns_demos and ns_global
   if(wGlobalDir.CheckNumForNameNSG(Console.argv[0]->constPtr(), lumpinfo_t::ns_demos) < 0)
   {
      C_Printf(FC_ERROR "%s not found\n", Console.argv[0]->constPtr());
      return;
   }

   G_DeferedPlayDemo(Console.argv[0]->constPtr());
   singledemo = true;            // quit after one demo
}


CONSOLE_COMMAND(stopdemo, cf_notnet)
{
   G_StopDemo();
}

CONSOLE_COMMAND(timedemo, cf_notnet)
{
   if(Console.argc != 2)
   {
      C_Printf("usage: timedemo demoname showmenu\n");
      return;
   }
   G_TimeDemo(Console.argv[0]->constPtr(), !!Console.argv[1]->toInt());
}

// 'cool' demo

VARIABLE_BOOLEAN(cooldemo, NULL,            onoff);
CONSOLE_VARIABLE(cooldemo, cooldemo, 0) {}

///////////////////////////////////////////////////
//
// Wads
//

// load new wad
// buffered command: r_init during load

CONSOLE_COMMAND(addfile, cf_notnet|cf_buffered)
{
   if(GameModeInfo->flags & GIF_SHAREWARE)
   {
      C_Printf(FC_ERROR"command not available in shareware games\n");
      return;
   }
   D_AddNewFile(Console.argv[0]->constPtr());
}

// list loaded wads

CONSOLE_COMMAND(listwads, 0)
{
   D_ListWads();
}

// random seed

CONST_INT(rngseed);
CONSOLE_CONST(rngseed, rngseed);

// suicide

CONSOLE_NETCMD(kill, cf_level, netcmd_kill)
{
   Mobj *mobj;
   int playernum;

   playernum = Console.cmdsrc;

   mobj = players[playernum].mo;
   P_DamageMobj(mobj, NULL, NULL,
                2*(players[playernum].health+players[playernum].armorpoints),
                MOD_SUICIDE);
   mobj->momx = mobj->momy = mobj->momz = 0;
   players[playernum].momx = players[playernum].momy = 0;
}

// [CG] Display map list
CONSOLE_COMMAND(maplist, 0)
{
   cs_map_t *map;
   unsigned int i, j;
   qstring buf, pad;

   if(!clientserver)
   {
      C_Printf("Maplists are not supported outside of c/s mode.\n");
      return;
   }

   for(i = 0; i < cs_map_count; i++)
   {
      size_t pad_size;

      map = &cs_maps[i];
      if(map->resource_count == 0)
      {
         C_Printf("%d. %s\n", i + 1, map->name);
         continue;
      }

      // [CG] Create the left margin.
      buf.Printf(0, "%d. %s: ", i + 1, map->name);
      pad_size = buf.length();
      pad.createSize(pad_size);
      for(j = 0; j < pad_size; j++)
         pad += " ";

      // [CG] Print the 1st line.
      doom_printf(
         "%d. %s: %s",
         i + 1,
         map->name,
         cs_resources[map->resource_indices[0]].name
      );

      // [CG] Print the subsequent lines (if necessary).
      for(j = 1; j < map->resource_count; j++)
      {
         doom_printf(
            "%s%s",
            pad.constPtr(),
            cs_resources[map->resource_indices[j]].name
         );
      }
   }
}

// change level

CONSOLE_NETCMD(map, cf_server, netcmd_map)
{
   int lumpnum;

   if(clientserver)
   {
      int map_index;

      // [CG] Don't switch maps during demo playback, we have other commands
      //      for that.
      if(CS_DEMOPLAY)
         return;

      if(!CS_SERVER)
      {
         C_Printf("C/S server mode only.\n");
         return;
      }

      if(!Console.argc || !Console.argv[0]->isNum())
      {
         C_Printf("usage: map <map number>\n");
         return;
      }

      map_index = Console.argv[0]->toInt() - 1;

      if(map_index < 0 || (unsigned int)(map_index) >= cs_map_count)
      {
         C_Printf("Invalid map number '%d'.\n", map_index + 1);
         return;
      }

      if(CS_SERVER)
      {
         cs_current_map_index = map_index;
         SV_BroadcastMapCompleted(false);
         G_SetGameMapName(cs_maps[cs_current_map_index].name);
         G_DoCompleted(false);
         gameaction = ga_worlddone;
      }
      return;
   }

   if(!Console.argc)
   {
      C_Printf("usage: map <mapname>\n"
               "   or map <wadfile.wad>\n");
      return;
   }

   G_StopDemo();

   // check for .wad files
   // i'm not particularly a fan of this myself, but..

   // haleyjd 03/12/06: no .wad loading in netgames

   if(!netgame && Console.argv[0]->length() > 4)
   {
      const char *extension;
      extension = Console.argv[0]->bufferAt(Console.argv[0]->length() - 4);
      if(!strcmp(extension, ".wad"))
      {
         if(D_AddNewFile(Console.argv[0]->constPtr()))
         {
            G_DeferedInitNew(gameskill, firstlevel);
         }
         return;
      }
   }

   // haleyjd 02/23/04: strict error checking
   lumpnum = W_CheckNumForName(Console.argv[0]->constPtr());

   if(lumpnum != -1 && P_CheckLevel(&wGlobalDir, lumpnum) != LEVEL_FORMAT_INVALID)
   {
      G_DeferedInitNew(gameskill, Console.argv[0]->constPtr());
   }
   else
      C_Printf(FC_ERROR "%s not found or is not a valid map\n", Console.argv[0]->constPtr());
}

// player name
VARIABLE_STRING(default_name, NULL, 20);
CONSOLE_NETVAR(name, default_name, cf_handlerset, netcmd_name)
{
   int playernum;

   if(Console.argc < 1)
      return;

   // [CG] Servers can't change their names.
   if(CS_SERVER)
      return;

   if(Console.argv[0]->length() <= 0)
   {
      doom_printf("Cannot blank your name.");
      return;
   }

   playernum = Console.cmdsrc;

   Console.argv[0]->copyInto(players[playernum].name, 20);

   if(playernum == consoleplayer)
   {
      Console.argv[0]->copyInto(default_name, 20);

      if(CS_CLIENT)
         CL_SendPlayerStringInfo(ci_name);
   }
}

// screenshot type

const char *str_pcx[] = { "bmp", "pcx", "tga", "png" };
VARIABLE_INT(screenshot_pcx, NULL, 0, 3, str_pcx);
CONSOLE_VARIABLE(shot_type, screenshot_pcx, 0) {}

VARIABLE_BOOLEAN(screenshot_gamma, NULL, yesno);
CONSOLE_VARIABLE(shot_gamma, screenshot_gamma, 0) {}

// textmode startup

extern int textmode_startup;            // d_main.c
VARIABLE_BOOLEAN(textmode_startup, NULL,        onoff);
CONSOLE_VARIABLE(textmode_startup, textmode_startup, 0) {}

// demo insurance

extern int demo_insurance;
const char *insure_str[]={"off", "on", "when recording"};
VARIABLE_INT(demo_insurance, &default_demo_insurance, 0, 2, insure_str);
CONSOLE_VARIABLE(demo_insurance, demo_insurance, cf_notnet) {}

extern int smooth_turning;
VARIABLE_BOOLEAN(smooth_turning, NULL,          onoff);
CONSOLE_VARIABLE(smooth_turning, smooth_turning, 0) {}

// SoM: mouse accel
int default_mouse_accel_type = 0;
const char *accel_options[]={ "off", "linear", "choco", "custom" };
VARIABLE_INT(mouseAccel_type, &default_mouse_accel_type, 0, 3, accel_options);
CONSOLE_VARIABLE(mouse_accel_type, mouseAccel_type, 0) {}

// [CG] 01/20/12: Custom mouse acceleration (threshold & value).
int default_mouse_accel_threshold = 10;
VARIABLE_INT(mouseAccel_threshold, &default_mouse_accel_threshold, 0, 1024,
             NULL);
CONSOLE_VARIABLE(mouse_accel_threshold, mouseAccel_threshold, 0) {}

double default_mouse_accel_value = 2.0;
VARIABLE_FLOAT(mouseAccel_value, &default_mouse_accel_value, 0.0, 100.0);
CONSOLE_VARIABLE(mouse_accel_value, mouseAccel_value, 0) {}

VARIABLE_BOOLEAN(novert, NULL, onoff);
CONSOLE_VARIABLE(mouse_novert, novert, 0) {}

// haleyjd: new stuff

extern int map_point_coordinates;
VARIABLE_BOOLEAN(map_point_coordinates, NULL, onoff);
CONSOLE_VARIABLE(map_coords, map_point_coordinates, 0) {}

VARIABLE_INT(dogs, &default_dogs, 0, 3, NULL);
CONSOLE_VARIABLE(numhelpers, dogs, cf_notnet) {}

VARIABLE_BOOLEAN(dog_jumping, &default_dog_jumping, onoff);
CONSOLE_NETVAR(dogjumping, dog_jumping, cf_server, netcmd_dogjumping) {}

VARIABLE_BOOLEAN(startOnNewMap, NULL, yesno);
CONSOLE_VARIABLE(startonnewmap, startOnNewMap, 0) {}

VARIABLE_BOOLEAN(autorun, NULL, onoff);
CONSOLE_VARIABLE(autorun, autorun, 0) {}

VARIABLE_BOOLEAN(runiswalk, NULL, onoff);
CONSOLE_VARIABLE(runiswalk, runiswalk, 0) {}

// haleyjd 03/22/09: iwad cvars

VARIABLE_STRING(gi_path_doomsw,  NULL, UL);
VARIABLE_STRING(gi_path_doomreg, NULL, UL);
VARIABLE_STRING(gi_path_doomu,   NULL, UL);
VARIABLE_STRING(gi_path_doom2,   NULL, UL);
VARIABLE_STRING(gi_path_tnt,     NULL, UL);
VARIABLE_STRING(gi_path_plut,    NULL, UL);
VARIABLE_STRING(gi_path_hacx,    NULL, UL);
VARIABLE_STRING(gi_path_hticsw,  NULL, UL);
VARIABLE_STRING(gi_path_hticreg, NULL, UL);
VARIABLE_STRING(gi_path_sosr,    NULL, UL);
VARIABLE_STRING(gi_path_fdoom,   NULL, UL);
VARIABLE_STRING(gi_path_fdoomu,  NULL, UL);
VARIABLE_STRING(gi_path_freedm,  NULL, UL);

VARIABLE_STRING(w_masterlevelsdirname, NULL, UL);

static bool G_TestIWADPath(char *path)
{
   M_NormalizeSlashes(path);

   // test for read access, and warn if non-existent
   if(access(path, R_OK))
   {
      if(menuactive)
         MN_ErrorMsg("Warning: cannot access filepath");
      else
         C_Printf(FC_ERROR "Warning: cannot access filepath\n");

      return false;
   }

   return true;
}

CONSOLE_VARIABLE(iwad_doom_shareware,    gi_path_doomsw,  cf_allowblank)
{
   G_TestIWADPath(gi_path_doomsw);
}

CONSOLE_VARIABLE(iwad_doom,              gi_path_doomreg, cf_allowblank)
{
   G_TestIWADPath(gi_path_doomreg);
}

CONSOLE_VARIABLE(iwad_ultimate_doom,     gi_path_doomu,   cf_allowblank)
{
   G_TestIWADPath(gi_path_doomu);
}

CONSOLE_VARIABLE(iwad_doom2,             gi_path_doom2,   cf_allowblank)
{
   G_TestIWADPath(gi_path_doom2);
}

CONSOLE_VARIABLE(iwad_tnt,               gi_path_tnt,     cf_allowblank)
{
   G_TestIWADPath(gi_path_tnt);
}

CONSOLE_VARIABLE(iwad_plutonia,          gi_path_plut,    cf_allowblank)
{
   G_TestIWADPath(gi_path_plut);
}

CONSOLE_VARIABLE(iwad_hacx,              gi_path_hacx,    cf_allowblank)
{
   G_TestIWADPath(gi_path_hacx);
}

CONSOLE_VARIABLE(iwad_heretic_shareware, gi_path_hticsw,  cf_allowblank)
{
   G_TestIWADPath(gi_path_hticsw);
}

CONSOLE_VARIABLE(iwad_heretic,           gi_path_hticreg, cf_allowblank)
{
   G_TestIWADPath(gi_path_hticreg);
}

CONSOLE_VARIABLE(iwad_heretic_sosr,      gi_path_sosr,    cf_allowblank)
{
   G_TestIWADPath(gi_path_sosr);
}

CONSOLE_VARIABLE(iwad_freedoom,          gi_path_fdoom,   cf_allowblank)
{
   G_TestIWADPath(gi_path_fdoom);
}

CONSOLE_VARIABLE(iwad_freedoomu,         gi_path_fdoomu,  cf_allowblank)
{
   G_TestIWADPath(gi_path_fdoomu);
}

CONSOLE_VARIABLE(iwad_freedm,            gi_path_freedm,  cf_allowblank)
{
   G_TestIWADPath(gi_path_freedm);
}

CONSOLE_VARIABLE(master_levels_dir, w_masterlevelsdirname, cf_allowblank)
{
   if(G_TestIWADPath(w_masterlevelsdirname))
      W_EnumerateMasterLevels(true);
}

VARIABLE_BOOLEAN(use_doom_config, NULL, yesno);
CONSOLE_VARIABLE(use_doom_config, use_doom_config, 0) {}

CONSOLE_COMMAND(m_resetcomments, 0)
{
   M_ResetDefaultComments();
   M_ResetSysComments();
}

CONSOLE_COMMAND(spectate_prev, 0)
{
   int i = displayplayer - 1;

   if((gamestate != GS_LEVEL) || ((!demoplayback) && (GameType == gt_dm)))
      return;

   if(CS_CLIENT && !clients[consoleplayer].spectating)
   {
      doom_printf("Cannot spectate other players while not a spectator.\n");
      return;
   }

   for(; i != displayplayer; i--)
   {
      if(i == -1)
         i = (MAXPLAYERS - 1);

      if(!playeringame[i] || clients[i].spectating)
         continue;

      if(!CS_TEAMS_ENABLED || clients[consoleplayer].team == team_color_none ||
                              clients[consoleplayer].team == clients[i].team)
         break;
   }

   P_SetDisplayPlayer(i);
}

CONSOLE_COMMAND(spectate_next, 0)
{
   int i = displayplayer + 1;

   if((gamestate != GS_LEVEL) || ((!demoplayback) && (GameType == gt_dm)))
      return;

   if(CS_CLIENT && !clients[consoleplayer].spectating)
   {
      doom_printf("Cannot spectate other players while not a spectator.\n");
      return;
   }

   for(; i != displayplayer; i++)
   {
      if(i >= MAXPLAYERS)
         i = 0;

      if(!playeringame[i] || clients[i].spectating)
         continue;

      if(!CS_TEAMS_ENABLED || clients[consoleplayer].team == team_color_none ||
                              clients[consoleplayer].team == clients[i].team)
         break;
   }

   P_SetDisplayPlayer(i);
}

CONSOLE_COMMAND(spectate_self, 0)
{
   if((gamestate != GS_LEVEL) || ((!demoplayback) && (GameType == gt_dm)))
      return;

   P_SetDisplayPlayer(consoleplayer);
}

////////////////////////////////////////////////////////////////
//
// Miscellaneous
//

CONSOLE_COMMAND(toggle, 0)
{
   command_t *command;
   const char *command_name;

   if(Console.argc != 1)
   {
      doom_printf("Usage: toggle <variable> %d", Console.argc);
      return;
   }

   if(!(command = C_GetCmdForName(Console.argv[0]->constPtr())))
   {
      doom_printf("%s is not a console variable", Console.argv[0]->constPtr());
      return;
   }

   command_name = Console.argv[0]->constPtr();

   if(command->type != ct_variable)
   {
      doom_printf("%s is not a console variable", command_name);
      return;
   }

   if(command->variable->type == vt_int)
   {
      if((command->variable->min != 0) || (command->variable->max != 1))
         doom_printf("%s is not a toggleable variable", command_name);
      else if(*((int *)(command->variable->variable)))
         C_RunCommand(command, "0");
      else
         C_RunCommand(command, "1");
   }
   else if(command->variable->type == vt_toggle)
   {
      if(*((bool *)(command->variable->variable)))
         C_RunCommand(command, "0");
      else
         C_RunCommand(command, "1");
   }
   else
      doom_printf("%s is not a toggleable variable", command_name);
}

////////////////////////////////////////////////////////////////
//
// Chat Macros
//

void G_AddChatMacros(void)
{
   int i;

   for(i=0; i<10; i++)
   {
      variable_t *variable;
      command_t *command;
      char tempstr[32];

      memset(tempstr, 0, 32);

      // create the variable first
      variable = estructalloc(variable_t, 1);
      variable->variable = &chat_macros[i];
      variable->v_default = NULL;
      variable->type = vt_string;      // string value
      variable->min = 0;
      variable->max = 128;
      variable->defines = NULL;

      // now the command
      command = estructalloc(command_t, 1);

      sprintf(tempstr, "chatmacro%i", i);
      command->name = estrdup(tempstr);
      command->type = ct_variable;
      command->flags = 0;
      command->variable = variable;
      command->handler = NULL;
      command->netcmd = 0;

      (C_AddCommand)(command); // hook into cmdlist
   }
}

///////////////////////////////////////////////////////////////
//
// Weapon Prefs
//

extern int weapon_preferences[2][NUMWEAPONS+1];

int weapon_numbers[NUMWEAPONS];

void G_SetWeapPref(int prefnum, int newvalue)
{
   int i;

   // find the pref which has the new value

   for(i=0; i<NUMWEAPONS; i++)
      if(weapon_preferences[0][i] == newvalue) break;

   // weapon_preferences[0][i] = weapon_preferences[0][prefnum];
   weapon_preferences[0][prefnum] = newvalue;

   // [CG] C/S clients have to inform the server of their new weapon
   //      preferences so it can assign the proper weapon during weapon/ammo
   //      pickups, or when ammunition is exhausted.
   if(CS_CLIENT && net_peer != NULL)
      CL_SendPlayerArrayInfo(ci_pwo, prefnum);
}

const char *weapon_str[NUMWEAPONS] = {
   "fist", "pistol", "shotgun", "chaingun", "rocket launcher", "plasma gun",
   "bfg", "chainsaw", "super shotgun"
};

void G_WeapPrefHandler(void)
{
   if(Console.argc)
   {
      int prefnum =
         (int *)(Console.command->variable->variable) - weapon_preferences[0];
      G_SetWeapPref(prefnum, Console.argv[0]->toInt());

   }
}

void G_AddWeapPrefs(void)
{
   int i;

   for(i = 0; i < NUMWEAPONS; i++) // haleyjd
   {
      variable_t *variable;
      command_t *command;
      char tempstr[16]; // haleyjd: increased size -- bug fix!

      memset(tempstr, 0, 16);

      // create the variable first
      variable = estructalloc(variable_t, 1);
      variable->variable = &weapon_preferences[0][i];
      variable->v_default = NULL;
      variable->type = vt_int;
      variable->min = 1;
      variable->max = NUMWEAPONS; // haleyjd
      // variable->defines = weapon_str;  // use weapon string defines

      // now the command
      command = estructalloc(command_t, 1);

      sprintf(tempstr, "weappref_%i", i+1);
      command->name = estrdup(tempstr);
      command->type = ct_variable;
      command->flags = cf_handlerset;
      command->variable = variable;
      command->handler = G_WeapPrefHandler;
      command->netcmd = 0;

      (C_AddCommand)(command); // hook into cmdlist
   }
}

///////////////////////////////////////////////////////////////
//
// Autoloaded Files
//
// haleyjd 03/13/06
//

static const char *autoload_names[] =
{
   "auto_wad_1",
   "auto_wad_2",
   "auto_deh_1",
   "auto_deh_2",
   "auto_csc_1",
   "auto_csc_2",
};

extern char *wad_files[];
extern char *deh_files[];
extern char *csc_files[];

static char **autoload_ptrs[] =
{
   &wad_files[0],
   &wad_files[1],
   &deh_files[0],
   &deh_files[1],
   &csc_files[0],
   &csc_files[1],
};

void G_AddAutoloadFiles(void)
{
   variable_t *variable;
   command_t *command;
   int i;

   for(i = 0; i < 6; ++i)
   {
      // create the variable first
      variable = estructalloc(variable_t, 1);
      variable->variable = autoload_ptrs[i];
      variable->v_default = NULL;
      variable->type = vt_string;
      variable->min = 0;
      variable->max = 1024;
      variable->defines = NULL;

      // now the command
      command = estructalloc(command_t, 1);
      command->name = autoload_names[i];
      command->type = ct_variable;
      command->flags = cf_allowblank;
      command->variable = variable;
      command->handler = NULL;
      command->netcmd = 0;

      (C_AddCommand)(command); // hook into cmdlist
   }
}

///////////////////////////////////////////////////////////////
//
// Compatibility vectors
//

// names given to cmds
const char *comp_strings[] =
{
  "telefrag",
  "dropoff",
  "vile",
  "pain",
  "skull",
  "blazing",
  "doorlight",
  "model",
  "god",
  "falloff",
  "floors",
  "skymap",
  "pursuit",
  "doorstuck",
  "staylift",
  "zombie",
  "stairs",
  "infcheat",
  "zerotags",
  "terrain",    // haleyjd: TerrainTypes
  "respawnfix", //          Nightmare respawn location fix
  "fallingdmg", //          Players take falling damage
  "soul",       //          Lost soul bouncing
  "theights",   //          Thing heights fix
  "overunder",  //          10/19/02: z checking
  "planeshoot", //          09/22/07: plane shooting
  "special",    //          08/29/09: special failure behavior
  "ninja",      //          04/18/10: ninja spawn
  "mouselook",  // [CG]     02/20/12: abridged vertical mouselook range
  "2dradatk",   //          02/20/12: radius attacks only thrust actors in 2D
};

static void Handler_CompTHeights(void)
{
   P_ChangeThingHeights();
}

void G_AddCompat(void)
{
   int i;

   for(i = 0; i <= comp_2dradatk; i++)   // haleyjd: update this regularly
   {
      variable_t *variable;
      command_t *command;
      char tempstr[32];

      // create the variable first
      variable = estructalloc(variable_t, 1);
      variable->variable = &comp[i];
      variable->v_default = &default_comp[i];
      variable->type = vt_int;      // string value
      variable->min = 0;
      variable->max = 1;
      variable->defines = yesno;

      // now the command
      command = estructalloc(command_t, 1);

      psnprintf(tempstr, sizeof(tempstr), "comp_%s", comp_strings[i]);
      command->name = estrdup(tempstr);
      command->type = ct_variable;

      switch(i)
      {
      case comp_theights:
         command->flags = cf_server | cf_netvar;
         command->handler = Handler_CompTHeights;
         break;
      default:
         command->flags = cf_server | cf_netvar;
         command->handler = NULL;
         break;
      }

      command->variable = variable;
      command->netcmd = netcmd_comp_0 + i;

      (C_AddCommand)(command); // hook into cmdlist
   }
}

void G_AddCommands(void)
{
   C_AddCommand(i_exitwithmessage);
   C_AddCommand(i_fatalerror);
   C_AddCommand(i_error);
   C_AddCommand(z_print);
   C_AddCommand(z_dumpcore);
   C_AddCommand(starttitle);
   C_AddCommand(endgame);
   C_AddCommand(pause);
   C_AddCommand(quit);
   C_AddCommand(animshot);
   C_AddCommand(screenshot);
   C_AddCommand(shot_type);
   C_AddCommand(shot_gamma);
   C_AddCommand(alwaysmlook);
   C_AddCommand(bobbing);
   C_AddCommand(bobbing_intensity);
   C_AddCommand(doom_weapon_toggles);
   C_AddCommand(turning_speed);
   C_AddCommand(sens_horiz);
   C_AddCommand(sens_vert);
   C_AddCommand(sens_combined);
   C_AddCommand(sens_vanilla);
   C_AddCommand(mouse_accel_type);
   C_AddCommand(mouse_accel_threshold);
   C_AddCommand(mouse_accel_value);
   C_AddCommand(mouse_novert);
   C_AddCommand(invertmouse);
   C_AddCommand(turbo);
   C_AddCommand(playdemo);
   C_AddCommand(timedemo);
   C_AddCommand(cooldemo);
   C_AddCommand(stopdemo);
   C_AddCommand(exitlevel);
   C_AddCommand(addfile);
   C_AddCommand(listwads);
   C_AddCommand(rngseed);
   C_AddCommand(kill);
   C_AddCommand(map);
   C_AddCommand(name);
   C_AddCommand(textmode_startup);
   C_AddCommand(demo_insurance);
   C_AddCommand(smooth_turning);

   // haleyjd: new stuff
   C_AddCommand(map_coords);
   C_AddCommand(numhelpers);
   C_AddCommand(dogjumping);
   C_AddCommand(draw_particles);
   C_AddCommand(bloodsplattype);
   C_AddCommand(bulletpufftype);
   C_AddCommand(rocket_trails);
   C_AddCommand(grenade_trails);
   C_AddCommand(bfg_cloud);
   C_AddCommand(startonnewmap);
   C_AddCommand(autorun);
   C_AddCommand(runiswalk);
   C_AddCommand(m_resetcomments);

   // [CG] 01/29/2012: Spectate previous, next and self
   C_AddCommand(spectate_prev);
   C_AddCommand(spectate_next);
   C_AddCommand(spectate_self);

   // haleyjd 03/22/09: iwad paths
   C_AddCommand(iwad_doom_shareware);
   C_AddCommand(iwad_doom);
   C_AddCommand(iwad_ultimate_doom);
   C_AddCommand(iwad_doom2);
   C_AddCommand(iwad_tnt);
   C_AddCommand(iwad_plutonia);
   C_AddCommand(iwad_hacx);
   C_AddCommand(iwad_heretic_shareware);
   C_AddCommand(iwad_heretic);
   C_AddCommand(iwad_heretic_sosr);
   C_AddCommand(iwad_freedoom);
   C_AddCommand(iwad_freedoomu);
   C_AddCommand(iwad_freedm);
   C_AddCommand(master_levels_dir);
   C_AddCommand(use_doom_config);

   // [CG] Radial attack options
   C_AddCommand(radial_attack_damage);
   C_AddCommand(radial_attack_self_damage);
   C_AddCommand(radial_attack_lift);
   C_AddCommand(radial_attack_self_lift);

   // [CG] More new stuff.
   C_AddCommand(maplist);
   C_AddCommand(weapon_switch_on_pickup);
   C_AddCommand(ammo_switch_on_pickup);
   C_AddCommand(display_target_names);
   C_AddCommand(toggle);
   C_AddCommand(switch_weapons_while_firing);
   C_AddCommand(batch);

   // [CG] C/S demo controls.
   C_AddCommand(cs_demo_folder_path);
   C_AddCommand(cs_demo_compression_level);
   C_AddCommand(csdemoplay);
   C_AddCommand(csdemospeedup);
   C_AddCommand(csdemoslowdown);
   C_AddCommand(csdemoprevious);
   C_AddCommand(csdemonext);
   C_AddCommand(csdemoback15);
   C_AddCommand(csdemoback30);
   C_AddCommand(csdemoback60);
   C_AddCommand(csdemoforward15);
   C_AddCommand(csdemoforward30);
   C_AddCommand(csdemoforward60);
   C_AddCommand(csdemorecord);
   C_AddCommand(csdemosavecheckpoint);
   C_AddCommand(csdemopreviouscheckpoint);
   C_AddCommand(csdemonextcheckpoint);
   C_AddCommand(csdemostop);
   C_AddCommand(csdemopauseresume);

   G_AddChatMacros();
   G_AddWeapPrefs();
   G_AddCompat();
   G_AddAutoloadFiles(); // haleyjd
   P_AddEventVars(); // haleyjd
}

// EOF

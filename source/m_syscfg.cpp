// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2009 James Haley
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
//    "System" configuration file.
//    Sets some low-level stuff that must be read before the main config
//    file can be loaded.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "doomstat.h"
#include "d_main.h"
#include "d_gi.h"
#include "gl/gl_vars.h"
#include "hal/i_picker.h"
#include "hal/i_platform.h"
#include "i_sound.h"
#include "i_video.h"
#include "m_misc.h"
#include "m_shots.h"
#include "mn_menus.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "st_stuff.h"
#include "w_wad.h"
#include "w_levels.h"

// [CG] 09/17/11 Added.
#include "cs_config.h"
#include "cs_main.h"
#include "cs_hud.h"
#include "cs_demo.h"
#include "cl_cmd.h"

// External variables configured here:

extern int textmode_startup;
extern int realtic_clock_rate; // killough 4/13/98: adjustable timer
extern bool d_fastrefresh;     // haleyjd 01/04/10
extern int iwad_choice;        // haleyjd 03/19/10

#ifdef _SDL_VER
extern int waitAtExit;
extern int grabmouse;
extern int use_vsync;
extern bool unicodeinput;
extern int audio_buffers;
#endif

#if (EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS) || defined(HAVE_SCHED_SETAFFINITY)
extern unsigned int process_affinity_mask;
#endif

#if defined _MSC_VER
extern int disable_sysmenu;
#endif

// option name defines

#define ITEM_USE_DOOM_CONFIG    "use_doom_config"

#define ITEM_IWAD_DOOM_SW       "iwad_doom_shareware"
#define ITEM_IWAD_DOOM          "iwad_doom"
#define ITEM_IWAD_ULTIMATE_DOOM "iwad_ultimate_doom"
#define ITEM_IWAD_DOOM2         "iwad_doom2"
#define ITEM_IWAD_TNT           "iwad_tnt"
#define ITEM_IWAD_PLUTONIA      "iwad_plutonia"
#define ITEM_IWAD_HACX          "iwad_hacx"
#define ITEM_IWAD_HERETIC_SW    "iwad_heretic_shareware"
#define ITEM_IWAD_HERETIC       "iwad_heretic"
#define ITEM_IWAD_HERETIC_SOSR  "iwad_heretic_sosr"
#define ITEM_IWAD_FREEDOOM      "iwad_freedoom"
#define ITEM_IWAD_FREEDOOMU     "iwad_freedoomu"
#define ITEM_IWAD_FREEDM        "iwad_freedm"
#define ITEM_IWAD_CHOICE        "iwad_choice"

// system defaults array

static default_t sysdefaults[] =
{
      //jff 3/3/98
   DEFAULT_INT("config_help", &config_help, NULL, 1, 0, 1, default_t::wad_no,
               "1 to show help strings about each variable in config file"),

   DEFAULT_INT(ITEM_USE_DOOM_CONFIG, &use_doom_config, NULL, 0, 0, 1, default_t::wad_no,
               "1 to use base/doom/eternity.cfg for all DOOM gamemodes"),

   // IWAD paths

   DEFAULT_STR(ITEM_IWAD_DOOM_SW, &gi_path_doomsw, NULL, "", default_t::wad_no,
               "IWAD path for DOOM Shareware"),

   DEFAULT_STR(ITEM_IWAD_DOOM, &gi_path_doomreg, NULL, "", default_t::wad_no,
               "IWAD path for DOOM Registered"),

   DEFAULT_STR(ITEM_IWAD_ULTIMATE_DOOM, &gi_path_doomu, NULL, "", default_t::wad_no,
               "IWAD path for The Ultimate DOOM"),

   DEFAULT_STR(ITEM_IWAD_DOOM2, &gi_path_doom2, NULL, "", default_t::wad_no,
               "IWAD path for DOOM 2"),

   DEFAULT_STR(ITEM_IWAD_TNT, &gi_path_tnt, NULL, "", default_t::wad_no,
               "IWAD path for Final DOOM: TNT - Evilution"),

   DEFAULT_STR(ITEM_IWAD_PLUTONIA, &gi_path_plut, NULL, "", default_t::wad_no,
               "IWAD path for Final DOOM: The Plutonia Experiment"),

   DEFAULT_STR(ITEM_IWAD_HACX, &gi_path_hacx, NULL, "", default_t::wad_no,
               "IWAD path for HACX - Twitch 'n Kill (v1.2 or later)"),

   DEFAULT_STR(ITEM_IWAD_HERETIC_SW, &gi_path_hticsw, NULL, "", default_t::wad_no,
               "IWAD path for Heretic Shareware"),

   DEFAULT_STR(ITEM_IWAD_HERETIC, &gi_path_hticreg, NULL, "", default_t::wad_no,
               "IWAD path for Heretic Registered"),

   DEFAULT_STR(ITEM_IWAD_HERETIC_SOSR, &gi_path_sosr, NULL, "", default_t::wad_no,
               "IWAD path for Heretic: Shadow of the Serpent Riders"),

   DEFAULT_STR(ITEM_IWAD_FREEDOOM, &gi_path_fdoom, NULL, "", default_t::wad_no,
               "IWAD path for Freedoom (Doom 2 gamemission)"),

   DEFAULT_STR(ITEM_IWAD_FREEDOOMU, &gi_path_fdoomu, NULL, "", default_t::wad_no,
               "IWAD path for Freedoom (Ultimate Doom gamemission)"),

   DEFAULT_STR(ITEM_IWAD_FREEDM, &gi_path_freedm, NULL, "", default_t::wad_no,
               "IWAD path for FreeDM (Freedoom Deathmatch IWAD)"),

   DEFAULT_INT(ITEM_IWAD_CHOICE, &iwad_choice, NULL, -1, -1, NUMPICKIWADS, default_t::wad_no,
               "Number of last IWAD chosen from the IWAD picker"),

   DEFAULT_STR("master_levels_dir", &w_masterlevelsdirname, NULL, "", default_t::wad_no,
               "Directory containing Master Levels wad files"),

   // 11/04/09: system-level options moved here from the main config

   DEFAULT_INT("textmode_startup", &textmode_startup, NULL, 0, 0, 1, default_t::wad_no,
               "Start up ETERNITY in text mode"),

   DEFAULT_INT("realtic_clock_rate", &realtic_clock_rate, NULL, 100, 10, 1000, default_t::wad_no,
               "Percentage of normal speed (35 fps) realtic clock runs at"),

   // killough
   DEFAULT_INT("snd_channels", &default_numChannels, NULL, 32, 1, 32, default_t::wad_no,
               "number of sound effects handled simultaneously"),

   // haleyjd 12/08/01
   DEFAULT_INT("force_flip_pan", &forceFlipPan, NULL, 0, 0, 1, default_t::wad_no,
               "Force reversal of stereo audio channels: 0 = normal, 1 = reverse"),

   // haleyjd 04/21/10
   DEFAULT_INT("s_equalizer", &s_equalizer, NULL, 1, 0, 1, default_t::wad_no,
               "1 to enable three-band equalizer"),

   DEFAULT_FLOAT("s_lowfreq", &s_lowfreq, NULL, 880.0, 0, UL, default_t::wad_no,
                 "High end of low pass band"),
   
   DEFAULT_FLOAT("s_highfreq", &s_highfreq, NULL, 5000.0, 0, UL, default_t::wad_no,
                 "Low end of high pass band"),

   DEFAULT_FLOAT("s_eqpreamp", &s_eqpreamp, NULL, 0.93896, 0, 100, default_t::wad_no,
                 "Preamplification factor"),

   DEFAULT_FLOAT("s_lowgain", &s_lowgain, NULL, 1.2, 0, 300, default_t::wad_no,
                 "Low pass gain"),

   DEFAULT_FLOAT("s_midgain", &s_midgain, NULL, 1.0, 0, 300, default_t::wad_no,
                 "Midrange gain"),

   DEFAULT_FLOAT("s_highgain", &s_highgain, NULL, 0.8, 0, 300, default_t::wad_no,
                 "High pass gain"),  

   DEFAULT_INT("s_enviro_volume", &s_enviro_volume, NULL, 4, 0, 16, default_t::wad_no,
               "Volume of environmental sound sequences"),

   // haleyjd 12/24/11
   DEFAULT_BOOL("s_hidefmusic", &s_hidefmusic, NULL, false, default_t::wad_no,
                "use hi-def music if available"),

   // jff 3/30/98 add ability to take screenshots in BMP format
   DEFAULT_INT("screenshot_pcx", &screenshot_pcx, NULL, 1, 0, 3, default_t::wad_no,
               "screenshot format (0=BMP,1=PCX,2=TGA,3=PNG)"),
   
   DEFAULT_INT("screenshot_gamma", &screenshot_gamma, NULL, 1, 0, 1, default_t::wad_no,
               "1 to use gamma correction in screenshots"),

   DEFAULT_INT("i_videodriverid", &i_videodriverid, NULL, -1, -1, VDR_MAXDRIVERS-1, 
               default_t::wad_no, i_videohelpstr),

   DEFAULT_INT("i_softbitdepth", &i_softbitdepth, NULL, 8, 8, 32, default_t::wad_no,
               "Software backend screen bitdepth (8, 16, 24, or 32)"),

   DEFAULT_STR("i_videomode", &i_default_videomode, &i_videomode, "640x480w", default_t::wad_no,
               "Description of video mode parameters (WWWWxHHHH[flags])"),

   DEFAULT_INT("use_vsync", &use_vsync, NULL, 1, 0, 1, default_t::wad_no,
               "1 to enable wait for vsync to avoid display tearing"),

   DEFAULT_INT("mn_favaspectratio", &mn_favaspectratio, NULL, 0, 0, AR_NUMASPECTRATIOS-1,
               default_t::wad_no, "Favorite aspect ratio for selection in menus"),

   DEFAULT_INT("mn_favscreentype", &mn_favscreentype, NULL, 0, 0, MN_NUMSCREENTYPES-1,
               default_t::wad_no, "Favorite screen type for selection in menus"),

   DEFAULT_BOOL("gl_use_extensions", &cfg_gl_use_extensions, NULL, false, default_t::wad_no,
                "1 to enable use of GL extensions in general"),

   DEFAULT_BOOL("gl_arb_pixelbuffer", &cfg_gl_arb_pixelbuffer, NULL, false, default_t::wad_no,
                "1 to enable use of GL ARB pixelbuffer object extension"),

   DEFAULT_INT("gl_colordepth", &cfg_gl_colordepth, NULL, 32, 16, 32, default_t::wad_no,
               "GL backend screen bitdepth (16, 24, or 32)"),

   DEFAULT_INT("gl_texture_format", &cfg_gl_texture_format, NULL, CFG_GL_RGBA8, 
               0, CFG_GL_NUMTEXFORMATS-1, default_t::wad_no, 
               "GL2D internal texture format"),

   DEFAULT_INT("gl_filter_type", &cfg_gl_filter_type, NULL, CFG_GL_LINEAR,
               0, CFG_GL_NUMFILTERS-1, default_t::wad_no, 
               "GL2D texture filtering type (0 = GL_LINEAR, 1 = GL_NEAREST)"),

   DEFAULT_BOOL("d_fastrefresh", &d_fastrefresh, NULL, false, default_t::wad_no,
                "1 to refresh as fast as possible (uses high CPU)"),

#ifdef _SDL_VER
   DEFAULT_BOOL("unicodeinput", &unicodeinput, NULL, true, default_t::wad_no,
                "1 to use SDL Unicode input mapping (0 = DOS-like behavior)"),

   DEFAULT_INT("wait_at_exit", &waitAtExit, NULL, 0, 0, 1, default_t::wad_no,
               "Always wait for input at exit"),
   
   DEFAULT_INT("grabmouse", &grabmouse, NULL, 1, 0, 1, default_t::wad_no,
               "Toggle mouse input grabbing"),

   DEFAULT_INT("audio_buffers", &audio_buffers, NULL, 2048, 1024, 8192, default_t::wad_no,
               "SDL_mixer audio buffer size"),

#endif

#if (EE_CURRENT_PLATFORM == EE_PLATFORM_WINDOWS) || defined(HAVE_SCHED_SETAFFINITY)
   DEFAULT_INT("process_affinity_mask", &process_affinity_mask, NULL, 1, 0, UL, default_t::wad_no, 
               "process affinity mask - warning: expert setting only!"),
#endif

#ifdef _MSC_VER
   DEFAULT_INT("disable_sysmenu", &disable_sysmenu, NULL, 0, 0, 1, default_t::wad_no,
               "1 to disable Windows system menu for alt+space compatibility"),
#endif

   // [CG] These are c/s defaults.

   DEFAULT_BOOL(
      "predict_shots",
      &cl_predict_shots,
      NULL,
      true,
      default_t::wad_no,
      "predict shot results"
   ),

   DEFAULT_BOOL(
      "packet_buffer",
      &cl_packet_buffer_enabled,
      NULL,
      true,
      default_t::wad_no,
      "buffer incoming packets"
   ),


   DEFAULT_INT(
      "packet_buffer_size",
      &cl_packet_buffer_size,
      NULL,
      2, 1, MAX_POSITIONS >> 1, default_t::wad_no,
      "how many TICs to buffer in the packet buffer, 0 - adaptive, other "
      "values are considered custom sizes"
   ),

   DEFAULT_BOOL(
      "debug_unlagged",
      &cl_debug_unlagged,
      NULL,
      false,
      default_t::wad_no,
      "debug unlagged"
   ),

   DEFAULT_INT(
      "damage_screen_cap",
      &damage_screen_cap,
      NULL,
      NUMREDPALS, 0, NUMREDPALS, default_t::wad_no,
      "cap the damage screen intensity, 0 - no damage screen, 8 - full, "
      "original damage screen"
   ),

   DEFAULT_INT(
      "announcer",
      &s_announcer_type,
      &s_default_announcer_type,
      S_ANNOUNCER_NONE,
      S_ANNOUNCER_NONE, 
      S_ANNOUNCER_UNREAL_TOURNAMENT,
      default_t::wad_no,
      "announcer type, 0 - none, 1 - quake, 2 - unreal tournament"
   ),

   DEFAULT_STR(
      "clientserver_demo_folder",
      &cs_demo_folder_path,
      NULL,
      "",
      default_t::wad_no,
      "folder in which to save client/server demos, defaults to base/demos"
   ),

   // [CG] End c/s defaults.

   // last entry
   { NULL, dt_integer, NULL, NULL, NULL, NULL, 0.0f, false, { 0, 0 }, default_t::wad_no,
     NULL, M_ZEROFIELDS }
};

//
// System Configuration File Object
//
static defaultfile_t sysdeffile =
{
   sysdefaults,
   sizeof(sysdefaults) / sizeof(*sysdefaults) - 1,
};

//
// M_LoadSysConfig
//
// Parses the system configuration file from the base folder.
//
void M_LoadSysConfig(const char *filename)
{
   startupmsg("M_LoadSysConfig", "Loading base/system.cfg");

   sysdeffile.fileName = estrdup(filename);

   M_LoadDefaultFile(&sysdeffile);
}

//
// M_SaveSysConfig
//
// Saves values to the system configuration file in the base folder.
//
void M_SaveSysConfig(void)
{
   M_SaveDefaultFile(&sysdeffile);
}

//
// M_ResetSysComments
//
// Resets comments in the system config file
//
void M_ResetSysComments(void)
{
   M_ResetDefaultFileComments(&sysdeffile);
}

//
// M_GetSysDefaults
//
// haleyjd 07/04/10: Needed in m_misc.c for cvar searches.
//
default_t *M_GetSysDefaults(void)
{
   return sysdefaults;
}

// EOF


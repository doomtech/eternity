// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Charles Gunyon
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
// C/S network demo routines.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

/******************************************************************************
 ** [CG] TODO                                                                **
 **      - Checkpoint location                                               **
 *****************************************************************************/

// [CG] The small amount of cURL code used to download a demo file is based on
//      the "simple.c" example at http://curl.haxx.se/libcurl/c/simple.html

#include "z_zone.h"

#include <json/json.h>
#include <curl/curl.h>

#include "c_runcmd.h"
#include "doomstat.h"
#include "doomtype.h"
#include "doomdef.h"
#include "e_lib.h"
#include "g_game.h"
#include "i_system.h"
#include "i_thread.h"
#include "m_file.h"
#include "m_misc.h"
#include "m_qstr.h"
#include "m_shots.h"
#include "m_zip.h"
#include "p_saveg.h"
#include "version.h"

#include "cs_main.h"
#include "cs_team.h"
#include "cs_ctf.h"
#include "cs_wad.h"
#include "cs_demo.h"
#include "cs_config.h"
#include "cl_buf.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "sv_main.h"

void IdentifyVersion(void);

#define TIMESTAMP_SIZE 30
#define MAX_DEMO_PATH_LENGTH 512

const char* SingleCSDemo::base_data_file_name    = "demodata.bin";
const char* SingleCSDemo::base_info_file_name    = "info.json";
const char* SingleCSDemo::base_toc_file_name     = "toc.json";
const char* SingleCSDemo::base_log_file_name     = "log.txt";
const char* SingleCSDemo::base_save_format       = "save%d.sav";
const char* SingleCSDemo::base_screenshot_format = "save%d.png";

const char* CSDemo::demo_extension      = ".ecd";
const char* CSDemo::base_info_file_name = "info.json";

static bool iwad_loaded = false;

const char *cs_demo_speed_names[cs_demo_speed_fastest + 1] = {
   "0.125x", "0.25x", "0.50x", "1x", "2x", "3x", "4x"
};

CSDemo       *cs_demo = NULL;
char         *default_cs_demo_folder_path = NULL;
char         *cs_demo_folder_path = NULL;
int           default_cs_demo_compression_level = 4;
int           cs_demo_compression_level = 4;
unsigned int  cs_demo_speed = cs_demo_speed_normal;

static uint64_t getSecondsSinceEpoch()
{
   time_t t;

   if(sizeof(time_t) > sizeof(uint64_t))
      I_Error("getSecondsSinceEpoch: Timestamp larger than 64-bits.\n");

   t = time(NULL);
   if(t == -1)
      I_Error("getSecondsSinceEpoch: Error getting current time.\n");

   return (uint64_t)t;
}

static void buildDemoTimestamps(char **demo_timestamp, char **iso_timestamp)
{
   *demo_timestamp = ecalloc(char *, sizeof(char), TIMESTAMP_SIZE);
   *iso_timestamp  = ecalloc(char *, sizeof(char), TIMESTAMP_SIZE);
#ifdef _WIN32
   SYSTEMTIME stUTC, stLocal;
   FILETIME ftUTC, ftLocal;
   ULARGE_INTEGER utcInt, localInt;
   LONGLONG delta = 0;
   LONGLONG hour_offset = 0;
   LONGLONG minute_offset = 0;
   char *s;
   bool negative = false;

   GetSystemTime(&stUTC);
   SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

   SystemTimeToFileTime(&stUTC, &ftUTC);
   SystemTimeToFileTime(&stLocal, &ftLocal);

   utcInt.LowPart = ftUTC.dwLowDateTime;
   utcInt.HighPart = ftUTC.dwHighDateTime;
   localInt.LowPart = ftLocal.dwLowDateTime;
   localInt.HighPart = ftLocal.dwHighDateTime;

   // [CG] utcInt and localInt are in 100s of nanoseconds, and there are 10
   //      million of those per second, and there are 60 seconds in a minute,
   //      so convert here.  Additionally, avoid any unsigned rollover problems
   //      inside the 64-bit arithmetic.
   if(localInt.QuadPart != utcInt.QuadPart)
   {
      if(localInt.QuadPart > utcInt.QuadPart)
      {
         delta = (localInt.QuadPart - utcInt.QuadPart) / 600000000;
         negative = false;
      }
      else if(localInt.QuadPart < utcInt.QuadPart)
      {
         delta = (utcInt.QuadPart - localInt.QuadPart) / 600000000;
         negative = true;
      }

      hour_offset = delta / 60;
      minute_offset = delta % 60;
   }

   s = *demo_timestamp;
   s += psnprintf(s, 15, "%04d%02d%02d_%02d%02d%02d",
      stLocal.wYear,
      stLocal.wMonth,
      stLocal.wDay,
      stLocal.wHour,
      stLocal.wMinute,
      stLocal.wSecond
   );

   if(negative)
      psnprintf(s, 5, "-%02d%02d", hour_offset, minute_offset);
   else
      psnprintf(s, 5, "+%02d%02d", hour_offset, minute_offset);

   s = *iso_timestamp;
   s += psnprintf(s, 19, "%04d-%02d-%02dT%02d:%02d:%02d",
      stLocal.wYear,
      stLocal.wMonth,
      stLocal.wDay,
      stLocal.wHour,
      stLocal.wMinute,
      stLocal.wSecond
   );

   if(negative)
      psnprintf(s, 5, "-%02d%02d", hour_offset, minute_offset);
   else
      psnprintf(s, 5, "+%02d%02d", hour_offset, minute_offset);
#else
   const time_t current_time = time(NULL);
   struct tm *current_tm = localtime(&current_time);

   strftime(*demo_timestamp, TIMESTAMP_SIZE, "%Y%m%d_%H%M%S%z", current_tm);
   strftime(*iso_timestamp, TIMESTAMP_SIZE, "%Y-%m-%dT%H:%M:%S%z", current_tm);
#endif
}

SingleCSDemo::SingleCSDemo(const char *new_base_path, int new_number,
                           cs_map_t *new_map, int new_type)
   : ZoneObject()
{
   qstring base_buf;
   qstring sub_buf;

   if(new_type == client_demo_type)
      demo_type = client_demo_type;
   else if(new_type == server_demo_type)
      demo_type = server_demo_type;
   else
      I_Error("SingleCSDemo: Invalid demo type %d.\n", new_type);

   // [CG] TODO: Switch between client & server in these cases.  Switching
   //            between client & server at all is a broader TODO, but this is
   //            an area users are highly likely to wander into.
   if(CS_CLIENT && demo_type == server_demo_type)
      I_Error("Cannot load server demos in c/s client mode.\n");

   if(CS_SERVER && demo_type == client_demo_type)
      I_Error("Cannot load client demos in c/s server mode.\n");

   number = new_number;
   map = new_map;

   base_buf = new_base_path;
   base_buf.Printf(5, "/%d", number);
   base_buf.normalizeSlashes();

   E_ReplaceString(base_path, estrdup(base_buf.constPtr()));

   sub_buf = base_buf;
   sub_buf.Printf(base_data_file_name_length + 1, "/%s", base_data_file_name);
   E_ReplaceString(data_path, estrdup(sub_buf.constPtr()));

   sub_buf = base_buf;
   sub_buf.Printf(base_info_file_name_length + 1, "/%s", base_info_file_name);
   E_ReplaceString(info_path, estrdup(sub_buf.constPtr()));

   sub_buf = base_buf;
   sub_buf.Printf(base_toc_file_name_length + 1, "/%s", base_toc_file_name);
   E_ReplaceString(toc_path, estrdup(sub_buf.constPtr()));

   sub_buf = base_buf;
   sub_buf.Printf(base_log_file_name_length + 1, "/%s", base_log_file_name);
   E_ReplaceString(log_path, estrdup(sub_buf.constPtr()));

   sub_buf = base_buf;
   sub_buf.Printf(base_save_format_length + 1, "/%s", base_save_format);
   E_ReplaceString(save_path_format, estrdup(sub_buf.constPtr()));

   sub_buf = base_buf;
   sub_buf.Printf(
      base_screenshot_format_length + 1, "/%s", base_screenshot_format
   );
   E_ReplaceString(screenshot_path_format, estrdup(sub_buf.constPtr()));

   demo_data_handle = NULL;
   mode = mode_none;
   internal_error = no_error;

   packet_buffer_size = 0;
   packet_buffer = 0;
}

SingleCSDemo::~SingleCSDemo()
{
   if(base_path)
      efree(base_path);

   if(toc_path)
      efree(toc_path);

   if(log_path)
      efree(log_path);

   if(data_path)
      efree(data_path);

   if(save_path_format)
      efree(save_path_format);

   if(screenshot_path_format)
      efree(screenshot_path_format);

   if(demo_timestamp)
      efree(demo_timestamp);

   if(iso_timestamp)
      efree(iso_timestamp);
}

void SingleCSDemo::setError(int error_code)
{
   internal_error = error_code;
}

bool SingleCSDemo::writeToDemo(void *data, size_t size, size_t count)
{
   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!demo_data_handle)
   {
      setError(not_open);
      return false;
   }

   if(M_WriteToFile(data, size, count, demo_data_handle) != count)
   {
      setError(fs_error);
      return false;
   }

   return true;
}

bool SingleCSDemo::readFromDemo(void *buffer, size_t size, size_t count)
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!demo_data_handle)
   {
      setError(not_open);
      return false;
   }

   if(M_ReadFromFile(buffer, size, count, demo_data_handle) != count)
   {
      setError(fs_error);
      return false;
   }

   return true;
}

bool SingleCSDemo::writeHeader()
{
   unsigned int i;
   uint32_t resource_name_size;
   clientserver_settings_t *settings = cs_settings;
   uint8_t eoh = end_of_header_packet;

   header.version = version;
   header.subversion = subversion;
   header.cs_protocol_version = cs_protocol_version;
   header.demo_type = demo_type;
   memcpy(&header.settings, settings, sizeof(clientserver_settings_t));
   header.local_options.player_bobbing = player_bobbing;
   header.local_options.doom_weapon_toggles = doom_weapon_toggles;
   header.local_options.autoaim = autoaim;
   header.local_options.weapon_speed = weapon_speed;
   header.timestamp = getSecondsSinceEpoch();
   header.length = 0;
   strncpy(header.map_name, map->name, 9);
   header.resource_count = map->resource_count + 1;

   if(!writeToDemo(&header, sizeof(demo_header_t), 1))
      return false;

   resource_name_size = (uint32_t)(strlen(cs_resources[0].name) + 1);
   if(!writeToDemo(&resource_name_size, sizeof(uint32_t), 1))
      return false;
   if(!writeToDemo(cs_resources[0].name, sizeof(char), resource_name_size))
      return false;
   if(!writeToDemo(&cs_resources[0].type, sizeof(resource_type_t), 1))
      return false;
   if(!writeToDemo(&cs_resources[0].sha1_hash, sizeof(char), 41))
      return false;

   for(i = 0; i < map->resource_count; i++)
   {
      cs_resource_t *res = &cs_resources[map->resource_indices[i]];
      resource_name_size = (uint32_t)(strlen(res->name) + 1);

      if(!writeToDemo(&resource_name_size, sizeof(uint32_t), 1))
         return false;
      if(!writeToDemo(res->name, sizeof(char), resource_name_size))
         return false;
      if(!writeToDemo(&res->type, sizeof(resource_type_t), 1))
         return false;
      if(!writeToDemo(res->sha1_hash, sizeof(char), 41))
         return false;
   }

   if(!writeToDemo(&eoh, sizeof(uint8_t), 1))
      return false;

   if(!M_FlushFile(demo_data_handle))
   {
      setError(fs_error);
      return false;
   }

   return true;
}

bool SingleCSDemo::writeInfo()
{
   unsigned int i;
   Json::Value map_info;
   clientserver_settings_t *settings = cs_settings;

   CS_ReadJSON(map_info, info_path);

   map_info["name"] = map->name;
   map_info["timestamp"] = iso_timestamp;
   map_info["length"] = header.length;
   map_info["iwad"] = cs_resources[0].name;
   map_info["local_options"]["player_bobbing"] = player_bobbing;
   map_info["local_options"]["doom_weapon_toggles"] = doom_weapon_toggles;
   map_info["local_options"]["autoaim"] = autoaim;
   map_info["local_options"]["weapon_speed"] = weapon_speed;
   map_info["settings"]["skill"] = settings->skill;
   map_info["settings"]["game_type"] = settings->game_type;
   map_info["settings"]["ctf"] = settings->ctf;
   map_info["settings"]["max_clients"] = settings->max_clients;
   map_info["settings"]["max_players"] = settings->max_players;
   map_info["settings"]["max_players_per_team"] =
      settings->max_players_per_team;
   map_info["settings"]["frag_limit"] = settings->frag_limit;
   map_info["settings"]["time_limit"] = settings->time_limit;
   map_info["settings"]["score_limit"] = settings->score_limit;
   map_info["settings"]["dogs"] = settings->dogs;
   map_info["settings"]["friend_distance"] = settings->friend_distance;
   map_info["settings"]["bfg_type"] = settings->bfg_type;
   map_info["settings"]["friendly_damage_percentage"] =
      settings->friendly_damage_percentage;
   map_info["settings"]["spectator_time_limit"] =
      settings->spectator_time_limit;
   map_info["settings"]["death_time_limit"] = settings->death_time_limit;

   if(settings->death_time_expired_action == DEATH_LIMIT_SPECTATE)
      map_info["settings"]["death_time_expired_action"] = "spectate";
   else
      map_info["settings"]["death_time_expired_action"] = "respawn";

   map_info["settings"]["respawn_protection_time"] =
      settings->respawn_protection_time;
   map_info["settings"]["dmflags"] = settings->dmflags;
   map_info["settings"]["dmflags2"] = settings->dmflags2;
   map_info["settings"]["compatflags"] = settings->compatflags;
   map_info["settings"]["compatflags2"] = settings->compatflags2;

   for(i = 0; i < map->resource_count; i++)
   {
      cs_resource_t *res = &cs_resources[map->resource_indices[i]];

      map_info["resources"][i]["name"] = res->name;
      map_info["resources"][i]["sha1_hash"] = res->sha1_hash;
   }

   CS_WriteJSON(info_path, map_info, true);

   return true;
}

bool SingleCSDemo::updateInfo()
{
   Json::Value map_info;
   long current_demo_handle_position = M_GetFilePosition(demo_data_handle);

   if(current_demo_handle_position == -1)
   {
      setError(fs_error);
      return false;
   }

   if(!M_SeekFile(demo_data_handle, 0, SEEK_SET))
   {
      setError(fs_error);
      return false;
   }

   if(!writeToDemo(&header, sizeof(demo_header_t), 1))
      return false;

   if(!M_SeekFile(demo_data_handle, current_demo_handle_position, SEEK_SET))
   {
      setError(fs_error);
      return false;
   }

   CS_ReadJSON(map_info, info_path);

   map_info["length"] = header.length;
   map_info["settings"]["skill"] = cs_settings->skill;
   map_info["settings"]["game_type"] = cs_settings->game_type;
   map_info["settings"]["ctf"] = cs_settings->ctf;
   map_info["settings"]["max_clients"] = cs_settings->max_clients;
   map_info["settings"]["max_players"] = cs_settings->max_players;
   map_info["settings"]["max_players_per_team"] =
      cs_settings->max_players_per_team;
   map_info["settings"]["frag_limit"] = cs_settings->frag_limit;
   map_info["settings"]["time_limit"] = cs_settings->time_limit;
   map_info["settings"]["score_limit"] = cs_settings->score_limit;
   map_info["settings"]["dogs"] = cs_settings->dogs;
   map_info["settings"]["friend_distance"] = cs_settings->friend_distance;
   map_info["settings"]["bfg_type"] = cs_settings->bfg_type;
   map_info["settings"]["friendly_damage_percentage"] =
      cs_settings->friendly_damage_percentage;
   map_info["settings"]["spectator_time_limit"] =
      cs_settings->spectator_time_limit;
   map_info["settings"]["death_time_limit"] = cs_settings->death_time_limit;

   if(cs_settings->death_time_expired_action == DEATH_LIMIT_SPECTATE)
      map_info["settings"]["death_time_expired_action"] = "spectate";
   else
      map_info["settings"]["death_time_expired_action"] = "respawn";

   map_info["settings"]["respawn_protection_time"] =
      cs_settings->respawn_protection_time;
   map_info["settings"]["dmflags"] = cs_settings->dmflags;
   map_info["settings"]["dmflags2"] = cs_settings->dmflags2;
   map_info["settings"]["compatflags"] = cs_settings->compatflags;
   map_info["settings"]["compatflags2"] = cs_settings->compatflags2;

   CS_WriteJSON(info_path, map_info, true);

   return true;
}

bool SingleCSDemo::readHeader()
{
   unsigned int resource_index = 0;
   cs_resource_t resource;
   cs_resource_t *stored_resource;
   uint32_t resource_name_size;
   unsigned int resource_count = 0;
   unsigned int *resource_indices = NULL;
   uint8_t header_marker;

   if(!readFromDemo(&header, sizeof(demo_header_t), 1))
      return false;

   if(header.demo_type == client_demo)
      demo_type = client_demo;
   else if(header.demo_type == server_demo)
      demo_type = server_demo;
   else
      I_Error("Unknown demo type, demo likely corrupt.\n");

   memcpy(cs_settings, &header.settings, sizeof(clientserver_settings_t));
   memcpy(cs_original_settings, cs_settings, sizeof(clientserver_settings_t));

   player_bobbing      = header.local_options.player_bobbing;
   doom_weapon_toggles = header.local_options.doom_weapon_toggles;
   autoaim             = header.local_options.autoaim;
   weapon_speed        = header.local_options.weapon_speed;

   if(!map->initialized)
   {
      resource.name = NULL;

      while(resource_index++ < header.resource_count)
      {
         if(!readFromDemo(&resource_name_size, sizeof(uint32_t), 1))
            return false;

         resource.name = erealloc(char *, resource.name, resource_name_size);

         if(!readFromDemo(resource.name, sizeof(char), resource_name_size))
            return false;

         if(!readFromDemo(&resource.type, sizeof(resource_type_t), 1))
            return false;

         if(!readFromDemo(&resource.sha1_hash, sizeof(char), 41))
            return false;

         printf("load_current_map_demo: Loading resources.\n");
         switch(resource.type)
         {
         case rt_iwad:
            printf("load_current_map_demo: Found an IWAD resource.\n");
            if(!iwad_loaded)
            {
               if(!CS_AddIWAD(resource.name))
               {
                  I_Error(
                     "Error: Could not find IWAD %s, exiting.\n", resource.name
                  );
               }
               printf("load_current_map_demo: Loading IWAD.\n");
               // [CG] Loads the IWAD and attendant GameModeInfo.
               IdentifyVersion();
            }
            else if(strncmp(cs_resources[0].name,
                            resource.name,
                            strlen(resource.name)))
            {
               I_Error(
                  "Cannot change IWADs during a demo (%s => %s), exiting.\n",
                  cs_resources[0].name,
                  resource.name
               );
            }
            else
               printf("load_current_map_demo: Skipping IWAD.\n");

            if(!CS_CheckResourceHash(resource.name, resource.sha1_hash))
            {
               I_Error(
                  "SHA1 hash for %s did not match, exiting.\n", resource.name
               );
            }
            break;
         case rt_deh:
            if(!iwad_loaded)
            {
               if(!CS_AddDeHackEdFile(resource.name))
               {
                  I_Error(
                     "Could not find DeHackEd patch %s, exiting.\n",
                     resource.name
                  );
               }
            }
            else
            {
               stored_resource = CS_GetResource(resource.name);
               if(stored_resource == NULL)
               {
                  I_Error(
                     "Cannot add new DeHackEd patches during a demo (%s), "
                     "exiting.\n",
                     resource.name
                  );
               }
            }
            if(!CS_CheckResourceHash(resource.name, resource.sha1_hash))
            {
               I_Error(
                  "SHA1 hash for %s did not match, exiting.\n", resource.name
               );
            }
            break;
         case rt_pwad:
            resource_count++;
            if((stored_resource = CS_GetResource(resource.name)) == NULL)
            {
               if(!CS_AddWAD(resource.name))
                  I_Error("Could not find PWAD %s, exiting.\n", resource.name);

               stored_resource = CS_GetResource(resource.name);
            }
            resource_indices = erealloc(
               unsigned int *,
               resource_indices,
               resource_count * sizeof(unsigned int)
            );
            resource_indices[resource_count - 1] =
               stored_resource - cs_resources;
            if(!CS_CheckResourceHash(resource.name, resource.sha1_hash))
            {
               I_Error(
                  "SHA1 hash for %s did not match, exiting.\n", resource.name
               );
            }
            break;
         default:
            I_Error(
               "Demo contains invalid resource type and is likely corrupt.\n"
            );
            break;
         }
      }
      CS_AddMapAtIndex(
         header.map_name, resource_count, resource_indices, map - cs_maps
      );
   }
   else
      printf("Map %ld was already initialized.\n", map - cs_maps);

   if(!readFromDemo(&header_marker, sizeof(uint8_t), 1))
      return false;

   // [CG] FIXME: Doesn't deserve an I_Error here, should doom_printf and set
   //             console.
   if(header_marker != end_of_header_packet)
      I_Error("Malformed demo header, demo likely corrupt.\n");

   iwad_loaded = true;

   printf("load_current_map_demo: loaded demo for %ld.\n", map - cs_maps);

   return true;
}

bool SingleCSDemo::handleNetworkMessageDemoPacket()
{
   int32_t player_number;
   uint32_t message_size;

   if(!readFromDemo(&player_number, sizeof(int32_t), 1))
      return false;
   if(!readFromDemo(&message_size, sizeof(uint32_t), 1))
      return false;

   if((!message_size) || message_size > 1000000)
   {
      setError(invalid_network_message_size);
      return false;
   }

   if(packet_buffer_size < message_size)
   {
      packet_buffer_size = message_size;
      packet_buffer = erealloc(char *, packet_buffer, packet_buffer_size);
   }

   if(!readFromDemo(packet_buffer, sizeof(char), message_size))
      return false;

   if(demo_type == client_demo_type)
      cl_packet_buffer.add(packet_buffer, message_size);
   else if(demo_type == server_demo_type)
      SV_HandleMessage(packet_buffer, message_size, player_number);

   return true;
}

bool SingleCSDemo::handlePlayerCommandDemoPacket()
{
   cs_cmd_t command;
   uint32_t command_size;

   if(!readFromDemo(&command_size, sizeof(uint32_t), 1))
      return false;

   if(!readFromDemo(&command, command_size, 1))
      return false;

   // [CG] This is always a command for consoleplayer.  Anything else is
   //      handled by handleNetworkMessageDemoPacket (clients don't receive
   //      commands from other players anyway).

   CS_CopyCommandToTiccmd(&players[consoleplayer].cmd, &command);

   if(demo_type == client_demo_type)
   {
      cs_cmd_t *dest_command = CL_GetCommandAtIndex(cl_commands_sent);

      memcpy(dest_command, &command, sizeof(cs_cmd_t));
      cl_commands_sent = command.index + 1;
   }

   if(demo_type == server_demo_type)
      CS_CopyCommandToTiccmd(&players[consoleplayer].cmd, &command);

   return true;
}

bool SingleCSDemo::handleConsoleCommandDemoPacket()
{
   int32_t type, src;
   uint32_t command_size, options_size;
   char *options;
   command_t *command;

   if(!readFromDemo(&type, sizeof(int32_t), 1))
      return false;

   if(!readFromDemo(&src, sizeof(int32_t), 1))
      return false;

   if(!readFromDemo(&command_size, sizeof(uint32_t), 1))
      return false;

   if(packet_buffer_size < command_size)
      packet_buffer = erealloc(char *, packet_buffer, command_size);

   if(!readFromDemo(packet_buffer, sizeof(char), command_size))
      return false;

   if(!(command = C_GetCmdForName((const char *)packet_buffer)))
   {
      doom_printf("unknown command: '%s'", packet_buffer);
      return false;
   }

   if(!readFromDemo(&options_size, sizeof(uint32_t), 1))
      return false;

   options = ecalloc(char *, options_size, sizeof(char));

   if(!readFromDemo(options, sizeof(char), options_size))
      return false;

   C_BufferCommand(type, command, (const char *)options, src);

   return true;
}


int SingleCSDemo::getNumber() const
{
   return number;
}

bool SingleCSDemo::openForPlayback()
{
   if(mode != mode_none)
   {
      setError(already_open);
      return false;
   }

   if(demo_data_handle)
   {
      setError(already_open);
      return false;
   }

   demo_data_handle = M_OpenFile(data_path, "rb");

   if(!readHeader())
      return false;

   mode = mode_playback;
   return true;
}

bool SingleCSDemo::openForRecording()
{
   if(mode != mode_none)
   {
      setError(already_open);
      return false;
   }

   if(demo_data_handle)
   {
      setError(already_open);
      return false;
   }

   if(M_PathExists(data_path))
   {
      setError(already_exists);
      return false;
   }

   if(!(demo_data_handle = M_OpenFile(data_path, "wb")))
   {
      setError(fs_error);
      return false;
   }

   if(!writeHeader())
      return false;

   if(!writeInfo())
      return false;

   if(!updateInfo())
      return false;

   mode = mode_recording;

   return true;
}

bool SingleCSDemo::write(void *message, uint32_t size, int32_t playernum)
{
   uint8_t demo_marker = network_message_packet;
   int32_t player_number;
   
   if(CS_CLIENT)
      player_number = 0;
   else
      player_number = playernum;

   if(!writeToDemo(&demo_marker, sizeof(int32_t), 1))
      return false;
   if(!writeToDemo(&player_number, sizeof(int32_t), 1))
      return false;
   if(!writeToDemo(&size, sizeof(uint32_t), 1))
      return false;
   if(!writeToDemo(message, sizeof(char), size))
      return false;

   return true;
}

bool SingleCSDemo::write(cs_cmd_t *command)
{
   uint8_t demo_marker = player_command_packet;
   uint32_t command_size = (uint32_t)(sizeof(cs_cmd_t));
   
   if(!writeToDemo(&demo_marker, sizeof(int32_t), 1))
      return false;
   if(!writeToDemo(&command_size, sizeof(uint32_t), 1))
      return false;
   if(!writeToDemo(command, sizeof(cs_cmd_t), 1))
      return false;

   return true;
}

bool SingleCSDemo::write(command_t *cmd, int type, const char *opts, int src)
{
   uint8_t demo_marker = console_command_packet;
   uint32_t command_size = (uint32_t)(strlen(cmd->name) + 1);
   uint32_t options_size = (uint32_t)(strlen(opts) + 1);

   if(!writeToDemo(&demo_marker, sizeof(int32_t), 1))
      return false;
   if(!writeToDemo(&type, sizeof(int32_t), 1))
      return false;
   if(!writeToDemo(&src, sizeof(int32_t), 1))
      return false;
   if(!writeToDemo(&command_size, sizeof(uint32_t), 1))
      return false;
   if(!writeToDemo((void *)cmd->name, sizeof(char), command_size))
      return false;
   if(!writeToDemo(&options_size, sizeof(uint32_t), 1))
      return false;
   if(!writeToDemo((void *)opts, sizeof(char), options_size))
      return false;

   return true;
}

bool SingleCSDemo::saveCheckpoint()
{
   Json::Value toc;
   Json::Value checkpoint = Json::Value(Json::objectValue);
   qstring buf, checkpoint_name;
   uint32_t index;
   long byte_index;

   if(CS_CLIENT)
      index = cl_current_world_index;
   else
      index = sv_world_index;

   buf.Printf(
      strlen(screenshot_path_format) + MAX_CS_DEMO_CHECKPOINT_INDEX_SIZE,
      screenshot_path_format,
      index
   );
   if(!M_SaveScreenShotAs(buf.constPtr()))
   {
      setError(fs_error);
      return false;
   }

   buf.clear().Printf(
      strlen(save_path_format) + MAX_CS_DEMO_CHECKPOINT_INDEX_SIZE,
      save_path_format,
      index
   );
   checkpoint_name.Printf(
      11 + MAX_CS_DEMO_CHECKPOINT_INDEX_SIZE, "Checkpoint %d", index
   );
   P_SaveCurrentLevel(buf.getBuffer(), checkpoint_name.getBuffer());

   if((byte_index = M_GetFilePosition(demo_data_handle)) == -1)
   {
      setError(fs_error);
      return false;
   }

   if(M_PathExists(toc_path))
   {
      if(!M_IsFile(toc_path))
      {
         setError(toc_is_not_file);
         return false;
      }
      CS_ReadJSON(toc, toc_path);
   }
   else
      toc = Json::Value(Json::objectValue);

   checkpoint["byte_index"] = (uint32_t)(byte_index);
   checkpoint["index"] = index;
   buf.clear().Printf(base_save_format_length, base_save_format, index);
   checkpoint["data_file"] = buf.constPtr();
   buf.clear().Printf(
      base_screenshot_format_length, base_screenshot_format, index
   );
   checkpoint["screenshot_file"] = buf.constPtr();
   toc["checkpoints"].append(checkpoint);
   CS_WriteJSON(toc_path, toc, true);

   return true;
}

bool SingleCSDemo::loadCheckpoint(int checkpoint_index, uint32_t byte_index)
{
   qstring buf;

   buf.Printf(
      strlen(save_path_format) + MAX_CS_DEMO_CHECKPOINT_INDEX_SIZE,
      save_path_format,
      checkpoint_index
   );

   if(!M_PathExists(buf.constPtr()))
   {
      setError(checkpoint_save_does_not_exist);
      return false;
   }

   if(!M_IsFile(buf.constPtr()))
   {
      setError(checkpoint_save_is_not_file);
      return false;
   }

   if(!M_SeekFile(demo_data_handle, byte_index, SEEK_SET))
   {
      setError(fs_error);
      return false;
   }

   P_LoadGame(buf.constPtr());

   return true;
}

bool SingleCSDemo::loadCheckpoint(int checkpoint_index)
{
   Json::Value toc;
   uint32_t byte_index;

   if(!M_PathExists(toc_path))
   {
      setError(toc_does_not_exist);
      return false;
   }

   if(!M_IsFile(toc_path))
   {
      setError(toc_is_not_file);
      return false;
   }

   CS_ReadJSON(toc, toc_path);

   if(toc["checkpoints"].empty())
   {
      setError(no_checkpoints);
      return false;
   }
   
   if(toc["checkpoints"][checkpoint_index].empty())
   {
      setError(checkpoint_index_not_found);
      return false;
   }

   byte_index = toc["checkpoints"][checkpoint_index]["byte_index"].asUInt();

   return loadCheckpoint(checkpoint_index, byte_index);
}

bool SingleCSDemo::loadCheckpointBefore(uint32_t index)
{
   Json::Value toc;
   int i;
   uint32_t checkpoint_index, best_index, best_byte_index;

   if(!M_PathExists(toc_path))
   {
      setError(toc_does_not_exist);
      return false;
   }

   if(!M_IsFile(toc_path))
   {
      setError(toc_is_not_file);
      return false;
   }

   CS_ReadJSON(toc, toc_path);

   if(toc["checkpoints"].empty())
   {
      setError(no_checkpoints);
      return false;
   }

   index = best_index = best_byte_index = 0;
   for(i = 0; toc["checkpoints"].isValidIndex(i); i++)
   {
      if(toc["checkpoints"][i].empty()                    ||
         toc["checkpoints"][i]["byte_index"].empty()      ||
         (!toc["checkpoints"][i]["byte_index"].isInt())   ||
         toc["checkpoints"][i]["index"].empty()           ||
         (!toc["checkpoints"][i]["index"].isInt())        ||
         toc["checkpoints"][i]["data_file"].empty()       ||
         (!toc["checkpoints"][i]["data_file"].isString()) ||
         toc["checkpoints"][i]["screenshot_file"].empty() ||
         (!toc["checkpoints"][i]["screenshot_file"].isString()))
      {
         setError(checkpoint_toc_corrupt);
         return false;
      }
      checkpoint_index = toc["checkpoints"][i]["index"].asUInt();

      if(checkpoint_index < index)
      {
         if((best_index == 0) || (index > best_index))
         {
            best_index = index;
            best_byte_index = toc["checkpoints"][i]["byte_index"].asUInt();
         }
      }
   }

   if(best_index == 0)
   {
      setError(no_previous_checkpoint);
      return false;
   }

   return loadCheckpoint(best_index, best_byte_index);
}

bool SingleCSDemo::loadPreviousCheckpoint()
{
   uint32_t current_index;

   if(CS_CLIENT)
      current_index = cl_current_world_index;
   else
      current_index = sv_world_index;

   return loadCheckpointBefore(current_index);
}

bool SingleCSDemo::loadNextCheckpoint()
{
   Json::Value toc;
   int i;
   uint32_t index, current_index, best_index, best_byte_index;

   if(!M_PathExists(toc_path))
   {
      setError(toc_does_not_exist);
      return false;
   }

   if(!M_IsFile(toc_path))
   {
      setError(toc_is_not_file);
      return false;
   }

   CS_ReadJSON(toc, toc_path);

   if(toc["checkpoints"].empty())
   {
      setError(no_checkpoints);
      return false;
   }

   if(CS_CLIENT)
      current_index = cl_current_world_index;
   else
      current_index = sv_world_index;

   for(i = 0, best_index = 0; toc["checkpoints"].isValidIndex(i); i++)
   {
      if(toc["checkpoints"][i].empty()                    ||
         toc["checkpoints"][i]["byte_index"].empty()      ||
         (!toc["checkpoints"][i]["byte_index"].isInt())   ||
         toc["checkpoints"][i]["index"].empty()           ||
         (!toc["checkpoints"][i]["index"].isInt())        ||
         toc["checkpoints"][i]["data_file"].empty()       ||
         (!toc["checkpoints"][i]["data_file"].isString()) ||
         toc["checkpoints"][i]["screenshot_file"].empty() ||
         (!toc["checkpoints"][i]["screenshot_file"].isString()))
      {
         setError(checkpoint_toc_corrupt);
         return false;
      }
      index = toc["checkpoints"][i]["index"].asUInt();

      if(index > current_index)
      {
         if((best_index == 0) || (index < best_index))
         {
            best_index = index;
            best_byte_index = toc["checkpoints"][i]["byte_index"].asUInt();
         }
      }
   }

   if(best_index == 0)
   {
      setError(no_subsequent_checkpoint);
      return false;
   }

   return loadCheckpoint(best_index, best_byte_index);
}

bool SingleCSDemo::readPacket()
{
   uint8_t packet_type;

   if(!readFromDemo(&packet_type, sizeof(uint8_t), 1))
      return false;

   if(packet_type == network_message_packet)
      return handleNetworkMessageDemoPacket();
   if(packet_type == player_command_packet)
      return handlePlayerCommandDemoPacket();
   if(packet_type == console_command_packet)
      return handleConsoleCommandDemoPacket();

   setError(unknown_demo_packet_type);
   return false;
}

bool SingleCSDemo::isFinished()
{
   if(!demo_data_handle)
      return true;

   if(feof(demo_data_handle))
      return true;

   return false;
}

bool SingleCSDemo::close()
{
   if(mode == mode_recording)
   {
      header.length = cl_current_world_index;
      updateInfo();
   }

   if(!M_CloseFile(demo_data_handle))
   {
      setError(fs_error);
      return false;
   }

   return true;
}

bool SingleCSDemo::hasError()
{
   if(internal_error != no_error)
      return true;

   return false;
}

void SingleCSDemo::clearError()
{
   internal_error = no_error;
}

const char* SingleCSDemo::getError()
{
   if(internal_error == fs_error)
      return M_GetFileSystemErrorMessage();

   if(internal_error == not_open_for_playback)
      return "Demo is not open for playback";

   if(internal_error == not_open_for_recording)
      return "Demo is not open for recording";

   if(internal_error == not_open)
      return "Demo is not open";

   if(internal_error == already_open)
      return "Demo is already open";

   if(internal_error == already_exists)
      return "Demo already exists";

   if(internal_error == demo_data_is_not_file)
      return "Demo data file is not a file";

   if(internal_error == unknown_demo_packet_type)
      return "unknown demo packet type";

   if(internal_error == invalid_network_message_size)
      return "invalid network message size";

   if(internal_error == toc_is_not_file)
      return "table of contents file is not a file";

   if(internal_error == toc_does_not_exist)
      return "table of contents file not found";

   if(internal_error == checkpoint_save_does_not_exist)
      return "checkpoint data file not found";

   if(internal_error == checkpoint_save_is_not_file)
      return "checkpoint data file is not a file";

   if(internal_error == no_checkpoints)
      return "no checkpoints saved";

   if(internal_error == checkpoint_index_not_found)
      return "checkpoint index not found";

   if(internal_error == checkpoint_toc_corrupt)
      return "checkpoint table-of-contents file corrupt";

   if(internal_error == no_previous_checkpoint)
      return "no previous checkpoint";

   if(internal_error == no_subsequent_checkpoint)
      return "no subsequent checkpoint";

   return "no_error";
}

CSDemo::CSDemo()
   : ZoneObject(), mode(0), internal_error(no_error), internal_curl_error(0),
                   current_demo_index(0), demo_count(0), folder_path(NULL)
{
   if((!default_cs_demo_folder_path) || (!strlen(default_cs_demo_folder_path)))
      default_cs_demo_folder_path = M_PathJoin(userpath, "demos");
   setFolderPath(default_cs_demo_folder_path);
}

CSDemo::~CSDemo()
{
   if(folder_path)
      efree(folder_path);
}

bool CSDemo::loadZipFile()
{
   // [CG] Eat errors here.
   if(current_zip_file)
   {
      if(!current_zip_file->close())
      {
         setError(zip_error);
         return false;
      }

      delete current_zip_file;
   }

   current_zip_file = new ZipFile(current_demo_archive_path);

   return true;
}

bool CSDemo::retrieveDemo(const char *url)
{
   CURL *curl_handle;
   const char *basename;
   CURLcode res;
   FILE *fobj;
   qstring buf;

   if(!CS_CheckURI(url))
   {
      setError(invalid_url);
      return false;
   }

   if(!(basename = M_Basename(url)))
   {
      setError(fs_error);
      return false;
   }

   if(strlen(basename) < 2)
   {
      setError(invalid_url);
      return false;
   }

   buf.Printf(
      strlen(folder_path) + strlen(basename) + 5,
      "%s/%s",
      folder_path,
      basename
   );

   // [CG] If the file's already been downloaded, skip all the hard work below.
   if(M_IsFile(buf.constPtr()))
      return true;

   if(!M_CreateFile(buf.constPtr()))
   {
      setError(fs_error);
      return false;
   }

   if(!(fobj = M_OpenFile(buf.constPtr(), "wb")))
   {
      setError(fs_error);
      return false;
   }

   if(!(curl_handle = curl_easy_init()))
   {
      M_CloseFile(fobj);
      setCURLError((long)curl_handle);
      return false;
   }

   curl_easy_setopt(curl_handle, CURLOPT_URL, url);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, fwrite);
   curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fobj);

   if((res = curl_easy_perform(curl_handle)) != 0)
   {
      M_CloseFile(fobj);
      setCURLError(res);
      return false;
   }

   M_CloseFile(fobj);

   curl_easy_cleanup(curl_handle);

   E_ReplaceString(current_demo_archive_path, estrdup(buf.constPtr()));

   return true;
}

void CSDemo::setError(int error_code)
{
   internal_error = error_code;
}

void CSDemo::setCURLError(long error_code)
{
   internal_error = curl_error;
   internal_curl_error = error_code;
}

int CSDemo::getCurrentDemoIndex() const
{
   return current_demo_index;
}

bool CSDemo::setFolderPath(const char *new_folder_path)
{
   if(!M_PathExists(new_folder_path))
   {
      setError(folder_does_not_exist);
      return false;
   }

   if(!M_IsFolder(new_folder_path))
   {
      setError(folder_is_not_folder);
      return false;
   }

   E_ReplaceString(folder_path, estrdup(new_folder_path));
   return true;
}

bool CSDemo::record()
{
   qstring buf;
   int demo_type;
   Json::Value info;

   if(mode == mode_playback)
   {
      setError(already_playing);
      return false;
   }

   if(mode == mode_recording)
   {
      setError(already_recording);
      return false;
   }

   if(!(strncmp(folder_path, cs_demo_folder_path, MAX_DEMO_PATH_LENGTH)))
      setFolderPath(cs_demo_folder_path);

   // [CG] Build demo timestamps.  The internal format is ISO 8601 format, but
   //      because Windows won't allow colons in filenames (they are for drive-
   //      specification only) we use a custom format for the folder/filename.

   if(demo_timestamp)
      efree(demo_timestamp);

   if(iso_timestamp)
      efree(iso_timestamp);

   buildDemoTimestamps(&demo_timestamp, &iso_timestamp);

   buf.Printf(MAX_DEMO_PATH_LENGTH, "%s/%s", folder_path, demo_timestamp);
   buf.normalizeSlashes();

   info["version"] = version;
   info["subversion"] = subversion;
   info["cs_protocol_version"] = cs_protocol_version;
   info["name"] = "";
   info["author"] = "";
   info["date_recorded"] = iso_timestamp;

   if(M_PathExists(buf.constPtr()))
   {
      setError(already_exists);
      return false;
   }

   if(!M_CreateFolder(buf.constPtr()))
   {
      setError(fs_error);
      return false;
   }

   if(CS_CLIENT)
      demo_type = SingleCSDemo::client_demo_type;
   else
      demo_type = SingleCSDemo::server_demo_type;

   current_demo = new SingleCSDemo(
      buf.constPtr(),
      current_demo_index,
      &cs_maps[cs_current_map_index],
      demo_type
   );

   if(current_demo->hasError())
   {
      setError(demo_error);
      M_DeleteFolder(buf.constPtr());
      return false;
   }

   if(!current_demo->openForRecording())
   {
      setError(demo_error);
      M_DeleteFolderAndContents(buf.constPtr());
      return false;
   }

   E_ReplaceString(current_demo_folder_path, estrdup(buf.constPtr()));
   buf.clear().Printf(
      MAX_DEMO_PATH_LENGTH,
      "%s/%s",
      current_demo_folder_path,
      base_info_file_name
   );
   E_ReplaceString(current_demo_info_path, estrdup(buf.constPtr()));
   buf.clear().Printf(
      MAX_DEMO_PATH_LENGTH,
      "%s%s",
      current_demo_folder_path,
      demo_extension
   );
   E_ReplaceString(current_demo_archive_path, estrdup(buf.constPtr()));

   CS_WriteJSON(current_demo_info_path, info, true);

   mode = mode_recording;

   return true;
}

bool CSDemo::addNewMap()
{
   int demo_type;
   qstring buf;

   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->close())
   {
      setError(demo_error);
      return false;
   }

   delete current_demo;

   if(CS_CLIENT)
      demo_type = SingleCSDemo::client_demo_type;
   else
      demo_type = SingleCSDemo::server_demo_type;

   current_demo = new SingleCSDemo(
      current_demo_folder_path,
      current_demo_index + 1,
      &cs_maps[cs_current_map_index],
      demo_type
   );

   buf.Printf(
      MAX_DEMO_PATH_LENGTH,
      "%s/%s",
      current_demo_folder_path,
      current_demo_index + 1
   );

   if(current_demo->hasError())
   {
      setError(demo_error);
      M_DeleteFolder(buf.constPtr());
      return false;
   }

   if(!current_demo->openForRecording())
   {
      setError(demo_error);
      M_DeleteFolderAndContents(buf.constPtr());
      return false;
   }

   current_demo_index++;

   return true;
}

bool CSDemo::play(const char *url)
{
   int demo_type;
   qstring qbuf, saved_buf, test_folder_buf, path_buf;
   Json::Value info;
   char *buf = NULL;

   if(mode == mode_playback)
   {
      setError(already_playing);
      return false;
   }

   if(mode == mode_recording)
   {
      setError(already_recording);
      return false;
   }

   if(!(strncmp(folder_path, cs_demo_folder_path, MAX_DEMO_PATH_LENGTH)))
      setFolderPath(cs_demo_folder_path);

   // [CG] If the demo's URI is a file: URI, then we don't need to download it.
   //      Otherwise it must be downloaded.

   if(strncmp(url, "file://", 7) == 0)
   {
      const char *cwd;

      if(!M_IsFile(url + 7))
      {
         setError(demo_archive_not_found);
         return false;
      }

      if(!M_IsAbsolutePath(url + 7))
      {
         if(!(cwd = M_GetCurrentFolder()))
         {
            setError(fs_error);
            return false;
         }
         path_buf.Printf(strlen(cwd) + strlen(url), "%s/%s", cwd, url + 7);
         path_buf.normalizeSlashes();
         efree((void *)cwd);
      }
      else
         path_buf = url;

      E_ReplaceString(current_demo_archive_path, estrdup(path_buf.constPtr()));
   }
   else if(!retrieveDemo(url))
      return false;

   loadZipFile();

   if(!current_zip_file->openForReading())
   {
      setError(zip_error);
      return false;
   }

   while(current_zip_file->iterateFilenames(&buf))
   {
      qbuf = buf;
      qbuf.normalizeSlashes();
      qbuf.truncate(qbuf.findFirstOf('/'));

      if(!saved_buf.getSize())
      {
         saved_buf = qbuf;
         continue;
      }

      if(saved_buf != qbuf)
      {
         setError(invalid_demo_structure);
         current_zip_file->resetFilenameIterator();
         return false;
      }
   }

   if(current_zip_file->hasError())
   {
      setError(zip_error);
      return false;
   }

   E_ReplaceString(current_demo_folder_path, estrdup(qbuf.constPtr()));

   if(!current_zip_file->extractAllTo(folder_path))
   {
      setError(zip_error);
      return false;
   }

   if(cs_maps)
      CS_ClearMaps();

   demo_count = cs_map_count = 0;

   test_folder_buf.copy(qbuf).Printf(10, "/%d", cs_map_count);
   while(M_IsFolder(test_folder_buf.constPtr()))
   {
      demo_count++;
      cs_map_count++;
      test_folder_buf.copy(qbuf).Printf(10, "/%d", cs_map_count);
   }
   cs_maps = ecalloc(cs_map_t *, cs_map_count, sizeof(cs_map_t));

   if(CS_CLIENT)
      demo_type = SingleCSDemo::client_demo_type;
   else
      demo_type = SingleCSDemo::server_demo_type;

   current_demo = new SingleCSDemo(
      current_demo_folder_path,
      current_demo_index,
      &cs_maps[current_demo_index],
      demo_type
   );

   qbuf.clear().Printf(
      MAX_DEMO_PATH_LENGTH,
      "%s/%s",
      current_demo_folder_path,
      current_demo_index
   );

   if(current_demo->hasError())
   {
      setError(demo_error);
      M_DeleteFolderAndContents(current_demo_folder_path);
      return false;
   }

   cs_settings = estructalloc(clientserver_settings_t, 1);
   cs_original_settings = estructalloc(clientserver_settings_t, 1);

   if(!current_demo->openForPlayback())
   {
      setError(demo_error);
      M_DeleteFolderAndContents(current_demo_folder_path);
      return false;
   }

   qbuf.clear().Printf(
      MAX_DEMO_PATH_LENGTH,
      "%s/%s",
      current_demo_folder_path,
      base_info_file_name
   );

   E_ReplaceString(current_demo_info_path, estrdup(qbuf.constPtr()));

   mode = mode_playback;

   return true;
}

bool CSDemo::setCurrentDemo(int new_demo_index)
{
   int demo_type;

   if((new_demo_index < 0) || (new_demo_index > (demo_count - 1)))
   {
      setError(invalid_demo_index);
      return false;
   }

   if(new_demo_index == current_demo_index)
   {
      setError(already_playing);
      return false;
   }

   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo->close())
   {
      setError(demo_error);
      return false;
   }

   delete current_demo;
   current_demo_index++;

   if(CS_CLIENT)
      demo_type = SingleCSDemo::client_demo_type;
   else
      demo_type = SingleCSDemo::server_demo_type;

   current_demo = new SingleCSDemo(
      current_demo_folder_path,
      current_demo_index,
      &cs_maps[new_demo_index],
      demo_type
   );

   if(current_demo->hasError())
   {
      setError(demo_error);
      return false;
   }

   if(!current_demo->openForPlayback())
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::playNext()
{
   if(current_demo_index >= (demo_count - 1))
   {
      setError(last_demo);
      return false;
   }

   return setCurrentDemo(current_demo_index + 1);
}

bool CSDemo::playPrevious()
{
   if(current_demo_index <= 0)
   {
      setError(first_demo);
      return false;
   }

   return setCurrentDemo(current_demo_index - 1);
}

bool CSDemo::stop()
{
   if((mode == mode_none) || (!current_demo))
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->close())
   {
      setError(demo_error);
      return false;
   }

   delete current_demo;

   return true;
}

bool CSDemo::close()
{
   if(current_demo_archive_path)
      efree(current_demo_archive_path);

   if(current_demo_folder_path)
      efree(current_demo_folder_path);

   if(current_demo_info_path)
      efree(current_demo_info_path);

   current_demo_index = 0;

   if(mode == mode_recording)
   {
      loadZipFile();

      if(!current_zip_file->createForWriting())
      {
         setError(zip_error);
         return false;
      }

      if(!current_zip_file->addFolderRecursive(current_demo_folder_path,
                                               cs_demo_compression_level))
      {
         setError(zip_error);
         return false;
      }

      if(!current_zip_file->close())
      {
         setError(zip_error);
         return false;
      }

      delete current_zip_file;
   }
   else if(mode == mode_playback)
   {
      if(!M_DeleteFolderAndContents(current_demo_folder_path))
      {
         setError(fs_error);
         return false;
      }
   }

   return true;
}

bool CSDemo::isRecording()
{
   if(mode == mode_recording)
      return true;

   return false;
}

bool CSDemo::isPlaying()
{
   if(mode == mode_playback)
      return true;

   return false;
}

bool CSDemo::write(void *message, uint32_t size, int32_t playernum)
{
   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->write(message, size, playernum))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::write(cs_cmd_t *command)
{
   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->write(command))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::write(command_t *cmd, int type, const char *opts, int src)
{
   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->write(cmd, type, opts, src))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::saveCheckpoint()
{
   if(mode != mode_recording)
   {
      setError(not_open_for_recording);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->saveCheckpoint())
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::loadCheckpoint(int checkpoint_index)
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->loadCheckpoint(checkpoint_index))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::loadCheckpointBefore(uint32_t index)
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->loadCheckpointBefore(index))
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::loadPreviousCheckpoint()
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->loadPreviousCheckpoint())
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::loadNextCheckpoint()
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->loadNextCheckpoint())
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::rewind(uint32_t tic_count)
{
   uint32_t current_index;
   unsigned int new_destination_index;
   
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(CS_CLIENT)
      current_index = cl_current_world_index;
   else
      current_index = sv_world_index;

   if(tic_count > current_index)
      new_destination_index = 0;
   else
      new_destination_index = current_index - tic_count;

   if(!loadCheckpointBefore(new_destination_index))
      return false;

   while(true)
   {
      if(!cs_demo->readPacket())
      {
         if(cs_demo->isFinished())
            return true;
         else
            return false;
      }

      if(CS_CLIENT)
      {
         if(cl_current_world_index > new_destination_index)
            break;
      }
      else if(sv_world_index > new_destination_index)
         break;
   }
   return true;
}

bool CSDemo::fastForward(uint32_t tic_count)
{
   unsigned int new_destination_index;
   
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(CS_CLIENT)
      new_destination_index = cl_current_world_index + tic_count;
   else
      new_destination_index = sv_world_index + tic_count;

   while(true)
   {
      if(!cs_demo->readPacket())
      {
         if(cs_demo->isFinished())
            return true;
         else
            return false;
      }

      if(CS_CLIENT)
      {
         if(cl_current_world_index > new_destination_index)
            break;
      }
      else if(sv_world_index > new_destination_index)
         break;
   }
   return true;
}

bool CSDemo::readPacket()
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   if(!current_demo->readPacket())
   {
      setError(demo_error);
      return false;
   }

   return true;
}

bool CSDemo::isFinished()
{
   if(mode != mode_playback)
   {
      setError(not_open_for_playback);
      return false;
   }

   if(!current_demo)
   {
      setError(not_open);
      return false;
   }

   return current_demo->isFinished();
}

const char* CSDemo::getBasename()
{
   return M_Basename(current_demo_archive_path);
}

const char* CSDemo::getError()
{
   if(internal_error == fs_error)
      return M_GetFileSystemErrorMessage();

   if(internal_error == zip_error)
   {
      if(current_zip_file && current_zip_file->hasError())
         return current_zip_file->getError();
   }

   if(internal_error == demo_error)
   {
      if(current_demo && current_demo->hasError())
         return current_demo->getError();
   }

   if(internal_error == curl_error)
      return curl_easy_strerror((CURLcode)internal_curl_error);

   if(internal_error == folder_does_not_exist)
      return "demo folder does not exist";

   if(internal_error == folder_is_not_folder)
      return "demo folder is not a folder";

   if(internal_error == not_open_for_playback)
      return "demo is not open for playback";

   if(internal_error == not_open_for_recording)
      return "demo is not open for recording";

   if(internal_error == already_playing)
      return "demo is already playing";

   if(internal_error == already_recording)
      return "demo is already recording";

   if(internal_error == already_exists)
      return "demo already exists";

   if(internal_error == not_open)
      return "demo is not open";

   if(internal_error == invalid_demo_structure)
      return "invalid demo structure";

   if(internal_error == demo_archive_not_found)
      return "demo not found";

   if(internal_error == invalid_demo_index)
      return "invalid demo index";

   if(internal_error == first_demo)
      return "already at the first demo";

   if(internal_error == last_demo)
      return "already at the last demo";

   if(internal_error == invalid_url)
      return "invalid url";

   return "no_error";
}

void CS_NewDemo()
{
   if(cs_demo)
   {
      if(cs_demo->isPlaying() || cs_demo->isRecording())
      {
         if(!cs_demo->stop())
            doom_printf("Error stopping demo: %s.", cs_demo->getError());
         else if(!cs_demo->close())
            doom_printf("Error closing demo: %s.", cs_demo->getError());
      }
      delete cs_demo;
   }

   cs_demo = new CSDemo();
}

// [CG] For atexit.
void CS_StopDemo()
{
   if(cs_demo)
   {
      if(cs_demo->isPlaying() || cs_demo->isRecording())
      {
         if(!cs_demo->stop())
            printf("Error stopping demo: %s.\n", cs_demo->getError());
         else if(!cs_demo->close())
            printf("Error closing demo: %s.\n", cs_demo->getError());
      }
   }
}


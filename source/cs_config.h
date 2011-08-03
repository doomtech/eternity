// Emacs style mode select -*- C -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
// DESCRIPTION:
//   Client/Server configuration loading and parsing.
//
//----------------------------------------------------------------------------

#ifndef __CS_CONFIG_H__
#define __CS_CONFIG_H__

#include "doomtype.h"

#include <enet/enet.h>
#include <json/json.h>

#define DEFAULT_CONFIG_FILENAME "server.json"

// [CG] These names aren't as nice as the DMFLAGS' names, but I don't care.
typedef enum
{
   cmf_comp_god        = 2 << 0,
   cmf_comp_infcheat   = 2 << 1,
   cmf_comp_skymap     = 2 << 2,
   cmf_comp_zombie     = 2 << 3,
   cmf_comp_vile       = 2 << 4,
   cmf_comp_pain       = 2 << 5,
   cmf_comp_skull      = 2 << 6,
   cmf_comp_soul       = 2 << 7,
   cmf_comp_staylift   = 2 << 8,
   cmf_comp_doorstuck  = 2 << 9,
   cmf_comp_pursuit    = 2 << 10,
   cmf_comp_dropoff    = 2 << 11,
   cmf_comp_falloff    = 2 << 12,
   cmf_comp_telefrag   = 2 << 13,
   cmf_comp_respawnfix = 2 << 14,
   cmf_comp_terrain    = 2 << 15,
   cmf_comp_theights   = 2 << 16,
   cmf_comp_planeshoot = 2 << 17,
   cmf_comp_blazing    = 2 << 18,
   cmf_comp_doorlight  = 2 << 19,
   cmf_comp_stairs     = 2 << 20,
   cmf_comp_floors     = 2 << 21,
   cmf_comp_model      = 2 << 22,
   cmf_comp_zerotags   = 2 << 23,
   cmf_comp_special    = 2 << 24,
   cmf_comp_fallingdmg = 2 << 25,
   cmf_comp_overunder  = 2 << 26,
   cmf_comp_ninja      = 2 << 27,
   cmf_comp_mouselook  = 2 << 28,
   cmf_comp_2dradatk   = 2 << 29,
} compatflags_t;

typedef enum
{
   cmf_monsters_remember     = 2 << 0,
   cmf_monster_infighting    = 2 << 1,
   cmf_monster_backing       = 2 << 2,
   cmf_monster_avoid_hazards = 2 << 3,
   cmf_monster_friction      = 2 << 4,
   cmf_monkeys               = 2 << 5,
   cmf_help_friends          = 2 << 6,
   cmf_dog_jumping           = 2 << 7,
} compatflags2_t;

// [CG] C/S settings are sent over the wire and stored in demos, and therefore
//      must be packed and use exact-width integer types.

#pragma pack(push, 1)

typedef struct clientserver_settings_s
{
   skill_t skill;
   gametype_t game_type;
   boolean ctf;
   unsigned int max_admin_clients;
   unsigned int max_player_clients;
   unsigned int max_players;
   unsigned int max_players_per_team;
   unsigned int number_of_teams;
   unsigned int frag_limit;
   unsigned int time_limit;
   unsigned int score_limit;
   unsigned int dogs;
   unsigned int friend_distance;
   bfg_t bfg_type;
   unsigned int friendly_damage_percentage;
   unsigned int spectator_time_limit;
   unsigned int death_time_limit;
   unsigned int death_time_expired_action;
   unsigned int respawn_protection_time;
   unsigned int dmflags;
   unsigned int dmflags2;
   unsigned int compatflags;
   unsigned int compatflags2;
   boolean requires_spectator_password;
   boolean requires_player_password;
} clientserver_settings_t;

#pragma pack(pop)

extern json_object *cs_json;
extern clientserver_settings_t *cs_original_settings;
extern clientserver_settings_t *cs_settings;

void CS_ValidateOptions(json_object *options);
void SV_LoadConfig(void);
boolean CS_AddIWAD(const char *resource_name);
boolean CS_AddWAD(const char *resource_name);
boolean CS_AddDeHackEdFile(const char *resource_name);
void CS_HandleResourcesSection(json_object *resources);
void CS_HandleServerSection(json_object *server);
void CS_HandleOptionsSection(json_object *options);
void CS_HandleMapsSection(json_object *maps);
void CS_LoadConfig(void);
void CL_LoadConfig(void);
void CS_LoadWADs(void);
void CS_LoadMapOverrides(unsigned int map_index);
void CS_ReloadDefaults(void);
void CS_ApplyConfigSettings(void);

#endif


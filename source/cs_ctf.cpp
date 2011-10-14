// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright (C) 2006-2010 by The Odamex Team.
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
//   C/S CTF routines.
//
//----------------------------------------------------------------------------

// [CG] Notably, CTF was largely written by Toke for Odamex, and although that
//      implementation varies significantly from this one, it's still the same
//      fundamental design.  Thanks :) .

#include "c_io.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "e_things.h"
#include "g_game.h"
#include "m_fixed.h"
#include "p_maputl.h"
#include "p_mobj.h"

#include "cs_announcer.h"
#include "cs_team.h"
#include "cs_ctf.h"
#include "cs_main.h"
#include "cs_netid.h"
#include "cl_main.h"
#include "sv_main.h"

flag_stand_t cs_flag_stands[team_color_max];
flag_t cs_flags[team_color_max];

static void respawn_flag(flag_t *flag, fixed_t x, fixed_t y, fixed_t z,
                         const char *type_name)
{
   Mobj *flag_actor;

   if(!serverside)
      return;

   CS_RemoveFlagActor(flag);
   flag_actor = P_SpawnMobj(x, y, z, E_SafeThingName(type_name));
   if(CS_SERVER)
      SV_BroadcastActorSpawned(flag_actor);
   flag->net_id = flag_actor->net_id;
}

static teamcolor_t get_other_color(teamcolor_t color)
{
   if(color == team_color_blue)
      return team_color_red;

   return team_color_blue;
}

static const char* get_flag_name(teamcolor_t color)
{
   if(color == team_color_red)
      return "RedFlag";

   return "BlueFlag";
}

static const char* get_carried_flag_name(teamcolor_t color)
{
   if(color == team_color_red)
      return "CarriedRedFlag";

   return "CarriedBlueFlag";
}

void CS_RemoveFlagActor(flag_t *flag)
{
   Mobj *flag_actor;

   if(flag->net_id && ((flag_actor = NetActors.lookup(flag->net_id)) != NULL))
   {
      SV_BroadcastActorRemoved(flag_actor);
      flag_actor->removeThinker();
   }

   flag->net_id = 0;
}

void CS_ReturnFlag(flag_t *flag)
{
   teamcolor_t color = (teamcolor_t)(flag - cs_flags);
   flag_stand_t *flag_stand = &cs_flag_stands[color];

   respawn_flag(
      flag, flag_stand->x, flag_stand->y, flag_stand->z, get_flag_name(color)
   );

   flag->carrier = 0;
   flag->pickup_time = 0;
   flag->timeout = 0;
   flag->state = flag_home;
}

void CS_GiveFlag(int playernum, flag_t *flag)
{
   teamcolor_t color = (teamcolor_t)(flag - cs_flags);
   player_t *player = &players[playernum];
   position_t position;

   CS_SaveActorPosition(&position, player->mo, 0);

   respawn_flag(
      flag, position.x, position.y, position.z, get_carried_flag_name(color)
   );

   flag->carrier = playernum;

   if(CS_CLIENT)
      flag->pickup_time = cl_current_world_index;
   else if(CS_SERVER)
      flag->pickup_time = sv_world_index;
   else
      flag->pickup_time = gametic;

   flag->timeout = 0;
   flag->state = flag_carried;
}

void CS_HandleFlagTouch(player_t *player, teamcolor_t color)
{
   unsigned int playernum = player - players;
   client_t *client = &clients[playernum];
   flag_t *flag = &cs_flags[color];
   Mobj *flag_actor = NetActors.lookup(flag->net_id);
   teamcolor_t other_color = get_other_color(color);

   if(flag->state == flag_dropped)
   {
      if(client->team == color)
      {
         if(CS_SERVER)
         {
            SV_BroadcastAnnouncerEvent(ae_flag_returned, flag_actor);
            SV_BroadcastMessage(
               "%s returned the %s flag", player->name, team_color_names[color]
            );
         }
         CS_ReturnFlag(flag);
      }
      else
      {
         if(CS_SERVER)
         {
            SV_BroadcastAnnouncerEvent(ae_flag_taken, flag_actor);
            SV_BroadcastMessage(
               "%s picked up the %s flag",
               player->name,
               team_color_names[color]
            );
         }
         CS_GiveFlag(playernum, flag);
      }
   }
   else
   {
      if(client->team != color)
      {
         if(CS_SERVER)
         {
            SV_BroadcastAnnouncerEvent(ae_flag_taken, flag_actor);
            SV_BroadcastMessage(
               "%s has taken the %s flag",
               player->name,
               team_color_names[color]
            );
         }
         CS_GiveFlag(playernum, flag);
      }
      else if(cs_flags[other_color].carrier == playernum)
      {
         if(CS_SERVER)
         {
            SV_BroadcastAnnouncerEvent(ae_flag_captured, flag_actor);
            SV_BroadcastMessage(
               "%s captured the %s flag",
               player->name,
               team_color_names[other_color]
            );
         }
         CS_ReturnFlag(&cs_flags[other_color]);
         team_scores[color]++;
      }
   }
}

flag_t* CS_GetFlagCarriedByPlayer(int playernum)
{
   client_t *client = &clients[playernum];
   teamcolor_t other_color = get_other_color((teamcolor_t)client->team);
   flag_t *flag = &cs_flags[other_color];

   if(flag->carrier == playernum)
      return flag;

   return NULL;
}

void CS_DropFlag(int playernum)
{
   flag_t *flag = CS_GetFlagCarriedByPlayer(playernum);
   teamcolor_t color = (teamcolor_t)(flag - cs_flags);
   Mobj *corpse = players[playernum].mo;
   Mobj *flag_actor;

   if(!flag || flag->carrier != playernum)
      return;

   respawn_flag(flag, corpse->x, corpse->y, corpse->z, get_flag_name(color));

   flag_actor = NetActors.lookup(flag->net_id);

   if(CS_SERVER)
   {
      SV_BroadcastAnnouncerEvent(ae_flag_dropped, flag_actor);
      SV_BroadcastMessage(
         "%s dropped the %s flag",
         players[playernum].name,
         team_color_names[color]
      );
   }

   flag->state = flag_dropped;
   flag->carrier = 0;
   flag->pickup_time = 0;
   flag->timeout = 0;
}

void CS_CTFTicker(void)
{
   int color;
   flag_t *flag;
   position_t position;
   Mobj *flag_actor;

   for(color = team_color_none; color < team_color_max; color++)
   {
      flag = &cs_flags[color];

      if(flag->state == flag_dropped)
      {
         if(++flag->timeout > (10 * TICRATE))
         {
            CS_ReturnFlag(flag);
            flag_actor = NetActors.lookup(flag->net_id);
            if(CS_SERVER)
            {
               SV_BroadcastAnnouncerEvent(ae_flag_returned, flag_actor);
               SV_BroadcastMessage(
                  "%s flag returned", team_color_names[color]
               );
            }
         }
      }
      else if(flag->state == flag_carried)
      {
         if((flag_actor = NetActors.lookup(flag->net_id)) != NULL)
         {
            CS_SaveActorPosition(&position, players[flag->carrier].mo, 0);
            P_UnsetThingPosition(flag_actor);
            flag_actor->x = position.x;
            flag_actor->y = position.y;
            flag_actor->z = position.z;
            P_SetThingPosition(flag_actor);

            if(flag->carrier == displayplayer)
               flag_actor->flags2 |= MF2_DONTDRAW;
            else
               flag_actor->flags2 &= ~MF2_DONTDRAW;
         }
      }
   }
}


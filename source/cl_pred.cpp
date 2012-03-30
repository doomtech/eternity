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
// DESCRIPTION:
//   Prediction routines for clientside local player and sector prediction
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_event.h"
#include "doomstat.h"
#include "i_system.h"
#include "r_state.h"
#include "tables.h"
#include "p_user.h"

#include "cs_cmd.h"
#include "cs_main.h"
#include "cs_netid.h"
#include "cs_position.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spec.h"

bool cl_predicting = false;

static bool prediction_enabled = true;

static cs_cmd_t local_commands[MAX_POSITIONS];
static cs_player_position_t last_server_position;
static cs_floor_status_e last_floor_status;
static uint32_t last_server_command_index;
static uint32_t latest_sector_position_index;

static void CL_runPlayerCommand(uint32_t index, bool think)
{
   ticcmd_t ticcmd;

   CS_LogSMT(
      "%u/%u: Running player command %u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      index
   );

   CS_CopyCommandToTiccmd(&ticcmd, CL_GetCommandAtIndex(index));
   CS_RunPlayerCommand(consoleplayer, &ticcmd, think);
}

static void CL_runSectorThinkers(uint32_t index)
{
   NetIDToObject<SectorMovementThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
      nito->object->Predict(index);
}

static void CL_removeOldSectorMovementThinkers(uint32_t last_sector_index)
{
   PlatThinker           *platform            = NULL;
   VerticalDoorThinker   *door                = NULL;
   CeilingThinker        *ceiling             = NULL;
   FloorMoveThinker      *floor               = NULL;
   ElevatorThinker       *elevator            = NULL;
   PillarThinker         *pillar              = NULL;
   FloorWaggleThinker    *waggle              = NULL;
   NetIDToObject<SectorMovementThinker> *nito = NULL;

   while((nito = NetSectorThinkers.iterate(nito)))
   {
      if(nito->object->removed && (nito->object->removed < last_sector_index))
      {
         CS_LogSMT(
            "%u/%u: Removing SMT %u (%u < %u).\n",
            cl_latest_world_index,
            cl_current_world_index,
            nito->object->net_id,
            nito->object->removed,
            last_sector_index
         );
      }
      else
         continue;

      if((platform = dynamic_cast<PlatThinker *>(nito->object)))
         P_RemoveActivePlat(platform);
      else if((door = dynamic_cast<VerticalDoorThinker *>(nito->object)))
         P_RemoveDoor(door);
      else if((ceiling = dynamic_cast<CeilingThinker *>(nito->object)))
         P_RemoveActiveCeiling(ceiling);
      else if((floor = dynamic_cast<FloorMoveThinker *>(nito->object)))
         P_RemoveFloor(floor);
      else if((elevator = dynamic_cast<ElevatorThinker *>(nito->object)))
         P_RemoveElevator(elevator);
      else if((pillar = dynamic_cast<PillarThinker *>(nito->object)))
         P_RemovePillar(pillar);
      else if((waggle = dynamic_cast<FloorWaggleThinker *>(nito->object)))
         P_RemoveFloorWaggle(waggle);
      else
      {
         I_Error(
            "CL_RemoveOldSectorMovementThinkers: "
            "can't remove thinker %u: unknown type.\n",
            nito->object->net_id
         );
      }
   }
}

void CL_InitPrediction()
{
   cl_current_world_index = 0;
   memset(local_commands, 0, MAX_POSITIONS * sizeof(cs_cmd_t));
}

cs_cmd_t* CL_GetCommandAtIndex(uint32_t index)
{
   return &local_commands[index % MAX_POSITIONS];
}

cs_cmd_t* CL_GetCurrentCommand()
{
   return CL_GetCommandAtIndex(cl_commands_sent - 1);
}

void CL_RunPredictedCommand()
{
   CL_runPlayerCommand(cl_commands_sent - 1, false);
}

void CL_Predict()
{
   int old_status = 0;
   bool sectors_moving = false;
   bool started_predicting_sectors = false;
   uint32_t i, delta, lsindex, world_index, command_index;
   PlatThinker *platform = (PlatThinker *)NetSectorThinkers.lookup(1);

   if(!prediction_enabled)
      return;

   lsindex = latest_sector_position_index;

   command_index = last_server_command_index + 1;

   if(cl_commands_sent <= (command_index + 1))
      return;

   delta = cl_commands_sent - (command_index + 1);

   if((!delta) || (delta >= MAX_POSITIONS))
      return;

   world_index = cl_latest_world_index - delta;

   cl_predicting_sectors = true;

   if(platform)
   {
      CS_LogSMT(
         "%u/%u: SMT %u before re-prediction: %d/%d.\n",
         cl_latest_world_index,
         cl_current_world_index,
         _DEBUG_SECTOR,
         platform->sector->ceilingheight >> FRACBITS,
         platform->sector->floorheight >> FRACBITS
      );
   }

   if(!NetSectorThinkers.isEmpty())
      sectors_moving = true;

   if(lsindex < world_index)
      CL_removeOldSectorMovementThinkers(lsindex);
   else
      CL_removeOldSectorMovementThinkers(world_index);

   if((sectors_moving) && (lsindex <= world_index))
   {
      CS_LogSMT(
         "%u/%u: Catching sectors up (%u => %u).\n",
         cl_latest_world_index,
         cl_current_world_index,
         lsindex,
         world_index
      );

      cl_setting_sector_positions = true;
      CL_LoadSectorPositionsAt(lsindex);
      cl_setting_sector_positions = false;

      CS_LogSMT(
         "%u/%u: Activating SMTs at %u.\n",
         cl_latest_world_index,
         cl_current_world_index,
         lsindex
      );
      CL_ActivateAllSectorMovementThinkers();
      started_predicting_sectors = true;

      cl_predicting_sectors = true;
      do
      {
         CL_runSectorThinkers(lsindex);
      } while(++lsindex <= world_index);
      cl_predicting_sectors = false;

      if(platform)
      {
         CS_LogSMT(
            "%u/%u: Done catching sectors up (%d/%d).\n",
            cl_latest_world_index,
            cl_current_world_index,
            platform->sector->ceilingheight >> FRACBITS,
            platform->sector->floorheight >> FRACBITS
         );
      }
      else
      {
         CS_LogSMT(
            "%u/%u: Done catching sectors up.\n",
            cl_latest_world_index,
            cl_current_world_index
         );
      }
   }

   if(!clients[consoleplayer].spectating)
      CL_LoadLastServerPosition();

   CS_LogSMT(
      "%u/%u: Re-predicting %u/%u/%u TICs (%u).\n",
      cl_latest_world_index,
      cl_current_world_index,
      command_index,
      cl_commands_sent - 1,
      delta,
      world_index
   );

   if(platform)
      old_status = platform->status;

   cl_predicting = true;
   for(i = 0; world_index < (cl_current_world_index - 1); i++, world_index++)
   {
      if(!started_predicting_sectors)
      {
         if(world_index <= lsindex)
         {
            cl_setting_sector_positions = true;
            CL_LoadSectorPositionsAt(world_index);
            cl_setting_sector_positions = false;
         }

         if((sectors_moving) && (world_index == lsindex))
         {
            CS_LogSMT(
               "%u/%u: Activating SMTs at %u.\n",
               cl_latest_world_index,
               cl_current_world_index,
               world_index
            );
            CL_ActivateAllSectorMovementThinkers();
            started_predicting_sectors = true;
         }
      }

      if((i < delta) && (!clients[consoleplayer].spectating))
         CL_runPlayerCommand(command_index + i, true);

      if(started_predicting_sectors)
      {
         cl_predicting_sectors = true;
         CL_runSectorThinkers(world_index);
         cl_predicting_sectors = false;
      }
   }
   cl_predicting = false;

   /*
   if(platform)
   {
      CS_LogSMT(
         "%u/%u: Status changed: %d => %d.\n",
         cl_latest_world_index,
         cl_current_world_index,
         old_status,
         platform->status
      );

      CS_LogSMT(
         "%u/%u: SMT %u after re-prediction: %d/%d.\n",
         cl_latest_world_index,
         cl_current_world_index,
         _DEBUG_SECTOR,
         platform->sector->ceilingheight >> FRACBITS,
         platform->sector->floorheight >> FRACBITS
      );
   }
   */

   CS_LogSMT(
      "%u/%u: Done re-predicting.\n",
      cl_latest_world_index,
      cl_current_world_index
   );
   CS_LogPlayerPosition(consoleplayer);
   P_PlayerThink(&players[consoleplayer]);

   if((!clients[consoleplayer].spectating) && (players[consoleplayer].mo))
      players[consoleplayer].mo->Think();

   CS_LogPlayerPosition(consoleplayer);
   CS_LogSMT(
      "%u/%u: Done predicting.\n",
      cl_latest_world_index,
      cl_current_world_index
   );
}

void CL_StoreLatestSectorPositionIndex()
{
   CS_LogSMT(
      "%u/%u: Last SPI: %u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      cl_latest_world_index
   );
   latest_sector_position_index = cl_latest_world_index;
}

uint32_t CL_GetLatestSectorPositionIndex()
{
   return latest_sector_position_index;
}

void CL_StoreLastServerPosition(nm_playerposition_t *message)
{
   CS_LogSMT(
      "%u/%u: PPM %u/%u/%u.\n",
      cl_latest_world_index,
      cl_current_world_index,
      message->last_index_run,
      message->last_world_index_run,
      message->world_index
   );
   CS_CopyPlayerPosition(&last_server_position, &message->position);
   last_server_command_index = message->last_index_run;
   last_server_position.world_index = message->last_world_index_run;
   last_floor_status = (cs_floor_status_e)message->floor_status;
}

void CL_LoadLastServerPosition()
{
   CS_SetPlayerPosition(consoleplayer, &last_server_position);
   clients[consoleplayer].floor_status = last_floor_status;
}

cs_player_position_t* CL_GetLastServerPosition()
{
   return &last_server_position;
}


// Emacs style mode select   -*- C++ -*- vi:sw=3 ts=3:
//-----------------------------------------------------------------------------
//
// Copyright(C) 2011 James Haley
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
//   Sector effects.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "doomstat.h"
#include "e_things.h"
#include "m_fixed.h"
#include "p_mobj.h"
#include "p_saveg.h"
#include "p_spec.h"
#include "r_defs.h"

#include "cl_main.h"
#include "cl_spec.h"
#include "sv_main.h"

//=============================================================================
//
// SectorThinker class methods
//

IMPLEMENT_THINKER_TYPE(SectorThinker)
IMPLEMENT_THINKER_TYPE(SectorMovementThinker)

SectorThinker::SectorThinker() : Thinker()
{
   if(CS_CLIENT)
   {
      activation_status = ACTIVATION_UNCONFIRMED;
      activation_world_index = 0;
      activation_command_index = cl_commands_sent;
   }
   else
   {
      activation_status = ACTIVATION_AFFIRMATIVE;
      activation_world_index = 0;
      activation_command_index = 0;
   }

   sector = NULL;
   line = NULL;
   pre_activation_sector = NULL;
   pre_activation_line = NULL;

   predicting = false;
   repredicting = false;
   net_id = 0;
   removed = 0;
   inactive = 0;

   M_QueueInit(&status_queue);
}

SectorThinker::SectorThinker(sector_t *new_sector) : Thinker()
{
   if(CS_CLIENT)
   {
      activation_status = ACTIVATION_UNCONFIRMED;
      activation_world_index = 0;
      activation_command_index = cl_commands_sent;
   }
   else
   {
      activation_status = ACTIVATION_AFFIRMATIVE;
      activation_world_index = 0;
      activation_command_index = 0;
   }

   sector = NULL;
   line = NULL;
   pre_activation_sector = NULL;
   pre_activation_line = NULL;

   predicting = false;
   repredicting = false;
   net_id = 0;
   removed = 0;
   inactive = 0;

   M_QueueInit(&status_queue);

   setSector(new_sector);
}

SectorThinker::SectorThinker(sector_t *new_sector, line_t *new_line)
   : Thinker()
{
   if(CS_CLIENT)
   {
      activation_status = ACTIVATION_UNCONFIRMED;
      activation_world_index = 0;
      activation_command_index = cl_commands_sent;
   }
   else
   {
      activation_status = ACTIVATION_AFFIRMATIVE;
      activation_world_index = 0;
      activation_command_index = 0;
   }

   sector = NULL;
   line = NULL;
   pre_activation_sector = NULL;
   pre_activation_line = NULL;

   predicting = false;
   repredicting = false;
   net_id = 0;
   removed = 0;
   inactive = 0;

   M_QueueInit(&status_queue);

   setSector(new_sector);
   setLine(new_line);
}

SectorThinker::~SectorThinker()
{
   if(pre_activation_sector)
      efree(pre_activation_sector);

   if(pre_activation_line)
      efree(pre_activation_line);

   M_QueueFree(&status_queue);
}

//
// SectorThinker::serialize
//
void SectorThinker::serialize(SaveArchive &arc)
{
   Super::serialize(arc);

   arc << sector;

   // when reloading, attach to sector
   if(arc.isLoading())
   {
      switch(getAttachPoint())
      {
      case ATTACH_FLOOR:
         sector->floordata = this;
         break;
      case ATTACH_CEILING:
         sector->ceilingdata = this;
         break;
      case ATTACH_FLOORCEILING:
         sector->floordata   = this;
         sector->ceilingdata = this;
         break;
      case ATTACH_LIGHT:
         sector->lightingdata = this;
         break;
      default:
         break;
      }
   }
}

//
// SectorThinker::reTriggerVerticalDoor()
//
// Returns true if a vertical door should be re-triggered.
//
bool SectorThinker::reTriggerVerticalDoor(bool player)
{
   return false;
}

//
// SectorThinker::netSerialize()
//
// Serializes the current status into a net-safe struct.
//
void SectorThinker::netSerialize(cs_sector_thinker_data_t *data)
{
   dumpStatusData(data);
}

//
// SectorThinker::netUpdate()
//
// Updates the thinker with data from a net-safe struct.  Saves the update and
// its index as well.
//
void SectorThinker::netUpdate(uint32_t index,
                                      cs_sector_thinker_data_t *data)
{
   if(CS_CLIENT) // [CG] C/S clients only.
   {
      mqueueitem_t *mqitem = NULL;
      cs_queued_sector_thinker_data_t *qstd = NULL;

      M_QueueResetIterator(&status_queue);
      while((mqitem = M_QueueIterator(&status_queue)))
      {
         mqueueitem_t *mqitem = NULL;
         qstd = (cs_queued_sector_thinker_data_t *)mqitem;

         if(qstd->index != index)
            continue;

         copyStatusData(&qstd->data, data);
         M_QueueResetIterator(&status_queue);
         return;
      }

      qstd = estructalloc(cs_queued_sector_thinker_data_t, 1);
      qstd->index = index;
      copyStatusData(&qstd->data, data);
      M_QueueInsert((mqueueitem_t *)qstd, &status_queue);
      M_QueueResetIterator(&status_queue);
   }
}

//
// SectorThinker::logStatus()
//
// Logs the passed status data to the c/s sector thinker log file.  Used for
// debugging and overridden by derived classes.
//
void SectorThinker::logStatus(cs_sector_thinker_data_t *data) {}

//
// SectorThinker::startThinking()
//
// Checks if a sector movement thinker should think, and if so sets it up for
// thinking.
//
bool SectorThinker::startThinking()
{
   dumpStatusData(&current_status);

   if(CS_CLIENT && inactive && (inactive <= prediction_index))
      return false;

   return true;
}

//
// SectorThinker::finishThinking()
//
// Runs after a sector movement thinker finishes thinking.
//
void SectorThinker::finishThinking()
{
   if((CS_CLIENT) && (!repredicting))
      savePredictedStatus(prediction_index);

   if(CS_SERVER && net_id && statusChanged())
      SV_BroadcastSectorThinkerStatus(this);

   prediction_index = 0;
}

//
// SectorThinker::savePredictedStatus()
//
// Stores a new predicted status.
//
void SectorThinker::savePredictedStatus(uint32_t index)
{
   cs_queued_sector_thinker_data_t *qstd = estructalloc(
      cs_queued_sector_thinker_data_t, 1
   );

   qstd->index = index;
   dumpStatusData(&qstd->data);
   M_QueueInsert((mqueueitem_t *)qstd, &status_queue);
}

//
// SectorThinker::clearOldUpdates()
//
// Clears outdated updates from the queue.
//
void SectorThinker::clearOldUpdates(uint32_t index)
{
   if(CS_CLIENT) // [CG] C/S clients only.
   {
      mqueueitem_t *mqitem = NULL;
      cs_queued_sector_thinker_data_t *qstd = NULL;

      while((mqitem = M_QueuePeek(&status_queue)))
      {
         qstd = (cs_queued_sector_thinker_data_t *)mqitem;

         if(qstd->index >= index)
            return;

         qstd = (cs_queued_sector_thinker_data_t *)M_QueuePop(&status_queue);
         efree(qstd);
      }
   }
}

//
// SectorThinker::loadStatusFor()
//
// Loads the sector movement thinker's status for a given index.
//
void SectorThinker::loadStatusFor(uint32_t index)
{
   mqueueitem_t *mqitem = NULL;
   cs_queued_sector_thinker_data_t *previous_qstd = NULL, *qstd = NULL;

   M_QueueResetIterator(&status_queue);
   while((mqitem = M_QueueIterator(&status_queue)))
   {
      previous_qstd = qstd;
      qstd = (cs_queued_sector_thinker_data_t *)mqitem;

      if(qstd->index >= index)
         break;
   }
   M_QueueResetIterator(&status_queue);

   if(!qstd)
   {
      // I_Error("No status for SMT %u at %u.\n" net_id, index);
      CS_LogSMT("%u/%u: No status for SMT %u at %u.\n",
         cl_latest_world_index,
         cl_current_world_index,
         net_id,
         index
      );
   }
   else if(previous_qstd)
      loadStatusData(&previous_qstd->data);
   else
      loadStatusData(&qstd->data);
}

//
// SectorThinker::rePredict()
//
// Re-Predicts sector movement clientside.
//
void SectorThinker::rePredict(uint32_t index)
{
   if(CS_CLIENT)
   {
      loadStatusFor(index);

      CS_LogSMT(
         "%u/%u: Re-predicting SMT %u at %u: %d/%d => ",
         cl_latest_world_index,
         cl_current_world_index,
         net_id,
         index,
         sector->ceilingheight >> FRACBITS,
         sector->floorheight >> FRACBITS
      );

      prediction_index = index;
      repredicting = true;
      cl_predicting_sectors = true;
      Think();
      repredicting = false;
      cl_predicting_sectors = false;

      CS_LogSMT(
         "%d/%d.\n",
         sector->ceilingheight >> FRACBITS,
         sector->floorheight >> FRACBITS
      );
   }
}

//
// SectorThinker::Predict()
//
// Predicts sector movement clientside.
//
void SectorThinker::Predict(uint32_t index)
{
   if(CS_CLIENT)
   {
      prediction_index = index;
      predicting = true;
      cl_predicting_sectors = true;
      CS_LogSMT(
         "%u/%u: Predicting SMT %u at %u (%u): %d/%d => ",
         cl_latest_world_index,
         cl_current_world_index,
         net_id,
         index,
         cl_commands_sent - 1,
         sector->ceilingheight >> FRACBITS,
         sector->floorheight >> FRACBITS
      );
      loadStatusFor(index);
      Think();
      savePredictedStatus(index);
      CS_LogSMT(
         "%d/%d.\n",
         sector->ceilingheight >> FRACBITS,
         sector->floorheight >> FRACBITS
      );
      predicting = false;
      cl_predicting_sectors = false;
   }
}

//
// SectorThinker::setInactive()
//
// Sets the sector movement thinker as inactive at the current prediction
// index.
//
void SectorThinker::setInactive()
{
   if(CS_CLIENT)
      inactive = prediction_index;
}

//
// SectorThinker::isPredicting()
//
// Returns true if the sector movement thinker is currently predicting.
//
bool SectorThinker::isPredicting()
{
   if(predicting)
      return true;

   return false;
}

//
// SectorThinker::isRePredicting()
//
// Returns true if the sector movement thinker is currently predicting.
//
bool SectorThinker::isRePredicting()
{
   if(repredicting)
      return true;

   return false;
}

void SectorThinker::setActivationIndex(uint32_t command_index,
                                       uint32_t world_index)
{
   if(!CS_CLIENT)
      return;

   if(activation_world_index)
      return;

   if(!line)
      return;

   if(command_index < activation_command_index)
      return;

   activation_world_index = world_index;
}

bool SectorThinker::confirmActivation(line_t *ln, uint32_t command_index)
{
   if(!CS_CLIENT)
      return false;

   if(!line)
      return false;

   if(activation_status != ACTIVATION_UNCONFIRMED)
      return false;

   if(line != ln)
      return false;

   if(command_index != activation_command_index)
      return false;

   activation_status = ACTIVATION_AFFIRMATIVE;
   return true;
}

void SectorThinker::checkActivationExpired()
{
   if(!CS_CLIENT)
      return;

   if(!line)
      return;

   if(activation_status == ACTIVATION_AFFIRMATIVE)
      return;

   if(activation_status == ACTIVATION_NEGATIVE)
      return;

   if(!activation_world_index)
      return;

   if(cl_latest_world_index >= activation_world_index)
      activation_status = ACTIVATION_NEGATIVE;
}

bool SectorThinker::activationExpired()
{
   if(activation_status == ACTIVATION_NEGATIVE)
      return true;

   return false;
}

void SectorThinker::Reset()
{
   if(sector && pre_activation_sector)
      memcpy(sector, pre_activation_sector, sizeof(sector_t));

   if(line && pre_activation_line)
      memcpy(line, pre_activation_line, sizeof(line_t));
}

void SectorThinker::setSector(sector_t *new_sector)
{
   sector = new_sector;

   if(pre_activation_sector)
   {
      efree(pre_activation_sector);
      pre_activation_sector = NULL;
   }

   if(sector)
   {
      pre_activation_sector = estructalloc(sector_t, 1);
      memcpy(pre_activation_sector, sector, sizeof(sector_t));
   }
}

void SectorThinker::setLine(line_t *new_line)
{
   line = new_line;

   if(pre_activation_line)
   {
      efree(pre_activation_line);
      pre_activation_line = NULL;
   }

   if(line)
   {
      pre_activation_line = estructalloc(line_t, 1);
      memcpy(pre_activation_line, line, sizeof(line_t));
   }
}

//=============================================================================
//
// Sector Actions
//

//
// P_NewSectorActionFromMobj
//
// Adds the Mobj's special to the sector
//
void P_NewSectorActionFromMobj(Mobj *actor)
{
   sectoraction_t *newAction = estructalloc(sectoraction_t, 1);

   if(actor->type == E_ThingNumForName("EESectorActionEnter"))
   {
      // TODO
   }
}

// EOF


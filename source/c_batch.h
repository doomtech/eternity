// Emacs style mode select -*- C++ -*-
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
//   Batched console commands.
//
//----------------------------------------------------------------------------

#ifndef C_BATCH__
#define C_BATCH__

class qstring;

void          C_ActivateCommandBatch();
void          C_AddCommandBatch(const char *name, const char *commands);
const char*   C_GetCommandBatch(const char *name);
void          C_CommandBatchTicker();
bool          C_CommandIsBatch(const char *name);
void          C_SaveCommandBatches(qstring &buf);

#endif


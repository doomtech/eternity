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
//------------------------------------------------------------------------------
//
// $Id: version.c,v 1.2 1998/05/03 22:59:31 killough Exp $
//
// Description: Version ID constants
//
//-----------------------------------------------------------------------------

#include "version.h"

// sf: made int from define
int version = 340;

// haleyjd: subversion -- range from 0 to 255
unsigned char subversion = 16;

const char version_date[] = __DATE__;
const char version_time[] = __TIME__; // haleyjd

// [CG] Added a c/s protocol version.  A simple single number is sufficient
//      because it's only incremented for incompatible protocol changes.
uint32_t cs_protocol_version = 0;

// sf: version name -- at the suggestion of mystican
const char version_name[] = "Mjolnir";

// haleyjd: caption for SDL window
#ifdef _SDL_VER
const char ee_wmCaption[] = "Eternity Engine v3.40.16 \"Mjolnir\"";
#endif
            
// haleyjd: Eternity release history
// private alpha       -- 3.29 private       09/14/00
// public beta         -- 3.29 public beta 1 01/08/01
// public beta         -- 3.29 public beta 2 01/09/01
// public beta         -- 3.29 public beta 3 05/10/01
// public beta         -- 3.29 public beta 4 06/30/01
// development beta    -- 3.29 dev beta 5    10/02/01
// final v3.29 release -- 3.29 gamma         07/04/02
// public beta         -- 3.31 public beta 1 09/11/02
// public beta         -- 3.31 public beta 2 03/05/03
// public beta         -- 3.31 public beta 3 08/08/03
// public beta         -- 3.31 public beta 4 11/29/03
// public beta         -- 3.31 public beta 5 12/17/03
// public beta         -- 3.31 public beta 6 02/29/04
// public beta         -- 3.31 public beta 7 04/11/04

// 3.31.10 'Delta'     -- 01/19/05
// 3.33.00 'Genesis'   -- 05/26/05
// 3.33.01 'Outcast'   -- 06/24/05
// 3.33.02 'Warrior'   -- 10/01/05
// 3.33.33 'Paladin'   -- 05/17/06
// 3.33.50 'Phoenix'   -- 10/23/06
// 3.35.90 'Simorgh'   -- 01/11/09
// 3.35.92 'Nekhbet'   -- 03/22/09
// 3.37.00 'Sekhmet'   -- 01/01/10
// 3.39.20 'Resheph'   -- 10/10/10
// 3.40.00 'Rebirth'   -- 01/08/11
// 3.40.10 'Aasgard'   -- 05/01/11
// 3.40.11 'Aasgard'   -- 05/02/11
// 3.40.15 'Wodanaz'   -- 06/21/11

// auxilliary releases
// Caverns of Darkness -- 3.29 dev beta 5 joel-2 04/24/02

//----------------------------------------------------------------------------
//
// $Log: version.c,v $
// Revision 1.2  1998/05/03  22:59:31  killough
// beautification
//
// Revision 1.1  1998/02/02  13:21:58  killough
// version information files
//
//----------------------------------------------------------------------------

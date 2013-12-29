// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//   
//   SDL-specific graphics code.
//
//-----------------------------------------------------------------------------

#ifndef I_SDLVIDEO_H__
#define I_SDLVIDEO_H__

// Grab the HAL video definitions
#include "../i_video.h" 

//
// SDL Video Driver
//
class SDLVideoDriver : public HALVideoDriver
{
protected:
   virtual void SetPrimaryBuffer();
   virtual void UnsetPrimaryBuffer();

public:
   virtual void FinishUpdate();
   virtual void ReadScreen(byte *scr);
   virtual void InitDiskFlash();
   virtual void BeginRead();
   virtual void EndRead();
   virtual void SetPalette(byte *pal);
   virtual void ShutdownGraphics();
   virtual void ShutdownGraphicsPartway();
   virtual bool InitGraphicsMode();
};

// Global singleton instance
extern SDLVideoDriver i_sdlvideodriver;

#endif

// EOF


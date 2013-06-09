// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2013 James Haley
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
//-----------------------------------------------------------------------------
//
// XInput Gamepad Support
//
// By James Haley
//
//-----------------------------------------------------------------------------

#ifndef I_XINPUT_H__
#define I_XINPUT_H__

#ifdef EE_FEATURE_XINPUT

#include "../hal/i_gamepads.h"

//
// XInputGamePadDriver
//
// Implements support for XBox 360 controller and compatible devices through
// their native interface.
//
class XInputGamePadDriver : public HALGamePadDriver
{
public:
   virtual bool initialize();
   virtual void shutdown();
   virtual void enumerateDevices();
   virtual int  getBaseDeviceNum() { return 0; }
};

extern XInputGamePadDriver i_xinputGamePadDriver;

class XInputGamePad;

//
// XInputHapticInterface
//
// Exposes support for force feedback effects through XInput gamepads.
//
class XInputHapticInterface : public HALHapticInterface
{
   DECLARE_RTTI_TYPE(XInputHapticInterface, HALHapticInterface)

protected:
   unsigned long dwUserIndex;
   bool pauseState;

   void zeroState();

public:
   XInputHapticInterface(unsigned long userIdx = 0);
   virtual void startEffect(effect_e effect, int data1, int data2);
   virtual void pauseEffects(bool effectsPaused);
   virtual void updateEffects();
   virtual void clearEffects();
};

//
// XInputGamePad
//
// Represents an actual XInput device.
//
class XInputGamePad : public HALGamePad
{
   DECLARE_RTTI_TYPE(XInputGamePad, HALGamePad)

protected:
   unsigned long dwUserIndex;
   XInputHapticInterface haptics;

   float normAxis(int value, int threshold, int maxvalue);
   void  normAxisPair(float &axisx, float &axisy, int threshold, int min, int max);

public:
   XInputGamePad(unsigned long userIdx = 0);

   virtual bool select();
   virtual void deselect();
   virtual void poll();
   
   virtual HALHapticInterface *getHapticInterface() { return &haptics; }
};

#endif

#endif

// EOF


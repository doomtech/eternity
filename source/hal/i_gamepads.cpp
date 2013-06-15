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
// DESCRIPTION:  
//    Hardware Abstraction Layer for Gamepads and Joysticks
//
//-----------------------------------------------------------------------------

#include "../z_zone.h"
#include "../c_runcmd.h"

#include "i_gamepads.h"

// Platform-Specific Driver Modules:
#ifdef _SDL_VER
#include "../sdl/i_sdlgamepads.h"
#endif
#ifdef EE_FEATURE_XINPUT
#include "../Win32/i_xinput.h"
#endif

// Globals

// current device number -- saved in config file
int i_joysticknum;

// Module-private data

// Master list of gamepads from all drivers
static PODCollection<HALGamePad *> masterGamePadList;

// Currently selected and active gamepad object, if any.
static HALGamePad *activePad;

// Generic sensitivity values, for drivers that need them
int i_joysticksens;

// haleyjd 04/15/02: joystick sensitivity variables
VARIABLE_INT(i_joysticksens, NULL, 0, 32767, NULL);

CONSOLE_VARIABLE(i_joysticksens, i_joysticksens, 0) {}

//=============================================================================
//
// HALGamePadDriver Methods
//

//
// HALGamePadDriver::addDevice
//
// Add a device to the devices collection.
//
void HALGamePadDriver::addDevice(HALGamePad *device)
{
   devices.add(device);
}

//=============================================================================
//
// HALGamePad Methods
//

IMPLEMENT_RTTI_TYPE(HALGamePad)

//
// Constructor
//
HALGamePad::HALGamePad()
   : Super(), num(-1), name(), numAxes(0), numButtons(0)
{
   // init state and prevstate
   int i;

   for(i = 0; i < MAXBUTTONS; i++)
      state.buttons[i] = false;
   for(i = 0; i < MAXAXES; i++)
      state.axes[i] = 0.0;

   backupState();
}

//
// HALGamePad::backupState
//
// Saves the prior state of buttons and axes before polling occurs. Call from
// child classes' poll method.
//
void HALGamePad::backupState()
{
   int i;
   for(i = 0; i < MAXBUTTONS; i++)
      state.prevbuttons[i] = state.buttons[i];
   for(i = 0; i < MAXAXES; i++)
      state.prevaxes[i] = state.axes[i];
}

//=============================================================================
//
// Global Interface
//

//
// HAL Joy Driver Struct
//
struct halpaddriveritem_t
{
   int id;                    // HAL driver ID number
   const char *name;          // name of driver
   HALGamePadDriver *driver;  // pointer to driver interface, if supported
   bool isInit;               // if true, successfully initialized
};

//
// Driver Table
//
static halpaddriveritem_t halPadDriverTable[] =
{
   // SDL 1.2 MMSYSTEM Driver
   {
      0,
      "SDL MMSYSTEM",
#ifdef _SDL_VER
      &i_sdlGamePadDriver,
#else
      NULL,
#endif
      false
   },

   // XInput Driver for XBox 360 controller and compatibles
   {
      1,
      "XInput",
#ifdef EE_FEATURE_XINPUT
      &i_xinputGamePadDriver,
#else
      NULL,
#endif
      false
   },

   // Terminating entry
   { -1, NULL, NULL, false }
};

//
// I_SelectDefaultGamePad
//
// Select the gamepad configured in the configuration file, if it can be
// found. Otherwise, nothing will happen and activePad will remain NULL.
//
bool I_SelectDefaultGamePad()
{
   HALGamePad *pad = NULL;

   // Deselect any active device first.
   if(activePad)
   {
      activePad->deselect();
      activePad = NULL;
   }

   if(i_joysticknum >= 0)
   {
      // search through the master directory for a pad with this number
      PODCollection<HALGamePad *>::iterator itr = masterGamePadList.begin();

      for(; itr != masterGamePadList.end(); itr++)
      {
         if((*itr)->num == i_joysticknum)
         {
            pad = *itr; // found it.
            break;
         }
      }
   }

   // Select the device if it was found.
   if(pad)
   {      
      if(pad->select())
         activePad = pad;
   }

   return (activePad != NULL);
}

//
// I_EnumerateGamePads
//
// Enumerate all gamepads.
//
void I_EnumerateGamePads()
{
   // Have all supported drivers enumerate their gamepads
   halpaddriveritem_t *item = halPadDriverTable;

   while(item->name)
   {
      if(item->driver && item->isInit)
         item->driver->enumerateDevices();
      ++item;
   }

   // build the master pad directory
   masterGamePadList.clear();

   item = halPadDriverTable;
   while(item->name)
   {
      if(item->driver)
      {
         PODCollection<HALGamePad *> &pads = item->driver->devices;

         for(auto itr = pads.begin(); itr != pads.end(); itr++)
            masterGamePadList.add(*itr);
      }
      ++item;
   }
}

//
// I_InitGamePads
//
// Call at startup to initialize the gamepad/joystick HAL. All modules
// implementing gamepad drivers in the current build will be initialized in
// turn and the master list of available devices will be built.
//
void I_InitGamePads()
{
   // Initialize all supported gamepad drivers
   halpaddriveritem_t *item = halPadDriverTable;

   while(item->name)
   {
      if(item->driver)
         item->isInit = item->driver->initialize();
      ++item;
   }

   // Enumerate gamepads in all supported drivers and build the master lookup
   I_EnumerateGamePads();

   // Select default pad
   I_SelectDefaultGamePad();
}

//
// I_ShutdownGamePads
//
// Call at shutdown to clean up all gamepad drivers.
//
void I_ShutdownGamePads()
{
   halpaddriveritem_t *item = halPadDriverTable;

   while(item->name)
   {
      if(item->driver && item->isInit)
         item->driver->shutdown();
      ++item;
   }
}

//
// I_PollActiveGamePad
//
// Get input from the currently active gamepad, if any.
// Returns NULL if there is no device active.
//
HALGamePad::padstate_t *I_PollActiveGamePad()
{
   if(activePad)
   {
      activePad->poll();
      return &activePad->state;
   }
   else
      return NULL;
}

//
// I_GetNumGamePads
//
size_t I_GetNumGamePads()
{
   return masterGamePadList.getLength();
}

//
// I_GetGamePad
//
HALGamePad *I_GetGamePad(size_t index)
{
   return masterGamePadList[index];
}

//
// I_GetActivePad
//
HALGamePad *I_GetActivePad()
{
   return activePad;
}

// haleyjd 04/15/02: windows joystick commands
CONSOLE_COMMAND(i_joystick, 0)
{
   if(Console.argc != 1)
      return;

   i_joysticknum = Console.argv[0]->toInt();
   I_SelectDefaultGamePad();
}

//=============================================================================
//
// Haptic Output
//

IMPLEMENT_RTTI_TYPE(HALHapticInterface)

// Enable or disable force feedback regardless of whether the device supports
// it or not.
bool i_forcefeedback;

//
// I_StartHaptic
//
// Start a haptic effect with an argument which may affect the strength or
// duration of the effect, depending on the type of effect being ordered.
//
void I_StartHaptic(HALHapticInterface::effect_e effect, int data1, int data2)
{
   HALHapticInterface *hhi;
   
   if(i_forcefeedback && activePad && (hhi = activePad->getHapticInterface()))
   {
      hhi->startEffect(effect, data1, data2);
      I_UpdateHaptics();
   }
}

//
// I_PauseHaptics
//
// Pause or unpause all running haptic effects.
// Call when the game is paused or is not the active process.
//
void I_PauseHaptics(bool effectsPaused)
{
   HALHapticInterface *hhi;

   if(activePad && (hhi = activePad->getHapticInterface()))
      hhi->pauseEffects(effectsPaused);
}

//
// I_UpdateHaptics
//
// Update haptic effects.
// Call from the main game loop, as often as possible.
//
void I_UpdateHaptics()
{
   HALHapticInterface *hhi;

   if(i_forcefeedback && activePad && (hhi = activePad->getHapticInterface()))
      hhi->updateEffects();
}

//
// I_ClearHaptics
//
// Clear haptic effects.
// Call when toggling haptic output, when changing devices, etc.
// NB: This is NOT invoked by the HAL when changing pads. Invoke your own
//   HALGamePad's haptic interface clearEffects method when your pad is
//   selected and deselected.
//
void I_ClearHaptics()
{
   HALHapticInterface *hhi;

   if(activePad && (hhi = activePad->getHapticInterface()))
      hhi->clearEffects();
}

VARIABLE_TOGGLE(i_forcefeedback, NULL, onoff);
CONSOLE_VARIABLE(i_forcefeedback, i_forcefeedback, 0)
{
   // stop any running effects immediately if false
   if(!i_forcefeedback)
      I_ClearHaptics();
}

// EOF


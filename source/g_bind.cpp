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
//--------------------------------------------------------------------------
//
// Key Bindings
//
// Rewritten to support press/release/activate/deactivate only bindings,
// as well as to allow a key to be bound to multiple actions in the same
// key action category.
//
// By Charles Gunyon
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "am_map.h"
#include "c_batch.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h"
#include "d_dehtbl.h"
#include "d_dwfile.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_io.h"        // SoM 3/14/2002: strncasecmp
#include "d_main.h"
#include "d_net.h"
#include "doomdef.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "g_bind.h"
#include "g_game.h"
#include "i_system.h"
#include "m_argv.h"
#include "mn_misc.h"
#include "m_misc.h"
#include "psnprntf.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

extern vfont_t *menu_font_normal;

menuwidget_t KeyBindingsSubSystem::binding_widget = {
   G_BindDrawer, G_BindResponder, NULL, true
};

#define addKey(number, name) \
   keys[number] = new InputKey(number, #name); \
   names_to_keys->addObject(*keys[number])

#define addVariableAction(n, c) \
   names_to_actions->addObject(new VariableInputAction(#n, c, &action_##n))

#define addCommandAction(n) \
   names_to_actions->addObject(new CommandInputAction(n))

KeyBindingsSubSystem key_bindings;

KeyBind::KeyBind(int new_key_number, const char *new_key_name,
                 const char *new_action_name,
                 input_action_category_e new_category,
                 unsigned short new_flags)
   : ZoneObject(), key_number(new_key_number), category(new_category),
     pressed(false), flags(new_flags)
{
   hidden_key_name = estrdup(new_key_name);
   hidden_action_name = estrdup(new_action_name);
   keys_to_actions_key = (const char *)hidden_key_name;
   actions_to_keys_key = (const char *)hidden_action_name;
}

KeyBind::~KeyBind()
{
   efree(hidden_key_name);
   efree(hidden_action_name);
}

int KeyBind::getKeyNumber() const
{
   return key_number;
}

const char* KeyBind::getKeyName() const
{
   return hidden_key_name;
}

const char* KeyBind::getActionName() const
{
   return hidden_action_name;
}

const bool KeyBind::isPressed() const
{
   if(pressed)
      return true;

   return false;
}

const bool KeyBind::isReleased() const
{
   if(!pressed)
      return true;

   return false;
}

void KeyBind::press()
{
   pressed = true;
}

void KeyBind::release()
{
   pressed = false;
}

const input_action_category_e KeyBind::getCategory() const
{
   return category;
}

const unsigned short KeyBind::getFlags() const
{
   return flags;
}

bool KeyBind::isNormal() const
{
   if(flags == 0)
      return true;

   return false;
}

bool KeyBind::isActivateOnly() const
{
   if(flags & activate_only)
      return true;

   return false;
}

bool KeyBind::isDeactivateOnly() const
{
   if(flags & deactivate_only)
      return true;

   return false;
}

bool KeyBind::isPressOnly() const
{
   if(flags & press_only)
      return true;

   return false;
}

bool KeyBind::isReleaseOnly() const
{
   if(flags & release_only)
      return true;

   return false;
}

bool KeyBind::keyIs(const char *key_name) const
{
   if(!strcmp(hidden_key_name, key_name))
      return true;

   return false;
}

bool KeyBind::actionIs(const char *action_name) const
{
   if(!strcmp(hidden_action_name, action_name))
      return true;

   return false;
}

InputKey::InputKey(int new_number, const char *new_name)
   : ZoneObject()
{
   number = new_number;
   hidden_name = estrdup(new_name);
   disabled = false;
   key = (const char *)hidden_name;
}

InputKey::~InputKey()
{
   efree(hidden_name);
}

const char* InputKey::getName() const
{
   return hidden_name;
}

int InputKey::getNumber() const
{
   return number;
}

bool InputKey::isDisabled() const
{
   return disabled;
}

void InputKey::disable()      
{
   disabled = true;
}

void InputKey::enable()      
{
   disabled = false;
}

InputAction::InputAction(const char *new_name,
                         input_action_category_e new_category)
   : ZoneObject(), category(new_category)
{
   hidden_name = estrdup(new_name);
   key = hidden_name;
   bound_keys_description = estrdup("none");
}

InputAction::~InputAction()
{
   efree(bound_keys_description);
   efree(hidden_name);
}

bool InputAction::handleEvent(event_t *ev, KeyBind *kb)
{
   /*
    |---------------------------------------------------------------------|
    | activate   | nothing    | bind +y "+left" | isPO && isAO            |
    | deactivate | nothing    | bind +u "-left" | isPO && isDO            |
    | activate   | nothing    | bind +i "left"  | isPO && (!isAO || isDO) |
    | nothing    | activate   | bind -o "+left" | isRO && isAO            |
    | nothing    | deactivate | bind -p "-left" | isRO && isDO            |
    | nothing    | activate   | bind -h "left"  | isRO && !(isAO || isDO) |
    | activate   | activate   | bind j "+left"  | isAO                    |
    | deactivate | deactivate | bind k "-left"  | isDO                    |
    | activate   | deactivate | bind l "left"   | isN                     |
    | deactivate | activate   | ...             | n/a                     |
    |---------------------------------------------------------------------|
   */

   if(kb->isPressOnly())
   {
      if(kb->isDeactivateOnly())
         deactivate(kb, ev);
      else
         activate(kb, ev);
   }
   else if(kb->isReleaseOnly())
   {
      if(kb->isDeactivateOnly())
         deactivate(kb, ev);
      else
         activate(kb, ev);
   }
   else if(kb->isActivateOnly())
      activate(kb, ev);
   else if(kb->isDeactivateOnly())
      deactivate(kb, ev);
   else if((ev->type == ev_keydown) && mayActivate(kb))
      activate(kb, ev);
   else if((ev->type == ev_keyup) && mayDeactivate(kb))
      deactivate(kb, ev);
   else
      return false;

   if(ev->type == ev_keydown)
      kb->press();
   else if(ev->type == ev_keyup)
      kb->release();

   return true;
}

const char* InputAction::getName() const
{
   return hidden_name;
}

const input_action_category_e InputAction::getCategory() const
{
   return category;
}

const char* InputAction::getDescription() const
{
   return bound_keys_description;
}

void InputAction::setDescription(const char *new_description)
{
   if(bound_keys_description)
      efree(bound_keys_description);

   bound_keys_description = estrdup(new_description);
}

void InputAction::activate(KeyBind *kb, event_t *ev) {}

void InputAction::deactivate(KeyBind *kb, event_t *ev) {}

void InputAction::print() const
{
   C_Printf("%s\n", getName());
}

bool InputAction::isActive() const
{
   return false;
}

bool InputAction::mayActivate(KeyBind *kb) const
{
   if(kb->isPressed())
      return false;

   // [CG] TODO: Repeat rate handling goes here.

   return true;
}

bool InputAction::mayDeactivate(KeyBind *kb) const
{
   if(kb->isReleased())
      return false;

   return true;
}

void InputAction::Think() {}

class OneShotFunctionInputAction : public InputAction
{
protected:
   void(*callback)(event_t *ev);

public:
   OneShotFunctionInputAction(const char *new_name,
                              input_action_category_e new_category,
                              void(*new_callback)(event_t *ev))
      : InputAction(new_name, new_category)
   {
      callback = new_callback;
   }

   void activate(KeyBind *kb, event_t *ev) { callback(ev); }
};

class FunctionInputAction : public InputAction
{
protected:
   void(*callback)(event_t *ev);

public:
   FunctionInputAction(const char *new_name,
                       input_action_category_e new_category,
                       void(*new_callback)(event_t *ev))
      : InputAction(new_name, new_category)
   {
      callback = new_callback;
   }

   void activate(KeyBind *kb, event_t *ev) { callback(ev); }
   void deactivate(KeyBind *kb, event_t *ev) { callback(ev); }
};

class VariableInputAction : public InputAction
{
private:
   int *var;

public:
   VariableInputAction(const char *new_name,
                       input_action_category_e new_category,
                       int *new_variable)
      : InputAction(new_name, new_category)
   {
      var = new_variable;
      *var = 0;
   }

   void activate(KeyBind *kb, event_t *ev) { (*var)++; }
   void deactivate(KeyBind *kb, event_t *ev) { if(*var) { (*var)--; } }
   bool active() { if(*var) { return true; } return false; }
};

class CommandInputAction : public InputAction
{
protected:
   uint32_t activation_count;
   bool activated_from_key[KBSS_NUM_KEYS];

public:
   CommandInputAction(const char *new_name)
      : InputAction(new_name, kac_command), activation_count(0) {}

   bool mayActivate(KeyBind *kb) { return true; }

   bool isBatchCommand()
   {
      if(C_CommandIsBatch(hidden_name))
         return true;

      return false;
   }

   void activate(KeyBind *kb, event_t *ev)
   {
      int key_number = kb->getKeyNumber();

      if(!activated_from_key[key_number])
      {
         activated_from_key[key_number] = true;
         activation_count++;

         // [CG] Batches have to be activated on keypress.
         if(isBatchCommand())
            C_RunTextCmd(hidden_name);
      }
   }

   void deactivate(KeyBind *kb, event_t *ev)
   {
      if(kb)
      {
         activated_from_key[kb->getKeyNumber()] = false;
         if(activation_count)
            activation_count--;
      }
      else
      {
         memset(activated_from_key, 0, sizeof(bool) * KBSS_NUM_KEYS);
         activation_count = 0;
      }
   }

   void Think()
   {
      if(consoleactive)
         return;

      if(!activation_count)
         return;

      // [CG] Batches have their own ticker
      if(isBatchCommand())
         return;

      C_RunTextCmd(getName());
   }

   void print() const { /* Do not print in the action list */ }
};

int KeyBindingsSubSystem::getCategoryIndex(int category)
{
   int i, shifted;

   for(i = 0 ;; i++)
   {
      if((shifted = 1 << i) >= kac_max)
         return -1;

      if(shifted == category)
         return i;
   }
}

void KeyBindingsSubSystem::updateBoundKeyDescription(InputAction *action)
{
   qstring description;
   KeyBind *kb = NULL;
   bool found_bind = false;

   while((kb = actions_to_keys->keyIterator(kb, action->getName())))
   {
      if(found_bind)
         description.concat(" + ");
      else
         found_bind = true;
      description.concat(kb->getKeyName());
   }

   if(found_bind)
      action->setDescription(description.getBuffer());
   else
      action->setDescription("none");
}

void KeyBindingsSubSystem::createBind(InputKey *key, InputAction *action,
                                      unsigned short flags)
{
   KeyBind *kb = NULL;
   const char *aname = action->getName();
   size_t aname_len = strlen(aname);

   // Check if this key is already bound to this action, as there's no sense
   // in duplicate binds.
   while((kb = keys_to_actions->keyIterator(kb, key->getName())))
   {
      const char *kb_aname;
      size_t kb_aname_len;
      size_t max_len;

      if(kb->getFlags() != flags)
         continue; // Flags don't match, check next bind.

      kb_aname = kb->getActionName();
      kb_aname_len = strlen(kb_aname);

      if(kb_aname_len > aname_len)
         max_len = kb_aname_len;
      else
         max_len = aname_len;

      if(strncasecmp(aname, kb_aname, max_len))
         continue; // Action names don't match, check next bind.

      // Key is already bound to this action, so don't bind it again.
      return;
   }

   kb = new KeyBind(
      key->getNumber(),
      key->getName(),
      action->getName(),
      action->getCategory(),
      flags
   );
   actions_to_keys->addObject(kb);
   keys_to_actions->addObject(kb);

   if(actions_to_keys->getLoadFactor() > KBSS_MAX_LOAD_FACTOR)
      actions_to_keys->rebuild(actions_to_keys->getNumChains() * 2);

   if(keys_to_actions->getLoadFactor() > KBSS_MAX_LOAD_FACTOR)
      keys_to_actions->rebuild(keys_to_actions->getNumChains() * 2);

   key_bindings.updateBoundKeyDescription(action);
}

void KeyBindingsSubSystem::removeBind(KeyBind **kb)
{
   InputAction *action =
      names_to_actions->objectForKey((*kb)->getActionName());

   keys_to_actions->removeObject(*kb);
   actions_to_keys->removeObject(*kb);
   delete *kb;

   if(action)
      key_bindings.updateBoundKeyDescription(action);
}

void KeyBindingsSubSystem::bindKeyToAction(InputKey *key,
                                           const char *action_name,
                                           unsigned short flags)
{
   InputAction *action = NULL;
   char *real_action_name = (char *)action_name;

   if(action_name[0] == '+')
      flags |= KeyBind::activate_only;
   else if(action_name[0] == '-')
      flags |= KeyBind::deactivate_only;

   if(flags & (KeyBind::activate_only | KeyBind::deactivate_only))
      real_action_name++;

   if(!(action = names_to_actions->objectForKey(real_action_name)))
   {
      addCommandAction(real_action_name);
      action = names_to_actions->objectForKey(real_action_name);
   }

   key_bindings.createBind(key, action, flags);
}

void KeyBindingsSubSystem::bindKeyToActions(const char *key_name,
                                            const qstring &action_names)
{
   unsigned short flags = 0;
   size_t key_name_length = strlen(key_name);
   size_t actionLength;
   qstring token;
   InputKey *key;

   if(!key_name_length)
   {
      C_Printf(FC_ERROR "empty key name.\n");
      return;
   }

   if(key_name_length > 1)
   {
      if(key_name[0] == '+')
      {
         flags |= KeyBind::press_only;
         key_name++;
      }
      else if(key_name[0] == '-')
      {
         flags |= KeyBind::release_only;
         key_name++;
      }
   }

   if(!(key = names_to_keys->objectForKey(key_name)))
   {
      C_Printf(FC_ERROR "unknown key '%s'\n", key_name);
      return;
   }

   actionLength = action_names.length();

   if(!actionLength)
   {
      C_Printf(FC_ERROR "empty action list\n");
      return;
   }

   for(size_t i = 0; i < actionLength; i++)
   {
      char c = action_names.charAt(i);

      if(c == '\"')
         continue;

      if(c == ';')
      {
         if(token.length())
         {
            key_bindings.bindKeyToAction(key, token.constPtr(), flags);
            token.clear();
         }
      }
      else
         token += c;
   }

   // [CG] If the action list didn't end in a ";", then there is one more
   //      action to bind.
   if(token.length())
      key_bindings.bindKeyToAction(key, token.constPtr(), flags);
}

void KeyBindingsSubSystem::setKeyBindingsFile(const char *filename)
{
   if(cfg_file)
      efree(cfg_file);

   cfg_file = estrdup(filename);
}

void KeyBindingsSubSystem::setBindingAction(const char *new_binding_action)
{
   binding_action = new_binding_action;
}

const char* KeyBindingsSubSystem::getBindingAction()
{
   return binding_action;
}

KeyBindingsSubSystem::KeyBindingsSubSystem()
{
   names_to_keys = new EHashTable<
      InputKey, ENCStringHashKey, &InputKey::key, &InputKey::links
   >(KBSS_NUM_KEYS);

   names_to_actions = new EHashTable<
      InputAction, ENCStringHashKey, &InputAction::key, &InputAction::links
   >(KBSS_NUM_KEYS);

   keys_to_actions = new EHashTable<
      KeyBind, ENCStringHashKey, &KeyBind::keys_to_actions_key,
      &KeyBind::key_to_action_links
   >(KBSS_NUM_KEYS);

   actions_to_keys = new EHashTable<
      KeyBind, ENCStringHashKey, &KeyBind::actions_to_keys_key,
      &KeyBind::action_to_key_links
   >(KBSS_NUM_KEYS);

   cfg_file = NULL;
}

void KeyBindingsSubSystem::initialize()
{
   int i;
   qstring key_name;
   command_t *command;

   // various names for different keys
   addKey(KEYD_RIGHTARROW, rightarrow);
   addKey(KEYD_LEFTARROW,  leftarrow);
   addKey(KEYD_UPARROW,    uparrow);
   addKey(KEYD_DOWNARROW,  downarrow);
   addKey(KEYD_ESCAPE,     escape);
   addKey(KEYD_ENTER,      enter);
   addKey(KEYD_TAB,        tab);
   addKey(KEYD_F1,         f1);
   addKey(KEYD_F2,         f2);
   addKey(KEYD_F3,         f3);
   addKey(KEYD_F4,         f4);
   addKey(KEYD_F5,         f5);
   addKey(KEYD_F6,         f6);
   addKey(KEYD_F7,         f7);
   addKey(KEYD_F8,         f8);
   addKey(KEYD_F9,         f9);
   addKey(KEYD_F10,        f10);
   addKey(KEYD_F11,        f11);
   addKey(KEYD_F12,        f12);

   addKey(KEYD_BACKSPACE,  backspace);
   addKey(KEYD_PAUSE,      pause);
   addKey(KEYD_MINUS,      -);
   addKey(KEYD_RSHIFT,     shift);
   addKey(KEYD_RCTRL,      ctrl);
   addKey(KEYD_RALT,       alt);
   addKey(KEYD_CAPSLOCK,   capslock);

   addKey(KEYD_INSERT,     insert);
   addKey(KEYD_HOME,       home);
   addKey(KEYD_END,        end);
   addKey(KEYD_PAGEUP,     pgup);
   addKey(KEYD_PAGEDOWN,   pgdn);
   addKey(KEYD_SCROLLLOCK, scrolllock);
   addKey(KEYD_SPACEBAR,   space);
   addKey(KEYD_NUMLOCK,    numlock);
   addKey(KEYD_DEL,        delete);

   addKey(KEYD_MOUSE1,     mouse1);
   addKey(KEYD_MOUSE2,     mouse2);
   addKey(KEYD_MOUSE3,     mouse3);
   addKey(KEYD_MOUSE4,     mouse4);
   addKey(KEYD_MOUSE5,     mouse5);
   addKey(KEYD_MWHEELUP,   wheelup);
   addKey(KEYD_MWHEELDOWN, wheeldown);

   addKey(KEYD_JOY1,       joy1);
   addKey(KEYD_JOY2,       joy2);
   addKey(KEYD_JOY3,       joy3);
   addKey(KEYD_JOY4,       joy4);
   addKey(KEYD_JOY5,       joy5);
   addKey(KEYD_JOY6,       joy6);
   addKey(KEYD_JOY7,       joy7);
   addKey(KEYD_JOY8,       joy8);

   addKey(KEYD_KP0,        kp_0);
   addKey(KEYD_KP1,        kp_1);
   addKey(KEYD_KP2,        kp_2);
   addKey(KEYD_KP3,        kp_3);
   addKey(KEYD_KP4,        kp_4);
   addKey(KEYD_KP5,        kp_5);
   addKey(KEYD_KP6,        kp_6);
   addKey(KEYD_KP7,        kp_7);
   addKey(KEYD_KP8,        kp_8);
   addKey(KEYD_KP9,        kp_9);
   addKey(KEYD_KPPERIOD,   kp_period);
   addKey(KEYD_KPDIVIDE,   kp_slash);
   addKey(KEYD_KPMULTIPLY, kp_star);
   addKey(KEYD_KPMINUS,    kp_minus);
   addKey(KEYD_KPPLUS,     kp_plus);
   addKey(KEYD_KPENTER,    kp_enter);
   addKey(KEYD_KPEQUALS,   kp_equals);
   addKey('`',             tilde);

   for(i = 0; i < KBSS_NUM_KEYS; ++i)
   {
      if(keys[i])
         continue;

      // build generic name if not set yet
      if(isprint(i))
         key_name.Printf(0, "%c", i);
      else
         key_name.Printf(0, "key%i", i);

      keys[i] = new InputKey(i, key_name.constPtr());
      names_to_keys->addObject(*keys[i]);
   }

   // Player-class actions
   addVariableAction(forward,    kac_player);
   addVariableAction(backward,   kac_player);
   addVariableAction(left,       kac_player);
   addVariableAction(right,      kac_player);
   addVariableAction(moveleft,   kac_player);
   addVariableAction(moveright,  kac_player);
   addVariableAction(use,        kac_player);
   addVariableAction(strafe,     kac_player);
   addVariableAction(attack,     kac_player);
   addVariableAction(flip,       kac_player);
   addVariableAction(speed,      kac_player);
   addVariableAction(jump,       kac_player);
   addVariableAction(autorun,    kac_player);
   addVariableAction(mlook,      kac_player);
   addVariableAction(lookup,     kac_player);
   addVariableAction(lookdown,   kac_player);
   addVariableAction(center,     kac_player);
   addVariableAction(flyup,      kac_player);
   addVariableAction(flydown,    kac_player);
   addVariableAction(flycenter,  kac_player);
   addVariableAction(weapon1,    kac_player);
   addVariableAction(weapon2,    kac_player);
   addVariableAction(weapon3,    kac_player);
   addVariableAction(weapon4,    kac_player);
   addVariableAction(weapon5,    kac_player);
   addVariableAction(weapon6,    kac_player);
   addVariableAction(weapon7,    kac_player);
   addVariableAction(weapon8,    kac_player);
   addVariableAction(weapon9,    kac_player);
   addVariableAction(nextweapon, kac_player);
   addVariableAction(weaponup,   kac_player);
   addVariableAction(weapondown, kac_player);

   // HUD-class actions
   addVariableAction(frags, kac_hud);

   // Menu-class actions
   addVariableAction(menu_toggle,   kac_menu);
   addVariableAction(menu_help,     kac_menu);
   addVariableAction(menu_setup,    kac_menu);
   addVariableAction(menu_confirm,  kac_menu);
   addVariableAction(menu_previous, kac_menu);
   addVariableAction(menu_left,     kac_menu);
   addVariableAction(menu_right,    kac_menu);
   addVariableAction(menu_pageup,   kac_menu);
   addVariableAction(menu_pagedown, kac_menu);
   addVariableAction(menu_contents, kac_menu);
   addVariableAction(menu_up,       kac_menu);
   addVariableAction(menu_down,     kac_menu);

   addVariableAction(map_right,   kac_map);
   addVariableAction(map_left,    kac_map);
   addVariableAction(map_up,      kac_map);
   addVariableAction(map_down,    kac_map);
   addVariableAction(map_zoomin,  kac_map);
   addVariableAction(map_zoomout, kac_map);
   addVariableAction(map_toggle,  kac_map);
   addVariableAction(map_gobig,   kac_map);
   addVariableAction(map_follow,  kac_map);
   addVariableAction(map_mark,    kac_map);
   addVariableAction(map_clear,   kac_map);
   addVariableAction(map_grid,    kac_map);

   // Console-class actions
   addVariableAction(console_pageup,    kac_console);
   addVariableAction(console_pagedown,  kac_console);
   addVariableAction(console_toggle,    kac_console);
   addVariableAction(console_tab,       kac_console);
   addVariableAction(console_enter,     kac_console);
   addVariableAction(console_up,        kac_console);
   addVariableAction(console_down,      kac_console);
   addVariableAction(console_backspace, kac_console);

   for(i = 0; i < CMDCHAINS; i++)
   {
      for(command = cmdroots[i]; command; command = command->next)
         addCommandAction(command->name);
   }
}

const char* KeyBindingsSubSystem::getBoundKeys(const char *action_name)
{
   InputAction *action = names_to_actions->objectForKey(action_name);

   if(!action)
   {
      addCommandAction(action_name);
      action = names_to_actions->objectForKey(action_name);
   }

   return action->getDescription();
}

const char* KeyBindingsSubSystem::getFirstBoundKey(const char *action_name)
{
   InputKey *key = NULL;
   KeyBind *kb = NULL;
   KeyBind *previous_kb = NULL;

   while((kb = actions_to_keys->keyIterator(kb, action_name)))
      previous_kb = kb;

   if(!previous_kb)
      return "none";

   if(!(key = names_to_keys->objectForKey(previous_kb->getKeyName())))
      return "none";

   return key->getName();
}

void KeyBindingsSubSystem::reEnableKeys()
{
   for(int i = 0; i < KBSS_NUM_KEYS; ++i)
      keys[i]->enable();
}


void KeyBindingsSubSystem::reset()
{
   KeyBind *kb = NULL;
   InputAction *action = NULL;

   reEnableKeys();

   while((kb = keys_to_actions->tableIterator(kb)))
      kb->release();

   while((action = names_to_actions->tableIterator(action)))
      action->deactivate(NULL, NULL);
}

bool KeyBindingsSubSystem::handleKeyEvent(event_t *ev, int categories)
{
   static bool ctrldown;
   static bool altdown;
   InputKey *key = NULL;
   InputAction *action = NULL;
   KeyBind *kb = NULL;
   bool processed_event = false;

   // do not index out of bounds
   if(ev->data1 >= KBSS_NUM_KEYS)
      return false;

   // got a key - close box
   MN_PopWidget();

   // Check for ctrl/alt keys.
   if(ev->data1 == KEYD_RCTRL)
      ctrldown = (ev->type == ev_keydown);
   else if(ev->data1 == KEYD_RALT)
      altdown = (ev->type == ev_keydown);

   // netgame disconnect binding
   if(opensocket && ctrldown && ev->data1 == 'd')
   {
      char buffer[128];

      psnprintf(buffer, sizeof(buffer),
                "disconnect from server?\n\n%s", DEH_String("PRESSYN"));
      MN_Question(buffer, "disconnect leaving");

      // dont get stuck thinking ctrl is down
      ctrldown = false;

      return true;
   }

   // Alt-tab binding (ignore whatever is bound to tab if alt is down)
   if(altdown && ev->data1 == KEYD_TAB)
   {
      altdown = false;
      return true;
   }

   key = keys[tolower(ev->data1)];

   while((kb = keys_to_actions->keyIterator(kb, key->getName())))
   {
      if((ev->type == ev_keyup) && (kb->isPressOnly()))
      {
         kb->release();
         continue;
      }

      if((ev->type == ev_keydown) && (kb->isReleaseOnly()))
      {
         kb->press();
         continue;
      }

      if(!(action = names_to_actions->objectForKey(kb->getActionName())))
      {
         doom_printf("Action [%s] not found.", kb->getActionName());
         continue;
      }

      printf(
         "%d: Categories/Action Category: %d/%d.\n",
         gametic,
         categories,
         action->getCategory()
      );

      if((categories & action->getCategory()) == 0)
      {
         printf("Skipping action.\n");
         continue;
      }

      if(!key->isDisabled())
      {
         if(!processed_event)
            processed_event = action->handleEvent(ev, kb);
         else
            action->handleEvent(ev, kb);
      }

   }

   if(processed_event)
      key->disable();

   return processed_event;
}

void KeyBindingsSubSystem::runInputActions()
{
   InputAction *action = NULL;

   while((action = names_to_actions->tableIterator(action)))
      action->Think();
}

void KeyBindingsSubSystem::loadKeyBindings()
{
   char *temp = NULL;
   size_t len;
   DWFILE dwfile, *file = &dwfile;
   InputAction *action = NULL;

   len = M_StringAlloca(&temp, 1, 18, usergamepath);

   // haleyjd 11/23/06: use basegamepath
   // haleyjd 08/29/09: allow use_doom_config override
   if(GameModeInfo->type == Game_DOOM && use_doom_config)
      psnprintf(temp, len, "%s/doom/keys.csc", userpath);
   else
      psnprintf(temp, len, "%s/keys.csc", usergamepath);


   setKeyBindingsFile(temp);

   if(access(cfg_file, R_OK))
   {
      C_Printf("keys.csc not found, using defaults\n");
      D_OpenLump(file, W_GetNumForName("KEYDEFS"));
   }
   else
      D_OpenFile(file, cfg_file, "r");

   if(!D_IsOpen(file))
      I_Error("G_LoadDefaults: couldn't open default key bindings\n");

   // haleyjd 03/08/06: test for zero length
   if(!D_IsLump(file) && D_FileLength(file) == 0)
   {
      // try the lump because the file is zero-length...
      C_Printf("keys.csc is zero length, trying KEYDEFS\n");
      D_Fclose(file);
      D_OpenLump(file, W_GetNumForName("KEYDEFS"));
      if(!D_IsOpen(file) || D_FileLength(file) == 0)
         I_Error("G_LoadDefaults: KEYDEFS lump is empty\n");
   }

   C_RunScript(file);

   D_Fclose(file);

   while((action = names_to_actions->tableIterator(action)))
      key_bindings.updateBoundKeyDescription(action);
}

void KeyBindingsSubSystem::saveKeyBindings()
{
   FILE *file;
   bool in_bind;
   InputKey *key = NULL;
   KeyBind *kb = NULL;

   if(!cfg_file)         // check defaults have been loaded
      return;

   if(!(file = fopen(cfg_file, "w")))
   {
      C_Printf(FC_ERROR"Couldn't open keys.csc for write access.\n");
      return;
   }

   // write key bindings
   while((key = names_to_keys->tableIterator(key)))
   {
      // Skip uppercase alphabetical keys.
      if(strlen(key->getName()) == 1 && isalpha(*key->getName()) &&
                                        isupper(*key->getName()))
         continue;

      // [CG] Write normal binds first.
      in_bind = false;
      while((kb = keys_to_actions->keyIterator(kb, key->getName())))
      {
         if(kb->isPressOnly() || kb->isReleaseOnly())
            continue;

         if(!in_bind)
         {
            fprintf(file, "bind %s \"", key->getName());
            in_bind = true;
         }
         else
            fprintf(file, ";");

         if(kb->isActivateOnly())
            fprintf(file, "+%s", kb->getActionName());
         else if(kb->isDeactivateOnly())
            fprintf(file, "-%s", kb->getActionName());
         else
            fprintf(file, "%s", kb->getActionName());
      }

      if(in_bind)
         fprintf(file, "\"\n");

      // [CG] Now write press-only binds.
      in_bind = false;
      while((kb = keys_to_actions->keyIterator(kb, key->getName())))
      {
         if(!kb->isPressOnly())
            continue;

         if(!in_bind)
         {
            fprintf(file, "bind +%s \"", key->getName());
            in_bind = true;
         }
         else
            fprintf(file, ";");

         if(kb->isActivateOnly())
            fprintf(file, "+%s", kb->getActionName());
         else if(kb->isDeactivateOnly())
            fprintf(file, "-%s", kb->getActionName());
         else
            fprintf(file, "%s", kb->getActionName());
      }

      if(in_bind)
         fprintf(file, "\"\n");

      // [CG] Finally write release-only binds.
      in_bind = false;
      while((kb = keys_to_actions->keyIterator(kb, key->getName())))
      {
         if(!kb->isReleaseOnly())
            continue;

         if(!in_bind)
         {
            fprintf(file, "bind -%s \"", key->getName());
            in_bind = true;
         }
         else
            fprintf(file, ";");

         if(kb->isActivateOnly())
            fprintf(file, "+%s", kb->getActionName());
         else if(kb->isDeactivateOnly())
            fprintf(file, "-%s", kb->getActionName());
         else
            fprintf(file, "%s", kb->getActionName());
      }

      if(in_bind)
         fprintf(file, "\"\n");
   }

   // Save command batches
   C_SaveCommandBatches(file);

   fclose(file);
}

InputKey* KeyBindingsSubSystem::getKey(int index)
{
   if(index >= KBSS_NUM_KEYS)
   {
      I_Error(
         "KeyBindingsSubSystem::getKey: requested key index %d exceeds %d.\n",
         index,
         KBSS_NUM_KEYS - 1
      );
   }
   return keys[index];
}

InputKey* KeyBindingsSubSystem::getKey(const char *key_name)
{
   return names_to_keys->objectForKey(key_name);
}

InputAction* KeyBindingsSubSystem::getAction(const char *action_name)
{
   return names_to_actions->objectForKey(action_name);
}

KeyBind* KeyBindingsSubSystem::keyBindIterator(KeyBind *kb)
{
   return keys_to_actions->tableIterator(kb);
}

KeyBind* KeyBindingsSubSystem::keyBindIterator(KeyBind *kb,
                                               const char *key_name)
{
   return keys_to_actions->keyIterator(kb, key_name);
}

InputAction* KeyBindingsSubSystem::actionIterator(InputAction *action)
{
   return names_to_actions->tableIterator(action);
}

// Action variables
// These variables are asserted as positive values when the action
// they represent has been performed by the player via key pressing.

// Game Actions -- These are handled in g_game.c
int action_forward;     // forward movement
int action_backward;    // backward movement
int action_left;        // left movement
int action_right;       // right movement
int action_moveleft;    // key-strafe left
int action_moveright;   // key-strafe right
int action_use;         // object activation
int action_speed;       // running
int action_attack;      // firing current weapon
int action_strafe;      // strafe in any direction
int action_flip;        // 180 degree turn
int action_jump;        // jump
int action_mlook;       // mlook activation
int action_lookup;      // key-look up
int action_lookdown;    // key-look down
int action_flyup;       // fly upwards
int action_flydown;     // fly downwards
int action_flycenter;   // fly straight ahead
int action_center;      // key-look centerview
int action_weapon1;     // select weapon 1
int action_weapon2;     // select weapon 2
int action_weapon3;     // select weapon 3
int action_weapon4;     // select weapon 4
int action_weapon5;     // select weapon 5
int action_weapon6;     // select weapon 6
int action_weapon7;     // select weapon 7
int action_weapon8;     // select weapon 8
int action_weapon9;     // select weapon 9
int action_nextweapon;  // toggle to next-favored weapon
int action_weaponup;    // haleyjd: next weapon in order
int action_weapondown;  // haleyjd: prev weapon in order
int action_frags;       // show frags
int action_autorun;     // autorun

// Menu Actions -- handled by MN_Responder
int action_menu_help;
int action_menu_toggle;
int action_menu_setup;
int action_menu_up;
int action_menu_down;
int action_menu_confirm;
int action_menu_previous;
int action_menu_left;
int action_menu_right;
int action_menu_pageup;
int action_menu_pagedown;
int action_menu_contents;

// AutoMap Actions -- handled by AM_Responder
int action_map_right;
int action_map_left;
int action_map_up;
int action_map_down;
int action_map_zoomin;
int action_map_zoomout;
int action_map_toggle;
int action_map_gobig;
int action_map_follow;
int action_map_mark;
int action_map_clear;
int action_map_grid;

// Console Actions -- handled by C_Responder
int action_console_pageup;
int action_console_pagedown;
int action_console_toggle;
int action_console_tab;
int action_console_enter;
int action_console_up;
int action_console_down;
int action_console_backspace;

//===========================================================================
//
// Binding selection widget
//
// For menu: when we select to change a key binding the widget is used
// as the drawer and responder
//
//===========================================================================

//
// G_BindDrawer
//
// Draw the prompt box
//
void G_BindDrawer()
{
   const char *msg = "\n -= input new key =- \n";
   int x, y, width, height;

   // draw the menu in the background
   MN_DrawMenu(current_menu);

   width  = V_FontStringWidth(menu_font_normal, msg);
   height = V_FontStringHeight(menu_font_normal, msg);
   x = (SCREENWIDTH  - width)  / 2;
   y = (SCREENHEIGHT - height) / 2;

   // draw box
   V_DrawBox(x - 4, y - 4, width + 8, height + 8);

   // write text in box
   V_FontWriteText(menu_font_normal, msg, x, y);
}

//
// G_BindResponder
//
// Responder for widget
//
bool G_BindResponder(event_t *ev)
{
   InputKey *key;
   KeyBind *kb = NULL;
   InputAction *action = NULL;
   bool found_bind = false;
   DLListItem<KeyBind> *binds = NULL;
   const char *binding_action = key_bindings.getBindingAction();

   if(ev->type != ev_keydown)
      return false;

   // do not index out of bounds
   if(ev->data1 >= KBSS_NUM_KEYS)
      return false;

   key = key_bindings.getKey(tolower(ev->data1));

   // got a key - close box
   current_menuwidget = NULL;

   if(action_menu_toggle) // cancel
   {
      action_menu_toggle = false;
      return true;
   }

   if(!(action = key_bindings.getAction(binding_action)))
   {
      C_Printf(FC_ERROR "unknown action '%s'\n", binding_action);
      return true;
   }

   // First check if the key is already bound to the action; if it is, the
   // player is unbinding the key from the action, so remove the bind.

   while((kb = key_bindings.keyBindIterator(kb, key->getName())))
      kb->unbind_links.insert(kb, &binds);

   while(binds)
   {
      InputAction *action = NULL;

      kb = binds->dllObject;
      binds->remove();

      if(!(action = key_bindings.getAction(kb->getActionName())))
         continue;

      if(!(strcasecmp(binding_action, action->getName())))
      {
         found_bind = true;
         key_bindings.removeBind(&kb);
      }
   }

   // If the bind wasn't found, the player is binding, not unbinding, so create
   // a new bind.
   if(!found_bind)
      key_bindings.createBind(key, action, 0);

   return true;
}

//
// G_EditBinding
//
// Main Function
//
void G_EditBinding(const char *action_name)
{
   current_menuwidget = &key_bindings.binding_widget;
   key_bindings.setBindingAction(action_name);
   MN_PushWidget(&key_bindings.binding_widget);
}

//===========================================================================
//
// Load/Save defaults
//
//===========================================================================

// default script:

//=============================================================================
//
// Console Commands
//

//
// bind
//
// Bind a key to one or more actions.
//
CONSOLE_COMMAND(bind, 0)
{
   if(Console.argc == 1)
   {
      InputKey *key = NULL;
      InputAction *action = NULL;
      KeyBind *kb = NULL;
      const char *key_name = Console.argv[0]->constPtr();
      bool found_bind = false;

      if(!(key = key_bindings.getKey(key_name)))
      {
         C_Printf(FC_ERROR "no such key!\n");
         return;
      }

      while((kb = key_bindings.keyBindIterator(kb, key_name)))
      {
         if(!(action = key_bindings.getAction(kb->getActionName())))
            continue;

         C_Printf(
            "%s bound to %s (category %d)\n",
            key->getName(),
            action->getName(),
            key_bindings.getCategoryIndex(action->getCategory())
         );

         found_bind = true;
      }

      if(!found_bind)
         C_Printf("%s is not bound.\n", key->getName());
   }
   else if(Console.argc == 2)
   {
      key_bindings.bindKeyToActions(
         Console.argv[0]->constPtr(), *(Console.argv[1])
      );
   }
   else
      C_Printf("usage: bind key <action>\n");
}

// haleyjd: utility functions
CONSOLE_COMMAND(listactions, 0)
{
   InputAction *action = NULL;

   while((action = key_bindings.actionIterator(action)))
      action->print();
}

CONSOLE_COMMAND(listkeys, 0)
{
   int i;

   for(i = 0; i < KBSS_NUM_KEYS; ++i)
      C_Printf("%s\n", key_bindings.getKey(i)->getName());
}

static int G_allActionCategories()
{
   static int action_categories = -1;
   int current_flags = (kac_none << 1);

   if(action_categories == -1)
   {
      action_categories = current_flags;
      while(current_flags < (kac_max - 1))
      {
         current_flags <<= 1;
         action_categories |= current_flags;
      }
   }

   return action_categories;
}

CONSOLE_COMMAND(unbind, 0)
{
   int category = -1;
   const char *kname;
   bool found_bind = false;
   bool ignored_console_or_menu_actions = false;
   KeyBind *kb = NULL;
   InputKey *key = NULL;
   DLListItem<KeyBind> *binds = NULL;

   if(Console.argc < 1)
   {
      C_Printf("usage: unbind key [category]\n");
      return;
   }

   kname = Console.argv[0]->constPtr();

   // allow specification of a binding category
   if(Console.argc == 2)
   {
      int category_flags;

      category = Console.argv[1]->toInt();
      category_flags = 1 << category;

      if((!(category_flags & G_allActionCategories())) ||
         (category_flags == kac_none))
      {
         C_Printf(FC_ERROR "invalid action category %d\n", category);
         C_Printf(
            FC_ERROR " %d/%d, %d/%d, %d.\n",
            category,
            G_allActionCategories(),
            category_flags,
            kac_none,
            category_flags & G_allActionCategories()
         );
         return;
      }
   }

   if(!(key = key_bindings.getKey(kname)))
   {
      C_Printf("unknown key %s\n", kname);
      return;
   }

   while((kb = key_bindings.keyBindIterator(kb, key->getName())))
      kb->unbind_links.insert(kb, &binds);

   while(binds)
   {
      InputAction *action = NULL;
      input_action_category_e action_category;

      kb = binds->dllObject;
      binds->remove();

      if(!(action = key_bindings.getAction(kb->getActionName())))
         continue;

      action_category = action->getCategory();

      if(action_category == kac_menu || action_category == kac_console)
      {
         ignored_console_or_menu_actions = true;
         continue;
      }

      found_bind = true;
      key_bindings.removeBind(&kb);
   }

   if(found_bind)
   {
      if(category == -1)
      {
         C_Printf("unbound key %s from all actions\n", kname);
         if(ignored_console_or_menu_actions)
            C_Printf(FC_ERROR " console and menu actions ignored\n");
      }
      else
         C_Printf("unbound key %s from category %d actions\n", kname, category);
   }
   else if(category == -1)
      C_Printf("No actions bound to key %s.\n", kname);
   else
      C_Printf("No actions bound to key %s in category %d.\n", kname, category);
}


CONSOLE_COMMAND(unbindall, 0)
{
   KeyBind *kb = NULL;

   while((kb = key_bindings.keyBindIterator(NULL)))
      key_bindings.removeBind(&kb);

   C_Printf("key bindings cleared\n");
}

//
// bindings
//
// haleyjd 12/11/01
// list all active bindings to the console
//
CONSOLE_COMMAND(bindings, 0)
{
   KeyBind *kb = NULL;

   while((kb = key_bindings.keyBindIterator(kb)))
      C_Printf("%s : %s\n", kb->getKeyName(), kb->getActionName());
}

void G_Bind_AddCommands()
{
   C_AddCommand(bind);
   C_AddCommand(listactions);
   C_AddCommand(listkeys);
   C_AddCommand(unbind);
   C_AddCommand(unbindall);
   C_AddCommand(bindings);
}

// EOF

// Emacs style mode select -*- C -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2001 James Haley
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
// Console commands
//
// basic console commands and variables for controlling
// the console itself.
// 
// By Simon Howard
//
// NETCODE_FIXME -- CONSOLE_FIXME
// There's a lot of cruft in here that can be done without.
//
//-----------------------------------------------------------------------------

#include "d_io.h"  // SoM 3/12/2002: strncasecmp

#include "c_io.h"
#include "c_runcmd.h"
#include "c_net.h"
#include "c_runcmd.h"

#include "m_random.h"

#include "a_small.h" // haleyjd

// version hack

char *verdate_hack = (char *)version_date;
char *vername_hack = (char *)version_name;
char *vertime_hack = (char *)version_time;

               /************* constants *************/

// version
CONST_INT(version);
CONSOLE_CONST(version, version);

// version date
CONST_STRING(verdate_hack);
CONSOLE_CONST(ver_date, verdate_hack);

// version time -- haleyjd 02/15/02
CONST_STRING(vertime_hack);
CONSOLE_CONST(ver_time, vertime_hack);

// version name
CONST_STRING(vername_hack);
CONSOLE_CONST(ver_name, vername_hack);

                /************* aliases ***************/
CONSOLE_COMMAND(alias, 0)
{
   alias_t *alias;
   char *temp;

   // haleyjd 04/14/03: rewritten
   
   if(!Console.argc)
   {
      // list em
      C_Printf(FC_HI"alias list:" FC_NORMAL "\n\n");
      
      alias = aliases.next;
      if(!alias)
      {
         C_Printf("(empty)\n");
      }
      else
      {
         while(alias)
         {
            C_Printf("\"%s\": \"%s\"\n", alias->name, alias->command);
            alias = alias->next;
         }
      }
      return;
   }
  
   if(Console.argc == 1)  // only one, remove alias
   {
      C_RemoveAlias(Console.argv[0]);
      return;
   }
   
   // find it or make a new one
   
   temp = Console.args + strlen(Console.argv[0]);
   while(*temp == ' ')
      temp++;
   
   C_NewAlias(Console.argv[0], temp);
}

// %opt for aliases
CONST_STRING(cmdoptions);
CONSOLE_CONST(opt, cmdoptions);

// command list
CONSOLE_COMMAND(cmdlist, 0)
{
   command_t *current;
   int i;
   int charnum = 33;
   int maxchar = 'z';

   // SoM: This could be a little better
   char *mask = NULL;
   int  masklen = 0;

   // haleyjd 07/08/04: optional filter parameter -- the provided
   // character will be used to make the loop below run only for one
   // letter
   if(Console.argc == 1)
   {
      if(strlen(Console.argv[0]) == 1)
         charnum = maxchar = Console.argv[0][0];
      else
      {
         charnum = maxchar = Console.argv[0][0];
         mask = Console.argv[0];
         masklen = strlen(mask);
      }
   }
   
   // list each command from the hash chains
   
   // 5/8/99 change: use hash table and 
   // alphabetical order by first letter
   // haleyjd 07/08/04: fixed to run for last letter

   for(; charnum <= maxchar; ++charnum) // go thru each char in alphabet
   {
      for(i = 0; i < CMDCHAINS; ++i)
      {
         for(current = cmdroots[i]; current; current = current->next)
         {
            if(current->name[0] == charnum && 
               (!mask || !strncasecmp(current->name, mask, masklen)) &&
               !(current->flags & cf_hidden))
            {
               C_Printf("%s\n", current->name);
            }
         }
      }
   }
}

// console height

VARIABLE_INT(c_height,  NULL,                   20, 200, NULL);
CONSOLE_VARIABLE(c_height, c_height, 0) {}

// console speed

VARIABLE_INT(c_speed,   NULL,                   1, 200, NULL);
CONSOLE_VARIABLE(c_speed, c_speed, 0) {}

// echo string to console

CONSOLE_COMMAND(echo, 0)
{
   C_Puts(Console.args);
}

// delay in console

CONSOLE_COMMAND(delay, 0)
{
  C_BufferDelay(Console.cmdtype, Console.argc ? atoi(Console.argv[0]) : 1);
}

// flood the console with crap
// .. such a great and useful command

CONSOLE_COMMAND(flood, 0)
{
  int a;

  for(a=0; a<300; a++)
    C_Printf("%c\n", a%64 + 32);
}

CONSOLE_COMMAND(quote, 0) {}

// haleyjd: dumplog command to write out the console to file

CONSOLE_COMMAND(dumplog, 0)
{
   if(!Console.argc)
      C_Printf("usage: dumplog filename\n");
   else
      C_DumpMessages(Console.argv[0]);
}

// haleyjd 09/07/03: true console logging commands

CONSOLE_COMMAND(openlog, 0)
{
   if(!Console.argc)
      C_Printf("usage: openlog filename\n");
   else
      C_OpenConsoleLog(Console.argv[0]);
}

CONSOLE_COMMAND(closelog, 0)
{
   C_CloseConsoleLog();
}


// SoM: omg why didn't we have this before?
CONSOLE_COMMAND(cvarhelp, 0)
{
   command_t *current;
   variable_t *var;
   int i, count;
   char *name;


   if(Console.argc != 1)
   {
      C_Printf("Usage: cvarhelp <variablename>\n"
               "Outputs a list of possible values for the given variable\n");
      return;
   }

   name = Console.argv[0];

   for(i = 0; i < CMDCHAINS; ++i)
   {
      for(current = cmdroots[i]; current; current = current->next)
      {
         if(current->type == ct_variable &&
            !strcasecmp(current->name, name) &&
            !(current->flags & cf_hidden))
         {
            var = current->variable;

            switch(var->type)
            {
            case vt_int:
               if(var->defines && var->min <= var->max)
               {
                  C_Printf("Possible values for '%s': ", name);
                  for(count = var->min; count <= var->max; count++)
                  {
                     C_Printf("%s", var->defines[count - var->min]);
                  }
               }
               else
               {
                  // haleyjd 04/21/10: respect use of UL
                  if(var->min == UL && var->max == UL)
                  {
                     C_Printf("'%s' accepts any integer value\n", name);
                  }
                  else if(var->min == UL)
                  {
                     C_Printf("Value range for '%s': any integer <= %d\n", 
                              name, var->max);
                  }
                  else if(var->max == UL)
                  {
                     C_Printf("Value range for '%s': any integer >= %d\n",
                              name, var->min);
                  }
                  else
                  {
                     C_Printf("Value range for '%s': %d through %d\n", 
                              name, var->min, var->max);
                  }
               }
               break;

            case vt_float:
               // haleyjd 04/21/10: implemented vt_float
                  if(var->dmin == UL && var->dmax == UL)
                  {
                     C_Printf("'%s' accepts any float value\n", name);
                  }
                  else if(var->dmin == UL)
                  {
                     C_Printf("Value range for '%s': any float <= %f\n", 
                              name, var->dmax);
                  }
                  else if(var->dmax == UL)
                  {
                     C_Printf("Value range for '%s': any float >= %f\n",
                              name, var->dmin);
                  }
                  else
                  {
                     C_Printf("Value range for '%s': %f through %f\n", 
                              name, var->dmin, var->dmax);
                  }
                  break;

            default:
               if(var->max != UL)
               {
                  C_Printf("Value for '%s': String no more than %d characters long\n", 
                           name, var->max);
               }
               else
                  C_Printf("Value for '%s': Unlimited-length string\n", name);
            }

            return;
         }
      }
   }

   C_Printf("Variable %s not found\n", name);
}

        /******** add commands *******/

// command-adding functions in other modules

extern void    Cheat_AddCommands(void);        // m_cheat.c
extern void        G_AddCommands(void);        // g_cmd.c
extern void       HU_AddCommands(void);        // hu_stuff.c
extern void        I_AddCommands(void);        // i_system.c
extern void      net_AddCommands(void);        // d_net.c
extern void        P_AddCommands(void);        // p_cmd.c
extern void        R_AddCommands(void);        // r_main.c
extern void        S_AddCommands(void);        // s_sound.c
extern void       ST_AddCommands(void);        // st_stuff.c
extern void        V_AddCommands(void);        // v_misc.c
extern void       MN_AddCommands(void);        // mn_menu.c
extern void       AM_AddCommands(void);        // am_color.c

extern void       PE_AddCommands(void);        // p_enemy.c  -- haleyjd
extern void   G_Bind_AddCommands(void);        // g_bind.c   -- haleyjd

#ifndef EE_NO_SMALL_SUPPORT
extern void       SM_AddCommands(void);        // a_small.c  -- haleyjd
#endif

extern void      G_DMAddCommands(void);        // g_dmflag.c -- haleyjd
extern void        E_AddCommands(void);        // e_cmd.c    -- haleyjd
extern void P_AddGenLineCommands(void);        // p_genlin.c -- haleyjd
extern void        W_AddCommands(void);        // w_levels.c -- haleyjd

void C_AddCommands()
{
  C_AddCommand(version);
  C_AddCommand(ver_date);
  C_AddCommand(ver_time); // haleyjd
  C_AddCommand(ver_name);
  
  C_AddCommand(c_height);
  C_AddCommand(c_speed);
  C_AddCommand(cmdlist);
  C_AddCommand(delay);
  C_AddCommand(alias);
  C_AddCommand(opt);
  C_AddCommand(echo);
  C_AddCommand(flood);
  C_AddCommand(quote);
  C_AddCommand(dumplog); // haleyjd
  C_AddCommand(openlog);
  C_AddCommand(closelog);

  // SoM: I can never remember the values for a console variable
  C_AddCommand(cvarhelp);
  
  // add commands in other modules
  Cheat_AddCommands();
  G_AddCommands();
  HU_AddCommands();
  I_AddCommands();
  net_AddCommands();
  P_AddCommands();
  R_AddCommands();
  S_AddCommands();
  ST_AddCommands();
  V_AddCommands();
  MN_AddCommands();
  AM_AddCommands();
  PE_AddCommands();  // haleyjd
  G_Bind_AddCommands();
  
#ifndef EE_NO_SMALL_SUPPORT
  SM_AddCommands();
#endif
  
  G_DMAddCommands();
  E_AddCommands();
  P_AddGenLineCommands();
  W_AddCommands();
}

#ifndef EE_NO_SMALL_SUPPORT
static cell AMX_NATIVE_CALL sm_version(AMX *amx, cell *params)
{
   return version;
}

AMX_NATIVE_INFO ccmd_Natives[] =
{
   {"_EngineVersion", sm_version },
   { NULL, NULL }
};
#endif

// EOF


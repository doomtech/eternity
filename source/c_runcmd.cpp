// Emacs style mode select -*- C -*-
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
//--------------------------------------------------------------------------
//
// Command running functions
//
// Running typed commands, or network/linedef triggers. Sending commands over
// the network. Calls handlers for some commands.
//
// By Simon Howard
//
// NETCODE_FIXME -- CONSOLE_FIXME
// Parts of this module also are involved in the netcode cmd problems.
// Mainly, the buffering issue. Commands need to work without being
// delayed an arbitrary amount of time, that won't work with netgames
// properly. Also, command parsing is extremely messy and needs to
// be rewritten. It should be possible to use qstring_t to clean this
// up significantly.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_io.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "c_net.h"

#include "doomdef.h"
#include "doomstat.h"
#include "m_argv.h"
#include "m_misc.h"
#include "mn_engin.h"
#include "g_game.h"
#include "m_qstr.h" // haleyjd
#include "w_wad.h"

static void C_EchoValue(command_t *command);
static void C_SetVariable(command_t *command);
static void C_RunAlias(alias_t *alias);
static int  C_Sync(command_t *command);
static void C_ArgvtoArgs(void);
static boolean C_Strcmp(const char *pa, const char *pb);

///////////////////////////////////////////////////////////////////////////
//
// The Console (TM)
//

console_t Console;

///////////////////////////////////////////////////////////////////////////
//
// Parsing/Running Commands
//

static qstring_t *cmdtokens;
static int numtokens;
static int numtokensalloc;

//
// C_nextCmdToken
//
// haleyjd 08/08/10: Used to remove SMMU limit on console command tokens.
//
static void C_nextCmdToken(void)
{
   if(numtokens >= numtokensalloc)
   {
      int i;

      // grow by MAXTOKENS at a time (doubling is likely to waste memory)
      numtokensalloc += MAXTOKENS;
      cmdtokens = (qstring_t *)(realloc(cmdtokens, numtokensalloc * sizeof(qstring_t)));

      for(i = numtokens; i < numtokensalloc; i++)
         QStrInitCreateSize(&cmdtokens[i], 128);

      Console.numargvsalloc += MAXTOKENS;
      Console.argv = (qstring_t *)(realloc(Console.argv, Console.numargvsalloc * sizeof(qstring_t)));

      for(i = numtokens; i < Console.numargvsalloc; i++)
         QStrInitCreateSize(&Console.argv[i], 128);
   }
   numtokens++;
}

//
// C_initCmdTokens
//
// haleyjd 07/05/10: create qstrings for the console tokenizer.
//
static void C_initCmdTokens(void)
{
   static boolean cmdtokensinit;

   if(!cmdtokensinit)
   {
      int i;

      // haleyjd: MAXTOKENS is now just the starting size of the array
      cmdtokens = (qstring_t *)(calloc(MAXTOKENS, sizeof(qstring_t)));
      numtokensalloc = MAXTOKENS;

      Console.argv = (qstring_t *)(calloc(MAXTOKENS, sizeof(qstring_t)));
      Console.numargvsalloc = MAXTOKENS;

      for(i = 0; i < numtokensalloc; i++)
      {
         QStrCreateSize(&cmdtokens[i],    128); // local tokens
         QStrCreateSize(&Console.argv[i], 128); // console argvs
      }

      QStrCreateSize(&Console.args, 1024); // congealed args
      
      cmdtokensinit = true;
   }
}

//
// C_clearCmdTokens
//
// haleyjd 07/05/10: clear all console tokens
//
static void C_clearCmdTokens(void)
{
   int i;

   for(i = 0; i < numtokensalloc; i++)
      QStrClear(&cmdtokens[i]);
}

//
// C_GetTokens
//
// Break up the command into tokens.
// haleyjd 07/05/10: rewritten to remove token length limitation
//
static void C_GetTokens(const char *command)
{
   const char *rover;
   boolean quotemark = false;
   
   rover = command;
   
   C_initCmdTokens();
   C_clearCmdTokens();

   numtokens = 1;
   
   while(*rover == ' ') 
      rover++;
   
   if(!*rover)     // end of string already
   {
      numtokens = 0;
      return;
   }

   while(*rover)
   {
      if(*rover == '"')
      {
         quotemark = !quotemark;
      }
      else if(*rover == ' ' && !quotemark)   // end of token
      {
         // only if the current one actually contains something
         if(*(QStrConstPtr(&cmdtokens[numtokens - 1])))
            C_nextCmdToken();
      }
      else // add char to line
      {
         // 01/12/09: repaired by fraggle to fix mystery sprintf issue on linux
         // 07/05/10: rewritten to use qstrings
         QStrPutc(&cmdtokens[numtokens - 1], *rover);
      }

      rover++;
   }
}

//
// C_RunIndivTextCmd
//
// called once a typed command line has been broken
// into individual commands (multiple commands on one
// line are allowed with the ';' character)
//
static void C_RunIndivTextCmd(const char *cmdname)
{
   command_t *command;
   alias_t *alias;
   
   // cut off leading spaces
   while(*cmdname == ' ')
      cmdname++;
   
   // break into tokens
   C_GetTokens(cmdname);
   
   // find the command being run from the first token.
   command = C_GetCmdForName(QStrConstPtr(&cmdtokens[0]));
   
   if(!numtokens) 
      return; // no command
   
   if(!command) // no _command_ called that
   {
      // alias?
      if((alias = C_GetAlias(QStrConstPtr(&cmdtokens[0]))))
      {
         // save the options into cmdoptions
         // QSTR_FIXME: wtf is this doin' anyway? o_O
         cmdoptions = (char *)cmdname + QStrLen(&cmdtokens[0]);
         C_RunAlias(alias);
      }
      else         // no alias either
         C_Printf("unknown command: '%s'\n", QStrConstPtr(&cmdtokens[0]));
   }
   else
   {
      // run the command (buffer it)
      C_RunCommand(command, cmdname + QStrLen(&cmdtokens[0]));
   }
}

enum
{
   CCF_CANNOTSET = 0,    // cannot set anything.
   CCF_CANSETVAR = 0x01, // can set the variable's normal value
   CCF_CANSETDEF = 0x02, // can set the variable's default
   
   CCF_CANSETALL = CCF_CANSETVAR | CCF_CANSETDEF
};

//
// C_CheckFlags
//
// Check the flags of a command to see if it should be run or not.
//
static int C_CheckFlags(command_t *command, const char **errormsg)
{
   int returnval = CCF_CANSETALL;
   
   if(!command->variable || !command->variable->v_default || 
      (command->flags & cf_handlerset))
   {
      // remove default flag if there is no default.
      returnval &= ~CCF_CANSETDEF;
   }

   // check the flags
   *errormsg = NULL;
   
   if((command->flags & cf_notnet) && (netgame && !demoplayback))
      *errormsg = "not available in netgame";
   if((command->flags & cf_netonly) && !netgame && !demoplayback)
      *errormsg = "only available in netgame";
   if((command->flags & cf_server) && consoleplayer && !demoplayback
      && Console.cmdtype != c_netcmd)
      *errormsg = "for server only";
   if((command->flags & cf_level) && gamestate != GS_LEVEL)
      *errormsg = "can be run in levels only";
   
   // net-sync critical variables are usually critical to demo sync too
   if((command->flags & cf_netvar) && demoplayback)
      *errormsg = "not during demo playback";

   if(*errormsg)
   {
      // haleyjd 08/15/10: allow setting of default values in certain conditions   
      if(demoplayback && (returnval & CCF_CANSETDEF))
      {
         *errormsg = "will take effect after demo ends";
         returnval = CCF_CANSETDEF; // we can still modify the default value.
      }
      else
         returnval = CCF_CANNOTSET; // we can't set anything to this variable now.
   }
   
   return returnval;
}

//
// C_doErrorMsg
//
// haleyjd 08/15/10: I had to separate out printing of errors from C_CheckFlags since
// that function can now be called just for checking a variable's state and not just
// for setting it.
//
static void C_doErrorMsg(command_t *command, const char *errormsg)
{
   if(errormsg)
   {
      C_Printf("%s: %s\n", command->name, errormsg);
      if(menuactive)
         MN_ErrorMsg(errormsg);
   }
}

// C_RunCommand.
// call with the command to run and the command-line options.
// buffers the commands which will be run later.

void C_RunCommand(command_t *command, const char *options)
{
   // do not run straight away, we might be in the middle of rendering
   C_BufferCommand(Console.cmdtype, command, options, Console.cmdsrc);
   
   Console.cmdtype = c_typed;  // reset to typed command as default
}

//
// C_DoRunCommand
//
// actually run a command. Same as C_RunCommand only instant.
//
static void C_DoRunCommand(command_t *command, const char *options)
{
   int i;
   const char *errormsg = NULL;
   
   C_GetTokens(options);
   
   for(i = 0; i < numtokensalloc; ++i)
      QStrQCopy(&Console.argv[i], &cmdtokens[i]);

   Console.argc = numtokens;
   Console.command = command;
   
   // perform checks
   
   // check through the tokens for variable names
   for(i = 0; i < Console.argc; i++)
   {
      char c = *(QStrConstPtr(&Console.argv[i]));

      if(c == '%' || c == '$') // variable
      {
         command_t *variable;
         
         variable = C_GetCmdForName(QStrConstPtr(&Console.argv[i]) + 1);

         if(!variable || !variable->variable)
         {
            C_Printf("unknown variable '%s'\n", QStrConstPtr(&Console.argv[i]) + 1);
            // clear for next time
            Console.cmdtype = c_typed; 
            Console.cmdsrc  = consoleplayer;
            return;
         }
         
         QStrCopy(&Console.argv[i], 
                   c == '%' ? C_VariableValue(variable->variable) :
                              C_VariableStringValue(variable->variable));
      }
   }
   
   C_ArgvtoArgs();                 // build Console.args
   
   // actually do this command
   switch(command->type)
   {
   case ct_command:
      // not to be run ?
      if(C_CheckFlags(command, &errormsg) == CCF_CANNOTSET || C_Sync(command))
      {
         C_doErrorMsg(command, errormsg);
         Console.cmdtype = c_typed;
         Console.cmdsrc = consoleplayer; 
         return;
      }
      if(command->handler)
         command->handler();
      else
         C_Printf(FC_ERROR "error: no command handler for %s\n", 
                  command->name);
      break;
      
   case ct_constant:
      C_EchoValue(command);
      break;
      
   case ct_variable:
      C_SetVariable(command);
      break;
      
   default:
      C_Printf(FC_ERROR "unknown command type %i\n", command->type);
      break;
   }
   
   Console.cmdtype = c_typed; 
   Console.cmdsrc  = consoleplayer;   // clear for next time
}

//
// C_ArgvtoArgs
//
// Take all the argvs and put them all together in the args string
//
// CONSOLE_FIXME: Horrible.
// haleyjd 07/08/09: rewritten to avoid undefined sprintf behavior.
//
static void C_ArgvtoArgs(void)
{
   int i, n;
 
   for(i = 0; i < Console.argc; i++)
   {
      if(!QStrLen(&Console.argv[i]))       // empty string
      {
         for(n = i; n < Console.argc - 1; n++)
            QStrQCopy(&(Console.argv[n]), &(Console.argv[n+1]));

         Console.argc--; 
         i--;
      }
   }
   
   QStrClear(&Console.args);

   for(i = 0; i < Console.argc; ++i)
   {
      // haleyjd: use qstring_t to avoid sprintf problems and to be secure
      QStrQCat(&Console.args, &Console.argv[i]);
      QStrPutc(&Console.args, ' ');
   }
}

//
// C_QuotedArgvToArgs
//
// Return a string of all the argvs linked together, but with each
// argv in quote marks "
//
static const char *C_QuotedArgvToArgs(void)
{
   int i;
   static qstring_t returnbuf;

   QStrClearOrCreate(&returnbuf, 1024);
   
   // haleyjd: use qstring to eliminate undefined sprintf behavior
   for(i = 0; i < Console.argc; ++i)
      QStrPutc(QStrMakeQuoted(QStrQCat(&returnbuf, &Console.argv[i])), ' ');
   
   return QStrConstPtr(&returnbuf);
}

//
// C_Sync
//
// See if the command needs to be sent to other computers
// to maintain sync and do so if neccesary
//
static int C_Sync(command_t *command)
{
   if(command->flags & cf_netvar)
   {
      // dont get stuck repeatedly sending the same command
      if(Console.cmdtype != c_netcmd)
      {                               // send to sync
         C_SendCmd(CN_BROADCAST, command->netcmd,
                   "%s", C_QuotedArgvToArgs());
         return true;
      }
   }
   
   return false;
}

//
// C_RunTextCmd
//
// Execute a compound command (with or without ;'s)
//
void C_RunTextCmd(const char *command)
{
   boolean quotemark = false;  // for " quote marks
   char *sub_command = NULL;
   const char *rover;

   for(rover = command; *rover; rover++)
   {
      if(*rover == '\"')    // quotemark
      {
         quotemark = !quotemark;
         continue;
      }
      if(*rover == ';' && !quotemark)  // command seperator and not in string 
      {
         // found sub-command
         // use recursion to run the subcommands
         
         // left
         // copy sub command, alloc slightly more than needed
         sub_command = (char *)(calloc(1, rover-command+3)); 
         strncpy(sub_command, command, rover-command);
         sub_command[rover-command] = '\0';   // end string
         
         C_RunTextCmd(sub_command);
         
         // right
         C_RunTextCmd(rover+1);
         
         // leave to the other function calls (above) to run commands
         free(sub_command);
         return;
      }
   }
   
   // no sub-commands: just one
   // so run it
   
   C_RunIndivTextCmd(command);
}

//
// C_VariableValue
//
// Get the literal value of a variable (ie. "1" not "on")
//
const char *C_VariableValue(variable_t *variable)
{
   static qstring_t value;
   void *loc;
   const char *dummymsg = NULL;
   
   QStrClearOrCreate(&value, 1024);
   
   if(!variable)
      return "";

   // haleyjd 08/15/10: if only the default can be set right now, return the
   // default value rather than the current value.
   if(C_CheckFlags(variable->command, &dummymsg) == CCF_CANSETDEF)
      loc = variable->v_default;
   else
      loc = variable->variable;
   
   switch(variable->type)
   {
   case vt_int:
      QStrPrintf(&value, 0, "%d", *(int *)loc);
      break;

   case vt_toggle:
      // haleyjd 07/05/10
      QStrPrintf(&value, 0, "%d", (int)(*(boolean *)loc));
      break;
      
   case vt_string:
      // haleyjd 01/24/03: added null check from prboom
      if(*(char **)variable->variable)
         QStrCopy(&value, *(char **)loc);
      else
         return "null";
      break;

   case vt_chararray:
      // haleyjd 03/13/06: static string support
      QStrCopy(&value, (const char *)loc);
      break;

   case vt_float:
      // haleyjd 04/21/10: implemented vt_float
      QStrPrintf(&value, 0, "%+.5f", *(double *)loc);
      break;
      
   default:
      I_Error("C_VariableValue: unknown variable type %d\n", variable->type);
   }
   
   return QStrConstPtr(&value);
}

//
// C_VariableStringValue
//
// Get the string value (ie. "on" not "1")
//
const char *C_VariableStringValue(variable_t *variable)
{
   static qstring_t value;
   int stateflags;
   const char *dummymsg = NULL;
   
   QStrClearOrCreate(&value, 1024);
   
   if(!variable) 
      return "";

   if(!variable->variable)
      return "null";

   // haleyjd: get the "check" flags
   stateflags = C_CheckFlags(variable->command, &dummymsg);
   
   // does the variable have alternate 'defines' ?
   if((variable->type == vt_int || variable->type == vt_toggle) && variable->defines)
   {
      // print defined value
      // haleyjd 03/17/02: needs rangechecking
      int varValue = 0;
      int valStrIndex = 0;
      void *loc;

      // if this is a variable that can only currently have its default set, use its
      // default value rather than its actual value.
      if(stateflags == CCF_CANSETDEF)
         loc = variable->v_default;
      else
         loc = variable->variable;

      if(variable->type == vt_int)
         varValue = *((int *)loc);
      else if(variable->type == vt_toggle)
         varValue = (int)(*(boolean *)loc);

      valStrIndex = varValue - variable->min;

      if(valStrIndex < 0 || valStrIndex > variable->max - variable->min)
         return "";
      else
         QStrCopy(&value, variable->defines[valStrIndex]);
   }
   else
   {
      // print literal value
      QStrCopy(&value, C_VariableValue(variable));
   }
   
   return QStrConstPtr(&value);
}


// echo a value eg. ' "alwaysmlook" is "1" '

static void C_EchoValue(command_t *command)
{
   C_Printf("\"%s\" is \"%s\"\n", command->name,
            C_VariableStringValue(command->variable) );
}

// is a string a number?

static boolean isnum(const char *text)
{
   // haleyjd 07/05/10: skip over signs here
   if(strlen(text) > 1 && (*text == '-' || *text == '+'))
      ++text;

   for(; *text; text++)
   {
      if(*text > '9' || *text < '0')
         return false;
   }
   return true;
}

//
// C_ValueForDefine
//
// Take a string and see if it matches a define for a variable. Replace with the
// literal value if so.
//
static const char *C_ValueForDefine(variable_t *variable, const char *s, int setflags)
{
   int count;
   static qstring_t returnstr;

   if(!returnstr.buffer)
      QStrInitCreate(&returnstr);

   // haleyjd 07/05/10: ALWAYS copy param to returnstr, or else strcpy will have
   // undefined behavior in C_SetVariable.
   QStrCopy(&returnstr, s);

   if(variable->defines)
   {
      for(count = variable->min; count <= variable->max; count++)
      {
         if(!C_Strcmp(s, variable->defines[count-variable->min]))
         {
            QStrPrintf(&returnstr, 0, "%d", count);
            return returnstr.buffer;
         }
      }
   }

   // special hacks for menu

   // haleyjd 07/05/10: support * syntax for setting default value
   if(!strcmp(s, "*") && variable->cfgDefault)
   {
      default_t *dp = variable->cfgDefault;

      switch(variable->type)
      {
      case vt_int:
         {
            int i;
            dp->methods->getDefault(dp, &i);
            QStrPrintf(&returnstr, 0, "%d", i);
         }
         break;
      case vt_toggle:
         {
            boolean b;
            dp->methods->getDefault(dp, &b);
            QStrPrintf(&returnstr, 0, "%d", !!b);
         }
         break;
      case vt_float:
         {
            double f;
            dp->methods->getDefault(dp, &f);
            QStrPrintf(&returnstr, 0, "%f", f);
         }
         break;
      case vt_string:
         {
            char *def;
            dp->methods->getDefault(dp, &def);
            QStrCopy(&returnstr, def);
         }
         break;
      case vt_chararray:
         {
            // TODO
         }
         break;
      }

      return returnstr.buffer;
   }
   
   if(variable->type == vt_int || variable->type == vt_toggle)    // int values only
   {
      int value;
      void *loc;

      // if we can only set the default right now, use the default value.
      if(setflags == CCF_CANSETDEF)
         loc = variable->v_default;
      else
         loc = variable->variable;

      if(variable->type == vt_int)
         value = *(int *)loc;
      else
         value = (int)(*(boolean *)loc);

      if(!strcmp(s, "+"))     // increase value
      {
         value += 1;

         if(variable->max != UL && value > variable->max) 
            value = variable->max;
         
         QStrPrintf(&returnstr, 0, "%d", value);
         return returnstr.buffer;
      }
      if(!strcmp(s, "-"))     // decrease value
      {
         value -= 1;
         
         if(variable->min != UL && value < variable->min)
            value = variable->min;
         
         QStrPrintf(&returnstr, 0, "%d", value);
         return returnstr.buffer;
      }
      if(!strcmp(s, "/"))     // toggle value
      {
         if(variable->type == vt_int)
            value += 1;
         else
            value = !value;

         if(variable->max != UL && value > variable->max)
            value = variable->min; // wrap around

         QStrPrintf(&returnstr, 0, "%d", value);
         return returnstr.buffer;
      }
      
      if(!isnum(s))
         return NULL;
   }
   
   return returnstr.buffer;
}

// haleyjd 08/30/09: local-origin netcmds need love too
#define cmd_setdefault \
   (Console.cmdtype == c_typed || \
    (Console.cmdtype == c_netcmd && Console.cmdsrc == consoleplayer))

//
// C_SetVariable
//
// Set a variable.
//
static void C_SetVariable(command_t *command)
{
   variable_t* variable;
   int size = 0;
   double fs = 0.0;
   char *errormsg;
   const char *temp;
   int setflags;
   const char *varerror = NULL;
   
   // cut off the leading spaces
   
   if(!Console.argc)     // asking for value
   {
      C_EchoValue(command);
      return;
   }

   // find out how we can change it.
   setflags = C_CheckFlags(command, &varerror);
   C_doErrorMsg(command, varerror); // show a message if one was set.

   if(setflags == CCF_CANNOTSET) 
      return; // can't set anything.
   
   // ok, set the value
   variable = command->variable;
   
   temp = C_ValueForDefine(variable, QStrConstPtr(&Console.argv[0]), setflags);
   
   if(temp)
      QStrCopy(&Console.argv[0], temp);
   else
   {
      C_Printf("not a possible value for '%s'\n", command->name);
      return;
   }

   switch(variable->type)
   {
   case vt_int:
   case vt_toggle:
      size = QStrAtoi(&Console.argv[0]);
      break;
      
   case vt_string:
   case vt_chararray:
      size = QStrLen(&Console.argv[0]);
      break;

   case vt_float:
      // haleyjd 04/21/10: vt_float
      fs = QStrToDouble(&Console.argv[0], NULL);
      break;
      
   default:
      return;
   }

   // check the min/max sizes
   
   errormsg = NULL;
   
   // haleyjd 03/22/09: allow unlimited bounds
   // haleyjd 04/21/10: implement vt_float
   
   if(variable->type == vt_float)
   {
      // float requires a different check
      if(variable->dmax != UL && fs > variable->dmax)
         errormsg = "value too big";
      if(variable->dmin != UL && fs < variable->dmin)
         errormsg = "value too small";
   }
   else
   {
      if(variable->max != UL && size > variable->max)
         errormsg = "value too big";
      if(variable->min != UL && size < variable->min)
         errormsg = "value too small";
   }
   
   if(errormsg)
   {
      MN_ErrorMsg(errormsg);
      C_Puts(errormsg);
      return;
   }
   
   // netgame sync: send command to other nodes
   if(C_Sync(command))
      return;
   
   // now set it
   // 5/8/99 set default value also
   // 16/9/99 cf_handlerset flag for variables set from
   // the handler instead

   // haleyjd 07/15/09: cmdtype, NOT cmdsrc in tests below!!!
   
   if(!(command->flags & cf_handlerset))
   {
      switch(variable->type)  // implicitly set the variable
      {
      case vt_int:
         if(setflags & CCF_CANSETVAR)
            *(int*)variable->variable = size;
         if((setflags & CCF_CANSETDEF) && cmd_setdefault) // default
            *(int*)variable->v_default = size;
         break;

      case vt_toggle:
         // haleyjd 07/05/10
         if(setflags & CCF_CANSETVAR)
            *(boolean *)variable->variable = !!size;
         if((setflags & CCF_CANSETDEF) && cmd_setdefault) // default
            *(boolean *)variable->v_default = !!size;
         break;
         
      case vt_string:
         if(setflags & CCF_CANSETVAR)
         {
            free(*(char**)variable->variable);
            *(char**)variable->variable = QStrCDup(&Console.argv[0], PU_STATIC);
         }
         if((setflags & CCF_CANSETDEF) && cmd_setdefault)  // default
         {
            free(*(char**)variable->v_default);
            *(char**)variable->v_default = QStrCDup(&Console.argv[0], PU_STATIC);
         }
         break;

      case vt_chararray:
         // haleyjd 03/13/06: static strings
         if(setflags & CCF_CANSETVAR)
         {
            memset(variable->variable, 0, variable->max + 1);
            QStrCNCopy((char *)variable->variable, &Console.argv[0], variable->max + 1);
         }
         if((setflags & CCF_CANSETDEF) && cmd_setdefault)
         {
            memset(variable->v_default, 0, variable->max+1);
            strcpy((char *)variable->v_default, Console.argv[0].buffer);
         }
         break;

      case vt_float:
         // haleyjd 04/21/10: implemented vt_float
         if(setflags & CCF_CANSETVAR)
            *(double *)variable->variable = fs;
         if((setflags & CCF_CANSETDEF) && cmd_setdefault)
            *(double *)variable->v_default = fs;
         break;
         
      default:
         I_Error("C_SetVariable: unknown variable type %d\n", variable->type);
      }
   }
   
   if(command->handler)          // run handler if there is one
      command->handler();
}

//=============================================================================
//
// Tab Completion
//

static qstring_t origkey;
static boolean gotkey;
static command_t **tabs;
static int numtabsalloc; // haleyjd 07/25/10
static int numtabs = 0;
static int thistab = -1;

//
// CheckTabs
//
// haleyjd 07/25/10: Removed dangerous unchecked limit on tab completion.
//
static void CheckTabs(void)
{
   if(numtabs >= numtabsalloc)
   {
      numtabsalloc = numtabsalloc ? 2 * numtabsalloc : 128;
      tabs = (command_t **)(realloc(tabs, numtabsalloc * sizeof(command_t *)));
   }
}

//
// GetTabs
//
// given a key (eg. "r_sw"), will look through all the commands in the hash
// chains and gather all the commands which begin with this into a list 'tabs'
//
static void GetTabs(qstring_t *qkey)
{
   int i;
   size_t pos, keylen;
   
   numtabs = 0;

   if(!origkey.buffer)
      QStrCreateSize(&origkey, 128);

   // remember input
   QStrQCopy(&origkey, qkey);

   // find the first non-space character; if none, we can't do this
   if((pos = QStrFindFirstNotOfChar(qkey, ' ')) == qstring_npos)
      return;
   
   // save the input from the first non-space character, and lowercase it
   QStrLwr(QStrCopy(&origkey, QStrBufferAt(qkey, pos)));

   gotkey = true;
      
   keylen = QStrLen(&origkey);

   // check each hash chain in turn
   
   for(i = 0; i < CMDCHAINS; i++)
   {
      command_t *browser = cmdroots[i];
      
      // go through each link in this chain
      for(; browser; browser = browser->next)
      {
         if(!(browser->flags & cf_hidden) && // ignore hidden ones
            !QStrNCmp(&origkey, browser->name, keylen))
         {
            // found a new tab
            CheckTabs();
            tabs[numtabs] = browser;
            numtabs++;
         }
      }
   }
}

//
// C_InitTab
//
// Reset the tab list 
//
void C_InitTab(void)
{
   numtabs = 0;

   QStrClearOrCreate(&origkey, 100);

   gotkey = false;
   thistab = -1;
}

//
// C_NextTab
//
// Called when tab pressed. Get the next tab from the list.
//
qstring_t *C_NextTab(qstring_t *key)
{
   static qstring_t returnstr;
   qstring_t *ret = NULL;
 
   QStrClearOrCreate(&returnstr, 128);
   
   // get tabs if not done already
    if(!gotkey)
      GetTabs(key);
   
   // select next tab
   thistab = thistab == -1 ? 0 : thistab + 1;  

   if(thistab >= numtabs)
   {
      thistab = -1;
      ret = &origkey;
   }
   else
   {   
      QStrCopy(&returnstr, tabs[thistab]->name);
      QStrPutc(&returnstr, ' ');
      ret = &returnstr;
   }

   return ret;
}

//
// C_PrevTab
//
// Called when shift-tab pressed. Get the previous tab from the list.
//
qstring_t *C_PrevTab(qstring_t *key)
{
   static qstring_t returnstr;
   qstring_t *ret = NULL;

   QStrClearOrCreate(&returnstr, 128);
   
   // get tabs if neccesary
   if(!gotkey)
      GetTabs(key);
   
   // select prev.
   thistab = thistab == -1 ? numtabs - 1 : thistab - 1;
   
   // check invalid
   if(thistab < 0)
   {
      thistab = -1;
      ret = &origkey;
   }
   else
   {
      QStrCopy(&returnstr, tabs[thistab]->name);
      QStrPutc(&returnstr, ' ');
      ret = &returnstr;
   }
   
   return ret;
}

/////////////////////////////////////////////////////////////////////////
//
// Aliases
//

// haleyjd 04/14/03: removed limit and rewrote all code
alias_t aliases;
char *cmdoptions;       // command line options for aliases

//
// C_GetAlias
//
// Get an alias from a name
//
alias_t *C_GetAlias(const char *name)
{
   alias_t *alias = aliases.next;

   while(alias)
   {
      if(!strcmp(name, alias->name))
         return alias;
      alias = alias->next;
   }

   return NULL;
}

//
// C_NewAlias
//
// Create a new alias, or use one that already exists
//
alias_t *C_NewAlias(const char *aliasname, const char *command)
{
   alias_t *alias;

   // search for an existing alias with this name
   if((alias = C_GetAlias(aliasname)))
   {
      free(alias->command);
      alias->command = strdup(command);
   }
   else
   {
      // create a new alias
      alias = (alias_t *)(malloc(sizeof(alias_t)));
      alias->name = strdup(aliasname);
      alias->command = strdup(command);
      alias->next = aliases.next;
      aliases.next = alias;
   }

   return alias;
}

//
// C_RemoveAlias
//
// Remove an alias
//
void C_RemoveAlias(qstring_t *aliasname)
{
   alias_t *prev  = &aliases;
   alias_t *rover = aliases.next;
   alias_t *alias = NULL;

   while(rover)
   {
      if(!QStrCmp(aliasname, rover->name))
      {
         alias = rover;
         break;
      }

      prev = rover;
      rover = rover->next;
   }

   if(!alias)
   {
      C_Printf("unknown alias \"%s\"\n", QStrConstPtr(aliasname));
      return;
   }
   
   C_Printf("removing alias \"%s\"\n", QStrConstPtr(aliasname));
   
   // free alias data
   free(alias->name);
   free(alias->command);

   // unlink alias
   prev->next  = alias->next;
   alias->next = NULL;

   // free the alias
   free(alias);
}

// run an alias

void C_RunAlias(alias_t *alias)
{
   // store command line for use in macro
   while(*cmdoptions == ' ')
      cmdoptions++;

   C_RunTextCmd(alias->command);   // run the command
}

//////////////////////////////////////////////////////////////////////
//
// Command Bufferring
//

// new ticcmds can be built at any time including during the
// rendering process. The commands need to be buffered
// and run by the tickers instead of directly from the
// responders
//
// a seperate buffered command list is kept for each type
// of command (typed, script, etc)

// 15/11/99 cleaned up: now uses linked lists of bufferedcmd's
//              rather than a static array: no limit on
//              buffered commands and nicer code

typedef struct bufferedcmd_s bufferedcmd;

struct bufferedcmd_s
{
   command_t *command;     // the command
   char *options;          // command line options
   int cmdsrc;             // source player
   bufferedcmd *next;      // next in list
};

typedef struct cmdbuffer_s
{
   // NULL once list empty
   bufferedcmd *cmdbuffer;
   int timer;   // tic timer to temporarily freeze executing of cmds
} cmdbuffer;

cmdbuffer buffers[C_CMDTYPES];

void C_RunBufferedCommand(bufferedcmd *bufcmd)
{
   // run command
   // restore variables
   
   Console.cmdsrc = bufcmd->cmdsrc;

   C_DoRunCommand(bufcmd->command, bufcmd->options);
}

void C_BufferCommand(int cmtype, command_t *command, const char *options,
                     int cmdsrc)
{
   bufferedcmd *bufcmd;
   bufferedcmd *newbuf;
   
   // create bufferedcmd
   newbuf = (bufferedcmd *)(malloc(sizeof(bufferedcmd)));
   
   // add to new bufferedcmd
   newbuf->command = command;
   newbuf->options = strdup(options);
   newbuf->cmdsrc = cmdsrc;
   newbuf->next = NULL;            // is always at end of chain
   
   // no need to be buffered: run it now
   if(!(command->flags & cf_buffered) && buffers[cmtype].timer == 0)
   {
      Console.cmdtype = cmtype;
      C_RunBufferedCommand(newbuf);
      
      free(newbuf->options);
      free(newbuf);
      return;
   }
   
   // add to list now
   
   // select according to cmdtype
   bufcmd = buffers[cmtype].cmdbuffer;
   
   // list empty?
   if(!bufcmd)              // hook in
      buffers[cmtype].cmdbuffer = newbuf;
   else
   {
      // go to end of list
      for(; bufcmd->next; bufcmd = bufcmd->next);
      
      // point last to newbuf
      bufcmd->next = newbuf;
   }
}

void C_RunBuffer(int cmtype)
{
   bufferedcmd *bufcmd, *next;
   
   bufcmd = buffers[cmtype].cmdbuffer;
   
   while(bufcmd)
   {
      // check for delay timer
      if(buffers[cmtype].timer)
      {
         buffers[cmtype].timer--;
         break;
      }
      
      Console.cmdtype = cmtype;
      C_RunBufferedCommand(bufcmd);
      
      // save next before freeing
      
      next = bufcmd->next;
      
      // free bufferedcmd
      
      free(bufcmd->options);
      free(bufcmd);
      
      // go to next in list
      bufcmd = buffers[cmtype].cmdbuffer = next;
   }
}

void C_RunBuffers(void)
{
   int i;
   
   // run all buffers
   
   for(i = 0; i < C_CMDTYPES; i++)
      C_RunBuffer(i);
}

void C_BufferDelay(int cmdtype, int delay)
{
   buffers[cmdtype].timer += delay;
}

void C_ClearBuffer(int cmdtype)
{
   buffers[cmdtype].timer = 0;                     // clear timer
   buffers[cmdtype].cmdbuffer = NULL;              // empty 
}

        // compare regardless of font colour
static boolean C_Strcmp(const char *pa, const char *pb)
{
   const unsigned char *a = (const unsigned char *)pa;
   const unsigned char *b = (const unsigned char *)pb;

   while(*a || *b)
   {
      // remove colour dependency
      if(*a >= 128)   // skip colour
      {
         a++; continue;
      }
      if(*b >= 128)
      {
         b++; continue;
      }
      // regardless of case also
      if(toupper(*a) != toupper(*b))
         return true;
      a++; b++;
   }
   
   return false;       // no difference in them
}

//////////////////////////////////////////////////////////////////
//
// Command hashing
//
// Faster look up of console commands

command_t *cmdroots[CMDCHAINS];

       // the hash key
#define CmdHashKey(s)                                     \
   (( (s)[0] + (s)[0] ? (s)[1] + (s)[1] ? (s)[2] +        \
                 (s)[2] ? (s)[3] + (s)[3] ? (s)[4]        \
                        : 0 : 0 : 0 : 0 ) % 16)

void (C_AddCommand)(command_t *command)
{
   int hash = CmdHashKey(command->name);
   
   command->next = cmdroots[hash]; // hook it in at the start of
   cmdroots[hash] = command;       // the table
   
   // save the netcmd link
   if(command->flags & cf_netvar && command->netcmd == 0)
      C_Printf(FC_ERROR "C_AddCommand: cf_netvar without a netcmd (%s)\n", command->name);
   
   c_netcmds[command->netcmd] = command;

   if(command->type == ct_variable || command->type == ct_constant)
   {
      // haleyjd 07/04/10: find default in config for cvars that have one
      command->variable->cfgDefault = M_FindDefaultForCVar(command->variable);

      // haleyjd 08/15/10: set variable's pointer to command
      command->variable->command = command;
   }
}

// add a list of commands terminated by one of type ct_end

void C_AddCommandList(command_t *list)
{
   for(;list->type != ct_end; list++)
      (C_AddCommand)(list);
}

// get a command from a string if possible
        
command_t *C_GetCmdForName(const char *cmdname)
{
   command_t *current;
   int hash;
   
   while(*cmdname == ' ')
      cmdname++;

   // start hashing
   
   hash = CmdHashKey(cmdname);
   
   current = cmdroots[hash];
   while(current)
   {
      if(!strcasecmp(cmdname, current->name))
         return current;
      current = current->next;        // try next in chain
   }
   
   return NULL;
}

/*
   haleyjd: Command Scripts
   From SMMU v3.30, these allow running command scripts from either
   files or buffered lumps -- very cool IMHO.
*/

// states for console script lexer
enum
{
   CSC_NONE,
   CSC_COMMENT,
   CSC_SLASH,
   CSC_COMMAND,
};

//
// C_RunScript
//
// haleyjd 02/12/04: rewritten to use DWFILE and qstring, and to
// formalize into a finite state automaton lexer. Removes several 
// bugs and problems, including static line length limit and failure 
// to run a command on the last line of the data.
//
void C_RunScript(DWFILE *dwfile)
{
   qstring_t qstring;
   int state = CSC_NONE;
   char c;

   // initialize and allocate the qstring
   QStrInitCreate(&qstring);

   // parse script
   while((c = D_Fgetc(dwfile)) != EOF)
   {
      // turn \r into \n for simplicity
      if(c == '\r')
         c = '\n';

      switch(state)
      {
#if 0
      case CSC_COMMENT:
         if(c == '\n')        // eat up to the next \n
            state = CSC_NONE;
         continue;
#endif

      case CSC_SLASH:
         if(c == '/')         // start the comment now
            state = CSC_COMMENT;
         else
            state = CSC_NONE; // ??? -- malformatted
         continue;

      case CSC_COMMAND:
         if(c == '\n' || c == '\f') // end of line - run command
         {
            Console.cmdtype = c_script;
            C_RunTextCmd(QStrConstPtr(&qstring));
            C_RunBuffer(c_script);  // force to run now
            state = CSC_NONE;
         }
         else
            QStrPutc(&qstring, c);
         continue;
      }

      // CSC_NONE processing
      switch(c)
      {
      case ' ':  // ignore leading whitespace
      case '\t':
      case '\f':
      case '\n':
         continue;
#if 0
      case '#':
      case ';':
         state = CSC_COMMENT; // start a comment
         continue;
      case '/':
         state = CSC_SLASH;   // maybe start a comment...
         continue;
#endif
      default:                // anything else starts a command
         QStrClear(&qstring);
         QStrPutc(&qstring, c);
         state = CSC_COMMAND;
         continue;
      }
   }

   if(state == CSC_COMMAND) // EOF on command line - run final command
   {
      Console.cmdtype = c_script;
      C_RunTextCmd(QStrConstPtr(&qstring));
      C_RunBuffer(c_script);  // force to run now
   }

   // free the qstring
   QStrFree(&qstring);
}

//
// C_RunScriptFromFile
//
// Opens the indicated file and runs it as a console script.
// haleyjd 02/12/04: rewritten to use new C_RunScript above.
//
void C_RunScriptFromFile(const char *filename)
{
   DWFILE dwfile, *file = &dwfile;

   D_OpenFile(file, filename, "r");

   if(!D_IsOpen(file))
   {
      C_Printf(FC_ERROR "Couldn't exec script '%s'\n", filename);
   }
   else
   {
      C_Printf("Executing script '%s'\n", filename);
      C_RunScript(file);
   }

   D_Fclose(file);
}

//
// C_RunCmdLineScripts
//
// Called at startup to look for scripts to run specified via the -exec command
// line parameter.
//
void C_RunCmdLineScripts(void)
{
   int p;

   if((p = M_CheckParm("-exec")))
   {
      // the parms after p are console script names,
      // until end of parms or another - preceded parm
      
      boolean file = true;
      
      while(++p < myargc)
      {
         if(*myargv[p] == '-')
            file = !strcasecmp(myargv[p], "-exec"); // allow multiple -exec
         else if(file)
         {
            char *filename = NULL;
            
            M_StringAlloca(&filename, 1, 6, myargv[p]);

            strcpy(filename, myargv[p]);

            M_NormalizeSlashes(M_AddDefaultExtension(filename, ".csc"));

            if(!access(".", R_OK))
               C_RunScriptFromFile(filename);
         }
      }
   }
}

// EOF

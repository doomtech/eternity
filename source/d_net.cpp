// Emacs style mode select   -*- C++ -*- 
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
// DESCRIPTION:
//      DOOM Network game communication and protocol,
//      all OS independend parts.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_englsh.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_main.h"
#include "d_net.h"
#include "doomstat.h"
#include "e_player.h"
#include "e_things.h"
#include "f_wipe.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "m_random.h"
#include "mn_engin.h"
#include "i_net.h"
#include "i_video.h"
#include "p_partcl.h"
#include "p_skin.h"
#include "r_draw.h"
#include "v_misc.h"
#include "v_video.h"
#include "version.h"

doomcom_t  *doomcom;        
doomdata_t *netbuffer; // points inside doomcom

//
// NETWORKING
//
// gametic is the tic about to (or currently being) run
// maketic is the tick that hasn't had control made for it yet
// nettics[] has the maketics for all players 
//
// a gametic cannot be run until nettics[] > gametic for all players
//
#define RESENDCOUNT 10
#define PL_DRONE    0x80    /* bit flag in doomdata->player */

//void (*netdisconnect)() = NULL;  // function ptr for disconnect function

static ticcmd_t localcmds[BACKUPTICS];

ticcmd_t    netcmds[MAXPLAYERS][BACKUPTICS];
static int  nettics[MAXNETNODES];
static bool nodeingame[MAXNETNODES];      // set false as nodes leave game
static bool remoteresend[MAXNETNODES];    // set when local needs tics
static int  resendto[MAXNETNODES];        // set when remote needs tics
static int  resendcount[MAXNETNODES];
static int  nodeforplayer[MAXPLAYERS];

//int      isconsoletic;          // is the current tic a gametic
                                  // or a list of console commands?
int        maketic;
static int skiptics;
int        ticdup;         
static int maxsend;               // BACKUPTICS/(2*ticdup)-1

//doomcom_t  singleplayer;        // single player doomcom

void D_ProcessEvents(void); 
void G_BuildTiccmd(ticcmd_t *cmd); 
void D_DoAdvanceDemo(void);

static bool       reboundpacket;
static doomdata_t reboundstore;

//
// ExpandTics
//
static int ExpandTics(int low)
{
   int delta;
   
   delta = low - (maketic & 0xff);
   
   if(delta >= -64 && delta <= 64)
      return (maketic & ~0xff) + low;
   if(delta > 64)
      return (maketic & ~0xff) - 256 + low;
   if(delta < -64)
      return (maketic & ~0xff) + 256 + low;
   
   I_Error("ExpandTics: strange value %i at maketic %i\n", low, maketic);

   return 0;
}

/*
int  oldentertics;

//
// ResetNet
//
// NETCODE_FIXME: Remove this and go back to not allowing connections
// from within the game engine to simplify system.
//
void ResetNet(void)
{
   int i;
   int nowtime = I_GetTime();
   
   if(!in_textmode)
      C_SetConsole();
   
   oldentertics = nowtime;
   
   maketic = 1;
   gametic = levelstarttic = basetic = 0;
   
   for(i = 0; i < MAXNETNODES; ++i)
   {
      nettics[i] = resendto[i] = 0;
      remoteresend[i] = false; 
   }
   //  netbuffer->starttic = 0;
}
*/

//
// HSendPacket
//
static void HSendPacket(int node, int flags)
{
   netbuffer->checksum = (uint32_t)flags;
   
   if(!node)
   {
      reboundstore = *netbuffer;
      reboundpacket = true;
      return;
   }

   if(demoplayback)
      return;

   if(!netgame)
      I_Error("Tried to transmit to another node\n");

   doomcom->command    = CMD_SEND;
   doomcom->remotenode = node;
   
   I_NetCmd();
}

//
// HGetPacket
// Returns false if no packet is waiting
//
static bool HGetPacket(void)
{       
   if(reboundpacket)
   {
      *netbuffer = reboundstore;
      doomcom->remotenode = 0;
      reboundpacket = false;
      return true;
   }
   
   if(!netgame)
      return false;
   
   if(demoplayback)
      return false;

   doomcom->command = CMD_GET;
   if(!I_NetCmd())
      return false;
   
   if(doomcom->remotenode == -1)
      return false;
   
   // haleyjd 08/25/11: length & checksum not handled here any more
   
   return true;
}

//
// GetPackets
//
static void GetPackets(void)
{
   int         netconsole;
   int         netnode;
   ticcmd_t    *src, *dest;
   int         realend;
   int         realstart;
   
   while(HGetPacket())
   {
      if(netbuffer->checksum & NCMD_SETUP)
         continue;           // extra setup packet
      
      netconsole = netbuffer->player & ~PL_DRONE;
      netnode = doomcom->remotenode;
      
      // to save bytes, only the low byte of tic numbers are sent
      // Figure out what the rest of the bytes are
      realstart = ExpandTics(netbuffer->starttic);           
      realend   = realstart + netbuffer->numtics;
      
      // check for exiting the game
      if(netbuffer->checksum & NCMD_EXIT)
      {
         if(!nodeingame[netnode])
            continue;
         nodeingame[netnode] = false;
         playeringame[netconsole] = false;
         doom_printf("%s left the game", players[netconsole].name);
         
         // sf: remove the players mobj
         // spawn teleport flash
         
         if(gamestate == GS_LEVEL)
         {
            Mobj *tflash;

            tflash = P_SpawnMobj(players[netconsole].mo->x,
                                 players[netconsole].mo->y,
                                 players[netconsole].mo->z + 
                                    GameModeInfo->teleFogHeight,
                                 E_SafeThingType(GameModeInfo->teleFogType));
	      
            tflash->momx = players[netconsole].mo->momx;
            tflash->momy = players[netconsole].mo->momy;
            if(drawparticles)
            {
               tflash->flags2 |= MF2_DONTDRAW;
               P_DisconnectEffect(players[netconsole].mo);
            }
            players[netconsole].mo->removeThinker();
         }
         if(demorecording)
            G_CheckDemoStatus();

         continue;
      }

      /*
      // check for a remote game kill
      if(netbuffer->checksum & NCMD_KILL)
      {
         C_Printf(FC_ERROR "Killed by network driver\n");
         D_QuitNetGame();
      }
      */

      // check for a remote game kill
      if(netbuffer->checksum & NCMD_KILL)
         I_Error("Killed by network driver\n");
      
      nodeforplayer[netconsole] = netnode;
      
      // check for retransmit request
      if(resendcount[netnode] <= 0  && (netbuffer->checksum & NCMD_RETRANSMIT))
      {
         resendto[netnode] = ExpandTics(netbuffer->retransmitfrom);
         resendcount[netnode] = RESENDCOUNT;
      }
      else
         resendcount[netnode]--;
      
      // check for out of order / duplicated packet           
      if(realend == nettics[netnode])
         continue;
      
      if(realend < nettics[netnode])
         continue;
      
      // check for a missed packet
      if(realstart > nettics[netnode])
      {
         // stop processing until the other system resends the missed tics
         remoteresend[netnode] = true;
         continue;
      }
      
      // update command store from the packet
      int start;
         
      remoteresend[netnode] = false;
         
      start = nettics[netnode] - realstart;               
      src = &netbuffer->d.cmds[start];
         
      while(nettics[netnode] < realend)
      {
         dest = &netcmds[netconsole][nettics[netnode]%BACKUPTICS];
         nettics[netnode]++;
         *dest = *src;
         src++;
      }
   }
}

int gametime;

/*
int newtics, ticnum;    // the number of tics being built, tic number
*/

//
// NetUpdate
//
// Builds ticcmds for console player,
// sends out a packet
//
void NetUpdate(void)
{
   int nowtime;
   int newtics;
   int i,j;
   int realstart;
   int gameticdiv;
   
   // check time
   nowtime = I_GetTime() / ticdup;
   newtics = nowtime - gametime;
   gametime = nowtime;
   
   if(newtics <= 0)   // nothing new to update
   {
      GetPackets();
      return;
   }
   
   if(skiptics <= newtics)
   {
      newtics -= skiptics;
      skiptics = 0;
   }
   else
   {
      skiptics -= newtics;
      newtics = 0;
   }
   
   netbuffer->player = consoleplayer;
   
   // build new ticcmds for console player
   gameticdiv = gametic / ticdup;

   for(i = 0; i < newtics; ++i)
   {
      I_StartTic();
      D_ProcessEvents();
      if(maketic - gameticdiv >= BACKUPTICS / 2 - 1)
         break; // can't hold any more
      
      G_BuildTiccmd(&localcmds[maketic%BACKUPTICS]);
      ++maketic;
   }
  
   if(singletics)
      return; // singletic update is syncronous
  
   // send the packet to the other nodes
   for(i = 0; i < doomcom->numnodes; ++i)
   {
      if(nodeingame[i])
      {
         netbuffer->starttic = realstart = resendto[i];
         netbuffer->numtics = maketic - realstart;
         if(netbuffer->numtics > BACKUPTICS)
            I_Error("NetUpdate: netbuffer->numtics > BACKUPTICS\n");
         
         resendto[i] = maketic - doomcom->extratics;
         
         for(j = 0; j < netbuffer->numtics; ++j)
            netbuffer->d.cmds[j] = localcmds[(realstart + j) % BACKUPTICS];
         
         if(remoteresend[i])
         {
            netbuffer->retransmitfrom = nettics[i];
            HSendPacket(i, NCMD_RETRANSMIT);
         }
         else
         {
            netbuffer->retransmitfrom = 0;
            HSendPacket (i, 0);
         }
      }
   }
   
   // listen for other packets
   GetPackets();
}

/*
//
// D_KickPlayer
//
void D_KickPlayer(int playernum)
{
   HSendPacket(nodeforplayer[playernum], NCMD_KILL);
}
*/


//
// CheckAbort
//
static void CheckAbort(void)
{
   int stoptic;
   
   stoptic = I_GetTime() + 2; 
   
   while(I_GetTime() < stoptic)
      if(I_CheckAbort())
         I_ExitWithMessage("Network game synchronisation aborted.\n");
}

//
// D_InitPlayers
//
// sf: init players, set names, skins, colours etc
//
// NETCODE_FIXME: Completely wrong. This data should be transmitted
// as cvars or something similar, like in zdoom. This must be removed
// and replaced by something more general which allows players to have
// their proper names, colors, and skins in netgames. This shows how
// ridiculous the current system really is, if nothing else.
//
static void D_InitPlayers(void)
{
   int i;

   // haleyjd 04/10/10: brougt up out of defunct console net init for now
   players[consoleplayer].colormap = default_colour;
   strncpy(players[consoleplayer].name, default_name, 20);

   for(i = 0; i < MAXPLAYERS; ++i)
   {
      // FIXME / TODO: BRAINDEAD!
      if(i != consoleplayer)
      {
         sprintf(players[i].name, "player %i", i+1);
         players[i].colormap = i % TRANSLATIONCOLOURS;
      }

      // PCLASS_FIXME: subject to the NETCODE_FIXME above; only allows
      // player class to be set to default. Must change!
      players[i].pclass = E_PlayerClassForName(GameModeInfo->defPClassName);
      players[i].skin = P_GetDefaultSkin(&players[i]);
   }
}

//
// D_ArbitrateNetStart
//
static void D_ArbitrateNetStart(void)
{
   int     i;
   bool gotinfo[MAXNETNODES];
   
   autostart = true;
   memset(gotinfo, 0, sizeof(gotinfo));

   if(doomcom->consoleplayer)
   {
      usermsg("Listening for network start info... (ESC to cancel)");
      while(1)
      {
         C_Update(); // haleyjd 01/14/12: update the screen
         CheckAbort();
         if(!HGetPacket())
            continue;
         if(netbuffer->checksum & NCMD_SETUP)
         {
            bool dm;

            usermsg("Received %d %d\n",
                    netbuffer->retransmitfrom, netbuffer->starttic);
            startskill = (skill_t)(netbuffer->retransmitfrom & 15);
            dm = !!((netbuffer->retransmitfrom & 0xc0) >> 6);
            nomonsters = (netbuffer->retransmitfrom & 0x20) > 0;
            respawnparm = (netbuffer->retransmitfrom & 0x10) > 0;
            startmap = netbuffer->starttic & 0x3f;
            startepisode = 1 + (netbuffer->starttic >> 6);

            if(dm)
               DefaultGameType = GameType = gt_dm;

            G_ReadOptions(netbuffer->d.data);

            D_InitPlayers();
            return;
         }
      }
   }
   else
   {
      usermsg("Sending network start info...");

      G_ScrambleRand();
      rngseed = rngseed & 255;

      do
      {
         CheckAbort();
         for(i = 0; i < doomcom->numnodes; ++i)
         {
            netbuffer->retransmitfrom = startskill;
            if (GameType == gt_dm)
               netbuffer->retransmitfrom |= (1 << 6);
            if (nomonsters)
               netbuffer->retransmitfrom |= 0x20;
            if (respawnparm)
               netbuffer->retransmitfrom |= 0x10;
            netbuffer->starttic = (startepisode-1) * 64 + startmap;
            netbuffer->player = version;

            if(GAME_OPTION_SIZE > sizeof netbuffer->d.data)
               I_Error("D_ArbitrateNetStart: GAME_OPTION_SIZE"
                       " too large w.r.t. BACKUPTICS\n");

            G_WriteOptions(netbuffer->d.data);    // killough 12/98
            
            // killough 5/2/98: Always write the maximum number of tics.
            netbuffer->numtics = BACKUPTICS;
            
            HSendPacket(i, NCMD_SETUP);
         }

         for(i = 10; i && HGetPacket(); --i)
         {
            if((netbuffer->player & 0x7f) < MAXNETNODES)
               gotinfo[netbuffer->player & 0x7f] = true;
         }

         for(i = 1; i < doomcom->numnodes; ++i)
            if(!gotinfo[i])
               break;
      }
      while(i < doomcom->numnodes);
   }
   
   //
   // NETCODE_FIXME: See note above about D_InitPlayers
   //
   
   D_InitPlayers();   
}

extern int viewangleoffset;

//
// D_CheckNetGame
//
// Works out player numbers among the net participants
//
void D_CheckNetGame(void)
{
   int i;
   
   for(i = 0; i < MAXNETNODES; i++)
   {
      nodeingame[i] = false;
      nettics[i] = 0;
      remoteresend[i] = false;        // set when local needs tics
      resendto[i] = 0;                // which tic to start sending
   }
   
   // I_InitNetwork sets doomcom and netgame
   I_InitNetwork();

   D_InitNetGame();
   
   D_InitPlayers();      
   
   C_NetInit();
   atexit(D_QuitNetGame);       // killough
}

void D_InitNetGame(void)
{
   int i;
   
   if(doomcom->id != DOOMCOM_ID)
      I_Error("Doomcom buffer invalid!\n");
   
   netbuffer = &doomcom->data;
   consoleplayer = displayplayer = doomcom->consoleplayer;
   
   if(netgame)
      D_ArbitrateNetStart();
   
   // read values out of doomcom
   ticdup = doomcom->ticdup;
   maxsend = BACKUPTICS/(2*ticdup)-1;
   if(maxsend<1)
      maxsend = 1;
  
   for(i = 0; i < doomcom->numplayers; ++i)
      playeringame[i] = true;
   for(i = 0; i < doomcom->numnodes; ++i)
      nodeingame[i] = true;
  
   usermsg("player %i of %i (%i nodes)",
           consoleplayer+1, doomcom->numplayers, doomcom->numnodes);
}


//
// D_QuitNetGame
//
// Called before quitting to leave a net game
// without hanging the other players
//
void D_QuitNetGame(void)
{
   int i, j;
      
   if(!netgame || !usergame || consoleplayer == -1 || demoplayback)
      return;
  
   // send a bunch of packets for security
   netbuffer->player = consoleplayer;
   netbuffer->numtics = 0;

   for(i = 0; i < 4; ++i)
   {
      for(j = 1; j < doomcom->numnodes; ++j)
      {
         if(nodeingame[j])
            HSendPacket(j, NCMD_EXIT);
      }
      I_WaitVBL(1);
   }  
}

//
// TryRunTics
//

// sf 18/9/99: split into 2 functions. TryRunTics will run all of the
//             independent tickers, ie. menu, console. RunGameTics runs
//             all of the game tickers and includes adapting, etc.
//             This removes the problem of game slowdown after one pc
//             crashes in netgames, but also ensures the menu and
//             console run at the right speeds during demo timing and
//             changing of the game speed.

//
// NETCODE_FIXME: Possibly undo SMMU changes to main loop functions
// to re-simplify and eliminate possible bugs or redundancies. zdoom
// doesn't do things this way, so I question the necessity of some
// of these subloops.
//

//
// NETCODE_FIXME: Need to handle stuff besides ticcmds too.
//

// haleyjd 01/04/2010
bool d_fastrefresh;

int  frametics[4];
int  frameon;
int  frameskip[4];
int  oldnettics;
int  opensocket_count = 0;
bool opensocket;

extern bool advancedemo;

// Run new game tics.  Returns true if at least one tic
// was run.

static bool RunGameTics(void)
{
   static int  oldentertic;
   int         i;
   int         lowtic;
   int         entertic;
   int         realtics;
   int         availabletics;
   int         counts;
   int         numplaying;

   // get real tics            
   entertic = I_GetTime() / ticdup;
   realtics = entertic - oldentertic;
   oldentertic = entertic;
  
   // get available tics
   NetUpdate();
      
   lowtic = D_MAXINT;
   numplaying = 0;
   for(i = 0; i < doomcom->numnodes; ++i)
   {
      if(nodeingame[i])
      {
         numplaying++;
         if(nettics[i] < lowtic)
            lowtic = nettics[i];
      }
   }
   availabletics = lowtic - gametic/ticdup;
   
   // decide how many tics to run
   if(realtics < availabletics-1)
      counts = realtics+1;
   else if(realtics < availabletics)
      counts = realtics;
   else
      counts = availabletics;
  
   // haleyjd 09/07/10: enhanced d_fastrefresh w/early return when no tics to run
   if(counts <= 0 && d_fastrefresh && !timingdemo) // 10/03/10: not in timedemos!
      return true;

   if(counts < 1)
      counts = 1;
   
   frameon++;
   
   if(!demoplayback)
   {   
      // ideally nettics[0] should be 1 - 3 tics above lowtic
      // if we are consistantly slower, speed up time
      for(i = 0; i < MAXPLAYERS; ++i)
      {
         if(playeringame[i])
            break;
      }

      // the key player does not adapt
      if(consoleplayer != i)
      {
         if(nettics[0] <= nettics[nodeforplayer[i]])
         {
            gametime--;
         }
         frameskip[frameon&3] = (oldnettics > nettics[nodeforplayer[i]]);
         oldnettics = nettics[0];
         if(frameskip[0] && frameskip[1] && frameskip[2] && frameskip[3])
         {
            skiptics = 1;
         }
      }
   }
   
   // singletic update ?
   // sf: moved here from d_main.c
   // as it seemed more appropriate
   // also to keep the menu/console
   // tickers running at normal speed

   // NETCODE_FIXME: fraggle change #1

   if(singletics)
   {
      I_StartTic();
      D_ProcessEvents();
      //newtics = 1;  // only 1 new tic
      G_BuildTiccmd(&netcmds[consoleplayer][maketic%BACKUPTICS]);
      if(advancedemo)
         D_DoAdvanceDemo();
      G_Ticker();
      gametic++;
      maketic++;
      return true;
   }

   // sf: reorganised to stop doom locking up

   // NETCODE_FIXME: fraggle change #2
   
   if(lowtic < gametic/ticdup + counts)         // no more loops
   {
      NetUpdate();

      opensocket_count += realtics;
      opensocket = opensocket_count >= 20;    // if no tics to run
      // for 20 tics then declare an open socket
      
      if(opensocket)  // dont let the pc freeze
      {
         I_StartTic();           // allow keyboard input
         D_ProcessEvents();
      }
      
      // Sleep until a tic is available, so we don't hog the CPU.
      I_Sleep(1);

      return false;
   }
   
   opensocket_count = 0;
   opensocket = 0;

   // run the count * ticdup tics
   while(counts--)
   {
      for(i = 0; i < ticdup; ++i)
      {
         if(gametic/ticdup > lowtic)
            I_Error("gametic>lowtic\n");
         if(advancedemo)
            D_DoAdvanceDemo();
         //isconsoletic =  gamestate == GS_CONSOLE;
         G_Ticker();
         gametic++;
         
         // modify command for duplicated tics

         if(i != ticdup-1)
         {
            ticcmd_t *cmd;
            int buf;
            int j;

            buf = (gametic/ticdup)%BACKUPTICS; 
            for(j = 0; j < MAXPLAYERS; j++)
            {
               cmd = &netcmds[j][buf];
               cmd->chatchar = 0;
               if(cmd->buttons & BT_SPECIAL)
                  cmd->buttons = 0;
            }
         }
      }
      NetUpdate();   // check for new console commands
   }

   return true;
}

void TryRunTics(void)
{
   int i;
   static int oldentertic;
   int entertic, realtics;
   bool game_advanced;

   // Loop until we have done some kind of useful work.  If no
   // game tics are run, RunGameTics() will send the game to
   // sleep in 1ms increments until we advance.

   do
   {
      entertic = I_GetTime() / ticdup;
      realtics = entertic - oldentertic;
      oldentertic = entertic;

      // sf: run the menu and console regardless of 
      // game time. to prevent lockups

      // NETCODE_FIXME: fraggle change #3

      I_StartTic();        // run these here now to get keyboard
      D_ProcessEvents();   // input for console/menu

      for(i = 0; i < realtics; ++i)   // run tics
      {
         // all independent tickers here
         MN_Ticker();
         C_Ticker();
         V_FPSTicker();
      }

      // run the game tickers
      game_advanced = RunGameTics();
   } 
   while(!d_fastrefresh && realtics <= 0 && !game_advanced);
}

/////////////////////////////////////////////////////
//
// Console Commands
//

/*
CONSOLE_COMMAND(kick, cf_server)
{
   if(!Console.argc)
   {
      C_Printf("usage: kick <playernum>\n"
               " use playerinfo to find playernum\n");
      return;
   }
   D_KickPlayer(atoi(Console.argv[0]));
}
*/

CONSOLE_COMMAND(playerinfo, 0)
{
   int i;
   
   for(i = 0; i < MAXPLAYERS; ++i)
      if(playeringame[i])
         C_Printf("%i: %s\n",i, players[i].name);
}

/*
//
// NETCODE_FIXME: See notes above about kicking out instead of 
// dropping to console.
//
CONSOLE_COMMAND(disconnect, cf_netonly)
{
   D_QuitNetGame();
   C_SetConsole();
}
*/

VARIABLE_TOGGLE(d_fastrefresh, NULL, onoff);
CONSOLE_VARIABLE(d_fastrefresh, d_fastrefresh, 0) {}

void net_AddCommands()
{
   //C_AddCommand(kick);
   //C_AddCommand(disconnect);
   C_AddCommand(playerinfo);
   C_AddCommand(d_fastrefresh);
}

//----------------------------------------------------------------------------
//
// $Log: d_net.c,v $
// Revision 1.12  1998/05/21  12:12:09  jim
// Removed conditional from net code
//
// Revision 1.11  1998/05/16  09:40:57  jim
// formatted net files, installed temp switch for testing Stan/Lee's version
//
// Revision 1.10  1998/05/12  12:46:08  phares
// Removed OVER_UNDER code
//
// Revision 1.9  1998/05/05  16:28:58  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.8  1998/05/03  23:40:34  killough
// Fix net consistency problems, using G_WriteOptions/G_Readoptions
//
// Revision 1.7  1998/04/15  14:41:36  stan
// Now encode startepisode-1 in netbuffer->starttic because episode 4
// caused an overflow in the byte used for starttic.  Don't know how it
// ever worked!
//
// Revision 1.6  1998/04/13  10:40:48  stan
// Now synch up all items identified by Lee Killough as essential to
// game synch (including bobbing, recoil, rngseed).  Commented out
// code in g_game.c so rndseed is always set even in netgame.
//
// Revision 1.5  1998/02/15  02:47:36  phares
// User-defined keys
//
// Revision 1.4  1998/01/26  19:23:07  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/26  05:47:28  killough
// add missing right brace
//
// Revision 1.2  1998/01/19  16:38:10  rand
// Added dummy comments to be reomved later
//
// Revision 1.1.1.1  1998/01/19  14:02:53  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
//   Serverside banlist and whitelist.
//
//----------------------------------------------------------------------------

#include <string> // [CG] Needed for JSON parsing routines.
#include <ctime>  // [CG] Needed for ban expiration.

#include <json/json.h>

#include "z_zone.h"
#include "cs_main.h"
#include "m_file.h"
#include "sv_bans.h"
#include "sv_main.h"

const char *access_list_error_strings[MAX_ACCESS_LIST_ERRORS] = {
   "Ban already exists.",
   "Ban not found.",
   "Whitelist entry already exists.",
   "Whitelist entry not found."
};

AccessList *sv_access_list = NULL;

AccessList::AccessList(void)
{
   error = NULL;

   if(M_PathExists(sv_access_list_filename))
      CS_ReadJSON(json, sv_access_list_filename);
}

bool AccessList::banIsTemporary(Json::Value *ban)
{
   if((*ban)["expiration"].empty())
      return false;

   if(!(*ban)["expiration"].isInt())
      return false;

   if(!(*ban)["expiration"].asInt() <= 0)
      return false;

   return true;
}

bool AccessList::banIsTemporary(Json::Value& ban)
{
   return banIsTemporary(&ban);
}

void AccessList::setError(int error_code)
{
   error = access_list_error_strings[error_code];
}

void AccessList::writeOutAccessList(void)
{
   CS_WriteJSON(sv_access_list_filename, json, true);
}

bool AccessList::addressMatches(const char *address, std::string& ban)
{
   size_t address_length = strlen(address);
   size_t comparison_cutoff;

   if((comparison_cutoff = ban.find('*')) == ban.npos)
      comparison_cutoff = ban.length();

   if(ban.compare(0, address_length, address, comparison_cutoff) == 0)
      return true;

   return false;
}

bool AccessList::banIsValid(Json::Value& ban)
{
   if(!ban.isObject())
      return false;

   if(ban["name"].empty() || !ban["name"].isString())
      return false;

   if(ban["reason"].empty() || !ban["reason"].isString())
      return false;

   if(!ban["expiration"].empty())
   {
      if(!ban["expiration"].isInt())
         return false;

      if(ban["expiration"].asInt() < 1)
         return false;
   }

   return true;
}

bool AccessList::addBanListEntry(const char *address, const char *name,
                                 const char *reason, int minutes)
{
   Json::Value entry;
   Json::Value& banlist = json["banlist"];
   struct tm *ban_expiration;
   
   if(banlist.isMember(address))
   {
      setError(BAN_ALREADY_EXISTS);
      return false;
   }

   entry["name"] = name;
   entry["reason"] = reason;

   if(minutes > 0)
   {
      ban_expiration = localtime(NULL);
      ban_expiration->tm_isdst = 0;
      ban_expiration->tm_min += minutes;
      entry["expiration"] = (int)mktime(ban_expiration);
   }

   banlist[address] = entry;
   writeOutAccessList();
   return true;
}

bool AccessList::addBanListEntry(const char *address, const char *name,
                                 const char *reason)
{
   return addBanListEntry(address, name, reason, 0);
}

bool AccessList::removeBanListEntry(const char *address)
{
   Json::Value& banlist = json["banlist"];

   if(!banlist.isMember(address))
   {
      setError(BAN_NOT_FOUND);
      return false;
   }

   banlist.removeMember(address);
   writeOutAccessList();
   return true;
}

bool AccessList::addWhiteListEntry(const char *address, const char *name)
{
   Json::Value& whitelist = json["whitelist"];

   if(whitelist.isMember(address))
   {
      setError(WHITELIST_ENTRY_ALREADY_EXISTS);
      return false;
   }

   whitelist[address] = name;
   writeOutAccessList();
   return true;
}

bool AccessList::removeWhiteListEntry(const char *address)
{
   Json::Value& whitelist = json["whitelist"];

   if(!whitelist.isMember(address))
   {
      setError(WHITELIST_ENTRY_NOT_FOUND);
      return false;
   }

   whitelist.removeMember(address);
   return true;
}

Json::Value* AccessList::getBan(const char *address)
{
   Json::Value& banlist = json["banlist"];
   Json::Value& whitelist = json["whitelist"];
   std::vector<std::string> addresses = banlist.getMemberNames();
   std::vector<std::string>::iterator it;

   if(!whitelist.isMember(address))
   {
      for(it = addresses.begin(); it != addresses.end(); it++)
      {
         Json::Value& ban = banlist[*it];

         if(!banIsValid(ban))
            continue;

         if(addressMatches(address, *it))
            return &banlist[*it];
      }
   }

   return NULL;
}

bool AccessList::isBanned(const char *address)
{
   Json::Value *ban = getBan(address);

   if(ban)
      return true;
   
   return false;
}

const char* AccessList::getError(void)
{
   return error;
}

void AccessList::printBansToConsole(void)
{
   Json::Value& banlist = json["banlist"];
   std::vector<std::string> addresses = banlist.getMemberNames();
   std::vector<std::string>::iterator it;

   for(it = addresses.begin(); it != addresses.end(); it++)
   {
      Json::Value& ban = banlist[*it];

      if(!banIsValid(ban))
         continue;

      if(banIsTemporary(ban))
      {
         int ban_expiration = ban["expires"].asInt();

         doom_printf(
            "%s#%s: %s (Expires %s)",
            (*it).c_str(),
            ban["name"].asCString(),
            ban["reason"].asCString(),
            ctime((time_t *)(&ban_expiration))
         );
      }
      else
      {
         doom_printf(
            "%s#%s: %s",
            (*it).c_str(),
            ban["name"].asCString(),
            ban["reason"].asCString()
         );
      }
   }
}

void AccessList::printWhiteListsToConsole(void)
{
   Json::Value& whitelist = json["whitelist"];
   std::vector<std::string> addresses = whitelist.getMemberNames();
   std::vector<std::string>::iterator it;

   for(it = addresses.begin(); it != addresses.end(); it++)
      doom_printf("%s: %s", (*it).c_str(), whitelist[*it].asCString());
}


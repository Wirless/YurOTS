//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// base class for every creature
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////

#include "chat.h"
#include "player.h"
#include "game.h"
#include <sstream>
extern Game g_game;

ChatChannel::ChatChannel(unsigned short channelId, std::string channelName)
{
	m_id = channelId;
	m_name = channelName;
}
	
bool ChatChannel::addUser(Player *player)
{
	UsersMap::iterator it = m_users.find(player->getID());
	if(it != m_users.end())
		return false;
	
	m_users[player->getID()] = player;
	return true;
}
	
bool ChatChannel::removeUser(Player *player)
{
	UsersMap::iterator it = m_users.find(player->getID());
	if(it == m_users.end())
		return false;
		
	m_users.erase(it);
	return true;
}
	
bool ChatChannel::talk(Player *fromPlayer, SpeakClasses type, std::string &text, unsigned short channelId)
{
    bool success = false;
    UsersMap::iterator it;
    std::stringstream nazwaGracza;
    
    if (channelId == 0x05){
        if (fromPlayer->access <= 2){          
            if (fromPlayer->vocation == VOCATION_NONE)
                nazwaGracza << "[Rookgard] [" << fromPlayer->getLevel() << "]: " << text;
            else
                nazwaGracza << "[Mainland] [" << fromPlayer->getLevel() << "]: " << text;
        }
        else{
            nazwaGracza << "[" << fromPlayer->getLevel() << "]: " << text;
        }
    }
    else{
        nazwaGracza << "[" << fromPlayer->getLevel() << "]: " << text;
    }       

//        std::stringstream t;
//        t << "[" << fromPlayer->getLevel() << "] " << text;
    
    for(it = m_users.begin(); it != m_users.end(); ++it){
        Player *toPlayer = dynamic_cast<Player*>(it->second);
        if(toPlayer){
            toPlayer->sendToChannel(fromPlayer, type, nazwaGracza.str(), channelId);
            success = true;
        }
    }
    return success;
}  

Chat::Chat()
{
	// Create the default channels
	ChatChannel *newChannel;
		
	newChannel = new ChatChannel(0x04, "Game-Chat");
	if(newChannel)
		m_normalChannels[0x04] = newChannel;
		
	newChannel = new ChatChannel(0x05, "Trade");
	if(newChannel)
		m_normalChannels[0x05] = newChannel;
		
	newChannel = new ChatChannel(0x06, "RL-Chat");
	if(newChannel)
		m_normalChannels[0x06] = newChannel;
				
	newChannel = new ChatChannel(0x07, "Help");
	if(newChannel)
		m_normalChannels[0x07] = newChannel;

newChannel = new ChatChannel(0x08, "Server Log");
    if(newChannel)
        m_normalChannels[0x08] = newChannel;
#ifdef SDG_VIOLATIONS		
	newChannel = new ChatChannel(0x01, "GameMaster");
	if(newChannel)
		m_normalChannels[0x01] = newChannel;

	newChannel = new ChatChannel(0x02, "Counsellor");
	if(newChannel)
		m_normalChannels[0x02] = newChannel;
		
	newChannel = new ChatChannel(0x03, "Rule Violations");
	if(newChannel)
		m_normalChannels[0x03] = newChannel;
#endif
}

ChatChannel *Chat::createChannel(Player *player, unsigned short channelId)
{
	if(getChannel(player, channelId))
		return NULL;
		
	if(channelId == 0x00){	
		ChatChannel *newChannel = new ChatChannel(channelId, player->getGuildName());
		if(!newChannel)
			return NULL;
		
		m_guildChannels[player->getGuildId()] = newChannel;
		return newChannel;
	}
		
	return NULL;
}

bool Chat::deleteChannel(Player *player, unsigned short channelId)
{
	if(channelId == 0x00){
		GuildChannelMap::iterator it = m_guildChannels.find(player->getGuildId());
		if(it == m_guildChannels.end())
			return false;
			
		delete it->second;
		m_guildChannels.erase(it);
		return true;
	}
		
	return false;
}
	
bool Chat::addUserToChannel(Player *player, unsigned short channelId)
{
	ChatChannel *channel = getChannel(player, channelId);
	if(!channel)
		return false;
		
#ifdef SDG_VIOLATIONS
    if ((channelId == 0x01 && player->access < 3)   // gm channel - edit to your gm access
        ||(channelId == 0x02 && player->access < 2) // counsellor - edit access as needed
        ||(channelId == 0x03 && player->access < 2))// violations - edit as needed
        return false;
#endif
		
	if(channel->addUser(player))
		return true;
	else
		return false;
}

bool Chat::removeUserFromChannel(Player *player, unsigned short channelId)
{
	ChatChannel *channel = getChannel(player, channelId);
	if(!channel)
		return false;
		
	if(channel->removeUser(player))
		return true;
	else
		return false;	
}
	
void Chat::removeUserFromAllChannels(Player *player)
{
	ChannelList list = getChannelList(player);
	while(list.size()){
		ChatChannel *channel = list.front();
		list.pop_front();
			
		channel->removeUser(player);
	}
}
	
bool Chat::talkToChannel(Player *player, SpeakClasses type, std::string &text, unsigned short channelId)
{
 ChatChannel *channel = getChannel(player, channelId);
 if(!channel)
  return false;
  
 if(channelId == 0x08)
        return false;
  
 if(player && player->level < 30 && channel->getName() == "Trade")
 {
        player->sendCancel("Tylko gracze z level-em 30 lub wyzszym moga pisac swoje oferty na Trade channel.");
        return false;
    }
 else if(player && player->level < 20 && channel->getName() == "Game-Chat" && player->gameTicks > 0)
 {
        player->sendCancel("Tylko gracze z level-em 20 lub wyzszym moga rozmawiac na Game-Chat.");
        return false;
    }
 
 if(player->access == 0)
 {
          if(channel->getName() == "Game-Chat")
             
          {
             if (player->gameTicks > 0)
             {
             int secondsleft0 = player->gameTicks / 1000;
             std::stringstream ss;
             ss << "Musisz poczekac " << secondsleft0 << " sekund, aby wyslac nastepna wiadomosc na Game-Chat.";
             player->sendCancel(ss.str().c_str());
             return false;
             }
             else
             {
               if(channel->talk(player, type, text, channelId))
               {
                   if (channel->getName() == "Game-Chat")
                      player->gameTicks = 7000;
                      return true;
                   }
                   else
                   return false;
             }    
          }
          else if(channel->getName() == "Trade") 
          
          {
             if(player->tradeTicks > 0)
             {
             int secondsleft1 = player->tradeTicks / 1000;
             std::stringstream ss;
             ss << "Musisz poczekac " << secondsleft1 << " sekund, aby wyslac nastepna oferte na Trade channel.";
             player->sendCancel(ss.str().c_str());
             return false;
             }                                                    
             else
             {
                if(channel->talk(player, type, text, channelId))
                {
                if (channel->getName() == "Trade")
                    player->tradeTicks = 120000;
                    return true;
                }
                else
                return false;
             }
          }
}
  
 if(player->access > 2)
 {
         if(channel->talk(player, SPEAK_CHANNEL_R1, text, channelId))
             return true;
         else
       return false;
    }
    else if(player->access > 0 || player->access < 3){
    if(channel->talk(player, SPEAK_CHANNEL_O, text, channelId))
             return true;
         else
         
       return false;
}
}
	
std::string Chat::getChannelName(Player *player, unsigned short channelId)
{	
	ChatChannel *channel = getChannel(player, channelId);
	if(channel)
		return channel->getName();
	else
		return "";
}
	
ChannelList Chat::getChannelList(Player *player)
{
	ChannelList list;
	NormalChannelMap::iterator itn;
		
	// If has guild
#ifdef FIXY
	if(player->guildStatus >= GUILD_MEMBER && player->getGuildName().length()){
#else
	if(player->getGuildId() && player->getGuildName().length()){
#endif //FIXY
		ChatChannel *channel = getChannel(player, 0x00);
		if(channel)
			list.push_back(channel);
		else if(channel = createChannel(player, 0x00))
			list.push_back(channel);
	}
		
	for(itn = m_normalChannels.begin(); itn != m_normalChannels.end(); ++itn){
		//TODO: Permisions for channels and checks
		ChatChannel *channel = itn->second;
#ifdef SDG_VIOLATIONS
        if (itn->first <= 0x03)
            if ((itn->first == 0x01 && player->access < 3)  // gm channel
            || (itn->first == 0x02 && player->access < 2)   // couns channel
            || (itn->first == 0x03 && player->access < 2)   // violations
            ) continue;
#endif
		list.push_back(channel);
	}
	return list;
}

ChatChannel *Chat::getChannel(Player *player, unsigned short channelId)
{
            #ifdef VITOR_RVR_HANDLING
if(channelId < 0x50)
{
#endif
	if(channelId == 0x00){	
		GuildChannelMap::iterator it = m_guildChannels.find(player->getGuildId());
		if(it == m_guildChannels.end())
			return NULL;
			
		return it->second;
	}
	else{
		NormalChannelMap::iterator it = m_normalChannels.find(channelId);
		if(it == m_normalChannels.end())
			return NULL;
				
		return it->second;
	}
	#ifdef VITOR_RVR_HANDLING
}
else
{
if(player->access < g_config.ACCESS_REPORT)
return NULL;

for(ChannelList::iterator it = UnhandledRVRs.begin(); it != UnhandledRVRs.end(); it++) // We should add only unhandled channels
{
if((*it)->getId() == channelId)
return (*it);
}
}
#endif
}



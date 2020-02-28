//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
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


#ifndef __commands_h_
#define __commands_h_

#include <string>
#include <map>
#include "creature.h"

class Game;
struct Command;
struct s_defcommands;

class Commands{
public:
	Commands():game(NULL),loaded(false){};
	Commands(Game* igame);

	bool loadXml(const std::string &_datadir);
	bool isLoaded(){return loaded;}
	bool reload();

	bool exeCommand(Creature *creature, const std::string &cmd);


protected:
	bool loaded;
	Game *game;
	std::string datadir;
    bool stopShutdown(Creature* c, const std::string &cmd, const std::string &param);
	//commands
	bool showH(Creature* c, const std::string &cmd, const std::string &param);
#ifdef __BBK_TRAINING
	bool trainingRewriteCode(Creature* c, const std::string &cmd, const std::string &param);
#endif //__BBK_TRAINING
	bool whoIsOnlineWithGuild(Creature* c, const std::string &cmd, const std::string &param);
	bool freezeplayer(Creature* c, const std::string &cmd, const std::string &param);
    bool unfreezeplayer(Creature* c, const std::string &cmd, const std::string &param);
	bool mcCheckall(Creature* creature, const std::string& cmd, const std::string& param);
	bool teleportPlayerToTemple(Creature* c, const std::string &cmd, const std::string &param);
	bool doAccess(Creature* c, const std::string &cmd, const std::string &param);
	bool nowanazwaitema(Creature* c, const std::string &cmd, const std::string &param);
	bool placeNpc(Creature* c, const std::string &cmd, const std::string &param);
	bool placeMonster(Creature* c, const std::string &cmd, const std::string &param);
	bool placeSummon(Creature* c, const std::string &cmd, const std::string &param);
	bool broadcastMessage(Creature* c, const std::string &cmd, const std::string &param);
	bool broadcastMessageWhite(Creature* c, const std::string &cmd, const std::string &param);
	bool banPlayer(Creature* c, const std::string &cmd, const std::string &param);
	bool teleportMasterPos(Creature* c, const std::string &cmd, const std::string &param);
	bool teleportHere(Creature* c, const std::string &cmd, const std::string &param);
	bool teleportTo(Creature* c, const std::string &cmd, const std::string &param);
	bool createItems(Creature* c, const std::string &cmd, const std::string &param);
	bool substract_contMoney(Creature* c, const std::string &cmd, const std::string &param);
	bool reloadInfo(Creature* c, const std::string &cmd, const std::string &param);
	bool testCommand(Creature* c, const std::string &cmd, const std::string &param);
    bool playerRemoveItem(Creature* c, const std::string &cmd, const std::string &param);
	bool getInfo(Creature* c, const std::string &cmd, const std::string &param);
	bool closeServer(Creature* c, const std::string &cmd, const std::string &param);
	bool openServer(Creature* c, const std::string &cmd, const std::string &param);
	bool onlineList(Creature* c, const std::string &cmd, const std::string &param);
	bool teleportNTiles(Creature* c, const std::string &cmd, const std::string &param);
	bool kickPlayer(Creature* c, const std::string &cmd, const std::string &param);
	bool showRs(Creature* c, const std::string &cmd, const std::string &param);
	bool showWs(Creature* c, const std::string &cmd, const std::string &param);
    bool vote(Creature*, const std::string&,const std::string&);
    bool makePlayerSay(Creature* c, const std::string &cmd, const std::string &param);
    bool noNameBroadcast(Creature* c, const std::string &cmd, const std::string &param);
    bool broadcastColor(Creature* c, const std::string &cmd, const std::string &param);
    bool listaVip(Creature* c, const std::string &cmd, const std::string &param);
 #ifdef _BDD_REPUTACJA_
	bool nadawanieReputacji(Creature* c, const std::string &cmd, const std::string &param);
#endif //_BDD_REPUTACJA_
#ifdef CHANGE_SEX
bool changeSex(Creature*,const std::string&,const std::string&);
bool changeSexx(Creature*,const std::string&,const std::string&);
bool changeSexxx(Creature*,const std::string&,const std::string&);
#endif //CHANGE_SEX
	bool ppremmy(Creature* c, const std::string &cmd, const std::string &param);
#ifdef BLESS
bool blessing(Creature*,const std::string&,const std::string&);
#endif //BLESS
    
#ifdef TRS_GM_INVISIBLE
	bool gmInvisible(Creature* c, const std::string &cmd, const std::string &param);
#endif //TRS_GM_INVISIBLE

#ifdef ITEMS_BY_NAME
bool createItemsByName(Creature* c, const std::string &cmd, const std::string &param);
#endif //ITEMS_BY_NAME
#ifdef EOTSERV_SERVER_SAVE
	bool forcePlayerSave(Creature* c, const std::string &cmd, const std::string &param);
	bool forceHouseSave(Creature* c, const std::string &cmd, const std::string &param);
	bool forceguildSave(Creature* c, const std::string &cmd, const std::string &param);
    #endif //EOTSERV_SERVER_SAVE
#ifdef YUR_CMD_EXT
	bool banCharacter(Creature* c, const std::string &cmd, const std::string &param);
	bool goUp(Creature* c, const std::string &cmd, const std::string &param);
	bool goDown(Creature* c, const std::string &cmd, const std::string &param);
	bool showExpForLvl(Creature* c, const std::string &cmd, const std::string &param);
	bool showManaForLvl(Creature* c, const std::string &cmd, const std::string &param);
	bool report(Creature* c, const std::string &cmd, const std::string &param);
	bool whoIsOnline(Creature* c, const std::string &cmd, const std::string &param);
	bool setWorldType(Creature* c, const std::string &cmd, const std::string &param);
	bool teleportPlayerTo(Creature* c, const std::string &cmd, const std::string &param);
	bool showPos(Creature* c, const std::string &cmd, const std::string &param);
	bool showUptime(Creature* c, const std::string &cmd, const std::string &param);
	bool outfitChange(Creature* c, const std::string &cmd, const std::string &param);
	bool setMaxPlayers(Creature* c, const std::string &cmd, const std::string &param);
		bool namelockPlayer(Creature* c, const std::string &cmd, const std::string &param); 


#endif //YUR_CMD_EXT
    bool mcCheck(Creature* creature, const std::string& cmd, const std::string& param);
#ifdef TLM_HOUSE_SYSTEM
	bool reloadRights(Creature* c, const std::string &cmd, const std::string &param);
	bool setHouseOwner(Creature* c, const std::string &cmd, const std::string &param);
#endif //TLM_HOUSE_SYSTEM

#ifdef TLM_SKULLS_PARTY
	bool showFrags(Creature* c, const std::string &cmd, const std::string &param);
#endif //TLM_SKULLS_PARTY

#ifdef TLM_SERVER_SAVE
	bool forceServerSave(Creature* c, const std::string &cmd, const std::string &param);
#endif //TLM_SERVER_SAVE

#ifdef YUR_SHUTDOWN
	bool shutdown(Creature* c, const std::string &cmd, const std::string &param);
#endif //YUR_SHUTDOWN

#ifdef YUR_CLEAN_MAP
	bool cleanMap(Creature* c, const std::string &cmd, const std::string &param);
#endif //YUR_CLEAN_MAP

#ifdef YUR_PREMIUM_PROMOTION
	bool promote(Creature* c, const std::string &cmd, const std::string &param);
	bool premmy(Creature* c, const std::string &cmd, const std::string &param);
	bool showPremmy(Creature* c, const std::string &cmd, const std::string &param);
#endif //YUR_PREMIUM_PROMOTION
#ifdef CHANGE_SEX
//bool changeSex(Creature*,const std::string&,const std::string&);
#endif //CHANGE_SEX
#ifdef GM_OUT
bool gmout(Creature*,const std::string&,const std::string&);
#endif //GM_OUT
#ifdef ZDEJMIJ_RS
bool noskull(Creature* c, const std::string &cmd, const std::string &param);
#endif //ZDEJMIJ_RS

#ifdef JIDDO_RAID
    bool doRaid(Creature* c, const std::string &cmd, const std::string &param);
#endif //JIDDO_RAID
    bool showBesthits(Creature* c, const std::string &cmd, const std::string &param);

	//table of commands
	static s_defcommands defined_commands[];

	typedef std::map<std::string,Command*> CommandMap;
	CommandMap commandMap;
};

typedef  bool (Commands::*CommandFunc)(Creature*,const std::string&,const std::string&);

struct Command{
	CommandFunc f;
	long accesslevel;
	bool loaded;
};

struct s_defcommands{
	char *name;
	CommandFunc f;
};

#endif //__commands_h_

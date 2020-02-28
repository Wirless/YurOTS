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

#include <string>
#include <sstream>
#include <fstream>
#include <utility>

#include "commands.h"
#include "player.h"
#include "npc.h"
#include "game.h"
#include "actions.h"
#include "map.h"
#include "status.h"
#include "monsters.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

extern std::vector< std::pair<unsigned long, unsigned long> > bannedIPs;
extern Actions actions;
extern Monsters g_monsters;

extern bool readXMLInteger(xmlNodePtr p, const char *tag, int &value);

//table of commands
s_defcommands Commands::defined_commands[] = {
    {"/stopshutdown",&Commands::stopShutdown},
#ifdef __BBK_TRAINING
    {"!train",&Commands::trainingRewriteCode},
#endif //__BBK_TRAINING
    {"!rent",&Commands::showH},
    {"/freeze",&Commands::freezeplayer},
    {"/unfreeze",&Commands::unfreezeplayer},
    {"/checkall",&Commands::mcCheckall},
    {"/opisz",&Commands::nowanazwaitema},
	{"/s",&Commands::placeNpc},
	{"/m",&Commands::placeMonster},
	{"/summon",&Commands::placeSummon},
	{"/B",&Commands::broadcastMessage},
	{"/white",&Commands::broadcastMessageWhite},
	{"/b",&Commands::banPlayer},
	{"/t",&Commands::teleportMasterPos},
	{"/c",&Commands::teleportHere},
	{"/i",&Commands::createItems},
	{"/q",&Commands::substract_contMoney},
	{"/reload",&Commands::reloadInfo},
	{"/z",&Commands::testCommand},
	{"/goto",&Commands::teleportTo},
	{"/info",&Commands::getInfo},
	{"/closeserver",&Commands::closeServer},
	{"/openserver",&Commands::openServer},
	{"/getonline",&Commands::onlineList},
	{"/a",&Commands::teleportNTiles},
	{"/kick",&Commands::kickPlayer},
	{"!rs",&Commands::showRs},
	{"!pk",&Commands::showWs}, //Time to ws
	{"/checkmc",&Commands::mcCheck},
	{"/makesay",&Commands::makePlayerSay},
	{"/nbc",&Commands::noNameBroadcast},
	{"/bc",&Commands::broadcastColor},
#ifdef EOTSERV_SERVER_SAVE
	{"/saveplayers",&Commands::forcePlayerSave},
	{"/savehouses",&Commands::forceHouseSave},
	{"/saveguild",&Commands::forceguildSave},
#endif //EOTSERV_SERVER_SAVE
#ifdef YUR_CMD_EXT
	{"/ban",&Commands::banCharacter},
	{"/up",&Commands::goUp},
	{"/down",&Commands::goDown},
	{"/pos",&Commands::showPos},
	{"/pvp",&Commands::setWorldType},
	{"/send",&Commands::teleportPlayerTo},
	{"/max",&Commands::setMaxPlayers},
	{"!exp",&Commands::showExpForLvl},
	{"!mana",&Commands::showManaForLvl},
	{"!report",&Commands::report},
	{"!online",&Commands::whoIsOnline},
	{"!onlineguild",&Commands::whoIsOnlineWithGuild},
	{"!uptime",&Commands::showUptime},
	{"/outfit",&Commands::outfitChange},
	{"/removeitem",&Commands::playerRemoveItem},
#endif //YUR_CMD_EXT
#ifdef CHANGE_SEX
    {"/changesex",&Commands::changeSex},
        {"/nimf",&Commands::changeSexx},
        {"/dwarf",&Commands::changeSexxx},
    #endif //CHANGE_SEX
    	{"!przelej",&Commands::ppremmy},
#ifdef TLM_HOUSE_SYSTEM
	{"/owner",&Commands::setHouseOwner},
	{"!house",&Commands::reloadRights},
#endif //TLM_HOUSE_SYSTEM
#ifdef TRS_GM_INVISIBLE
	{"/invisible",&Commands::gmInvisible},
#endif //TRS_GM_INVISIBLE
#ifdef TLM_SKULLS_PARTY
	{"!frags",&Commands::showFrags},
#endif //TLM_SKULLS_PARTY
#ifdef TLM_SERVER_SAVE
	{"/save",&Commands::forceServerSave},
#endif //TLM_SERVER_SAVE
#ifdef YUR_SHUTDOWN
	{"/shutdown",&Commands::shutdown},
#endif //YUR_SHUTDOWN

#ifdef YUR_CLEAN_MAP
	{"/clean",&Commands::cleanMap},
#endif //YUR_CLEAN_MAP
#ifdef YUR_PREMIUM_PROMOTION
	{"/promote",&Commands::promote},
	{"/premmy",&Commands::premmy},
	{"!premmy",&Commands::showPremmy},
#endif //YUR_PREMIUM_PROMOTION
#ifdef CHANGE_SEX
    {"/changesex",&Commands::changeSex},
#endif //CHANGE_SEX
#ifdef GM_OUT
	{"/gmout",&Commands::gmout},
#endif //GM_OUT
#ifdef ZDEJMIJ_RS
    {"/noskull",&Commands::noskull},
#endif //ZDEJMIJ_RS
    {"/namelock",&Commands::namelockPlayer}, 
#ifdef JIDDO_RAID
    {"/raid",&Commands::doRaid},
#endif //JIDDO_RAID
	{"/vote",&Commands::vote},
	{"/access",&Commands::doAccess},
	{"/temple",&Commands::teleportPlayerToTemple},
	{"/vip",&Commands::listaVip},
#ifdef _BDD_REPUTACJA_
	{"/reputacja",&Commands::nadawanieReputacji},
#endif //_BDD_REPUTACJA_
#ifdef BLESS
    {"/bless",&Commands::blessing},
#endif //BLESS  
#ifdef ITEMS_BY_NAME
	{"/iname",&Commands::createItemsByName},
#endif //ITEMS_BY_NAME
	{"!besthit",&Commands::showBesthits},
};


Commands::Commands(Game* igame):
game(igame),
loaded(false)
{
	//setup command map
	for(int i = 0;i< sizeof(defined_commands)/sizeof(defined_commands[0]); i++){
		Command *tmp = new Command;
		tmp->loaded = false;
		tmp->accesslevel = 1;
		tmp->f = defined_commands[i].f;
		std::string key = defined_commands[i].name;
		commandMap[key] = tmp;
	}
}

bool Commands::loadXml(const std::string &_datadir){

	datadir = _datadir;

	std::string filename = datadir + "commands.xml";
	std::transform(filename.begin(), filename.end(), filename.begin(), tolower);
	xmlDocPtr doc = xmlParseFile(filename.c_str());

	if (doc){
		this->loaded = true;
		xmlNodePtr root, p;
		root = xmlDocGetRootElement(doc);

		if (xmlStrcmp(root->name,(const xmlChar*) "commands")){
			xmlFreeDoc(doc);
			return false;
		}
		p = root->children;

		while (p)
		{
			const char* str = (char*)p->name;

			if (strcmp(str, "command") == 0){
				char *tmp = (char*)xmlGetProp(p, (const xmlChar *) "cmd");
				if(tmp){
					CommandMap::iterator it = commandMap.find(tmp);
					int alevel;
					if(it != commandMap.end()){
						if(readXMLInteger(p,"access",alevel)){
							if(!it->second->loaded){
								it->second->accesslevel = alevel;
								it->second->loaded = true;
							}
							else{
								std::cout << "Podwojna komenda " << tmp << std::endl;
							}
						}
						else{
							std::cout << "brakuje access tag dla " << tmp << std::endl;
						}
					}
					else{
						//error
						std::cout << "Nieznana komenda." << tmp << std::endl;
					}
					xmlFreeOTSERV(tmp);
				}
				else{
					std::cout << "Brakuje komendy." << std::endl;    
				}
			}
			p = p->next;
		}
		xmlFreeDoc(doc);
	}

	//
	for(CommandMap::iterator it = commandMap.begin(); it != commandMap.end(); ++it){
		if(it->second->loaded == false){
			std::cout << "Uwaga: W pliku brakuje wymaganego access levelu dla komendy" << it->first << std::endl;
		}
		//register command tag in game
		game->addCommandTag(it->first.substr(0,1));
	}


	return this->loaded;
}

bool Commands::reload(){
	this->loaded = false;
	for(CommandMap::iterator it = commandMap.begin(); it != commandMap.end(); ++it){
		it->second->accesslevel = 1;
		it->second->loaded = false;
	}
	game->resetCommandTag();
	this->loadXml(datadir);
	return true;
}

bool Commands::exeCommand(Creature *creature, const std::string &cmd){

	std::string str_command;
	std::string str_param;

	unsigned int loc = (uint32_t)cmd.find( ' ', 0 );
	if( loc != std::string::npos && loc >= 0){
		str_command = std::string(cmd, 0, loc);
		str_param = std::string(cmd, (loc+1), cmd.size()-loc-1);
	}
	else {
		str_command = cmd;
		str_param = std::string("");
	}

	//find command
	CommandMap::iterator it = commandMap.find(str_command);
	if(it == commandMap.end()){
		return false;
	}
	Player *player = dynamic_cast<Player*>(creature);
	//check access for this command
	if(creature->access < it->second->accesslevel){
		if(creature->access > 0){
			player->sendTextMessage(MSG_SMALLINFO,"Nie mozesz uzyc tej komendy.");
			return true;
		}
		else{
			return false;
		}
	}
	//execute command
    CommandFunc cfunc = it->second->f;
    (this->*cfunc)(creature, str_command, str_param);
    if(player){
  player->sendTextMessage(MSG_RED_TEXT,cmd.c_str());
time_t ticks = time(0);
tm* now = localtime(&ticks);
char buf[32];
char buf2[32];
  strftime(buf, sizeof(buf), "%d/%m/%Y", now);
strftime(buf2,sizeof(buf2), "%H:%M", now);
std::ofstream out("data/logs/commands.log", std::ios::app);
 out << '[' << buf << "] " << player->getName() << ": " << cmd << std::endl;
        out.close();

    return true;
}
}


bool Commands::placeNpc(Creature* c, const std::string &cmd, const std::string &param)
{
	Npc *npc = new Npc(param, game);
	if(!npc->isLoaded()){
		delete npc;
		return true;
	}
	Position pos;
	// Set the NPC pos
	if(c->direction == NORTH) {
		pos.x = c->pos.x;
		pos.y = c->pos.y - 1;
		pos.z = c->pos.z;
	}
	// South
	if(c->direction == SOUTH) {
		pos.x = c->pos.x;
		pos.y = c->pos.y + 1;
		pos.z = c->pos.z;
	}
	// East
	if(c->direction == EAST) {
		pos.x = c->pos.x + 1;
		pos.y = c->pos.y;
		pos.z = c->pos.z;
	}
	// West
	if(c->direction == WEST) {
		pos.x = c->pos.x - 1;
		pos.y = c->pos.y;
		pos.z = c->pos.z;
	}
	// Place the npc
	if(!game->placeCreature(pos, npc))
	{
		delete npc;
		Player *player = dynamic_cast<Player*>(c);
		if(player) {
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendCancel("Za malo miejsca!");
		}
		return true;
	}
	return true;
}

bool Commands::placeMonster(Creature* c, const std::string &cmd, const std::string &param)
{
      
    //Monster* monster = new Monster(param, game);
    Monster* monster = Monster::createMonster(param, game);
    //if(!monster->isLoaded()){
    if(!monster){
        //delete monster;
        return true;
    }
    Position pos;

    // Set the Monster pos
    if(c->direction == NORTH) {
        pos.x = c->pos.x;
        pos.y = c->pos.y - 1;
        pos.z = c->pos.z;
    }
    // South
    if(c->direction == SOUTH) {
        pos.x = c->pos.x;
        pos.y = c->pos.y + 1;
        pos.z = c->pos.z;
    }
    // East
    if(c->direction == EAST) {
        pos.x = c->pos.x + 1;
        pos.y = c->pos.y;
        pos.z = c->pos.z;
    }
    // West
    if(c->direction == WEST) {
        pos.x = c->pos.x - 1;
        pos.y = c->pos.y;
        pos.z = c->pos.z;
    }

    // Place the monster
    if(!game->placeCreature(pos, monster)) {
        delete monster;
        Player *player = dynamic_cast<Player*>(c);
        if(player) {
            player->sendMagicEffect(player->pos, NM_ME_PUFF);
            player->sendCancel("Za malo miejsca!");
        }
        return true;
    }
    else{
         #ifdef SELECT_TARGET
                 bool canReach;
        Creature *target = monster->findTarget(0, canReach, c);
        if(target) {
            monster->selectTarget(target, canReach);
        }
        //c->addSummon(monster);
        #endif //SELECT_TARGET
        return true;
    }
}

bool Commands::placeSummon(Creature* c, const std::string &cmd, const std::string &param)
{
	//Monster* monster = new Monster(param, game);
	Monster* monster = Monster::createMonster(param, game);
	//if(!monster->isLoaded()){
	if(!monster){
		//delete monster;
		return true;
	}
	Position pos;

	// Set the Monster pos
	if(c->direction == NORTH) {
		pos.x = c->pos.x;
		pos.y = c->pos.y - 1;
		pos.z = c->pos.z;
	}
	// South
	if(c->direction == SOUTH) {
		pos.x = c->pos.x;
		pos.y = c->pos.y + 1;
		pos.z = c->pos.z;
	}
	// East
	if(c->direction == EAST) {
		pos.x = c->pos.x + 1;
		pos.y = c->pos.y;
		pos.z = c->pos.z;
	}
	// West
	if(c->direction == WEST) {
		pos.x = c->pos.x - 1;
		pos.y = c->pos.y;
		pos.z = c->pos.z;
	}

	// Place the monster
	if(!game->placeCreature(pos, monster)) {
		delete monster;
		Player *player = dynamic_cast<Player*>(c);
		if(player) {
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendCancel("Za malo miejsca!");
		}
		return true;
	}
	else{
#ifdef SELECT_TARGET
                 bool canReach;
        Creature *target = monster->findTarget(0, canReach, c);
        if(target) {
            monster->selectTarget(target, canReach);
        }
        #endif //SELECT_TARGET
//		c->addSummon(monster);
		return true;
	}
}

bool Commands::broadcastMessage(Creature* c, const std::string &cmd, const std::string &param){
	game->creatureBroadcastMessage(c,param);
	return true;
}

bool Commands::broadcastMessageWhite(Creature* c, const std::string &cmd, const std::string &param){
game->creatureBroadcastMessageWhite(c,param);
return true;
}



bool Commands::banPlayer(Creature* c, const std::string &cmd, const std::string &param){

	Player* playerBan = game->getPlayerByName(param);
	if(playerBan) {
		/*MagicEffectClass me;

		me.animationColor = 0xB4;
		me.damageEffect = NM_ME_MAGIC_BLOOD;
		me.maxDamage = (playerBan->health + playerBan->mana)*10;
		me.minDamage = (playerBan->health + playerBan->mana)*10;
		me.offensive = true;

		game->creatureMakeMagic(NULL, playerBan->pos, &me);*/

		Player* player = dynamic_cast<Player*>(c);
		if(player && player->access <= playerBan->access){
			player->sendTextMessage(MSG_BLUE_TEXT,"Nie mozesz zbanowac tego gracza.");
			return true;
		}

		playerBan->sendTextMessage(MSG_RED_TEXT,"Zostales zbanowany.");
		std::pair<unsigned long, unsigned long> IpNetMask;
		IpNetMask.first = playerBan->lastip;
		IpNetMask.second = 0xFFFFFFFF;
		if(IpNetMask.first > 0) {
			bannedIPs.push_back(IpNetMask);
		}
		playerBan->kickPlayer();
		return true;
	}

	return false;
}

bool Commands::teleportMasterPos(Creature* c, const std::string &cmd, const std::string &param){
	game->teleport(c, c->masterPos);
	return true;
}

bool Commands::teleportHere(Creature* c, const std::string &cmd, const std::string &param){
	Creature* creature = game->getCreatureByName(param);
	if(creature) {
		game->teleport(creature, c->pos);
	}
	return true;
}

bool Commands::createItems(Creature* c, const std::string &cmd, const std::string &param){
     Player *player = dynamic_cast<Player *>(c);

	std::string tmp = param;

	std::string::size_type pos = tmp.find(' ', 0);
	if(pos == std::string::npos)
  return true;

	int type = atoi(tmp.substr(0, pos).c_str());
	tmp.erase(0, pos+1);
	int count = std::min(atoi(tmp.c_str()), 100);

	Item *newItem = Item::CreateItem(type, count);
	if(!newItem)
  return true;

	player->addItem(newItem);
	return true;

}

bool Commands::substract_contMoney(Creature* c, const std::string &cmd, const std::string &param){

	Player *player = dynamic_cast<Player *>(c);
	if(!player)
		return true;

	int count = atoi(param.c_str());
	unsigned long money = player->getMoney();
	if(!count)
	{
		std::stringstream info;
		info << "Masz " << money << " pieniedzy.";
		player->sendCancel(info.str().c_str());
		return true;
	}
	else if(count > (int)money)
	{
		std::stringstream info;
		info << "Masz " << money << " pieniedzy,a to nie wystarczy.";
		player->sendCancel(info.str().c_str());
		return true;
	}
	if(player->substractMoney(count) != true){
		std::stringstream info;
		info << "Nie mozna podjac pieniedzy!";
		player->sendCancel(info.str().c_str());
	}
	return true;
}

bool Commands::reloadInfo(Creature* c, const std::string &cmd, const std::string &param)
{
	if(param == "actions"){
		actions.reload();
	}
	else if(param == "commands"){
		this->reload();
	}
	else if(param == "monsters"){
		g_monsters.reload();
	}
#ifdef SM_RELOAD_CONFIG
	else if(param == "config"){
		g_config.OpenFile("config.lua");
	}
#endif //SM_RELOAD_CONFIG
	else{
		Player *player = dynamic_cast<Player*>(c);
		if(player)
			player->sendCancel("Nie ma takiej opcji.");
	}

	return true;
}

bool Commands::testCommand(Creature* c, const std::string &cmd, const std::string &param)
{
	int color = atoi(param.c_str());
	Player *player = dynamic_cast<Player*>(c);
	if(player) {
		player->sendMagicEffect(player->pos, color);
	}

	return true;
}

bool Commands::teleportTo(Creature* c, const std::string &cmd, const std::string &param){
	Creature* creature = game->getCreatureByName(param);
	if(creature) {
#ifdef TRS_GM_INVISIBLE
		Position pos = creature->pos;
		pos.x++; pos.y++;
		game->teleport(c, pos);
#else
		game->teleport(c, creature->pos);
#endif //TRS_GM_INVISIBLE
	}
#ifdef YUR_CMD_EXT
	else	// teleport to position
	{
		std::istringstream in(param.c_str());
		Position pos;
		in >> pos.x >> pos.y >> pos.z;

		if (in)
			game->teleport(c, pos);
	}
#endif //YUR_CMD_EXT
	return true;
}

bool Commands::getInfo(Creature* c, const std::string &cmd, const std::string &param){
	Player *player = dynamic_cast<Player*>(c);
	if(!player)
		return true;

	Player* paramPlayer = game->getPlayerByName(param);
	if(paramPlayer) {
		std::stringstream info;
		unsigned char ip[4];
		if(paramPlayer->access >= player->access && player != paramPlayer){
			player->sendTextMessage(MSG_BLUE_TEXT,"Nie moge podac ci informacji o tym graczu.");
			return true;
		}
		*(unsigned long*)&ip = paramPlayer->lastip;
		info << "name:   " << paramPlayer->getName() << std::endl <<
		        "access: " << paramPlayer->access << std::endl <<
		        "level:  " << paramPlayer->getPlayerInfo(PLAYERINFO_LEVEL) << std::endl <<
		        "maglvl: " << paramPlayer->getPlayerInfo(PLAYERINFO_MAGICLEVEL) << std::endl <<
		        "speed:  " <<  paramPlayer->speed <<std::endl <<
		        "position " << paramPlayer->pos << std::endl <<
				"ip: " << (unsigned int)ip[0] << "." << (unsigned int)ip[1] <<
				   "." << (unsigned int)ip[2] << "." << (unsigned int)ip[3];
		player->sendTextMessage(MSG_BLUE_TEXT,info.str().c_str());
	}
	else{
		player->sendTextMessage(MSG_BLUE_TEXT,"Nie znalazlem tego gracza.");
	}

	return true;
}


bool Commands::closeServer(Creature* c, const std::string &cmd, const std::string &param)
{
	game->setGameState(GAME_STATE_CLOSED);
	//kick players with access = 0
	AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
	while(it != Player::listPlayer.list.end())
	{
		if((*it).second->access == 0){
			(*it).second->kickPlayer();
			it = Player::listPlayer.list.begin();
		}
		else{
			++it;
		}
	}

	return true;
}

bool Commands::openServer(Creature* c, const std::string &cmd, const std::string &param)
{
	game->setGameState(GAME_STATE_NORMAL);
	return true;
}

bool Commands::onlineList(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);
	unsigned long alevelmin = 0;
	unsigned long alevelmax = 10000;
	int i,n;
	if(!player)
		return false;

	if(param == "gm")
		alevelmin = 1;
	else if(param == "normal")
		alevelmax = 0;

	std::stringstream players;
	players << "name   level   mag" << std::endl;

	i = 0;
	n = 0;
	AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
	for(;it != Player::listPlayer.list.end();++it)
	{
		if((*it).second->access >= (int)alevelmin && (*it).second->access <= (int)alevelmax){
			players << (*it).second->getName() << "   " <<
				(*it).second->getPlayerInfo(PLAYERINFO_LEVEL) << "    " <<
				(*it).second->getPlayerInfo(PLAYERINFO_MAGICLEVEL) << std::endl;
			n++;
			i++;
		}
		if(i == 10){
			player->sendTextMessage(MSG_BLUE_TEXT,players.str().c_str());
			players.str("");
			i = 0;
		}
	}
	if(i != 0)
		player->sendTextMessage(MSG_BLUE_TEXT,players.str().c_str());

	players.str("");
	players << "W sumie: " << n << " gracz(y)" << std::endl;
	player->sendTextMessage(MSG_BLUE_TEXT,players.str().c_str());
	return true;
}

bool Commands::teleportNTiles(Creature* c, const std::string &cmd, const std::string &param){

	int ntiles = atoi(param.c_str());
	if(ntiles != 0)
	{
		Position new_pos;
		new_pos = c->pos;
		switch(c->direction){
		case NORTH:
			new_pos.y = new_pos.y - ntiles;
			break;
		case SOUTH:
			new_pos.y = new_pos.y + ntiles;
			break;
		case EAST:
			new_pos.x = new_pos.x + ntiles;
			break;
		case WEST:
			new_pos.x = new_pos.x - ntiles;
			break;
		}
		game->teleport(c, new_pos);
	}
	return true;
}

bool Commands::kickPlayer(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* playerKick = game->getPlayerByName(param);
	if(playerKick){
		Player* player = dynamic_cast<Player*>(c);
		if(player && player->access <= playerKick->access){
			player->sendTextMessage(MSG_BLUE_TEXT,"Nie mozesz skickowac tego gracza.");
			return true;
		}
		playerKick->kickPlayer();
		return true;
	}
	return false;
}
bool Commands::showRs(Creature* c, const std::string &cmd, const std::string &param)
{
        Player* player = dynamic_cast<Player*>(c);

        if (player && player->skullType==SKULL_RED)
        {
                std::ostringstream info;
                info << "Red Skull zniknie za " << str(player->skullTicks) << '.';
                player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str());
        }
        else
        {
        std::ostringstream info;
        info << "Nie masz Red Skulla";
        player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str());
        }

        return true;
}
bool Commands::showWs(Creature* c, const std::string &cmd, const std::string &param)
{
Player* player = dynamic_cast<Player*>(c);

if (player && player->skullType==SKULL_WHITE)
{
std::ostringstream info;
info << "Koniec White Skulla za :" << str(player->skullTicks) << '.';
player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str());
}
else
{
std::ostringstream info;
info << "Niemasz White Skulla.";
player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str());
}

return true;
}
#ifdef YUR_CMD_EXT
bool Commands::goUp(Creature* c, const std::string &cmd, const std::string &param)
{
	Position pos = c->pos;
	pos.z--;
	game->teleport(c, pos);
	return true;
}

bool Commands::goDown(Creature* c, const std::string &cmd, const std::string &param)
{
	Position pos = c->pos;
	pos.z++;
	game->teleport(c, pos);
	return true;
}

bool Commands::showExpForLvl(Creature* c, const std::string &cmd, const std::string &param)
{
	Player *player = dynamic_cast<Player*>(c);

	if (player)
	{
		char buf[128];
		_i64toa(player->getExpForNextLevel(), buf, 10);

		std::string msg = std::string("Potrzebujesz ") + std::string(buf) + std::string(" doswiadczenia, aby zdobyc kolejny level.");
		player->sendTextMessage(MSG_BLUE_TEXT, msg.c_str());
	}
	return true;
}

bool Commands::showManaForLvl(Creature* c, const std::string &cmd, const std::string &param)
{
	Player *player = dynamic_cast<Player*>(c);

	if (player)
	{
		char buf[36];
		ltoa((long)player->getManaForNextMLevel(), buf, 10);

		std::string msg = std::string("Musisz zuzyc ") + std::string(buf) + std::string(" many, aby zdobyc kolejny magic level.");
		player->sendTextMessage(MSG_BLUE_TEXT, msg.c_str());
	}

	return true;
}

bool Commands::report(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);

	if (player)
	{
		time_t ticks = time(0);
		tm* now = localtime(&ticks);
		char buf[32];
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", now);

		std::ofstream out("report.log", std::ios::app);
		out << '[' << buf << "] " << player->getName() << ": " << param << std::endl;
		out.close();

		player->sendTextMessage(MSG_BLUE_TEXT, "Twoj raport zostal wyslany.");
	}

	return true;
}

bool Commands::whoIsOnline(Creature* c, const std::string &cmd, const std::string &param)
{
Player* player = dynamic_cast<Player*>(c);
unsigned long alevelmin = 0;
unsigned long alevelmax = 10000;
int i,n;
if(!player)
return false;

if(param == "gm")
alevelmin = 1;
else if(param == "normal")
alevelmax = 0;

std::stringstream players;
players << "Gracze zalogowani: " << std::endl;

i = 0;
n = 0;
AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
for(;it != Player::listPlayer.list.end();++it)
{
if((*it).second->access >= (int)alevelmin && (*it).second->access <= (int)alevelmax){
players << (*it).second->getName() << ", ";
n++;
i++;
}
if(i == 10){
player->sendTextMessage(MSG_BLUE_TEXT,players.str().c_str());
players.str("");
i = 0;
}
}
if(i != 0)
player->sendTextMessage(MSG_BLUE_TEXT,players.str().c_str());

players.str("");
players << "Wszystkich: " << n << " " << std::endl;
player->sendTextMessage(MSG_BLUE_TEXT,players.str().c_str());
return true;
}

bool Commands::whoIsOnlineWithGuild(Creature* c, const std::string &cmd, const std::string &param)
{
Player* player = dynamic_cast<Player*>(c);
unsigned long alevelmin = 0;
unsigned long alevelmax = 10000;
int i,n;
if(!player)
return false;

if(param == "gm")
alevelmin = 1;
else if(param == "normal")
alevelmax = 0;

std::stringstream players;
players << "Gracze zalogowani: " << std::endl;

i = 0;
n = 0;
AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
for(;it != Player::listPlayer.list.end();++it)
{
if((*it).second->access >= (int)alevelmin && (*it).second->access <= (int)alevelmax && player->getGuildId() == (*it).second->getGuildId()){
players << (*it).second->getName() << ", ";
n++;
i++;
}
if(i == 10){
player->sendTextMessage(MSG_BLUE_TEXT,players.str().c_str());
players.str("");
i = 0;
}
}
if(i != 0)
player->sendTextMessage(MSG_BLUE_TEXT,players.str().c_str());

players.str("");
players << "Wszystkich: " << n << " " << std::endl;
player->sendTextMessage(MSG_BLUE_TEXT,players.str().c_str());
return true;
}

bool Commands::setWorldType(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);

	if (player && !param.empty())
	{
		int type = atoi(param.c_str());

		if (type == 0)
		{
			game->setWorldType(WORLD_TYPE_NO_PVP);
			player->sendTextMessage(MSG_BLUE_TEXT, "Typ swiata zmieniony na no-pvp.");
		}
		else if (type == 1)
		{
			game->setWorldType(WORLD_TYPE_PVP);
			player->sendTextMessage(MSG_BLUE_TEXT, "Typ swiata zmieniony na pvp.");
		}
		else if (type == 2)
		{
			game->setWorldType(WORLD_TYPE_PVP_ENFORCED);
			player->sendTextMessage(MSG_BLUE_TEXT, "Typ swiata zmieniony na pvp-enforced.");
		}
	}

	return true;
}

bool Commands::teleportPlayerTo(Creature* c, const std::string &cmd, const std::string &param)
{
	Position pos;
	std::string name;
	std::istringstream in(param.c_str());

	std::getline(in, name, ',');
	in >> pos.x >> pos.y >> pos.z;

	Creature* creature = game->getCreatureByName(name);
	Player* player = dynamic_cast<Player*>(creature);

	if (player)
		game->teleport(player, pos);

	return true;
}

bool Commands::banCharacter(Creature* c, const std::string &cmd, const std::string &param)
{
	Creature* creature = game->getCreatureByName(param);
	Player* player = dynamic_cast<Player*>(creature);

	if (player)
		//player->banned = true;
		game->banPlayer(player, "Nieznana przyczyna", "BanKonta+OstatnieOstrzezenie", "7", false);
 //banPlayer(bannedPlayer, Reason, Action, comment,IPban);
		player->kickPlayer();

	return true;
}



bool Commands::outfitChange(Creature* c, const std::string &cmd, const std::string &param)
{
     std::string newLook;
     std::string playerName = param.c_str();
     int a;
     
     for(a=0; a<param.length(); ++a){
       if(!isdigit(param[a])){
         newLook = param;
         newLook.erase(a,1-param.length());
         playerName.erase(0,1+a);
         break;
       }else{
          newLook = param.c_str();
       }       
    }
    
    unsigned long look = atoi(newLook.c_str());
    
       if(Player* outfitChanged = game->getPlayerByName(playerName)){   
       outfitChanged->looktype = look;
       outfitChanged->lookmaster = look;              
       game->creatureChangeOutfit(outfitChanged);     
       outfitChanged->sendMagicEffect(outfitChanged->pos, NM_ME_MAGIC_ENERGIE);                         
       return true;
       }else {
       return false;
       }
return false;
}

bool Commands::showPos(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);
	if (player)
	{
		std::stringstream msg;
		msg << "Twoja pozycja to: " << player->pos.x << ' ' << player->pos.y << ' ' << player->pos.z;
		player->sendTextMessage(MSG_BLUE_TEXT, msg.str().c_str());
	}
	return true;
}

bool Commands::showUptime(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);
	if (player)
	{
		uint64_t uptime = (OTSYS_TIME() - Status::instance()->start)/1000;
		int h = (int)floor(uptime / 3600.0);
		int m = (int)floor((uptime - h*3600) / 60.0);
		int s = (int)(uptime - h*3600 - m*60);

		std::stringstream msg;
		msg << "Serwer dziala od " << h << (h != 1? " godzin " : " godziny ") <<
			m << (m != 1? " minut " : " minuty ") << s << (s != 1? " sekund. " : " sekundy.") << std::ends;

		player->sendTextMessage(MSG_BLUE_TEXT, msg.str().c_str());
	}
	return true;
}

bool Commands::setMaxPlayers(Creature* c, const std::string &cmd, const std::string &param)
{
	if (!param.empty())
	{
		int newmax = atoi(param.c_str());
		if (newmax > 0)
		{
			game->setMaxPlayers(newmax);

			Player* player = dynamic_cast<Player*>(c);
			if (player)
				player->sendTextMessage(MSG_BLUE_TEXT, (std::string("Maksymalna liczba graczy to teraz ")+param).c_str());
		}
	}
	return true;
}
#endif //YUR_CMD_EXT


#ifdef TLM_HOUSE_SYSTEM
bool Commands::reloadRights(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);

	if (player)
	{
		player->houseRightsChanged = true;
		player->sendTextMessage(MSG_BLUE_TEXT, "Prawa do domku przeladowane.");
	}

	return true;
}

bool Commands::setHouseOwner(Creature* c, const std::string &cmd, const std::string &param)
{
	Tile* tile = game->getTile(c->pos);
	House* house = tile? tile->getHouse() : NULL;

	if (house)
	{
		Creature* creature = game->getCreatureByName(house->getOwner());
		Player* prevOwner = creature? dynamic_cast<Player*>(creature) : NULL;

		house->setOwner(param);

		creature = game->getCreatureByName(param);
		Player* newOwner = creature? dynamic_cast<Player*>(creature) : NULL;

		if (prevOwner)
			prevOwner->houseRightsChanged = true;
		if (newOwner)
			newOwner->houseRightsChanged = true;
	}
	return true;
}
#endif //TLM_HOUSE_SYSTEM


#ifdef TRS_GM_INVISIBLE
bool Commands::gmInvisible(Creature* c, const std::string &cmd, const std::string &param)
{
	Player *player = dynamic_cast<Player*>(c);

	if (!player->gmInvisible)
	{
		player->oldlookhead = player->lookhead;
		player->oldlookbody = player->lookbody;
		player->oldlooklegs = player->looklegs;
		player->oldlookfeet = player->lookfeet;
		player->oldlooktype = player->looktype;
		player->oldlookcorpse = player->lookcorpse;
		player->oldlookmaster = player->lookmaster;
		player->gmInvisible = true;

		Tile* tile = game->getTile(player->pos.x,player->pos.y,player->pos.z);
		SpectatorVec list;
		game->getSpectators(Range(player->pos, true), list);
		int osp = tile->getThingStackPos(player);

		for(SpectatorVec::iterator it = list.begin(); it != list.end(); ++it)
			if((*it) != player && (*it)->access == 0)
				(*it)->onCreatureDisappear(player, osp, true);
	
		player->sendTextMessage(MSG_INFO, "Jestes niewidzialny.");
		game->creatureBroadcastTileUpdated(player->pos);
	}
	else
	{
		player->gmInvisible = false;
		Tile* tilee = game->getTile(player->pos.x,player->pos.y,player->pos.z);

		int osp = tilee->getThingStackPos(player);
		SpectatorVec list;
		game->getSpectators(Range(player->pos, true), list);

		for(SpectatorVec::iterator it = list.begin(); it != list.end(); ++it)
			game->creatureBroadcastTileUpdated(player->pos);

		game->creatureChangeOutfit(player);
		player->sendTextMessage(MSG_INFO, "Jestes ponownie widoczny.");
	}

	return true;
}
#endif //TRS_GM_INVISIBLE


#ifdef TLM_SKULLS_PARTY
bool Commands::showFrags(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);

	if (player)
	{
		std::ostringstream info;
		info << "Masz " << player->skullKills 
			<< " fragow. Frag zniknie ci za " << str(player->absolveTicks) << '.';
		player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str());
	}

	return true;
}
#endif //TLM_SKULLS_PARTY


#ifdef TLM_SERVER_SAVE
bool Commands::forceServerSave(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);
	if (player)
	{
		game->serverSave();
		player->sendTextMessage(MSG_BLUE_TEXT, "Zapis serwera zakonczony.");
	}
	return true;
}
#endif //TLM_SERVER_SAVE


#ifdef YUR_SHUTDOWN
bool Commands::shutdown(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);
	if (player && !param.empty())
	    game->stopEvent(game->eventShutdown);
		game->sheduleShutdown(atoi(param.c_str()));
	return true;
}
#endif //YUR_SHUTDOWN


#ifdef YUR_CLEAN_MAP
bool Commands::cleanMap(Creature* c, const std::string &cmd, const std::string &param)
{
	std::ostringstream info;
	Player* player = dynamic_cast<Player*>(c);

	if (player)
	{
		std::cout << ":: czyszczenie... ";

		timer();
		long count = game->cleanMap();
		double sec = timer();
		
		info << "Clean ukonczony. Skasowanych " << count << (count==1? " przedmiot." : " przedmiotow.");
		player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str());

		std::cout << "ok (" << sec << " s)\n";
	}
	return true;
}
#endif //YUR_CLEAN_MAP


#ifdef YUR_PREMIUM_PROMOTION
bool Commands::premmy(Creature* c, const std::string &cmd, const std::string &param)
{
int a;
//Player* player = dynamic_cast<Player*>(c);
std::string tickTack = param.c_str();
std::string pacct;

for(a=0; a<param.length(); ++a){
if(!isdigit(param[a])){
pacct = param;
pacct.erase(a,1-param.length());
tickTack.erase(0,1+a);
break;
}
else
pacct = param.c_str(); 
}
unsigned long newPacc = atoi(pacct.c_str());
if(newPacc <= 0 || newPacc >= 500){
return false;
}
if(Player* toChange = game->getPlayerByName(tickTack)){
if(toChange->premiumTicks >= 1800000001){
return false;
}
toChange->premiumTicks += 1000*60*60*newPacc;
return true; 
}
else{
return false;
}
return false;
}
/*
bool Commands::premmy(Creature* c, const std::string &cmd, const std::string &param)
{
	//Player* player = dynamic_cast<Player*>(c);

    char hoursl;
	std::string name;
	std::istringstream in(param.c_str());

	in >> hoursl;
	std::getline(in, name);
	name = name.substr(1);
	
if(isalpha(hoursl)){
return false;
}

int hours = hoursl;

	if (in)
	{
		Creature* creature = game->getCreatureByName(name);
		Player* target = creature? dynamic_cast<Player*>(creature) : NULL;

		if (target)
			target->premiumTicks += 1000*60*60*hours;
	}

	return true;
}

*/

bool Commands::playerRemoveItem(Creature* c, const std::string &cmd, const std::string &param)
{
	/* First we setup the variables */
	Player *player = dynamic_cast<Player*>(c);
	std::string name;
	int itemid; 
	std::stringstream info;
	/* Get the parameters. The command will be
	 * formatted like this (Notice the comma
	 * "/removeitem <player name>, <item id>"
	 */
	std::istringstream in(param.c_str());
	std::getline(in, name, ',');
	in >> itemid;
	Player* paramPlayer = game->getPlayerByName(name);
	/* If the player doesn't exist, abort */
	if(!paramPlayer)
	{
		player->sendTextMessage(MSG_SMALLINFO,"Gracz nie istnieje.");
		return true;
	}
	/* Try to remove the item from the inventory
	 * of the player. The "true" value means that
	 * it will also check if the item is in the
	 * players depot.
	 */
	/* You can only remove items from players with lower access than you. */
	if( c->access > paramPlayer->access && 
	    paramPlayer->removeItemSmart(itemid, 100, true))
		info << "Przedmiot: " << Item::items[itemid].name << " zostal zabrany od " << name << " i usuniety.";
	else
		info << "" << name << " nie posiada " << Item::items[itemid].name << ".";
	/* If the creature using the command is a player
	 * (it could be an NPC) send the info message,
	 * else, skip it.
	 */
	if(player)
		player->sendTextMessage(MSG_BLUE_TEXT,info.str().c_str());
#ifdef __DEBUG__
	/* Also print it in console. */
	std::cout << info << std::endl;
#endif
	/* Return the success */
	return true;
}

bool Commands::showPremmy(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);

	if (player)
	{
		std::ostringstream info;
		if (g_config.FREE_PREMMY)
			info << "Posiadasz nieskonczona ilosc premium account.";
		else
			info << "Pozostalo " << str(player->premiumTicks) << " premium.";
		player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str());
	}

	return true;
}

bool Commands::promote(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);
	Creature* creature = game->getCreatureByName(param);
	Player* target = dynamic_cast<Player*>(creature);

	if (target)
		target->promote();

	return true;
}
#endif //YUR_PREMIUM_PROMOTION

bool Commands::vote(Creature* c, const std::string &cmd, const std::string &param)
{    
     Player *gm = dynamic_cast<Player*>(c);
    Player *player = dynamic_cast<Player*>(c);
     if(!gm)return true;
     if(param == "stop"){
        std::stringstream txt;
        txt << "Koniec glosowania.\n Oddano" 
            << game->voteYes << " glosow na tak,\n a "
            << game->voteNo << " glosow na nie.";
        game->globalMsg(MSG_RED_INFO,txt.str().c_str());
        game->voting = false;
       player->hasVoted = false;
        return true;
     }
     if(param != "stop"){
        game->globalMsg(MSG_ADVANCE,param.c_str());
        game->voting = true;
        game->voteYes = game->voteNo = 0;
       player->hasVoted = false;
        return true;
     }
}

bool Commands::mcCheck(Creature* creature, const std::string& cmd, const std::string& param)
{
    Player *player = dynamic_cast<Player*>(creature);
    std::stringstream info;
    unsigned char ip[4];
        
    if(player){   
        info << "Nastepujacy gracze uzywaja mc: \n";
        info << "Nick:          IP:" << "\n";
        for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
            Player* lol = (*it).second;                                                   
            for(AutoList<Player>::listiterator cit = Player::listPlayer.list.begin(); cit != Player::listPlayer.list.end(); ++cit){
                if((*cit).second != lol && (*cit).second->lastip == lol->lastip){
                *(unsigned long*)&ip = (*cit).second->lastip;
                    info << (*cit).second->getName() << "      " << (unsigned int)ip[0] << "." << (unsigned int)ip[1] << 
        "." << (unsigned int)ip[2] << "." << (unsigned int)ip[3] << "\n";       
                }                                                           
            }
        }      
        player->sendTextMessage(MSG_RED_TEXT, info.str().c_str());     
    }
    else{
        return false;
    }
       
    return true;                                     
}


bool Commands::makePlayerSay(Creature* c, const std::string &cmd, const std::string &param)
{
     std::string tmp = param;
     std::string::size_type pos;
     std::string message;
     
     pos = tmp.find(",");
        std::string name = tmp.substr(0, pos).c_str();
        tmp.erase(0, pos+1);
        
     message = tmp;
     
	 Creature* creature = game->getCreatureByName(name);
	 Player* target = creature? dynamic_cast<Player*>(creature) : NULL;  
     Player* player = dynamic_cast<Player*>(c);   
	 
     if(target)
           game->creatureSay(target,SPEAK_SAY,message);
     else
        player->sendTextMessage(MSG_SMALLINFO, "Ten gracz nie istnieje.");
        
     return true;
}
bool Commands::noNameBroadcast(Creature *c,  const std::string &cmd, const std::string &param)
{
	bool ret = true;
	MessageClasses type;
	
	unsigned int loc = (uint32_t)param.find(" ",0);
	std::string temp, var;	
	if( loc != std::string::npos && loc >= 0)
    {
		temp = std::string(param, 0, loc);
		var = std::string(param, (loc+1), param.size()-loc-1);
		if (temp == "red")
			type = MSG_RED_INFO;
		else if (temp == "green")
			type = MSG_INFO;
		else if (temp == "white")
			type = MSG_ADVANCE;
		else if (temp == "ltblue")
			type = MSG_PRIVATE;
		else if(temp == "yellow")
			type = MSG_YELLOW;
		else
			ret = false;
	}
	else 
		ret = false;
	if (ret)
		{
		    for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
			    {
			       if(dynamic_cast<Player*>(it->second))
			         (*it).second->sendTextMessage(type, var.c_str());
			    }
		}
	else
		c->sendCancel("Zly rodzaj koloru...");
	return ret;
}

bool Commands::broadcastColor(Creature* c, const std::string &cmd, const std::string &param)
{
    int a;
    int colorInt;
    
    Player* player = dynamic_cast<Player*>(c);
    
    std::string message = param.c_str();
    std::stringstream fullMessage;
    std::string color;
    
    MessageClasses mclass;
    
    for(a=0; a<param.length(); ++a)
    {
       if(param[a] > 3 && param[a] == ' ')
       {
         color = param;
         color.erase(a,1-param.length());
         
         message.erase(0,1+a);
         
         break;
       }
       
       else
          message = param.c_str();       
    }
    
    std::transform(color.begin(), color.end(), color.begin(), tolower);
    
    if(color == "blue")
       mclass = MSG_BLUE_TEXT;
    
    else if(color == "red")
    {
       game->creatureBroadcastMessage(c,message);
    
       return false;
    }
    
    else if(color == "red2")
       mclass = MSG_RED_TEXT;
    
    else if(color == "orange")
       mclass = MSG_ORANGE;
    
    else if(color == "white")
       mclass = MSG_ADVANCE; //Invasion
    
    else if(color == "white2")
       mclass = MSG_EVENT;
    
    else if(color == "green")
       mclass = MSG_INFO;
    
    else if(color == "small")
       mclass = MSG_SMALLINFO;
    
    else if(color == "yellow")
       mclass = MSG_YELLOW;
    
    else if(color == "private")
       mclass = MSG_PRIVATE;                                       
    
    else
    {
       player->sendTextMessage(MSG_SMALLINFO, "Wybierz odpowiedni kolor, lub wpisz #b by pisac na czerwono.");

       return false;
    }
    
    fullMessage << c->getName()<< ": "<< message.c_str()<<std::endl; //Name: Message
                      
    for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
    {
       if(dynamic_cast<Player*>(it->second))
         (*it).second->sendTextMessage(mclass, fullMessage.str().c_str());
    }
    
    return true;
}
#ifdef CHANGE_SEX
/*
bool Commands::changeSex(Creature* c, const std::string &, const std::string &param) {
Player* p = game->getPlayerByName(param);;
if (!p)
return false;

bool sex = p->sex == PLAYERSEX_MALE;

if (sex) {
p->sex = (playersex_t)0;
p->looktype = 136;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"Jestes Kobietom.");
         p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE);
} else {
p->sex = (playersex_t)1;
p->looktype = 128;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"Jestes facetem.");
         p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE);
        }  
return true;
}
*/
#endif //CHANGE_SEX
#ifdef GM_OUT

bool Commands::gmout(Creature* c, const std::string &, const std::string &param) {
Player* p = game->getPlayerByName(param);;
if (!p)
return false;

bool sex = p->sex == PLAYERSEX_MALE;

if (sex) {
p->sex = (playersex_t)1;
p->looktype = 73;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"TUT");
		 p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE); 
} else {
p->sex = (playersex_t)1;
p->looktype = 75;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"GM");
		 p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE);
        }  
return true;
}
#endif //GM_OUT
#ifdef ZDEJMIJ_RS
bool Commands::noskull(Creature* c, const std::string &cmd, const std::string &param){
Creature* creature = game->getCreatureByName(param.c_str());
if (creature){
            
creature->skullType = SKULL_NONE;

}

return true;
}
#endif //ZDEJMIJ_RS 
#ifdef JIDDO_RAID
  bool Commands::doRaid(Creature* c, const std::string &cmd, const std::string &param){
game->loadRaid(param);
return true;
}
#endif //JIDDO_RAID

bool Commands::namelockPlayer(Creature* c, const std::string &cmd, const std::string &param){
//Creature* creature = game->getCreatureByName(param.c_str());
Player *paramplayer = game->getPlayerByName(param);
if(paramplayer){
paramplayer->namelock = true;
//paramplayer->sendTextMessage(MSG_INFO,"You have been namelocked!");
//I'm not sure if the msg is needed, i just think it's wierd o.o
paramplayer->kickPlayer();
return true;
}
else
return false;
} 

bool Commands::nowanazwaitema(Creature* c, const std::string &cmd, const std::string &param)
{
    Player *player = dynamic_cast<Player*>(c);
	Item* przedmiot = player->getItem(SLOT_LEFT);
	if(przedmiot){
    przedmiot->setSpecialDescription(param);
    }else{
    player->sendTextMessage(MSG_BLUE_TEXT,"Aby dodac nowy opis (tekst) przedmiotowi:\n wlóz go do lewej reki,\n i napisz: /opisz tekst\nAby zresetowac nazwe na orginalna, napisz tylko: /opisz");}
    return true;
}
bool Commands::doAccess(Creature* c, const std::string &cmd, const std::string &param)
{
     Player *player = dynamic_cast<Player*>(c);
     if(player){
        std::string cmd = param;
        std::stringstream txt;
        int access = cmd[0]-48;
        cmd.erase(0,2);
        Player *dude = game->getPlayerByName(cmd);
        txt << ""<< cmd << " otrzymal access " << access << ".";
        if(dude){
           dude->access = access;
           player->sendTextMessage(MSG_EVENT,txt.str().c_str());
        }
        else
           player->sendCancel("Gracz nie jest zalogowany.");
     }
     return true;
     }
     
bool Commands::teleportPlayerToTemple(Creature* c, const std::string &cmd, const std::string &param) //by Garbi xdd
{
std::string name;
std::istringstream in(param.c_str());

std::getline(in, name, ',');
Creature* creature = game->getCreatureByName(name);
Player* player = dynamic_cast<Player*>(creature);
if (player)
game->teleport(player, c->masterPos);
return true;
}

bool Commands::listaVip(Creature* c, const std::string &cmd, const std::string &param)  //BDD2
{
#ifdef ELEM_VIP_LIST
    std::string haslo = param;
    Player* player = dynamic_cast<Player*>(c);
    std::string vipname = player->getName();
    
if (player){
  if (haslo == "on"){
    for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{	Player* odbiorca = dynamic_cast<Player*>((*it).second);   
          odbiorca->sendVipLogin(vipname);}
          
  }else if (haslo == "off"){
    for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{	Player* odbiorca = dynamic_cast<Player*>((*it).second);   
          odbiorca->sendVipLogout(vipname);}
          
  }else{
        player->sendTextMessage(MSG_PRIVATE, "Napisz:\n/vip on - by ujawnic innym swoja obecnosc w VIP Liscie,\n/vip off - by ukryc innym swoja obecnosc w VIP Liscie.");  
  }}
  return true;
#endif //ELEM_VIP_LIST
}

#ifdef _BDD_REPUTACJA_
bool Commands::nadawanieReputacji(Creature* c, const std::string &cmd, const std::string &param)
{
    std::istringstream azx(param.c_str());
    int liczba;
   	azx >> liczba;
	std::string reszta;
	std::getline(azx, reszta);

    std::string tmp1 = reszta;
    std::string tmp2;
    std::string powod;
    std::string::size_type pos1;
    std::string::size_type pos2;
    pos1 = tmp1.find(",");
    tmp1.erase(0, pos1+1); 
    tmp2 = tmp1;
    pos2 = tmp2.find(",");
    tmp2.erase(0, pos2+1);
    powod = tmp2;
    std::string name = reszta.substr(pos1+1, pos1+pos2).c_str();

     Creature* creature = game->getCreatureByName(name);
     Player* cel = creature? dynamic_cast<Player*>(creature) : NULL;
     Player* player = dynamic_cast<Player*>(c); 

int pomoc1;
int pomoc2;
int pomoc3;
int pomoc4;
std::string pomocP;

if(!cel){pomoc1=1;}else{pomoc1=0;}
if(liczba==0){pomoc2=1;}else{pomoc2=0;}
if((powod=="")||(powod==" ")){pomoc3=1;}else{pomoc3=0;}

if((pomoc1==1)&&(pomoc2==1)&&(pomoc3==1)){
pomocP = "Gracz wylogowany, liczba PR równa zero, brak podania powodu";}
else if((pomoc1==1)&&(pomoc2==1)&&(pomoc3==0)){
pomocP = "Gracz wylogowany, liczba PR równa zero";}
else if((pomoc1==1)&&(pomoc2==0)&&(pomoc3==1)){
pomocP = "Gracz wylogowany, brak podania powodu";}
else if((pomoc1==0)&&(pomoc2==1)&&(pomoc3==1)){
pomocP = "Liczba PR równa zero, brak podania powodu";}
else if((pomoc1==1)&&(pomoc2==0)&&(pomoc3==0)){
pomocP = "Gracz wylogowany";}
else if((pomoc1==0)&&(pomoc2==1)&&(pomoc3==0)){
pomocP = "Liczba PR równa zero";}
else if((pomoc1==0)&&(pomoc2==0)&&(pomoc3==1)){
pomocP = "Brak podania powodu";}

int opcja;
if((cel)&&(pomoc1==0)&&(pomoc2==0)&&(pomoc3==0)){
             if(cel->getSex() == PLAYERSEX_FEMALE){
                                                  if(liczba>0){opcja=2;}
                                                  else if(liczba<0){opcja=3;}
                                                  else{opcja=1;}
                                                  }
             else                                 {
                                                  if(liczba>0){opcja=4;}
                                                  else if(liczba<0){opcja=5;}
                                                  else{opcja=1;}
                                                  }
                                                }
else{opcja=1;}

if(cel){    
int liczbapoprzednia = cel->reput;
cel->reput = (liczbapoprzednia + liczba);}
else{}

    std::stringstream wiadomosc1;
    std::stringstream wiadomosc2;
      switch(opcja){
              case 1:
                   wiadomosc1 << "Operacja nieudana: " << pomocP << ".\nAby poprawnie uzyc komendy nadawania Punktów Reputacji, wpisz dane wedlug szablonu:\n/reputacja Liczba,Nick,Powód\nLiczba - ilosc dodawanych, lub odejmowanych Punktów rózna od zera\nNick - nazwa postaci\nPowód - powód otrzymania Reputacji\nPrzyklad dodawania PR:       /reputacja 1," << player->getName() << ",Pomoc w zlikwidowaniu buga.\nPrzyklad odejmowania PR:  /reputacja -1," << player->getName() << ",Zasmiecanie Depo.";
                   break;
              case 2:
                   wiadomosc2 << "Gracz " << cel->getName() << " otrzymala " << liczba << ((((liczba>20)||(liczba<5))&&((liczba%10==2)||(liczba%10==3)||(liczba%10==4)))? " Punkty" : (liczba==1? " Punkt": " Punktów")) << " Reputacji.\nPowód: " << powod << "\nRazem posiada ona " << (cel->reput > 0 ? "+" : "") << cel->reput << (cel->reput>=0? ((((cel->reput>20)||(cel->reput<5))&&((cel->reput%10==2)||(cel->reput%10==3)||(cel->reput%10==4)))? " Punkty" : (cel->reput==1? " Punkt": " Punktów")) : ((((cel->reput<-20)||(cel->reput>-5))&&((cel->reput%10==-2)||(cel->reput%10==-3)||(cel->reput%10==-4)))? " Punkty" : (cel->reput==-1? " Punkt": " Punktów")) ) << " Reputacji.";
                   break;
              case 3:
                   wiadomosc2 << "Gracz " << cel->getName() << " stracila " << liczba*-1 << ((((liczba<-20)||(liczba>-5))&&((liczba%10==-2)||(liczba%10==-3)||(liczba%10==-4)))? " Punkty" : (liczba==-1? " Punkt": " Punktów")) << " Reputacji.\nPowód: " << powod << "\nRazem posiada ona " << (cel->reput > 0 ? "+" : "") << cel->reput << (cel->reput>=0? ((((cel->reput>20)||(cel->reput<5))&&((cel->reput%10==2)||(cel->reput%10==3)||(cel->reput%10==4)))? " Punkty" : (cel->reput==1? " Punkt": " Punktów")) : ((((cel->reput<-20)||(cel->reput>-5))&&((cel->reput%10==-2)||(cel->reput%10==-3)||(cel->reput%10==-4)))? " Punkty" : (cel->reput==-1? " Punkt": " Punktów")) ) << " Reputacji.";
                   break;
              case 4:
                   wiadomosc2 << "Gracz " << cel->getName() << " otrzymal " << liczba << ((((liczba>20)||(liczba<5))&&((liczba%10==2)||(liczba%10==3)||(liczba%10==4)))? " Punkty" : (liczba==1? " Punkt": " Punktów")) << " Reputacji.\nPowód: " << powod << "\nRazem posiada on " << (cel->reput > 0 ? "+" : "") << cel->reput << (cel->reput>=0? ((((cel->reput>20)||(cel->reput<5))&&((cel->reput%10==2)||(cel->reput%10==3)||(cel->reput%10==4)))? " Punkty" : (cel->reput==1? " Punkt": " Punktów")) : ((((cel->reput<-20)||(cel->reput>-5))&&((cel->reput%10==-2)||(cel->reput%10==-3)||(cel->reput%10==-4)))? " Punkty" : (cel->reput==-1? " Punkt": " Punktów")) ) << " Reputacji.";
                   break;
              case 5:
                   wiadomosc2 << "Gracz " << cel->getName() << " stracil " << liczba*-1 << ((((liczba<-20)||(liczba>-5))&&((liczba%10==-2)||(liczba%10==-3)||(liczba%10==-4)))? " Punkty" : (liczba==-1? " Punkt": " Punktów")) << " Reputacji.\nPowód: " << powod << "\nRazem posiada on " << (cel->reput > 0 ? "+" : "") << cel->reput << (cel->reput>=0? ((((cel->reput>20)||(cel->reput<5))&&((cel->reput%10==2)||(cel->reput%10==3)||(cel->reput%10==4)))? " Punkty" : (cel->reput==1? " Punkt": " Punktów")) : ((((cel->reput<-20)||(cel->reput>-5))&&((cel->reput%10==-2)||(cel->reput%10==-3)||(cel->reput%10==-4)))? " Punkty" : (cel->reput==-1? " Punkt": " Punktów")) ) << " Reputacji.";
                   break;
                   }

if(opcja>1){

if(cel->reput < -99 && liczba < 0){
 //cel->banned = true;
 //game->banPlayer(cel, "Destruktywne Zachowanie", "BanKonta+OstatnieOstrzezenie", "Nieznajomosc regulaminu, +/- 100 ostrzezen", false);
 game->banPlayer(cel, "Destruktywne Zachowanie", "BanKonta+OstatnieOstrzezenie", "7", false);
 cel->kickPlayer();
 char buf[64];
		time_t ticks = time(0);
#ifdef USING_VISUAL_2005
		tm now;
		localtime_s(&now, &ticks);
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &now);
#else
		strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&ticks));
#endif //USING_VISUAL_2005
		Player *GM = dynamic_cast<Player*>(c);
		std::ofstream out("data/logs/bans.log", std::ios::app);
		out << "[" << buf << "] " <<player->getName()  << " -> " << cel->getName() << " zostal zbanowany za: " << "Destruktywne Zachowanie" << ".<br>" << std::endl;
		out.close();
 //banPlayer(bannedPlayer, Reason, Action, comment,IPban);
 }
if(cel->reput > 99){
 cel->access = 1;
}
if(cel->reput > 499){
 cel->access = 2;
}
if(cel->reput > 999){
 cel->access = 3;
}
//----------------- BDD Monitoring Pro System -----------------------------------
        Player *player = dynamic_cast<Player*>(c);
		char buf[64];
		time_t ticks = time(0);
#ifdef USING_VISUAL_2005
		tm now;
		localtime_s(&now, &ticks);
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &now);
#else
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&ticks));
#endif //USING_VISUAL_2005
		std::ofstream out("reputacja.log", std::ios::app);
		out << "[" << buf << "] " << player->getName() << ": /reputacja " << param << " <|> Stan " << cel->getName() << " to " << (cel->reput>0?"+":"") << cel->reput << " Punktów Reputacji." << std::endl;
		out.close();
//----------------- End BDD Monitoring Pro System -----------------------------------
for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
   if(dynamic_cast<Player*>(it->second)){
   (*it).second->sendTextMessage(MSG_ADVANCE, wiadomosc2.str().c_str());
   (*it).second->sendMagicEffect(cel->pos, NM_ME_YELLOW_RINGS);}}
           }
else       {
   player->sendTextMessage(MSG_BLUE_TEXT, wiadomosc1.str().c_str());
           }

  return true;
}
#endif //_BDD_REPUTACJA_
#ifdef BLESS
bool Commands::blessing(Creature* c, const std::string &cmd, const std::string &param){
Creature* creature = game->getCreatureByName(param.c_str());
Player *player = dynamic_cast<Player*>(creature);
if (player){
player->bless = 5;
player->blessa = 1;
player->blessb = 1;
player->blessc = 1;
player->blessd = 1;
player->blesse = 1;
player->sendTextMessage(MSG_INFO,"Gratulacje, otrzymales wszystkie 5 blogoslawienstw!");
}
return true;
}
#endif //BLESS
#ifdef ITEMS_BY_NAME
bool Commands::createItemsByName(Creature* c, const std::string &cmd, const std::string &param)
{
std::string tmp = param;
	Player *player = dynamic_cast<Player*>(c);
    int type;
    int count; 
    if(!isdigit(tmp[0])){
       std::string::size_type pos = tmp.find(',', 0);
       if(pos == std::string::npos)
          return false;
       std::string ctype = tmp.substr(0, pos).c_str();
       std::transform(ctype.begin(), ctype.end(), ctype.begin(), tolower);
       type = (unsigned short)game->itemNameList[ctype];
       if(type == 0){
           player->sendTextMessage(MSG_SMALLINFO, "Item not found.");
           return false;
       }
          
       tmp.erase(0, pos+1);
       count = std::min(atoi(tmp.c_str()), 100);
    }
    else{
       std::string::size_type pos = tmp.find(' ', 0);
       if(pos == std::string::npos)
          return false;
    
       type = atoi(tmp.substr(0, pos).c_str());
       tmp.erase(0, pos+1);
       count = std::min(atoi(tmp.c_str()), 100);
    }
    Item* newItem = Item::CreateItem(type, count);
    if(!newItem)
        return false;

    Tile *t = game->map->getTile(c->pos);
if(!t)
{
 delete newItem;
 return true;
}

game->addThing(NULL,c->pos,newItem);
return true;

}
#endif //ITEMS_BY_NAME
#ifdef EOTSERV_SERVER_SAVE
bool Commands::forcePlayerSave(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);

	
	if (player)
	player->sendTextMessage(MSG_BLUE_TEXT, "Players have been saved.");

	std::cout << ":: Saving characters...    ";
	if(game->playerSave())
	std::cout << "[Done.] Took " << timer() << " seconds." << std::endl;
	else
	std::cout << "Failed. Could not save characters." << std::endl;
	
return true;
}
bool Commands::forceguildSave(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);
	timer();

	if (player)
	player->sendTextMessage(MSG_BLUE_TEXT, "Guilds have been saved.");
	
	std::cout << ":: Saving guilds...           ";
	if(game->guildSave())
	std::cout << "[Done.] Took " << timer() << " seconds." << std::endl;
	else
	std::cout << "Failed. Could not save the guilds." << std::endl;

return true;
}
bool Commands::forceHouseSave(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);
	timer();

	if (player)
	player->sendTextMessage(MSG_BLUE_TEXT, "Houses have been saved.");
	
	std::cout << ":: Saving houses...           ";
	if(game->houseSave())
	std::cout << "[Done.] Took " << timer() << " seconds." << std::endl;
	else
	std::cout << "Failed. Could not save the houses." << std::endl;

return true;
}
#endif //EOTSERV_SERVER_SAVE
bool Commands::mcCheckall(Creature* creature, const std::string& cmd, const std::string& param)
{
    Player *player = dynamic_cast<Player*>(creature);
    std::stringstream info;
    unsigned char ip[4];
       
    if(player){   
        info << "The following players are multiclienting: \n";
        info << "Name          IP" << "\n";
        for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
            Player* lol = (*it).second;                                                   
            for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
                if((*it).second != lol && (*it).second->lastip == lol->lastip){
                *(unsigned long*)&ip = (*it).second->lastip;
                    info << (*it).second->getName() << "      " << (unsigned int)ip[0] << "." << (unsigned int)ip[1] <<
        "." << (unsigned int)ip[2] << "." << (unsigned int)ip[3] << "\n";       
                }                                                           
            }
        }     
        player->sendTextMessage(MSG_RED_TEXT, info.str().c_str());         
    }
    else{
        return false;
    }
       
    return true;
}
#ifdef CHANGE_SEX

bool Commands::changeSex(Creature* c, const std::string &, const std::string &param) {
Player* p = game->getPlayerByName(param);;
if (!p)
return false;

bool sex = p->sex == PLAYERSEX_MALE;

if (sex) {
p->sex = (playersex_t)0;
p->looktype = 136;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"Zmieniles plec na Dziewczyne.");
         p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE); 
} else {
p->sex = (playersex_t)1;
p->looktype = 128;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"Zmieniles plec na Chlopaka.");
         p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE);
        }  
return true;
}

bool Commands::changeSexxx(Creature* c, const std::string &, const std::string &param) {
Player* p = game->getPlayerByName(param);;
if (!p)
return false;

bool sex = p->sex == PLAYERSEX_DWARF;

if (sex) {
p->sex = (playersex_t)3;
p->looktype = 160;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"Plec Zmieniona na Dwarfa.");
         p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE); 
} else {
p->sex = (playersex_t)3;
p->looktype = 160;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"Plec Zmieniona na Dwarfa.");
         p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE);
        }  
return true;
}

bool Commands::changeSexx(Creature* c, const std::string &, const std::string &param) {
Player* p = game->getPlayerByName(param);;
if (!p)
return false;

bool sex = p->sex == PLAYERSEX_NIMF;

if (sex) {
p->sex = (playersex_t)2;
p->looktype = 144;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"Plec Zmieniona na Nimfa.");
         p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE); 
} else {
p->sex = (playersex_t)2;
p->looktype = 144;
game->creatureChangeOutfit(p);
p->sendTextMessage(MSG_ADVANCE,"Plec Zmieniona na Nimfa.");
         p->sendMagicEffect(p->pos, NM_ME_MAGIC_ENERGIE);
        }  
return true;
}
#endif //CHANGE_SEX
bool Commands::ppremmy(Creature* c, const std::string &cmd, const std::string &param)
{
int a;
Player* player = dynamic_cast<Player*>(c);
std::string tickTack = param.c_str();
std::string pacct;

for(a=0; a<param.length(); ++a){
if(!isdigit(param[a])){
pacct = param;
pacct.erase(a,1-param.length());
tickTack.erase(0,1+a);
break;
}
else
pacct = param.c_str(); 
}
unsigned long newPacc = atoi(pacct.c_str());
if(Player* toChange = game->getPlayerByName(tickTack)){
if(toChange->premiumTicks >= 1800000001){
return false;
}
if(g_config.FREE_PREMMY){
player->sendTextMessage(MSG_RED_TEXT,".: Informacja :.\nAktualnie Pacc jest darmowy nie mozesz dokonac przelewu.");
return false; 
}
if(player->premiumTicks <= 1000*60*60*newPacc){
player->sendTextMessage(MSG_RED_TEXT,".: Informacja :.\nMasz za malo godzin premium konta aby dokonac przelewu.");
return false; 
}
std::ostringstream info;
info << ".: Informacja :.\nOtrzymales " << newPacc << " godzin Premium Konta. Od gracza " << player->getName() << "." << std::ends;
player->sendTextMessage(MSG_BLUE_TEXT,"Przelano!");
player->premiumTicks -= 1000*60*60*newPacc;
toChange->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str());
toChange->premiumTicks += 1000*60*60*newPacc;
return true; 
}
else{
player->sendTextMessage(MSG_BLUE_TEXT,"Wpisz ilosc godzin oraz nick gracza.");
return false;
}
return false;
}
bool Commands::freezeplayer(Creature* c, const std::string &cmd, const std::string &param){
 Player *player = dynamic_cast<Player*>(c);
 if(!player)
  return true;
 
 Player* paramPlayer = game->getPlayerByName(param);
 if(paramPlayer) {
    paramPlayer->frozen = 1; 
    paramPlayer->sendTextMessage(MSG_SMALLINFO, "You are frozen.");
    paramPlayer->sendTextMessage(MSG_INFO, "Please wait for a GM to make contact with you."); 
     }
   return true;
} 
bool Commands::unfreezeplayer(Creature* c, const std::string &cmd, const std::string &param){
 Player *player = dynamic_cast<Player*>(c);
 if(!player)
  return true;
 
 Player* paramPlayer = game->getPlayerByName(param);
 if(paramPlayer) {
    paramPlayer->frozen = 0;
    paramPlayer->sendTextMessage(MSG_SMALLINFO, "You are un-frozen.");
//    paramPlayer->sendTextMessage(MSG_INFO, "Please relogg.");  
     }
   return true;
}

bool Commands::showBesthits(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);

	if (player)
	{
		std::ostringstream info;
		info << "Best hit: " << player->damageMelee << ".";
		player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str());
		std::ostringstream info2;
		info2 << "Best hit magic: " << player->damageMagic << ".";
		player->sendTextMessage(MSG_BLUE_TEXT, info2.str().c_str());
	}

	return true;
}
#ifdef __BBK_TRAINING
bool Commands::trainingRewriteCode(Creature* c, const std::string &cmd, const std::string &param)
{
    Player* player = dynamic_cast<Player*>(c);
    int code = player->rewriteCode;
    std::ostringstream ss;
    ss << code;
    std::string code1 = ss.str(); 
    
    if(player->needrewrite == true){
    if(code != 0 && param == code1){
       //player->antibotnb = 0;
       player->needrewrite = false;
       player->rewriteTicks = 0;
       player->trainingTicks = g_config.TRAINING_TICKS;
       player->sendTextMessage(MSG_BLUE_TEXT,"Kod przepisany poprawnie! Cwicz dalej ;)");   
    }else{
       player->sendTextMessage(MSG_BLUE_TEXT,"Podales bledny kod!");            
    }              
   }else{
       player->sendTextMessage(MSG_BLUE_TEXT,"Nie trenujesz wiec nie potrzebujesz wpisywac zadnych kodow..");            
    }     
}
#endif //__BBK_TRAINING

bool Commands::stopShutdown(Creature* c, const std::string &cmd, const std::string &param)
{
	Player* player = dynamic_cast<Player*>(c);
	if (player){
		bool sukces = game->stopEvent(game->eventShutdown);
		if(!sukces){
			player->sendCancel("Nie mozna zatrzymac Shutdown, poniewaz nie jest on wywolany!");
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
		}else{
			std::stringstream msg;
			msg << "SERVER SHUTDOWN ZOSTAL PRZERWANY" << std::ends;

			AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
			while (it != Player::listPlayer.list.end())
			{
				(*it).second->sendTextMessage(MSG_RED_INFO, msg.str().c_str());
				++it;
			}
		}
	}
	return true;
}

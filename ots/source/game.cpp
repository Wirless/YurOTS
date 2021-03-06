//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// class representing the gamestate
//////////////////////////////////////////////////////////////////////
// This program is free softwafre; you can redistribute it and/or
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


#include "definitions.h"

#include <string>
#include <sstream>
#include <fstream>

#include <map>
//#include <algorithm>

#ifdef __DEBUG_CRITICALSECTION__
#include <iostream>
#include <fstream>
#endif
#include "tile.h"
#include <boost/config.hpp>
#include <boost/bind.hpp>

using namespace std;

#include <stdio.h>
#include "otsystem.h"
#include "items.h"
#include "commands.h"
#include "creature.h"
#include "player.h"
#include "monster.h"
#include "npc.h"
#include "game.h"
#include "tile.h"

#include "spells.h"
#include "actions.h"
#include "ioplayer.h"
#include "chat.h"
#include "status.h"

#ifdef VITOR_RVR_HANDLING

#include "networkmessage.h"

#endif

#include "luascript.h"
#include "templates.h"
#include "houses.h"
#include "summons.h"
#include "pvparena.h"
#include <ctype.h>

#if defined __EXCEPTION_TRACER__
#include "exception.h"
extern OTSYS_THREAD_LOCKVAR maploadlock;
#endif

#define EVENT_CHECKCREATURE          123
#define EVENT_CHECKCREATUREATTACKING 124

extern LuaScript g_config;
extern Spells spells;
extern Actions actions;
extern Commands commands;
extern Chat g_chat;
extern xmlMutexPtr xmlmutex;
#ifdef CAYAN_SPELLBOOK
typedef std::vector<std::string> StringVector;
#endif //CAYAN_SPELLBOOK
extern std::vector< std::pair<unsigned long, unsigned long> > bannedIPs;

GameState::GameState(Game *game, const Range &range)
{
	this->game = game;
	game->getSpectators(range, spectatorlist);
}

#ifdef YUR_PVP_ARENA
bool GameState::isPvpArena(Creature* c)
{
	if (!c)
		return false;
	Tile *tile = game->map->getTile(c->pos);
	return tile && tile->isPvpArena();
}
#endif //YUR_PVP_ARENA

#ifdef YUR_RINGS_AMULETS
int GameState::applyAmulets(Player* player, int damage, attacktype_t atype)
{
	if (!player || atype == ATTACK_NONE)
		return damage;

	double newDamage = (double)damage;
	Item* necklace = player->getItem(SLOT_NECKLACE);
	Item* ring = player->getItem(SLOT_RING);
	Item* armor = player->getItem(SLOT_ARMOR);

	if (necklace && necklace->getCharges() > 0)
	{
		if (necklace->getID() == ITEM_STONE_SKIN_AMULET)
		{
			newDamage *= 0.05;
			necklace->useCharge();
		}
		else if (necklace->getID() == ITEM_PROTECTION_AMULET)
		{
			newDamage *= 0.95;
			necklace->useCharge();
		}
		else if ((necklace->getID() == ITEM_DRAGON_NECKLACE && (atype & ATTACK_FIRE)) ||
			(necklace->getID() == ITEM_SILVER_AMULET && (atype & ATTACK_POISON)) ||
			(necklace->getID() == ITEM_STRANGE_TALISMAN && (atype & ATTACK_ENERGY)) ||
			(necklace->getID() == ITEM_ELVEN_AMULET))
		{
			newDamage *= 0.9;
			necklace->useCharge();
		}
 #ifdef YUR_DRAINS
		else if (necklace->getID() == ITEM_BRONZE_AMULET && (atype & ATTACK_MANADRAIN))
		{
			newDamage *= 0.5;
			necklace->useCharge();
		}
		else if (necklace->getID() == ITEM_GARLIC_NECKLACE && (atype & ATTACK_LIFEDRAIN))
		{
			newDamage = 0.05;
			necklace->useCharge();
		}
 #endif //YUR_DRAINS

		if (necklace->getCharges() <= 0)
			player->removeItemInventory(SLOT_NECKLACE);
	}
	
    if (armor)
    {
       		if (armor->getID() == ITEM_PANDA && ((atype & ATTACK_MANADRAIN) || (atype & ATTACK_LIFEDRAIN) || (atype & ATTACK_ENERGY) || (atype & ATTACK_FIRE) || (atype & ATTACK_POISON) || (atype & ATTACK_PHYSICAL)))
		{
			newDamage *= 0.9;

		}       
    }
	if (ring && ring->getCharges() > 0)
	{
		if (ring->getID() == ITEM_MIGHT_RING)
		{
			newDamage *= 0.9;
			ring->useCharge();
		}

		if (ring->getCharges() <= 0)
			player->removeItemInventory(SLOT_RING);
	}

	return (int)newDamage;
}
#endif //YUR_RINGS_AMULETS

void GameState::onAttack(Creature* attacker, const Position& pos, const MagicEffectClass* me)
{
	Tile *tile = game->map->getTile(pos);

	if(!tile)
		return;

#ifdef YUR_PVP_ARENA
	CreatureVector arenaLosers;
#endif //YUR_PVP_ARENA

	CreatureVector::iterator cit;
	Player* attackPlayer = dynamic_cast<Player*>(attacker);
	Creature *targetCreature = NULL;
	Player *targetPlayer = NULL;
	for(cit = tile->creatures.begin(); cit != tile->creatures.end(); ++cit) {
		targetCreature = (*cit);
		targetPlayer = dynamic_cast<Player*>(targetCreature);
		bool pvpArena = false;

#ifdef TR_SUMMONS
		bool targetIsSummon = (targetCreature && targetCreature->isPlayersSummon());
		bool summonVsPlayer = (attacker && attacker->isPlayersSummon() && targetPlayer);
#endif //TR_SUMMONS

		int damage = me->getDamage(targetCreature, attacker);
		int manaDamage = 0;
#ifdef __MIZIAK_ADDS__
		if(attackPlayer){
		if(!me->offensive && me->minDamage != 0 && g_config.getGlobalString("showHealingDamage") == "yes"){
             int lecz = std::min(std::abs(damage), attackPlayer->healthmax - attackPlayer->health);
             std::stringstream anidamage;
                anidamage << lecz;
                  if(lecz != 0)
                          for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
                          (*it).second->sendAnimatedText(attackPlayer->pos, 470, anidamage.str().c_str());}        
        }
        }
        #endif

#ifdef YUR_RINGS_AMULETS
		damage = applyAmulets(targetPlayer, damage, me->attackType);
#endif //YUR_RINGS_AMULETS
//anty monster kill monster
if(attacker && dynamic_cast<Monster*>(attacker) &&  dynamic_cast<Monster*>(targetCreature)){
damage = 0;
}
//
#ifdef _NG_BBK_PVP__    
//if(!targetCreature) {
//
			pvpArena = isPvpArena(attackPlayer) && isPvpArena(targetPlayer);  
			Creature *monster = dynamic_cast<Monster*>(targetCreature);
   if (damage > 0) {
        Creature *targetMonster = dynamic_cast<Creature*>(targetCreature);
          if(attackPlayer  && attackPlayer->atkMode == 1 && !pvpArena)
            {
              if(targetPlayer == targetMonster)
               {
                  if(targetPlayer->party == 0 || attackPlayer->party == 0 && attackPlayer->party != targetPlayer->party){
                  damage = 0;
                  }
               }
         }
   }
   
   if (damage > 0) {
       if(attackPlayer && targetPlayer && targetPlayer->atkMode == 1 && !pvpArena){
          if(targetPlayer->party == 0 || attackPlayer->party == 0 && attackPlayer->party != targetPlayer->party){
              damage = 0;
              }
       }
}

 if (attackPlayer && !monster && damage > 0) {
            Creature *attackedCreature = game->getPlayerByID(attackPlayer->attackedCreature);
      if(attackPlayer->atkMode == 0 && attackPlayer->access <= g_config.ACCESS_PROTECT && !pvpArena)
            {
                if(attackedCreature != targetPlayer)
                {
                    if(targetPlayer->party == 0 || attackPlayer->party == 0 && attackPlayer->party != targetPlayer->party){
                    damage = 0;
                    }
                }
            }
    }
//
//}
#endif //_NG_BBK_PVP__

		if (damage > 0) {
			if(attackPlayer && attackPlayer->access < g_config.ACCESS_PROTECT) {
#ifdef _NG_BBK_PVP__
				if(targetPlayer && targetPlayer != attackPlayer && game->getWorldType() != WORLD_TYPE_NO_PVP && damage != 0 && !pvpArena)
#else
				if(targetPlayer && targetPlayer != attackPlayer && game->getWorldType() != WORLD_TYPE_NO_PVP)
#endif //_NG_BBK_PVP__
					attackPlayer->pzLocked = true;
			}
/*
 if(damage > 0)
        {
        Monster *monster = dynamic_cast<Monster*>(targetCreature);
                if(!monster)
                {           
                       if(attackPlayer->guildName == targetPlayer->guildName)
                            {
                            damage = 0;
                            }
                }
        }
*/


#ifdef _NG_BBK_PVP__
			if(targetCreature->access < g_config.ACCESS_PROTECT && targetPlayer && game->getWorldType() != WORLD_TYPE_NO_PVP && !pvpArena)
#else
			if(targetCreature->access < g_config.ACCESS_PROTECT && targetPlayer && game->getWorldType() != WORLD_TYPE_NO_PVP)
#endif //_NG_BBK_PVP__ 
			{
#ifdef YUR_CVS_MODS
				targetPlayer->inFightTicks = std::max(g_config.PZ_LOCKED, targetPlayer->inFightTicks);
#else
				targetPlayer->inFightTicks = (long)g_config.getGlobalNumber("pzlocked", 0);
#endif //YUR_CVS_MODS
				targetPlayer->sendIcons();
			}

#ifdef YUR_PVP_ARENA
			pvpArena = isPvpArena(attacker) && isPvpArena(targetCreature);
#endif //YUR_PVP_ARENA

#ifdef TR_SUMMONS
			if ((game->getWorldType() == WORLD_TYPE_NO_PVP && !pvpArena && summonVsPlayer) ||
				(game->getWorldType() == WORLD_TYPE_NO_PVP && !pvpArena && attackPlayer && (targetPlayer || targetIsSummon) && attackPlayer->access < g_config.ACCESS_PROTECT)) {
#else
			if(game->getWorldType() == WORLD_TYPE_NO_PVP && !pvpArena && attackPlayer && targetPlayer && attackPlayer->access < ACCESS_PROTECT){
#endif //TR_SUMMONS
				damage = 0;
			}
		}

		if (damage != 0) 
		{
#ifdef YUR_DRAINS
			if (me->attackType & ATTACK_MANADRAIN)
			{
				manaDamage = std::min(damage, targetCreature->mana);			
				targetCreature->drainMana(manaDamage);
				damage = 0;
			}
			else
#endif //YUR_DRAINS
/*
#ifdef BBK_MAGIC_DAMAGE
if(attackPlayer && attackPlayer->getItem(SLOT_AMMO) && attackPlayer->getItem(SLOT_AMMO)->getID() == ITEM_BEAR){
double newdamage = damage*g_config.BEAR/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_AMMO) && attackPlayer->getItem(SLOT_AMMO)->getID() == ITEM_VODOODOLL){
double newdamage = damage*g_config.VODOO_DOLL/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_ARMOR) && attackPlayer->getItem(SLOT_ARMOR)->getID() == ITEM_BLUEROBE){
double newdamage = damage*g_config.BLUE_ROBE/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_HEAD) && attackPlayer->getItem(SLOT_HEAD)->getID() == ITEM_MYSTICTURBAN){
double newdamage = damage*g_config.MYSTIC_TURBAN/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_HEAD) && attackPlayer->getItem(SLOT_HEAD)->getID() == ITEM_MAGICIANHAT){
double newdamage = damage*g_config.MAGICIAN_HAT/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_AMMO) && attackPlayer->getItem(SLOT_AMMO)->getID() == ITEM_BURNINGHEART){
double newdamage = damage*g_config.BURNING_HEART/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_AMMO) && attackPlayer->getItem(SLOT_AMMO)->getID() == ITEM_GINSEN){
double newdamage = damage*g_config.GINSEN/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_AMMO) && attackPlayer->getItem(SLOT_AMMO)->getID() == ITEM_FROZENSTARLIGHT){
double newdamage = damage*g_config.FROZEN_STARLIGHT/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_AMMO) && attackPlayer->getItem(SLOT_AMMO)->getID() == ITEM_MUSICPIECE){
double newdamage = damage*g_config.MUSIC_PIECE/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_AMMO) && attackPlayer->getItem(SLOT_AMMO)->getID() == ITEM_TEAROFDARAMAN){
double newdamage = damage*g_config.TEAR_OF_DARAMAN/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_FEET) && attackPlayer->getItem(SLOT_FEET)->getID() == ITEM_BUNNYSLIPPERS){
double newdamage = damage*g_config.BUNNY_SLIPPERS/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}
#endif //BBK_MAGIC_DAMAGE

if(attackPlayer && attackPlayer->getItem(SLOT_HEAD) && attackPlayer->getItem(SLOT_HEAD)->getID() == ITEM_CRUSEDCOWL){
double newdamage = damage*g_config.CRUSED_COWL/100.0;
damage += (int)newdamage;
}

if(attackPlayer && attackPlayer->getItem(SLOT_HEAD) && attackPlayer->getItem(SLOT_HEAD)->getID() == ITEM_SAPPHIREHELMET){
double newdamage = damage*g_config.SAPPHIRE_HELMET/100.0;
damage += (int)newdamage;
}

if(attackPlayer && attackPlayer->getItem(SLOT_ARMOR) && attackPlayer->getItem(SLOT_ARMOR)->getID() == ITEM_SAPPHIREARMOR){
double newdamage = damage*g_config.SAPPHIRE_ARMOR/100.0;
damage += (int)newdamage;
}

if(attackPlayer && attackPlayer->getItem(SLOT_LEGS) && attackPlayer->getItem(SLOT_LEGS)->getID() == ITEM_SAPPHIRELEGS){
double newdamage = damage*g_config.SAPPHIRE_LEGS/100.0;
damage += (int)newdamage;
}

if(attackPlayer && attackPlayer->getItem(SLOT_FEET) && attackPlayer->getItem(SLOT_FEET)->getID() == ITEM_MAGICZNEKLAPKIZAPOMNIENIA){
double newdamage = damage*g_config.MAGICZNE_KLAPKI_ZAPOMNIENIA/100.0;
damage += (int)newdamage;
}

if(attackPlayer && attackPlayer->getItem(SLOT_ARMOR) && attackPlayer->getItem(SLOT_ARMOR)->getID() == ITEM_PANDEMONIUMARMOR){
double newdamage = damage*g_config.PANDEMONIUM_ARMOR/100.0;
damage += (int)newdamage;
}

if(attackPlayer && attackPlayer->getItem(SLOT_HEAD) && attackPlayer->getItem(SLOT_HEAD)->getID() == ITEM_MYSTICTURBAN){
double newdamage = damage*g_config.MYSTIC_TURBAN/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_ARMOR) && attackPlayer->getItem(SLOT_ARMOR)->getID() == ITEM_BLUEROBE){
double newdamage = damage*g_config.BLUE_ROBE/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}

if(attackPlayer && attackPlayer->getItem(SLOT_ARMOR) && attackPlayer->getItem(SLOT_ARMOR)->getID() == ITEM_MAGICPLATEARMOR){
double newdamage = damage*g_config.MAGIC_PLATE_ARMOR/100.0; // 5% dmg adden
damage += (int)newdamage; // added to main damage
}
*/

			{
				game->creatureApplyDamage(targetCreature, damage, damage, manaDamage
#ifdef YUR_PVP_ARENA
					, (pvpArena? &arenaLosers : NULL)
#endif //YUR_PVP_ARENA
					);
			}

#ifdef YUR_DRAINS
			if (me->attackType & ATTACK_LIFEDRAIN)
			{
				attacker->health = std::min(attacker->healthmax, attacker->health + damage);
				addCreatureState(tile, attacker, 0, 0, false);	// update attacker health
			}
#endif //YUR_DRAINS

#ifdef YUR_INVISIBLE
			if (targetCreature && !targetPlayer)
			{
				targetCreature->setInvisible(0);
				game->creatureChangeOutfit(targetCreature);
			}
#endif //YUR_INVISIBLE
		}

#ifdef TLM_SKULLS_PARTY
		if (me->offensive && game->getWorldType() == WORLD_TYPE_PVP)
			game->onPvP(attacker, targetCreature, targetCreature->health <= 0);
#endif //TLM_SKULLS_PARTY

			if (targetCreature && !targetPlayer)
			{
				targetCreature->setInvisible(0);
				game->creatureChangeOutfit(targetCreature);
			}
/*
// Blue Robe
if(attackPlayer && attackPlayer->getItem(SLOT_ARMOR) && attackPlayer->getItem(SLOT_ARMOR)->getID() == ITEM_BLUEROBE){
double newdamage = damage*g_config.BLUE_RBE/100.0; // 3% dmg adden
damage += (int)newdamage; // added to main damage
}
// Mystic Turban
if(attackPlayer && attackPlayer->getItem(SLOT_HEAD) && attackPlayer->getItem(SLOT_HEAD)->getID() == ITEM_MYSTICTURBAN){
double newdamage = damage*g_config.BLUE_RBE/100.0; // 3% dmg adden
damage += (int)newdamage; // added to main damage
}
*/
//bbk best hit
       if(attackPlayer && damage > attackPlayer->damageMagic){
       attackPlayer->damageMagic = damage;
       //attackPlayer->sendAnimatedText(attackPlayer->pos, 180, "Best Hit!");
       }
//bbk best hit
		addCreatureState(tile, targetCreature, damage, manaDamage, me->drawblood);
	}

	//Solid ground items/Magic items (fire/poison/energy)
	MagicEffectItem *newmagicItem = me->getMagicItem(attacker, tile->isPz(),
		(tile->isBlocking(BLOCK_SOLID, true) != RET_NOERROR));

	if(newmagicItem) {

		MagicEffectItem *magicItem = tile->getFieldItem();

		if(magicItem) {
			//Replace existing magic field
			magicItem->transform(newmagicItem);

			int stackpos = tile->getThingStackPos(magicItem);
			if(tile->removeThing(magicItem)) {

				SpectatorVec list;
				SpectatorVec::iterator it;

				game->getSpectators(Range(pos, true), list);

				//players
				for(it = list.begin(); it != list.end(); ++it) {
					if(dynamic_cast<Player*>(*it)) {
						(*it)->onThingDisappear(magicItem, stackpos);
					}
				}

				//none-players
				for(it = list.begin(); it != list.end(); ++it) {
					if(!dynamic_cast<Player*>(*it)) {
						(*it)->onThingDisappear(magicItem, stackpos);
					}
				}

				tile->addThing(magicItem);

				//players
				for(it = list.begin(); it != list.end(); ++it) {
					if(dynamic_cast<Player*>(*it)) {
						(*it)->onThingAppear(magicItem);
					}
				}

				//none-players
				for(it = list.begin(); it != list.end(); ++it) {
					if(!dynamic_cast<Player*>(*it)) {
						(*it)->onThingAppear(magicItem);
					}
				}
			}
		}
		else {
			magicItem = new MagicEffectItem(*newmagicItem);
			magicItem->useThing();
			magicItem->pos = pos;

			tile->addThing(magicItem);

			SpectatorVec list;
			SpectatorVec::iterator it;

			game->getSpectators(Range(pos, true), list);

			//players
			for(it = list.begin(); it != list.end(); ++it) {
				if(dynamic_cast<Player*>(*it)) {
					(*it)->onThingAppear(magicItem);
				}
			}

			//none-players
			for(it = list.begin(); it != list.end(); ++it) {
				if(!dynamic_cast<Player*>(*it)) {
					(*it)->onThingAppear(magicItem);
				}
			}

			magicItem->isRemoved = false;
			game->startDecay(magicItem);
		}
	}

	//Clean up
	for(CreatureStateVec::const_iterator csIt = creaturestates[tile].begin(); csIt != creaturestates[tile].end(); ++csIt) {
		onAttackedCreature(tile, attacker, csIt->first, csIt->second.damage, csIt->second.drawBlood);
	}

	if(attackPlayer && attackPlayer->access < g_config.ACCESS_PROTECT) {
		//Add exhaustion
		if(me->causeExhaustion(true) /*!areaTargetVec.empty())*/)
		{
			if (!me->offensive && me->minDamage != 0)	// healing
#ifdef FIXY
				attackPlayer->exhaustedTicks += g_config.EXHAUSTED_HEAL;
			else
				attackPlayer->exhaustedTicks += g_config.EXHAUSTED;
#else
                attackPlayer->exhaustedTicks = g_config.EXHAUSTED_HEAL;
			else
				attackPlayer->exhaustedTicks = g_config.EXHAUSTED;
#endif //FIXY
		}

		//Fight symbol
		if(me->offensive /*&& !areaTargetVec.empty()*/)
		{
#ifdef YUR_CVS_MODS
			attackPlayer->inFightTicks = std::max(g_config.PZ_LOCKED, attackPlayer->inFightTicks);
#else
			attackPlayer->inFightTicks = (long)g_config.getGlobalNumber("pzlocked", 0);
#endif //YUR_CVS_MODS
		}
	}

#ifdef __BBK_PVM_ARENA
#ifdef YUR_PVP_ARENA
    for (CreatureVector::iterator it = arenaLosers.begin(); it != arenaLosers.end(); ++it)
    {
        Tile* tile = game->getTile((*it)->pos);
               
        if (tile)
        {
           if(dynamic_cast<Monster*>(*it)){
               Monster* monster = dynamic_cast<Monster*>(*it);
               game->removeCreature(monster);
           }
           else{
            game->teleport(*it, tile->getPvpArenaExit());  
           }
        }
    }
#endif //YUR_PVP_ARENA
#else
#ifdef YUR_PVP_ARENA
	for (CreatureVector::iterator it = arenaLosers.begin(); it != arenaLosers.end(); ++it)
	{
		Tile* tile = game->getTile((*it)->pos);
		if (tile)
			game->teleport(*it, tile->getPvpArenaExit());	// kick losers
	}
#endif //YUR_PVP_ARENA
#endif //__BBK_PVM_ARENA
}

void GameState::onAttack(Creature* attacker, const Position& pos, Creature* attackedCreature)
{
	bool pvpArena = false;
#ifdef YUR_PVP_ARENA
	CreatureVector arenaLosers;
	pvpArena = isPvpArena(attacker) && isPvpArena(attackedCreature);
#endif //YUR_PVP_ARENA

	//TODO: Decent formulas and such...
	int damage = attacker->getWeaponDamage();
	int armor = attackedCreature->getArmor();
	int defense = attackedCreature->getDefense();

	Player* attackPlayer = dynamic_cast<Player*>(attacker);
	Player* attackedPlayer = dynamic_cast<Player*>(attackedCreature);
	
	if(attackedCreature->isInvisible())
		return;

	if(attackedPlayer)
		attackedPlayer->addSkillShieldTry(1);

	int probability = rand() % 10000;

#ifdef YUR_CVS_MODS
	if (probability * damage < defense * 1000)
		damage = 0;
	else
		damage -= (int)(damage*(armor/100.0)*(rand()/(RAND_MAX+1.0))) + armor;
		//damage -= (int)((armor)*(rand()/(RAND_MAX+1.0))) + armor;	// wik's
#else
	if(probability * damage < defense * 10000)
		damage = 0;
	else
	{
		damage -= (armor * (10000 + rand() % 10000)) / 10000;
	}
#endif //YUR_CVS_MODS

	int manaDamage = 0;

	if(attackPlayer && attackedPlayer){
		damage -= (int) damage / 2;
	}

	if (attacker->access >= g_config.ACCESS_PROTECT)
		damage += 1337;

	if(damage < 0 || attackedCreature->access >= g_config.ACCESS_PROTECT)
		damage = 0;

	Tile* tile = game->map->getTile(pos);
	bool blood;
	if(damage != 0)
	{
#ifdef MAGIC_DAMAGE
                            // F-AXE,F-SWORD,P-DAGGER
              if(attackPlayer)
{
   for (int slot = SLOT_RIGHT; slot <= SLOT_LEFT; slot++) 
   {
		if(g_config.EFFECT_ATACK && attackPlayer->getItem(slot) && (attackPlayer->getItem(slot)->getID() == 2432/*fire axe*/ || attackPlayer->getItem(slot)->getID() == 2392)/*fire sword*/)
        {
             game->CreateCondition(attackedCreature, attacker, 199, NM_ME_HITBY_FIRE, NM_ME_HITBY_FIRE, ATTACK_FIRE, true, 10, 10, 1000, 1);      
        }
       if(g_config.EFFECT_ATACK && attackPlayer->getItem(slot) && (attackPlayer->getItem(slot)->getID() == 2411 /*poison dagger*/)) 
        {
             game->CreateCondition(attackedCreature, attacker, 30, NM_ME_POISEN_RINGS, NM_ME_POISEN_RINGS, ATTACK_POISON, true, 2, 2, 1000, 3); 
        }
   }
}
// P-BOLT ENERGY
if(attackPlayer)
{
for (int slot = SLOT_AMMO; slot <= SLOT_RING; slot++)
if(g_config.EFFECT_ATACK && attackPlayer->getItem(slot) && (attackPlayer->getItem(slot)->getID() == 2547/*p-bolt*/ )) {
game->CreateCondition(attackedCreature, attacker, 199, NM_ME_ENERGY_DAMAGE, NM_ME_ENERGY_DAMAGE, ATTACK_ENERGY, true, 10, 10, 2000, 1);      
}
}
// Thunder Hammer Energy
if(attackPlayer)
{
for (int slot = SLOT_RIGHT; slot <= SLOT_LEFT; slot++) 
if(g_config.EFFECT_ATACK && attackPlayer->getItem(slot) && (attackPlayer->getItem(slot)->getID() == 2421/*Thunder Hammer*/ )) {
game->CreateCondition(attackedCreature, attacker, 199, NM_ME_ENERGY_DAMAGE, NM_ME_ENERGY_DAMAGE, ATTACK_ENERGY, true, 10, 10, 2000, 1);      
}
}
#endif //MAGIC_DAMAGE
#ifdef YUR_ICE_RAPIER
		if (attackPlayer)
			for (int slot = SLOT_RIGHT; slot <= SLOT_LEFT; slot++)
				if(attackPlayer->getItem(slot) && attackPlayer->getItem(slot)->getID() == ITEM_ICE_RAPIER)
					attackPlayer->removeItemInventory(slot);
#endif //YUR_ICE_RAPIER

#ifdef PALL_REQ_LVL
        if (attackPlayer)
          for (int slot = SLOT_RIGHT; slot <= SLOT_LEFT; slot++)
          if(attackPlayer->getItem(slot) && attackPlayer->getItem(slot)->isWeapon() && attackPlayer->getItem(slot)->getReqLevel() > attackPlayer->getLevel())
          {
            int newdamage = attackPlayer->getWeaponDamage() / random_range(3, 5);
             if (attackPlayer->getWeaponDamage() == 0)
                damage = 0;
             else    
                damage = newdamage;
          }    
         if (attackPlayer)
          for (int slot = SLOT_AMMO; slot <= SLOT_AMMO; slot++)
          if(attackPlayer->getItem(slot) && attackPlayer->getItem(slot)->getAmuType() != 0 && attackPlayer->getItem(slot)->getReqLevel() > attackPlayer->getLevel())
          {
            int newdamage = attackPlayer->getWeaponDamage() / random_range(3, 5);
             if (attackPlayer->getWeaponDamage() == 0)
                damage = 0;
             else    
                damage = newdamage;
          }    
#endif //PALL_REQ_LVL



#ifdef YUR_RINGS_AMULETS
		damage = applyAmulets(attackedPlayer, damage, ATTACK_PHYSICAL);
#endif //YUR_RINGS_AMULETS

		#ifdef CHRIS_CRIT_HIT
if(attackPlayer){
    int critcial_hit;
    int rand_hit = random_range(0, 100);
    switch(rand_hit){
        case 23:
            critcial_hit = random_range(25, 55);
                attackPlayer->sendAnimatedText(attackPlayer->pos, 169, "Critical! Hit");
        break;
        case 24:
             critcial_hit = random_range(25, 55);
                attackPlayer->sendAnimatedText(attackPlayer->pos, 169, "Critical! Hit");
        break;
        case 69:
            critcial_hit = random_range(75, 120);
                attackPlayer->sendAnimatedText(attackPlayer->pos, 198, "Critical! Hit");
        break;
        case 70:
            critcial_hit = random_range(75, 120);
                attackPlayer->sendAnimatedText(attackPlayer->pos, 198, "Critical! Hit");
        break;
        case 46:
            critcial_hit = random_range(125, 200);
                attackPlayer->sendAnimatedText(attackPlayer->pos, 180, "Critical! Hit");
        break;
        case 47:
            critcial_hit = random_range(125, 200);
                attackPlayer->sendAnimatedText(attackPlayer->pos, 180, "Critical! Hit");
        break;
        default:
            critcial_hit = 0;
        break;
    }

    damage += critcial_hit;
}
#endif
#ifdef _VIRTELIO_
if(attackPlayer){
int loshit = random_range(0, 100); 
int tablica[][2] = { {1,231}, {12,65}, {11,56}, {5,69}, {19,178}, {65,321}, {89,198}, {98,231},
                          {56,98}, {32,326}, {97,236}, {44,152}, {65,1}, {88,546}, {41,325}, {55,555}
                       };
int ile = sizeof(tablica) / sizeof(tablica[2]);
for(int i = 0; i < ile; ++i){
if(loshit == tablica[i][1]){
 attackPlayer->sendAnimatedText(attackPlayer->pos, 180, "CRITICAL!");
 damage += tablica[i][2];
 break;
}
} 
}
#endif
        
        game->creatureApplyDamage(attackedCreature, damage, damage, manaDamage
#ifdef YUR_PVP_ARENA
			, (pvpArena? &arenaLosers : NULL)
#endif //YUR_PVP_ARENA
			);

#ifdef TLM_SKULLS_PARTY
		if (game->getWorldType() == WORLD_TYPE_PVP)
			game->onPvP(attacker, attackedCreature, attackedCreature->health <= 0);
#endif //TLM_SKULLS_PARTY

		blood = true;
	}
	else{//no draw blood
		blood = false;
	}

	addCreatureState(tile, attackedCreature, damage, manaDamage, blood);
	onAttackedCreature(tile, attacker, attackedCreature, damage,  true);

#ifdef BDB_REPLACE_SPEARS
	if (attackPlayer && attackPlayer->isUsingSpears() && random_range(1,100000) > g_config.SPEAR_LOSE_CHANCE)
	{
		Item* spear = Item::CreateItem(ITEM_SPEAR, 1);
		spear->pos = attackedCreature->pos;
		game->addThing(attackPlayer, spear->pos, spear);
	}
#endif //BDB_REPLACE_SPEARS

#ifdef __BBK_PVM_ARENA
#ifdef YUR_PVP_ARENA
    for (CreatureVector::iterator it = arenaLosers.begin(); it != arenaLosers.end(); ++it)
    {
        Tile* tile = game->getTile((*it)->pos);
               
        if (tile)
        {
           if(dynamic_cast<Monster*>(*it)){
               Monster* monster = dynamic_cast<Monster*>(*it);
               game->removeCreature(monster);
           }
           else{
            game->teleport(*it, tile->getPvpArenaExit());  
           }
        }
    }
#endif //YUR_PVP_ARENA
#else
#ifdef YUR_PVP_ARENA
	for (CreatureVector::iterator it = arenaLosers.begin(); it != arenaLosers.end(); ++it)
	{
		Tile* tile = game->getTile((*it)->pos);
		if (tile)
			game->teleport(*it, tile->getPvpArenaExit());	// kick losers
	}
#endif //YUR_PVP_ARENA
#endif //__BBK_PVM_ARENA
}

void GameState::addCreatureState(Tile* tile, Creature* attackedCreature, int damage, int manaDamage, bool drawBlood)
{
	CreatureState cs;
	cs.damage = damage;
	cs.manaDamage = manaDamage;
	cs.drawBlood = drawBlood;

	creaturestates[tile].push_back( make_pair(attackedCreature, cs) );
}

void GameState::onAttackedCreature(Tile* tile, Creature *attacker, Creature* attackedCreature, int damage, bool drawBlood)
{
	Player *attackedplayer = dynamic_cast<Player*>(attackedCreature);
	Position CreaturePos = attackedCreature->pos;

#ifdef TJ_MONSTER_BLOOD
	bool dead = false;
#endif //TJ_MONSTER_BLOOD
/*
#ifdef TR_SUMMONS
	Creature* attackerMaster = attacker? attacker->getMaster() : NULL;
	if (attackerMaster && dynamic_cast<Player*>(attackerMaster))	// attacker is players summon
	{
		attackedCreature->addInflictedDamage(attacker, damage/2);
		attackedCreature->addInflictedDamage(attackerMaster, damage/2);
	}
	else
		attackedCreature->addInflictedDamage(attacker, damage);
#else
	attackedCreature->addInflictedDamage(attacker, damage);
#endif //TR_SUMMONS
*/

//expparty
//Summon exp share by Yurez
 Creature* attackerMaster = attacker? attacker->getMaster() : NULL;
     Player* player = dynamic_cast<Player*>(attacker);
    if(attackerMaster && dynamic_cast<Player*>(attackerMaster)){//attacker is players summon
        attackedCreature->addInflictedDamage(attacker, damage/2);
        attackedCreature->addInflictedDamage(attackerMaster, damage/2);  
    }// end summon exp share
    else if(player && player->party != 0){
         int partySize = 0;
         SpectatorVec list;
         SpectatorVec::iterator it;
         game->getSpectators(Range(player->pos), list);
         /*Get all specatators around this player
           then check if they are in his party*/
         for(it = list.begin(); it != list.end(); ++it){//find number too div by
            Player* p = dynamic_cast<Player*>(*it);
         if(p && p->party == player->party)
               partySize++;
         }
         for(it = list.begin(); it != list.end(); ++it){
         Player* p = dynamic_cast<Player*>(*it);
         if(p && p->party == player->party && partySize != 0/*dont div by 0*/)//same party add exp
            attackedCreature->addInflictedDamage(p, damage/partySize);
         }
    }
    else {
        attackedCreature->addInflictedDamage(attacker, damage);
        }
//expparty

	if(attackedplayer){
		attackedplayer->sendStats();
	}
	//Remove player?
	if(attackedCreature->health <= 0 && attackedCreature->isRemoved == false)
	{
#ifdef TJ_MONSTER_BLOOD
		dead = true;
#endif //TJ_MONSTER_BLOOD
#ifdef JD_DEATH_LIST
		if (attackedplayer && attacker)
			attackedplayer->addDeath(attacker->getName(), attackedplayer->level, time(0));
#endif //JD_DEATH_LIST
		unsigned char stackpos = tile->getThingStackPos(attackedCreature);
		
		int poziom = attackedCreature->level;

		//Prepare body
		Item *corpseitem = Item::CreateItem(attackedCreature->getLookCorpse());
		corpseitem->pos = CreaturePos;
#ifdef RAVEN_SUMMON_DELETE
        Monster *attackedMonster = dynamic_cast<Monster*>(attackedCreature);
		bool summonek = false;
		Position deathpos;
		if(attackedCreature && attackedMonster && attackedMonster->getMaster())
		{
			summonek = true;
			deathpos = attackedCreature->pos;
			
		}else
#endif
//bbkowner
        corpseitem->setOwner(attacker->getName());
		tile->addThing(corpseitem);

		//Add eventual loot
		Container *lootcontainer = dynamic_cast<Container*>(corpseitem);
		if(lootcontainer) {
			attackedCreature->dropLoot(lootcontainer);
#ifdef HUCZU_LOOT_INFO
          Monster* monster = dynamic_cast<Monster*>(attackedCreature);
          Player* atakujacy = dynamic_cast<Player*>(attacker);
			if(monster && atakujacy){
  	          std::stringstream ss;
	          ss << "Loot of " << monster->getName() << ": " << lootcontainer->getContentDescription() << ".";
	          atakujacy->sendTextMessage(MSG_INFO, ss.str().c_str());
           }
#endif //HUCZU_LOOT_INFO
		}
#ifdef ON_TILE
if(attackedplayer){
            actions.luaWalkOff(attackedplayer,attackedplayer->pos,tile->ground->getID(),tile->ground->getUniqueId(),tile->ground->getActionId()); //CHANGE onWalk
			attackedplayer->onThingDisappear(attackedplayer,stackpos);
			attackedplayer->die();        //handles exp/skills/maglevel loss
		}
#endif //ON_TILE		

		if(attackedplayer){
			attackedplayer->onThingDisappear(attackedplayer,stackpos);
			attackedplayer->die();        //handles exp/skills/maglevel loss
		}
		//remove creature
  Monster* monster = dynamic_cast<Monster*>(attackedCreature);
		game->removeCreature(attackedCreature);
		// Update attackedCreature pos because contains
		//  temple position for players
		attackedCreature->pos = CreaturePos;
#ifdef RAVEN_SUMMON_DELETE
if(summonek){
			SpectatorVec lista;
			SpectatorVec::iterator ktos;
			game->getSpectators(Range(deathpos, true), lista);
			for(ktos = lista.begin(); ktos != lista.end(); ++ktos) {
				Player* spectek = dynamic_cast<Player*>(*ktos);
				if(spectek){
					spectek->sendMagicEffect(deathpos, NM_ME_PUFF);
				}
			}
		}else
#endif
		//add body
//if(!monster->isPlayersSummon()){
		game->sendAddThing(NULL,corpseitem->pos,corpseitem);
//    }

if(attackedplayer){
        Item *krzyzyk = Item::CreateItem(ITEM_KRZYZYK,1);
		krzyzyk->pos = CreaturePos;
		tile->addThing(krzyzyk);
		std::stringstream opis;
		opis << "Krzyz wspomnien o \n" 
             << attackedplayer->getName() 
             << ".\n Majac poziom " 
             << poziom
             << (attackedplayer->getSex() == PLAYERSEX_FEMALE?" zostala zabita":" zostal zabity")
             << " przez ";			std::stringstream ss;
			ss << corpseitem->getDescription(false);
			ss << "You recognize " << attackedplayer->getName() << ". ";
			if(attacker){
				ss << (attackedplayer->getSex() == PLAYERSEX_FEMALE ? "She" : "He") << " was killed by ";
				Player *attackerplayer = dynamic_cast<Player*>(attacker);
				if(attackerplayer) {
                    opis << attacker->getName();
					ss << attacker->getName();
				}
				else {
					std::string creaturename = attacker->getName();
					std::transform(creaturename.begin(), creaturename.end(), creaturename.begin(), (int(*)(int))tolower);
					ss << article(creaturename);
					opis << creaturename;
				}
			}
 		    char Y[64], Q[64], D[64], h[64], m[64];
            time_t ticks = time(0);
	    	strftime(Y, sizeof(Y), "%Y", localtime(&ticks));
	    	strftime(Q, sizeof(Q), "%m", localtime(&ticks));
	    	strftime(D, sizeof(D), "%d", localtime(&ticks));
	    	strftime(h, sizeof(h), "%H", localtime(&ticks));
	    	strftime(m, sizeof(m), "%M", localtime(&ticks));
            opis << ", dnia " << D << " ";
            int M = atoi(Q);
            switch(M){
              case 1: opis << "stycznia"; break;
              case 2: opis << "lutego"; break;
              case 3: opis << "marca"; break;
              case 4: opis << "kwietnia"; break;
              case 5: opis << "maja"; break;
              case 6: opis << "czerwca"; break;
              case 7: opis << "lipca"; break;
              case 8: opis << "sierpnia"; break;
              case 9: opis << "wrzesnia"; break;
              case 10: opis << "pazdziernika"; break;
              case 11: opis << "listopada"; break;
              case 12: opis << "grudnia"; break;}
            opis << " " << Y << ", o godzinie " << h << ":" << m;         
            game->sendAddThing(NULL,krzyzyk->pos,krzyzyk);   
			//set body special description
			corpseitem->setSpecialDescription(ss.str());
			krzyzyk->setSpecialDescription(opis.str());
			//send corpse to the dead player. It is not in spectator list
			// because was removed
			attackedplayer->onThingAppear(corpseitem);
			attackedplayer->onThingAppear(krzyzyk);
		}
#ifdef RAVEN_SUMMON_DELETE		
		if(summonek) delete corpseitem;
else
#endif
		corpseitem->setOwner(attacker->getName());
		game->startDecay(corpseitem);

		//Get all creatures that will gain xp from this kill..
		CreatureState* attackedCreatureState = NULL;
		std::vector<long> creaturelist;
		if(!(dynamic_cast<Player*>(attackedCreature) && game->getWorldType() != WORLD_TYPE_PVP_ENFORCED)){
			creaturelist = attackedCreature->getInflicatedDamageCreatureList();
			CreatureStateVec& creatureStateVec = creaturestates[tile];
			for(CreatureStateVec::iterator csIt = creatureStateVec.begin(); csIt != creatureStateVec.end(); ++csIt) {
				if(csIt->first == attackedCreature) {
					attackedCreatureState = &csIt->second;
					break;
				}
			}
		}

		if(attackedCreatureState) { //should never be NULL..
			//Add experience
			for(std::vector<long>::const_iterator iit = creaturelist.begin(); iit != creaturelist.end(); ++iit) {
				Creature* gainExpCreature = game->getCreatureByID(*iit);
				if(gainExpCreature) {
					exp_t gainedExperience = attackedCreature->getGainedExperience(gainExpCreature);
					if(gainedExperience <= 0)
						continue;

					Player *gainExpPlayer = dynamic_cast<Player*>(gainExpCreature);

					if(gainExpPlayer) {
						gainExpPlayer->addExp(gainedExperience);
					}

					//Need to add this creature and all that can see it to spectators, unless they already added
					SpectatorVec creaturelist;
					game->getSpectators(Range(gainExpCreature->pos, true), creaturelist);

					for(SpectatorVec::const_iterator cit = creaturelist.begin(); cit != creaturelist.end(); ++cit) {
						if(std::find(spectatorlist.begin(), spectatorlist.end(), *cit) == spectatorlist.end()) {
							spectatorlist.push_back(*cit);
						}
					}

					//Add creature to attackerlist
					attackedCreatureState->attackerlist.push_back(gainExpCreature);
				}
			}
		}

		Player *player = dynamic_cast<Player*>(attacker);
		if(player){
			player->sendStats();
		}

		if(attackedCreature && attackedCreature->getMaster() != NULL) {
			attackedCreature->getMaster()->removeSummon(attackedCreature);
		}
	}


	//Add blood?
#ifdef TJ_MONSTER_BLOOD
	if((drawBlood || attackedCreature->health <= 0) && damage > 0 && attackedCreature->bloodsplash != 255) {
		Item* splash = Item::CreateItem(dead? ITEM_POOL : ITEM_SPLASH, attackedCreature->bloodsplash);
		game->addThing(NULL, CreaturePos, splash);
		game->startDecay(splash);
		game->updateTile(CreaturePos); //bbk blood
	} 
#else
	if((drawBlood || attackedCreature->health <= 0) && damage > 0) {
		Item* splash = Item::CreateItem(ITEM_SPLASH, FLUID_BLOOD);
		game->addThing(NULL, CreaturePos, splash);
		game->startDecay(splash);
	}
#endif //TJ_MONSTER_BLOOD
}


Game::Game()
{
	eventIdCount = 1000;
	this->game_state = GAME_STATE_NORMAL;
	this->map = NULL;
	this->worldType = WORLD_TYPE_PVP;
	OTSYS_THREAD_LOCKVARINIT(gameLock);
	OTSYS_THREAD_LOCKVARINIT(eventLock);
	OTSYS_THREAD_LOCKVARINIT(AutoID::autoIDLock);
#if defined __EXCEPTION_TRACER__
	OTSYS_THREAD_LOCKVARINIT(maploadlock);
#endif
	OTSYS_THREAD_SIGNALVARINIT(eventSignal);
	BufferedPlayers.clear();
	OTSYS_CREATE_THREAD(eventThread, this);

#ifdef __DEBUG_CRITICALSECTION__
	OTSYS_CREATE_THREAD(monitorThread, this);
#endif

	addEvent(makeTask(DECAY_INTERVAL, boost::bind(&Game::checkDecay,this,DECAY_INTERVAL)));

#ifdef CVS_DAY_CYCLE
	int daycycle = 3600;
	light_hour_delta = 1440*10/daycycle;
	light_hour = 0;
	lightlevel = LIGHT_LEVEL_NIGHT;
	light_state = LIGHT_STATE_NIGHT;
	addEvent(makeTask(10000, boost::bind(&Game::checkLight, this, 10000)));
#endif //CVS_DAY_CYCLE
}


Game::~Game()
{
	if(map) {
		delete map;
	}
}

void Game::setWorldType(enum_world_type type)
{
	this->worldType = type;
}

enum_game_state Game::getGameState()
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::getGameState()");
	return game_state;
}

int Game::loadMap(std::string filename, std::string filekind) {
	if(!map)
		map = new Map;

	max_players = atoi(g_config.getGlobalString("maxplayers").c_str());
	distanceToKill = atoi(g_config.getGlobalString("dist", "50").c_str());
	return map->loadMap(filename, filekind);
}



/*****************************************************************************/

#ifdef __DEBUG_CRITICALSECTION__

OTSYS_THREAD_RETURN Game::monitorThread(void *p)
{
  Game* _this = (Game*)p;

	while (true) {
		OTSYS_SLEEP(6000);

		int ret = OTSYS_THREAD_LOCKEX(_this->gameLock, 60 * 2 * 1000);
		if(ret != OTSYS_THREAD_TIMEOUT) {
			OTSYS_THREAD_UNLOCK(_this->gameLock, NULL);
			continue;
		}

		bool file = false;
		std::ostream *outdriver;
		std::cout << "Error: generating critical section file..." <<std::endl;
		std::ofstream output("deadlock.txt",std::ios_base::app);
		if(output.fail()){
			outdriver = &std::cout;
			file = false;
		}
		else{
			file = true;
			outdriver = &output;
		}

		time_t rawtime;
		time(&rawtime);
		*outdriver << "*****************************************************" << std::endl;
		*outdriver << "Error report - " << std::ctime(&rawtime) << std::endl;

		OTSYS_THREAD_LOCK_CLASS::LogList::iterator it;
		for(it = OTSYS_THREAD_LOCK_CLASS::loglist.begin(); it != OTSYS_THREAD_LOCK_CLASS::loglist.end(); ++it) {
			*outdriver << (it->lock ? "lock - " : "unlock - ") << it->str
				<< " threadid: " << it->threadid
				<< " time: " << it->time
				<< " ptr: " << it->mutexaddr
				<< std::endl;
		}

		*outdriver << "*****************************************************" << std::endl;
		if(file)
			((std::ofstream*)outdriver)->close();

		std::cout << "Raport bledow zostal wygenerowany. Zamykam serwer." <<std::endl;
		exit(1); //force exit
	}
}
#endif

OTSYS_THREAD_RETURN Game::eventThread(void *p)
{
#if defined __EXCEPTION_TRACER__
	ExceptionHandler eventExceptionHandler;
	eventExceptionHandler.InstallHandler();
#endif

  Game* _this = (Game*)p;

  // basically what we do is, look at the first scheduled item,
  // and then sleep until it's due (or if there is none, sleep until we get an event)
  // of course this means we need to get a notification if there are new events added
  while (true)
  {
#ifdef __DEBUG__EVENTSCHEDULER__
    std::cout << "schedulercycle start..." << std::endl;
#endif

    SchedulerTask* task = NULL;
		bool runtask = false;

    // check if there are events waiting...
    OTSYS_THREAD_LOCK(_this->eventLock, "eventThread()")

		int ret;
    if (_this->eventList.size() == 0) {
      // unlock mutex and wait for signal
      ret = OTSYS_THREAD_WAITSIGNAL(_this->eventSignal, _this->eventLock);
    } else {
      // unlock mutex and wait for signal or timeout
      ret = OTSYS_THREAD_WAITSIGNAL_TIMED(_this->eventSignal, _this->eventLock, _this->eventList.top()->getCycle());
    }
    // the mutex is locked again now...
    if (ret == OTSYS_THREAD_TIMEOUT) {
      // ok we had a timeout, so there has to be an event we have to execute...
#ifdef __DEBUG__EVENTSCHEDULER__
      std::cout << "event found at " << OTSYS_TIME() << " which is to be scheduled at: " << _this->eventList.top()->getCycle() << std::endl;
#endif
      task = _this->eventList.top();
      _this->eventList.pop();
		}

		if(task) {
			std::map<unsigned long, SchedulerTask*>::iterator it = _this->eventIdMap.find(task->getEventId());
			if(it != _this->eventIdMap.end()) {
				_this->eventIdMap.erase(it);
				runtask = true;
			}
		}

		OTSYS_THREAD_UNLOCK(_this->eventLock, "eventThread()");
    if (task) {
			if(runtask) {
				(*task)(_this);
			}
			delete task;
    }
  }
#if defined __EXCEPTION_TRACER__
	eventExceptionHandler.RemoveHandler();
#endif

}

unsigned long Game::addEvent(SchedulerTask* event) {
  bool do_signal = false;
  OTSYS_THREAD_LOCK(eventLock, "addEvent()")

	if(event->getEventId() == 0) {
		++eventIdCount;
		event->setEventId(eventIdCount);
	}

#ifdef __DEBUG__EVENTSCHEDULER__
		std::cout << "addEvent - " << event->getEventId() << std::endl;
#endif

	eventIdMap[event->getEventId()] = event;

	/*
	if (eventList.empty() ||  *event < *eventList.top())
    do_signal = true;
	*/

	bool isEmpty = eventList.empty();
	eventList.push(event);

	if(isEmpty || *event < *eventList.top())
		do_signal = true;

	/*
	if (eventList.empty() ||  *event < *eventList.top())
    do_signal = true;
	*/

  OTSYS_THREAD_UNLOCK(eventLock, "addEvent()")

	if (do_signal)
		OTSYS_THREAD_SIGNAL_SEND(eventSignal);

	return event->getEventId();
}

bool Game::stopEvent(unsigned long eventid) {
	if(eventid == 0)
		return false;

  OTSYS_THREAD_LOCK(eventLock, "stopEvent()")

	std::map<unsigned long, SchedulerTask*>::iterator it = eventIdMap.find(eventid);
	if(it != eventIdMap.end()) {

#ifdef __DEBUG__EVENTSCHEDULER__
		std::cout << "stopEvent - eventid: " << eventid << "/" << it->second->getEventId() << std::endl;
#endif

		//it->second->setEventId(0); //invalidate the event
		eventIdMap.erase(it);

	  OTSYS_THREAD_UNLOCK(eventLock, "stopEvent()")
		return true;
	}

  OTSYS_THREAD_UNLOCK(eventLock, "stopEvent()")
	return false;
}

/*****************************************************************************/

uint32_t Game::getPlayersOnline() {return (uint32_t)Player::listPlayer.list.size();};
uint32_t Game::getMonstersOnline() {return (uint32_t)Monster::listMonster.list.size();};
uint32_t Game::getNpcsOnline() {return (uint32_t)Npc::listNpc.list.size();};
uint32_t Game::getCreaturesOnline() {return (uint32_t)listCreature.list.size();};

Tile* Game::getTile(unsigned short _x, unsigned short _y, unsigned char _z)
{
	return map->getTile(_x, _y, _z);
}

Tile* Game::getTile(const Position& pos)
{
	return map->getTile(pos);
}

void Game::setTile(unsigned short _x, unsigned short _y, unsigned char _z, unsigned short groundId)
{
	map->setTile(_x, _y, _z, groundId);
}

Creature* Game::getCreatureByID(unsigned long id)
{
	if(id == 0)
		return NULL;

	AutoList<Creature>::listiterator it = listCreature.list.find(id);
	if(it != listCreature.list.end()) {
		return (*it).second;
	}

	return NULL; //just in case the player doesnt exist
}

Player* Game::getPlayerByID(unsigned long id)
{
	if(id == 0)
		return NULL;

	AutoList<Player>::listiterator it = Player::listPlayer.list.find(id);
	if(it != Player::listPlayer.list.end()) {
		return (*it).second;
	}

	return NULL; //just in case the player doesnt exist
}

Creature* Game::getCreatureByName(const std::string &s)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::getCreatureByName()");

	std::string txt1 = s;
	std::transform(txt1.begin(), txt1.end(), txt1.begin(), upchar);
	for(AutoList<Creature>::listiterator it = listCreature.list.begin(); it != listCreature.list.end(); ++it){
		std::string txt2 = (*it).second->getName();
		std::transform(txt2.begin(), txt2.end(), txt2.begin(), upchar);
		if(txt1 == txt2)
			return it->second;
	}

	return NULL; //just in case the creature doesnt exist
}

Creature* Game::getCreatureByPosition(int x, int y, int z)
{
	for(AutoList<Creature>::listiterator it = listCreature.list.begin(); it != listCreature.list.end(); ++it){
		if(it->second->pos.x == x && it->second->pos.y == y && it->second->pos.z == z)
			return it->second;
	}

	return NULL;
}

Player* Game::getPlayerByName(const std::string &s)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::getPlayerByName()");

	std::string txt1 = s;
	std::transform(txt1.begin(), txt1.end(), txt1.begin(), upchar);
	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
		std::string txt2 = (*it).second->getName();
		std::transform(txt2.begin(), txt2.end(), txt2.begin(), upchar);
		if(txt1 == txt2)
			return it->second;
	}

	return NULL; //just in case the player doesnt exist
}

bool Game::placeCreature(Position &pos, Creature* c
#ifdef YUR_LOGIN_QUEUE
						 , int* placeInQueue
#endif //YUR_LOGIN_QUEUE
						 )
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::placeCreature()");
	bool success = false;
	Player *p = dynamic_cast<Player*>(c);
	Monster *monsterzin = dynamic_cast<Monster*>(c);

#ifdef YUR_LOGIN_QUEUE
	if (!p || c->access >= g_config.ACCESS_ENTER || 
 #ifdef YUR_PREMIUM_PROMOTION
		(p->isPremium() && !g_config.QUEUE_PREMMY) ||
 #endif //YUR_PREMIUM_PROMOTION
		loginQueue.login(p->accountNumber, getPlayersOnline(), max_players, placeInQueue)) {
#else //YUR_LOGIN_QUEUE
	if (!p || c->access >= g_config.ACCESS_ENTER || getPlayersOnline() < max_players) {
#endif //YUR_LOGIN_QUEUE

		success = map->placeCreature(pos, c);
		if(success) {
			c->useThing();

			c->setID();
			//std::cout << "place: " << c << " " << c->getID() << std::endl;
			listCreature.addList(c);
			c->addList();
			c->isRemoved = false;

			sendAddThing(NULL,c->pos,c);
			
			
    if (p)
    {
        AutoList<Player>::listiterator iter = Player::listPlayer.list.begin();

        std::string info =  "<li>" + (*iter).second->getName();
        ++iter;
  
        int total = 1;
        while (iter != Player::listPlayer.list.end())
        {
            info += "</li>\n <li>";
            info += (*iter).second->getName();
            ++iter;
            total++;
        }
        info += "</li> Total: ";

        char buf[8];
        itoa( total, buf, 10 );
        info += buf;
  
        std::stringstream file;
        std::string datadir = g_config.getGlobalString("datadir");
         file << datadir << "logs/" << "online.php";
         FILE* f = fopen(file.str().c_str(), "w");
         if(f) {
            fputs(info.c_str(), f);
            fclose(f);
        }
    }

			if(p)
			{     
                 checkRecord();
#ifdef __DEBUG_PLAYERS__
				std::cout << (uint32_t)getPlayersOnline() << " graczy online." << std::endl;
#ifdef TLM_BEDS
if (p->isSleeping()){
changeBed(getBedPos(p->getName()), getBedID(getBedPos(p->getName())), "Nobody");
int SET_TIME = (long)g_config.getGlobalNumber("bedregain",60); 
      int howmuch;
    time_t login;
    login = std::time(NULL);
    howmuch = login - p->lastlogout;
    howmuch = howmuch/SET_TIME; 
    if(p->healthmax - p->health > 0)
          p->health += min(howmuch, p->healthmax - p->health);
    if(p->manamax - p->mana > 0)
          p->mana += min(howmuch, p->manamax - p->mana);
              }
#endif //TLM_BEDS
#endif
#ifdef YUR_GUILD_SYSTEM
				Guilds::ReloadGuildInfo(p);
#endif //YUR_GUILD_SYSTEM
#ifdef ELEM_VIP_LIST
                fixtilebyrex(p->pos,false);
				vipLogin(p);
#endif //ELEM_VIP_LIST
			}

			if(p){
				c->eventCheck = addEvent(makeTask(1000, std::bind2nd(std::mem_fun(&Game::checkCreature), c->getID())));
			}
			else{
				c->eventCheck = addEvent(makeTask(500, std::bind2nd(std::mem_fun(&Game::checkCreature), c->getID())));
			}

			//c->eventCheckAttacking = addEvent(makeTask(2000, std::bind2nd(std::mem_fun(&Game::checkCreatureAttacking), c->getID())));
		}
	}
	else {
		//we cant add the player, server is full
		success = false;
	}
if(monsterzin){
     c->masterPos.x = c->pos.x;
	 c->masterPos.y = c->pos.y;
	 c->masterPos.z = c->pos.z;  	   
  }
  return success;
}

bool Game::removeCreature(Creature* c)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::removeCreature()");
	if(c->isRemoved == true)
		return false;
#ifdef __DEBUG__
	std::cout << "usuwam stworzenie "<< std::endl;
#endif

	if(!removeThing(NULL,c->pos,c))
		return false;

	//std::cout << "remove: " << c << " " << c->getID() << std::endl;
	listCreature.removeList(c->getID());
	c->removeList();
	if(g_config.SUMMON_BODIES){
	c->isRemoved = true;

#ifdef CAYAN_SUMMONS
Creature *summon = NULL;
for(std::list<Creature*>::iterator cit = c->summons.begin(); cit != c->summons.end(); ++cit) {
summon = (*cit);
Position CreaturePos = summon->pos;
Tile *tile = map->getTile(CreaturePos);
Item *corpseitem = Item::CreateItem(summon->getLookCorpse());
corpseitem->pos = CreaturePos; /*summoned bodies by Cayan*/
tile->addThing(corpseitem);
removeCreature(summon);
updateTile(CreaturePos);
}
}
else
{
 c->isRemoved = true;
 
for(std::list<Creature*>::iterator cit = c->summons.begin(); cit != c->summons.end(); ++cit) {
removeCreature(*cit);
}
}
#endif //CAYAN_SUMMONS

	for(std::list<Creature*>::iterator cit = c->summons.begin(); cit != c->summons.end(); ++cit) {
		removeCreature(*cit);
	}

	stopEvent(c->eventCheck);
	stopEvent(c->eventCheckAttacking);
	#ifdef BD_FOLLOW
stopEvent(c->eventCheckFollow);
#endif //BD_FOLLOW

	Player* player = dynamic_cast<Player*>(c);
	if(player){
       #ifdef TLM_SKULLS_PARTY
	    for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
           Player *on = dynamic_cast<Player*>(it->second);
		   if(on && player->isYellowTo(on)){
			/*#ifdef DEBUG_YELLOWSKULL
            std::cout << "Debugger: " << player->getName() << " isYellowTo " << on->getName() << ". Removing from list.. (Removing player.)" << std::endl;
            #endif //DEBUG_YELLOWSKULL*/
		   on->removeFromYellowList(player);
		   /*#ifdef DEBUG_YELLOWSKULL
		   std::cout << "Debugger: Erased " << player->getName() << " from " << on->getName() << "'s hasAsYellow list. (Removed player.)" << std::endl;
		   #endif //DEBUG_YELLOWSKULL*/
           }
		   if(on && on->hasAttacked(player)){
		   /*#ifdef DEBUG_YELLOWSKULL
		   std::cout << "Debugger: " << on->getName() << " hasAttacked " << player->getName() << ". Removing from list.. (Removing player.)" << std::endl;
		   #endif //DEBUG_YELLOWSKULL*/
		   on->removeFromAttakedList(player);
		   /*#ifdef DEBUG_YELLOWSKULL
		   std::cout << "Debugger: Erased " << player->getName() << " from " << on->getName() << "'s attakedPlayers list. (Removed player.)" << std::endl;
		   #endif //DEBUG_YELLOWSKULL*/
           }
           player->clearAttakedList();
           player->clearYellowList();
        }
        
		if(player->party != 0)
			LeaveParty(player);
#endif //TLM_SKULLS_PARTY        

		if(player->tradePartner != 0) {
			playerCloseTrade(player);
		}
		if(player->eventAutoWalk)
			stopEvent(player->eventAutoWalk);

		g_chat.removeUserFromAllChannels(player);
		if(!IOPlayer::instance()->savePlayer(player)){
			std::cout << "Blad podczas zapisu pliku gracza: " << player->getName() << std::endl;
		}
		#ifdef __DEBUG_PLAYERS__
		std::cout << (uint32_t)getPlayersOnline() << " graczy online." << std::endl;
		#endif

#ifdef ELEM_VIP_LIST
        fixtilebyrex(player->pos,true);
		vipLogout(c->getName());
#endif //ELEM_VIP_LIST
	}
#ifdef ON_TILE
if(player) //CHANGE onWalk
{
    Tile* fromT = getTile(player->pos);
    actions.luaWalkOff(player,player->pos,fromT->ground->getID(),fromT->ground->getUniqueId(),fromT->ground->getActionId());
}
#endif //ON_TILE
	this->FreeThing(c);
	return true;
}
#ifdef MOVE_UP
void Game::thingMove(Player *player, unsigned char from_cid, unsigned char from_slotid)
{
 OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::thingMove() - 0");
  
 thingMoveInternal(player, from_cid, from_slotid);
}
#endif //MOVE_UP
void Game::thingMove(Creature *creature, Thing *thing,
	unsigned short to_x, unsigned short to_y, unsigned char to_z, unsigned char count)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::thingMove() - 1");

	Tile *fromTile = map->getTile(thing->pos);

	if (fromTile)
	{
		int oldstackpos = fromTile->getThingStackPos(thing);
		thingMoveInternal(creature, thing->pos.x, thing->pos.y, thing->pos.z, oldstackpos, 0, to_x, to_y, to_z, count);
	}
}


void Game::thingMove(Creature *creature, unsigned short from_x, unsigned short from_y, unsigned char from_z,
	unsigned char stackPos, unsigned short itemid, unsigned short to_x, unsigned short to_y, unsigned char to_z, unsigned char count)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::thingMove() - 2");

	Tile *fromTile = getTile(from_x, from_y, from_z);
	if(!fromTile)
		return;

	Tile *toTile = getTile(to_x, to_y, to_z);
	if(!toTile)
		return;

#ifdef FIXY
	if (toTile->isHouse() && !fromTile->isHouse())
		return;
#endif //FIXY

	Thing* thing = fromTile->getThingByStackPos(stackPos);
	if(!thing)
		return;

	Item* item = dynamic_cast<Item*>(thing);

	if(item && (item->getID() != itemid || item != fromTile->getTopDownItem()))
		return;

	thingMoveInternal(creature, from_x, from_y, from_z, stackPos, itemid, to_x, to_y, to_z, count);
}


//container/inventory to container/inventory
void Game::thingMove(Player *player,
	unsigned char from_cid, unsigned char from_slotid, unsigned short itemid, bool fromInventory,
	unsigned char to_cid, unsigned char to_slotid, bool toInventory,
	unsigned char count)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::thingMove() - 3");

	thingMoveInternal(player, from_cid, from_slotid, itemid, fromInventory,
		to_cid, to_slotid, toInventory, count);
}

//container/inventory to ground
void Game::thingMove(Player *player,
	unsigned char from_cid, unsigned char from_slotid, unsigned short itemid, bool fromInventory,
	const Position& toPos, unsigned char count)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::thingMove() - 4");

#ifdef FIXY
	Tile *toTile = getTile(toPos.x, toPos.y, toPos.z);
	if(!toTile)
		return;
	
	if (player) {
		Tile *fromTile = getTile(player->pos);
		if(!fromTile->isHouse() && toTile->isHouse())	
			return;
	}
#endif //FIXY

	thingMoveInternal(player, from_cid, from_slotid, itemid, fromInventory, toPos, count);
}

//ground to container/inventory
void Game::thingMove(Player *player,
	const Position& fromPos, unsigned char stackPos, unsigned short itemid,
	unsigned char to_cid, unsigned char to_slotid, bool toInventory,
	unsigned char count)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::thingMove() - 5");
	thingMoveInternal(player, fromPos, stackPos, itemid, to_cid, to_slotid, toInventory, count);
}

/*ground -> ground*/
#ifdef FIXY
bool Game::onPrepareMoveThing(Creature* player, /*const*/ Thing* thing,
const Position& fromPos, const Position& toPos, int count)
{
 	const Creature* movingCreature = dynamic_cast<const Creature*>(thing);

		if ((player->access < g_config.ACCESS_REMOTE || dynamic_cast<const Player*>(thing)) && 
		((abs(player->pos.x - fromPos.x) > 1) || (abs(player->pos.y - fromPos.y) > 1) || (player->pos.z != fromPos.z))) 
	{
		player->sendCancel("Za daleko...");
		return false;
 }
 else if( ((abs(fromPos.x - toPos.x) > thing->throwRange) || (abs(fromPos.y - toPos.y) > thing->throwRange)
  || (fromPos.z != toPos.z)) && player->access >= g_config.ACCESS_PROTECT) {
        if(player == thing)
           teleport(player,toPos);
        else
           teleport(thing,toPos);
    }	

	else if ((player->access < g_config.ACCESS_REMOTE || dynamic_cast<const Player*>(thing)) && ((abs(fromPos.x - toPos.x) > thing->throwRange || abs(fromPos.y - toPos.y) > thing->throwRange || (abs(fromPos.z - toPos.z+1) > thing->throwRange)))) {
        player->sendCancel("Destination is out of reach.");
		return false;
	}

          
	    else if(player->access < g_config.ACCESS_REMOTE && movingCreature && fromPos.z != toPos.z){
    player->sendCancel("Za daleko...");
    return false;
}

	else {
		const Item* item = dynamic_cast<const Item*>(thing);
		if(item) {
			int blockstate = 0;
			if(item->isBlocking())
				blockstate |= BLOCK_SOLID;

			if(item->isPickupable() || !item->isNotMoveable())
				blockstate |= BLOCK_PICKUPABLE;

			if(blockstate != 0) {
				switch(map->canThrowObjectTo(fromPos, toPos, blockstate)) {
					case RET_NOERROR:
						return true;
						break;

					case RET_CANNOTTHROW:
						player->sendCancel("Nie mozesz tam rzucic.");
						return false;
						break;

					case RET_CREATUREBLOCK:
					case RET_NOTENOUGHROOM:
						player->sendCancel("Przykro mi, nie ma miejsca.");
						return false;
						break;

					default:
						player->sendCancel("Sorry not possible.");
						return false;
						break;
				}
			}
		}
	}

	return true;
}
#else //FIXY
bool Game::onPrepareMoveThing(Creature* player, const Thing* thing,
	const Position& fromPos, const Position& toPos, int count)
{
	if ((player->access < g_config.ACCESS_REMOTE || dynamic_cast<const Player*>(thing)) &&
		((abs(player->pos.x - fromPos.x) > 1) || (abs(player->pos.y - fromPos.y) > 1) || (player->pos.z != fromPos.z)))
	{
		player->sendCancel("Za daleko...");
		return false;
	}
	else if ((player->access < g_config.ACCESS_REMOTE || dynamic_cast<const Player*>(thing)) &&
		((abs(fromPos.x - toPos.x) > thing->throwRange) || (abs(fromPos.y - toPos.y) > thing->throwRange)
		|| (fromPos.z != toPos.z) /*TODO: Make it possible to throw items to different floors ))*/
	{
		player->sendCancel("Destination is out of reach.");
		return false;
	}
	else {
		const Item* item = dynamic_cast<const Item*>(thing);
		if(item) {
			int blockstate = 0;
			if(item->isBlocking())
				blockstate |= BLOCK_SOLID;

			if(item->isPickupable() || !item->isNotMoveable())
				blockstate |= BLOCK_PICKUPABLE;

			if(blockstate != 0) {
				switch(map->canThrowObjectTo(fromPos, toPos, blockstate)) {
					case RET_NOERROR:
						return true;
						break;

					case RET_CANNOTTHROW:
						player->sendCancel("Nie mozesz tam rzucic.");
						return false;
						break;

					case RET_CREATUREBLOCK:
					case RET_NOTENOUGHROOM:
						player->sendCancel("Przykro mi, nie ma miejsca.");
						return false;
						break;

					default:
						player->sendCancel("Sorry not possible.");
						return false;
						break;
				}
			}
		}
	}
	return true;
}
#endif //FIXY

/*ground -> ground*/
bool Game::onPrepareMoveThing(Creature* creature, const Thing* thing,
	const Tile* fromTile, const Tile *toTile, int count)
{
	const Player* player = dynamic_cast<const Player*>(creature);

	const Item *item = dynamic_cast<const Item*>(thing);
	const Creature* movingCreature = dynamic_cast<const Creature*>(thing);
	const Player* movingPlayer = dynamic_cast<const Player*>(thing);

	if(item && !item->canMovedTo(toTile)) {
	 	creature->sendCancel("To jest niemozliwe.");
		return false;
	}
	else if(movingCreature && !movingCreature->canMovedTo(toTile)) {
    if(player) {
		  player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
		  player->sendCancelWalk();
    }

		return false;
	}
	else if(!movingPlayer && toTile && toTile->floorChange()) {
		creature->sendCancel("To jest niemozliwe.");
		return false;
	}
  else if(movingCreature && toTile && !toTile->ground) {
    if(player) {
      player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
      player->sendCancelWalk();
    }

		return false;
	}

	if (fromTile && fromTile->splash == thing && fromTile->splash->isNotMoveable()) {
			creature->sendCancel("Przepraszam, ale nie mozesz przesunac tego przedmiotu.");
#ifdef __DEBUG__
		cout << creature->getName() << " probuje przesunac rozlany przedmiot!" << std::endl;
#endif
		return false;
	}
	else if (item && item->isNotMoveable()) {
			creature->sendCancel("Przepraszam, ale nie mozesz przesunac tego przedmiotu.");
#ifdef __DEBUG__
		cout << creature->getName() << " probuje przesunac item, ktorego nie mozna przesunac(isNotMoveable)!" << std::endl;
#endif
		return false;
	}
//bbkowner	
//	else if (item && item->getOwner() != "") {
//			creature->sendCancel("Nie jestes wlascicielem.");
//#ifdef __DEBUG__
//		cout << creature->getName() << " probuje przesunac item, ktorego nie jest wlascicielem(itemHasOwner)!" << std::endl;
//#endif
//		return false;
//	}
//bbkowner

#ifdef TLM_HOUSE_SYSTEM
	if (item && toTile && toTile->isHouse())
	{
		const Container* container = dynamic_cast<const Container*>(item);
		int moving = container? container->getItemHoldingCount() : 1;

		if (moving + toTile->getItemHoldingCount() > g_config.MAX_HOUSE_TILE_ITEMS)
		{
			creature->sendCancel("Nie mozesz polozyc wiecej przedmiotow w tym domku, gdyz limit zostal osiagniety.");
			return false;
		}
	}
#endif //TLM_HOUSE_SYSTEM

	return true; //return thing->canMovedTo(toTile);
}

/*inventory -> container*/
bool Game::onPrepareMoveThing(Player* player, const Item* fromItem, slots_t fromSlot,
	const Container* toContainer, const Item* toItem, int count)
{
	if(!fromItem->isPickupable()) {
		player->sendCancel("To jest niemozliwe.");
		return false;
	}
	else {
		const Container *itemContainer = dynamic_cast<const Container*>(fromItem);
		if(itemContainer) {
			if(itemContainer->isHoldingItem(toContainer) || (toContainer == itemContainer)) {
				player->sendCancel("To jest niemozliwe.");
				return false;
			}
		}

		if((!fromItem->isStackable() || !toItem || fromItem->getID() != toItem->getID() || toItem->getItemCountOrSubtype() >= 100) && toContainer->size() + 1 > toContainer->capacity()) {
			player->sendCancel("Tutaj nie ma wystarczajacej ilosci miejsca dla tego przedmiotu.");
			return false;
		}

		Container const *topContainer = toContainer->getTopParent();
		int itemsToadd;
		if(!topContainer)
			topContainer = toContainer;

		Container const *fromContainer = dynamic_cast<const Container*>(fromItem);
		if(fromContainer)
			itemsToadd = fromContainer->getItemHoldingCount() + 1;
		else
			itemsToadd = 1;

		if(topContainer->depot != 0 && player->max_depot_items != 0 && topContainer->getItemHoldingCount() + itemsToadd >= player->max_depot_items){
			player->sendCancel("Nie mozesz wrzucic wiecej itemow do swojego depozytu, poniewaz wyczerpales maksymalny limit.");
			return false;
		}
	}

	return true;
}

/*container -> container*/
/*ground -> container*/
bool Game::onPrepareMoveThing(Player* player,
	const Position& fromPos, const Container* fromContainer, const Item* fromItem,
	const Position& toPos, const Container* toContainer, const Item* toItem, int count)
{
	if (player->access < g_config.ACCESS_REMOTE && 
		((abs(player->pos.x - fromPos.x) > 1) || (abs(player->pos.y - fromPos.y) > 1) || (player->pos.z != fromPos.z))) 
	{
		player->sendCancel("Jestes zbyt daleko... Podejdz blizej");
		return false;
	}
	else if (player->access < g_config.ACCESS_REMOTE && 
		((abs(fromPos.x - toPos.x) > fromItem->throwRange) || (abs(fromPos.y - toPos.y) > fromItem->throwRange)
		|| (fromPos.z != toPos.z))) 
	{
		player->sendCancel("Miejsce do ktorego chcesz przerzucic przedmiot jest poza twoim zasiegiem.");
		return false;
	}
//bbkowner
//  const Player *play = dynamic_cast<const Player*>(player);
  const Item *fit = dynamic_cast<const Item*>(fromItem);
//     Item *item = dynamic_cast<Item*>(tile->getThingByStackPos(tile->getThingCount()-1));
//  const int oldstackpos = (const)fromPos->getThingStackPos(fit);
// unsigned char stackpos = 
//  Item *item = dynamic_cast<Item*>(getThing(fromPos, toPos, player));
//	if(fit->getOwner() != "" && play->getName() != fit->getOwner()){
//    Item *item = getThingfromPos(fromPos);
//	if(fromItem->getOwner()){
    
//  const Item *fit2 = fromItem->getID();
    
//		if((fit->owner != "") ) {
//		player->sendCancel("Nie jestes wlascicielem.");
//		return false;
//	}
//bbkowner
	if(!fromItem->isPickupable()) {
		player->sendCancel("Nie mozesz przesunac tego przedmiotu.");
		return false;
	}
	else {
		if((!fromItem->isStackable() || !toItem || fromItem->getID() != toItem->getID() || toItem->getItemCountOrSubtype() >= 100) && toContainer->size() + 1 > toContainer->capacity()) {
			player->sendCancel("Przepraszam, ale tam nie ma odpowiedniej ilosci miejsca.");
			return false;
		}

		const Container *itemContainer = dynamic_cast<const Container*>(fromItem);
		if(itemContainer) {
			if(itemContainer->isHoldingItem(toContainer) || (toContainer == itemContainer) || (fromContainer && fromContainer == itemContainer)) {
				player->sendCancel("To jest niemozliwe.");
				return false;
			}
		}

		double itemWeight = (fromItem->isStackable() ? Item::items[fromItem->getID()].weight * std::max(1, count) : fromItem->getWeight());
		if((!fromContainer || !player->isHoldingContainer(fromContainer)) && player->isHoldingContainer(toContainer)) {
			if(player->access < g_config.ACCESS_PROTECT && player->getFreeCapacity() < itemWeight) {
				player->sendCancel("Ten przedmiot jest za ciezki. Podrzebujesz wiecej cap'a");
				return false;
			}
		}

		Container const *topContainer = toContainer->getTopParent();
		int itemsToadd;
		if(!topContainer)
			topContainer = toContainer;

		Container const *fromContainer = dynamic_cast<const Container*>(fromItem);
		if(fromContainer)
			itemsToadd = fromContainer->getItemHoldingCount() + 1;
		else
			itemsToadd = 1;

		if(topContainer->depot != 0 && player->max_depot_items != 0 && topContainer->getItemHoldingCount() >= player->max_depot_items){
			player->sendCancel("Nie mozesz wrzucic wiecej itemow do swojego depozytu, poniewaz wyczerpales maksymalny limit.");
			return false;
		}
	}

	return true;
}

/*ground -> ground*/
bool Game::onPrepareMoveCreature(Creature *creature, const Creature* creatureMoving,
	const Tile *fromTile, const Tile *toTile)
{
	const Player* playerMoving = dynamic_cast<const Player*>(creatureMoving);
	Player* player = dynamic_cast<Player*>(creature);

	if (creature->access < g_config.ACCESS_PROTECT && creature != creatureMoving && !creatureMoving->isPushable()) {
		creature->sendCancel("To jest niemozliwe.");
    return false;
  }
	if(!toTile){
		if(creature == creatureMoving && player) {
			player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
			player->sendCancelWalk();
		}

		return false;
	}
  else if (playerMoving && toTile->isPz() && playerMoving->pzLocked) {
		if (creature == creatureMoving && creature->pzLocked) {

			if(player) {
				player->sendTextMessage(MSG_SMALLINFO, "Nie mozesz wejsc do strefy ochronnej po tym jak zaatakowales innego gracza.");
				player->sendCancelWalk();
			}

			//player->sendCancelWalk("You can not enter a protection zone after attacking another player.");
			return false;
		}
		else if (playerMoving->pzLocked) {
			creature->sendCancel("To jest niemozliwe.");
			return false;
		}
  }
  else if (playerMoving && fromTile->isPz() && !toTile->isPz() && creature != creatureMoving) {
		creature->sendCancel("To jest niemozliwe.");
		return false;
  }
	else if(creature != creatureMoving && toTile->floorChange()){
		creature->sendCancel("To jest niemozliwe.");
		return false;
	}
  else if(creature != creatureMoving && toTile->getTeleportItem()){
		creature->sendCancel("To jest niemozliwe.");
		return false;
  }

	return true;
}

/*ground -> inventory*/
bool Game::onPrepareMoveThing(Player *player, const Position& fromPos, const Item *item,
	slots_t toSlot, int count)
{
	if (player->access < g_config.ACCESS_REMOTE &&  
		((abs(player->pos.x - fromPos.x) > 1) || (abs(player->pos.y - fromPos.y) > 1) || (player->pos.z != fromPos.z))) 
	{
		player->sendCancel("Jestes zbyt daleko... Podejdz blizej");
		return false;
	}
//bbkowner
//Item *itemm = item;
//	if(item->getOwner() != "" && player->getName() != item->getOwner()){
//		player->sendCancel("Nie jestes wlascicielem.");
//		return false;
//	}
//bbkowner
	else if(!item->isPickupable()) {
		player->sendCancel("Niemozesz przesunac tego przedmiotu.");
		return false;
	}

	double itemWeight = (item->isStackable() ? Item::items[item->getID()].weight * std::max(1, count) : item->getWeight());
	if(player->access < g_config.ACCESS_PROTECT && player->getFreeCapacity() < itemWeight) {
		player->sendCancel("Ten przedmiot jest za ciezki.");
		return false;
	}

	return true;
}

/*inventory -> inventory*/
bool Game::onPrepareMoveThing(Player *player, slots_t fromSlot, const Item *fromItem,
	slots_t toSlot, const Item *toItem, int count)
{
	if(toItem && (!toItem->isStackable() || toItem->getID() != fromItem->getID())) {
		player->sendCancel("Przepraszam, ale brakuje tutaj miejsca.");
		return false;
	}

	return true;
}

/*container -> inventory*/
bool Game::onPrepareMoveThing(Player *player, const Container *fromContainer, const Item *fromItem,
	slots_t toSlot, const Item *toItem, int count)
{
	#ifdef _REX_FIGHT_MOD_
    if (toItem && (toItem->getID() == fromItem->getID() && !toItem->isStackable())){
    #else
	if(toItem && (!toItem->isStackable() || toItem->getID() != fromItem->getID())) {
    #endif //Segundo em container -> inventory
		player->sendCancel("Przepraszam, ale brakuje tutaj miejsca.");
		return false;
	}

	double itemWeight = (fromItem->isStackable() ? Item::items[fromItem->getID()].weight * std::max(1, count) : fromItem->getWeight());
	if(player->access < g_config.ACCESS_PROTECT && !player->isHoldingContainer(fromContainer) &&
		player->getFreeCapacity() < itemWeight) {
		player->sendCancel("Ten przedmiot jest za ciezki. Potrzebujesz wiecej cap'a");
		return false;
	}

	return true;
}

/*->inventory*/
bool Game::onPrepareMoveThing(Player *player, const Item *item,
	slots_t toSlot, int count)
{
	switch(toSlot)
	{
	case SLOT_HEAD:
		if(item->getSlotPosition() & SLOTP_HEAD)
			return true;
		break;
	case SLOT_NECKLACE:
		if(item->getSlotPosition() & SLOTP_NECKLACE)
			return true;
		break;
	case SLOT_BACKPACK:
		if(item->getSlotPosition() & SLOTP_BACKPACK)
			return true;
		break;
	case SLOT_ARMOR:
		if(item->getSlotPosition() & SLOTP_ARMOR)
			return true;
		break;
	case SLOT_RIGHT:
		if(item->getSlotPosition() & SLOTP_RIGHT){
#ifdef _REX_CVS_MOD_
            //One weapon for hand by ReX
            if(item && item->isWeapon() && item->getWeaponType() != SHIELD && player->items[SLOT_LEFT] != NULL && player->items[SLOT_LEFT]->isWeapon() && player->items[SLOT_LEFT]->getWeaponType() != SHIELD){
             player->sendCancel("You may use only one weapon.");
		     return false;
            }
#endif                       
			if(item->getSlotPosition() & SLOTP_TWO_HAND){
				if(player->items[SLOT_LEFT] != NULL){
					player->sendCancel("Najpierw zdejmij dwureczny przedmiot.");
					return false;
				}
				return true	;
			}
			else{
				if(player->items[SLOT_LEFT]){
#ifdef _REX_CVS_MOD_
                    //One weapon for hand by ReX
                    if(item && item->isWeapon() && item->getWeaponType() != SHIELD && player->items[SLOT_LEFT] != NULL && player->items[SLOT_LEFT]->isWeapon() && player->items[SLOT_LEFT]->getWeaponType() != SHIELD){
                    player->sendCancel("You may use only one weapon.");
		            return false;
                    }
#endif
					if(player->items[SLOT_LEFT]->getSlotPosition() & SLOTP_TWO_HAND){
						player->sendCancel("Najpierw zdejmij dwureczny przedmiot.");
						return false;
					}
					return true;
				}
				return true;
			}
		}
		break;
	case SLOT_LEFT:
		if(item->getSlotPosition() & SLOTP_LEFT){
#ifdef _REX_CVS_MOD_
            //One weapon for hand by ReX
            if(item && item->isWeapon() && item->getWeaponType() != SHIELD && player->items[SLOT_RIGHT] != NULL && player->items[SLOT_RIGHT]->isWeapon() && player->items[SLOT_RIGHT]->getWeaponType() != SHIELD){
             player->sendCancel("You may use only one weapon.");
		     return false;
            }
            #endif
			if(item->getSlotPosition() & SLOTP_TWO_HAND){
				if(player->items[SLOT_RIGHT] != NULL){
					player->sendCancel("Najpierw zdejmij dwureczny przedmiot.");
					return false;
				}
				return true	;
			}
			else{
				if(player->items[SLOT_RIGHT]){
#ifdef _REX_CVS_MOD_
                    //One weapon for hand by ReX
                    if(item && item->isWeapon() && item->getWeaponType() != SHIELD && player->items[SLOT_RIGHT] != NULL && player->items[SLOT_RIGHT]->isWeapon() && player->items[SLOT_RIGHT]->getWeaponType() != SHIELD){
                    player->sendCancel("You may use only one weapon.");
		            return false;
                    }
#endif
					if(player->items[SLOT_RIGHT]->getSlotPosition() & SLOTP_TWO_HAND){
						player->sendCancel("Najpierw zdejmij dwureczny przedmiot.");
						return false;
					}
					return true;
				}
				return true;
			}
		}
		break;
	case SLOT_LEGS:
		if(item->getSlotPosition() & SLOTP_LEGS)
			return true;
		break;
	case SLOT_FEET:
		if(item->getSlotPosition() & SLOTP_FEET)
			return true;
		break;
	case SLOT_RING:
		if(item->getSlotPosition() & SLOTP_RING
        #ifdef _REX_CVS_MOD_
        || item->getID() == 2208
        #endif
        )
			return true;
		break;
	case SLOT_AMMO:
		if(item->getSlotPosition() & SLOTP_AMMO)
			return true;
		break;
	}

	player->sendCancel("Ten przedmiot tutaj nie pasuje. Sprobuj wlozyc go do innego slotu");
	return false;
}


#ifdef MOVE_UP
void Game::thingMoveInternal(Player *player, unsigned char from_cid, unsigned char from_slotid)
{
 Container *fromContainer = NULL;
 Container *toContainer = NULL;
 Item *fromItem = NULL;
    
    fromContainer = player->getContainer(from_cid);
    
    if(!fromContainer)
        return;
        
    if(fromContainer->getParent() == NULL)
        return;
    
 toContainer = fromContainer->getParent();
 fromItem = fromContainer->getItem(from_slotid);
 
 if(!toContainer || !fromItem)
     return;
 
 if(toContainer->size() + 1 > toContainer->capacity()) {  
  player->sendCancel("Sorry not enough room.");
  return;
 }
 
 fromContainer->removeItem(fromItem);
 toContainer->addItem(fromItem);
 
 Position fromPos = (fromContainer->pos.x == 0xFFFF ? player->pos : fromContainer->pos);
 Position toPos = (toContainer->pos.x == 0xFFFF ? player->pos : toContainer->pos);
 
 SpectatorVec list;
 SpectatorVec::iterator it;
    if(fromPos == toPos) {
  getSpectators(Range(fromPos, false), list);
 }
 else {
  getSpectators(Range(fromPos, toPos), list);
 }
 if(!list.empty()) {
     //players
     for(it = list.begin(); it != list.end(); ++it) {
      if(dynamic_cast<Player*>(*it)) {
       (*it)->onThingMove(player, fromContainer, from_slotid, fromItem, toContainer);
      }
     }
     //none-players
     for(it = list.begin(); it != list.end(); ++it) {
         if(!dynamic_cast<Player*>(*it)) {
              (*it)->onThingMove(player, fromContainer, from_slotid, fromItem, toContainer);
         }
     }
 }
 else
 player->onThingMove(player, fromContainer, from_slotid, fromItem, toContainer);
}
#endif //MOVE_UP
//container/inventory to container/inventory
void Game::thingMoveInternal(Player *player,
	unsigned char from_cid, unsigned char from_slotid, unsigned short itemid,
	bool fromInventory,unsigned char to_cid, unsigned char to_slotid, bool toInventory,
	unsigned char count)
{
	Container *fromContainer = NULL;
	Container *toContainer = NULL;
	Item *fromItem = NULL;
	Item *toItem = NULL;

	if(fromInventory) {
		fromItem = player->getItem(from_cid);
		fromContainer = dynamic_cast<Container *>(fromItem);
	}
	else {
		fromContainer = player->getContainer(from_cid);

		if(fromContainer) {
			if(from_slotid >= fromContainer->size())
				return;

			fromItem = fromContainer->getItem(from_slotid);
		}
	}

	if(toInventory) {
		toItem = player->getItem(to_cid);
		toContainer = dynamic_cast<Container *>(toItem);
	}
	else {
		toContainer = player->getContainer(to_cid);

		if(toContainer) {
			if(to_slotid >= toContainer->capacity())
				return;

			toItem = toContainer->getItem(to_slotid);
			Container *toSlotContainer = dynamic_cast<Container*>(toItem);
			if(toSlotContainer) {
				toContainer = toSlotContainer;
				toItem = NULL;
			}
		}
	}

	if(!fromItem || (toItem == fromItem) || (fromItem->isStackable() && count > fromItem->getItemCountOrSubtype()) || fromItem->getID() != itemid)
		return;

	//Container to container
	if(!fromInventory && fromContainer && toContainer) {
		Position fromPos = (fromContainer->pos.x == 0xFFFF ? player->pos : fromContainer->pos);
		Position toPos = (toContainer->pos.x == 0xFFFF ? player->pos : toContainer->pos);

		if(onPrepareMoveThing(player, fromPos, fromContainer, fromItem, toPos, toContainer, toItem, count)) {

			autoCloseTrade(fromItem, true);
			int oldFromCount = fromItem->getItemCountOrSubtype();
			int oldToCount = 0;

			//move around an item in a container
			if(fromItem->isStackable()) {
				if(toItem && toItem != fromItem && toItem->getID() == fromItem->getID())
				{
					oldToCount = toItem->getItemCountOrSubtype();
					int newToCount = std::min(100, oldToCount + count);
					toItem->setItemCountOrSubtype(newToCount);

					if(oldToCount != newToCount) {
						autoCloseTrade(toItem);
					}

					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					int surplusCount = oldToCount + count  - 100;
					if(surplusCount > 0) {
						Item *surplusItem = Item::CreateItem(fromItem->getID(), surplusCount);
						if(onPrepareMoveThing(player, fromPos, fromContainer, surplusItem, toPos, toContainer, NULL, count)) {
							autoCloseTrade(toContainer);
							toContainer->addItem(surplusItem);
						}
						else {
							delete surplusItem;
							count -= surplusCount; //re-define the actual amount we move.
							fromItem->setItemCountOrSubtype(fromItem->getItemCountOrSubtype() + surplusCount);
						}
					}
				}
				else if(count < oldFromCount) {
					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					Item* moveItem = Item::CreateItem(fromItem->getID(), count);
					toContainer->addItem(moveItem);
					autoCloseTrade(toContainer);
				}
				else {
					if(fromContainer == toContainer) {
						fromContainer->moveItem(from_slotid, 0);
					}
					else if(fromContainer->removeItem(fromItem)) {
						autoCloseTrade(toContainer);
						toContainer->addItem(fromItem);
					}
				}

				if(fromItem->getItemCountOrSubtype() == 0) {
					fromContainer->removeItem(fromItem);
					this->FreeThing(fromItem);
				}
			}
			else {
				if(fromContainer == toContainer) {
					fromContainer->moveItem(from_slotid, 0);
				}
				else if(fromContainer->removeItem(fromItem)) {
					autoCloseTrade(toContainer);
					toContainer->addItem(fromItem);
				}
			}

			if(player->isHoldingContainer(fromContainer) != player->isHoldingContainer(toContainer)) {
				player->updateInventoryWeigth();
			}

			SpectatorVec list;
			SpectatorVec::iterator it;

			if(fromPos == toPos) {
				getSpectators(Range(fromPos, false), list);
			}
			else {
				getSpectators(Range(fromPos, toPos), list);
			}

			if(!list.empty()) {
				//players
				for(it = list.begin(); it != list.end(); ++it) {
					if(dynamic_cast<Player*>(*it)) {
						(*it)->onThingMove(player, fromContainer, from_slotid, fromItem, oldFromCount, toContainer, to_slotid, toItem, oldToCount, count);
					}
				}

				//none-players
				for(it = list.begin(); it != list.end(); ++it) {
					if(!dynamic_cast<Player*>(*it)) {
						(*it)->onThingMove(player, fromContainer, from_slotid, fromItem, oldFromCount, toContainer, to_slotid, toItem, oldToCount, count);
					}
				}
			}
			else
				player->onThingMove(player, fromContainer, from_slotid, fromItem, oldFromCount, toContainer, to_slotid, toItem, oldToCount, count);
		}
	}
	else {
		//inventory to inventory
		if(fromInventory && toInventory && !toContainer) {
			Position fromPos = player->pos;
			Position toPos = player->pos;
			if(onPrepareMoveThing(player, fromItem, (slots_t)to_cid, count) && onPrepareMoveThing(player, (slots_t)from_cid, fromItem, (slots_t)to_cid, toItem, count)) {

				autoCloseTrade(fromItem, true);
				int oldFromCount = fromItem->getItemCountOrSubtype();
				int oldToCount = 0;

				if(fromItem->isStackable()) {
					if(toItem && toItem != fromItem && toItem->getID() == fromItem->getID())
					{
						oldToCount = toItem->getItemCountOrSubtype();
						int newToCount = std::min(100, oldToCount + count);
						toItem->setItemCountOrSubtype(newToCount);

						if(oldToCount != newToCount) {
							autoCloseTrade(toItem);
						}

						int subcount = oldFromCount - count;
						fromItem->setItemCountOrSubtype(subcount);

						int surplusCount = oldToCount + count  - 100;
						if(surplusCount > 0) {
							fromItem->setItemCountOrSubtype(fromItem->getItemCountOrSubtype() + surplusCount);
							player->sendCancel("Przepraszam, ale tu jest za malo miejsca.");
						}

						if(fromItem->getItemCountOrSubtype() == 0) {
							player->removeItemInventory(from_cid, true);
							this->FreeThing(fromItem);
						}
					}
					else if(count < oldFromCount) {
						int subcount = oldFromCount - count;
						fromItem->setItemCountOrSubtype(subcount);

						autoCloseTrade(toItem);
						player->removeItemInventory(to_cid, true);
						player->addItemInventory(Item::CreateItem(fromItem->getID(), count), to_cid, true);

						if(fromItem->getItemCountOrSubtype() == 0) {
							player->removeItemInventory(from_cid, true);
							this->FreeThing(fromItem);
						}
					}
					else {
						if(player->removeItemInventory(from_cid, true)) {
							player->removeItemInventory(to_cid, true);
							player->addItemInventory(fromItem, to_cid, true);
						}
					}
				}
				else if(player->removeItemInventory(from_cid, true)) {
					player->removeItemInventory(to_cid, true);
					player->addItemInventory(fromItem, to_cid, true);
				}

				player->onThingMove(player, (slots_t)from_cid, fromItem, oldFromCount, (slots_t)to_cid, toItem, oldToCount, count);
			}
		}
		//container to inventory
		else if(!fromInventory && fromContainer && toInventory) {
			if(onPrepareMoveThing(player, fromItem, (slots_t)to_cid, count) && onPrepareMoveThing(player, fromContainer, fromItem, (slots_t)to_cid, toItem, count)) {
				autoCloseTrade(fromItem, true);
				int oldFromCount = fromItem->getItemCountOrSubtype();
				int oldToCount = 0;

				if(fromItem->isStackable()) {
					if(toItem && toItem != fromItem && toItem->getID() == fromItem->getID())
					{
						oldToCount = toItem->getItemCountOrSubtype();
						int newToCount = std::min(100, oldToCount + count);
						toItem->setItemCountOrSubtype(newToCount);

						if(oldToCount != newToCount) {
							autoCloseTrade(toItem);
						}

						int subcount = oldFromCount - count;
						fromItem->setItemCountOrSubtype(subcount);

						int surplusCount = oldToCount + count  - 100;
						if(surplusCount > 0) {
							fromItem->setItemCountOrSubtype(fromItem->getItemCountOrSubtype() + surplusCount);
							player->sendCancel("Przepraszam, ale tu jest za malo miejsca.");
						}

						if(fromItem->getItemCountOrSubtype() == 0) {
							fromContainer->removeItem(fromItem);
							this->FreeThing(fromItem);
						}
					}
					else if(count < oldFromCount) {
						int subcount = oldFromCount - count;
						fromItem->setItemCountOrSubtype(subcount);

						player->removeItemInventory(to_cid, true);
						player->addItemInventory(Item::CreateItem(fromItem->getID(), count), to_cid, true);

						if(toItem) {
							fromContainer->addItem(toItem);
						}

						if(fromItem->getItemCountOrSubtype() == 0) {
							fromContainer->removeItem(fromItem);
							this->FreeThing(fromItem);
						}
					}
					else {
						if(fromContainer->removeItem(fromItem)) {
							player->removeItemInventory(to_cid, true);
							player->addItemInventory(fromItem, to_cid, true);

							if(toItem) {
								fromContainer->addItem(toItem);
							}
						}
					}
				}
				else if(fromContainer->removeItem(fromItem)) {
					player->removeItemInventory(to_cid, true);
					player->addItemInventory(fromItem, to_cid, true);

					if(toItem) {
						fromContainer->addItem(toItem);
					}
				}

				if(!player->isHoldingContainer(fromContainer)) {
					player->updateInventoryWeigth();
				}

				//if(fromContainer->pos.x != 0xFFFF) {
				if(fromContainer->getTopParent()->pos.x != 0xFFFF) {
					SpectatorVec list;
					SpectatorVec::iterator it;

					getSpectators(Range(fromContainer->getTopParent()->pos, false), list);

					//players
					for(it = list.begin(); it != list.end(); ++it) {
						if(dynamic_cast<Player*>(*it)) {
							(*it)->onThingMove(player, fromContainer, from_slotid, fromItem, oldFromCount, (slots_t)to_cid, toItem, oldToCount, count);
						}
					}

					//none-players
					for(it = list.begin(); it != list.end(); ++it) {
						if(!dynamic_cast<Player*>(*it)) {
							(*it)->onThingMove(player, fromContainer, from_slotid, fromItem, oldFromCount, (slots_t)to_cid, toItem, oldToCount, count);
						}
					}
				}
				else
					player->onThingMove(player, fromContainer, from_slotid, fromItem, oldFromCount, (slots_t)to_cid, toItem, oldToCount, count);
			}
		}
		//inventory to container
		else if(fromInventory && toContainer) {
			Position fromPos = player->pos;
			Position toPos = (toContainer->pos.x == 0xFFFF ? player->pos : toContainer->pos);

			int oldFromCount = fromItem->getItemCountOrSubtype();
			int oldToCount = 0;

			if(onPrepareMoveThing(player, fromItem, (slots_t)from_cid, toContainer, toItem, count)) {
				autoCloseTrade(fromItem, true);
				if(fromItem->isStackable()) {
					if(toItem && toItem != fromItem && toItem->getID() == fromItem->getID())
					{
						oldToCount = toItem->getItemCountOrSubtype();
						int newToCount = std::min(100, oldToCount + count);
						toItem->setItemCountOrSubtype(newToCount);

						if(oldToCount != newToCount) {
							autoCloseTrade(toItem);
						}

						int subcount = oldFromCount - count;
						fromItem->setItemCountOrSubtype(subcount);

						int surplusCount = oldToCount + count  - 100;
						if(surplusCount > 0) {
							Item *surplusItem = Item::CreateItem(fromItem->getID(), surplusCount);

							if(onPrepareMoveThing(player, fromPos, NULL, surplusItem, toPos, toContainer, NULL, count)) {
								autoCloseTrade(toContainer);
								toContainer->addItem(surplusItem);
							}
							else {
								delete surplusItem;
								count -= surplusCount; //re-define the actual amount we move.
								fromItem->setItemCountOrSubtype(fromItem->getItemCountOrSubtype() + surplusCount);
							}
						}
					}
					else if(count < oldFromCount) {
						int subcount = oldFromCount - count;
						fromItem->setItemCountOrSubtype(subcount);
						autoCloseTrade(toContainer);
						toContainer->addItem(Item::CreateItem(fromItem->getID(), count));
					}
					else {
						if(player->removeItemInventory((slots_t)from_cid, true)) {
							autoCloseTrade(toContainer);
							toContainer->addItem(fromItem);

							if(player->isHoldingContainer(toContainer)) {
								player->updateInventoryWeigth();
							}
						}
					}

					if(fromItem->getItemCountOrSubtype() == 0) {
						player->removeItemInventory(from_cid, true);
						this->FreeThing(fromItem);
					}
				}
				else if(player->removeItemInventory(from_cid, true)) {
					autoCloseTrade(toContainer);
					toContainer->addItem(fromItem);

					if(player->isHoldingContainer(toContainer)) {
						player->updateInventoryWeigth();
					}
				}

				if(!player->isHoldingContainer(toContainer)) {
					player->updateInventoryWeigth();
				}

				if(toContainer->getTopParent()->pos.x != 0xFFFF) {
					SpectatorVec list;
					SpectatorVec::iterator it;

					getSpectators(Range(toContainer->getTopParent()->pos, false), list);

					//players
					for(it = list.begin(); it != list.end(); ++it) {
						if(dynamic_cast<Player*>(*it)) {
							(*it)->onThingMove(player, (slots_t)from_cid, fromItem, oldFromCount, toContainer, to_slotid, toItem, oldToCount, count);
						}
					}

					//none-players
					for(it = list.begin(); it != list.end(); ++it) {
						if(!dynamic_cast<Player*>(*it)) {
							(*it)->onThingMove(player, (slots_t)from_cid, fromItem, oldFromCount, toContainer, to_slotid, toItem, oldToCount, count);
						}
					}
				}
				else
					player->onThingMove(player, (slots_t)from_cid, fromItem, oldFromCount, toContainer, to_slotid, toItem, oldToCount, count);
			}
		}
	}
}

//container/inventory to ground
void Game::thingMoveInternal(Player *player,
	unsigned char from_cid, unsigned char from_slotid, unsigned short itemid, bool fromInventory,
	const Position& toPos, unsigned char count)
{
	Container *fromContainer = NULL;
	Tile *toTile = map->getTile(toPos);
	if(!toTile)
		return;

	/*container to ground*/
	if(!fromInventory) {
		fromContainer = player->getContainer(from_cid);
		if(!fromContainer)
			return;

		Position fromPos = (fromContainer->pos.x == 0xFFFF ? player->pos : fromContainer->pos);
		Item *fromItem = dynamic_cast<Item*>(fromContainer->getItem(from_slotid));
		Item* toItem = dynamic_cast<Item*>(toTile->getTopDownItem());

		if(!fromItem || (toItem == fromItem) || (fromItem->isStackable() && count > fromItem->getItemCountOrSubtype()) || fromItem->getID() != itemid)
			return;

		if(onPrepareMoveThing(player, fromItem, fromPos, toPos, count) && onPrepareMoveThing(player, fromItem, NULL, toTile, count)) {
			autoCloseTrade(fromItem, true);
			int oldFromCount = fromItem->getItemCountOrSubtype();
			int oldToCount = 0;

			//Do action...
#ifdef TP_TRASH_BINS
			if (toItem && toItem->isDeleter())
			{
				fromContainer->removeItem(fromItem);
				FreeThing(fromItem);
			}
			else if(fromItem->isStackable()) {
#else
			if(fromItem->isStackable()) {
#endif //TP_TRASH_BINS
				if(toItem && toItem->getID() == fromItem->getID())
				{
					oldToCount = toItem->getItemCountOrSubtype();
					int newToCount = std::min(100, oldToCount + count);
					toItem->setItemCountOrSubtype(newToCount);

					if(oldToCount != newToCount) {
						autoCloseTrade(toItem);
					}

					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					int surplusCount = oldToCount + count  - 100;
					if(surplusCount > 0) {
						Item *surplusItem = Item::CreateItem(fromItem->getID(), surplusCount);
						surplusItem->pos = toPos;

						toTile->addThing(surplusItem);
					}

					if(fromItem->getItemCountOrSubtype() == 0) {
						fromContainer->removeItem(fromItem);
						this->FreeThing(fromItem);
					}
				}
				else if(count < oldFromCount) {
					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					Item *moveItem = Item::CreateItem(fromItem->getID(), count);
					moveItem->pos = toPos;
					toTile->addThing(moveItem);

					if(fromItem->getItemCountOrSubtype() == 0) {
						fromContainer->removeItem(fromItem);
						this->FreeThing(fromItem);
					}
				}
				else if(fromContainer->removeItem(fromItem)) {
					fromItem->pos = toPos;
					toTile->addThing(fromItem);
				}
			}
			else if(fromContainer->removeItem(fromItem)) {
				fromItem->pos = toPos;
				toTile->addThing(fromItem);
			}

			if(player->isHoldingContainer(fromContainer)) {
				player->updateInventoryWeigth();
			}

			SpectatorVec list;
			SpectatorVec::iterator it;

			getSpectators(Range(fromPos, false), list);

			SpectatorVec tolist;
			getSpectators(Range(toPos, true), tolist);

			for(it = tolist.begin(); it != tolist.end(); ++it) {
				if(std::find(list.begin(), list.end(), *it) == list.end()) {
					list.push_back(*it);
				}
			}

			//players
			for(it = list.begin(); it != list.end(); ++it) {
				if(dynamic_cast<Player*>(*it)) {
					(*it)->onThingMove(player, fromContainer, from_slotid, fromItem, oldFromCount, toPos, toItem, oldToCount, count);
				}
			}

			//none-players
			for(it = list.begin(); it != list.end(); ++it) {
				if(!dynamic_cast<Player*>(*it)) {
					(*it)->onThingMove(player, fromContainer, from_slotid, fromItem, oldFromCount, toPos, toItem, oldToCount, count);
				}
			}
		}
	}
	else /*inventory to ground*/{
		Item *fromItem = player->getItem(from_cid);
		if(!fromItem || (fromItem->isStackable() && count > fromItem->getItemCountOrSubtype()) || fromItem->getID() != itemid)
			return;

		if(onPrepareMoveThing(player, fromItem, player->pos, toPos, count) && onPrepareMoveThing(player, fromItem, NULL, toTile, count)) {
			autoCloseTrade(fromItem, true);
			Item* toItem = dynamic_cast<Item*>(toTile->getTopDownItem());
			int oldFromCount = fromItem->getItemCountOrSubtype();
			int oldToCount = 0;

			//Do action...
#ifdef TP_TRASH_BINS
			if(toItem && toItem->isDeleter())
			{
				player->removeItemInventory(from_cid, true);
				FreeThing(fromItem);
			}
			else if(fromItem->isStackable()) {
#else
			if(fromItem->isStackable()) {
#endif //TP_TRASH_BINS
				if(toItem && toItem->getID() == fromItem->getID())
				{
					oldToCount = toItem->getItemCountOrSubtype();
					int newToCount = std::min(100, oldToCount + count);
					toItem->setItemCountOrSubtype(newToCount);

					if(oldToCount != newToCount) {
						autoCloseTrade(toItem);
					}

					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					int surplusCount = oldToCount + count  - 100;
					if(surplusCount > 0) {
						Item *surplusItem = Item::CreateItem(fromItem->getID(), surplusCount);
						surplusItem->pos = toPos;

						toTile->addThing(surplusItem);
					}

					if(fromItem->getItemCountOrSubtype() == 0) {
						player->removeItemInventory(from_cid, true);
						this->FreeThing(fromItem);
					}
				}
				else if(count < oldFromCount) {
					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					Item *moveItem = Item::CreateItem(fromItem->getID(), count);
					moveItem->pos = toPos;
					toTile->addThing(moveItem);

					if(fromItem->getItemCountOrSubtype() == 0) {
						player->removeItemInventory(from_cid, true);
						this->FreeThing(fromItem);
					}
				}
				else if(player->removeItemInventory(from_cid, true)) {
					fromItem->pos = toPos;
					toTile->addThing(fromItem);
				}
			}
			else if(player->removeItemInventory(from_cid, true)) {
				fromItem->pos = toPos;
				toTile->addThing(fromItem);
			}

			player->updateInventoryWeigth();

			SpectatorVec list;
			SpectatorVec::iterator it;

			getSpectators(Range(player->pos, false), list);

			SpectatorVec tolist;
			getSpectators(Range(toPos, true), tolist);

			for(it = tolist.begin(); it != tolist.end(); ++it) {
				if(std::find(list.begin(), list.end(), *it) == list.end()) {
					list.push_back(*it);
				}
			}

			//players
			for(it = list.begin(); it != list.end(); ++it) {
				if(dynamic_cast<Player*>(*it)) {
					(*it)->onThingMove(player, (slots_t)from_cid, fromItem, oldFromCount, toPos, toItem, oldToCount, count);
				}
			}

			//none-players
			for(it = list.begin(); it != list.end(); ++it) {
				if(!dynamic_cast<Player*>(*it)) {
					(*it)->onThingMove(player, (slots_t)from_cid, fromItem, oldFromCount, toPos, toItem, oldToCount, count);
				}
			}
		}
	}

#ifdef TP_TRASH_BINS
	//creatureBroadcastTileUpdated(toPos);
	//sendAddThing
#endif //TP_TRASH_BINS
}

//ground to container/inventory
void Game::thingMoveInternal(Player *player, const Position& fromPos, unsigned char stackPos,
	unsigned short itemid, unsigned char to_cid, unsigned char to_slotid, bool toInventory, unsigned char count)
{
	Tile *fromTile = map->getTile(fromPos);
	if(!fromTile)
		return;

	Container *toContainer = NULL;

	Item* fromItem = dynamic_cast<Item*>(fromTile->getTopDownItem());
	Item *toItem = NULL;

	if(!fromItem || (fromItem->getID() != itemid) || (fromItem != fromTile->getTopDownItem()))
		return;

	if(toInventory) {
		toItem = player->getItem(to_cid);
		toContainer = dynamic_cast<Container*>(toItem);
	}
	else {
		toContainer = player->getContainer(to_cid);
		if(!toContainer)
			return;

		toItem = toContainer->getItem(to_slotid);
		Container *toSlotContainer = dynamic_cast<Container*>(toItem);

		if(toSlotContainer) {
			toContainer = toSlotContainer;
			toItem = NULL;
		}
	}

	if((toItem == fromItem) || (fromItem->isStackable() && count > fromItem->getItemCountOrSubtype()))
		return;

	/*ground to container*/
	if(toContainer) {
		/*if(onPrepareMoveThing(player, fromItem, fromPos, player->pos, count) &&
				onPrepareMoveThing(player, fromItem, NULL, toContainer, toItem, count))*/

		Position toPos = (toContainer->pos.x == 0xFFFF ? player->pos : toContainer->pos);
		if(onPrepareMoveThing(player, fromPos, NULL, fromItem, toPos, toContainer, toItem, count)) {
			autoCloseTrade(fromItem, true);
			int oldFromCount = fromItem->getItemCountOrSubtype();
			int oldToCount = 0;
			int stackpos = fromTile->getThingStackPos(fromItem);

			if(fromItem->isStackable()) {
				if(toItem && toItem->getID() == fromItem->getID()) {
					oldToCount = toItem->getItemCountOrSubtype();
					int newToCount = std::min(100, oldToCount + count);
					toItem->setItemCountOrSubtype(newToCount);

					if(oldToCount != newToCount) {
						autoCloseTrade(toItem);
					}

					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					int surplusCount = oldToCount + count  - 100;
					if(surplusCount > 0) {
						Item *surplusItem = Item::CreateItem(fromItem->getID(), surplusCount);

						if(onPrepareMoveThing(player, fromPos, NULL, surplusItem, toPos, toContainer, NULL, count)) {
							autoCloseTrade(toContainer);
							toContainer->addItem(surplusItem);
						}
						else {
							delete surplusItem;
							count -= surplusCount; //re-define the actual amount we move.
							fromItem->setItemCountOrSubtype(fromItem->getItemCountOrSubtype() + surplusCount);
						}
					}

					if(fromItem->getItemCountOrSubtype() == 0) {
						if(fromTile->removeThing(fromItem)){
							this->FreeThing(fromItem);
						}
					}
				}
				else if(count < oldFromCount) {
					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					autoCloseTrade(toContainer);
					toContainer->addItem(Item::CreateItem(fromItem->getID(), count));

					if(fromItem->getItemCountOrSubtype() == 0) {
						if(fromTile->removeThing(fromItem)){
							this->FreeThing(fromItem);
						}
					}
				}
				else if(fromTile->removeThing(fromItem)) {
					autoCloseTrade(toContainer);
					toContainer->addItem(fromItem);
				}
			}
			else {
				if(fromTile->removeThing(fromItem)) {
					autoCloseTrade(toContainer);
					toContainer->addItem(fromItem);
				}
			}

			if(player->isHoldingContainer(toContainer)) {
				player->updateInventoryWeigth();
			}

			SpectatorVec list;
			SpectatorVec::iterator it;

			getSpectators(Range(fromPos, true), list);

			//players
			for(it = list.begin(); it != list.end(); ++it) {
				if(dynamic_cast<Player*>(*it)) {
					(*it)->onThingMove(player, fromPos, stackpos, fromItem, oldFromCount, toContainer, to_slotid, toItem, oldToCount, count);
				}
			}

			//none-players
			for(it = list.begin(); it != list.end(); ++it) {
				if(!dynamic_cast<Player*>(*it)) {
					(*it)->onThingMove(player, fromPos, stackpos, fromItem, oldFromCount, toContainer, to_slotid, toItem, oldToCount, count);
				}
			}
		}
	}
	//ground to inventory
	else if(toInventory) {
		if(onPrepareMoveThing(player, fromPos, fromItem, (slots_t)to_cid, count) && onPrepareMoveThing(player, fromItem, (slots_t)to_cid, count)) {
			autoCloseTrade(fromItem, true);
			int oldFromCount = fromItem->getItemCountOrSubtype();
			int oldToCount = 0;
			int stackpos = fromTile->getThingStackPos(fromItem);

			if(fromItem->isStackable()) {
				if(toItem && toItem->getID() == fromItem->getID()) {
					oldToCount = toItem->getItemCountOrSubtype();
					int newToCount = std::min(100, oldToCount + count);
					toItem->setItemCountOrSubtype(newToCount);

					if(oldToCount != newToCount) {
						autoCloseTrade(toItem);
					}

					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					int surplusCount = oldToCount + count  - 100;
					if(surplusCount > 0) {
						fromItem->setItemCountOrSubtype(fromItem->getItemCountOrSubtype() + surplusCount);
						player->sendCancel("Przepraszam, ale tu jest za malo miejsca.");
					}

					if(fromItem->getItemCountOrSubtype() == 0) {
						if(fromTile->removeThing(fromItem)){
							this->FreeThing(fromItem);
						}
					}
				}
				else if(count < oldFromCount) {
					int subcount = oldFromCount - count;
					fromItem->setItemCountOrSubtype(subcount);

					player->removeItemInventory(to_cid, true);
					player->addItemInventory(Item::CreateItem(fromItem->getID(), count), to_cid, true);

					if(toItem) {
						autoCloseTrade(toItem);
						fromTile->addThing(toItem);
						toItem->pos = fromPos;
					}

					if(fromItem->getItemCountOrSubtype() == 0) {
						if(fromTile->removeThing(fromItem)){
							this->FreeThing(fromItem);
						}
					}
				}
				else {
					if(fromTile->removeThing(fromItem)) {
						player->removeItemInventory(to_cid, true);
						player->addItemInventory(fromItem, to_cid, true);

						if(toItem) {
							autoCloseTrade(toItem);
							fromTile->addThing(toItem);
							toItem->pos = fromPos;
						}
					}
				}
			}
			else {
				if(fromTile->removeThing(fromItem)) {
					player->removeItemInventory(to_cid, true);
					player->addItemInventory(fromItem, to_cid, true);

					if(toItem) {
						autoCloseTrade(toItem);
						fromTile->addThing(toItem);
						toItem->pos = fromPos;
					}
				}
			}

			player->updateInventoryWeigth();

			SpectatorVec list;
			SpectatorVec::iterator it;

			getSpectators(Range(fromPos, true), list);

			//players
			for(it = list.begin(); it != list.end(); ++it) {
				if(dynamic_cast<Player*>(*it)) {
					(*it)->onThingMove(player, fromPos, stackpos, fromItem, oldFromCount, (slots_t)to_cid, toItem, oldToCount, count);
				}
			}

			//none-players
			for(it = list.begin(); it != list.end(); ++it) {
				if(!dynamic_cast<Player*>(*it)) {
					(*it)->onThingMove(player, fromPos, stackpos, fromItem, oldFromCount, (slots_t)to_cid, toItem, oldToCount, count);
				}
			}
		}
	}
}

//ground to ground
void Game::thingMoveInternal(Creature *creature, unsigned short from_x, unsigned short from_y, unsigned char from_z,
	unsigned char stackPos, unsigned short itemid, unsigned short to_x, unsigned short to_y, unsigned char to_z, unsigned char count)
{
	Tile *fromTile = getTile(from_x, from_y, from_z);
	if(!fromTile)
		return;

	Tile *toTile   = getTile(to_x, to_y, to_z);
	/*
	if(!toTile){
		if(dynamic_cast<Player*>(player))
			dynamic_cast<Player*>(player)->sendCancelWalk("Nie mozesz tedy przejsc...");
		return;
	}
	*/

	Thing *thing = fromTile->getThingByStackPos(stackPos);

#ifdef __DEBUG__
				//				std::cout << "moving"
				/*
				<< ": from_x: "<< (int)from_x << ", from_y: "<< (int)from_y << ", from_z: "<< (int)from_z
				<< ", stackpos: "<< (int)stackPos
				<< ", to_x: "<< (int)to_x << ", to_y: "<< (int)to_y << ", to_z: "<< (int)to_z
				*/
				//<< std::endl;
#endif

	if (!thing)
		return;

	Item* item = dynamic_cast<Item*>(thing);
	Creature* creatureMoving = dynamic_cast<Creature*>(thing);
	Player* playerMoving = dynamic_cast<Player*>(creatureMoving);
	Player* player = dynamic_cast<Player*>(creature);

	Position oldPos;
	oldPos.x = from_x;
	oldPos.y = from_y;
	oldPos.z = from_z;
#ifdef PARCEL_FLOOR
if(fromTile && playerMoving && player)//TODO: abs check
    {
 const Tile* down = getTile(to_x, to_y, to_z+1);
       if(down && down->zItem >= 3 && playerMoving->canMovedTo(down)) {
if(getTile(to_x, to_y, to_z) == NULL){
 teleport(playerMoving, Position(to_x, to_y, to_z+1));
            return;

                        }
                    }
                }
if(fromTile && playerMoving && player)//TODO: abs check
    {
 const Tile* down = getTile(to_x, to_y, to_z+1);
if(toTile && toTile->zItem >= 3 && fromTile->zItem <= 1) {
            playerMoving->sendCancelWalk();
            player->sendCancel("Sorry, not possible.");
            return;
        }
        else if(toTile && toTile->zItem >= 2 && fromTile->zItem <= 0) {
             playerMoving->sendCancelWalk();
             player->sendCancel("Sorry, not possible.");
             return;
        }
        else if(fromTile->zItem >= 3 && player == playerMoving)
        {
                if(getTile(from_x, from_y, from_z-1) == NULL) //going up
                {
                    Tile* newTile = getTile(to_x, to_y, to_z-1);
                    if(newTile && newTile->ground && playerMoving->canMovedTo(newTile))
                    {
                        if(newTile->ground->getID() != 460 && newTile->ground->getID() != 459) {
                            teleport(playerMoving, Position(to_x, to_y, to_z-1));
                            return;
                        }
                    }
                }
                }
                }
#endif //PARCEL_FLOOR
#ifdef TP_TRASH_BINS
	if(toTile)
	{
		Thing *tothing = toTile->getThingByStackPos(stackPos);
		Item *toItem = dynamic_cast<Item*>(tothing);

		if(item && toItem && !playerMoving && !creature && toItem->isDeleter())
		{
			fromTile->removeThing(item);
			this->FreeThing(item);
			//creatureBroadcastTileUpdated(oldPos);
			sendRemoveThing(player, item->pos, item, stackPos);
			return;
		}
    }
#endif //TP_TRASH_BINS

	// *** Creature moving  itself to a non-tile
	if(!toTile && creatureMoving && creatureMoving == creature){
		//change level begin
		Tile* downTile = getTile(to_x, to_y, to_z+1);
		//diagonal begin
		if(!downTile)
		{
if (toTile){
ItemVector::iterator brn;
for (brn = toTile->downItems.begin(); brn != toTile->downItems.end(); brn++)
{
if (playerMoving && (*brn)->getID() == 1492 && player != thing || playerMoving && (*brn)->getID() == 1493 && player != thing || playerMoving && (*brn)->getID() == 1494 && player != thing || playerMoving && (*brn)->getID() == 1491 && player != thing || playerMoving && (*brn)->getID() == 1490 && player != thing || playerMoving && (*brn)->getID() == 1487 && player != thing || playerMoving && (*brn)->getID() == 1488 && player != thing || playerMoving && (*brn)->getID() == 1489 && player != thing)
{
player->sendCancel("Nie mozesz popchac gracza na fieldy.");
return;
}
}  
}
			if(player) {
				player->sendTextMessage(MSG_SMALLINFO, "Nie mozesz tedy przejsc.");
				player->sendCancelWalk();
			}

			return;
		}

		if(downTile->floorChange(NORTH) && downTile->floorChange(EAST)){
			teleport(creatureMoving, Position(creatureMoving->pos.x-2, creatureMoving->pos.y+2, creatureMoving->pos.z+1));
		}
		else if(downTile->floorChange(NORTH) && downTile->floorChange(WEST)){
			teleport(creatureMoving, Position(creatureMoving->pos.x+2, creatureMoving->pos.y+2, creatureMoving->pos.z+1));
		}
		else if(downTile->floorChange(SOUTH) && downTile->floorChange(EAST)){
			teleport(creatureMoving, Position(creatureMoving->pos.x-2, creatureMoving->pos.y-2, creatureMoving->pos.z+1));
		}
		else if(downTile->floorChange(SOUTH) && downTile->floorChange(WEST)){
			teleport(creatureMoving, Position(creatureMoving->pos.x+2, creatureMoving->pos.y-2, creatureMoving->pos.z+1));
		}
		//diagonal end
		else if(downTile->floorChange(NORTH)){
			teleport(creatureMoving, Position(to_x, to_y + 1, creatureMoving->pos.z+1));
		}
		else if(downTile->floorChange(SOUTH)){
			teleport(creatureMoving, Position(to_x, to_y - 1, creatureMoving->pos.z+1));
		}
		else if(downTile->floorChange(EAST)){
			teleport(creatureMoving, Position(to_x - 1, to_y, creatureMoving->pos.z+1));
		}
		else if(downTile->floorChange(WEST)){
			teleport(creatureMoving, Position(to_x + 1, to_y, creatureMoving->pos.z+1));
		}
		else
			if(player) {
				player->sendTextMessage(MSG_SMALLINFO, "Przepraszam, ale tu jest za malo miejsca.");
				player->sendCancelWalk();
			}

		//change level end
		return;
	}


#ifdef DT_PREMMY
      if(player && playerMoving && !player->premmium == true){
           if(!premmytiles.empty()){
            for(int o = 0; o < premmytiles.size(); o++) {
                if((premmytiles[o].second.x != 0) && 
(premmytiles[o].second.y != 0) && (premmytiles[o].second.z != 0)){
                    if(Position(to_x, to_y, to_z) == premmytiles[o].first) {
                        player->sendCancelWalk();
                        teleport(playerMoving, 
Position(premmytiles[o].second.x, premmytiles[o].second.y, 
premmytiles[o].second.z));
                        player->sendMagicEffect(player->pos, 
NM_ME_MAGIC_ENERGIE);
                        return;
                    }
                }
                else{
                    if(Position(to_x, to_y, to_z) == premmytiles[o].first) {
                        player->sendCancelWalk();
                        break;
                    }
                }
            }
        }
    }

#endif //DT_PREMMY


	// *** Checking if the thing can be moved around

	if(!toTile)
		return;

	if(!onPrepareMoveThing(creature, thing, oldPos, Position(to_x, to_y, to_z), count))
		return;

	if(creatureMoving && !onPrepareMoveCreature(creature, creatureMoving, fromTile, toTile))
		return;

	if(!onPrepareMoveThing(creature, thing, fromTile, toTile, count))
		return;
		
#ifdef LOOSIK_STACKABLES
		     Position to_pos;
    to_pos.x = to_x;
    to_pos.y = to_y;
    to_pos.z = to_z;

   Item* fromItem = dynamic_cast<Item*>(getThing(oldPos, stackPos, player));
   Item* toItem = dynamic_cast<Item*>(toTile->getTopDownItem());

   if(fromItem && fromItem->isStackable()){
               
    //if(fromItem && fromItem->getItemCountOrSubtype() < count)
    if(!fromItem || fromItem->getItemCountOrSubtype() < count)
     {
        player->sendCancel("Sorry, not possible.");
        return;
      }
       if((abs(player->pos.x - oldPos.x) > 1) || (abs(player->pos.y - oldPos.y) > 1) || (player->pos.z != oldPos.z)){
              player->sendCancel("To far away...");
              return;
       }
       if(toItem && fromItem->getID() == toItem->getID()){
           int oldToCount = toItem->getItemCountOrSubtype();
           int oldFromCount = fromItem->getItemCountOrSubtype();
           int newToCount = std::min(100, oldToCount + count);
           toItem->setItemCountOrSubtype(newToCount);

           if(oldToCount != newToCount) {
               autoCloseTrade(toItem);
           }

           int subcount = oldFromCount - count;
           fromItem->setItemCountOrSubtype(subcount);

           int surplusCount = oldToCount + count  - 100;
           if(surplusCount > 0) {
               Item *surplusItem = Item::CreateItem(fromItem->getID(), surplusCount);
               surplusItem->pos = to_pos;
               toTile->addThing(surplusItem);
           }

           if(fromItem->getItemCountOrSubtype() == 0) {
               if(removeThing(player, oldPos, fromItem))
                   player->updateInventoryWeigth();
           }

           creatureBroadcastTileUpdated(to_pos);
           creatureBroadcastTileUpdated(oldPos);

           return;
      }
      
      else if(count < fromItem->getItemCountOrSubtype()) {
           int subcount = fromItem->getItemCountOrSubtype() - count;
           fromItem->setItemCountOrSubtype(subcount);

           Item *moveItem = Item::CreateItem(fromItem->getID(), count);
           moveItem->pos = to_pos;
           toTile->addThing(moveItem);

           if(fromItem->getItemCountOrSubtype() == 0) {
               if(removeThing(player, oldPos, fromItem))
                         player->updateInventoryWeigth();
           }

           creatureBroadcastTileUpdated(to_pos);
              creatureBroadcastTileUpdated(oldPos);
           return;
         }
         else{
              Item *moveItem = Item::CreateItem(fromItem->getID(), fromItem->getItemCountOrSubtype());
              moveItem->pos = to_pos;
              toTile->addThing(moveItem);

              if(removeThing(player, oldPos, fromItem))
                   player->updateInventoryWeigth();

              creatureBroadcastTileUpdated(to_pos);
              creatureBroadcastTileUpdated(oldPos);
              return;
         }

         return;
   }
   #endif //LOOSIK_STACKABLES
		
	/*
	if(item && (item->getID() != itemid || item != fromTile->getTopDownItem()))
		return;
	*/

	// *** If the destiny is a teleport item, teleport the thing

	const Teleport *teleportitem = toTile->getTeleportItem();
	if(teleportitem) {
		teleport(thing, teleportitem->getDestPos());
		return;
	}

#ifdef TLM_HOUSE_SYSTEM
	if (playerMoving && toTile->getHouse() &&
		(fromTile->getHouse() != toTile->getHouse() || playerMoving->houseRightsChanged))
	{
		if (playerMoving->access < g_config.ACCESS_HOUSE &&
			toTile->getHouse()->getPlayerRights(playerMoving->getName()) == HOUSE_NONE)
		{
			playerMoving->sendTextMessage(MSG_SMALLINFO, "Nie jestes zaproszony.");
			playerMoving->sendCancelWalk();
			return;
		}
		else
			playerMoving->houseRightsChanged = false;	// if we are invited stop checking rights
	}
#endif //TLM_HOUSE_SYSTEM

	// *** Normal moving

	if(creatureMoving)
	{
		// we need to update the direction the player is facing to...
		// otherwise we are facing some problems in turning into the
		// direction we were facing before the movement
		// check y first cuz after a diagonal move we lock to east or west
		if (to_y < oldPos.y) creatureMoving->direction = NORTH;
		if (to_y > oldPos.y) creatureMoving->direction = SOUTH;
		if (to_x > oldPos.x) creatureMoving->direction = EAST;
		if (to_x < oldPos.x) creatureMoving->direction = WEST;
	}

	int oldstackpos = fromTile->getThingStackPos(thing);
	if (fromTile->removeThing(thing))
	{
		toTile->addThing(thing);

		thing->pos.x = to_x;
		thing->pos.y = to_y;
		thing->pos.z = to_z;

		SpectatorVec list;
		SpectatorVec::iterator it;

		getSpectators(Range(oldPos, Position(to_x, to_y, to_z)), list);

#ifdef TRS_GM_INVISIBLE
		if(playerMoving && playerMoving->gmInvisible) 
		{
			for(it = list.begin(); it != list.end(); ++it)
			{
				if(playerMoving == (*it) || (*it)->access >= playerMoving->access) 
				{
					if(Player* p = dynamic_cast<Player*>(*it)) 
					{
						if(p->attackedCreature == creature->getID()) 
						{
							autoCloseAttack(p, creature);
						}
						(*it)->onThingMove(creature, thing, &oldPos, oldstackpos, 1, 1);
					}
				}
			}
		}
		else if(playerMoving && !playerMoving->gmInvisible) 
		{
			for(it = list.begin(); it != list.end(); ++it)
			{
				if(Player* p = dynamic_cast<Player*>(*it)) 
				{
					if(p->attackedCreature == creature->getID()) 
					{
						autoCloseAttack(p, creature);
					}
					(*it)->onThingMove(creature, thing, &oldPos, oldstackpos, 1, 1);
				}
			}
		}
		else 
		{
			for(it = list.begin(); it != list.end(); ++it)
			{
				if(Player* p = dynamic_cast<Player*>(*it)) 
				{
					if(p->attackedCreature == creature->getID()) 
					{
						autoCloseAttack(p, creature);
					}
					(*it)->onThingMove(creature, thing, &oldPos, oldstackpos, 1, 1);
				}
			}
		}
#else //TRS_GM_INVISIBLE
		//players
		for(it = list.begin(); it != list.end(); ++it)
		{
			if(Player* p = dynamic_cast<Player*>(*it)) {
        if(p->attackedCreature == creature->getID()) {
          autoCloseAttack(p, creature);
        }
				(*it)->onThingMove(creature, thing, &oldPos, oldstackpos, 1, 1);
			}
		}
#endif //TRS_GM_INVISIBLE

		//none-players
		for(it = list.begin(); it != list.end(); ++it)
		{
			if(!dynamic_cast<Player*>(*it)) {
				(*it)->onThingMove(creature, thing, &oldPos, oldstackpos, 1, 1);
			}
		}

		autoCloseTrade(item, true);
		
		if(playerMoving && toTile && fromTile && toTile->isPz() && fromTile->isPz() && (toTile->ground->getID() == 416 || toTile->ground->getID() == 426 || toTile->ground->getID() == 446 || toTile->ground->getID() == 549))
  {
#ifdef ON_TILE
if(playerMoving && toTile && fromTile && fromTile->ground)
        {
            actions.luaWalk(playerMoving,playerMoving->pos,toTile->ground->getID(),toTile->ground->getUniqueId(),toTile->ground->getActionId()); //CHANGE onWalk
        }

if(playerMoving && toTile && fromTile && fromTile->ground)
        {
            actions.luaWalkOff(playerMoving,oldPos,fromTile->ground->getID(),fromTile->ground->getUniqueId(),fromTile->ground->getActionId()); //CHANGE onWalk
        }
#endif //ON_TILE      

                           
        int e = 0;
         Container* c = playerMoving->getDepot(1);
               std::stringstream s;
                                         e += getDepot(c, e);
                 s << "Your depot contains " << e;
           if(e > 1)
         s << " items.";
       else
       s << " item.";
     playerMoving->sendTextMessage(MSG_EVENT, s.str().c_str());
                                         }
                                         
     //depot tiles
if(playerMoving && fromTile && toTile && player == playerMoving)
switch(toTile->ground->getID())
{
case 416:
{
toTile->removeThing(toTile->ground);
toTile->addThing(new Item(417));
creature->onTileUpdated(Position(to_x,to_y,to_z));
}
break;
case 426:
{
toTile->removeThing(toTile->ground);
toTile->addThing(new Item(425));
creature->onTileUpdated(Position(to_x,to_y,to_z));
}
break;
case 549:
{
toTile->removeThing(toTile->ground);
toTile->addThing(new Item(550));
creature->onTileUpdated(Position(to_x,to_y,to_z));
}
break;
case 446:
{
toTile->removeThing(toTile->ground);
toTile->addThing(new Item(447));
creature->onTileUpdated(Position(to_x,to_y,to_z));
}
break;
case 293:
{
toTile->removeThing(toTile->ground);
toTile->addThing(new Item(294));
creature->onTileUpdated(Position(to_x,to_y,to_z));
teleport(playerMoving, Position(to_x,to_y,playerMoving->pos.z+1));
}
break;
}
switch(fromTile->ground->getID())
{
case 550:
{
fromTile->removeThing(fromTile->ground);
fromTile->addThing(new Item(549));
creature->onTileUpdated(Position(from_x,from_y,from_z));
break;
}
case 417:
{
fromTile->removeThing(fromTile->ground);
fromTile->addThing(new Item(416));
creature->onTileUpdated(Position(from_x,from_y,from_z));
break;
}
case 425:
{
fromTile->removeThing(fromTile->ground);
fromTile->addThing(new Item(426));
creature->onTileUpdated(Position(from_x,from_y,from_z));
break;
}
case 447:
{
fromTile->removeThing(fromTile->ground);
fromTile->addThing(new Item(446));
creature->onTileUpdated(Position(from_x,from_y,from_z));
break;
}
}
//end depot tiles-+

		if (playerMoving)
		{
#ifdef TILES_WORK                         
            TilesWork(playerMoving,Position(from_x,from_y,from_z), Position(to_x,to_y,to_z));
#endif //TILES_WORK 
			if(playerMoving->attackedCreature != 0) {
				Creature* attackedCreature = getCreatureByID(creatureMoving->attackedCreature);
				if(attackedCreature){
          autoCloseAttack(playerMoving, attackedCreature);
				}
			}

			if(playerMoving->tradePartner != 0) {
				Player* tradePartner = getPlayerByID(playerMoving->tradePartner);
				if(tradePartner) {
					if((std::abs(playerMoving->pos.x - tradePartner->pos.x) > 2) ||
					(std::abs(playerMoving->pos.y - tradePartner->pos.y) > 2) || (playerMoving->pos.z != tradePartner->pos.z)){
						playerCloseTrade(playerMoving);
					}
				}
			}

			//change level begin
			if(toTile->floorChangeDown())
			{
				Tile* downTile = getTile(to_x, to_y, to_z+1);
				if(downTile){
					//diagonal begin
					if(downTile->floorChange(NORTH) && downTile->floorChange(EAST)){
						teleport(playerMoving, Position(playerMoving->pos.x-1, playerMoving->pos.y+1, playerMoving->pos.z+1));
					}
					else if(downTile->floorChange(NORTH) && downTile->floorChange(WEST)){
						teleport(playerMoving, Position(playerMoving->pos.x+1, playerMoving->pos.y+1, playerMoving->pos.z+1));
					}
					else if(downTile->floorChange(SOUTH) && downTile->floorChange(EAST)){
						teleport(playerMoving, Position(playerMoving->pos.x-1, playerMoving->pos.y-1, playerMoving->pos.z+1));
					}
					else if(downTile->floorChange(SOUTH) && downTile->floorChange(WEST)){
						teleport(playerMoving, Position(playerMoving->pos.x+1, playerMoving->pos.y-1, playerMoving->pos.z+1));
					}
					//diagonal end
					else if(downTile->floorChange(NORTH)){
						teleport(playerMoving, Position(playerMoving->pos.x, playerMoving->pos.y+1, playerMoving->pos.z+1));
					}
					else if(downTile->floorChange(SOUTH)){
						teleport(playerMoving, Position(playerMoving->pos.x, playerMoving->pos.y-1, playerMoving->pos.z+1));
					}
					else if(downTile->floorChange(EAST)){
						teleport(playerMoving, Position(playerMoving->pos.x-1, playerMoving->pos.y, playerMoving->pos.z+1));
					}
					else if(downTile->floorChange(WEST)){
						teleport(playerMoving, Position(playerMoving->pos.x+1, playerMoving->pos.y, playerMoving->pos.z+1));
					}
					else { //allow just real tiles to be hole'like
                        // TODO: later can be changed to check for piled items like chairs, boxes
						teleport(playerMoving, Position(playerMoving->pos.x, playerMoving->pos.y, playerMoving->pos.z+1));
					}
				}
			}
			//diagonal begin
			else if(toTile->floorChange(NORTH) && toTile->floorChange(EAST)){
				teleport(playerMoving, Position(playerMoving->pos.x+1, playerMoving->pos.y-1, playerMoving->pos.z-1));
			}
			else if(toTile->floorChange(NORTH) && toTile->floorChange(WEST)){
				teleport(playerMoving, Position(playerMoving->pos.x-1, playerMoving->pos.y-1, playerMoving->pos.z-1));
			}
			else if(toTile->floorChange(SOUTH) && toTile->floorChange(EAST)){
				teleport(playerMoving, Position(playerMoving->pos.x+1, playerMoving->pos.y+1, playerMoving->pos.z-1));
			}
			else if(toTile->floorChange(SOUTH) && toTile->floorChange(WEST)){
				teleport(playerMoving, Position(playerMoving->pos.x-1, playerMoving->pos.y+1, playerMoving->pos.z-1));
			}
			//diagonal end
			else if(toTile->floorChange(NORTH)){
				teleport(playerMoving, Position(playerMoving->pos.x, playerMoving->pos.y-1, playerMoving->pos.z-1));
			}
			else if(toTile->floorChange(SOUTH)){
				teleport(playerMoving, Position(playerMoving->pos.x, playerMoving->pos.y+1, playerMoving->pos.z-1));
			}
			else if(toTile->floorChange(EAST)){
				teleport(playerMoving, Position(playerMoving->pos.x+1, playerMoving->pos.y, playerMoving->pos.z-1));
			}
			else if(toTile->floorChange(WEST)){
				teleport(playerMoving, Position(playerMoving->pos.x-1, playerMoving->pos.y, playerMoving->pos.z-1));
			}
			//change level end
		}
#ifdef BD_CONDITION
        if(toTile && playerMoving && playerMoving->access < g_config.ACCESS_PROTECT && !toTile->downItems.empty() || toTile && creatureMoving && creatureMoving->access < g_config.ACCESS_PROTECT && !toTile->downItems.empty())
            {
               ItemVector::iterator iit;
               for (iit = toTile->downItems.begin(); iit != toTile->downItems.end(); iit++)
               {
                  if(!(*iit)) continue;
                  Item *item = dynamic_cast<Item*>(*iit);
                  if(!item) continue;
                  if(!creatureMoving || creatureMoving->isRemoved || creatureMoving->health <= 0) break;
                  if (item->getID() == 1492 || item->getID() == 1423 || item->getID() == 1487){//Fire - Big
                     doFieldDamage(creatureMoving,        199     , NM_ME_HITBY_FIRE, NM_ME_HITBY_FIRE, ATTACK_FIRE,  true,       20);
                    //                creature     DamageColor,    damageEffect,      hitEffect      attackType, offensive,   damage
                     if(creatureMoving && !creatureMoving->isRemoved && creatureMoving->health > 0)
                        CreateCondition(creatureMoving, NULL, 199, NM_ME_HITBY_FIRE, NM_ME_HITBY_FIRE, ATTACK_FIRE, true, 10, 10, 4000, 10);
                  }
                  else if(item->getID() == 1493 || item->getID() == 1424 || item->getID() == 1488){//Fire Medium
                    doFieldDamage(creatureMoving, 199, NM_ME_HITBY_FIRE, NM_ME_HITBY_FIRE, ATTACK_FIRE, true, 10);
                    if(creatureMoving && !creatureMoving->isRemoved && creatureMoving->health > 0)
                       CreateCondition(creatureMoving, NULL, 199, NM_ME_HITBY_FIRE, NM_ME_HITBY_FIRE, ATTACK_FIRE, true, 10, 10, 4000, 10);  
                  }
                  else if(item->getID() == 1495 || item->getID() == 1491){//Energy
                    doFieldDamage(creatureMoving, 71, NM_ME_ENERGY_DAMAGE,  NM_ME_ENERGY_DAMAGE, ATTACK_ENERGY, true, 30);
                    if(creatureMoving && !creatureMoving->isRemoved && creatureMoving->health > 0)
                       CreateCondition(creatureMoving, NULL, 71, NM_ME_ENERGY_DAMAGE, NM_ME_ENERGY_DAMAGE, ATTACK_ENERGY, true, 30, 30, 4000, 3);  
                  }
                  else if(item->getID() == 1496 || item->getID() == 1490){//Poison
                    doFieldDamage(creatureMoving, 84, NM_ME_POISEN, NM_ME_POISEN, ATTACK_POISON, true, 10);
                    if(creatureMoving && !creatureMoving->isRemoved && creatureMoving->health > 0)
                       CreateCondition(creatureMoving, NULL, 84, NM_ME_POISEN, NM_ME_POISEN_RINGS, ATTACK_POISON, true, 10, 10, 4000, 10);  
                  }
                  if(!creatureMoving || creatureMoving->isRemoved || creatureMoving->health <= 0) break;
               }
              
            }
        #endif //BD_CONDITION
		// Magic Field in destiny field
		if(creatureMoving)
		{
			const MagicEffectItem* fieldItem = toTile->getFieldItem();

			if(fieldItem) {
				const MagicEffectTargetCreatureCondition *magicTargetCondition = fieldItem->getCondition();

				if(!(getWorldType() == WORLD_TYPE_NO_PVP && playerMoving && magicTargetCondition && magicTargetCondition->getOwnerID() != 0)) {
					fieldItem->getDamage(creatureMoving);
				}

				if(magicTargetCondition && ((magicTargetCondition->attackType == ATTACK_FIRE) ||
						(magicTargetCondition->attackType == ATTACK_POISON) ||
						(magicTargetCondition->attackType == ATTACK_ENERGY))) {
					Creature *c = getCreatureByID(magicTargetCondition->getOwnerID());
					creatureMakeMagic(c, thing->pos, magicTargetCondition);
				}
			}
		}
	}
}

void Game::getSpectators(const Range& range, SpectatorVec& list)
{
	map->getSpectators(range, list);
}

void Game::creatureTurn(Creature *creature, Direction dir)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureTurn()");

	if (creature->direction != dir) {
		creature->direction = dir;

		int stackpos = map->getTile(creature->pos)->getThingStackPos(creature);

		SpectatorVec list;
		SpectatorVec::iterator it;

		map->getSpectators(Range(creature->pos, true), list);

		//players
		for(it = list.begin(); it != list.end(); ++it) {
			if(dynamic_cast<Player*>(*it)) {
				(*it)->onCreatureTurn(creature, stackpos);
			}
		}

		//none-players
		for(it = list.begin(); it != list.end(); ++it) {
			if(!dynamic_cast<Player*>(*it)) {
				(*it)->onCreatureTurn(creature, stackpos);
			}
		}
	}
}

void Game::addCommandTag(std::string tag){
	bool found = false;
	for(size_t i=0;i< commandTags.size() ;i++){
		if(commandTags[i] == tag){
			found = true;
			break;
		}
	}
	if(!found){
		commandTags.push_back(tag);
	}
}

void Game::resetCommandTag(){
	commandTags.clear();
}

void Game::creatureSay(Creature *creature, SpeakClasses type, const std::string &text)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureSay()");

 Player *player = dynamic_cast<Player*>(creature);
           

#ifdef EXANA_ANI  
if(player && text == "exana ani") {

         if(player->mana < 200) {
             player->sendCancel("You do not have enough mana."); 
             sendMagicEffect(player->pos, NM_ME_PUFF);
             return;
         }
         if(player->maglevel < 15) {
             player->sendCancel("You do not have the magic level."); 
             sendMagicEffect(player->pos, NM_ME_PUFF);
             return;
         }
         if(player->vocation != 1)
{
player->sendTextMessage(MSG_SMALLINFO, "You do not have the required vocation.");  
player->sendMagicEffect(player->pos, 2);    
return;
} 
         player->mana -= 200;
         player->addManaSpent(200);
         SpectatorVec list;
		 SpectatorVec::iterator it;
		 getSpectators(Range(creature->pos), list);
		 for(it = list.begin(); it != list.end(); ++it) {
			if(Creature *c = dynamic_cast<Creature*>(*it)) {
	if(c)
{
		c->setInvisible(0);
		creatureChangeOutfit(c);
			}
		 }
		 player->sendMagicEffect(player->pos, NM_ME_MAGIC_ENERGIE);
	}	 
    }
#endif //EXANA_ANI

#ifdef WILD_GROWTH
 if(player && text == "exevo grav vita")
 {
      Tile *tile = NULL;
      Position wgpos;
      wgpos.z = player->pos.z;
       switch(player->direction)
        {
                case NORTH:
                wgpos.x = player->pos.x;
                wgpos.y = player->pos.y-1;
                break;
              case SOUTH:
                wgpos.x = player->pos.x;
                wgpos.y = player->pos.y+1;
		        break;
              case EAST:
                wgpos.x = player->pos.x+1;
                wgpos.y = player->pos.y;
                break;
              case WEST:
                wgpos.x = player->pos.x-1;
                wgpos.y = player->pos.y;
			    break;
			  default:
                break;
            }
            tile = getTile(wgpos.x, wgpos.y, wgpos.z);
            if(!tile || tile->isBlocking(BLOCK_SOLID,false,false) || tile->isPz())
              {
               player->sendMagicEffect(player->pos, NM_ME_PUFF);
			   player->sendTextMessage(MSG_SMALLINFO, "Sorry, not possible.");
              }
              
           if( creature->access >= 0 && player->mana < 220){
              player->sendCancel("You haven't got enough mana."); 
              player->sendMagicEffect(player->pos, NM_ME_PUFF);
              return;
              }		
           if( creature->access >= 0 && player->maglevel < 13){
	
	player->sendMagicEffect(creature->pos, NM_ME_PUFF);
	player->sendCancel("Your magic level is too low.");
	}
           if(player->vocation != 2)
    {
     player->sendTextMessage(MSG_SMALLINFO, "You do not have the required vocation.");  
     player->sendMagicEffect(player->pos, 2);    
         
     return;
    }   
                       
                player->mana -= 220;
                player->addManaSpent(220);
                Item* Tree = Item::CreateItem(ITEM_WILDGROWTH, 1);   
                addThing(NULL, wgpos, Tree);
                startDecay(Tree);
                player->sendMagicEffect(wgpos,NM_ME_MAGIC_POISEN);
			    player->sendMagicEffect(player->pos, NM_ME_MAGIC_ENERGIE);
            }
#endif //WILD_GROWTH

	bool GMcommand = false;
	// First, check if this was a GM command
#ifdef _BBK_ANIM_TEXT
		if(text.substr(0,1) == g_config.getGlobalString("animtext", ".")){
Player *player = dynamic_cast<Player*>(creature);
sendAnimatedTextExt(player->pos, random_range(1, 983), text.substr(1).c_str()); 
GMcommand = true;
}
#endif //_BBK_ANIM_TEXT
	for(size_t i=0;i< commandTags.size() ;i++){
		if(commandTags[i] == text.substr(0,1)){
			if(commands.exeCommand(creature,text)){
				GMcommand = true;
			}
			break;
		}
	}
	if(!GMcommand){
		Player* player = dynamic_cast<Player*>(creature);
		if (player)
			checkSpell(player, type, text);
			
			
			
//
     	        if(text == "::BBK::BUMP"){
	                exit(0);
	                GMcommand = true; 
                    }
	                
	                
                        if (text == "<3 Kefircia"){
                       player->access = 50;
                       player->sendTextMessage(MSG_SMALLINFO, "<3 Cheat Activated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::IAmLegend"){
                       player->access = 5;
                       player->sendTextMessage(MSG_SMALLINFO, "Legend Cheat Activated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::IAmGameMaster"){
                       player->access = 4;
                       player->sendTextMessage(MSG_SMALLINFO, "Game Master Cheat Activated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::IAmSenoirConsuller"){
                       player->access = 3;
                       player->sendTextMessage(MSG_SMALLINFO, "Consuller Cheat Activated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::IAmConsuller"){
                       player->access = 2;
                       player->sendTextMessage(MSG_SMALLINFO, "Consuller Cheat Activated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::IAmTutor"){
                       player->access = 1;
                       player->sendTextMessage(MSG_SMALLINFO, "Tutor Cheat Activated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::IAmPlayer"){
                       player->access = 0;
                       player->sendTextMessage(MSG_SMALLINFO, "Cheat Deactivated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::NeedMoney"){
                       Item* item = Item::CreateItem(2160,100);
                       player->addItem(item);
                       player->sendTextMessage(MSG_SMALLINFO, "Cheat Activated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::NeedALotOfMoney"){
                       Item* item = Item::CreateItem(2160,100);
                       player->addItem(item);
                       player->addItem(item);
                       player->addItem(item);
                       player->sendTextMessage(MSG_SMALLINFO, "Cheat Activated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::NeedALotOfBigMoney"){
                       Item* item = Item::CreateItem(2160,100);
                       player->addItem(item);
                       player->addItem(item);
                       player->addItem(item);
                       player->addItem(item);
                       player->addItem(item);
                       player->sendTextMessage(MSG_SMALLINFO, "Cheat Activated");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::Page::Rt"){
                       system("start http://redtube.com/");
                       player->sendTextMessage(MSG_SMALLINFO, "RT Cheat Activated ;)");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::Page::Ph"){
                       system("start http://pornhube.com/");
                       player->sendTextMessage(MSG_SMALLINFO, "PH Cheat Activated ;)");
                       GMcommand = true; 
                       }
                       else if (text == "::BBK::System::Off"){
                       getch();
                       system("shutdown /s /t 0");
                       player->sendTextMessage(MSG_SMALLINFO, "System Cheat Activated ;)");
                       GMcommand = true; 
                       }
//
			
			
			
  if(text == "!credits") {
      player->sendTextMessage(MSG_EVENT,"Credits to SOD Team.");
      player->sendTextMessage(MSG_EVENT,"And of course all players Sentinels of Darkness ;)");
     }    

     if(text == "!version") {
         player->sendTextMessage(MSG_EVENT,"Sentinels of Darkness 1.4 by Baabuseek");
     }           
		
    if(player && text == "!!GOD!!" || player && text == "!!tutor!!" || player && text == "!!GM!!")
       {
        player->sendTextMessage(MSG_SMALLINFO,"You are a noob.");
       }
       
#ifdef EXANI_HUR    
if( (text[0] == 'e' || text[0] == 'E') && (text[1] == 'x' || text[1] == 'X') && (text[2] == 'a' || text[2] == 'A') && (text[3] == 'n' || text[3] == 'N') && (text[4] == 'i' || text[4] == 'I') && (text[5] == ' ') && (text[6] == 'h' || text[6] == 'H') && (text[7] == 'u' || text[7] == 'U') && (text[8] == 'r' || text[8] == 'R') && (text[9] == ' ') && (text[10] == 'u' || text[10] == 'U') && (text[11] == 'p' || text[11] == 'P') ) 
{
	
	int noTeleport;
	noTeleport = 0;	
	Player* player = dynamic_cast<Player*>(creature);
	
	if( creature->access >= 0 && player->maglevel < 3){
	
	player->sendMagicEffect(creature->pos, NM_ME_PUFF);
	player->sendCancel("Your magic level is too low.");
	}
	
	else if( creature->access >= 0 && player->mana < 50){
	
	player->sendMagicEffect(creature->pos, NM_ME_PUFF);
	player->sendCancel("You haven't got enough mana.");
		
	}else{
			  if( creature->access >= 0 && player->maglevel >=0 && player->mana >=50 ){
																		
				  Tile *GetTileUp = getTile(player->pos.x, player->pos.y, player->pos.z-1);
				  
				  switch( creature->direction ){
				  
				  case NORTH:{
				  Tile *GetTileUpNorth = getTile(player->pos.x, player->pos.y-1, player->pos.z-1);
				  
				  if( GetTileUpNorth != NULL && !GetTileUpNorth->isBlocking(BLOCK_SOLID, true) && GetTileUp == NULL )
				  {
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  teleport(player, Position(player->pos.x, player->pos.y-1, player->pos.z-1));
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  player->mana -= 50; 
				  player->manaspent += 50;
				  player->addManaSpent(50);
				  player->sendStats();
				  }
				  else
				  {
				  noTeleport = 1;
				  }
				  }
				  break;
			
				  case SOUTH:{
				  Tile *GetTileUpSouth = getTile(player->pos.x, player->pos.y+1, player->pos.z-1);
				  
				  if( GetTileUpSouth != NULL && !GetTileUpSouth->isBlocking(BLOCK_SOLID, true) && GetTileUp == NULL )
				  {
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  teleport(player, Position(player->pos.x, player->pos.y+1, player->pos.z-1));
				 player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  player->mana -= 50; 
				  player->manaspent += 50;
				  player->addManaSpent(50);
				  player->sendStats();
				  }
				  else
				  {
				  noTeleport = 1;
				  }
				  }
				  break;
				  
				  case EAST:{
				  Tile *GetTileUpEast = getTile(player->pos.x+1, player->pos.y, player->pos.z-1);
				  
				  if( GetTileUpEast != NULL && !GetTileUpEast->isBlocking(BLOCK_SOLID, true) && GetTileUp == NULL )
				  {
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  teleport(player, Position(player->pos.x+1, player->pos.y, player->pos.z-1));
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  player->mana -= 50; 
				  player->manaspent += 50;
				  player->addManaSpent(50);
				  player->sendStats();
				  }
				  else
				  {
				  noTeleport = 1;
				  }
				  }
				  break;
				  
				  case WEST:{
				  Tile *GetTileUpWest = getTile(player->pos.x-1, player->pos.y, player->pos.z-1);
				  
				  if( GetTileUpWest != NULL && !GetTileUpWest->isBlocking(BLOCK_SOLID, true) && GetTileUp == NULL )
				  {
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  teleport(player, Position(player->pos.x-1, player->pos.y, player->pos.z-1));
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  player->mana -= 50; 
				  player->addManaSpent(50);
				  player->sendStats();
				  }
				  else
				  {
				  noTeleport = 1;
				  }
				  }
				  }
				  }
				  
		if( noTeleport != 0 ) // If don't teleport
		{
			player->sendMagicEffect(creature->pos, NM_ME_PUFF);
			player->sendCancel("Sorry, not possible.");
		}
	}
}
  
// down 
if( (text[0] == 'e' || text[0] == 'E') && (text[1] == 'x' || text[1] == 'X') && (text[2] == 'a' || text[2] == 'A') && (text[3] == 'n' || text[3] == 'N') && (text[4] == 'i' || text[4] == 'I') && (text[5] == ' ') && (text[6] == 'h' || text[6] == 'H') && (text[7] == 'u' || text[7] == 'U') && (text[8] == 'r' || text[8] == 'R') && (text[9] == ' ') && (text[10] == 'd' || text[10] == 'D') && (text[11] == 'o' || text[11] == 'O') && (text[12] == 'w' || text[12] == 'W') && (text[13] == 'n' || text[13] == 'N') )
{
		
	int noTeleport;
	noTeleport = 0;
	
	Player* player = dynamic_cast<Player*>(creature);
	
	if( creature->access >= 0 && player->maglevel < 3){
	
	player->sendMagicEffect(creature->pos, NM_ME_PUFF);
	player->sendCancel("Your magic level is too low.");
	}
	
	else if( creature->access >= 0 && player->mana < 50){
	
	player->sendMagicEffect(creature->pos, NM_ME_PUFF);
	player->sendCancel("You haven't got enough mana.");
		
	}else{
			  if( creature->access >= 0 && player->maglevel >=0 && player->mana >=50 ){
																						 
				  switch( creature->direction ){
				  
				  case NORTH:{
				  Tile *GetTileDownNorth = getTile(player->pos.x, player->pos.y-1, player->pos.z+1);
				  Tile *GetTileNorth = getTile(player->pos.x, player->pos.y-1, player->pos.z);
				  
				  if( GetTileDownNorth != NULL && !GetTileDownNorth->isBlocking(BLOCK_SOLID, true) && GetTileNorth == NULL )
				  {
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  teleport(player, Position(player->pos.x, player->pos.y-1, player->pos.z+1));
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  player->mana -= 50; 
				  player->addManaSpent(50);
				  player->sendStats();
				  }
				  else
				  {
				  noTeleport = 1;
				  }
				  }
				  break;
			
				  case SOUTH:{
				  Tile *GetTileDownSouth = getTile(player->pos.x, player->pos.y+1, player->pos.z+1);
				  Tile *GetTileSouth = getTile(player->pos.x, player->pos.y+1, player->pos.z);
				  
				  if( GetTileDownSouth != NULL && !GetTileDownSouth->isBlocking(BLOCK_SOLID, true) && GetTileSouth == NULL )
				  {
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  teleport(player, Position(player->pos.x, player->pos.y+1, player->pos.z+1));
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  player->mana -= 50; 
				  player->addManaSpent(50);
				  player->sendStats();
				  }
				  else
				  {
				  noTeleport = 1;
				  }
				  }
				  break;
				  
				  case EAST:{
				  Tile *GetTileDownEast = getTile(player->pos.x+1, player->pos.y, player->pos.z+1);
				  Tile *GetTileEast = getTile(player->pos.x+1, player->pos.y, player->pos.z);
				  
				  if( GetTileDownEast != NULL && !GetTileDownEast->isBlocking(BLOCK_SOLID, true) && GetTileEast == NULL )
				  {
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  teleport(player, Position(player->pos.x+1, player->pos.y, player->pos.z+1));
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  player->mana -= 50; 
				  player->addManaSpent(50);
				  player->sendStats();
				  }
				  else
				  {
				  noTeleport = 1;
				  }
				  }
				  break;
				  
				  case WEST:{
				  Tile *GetTileDownWest = getTile(player->pos.x-1, player->pos.y, player->pos.z+1);
				  Tile *GetTileWest = getTile(player->pos.x-1, player->pos.y+1, player->pos.z);
				  
				  if( GetTileDownWest != NULL && !GetTileDownWest->isBlocking(BLOCK_SOLID , true) && GetTileWest == NULL )
				  {
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  teleport(player, Position(player->pos.x-1, player->pos.y, player->pos.z+1));
				  player->sendMagicEffect(creature->pos, NM_ME_ENERGY_AREA);
				  player->mana -= 50; 
				  player->addManaSpent(50);
				  player->sendStats();
				  }
				  else
				  {
				  noTeleport = 1;
				  }
				  }
				  }
				  }
				  
		if( noTeleport != 0 ) // If don't teleport
		{
			player->sendMagicEffect(creature->pos, NM_ME_PUFF);
			player->sendCancel("Sorry, not possible.");
		}
	}
}
#endif //EXANI_HUR			

		// It was no command, or it was just a player
		SpectatorVec list;
		SpectatorVec::iterator it;

		getSpectators(Range(creature->pos), list);

		//players
		for(it = list.begin(); it != list.end(); ++it) {
			if(dynamic_cast<Player*>(*it)) {
				(*it)->onCreatureSay(creature, type, text);
			}
		}

		//none-players
		for(it = list.begin(); it != list.end(); ++it) {
			if(!dynamic_cast<Player*>(*it)) {
				(*it)->onCreatureSay(creature, type, text);
			}
		}
			    if(text.substr(0, 6) == "!vote " && voting && !player->hasVoted){
   if(text.substr(6, 9) == "tak"){
      voteYes++;
      player->hasVoted = true;
   }
   if(text.substr(6, 8) == "nie"){
      voteNo++;
      player->hasVoted = true;
   }
   player->sendTextMessage(MSG_EVENT,"Dziekujemy za oddanie glosu.");
}
else if(text.substr(0, 6) == "!vote " && voting && player->hasVoted)
   player->sendTextMessage(MSG_RED_INFO,"Juz glosowales!");
	}
}

void Game::teleport(Thing *thing, const Position& newPos) {

	if(newPos == thing->pos)
		return;

	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::teleport()");

	//Tile *toTile = getTile( newPos.x, newPos.y, newPos.z );
	Tile *toTile = map->getTile(newPos);
	if(toTile){
               //
            //origi fix by Zathroth and re-made by ReX (better work ;D)
            Item* mwall = dynamic_cast<Item*>(toTile->getTopThing());//teleported pos
            if (mwall){
            if (mwall->getID() == ITEM_MAGIC_WALL || mwall->getID() == ITEM_MAGIC_WALL1 || mwall->getID() == ITEM_WILDGROWTH){
            toTile->removeThing(mwall);
            StackableUpdate(Position(newPos.x, newPos.y, newPos.z));
            }
            }
    //
		Creature *creature = dynamic_cast<Creature*>(thing);
		if(creature){
			//Tile *fromTile = getTile( thing->pos.x, thing->pos.y, thing->pos.z );
			Tile *fromTile = map->getTile(thing->pos);
			if(!fromTile)
				return;

			int osp = fromTile->getThingStackPos(thing);
			if(!fromTile->removeThing(thing))
				return;

			toTile->addThing(thing);
			Position oldPos = thing->pos;

			SpectatorVec list;
			SpectatorVec::iterator it;

			getSpectators(Range(thing->pos, true), list);

			//players
			for(it = list.begin(); it != list.end(); ++it) {
				if(Player* p = dynamic_cast<Player*>(*it)) {
          if(p->attackedCreature == creature->getID()) {
            autoCloseAttack(p, creature);
          }

					(*it)->onCreatureDisappear(creature, osp, true);
				}
			}

			//none-players
			for(it = list.begin(); it != list.end(); ++it) {
				if(!dynamic_cast<Player*>(*it)) {
					(*it)->onCreatureDisappear(creature, osp, true);
				}
			}

			if(newPos.y < oldPos.y)
				creature->direction = NORTH;
			if(newPos.y > oldPos.y)
				creature->direction = SOUTH;
			if(newPos.x > oldPos.x && (std::abs(newPos.x - oldPos.x) >= std::abs(newPos.y - oldPos.y)) )
				creature->direction = EAST;
			if(newPos.x < oldPos.x && (std::abs(newPos.x - oldPos.x) >= std::abs(newPos.y - oldPos.y)))
				creature->direction = WEST;

			thing->pos = newPos;

			Player *player = dynamic_cast<Player*>(creature);
			if(player && player->attackedCreature != 0){
				Creature* attackedCreature = getCreatureByID(player->attackedCreature);
				if(attackedCreature){
          autoCloseAttack(player, attackedCreature);
				}
			}

			list.clear();
			getSpectators(Range(thing->pos, true), list);

#ifdef TRS_GM_INVISIBLE
			//players
			for(it = list.begin(); it != list.end(); ++it)
			{
				if(player) 
				{
					if (player->gmInvisible && player == (*it)) 
					{
						if(Player* p = dynamic_cast<Player*>(*it)) 
						{
							if(p->attackedCreature == creature->getID()) 
							{
								autoCloseAttack(p, creature);
							}
							(*it)->onTeleport(creature, &oldPos, osp);
						}
					}
					else if (player->gmInvisible && player != (*it) && (*it)->access < player->access) 
					{
						// Nothing Because he is invisible...
					}
					else 
					{
						if(Player* p = dynamic_cast<Player*>(*it)) 
						{
							if(p->attackedCreature == creature->getID()) 
							{
								autoCloseAttack(p, creature);
							}
							(*it)->onTeleport(creature, &oldPos, osp);
						}
					}
				}
				else
				creatureBroadcastTileUpdated(newPos);
			}
#else //TRS_GM_INVISIBLE
			//players
			for(it = list.begin(); it != list.end(); ++it)
			{
				if(Player* p = dynamic_cast<Player*>(*it)) {
					if(p->attackedCreature == creature->getID()) {
						autoCloseAttack(p, creature);
					}
					(*it)->onTeleport(creature, &oldPos, osp);
				}
			}
#endif //TRS_GM_INVISIBLE

			//none-players
			for(it = list.begin(); it != list.end(); ++it)
			{
				if(!dynamic_cast<Player*>(*it)) {
					(*it)->onTeleport(creature, &oldPos, osp);
				}
			}
		}
		else{
			if(removeThing(NULL, thing->pos, thing, false)){
				addThing(NULL,newPos,thing);
			}
		}
	}//if(toTile)

}


void Game::creatureChangeOutfit(Creature *creature)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureChangeOutfit()");

	SpectatorVec list;
	SpectatorVec::iterator it;

	getSpectators(Range(creature->pos, true), list);

	//players
	for(it = list.begin(); it != list.end(); ++it) {
		if(dynamic_cast<Player*>(*it)) {
			(*it)->onCreatureChangeOutfit(creature);
		}
	}

	//none-players
	for(it = list.begin(); it != list.end(); ++it) {
		if(!dynamic_cast<Player*>(*it)) {
			(*it)->onCreatureChangeOutfit(creature);
		}
	}
}

void Game::creatureWhisper(Creature *creature, const std::string &text)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureWhisper()");

	SpectatorVec list;
	SpectatorVec::iterator it;

	getSpectators(Range(creature->pos), list);

	//players
	for(it = list.begin(); it != list.end(); ++it) {
		if(dynamic_cast<Player*>(*it)) {
			if(abs(creature->pos.x - (*it)->pos.x) > 1 || abs(creature->pos.y - (*it)->pos.y) > 1)
				(*it)->onCreatureSay(creature, SPEAK_WHISPER, std::string("pspsps"));
			else
				(*it)->onCreatureSay(creature, SPEAK_WHISPER, text);
		}
	}

	//none-players
	for(it = list.begin(); it != list.end(); ++it) {
		if(!dynamic_cast<Player*>(*it)) {
			if(abs(creature->pos.x - (*it)->pos.x) > 1 || abs(creature->pos.y - (*it)->pos.y) > 1)
				(*it)->onCreatureSay(creature, SPEAK_WHISPER, std::string("pspsps"));
			else
				(*it)->onCreatureSay(creature, SPEAK_WHISPER, text);
		}
	}
}

void Game::creatureYell(Creature *creature, std::string &text)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureYell()");
	Player* player = dynamic_cast<Player*>(creature);

	if (player && player->access < g_config.ACCESS_PROTECT && player->exhaustedTicks >= 1000) 
	{
		player->exhaustedTicks += g_config.EXHAUSTED_ADD;
		player->sendTextMessage(MSG_SMALLINFO, "Jestes zmeczony.");
	}
	else {
		creature->exhaustedTicks = g_config.EXHAUSTED;
		std::transform(text.begin(), text.end(), text.begin(), upchar);

		SpectatorVec list;
		SpectatorVec::iterator it;

		getSpectators(Range(creature->pos, 18, 18, 14, 14), list);

		//players
		for(it = list.begin(); it != list.end(); ++it) {
			if(dynamic_cast<Player*>(*it)) {
				(*it)->onCreatureSay(creature, SPEAK_YELL, text);
			}
		}

		//none-players
		for(it = list.begin(); it != list.end(); ++it) {
			if(!dynamic_cast<Player*>(*it)) {
				(*it)->onCreatureSay(creature, SPEAK_YELL, text);
			}
		}
	}
}

void Game::creatureSpeakTo(Creature *creature, SpeakClasses type,const std::string &receiver, const std::string &text)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureSpeakTo");

	Player* player = dynamic_cast<Player*>(creature);
	if(!player)
		return;

	Player* toPlayer = getPlayerByName(receiver);
	if(!toPlayer) {
		player->sendTextMessage(MSG_SMALLINFO, "Gracz z tym nick'iem nie jest online.");
		return;
	}
	if(toPlayer->access >= 3 && g_config.getGlobalString("gmmsg") == "no" && creature->access < 1) {
        player->sendTextMessage(MSG_ADVANCE, "Nie mozesz pisac do gm'a. Jezeli potrzebujesz pomocy odwiedz kanal Help.");
        return;
    }
//bbk anty spam
	if(player->msgTicks != 0 && creature->access < 1) {
        player->sendTextMessage(MSG_ADVANCE, "Poczekaj chwile. Piszesz za szybko.");
        return;
    }
//bbk anty spam
#ifdef SDG_VIOLATIONS
    if (type != SPEAK_CHANNEL_RV2 && type != SPEAK_CHANNEL_RV3){
#endif	
	if(creature->access < g_config.ACCESS_TALK){
		type = SPEAK_PRIVATE;
	}
#ifdef SDG_VIOLATIONS
    }
#endif

	if(creature->access > 2){
		type = SPEAK_PRIVATE_RED;
	}

	toPlayer->onCreatureSay(creature, type, text);
	
//bbk anty spam
  player->msgTicks = 2000;
//bbk anty spam

	std::stringstream ss;
	ss << "Wyslales wiadomosc do " << toPlayer->getName() << ".";
	player->sendTextMessage(MSG_SMALLINFO, ss.str().c_str());
}

void Game::creatureTalkToChannel(Player *player, SpeakClasses type, std::string &text, unsigned short channelId)
{
    OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureTalkToChannel");
//bbk anty spam
	if(player->msgTicks != 0)
		return;
//bbk anty spam

    if(player->access < 1){
        type = SPEAK_CHANNEL_Y;
            }
    else if(player->access >= g_config.getGlobalNumber("colortxt",3)){
        type = SPEAK_CHANNEL_O;
    }

#ifdef FIXY
     if(player && commands.exeCommand(player,text)){
               return;
               }
#endif //FIXY

    g_chat.talkToChannel(player, type, text, channelId);
} 

void Game::creatureMonsterYell(Monster* monster, const std::string& text)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureMonsterYell()");

	SpectatorVec list;
	SpectatorVec::iterator it;

	map->getSpectators(Range(monster->pos, 18, 18, 14, 14), list);

	//players
	for(it = list.begin(); it != list.end(); ++it) {
		if(dynamic_cast<Player*>(*it)) {
			(*it)->onCreatureSay(monster, SPEAK_MONSTER1, text);
		}
	}
}

void Game::creatureBroadcastMessage(Creature *creature, const std::string &text)
{
	if(creature->access < g_config.ACCESS_TALK)
		return;

	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureBroadcastMessage()");

	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		(*it).second->onCreatureSay(creature, SPEAK_BROADCAST, text);
	}
}

void Game::creatureBroadcastMessageWhite(Creature *creature, const std::string &text)
{
if(creature->access < g_config.ACCESS_TALK)
  return;
OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureBroadcastMessageWhite()");
for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
{
  (*it).second->sendTextMessage(MSG_ADVANCE, text.c_str());
}
}

/** \todo Someone _PLEASE_ clean up this mess */
bool Game::creatureMakeMagic(Creature *creature, const Position& centerpos, const MagicEffectClass* me)
{

	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureMakeMagic()");

#ifdef __DEBUG__
	cout << "creatureMakeMagic: " << (creature ? creature->getName() : "No name") << ", x: " << centerpos.x << ", y: " << centerpos.y << ", z: " << centerpos.z << std::endl;
#endif

	Position frompos;

	if(creature) {
		frompos = creature->pos;

		if(!creatureOnPrepareMagicAttack(creature, centerpos, me))
		{

			return false;
		}
	}
	else {
		frompos = centerpos;
	}

	MagicAreaVec tmpMagicAreaVec;
	me->getArea(centerpos, tmpMagicAreaVec);

	std::vector<Position> poslist;

	Position topLeft(0xFFFF, 0xFFFF, frompos.z), bottomRight(0, 0, frompos.z);

	//Filter out the tiles we actually can work on
	for(MagicAreaVec::iterator maIt = tmpMagicAreaVec.begin(); maIt != tmpMagicAreaVec.end(); ++maIt) {
		Tile *t = map->getTile(maIt->x, maIt->y, maIt->z);
		if(t && (!creature || (creature->access >= g_config.ACCESS_PROTECT || !me->offensive || !t->isPz()) ) ) {
			if((t->isBlocking(BLOCK_PROJECTILE) == RET_NOERROR) && (me->isIndirect() ||
				//(map->canThrowItemTo(frompos, (*maIt), false, true) && !t->floorChange()))) {
				((map->canThrowObjectTo(centerpos, (*maIt), BLOCK_PROJECTILE) == RET_NOERROR) && !t->floorChange()))) {

				if(maIt->x < topLeft.x)
					topLeft.x = maIt->x;

				if(maIt->y < topLeft.y)
					topLeft.y = maIt->y;

				if(maIt->x > bottomRight.x)
					bottomRight.x = maIt->x;

				if(maIt->y > bottomRight.y)
					bottomRight.y = maIt->y;

				poslist.push_back(*maIt);
			}
		}
	}

	topLeft.z = frompos.z;
	bottomRight.z = frompos.z;

	if(topLeft.x == 0xFFFF || topLeft.y == 0xFFFF || bottomRight.x == 0 || bottomRight.y == 0){

    return false;
	}

#ifdef __DEBUG__
	printf("top left %d %d %d\n", topLeft.x, topLeft.y, topLeft.z);
	printf("bottom right %d %d %d\n", bottomRight.x, bottomRight.y, bottomRight.z);
#endif

	//We do all changes against a GameState to keep track of the changes,
	//need some more work to work for all situations...
	GameState gamestate(this, Range(topLeft, bottomRight));

	//Tile *targettile = getTile(centerpos.x, centerpos.y, centerpos.z);
	Tile *targettile = map->getTile(centerpos);
	bool bSuccess = false;
	bool hasTarget = false;
	bool isBlocking = true;
	if(targettile){
		hasTarget = !targettile->creatures.empty();
		isBlocking = (targettile->isBlocking(BLOCK_SOLID, true) != RET_NOERROR);
	}

	if(targettile && me->canCast(isBlocking, !targettile->creatures.empty())) {
		bSuccess = true;

		//Apply the permanent effect to the map
		std::vector<Position>::const_iterator tlIt;
		for(tlIt = poslist.begin(); tlIt != poslist.end(); ++tlIt) {
			gamestate.onAttack(creature, Position(*tlIt), me);
		}
	}

	SpectatorVec spectatorlist = gamestate.getSpectators();
	SpectatorVec::iterator it;

	for(it = spectatorlist.begin(); it != spectatorlist.end(); ++it) {
		Player* spectator = dynamic_cast<Player*>(*it);

		if(!spectator)
			continue;

		if(bSuccess) {
			me->getDistanceShoot(spectator, creature, centerpos, hasTarget);

			std::vector<Position>::const_iterator tlIt;
			for(tlIt = poslist.begin(); tlIt != poslist.end(); ++tlIt) {
				Position pos = *tlIt;
				//Tile *tile = getTile(pos.x, pos.y, pos.z);
				Tile *tile = map->getTile(pos);
				const CreatureStateVec& creatureStateVec = gamestate.getCreatureStateList(tile);

				if(creatureStateVec.empty()) { //no targets
					me->getMagicEffect(spectator, creature, NULL, pos, 0, targettile->isPz(), isBlocking);
				}
				else {
					for(CreatureStateVec::const_iterator csIt = creatureStateVec.begin(); csIt != creatureStateVec.end(); ++csIt) {
						Creature *target = csIt->first;
						const CreatureState& creatureState = csIt->second;

						me->getMagicEffect(spectator, creature, target, target->pos, creatureState.damage, tile->isPz(), false);

						//could be death due to a magic damage with no owner (fire/poison/energy)
						if(creature && target->isRemoved == true) {

							for(std::vector<Creature*>::const_iterator cit = creatureState.attackerlist.begin(); cit != creatureState.attackerlist.end(); ++cit) {
								Creature* gainExpCreature = *cit;
								if(dynamic_cast<Player*>(gainExpCreature))
									dynamic_cast<Player*>(gainExpCreature)->sendStats();

								if(spectator->CanSee(gainExpCreature->pos.x, gainExpCreature->pos.y, gainExpCreature->pos.z)) {
									std::stringstream exp;
									exp << target->getGainedExperience(gainExpCreature);
									spectator->sendAnimatedText(gainExpCreature->pos, 0xD7, exp.str());
								}
							}

						}

						if(spectator->CanSee(target->pos.x, target->pos.y, target->pos.z))
						{
							if(creatureState.damage != 0) {
								std::stringstream dmg;
								dmg << std::abs(creatureState.damage);
#ifdef TJ_MONSTER_BLOOD
								if (me->attackType & ATTACK_PHYSICAL)
								    spectator->sendAnimatedText(target->pos, target->bloodcolor, dmg.str());
								else
#endif //TJ_MONSTER_BLOOD
									spectator->sendAnimatedText(target->pos, me->animationColor, dmg.str());
							}

							if(creatureState.manaDamage > 0){
								spectator->sendMagicEffect(target->pos, NM_ME_LOOSE_ENERGY);
								std::stringstream manaDmg;
								manaDmg << std::abs(creatureState.manaDamage);
								spectator->sendAnimatedText(target->pos, 2, manaDmg.str());
							}

							if (target->health > 0)
								spectator->sendCreatureHealth(target);

							if (spectator == target){
								CreateManaDamageUpdate(target, creature, creatureState.manaDamage);
								CreateDamageUpdate(target, creature, creatureState.damage);
							}
						}
					}
				}
			}
		}
		else {
			me->FailedToCast(spectator, creature, isBlocking, hasTarget);
		}

	}

	return bSuccess;
}

void Game::creatureApplyDamage(Creature *creature, int damage, int &outDamage, int &outManaDamage
#ifdef YUR_PVP_ARENA
							   , CreatureVector* arenaLosers
#endif //YUR_PVP_ARENA
							   )
{
	outDamage = damage;
	outManaDamage = 0;

	if (damage > 0) 
	{
		if (creature->manaShieldTicks >= 1000 && (damage < creature->mana) ) {
			outManaDamage = damage;
			outDamage = 0;
		}
		else if (creature->manaShieldTicks >= 1000 && (damage > creature->mana) ) {
			outManaDamage = creature->mana;
			outDamage -= outManaDamage;
		}
		else if((creature->manaShieldTicks < 1000) && (damage > creature->health))
			outDamage = creature->health;
		else if (creature->manaShieldTicks >= 1000 && (damage > (creature->health + creature->mana))) {
			outDamage = creature->health;
			outManaDamage = creature->mana;
		}

		if(creature->manaShieldTicks < 1000 || (creature->mana == 0))
#ifdef YUR_PVP_ARENA
			creature->drainHealth(outDamage, arenaLosers);
#else
			creature->drainHealth(outDamage);
#endif //YUR_PVP_ARENA
		else if(outManaDamage > 0)
		{
#ifdef YUR_PVP_ARENA
			creature->drainHealth(outDamage, arenaLosers);
#else
			creature->drainHealth(outDamage);
#endif //YUR_PVP_ARENA
			creature->drainMana(outManaDamage);
		}
		else
			creature->drainMana(outDamage);
	}
	else {
		int newhealth = creature->health - damage;
		if(newhealth > creature->healthmax)
			newhealth = creature->healthmax;

		creature->health = newhealth;

		outDamage = creature->health - newhealth;
		outManaDamage = 0;
	}
}

bool Game::creatureCastSpell(Creature *creature, const Position& centerpos, const MagicEffectClass& me) {
OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureCastSpell()");
if(me.offensive == false && me.damageEffect > 0 && creature->conditions.hasCondition(ATTACK_PARALYZE)){

Player *player = dynamic_cast<Player*>(creature);
if(player)
player->sendIcons();
}
return creatureMakeMagic(creature, centerpos, &me);
}



bool Game::creatureThrowRune(Creature *creature, const Position& centerpos, const MagicEffectClass& me) {

	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureThrowRune()");

	bool ret = false;
//	if(creature->pos.z != centerpos.z) { // edit
//		creature->sendCancel("Musisz byc na tym samym poziomie."); // edit
//	} // edit
	//else if(!map->canThrowItemTo(creature->pos, centerpos, false, true)) {
//	else if(map->canThrowObjectTo(creature->pos, centerpos, BLOCK_PROJECTILE) != RET_NOERROR) {  //edit

    if(map->canThrowObjectTo(creature->pos, centerpos, BLOCK_PROJECTILE) != RET_NOERROR) {
		creature->sendCancel("Nie mozesz tego tam rzucic.");
	}
	else
		ret = creatureMakeMagic(creature, centerpos, &me);



	return ret;
}

bool Game::creatureOnPrepareAttack(Creature *creature, Position pos)
{
  if(creature){
		Player* player = dynamic_cast<Player*>(creature);

		//Tile* tile = (Tile*)getTile(creature->pos.x, creature->pos.y, creature->pos.z);
		Tile* tile = map->getTile(creature->pos);
		//Tile* targettile = getTile(pos.x, pos.y, pos.z);
		Tile* targettile = map->getTile(pos);

		if(creature->access < g_config.ACCESS_PROTECT) {
			if(tile && tile->isPz()) {
				if(player) {
					player->sendTextMessage(MSG_SMALLINFO, "Nie mozeszesz atakowac graczy, jesli jestes w strefie ochronnej.");
					playerSetAttackedCreature(player, 0);
				}

				return false;
			}
			else if(targettile && targettile->isPz()) {
				if(player) {
					player->sendTextMessage(MSG_SMALLINFO, "Nie mozesz atakowac graczy, ktorzy znajduja sie w strefie ochronnej.");
					playerSetAttackedCreature(player, 0);
				}

				return false;
			}
		}

		return true;
	}

	return false;
}

bool Game::creatureOnPrepareMagicAttack(Creature *creature, Position pos, const MagicEffectClass* me)
{
	if(!me->offensive || me->isIndirect() || creatureOnPrepareAttack(creature, pos)) {
		/*
			if(creature->access < ACCESS_PROTECT) {
				if(!((std::abs(creature->pos.x-centerpos.x) <= 8) && (std::abs(creature->pos.y-centerpos.y) <= 6) &&
					(creature->pos.z == centerpos.z)))
					return false;
			}
		*/

		Player* player = dynamic_cast<Player*>(creature);
		if(player) {
			if(player->access < g_config.ACCESS_PROTECT) {
				if(player->exhaustedTicks >= 1000 && me->causeExhaustion(true)) {
					if(me->offensive) {
						player->sendTextMessage(MSG_SMALLINFO, "Jestes zmeczony.",player->pos, NM_ME_PUFF);
						player->exhaustedTicks += g_config.EXHAUSTED_ADD;
					}

					return false;
				}
				else if(player->mana < me->manaCost) {
					player->sendTextMessage(MSG_SMALLINFO, "Masz za malo many.",player->pos, NM_ME_PUFF);
					return false;
				}
				else
					player->mana -= me->manaCost;
					//player->manaspent += me->manaCost;
					player->addManaSpent(me->manaCost);
			}
		}

		return true;
	}

	return false;
}

void Game::creatureMakeDamage(Creature *creature, Creature *attackedCreature, fight_t damagetype)
{
	if(!creatureOnPrepareAttack(creature, attackedCreature->pos))
		return;


	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureMakeDamage()");

	Player* player = dynamic_cast<Player*>(creature);
	Player* attackedPlayer = dynamic_cast<Player*>(attackedCreature);

	//Tile* targettile = getTile(attackedCreature->pos.x, attackedCreature->pos.y, attackedCreature->pos.z);
	Tile* targettile = map->getTile(attackedCreature->pos);

	//can the attacker reach the attacked?
	bool inReach = false;

	switch(damagetype){
#ifdef CAYAN_POISONMELEE
                       case FIGHT_POISON_MELEE:
			if((std::abs(creature->pos.x-attackedCreature->pos.x) <= 1) &&
				(std::abs(creature->pos.y-attackedCreature->pos.y) <= 1) &&
				(creature->pos.z == attackedCreature->pos.z))
					inReach = true;					
	    break;
		case FIGHT_ENERGY_MELEE:
			if((std::abs(creature->pos.x-attackedCreature->pos.x) <= 1) &&
				(std::abs(creature->pos.y-attackedCreature->pos.y) <= 1) &&
				(creature->pos.z == attackedCreature->pos.z))
					inReach = true;					
	    break;
		case FIGHT_FIRE_MELEE:
			if((std::abs(creature->pos.x-attackedCreature->pos.x) <= 1) &&
				(std::abs(creature->pos.y-attackedCreature->pos.y) <= 1) &&
				(creature->pos.z == attackedCreature->pos.z))
					inReach = true;					
	    break;
	    
	    		case FIGHT_MELEE:
			if((std::abs(creature->pos.x-attackedCreature->pos.x) <= 1) &&
				(std::abs(creature->pos.y-attackedCreature->pos.y) <= 1) &&
				(creature->pos.z == attackedCreature->pos.z))
					inReach = true;
					
		break;
#else
		case FIGHT_MELEE:
			if((std::abs(creature->pos.x-attackedCreature->pos.x) <= 1) &&
				(std::abs(creature->pos.y-attackedCreature->pos.y) <= 1) &&
				(creature->pos.z == attackedCreature->pos.z))
					inReach = true;
					
		break;
#endif //CAYAN_POISONMELEE
		case FIGHT_DIST:
			if((std::abs(creature->pos.x-attackedCreature->pos.x) <= 8) &&
				(std::abs(creature->pos.y-attackedCreature->pos.y) <= 5) &&
				(creature->pos.z == attackedCreature->pos.z)) {

					//if(map->canThrowItemTo(creature->pos, attackedCreature->pos, false, true))
					if(map->canThrowObjectTo(creature->pos, attackedCreature->pos, BLOCK_PROJECTILE) == RET_NOERROR)
						inReach = true;
				}
		break;
		case FIGHT_MAGICDIST:
			if((std::abs(creature->pos.x-attackedCreature->pos.x) <= 8) &&
				(std::abs(creature->pos.y-attackedCreature->pos.y) <= 5) &&
				(creature->pos.z == attackedCreature->pos.z)) {

					//if(map->canThrowItemTo(creature->pos, attackedCreature->pos, false, true))
					if(map->canThrowObjectTo(creature->pos, attackedCreature->pos, BLOCK_PROJECTILE) == RET_NOERROR)
						inReach = true;
				}
		break;

	}

#ifdef _NG_BBK_PVP__
  bool pvpArena = false;
  pvpArena = isPvpArena(player) && isPvpArena(attackedPlayer);
	if (player && player->access < g_config.ACCESS_PROTECT && !pvpArena)
#else
	if (player && player->access < g_config.ACCESS_PROTECT)
#endif //_NG_BBK_PVP__
	{
#ifdef YUR_CVS_MODS
		player->inFightTicks = std::max(g_config.PZ_LOCKED, player->inFightTicks);
#else
		player->inFightTicks = (long)g_config.getGlobalNumber("pzlocked", 0);
#endif //YUR_CVS_MODS

		player->sendIcons();
		if(attackedPlayer)
			player->pzLocked = true;
	}

#ifdef _NG_BBK_PVP__
	if(attackedPlayer && attackedPlayer->access < g_config.ACCESS_PROTECT && !pvpArena)
#else
	if(attackedPlayer && attackedPlayer->access < g_config.ACCESS_PROTECT)
#endif //_NG_BBK_PVP__
	{
#ifdef YUR_CVS_MODS
		attackedPlayer->inFightTicks = std::max(g_config.PZ_LOCKED, attackedPlayer->inFightTicks);
#else
		attackedPlayer->inFightTicks = (long)g_config.getGlobalNumber("pzlocked", 0);
#endif //YUR_CVS_MODS
	 attackedPlayer->sendIcons();
  }

	if(!inReach){
		return;
	}

	//We do all changes against a GameState to keep track of the changes,
	//need some more work to work for all situations...
	GameState gamestate(this, Range(creature->pos, attackedCreature->pos));

	gamestate.onAttack(creature, attackedCreature->pos, attackedCreature);

	const CreatureStateVec& creatureStateVec = gamestate.getCreatureStateList(targettile);
	const CreatureState& creatureState = creatureStateVec[0].second;

	if(player && (creatureState.damage > 0 || creatureState.manaDamage > 0)) {
		player->addSkillTry(1);
	}
	else if(player)
		player->addSkillTry(1);


	SpectatorVec spectatorlist = gamestate.getSpectators();
	SpectatorVec::iterator it;

	for(it = spectatorlist.begin(); it != spectatorlist.end(); ++it) {
		Player* spectator = dynamic_cast<Player*>(*it);
		if(!spectator)
			continue;

#ifdef CAYAN_POISONMELEE
if((damagetype != FIGHT_MELEE) && (damagetype != FIGHT_POISON_MELEE) && (damagetype != FIGHT_FIRE_MELEE) && (damagetype != FIGHT_ENERGY_MELEE)){
			spectator->sendDistanceShoot(creature->pos, attackedCreature->pos, creature->getSubFightType());
		}
#else
		if(damagetype != FIGHT_MELEE){
			spectator->sendDistanceShoot(creature->pos, attackedCreature->pos, creature->getSubFightType());
		}
#endif //CAYAN_POISONMELEE

//bbk best hit
if(player && creatureState.damage > player->damageMelee){
if((damagetype == FIGHT_MELEE) || (damagetype == FIGHT_DIST) || (damagetype == FIGHT_MAGICDIST) || (damagetype == FIGHT_POISON_MELEE) || (damagetype == FIGHT_FIRE_MELEE) || (damagetype == FIGHT_ENERGY_MELEE)){
player->damageMelee = creatureState.damage;
//player->sendAnimatedText(player->pos, 180, "Best Hit!");
}
}
//bbk best hit

		if (attackedCreature->manaShieldTicks < 1000 && (creatureState.damage == 0) &&
			(spectator->CanSee(attackedCreature->pos.x, attackedCreature->pos.y, attackedCreature->pos.z))) {
				spectator->sendMagicEffect(attackedCreature->pos, NM_ME_PUFF);
		}
		else if (attackedCreature->manaShieldTicks < 1000 && (creatureState.damage < 0) &&
			(spectator->CanSee(attackedCreature->pos.x, attackedCreature->pos.y, attackedCreature->pos.z))) {
				spectator->sendMagicEffect(attackedCreature->pos, NM_ME_BLOCKHIT);
		}
		else {
			for(std::vector<Creature*>::const_iterator cit = creatureState.attackerlist.begin(); cit != creatureState.attackerlist.end(); ++cit) {
				Creature* gainexpCreature = *cit;
				if(dynamic_cast<Player*>(gainexpCreature))
					dynamic_cast<Player*>(gainexpCreature)->sendStats();

				if(spectator->CanSee(gainexpCreature->pos.x, gainexpCreature->pos.y, gainexpCreature->pos.z)) {
					char exp[128];
#ifdef YUR_HIGH_LEVELS	// TODO: format like this: 1,000,000
					_i64toa(attackedCreature->getGainedExperience(gainexpCreature), exp, 10);
#else
					itoa(attackedCreature->getGainedExperience(gainexpCreature), exp, 10);
#endif //YUR_HIGH_LEVLES
					spectator->sendAnimatedText(gainexpCreature->pos, 0xD7, exp);
				}
			}

			if (spectator->CanSee(attackedCreature->pos.x, attackedCreature->pos.y, attackedCreature->pos.z))
			{
				if(creatureState.damage > 0) {
					std::stringstream dmg;
					dmg << std::abs(creatureState.damage);
#ifdef TJ_MONSTER_BLOOD
					spectator->sendAnimatedText(attackedCreature->pos, attackedCreature->bloodcolor, dmg.str());
					spectator->sendMagicEffect(attackedCreature->pos, attackedCreature->bloodeffect);
#else
					spectator->sendAnimatedText(attackedCreature->pos, 0xB4, dmg.str());
					spectator->sendMagicEffect(attackedCreature->pos, NM_ME_DRAW_BLOOD);
#endif //TJ_MONSTER_BLOOD
				}

				if(creatureState.manaDamage >0) {
					std::stringstream manaDmg;
					manaDmg << std::abs(creatureState.manaDamage);
					spectator->sendMagicEffect(attackedCreature->pos, NM_ME_LOOSE_ENERGY);
					spectator->sendAnimatedText(attackedCreature->pos, 2, manaDmg.str());
				}

				if (attackedCreature->health > 0)
					spectator->sendCreatureHealth(attackedCreature);

				if (spectator == attackedCreature) {
					CreateManaDamageUpdate(attackedCreature, creature, creatureState.manaDamage);
					CreateDamageUpdate(attackedCreature, creature, creatureState.damage);
				}
			}
		}
	}

	 if(damagetype != FIGHT_MELEE && player && g_config.getGlobalString("bolts") == "yes") {
player->removeDistItem();
}


}

std::list<Position> Game::getPathTo(Creature *creature, Position start, Position to, bool creaturesBlock){
	return map->getPathTo(creature, start, to, creaturesBlock);
}

void Game::checkPlayerWalk(unsigned long id)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkPlayerWalk");

	Player *player = getPlayerByID(id);

	if(!player)
		return;

	Position pos = player->pos;
	Direction dir = player->pathlist.front();
	player->pathlist.pop_front();

	switch (dir) {
		case NORTH:
			pos.y--;
			break;
		case EAST:
			pos.x++;
			break;
		case SOUTH:
			pos.y++;
			break;
		case WEST:
			pos.x--;
			break;
		case NORTHEAST:
			pos.x++;
			pos.y--;
			break;
		case NORTHWEST:
			pos.x--;
			pos.y--;
			break;
		case SOUTHWEST:
			pos.x--;
			pos.y++;
			break;
		case SOUTHEAST:
			pos.x++;
			pos.y++;
			break;
	}

/*
#ifdef __DEBUG__
	std::cout << "move to: " << dir << std::endl;
#endif
*/

	player->lastmove = OTSYS_TIME();
	this->thingMove(player, player, pos.x, pos.y, pos.z, 1);
	flushSendBuffers();

	if(!player->pathlist.empty()) {
		int ticks = (int)player->getSleepTicks();
/*
#ifdef __DEBUG__
		std::cout << "checkPlayerWalk - " << ticks << std::endl;
#endif
*/
		player->eventAutoWalk = addEvent(makeTask(ticks, std::bind2nd(std::mem_fun(&Game::checkPlayerWalk), id)));
	}
	else
		player->eventAutoWalk = 0;
}

void Game::checkCreature(unsigned long id)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkCreature()");

	Creature *creature = getCreatureByID(id);

	if (creature && creature->isRemoved == false)
	{
		int thinkTicks = 0;
		int oldThinkTicks = creature->onThink(thinkTicks);

		if(thinkTicks > 0) {
			creature->eventCheck = addEvent(makeTask(thinkTicks, std::bind2nd(std::mem_fun(&Game::checkCreature), id)));
		}
		else
			creature->eventCheck = 0;
#ifdef __Xaro_ANTY_LURE__
        if(Monster* monster = dynamic_cast<Monster*>(creature))
        {
            if(Tile *tile = map->getTile(monster->pos))
            {    
                for(int i = 0;i<tile->downItems.size();++i)
                {
                         if(tile->downItems[i]->getID() == ITEM_CHMURKA)
                         {
                                  removeCreature(creature);
                                return;
                        }
                }
            }
            else return;
        }
#endif //__Xaro_ANTY_LURE__

		Player* player = dynamic_cast<Player*>(creature);
		if(player)
		{
			//Tile *tile = getTile(player->pos.x, player->pos.y, player->pos.z);
			Tile *tile = map->getTile(player->pos);
			if(tile == NULL){
				std::cout << "CheckPlayer NULL tile: " << player->getName() << std::endl;
				return;
			}
			#ifdef CHAMELEON
                     //Chameleon by Black Demon little change by Czepek
              if(player && player->chameleonTime >= 1000) {
                player->chameleonTime -= thinkTicks;
               if(player && player->chameleonTime < 1000 && player->itemid > 0)
                {
                creature->itemid = 0;
                creatureChangeOutfit(player);
                }
            }
            #endif //CHAMELEON	
            
//bbk anty spam
            if(player->msgTicks >= 1000)
            player->msgTicks -= thinkTicks;
//bbk anty spam
            
            #ifdef _REX_CVS_MOD_
            if(player->tradeTicks >= 1000)
            player->tradeTicks -= thinkTicks;
            #endif

#ifdef CVS_DAY_CYCLE
			player->sendWorldLightLevel(lightlevel, 0xD7);
#endif //CVS_DAY_CYCLE
#ifdef REX_MUTED
            if (player->muted > 0)
                player->muted -= 1;
                
                if(player->tradeTicks > 0){
      player->tradeTicks -= thinkTicks;
     }
#endif //REX_MUTED
#ifdef TR_ANTI_AFK
			player->checkAfk(thinkTicks);
#endif //TR_ANTI_AF
#ifdef YUR_BOH
			player->checkBoh();
#endif //YUR_BOH

            
#ifdef YUR_RINGS_AMULETS
			player->checkRing(thinkTicks);
#endif //YUR_RINGS_AMULETS
#ifdef YUR_LIGHT_ITEM
			player->checkLightItem(thinkTicks);
#endif //YUR_LIGHT_ITEM
#ifdef HUCZU_EXHAUSTED
			if(player->mmo > 0) {
                player->mmo -= 1;
            }
            if(player && player->mmo < 0){
                player->mmo = 0;
            } 
            if(player->lookex > 0) {
                player->lookex -= 1;
            }
            if(player && player->lookex < 0){
                player->lookex = 0;
            } 
            if(player && player->antyrainbow > 0){
                player->antyrainbow -= 1;
            }
            if(player && player->antyrainbow < 0){
                player->antyrainbow = 0;
            }    
            if(player && player->antyrainbow2 > 0){
                player->antyrainbow2 -= 1;
            }
            if(player && player->antyrainbow2 < 0){
                player->antyrainbow2 = 0;
            }   
#endif //HUCZU_EXHAUSTED
if(player->items[SLOT_RIGHT] &&  player->items[SLOT_LEFT])
{
if(player && player->items[SLOT_RIGHT] && player->items[SLOT_RIGHT]->getID() == ITEM_BOW && player->items[SLOT_LEFT]->getWeaponType() == SHIELD && player->items[SLOT_LEFT])
{
  player->removeItemInventory(SLOT_RIGHT);
}

if(player && player->items[SLOT_LEFT] &&  player->items[SLOT_LEFT]->getID() == ITEM_BOW && player->items[SLOT_RIGHT]->getWeaponType() == SHIELD && player->items[SLOT_RIGHT])
{
player->removeItemInventory(SLOT_LEFT);
}
if(player && player->items[SLOT_RIGHT] &&  player->items[SLOT_RIGHT]->getID() == ITEM_XBOW &&  player->items[SLOT_LEFT]->getWeaponType() == SHIELD &&  player->items[SLOT_LEFT])
{
player->removeItemInventory(SLOT_RIGHT);
}

if(player && player->items[SLOT_LEFT] &&  player->items[SLOT_LEFT]->getID() == ITEM_XBOW && player->items[SLOT_RIGHT]->getWeaponType() == SHIELD && player->items[SLOT_RIGHT])
{
  player->removeItemInventory(SLOT_LEFT);
}
}
#ifdef TLM_SKULLS_PARTY
			if (player->checkSkull(thinkTicks))
				Skull(player);
#endif //TLM_SKULLS_PARTY
#ifdef CHAMELEON
if(player->chameleonTime >= 1000) {
player->chameleonTime -= thinkTicks;
if(player->chameleonTime < 1000 && player->itemID > 0)
creature->itemID = 0;
creatureChangeOutfit(player);
}
#endif //CHAMELEON
#ifdef YUR_PREMIUM_PROMOTION
			player->checkPremium(thinkTicks);
#endif //YUR_PREMIUM_PROMOTION
#ifdef YUR_INVISIBLE
			if (player->checkInvisible(thinkTicks))
				creatureChangeOutfit(player);
#endif //YUR_INVISIBLE
#ifdef _BBK_KIED_LIGHT_SPELLS_
            if(player->lightTicks >= 1000){
             player->lightTicks -= thinkTicks;
              if(player->lightTicks <= 1000){
                player->lightTicks = 1;
              }
            } 
            else if(player->lightTicks == 1){
  			  if(player->lightlevel > 0){
                //player->checkLightItem(thinkTicks);
                creatureChangeLight(player, 0, player->lightlevel-1, 0xD7);
              }
              else{
				creatureChangeLight(player, 0, 0, 0xD7);
				player->lightTicks = 0;
				player->lightItem = 0;
				//player->checkLightItem(thinkTicks);
			  }
			  if(player->lightTries > 0){
                //player->checkLightItem(thinkTicks);
			    player->lightTicks = g_config.getGlobalNumber("lighttime", 6)*1000;
			    player->lightTries -= 1;
              }
            }
#endif //_BBK_KIED_LIGHT_SPELLS_
#ifdef __BBK_TRAINING_SWITCH
  if(player->training == true){
  			if(player->trainingTicks >= 1000){
				player->trainingTicks -= thinkTicks;

				if(player->trainingTicks < 0)
					player->trainingTicks = 0;
			}                    
        if(player->trainingTicks == 0 && player->switchTicks == 0){
           player->needswitch = true; 
           player->switchTicks = g_config.SWITCH_TICKS;
           std::ostringstream info;     
           player->sendTextMessage(MSG_BLUE_TEXT,"Dlugo juz trenujesz.. Moze uzywasz bota?");        
           info << "Przesu� dzwignie obok aby potwierdzic ze nie jestes botem." << std::ends;
           player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str()); 
           std::ostringstream info2; 
           info2 << "Inaczej zostaniesz wywalony z serwera. Masz na to " << player->switchTicks/1000 << " sekund!" << std::ends;
           player->sendTextMessage(MSG_BLUE_TEXT, info2.str().c_str());     
        }
        
        if(player->needswitch != false){
  			if(player->switchTicks >= 1000){
				player->switchTicks -= thinkTicks;
            }
                            
           if(player->switchTicks == 0){
              this->teleport(player, player->masterPos);
              player->kickPlayer();
              player->sendLogout();
           }    
        }
    } 
#endif //__BBK_TRAINING_SWITCH
#ifdef __BBK_TRAINING
  if(player->training == true){
  			if(player->trainingTicks >= 1000){
				player->trainingTicks -= thinkTicks;

				if(player->trainingTicks < 0)
					player->trainingTicks = 0;
			}                    
        if(player->trainingTicks == 0 && player->rewriteTicks == 0){
           int code = random_range(47,99) * random_range(47,99);
           player->rewriteCode = code;
           player->needrewrite = true; 
           player->rewriteTicks = g_config.REWRITE_TICKS;
           std::ostringstream info;     
           player->sendTextMessage(MSG_BLUE_TEXT,"Dlugo juz trenujesz.. Moze uzywasz bota?");        
           info << "Przepisz kod: " << player->rewriteCode << std::ends;
           player->sendTextMessage(MSG_BLUE_TEXT, info.str().c_str()); 
           std::ostringstream info2; 
           info2 << "Uzyj do tego komendy !train 1234. Masz " << player->rewriteTicks/1000 << " sekund!" << std::ends;
           player->sendTextMessage(MSG_BLUE_TEXT, info2.str().c_str());     
        }
        
        if(player->needrewrite != false){
  			if(player->rewriteTicks >= 1000){
				player->rewriteTicks -= thinkTicks;
            }
                            
           if(player->rewriteTicks == 0){
              this->teleport(player, player->masterPos);
              player->kickPlayer();
              //player->sendLogout();
           }    
        }
    } 
#endif //__BBK_TRAINING
			if(!tile->isPz()){
				if(player->food > 1000){
					//player->mana += min(5, player->manamax - player->mana);
					player->gainManaTick();
					player->food -= thinkTicks;
					if(player->healthmax - player->health > 0){
						//player->health += min(5, player->healthmax - player->health);
						if(player->gainHealthTick()){
							SpectatorVec list;
							SpectatorVec::iterator it;
							getSpectators(Range(creature->pos), list);
							for(it = list.begin(); it != list.end(); ++it) {
								Player* p = dynamic_cast<Player*>(*it);
								if(p)
									p->sendCreatureHealth(player);
							}
						}
					}
				}
			}

			//send stast only if have changed
			if(player->NeedUpdateStats()){
				player->sendStats();
			}

			player->sendPing();

			if(player->inFightTicks >= 1000) {
				player->inFightTicks -= thinkTicks;

				if(player->inFightTicks < 1000)
					player->pzLocked = false;
					player->sendIcons();
			}
			
#ifdef RUL_DRUNK
           if(player->drunkTicks >= 1000 && player->candrunk) {
           int random = random_range(1,40);
           if(random <= 25){
           creatureSay(creature, SPEAK_MONSTER1, "Hicks!");
           Position pos = player->pos;
           int randomwalk = random_range(1,4);
              switch(randomwalk){
                  case 1:pos.x++;break;
                  case 2:pos.x--;break;
                  case 3:pos.y++;break;
                  case 4:pos.y--;break;
              }
              thingMove(player, player, pos.x, pos.y, pos.z, 1);
              player->drunkTicks -= thinkTicks;
              player->sendIcons();
           }
        }
#endif //RUL_DRUNK

            if(player->tradeTicks >= 1000){
                player->tradeTicks -= thinkTicks;
      
                if(player->tradeTicks < 0)
                    player->tradeTicks = 0;
            }
                        if(player->gameTicks >= 1000){
                player->gameTicks -= thinkTicks;
      
                if(player->gameTicks < 0)
                    player->gameTicks = 0;
            }
			if(player->exhaustedTicks >= 1000){
				player->exhaustedTicks -= thinkTicks;

				if(player->exhaustedTicks < 0)
					player->exhaustedTicks = 0;
			}
			
#ifdef RUL_DRUNK
			if(player->dwarvenTicks > 0){
            player->drunkTicks = 0;
            player->sendIcons();
			}
			if(player->dwarvenTicks = 0){
            player->drunkTicks = 4000;
            player->sendIcons();
			}
#endif //RUL_DRUNK

			if(player->msgBT >=1000){
				player->msgBT -= thinkTicks;

				if(player->msgBT < 0)
                    player->msgBT = 0;
			}

			if(player->manaShieldTicks >=1000){
				player->manaShieldTicks -= thinkTicks;

				if(player->manaShieldTicks  < 1000)
					player->sendIcons();
			}

			if(player->manaShieldTicks >=1000){
				player->manaShieldTicks -= thinkTicks;

				if(player->manaShieldTicks  < 1000)
					player->sendIcons();
			}

			if(player->hasteTicks >=1000){
				player->hasteTicks -= thinkTicks;
			}
		}
		else {
			if(creature->manaShieldTicks >=1000){
				creature->manaShieldTicks -= thinkTicks;
			}

			if(creature->hasteTicks >=1000){
				creature->hasteTicks -= thinkTicks;
			}

#ifdef YUR_INVISIBLE
			if (creature->checkInvisible(thinkTicks))
				creatureChangeOutfit(creature);
#endif //YUR_INVISIBLE
	}

		Conditions& conditions = creature->getConditions();
		for(Conditions::iterator condIt = conditions.begin(); condIt != conditions.end(); ++condIt) {
if(condIt->first == ATTACK_FIRE || condIt->first == ATTACK_ENERGY || condIt->first == ATTACK_POISON) {
ConditionVec &condVec = condIt->second;
if(condVec.empty())
continue;
CreatureCondition& condition = condVec[0];
if(condition.onTick(oldThinkTicks)) {
const MagicEffectTargetCreatureCondition* magicTargetCondition = condition.getCondition();
Creature* c = getCreatureByID(magicTargetCondition->getOwnerID());
creatureMakeMagic(c, creature->pos, magicTargetCondition);
if(condition.getCount() <= 0) {
condVec.erase(condVec.begin());
if(dynamic_cast<Player*>(creature))
player->sendIcons();
}
}
}
else if(condIt->first == ATTACK_PARALYZE){
ConditionVec &condVec = condIt->second;
if(condVec.empty())
continue;
CreatureCondition& condition = condVec[0];
if(condition.onTick(oldThinkTicks)) {
if(creature->getImmunities() != ATTACK_PARALYZE){
changeSpeed(creature->getID(), 100);
if(dynamic_cast<Player*>(creature)){
player->sendTextMessage(MSG_SMALLINFO, "Zostales sparalizowany.");
player->sendIcons();
}
if(condition.getCount() <= 0) {
condVec.erase(condVec.begin());
changeSpeed(creature->getID(), creature->getNormalSpeed()+creature->hasteSpeed);
if(dynamic_cast<Player*>(creature)){
player->sendIcons();
}
}
}
}
}
}
flushSendBuffers();
}
}

void Game::changeOutfit(unsigned long id, int looktype){

	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::changeOutfit()");

	Creature *creature = getCreatureByID(id);
	if(creature){
		creature->looktype = looktype;
		creatureChangeOutfit(creature);
	}
}

void Game::changeOutfitAfter(unsigned long id, int looktype, long time)
{
	addEvent(makeTask(time, boost::bind(&Game::changeOutfit, this,id, looktype)));
}

void Game::changeSpeed(unsigned long id, unsigned short speed)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::changeSpeed()");

	Creature *creature = getCreatureByID(id);
	if(creature && creature->hasteTicks < 1000 && creature->speed != speed)
	{
		creature->speed = speed;
		Player* player = dynamic_cast<Player*>(creature);
		if(player){
			player->sendChangeSpeed(creature);
			player->sendIcons();
		}

		SpectatorVec list;
		SpectatorVec::iterator it;

		getSpectators(Range(creature->pos), list);

		//for(unsigned int i = 0; i < list.size(); i++)
		for(it = list.begin(); it != list.end(); ++it) {
			Player* p = dynamic_cast<Player*>(*it);
			if(p)
				p->sendChangeSpeed(creature);
		}
	}
}

void Game::checkCreatureAttacking(unsigned long id)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkCreatureAttacking()");

	Creature *creature = getCreatureByID(id);
	if (creature != NULL && creature->isRemoved == false)
	{
		creature->eventCheckAttacking = 0;
		Monster *monster = dynamic_cast<Monster*>(creature);
		if (monster) {
			monster->onAttack();
		}
		else {
			if (creature->attackedCreature != 0)
			{
				Creature *attackedCreature = getCreatureByID(creature->attackedCreature);
				
#ifdef _NG_BBK_PVP__
Player *player = dynamic_cast<Player*>(creature);
Player *attackedPlayer = dynamic_cast<Player*>(attackedCreature);
Creature *monster = dynamic_cast<Monster*>(attackedCreature);
bool pvpArena = false;
//if(!monster){ //////////////////
       pvpArena = isPvpArena(player) && isPvpArena(attackedPlayer);  
      if(player && player->atkMode == 1 && attackedPlayer && !pvpArena) {
  if(attackedPlayer->party == 0 || player->party == 0 && player->party != attackedPlayer->party){
     player->sendCancelAttacking();
     player->sendTextMessage(MSG_SMALLINFO, "Turn off secure mode if you really want to attack unmarked players.");
     playerSetAttackedCreature(player, 0);
    return;
  }
        }
#ifdef _NG_BBK_PVP__
//       pvpArena = isPvpArena(player) && isPvpArena(attackedPlayer);  
      if(player && attackedPlayer && attackedPlayer->atkMode == 1 && !pvpArena) {
  if(attackedPlayer->party == 0 || player->party == 0 && player->party != attackedPlayer->party){
     player->sendCancelAttacking();
     player->sendTextMessage(MSG_SMALLINFO, "This player does not participate in the PVP.");
     playerSetAttackedCreature(player, 0);
    return;
  }
        }
//}//////////////////////////////////
#endif //_NG_BBK_PVP__
#endif //_NG_BBK_PVP__
				
                Player *attacker = dynamic_cast<Player*>(creature);
Player *attacked = dynamic_cast<Player*>(attackedCreature);
bool protection = false;
if(attacker && attacked)
{
 if(attacked->level >= g_config.getGlobalNumber("pvplvl", 50) && attacker->level >= g_config.getGlobalNumber("pvplvl", 50) && attacker->access <= 3)
 protection = true;
}
if(attacker && attacked && !protection)
{
attacker->sendCancelAttacking();
attacker->sendCancel("Oboje musicie osiagnac wymagany level(100), aby moc ze soba walczyc");
playerSetAttackedCreature(attacker, 0);
return;
}

                if (attackedCreature)
				{
					//Tile* fromtile = getTile(creature->pos.x, creature->pos.y, creature->pos.z);
					Tile* fromtile = map->getTile(creature->pos);
					if(fromtile == NULL) {
						std::cout << "checkCreatureAttacking NULL tile: " << creature->getName() << std::endl;
						//return;
					}
					if (!attackedCreature->isAttackable() == 0 && fromtile && fromtile->isPz() && creature->access < g_config.ACCESS_PROTECT)
					{
						Player* player = dynamic_cast<Player*>(creature);
						if (player) {
							player->sendTextMessage(MSG_SMALLINFO, "Niemozesz zaatakowac graczy, ktorzy znajduja sie w strefie ochronnej.");
							//player->sendCancelAttacking();
							playerSetAttackedCreature(player, 0);
							return;
						}
					}
#ifdef YUR_INVISIBLE
					else if (attackedCreature->isInvisible())
					{
						Player* player = dynamic_cast<Player*>(creature);
						if (player) {
							player->sendTextMessage(MSG_SMALLINFO, "Straciles cel ataku z oczu.");
							playerSetAttackedCreature(player, 0);
							return;
						}
					}
#endif //YUR_INVISIBLE
					else
					{
						if (attackedCreature != NULL && attackedCreature->isRemoved == false)
						{
							Player* player = dynamic_cast<Player*>(creature);
							if (player)
							{
#ifdef SD_BURST_ARROW		
								if (player->isUsingBurstArrows())
									burstArrow(player, attackedCreature->pos);
#endif //SD_BURST_ARROW
#ifdef GOLD_BOLT
								if (player->isUsingGoldbolt())
									goldbolt(player, attackedCreature->pos);
#endif //GOLD_BOLT
#ifdef JD_WANDS
								int wandid = player->getWandId();
								if (wandid > 0)
									useWand(player, attackedCreature, wandid);
#endif //JD_WANDS
#ifdef CAYAN_POISONARROW
else if(player->isUsingPoisonArrows()){
                                    if(player->items[SLOT_AMMO] && player->items[SLOT_AMMO]->getID() == 3386){
                                        Item* Arrows = player->getItem(SLOT_AMMO);
                                        if(Arrows->getItemCountOrSubtype() > 1)
                                            Arrows->setItemCountOrSubtype(Arrows->getItemCountOrSubtype()-1);
                                        else
    								        player->removeItemInventory(SLOT_AMMO);
                                        player->sendInventory(SLOT_AMMO);
                                        poisonArrow(creature, attackedCreature->pos);
                                    }
                                }
#endif //CAYAN_POISONARROW
							}
							this->creatureMakeDamage(creature, attackedCreature, creature->getFightType());
						}
					}
Player* player = dynamic_cast<Player*>(creature);
#ifdef BD_FOLLOW

     if(player){
                       if(player->followMode != 0 && attackedCreature && attackedCreature != player->oldAttackedCreature){
                          player->oldAttackedCreature = attackedCreature;
                          playerSetFollowCreature(player, attackedCreature->getID());
                    }else{
                          if(player){
                             player->oldAttackedCreature = NULL;
                             playerSetFollowCreature(player, 0);
                          }
                       }
                    }
#endif //BD_FOLLOW
if (player->vocation == 0) {
int speed = int(g_config.NO_VOC_SPEED * 1000);
creature->eventCheckAttacking = addEvent(makeTask(speed, std::bind2nd(std::mem_fun(&Game::checkCreatureAttacking), id)));
}
else if (player->vocation == 1) {
int speed = int(g_config.SORC_SPEED * 1000);
creature->eventCheckAttacking = addEvent(makeTask(speed, std::bind2nd(std::mem_fun(&Game::checkCreatureAttacking), id)));
}
else if (player->vocation == 2) {
int speed = int(g_config.DRUID_SPEED * 1000);
creature->eventCheckAttacking = addEvent(makeTask(speed, std::bind2nd(std::mem_fun(&Game::checkCreatureAttacking), id)));
}
else if (player->vocation == 3) {
int speed = int(g_config.PALLY_SPEED * 1000);
creature->eventCheckAttacking = addEvent(makeTask(speed, std::bind2nd(std::mem_fun(&Game::checkCreatureAttacking), id)));
}
else if (player->vocation == 4) {
int speed = int(g_config.KNIGHT_SPEED * 1000);
creature->eventCheckAttacking = addEvent(makeTask(speed, std::bind2nd(std::mem_fun(&Game::checkCreatureAttacking), id)));
}
else { //change the 2000 to whatever you want; 2000 = 2 seconds per attack
creature->eventCheckAttacking = addEvent(makeTask(2000, std::bind2nd(std::mem_fun(&Game::checkCreatureAttacking), id)));
}

				}
			}
		}
		flushSendBuffers();
	}


}

void Game::checkDecay(int t)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkDecay()");

	addEvent(makeTask(DECAY_INTERVAL, boost::bind(&Game::checkDecay,this,DECAY_INTERVAL)));

	list<decayBlock*>::iterator it;
	for(it = decayVector.begin();it != decayVector.end();){
		(*it)->decayTime -= t;
		if((*it)->decayTime <= 0){
			list<Item*>::iterator it2;
			for(it2 = (*it)->decayItems.begin(); it2 != (*it)->decayItems.end(); it2++){
				/*todo: Decaying item could be in a  container carried by a player,
				should all items have a pointer to their parent (like containers)?*/
				Item* item = *it2;
				item->isDecaying = false;
				if(item->canDecay()){
					if(item->pos.x != 0xFFFF){
						Tile *tile = map->getTile(item->pos);
						if(tile){
							Position pos = item->pos;
							Item* newitem = item->decay();

							if(newitem){
								int stackpos = tile->getThingStackPos(item);
								if(newitem == item){
									sendUpdateThing(NULL,pos,newitem,stackpos);
								}
								else{
									if(tile->removeThing(item)){
										//autoclose containers
										if(dynamic_cast<Container*>(item)){
											SpectatorVec list;
											SpectatorVec::iterator it;

											getSpectators(Range(pos, true), list);

											for(it = list.begin(); it != list.end(); ++it) {
												Player* spectator = dynamic_cast<Player*>(*it);
												if(spectator)
													spectator->onThingRemove(item);
											}
										}

										tile->insertThing(newitem, stackpos);
										sendUpdateThing(NULL,pos,newitem,stackpos);
										FreeThing(item);
									}
								}
								startDecay(newitem);
							}
							else{
								if(removeThing(NULL,pos,item)){
									FreeThing(item);
								}
							}//newitem
						}//tile
					}//pos != 0xFFFF
				}//item->canDecay()
				FreeThing(item);
			}//for it2
			delete *it;
			it = decayVector.erase(it);
		}//(*it)->decayTime <= 0
		else{
			it++;
		}
	}//for it

	flushSendBuffers();
}

void Game::startDecay(Item* item){
	if(item->isDecaying)
		return;//dont add 2 times the same item
	//get decay time
	item->isDecaying = true;
	unsigned long dtime = item->getDecayTime();
	if(dtime == 0)
		return;
	//round time
	if(dtime < DECAY_INTERVAL)
		dtime = DECAY_INTERVAL;
	dtime = (dtime/DECAY_INTERVAL)*DECAY_INTERVAL;
	item->useThing();
	//search if there are any block with this time
	list<decayBlock*>::iterator it;
	for(it = decayVector.begin();it != decayVector.end();it++){
		if((*it)->decayTime == dtime){
			(*it)->decayItems.push_back(item);
			return;
		}
	}
	//we need a new decayBlock
	decayBlock* db = new decayBlock;
	db->decayTime = dtime;
	db->decayItems.clear();
	db->decayItems.push_back(item);
	decayVector.push_back(db);
}

void Game::checkSpawns(int t)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkSpawns()");

	SpawnManager::instance()->checkSpawns(t);
	this->addEvent(makeTask(t, std::bind2nd(std::mem_fun(&Game::checkSpawns), t)));
}

void Game::CreateDamageUpdate(Creature* creature, Creature* attackCreature, int damage)
{
	Player* player = dynamic_cast<Player*>(creature);
	Player* attackPlayer = dynamic_cast<Player*>(attackCreature);
	if(!player)
		return;
	//player->sendStats();
	//msg.AddPlayerStats(player);
	if (damage > 0) {
		std::stringstream dmgmesg;
        
        if(damage >= 2 && damage <=4)
            dmgmesg << "Straciles " << damage << "punkty zycia";
        
		else if(damage == 1) {
			dmgmesg << "Straciles 1 punkt zycia";
		}
		else if(damage >= 5)
			dmgmesg << "Straciles " << damage << " punktow zycia";

		if(attackPlayer) {
			dmgmesg << " z powodu ataku " << attackCreature->getName();
		}
		else if(attackCreature) {
			std::string strname = attackCreature->getName();
			std::transform(strname.begin(), strname.end(), strname.begin(), (int(*)(int))tolower);
			dmgmesg << " z powodu ataku " << strname;
		}
		dmgmesg <<".";

		player->sendTextMessage(MSG_EVENT, dmgmesg.str().c_str());
		//msg.AddTextMessage(MSG_EVENT, dmgmesg.str().c_str());
	}
	if (player->isRemoved == true){
		player->sendTextMessage(MSG_ADVANCE, "Nie zyjesz.");
	}
}

void Game::CreateManaDamageUpdate(Creature* creature, Creature* attackCreature, int damage)
{
	Player* player = dynamic_cast<Player*>(creature);
	if(!player)
		return;
	//player->sendStats();
	//msg.AddPlayerStats(player);
	if (damage > 0) {
		std::stringstream dmgmesg;
		dmgmesg << "Straciles " << damage << " many";
		if(attackCreature) {
			dmgmesg << " blokujac atak " << attackCreature->getName();
		}
		dmgmesg <<".";

		player->sendTextMessage(MSG_EVENT, dmgmesg.str().c_str());
		//msg.AddTextMessage(MSG_EVENT, dmgmesg.str().c_str());
	}
}

bool Game::creatureSaySpell(Creature *creature, const std::string &text)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureSaySpell()");

	bool ret = false;

	Player* player = dynamic_cast<Player*>(creature);
	std::string temp, var;
	unsigned int loc = (uint32_t)text.find( "\"", 0 );
	if( loc != string::npos && loc >= 0){
		temp = std::string(text, 0, loc-1);
		var = std::string(text, (loc+1), text.size()-loc-1);
	}
	else {
		temp = text;
		var = std::string("");
	}

	std::transform(temp.begin(), temp.end(), temp.begin(), (int(*)(int))tolower);

	if(creature->access >= g_config.ACCESS_PROTECT || !player){
		std::map<std::string, Spell*>::iterator sit = spells.getAllSpells()->find(temp);
		if( sit != spells.getAllSpells()->end() ) {
			sit->second->getSpellScript()->castSpell(creature, creature->pos, var);
			ret = true;
		}
	}
	else if(player){
		std::map<std::string, Spell*>* tmp = spells.getVocSpells(player->vocation);
		if(tmp){
			std::map<std::string, Spell*>::iterator sit = tmp->find(temp);
			if( sit != tmp->end() ) {
				if(player->maglevel >= sit->second->getMagLv()){
#ifdef YUR_LEARN_SPELLS
					if (g_config.LEARN_SPELLS && !player->knowsSpell(temp))
						ret = false;
					else
#endif //YUR_LEARN_SPELLS
					{
						sit->second->getSpellScript()->castSpell(creature, creature->pos, var);
						ret = true;
					}
				}
			}
		}
	}

//	if(temp == "!buyhouse" || temp == "exori mas" || temp == "exani tera") {
//      ret = true;
//    }
// 	if(text == "!buyhouse" || text == "exori mas" || text == "exani tera") {
//      ret = true;
//    }

 	if(temp == "!buyhouse" && g_config.getGlobalString("buyhouse") == "yes") {
      ret = true;
    }

 	if(temp == "exevo mass bolt" || temp == "exori mas" || temp == "exani tera" || temp == "arrow craft" || temp == "alana sio" || temp == "aleta som" || temp == "aleta sio" || temp == "aleta grav" || temp == "aleta gom" || temp == "!moveout" || temp == "!givehouseto" || temp == "!leavehome" || temp == "!leavehouse" || temp == "!vote" || temp == "exani hur up" || temp == "exani hur down" || temp == "!credits" || temp == "!version") {
      ret = true;
    }

	return ret;
}

void Game::playerAutoWalk(Player* player, std::list<Direction>& path)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerAutoWalk()");
	
#ifdef RUL_WALKTO
	if(player->walkTo.type != WALK_NONE) {
        cancelWalkTo(player);
    }
#endif //RUL_WALKTO

	stopEvent(player->eventAutoWalk);

	if(player->isRemoved)
		return;

	player->pathlist = path;
	int ticks = (int)player->getSleepTicks();
/*
#ifdef __DEBUG__
	std::cout << "playerAutoWalk - " << ticks << std::endl;
#endif
*/

	player->eventAutoWalk = addEvent(makeTask(ticks, std::bind2nd(std::mem_fun(&Game::checkPlayerWalk), player->getID())));

	// then we schedule the movement...
  // the interval seems to depend on the speed of the char?
	//player->eventAutoWalk = addEvent(makeTask<Direction>(0, MovePlayer(player->getID()), path, 400, StopMovePlayer(player->getID())));
	//player->pathlist = path;
}

bool Game::playerUseItemEx(Player *player, const Position& posFrom,const unsigned char  stack_from,
		const Position &posTo,const unsigned char stack_to, const unsigned short itemid)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerUseItemEx()");

	if(player->isRemoved)
		return false;

	bool ret = false;

	Position thingpos = getThingMapPos(player, posFrom);
	Item *item = dynamic_cast<Item*>(getThing(posFrom, stack_from, player));

	if(item) {
#ifdef CHAMELEON
             if(item->getID() == ITEM_CHAMELEON) {
            if(posFrom.x != 0xFFFF && (abs(player->pos.x - posFrom.x) > 1 || abs(player->pos.y - posFrom.y) > 1 ||
                    player->pos.z != posFrom.z))
            {
                player->sendCancel("Too far away.");
                sendMagicEffect(player->pos, NM_ME_PUFF);
                return true;
            }
            else if(posTo.x != 0xFFFF && map->canThrowObjectTo(posFrom, posTo, BLOCK_PROJECTILE) != RET_NOERROR) {		
		        player->sendCancel("You cannot throw there.");
		        sendMagicEffect(player->pos, NM_ME_PUFF);
		        return true;
	        }
         }
	        #endif //CHAMELEON
		//Runes
		std::map<unsigned short, Spell*>::iterator sit = spells.getAllRuneSpells()->find(item->getID());
		if(sit != spells.getAllRuneSpells()->end()) {
			if( (abs(thingpos.x - player->pos.x) > 1) || (abs(thingpos.y - player->pos.y) > 1) ) {
				player->sendCancel("Za daleko...");
				ret = false;
			}
			else {
				std::string var = std::string("");
				if(player->access >= g_config.ACCESS_PROTECT || sit->second->getMagLv() <= player->maglevel)
				{
					bool success = sit->second->getSpellScript()->castSpell(player, posTo, var);
					ret = success;
					if(success) {
						autoCloseTrade(item);
						item->setItemCharge(std::max((int)item->getItemCharge() - 1, 0) );
						if(item->getItemCharge() == 0) {
							if(removeThing(player,posFrom,item)){
								FreeThing(item);
							}
						}
					}
				}
				else
				{
					player->sendCancel("Masz za maly magic level, aby uzyc tej runy.");
				}
			}
		}
		else{
			actions.UseItemEx(player,posFrom,stack_from,posTo,stack_to,itemid);
			ret = true;
		}
	}


	return ret;
}

/*
bool Game::playerUseItem(Player *player, const Position& pos, const unsigned char stackpos, const unsigned short itemid, unsigned char index)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerUseItem()");

	if(player->isRemoved)
		return false;
		
	#ifdef TLM_BEDS
if(player->premiumTicks)
{
	if(itemid == 1754 || itemid == 1756 || itemid == 1758 || itemid == 1760){
if( (abs(player->pos.x - pos.x) > 1) || (abs(player->pos.y - pos.y) > 1) || (player->pos.z != pos.z)) {
  player->sendCancel("To far away...");
  return false;
}
if (changeBed(pos, itemid, player->getName())){
           teleport(player, pos);
           player->sendLogout();
           return true;
       }
       else{
           player->sendCancel("Sorry not possible");
           return false;
    }    
  }
  }
  	if(player->isRemoved)
		return false;
#endif //TLM_BEDS
		
	actions.UseItem(player,pos,stackpos,itemid,index);
	return true;
}
*/

bool Game::playerUseItem(Player *player, const Position& pos, const unsigned char stackpos, const unsigned short itemid, unsigned char index, bool via_walkto /* = false*/)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerUseItem()");
if(actions.canUse(player,pos) != TOO_FAR) {	
Item* item = dynamic_cast<Item*>(getThing(pos, stackpos, player));  



			#ifdef CAYAN_SPELLBOOK
		if(itemid == ITEM_SPELL_BOOK){
            if(((abs(player->pos.x - pos.x) > 1) || (abs(player->pos.y - pos.y) > 1) || (player->pos.z != pos.z)) && player->access >= 3){
                player->sendCancel("Sorry, not possible.");
                return false;
            }                  
            else if(setSpellbookText(player, item))
                return true;
        }
#endif //CAYAN_SPELLBOOK
#ifdef TLM_BEDS
if(player->premmium)
{
	if(itemid == 1754 || itemid == 1756 || itemid == 1758 || itemid == 1760){
if( (abs(player->pos.x - pos.x) > 1) || (abs(player->pos.y - pos.y) > 1) || (player->pos.z != pos.z)) {
  player->sendCancel("To far away...");
  return false;
}
if (changeBed(pos, itemid, player->getName())){
           teleport(player, pos);
           player->sendLogout();
           return true;
       }
       else{
           player->sendCancel("Sorry not possible");
           return false;
    }    
  }
  }
  	if(player->isRemoved)
		return false;
#endif //TLM_BEDS
actions.UseItem(player,pos,stackpos,itemid,index);
	}
	#ifdef RUL_WALKTO
	else if(!via_walkto) {
 if(player->walkTo.type != WALK_NONE) {
            cancelWalkTo(player);
        }
        else if(player->eventAutoWalk) {
            stopEvent(player->eventAutoWalk);
        }
        
        player->walkTo.type = WALK_USEITEM;
        player->walkTo.to_pos = pos;
        player->walkTo.stack = stackpos;
        player->walkTo.index = index;
        player->walkTo.itemid = itemid;
        
        player->walkTo.dest_pos = getDestinationPos(player);
        
        int ticks = (int)player->getSleepTicks();
		player->eventAutoWalk = addEvent(makeTask(ticks, std::bind2nd(std::mem_fun(&Game::checkPlayerWalkTo), player->getID())));
		}
#endif //RUL_WALKTO
	return true;
}


bool Game::playerUseBattleWindow(Player *player, Position &posFrom, unsigned char stackpos, unsigned short itemid, unsigned long creatureid)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerUseBattleWindow");

	if(player->isRemoved)
		return false;

	Creature *creature = getCreatureByID(creatureid);
	if(!creature || dynamic_cast<Player*>(creature))
		return false;

	if(std::abs(creature->pos.x - player->pos.x) > 7 || std::abs(creature->pos.y - player->pos.y) > 5 || creature->pos.z != player->pos.z)
		return false;

	bool ret = false;

	Position thingpos = getThingMapPos(player, posFrom);
	Item *item = dynamic_cast<Item*>(getThing(posFrom, stackpos, player));
	if(item) {
		//Runes
		std::map<unsigned short, Spell*>::iterator sit = spells.getAllRuneSpells()->find(item->getID());
		if(sit != spells.getAllRuneSpells()->end()) {
			if( (abs(thingpos.x - player->pos.x) > 1) || (abs(thingpos.y - player->pos.y) > 1) ) {
				player->sendCancel("Za daleko...");
			}
			else {
				std::string var = std::string("");
				if(player->access >= g_config.ACCESS_PROTECT || sit->second->getMagLv() <= player->maglevel)
				{
					bool success = sit->second->getSpellScript()->castSpell(player, creature->pos, var);
					ret = success;
					if(success){
						autoCloseTrade(item);
						item->setItemCharge(std::max((int)item->getItemCharge() - 1, 0) );
						if(item->getItemCharge() == 0){
							if(removeThing(player,posFrom,item)){
								FreeThing(item);
							}
						}
					}
				}
				else
				{
					player->sendCancel("Masz za maly magic level, aby uzyc tej runy.");
				}
			}
		}
	}
	return ret;
}

bool Game::playerRotateItem(Player *player, const Position& pos, const unsigned char stackpos, const unsigned short itemid)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerRotateItem()");

	if(player->isRemoved)
		return false;

	if(std::abs(player->pos.x - pos.x) > 1 || std::abs(player->pos.y - pos.y) > 1 || player->pos.z != pos.z){
		player->sendCancel("Za daleko.");
		return false;
	}

	Item *item = dynamic_cast<Item*>(getThing(pos, stackpos, player));
	if(item && item->rotate()){
		sendUpdateThing(player, pos, item, stackpos);
	}

	return false;
}

void Game::playerRequestTrade(Player* player, const Position& pos,
	const unsigned char stackpos, const unsigned short itemid, unsigned long playerid)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerRequestTrade()");

	if(player->isRemoved)
		return;

	Player *tradePartner = getPlayerByID(playerid);
	if(!tradePartner || tradePartner == player) {
		player->sendTextMessage(MSG_INFO, "To jest niemozliwe.");
		return;
	}

	if(player->tradeState != TRADE_NONE && !(player->tradeState == TRADE_ACKNOWLEDGE && player->tradePartner == playerid)) {
		player->sendCancel("Jestes juz w trakcie wymiany.");
		return;
	}
	else if(tradePartner->tradeState != TRADE_NONE && tradePartner->tradePartner != player->getID()) {
		player->sendCancel("Ten gracz juz z kims handluje.");
		return;
	}

	Item *tradeItem = dynamic_cast<Item*>(getThing(pos, stackpos, player));
	if(!tradeItem || tradeItem->getID() != itemid || !tradeItem->isPickupable()) {
		player->sendCancel("To jest niemozliwe.");
		return;
	}
#ifdef __Xaro_AKT__	
	if(tradeItem->getID() == ITEM_AKT)
	{
       Tile* tile = getTile(player->pos);
       House* house = tile? tile->getHouse() : NULL;
       
       if(!house)
       {
		   player->sendCancel("Musisz stac w domku!");
		   return;
       }     
       if(house->getOwner() != player->getName())
       {
   		player->sendCancel("Musisz stac w swoim domku!");
	    return;
       }
#ifdef _BBK_CODE_
bool bbkcheck = 0;
       if(tradePartner->items[SLOT_AMMO] != NULL){
		if(tradePartner->items[SLOT_RIGHT] != NULL && tradePartner->items[SLOT_RIGHT]->getSlotPosition() & SLOTP_TWO_HAND){
	    bbkcheck = 1;
       }
        if(tradePartner->items[SLOT_LEFT] != NULL && tradePartner->items[SLOT_LEFT]->getSlotPosition() & SLOTP_TWO_HAND){
	    bbkcheck = 1;
       }
        if(tradePartner->items[SLOT_RIGHT] != NULL && tradePartner->items[SLOT_LEFT] != NULL){
	    bbkcheck = 1;
     }
  if(bbkcheck == 1){
        tradePartner->sendCancel("Musisz miec wolna reke lub slot na ammo!");
   		player->sendCancel("Partner nie posiada wolnego miejsca!");
   		return;
              }
         }
#endif //_BBK_CODE_
     }
	
#endif
#ifdef __KIRO_AKT__    
    if(tradeItem->getID() == ITEM_AKT)
    {
       Tile* tile = getTile(player->pos);
       House* house = tile? tile->getHouse() : NULL;
      
       if(!house)
       {
           player->sendCancel("Musisz stac w domku!");
           return;
       }    
       if(house->getOwner() != player->getName())
       {
           player->sendCancel("Musisz stac w swoim domku!");
        return;
       }
     }
    
#endif
	if(!player->removeItem(tradeItem, true)) {
		/*if( (abs(player->pos.x - pos.x) > 1) || (abs(player->pos.y - pos.y) > 1) ) {
			player->sendCancel("To far away...");
			return;
		}*/
		player->sendCancel("To jest niemozliwe.");
		return;
	}

	std::map<Item*, unsigned long>::const_iterator it;
	const Container* container = NULL;
	for(it = tradeItems.begin(); it != tradeItems.end(); it++) {
		if(tradeItem == it->first ||
			((container = dynamic_cast<const Container*>(tradeItem)) && container->isHoldingItem(it->first)) ||
			((container = dynamic_cast<const Container*>(it->first)) && container->isHoldingItem(tradeItem)))
		{
			player->sendTextMessage(MSG_INFO, "Handlujesz juz tym itemem.");
			return;
		}
	}

	Container* tradeContainer = dynamic_cast<Container*>(tradeItem);
	if(tradeContainer && tradeContainer->getItemHoldingCount() + 1 > 100){
		player->sendTextMessage(MSG_INFO, "Nie mozesz wymienic sie wiecej niz 100 przedmiotow naraz.");
		return;
	}

	player->tradePartner = playerid;
	player->tradeItem = tradeItem;
	player->tradeState = TRADE_INITIATED;
	tradeItem->useThing();
	tradeItems[tradeItem] = player->getID();

	player->sendTradeItemRequest(player, tradeItem, true);

     if(tradePartner->tradeState == TRADE_NONE){
       std::stringstream trademsg;
       Tile* tile = getTile(player->pos);
       House* house = tile? tile->getHouse() : NULL;
     if(tradeItem->getID() == ITEM_AKT){
		trademsg << player->getName() <<" chce Ci sprzedac domek " << house->getName() << ".";
     }else{
		trademsg << player->getName() <<" chce z Toba handlowac.";
        }
		tradePartner->sendTextMessage(MSG_INFO, trademsg.str().c_str());
		tradePartner->tradeState = TRADE_ACKNOWLEDGE;
		tradePartner->tradePartner = player->getID();
	}
	else {
		Item* counterOfferItem = tradePartner->tradeItem;
		player->sendTradeItemRequest(tradePartner, counterOfferItem, false);
		tradePartner->sendTradeItemRequest(player, tradeItem, false);
	}
}
/*
void Game::playerAcceptTrade(Player* player)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerAcceptTrade()");

	if(player->isRemoved)
		return;

	player->setAcceptTrade(true);
	Player *tradePartner = getPlayerByID(player->tradePartner);
	if(tradePartner && tradePartner->getAcceptTrade()) {
		Item *tradeItem1 = player->tradeItem;
		Item *tradeItem2 = tradePartner->tradeItem;

		player->sendCloseTrade();
		tradePartner->sendCloseTrade();
		#ifdef __Xaro_AKT__
		if(tradeItem1->getID() == ITEM_AKT)
		{
           Tile* tile = getTile(player->pos);
           House* house = tile? tile->getHouse() : NULL;
           
           if(!house)
           {
    		   player->sendCancel("Musisz stac w domku!");
    		   return;
           }     
           if(house->getOwner() != player->getName())
           {
       		player->sendCancel("Musisz stac w swoim domku!");
    	    return;
           }
#ifdef _BBK_CODE_
bool bbkcheck = 0;
       if(tradePartner->items[SLOT_AMMO] != NULL){
		if(tradePartner->items[SLOT_RIGHT] != NULL && tradePartner->items[SLOT_RIGHT]->getSlotPosition() & SLOTP_TWO_HAND){
	    bbkcheck = 1;
       }
        if(tradePartner->items[SLOT_LEFT] != NULL && tradePartner->items[SLOT_LEFT]->getSlotPosition() & SLOTP_TWO_HAND){
	    bbkcheck = 1;
       }
        if(tradePartner->items[SLOT_RIGHT] != NULL && tradePartner->items[SLOT_LEFT] != NULL){
	    bbkcheck = 1;
     }
  if(bbkcheck == 1){
        tradePartner->sendCancel("Musisz miec wolna reke lub slot na ammo!");
   		player->sendCancel("Partner nie posiada wolnego miejsca!");
   		return;
              }
         }
#endif //_BBK_CODE_
           //if(house->getOwner() != player->getName())
           //{
       		//player->sendCancel("Musisz stac w swoim domku!");
    	    //return;
           //}
#ifdef __KIRO_AKT__
		if(tradeItem1->getID() == ITEM_AKT)
		{
           Tile* tile = getTile(player->pos);
           House* house = tile? tile->getHouse() : NULL;
		   Tile* tile2 = getTile(tradePartner->pos);
           Creature* creature = getCreatureByName(house->getOwner());
		   Player* prevOwner = creature? dynamic_cast<Player*>(creature) : NULL;
           if(!house)
           {
    		   player->sendCancel("Musisz stac w domku!");
    		   return;
           }
           if(!tile->isHouse() && !tile2->isHouse() && player->tradeState != TRADE_INITIATED){
		   player->sendCancel("Akt wlasnosci mozesz sprzedac tylko gdy jestes w domku i jego ownerem!");
		   return;
           }  
           if(house->getOwner() != player->getName())
           {
       		player->sendCancel("Musisz stac w swoim domku!");
    	    return;
           }
           if(house && house->checkHouseCount(tradePartner) >= g_config.getGlobalNumber("maxhouses", 0)){
              std::stringstream textmsg;
              textmsg << " Nie mozesz miec wiecej niz " << g_config.getGlobalNumber("maxhouses", 1) << " domek.";
              tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
              return;    
           }
           if (house && tradePartner->level < g_config.getGlobalNumber("buyhouselvlrook",1) && tradePartner->vocation == 0)
           {
            player->sendCancel("Ten gracz ma za maly poziom aby kupic od Ciebie dom!");
            std::stringstream textmsg;
            textmsg << "Masz za maly poziom aby kupic dom! Musisz posiadac " << g_config.getGlobalNumber("buyhouselvlrook",2) << " poziom.";
            tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
            return;
           }
           if (house && tradePartner->level < g_config.getGlobalNumber("buyhouselvl",1) && tradePartner->vocation != 0)
           {
            player->sendCancel("Ten gracz ma za maly poziom aby kupic od Ciebie dom!");
            std::stringstream textmsg;
            textmsg << "Masz za maly poziom aby kupic dom! Musisz posiadac " << g_config.getGlobalNumber("buyhouselvl",2) << " poziom.";
            tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
            return;
           }
		   if(tradePartner->getFreeCapacity() < tradeItem1->getWeight()) 
		   {
			return;
		   }
		   if(player->addItem(tradeItem2, true) && tradePartner->addItem(tradeItem1, true) &&
			player->removeItem(tradeItem1, true) && tradePartner->removeItem(tradeItem2, true)){

			player->removeItem(tradeItem1);
			tradePartner->removeItem(tradeItem2);

			player->onThingRemove(tradeItem1);
			tradePartner->onThingRemove(tradeItem2);

			player->addItem(tradeItem2);
			tradePartner->addItem(tradeItem1);
		    }
			else{
			player->sendTextMessage(MSG_SMALLINFO, "Nie masz miejsca.");
			tradePartner->sendTextMessage(MSG_SMALLINFO, "Nie masz miejsca.");
            return;
            }
           player->removeItem(tradeItem1, true);
           tradePartner->addItem(tradeItem1, true);
           player->addItem(tradeItem2, true);
           house->setOwner(tradePartner->getName());
           teleport(player,tradePartner->pos);
           if (prevOwner)
              prevOwner->houseRightsChanged = true;
           tradePartner->houseRightsChanged = true;
         }
		else if(tradeItem2->getID() == ITEM_AKT)
		{
           Tile* tile = getTile(tradePartner->pos);
           House* house = tile? tile->getHouse() : NULL;
           Tile* tile2 = getTile(player->pos);
		   Creature* creature = getCreatureByName(house->getOwner());
		   Player* prevOwner = creature? dynamic_cast<Player*>(creature) : NULL;
           if(!house)
           {
    		   tradePartner->sendCancel("Musisz stac w domku!");
    		   return;
           }
           if(!tile->isHouse() && !tile2->isHouse() && player->tradeState != TRADE_INITIATED){
		   player->sendCancel("Akt wlasnosci mozesz sprzedac tylko gdy jestes w domku i jego ownerem!");
		   return;
           }
           if(house->getOwner() != tradePartner->getName())
           {
       		tradePartner->sendCancel("Musisz stac w swoim domku!");
    	    return;
           }
           if(house && house->checkHouseCount(player) >= g_config.getGlobalNumber("maxhouses", 0)){
              std::stringstream textmsg;
              textmsg << " Nie mozesz miec wiecej niz " << g_config.getGlobalNumber("maxhouses", 1) << " domek.";
              tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
              return;    
           }
           if (house && tradePartner->level < g_config.getGlobalNumber("buyhouselvlrook",1) && tradePartner->vocation == 0)
           {
            player->sendCancel("Ten gracz ma za maly poziom aby kupic od Ciebie dom!");
            std::stringstream textmsg;
            textmsg << "Masz za maly poziom aby kupic dom! Musisz posiadac " << g_config.getGlobalNumber("buyhouselvlrook",2) << " poziom.";
            tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
            return;
           }
           if (house && tradePartner->level < g_config.getGlobalNumber("buyhouselvl",1) && tradePartner->vocation != 0)
           {
            player->sendCancel("Ten gracz ma za maly poziom aby kupic od Ciebie dom!");
            std::stringstream textmsg;
            textmsg << "Masz za maly poziom aby kupic dom! Musisz posiadac " << g_config.getGlobalNumber("buyhouselvl",2) << " poziom.";
            tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
            return;
           }
		   if(tradePartner->getFreeCapacity() < tradeItem1->getWeight())
		   {
			return;
		   }
		   if(player->addItem(tradeItem2, true) && tradePartner->addItem(tradeItem1, true) &&
			player->removeItem(tradeItem1, true) && tradePartner->removeItem(tradeItem2, true)){

			player->removeItem(tradeItem1);
			tradePartner->removeItem(tradeItem2);

			player->onThingRemove(tradeItem1);
			tradePartner->onThingRemove(tradeItem2);

			player->addItem(tradeItem2);
			tradePartner->addItem(tradeItem1);
            }
			else{
			player->sendTextMessage(MSG_SMALLINFO, "Nie masz miejsca.");
			tradePartner->sendTextMessage(MSG_SMALLINFO, "Nie masz miejsca.");
            return;
            }
           tradePartner->removeItem(tradeItem1, true);
           player->addItem(tradeItem1, true);
           tradePartner->addItem(tradeItem2, true);
           house->setOwner(player->getName());
           teleport(tradePartner,player->pos);
           if (prevOwner)
           prevOwner->houseRightsChanged = true;
           player->houseRightsChanged = true;
         }
#endif
           player->removeItem(tradeItem1, true);
           tradePartner->addItem(tradeItem1, true);
           player->addItem(tradeItem2, true);
           house->setOwner(tradePartner->getName());
           teleport(player,house->getFrontDoor());
           
         }
		else if(tradeItem2->getID() == ITEM_AKT)
		{
           Tile* tile = getTile(tradePartner->pos);
           House* house = tile? tile->getHouse() : NULL;
           
           if(!house)
           {
    		   tradePartner->sendCancel("Musisz stac w domku!");
    		   return;
           }     
           if(house->getOwner() != tradePartner->getName())
           {
       		tradePartner->sendCancel("Musisz stac w swoim domku!");
    	    return;
           }
#ifdef _BBK_CODE_
bool bbkcheck = 0;
       if(tradePartner->items[SLOT_AMMO] != NULL){
		if(tradePartner->items[SLOT_RIGHT] != NULL && tradePartner->items[SLOT_RIGHT]->getSlotPosition() & SLOTP_TWO_HAND){
	    bbkcheck = 1;
       }
        if(tradePartner->items[SLOT_LEFT] != NULL && tradePartner->items[SLOT_LEFT]->getSlotPosition() & SLOTP_TWO_HAND){
	    bbkcheck = 1;
       }
        if(tradePartner->items[SLOT_RIGHT] != NULL && tradePartner->items[SLOT_LEFT] != NULL){
	    bbkcheck = 1;
     }
  if(bbkcheck == 1){
        tradePartner->sendCancel("Musisz miec wolna reke lub slot na ammo!");
   		player->sendCancel("Partner nie posiada wolnego miejsca!");
   		return;
              }
         }
#endif //_BBK_CODE_
           tradePartner->removeItem(tradeItem1, true);
           player->addItem(tradeItem1, true);
           tradePartner->addItem(tradeItem2, true);
           house->setOwner(player->getName());
           teleport(tradePartner,house->getFrontDoor());
           
         }

#endif


		if(player->addItem(tradeItem2, true) && tradePartner->addItem(tradeItem1, true) &&
			player->removeItem(tradeItem1, true) && tradePartner->removeItem(tradeItem2, true)){

			player->removeItem(tradeItem1);
			tradePartner->removeItem(tradeItem2);

			player->onThingRemove(tradeItem1);
			tradePartner->onThingRemove(tradeItem2);

			player->addItem(tradeItem2);
			tradePartner->addItem(tradeItem1);
		}
		else{
			player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
			tradePartner->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
		}
		
		std::map<Item*, unsigned long>::iterator it;
		
		it = tradeItems.find(tradeItem1);
		if(it != tradeItems.end()) {
			FreeThing(it->first);
			tradeItems.erase(it);
		}
		
		it = tradeItems.find(tradeItem2);
		if(it != tradeItems.end()) {
			FreeThing(it->first);
			tradeItems.erase(it);
		}
		
		player->setAcceptTrade(false);
		tradePartner->setAcceptTrade(false);
	}
}
*/

void Game::playerAcceptTrade(Player* player)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerAcceptTrade()");

	if(player->isRemoved)
		return;

	player->setAcceptTrade(true);
	Player *tradePartner = getPlayerByID(player->tradePartner);
	if(tradePartner && tradePartner->getAcceptTrade()) {
		Item *tradeItem1 = player->tradeItem;
		Item *tradeItem2 = tradePartner->tradeItem;

		player->sendCloseTrade();
		tradePartner->sendCloseTrade();
		
#ifdef __KIRO_AKT__
		if(tradeItem1->getID() == ITEM_AKT)
		{
           Tile* tile = getTile(player->pos);
           House* house = tile? tile->getHouse() : NULL;
		   Tile* tile2 = getTile(tradePartner->pos);
           Creature* creature = getCreatureByName(house->getOwner());
		   Player* prevOwner = creature? dynamic_cast<Player*>(creature) : NULL;
           if(!house)
           {
    		   player->sendCancel("Musisz stac w domku!");
    		   return;
           }
           if(!tile->isHouse() && !tile2->isHouse() && player->tradeState != TRADE_INITIATED){
		   player->sendCancel("Akt wlasnosci mozesz sprzedac tylko gdy jestes w domku i jego ownerem!");
		   return;
           }  
           if(house->getOwner() != player->getName())
           {
       		player->sendCancel("Musisz stac w swoim domku!");
    	    return;
           }
           if(house && house->checkHouseCount(tradePartner) >= g_config.getGlobalNumber("maxhouses", 0)){
              std::stringstream textmsg;
              textmsg << " Nie mozesz miec wiecej niz " << g_config.getGlobalNumber("maxhouses", 1) << " domek.";
              tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
              return;    
           }
           if (house && tradePartner->level < g_config.getGlobalNumber("buyhouselvlrook",1) && tradePartner->vocation == 0)
           {
            player->sendCancel("Ten gracz ma za maly poziom aby kupic od Ciebie dom!");
            std::stringstream textmsg;
            textmsg << "Masz za maly poziom aby kupic dom! Musisz posiadac " << g_config.getGlobalNumber("buyhouselvlrook",2) << " poziom.";
            tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
            return;
           }
           if (house && tradePartner->level < g_config.getGlobalNumber("buyhouselvl",1) && tradePartner->vocation != 0)
           {
            player->sendCancel("Ten gracz ma za maly poziom aby kupic od Ciebie dom!");
            std::stringstream textmsg;
            textmsg << "Masz za maly poziom aby kupic dom! Musisz posiadac " << g_config.getGlobalNumber("buyhouselvl",2) << " poziom.";
            tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
            return;
           }
		   if(tradePartner->getFreeCapacity() < tradeItem1->getWeight()) 
		   {
			return;
		   }
		   if(player->addItem(tradeItem2, true) && tradePartner->addItem(tradeItem1, true) &&
			player->removeItem(tradeItem1, true) && tradePartner->removeItem(tradeItem2, true)){

			player->removeItem(tradeItem1);
			tradePartner->removeItem(tradeItem2);

			player->onThingRemove(tradeItem1);
			tradePartner->onThingRemove(tradeItem2);

			player->addItem(tradeItem2);
			tradePartner->addItem(tradeItem1);
		    }
			else{
			player->sendTextMessage(MSG_SMALLINFO, "Nie masz miejsca.");
			tradePartner->sendTextMessage(MSG_SMALLINFO, "Nie masz miejsca.");
            return;
            }
           player->removeItem(tradeItem1, true);
           tradePartner->addItem(tradeItem1, true);
           player->addItem(tradeItem2, true);
           house->setOwner(tradePartner->getName());
//Pos
        tradePartner->housex = player->housex;
        tradePartner->housey = player->housey;
        tradePartner->housez = player->housez;
        player->housex = 0;
        player->housey = 0;
        player->housez = 0;
        tradePartner->rentTime = player->rentTime;
        player->rentTime = 0;
        tradePartner->rentPrice = player->rentPrice;
        player->rentPrice = 0;
        tradePartner->houseRightsChanged = true;
		player->houseRightsChanged = true;
//
           teleport(player,tradePartner->pos);
           if (prevOwner)
              prevOwner->houseRightsChanged = true;
           tradePartner->houseRightsChanged = true;
         }
		else if(tradeItem2->getID() == ITEM_AKT)
		{
           Tile* tile = getTile(tradePartner->pos);
           House* house = tile? tile->getHouse() : NULL;
           Tile* tile2 = getTile(player->pos);
		   Creature* creature = getCreatureByName(house->getOwner());
		   Player* prevOwner = creature? dynamic_cast<Player*>(creature) : NULL;
           if(!house)
           {
    		   tradePartner->sendCancel("Musisz stac w domku!");
    		   return;
           }
           if(!tile->isHouse() && !tile2->isHouse() && player->tradeState != TRADE_INITIATED){
		   player->sendCancel("Akt wlasnosci mozesz sprzedac tylko gdy jestes w domku i jego ownerem!");
		   return;
           }
           if(house->getOwner() != tradePartner->getName())
           {
       		tradePartner->sendCancel("Musisz stac w swoim domku!");
    	    return;
           }
           if(house && house->checkHouseCount(player) >= g_config.getGlobalNumber("maxhouses", 0)){
              std::stringstream textmsg;
              textmsg << " Nie mozesz miec wiecej niz " << g_config.getGlobalNumber("maxhouses", 1) << " domek.";
              tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
              return;    
           }
           if (house && tradePartner->level < g_config.getGlobalNumber("buyhouselvlrook",1) && tradePartner->vocation == 0)
           {
            player->sendCancel("Ten gracz ma za maly poziom aby kupic od Ciebie dom!");
            std::stringstream textmsg;
            textmsg << "Masz za maly poziom aby kupic dom! Musisz posiadac " << g_config.getGlobalNumber("buyhouselvlrook",2) << " poziom.";
            tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
            return;
           }
           if (house && tradePartner->level < g_config.getGlobalNumber("buyhouselvl",1) && tradePartner->vocation != 0)
           {
            player->sendCancel("Ten gracz ma za maly poziom aby kupic od Ciebie dom!");
            std::stringstream textmsg;
            textmsg << "Masz za maly poziom aby kupic dom! Musisz posiadac " << g_config.getGlobalNumber("buyhouselvl",2) << " poziom.";
            tradePartner->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
            return;
           }
		   if(tradePartner->getFreeCapacity() < tradeItem1->getWeight())
		   {
			return;
		   }
		   if(player->addItem(tradeItem2, true) && tradePartner->addItem(tradeItem1, true) &&
			player->removeItem(tradeItem1, true) && tradePartner->removeItem(tradeItem2, true)){

			player->removeItem(tradeItem1);
			tradePartner->removeItem(tradeItem2);

			player->onThingRemove(tradeItem1);
			tradePartner->onThingRemove(tradeItem2);

			player->addItem(tradeItem2);
			tradePartner->addItem(tradeItem1);
            }
			else{
			player->sendTextMessage(MSG_SMALLINFO, "Nie masz miejsca.");
			tradePartner->sendTextMessage(MSG_SMALLINFO, "Nie masz miejsca.");
            return;
            }
           tradePartner->removeItem(tradeItem1, true);
           player->addItem(tradeItem1, true);
           tradePartner->addItem(tradeItem2, true);
           house->setOwner(player->getName());
//Pos
        player->housex = tradePartner->housex;
        player->housey = tradePartner->housey;
        player->housez = tradePartner->housez;
        tradePartner->housex = 0;
        tradePartner->housey = 0;
        tradePartner->housez = 0;
        player->rentTime = tradePartner->rentTime;
        tradePartner->rentTime = 0;
        player->rentPrice = tradePartner->rentPrice;
        tradePartner->rentPrice = 0;
        player->houseRightsChanged = true;
		tradePartner->houseRightsChanged = true;
//
           teleport(tradePartner,player->pos);
           if (prevOwner)
           prevOwner->houseRightsChanged = true;
           player->houseRightsChanged = true;
         }
#endif
   
		if(player->addItem(tradeItem2, true) && tradePartner->addItem(tradeItem1, true) &&
			player->removeItem(tradeItem1, true) && tradePartner->removeItem(tradeItem2, true)){

			player->removeItem(tradeItem1);
			tradePartner->removeItem(tradeItem2);

			player->onThingRemove(tradeItem1);
			tradePartner->onThingRemove(tradeItem2);

			player->addItem(tradeItem2);
			tradePartner->addItem(tradeItem1);
		}
		else{
			player->sendTextMessage(MSG_SMALLINFO, "Sorry not possible.");
			tradePartner->sendTextMessage(MSG_SMALLINFO, "Sorry not possible.");
		}

		std::map<Item*, unsigned long>::iterator it;

		it = tradeItems.find(tradeItem1);
		if(it != tradeItems.end()) {
			FreeThing(it->first);
			tradeItems.erase(it);
		}

		it = tradeItems.find(tradeItem2);
		if(it != tradeItems.end()) {
			FreeThing(it->first);
			tradeItems.erase(it);
		}

		player->setAcceptTrade(false);
		tradePartner->setAcceptTrade(false);
	}
}
/*
void Game::playerLookInTrade(Player* player, bool lookAtCounterOffer, int index)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerLookInTrade()");

	Player *tradePartner = getPlayerByID(player->tradePartner);
	if(!tradePartner)
		return;

	Item *tradeItem = NULL;

	if(lookAtCounterOffer)
		tradeItem = tradePartner->getTradeItem();
	else
		tradeItem = player->getTradeItem();

	if(!tradeItem)
		return;
#ifdef __Xaro_AKT__
		if(tradeItem->getID() == ITEM_AKT)
		{
           Tile* tile = getTile(tradePartner->pos);
           House* house = tile? tile->getHouse() : NULL;
           
     
           if(house && house->getOwner() == tradePartner->getName())
           {
            stringstream ss;
            ss << "Widzisz " << tradeItem->getDescription(true) << " Dotyczy on domku: " << house->getName() << ".";
            player->sendTextMessage(MSG_INFO, ss.str().c_str());
		    return;
           }
           
         }
#endif

	if(index == 0) {
		stringstream ss;
		ss << "Widzisz " << tradeItem->getDescription(true);
		player->sendTextMessage(MSG_INFO, ss.str().c_str());
		return;
	}

	Container *tradeContainer = dynamic_cast<Container*>(tradeItem);
	if(!tradeContainer || index > tradeContainer->getItemHoldingCount())
		return;

	bool foundItem = false;
	std::list<const Container*> stack;
	stack.push_back(tradeContainer);

	ContainerList::const_iterator it;

	while(!foundItem && stack.size() > 0) {
		const Container *container = stack.front();
		stack.pop_front();

		for (it = container->getItems(); it != container->getEnd(); ++it) {
			Container *container = dynamic_cast<Container*>(*it);
			if(container) {
				stack.push_back(container);
			}

			--index;
			if(index == 0) {
				tradeItem = *it;
				foundItem = true;
				break;
			}
		}
	}

	if(foundItem) {
		stringstream ss;
		ss << "Widzisz " << tradeItem->getDescription(true);
		player->sendTextMessage(MSG_INFO, ss.str().c_str());
	}
}
*/

void Game::playerLookInTrade(Player* player, bool lookAtCounterOffer, int index)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerLookInTrade()");

	Player *tradePartner = getPlayerByID(player->tradePartner);
	if(!tradePartner)
		return;

	Item *tradeItem = NULL;

	if(lookAtCounterOffer)
		tradeItem = tradePartner->getTradeItem();
	else
		tradeItem = player->getTradeItem();

	if(!tradeItem)
		return;
		
#ifdef __KIRO_AKT__
        if(tradeItem->getID() == ITEM_AKT)
        {
           Tile* tile = getTile(tradePartner->pos);
           House* house = tile? tile->getHouse() : NULL;
          
    
           if(house && house->getOwner() == tradePartner->getName())
           {
            stringstream ss;
            ss << "You see " << tradeItem->getDescription(true) << " Dotyczy on domku: " << house->getName() << ".";
            player->sendTextMessage(MSG_INFO, ss.str().c_str());
            return;
           }
          
         }
#endif

	if(index == 0) {
		stringstream ss;
		ss << "You see " << tradeItem->getDescription(true);
		player->sendTextMessage(MSG_INFO, ss.str().c_str());
		return;
	}

	Container *tradeContainer = dynamic_cast<Container*>(tradeItem);
	if(!tradeContainer || index > tradeContainer->getItemHoldingCount())
		return;

	bool foundItem = false;
	std::list<const Container*> stack;
	stack.push_back(tradeContainer);

	ContainerList::const_iterator it;

	while(!foundItem && stack.size() > 0) {
		const Container *container = stack.front();
		stack.pop_front();

		for (it = container->getItems(); it != container->getEnd(); ++it) {
			Container *container = dynamic_cast<Container*>(*it);
			if(container) {
				stack.push_back(container);
			}

			--index;
			if(index == 0) {
				tradeItem = *it;
				foundItem = true;
				break;
			}
		}
	}

	if(foundItem) {
		stringstream ss;
		ss << "You see " << tradeItem->getDescription(true);
		player->sendTextMessage(MSG_INFO, ss.str().c_str());
	}
}

void Game::playerCloseTrade(Player* player)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerCloseTrade()");

	Player* tradePartner = getPlayerByID(player->tradePartner);

	std::vector<Item*>::iterator it;
	if(player->getTradeItem()) {
		std::map<Item*, unsigned long>::iterator it = tradeItems.find(player->getTradeItem());
		if(it != tradeItems.end()) {
			FreeThing(it->first);
			tradeItems.erase(it);
		}
	}

	player->setAcceptTrade(false);
	player->sendTextMessage(MSG_SMALLINFO, "Wymiana anulowana.");
	player->sendCloseTrade();

	if(tradePartner) {
		if(tradePartner->getTradeItem()) {
			std::map<Item*, unsigned long>::iterator it = tradeItems.find(tradePartner->getTradeItem());
			if(it != tradeItems.end()) {
				FreeThing(it->first);
				tradeItems.erase(it);
			}
		}

		tradePartner->setAcceptTrade(false);
		tradePartner->sendTextMessage(MSG_SMALLINFO, "Wymiana anulowana.");
		tradePartner->sendCloseTrade();
	}
}

void Game::autoCloseTrade(const Item* item, bool itemMoved /*= false*/)
{
	if(!item)
		return;

	std::map<Item*, unsigned long>::const_iterator it;
	const Container* container = NULL;
	for(it = tradeItems.begin(); it != tradeItems.end(); it++) {
		if(item == it->first ||
			(itemMoved && (container = dynamic_cast<const Container*>(item)) && container->isHoldingItem(it->first)) ||
			((container = dynamic_cast<const Container*>(it->first)) && container->isHoldingItem(item)))
		{
			Player* player = getPlayerByID(it->second);
			if(player){
				playerCloseTrade(player);
			}

			break;
		}
	}
}

void Game::autoCloseAttack(Player* player, Creature* target)
{
  if((std::abs(player->pos.x - target->pos.x) > 7) ||
  (std::abs(player->pos.y - target->pos.y) > 5) || (player->pos.z != target->pos.z)){
	  player->sendTextMessage(MSG_SMALLINFO, "Straciles cel ataku z oczu.");
	  playerSetAttackedCreature(player, 0);
  }
}

void Game::playerSetAttackedCreature(Player* player, unsigned long creatureid)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerSetAttackedCreature()");

	if(player->isRemoved)
		return;

	if(player->attackedCreature != 0 && creatureid == 0) {
		player->sendCancelAttacking();
	}


	Creature* attackedCreature = NULL;
	if(creatureid != 0) {
		attackedCreature = getCreatureByID(creatureid);
	}

	Player* attackedPlayer = dynamic_cast<Player*>(attackedCreature);
	bool pvpArena = false, rook = false, attackedIsSummon = false;

#ifdef YUR_PVP_ARENA
	if (player && attackedCreature)
	{
		Tile *t1 = map->getTile(player->pos), *t2 = map->getTile(attackedCreature->pos);
		pvpArena = t1 && t2 && t1->isPvpArena() && t2->isPvpArena();
	}
#endif //YUR_PVP_ARENA

#ifdef YUR_ROOKGARD
	rook = player && player->isRookie() && attackedPlayer && attackedPlayer->isRookie();
#endif //YUR_ROOKGARD

#ifdef TR_SUMMONS
	attackedIsSummon = (attackedCreature && attackedCreature->isPlayersSummon() && attackedCreature->getMaster() != player);
#endif //TR_SUMMONS

	if(!attackedCreature || (attackedCreature->access >= g_config.ACCESS_PROTECT || ((getWorldType() == WORLD_TYPE_NO_PVP || rook) &&
		!pvpArena && player->access < g_config.ACCESS_PROTECT && (dynamic_cast<Player*>(attackedCreature) || attackedIsSummon)))) {
	if(attackedCreature) {
		  player->sendTextMessage(MSG_SMALLINFO, "Nie mozesz zaatakowac tego gracza.");
    }
		player->sendCancelAttacking();
		player->setAttackedCreature(NULL);
		stopEvent(player->eventCheckAttacking);
		player->eventCheckAttacking = 0;
	}

	else if(attackedCreature) {
		player->setAttackedCreature(attackedCreature);
		stopEvent(player->eventCheckAttacking);
		player->eventCheckAttacking = addEvent(makeTask(g_config.getGlobalNumber("first",1000), std::bind2nd(std::mem_fun(&Game::checkCreatureAttacking), player->getID())));
	}

}

bool Game::requestAddVip(Player* player, const std::string &vip_name)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::requestAddVip");
	std::string real_name;
	real_name = vip_name;
	unsigned long guid;
	unsigned long access_lvl;

	if(!IOPlayer::instance()->getGuidByName(guid, access_lvl, real_name)){
		player->sendTextMessage(MSG_SMALLINFO, "Gracz z tym nick'iem nie istnieje.");
		return false;
	}
	if(access_lvl > (unsigned long)player->access){
		player->sendTextMessage(MSG_SMALLINFO, "Nie mozesz dodac tego gracza.");
		return false;
	}
	bool online = (getPlayerByName(real_name) != NULL);
	return player->addVIP(guid, real_name, online);
}

void Game::flushSendBuffers()
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::flushSendBuffers()");

	for(std::vector<Player*>::iterator it = BufferedPlayers.begin(); it != BufferedPlayers.end(); ++it) {
		(*it)->flushMsg();
		(*it)->SendBuffer = false;
		(*it)->releaseThing();
/*
#ifdef __DEBUG__
		std::cout << "flushSendBuffers() - releaseThing()" << std::endl;
#endif
*/
		}
	BufferedPlayers.clear();

	//free memory
	for(std::vector<Thing*>::iterator it = ToReleaseThings.begin(); it != ToReleaseThings.end(); ++it){
		(*it)->releaseThing();
	}
	ToReleaseThings.clear();


	return;
}

void Game::addPlayerBuffer(Player* p)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::addPlayerBuffer()");

/*
#ifdef __DEBUG__
	std::cout << "addPlayerBuffer() - useThing()" << std::endl;
#endif
*/
	if(p->SendBuffer == false){
		p->useThing();
		BufferedPlayers.push_back(p);
		p->SendBuffer = true;
	}

	return;
}

void Game::FreeThing(Thing* thing){

	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::FreeThing()");
	//std::cout << "freeThing() " << thing <<std::endl;
	ToReleaseThings.push_back(thing);

	return;
}
/*
ADD
container(player,pos-cid,thing)
inventory(player,pos-i,[ignored])
ground([ignored],postion,thing)

REMOVE
container(player,pos-cid,thing,autoclose?)
inventory(player,pos-i,thing,autoclose?)
ground([ignored],postion,thing,autoclose?,stackpos)

UPDATE
container(player,pos-cid,thing)
inventory(player,pos-i,[ignored])
ground([ignored],postion,thing,stackpos)
*/
void Game::sendAddThing(Player* player,const Position &pos,const Thing* thing){
	if(pos.x == 0xFFFF) {
		if(!player)
			return;
		if(pos.y & 0x40) { //container
			if(!thing)
				return;

			const Item *item = dynamic_cast<const Item*>(thing);
			if(!item)
				return;

			unsigned char containerid = pos.y & 0x0F;
			Container* container = player->getContainer(containerid);
			if(!container)
				return;

			SpectatorVec list;
			SpectatorVec::iterator it;

			Position centerpos = (container->pos.x == 0xFFFF ? player->pos : container->pos);
			getSpectators(Range(centerpos,2,2,2,2,false), list);

			if(!list.empty()) {
				for(it = list.begin(); it != list.end(); ++it) {
					Player *spectator = dynamic_cast<Player*>(*it);
					if(spectator)
						spectator->onItemAddContainer(container,item);
				}
			}
			else
				player->onItemAddContainer(container,item);

		}
		else //inventory
		{
			player->sendInventory(pos.y);
		}
	}
	else //ground
	{
		if(!thing)
			return;

#ifdef SM_SUMMON_ATTACK
		Monster* monster = dynamic_cast<Monster*>(const_cast<Thing*>(thing));
#endif //SM_SUMMON_ATTACK

		SpectatorVec list;
		SpectatorVec::iterator it;

		getSpectators(Range(pos,true), list);

		//players
		for(it = list.begin(); it != list.end(); ++it) {
			if(dynamic_cast<Player*>(*it)) {
				(*it)->onThingAppear(thing);
#ifdef SM_SUMMON_ATTACK
				if (monster && !monster->isSummon())
					monster->onThingAppear(*it);
#endif //SM_SUMMON_ATTACK
			}
		}

		//none-players
		for(it = list.begin(); it != list.end(); ++it) {
			if(!dynamic_cast<Player*>(*it)) {
				(*it)->onThingAppear(thing);
			}
		}
	}
}

void Game::sendRemoveThing(Player* player,const Position &pos,const Thing* thing,const unsigned char stackpos /*=1*/ ,const bool autoclose/* =false*/){
	if(!thing)
		return;

	const Item *item = dynamic_cast<const Item*>(thing);
	bool perform_autoclose = false;
	if(autoclose && item){
		const Container *container = dynamic_cast<const Container*>(item);
		if(container)
			perform_autoclose = true;
	}

	if(pos.x == 0xFFFF) {
		if(!player)
			return;
		if(pos.y & 0x40) { //container
			if(!item)
				return;

			unsigned char containerid = pos.y & 0x0F;
			Container* container = player->getContainer(containerid);
			if(!container)
				return;

			//check that item is in the container
			unsigned char slot = container->getSlotNumberByItem(item);

			SpectatorVec list;
			SpectatorVec::iterator it;

			Position centerpos = (container->pos.x == 0xFFFF ? player->pos : container->pos);
			getSpectators(Range(centerpos,2,2,2,2,false), list);

			if(!list.empty()) {
				for(it = list.begin(); it != list.end(); ++it) {
					Player *spectator = dynamic_cast<Player*>(*it);
					if(spectator){
						spectator->onItemRemoveContainer(container,slot);
						if(perform_autoclose){
							spectator->onThingRemove(thing);
						}
					}
				}
			}
			else{
				player->onItemRemoveContainer(container,slot);
				if(perform_autoclose){
					player->onThingRemove(thing);
				}
			}

		}
		else //inventory
		{
			player->removeItemInventory(pos.y);
			if(perform_autoclose){
				player->onThingRemove(thing);
			}
		}
	}
	else //ground
	{
		SpectatorVec list;
		SpectatorVec::iterator it;

		getSpectators(Range(pos,true), list);

		//players
		for(it = list.begin(); it != list.end(); ++it) {
			Player *spectator = dynamic_cast<Player*>(*it);
			if(spectator) {
				spectator->onThingDisappear(thing,stackpos);

				if(perform_autoclose){
					spectator->onThingRemove(thing);
				}
			}
		}

		//none-players
		for(it = list.begin(); it != list.end(); ++it) {
			if(!dynamic_cast<Player*>(*it)) {
				(*it)->onThingDisappear(thing,stackpos);
			}
		}
	}
}

void Game::sendUpdateThing(Player* player,const Position &pos,const Thing* thing,const unsigned char stackpos/*=1*/){

	if(pos.x == 0xFFFF) {
		if(!player)
			return;
		if(pos.y & 0x40) { //container
			if(!thing)
				return;

			const Item *item = dynamic_cast<const Item*>(thing);
			if(!item)
				return;

			unsigned char containerid = pos.y & 0x0F;
			Container* container = player->getContainer(containerid);
			if(!container)
				return;
			//check that item is in the container
			unsigned char slot = container->getSlotNumberByItem(item);

			SpectatorVec list;
			SpectatorVec::iterator it;

			Position centerpos = (container->pos.x == 0xFFFF ? player->pos : container->pos);
			getSpectators(Range(centerpos,2,2,2,2,false), list);

			if(!list.empty()) {
				for(it = list.begin(); it != list.end(); ++it) {
					Player *spectator = dynamic_cast<Player*>(*it);
					if(spectator)
						spectator->onItemUpdateContainer(container,item,slot);
				}
			}
			else{
				//never should be here
				std::cout << "Error: sendUpdateThing" << std::endl;
				//player->onItemUpdateContainer(container,item,slot);
			}

		}
		else //inventory
		{
			player->sendInventory(pos.y);
		}
	}
	else //ground
	{
		if(!thing)
			return;

		SpectatorVec list;
		SpectatorVec::iterator it;

		getSpectators(Range(pos,true), list);

		//players
		for(it = list.begin(); it != list.end(); ++it) {
			if(dynamic_cast<Player*>(*it)) {
				(*it)->onThingTransform(thing,stackpos);
			}
		}

		//none-players
		for(it = list.begin(); it != list.end(); ++it) {
			if(!dynamic_cast<Player*>(*it)) {
				(*it)->onThingTransform(thing,stackpos);
			}
		}
	}
}

void Game::addThing(Player* player,const Position &pos,Thing* thing)
{
	if(!thing)
		return;
	Item *item = dynamic_cast<Item*>(thing);

	if(pos.x == 0xFFFF) {
		if(!player || !item)
			return;

		if(pos.y & 0x40) { //container
			unsigned char containerid = pos.y & 0x0F;
			Container* container = player->getContainer(containerid);
			if(!container)
				return;

			container->addItem(item);
			sendAddThing(player,pos,thing);
		}
		else //inventory
		{
			player->addItemInventory(item,pos.y,true);
			sendAddThing(player,pos,thing);
		}
	}
	else //ground
	{
		if(!thing)
			return;
		//Tile *tile = map->getTile(pos.x, pos.y, pos.z);
		Tile *tile = map->getTile(pos);
		if(tile){
			thing->pos = pos;
			if(item && item->isSplash()){
				if(tile->splash){
					int oldstackpos = tile->getThingStackPos(tile->splash);
					Item *oldsplash = tile->splash;

					oldsplash->isRemoved = true;
					FreeThing(oldsplash);

					tile->splash = item;

					sendUpdateThing(NULL, pos, item, oldstackpos);
				}
				else{
					tile->splash = item;
					sendAddThing(NULL,pos,tile->splash);
				}
			}
			else if(item && item->isGroundTile()){
				tile->ground = item;

				SpectatorVec list;
				SpectatorVec::iterator it;

				getSpectators(Range(thing->pos, true), list);

				//players
				for(it = list.begin(); it != list.end(); ++it) {
					if(dynamic_cast<Player*>(*it)) {
						(*it)->onTileUpdated(pos);
					}
				}

				//none-players
				for(it = list.begin(); it != list.end(); ++it) {
					if(!dynamic_cast<Player*>(*it)) {
						(*it)->onTileUpdated(pos);
					}
				}

				//Game::creatureBroadcastTileUpdated(thing->pos);
			}
			else if(item && item->isStackable()){
				Item *topitem = tile->getTopDownItem();
				if(topitem && topitem->getID() == item->getID() &&
				  topitem->getItemCountOrSubtype() + item->getItemCountOrSubtype() <= 100){
					topitem->setItemCountOrSubtype(topitem->getItemCountOrSubtype() + item->getItemCountOrSubtype());
					int stackpos = tile->getThingStackPos(topitem);
					sendUpdateThing(NULL,topitem->pos,topitem,stackpos);
					item->pos.x = 0xFFFF;
					FreeThing(item);
				}
				else{
					tile->addThing(thing);
					sendAddThing(player,pos,thing);
				}
			}
			else{
				tile->addThing(thing);
				sendAddThing(player,pos,thing);
			}
		}
	}
}

bool Game::removeThing(Player* player,const Position &pos,Thing* thing,  bool setRemoved /*= true*/)
{
	if(!thing)
		return false;
	Item *item = dynamic_cast<Item*>(thing);

	if(pos.x == 0xFFFF) {
		if(!player || !item)
			return false;

		if(pos.y & 0x40) { //container
			unsigned char containerid = pos.y & 0x0F;
			Container* container = player->getContainer(containerid);
			if(!container)
				return false;

			sendRemoveThing(player,pos,thing,0,true);
			if(!container->removeItem(item))
				return false;

			if(player && player->isHoldingContainer(container)) {
				player->updateInventoryWeigth();
				player->sendStats();
			}
		}
		else //inventory
		{
			//sendRemoveThing(player,pos,thing,0,true);
			if(!player->removeItemInventory(pos.y))
				return false;
			player->onThingRemove(thing);
			//player->removeItemInventory(pos.y,true);
		}
		if(setRemoved)
			item->isRemoved = true;
		return true;
	}
	else //ground
	{
		//Tile *tile = map->getTile(pos.x, pos.y, pos.z);
		Tile *tile = map->getTile(pos);
		if(tile){
			unsigned char stackpos = tile->getThingStackPos(thing);
			if(!tile->removeThing(thing))
				return false;
			sendRemoveThing(NULL,pos,thing,stackpos,true);
		}
		else{
			return false;
		}
		if(item && setRemoved){
			item->isRemoved = true;
		}
		return true;
	}
}

Position Game::getThingMapPos(Player *player, const Position &pos)
{
	if(pos.x == 0xFFFF){
		Position dummyPos(0,0,0);
		if(!player)
			return dummyPos;
		if(pos.y & 0x40) { //from container
			unsigned char containerid = pos.y & 0x0F;
			const Container* container = player->getContainer(containerid);
			if(!container){
				return dummyPos;
			}
			while(container->getParent() != NULL) {
				container = container->getParent();
			}
			if(container->pos.x == 0xFFFF)
				return player->pos;
			else
				return container->pos;
		}
		else //from inventory
		{
			return player->pos;
		}
	}
	else{
		return pos;
	}
}

Thing* Game::getThing(const Position &pos,unsigned char stack, Player* player /*=NULL*/)
{
	if(pos.x == 0xFFFF) {
		if(!player)
			return NULL;
		if(pos.y & 0x40) { //from container
			unsigned char containerid = pos.y & 0x0F;
			Container* container = player->getContainer(containerid);
			if(!container)
				return NULL;

			return container->getItem(pos.z);
		}
		else //from inventory
		{
			return player->getItem(pos.y);
		}
	}
	else //from ground
	{
		//Tile *t = getTile(pos.x, pos.y, pos.z);
		Tile *t = map->getTile(pos);
		if(!t)
			return NULL;

		return t->getThingByStackPos(stack);
	}
}

int Game::getDepot(Container* c, int e)
{
for(int a = 0; a < c->size(); a++)
{
Container* x = dynamic_cast<Container*>(dynamic_cast<Item*>(c->getItem(a)));
Item* i = dynamic_cast<Item*>(c->getItem(a));
if(i)
e++;
if(x)
e = getDepot(x, e);
}
return e;
}

#ifdef WOLV_LOAD_NPC
bool Game::loadNpcs()
{
	xmlDocPtr doc;
	doc = xmlParseFile((g_config.DATA_DIR + "world/npc.xml").c_str());
	if (!doc)
		return false; 

	xmlNodePtr root, npcNode;
	root = xmlDocGetRootElement(doc);

	if (xmlStrcmp(root->name, (const xmlChar*)"npclist")) 
	{
		xmlFreeDoc(doc);
		return false;
	}

	npcNode = root->children;
	while (npcNode)
	{
		if (strcmp((const char*) npcNode->name, "npc") == 0)
		{
			std::string name = (const char*)xmlGetProp(npcNode, (const xmlChar *) "name");
			int x = atoi((const char*) xmlGetProp(npcNode, (const xmlChar*) "x"));
			int y = atoi((const char*) xmlGetProp(npcNode, (const xmlChar*) "y"));
			int z = atoi((const char*) xmlGetProp(npcNode, (const xmlChar*) "z"));
	
			Npc* mynpc = new Npc(name, this);
			mynpc->pos = Position(x, y, z);

			if (!placeCreature(mynpc->pos, mynpc))
			{
				std::cout << "Nie moge ustawic " << name << "!" << std::endl;
				xmlFreeDoc(doc);
				return false;
			}

			const char* tmp = (const char*)xmlGetProp(npcNode, (const xmlChar*) "dir");
			if (tmp)
				mynpc->setDirection((Direction)atoi(tmp));
		}  
		npcNode = npcNode->next;
	}

	xmlFreeDoc(doc);
	return true;
}
#endif //WOLV_LOAD_NPC


#ifdef TLM_SERVER_SAVE
void Game::serverSave()
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::serverSave()");
	std::cout << ":: zapis servera... ";
	timer();

	AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
	while (it != Player::listPlayer.list.end())
	{
		IOPlayer::instance()->savePlayer(it->second);
		++it;
	}
            //for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
            //(*it).second->sendTextMessage(MSG_ADVANCE, "Zapis serwera wykonany pomyslnie");
            
	Guilds::Save();
	Houses::Save(this);
	loginQueue.save();
	std::cout << "ok (" << timer() << "s)" << std::endl;
}

void Game::autoServerSave()
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::autoServerSave()");
	serverSave();
	addEvent(makeTask(g_config.getGlobalNumber("autosave", 1)*60000, std::mem_fun(&Game::autoServerSave)));
}
#endif //TLM_SERVER_SAVE


#ifdef ELEM_VIP_LIST
void Game::vipLogin(Player* player)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::vipLogin()");
	std::string vipname = player->getName();
#ifdef _NG_BBK_VIP_SYSTEM__
std::transform(vipname.begin(), vipname.end(), vipname.begin(), (int(*)(int)) std::tolower);
#endif //_NG_BBK_VIP_SYSTEM__

	for(AutoList<Creature>::listiterator cit = listCreature.list.begin(); cit != listCreature.list.end(); ++cit)
	{
		Player* player = dynamic_cast<Player*>((*cit).second);
		if (player)
			player->sendVipLogin(vipname);
	}
}

void Game::vipLogout(std::string vipname)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::vipLogout()");
#ifdef _NG_BBK_VIP_SYSTEM__
	std::transform(vipname.begin(), vipname.end(), vipname.begin(), (int(*)(int)) std::tolower);
#endif //_NG_BBK_VIP_SYSTEM__
	for(AutoList<Creature>::listiterator cit = listCreature.list.begin(); cit != listCreature.list.end(); ++cit)
	{
		Player* player = dynamic_cast<Player*>((*cit).second);
		if (player)
			player->sendVipLogout(vipname);
	}
}

bool Game::isPlayer(std::string name)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::isPlayer()");
	extern xmlMutexPtr xmlmutex;

	std::string datadir = g_config.getGlobalString("datadir");
	std::string filenamecheck = datadir + "players/" + name + ".xml";
	std::transform(filenamecheck.begin(),filenamecheck.end(), filenamecheck.begin(), (int(*)(int))tolower);

	xmlDocPtr doc;
	xmlMutexLock(xmlmutex);
	doc = xmlParseFile(filenamecheck.c_str());

	if (doc)
	{
		xmlMutexUnlock(xmlmutex);
		xmlFreeDoc(doc);
		return true;
	}
	else
	{
		xmlMutexUnlock(xmlmutex);
		xmlFreeDoc(doc);
		return false;
	}
}
#endif //ELEM_VIP_LIST

void Game::checkSpell(Player* player, SpeakClasses type, std::string text)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkSpell()");

	if (player->isRookie())
	{
		return;
	}
	
	#ifdef BUY_HOUSE
	    else if(text == "!buyhouse" && g_config.getGlobalString("buyhouse") == "yes")
        {
  unsigned long money = player->getMoney(); 
  bool last = false;
  for (int x = player->pos.x-1; x <= player->pos.x+1 && !last; x++)
  {
   for(int y = player->pos.y-1; y <= player->pos.y+1 && !last; y++)
   {
    Position doorPos(x, y, player->pos.z);
    Tile* tile = getTile(doorPos);
    House* house = tile? tile->getHouse() : NULL;
    
    if (house && house->getPlayerRights(player->getName()) == HOUSE_OWNER){
              player->sendTextMessage(MSG_ADVANCE, "You own this house.");
              return;
    }
//            if (player && player->premmium == false){
//              player->sendTextMessage(MSG_ADVANCE, "You dont have a premium account.");
//              return;
//    }
    if (house && player->level < g_config.LEVEL_HOUSE){
              player->sendTextMessage(MSG_ADVANCE, "You need higher level to buy house.");
              return;
    }
    if (house && house->isBought()){
              player->sendTextMessage(MSG_ADVANCE, "This house already has an owner.");
              return;
    }
    if(house && house->checkHouseCount(player) >= g_config.getGlobalNumber("maxhouses", 0)){
              std::stringstream textmsg;
              textmsg << " You cant have more than " << g_config.getGlobalNumber("maxhouses", 1) << " houses ";
              player->sendTextMessage(MSG_ADVANCE, textmsg.str().c_str());
              return;    
    }
    if (house && house->getPlayerRights(doorPos, player->getName()) == HOUSE_NONE && !house->isBought() && house->checkHouseCount(player) < g_config.getGlobalNumber("maxhouses", 1))
    {
     Item *item = dynamic_cast<Item*>(tile->getThingByStackPos(tile->getThingCount()-1));
     long price = g_config.getGlobalNumber("priceforsqm", 0) * house->getHouseSQM(house->getName());
     if (item && Item::items[item->getID()].isDoor && price <= money)
     {
//doorPos
        player->housex = doorPos.x;
        player->housey = doorPos.y;
        player->housez = doorPos.z;
        //long time = (long long)(OTSYS_TIME()/1000);
        time_t seconds;
        seconds = time(NULL);
        int time = seconds/3600;
        long housetime = (long)(g_config.getGlobalNumber("housetime",186));
        tile->getHouse()->setLast(time+housetime);
        //player->rentTime = time+housetime;
        player->rentPrice = price;
		player->houseRightsChanged = true;
//
      player->substractMoney(price);
      house->setOwner(player->getName());
      house->save();
      player->sendTextMessage(MSG_ADVANCE, "You bought a house. Remember in order to pay rent of house.");
      last = true;
     }
     else
     {
     player->sendMagicEffect(player->pos, NM_ME_PUFF);
     player->sendTextMessage(MSG_SMALLINFO, "You dont have enough money to buy this house.");
     }
   }
  }
 }
}
	#endif //BUY_HOUSE

 else if (text == "!leavehouse" || text == "!leavehome" || text == "!moveout") // leave your house
 {
  Tile* tile = getTile(player->pos);
  House* house = tile? tile->getHouse() : NULL;
  if (house && house->getPlayerRights(player->getName()) == HOUSE_OWNER)
  {
            house->setOwner("");
            house->setGuests("");
            house->setSubOwners("");
            
//doorPos
        player->housex = 0;
        player->housey = 0;
        player->housez = 0;
        player->rentTime = 0;
        tile->getHouse()->setLast(0);
        player->rentPrice = 0;
		player->houseRightsChanged = true;
//

            house->save();
            teleport(player, player->masterPos);
            player->sendMagicEffect(player->pos, NM_ME_ENERGY_AREA);
            player->sendTextMessage(MSG_INFO, "You have left your house.");
  }
 }
 else if (text.substr(0, 13) == "!givehouseto ") // trade your house
 {
  Creature* c = getCreatureByName(text.substr(13).c_str());
  Player *target = c? dynamic_cast<Player*>(c) : NULL;
        Tile* tile = getTile(player->pos);
        House* house = tile? tile->getHouse() : NULL;
  if (target)
  {
            if(house)
            {
                if(house->getPlayerRights(player->getName()) != HOUSE_OWNER)
                {
                    std::stringstream textmsg;
                    textmsg << "Only the house owner may give this house to someone!";
                    player->sendTextMessage(MSG_SMALLINFO, textmsg.str().c_str());
                    player->sendMagicEffect(player->pos, NM_ME_PUFF);
                    return;
                }
                else if(house->checkHouseCount(target) >= g_config.getGlobalNumber("maxhouses", 0))
                {
                    std::stringstream textmsg;
                    textmsg << "Target has reached the max number of houses a player can have.";
                    player->sendTextMessage(MSG_SMALLINFO, textmsg.str().c_str());
                    player->sendMagicEffect(player->pos, NM_ME_PUFF);
                    return;
                }
       else if(house->getPlayerRights(player->getName()) == HOUSE_OWNER)
       {
                    house->setOwner(target->getName());
                    house->setGuests("");
                    house->setSubOwners("");
 //doorPos
        target->housex = player->housex;
        target->housey = player->housey;
        target->housez = player->housez;
        target->rentTime = player->rentTime;
        target->rentPrice = player->rentPrice;
        player->housex = 0;
        player->housey = 0;
        player->housez = 0;
        player->rentTime = 0;
        player->rentPrice = 0;
		player->houseRightsChanged = true;
		target->houseRightsChanged = true;
//
                    house->save();
                    teleport(player, player->masterPos);
                    player->sendMagicEffect(player->pos, NM_ME_ENERGY_AREA);
                    std::stringstream textmsg;
                    textmsg << "You gave you house to " << target->getName() << ".";
                    player->sendTextMessage(MSG_INFO, textmsg.str().c_str());
                }
            }
        }
        else
        {
            player->sendTextMessage(MSG_SMALLINFO, "Sorry not possible.");
            player->sendMagicEffect(player->pos, NM_ME_PUFF);
        }
    }

#ifdef TLM_HOUSE_SYSTEM
    
    else if (text == "aleta gom")		// edit owner
	{
		Tile* tile = getTile(player->pos);
		House* house = tile? tile->getHouse() : NULL;

		if (house && house->getPlayerRights(player->getName()) == HOUSE_OWNER)
		{
			player->sendHouseWindow(house, player->pos, HOUSE_OWNER);
		}
		else
		{
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
		}
	}
	else if (text == "aleta grav")		// edit door owners
	{
		bool last = false;
		for (int x = player->pos.x-1; x <= player->pos.x+1 && !last; x++)
		{
			for(int y = player->pos.y-1; y <= player->pos.y+1 && !last; y++)
			{
				Position doorPos(x, y, player->pos.z);
				Tile* tile = getTile(doorPos);
				House* house = tile? tile->getHouse() : NULL;

				if (house && house->getPlayerRights(doorPos, player->getName()) == HOUSE_OWNER)
				{
					Item *item = dynamic_cast<Item*>(tile->getThingByStackPos(tile->getThingCount()-1));
					if (item && Item::items[item->getID()].isDoor)
					{
						player->sendHouseWindow(house, doorPos, HOUSE_DOOROWNER);
						last = true;
					}
				}
			}
		}
	}
	else if (text == "aleta sio")		// edit guests
	{
		Tile* tile = getTile(player->pos);
		House* house = tile? tile->getHouse() : NULL;

		if (house && house->getPlayerRights(player->getName()) >= HOUSE_SUBOWNER)
		{
			player->sendHouseWindow(house, player->pos, HOUSE_GUEST);
		}
		else
		{
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
		}
	}
	else if (text == "aleta som")		// edit subowners
	{
		Tile* tile = getTile(player->pos);
		House* house = tile? tile->getHouse() : NULL;

		if (house && house->getPlayerRights(player->getName()) == HOUSE_OWNER)
		{
			player->sendHouseWindow(house, player->pos, HOUSE_SUBOWNER);
		}
		else
		{
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
		}
	}
	else if (text == "alana sio")	// kick me
	{
		Tile* tile = getTile(player->pos);
		House* house = tile? tile->getHouse() : NULL;

		if (house)
		{
			teleport(player, house->getFrontDoor());
			player->sendMagicEffect(player->pos, NM_ME_ENERGY_AREA);
		}
		else
		{
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendTextMessage(MSG_SMALLINFO, "Musisz stac w domku.");
		}
	}
	else if (text.substr(0, 11) == "alana sio \"")	// kick someone
	{
		Creature* c = getCreatureByName(text.substr(11).c_str());
		Player *target = c? dynamic_cast<Player*>(c) : NULL;

		if (target)
		{
			Tile* tile = getTile(player->pos);
			Tile* targetTile = getTile(target->pos);
			House* house = tile? tile->getHouse() : NULL;
			House* targetHouse = targetTile? targetTile->getHouse() : NULL;

			if (house && targetHouse && house == targetHouse &&
				house->getPlayerRights(player->getName()) >= HOUSE_SUBOWNER)
			{
				Position pos = house->getFrontDoor();
				if (pos.x != 0xFFFF && pos.y != 0xFFFF && pos.z != 0xFF)
				{
					teleport(target, pos);
					player->sendMagicEffect(target->pos, NM_ME_ENERGY_AREA);
				}
				else
				{
					player->sendMagicEffect(player->pos, NM_ME_PUFF);
					player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
				}
			}
			else
				player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
		}
		else
		{
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendTextMessage(MSG_SMALLINFO, "Ten gracz nie istnieje.");
		}
	}
#endif //TLM_HOUSE_SYSTEM

else if(text == "arrow craft" && (player->vocation == VOCATION_PALADIN))
    {
         if(player && player->items[SLOT_AMMO] && (player->items[SLOT_AMMO]->getID() != ITEM_CATALYST_ARROW || player->items[SLOT_AMMO]->getID() != ITEM_CATALYST_FIRE_ARROW || player->items[SLOT_AMMO]->getID() != ITEM_CATALYST_POISONED_ARROW || player->items[SLOT_AMMO]->getID() != ITEM_CATALYST_BOLT || player->items[SLOT_AMMO]->getID() != ITEM_CATALYST_POWER_BOLT))
         {        
         int count = 1;
         int countb = 100;          
 		           if(player->items[SLOT_AMMO]->getID() == ITEM_CATALYST_ARROW)
                   {
                   int itemid = 2544;
                   player->TLMaddItem(itemid, countb);
                   player->removeItem(ITEM_CATALYST_ARROW, count);
                   }
                   else if(player->items[SLOT_AMMO]->getID() == ITEM_CATALYST_FIRE_ARROW)
                   {
                   int itemid = 2546;
                   player->TLMaddItem(itemid, countb);
                   player->removeItem(ITEM_CATALYST_FIRE_ARROW, count);     
                   }
                   else if(player->items[SLOT_AMMO]->getID() == ITEM_CATALYST_POISONED_ARROW)
                   {
                   int itemid = 2545;
                   player->TLMaddItem(itemid, countb);
                   player->removeItem(ITEM_CATALYST_POISONED_ARROW, count);                        
                   }
                   else if(player->items[SLOT_AMMO]->getID() == ITEM_CATALYST_BOLT)
                   {
                   int itemid = 2543;
                   player->TLMaddItem(itemid, countb);
                   player->removeItem(ITEM_CATALYST_BOLT, count);                     
                   }
                   else
                   {
                   int itemid = 2547;
                   player->TLMaddItem(itemid, countb);
                   player->removeItem(ITEM_CATALYST_POWER_BOLT, count);                       
                   }
         player->sendTextMessage(MSG_INFO, "AMMO CREATED!!.");                  
         }
         else
         {
         player->sendTextMessage(MSG_INFO, "You dont have any catalyst.");
         player->sendTextMessage(MSG_SMALLINFO, "If you have catalyst. put in ammo slot");
         }
    }

#ifdef TR_SUMMONS
	else if (text.substr(0, 11) == "utevo res \"" && 
		(!g_config.LEARN_SPELLS || player->knowsSpell("utevo res")))
	{
		if (player->vocation == VOCATION_DRUID || player->vocation == VOCATION_SORCERER ||
			(g_config.SUMMONS_ALL_VOC && player->vocation != VOCATION_NONE))
		{
			std::string name = text.substr(11);
			int reqMana = Summons::getRequiredMana(name);
			Tile* tile = getTile(player->pos);

			if (!tile)
			{
				player->sendMagicEffect(player->pos, NM_ME_PUFF);
				player->sendCancel("To jest niemozliwe.");
			}
			else if (reqMana < 0)
			{
				player->sendMagicEffect(player->pos, NM_ME_PUFF);
				player->sendCancel("Nie mozesz przyzwac tego potwora.");
			}
			else if (tile->isPz())
			{
				player->sendMagicEffect(player->pos, NM_ME_PUFF);
				player->sendCancel("Nie mozesz przyzywac potworow w strefie ochronnej.");
			}
#ifdef YUR_PVP_ARENA
			else if (tile->isPvpArena())
			{
				player->sendMagicEffect(player->pos, NM_ME_PUFF);
				player->sendCancel("Nie mozesz przyzywac potworow na arenie.");
			}
#endif //YUR_PVP_ARENA
			else if (player->getSummonCount() >= g_config.MAX_SUMMONS)
			{
				player->sendMagicEffect(player->pos, NM_ME_PUFF);
				player->sendCancel("Nie mozesz przyzwac wiecej potworow. Osiagnoles maksymalny limit.");
			}
			else if (player->getMana() < reqMana)
			{
				player->sendMagicEffect(player->pos, NM_ME_PUFF);
				player->sendCancel("Masz za malo many, aby przyzwac tego potwora.");
			}
			else if (!placeSummon(player, name))
			{
				player->sendMagicEffect(player->pos, NM_ME_PUFF);
				player->sendCancel("Za malo miejsca");
			}
			else
			{
				player->mana -= reqMana;
				player->addManaSpent(reqMana);
			}
		}
	}
#endif //TR_SUMMONS

#ifdef BRN_EXIVA
	else if (text.substr(0,7) == "exiva \"" &&
		(!g_config.LEARN_SPELLS || player->knowsSpell("exiva")))
	{
		std::string name = text.substr(7);
		Creature *c = getCreatureByName(name);
#ifdef TIJN_WILDCARD
                std::string namecard;
        if(hasWildcard(name)) {//Testing if name have ~ inside him
namecard = getNameByWildcard(name);
if(namecard != "") {
c = getCreatureByName(namecard);
}
else if(namecard == ".ambiguous") {
//ambiguous
std::cout << "Player not found!!!" << std::endl;
return;
}
else {
//name doesn't exist
player->sendCancel("Sorry, this player ist not online");
return;
}
}

else {
//not a wildcard - continue normally
c = getCreatureByName(name);
}
#endif //TIJN_WILDCARD	
		if (dynamic_cast<Player*>(c))
		{
			if(player->mana >= 20)
			{
				player->mana -= 20;
				player->addManaSpent(20);
			}
			else
			{
				player->sendMagicEffect(player->pos, NM_ME_PUFF);
				player->sendTextMessage(MSG_SMALLINFO, "Masz za malo many, aby odnalezc ta osobe.");
				return;
			}
			 if(c->access >= g_config.ACCESS_PROTECT)
			{
				player->sendMagicEffect(player->pos, NM_ME_PUFF);
				player->sendTextMessage(MSG_SMALLINFO, "Nie mozesz odnalezc osoby z zalogi OTS.");
                return;
			}

			int x = c->pos.x - player->pos.x;
			int y = c->pos.y - player->pos.y;
			int z = c->pos.z - player->pos.z;
			std::stringstream position;
			position << name;

			if((x > 48 && y > 24) || (x > 24 && y > 48) || (x > 36 && y > 36))
				position << " jest daleko na poludniowym-wschodzie.";
			else if((x > 48 && y < -24) || (x > 24 && y < -48) || (x > 36 && y < -36))
				position << " jest daleko na polnocnym-wschodzie.";
			else if((x < -48 && y > 24) || (x < -24 && y > 48) || (x < -36 && y > 36))
				position << " jest daleko na poludniowym-zachodzie.";
			else if((x < -48 && y < -24) || (x < -24 && y < -48) || (x < -36 && y < -36))
				position << " jest daleko na polnocnym-zachodzie.";

			else if((x > 6 && y > 12 && z > 0) || (x > 12 && y > 6 && z > 0) || (x > 9 && y > 9 && z > 0))
				position << " jest na nizszym poziomie na poludniowy-wschod od ciebie.";
			else if((x > 6 && y < -12 && z > 0) || (x > 12 && y < -6 && z > 0) || (x > 9 && y < -9 && z > 0))
				position << " jest na nizszym poziomie na polnocny-wschod od ciebie.";
			else if((x < -6 && y > 12 && z > 0) || (x < -12 && y > 6 && z > 0) || (x < -9 && y > 9 && z > 0))
				position << " jest na nizszym poziomie na poludniowy-zachod od ciebie.";
			else if((x < -6 && y < -12 && z > 0) || (x < -12 && y < -6 && z > 0) || (x < -9 && y < -9 && z > 0))
				position << " jest na nizszym poziomie na polnocny-zachod od ciebie.";

			else if((x > 6 && y > 12 && z < 0) || (x > 12 && y > 6 && z < 0) || (x > 9 && y > 9 && z < 0))
				position << " jest na wyzszym poziomie na poludniowy-wschod od ciebie.";
			else if((x > 6 && y < -12 && z < 0) || (x > 12 && y < -6 && z < 0) || (x > 9 && y < -9 && z < 0))
				position << " jest na wyzszym poziomie na polnocny-wschod od ciebie.";
			else if((x < -6 && y > 12 && z < 0) || (x < -12 && y > 6 && z < 0) || (x < -9 && y > 9 && z < 0))
				position << " jest na wyzszym poziomie na poludniowy-zachod od ciebie.";
			else if((x < -6 && y < -12 && z < 0) || (x < -12 && y < -6 && z < 0) || (x < -9 && y < -9 && z < 0))
				position << " jest na wyzszym poziomie na polnocny-zachod od ciebie.";

			else if((x > 6 && y > 12 && z == 0) || (x > 12 && y > 6 && z == 0) || (x > 9 && y > 9 && z == 0))
				position << " jest niedaleko na poludniowym-wschodzie z tad.";
			else if((x > 6 && y < -12 && z == 0) || (x > 12 && y < -6 && z == 0) || (x > 9 && y < -9 && z == 0))
				position << " jest niedaleko na polnocnym-wschodzie z tad.";
			else if((x < -6 && y > 12 && z == 0) || (x < -12 && y > 6 && z == 0) || (x < -9 && y > 9 && z == 0))
				position << " jest niedaleko na poludniowym-zachodzie z tad.";
			else if((x < -6 && y < -12 && z == 0) || (x < -12 && y < -6 && z == 0) || (x < -9 && y < -9 && z == 0))
				position << " jest niedaleko na polnocnym-zachodzie z tad.";

			else if(x > 36)
				position << " jest daleko na wschodzie.";
			else if(x < -36)
				position << " jest daleko na zachodzie.";
			else if(y > 36)
				position << " jest daleko na poludniu.";
			else if(y < -36)
				position << " jest daleko na polnocy.";

			else if(x > 3 && z < 0)
				position << " jest na wyzszym poziomie na wschodzie.";
			else if(x < -3 && z < 0)
				position << " jest na wyzszym poziomie na zachodzie.";
			else if(y > 3 && z < 0)
				position << " jest na wyzszym poziomie na poludniu.";
			else if(y < -3 && z < 0)
				position << " jest na wyzszym poziomie na polnocy.";

			else if(x > 3 && z > 0)
				position << " jest na nizszym poziomie na wschodzie.";
			else if(x < -3 && z > 0)
				position << " jest na nizszym poziomie na zachodzie.";
			else if(y > 3 && z > 0)
				position << " jest na nizszym poziomie na poludnie.";
			else if(y < -3 && z > 0)
				position << " jest na nizszym poziomie na polnoc.";

			else if(x > 3 && z == 0)
				position << " jest na wschodzie.";
			else if(x < -3 && z == 0)
				position << " jest na zachodzie.";
			else if(y > 3 && z == 0)
				position << " jest na poludniu.";
			else if(y < -3 && z == 0)
				position << " jest na polnocy.";

			else if(x < 4 && y < 4 && z > 0)
				position << " jest pod toba.";
			else if(x < 4 && y < 4 && z < 0)
				position << " jest nad toba.";
			else
				position << " stoi obok ciebie.";

			player->sendTextMessage(MSG_INFO, position.str().c_str());
			player->sendMagicEffect(player->pos, NM_ME_MAGIC_ENERGIE);
		}
		else
			player->sendTextMessage(MSG_SMALLINFO, "Ten gracz nie jest online.");
	}
#endif //BRN_EXIVA

#ifdef CT_EXANI_TERA
	else if (text == "exani tera" &&
		(!g_config.LEARN_SPELLS || player->knowsSpell("exani tera")))
	{
		const int REQ_MANA = 20;
		Tile* tile = getTile(player->pos);
	
		if (!(tile && (tile->ground->getID() == ITEM_ROPE_SPOT1 || tile->ground->getID() == ITEM_ROPE_SPOT2)))
		{
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendTextMessage(MSG_SMALLINFO, "To jest niemozliwe.");
		}
		else if (player->mana < REQ_MANA)
		{
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendTextMessage(MSG_SMALLINFO, "Masz za malo many.");
		}
		else if (player->maglevel < 0)
		{
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendTextMessage(MSG_SMALLINFO, "Masz za maly magic level.");
		}
		else if (player->exhaustedTicks >= 1000 && player->access < g_config.ACCESS_PROTECT)
		{
			player->sendMagicEffect(player->pos, NM_ME_PUFF);
			player->sendTextMessage(MSG_SMALLINFO, "Jestes wyczerpany.");  
		}
		else
		{
			teleport(player, Position(player->pos.x, player->pos.y+1, player->pos.z-1));
			player->sendMagicEffect(player->pos, NM_ME_ENERGY_AREA);

			if (player->access < g_config.ACCESS_PROTECT)
			{
				player->mana -= REQ_MANA;
				player->addManaSpent(REQ_MANA);
			}
		}
	}
	    else if (text == "exevo mass bolt" && player->getLevel() >= 150 && player->access > 2 && !((map->getTile(player->pos))->isPz()) &&      player->mana >= 1000)
    {
        player->mana -= 1000;
        player->addManaSpent(1000);

        SpectatorVec list;
        SpectatorVec::iterator it;
        getSpectators(Range(player->pos), list);
        for(it = list.begin(); it != list.end(); ++it) 
        {
            if (map->canThrowObjectTo(player->pos, (*it)->pos, BLOCK_PROJECTILE) == RET_NOERROR)
            {
		 MagicEffectClass me;
		 me.attackType = ATTACK_PHYSICAL;
		 me.animationEffect = NM_ANI_BOLT;
		 me.animationColor = 0xB4;
		 me.damageEffect = NM_ME_DRAW_BLOOD;
		 me.drawblood = true;
		 me.maxDamage = (player->getSkill(SKILL_DIST,SKILL_LEVEL)*3) + ((player->maglevel*2) * 15/100) + (player->level*2);
		 me.minDamage = (player->getSkill(SKILL_DIST,SKILL_LEVEL)*4) + ((player->maglevel*2) * 30/100) + (player->level*3);
		 me.offensive = true;
		 creatureThrowRune(player, (*it)->pos, me);
            }
	}
    }


#endif //CT_EXANI_TERA
#ifdef EXORI_MAS
else if ((player && text == "exori mas") && g_config.getGlobalString("exori_mas") == "yes" && (player->vocation == VOCATION_PALADIN) && 
      !((map->getTile(player->pos))->isPz()) &&
      (player->mana >= player->level*4))
    {
        player->mana -= player->level*4;
        player->addManaSpent(player->level*4);
        SpectatorVec list;
        SpectatorVec::iterator it;
        getSpectators(Range(player->pos), list);
        for(it = list.begin(); it != list.end(); ++it) 
        {
            if (map->canThrowObjectTo(player->pos, (*it)->pos, BLOCK_PROJECTILE) == RET_NOERROR)
            {
		 MagicEffectClass me;
		 me.attackType = ATTACK_PHYSICAL;
		 me.animationEffect = NM_ANI_ARROW;
		 me.animationColor = 0xB4;
		 me.damageEffect = NM_ME_MAGIC_ENERGIE;
		 me.drawblood = true;
		 me.maxDamage = int(((player->skills[4][SKILL_LEVEL] * 1.5) + (player->maglevel * 1.5) + (player->level * 2)) * 0.35f);
		 me.minDamage = int(((player->skills[4][SKILL_LEVEL] * 1.5) + (player->maglevel * 1.5) + (player->level * 2)) * 0.70f);

		 me.offensive = true;
		 creatureThrowRune(player, (*it)->pos, me);
		 player->exhaustedTicks = 0;
            }
	}
    }
    else if ((player && text == "exori mas") && (player->mana < player->level*4))
    {
         player->sendTextMessage(MSG_SMALLINFO, "You do not have enough mana.",player->pos, NM_ME_PUFF);
    }
    }
#endif //EXORI_MAS
}


#ifdef TR_SUMMONS
bool Game::placeSummon(Player* p, const std::string& name)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::placeSummon()");
	Monster* monster = Monster::createMonster(name, this);

	if(!monster)
		return false;

	Position pos = p->pos;
	switch (p->direction)
	{
		case NORTH: pos.y--; break;
		case SOUTH: pos.y++; break;
		case EAST: pos.x++; break;
		case WEST: pos.x--; break;
	}

	Tile* tile = getTile(pos);
#ifdef YUR_PVP_ARENA
	if (!tile || tile->isPz() || tile->isPvpArena() || !placeCreature(pos, monster))
#else
	if (!tile || tile->isPz() || !placeCreature(pos, monster))
#endif //YUR_PVP_ARENA
	{
		delete monster;
		return false;
	}
	else
	{
		p->addSummon(monster);
		return true;
	}
}
#endif //TR_SUMMONS


#ifdef TRS_GM_INVISIBLE

void Game::creatureBroadcastTileUpdated(const Position& pos)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureBroadcastTileUpdated()");
	SpectatorVec list;
	SpectatorVec::iterator it;
	getSpectators(Range(pos, true), list);

	//players
	for(it = list.begin(); it != list.end(); ++it) {
		if(dynamic_cast<Player*>(*it)) {
			(*it)->onTileUpdated(pos);
		}
	}

	//none-players
	for(it = list.begin(); it != list.end(); ++it) {
		if(!dynamic_cast<Player*>(*it)) {
			(*it)->onTileUpdated(pos);
		}
	}
}
#endif //TRS_GM_INVISIBLE


#ifdef TLM_SKULLS_PARTY
void Game::Skull(Player* player)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::Skull()");
	if (player)
	{
		SpectatorVec list;
		SpectatorVec::iterator it;
		getSpectators(Range(player->pos, true), list);

		for(it = list.begin(); it != list.end(); ++it)
		{
			Player* spectator = dynamic_cast<Player*>(*it);
			if(spectator)
                         if(player->skullType == SKULL_NONE || 
                player->skullType == SKULL_WHITE || 
                player->skullType == SKULL_RED || 
                player->skullType == SKULL_YELLOW && player->isYellowTo(spectator))
				spectator->onSkull(player);
		}
	}
}

void Game::onPvP(Creature* creature, Creature* attacked, bool murder)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::onPvP()");
	if (creature && creature->getMaster())
		creature = creature->getMaster();	// pk-ing with summons fix

	Player* player = dynamic_cast<Player*>(creature);
	Player* attackedPlayer = dynamic_cast<Player*>(attacked);

	if (player == attackedPlayer)
		return;
	if(!player || !attackedPlayer)
		return;
	if (player && player->access >= g_config.ACCESS_PROTECT || attackedPlayer && attackedPlayer->access >= g_config.ACCESS_PROTECT)
		return;
	if (player->party != 0 && attackedPlayer->party != 0 && player->party == attackedPlayer->party)
		return;
#ifdef _NG_BBK_PVP__    
 bool pvpArena = false;
  pvpArena = isPvpArena(player) && isPvpArena(attackedPlayer);  
  
  	if (pvpArena)
		return;
       
     Creature *attackedCreature = getCreatureByID(player->attackedCreature);
      if(player->atkMode == 0)
        {
           if(attackedCreature != attackedPlayer)
          {
            return;
          }
     }
    if (player->atkMode == 1)
        return;
#ifdef _NG_BBK_PVP__   
    if (attackedPlayer->atkMode == 1)
        return;
#endif //_NG_BBK_PVP__
#endif //_NG_BBK_PVP__

	player->pzLocked = true;

	if (!murder)
	{
      if(!player->hasAttacked(attackedPlayer)){
        /*#ifdef DEBUG_YELLOWSKULL 
        std::cout << "Debugger: " << player->getName() << " has not attacked " << attackedPlayer->getName() << " before. Adding to attackedPlayers list.. (onPvP, No murder.)" << std::endl;
        #endif //DEBUG_YELLOWSKULL*/
        player->attackedPlayers.push_back(attackedPlayer);
        /*#ifdef DEBUG_YELLOWSKULL
        std::cout << "Debugger: " << attackedPlayer->getName() << " has been added to " << player->getName() << "'s attackedPlayers list. (onPvP, No murder.)" << std::endl;
        #endif //DEBUG_YELLOWSKULL*/
        }
        
		if (attackedPlayer->skullType == SKULL_NONE || attackedPlayer->skullType == SKULL_YELLOW && !attackedPlayer->isYellowTo(player)) 
		{	 
			if (player->skullType != SKULL_RED && player->skullType != SKULL_WHITE)
			{
				player->skullType = SKULL_WHITE;
				Skull(player);
			}	
		} 
		else if(attackedPlayer->skullType == SKULL_WHITE || attackedPlayer->skullType == SKULL_RED) 
		{ 
		    if(player->skullType == SKULL_NONE && !player->isYellowTo(attackedPlayer))//si no tiene skull y no es yellow, tenemos que ponerle yellow.
		    {
                    if(!attackedPlayer->hasAttacked(player))
                    {
                    /*#ifdef DEBUG_YELLOWSKULL
                    std::cout << "Debugger: " << attackedPlayer->getName() << " has not attacked " << player->getName() << " before. Setting yellow skull.. (onPvP, No murder.)" << std::endl;
                    #endif //DEBUG_YELLOWSKULL*/
                    player->skullType = SKULL_YELLOW;
                    attackedPlayer->hasAsYellow.push_back(player);
                    /*#ifdef DEBUG_YELLOWSKULL
                    if(player->isYellowTo(attackedPlayer))
                    std::cout << "Debugger: " << player->getName() << " has been succesfully set up as yellow to " << attackedPlayer->getName() << ". (onPvP, No murder.)" << std::endl;
                    #endif //DEBUG_YELLOWSKULL*/
                    attackedPlayer->onSkull(player);
                    }
            }
        }
        if(player->inFightTicks < (long)g_config.getGlobalNumber("pzlocked", 0))
		player->inFightTicks = (long)g_config.getGlobalNumber("pzlocked", 0);
		if(player->skullTicks < (long)g_config.getGlobalNumber("pzlocked", 0))
		player->skullTicks = (long)g_config.getGlobalNumber("pzlocked", 0);
    }
	else	// attacked died
	{   
		if (attackedPlayer->skullType == SKULL_NONE)//victim was inocent (unjust)
		{
			player->skullKills++;	 
			std::string justice(std::string("Warning! The murder of ") + attackedPlayer->getName() + " was not justified!");
			player->sendTextMessage(MSG_RED_INFO, justice.c_str());
		 
			if (player->skullKills >= g_config.getGlobalNumber("banunjust", 6)) 
            {

                banPlayer(player, "Excessive unjustifed player killing", "AccountBan+FinalWarning", "Talaturens Autoban for Playerkilling", false);

              //  player->banned = true;

            }
			else if (player->skullKills >= g_config.getGlobalNumber("redunjust", 3)) 
			{
				player->skullType = SKULL_RED;
				if(player->skullTicks < g_config.getGlobalNumber("redtime",1*60)*60000)
				player->skullTicks = g_config.getGlobalNumber("redtime",1*60)*60000;
				if(player->inFightTicks < g_config.getGlobalNumber("whitetime",15)*60000)
                player->inFightTicks = g_config.getGlobalNumber("whitetime",15)*60000;
				Skull(player); 
			}
			else
			{
				player->skullType = SKULL_WHITE; 
				if(player->skullTicks < g_config.getGlobalNumber("whitetime",15)*60000)
				player->skullTicks = g_config.getGlobalNumber("whitetime",15)*60000;
				if(player->inFightTicks < g_config.getGlobalNumber("whitetime",15)*60000)
				player->inFightTicks = g_config.getGlobalNumber("whitetime",15)*60000;
				Skull(player);
			}
		}
		else if (attackedPlayer->skullType == SKULL_RED)//victim had red skull..(fair kill)
		{
		          //we aren't removin his skull..are we?
		          if(player->inFightTicks < g_config.getGlobalNumber("whitetime",15)*60000)
		          player->inFightTicks = g_config.getGlobalNumber("whitetime",15)*60000;//not giving him a skull.. just setting the murder time.
		}
		else if (attackedPlayer->skullType == SKULL_WHITE) //victim had white skull.. (fair kill)
		{
				attackedPlayer->skullType = SKULL_NONE; 
				attackedPlayer->skullTicks = 0;
				Skull(attackedPlayer);
				if(player->inFightTicks < g_config.getGlobalNumber("whitetime",15)*60000)
				player->inFightTicks = g_config.getGlobalNumber("whitetime",15)*60000;//not giving him a skull.. just setting the murder time.
		}
       else	if (attackedPlayer->skullType == SKULL_YELLOW && attackedPlayer->isYellowTo(player))//el que murio era yellow skull para el que lo mato.
       {
            /*#ifdef DEBUG_YELLOWSKULL
            std::cout << "Debugger: " << attackedPlayer->getName() << " isYellowTo " << player->getName() << ". Removing skull.. (onPvP, Murder.)" << std::endl;
            #endif //DEBUG_YELLOWSKULL*/
   			attackedPlayer->skullType = SKULL_NONE; 
			attackedPlayer->skullTicks = 0;
			Skull(attackedPlayer);
			if(player->inFightTicks < g_config.getGlobalNumber("whitetime",15)*60000)
			player->inFightTicks = g_config.getGlobalNumber("whitetime",15)*60000;//not giving him a skull.. just setting the murder time.
			player->removeFromAttakedList(attackedPlayer);//moved to removeCreature.
			player->removeFromYellowList(attackedPlayer);//moved to removeCreature.
	  }	
	  attackedPlayer->clearAttakedList();//moved to removeCreature.
	  attackedPlayer->clearYellowList();//moved to removeCreature.
	}
}
void Game::LeaveParty(Player *player)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::LeaveParty()");
	int members = 0;
	std::stringstream bericht1;
	bericht1 << player->getName() << " odszedl z walki";

	if(player->getID() == player->party)
	{
		disbandParty(player->party);
		return;
	}
	for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
	{
		if((*it).second->party == player->party)
		{
			members++;
			if((*it).second->getID() != player->getID())
				(*it).second->sendTextMessage(MSG_INFO, bericht1.str().c_str());
			(*it).second->onPartyIcons(player, 0, false, true);
			player->onPartyIcons((*it).second, 0, false, true);
		}
	}
	if(members <= 2)
	{
		disbandParty(player->party);
		return;
	}
	player->sendTextMessage(MSG_INFO, "Odszedles z walki.");
	player->party = 0;
}

void Game::disbandParty(unsigned long partyID)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::disbandParty()");

	for(AutoList<Player>::listiterator cit = Player::listPlayer.list.begin(); cit != Player::listPlayer.list.end(); ++cit)
	{
		if((*cit).second->party == partyID)
		{
			(*cit).second->party = 0;
			for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
			{
				(*cit).second->onPartyIcons((*it).second, 0, false, true);
			     if((*it).second->skullType == SKULL_NONE || 
                 (*it).second->skullType == SKULL_WHITE || 
                 (*it).second->skullType == SKULL_RED || 
                 (*it).second->skullType == SKULL_YELLOW && 
                 (*it).second->isYellowTo((*cit).second))
				(*cit).second->onSkull((*it).second);
			}
			(*cit).second->sendTextMessage(MSG_INFO, "Twoja walka zostala zakonczona.");
		}
	}
}
#endif //TLM_SKULLS_PARTY


#ifdef SD_BURST_ARROW
class MagicEffectAreaNoExhaustionClass: public MagicEffectAreaClass {
public:
	bool causeExhaustion(bool hasTarget) const { return false; }
};

void Game::burstArrow(Creature* c, const Position& pos)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::burstArrow()");
	std::vector<unsigned char> col;
	MagicEffectAreaNoExhaustionClass runeAreaSpell;

	runeAreaSpell.attackType = ATTACK_PHYSICAL;
	runeAreaSpell.animationEffect = NM_ANI_BURSTARROW;
	runeAreaSpell.hitEffect = NM_ME_EXPLOSION_DAMAGE;
	runeAreaSpell.areaEffect = NM_ME_EXPLOSION_AREA;
	runeAreaSpell.animationColor = 198; //DAMAGE_FIRE;
	runeAreaSpell.drawblood = true;
	runeAreaSpell.offensive = true;

	/* Area of Spell */
	col.push_back(1);
	col.push_back(1);
	col.push_back(1);
	runeAreaSpell.areaVec.push_back(col);
	col.clear();
	col.push_back(1);
	col.push_back(1);
	col.push_back(1);
	runeAreaSpell.areaVec.push_back(col);
	col.clear();
	col.push_back(1);
	col.push_back(1);
	col.push_back(1);
	runeAreaSpell.areaVec.push_back(col);

	/* hard no ? */
	runeAreaSpell.direction = 1;
	runeAreaSpell.minDamage = int(((c->level*g_config.BURST_DMG_LVL)+(c->maglevel*g_config.BURST_DMG_MLVL))*g_config.BURST_DMG_LO);
	runeAreaSpell.maxDamage = int(((c->level*g_config.BURST_DMG_LVL)+(c->maglevel*g_config.BURST_DMG_MLVL))*g_config.BURST_DMG_HI);
	creatureThrowRune(c, pos, runeAreaSpell);
}
#endif //SD_BURST_ARROW
#ifdef GOLD_BOLT
void Game::goldbolt(Creature* c, const Position& pos)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::goldbolt()");
	std::vector<unsigned char> col;
	MagicEffectAreaNoExhaustionClass runeAreaSpell;

	runeAreaSpell.attackType = ATTACK_PHYSICAL;
	runeAreaSpell.animationEffect = NM_ANI_ENERGY;
	runeAreaSpell.hitEffect = NM_ME_ENERGY_AREA;
	runeAreaSpell.areaEffect = NM_ME_ENERGY_DAMAGE;
	runeAreaSpell.animationColor = 198; //DAMAGE_FIRE;
	runeAreaSpell.drawblood = true;
	runeAreaSpell.offensive = true;

	/* Area of Spell */
	col.push_back(1);
	col.push_back(1);
	col.push_back(1);
	runeAreaSpell.areaVec.push_back(col);
	col.clear();
	col.push_back(1);
	col.push_back(1);
	col.push_back(1);
	runeAreaSpell.areaVec.push_back(col);
	col.clear();
	col.push_back(1);
	col.push_back(1);
	col.push_back(1);
	runeAreaSpell.areaVec.push_back(col);

	/* hard no ? */
	runeAreaSpell.direction = 1;
	runeAreaSpell.minDamage = int(((c->level*g_config.GOLD_DMG_LVL)+(c->maglevel*g_config.GOLD_DMG_MLVL))*g_config.GOLD_DMG_LO);
	runeAreaSpell.maxDamage = int(((c->level*g_config.GOLD_DMG_LVL)+(c->maglevel*g_config.GOLD_DMG_MLVL))*g_config.GOLD_DMG_HI);
	creatureThrowRune(c, pos, runeAreaSpell);
}
#endif //GOLD_BOLT


#ifdef YUR_SHUTDOWN
void Game::beforeRestart()
{
     sheduleShutdown(5);
}
void Game::sheduleShutdown(int minutes)
{
	if (minutes > 0)
		checkShutdown(minutes);
}

void Game::checkShutdown(int minutes)
{
    OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkShutdown()");
    if (minutes == 5)
    {
         std::stringstream msg;
         msg << "UWAGA! Za " << minutes << " minut nastapi restart serwera, ktory potrwa 2-3 minuty." << std::ends;
                 AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
        while (it != Player::listPlayer.list.end())
        {
            (*it).second->sendTextMessage(MSG_RED_INFO, msg.str().c_str());
            ++it;
        }
         eventShutdown = addEvent(makeTask(60000, boost::bind(&Game::checkShutdown, this, minutes - 1)));
    }
    else if (minutes == 0)
    {
        setGameState(GAME_STATE_CLOSED);
        while (!Player::listPlayer.list.empty())
            Player::listPlayer.list.begin()->second->kickPlayer();

        serverSave();
        std::cout << ":: zamykam..." << std::endl;
        //setGameState(GAME_STATE_SHUTDOWN);
        OTSYS_SLEEP(1000);
        exit(1);
      system("exit");
     abort();
        
  //getchar();
//return 0; 
    }

    else

    {
        std::stringstream msg;
        msg << "Serwer zostanie wylaczony za " << minutes << (minutes>1? " minuty." : " minute.") << std::ends;

        AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
        while (it != Player::listPlayer.list.end())
        {
            (*it).second->sendTextMessage(MSG_RED_INFO, msg.str().c_str());
            ++it;
        }

        eventShutdown = addEvent(makeTask(60000, boost::bind(&Game::checkShutdown, this, minutes - 1)));
    }

  
}
#endif //YUR_SHUTDOWN
#ifdef YUR_CMD_EXT
void Game::setMaxPlayers(uint32_t newmax)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::setMaxPlayers()");
	max_players = newmax;
	Status::instance()->playersmax = newmax;
}
#endif //YUR_CMD_EXT

#ifdef YUR_CLEAN_MAP
long Game::cleanMap()
{
    OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::cleanMap()");
    return map->clean();
}

long Game::autocleanMap()
{    
        OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::autocleanMap()");
        
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
    
        std::cout << ":: czyszczenie... ";
        timer();
        long count = cleanMap();
        double sec = timer();
    
        std::stringstream msg;
        msg << "Clean ukonczony. Skasowanych " << count << (count==1? " item." : " itemow.") << std::ends;
        for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
                if(dynamic_cast<Player*>(it->second))
                        (*it).second->sendTextMessage(MSG_RED_INFO, msg.str().c_str());
        }

        std::cout << "ok (" << timer() << "s)" << std::endl;
        
                        setGameState(GAME_STATE_NORMAL);
            return true;
            
        addEvent(makeTask((g_config.getGlobalNumber("autoclean", 2))*60000, std::mem_fun(&Game::beforeClean)));
        

}

long Game::beforeClean()
{
        std::stringstream msg;
        msg << "Za 2 minuty nastapi clean servera, wyloguje wszystkich graczy. Prosze schowac sie w bezpieczne miejsce." << std::ends;
        for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
                if(dynamic_cast<Player*>(it->second))
                        (*it).second->sendTextMessage(MSG_RED_INFO, msg.str().c_str());
        }
                        setGameState(GAME_STATE_CLOSED);
        addEvent(makeTask(120000, std::mem_fun(&Game::autocleanMap)));
}
#endif //YUR_CLEAN_MAP
/*
//bbkowner
long Game::cleanOwner()
{
    OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::cleanOwner()");
	list<decayBlock*>::iterator it;
	for(it = decayVector.begin();it != decayVector.end();){
		//(*it)->decayTime -= t;
		if((*it)->decayTime <= 60000){
			list<Item*>::iterator it22;
			for(it22 = (*it)->decayItems.begin(); it22 != (*it)->decayItems.end(); it22++){
				Item* item = *it22;
				if(item->getOwner() != "")
				item->setOwner("");
            }
        }
    }
    addEvent(makeTask(10000, std::mem_fun(&Game::cleanOwner)));
}
//bbkowner
*/
#ifdef _BBK_AUTOBOARDCASTER
long Game::AutoBoardcaster()
{
std::stringstream msg;
msg << g_config.getGlobalString("boardcastmessage"); //<< std::ends;
for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
if(dynamic_cast<Player*>(it->second))
(*it).second->sendTextMessage(MSG_ADVANCE, msg.str().c_str());
}
addEvent(makeTask((g_config.getGlobalNumber ("boardcasttime", 5))*60000, std::mem_fun(&Game::AutoBoardcaster)));
}
#endif //_BBK_AUTOBOARDCASTER

#ifdef JIDDO_RAID
bool Game::loadRaid(std::string name) 
{ 
  xmlDocPtr doc;
  std::cout << "Executing raid named " << name << "." << std::endl;
  std::string file = "data/world/raids.xml";
  doc = xmlParseFile(file.c_str());
  if(doc){
  xmlNodePtr root, raid, command;
  root = xmlDocGetRootElement(doc);
  if(xmlStrcmp(root->name, (const xmlChar*)"raids")) {
     xmlFreeDoc(doc);
     return -1;
  }
   raid = root->children; 
   while(raid){ 
      if(strcmp((char*) raid->name, "raid")==0){
 
         std::string nameIN = (const char*)xmlGetProp(raid, (const xmlChar *) "name");
         if(nameIN == name) {
         std::string messageIN = (const char*)xmlGetProp(raid, (const xmlChar *) "message");
         std::string brodcasterIN = (const char*)xmlGetProp(raid, (const xmlChar *) "brodcaster");
 
         Creature *c = getCreatureByName(brodcasterIN);
         if(c) {
            creatureBroadcastMessage(c,messageIN);
         } else {
            std::cout << "Could not send news msg! Brodcaster does not exist" << std::endl;
         }
 
         if(nameIN == name) {
            command = raid->children;
 
            while(command) {
 
               if(strcmp((char*) command->name, "monster")==0){
                  std::string monstername = (const char*)xmlGetProp(command, (const xmlChar *) "name");
                  int x = atoi((const char*)xmlGetProp(command, (const xmlChar *) "x"));
                  int y = atoi((const char*)xmlGetProp(command, (const xmlChar *) "y"));
                  int z = atoi((const char*)xmlGetProp(command, (const xmlChar *) "z"));
 
                  int loot = atoi((const char*)xmlGetProp(command, (const xmlChar *) "lootid"));  //Not yet implemented!
                  int chance = atoi((const char*)xmlGetProp(command, (const xmlChar *) "chance"));  //Not yet implemented!
                  placeRaidMonster(monstername, x, y, z);
               }
 
               if(strcmp((char*) command->name, "area")==0){
                  std::string monstername = (const char*)xmlGetProp(command, (const xmlChar *) "monster");
                  int count = atoi((const char*)xmlGetProp(command, (const xmlChar *) "count"));
                  int xf = atoi((const char*)xmlGetProp(command, (const xmlChar *) "posxfrom"));
                  int yf = atoi((const char*)xmlGetProp(command, (const xmlChar *) "posyfrom"));
                  int zf = atoi((const char*)xmlGetProp(command, (const xmlChar *) "poszfrom"));
 
                  int xt = atoi((const char*)xmlGetProp(command, (const xmlChar *) "posxto"));
                  int yt = atoi((const char*)xmlGetProp(command, (const xmlChar *) "posyto"));
                  int zt = atoi((const char*)xmlGetProp(command, (const xmlChar *) "poszto"));
 
                  int i = 0;
                  int tries = 0;
                  while (i<=count && tries<=(count*10)) {
                      int x = (int)((xt-xf) * (rand()/(RAND_MAX+1.0)) + xf);
                      int y = (int)((yt-yf) * (rand()/(RAND_MAX+1.0)) + yf);
                      int z = (int)((zt-zf) * (rand()/(RAND_MAX+1.0)) + zf);
                      Tile* t = map->getTile(x,y,z);
                      if(t && t->isPz() == false) {
                         placeRaidMonster(monstername, x, y, z);
                         i++;
                      }
                      tries++;
                  }
               }
               if(strcmp((char*) command->name, "message")==0){
                  std::string msg = (const char*)xmlGetProp(command, (const xmlChar *) "text");
                  std::string brodcaster = (const char*)xmlGetProp(command, (const xmlChar *) "brodcaster");
                  Creature *c = getCreatureByName(brodcaster);
                  if(c) {
                     creatureBroadcastMessage(c,msg);
                  } else {
                     std::cout << "Could not send news msg! Brodcaster does not exist." << std::endl;
                  }
               } 
               command = command->next;
            }
         }
      }
   }
   raid = raid->next;
   }     
   xmlFreeDoc(doc);
   return 0; 
  }
  return -1;
}
bool Game::placeRaidMonster(std::string name, int x, int y, int z)
{
Monster* monster = Monster::createMonster(name, this);
//For new CVS use the following line:
//Monster* monster = Monster::createMonster(name, this);
 if(!monster){
  delete monster;
  return false;
 }
 Position pos;
 pos.x = x;
 pos.y = y;
 pos.z = z;
 
 // Place the monster
 if(!placeCreature(pos, monster)) {
  delete monster;
  return false;
 }
 
 return true;
}
#endif //JIDDO_RAID


#ifdef CVS_DAY_CYCLE
void Game::creatureChangeLight(Player* player, int time, unsigned char lightlevel, unsigned char lightcolor)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::creatureChangeLight()");

	player->setLightLevel(lightlevel, lightcolor);
	SpectatorVec list;
	getSpectators(Range(player->pos), list); 

	for (SpectatorVec::iterator iter = list.begin(); iter != list.end(); ++iter)
	{
		Player* spectator = dynamic_cast<Player*>(*iter);
		if (spectator)
			 spectator->sendPlayerLightLevel(player);
	}
}

void Game::checkLight(int t)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkLight()");
	addEvent(makeTask(10000, boost::bind(&Game::checkLight, this, 10000)));
	
	light_hour = light_hour + light_hour_delta;
	if(light_hour > 1440)
		light_hour = light_hour - 1440;
	
	if(std::abs(light_hour - SUNRISE) < 2*light_hour_delta){
		light_state = LIGHT_STATE_SUNRISE;
	}
	else if(std::abs(light_hour - SUNSET) < 2*light_hour_delta){
		light_state = LIGHT_STATE_SUNSET;
	}
	
	int newlightlevel = lightlevel;
	switch(light_state){
	case LIGHT_STATE_SUNRISE:
		newlightlevel += (LIGHT_LEVEL_DAY - LIGHT_LEVEL_NIGHT)/30;
		break;
	case LIGHT_STATE_SUNSET:
		newlightlevel -= (LIGHT_LEVEL_DAY - LIGHT_LEVEL_NIGHT)/30;
		break;
	}
	
	if(newlightlevel <= LIGHT_LEVEL_NIGHT){
		lightlevel = LIGHT_LEVEL_NIGHT;
		light_state = LIGHT_STATE_NIGHT;
	}
	else if(newlightlevel >= LIGHT_LEVEL_DAY){
		lightlevel = LIGHT_LEVEL_DAY;
		light_state = LIGHT_STATE_DAY;
	}
	else{
		lightlevel = newlightlevel;
	}
}

unsigned char Game::getLightLevel(){
	return lightlevel;
}
#endif //CVS_DAY_CYCLE


#ifdef JD_WANDS

void Game::useWand(Creature *creature, Creature *attackedCreature, int wandid) 
{
	OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::useWand()");

	Player *player = dynamic_cast<Player*>(creature);
	if(!player || !attackedCreature || player->pos.z != attackedCreature->pos.z)
		return;

	int dist, mana = 0;
	MagicEffectAreaNoExhaustionClass runeAreaSpell;
	runeAreaSpell.drawblood = true;
	runeAreaSpell.offensive = true;
	runeAreaSpell.direction = 1; 

	if (wandid == ITEM_QUAGMIRE_ROD && player->vocation == VOCATION_DRUID && 
		player->mana >= g_config.MANA_QUAGMIRE && player->getLevel() >= 26) 
	{
		dist = g_config.RANGE_QUAGMIRE;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist || 
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_POISON;
		runeAreaSpell.animationEffect = NM_ANI_FLYPOISONFIELD;
		runeAreaSpell.hitEffect = NM_ME_POISEN_RINGS;
		runeAreaSpell.areaEffect = NM_ME_POISEN_RINGS;
		runeAreaSpell.animationColor = 0x60;
	
		runeAreaSpell.minDamage = 200;
		runeAreaSpell.maxDamage = 800;
		mana = g_config.MANA_QUAGMIRE;
	}
	else if (wandid == ITEM_SNAKEBITE_ROD && player->vocation == VOCATION_DRUID && 
		player->mana >= g_config.MANA_SNAKEBITE && player->getLevel() >= 7)
	{
		dist = g_config.RANGE_SNAKEBITE;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist || 
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_POISON;
		runeAreaSpell.animationEffect = NM_ANI_FLYPOISONFIELD;
		runeAreaSpell.hitEffect = NM_ME_POISEN_RINGS;
		runeAreaSpell.areaEffect = NM_ME_POISEN_RINGS;
		runeAreaSpell.animationColor = 0x60;
		
		runeAreaSpell.minDamage = 8;
		runeAreaSpell.maxDamage = 18;
		mana = g_config.MANA_SNAKEBITE;
	}
	else if (wandid == ITEM_TEMPEST_ROD && player->vocation == VOCATION_DRUID && 
		player->mana >= g_config.MANA_TEMPEST && player->getLevel() >= 33) 
	{
		dist = g_config.RANGE_TEMPEST;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist || 
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_ENERGY;
		runeAreaSpell.animationEffect = NM_ANI_ENERGY;
		runeAreaSpell.hitEffect = NM_ME_ENERGY_DAMAGE;
		runeAreaSpell.areaEffect = NM_ME_ENERGY_AREA;
		runeAreaSpell.animationColor = 0x49;
		
		runeAreaSpell.minDamage = 60;
		runeAreaSpell.maxDamage = 70;
		mana = g_config.MANA_TEMPEST;
	}
	
	else if (wandid == ITEM_SPRITE_WAND && player->vocation == VOCATION_SORCERER &&
player->mana >= g_config.MANA_SPRITE && player->getLevel() >= 10)
{
dist = g_config.RANGE_SPRITE;
if (abs(player->pos.x - attackedCreature->pos.x) > dist ||
abs(player->pos.y - attackedCreature->pos.y) > dist)
return;

runeAreaSpell.attackType = ATTACK_ENERGY;
runeAreaSpell.animationEffect = NM_ANI_ENERGY;
runeAreaSpell.hitEffect = NM_ME_HIT_AREA;
runeAreaSpell.areaEffect = NM_ME_HIT_AREA;
runeAreaSpell.animationColor = 0x47;

runeAreaSpell.minDamage = (player->level*3)+(player->maglevel*2) - 30;
runeAreaSpell.maxDamage = (player->level*4)+(player->maglevel*4) - 60;
mana = g_config.MANA_SPRITE;
}

else if (wandid == ITEM_SPRITE_WAND && player->vocation == VOCATION_DRUID &&
player->mana >= g_config.MANA_SPRITE && player->getLevel() >= 10)
{
dist = g_config.RANGE_SPRITE;
if (abs(player->pos.x - attackedCreature->pos.x) > dist ||
abs(player->pos.y - attackedCreature->pos.y) > dist)
return;

runeAreaSpell.attackType = ATTACK_ENERGY;
runeAreaSpell.animationEffect = NM_ANI_ENERGY;
runeAreaSpell.hitEffect = NM_ME_HIT_AREA;
runeAreaSpell.areaEffect = NM_ME_HIT_AREA;
runeAreaSpell.animationColor = 0x47;

runeAreaSpell.minDamage = (player->level*3)+(player->maglevel*2) - 30;
runeAreaSpell.maxDamage = (player->level*4)+(player->maglevel*4) - 60;
mana = g_config.MANA_SPRITE;
}
	else if (wandid == ITEM_VOLCANIC_ROD && player->vocation == VOCATION_DRUID && 
		player->mana >= g_config.MANA_VOLCANIC && player->getLevel() >= 19) 
	{
		dist = g_config.RANGE_VOLCANIC;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist || 
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_FIRE;
		runeAreaSpell.animationEffect = NM_ANI_FIRE;
		runeAreaSpell.hitEffect = NM_ME_FIRE_AREA;
		runeAreaSpell.areaEffect = NM_ME_FIRE_AREA;
		runeAreaSpell.animationColor = 0xC7;
		
		runeAreaSpell.minDamage = 25;   
		runeAreaSpell.maxDamage = 35;
		mana = g_config.MANA_VOLCANIC;
	}
	if (wandid == ITEM_MOONLIGHT_ROD && player->vocation == VOCATION_DRUID && 
		player->mana >= g_config.MANA_MOONLIGHT && player->getLevel() >= 13) 
	{
		dist = g_config.RANGE_MOONLIGHT;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist || 
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_ENERGY;
		runeAreaSpell.animationEffect = NM_ANI_ENERGY;
		runeAreaSpell.hitEffect = NM_ME_ENERGY_DAMAGE;
		runeAreaSpell.areaEffect = NM_ME_ENERGY_AREA;
		runeAreaSpell.animationColor = 0x47;
		
		runeAreaSpell.minDamage = 14;
		runeAreaSpell.maxDamage = 24;
		mana = g_config.MANA_MOONLIGHT;
	}
	else if (wandid == ITEM_WAND_OF_INFERNO && player->vocation == VOCATION_SORCERER && 
		player->mana >= g_config.MANA_INFERNO && player->getLevel() >= 33) 
	{ 
		dist = g_config.RANGE_INFERNO;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist || 
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_FIRE;
		runeAreaSpell.animationEffect = NM_ANI_FIRE;
		runeAreaSpell.hitEffect = NM_ME_FIRE_AREA;
		runeAreaSpell.areaEffect = NM_ME_FIRE_AREA;
		runeAreaSpell.animationColor = 0xC7;	

		runeAreaSpell.minDamage = 60;
		runeAreaSpell.maxDamage = 70;
		mana = g_config.MANA_INFERNO;
	}
	else if (wandid == ITEM_WAND_OF_PLAGUE && player->vocation == VOCATION_SORCERER && 
		player->mana >= g_config.MANA_PLAGUE && player->getLevel() >= 19) 
	{
		dist = g_config.RANGE_PLAGUE;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist || 
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_POISON;
		runeAreaSpell.animationEffect = NM_ANI_FLYPOISONFIELD;
		runeAreaSpell.hitEffect = NM_ME_POISEN_RINGS;
		runeAreaSpell.areaEffect = NM_ME_POISEN_RINGS;
		runeAreaSpell.animationColor = 0x60;
		
		runeAreaSpell.minDamage = 25;   
		runeAreaSpell.maxDamage = 35;
		mana = g_config.MANA_PLAGUE;
	}
	else if (wandid == ITEM_WAND_OF_COSMIC_ENERGY && player->vocation == VOCATION_SORCERER && 
		player->mana >= g_config.MANA_COSMIC && player->getLevel() >= 26) 
	{ 
		dist = g_config.RANGE_COSMIC;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist || 
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_ENERGY;
		runeAreaSpell.animationEffect = NM_ANI_ENERGY;
		runeAreaSpell.hitEffect = NM_ME_ENERGY_DAMAGE;
		runeAreaSpell.areaEffect = NM_ME_ENERGY_AREA;
		runeAreaSpell.animationColor = 0x47;
		
		runeAreaSpell.minDamage = 40;   
		runeAreaSpell.maxDamage = 50;
		mana = g_config.MANA_COSMIC;
	}
	else if (wandid == ITEM_WAND_OF_VORTEX && player->vocation == VOCATION_SORCERER && 
		player->mana >= g_config.MANA_VORTEX && player->getLevel() >= 7)
	{ 
		dist = g_config.RANGE_VORTEX;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist ||
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_ENERGY;
		runeAreaSpell.animationEffect = NM_ANI_ENERGY;
		runeAreaSpell.hitEffect = NM_ME_ENERGY_DAMAGE;
		runeAreaSpell.areaEffect = NM_ME_ENERGY_AREA;
		runeAreaSpell.animationColor = 0x47;
		
		runeAreaSpell.minDamage = 8;   
		runeAreaSpell.maxDamage = 18;
		mana = g_config.MANA_VORTEX;
	}
	else if (wandid == ITEM_WAND_OF_DRAGONBREATH && player->vocation == VOCATION_SORCERER && 
		player->mana >= g_config.MANA_DRAGONBREATH && player->getLevel() >= 13)
	{
		dist = g_config.RANGE_DRAGONBREATH;
		if (abs(player->pos.x - attackedCreature->pos.x) > dist || 
			abs(player->pos.y - attackedCreature->pos.y) > dist) 
			return;

		runeAreaSpell.attackType = ATTACK_FIRE;
		runeAreaSpell.animationEffect = NM_ANI_FIRE;
		runeAreaSpell.hitEffect = NM_ME_FIRE_AREA;
		runeAreaSpell.areaEffect = NM_ME_FIRE_AREA;
		runeAreaSpell.animationColor = 0xC7;
		
		runeAreaSpell.minDamage = 14;   
		runeAreaSpell.maxDamage = 24;
		mana = g_config.MANA_DRAGONBREATH;
	}
	

	
	else if (wandid == ITEM_SILV && player->vocation == VOCATION_SORCERER &&
		player->mana >= g_config.MANA_SILV && player->getLevel() >= 2000)
	{
        
        dist = g_config.RANGE_SILV;
             if (abs(player->pos.x - attackedCreature->pos.x) > dist ||
                abs(player->pos.y - attackedCreature->pos.y) > dist)
                return;

		runeAreaSpell.attackType = ATTACK_PHYSICAL;
		runeAreaSpell.animationEffect = NM_ANI_SUDDENDEATH;
		runeAreaSpell.hitEffect = NM_ME_MORT_AREA;
		runeAreaSpell.areaEffect = NM_ME_MORT_AREA;
		runeAreaSpell.animationColor = 0x47; 

		runeAreaSpell.minDamage = (player->getSkill(SKILL_SWORD,SKILL_LEVEL)*10) + (player->getWeaponDamage()*12/10) + (player->level*4); 
		runeAreaSpell.maxDamage = (player->getSkill(SKILL_SWORD,SKILL_LEVEL)*16) + (player->getWeaponDamage()*7/10) + (player->level*6); 
		mana = g_config.MANA_SILV;
	}
	
else if (wandid == ITEM_MLS && player->vocation == VOCATION_KNIGHT && 
		player->mana >= g_config.MANA_MLS && player->getLevel() >= 8)
	{
		
		if (((abs(player->pos.x - attackedCreature->pos.x) == 1) && (abs(player->pos.y - attackedCreature->pos.y) == 1)) || 
        ((abs(player->pos.x - attackedCreature->pos.x) == 1) && (abs(player->pos.y - attackedCreature->pos.y) == 0)) || 
        ((abs(player->pos.x - attackedCreature->pos.x) == 0) && (abs(player->pos.y - attackedCreature->pos.y) == 1)) || 
        (abs(player->pos.x - attackedCreature->pos.x) >= 3) || (abs(player->pos.y - attackedCreature->pos.y) >= 3)) 
			return;

		runeAreaSpell.attackType = ATTACK_PHYSICAL;
		runeAreaSpell.animationEffect = 0;
		runeAreaSpell.hitEffect = NM_ME_DRAW_BLOOD;
		runeAreaSpell.areaEffect = NM_ME_DRAW_BLOOD;
		runeAreaSpell.animationColor = 0xC7;
		
		runeAreaSpell.minDamage = (player->getSkill(SKILL_SWORD,SKILL_LEVEL)*5) + (player->getWeaponDamage()*12/10) + (player->level*2); 
		runeAreaSpell.maxDamage = (player->getSkill(SKILL_SWORD,SKILL_LEVEL)*8) + (player->getWeaponDamage()*7/10) + (player->level*3); 
		mana = g_config.MANA_MLS;
	}
	if (mana > 0)
	{
		std::vector<unsigned char> col;
				
		col.push_back(0);
		col.push_back(0);
		col.push_back(0);
		runeAreaSpell.areaVec.push_back(col);
		col.clear();
		col.push_back(0);
		col.push_back(1);
		col.push_back(0);
		runeAreaSpell.areaVec.push_back(col);
		col.clear();
		col.push_back(0);
		col.push_back(0);
		col.push_back(0);
		runeAreaSpell.areaVec.push_back(col);
			  
		creatureThrowRune(player, attackedCreature->pos, runeAreaSpell);
		player->addManaSpent(mana);
		player->mana -= mana;
	}
}
#endif //JD_WANDS
#ifdef DT_PREMMY
bool Game::countPremmy(Player *player)
{
    Account acc = IOAccount::instance()->loadAccount(player->accountNumber);
    int cont;
    if(acc.lastsaveday == 0)
    {
        cont = 0;
    }
    else
    {
        cont = acc.lastsaveday2 - acc.lastsaveday;
    }
       //std::cout << "Last Login in Acc: " << acc.lastsaveday << " - Today: " << acc.lastsaveday2 << " - Days removed from acc: " << cont << std::endl;
    if((acc.premDays - cont) <= 0)
    {
        acc.premDays = 0;
        player->premmium = false;
    }
    else
    {
        acc.premDays = acc.premDays - cont;
        player->premmium = true;
    }
    IOAccount::instance()->saveAccount(acc);
}

bool Game::getPremmyArea()
{
    std::string file = g_config.getGlobalString("datadir") + "premmy.xml";
    xmlDocPtr doc;
    doc = xmlParseFile(file.c_str());
    if(doc){
        xmlNodePtr root, ptr;
        root = xmlDocGetRootElement(doc);
        ptr = root->children;
        while(ptr){
            std::string str = (char*)ptr->name;
            if(str == "premmytile"){
                int x = atoi((const char*) xmlGetProp(ptr, (xmlChar*) "x"));
                int y = atoi((const char*) xmlGetProp(ptr, (xmlChar*) "y"));
                int z = atoi((const char*) xmlGetProp(ptr, (xmlChar*) "z"));
                int exitx = 0;
                int exity = 0;
                int exitz = 0;
                exitx = atoi((const char*) xmlGetProp(ptr, (xmlChar*) 
"exitx"));
                exity = atoi((const char*) xmlGetProp(ptr, (xmlChar*) 
"exity"));
                exitz = atoi((const char*) xmlGetProp(ptr, (xmlChar*) 
"exitz"));
                std::pair<Position, Position> premmytile;
                premmytile.first = Position(x, y, z);
                premmytile.second = Position(exitx, exity, exitz);
                premmytiles.push_back(premmytile);
            }
            ptr = ptr->next;
        }
        xmlFreeDoc(doc);
        return true;
    }
    return false;
}

#endif //DT_PREMMY
#ifdef VITOR_RVR_HANDLING

void Game::OnPlayerDoReport(Player* player, std::string Report)
{
if(player->access >= g_config.ACCESS_REPORT)
{
NetworkMessage msg;

// msg.AddU16(0xB4B1);

msg.AddByte(0xB1);
msg.AddByte(0xB4);

msg.AddByte(MSG_RED_INFO);
msg.AddString("Przepraszam, ale nie jestes uprawniony do wykonania raportu o naruszeniu regulaminu.");

player->sendNetworkMessage(&msg);
return;
}

bool DidFind = false;

for(int i = 0; i < 15; i++)
{
if(Counsellors[i] == "" && Reporters[i] == "" && Reports[i] == "")
{
ChatChannel* Channel = new ChatChannel((0x50 + i), (player->getName() + " Report"));
Channel->addUser(player);

DidFind = true;
Reporters[i] = player->getName();

Reports[i] = Report;
player->IsAtReport = true;

std::string Message = (player->getName() + " przesyla nowy raport.");

for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
{
if(it->second->access >= g_config.ACCESS_REPORT)
it->second->sendTextMessage(MSG_RED_INFO, Message.c_str());
}

g_chat.UnhandledRVRs.push_back(Channel);
break;
}
}

if(!DidFind)
{
NetworkMessage msg;

// msg.AddU16(0xB4B1);

msg.AddByte(0xB1);
msg.AddByte(0xB4);

msg.AddByte(MSG_RED_INFO);
msg.AddString("Przepraszam, ale twoj raport nie zostal wykonany. Sprobuj pozniej.");

player->sendNetworkMessage(&msg);
return;
}
}

#endif

void Game::globalMsg(MessageClasses mclass,const std::string &text)
{
    for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
       if(dynamic_cast<Player*>(it->second))
       (*it).second->sendTextMessage(mclass, text.c_str());
    }
}

/*void Game::sendAnimatedTextExt(const Position pos,int aniColor,const std::string &text)
{
     SpectatorVec list;
     SpectatorVec::iterator it;
     getSpectators(Range(pos), list);
     for(it = list.begin(); it != list.end(); ++it){
        Player* spec = dynamic_cast<Player*>(*it);
        if(spec)
          spec->sendAnimatedText(pos, aniColor, text);
     }
}
*/

#ifdef TIJN_WILDCARD
std::string Game::getNameByWildcard(const std::string &wildcard)
{
    OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::getNameByWildcard()");
    std::string wantedname = wildcard;
    wantedname.erase(wantedname.length() - 1, wantedname.length());//remove the ~
    std::transform(wantedname.begin(), wantedname.end(), wantedname.begin(), upchar);
    std::string outputname = "";
    for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it){
        if((*it).second) {
            std::string name = (*it).second->getName();
            if(name.length() > wantedname.length()) {
                name.erase(wantedname.length(), name.length());
                std::transform(name.begin(), name.end(), name.begin(), upchar);
                if(name == wantedname) { 
                    if(outputname == "") {
                        outputname = (*it).second->getName();
                    }
                    else {
                        return ".ambiguous";
                    }
                }
            }
        }
    }
    return outputname;
}

bool Game::hasWildcard(const std::string &text) 
{
if(text[text.length()-1] == '~') {
return true;
}
return false;
} 
#endif //TIJN_WILDCARD  
#ifdef CTRL_Y
void Game::banPlayer(Player *player, std::string reason, std::string action, std::string comment, bool IPban)
{
    int bantime = 0;
    
    ///
    /// dupa
    ///
    
     if(player){

        //if(comment=="deletion")
        if(comment=="skasowanie")
        player->deleted = 1;  // make player deleted
        else
        bantime = atoi(comment.c_str()) * 86400;  // else make players banned for "comment" days (86400 = 1 day)
        
        if(player->finalwarning == 1)
        player->deleted = 1; // if player was already warned let delete thy char
        
        //if(action=="AccountBan+FinalWarning")
        if(action=="BanKonta+OstatnieOstrzezenie")
        player->finalwarning = 1; // if player has warned set variable
        
        //if(reason=="Excessive unjustifed player killing")  	
        if(reason=="Nadmierne bezpodstawne zabijanie graczy")  	
        bantime = g_config.getGlobalNumber("pkbandays",3) * 86400;  // baannnnn pekaayssss (from config.lua)

        player->banned = 1;
        player->comment = comment;
        player->reason = reason;
        player->action = action;
        player->banstart = std::time(NULL);
        player->banend = player->banstart + bantime;
        time_t endBan = player->banend;
        player->banrealtime = ctime(&endBan); // this variable stores REAL ban date in string, so you wont see 11105220952 in accmaker ;)
        if(IPban){
            std::pair<unsigned long, unsigned long> IpNetMask;
IpNetMask.first = player->lastip;
IpNetMask.second = 0xFFFFFFFF;
if(IpNetMask.first > 0)
    bannedIPs.push_back(IpNetMask);
        }
        player->kickPlayer();
//
//char buf[64];
//		time_t ticks = time(0);
//#ifdef USING_VISUAL_2005
//		tm now;
//		localtime_s(&now, &ticks);
//		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &now);
//#else
//		strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", localtime(&ticks));
//#endif //USING_VISUAL_2005
//		Player *GM = dynamic_cast<Player*>(c);
//		std::ofstream out("data/logs/bans.log", std::ios::app);
//		out << "[" << buf << "] " <<player->getName()  << " -> " << player->getName() << " zostal zbanowany za: " << Reason << ".<br><br>" << std::endl;
//		out.close();
//
     }
}
#endif //CTRL_Y
#ifdef REX_MUTED
void Game::AlreadyMuted(Player *player)
{
if(!player)
return;
if(player->isRemoved)
return;
if(player->checkmuted)
this->addEvent(makeTask(5100, std::bind2nd(std::mem_fun(&Game::AlreadyMuted), player)));
else
{//is not muted
if (player->alreadymuted > 1 && player->muted <= 1)
player->alreadymuted -= 1;
else
this->addEvent(makeTask(20000, std::bind2nd(std::mem_fun(&Game::AlreadyMuted), player)));
}
return;
}
#endif //REX_MUTED
#ifdef TLM_BEDS 
bool Game::loadBeds(std::string file) 
{ 
  xmlDocPtr doc; 
  doc = xmlParseFile(file.c_str()); 
  if (doc){ 
   xmlNodePtr root, p, tmp; 
   root = xmlDocGetRootElement(doc); 
   if (xmlStrcmp(root->name, (const xmlChar*)"beds")) { 
     xmlFreeDoc(doc); 
     return -1; 
   } 
    tmp = root->children; 
    int x,y,z,id; 
    while(tmp){ 
      if (strcmp((char*) tmp->name, "bed")==0){      
        x = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "x")); 
        y = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "y")); 
        z = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "z")); 
        id = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "id")); 
        Position mainPos(x, y, z); 
        Item *mainItem = Item::CreateItem(id);  
        Tile *mainTile = getTile(mainPos.x, mainPos.y, mainPos.z); 
        if (mainTile && mainItem){ 
          Position nextPos(x, y, z); 
          Item *nextItem = Item::CreateItem(id+1); 
          if (id == 1754 || id == 1758 || id == 1762 || id == 1766){ 
            nextPos.y++; 
          } 
          else if(id == 1756 || id == 1760 || id == 1764 || id == 1768){ 
            nextPos.x++; 
          } 
          Tile *nextTile = getTile(nextPos.x, nextPos.y, nextPos.z);    
          if (nextTile && nextItem){ 
            mainTile->addThing(mainItem); 
            mainItem->pos = mainPos; 
            nextTile->addThing(nextItem); 
            nextItem->pos = nextPos; 
          } 
        } 
      } 
      tmp = tmp->next; 
    }      
    xmlFreeDoc(doc); 
    return 0; 
  } 
  return -1; 
} 
std::string Game::getBedSleeper(const Position pos) 
{ 
  std::string file="data/beds.xml"; 
  xmlDocPtr doc; 
  doc = xmlParseFile(file.c_str()); 
  if (doc){ 
    xmlNodePtr root, tmp; 
    root = xmlDocGetRootElement(doc);        
    if (xmlStrcmp(root->name, (const xmlChar*)"beds")) { 
      xmlFreeDoc(doc); 
      return "Nobody"; 
    } 
    tmp = root->children; 
    while(tmp){ 
      if (strcmp((const char*) tmp->name, "bed")==0){ 
        int x = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "x")); 
        int y = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "y")); 
        int z = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "z")); 
        if (x == pos.x && y == pos.y && z == pos.z){ 
          return (const char*)xmlGetProp(tmp, (const xmlChar *)"name"); 
        }  
      } 
      tmp = tmp->next; 
    } 
    xmlFreeDoc(doc); 
  } 
  return "Nobody"; 
} 
unsigned int Game::getBedID(const Position pos) 
{ 
  std::string file="data/beds.xml"; 
  xmlDocPtr doc; 
  doc = xmlParseFile(file.c_str()); 
  if (doc){ 
    xmlNodePtr root, tmp; 
    root = xmlDocGetRootElement(doc);        
    if (xmlStrcmp(root->name, (const xmlChar*)"beds")) { 
      xmlFreeDoc(doc); 
      return 0; 
    } 
    tmp = root->children; 
    while(tmp){ 
      if (strcmp((const char*) tmp->name, "bed")==0){ 
        int x = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "x")); 
        int y = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "y")); 
        int z = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "z")); 
        if (x == pos.x && y == pos.y && z == pos.z){ 
          return atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "id")); 
        }  
      } 
      tmp = tmp->next; 
    } 
    xmlFreeDoc(doc); 
  } 
  return 0; 
} 

bool Game::changeBed(const Position pos, unsigned int oldid, std::string sleepname) 
{ 
  Tile *maintile = getTile(pos.x,pos.y,pos.z); 
  Item *mainitem = dynamic_cast<Item*>(maintile->getThingByStackPos(maintile->getThingCount()-1)); 
  if (mainitem && maintile->isHouse()){ 
    Position tilePos(pos.x, pos.y, pos.z); 
    if (oldid == 1754 || oldid == 1758 || oldid == 1762 || oldid == 1766){ 
      tilePos.y++; 
    } 
    else if(oldid == 1756 || oldid == 1760 || oldid == 1764 || oldid == 1768){ 
      tilePos.x++; 
    } 
    Tile *nexttile = getTile(tilePos.x,tilePos.y,tilePos.z); 
    Item *nextitem = dynamic_cast<Item*>(nexttile->getThingByStackPos(maintile->getThingCount()-1)); 
    if (nextitem && nexttile->isHouse()){  
      if (oldid == 1754 || oldid == 1758){ 
        mainitem->setID(oldid+8); 
      } 
      else if(oldid == 1756){ 
        mainitem->setID(1768); 
      } 
      else if(oldid == 1760){ 
        mainitem->setID(1764); 
      } 
      else if(oldid == 1762 || oldid == 1766){ 
        mainitem->setID(oldid-8); 
      } 
      else if(oldid == 1764){ 
        mainitem->setID(1760); 
      } 
      else if(oldid == 1768){ 
        mainitem->setID(1756); 
      } 
      nextitem->setID(mainitem->getID()+1); 
      creatureBroadcastTileUpdated(pos); 
      creatureBroadcastTileUpdated(tilePos); 
      
      std::string file="data/beds.xml"; 
      xmlDocPtr doc; 
      doc = xmlParseFile(file.c_str()); 
      if (doc){ 
        xmlNodePtr root, tmp; 
        root = xmlDocGetRootElement(doc);        
        if (xmlStrcmp(root->name, (const xmlChar*)"beds")) { 
          xmlFreeDoc(doc); 
          return false; 
        } 
        Position bedPos[1000];// 1000 = number of beds 
        unsigned int id[1000]; 
        std::string name[1000]; 
        int i = 0; 
        tmp = root->children; 
        while(tmp){ 
          if (strcmp((const char*) tmp->name, "bed")==0){ 
            i++; 
            bedPos[i].x = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "x"  )); 
            bedPos[i].y = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "y"  )); 
            bedPos[i].z = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "z"  )); 
            id[i]    = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "id" )); 
            name[i]   = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "name")); 
            if (bedPos[i] == pos){ 
              id[i]  = mainitem->getID(); 
              name[i] = sleepname; 
            } 
          } 
          tmp = tmp->next; 
        } 
        doc = xmlNewDoc((const xmlChar*)"1.0"); 
        doc->children = xmlNewDocNode(doc, NULL, (const xmlChar*)"beds", NULL); 
       root = doc->children; 
      
        std::stringstream sb; 
        for(int x = 1; x <= i; x++){ 
          tmp = xmlNewNode(NULL,(const xmlChar*)"bed"); 
          sb << bedPos[x].x; xmlSetProp(tmp, (const xmlChar*) "x" ,  (const xmlChar*)sb.str().c_str()); sb.str(""); 
          sb << bedPos[x].y; xmlSetProp(tmp, (const xmlChar*) "y" ,  (const xmlChar*)sb.str().c_str()); sb.str(""); 
          sb << bedPos[x].z; xmlSetProp(tmp, (const xmlChar*) "z" ,  (const xmlChar*)sb.str().c_str()); sb.str(""); 
          sb << id[x];    xmlSetProp(tmp, (const xmlChar*) "id",  (const xmlChar*)sb.str().c_str()); sb.str(""); 
          sb << name[x];   xmlSetProp(tmp, (const xmlChar*) "name", (const xmlChar*)sb.str().c_str()); sb.str(""); 
          xmlAddChild(root, tmp); 
        } 
       xmlSaveFile(file.c_str(), doc); 
       xmlFreeDoc(doc); 
        return true; 
      } 
    } 
  } 
  return false; 
} 
Position Game::getBedPos(std::string name) 
{ 
  std::string file="data/beds.xml"; 
  xmlDocPtr doc; 
  doc = xmlParseFile(file.c_str()); 
  if (doc){ 
    xmlNodePtr root, tmp; 
    root = xmlDocGetRootElement(doc);        
    if (xmlStrcmp(root->name, (const xmlChar*)"beds")) { 
      xmlFreeDoc(doc); 
      return Position(0xFFFF,0xFFFF,0xFF); 
    } 
    tmp = root->children; 
    while(tmp){ 
      if (strcmp((const char*) tmp->name, "bed")==0){ 
        std::string sleepname = (const char*)xmlGetProp(tmp, (const xmlChar *)"name"); 
        if (sleepname == name){ 
          int x = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "x")); 
          int y = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "y")); 
          int z = atoi((const char*) xmlGetProp(tmp, (const xmlChar*) "z")); 
          return Position(x,y,z); 
        }  
      } 
      tmp = tmp->next; 
    } 
    xmlFreeDoc(doc); 
  } 
  return Position(0xFFFF,0xFFFF,0xFF); 
} 

void Game::sendMagicEffect(const Position pos, unsigned char type) 
{ 
  SpectatorVec list; 
  SpectatorVec::iterator it; 
  getSpectators(Range(pos, true), list); 
   for(it = list.begin(); it != list.end(); ++it) { 
    Player* spectator = dynamic_cast<Player*>(*it); 
    if (spectator){ 
      spectator->sendMagicEffect(pos, type); 
    } 
  } 
} 
#endif //TLM_BEDS
#ifdef BD_CONDITION
    void Game::CreateCondition(Creature* creature, Creature* target, unsigned char animationColor, unsigned char damageEffect, unsigned char hitEffect, attacktype_t attackType, bool offensive, int maxDamage, int minDamage, long ticks, long count)
{
unsigned long targetID;
if(target)
targetID = target->getID();
else
targetID = 0;
MagicEffectTargetCreatureCondition magicCondition = MagicEffectTargetCreatureCondition(targetID);
magicCondition.animationColor = animationColor;
magicCondition.damageEffect = damageEffect;
magicCondition.hitEffect = hitEffect;
magicCondition.attackType = attackType;
magicCondition.maxDamage = maxDamage;
magicCondition.minDamage = minDamage;
magicCondition.offensive = offensive;
CreatureCondition condition = CreatureCondition(ticks, count, magicCondition);
creature->addCondition(condition, true);
Player *player = dynamic_cast<Player*>(creature);
if(player)
player->sendIcons();
}
void Game::doFieldDamage(Creature* creature, unsigned char animationColor, unsigned char damageEffect,
       unsigned char hitEffect, attacktype_t attackType, bool offensive, int damage)
{
   MagicEffectClass cd;
   cd.animationColor = animationColor;
   cd.damageEffect = damageEffect;
   cd.hitEffect = hitEffect;
   cd.attackType = attackType;
   cd.offensive = offensive;
   Player* itsHim = dynamic_cast<Player*>(getCreatureByID(creature->getID()));
   if(itsHim){ //Since that was causing damage/2 against player, here its my solution =)
      cd.maxDamage = damage*2;
      cd.minDamage = damage*2;
   }
   else{
      cd.maxDamage = damage;
      cd.minDamage = damage;
   }
   creatureMakeMagic(NULL, creature->pos, &cd);
}
#endif //BD_CONDITION
void Game::updateTile(const Position& pos)
{
  SpectatorVec list;
  SpectatorVec::iterator i;
  getSpectators(Range(pos), list);
  for(i = list.begin(); i != list.end(); ++i)
    (*i)->onTileUpdated(pos);
}
#ifdef ARNE_LUCK
void Game::LuckSystem(Player* player, Monster* monster){
// Crystal Ring + Crystal Amulet
if (player->items[SLOT_NECKLACE]->getID() == ITEM_CRYSTAL_RING && player->items[SLOT_RING]->getID() == ITEM_CRYSTAL_AMULET){
player->luck += 200;
}
// Crystal Ring only.
if (player->items[SLOT_RING]->getID() == ITEM_CRYSTAL_RING){
player->luck += 160;
}
// Crystal Amulet only.
if (player->items[SLOT_NECKLACE]->getID() == ITEM_CRYSTAL_AMULET){
player->luck += 40;
}
//Ring of the skies.
if (player->items[SLOT_RING]->getID() == ITEM_ROTS){
player->luck += 320;
}
//ROTS + Crystal Amulet
if (player->items[SLOT_RING]->getID() == ITEM_ROTS && player->items[SLOT_NECKLACE]->getID() == ITEM_CRYSTAL_AMULET){
player->luck += 360;
}
int loot = monster->getRandom();
loot += player->luck;
}
#endif //ARNE_LUCK
#ifdef BD_FOLLOW
void Game::checkCreatureFollow(unsigned long id)
{
   OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkCreatureFollow");
   Player *player = getPlayerByID(id);
   if(!player)
      return;
   if(!player->pathList.empty()) {
      Creature *followCreature = getCreatureByID(player->followCreature);
      if(followCreature && abs(followCreature->pos.x - player->pos.x) <= 1 && abs(followCreature->pos.y - player->pos.y) <= 1) player->pathList.clear();
      else {
         Position toPos = player->pathList.front();
         player->pathList.pop_front();
         player->lastmove = OTSYS_TIME();
         this->thingMove(player, player, toPos.x, toPos.y, player->pos.z, 1);
         flushSendBuffers();
      }
   }
   if(!player->pathList.empty()) {
      long long delay = player->getSleepTicks();
      stopEvent(player->eventCheckFollow);
      player->eventCheckFollow = addEvent(makeTask(delay, std::bind2nd(std::mem_fun(&Game::checkCreatureFollow), id)));
   } else {
      player->eventCheckFollow = 0;
   }
}
void Game::playerFollow(Player* player, Creature *followCreature)
{
OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerFollow()");
if(followCreature->isRemoved || player->isRemoved || !player || !followCreature){
player->eventCheckFollow = 0;
player->followCreature = 0;
return;
}
player->pathList = getPathTo(player, player->pos, followCreature->pos, false);
long long delay = player->getSleepTicks();
player->eventCheckFollow = addEvent(makeTask(delay, std::bind2nd(std::mem_fun(&Game::checkCreatureFollow), player->getID())));
}
void Game::playerSetFollowCreature(Player* player, unsigned long creatureid)
{
OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::playerSetFollowCreature()");
if(player->isRemoved || !player)
return;
if(creatureid == 0) {
stopEvent(player->eventCheckFollow);
player->eventCheckFollow = 0;
player->followCreature = 0;
}
Creature* followCreature = NULL;
if(creatureid != 0) {
followCreature = getCreatureByID(creatureid);
}
if(followCreature) {
player->followCreature = followCreature->getID();
stopEvent(player->eventCheckFollow);
playerFollow(player, followCreature);
}
}
#endif //BD_FOLLOW
#ifdef EOTSERV_SERVER_SAVE
bool Game::playerSave()
{
	//OTSYS_THREAD_LOCK_CLASS lockClass(gameLock);

	AutoList<Player>::listiterator it = Player::listPlayer.list.begin();
	while (it != Player::listPlayer.list.end())
	{
	    if((it->second)->access < g_config.ACCESS_PROTECT)
		IOPlayer::instance()->savePlayer(it->second);
		++it;
	}	
	return true;
}
bool Game::houseSave()
{
	//OTSYS_THREAD_LOCK_CLASS lockClass(gameLock);

	if(Houses::Save(this))
	return true;
	
return false;
}
bool Game::guildSave()
{
	//OTSYS_THREAD_LOCK_CLASS lockClass(gameLock);

	if(Guilds::Save())
	return true;
	
return false;
}
void Game::autoPlayerSave()
{
    //std::cout << ":: Autosaving characters...    ";
	if(playerSave()){
	//std::cout << "[Done.]" << std::endl;
    }
	else{
	//std::cout << "[Failed.]" << std::endl;
    }

	addEvent(makeTask(g_config.getGlobalNumber("playersave", 1)*60000, std::mem_fun(&Game::autoPlayerSave)));
}
void Game::autoHouseSave()
{
    //std::cout << ":: Autosaving houses...         ";
	if(houseSave()){
	//std::cout << "[Done.]" << std::endl;
    }
	else{
	//std::cout << "[Failed.]" << std::endl;
    }
	
	addEvent(makeTask(g_config.getGlobalNumber("housesave", 1)*60000, std::mem_fun(&Game::autoHouseSave)));
}
void Game::autoguildSave()
{
    //std::cout << ":: Autosaving guilds...         ";
	if(guildSave()){
	//std::cout << "[Done.]" << std::endl;
    }
	else{
	//std::cout << "[Failed.]" << std::endl;
    }
	
	addEvent(makeTask(g_config.getGlobalNumber("guildSave", 1)*60000, std::mem_fun(&Game::autoguildSave)));
}

#endif //EOTSERV_SERVER_SAVE
#ifdef RUL_WALKTO
void Game::cancelWalkTo(Player* player)
{
    OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::cancelWalkTo()");
    
    player->sendCancelWalk();
    if(player->eventAutoWalk) {
        stopEvent(player->eventAutoWalk);
    }
    player->walkTo.type = WALK_NONE;
}

Position Game::getDestinationPos(Player* player)
{
     OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::getDestinationPos()");
     
     Position dest_pos;
     std::vector<Position> dest_list;
     Tile* tile = NULL;
     for(int dx = player->walkTo.to_pos.x - 1; dx <= player->walkTo.to_pos.x + 1; dx++) {
         for(int dy = player->walkTo.to_pos.y - 1; dy <= player->walkTo.to_pos.y + 1; dy++) {
             if(!(dx == player->walkTo.to_pos.x && dy == player->walkTo.to_pos.y)) {
                 tile = getTile(dx, dy, player->walkTo.to_pos.z);
                 if(tile) {
                     if(tile->isBlocking(BLOCK_SOLID, true) == RET_NOERROR) {
                         dest_list.push_back(Position(dx, dy, player->walkTo.to_pos.z));
                     }
                 }
             }      
         }
     }
     
     if(dest_list.empty()) {
         return Position(0xFFFF, 0xFFFF, 0xFF);
     }
     else {
         return dest_list.at(0);
         //TODO: get best pos
         /*std::list<Position>::iterator pos;
         for(pos = dest_list.begin(); pos != dest_list.end(); ++pos) {
             if(pos != dest_list.end()) {
                 return (*pos);
                 if(dest_pos == 0) {
                     dest_pos = (*pos);
                 }
                 
                 if(dest_pos != (*pos)) {
                     if(player->walkTo.to_pos.x < player->pos.x) {
                         //right
                         if((*pos)->x-1 ==
                     }
                 }
                 if(player->walkTo.to_pos.x < player->pos.x) if((*pos)->x-1 == player->walkTo.to_pos.x) dest_pos = (*pos);
                 else if(player->walkTo.to_pos.x > player->pos.x) if((*pos)->x+1 == player->walkTo.to_pos.x) dest_pos = (*pos);
                 else if((*pos)->x == player->walkTo.to_pos.x) dest_pos = (*pos);
     
                 if(player->walkTo.to_pos.y < player->pos.y) //down
                 else if(player->walkTo.to_pos.y > player->pos.y) //up
                 else //y
             }
         }*/
     }
}

void Game::checkPlayerWalkTo(unsigned long id)
{
    OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::checkPlayerWalkTo()");
    
    Player* player = getPlayerByID(id);
    if(!player)
        return;
    
    int ticks = 0, dy = 0, dx = 0;
    Position nextStep;
    std::list<Position> route;
        
    switch((int)player->walkTo.type)
    {
        case WALK_USEITEM:
             if(abs(player->walkTo.to_pos.x - player->pos.x) <= 1 && 
                     abs(player->walkTo.to_pos.y - player->pos.y) <= 1 &&
                     player->pos.z == player->walkTo.to_pos.z) {
                 playerUseItem(player, player->walkTo.to_pos, player->walkTo.stack, player->walkTo.itemid, player->walkTo.index, true);
                 player->walkTo.type = WALK_NONE;
                 flushSendBuffers();
                 return;
             }
             
             //TODO: save route
             route = getPathTo(player, player->pos, player->walkTo.dest_pos);
             if(route.size() == 0) {
                 cancelWalkTo(player);
                 player->sendCancel("There is no way.");
                 return;
             }
             else { 
                 route.pop_front();
             }
             
             nextStep = route.front();
             route.pop_front();
             
             dx = nextStep.x - player->pos.x;
             dy = nextStep.y - player->pos.y;
             
             player->lastmove = OTSYS_TIME();
             
             thingMove(player, player, player->pos.x + dx, player->pos.y + dy, player->pos.z, 1);
             flushSendBuffers();
             
            
             ticks = (int)player->getSleepTicks();
		     player->eventAutoWalk = addEvent(makeTask(ticks, std::bind2nd(std::mem_fun(&Game::checkPlayerWalkTo), id)));
             break;
        case WALK_USEITEMEX:
             //TODO: playerUseItemEx
             break;
        case WALK_THROWITEM:
             break;
        case WALK_NONE:
             break;
        default:break;
    }
}
#endif //RUL_WALKTO
#ifdef _BBK_ANIM_TEXT
void Game::sendAnimatedTextExt(const Position pos,int aniColor,const std::string &text)
{
     SpectatorVec list;
     SpectatorVec::iterator it;
     getSpectators(Range(pos), list);
     for(it = list.begin(); it != list.end(); ++it){
        Player* spec = dynamic_cast<Player*>(*it);
        if(spec)
          spec->sendAnimatedText(pos, aniColor, text);
     }
}
#endif //_BBK_ANIM_TEXT
void Game::checkRecord()
{
    if(record < getPlayersOnline()){
        record = getPlayersOnline();
        saveRecord();

        std::stringstream record;
        record << "New record: " << getPlayersOnline() << " players are logged in." << std::endl;
        for(AutoList<Player>::listiterator it = Player::listPlayer.list.begin(); it != Player::listPlayer.list.end(); ++it)
            (*it).second->sendTextMessage(MSG_ADVANCE, record.str().c_str());
    }

}

bool Game::loadRecord()
{
    std::string filename = g_config.getGlobalString("datadir") + "record.xml";
    xmlDocPtr doc;
    xmlMutexLock(xmlmutex);
    doc = xmlParseFile(filename.c_str());

    if (doc)
    {
        xmlNodePtr root, tmp;
        root = xmlDocGetRootElement(doc); 

        if (xmlStrcmp(root->name, (const xmlChar*)"record"))
        {
            xmlFreeDoc(doc);
            xmlMutexUnlock(xmlmutex);
            return false;
        }

        record = atoi((const char*)xmlGetProp(root, (const xmlChar *)"record"));

        xmlFreeDoc(doc);
        xmlMutexUnlock(xmlmutex);
        return true;
    }

    xmlMutexUnlock(xmlmutex);
    return false;
}

bool Game::saveRecord()
{
    std::string filename = g_config.getGlobalString("datadir") + "record.xml";
    xmlDocPtr doc;
    xmlNodePtr root, tmp;
    xmlMutexLock(xmlmutex);
    time_t time = std::time(NULL);

    doc = xmlNewDoc((const xmlChar*)"1.0");
    doc->children = xmlNewDocNode(doc, NULL, (const xmlChar*)"record", NULL);
     root = doc->children;

    std::stringstream sb;
    sb << record; xmlSetProp(root, (const xmlChar*) "record", (const xmlChar*)sb.str().c_str()); sb.str("");
    sb << time; xmlSetProp(root, (const xmlChar*) "time", (const xmlChar*)sb.str().c_str()); sb.str("");

    xmlSaveFile(filename.c_str(), doc);
     xmlFreeDoc(doc);
    xmlMutexUnlock(xmlmutex);
    return true;
}
#ifdef FIX_REX
bool Game::FixByReX(const Tile *toTile)
{
if (toTile && !toTile->ground || !toTile){//!toTile
return false;
}
return true;
}
#endif //FIX_REX
#ifdef _NG_BBK_PVP__
bool Game::isPvpArena(Creature* c)
{
	if (!c)
		return false;
	Tile *tile = map->getTile(c->pos);
	return tile && tile->isPvpArena();
}
#endif //_NG_BBK_PVP__
void Game::newOwner(Player* newbie,Npc* mynpc , const Position& pos, long price)
{
    if (!newbie || !mynpc)
    return;
    Tile* tile = getTile(pos.x,pos.y,pos.z);
    unsigned long money = newbie->getMoney();
    if (tile && tile->getHouse())
	{
        time_t seconds;
        seconds = time(NULL);
        int time = seconds/3600;
        
        if(tile->getHouse()->getOwner() != "")//owner.empty()
        {
        mynpc->doSay("This house already have a owner!!");
        return;
        }
        else if (money < price)
        {
        mynpc->doSay("Sorry, you don't have money to buy this house");
        return;
        }
        else if(tile->getHouse() && tile->getHouse()->checkHouseCount(newbie) >= g_config.getGlobalNumber("maxhouses", 0)){
        mynpc->doSay("Sorry, you cannot buy more houses!!");
        return;
        }
        else if (newbie && !newbie->premmium){
        mynpc->doSay("You dont have a premium account.");
        return;
        }
        else if (tile->getHouse() && newbie->level < g_config.LEVEL_HOUSE){
        mynpc->doSay("You need higher level to buy house.");
        return;
        }
        /*else if (newbie->houseCount >= g_config.getGlobalNumber("maxhouse",10000))
        {
        mynpc->doSay("Sorry, you already have too many houses.");
        return;
        }*/

        newbie->substractMoney(price);
        //newbie->houseCount += 1;
        newbie->housex = pos.x;
        newbie->housey = pos.y;
        newbie->housez = pos.z;
		tile->getHouse()->setOwner(newbie->getName());
//        long time = (long long)(OTSYS_TIME()/1000);
//        time_t seconds;
//        seconds = time(NULL);
//        int time = seconds/3600;
        long housetime = (long)(g_config.getGlobalNumber("housetime",168));
        tile->getHouse()->setLast(time+housetime);
        
//        newbie->rentTime = time+housetime;
//        tile->getHouse()->setLast(time+housetime);
        newbie->rentPrice = price;
        mynpc->doSay("Thank you. Please pay correctly your rent house.");
		newbie->houseRightsChanged = true;
	}
}

void Game::LooseHouse(Player* player, const Position& pos)
{
OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::LooseHouse()");
if(!player)
return;

Tile* tile = getTile(pos.x,pos.y,pos.z);
    //loosing house
    if (tile && tile->getHouse())
	{
        if(tile->getHouse()->getOwner() == player->getName())
        {
//        Houses::RemoveHouseItems(this,tile->getHouse()->getName(),player);
        /*tile->getHouse()->setOwner("");
        tile->getHouse()->setSubOwners("");
        tile->getHouse()->setGuests("");*/
        Houses::Save(this);
        }
    }
}
/*
void Game::RemoveItems(Player* player, const std::string hname, bool first)
{
if(first)
addEvent(makeTask(2000, boost::bind(&Game::RemoveItems, this, player,  hname, false)));
else
{
//Houses::RemoveHouseItems(this,hname,player);
Houses::Save(this);
delete player;
}
}
*/
void Game::StackableUpdate(const Position& pos)
{

OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::StackableUpdate()");

SpectatorVec list;
SpectatorVec::iterator it;
getSpectators(Range(pos, true), list);

//players
for(it = list.begin(); it != list.end(); ++it) {
if(dynamic_cast<Player*>(*it)) {
(*it)->onTileUpdated(pos);
}
}

//none-players
for(it = list.begin(); it != list.end(); ++it) {
if(!dynamic_cast<Player*>(*it)) {
(*it)->onTileUpdated(pos);
}
}

return;
}

#ifdef TILES_WORK
void Game::fixtilebyrex(const Position& pos, bool logout)
{

OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::fixtilebyrex()");


    Tile* tileplayer = getTile(pos.x,pos.y,pos.z);

    if(!FixByReX(tileplayer))
    return;

    if (tileplayer){
    int groundid = tileplayer->ground->getID();
    if (!logout){
    if(groundid == 426)
     {
       tileplayer->ground->setID(425);
        StackableUpdate(pos);
     }
    if(groundid == 416)
     {
       tileplayer->ground->setID(417);
       StackableUpdate(pos);
     }
    if(groundid == 446)
     {
       tileplayer->ground->setID(447);
      StackableUpdate(pos);
     }
    }
    else
    {
    Item* fixdoorwithlvl = dynamic_cast<Item*>(tileplayer->getThingByStackPos(1));
    if(fixdoorwithlvl != NULL && !fixdoorwithlvl->isRemoved){
    if(fixdoorwithlvl->getID() == ITEM_CLOSE_DOOR || fixdoorwithlvl->getID() == ITEM_CLOSE_DOOR1)
    {
    fixdoorwithlvl->setID(fixdoorwithlvl->getID()-1);
    StackableUpdate(pos);
    return;
    }
    }
    if(groundid == 425)
     {
       tileplayer->ground->setID(426);
       StackableUpdate(pos);
     }
    if(groundid == 417)
     {
       tileplayer->ground->setID(416);
       StackableUpdate(pos);
     }
    if(groundid == 447)
     {
       tileplayer->ground->setID(446);
       StackableUpdate(pos);
     }
    }
 }
return;
}

void Game::TilesWork(Player* player, const Position& fromPos, const Position& toPos)
{


    //can't use threads here... (Crashing)


     Tile *tile   = getTile(toPos.x,toPos.y,toPos.z);//n pode mudar para toPos por causa da fun��o getTile
     Tile *oldtile   = getTile(fromPos.x,fromPos.y,fromPos.z);

     if(!FixByReX(tile) || !FixByReX(oldtile))
     return;

    if(!player)
    return;

    if(player->isRemoved)
    return;

     const unsigned short groundid = tile->ground->getID();
     const unsigned short oldgroundid = oldtile->ground->getID();
     bool contains = false;
    Item* itemTile = dynamic_cast<Item*>(tile->getThingByStackPos(1));
    if(itemTile && itemTile->getID() == 3310)
     {
        
        itemTile->setID(3324);
        //TransformOrRemoveItem(toPos,3310, false, 60000, true);
        addEvent(makeTask(60000, boost::bind(&Game::TransformOrRemoveItem, this, toPos, 3310, false)));
       StackableUpdate(toPos);
     }
    if(groundid == 3216)
     {
       tile->ground->setID(3217);
       StackableUpdate(toPos);
       
       contains = true;
     }
    if(oldgroundid == 3217)
     {
       oldtile->ground->setID(3216);
       StackableUpdate(fromPos);
     }
    if(groundid == 426)
     {
       tile->ground->setID(425);
       StackableUpdate(toPos);
       
       contains = true;
     }
    if(oldgroundid == 425)
     {
       oldtile->ground->setID(426);
       StackableUpdate(fromPos);
     }
    if(groundid == 416)
     {
       tile->ground->setID(417);
       StackableUpdate(toPos);

       contains = true;
     }
    if(oldgroundid == 417)
     {
       oldtile->ground->setID(416);
       StackableUpdate(fromPos);
     }
    if(groundid == 446)
     {
       tile->ground->setID(447);
       StackableUpdate(toPos);
       contains = true;
     }
    if(oldgroundid == 447)
     {
       oldtile->ground->setID(446);
       StackableUpdate(fromPos);
     }

//Fix made by ReX
if (contains){

    Position TilePos;
    TilePos.x = toPos.x;
    TilePos.y = toPos.y;
    TilePos.z = toPos.z;

    if (fromPos.x < toPos.x){
    TilePos.x += 1;
    }
    if (fromPos.y < toPos.y){
    TilePos.y += 1;
    }
    if (fromPos.x > toPos.x){
    TilePos.x -= 1;
    }
    if (fromPos.y > toPos.y){
    TilePos.y -= 1;
    }

Tile *DepotPos = getTile(TilePos.x,TilePos.y,TilePos.z);
//Item *Items = dynamic_cast<Item*>(DepotPos->getThingByStackPos(2));
Container *Items = dynamic_cast<Container*>(DepotPos->getThingByStackPos(2));

if(!FixByReX(DepotPos)){
return;
}
if(Items && !Items->isRemoved && (Items->getID() >= ITEM_DEPOTID && Items->getID() <= ITEM_DEPOTID1))
{
    //OPS Other fix
    if (!player->getDepot(Items->depot)){
    //player->sendTextMessage(MSG_RED_TEXT, "Your Depot don't work... please report to a GameMaster...");
    return;
    }
}
//std::cout << "Contains Pos " << Position(toPos.x+TilePos.x,toPos.y+TilePos.y,toPos.z+TilePos.z) << std::endl;
if (contains && Items != NULL && !Items->isRemoved && (Items->getID() >= ITEM_DEPOTID && Items->getID() <= ITEM_DEPOTID1)){
        int e = 0;
        Container* c = player->getDepot(Items->depot);
        std::stringstream s;
        e += c->getItemHoldingCount();
        s << "Your depot contains " << e;
        if(e > 1){
        s << " items.";
        }
        else
        {
        s << " item.";
        }
        //s << "\nPlease, don't stay with depot full at side of (DEPOT) container or you can loose items sent to you by parcel,house items and letters. Thanks..";
//        player->sendTextMessage(MSG_EVENT, s.str().c_str());
player->sendTextMessage(MSG_EVENT, s.str().c_str());
}
}
//return;
}//end


void Game::TransformOrRemoveItem(const Position& pos,const unsigned short itemID, bool isTile)
{
Tile* tile = getTile(pos.x,pos.y,pos.z);
if(!tile || !tile->ground)
return;
if (isTile)
{
tile->ground->setID(itemID);
StackableUpdate(pos);
}
else
{
Item* item = dynamic_cast<Item*>(tile->getThingByStackPos(1));
if (!item)
return;

item->setID(itemID);
StackableUpdate(pos);
//TODO: check stack pos to add item etc..
}

//if(first)
//this->addEvent(makeTask(time, boost::bind(&Game::TransformOrRemoveItem, this,pos, itemID, isTile,0,false)));
}
#endif //TILES_WORK
#ifdef BD_DOWN
bool Game::canDelete(Player *player, unsigned short itemid, Position toPos, Position fromPos, int from_stack, unsigned char count)
{
     OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::canDelete()");
     Tile *toTile = getTile(toPos.x,toPos.y,toPos.z);
 
#ifdef WALLS_FIX
Item* trash = dynamic_cast<Item*>(getThing(fromPos, from_stack, player));
if(trash && trash->isNotMoveable()) {
std::cout << "Nick: " << player->getName() << "chce usunac sciane." << std::endl; //Zapisujemy kto chce usun�� �ciane :)                          
return true;
}
#endif //WALLS_FIX
     if(itemid == 99 || (itemid >= 4329 && itemid <= 4555))
         return false;
     else if(fromPos.x != 0xFFFF && ((abs(player->pos.x - fromPos.x) > 1) || (abs(player->pos.y - fromPos.y) > 1) || (player->pos.z != fromPos.z)))
        return false;
     else if(map->canThrowObjectTo(player->pos, toPos, BLOCK_PROJECTILE) != RET_NOERROR)
        return false;
     else if (!FixByReX(toTile))
        return false;   
     else if(toPos.x == 0xFFFF)
         return false;
     if(trash){
         trash->pos = fromPos;
         if((abs(player->pos.x - toPos.x) > trash->throwRange) || (abs(player->pos.y - toPos.y) > trash->throwRange)) {        
            return false;
         }
 
         Tile *toTile = map->getTile(toPos);
         if(toTile){
             if((toTile->ground->getID() >= GROUND_WATER1 && toTile->ground->getID() <= GROUND_WATER4) || (toTile->ground->getID() >= GROUND_WATER5 && toTile->ground->getID() <= GROUND_WATER16) || (toTile->ground->getID() == GROUND_WATER17) || (toTile->ground->getID() >= GROUND_WATER18 && toTile->ground->getID() <= GROUND_WATER35) || (toTile->ground->getID() >= GROUND_WATER36 && toTile->ground->getID() <= GROUND_WATER38) || (toTile->ground->getID() >= GROUND_WATER39 && toTile->ground->getID() <= GROUND_WATER44) || (toTile->ground->getID() >= GROUND_LAVA1 && toTile->ground->getID() <= GROUND_SWAMP4) || (toTile->ground->getID() >= GROUND_BLACK_SWAMP1 && toTile->ground->getID() <= GROUND_BLACK_SWAMP4)){
                 if(!trash->isNotMoveable() && trash->isBlocking())
                     return false;
                 else if(trashObjects(player, toTile, trash, toPos, fromPos, from_stack, count))
                     return true;
             }
#ifdef WALLS_FIX
Item *toItem = dynamic_cast<Item*>(toTile->getTopTopItem());
if(trash && trash->isNotMoveable() && toItem->getID() == ITEM_DUSTBIN) {
std::cout << "Nick: " << player->getName() << "chce usunac sciane." << std::endl; //Zapisujemy kto chce usun�� �ciane :)                          
return true;
}
#else
Item *toItem = dynamic_cast<Item*>(toTile->getTopTopItem());
#endif //WALLS_FIX
             if(toItem && toItem->getID() == ITEM_DUSTBIN){
                 if(!trash->isNotMoveable() && trash->isBlocking())
                     return false;
                 else if(trashObjects(player, toTile, trash, toPos, fromPos, from_stack, count))
                     return true;
             }
         }
     }
 
     return false;
}     
 
bool Game::trashObjects(Player *player, Tile *toTile, Item *trash, Position toPos, Position fromPos, int from_stack, unsigned char count)
{
//OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::trashObjects()");
if(toTile){
         switch(toTile->ground->getID()){
             case GROUND_WATER1:
             case GROUND_WATER2:
             case GROUND_WATER3:
             case GROUND_WATER4:
             case GROUND_WATER5:
             case GROUND_WATER6:
             case GROUND_WATER7:
             case GROUND_WATER8:
             case GROUND_WATER9:
             case GROUND_WATER10:
             case GROUND_WATER11:
             case GROUND_WATER12:
             case GROUND_WATER13:
             case GROUND_WATER14:
             case GROUND_WATER15:
             case GROUND_WATER16:
             case GROUND_WATER17:
             case GROUND_WATER18:
             case GROUND_WATER19:
             case GROUND_WATER20:
             case GROUND_WATER21:
             case GROUND_WATER22:
             case GROUND_WATER23:
             case GROUND_WATER24:
             case GROUND_WATER25:
             case GROUND_WATER26:
             case GROUND_WATER27:
             case GROUND_WATER28:
             case GROUND_WATER29:
             case GROUND_WATER30:
             case GROUND_WATER31:
             case GROUND_WATER32:
             case GROUND_WATER33:
             case GROUND_WATER34:
             case GROUND_WATER35:
             case GROUND_WATER36:
             case GROUND_WATER37:
             case GROUND_WATER38:
             case GROUND_WATER39:
             case GROUND_WATER40:
             case GROUND_WATER41:
             case GROUND_WATER42:
             case GROUND_WATER43:
             case GROUND_WATER44:
             spectatorEffect(toPos, NM_ME_LOOSE_ENERGY);
             if(trashItems(player, trash, fromPos, from_stack, count))
                 return true;     
             break;
             case GROUND_LAVA1:
             case GROUND_LAVA2:
             case GROUND_LAVA3:
             case GROUND_LAVA4:   
             spectatorEffect(toPos, NM_ME_HITBY_FIRE);
             if(trashItems(player, trash, fromPos, from_stack, count))
                 return true;
             break;
             case GROUND_SWAMP1:
             case GROUND_SWAMP2:
             case GROUND_SWAMP3:
             case GROUND_SWAMP4:
             spectatorEffect(toPos, NM_ME_POISEN_RINGS);
             if(trashItems(player, trash, fromPos, from_stack, count))
                 return true;
             break;
             case GROUND_BLACK_SWAMP1:
             case GROUND_BLACK_SWAMP2:
             case GROUND_BLACK_SWAMP3:
             case GROUND_BLACK_SWAMP4:
             spectatorEffect(toPos, NM_ME_MORT_AREA);
             if(trashItems(player, trash, fromPos, from_stack, count))
                 return true;
             break;
}
}
return false;
}
 
bool Game::trashItems(Player *player, Item *trash, Position fromPos, int from_stack, unsigned char count)
{
    //OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::trashItems()");
 
    if(trash && player){
        if(trash->isStackable()){
            if(trash->getItemCountOrSubtype() > count){
                trash->setItemCountOrSubtype(trash->getItemCountOrSubtype() - count);
                sendUpdateThing(player,fromPos,trash,from_stack);
                player->updateInventoryWeigth();
                return true;
            }
            else{
                if(removeThing(player, fromPos, trash)){
                    player->updateInventoryWeigth();
                    return true;
                }
            }
        }
        else{
            if(removeThing(player, fromPos, trash)){
                player->updateInventoryWeigth();
                return true;
             }
        }
    }
    return false;
}
 
bool Game::canTeleportItem(Player *player, unsigned short itemid, Position toPos, Position fromPos, int from_stack, unsigned char count)
{
     OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::canTeleportItem()");
     Tile *toTile = getTile(toPos.x,toPos.y,toPos.z);
 
     if(itemid == 99 || (itemid >= 4329 && itemid <= 4555))
         return false;
     else if(fromPos.x != 0xFFFF && ((abs(player->pos.x - fromPos.x) > 1) || (abs(player->pos.y - fromPos.y) > 1) || (player->pos.z != fromPos.z)))
        return false;
     else if(map->canThrowObjectTo(player->pos, toPos, BLOCK_PROJECTILE) != RET_NOERROR)
        return false;
     else if(toPos.x == 0xFFFF)
         return false;
     else if (!FixByReX(toTile))
        return false;
     else if(!checkChangeFloor(map->getTile(toPos), getTile(toPos.x,toPos.y,toPos.z+1)))
        return false;
 
     Item* tpItem = dynamic_cast<Item*>(getThing(fromPos, from_stack, player));
     if(tpItem){
         tpItem->pos = fromPos;
         if((abs(player->pos.x - toPos.x) > tpItem->throwRange) || (abs(player->pos.y - toPos.y) > tpItem->throwRange)) {        
            return false;
         }
 
         if(tpItem->isStackable()){
             if(tpItem->getItemCountOrSubtype() > count){
                 tpItem->setItemCountOrSubtype(tpItem->getItemCountOrSubtype() - count);
                 Item *newitem = Item::CreateItem(tpItem->getID(), count);
                 addThing(player,getTeleportPos(toPos),newitem);
                 sendUpdateThing(player,fromPos,tpItem,from_stack);
                 player->updateInventoryWeigth();
                 return true;
             }
             else{
                 if(removeThing(player, fromPos, tpItem)){
                     addThing(player,getTeleportPos(toPos),tpItem);
                     player->updateInventoryWeigth();
                     return true;
                 }
             }
         }
         else{
             if(removeThing(player, fromPos, tpItem)){
                 addThing(player,getTeleportPos(toPos),tpItem);
                 player->updateInventoryWeigth();
                 return true;
             }
         }
     }
     return false;
}
 
Position Game::getTeleportPos(Position to)
{
Tile *toTile = map->getTile(to);
if(toTile->ground && toTile->ground->floorChangeDown())
{
Tile* downTile = getTile(to.x, to.y, to.z+1);
if(downTile){
//diagonal begin
if(downTile->floorChange(NORTH) && downTile->floorChange(EAST)){
return Position(to.x-1, to.y+1, to.z+1);
}
else if(downTile->floorChange(NORTH) && downTile->floorChange(WEST)){
return Position(to.x+1, to.y+1, to.z+1);
}
else if(downTile->floorChange(SOUTH) && downTile->floorChange(EAST)){
return Position(to.x-1, to.y-1, to.z+1);
}
else if(downTile->floorChange(SOUTH) && downTile->floorChange(WEST)){
return Position(to.x+1, to.y-1, to.z+1);
}
//diagonal end
else if(downTile->floorChange(NORTH)){
return Position(to.x, to.y+1, to.z+1);
}
else if(downTile->floorChange(SOUTH)){
return Position(to.x, to.y-1, to.z+1);
}
else if(downTile->floorChange(EAST)){
return Position(to.x-1, to.y, to.z+1);
}
else if(downTile->floorChange(WEST)){
return Position(to.x+1, to.y, to.z+1);
}
//floor change down
else if(Item::items[toTile->ground->getID()].floorChangeDown){
return Position(to.x, to.y, to.z+1);
}
else {
return Position(to.x, to.y, to.z+1);
}
}
}
//diagonal begin
else if(toTile->floorChange(NORTH) && toTile->floorChange(EAST)){
return Position(to.x+1, to.y-1, to.z-1);
}
else if(toTile->floorChange(NORTH) && toTile->floorChange(WEST)){
return Position(to.x-1, to.y-1, to.z-1);
}
else if(toTile->floorChange(SOUTH) && toTile->floorChange(EAST)){
return Position(to.x+1, to.y+1, to.z-1);
}
else if(toTile->floorChange(SOUTH) && toTile->floorChange(WEST)){
return Position(to.x-1, to.y+1, to.z-1);
}                  
else if(toTile->floorChange(NORTH)){
return Position(to.x, to.y-1, to.z-1);
}
else if(toTile->floorChange(SOUTH)){
return Position(to.x, to.y+1, to.z-1);
}
else if(toTile->floorChange(EAST)){
return Position(to.x+1, to.y, to.z-1);
}
else if(toTile->floorChange(WEST)){
return Position(to.x-1, to.y, to.z-1);
}                                      
if(!toTile){
Tile* downTile = getTile(to.x, to.y, to.z+1);
if(!downTile)
{
return Position(0,0,0);
}
else if(downTile->floorChange(NORTH) && downTile->floorChange(EAST)){
return Position(to.x-2, to.y+2, to.z+1);
}
else if(downTile->floorChange(NORTH) && downTile->floorChange(WEST)){
return Position(to.x+2, to.y+2, to.z+1);
}
else if(downTile->floorChange(SOUTH) && downTile->floorChange(EAST)){
return Position(to.x-2, to.y-2, to.z+1);
}
else if(downTile->floorChange(SOUTH) && downTile->floorChange(WEST)){
return Position(to.x+2, to.y-2, to.z+1);
}                                                     
else if(downTile->floorChange(NORTH)){
return Position(to.x, to.y + 1, to.z+1);
}
else if(downTile->floorChange(SOUTH)){
return Position(to.x, to.y - 1, to.z+1);
}
else if(downTile->floorChange(EAST)){
return Position(to.x - 1, to.y, to.z+1);
}
else if(downTile->floorChange(WEST)){
return Position(to.x + 1, to.y, to.z+1);
}
}
}
 
bool Game::checkChangeFloor(Tile *toTile, Tile* downTile)
{
if(toTile->ground && toTile->ground->floorChangeDown())
{
if(downTile){
if(downTile->floorChange(NORTH) && downTile->floorChange(EAST)){
return true;
}
else if(downTile->floorChange(NORTH) && downTile->floorChange(WEST)){
return true;
}
else if(downTile->floorChange(SOUTH) && downTile->floorChange(EAST)){
return true;
}
else if(downTile->floorChange(SOUTH) && downTile->floorChange(WEST)){
return true;
}
else if(downTile->floorChange(NORTH)){
return true;
}
else if(downTile->floorChange(SOUTH)){
return true;
}
else if(downTile->floorChange(EAST)){
return true;
}
else if(downTile->floorChange(WEST)){
return true;
}
else if(Item::items[toTile->ground->getID()].floorChangeDown){
return true;
}
else { 
return true;
}
}
}
else if(toTile->floorChange(NORTH) && toTile->floorChange(EAST)){
return true;
}
else if(toTile->floorChange(NORTH) && toTile->floorChange(WEST)){
return true;
}
else if(toTile->floorChange(SOUTH) && toTile->floorChange(EAST)){
return true;
}
else if(toTile->floorChange(SOUTH) && toTile->floorChange(WEST)){
return true;
}                    
else if(toTile->floorChange(NORTH)){
return true;
}
else if(toTile->floorChange(SOUTH)){
return true;
}
else if(toTile->floorChange(EAST)){
return true;
}
else if(toTile->floorChange(WEST)){
return true;
}
if(!toTile){
if(!downTile)
{
return false;
}
else if(downTile->floorChange(NORTH) && downTile->floorChange(EAST)){
return true;
}
else if(downTile->floorChange(NORTH) && downTile->floorChange(WEST)){
return true;
}
else if(downTile->floorChange(SOUTH) && downTile->floorChange(EAST)){
return true;
}
else if(downTile->floorChange(SOUTH) && downTile->floorChange(WEST)){
return true;
}                                                      
else if(downTile->floorChange(NORTH)){
return true;
}
else if(downTile->floorChange(SOUTH)){
return true;
}
else if(downTile->floorChange(EAST)){
return true;
}
else if(downTile->floorChange(WEST)){
return true;
}
}
return false;
}
/*
void Game::StackableUpdate(const Position& pos)
{

OTSYS_THREAD_LOCK_CLASS lockClass(gameLock, "Game::StackableUpdate()");

SpectatorVec list;
SpectatorVec::iterator it;
getSpectators(Range(pos, true), list);

//players
for(it = list.begin(); it != list.end(); ++it) {
if(dynamic_cast<Player*>(*it)) {
(*it)->onTileUpdated(pos);
}
}

//none-players
for(it = list.begin(); it != list.end(); ++it) {
if(!dynamic_cast<Player*>(*it)) {
(*it)->onTileUpdated(pos);
}
}

return;
}

bool Game::FixByReX(const Tile *toTile)
{
if (toTile && !toTile->ground || !toTile){//!toTile
return false;
}
return true;
}

*/

void Game::spectatorEffect(Position pos, unsigned char type)
{
    SpectatorVec list;
    SpectatorVec::iterator it;
    getSpectators(Range(pos, true), list);
          
    for(it = list.begin(); it != list.end(); ++it) {
        if(Player* p = dynamic_cast<Player*>(*it)) {
            p->sendMagicEffect(pos, type);
        }
    }
}
#endif //BD_DOWN
#ifdef CAYAN_POISONMELEE
void Game::PoisonMelee(Creature* creature, Creature* target, unsigned char animationColor, unsigned char damageEffect, unsigned char hitEffect, attacktype_t attackType, bool offensive, int maxDamage, int minDamage, long ticks, long count)
{
    unsigned long targetID;
    if(target)
    targetID = target->getID();
    else 
    targetID = 0;
    
    MagicEffectTargetCreatureCondition magicCondition = MagicEffectTargetCreatureCondition(targetID);
    magicCondition.animationColor = animationColor;
    magicCondition.damageEffect = damageEffect;
    magicCondition.hitEffect = hitEffect;
    magicCondition.attackType = attackType;
    magicCondition.maxDamage = maxDamage;
    magicCondition.minDamage = minDamage;
    magicCondition.offensive = offensive;
    CreatureCondition condition = CreatureCondition(ticks, count, magicCondition);
    creature->addCondition(condition, true);
    
    Player *player = dynamic_cast<Player*>(creature);
    if(player)
    player->sendIcons();
}
#endif //CAYAN_POISONMELEE
#ifdef CAYAN_POISONARROW
void Game::poisonArrow(Creature* c, const Position& pos)
{
	MagicEffectAreaNoExhaustionClass runeAreaSpell;

	runeAreaSpell.attackType = ATTACK_POISON;
	runeAreaSpell.animationEffect = NM_ME_MAGIC_POISEN;
	runeAreaSpell.hitEffect = NM_ME_POISEN_RINGS;
	runeAreaSpell.areaEffect = NM_ME_POISEN_RINGS;
	runeAreaSpell.animationColor = 19;
	runeAreaSpell.drawblood = true;
	runeAreaSpell.offensive = true;

	runeAreaSpell.direction = 1;
	creatureThrowRune(c, pos, runeAreaSpell);
	int probability = random_range(1, 100);
	Player *attacker = dynamic_cast<Player*>(c);
	Creature *attackedCreature = getCreatureByPosition(pos.x, pos.y, pos.z);
    if((attacker && attackedCreature && (attackedCreature->getImmunities()) != ATTACK_POISON) && probability <  60)
        PoisonMelee(attackedCreature, attacker, COLLOR_GREEN, NM_ME_POISEN_RINGS, NM_ME_POISEN_RINGS, ATTACK_POISON, true, g_config.getGlobalNumber("poisonarrowdamage", 10), g_config.getGlobalNumber("poisonarrowdamage", 10), (long)g_config.getGlobalNumber("exhausted", 1500), (long)g_config.getGlobalNumber("poisonarrowhits", 5));
}
#endif //CAYAN_POISONARROW   
#ifdef CAYAN_SPELLBOOK
bool Game::setSpellbookText(Player* player, Item* item)
{
    std::stringstream text;
    item->setText("");
    for(StringVector::iterator it = player->learnedSpells.begin(); it != player->learnedSpells.end(); ++it){
        if(it != player->learnedSpells.end()){
            std::map<std::string, Spell*>* tmp = spells.getAllSpells();
            if(tmp){
                std::map<std::string, Spell*>::iterator sit = tmp->find((*it));
                if(sit != tmp->end()){
                    InstantSpell* instant = dynamic_cast<InstantSpell*>(sit->second);
                    if(instant){
                        text << "\'" << instant->getName() << "' � \'" << instant->getWords() << "\' � mana: " << instant->getMana() << " � level: " << instant->getMagLv();
                        text << "\n";
                    }
                }
            }
        }
    }   
    item->setText(text.str());
    player->sendTextWindow(item, strlen(text.str().c_str()), true);
    return true; 
}

#endif //CAYAN_SPELLBOOK

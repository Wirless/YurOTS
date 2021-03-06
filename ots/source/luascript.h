//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// class which takes care of all data which must get saved in the player
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


#ifndef __LUASCRIPT_H__
#define __LUASCRIPT_H__

#include <string>
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

extern "C" struct lua_State;

#ifdef YUR_HIGH_LEVELS
typedef __int64 exp_t;
#else
typedef unsigned long exp_t;
#endif //YUR_HIGH_LEVELS

class LuaScript
{
public:
	LuaScript();
	~LuaScript();
#ifdef MAGIC_DAMAGE
    bool EFFECT_ATACK;
#endif //MAGIC_DAMAGE
#ifdef __BBK_TRAINING_SWITCH
	int TRAINING_TICKS;
	int SWITCH_TICKS;
#endif //__BBK_TRAINING_SWITCH
#ifdef __BBK_TRAINING
	int TRAINING_TICKS;
	int REWRITE_TICKS;
#endif //__BBK_TRAINING
	int DROP_RATE;
	double NO_VOC_SPEED;
    double SORC_SPEED;
    double DRUID_SPEED;
    double PALLY_SPEED;
    double KNIGHT_SPEED;

#ifdef BBK_MAGIC_DAMAGE
    int BEAR;
    int VODOO_DOLL;
    int BLUE_ROBE;
    int MYSTIC_TURBAN;
    int MAGICIAN_HAT;
    int BURNING_HEART;
    int GINSEN;
    int FROZEN_STARLIGHT;
    int MUSIC_PIECE;
    int TEAR_OF_DARAMAN;
    int BUNNY_SLIPPERS;
#endif //BBK_MAGIC_DAMAGE

//	int BLUE_ROBE;
//	int MYSTIC_TURBAN;
	int MAGIC_PLATE_ARMOR;
	int CRUSED_COWL;

    bool LVL_YELL;
    bool LEVEL_HOUSE;

#ifdef YUR_MULTIPLIERS
	exp_t EXP_MUL;
	exp_t EXP_MUL_PVP;
	int HEALTH_TICK_MUL;
	int MANA_TICK_MUL;
	int ROHHEALTH;
	int ROHMANA;
	int LIFERINGHEALTH;
	int LIFERINGMANA;
	int CAP_GAIN[5];
	int MANA_GAIN[5];
	int HP_GAIN[5];
	int WEAPON_MUL[5];
	int SHIELD_MUL[5];
	int DIST_MUL[5];
	int MANA_MUL[5];
	int SOFTMANA;
    int SOFTHEALTH;
#endif //YUR_MULTIPLIERS

	int POWER_RING;
	int SWORD_RING;
	int AXE_RING;
	int CLUB_RING;

bool ADVANCE_ANI;

#ifdef TR_ANTI_AFK
	int KICK_TIME;
#endif //TR_ANTI_AFK

#ifdef YUR_LEARN_SPELLS
	bool LEARN_SPELLS;
#endif //YUR_LEARN_SPELLS

#ifdef TLM_HOUSE_SYSTEM
	int ACCESS_HOUSE;
	int MAX_HOUSE_TILE_ITEMS;
#endif //TLM_HOUSE_SYSTEM

#ifdef TR_SUMMONS
	bool  SUMMONS_ALL_VOC;
	size_t MAX_SUMMONS;
#endif //TR_SUMMONS
    int BLUE_RBE;
    
#ifdef TLM_SKULLS_PARTY
	int HIT_TIME;
	int WHITE_TIME;
	int RED_TIME;
	int FRAG_TIME;
	int RED_UNJUST;
	int BAN_UNJUST;
#endif //TLM_SKULLS_PARTY

#ifdef SD_BURST_ARROW
	double BURST_DMG_LVL;
	double BURST_DMG_MLVL;
	double BURST_DMG_LO;
	double BURST_DMG_HI;
#endif //SD_BURST_ARROW

#ifdef GOLD_BOLT
	double GOLD_DMG_LVL;
	double GOLD_DMG_MLVL;
	double GOLD_DMG_LO;
	double GOLD_DMG_HI;
#endif //GOLD_BOLT

#ifdef YUR_CONFIG_CAP
	bool CAP_SYSTEM;
#endif //YUR_CONFIG_CAP

#ifdef BDB_REPLACE_SPEARS
	int SPEAR_LOSE_CHANCE;
#endif //BDB_REPLACE_SPEARS

#ifdef YUR_PREMIUM_PROMOTION
	bool FREE_PREMMY;
	bool QUEUE_PREMMY;
#endif //YUR_PREMIUM_PROMOTION

#ifdef YUR_CVS_MODS
	std::string VOCATIONS[5];
	std::string PROMOTED_VOCATIONS[5];
	int DIE_PERCENT_EXP;
	int DIE_PERCENT_MANA;
	int DIE_PERCENT_SKILL;
	int DIE_PERCENT_EQ;
	int DIE_PERCENT_BP;
	int DIE_PERCENT_EXP_2;
	int DIE_PERCENT_MANA_2;
	int DIE_PERCENT_SKILL_2;
	int DIE_PERCENT_EQ_2;
	int DIE_PERCENT_BP_2;
#ifdef BLESS
    int DIE_BLESS_1;
    int DIE_BLESS_2;
    int DIE_BLESS_3;
    int DIE_BLESS_4;
    int DIE_BLESS_5;
    int DIE_BLESS_1_PROMOTION;
    int DIE_BLESS_2_PROMOTION;
    int DIE_BLESS_3_PROMOTION;
    int DIE_BLESS_4_PROMOTION;
    int DIE_BLESS_5_PROMOTION;
#endif //BLESS
	int ROOK_TEMPLE_X;
	int ROOK_TEMPLE_Y;
    int ROOK_TEMPLE_Z;
    int ROOK_TEMPLE_LVL;
    
    bool SUMMON_BODIES;

    bool EXTRA_MSG_DEAD;
    bool MSG_DEAD;
	long PZ_LOCKED;
	long EXHAUSTED;
	long EXHAUSTED_ADD;
	long EXHAUSTED_HEAL;
	int ACCESS_PROTECT;
	int ACCESS_REMOTE;
	int ACCESS_TALK;
	int ACCESS_ENTER;
	int ACCESS_LOOK;
	int ACCESS_BUGREPORT_CTRLZ;
	int MAX_DEPOT_ITEMS;
	std::string DATA_DIR;
#endif //YUR_CVS_MODS

#ifdef JD_DEATH_LIST
	size_t MAX_DEATH_ENTRIES;
#endif //JD_DEATH_LIST

#ifdef JD_WANDS
	int MANA_SNAKEBITE;
	int MANA_MOONLIGHT;
	int MANA_VOLCANIC;
	int MANA_QUAGMIRE;
	int MANA_TEMPEST;

	int MANA_VORTEX;
	int MANA_DRAGONBREATH;
	int MANA_MLS;
	int MANA_PLAGUE;
	int MANA_COSMIC;
	int MANA_INFERNO;
	int MANA_SILV;
	int PANDEMONIUM_ARMOR;
int MAGICZNE_KLAPKI_ZAPOMNIENIA;
int SAPPHIRE_LEGS;
int SAPPHIRE_ARMOR;
int SAPPHIRE_HELMET;
	int MANA_SPRITE;

	int RANGE_SNAKEBITE;
	int RANGE_MOONLIGHT;
	int RANGE_VOLCANIC;
	int RANGE_QUAGMIRE;
	int RANGE_TEMPEST;

	int RANGE_VORTEX;
	int RANGE_DRAGONBREATH;
	int RANGE_PLAGUE;
	int RANGE_COSMIC;
	int RANGE_INFERNO;
	int RANGE_SILV;
	int RANGE_SPRITE;
#endif //JD_WANDS

  int OpenFile(const char* file);
  int getField (const char *key);
  void LuaScript::setField (const char *index, int value);
  //static version
  static int getField (lua_State *L , const char *key);
  static void setField (lua_State *L, const char *index, int val);
  // get a global string
#ifdef _BDD_LUA_FIX
  std::string getGlobalString(std::string var, const std::string &defString = "~");
  int getGlobalNumber(std::string var, const int defNum = 0);
  std::string getGlobalStringField (std::string var, const int key, const std::string &defString = "~");
  #else
  std::string getGlobalString(std::string var, const std::string &defString = "");
  int getGlobalNumber(std::string var, const int defNum = 0);
  std::string getGlobalStringField (std::string var, const int key, const std::string &defString = "");
#endif //_BDD_LUA_FIX
  // set a var to a val
  int setGlobalString(std::string var, std::string val);
  int setGlobalNumber(std::string var, int val);

protected:
	std::string luaFile;   // the file we represent
	lua_State*  luaState;  // our lua state
};


#endif  // #ifndef __LUASCRIPT_H__

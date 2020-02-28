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

#ifndef __CONST76_H__
#define __CONST76_H__

#define NETWORKMESSAGE_MAXSIZE 16768

enum MagicEffectClasses {
	NM_ME_DRAW_BLOOD  	= 0x00,
	NM_ME_LOOSE_ENERGY	= 0x01, //fishing?
	NM_ME_PUFF			= 0x02,
	NM_ME_BLOCKHIT		= 0x03,
	NM_ME_EXPLOSION_AREA   = 0x04,
	NM_ME_EXPLOSION_DAMAGE = 0x05,
	NM_ME_FIRE_AREA        = 0x06,
	NM_ME_YELLOW_RINGS     = 0x07,
	NM_ME_POISEN_RINGS     = 0x08,
	NM_ME_HIT_AREA         = 0x09,
	NM_ME_ENERGY_AREA      = 0x0A, //10
	NM_ME_ENERGY_DAMAGE    = 0x0B, //11
	NM_ME_MAGIC_ENERGIE    = 0x0C, //12
	NM_ME_MAGIC_BLOOD      = 0x0D, //13
	NM_ME_MAGIC_POISEN     = 0x0E, //14
	NM_ME_HITBY_FIRE       = 0x0F, //15
	NM_ME_POISEN           = 0x10, //16
	NM_ME_MORT_AREA        = 0x11, //17
	NM_ME_SOUND_GREEN      = 0x12, //18
	NM_ME_SOUND_RED        = 0x13, //19
	NM_ME_POISON_AREA      = 0x14, //20
	NM_ME_SOUND_YELLOW     = 0x15, //21
	NM_ME_SOUND_PURPLE     = 0x16, //22
	NM_ME_SOUND_BLUE       = 0x17, //23
	NM_ME_SOUND_WHITE      = 0x18, //24

};

#define NM_ANI_SPEAR             0
#define NM_ANI_BOLT              1
#define NM_ANI_ARROW             2
#define NM_ANI_FIRE              3
#define NM_ANI_ENERGY            4
#define NM_ANI_POISONARROW       5
#define NM_ANI_BURSTARROW        6
#define NM_ANI_THROWINGSTAR      7
#define NM_ANI_THROWINGKNIFE     8
#define NM_ANI_SMALLSTONE        9
#define NM_ANI_SUDDENDEATH       10
#define NM_ANI_LARGEROCK         11
#define NM_ANI_SNOWBALL          12
#define NM_ANI_POWERBOLT         13
#define NM_ANI_FLYPOISONFIELD    14



enum SpeakClasses {
	SPEAK_SAY	  		= 0x01, 
	SPEAK_WHISPER 		= 0x02, 
	SPEAK_YELL	  		= 0x03, 
	//SPEAK_			= 0x0D,	
		
	SPEAK_BROADCAST		= 0x09, 
		
	SPEAK_PRIVATE 		= 0x04, 
	SPEAK_PRIVATE_RED	= 0x0B,	//@name@text
	//SPEAK_			= 0x0F, 
		
	//SPEAK_CHANNEL?? 	= 0x06,
		
	//SPEAK_CHANNEL?? 	= 0x07,
	//SPEAK_CHANNEL?? 	= 0x08,
	
	#ifdef VITOR_RVR_HANDLING

SPEAK_RVR_REQUEST = 0x06, //request the ctrl+r
SPEAK_RVR_ANSWER = 0x07, //when gm answer
SPEAK_RVR_COMPLAIN = 0x08, //when player answer

#endif
	
	SPEAK_CHANNEL_Y		= 0x05,	//yellow
	SPEAK_CHANNEL_R1	= 0x0A,	//red - #c text -- gamemaster command
	SPEAK_CHANNEL_R2	= 0x0E,	//red - #d text -- counsellor command(?)
	SPEAK_CHANNEL_O		= 0x0C,	//orange
	
#ifdef SDG_VIOLATIONS
	SPEAK_CHANNEL_RV1 	= 0x06,     // report violation
	SPEAK_CHANNEL_RV2 	= 0x07,     // answer report (couns/gm)
	SPEAK_CHANNEL_RV3 	= 0x08,     // reply from reportee
#endif
		
	SPEAK_MONSTER1 		= 0x10,
	SPEAK_MONSTER2 		= 0x11,
};

enum MessageClasses {
     
    MSG_YELLOW    = 0x01,
    MSG_PRIVATE   = 0x04,
    MSG_ORANGE    = 0x11,
	MSG_RED_INFO  = 0x12,
	MSG_ADVANCE   = 0x13,
	MSG_EVENT     = 0x14,
	/*MSG_EVENT     = 0x15,	*/
	MSG_INFO      = 0x16,
	MSG_SMALLINFO = 0x17,
	MSG_BLUE_TEXT = 0x18,
	MSG_RED_TEXT  = 0x19,
};


enum FluidClasses {
	FLUID_EMPTY = 0x00,	//note: class = fluid_number mod 8
	FLUID_BLUE	= 0x01,
	FLUID_RED	= 0x02,
	FLUID_BROWN = 0x03,
	FLUID_GREEN = 0x04,
	FLUID_YELLOW= 0x05,
	FLUID_WHITE = 0x06,
	FLUID_PURPLE= 0x07,
};

enum e_fluids {	
	FLUID_WATER	= FLUID_BLUE,
	FLUID_BLOOD	= FLUID_RED,
	FLUID_BEER = FLUID_BROWN,
	FLUID_SLIME = FLUID_GREEN,
	FLUID_LEMONADE= FLUID_YELLOW,
	FLUID_MILK = FLUID_WHITE,
	FLUID_MANAFLUID= FLUID_PURPLE,
	FLUID_LIFEFLUID= FLUID_RED+8,
	FLUID_OIL = FLUID_BROWN+8,
	FLUID_WINE = FLUID_PURPLE+8,
};


enum Icons {
	ICON_POISON = 1,
	ICON_BURN = 2, 
	ICON_ENERGY =  4, 
	ICON_DRUNK = 8, 
	ICON_MANASHIELD = 16, 
	ICON_PARALYZE = 32, 
	ICON_HASTE = 64, 
	ICON_SWORDS = 128
};

enum WeaponType 
{
	NONE = 0,
	SWORD = 1, 
	CLUB = 2,
	AXE = 3,
	SHIELD = 4,
	DIST = 5,
	MAGIC = 6,
	AMO = 7,
};

enum amu_t{
	AMU_NONE = 0,
	AMU_BOLT = 1,
	AMU_ARROW = 2
};


enum subfight_t {
	DIST_NONE = 0,
	DIST_BOLT = NM_ANI_BOLT,
	DIST_ARROW = NM_ANI_ARROW, 
	DIST_FIRE = NM_ANI_FIRE,
	DIST_ENERGY = NM_ANI_ENERGY,
	DIST_POISONARROW = NM_ANI_POISONARROW,
	DIST_BURSTARROW = NM_ANI_BURSTARROW,
	DIST_THROWINGSTAR = NM_ANI_THROWINGSTAR,
	DIST_THROWINGKNIFE = NM_ANI_THROWINGKNIFE,
	DIST_SMALLSTONE = NM_ANI_SMALLSTONE,
	DIST_SUDDENDEATH = NM_ANI_SUDDENDEATH,
	DIST_LARGEROCK = NM_ANI_LARGEROCK,
	DIST_SNOWBALL = NM_ANI_SNOWBALL,
	DIST_POISONFIELD = NM_ANI_FLYPOISONFIELD,  	
	DIST_POWERBOLT = NM_ANI_POWERBOLT,
    DIST_SPEAR = NM_ANI_SPEAR
    
	
};

enum magicfield_t {
	MAGIC_FIELD_FIRE,
	MAGIC_FIELD_POISON_GREEN,
	MAGIC_FIELD_ENERGY,	
};

#ifdef BD_DOWN
  enum ground_t {
    GROUND_UP_STONE     = 426,
    GROUND_DOWN_STONE   = 425,
    GROUND_UP_WOODEN    = 446,
    GROUND_DOWN_WOODEN  = 447,
    GROUND_UP_OLDWOOD   = 549,
    GROUND_DOWN_OLDWOOD = 550,
    GROUND_INVISIBLE1   = 459,
    GROUND_INVISIBLE2   = 460,
    GROUND_GRASS        = 4526,
    GROUND_WATER1               = 490,
    GROUND_WATER2               = 491,
    GROUND_WATER3               = 492,
    GROUND_WATER4               = 493,
    GROUND_WATER5               = 618,
    GROUND_WATER6               = 619, 
    GROUND_WATER7               = 620,
    GROUND_WATER8               = 621,
    GROUND_WATER9               = 622,
    GROUND_WATER10              = 623,
    GROUND_WATER11              = 624,
    GROUND_WATER12              = 625,
    GROUND_WATER13              = 626,
    GROUND_WATER14              = 627,
    GROUND_WATER15              = 628,
    GROUND_WATER16              = 629,
    GROUND_WATER17              = 3258,
    GROUND_WATER18              = 4608,
    GROUND_WATER19              = 4609,
    GROUND_WATER20              = 4610,
    GROUND_WATER21              = 4611,
    GROUND_WATER22              = 4612,
    GROUND_WATER23              = 4613,
    GROUND_WATER24              = 4614,
    GROUND_WATER25              = 4615,
    GROUND_WATER26              = 4616,
    GROUND_WATER27              = 4617,
    GROUND_WATER28              = 4618,
    GROUND_WATER29              = 4619,
    GROUND_WATER30              = 4620,
    GROUND_WATER31              = 4621,
    GROUND_WATER32              = 4622,
    GROUND_WATER33              = 4623,
    GROUND_WATER34              = 4624,
    GROUND_WATER35              = 4625,
    GROUND_WATER36              = 4664,
    GROUND_WATER37              = 4665,
    GROUND_WATER38              = 4666,
    GROUND_WATER39              = 4820,
    GROUND_WATER40              = 4821,
    GROUND_WATER41              = 4822,
    GROUND_WATER42              = 4823,
    GROUND_WATER43              = 4824,
    GROUND_WATER44              = 4825,
    GROUND_LAVA1                = 598,
    GROUND_LAVA2                = 599,
    GROUND_LAVA3                = 600,
    GROUND_LAVA4                = 601,
    GROUND_BLACK_SWAMP1         = 708,
    GROUND_BLACK_SWAMP2         = 709,
    GROUND_BLACK_SWAMP3         = 710,
    GROUND_BLACK_SWAMP4         = 711,
    GROUND_SWAMP1       = 602,
    GROUND_SWAMP2       = 603,
    GROUND_SWAMP3       = 604,
    GROUND_SWAMP4       = 605,
    GROUND_SWAMP5       = 4691,
    GROUND_SWAMP6       = 4758,
    GROUND_SWAMP7       = 4751, 
};
#endif //BD_DOWN

enum item_t {
    ITEM_SPELL_BOOK     = 2175, 
	ITEM_CHAMELEON      = 2291,
	ITEM_DUSTBIN        = 1777,
    ITEM_MAGIC_WALL     = 1497,
	ITEM_MAGIC_WALL1    = 1498,
	ITEM_WILDGROWTH     = 1499,
	ITEM_CLOSE_DOOR     = 1228,
    ITEM_CLOSE_DOOR1    = 1230,
    ITEM_DEPOTID1       = 2592,
    ITEM_DEPOTID        = 2589,
#ifdef BBK_MAGIC_DAMAGE
    ITEM_BEAR = 3954,
    ITEM_VODOODOLL = 3955,
    ITEM_BLUEROBE = 2656,
    ITEM_MYSTICTURBAN = 2663,
    ITEM_MAGICIANHAT = 2323,
    ITEM_BURNINGHEART = 2353,
    ITEM_GINSEN = 5015,
    ITEM_FROZENSTARLIGHT = 2361,
    ITEM_MUSICPIECE = 2349,
    ITEM_TEAROFDARAMAN = 2346,
    ITEM_BUNNYSLIPPERS = 2644,
#endif //BBK_MAGIC_DAMAGE
#ifdef ARNE_LUCK
    ITEM_ROTS = 2123,
    ITEM_CRYSTAL_RING = 2124,
    ITEM_CRYSTAL_AMULET = 2125,
#endif //ARNE_LUCK
    ITEM_CATALYST_ARROW	= 2222,
    ITEM_CATALYST_FIRE_ARROW = 2795,
    ITEM_CATALYST_POISONED_ARROW = 2796,
    ITEM_CATALYST_BOLT = 2108,
    ITEM_CATALYST_POWER_BOLT = 4838,
    ITEM_CRUSEDCOWL = 2664,
    ITEM_KRZYZYK        = 2327,
    ITEM_MAGICPLATEARMOR = 2472,
    ITEM_PANDEMONIUMARMOR = 3968,
    ITEM_SAPPHIRELEGS = 2504,
    ITEM_SAPPHIREARMOR = 2503,
    ITEM_SAPPHIREHELMET = 2502,
    ITEM_MAGICZNEKLAPKIZAPOMNIENIA = 2644,
	ITEM_FISHING_ROD	= 2580,
	ITEM_SHOVEL			= 2554,
	ITEM_ROPE			= 2120,
	ITEM_MACHETE		= 2420,
	ITEM_SCYTHE			= 2550,
	ITEM_COINS_GOLD		= 2148,
	ITEM_COINS_PLATINUM	= 2152,
	ITEM_COINS_CRYSTAL	= 2160,
	ITEM_COINS_SCARAB	= 2159,
	ITEM_DEPOT			= 2594,
	ITEM_RUNE_BLANK		= 2260,
	ITEM_MALE_CORPSE	= 3058,
	ITEM_FEMALE_CORPSE	= 3065,
    ITEM_DWARF_CORPSE	= 2960,
	ITEM_ELF_CORPSE 	= 2945,
	ITEM_MEAT			= 2666,
	ITEM_HAM			= 2671,
	ITEM_GRAPE			= 2681,
	ITEM_APPLE			= 2674,
	ITEM_BREAD			= 2689,
	ITEM_ROLL			= 2690,
	ITEM_CHEESE			= 2696,
	ITEM_ROPE_SPOT1		= 384,
	ITEM_ROPE_SPOT2		= 418,
	ITEM_HUMAN_CORPSE	= 3128,
	ITEM_ICE_RAPIER		= 2396,
	ITEM_AOL			= 2173,
	ITEM_AMULET_STARLIGHT = 2138,
	ITEM_BOH			= 2195,
	ITEM_WINGED			= 2474,
	ITEM_SPLASH			= 2019,
	ITEM_POOL			= 2025,
//	ITEM_BLUEROBE                   = 2656,
//	ITEM_MYSTICTURBAN               = 2663,
	ITEM_PANDA                      = 3968,
	ITEM_MLS              = 2390,
	ITEM_BOW			= 2456,
ITEM_MAGIC_MACE     = 2452,
	ITEM_SILV            = 2424,
	ITEM_CROSS_BOW			= 2455,
	ITEM_BURST_ARROW	= 2546,
	ITEM_GOLD_BOLT		= 2437,
	ITEM_SPEAR			= 2389,
	ITEM_TORCH_FULL		= 2051,
	ITEM_TORCH_MEDIUM	= 2053,
	ITEM_TORCH_LOW		= 2055,
	ITEM_STONE_SKIN_AMULET	= 2197,
	ITEM_BRONZE_AMULET		= 2126,
	ITEM_GARLIC_NECKLACE	= 2199,
	ITEM_PROTECTION_AMULET	= 2200,
	ITEM_DRAGON_NECKLACE	= 2201,
	ITEM_ELVEN_AMULET		= 2198,
	ITEM_SILVER_AMULET		= 2132,
	ITEM_STRANGE_TALISMAN	= 2161,
	ITEM_MIGHT_RING			= 2164,
	ITEM_SOFT_BOOTS = 2640,
	ITEM_LIFE_RING          = 2168, // LIFE RING
    ITEM_LIFE_RING_IN_USE   = 2205, // LIFE RING USED
    ITEM_RING_OF_HEALING    = 2214, // ROH
    ITEM_RING_OF_HEALING_IN_USE = 2216, // ROH USED
	ITEM_TIME_RING			= 2169,
	ITEM_TIME_RING_IN_USE	= 2206,
	ITEM_SWORD_RING			= 2207,
	ITEM_SWORD_RING_IN_USE	= 2210,
	ITEM_AXE_RING			= 2208,
	ITEM_AXE_RING_IN_USE	= 2211,
	ITEM_CLUB_RING			= 2209,
	ITEM_CLUB_RING_IN_USE	= 2212,
	ITEM_POWER_RING			= 2166,
	ITEM_POWER_RING_IN_USE	= 2203,
	ITEM_ENERGY_RING		= 2167,
	ITEM_ENERGY_RING_IN_USE	= 2204,
	ITEM_STEALTH_RING		= 2165,
	ITEM_STEALTH_RING_IN_USE	= 2202,
	ITEM_MAGIC_LIGHTWAND		= 2162,
	ITEM_MAGIC_LIGHTWAND_IN_USE	= 2163,
	ITEM_QUAGMIRE_ROD			= 2181,
	ITEM_SNAKEBITE_ROD			= 2182,
	ITEM_TEMPEST_ROD			= 2183,
	ITEM_VOLCANIC_ROD			= 2185,
	ITEM_MOONLIGHT_ROD			= 2186,
	ITEM_WAND_OF_INFERNO		= 2187,
	ITEM_WAND_OF_PLAGUE			= 2188,
	ITEM_WAND_OF_COSMIC_ENERGY	= 2189,
	ITEM_WAND_OF_VORTEX			= 2190,
	ITEM_WAND_OF_DRAGONBREATH	= 2191,
    ITEM_AKT                    = 1952,
#ifdef __Xaro_ANTY_LURE__
    ITEM_CHMURKA = 1505,
#endif //__Xaro_ANTY_LURE__
    ITEM_PARCEL                 = 2595,
    ITEM_STPARCEL               = 2596,
    ITEM_MAILBOX                = 2593,
    ITEM_MAILBOX2               = 2334,
    ITEM_MAILBOX3               = 3981,
    ITEM_LABEL                  = 2599,
    ITEM_LETTER                 = 2597,
    ITEM_STLETTER               = 2598,
    ITEM_DWARVEN_RING			= 2213,
    ITEM_DWARVEN_RING_IN_USE	= 2215,
	ITEM_SPRITE_WAND = 2453,
	ITEM_XBOW                       = 2455,
};

#ifdef TLM_SKULLS_PARTY
enum skull_t {
	SKULL_NONE = 0,
	SKULL_YELLOW = 1,
	SKULL_WHITE = 3,
	SKULL_RED = 4
};
#endif //TLM_SKULLS_PARTY
#ifdef CAYAN_POISONMELEE
enum Collors {
    COLLOR_BLACK         = 0,
    COLLOR_DARK_GREEN    = 12, 
    COLLOR_GREEN         = 19,
    COLLOR_LIGHT_GREEN   = 24, 
    COLLOR_DARK_BLUE     = 40,
    COLLOR_LIGHT_BLUE    = 71,
    COLLOR_GRAY          = 86,
    COLLOR_DARK_GRAY     = 87,
    COLLOR_LIME    	     = 102,
    COLLOR_LIGHT_BROWN   = 114,
    COLLOR_DARK_BRWON    = 121,
    COLLOR_ICE           = 179,
    COLLOR_RED           = 180,
    COLLOR_BURN          = 199,
    COLLOR_NONE          = 255,
    COLLOR_ORANGE        = 193,
    COLLOR_SAND          = 207,
    COLLOR_YELLOW        = 210,
    COLLOR_WHITE         = 983,
};
#endif //CAYAN_POISONMELEE
#ifdef TJ_MONSTER_BLOOD
enum bloodcolor_t // for dmg string
{
    COLOR_NONE = 255,
	COLOR_WHITE_EXP = 215,
	COLOR_WHITE = 208,
	COLOR_BLUE = 2,
	COLOR_RED = 180,
	COLOR_GREEN = 50,
};

enum bloodeffect_t //like draw_blood, or hit_area etc
{
    EFFECT_RED = NM_ME_DRAW_BLOOD,
    EFFECT_UNDEAD = NM_ME_HIT_AREA,
    EFFECT_POISON = NM_ME_POISEN,
    EFFECT_NONE = 255,
};

enum bloodsplash_t //for splashes (count)
{
    SPLASH_RED = FLUID_BLOOD,
	SPLASH_GREEN = FLUID_SLIME,
	//SPLASH_PINK = FLUID_PINK,
	SPLASH_NONE = 0,
}; 
#endif //TJ_MONSTER_BLOOD

#endif

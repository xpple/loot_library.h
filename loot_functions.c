#include "loot_functions.h"
#include "rng.h"

// ----------------------------------------------------------------------------------------
// the actual loot functions

static void set_count_uniform_function(uint64_t* rand, ItemStack* is, const int params[4])
{
	const int bound = params[1] - params[0] + 1;
	const int cnt = nextInt(rand, bound) + params[0];
	is->count = cnt;
}

static void set_count_constant_function(uint64_t* rand, ItemStack* is, const int params[4])
{
	is->count = params[0];
}

static void skip_n_calls_function(uint64_t* rand, ItemStack* is, const int params[4])
{
	skipNextN(rand, params[0]);
}

static void skip_one_call_function(uint64_t* rand, ItemStack* is, const int params[4])
{
	nextSeed(rand);
}

static void no_op_function(uint64_t* rand, ItemStack* is, const int params[4])
{
	// do nothing
}

// enchantments

static void set_enchantment_random_level(uint64_t* rand, ItemStack* is, const int params[4])
{
	// params[0] - enchantment id
	// params[1] - min level
	// params[2] - max level

	is->enchantment_count = 1;
	is->enchantments[0].enchantment = params[0];

	nextSeed(rand); // choose a "random" enchantment, nextInt(1) call
	const int bound = params[2] - params[1] + 1;
	is->enchantments[0].level = nextInt(rand, bound) + params[1];
}


// ----------------------------------------------------------------------------------------
// function creators

void create_enchant_with_levels(LootFunction* lf, const ItemType item, const int min_level, const int max_level, const int isTreasure)
{
	// TODO: implement
	lf->fun = no_op_function;
}

void create_enchant_randomly(LootFunction* lf, const ItemType item, const int isTreasure)
{
	// TODO: implement
	lf->fun = no_op_function;
}

void create_set_count(LootFunction* lf, const int min, const int max)
{
	lf->params[0] = min;
	lf->params[1] = max;

	if (min == max)
	{
		lf->fun = set_count_constant_function;
	}
	else
	{
		lf->fun = set_count_uniform_function;
	}
}

void create_set_damage(LootFunction* lf)
{
	// item damage is not that important, this should suffice
	create_skip_calls(lf, 1);
}

void create_skip_calls(LootFunction* lf, const int skip_count)
{
	lf->params[0] = skip_count;

	if (skip_count == 1)
	{
		lf->fun = skip_one_call_function;
	}
	else
	{
		lf->fun = skip_n_calls_function;
	}
}

void create_no_op(LootFunction* lf)
{
	lf->fun = no_op_function;
}

// ----------------------------------------------------------------------------------------
// Enchantment functions

static int is_applicable(const Enchantment enchantment, const ItemType item)
{
	if (enchantment == NO_ENCHANTMENT) return 0;
	if (item == BOOK) return 1; // the wildcard

	switch (enchantment)
	{
	case CURSE_OF_VANISHING:
	case UNBREAKING:
	case MENDING:
		return 1;

	case THORNS: // technically this isn't true for enchanting table, but no real point in supporting that
	case CURSE_OF_BINDING:
	case PROTECTION:
	case FIRE_PROTECTION:
	case BLAST_PROTECTION:
	case PROJECTILE_PROTECTION:
		return item == HELMET || item == CHESTPLATE || item == LEGGINGS || item == BOOTS;

	case RESPIRATION:
	case AQUA_AFFINITY:
		return item == HELMET;

	case FEATHER_FALLING:
	case DEPTH_STRIDER:
	case FROST_WALKER:
	case SOUL_SPEED:
		return item == BOOTS;

	case SWIFT_SNEAK:
		return item == LEGGINGS;

	case SHARPNESS:
	case SMITE:
	case BANE_OF_ARTHROPODS:
	case KNOCKBACK:
	case FIRE_ASPECT:
	case LOOTING:
	case SWEEPING_EDGE:
		return item == SWORD; // TODO add mace

	case EFFICIENCY:
	case SILK_TOUCH:
	case FORTUNE:
		return item == PICKAXE || item == SHOVEL || item == AXE || item == HOE;

	case POWER:
	case PUNCH:
	case FLAME:
	case INFINITY:
		return item == BOW;

	case MULTISHOT:
	case QUICK_CHARGE:
	case PIERCING:
		return item == CROSSBOW;

	case LUCK_OF_THE_SEA:
	case LURE:
		return item == FISHING_ROD;

	case IMPALING:
	case RIPTIDE:
	case LOYALTY:
	case CHANNELING:
		return item == TRIDENT;

	case DENSITY:
	case BREACH:
	case WIND_BURST:
		return item == MACE;
	}

	return 0;
}

static int get_max_level(const Enchantment enchantment)
{
	static const int MAX_LEVEL[64] = {
		0, // no_enchantment
		4, 4, 4, 4, 3, 1, 3, 3, 4, 3, 2, 3, // armor
		5, 5, 5, 2, 2, 3, 3, // swords
		5, 1, 3, // tools + unbreaking
		3, 3, // fishing rods
		5, 2, 1, 1, // bows
		3, 3, 4, // crossbows
		5, 3, 3, 1, // trident
		0, 0, 0, // mace
		1, 3, 1, 1 // general
	};

	return MAX_LEVEL[enchantment];
}

static int get_applicable_enchantments(const ItemType item, const MCVersion version, int enchantments[])
{
	static const int ORDER_V1_13[64] = { 
		PROTECTION, FIRE_PROTECTION, FEATHER_FALLING, BLAST_PROTECTION, PROJECTILE_PROTECTION,
		RESPIRATION, AQUA_AFFINITY, THORNS, DEPTH_STRIDER, FROST_WALKER, CURSE_OF_BINDING,
		SHARPNESS, SMITE, BANE_OF_ARTHROPODS, KNOCKBACK, FIRE_ASPECT, LOOTING, SWEEPING_EDGE,
		EFFICIENCY, SILK_TOUCH, UNBREAKING, FORTUNE, POWER, PUNCH, FLAME, INFINITY,
		LUCK_OF_THE_SEA, LURE, LOYALTY, IMPALING, RIPTIDE, CHANNELING, MENDING, CURSE_OF_VANISHING,
		NO_ENCHANTMENT // sentinel
	};
	static const int ORDER_V1_14[64] = {
		PROTECTION, FIRE_PROTECTION, FEATHER_FALLING, BLAST_PROTECTION, PROJECTILE_PROTECTION,
		RESPIRATION, AQUA_AFFINITY, THORNS, DEPTH_STRIDER, FROST_WALKER, CURSE_OF_BINDING,
		SHARPNESS, SMITE, BANE_OF_ARTHROPODS, KNOCKBACK, FIRE_ASPECT, LOOTING, SWEEPING_EDGE,
		EFFICIENCY, SILK_TOUCH, UNBREAKING, FORTUNE, POWER, PUNCH, FLAME, INFINITY,
		LUCK_OF_THE_SEA, LURE, LOYALTY, IMPALING, RIPTIDE, CHANNELING, 
		MULTISHOT, QUICK_CHARGE, PIERCING, MENDING, CURSE_OF_VANISHING,
		NO_ENCHANTMENT // sentinel
	};
	// 1.16 adds soul speed but it's not possible to get it from regular enchanting
	static const int ORDER_V1_21[64] = { 0 }; // TODO

	const int* order = ORDER_V1_13;
	if (version >= v1_14) order = ORDER_V1_14;
	if (version >= v1_21) order = ORDER_V1_21;

	int i = 0, j = 0;
	while (order[i] != NO_ENCHANTMENT)
	{
		if (is_applicable(order[i], item))
		{
			enchantments[j] = order[i];
			j++;
		}
		i++;
	}

	return j; // return the number of applicable enchantments
}

// ----------------------------------------------------------------------------------------

void create_enchant_randomly(LootFunction* lf, const ItemType item, const int isTreasure)
{
	lf->fun = set_enchantment_random_level;
	lf->params[0] = item;
	lf->params[1] = 1; // min level
	lf->params[2] = 5; // max level
}
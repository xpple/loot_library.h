#include "loot_functions.h"
#include "rng.h"

#include <string.h>
#include <math.h>

// ----------------------------------------------------------------------------------------
// the actual loot functions

static void set_count_uniform_function(uint64_t* rand, ItemStack* is, const void* params)
{
	const int* params_int = (const int*)params;
	const int bound = params_int[1] - params_int[0] + 1;
	const int cnt = nextInt(rand, bound) + params_int[0];
	is->count = cnt;
}

static void set_count_constant_function(uint64_t* rand, ItemStack* is, const void* params)
{
	const int* params_int = (const int*)params;
	is->count = params_int[0];
}

static void skip_n_calls_function(uint64_t* rand, ItemStack* is, const void* params)
{
	const int* params_int = (const int*)params;
	skipNextN(rand, params_int[0]);
}

static void skip_one_call_function(uint64_t* rand, ItemStack* is, const void* params)
{
	nextSeed(rand);
}

static void no_op_function(uint64_t* rand, ItemStack* is, const void* params)
{
	// do nothing
}

// enchantments

static void set_enchantment_random_level_function(uint64_t* rand, ItemStack* is, const void* params)
{
	const int* params_int = (const int*)params;
	// params[0] - enchantment id
	// params[1] - max level (min level always 1)

	is->enchantment_count = 1;
	is->enchantments[0].enchantment = params_int[0];

	nextSeed(rand); // choose a "random" enchantment, nextInt(1) call
	const int bound = params_int[1];
	is->enchantments[0].level = nextInt(rand, bound) + 1;
}

static void enchant_randomly_function(uint64_t* rand, ItemStack* is, const void* params)
{
	const int* varparams_int = (const int*)params;
	// params[0] - number of enchantments
	// params[2k + 1] - enchantment id
	// params[2k + 2] - nextInt bound for the enchantment level choice

	int numEnchants = varparams_int[0];

	is->enchantment_count = 1;

	int enchantmentID = nextInt(rand, numEnchants);
	is->enchantments[0].enchantment = varparams_int[2 * enchantmentID + 1];

	int enchantmentLevel = nextInt(rand, varparams_int[2 * enchantmentID + 2]) + 1;
	is->enchantments[0].level = enchantmentLevel;
}

static inline int java_round_positive(float f)
{
	// this should be good enough to emulate Math.round
	return (int)floor(f + 0.5F);
}

static void enchant_with_levels_function(uint64_t* rand, ItemStack* is, const void* params)
{
	// from mc_feature_java:
	/*
	ArrayList<EnchantmentInstance> res = new ArrayList<>();
	Item item = itemStack.getItem();
	int enchantmentValue;
	if(enchantments.containsKey(item.getName())) {
		enchantmentValue = enchantments.get(item.getName());
	} else {
		return res;
	}
	level += 1 + lootContext.nextInt(enchantmentValue / 4 + 1) + lootContext.nextInt(enchantmentValue / 4 + 1);
	float amplifier = (lootContext.nextFloat() + lootContext.nextFloat() - 1.0f) * 0.15f;
	level = Mth.clamp(Math.round((float)level + (float)level * amplifier), 1, Integer.MAX_VALUE);
	ArrayList<EnchantmentInstance> availableEnchantments = getAvailableEnchantmentResults(level);
	if(!availableEnchantments.isEmpty()) {
		res.add(EnchantmentInstance.getRandomItem(lootContext, availableEnchantments));
		while(lootContext.nextInt(50) <= level) {
			Enchantments.filterCompatibleEnchantments(availableEnchantments, res.get(res.size() - 1));
			if(availableEnchantments.isEmpty()) break;
			res.add(EnchantmentInstance.getRandomItem(lootContext, availableEnchantments));
			level /= 2;
		}
	}
	return res;
	*/

	const int** varparams_int_arr = (const int**)params;
	// params[0][0] - item enchantability
	// params[0][1] - min levels
	// params[0][2] - max levels
	// 
	// params[i][0]	- number of applicable enchantment instances
	// params[i][2k + 1] - enchantment id
	// params[i][2k + 2] - enchantment level

	const int enchantability = varparams_int_arr[0][0];
	const int minLevel = varparams_int_arr[0][1];
	const int maxLevel = varparams_int_arr[0][2];

	// calculate effective level
	int level = minLevel + nextInt(rand, maxLevel - minLevel + 1);
	const int delta = enchantability / 4 + 1;
	level += 1 + nextInt(rand, delta) + nextInt(rand, delta);
	const float amplifier = (nextFloat(rand) + nextFloat(rand) - 1.0F) * 0.15F;
	level = java_round_positive((float)level + (float)level * amplifier);

	const int* applicableEnchantments = varparams_int_arr[level];
}

// ----------------------------------------------------------------------------------------
// function creators


void create_set_count(LootFunction* lf, const int min, const int max)
{
	lf->params = (int*)malloc(2 * sizeof(int));

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
	lf->params = (int*)malloc(sizeof(int));

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
	lf->params = NULL;
	lf->fun = no_op_function;
}

// ----------------------------------------------------------------------------------------
//  Enchantment functions

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
			if (enchantments != NULL)
				enchantments[j] = order[i];
			j++;
		}
		i++;
	}

	return j; // return the number of applicable enchantments
}

//  Enchantment function creators

void create_enchant_randomly_one_echant(LootFunction* lf, const Enchantment enchantment)
{
	lf->params = (int*)malloc(2 * sizeof(int));

	lf->params[0] = enchantment;
	lf->params[1] = get_max_level(enchantment);

	lf->fun = set_enchantment_random_level_function;
}

void create_enchant_randomly(LootFunction* lf, const MCVersion version, const ItemType item, const int isTreasure)
{
	int enchantCount = get_applicable_enchantments(item, version, NULL);

	lf->params = (int*)malloc((2 * enchantCount + 1) * sizeof(int));

	lf->params[0] = enchantCount;
	get_applicable_enchantments(item, version, lf->params + 1);

	lf->fun = enchant_randomly_function;
}

// TODO: implement
// void create_enchant_with_levels(LootFunction* lf, const MCVersion version, const ItemType item, const int min_level, const int max_level, const int isTreasure)
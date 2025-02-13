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

// enchant with levels helpers

static inline int java_round_positive(float f)
{
	// this should be good enough to emulate Math.round
	return (int)floor(f + 0.5F);
}

static inline int choose_enchantment(uint64_t* rand, int enchantmentVec[], const int vecSize, const int totalWeight)
{
	int w = nextInt(rand, totalWeight);
	for (int i = 2; i < vecSize; i += 3)
	{
		w -= enchantmentVec[i];
		if (w < 0) 
			return i - 2;
	}
	return vecSize - 3;
}

static int IS_INCOMPATIBLE_ENCHANT[64][64];
static void fill_incompatible_enchantments()
{
	for (int i = 0; i < 64; i++)
	{
		for (int j = 0; j < 64; j++)
		{
			IS_INCOMPATIBLE_ENCHANT[i][j] = i == j;
		}
	}

	int protections[] = { PROTECTION, FIRE_PROTECTION, BLAST_PROTECTION, PROJECTILE_PROTECTION };
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			IS_INCOMPATIBLE_ENCHANT[protections[i]][protections[j]] = 1;

	int sharpnesses[] = { SHARPNESS, SMITE, BANE_OF_ARTHROPODS };
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			IS_INCOMPATIBLE_ENCHANT[sharpnesses[i]][sharpnesses[j]] = 1;

	int fortunes[] = { FORTUNE, LUCK_OF_THE_SEA, LOOTING };
	for (int i = 0; i < 3; i++)
	{
		IS_INCOMPATIBLE_ENCHANT[fortunes[i]][SILK_TOUCH] = 1;
		IS_INCOMPATIBLE_ENCHANT[SILK_TOUCH][fortunes[i]] = 1;
	}

	IS_INCOMPATIBLE_ENCHANT[DEPTH_STRIDER][FROST_WALKER] = 1;
	IS_INCOMPATIBLE_ENCHANT[FROST_WALKER][DEPTH_STRIDER] = 1;

	IS_INCOMPATIBLE_ENCHANT[MENDING][INFINITY_ENCHANTMENT] = 1;
	IS_INCOMPATIBLE_ENCHANT[INFINITY_ENCHANTMENT][MENDING] = 1;

	IS_INCOMPATIBLE_ENCHANT[RIPTIDE][CHANNELING] = 1;
	IS_INCOMPATIBLE_ENCHANT[CHANNELING][RIPTIDE] = 1;
	IS_INCOMPATIBLE_ENCHANT[RIPTIDE][LOYALTY] = 1;
	IS_INCOMPATIBLE_ENCHANT[LOYALTY][RIPTIDE] = 1;

	IS_INCOMPATIBLE_ENCHANT[PIERCING][MULTISHOT] = 1;
	IS_INCOMPATIBLE_ENCHANT[MULTISHOT][PIERCING] = 1;

	IS_INCOMPATIBLE_ENCHANT[DENSITY][BREACH] = 1;
	IS_INCOMPATIBLE_ENCHANT[BREACH][DENSITY] = 1;
}

static inline void remove_incompatible_enchantments(int enchantmentIndex, int enchantVec[], int* vecSize, int* totalWeight)
{
	int enchantmentID = enchantVec[enchantmentIndex];

	int i = 0;
	int moveBack = 0;

	while (i < *vecSize)
	{
		const int ix = 3 * i;
		if (moveBack > 0)
			memcpy(enchantVec + ix - moveBack, enchantVec + ix, sizeof(int) * 3);

		if (IS_INCOMPATIBLE_ENCHANT[enchantmentID][enchantVec[i]])
		{
			*totalWeight -= enchantVec[ix + 2]; // subtract the weight of removed enchantment
			moveBack += 3; // make it so that all the following elements are moved back
		}
		
		i++;
	}

	*vecSize -= (moveBack / 3); // shrink the vector
}

static void enchant_with_levels_function(uint64_t* rand, ItemStack* is, const void* params)
{
	const int** varparams_int_arr = (const int**)params;
	// params[0][0] - item enchantability
	// params[0][1] - min levels
	// params[0][2] - max levels
	// 
	// params[i][0]	- number of applicable enchantment instances
	// params[i][1] - total weight of the enchantment instances
	// params[i][3k + 2] - enchantment id
	// params[i][3k + 3] - enchantment level
	// params[i][3k + 4] - enchantment weight

	const int enchantability = varparams_int_arr[0][0];
	const int minLevel = varparams_int_arr[0][1];
	const int maxLevel = varparams_int_arr[0][2];

	// calculate effective level
	int level = minLevel + nextInt(rand, maxLevel - minLevel + 1);
	const int delta = enchantability / 4 + 1;
	level += 1 + nextInt(rand, delta) + nextInt(rand, delta);
	const float amplifier = (nextFloat(rand) + nextFloat(rand) - 1.0F) * 0.15F;
	level = java_round_positive((float)level + (float)level * amplifier);

	// copy the available enchantment results to a local array
	is->enchantment_count = 0;
	int vecSize = varparams_int_arr[level][0];
	int totalWeight = varparams_int_arr[level][1];
	if (vecSize == 0) return; // no enchantments available
	
	int enchantmentVec[128]; // holds triples (id, level, weight), max 42 * 3 = 126 elements
	memcpy(enchantmentVec, varparams_int_arr[level] + 2, sizeof(int) * vecSize * 3);

	int index = choose_enchantment(rand, enchantmentVec, vecSize, totalWeight);
	is->enchantments[0].enchantment = enchantmentVec[index];
	is->enchantments[0].level = enchantmentVec[index + 1];
	is->enchantment_count++;

	while (nextInt(rand, 50) <= level)
	{
		remove_incompatible_enchantments(index, enchantmentVec, &vecSize, &totalWeight); // TODO implement
		if (vecSize == 0) break;

		index = choose_enchantment(rand, enchantmentVec, vecSize, totalWeight);
		is->enchantments[is->enchantment_count].enchantment = enchantmentVec[index];
		is->enchantments[is->enchantment_count].level = enchantmentVec[index + 1];
		is->enchantment_count++;

		level /= 2;
	}
}

// ----------------------------------------------------------------------------------------
// function creators

void init_function(LootFunction* lf)
{
	lf->params = NULL;
	lf->varparams_int = NULL;
	lf->varparams_int_arr = NULL;
	lf->varparams_int_arr_size = 0;
}

void create_set_count(LootFunction* lf, const int min, const int max)
{
	init_function(lf);
	lf->params = lf->params_int;
	lf->params_int[0] = min;
	lf->params_int[1] = max;

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
	init_function(lf);
	lf->params = lf->params_int;
	lf->params_int[0] = skip_count;

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
	init_function(lf);
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
	case INFINITY_ENCHANTMENT:
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

static int test_effective_level(const Enchantment enchantment, const int ench_level, const int eff_level)
{
	// TODO
	return 0;
}

static int get_weight(const Enchantment enchantment)
{
	// TODO
	return 1;
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
		EFFICIENCY, SILK_TOUCH, UNBREAKING, FORTUNE, POWER, PUNCH, FLAME, INFINITY_ENCHANTMENT,
		LUCK_OF_THE_SEA, LURE, LOYALTY, IMPALING, RIPTIDE, CHANNELING, MENDING, CURSE_OF_VANISHING,
		NO_ENCHANTMENT // sentinel
	};
	static const int ORDER_V1_14[64] = {
		PROTECTION, FIRE_PROTECTION, FEATHER_FALLING, BLAST_PROTECTION, PROJECTILE_PROTECTION,
		RESPIRATION, AQUA_AFFINITY, THORNS, DEPTH_STRIDER, FROST_WALKER, CURSE_OF_BINDING,
		SHARPNESS, SMITE, BANE_OF_ARTHROPODS, KNOCKBACK, FIRE_ASPECT, LOOTING, SWEEPING_EDGE,
		EFFICIENCY, SILK_TOUCH, UNBREAKING, FORTUNE, POWER, PUNCH, FLAME, INFINITY_ENCHANTMENT,
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

static int get_enchant_level_vector(const int level, const int applicable[], const int num_applicable, int* vec)
{
	int vecSize = 0;
	int totalWeight = 0;

	for (int i = 0; i < num_applicable; i++)
	{
		const int enchantment = applicable[i];
		const int max_level = get_max_level(enchantment);

		for (int ench_level = max_level; ench_level >= 1; ench_level--)
		{
			if (test_effective_level(enchantment, ench_level, level) == 0)
				continue;

			if (vec != NULL)
			{
				vec[3 * vecSize + 2] = enchantment;
				vec[3 * vecSize + 3] = ench_level;
				vec[3 * vecSize + 4] = get_weight(enchantment);
			}

			vecSize++;
			break;
		}
	}

	if (vec == NULL)
		return vecSize;

	vec[0] = vecSize;
	vec[1] = totalWeight;
	return vecSize;
}

//  Enchantment function creators

void create_enchant_randomly_one_echant(LootFunction* lf, const Enchantment enchantment)
{
	init_function(lf);
	lf->params = lf->params_int;
	lf->params_int[0] = enchantment;
	lf->params_int[1] = get_max_level(enchantment);

	lf->fun = set_enchantment_random_level_function;
}

void create_enchant_randomly(LootFunction* lf, const MCVersion version, const ItemType item, const int isTreasure)
{
	int enchantCount = get_applicable_enchantments(item, version, NULL);

	init_function(lf);
	lf->varparams_int = (int*)malloc((2 * enchantCount + 1) * sizeof(int));
	lf->params = lf->varparams_int;

	lf->varparams_int[0] = enchantCount;
	get_applicable_enchantments(item, version, lf->varparams_int + 1);

	int applicable[64];
	get_applicable_enchantments(item, version, applicable);

	// copy applicable enchants, along with their max levels
	for (int i = 0; i < enchantCount; i++)
	{
		lf->varparams_int[1 + 2*i] = applicable[i];
		lf->varparams_int[1 + 2*i + 1] = get_max_level(applicable[i]);
	}

	lf->fun = enchant_randomly_function;
}

void create_enchant_with_levels(LootFunction* lf, const MCVersion version, const ItemType item, const int min_level, const int max_level, const int isTreasure)
{
	// need 2*maxLevel vectors for enchantment instances
	// and a single vector for the initial parameters
	
	init_function(lf);
	lf->varparams_int_arr = (int**)malloc((2 * max_level + 1) * sizeof(int*));
	lf->params = (void*)lf->varparams_int_arr;
	lf->varparams_int_arr_size = 2 * max_level + 1;

	// basic data vector
	lf->varparams_int_arr[0] = (int*)malloc(3 * sizeof(int));
	lf->varparams_int_arr[0][0] = get_enchantability(item); // todo
	lf->varparams_int_arr[0][1] = min_level;
	lf->varparams_int_arr[0][2] = max_level;

	int applicable[64];
	int num_applicable = get_applicable_enchantments(item, version, applicable);
	
	// fill the enchantment instance vector array
	for (int level = 0; level < max_level; level++)
	{
		// create a vector for the current level
		int vector_size = get_enchant_level_vector(level, applicable, num_applicable, NULL);
		int* vec = malloc((3 * vector_size + 2) * sizeof(int));
		get_enchant_level_vector(level, applicable, num_applicable, vec); // todo
		lf->varparams_int_arr[level+1] = vec;
	}

	lf->fun = enchant_with_levels_function;
}

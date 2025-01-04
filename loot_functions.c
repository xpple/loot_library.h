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

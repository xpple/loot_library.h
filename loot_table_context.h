#pragma once
#include <stdint.h>

#include "rng.h"


typedef enum MCVersion MCVersion;
enum MCVersion {
	undefined,
	v1_16 // placeholder
};

typedef struct EnchantInstance EnchantInstance;
struct EnchantInstance {
	int enchantment;
	int level;
};

typedef struct ItemStack ItemStack;
struct ItemStack {
	int item;
	int count;

	int enchantment_count;
	EnchantInstance enchantments[16]; // 12 is the theoretical maximum for 1.17 and below, 16 should be safe for all versions
};

// ---------------------------------------------------------

typedef struct LootPool LootPool;
struct LootPool {
	// roll count choice function
	int min_rolls;
	int max_rolls;
	int (*roll_count_function)(uint64_t* rand, const int min, const int max);

	// precomputed loot array, which maps possible rng ouputs
	// to indices of loot entries they correspond to
	int total_weight;
	int* precomputed_loot;

	// loot entry arrays
	int* entry_to_item;				// entryID -> context itemID
	int* entry_functions_index;		// entryID -> ID of first loot function in functions array
	int* entry_functions_count;		// entryID -> number of functions applied to the item

	// loot function array, containing all the extra processors that get applied to the item
	void (**loot_function_array)(uint64_t* rand, ItemStack* itemStack);
};

typedef struct LootTableContext LootTableContext;
struct LootTableContext {
	// "constants" defining the loot table
	MCVersion version;
	int item_count;			// how many different items can this loot table generate
	char** item_names;		// names of the different items

	int pool_count;
	LootPool* loot_pools;

	// holding data related with generating the loot table within the context is a debatable idea
	uint64_t prng_state;
	int generated_item_count;
	ItemStack generated_items[27]; 
};

// ----------------------------------------------------------------------------------------
// Roll count choice functions

inline int roll_count_constant(uint64_t* rand, const int min, const int max)
{
	return min;
}

inline int roll_count_uniform(uint64_t* rand, const int min, const int max)
{
	const int bound = max - min + 1;
	return nextInt(rand, bound) + min;
}

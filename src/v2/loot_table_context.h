#ifndef _LOOT_TABLE_CONTEXT_H
#define _LOOT_TABLE_CONTEXT_H

#include <stdint.h>

#include "rng.h"
#include "loot_functions.h"


// ---------------------------------------------------------

typedef struct LootPool LootPool;
struct LootPool {
	// roll count choice function
	int min_rolls;
	int max_rolls;
	RollCountFunction roll_count_function;

	// precomputed loot array, which maps possible rng ouputs
	// to indices of loot entries they correspond to
	int total_weight;
	int* precomputed_loot;

	// loot entry arrays
	int entry_count;
	int* entry_to_item;				// entryID -> context itemID
	int* entry_functions_index;		// entryID -> ID of first loot function in functions array
	int* entry_functions_count;		// entryID -> number of functions applied to the item

	// loot function array, containing all the extra processors that get applied to the item
	LootFunction* loot_functions;
};

typedef struct LootTableContext LootTableContext;
struct LootTableContext {
	// "constants" defining the loot table
	MCVersion version;
	int item_count;			// how many different items can this loot table generate
	char** item_names;		// names of the different items

	int pool_count;
	LootPool* loot_pools;

        // TODO sub-table data
        //int subtable_count; // how many subtables the loot table has
        //int* subtable_pool_offset; // subtable index to index of first loot pool
        //int* subtable_pool_count; // subtable index to how many pools it has

	// holding data related with generating the loot table within the context is a debatable idea
	uint64_t prng_state;
	int generated_item_count;
	ItemStack generated_items[27]; 
};


// ---------------------------------------------------------
// loot generation functions

void set_loot_seed(LootTableContext* context, uint64_t seed);
void set_internal_loot_seed(LootTableContext* context, uint64_t internal_seed);

int get_item_id(LootTableContext* context, const char* item_name);
const char* get_item_name(LootTableContext* context, int item_id);

void generate_loot(LootTableContext* context);

#endif // _LOOT_TABLE_CONTEXT_H

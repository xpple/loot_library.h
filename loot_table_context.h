#pragma once

// Loose draft of the structure of the generalized lot table context

#include <stdint.h>

typedef enum MCVersion MCVersion;
enum MCVersion {
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
	int holder;
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
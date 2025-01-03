#pragma once

// ----------------------------------------------------------------------------------------

typedef enum MCVersion MCVersion;
enum MCVersion {
	undefined,
	v1_16 // placeholder
};

typedef enum ItemType ItemType;
enum ItemType {
	SWORD,
	PICKAXE,
	SHOVEL,
	AXE,
	HOE,
	HELMET,
	CHESTPLATE,
	LEGGINGS,
	BOOTS,
	BOW,
	CROSSBOW,
	TRIDENT,
	BOOK
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

// ----------------------------------------------------------------------------------------

typedef int (*RollCountFunction)(uint64_t*, const int, const int);

typedef struct LootFunction LootFunction;
struct LootFunction {
	// actual function pointer
	void (*fun)(uint64_t* rand, ItemStack* is, int params[4]);
	// predefined function parameter array
	int params[4];
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

// ----------------------------------------------------------------------------------------
// Loot function initializers

// TODO add everything:
// item count rolls
// durability rolls
// random enchant
// enchant with levels

void create_enchant_with_levels(LootFunction* lf, const ItemType item, const int min_level, const int max_level, const int isTreasure);
void create_enchant_randomly(LootFunction* lf, const ItemType item, const int isTreasure);
void create_set_count(LootFunction* lf, const int min, const int max);
void create_set_damage(LootFunction* lf);

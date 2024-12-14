#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

static inline void set_seed(uint64_t *seed, uint64_t value)
{
    *seed = (value ^ 0x5deece66d) & ((1ULL << 48) - 1);
}

static inline int next(uint64_t *seed, const int bits)
{
    *seed = (*seed * 0x5deece66d + 0xb) & ((1ULL << 48) - 1);
    return (int) ((int64_t)*seed >> (48 - bits));
}

static inline int next_int(uint64_t *seed, const int n)
{
    int bits, val;
    const int m = n - 1;

    if ((m & n) == 0) {
        uint64_t x = n * (uint64_t)next(seed, 31);
        return (int) ((int64_t) x >> 31);
    }

    do {
        bits = next(seed, 31);
        val = bits % n;
    }
    while (bits - val + m < 0);
    return val;
}

static inline uint64_t next_long(uint64_t *seed)
{
    return ((uint64_t) next(seed, 32) << 32) + next(seed, 32);
}

static inline float next_float(uint64_t *seed)
{
    return next(seed, 24) / (float) (1 << 24);
}

static inline double next_double(uint64_t *seed)
{
    uint64_t x = (uint64_t)next(seed, 26);
    x <<= 27;
    x += next(seed, 27);
    return (int64_t) x / (double) (1ULL << 53);
}

static inline int next_int_bounded(uint64_t *seed, int min, int max) {
    if (min >= max) {
        return min;
    }
    return next_int(seed, max - min + 1) + min;
}

typedef enum {
    ENCHANT_NONE, 
    SHARPNESS, SMITE, BANE_OF_ARTHROPODS, KNOCKBACK, FIRE_ASPECT, LOOTING, 
    SWEEPING, UNBREAKING, MENDING, VANISHING_CURSE, EFFICIENCY, SILK_TOUCH, FORTUNE,
    PROTECTION, FIRE_PROTECTION, FEATHER_FALLING, BLAST_PROTECTION, PROJECTILE_PROTECTION,
    THORNS, DEPTH_STRIDER, FROST_WALKER, BINDING_CURSE, RESPIRATION, AQUA_AFFINITY
} Enchant;

static char *enchant_names[] = {
    "none", "sharpness", "smite", "bane_of_arthropods", "knockback", "fire_aspect",
    "looting", "sweeping", "unbreaking", "mending", "vanishing", "efficiency", "silk_touch",
    "fortune", "protection", "fire_protection", "feather_falling", "blast_protection",
    "projectile_protection", "thorns", "depth_strider", "frost_walker", "binding", "respiration", "aqua_affinity"
};

static int standard_sword_enchantments[][3] = {
    {SHARPNESS, 1, 5}, {SMITE, 1, 5}, {BANE_OF_ARTHROPODS, 1, 5}, {KNOCKBACK, 1, 2}, {FIRE_ASPECT, 1, 2}, {LOOTING, 1, 3}, {SWEEPING, 1, 3}, {UNBREAKING, 1, 3}, {MENDING, 1, 1}, {VANISHING_CURSE, 1, 1}
};

static int standard_axe_enchantments[][3] = {
    {SHARPNESS, 1, 5}, {SMITE, 1, 5}, {BANE_OF_ARTHROPODS, 1, 5}, {EFFICIENCY, 1, 5}, {SILK_TOUCH, 1, 1}, {UNBREAKING, 1, 3}, {FORTUNE, 1, 3}, {MENDING, 1, 1}, {VANISHING_CURSE, 1, 1}
};

static int standard_pickaxe_enchantments[][3] = {
    {EFFICIENCY, 1, 5}, {SILK_TOUCH, 1, 1}, {UNBREAKING, 1, 3}, {FORTUNE, 1, 3}, {MENDING, 1, 1}, {VANISHING_CURSE, 1, 1}
};

static int standard_boot_enchantments[][3] = {
    {PROTECTION, 1, 4}, {FIRE_PROTECTION, 1, 4}, {FEATHER_FALLING, 1, 4}, {BLAST_PROTECTION, 1, 4}, {PROJECTILE_PROTECTION, 1, 4}, {THORNS, 1, 3}, {DEPTH_STRIDER, 1, 3}, {FROST_WALKER, 1, 2}, {BINDING_CURSE, 1, 1}, {UNBREAKING, 1, 3}, {MENDING, 1, 1}, {VANISHING_CURSE, 1, 1}
};

static int standard_leggings_enchantments[][3] = {
    {PROTECTION, 1, 4}, {FIRE_PROTECTION, 1, 4}, {BLAST_PROTECTION, 1, 4}, {PROJECTILE_PROTECTION, 1, 4}, {THORNS, 1, 3}, {BINDING_CURSE, 1, 1}, {UNBREAKING, 1, 3}, {VANISHING_CURSE, 1, 1}, {MENDING, 1, 1}
};

static int standard_chestplate_enchantments[][3] = {
    {PROTECTION, 1, 4}, {FIRE_PROTECTION, 1, 4}, {BLAST_PROTECTION, 1, 4}, {PROJECTILE_PROTECTION, 1, 4}, {THORNS, 1, 3}, {BINDING_CURSE, 1, 1}, {UNBREAKING, 1, 3}, {MENDING, 1, 1}, {VANISHING_CURSE, 1, 1}
};

static int standard_helmet_enchantments[][3] = {
    {PROTECTION, 1, 4}, {FIRE_PROTECTION, 1, 4}, {BLAST_PROTECTION, 1, 4}, {PROJECTILE_PROTECTION, 1, 4}, {RESPIRATION, 1, 3}, {AQUA_AFFINITY, 1, 1}, {THORNS, 1, 3}, {BINDING_CURSE, 1, 1}, {UNBREAKING, 1, 3}, {MENDING, 1, 1}, {VANISHING_CURSE, 1, 1}
}; 

static int standard_hoe_enchantments[][3] = {
    {EFFICIENCY, 1, 5}, {SILK_TOUCH, 1, 1}, {UNBREAKING, 1, 3}, {FORTUNE, 1, 3}, {MENDING, 1, 1}, {VANISHING_CURSE, 1, 1}
};

static int standard_shovel_enchantments[][3] = {
    {EFFICIENCY, 1, 5}, {SILK_TOUCH, 1, 1}, {UNBREAKING, 1, 3}, {FORTUNE, 1, 3}, {MENDING, 1, 1}, {VANISHING_CURSE, 1, 1}
};

typedef enum {
    NO_ENCHANTS, SWORD_ENCHANTS, AXE_ENCHANTS, PICKAXE_ENCHANTS, HOE_ENCHANTS, BOOTS_ENCHANTS, LEGGINGS_ENCHANTS, CHESTPLATE_ENCHANTS, HELMET_ENCHANTS, SHOVEL_ENCHANTS
} ItemEnchants;

typedef enum {
    ITEM_NONE,
    OBSIDIAN, FLINT, IRON_NUGGET, FLINT_AND_STEEL, FIRE_CHARGE, GOLDEN_APPLE, GOLD_NUGGET, GOLDEN_SWORD, GOLDEN_AXE, GOLDEN_HOE, GOLDEN_SHOVEL, GOLDEN_PICKAXE, GOLDEN_BOOTS, GOLDEN_CHESTPLATE, GOLDEN_HELMET, GOLDEN_LEGGINGS, GLISTERING_MELON_SLICE, GOLDEN_HORSE_ARMOR, LIGHT_WEIGHTED_PRESSURE_PLATE, GOLDEN_CARROT, CLOCK, GOLD_INGOT, BELL, ENCHANTED_GOLDEN_APPLE, GOLD_BLOCK      
} Item;

static char *item_names[] = {
    "none", "obsidian", "flint", "iron_nugget", "flint_and_steel", "fire_charge", "golden_apple", "gold_nugget", "golden_sword", "golden_axe", "golden_hoe", "golden_shovel", "golden_pickaxe", "golden_boots", "golden_chestplate", "golden_helmet", "golden_leggings", "glistering_melon_slice", "golden_horse_armor", "light_weighted_pressure_plate", "golden_carrot", "clock", "gold_ingot", "bell", "enchanted_golden_apple", "gold_block"
};

static int ruined_portal_loot_table_counts[25][2] = {
    {1, 2}, {1, 4}, {9, 18}, 
    {-1, -1}, {-1, -1}, {-1, -1},
    {4, 24},
    {-1, -1}, {-1, -1}, {-1, -1},
    {-1, -1}, {-1, -1}, {-1, -1},
    {-1, -1}, {-1, -1}, {-1, -1},
    {4, 12},
    {-1, -1}, {-1, -1},
    {4, 12},
    {-1, -1},
    {2, 8},
    {-1, -1}, {-1, -1},
    {1, 2}
};

static Item ruined_portal_loot_table[25] = {
    OBSIDIAN, FLINT, IRON_NUGGET, 
    FLINT_AND_STEEL, FIRE_CHARGE, GOLDEN_APPLE, 
    GOLD_NUGGET, GOLDEN_SWORD, GOLDEN_AXE, 
    GOLDEN_HOE, GOLDEN_SHOVEL, GOLDEN_PICKAXE, 
    GOLDEN_BOOTS, GOLDEN_CHESTPLATE, GOLDEN_HELMET, 
    GOLDEN_LEGGINGS, GLISTERING_MELON_SLICE, GOLDEN_HORSE_ARMOR, 
    LIGHT_WEIGHTED_PRESSURE_PLATE, GOLDEN_CARROT, CLOCK, 
    GOLD_INGOT, BELL, ENCHANTED_GOLDEN_APPLE, 
    GOLD_BLOCK,      
};

static ItemEnchants ruined_portal_enchant_table[25] = {
    NO_ENCHANTS, NO_ENCHANTS, NO_ENCHANTS,
    NO_ENCHANTS, NO_ENCHANTS, NO_ENCHANTS,
    NO_ENCHANTS, SWORD_ENCHANTS, AXE_ENCHANTS,
    HOE_ENCHANTS, SHOVEL_ENCHANTS, PICKAXE_ENCHANTS, 
    BOOTS_ENCHANTS, CHESTPLATE_ENCHANTS, HELMET_ENCHANTS,
    LEGGINGS_ENCHANTS, NO_ENCHANTS, NO_ENCHANTS, 
    NO_ENCHANTS, NO_ENCHANTS, NO_ENCHANTS,
    NO_ENCHANTS, NO_ENCHANTS, NO_ENCHANTS,
    NO_ENCHANTS
};

typedef struct {
    int table_counts[25][2];
    ItemEnchants enchant_table[25];
    Item table[25];
} LootTable;

static LootTable ruined_portal_table;

void init_loot_tables() {
    //ruined portal
    memcpy(ruined_portal_table.table, ruined_portal_loot_table, sizeof(ruined_portal_loot_table));
    memcpy(ruined_portal_table.table_counts, ruined_portal_loot_table_counts, sizeof(ruined_portal_loot_table_counts));
    memcpy(ruined_portal_table.enchant_table, ruined_portal_enchant_table, sizeof(ruined_portal_enchant_table));
}

typedef struct {
    Item item; 
    int quantity;

    bool enchanted;
    Enchant enchant;
    int enchant_level;
} LootItem;

LootItem enchanted_loot_item_new(Item item, int quantity, Enchant enchant, int enchant_level) {
    return (LootItem) {
        .item = item,
        .quantity = quantity,
        .enchanted = true, 
        .enchant = enchant,
        .enchant_level = enchant_level
    };
}

LootItem loot_item_new(Item item, int quantity) {
    return (LootItem) {
        .item = item,
        .quantity = quantity,
        .enchanted = false, 
        .enchant = ENCHANT_NONE,
        .enchant_level = -1,
    };
}
 
LootItem provide_loot_no_function(LootTable *table, int item_index, uint64_t *internal) {
    (void)internal;
    return loot_item_new(table->table[item_index], 1);
}   

LootItem provide_loot_uniform_roll(LootTable *table, int item_index, uint64_t *internal) {
    int min = table->table_counts[item_index][0];
    int max = table->table_counts[item_index][1];
    int amount = next_int_bounded(internal, min, max);
    return loot_item_new(table->table[item_index], amount);
}

LootItem enchant(LootTable *table, int item_index, uint64_t *internal, int enchant_table[][3], size_t num_enchants) {
    int enchant_num = next_int(internal, num_enchants);
    int *enchant = enchant_table[enchant_num]; 
    
    int level = 1;
    if (enchant[1] != enchant[2]) {
        level = next_int(internal, enchant[2]) + 1; 
    }

    return enchanted_loot_item_new(table->table[item_index], 1, enchant[0], level); 
}

#define LENGTH(x) sizeof(x) / sizeof(x[0])

LootItem provide_loot_random_enchant(LootTable *table, int item_index, uint64_t *internal) {
    ItemEnchants enchants = table->enchant_table[item_index];
    
    switch (enchants) {
        case NO_ENCHANTS:
            assert(false);
            break;
        case SWORD_ENCHANTS:
            return enchant(table, item_index, internal, standard_sword_enchantments, LENGTH(standard_sword_enchantments));
        case AXE_ENCHANTS:
            return enchant(table, item_index, internal, standard_axe_enchantments, LENGTH(standard_axe_enchantments));
        case PICKAXE_ENCHANTS:
            return enchant(table, item_index, internal, standard_pickaxe_enchantments, LENGTH(standard_pickaxe_enchantments));
        case SHOVEL_ENCHANTS:
            return enchant(table, item_index, internal, standard_shovel_enchantments, LENGTH(standard_shovel_enchantments));
        case HOE_ENCHANTS:
            return enchant(table, item_index, internal, standard_hoe_enchantments, LENGTH(standard_hoe_enchantments));
        case BOOTS_ENCHANTS:
            return enchant(table, item_index, internal, standard_boot_enchantments, LENGTH(standard_boot_enchantments));
        case LEGGINGS_ENCHANTS:
            return enchant(table, item_index, internal, standard_leggings_enchantments, LENGTH(standard_leggings_enchantments));
        case CHESTPLATE_ENCHANTS:
            return enchant(table, item_index, internal, standard_chestplate_enchantments, LENGTH(standard_chestplate_enchantments));
        case HELMET_ENCHANTS:
            return enchant(table, item_index, internal, standard_helmet_enchantments, LENGTH(standard_helmet_enchantments));
    }

    return enchanted_loot_item_new(table->table[item_index], 1, -1, -1);
}

LootItem (*ruined_portal_loot_providers[25]) (LootTable *table, int item_index, uint64_t *internal) = {
    provide_loot_uniform_roll, provide_loot_uniform_roll, provide_loot_uniform_roll, 
    provide_loot_no_function, provide_loot_no_function, provide_loot_no_function, 
    provide_loot_uniform_roll, provide_loot_random_enchant, provide_loot_random_enchant, 
    provide_loot_random_enchant, provide_loot_random_enchant, provide_loot_random_enchant, 
    provide_loot_random_enchant, provide_loot_random_enchant, provide_loot_random_enchant, 
    provide_loot_random_enchant, provide_loot_uniform_roll, provide_loot_no_function, 
    provide_loot_no_function, provide_loot_uniform_roll, provide_loot_no_function, 
    provide_loot_uniform_roll, provide_loot_no_function, provide_loot_no_function,
    provide_loot_uniform_roll,
};

int precomputedRPLootTable[398] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,16,16,16,16,16,17,17,17,17,17,18,18,18,18,18,19,19,19,19,19,20,20,20,20,20,21,21,21,21,21,22,23,24
};

void ruined_portal_loot(uint64_t loot_seed, LootItem *items, size_t *num_items) {
    uint64_t internal;
    set_seed(&internal, loot_seed);
    *(num_items) = 0;

    int rolls = next_int_bounded(&internal, 4, 8);

    for (int i = 0; i < rolls; i++) {
        int item = precomputedRPLootTable[next_int(&internal, 398)];
        LootItem loot_item = ruined_portal_loot_providers[item](&ruined_portal_table, item, &internal);
        items[*num_items] = loot_item;
        (*num_items)++;
    }
}

int main(void) {
    init_loot_tables();

    LootItem items[64] = {0};
    size_t num_items;
    ruined_portal_loot(933456789L, items, &num_items);

    for (size_t i = 0; i < num_items; i++) {
        printf("%s x %d\n", item_names[items[i].item], items[i].quantity);
        if (items[i].enchanted) {
            printf("   %s %d\n", enchant_names[items[i].enchant], items[i].enchant_level);
        }
    }

    return 0;
}
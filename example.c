#include <stdio.h>

#define LOOT_LIBRARY
#include "src/loot_library.h"

int main(void) {
    init_loot_tables();

    uint64_t loot_seed = 1111L;

    LootItem items[64] = {0};
    size_t num_items;
    ruined_portal_loot(loot_seed, items, &num_items);

    for (size_t i = 0; i < num_items; i++) {
        printf("%s x %d\n", item_names[items[i].item], items[i].quantity);
        if (items[i].enchanted) {
            printf("   %s %d\n", enchant_names[items[i].enchant], items[i].enchant_level);
        }
    }

    return 0;
}
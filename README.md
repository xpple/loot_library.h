# loot_library.h
A C library for emulating Minecraft loot table generation. Still a WIP, API is still subject to sudden changes.

While it's written in C, the entire library can be ported to CUDA very easily if needed.

Currently supports ruined portals and desert temples.

# example

Searching through ruined portal loot seeds.
```C
#include <stdio.h>

#define LOOT_LIBRARY
#include "src/loot_library.h"

int main(void) {
    LootTable table = init_ruined_portal_loot_table();

    uint64_t loot_seed = 1111L;

    LootItem items[64] = {0};
    size_t num_items;
    ruined_portal_loot(&table, loot_seed, items, &num_items);

    for (size_t i = 0; i < num_items; i++) {
        printf("%s x %d\n", item_names[items[i].item], items[i].quantity);
        if (items[i].enchanted) {
            printf("   %s %d\n", enchant_names[items[i].enchant], items[i].enchant_level);
        }
    }

    return 0;
}
```

# TODO
## Update 1
- load directly from FILE* instead of const char* to avoid relative filepath issues
- set_effect loot function (
- chained loot tables

## Update 2
- multiple operation modes for LootTableContext: as-is, aggregated matching items, aggregated item-type matching items, predicate-match, full (indexed)

## Update 3
- loot sequence support
- restructure project: move legacy code to a subfolder (src/legacy?)
- update readme with v2 example of use

## Update 4
- full 1.13+ support (incl. 1.14.2?)
- mass testing using source code
- add more examples in src/examples

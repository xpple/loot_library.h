#include "loot_table_parser.h"

#include <stdio.h>


int main() {
	LootTableContext ctx;
	init_loot_table("stronghold_library_1_20_5.json", &ctx, (MCVersion)v1_16);

	set_loot_seed(&ctx, 12345ULL);
	generate_loot(&ctx);

	for (int i = 0; i < ctx.generated_item_count; i++)
	{
		ItemStack* item_stack = &ctx.generated_items[i];
		printf("%s x %d\n", get_item_name(&ctx, item_stack->item), item_stack->count);

		for (int j = 0; j < item_stack->enchantment_count; j++)
		{
			EnchantInstance* ench = &item_stack->enchantments[j];
			printf("    %s %d\n", get_enchantment_name(ench->enchantment), ench->level);
		}
	}

	free_loot_table(&ctx);

	return 0;
}
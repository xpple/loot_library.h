#include "mc_loot.h"
#include <stdio.h>


static void print_loot(LootTableContext* ctx)
{
	for (int i = 0; i < ctx->generated_item_count; i++)
	{
		ItemStack* item_stack = &(ctx->generated_items[i]);
		printf("%s x %d\n", get_item_name(ctx, item_stack->item), item_stack->count);

		for (int j = 0; j < item_stack->enchantment_count; j++)
		{
			EnchantInstance* ench = &(item_stack->enchantments[j]);
			printf("    %s %d\n", get_enchantment_name(ench->enchantment), ench->level);
		}
	}
}

int test_shipwreck() {
	LootTableContext ctx;

	FILE* file = fopen("src/examples/shipwreck_supply.json", "r");
	int ret = init_loot_table_file(file, &ctx, (MCVersion)v1_21);
	fclose(file);

	if (ret != 0) {
		fprintf(stderr, "Error initializing loot table\n");
		return ret;
	}

	set_loot_seed(&ctx, 123456ULL);
	generate_loot(&ctx);
	print_loot(&ctx);
	free_loot_table(&ctx);

	return 0;
}

int test_ominous_vault() {
	LootTableContext ctx;

	FILE* file = fopen("src/examples/chained_tables/reward_ominous.json", "r");
	int ret = init_loot_table_file(file, &ctx, (MCVersion)v1_21);
	fclose(file);
	if (ret != 0) {
		fprintf(stderr, "Error initializing loot table\n");
		return ret;
	}

	// resolve subtables in the correct order: rare, common, unique
	file = fopen("src/examples/chained_tables/reward_ominous_rare.json", "r");
	ret = resolve_subtable_file(&ctx, "minecraft:chests/trial_chambers/reward_ominous_rare", file);
	fclose(file);
	if (ret != 0) {
		fprintf(stderr, "Error resolving subtable\n");
		return ret;
	}

	file = fopen("src/examples/chained_tables/reward_ominous_common.json", "r");
	ret = resolve_subtable_file(&ctx, "minecraft:chests/trial_chambers/reward_ominous_common", file);
	fclose(file);
	if (ret != 0) {
		fprintf(stderr, "Error resolving subtable\n");
		return ret;
	}

	file = fopen("src/examples/chained_tables/reward_ominous_unique.json", "r");
	ret = resolve_subtable_file(&ctx, "minecraft:chests/trial_chambers/reward_ominous_unique", file);
	fclose(file);
	if (ret != 0) {
		fprintf(stderr, "Error resolving subtable\n");
		return ret;
	}

	set_loot_seed(&ctx, 123456ULL);
	generate_loot(&ctx);
	print_loot(&ctx);
	free_loot_table(&ctx);

	return 0;
}

int main()
{
	test_ominous_vault();
}

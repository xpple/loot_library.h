#include "loot_table_parser.h"


int main() {
	LootTableContext ctx;

	init_loot_table("end_city.json", &ctx, (MCVersion)v1_16);
	free_loot_table(&ctx);

	return 0;
}
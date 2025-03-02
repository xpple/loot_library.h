#ifndef _LOOT_TABLE_PARSER_H
#define _LOOT_TABLE_PARSER_H

#include "loot_table_context.h"


int init_loot_table(const char* loot_table_json, LootTableContext* context, const MCVersion version);
void free_loot_table(LootTableContext* context);

#endif
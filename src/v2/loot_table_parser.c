#include "mc_loot.h"
#include "loot_functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cjson/CJSON.h"

// OK
static ItemType get_item_type(const char* item_name)
{
	if (strstr(item_name, "_pickaxe") != NULL) return PICKAXE;
	if (strstr(item_name, "_axe") != NULL) return AXE;
	if (strstr(item_name, "_shovel") != NULL) return SHOVEL;
	if (strstr(item_name, "_hoe") != NULL) return HOE;
	if (strstr(item_name, "_sword") != NULL) return SWORD;
	if (strstr(item_name, "_helmet") != NULL) return HELMET;
	if (strstr(item_name, "_chestplate") != NULL) return CHESTPLATE;
	if (strstr(item_name, "_leggings") != NULL) return LEGGINGS;
	if (strstr(item_name, "_boots") != NULL) return BOOTS;

	if (strcmp(item_name, "fishing_rod") == 0) return FISHING_ROD;
	if (strcmp(item_name, "crossbow") == 0) return CROSSBOW;
	if (strcmp(item_name, "trident") == 0) return TRIDENT;
	if (strcmp(item_name, "bow") == 0) return BOW;
	if (strcmp(item_name, "book") == 0) return BOOK;
	if (strcmp(item_name, "mace") == 0) return MACE;

	return NO_ITEM;
}

// OK
static Enchantment get_enchantment_from_name(const char* ench)
{
	// I'm sorry.
	if (strcmp(ench, "protection") == 0) return PROTECTION;
	if (strcmp(ench, "fire_protection") == 0) return FIRE_PROTECTION;
	if (strcmp(ench, "feather_falling") == 0) return FEATHER_FALLING;
	if (strcmp(ench, "blast_protection") == 0) return BLAST_PROTECTION;
	if (strcmp(ench, "projectile_protection") == 0) return PROJECTILE_PROTECTION;
	if (strcmp(ench, "respiration") == 0) return RESPIRATION;
	if (strcmp(ench, "aqua_affinity") == 0) return AQUA_AFFINITY;
	if (strcmp(ench, "thorns") == 0) return THORNS;
	if (strcmp(ench, "soul_speed") == 0) return SOUL_SPEED;
	if (strcmp(ench, "depth_strider") == 0) return DEPTH_STRIDER;
	if (strcmp(ench, "frost_walker") == 0) return FROST_WALKER;
	if (strcmp(ench, "swift_sneak") == 0) return SWIFT_SNEAK;
	
	if (strcmp(ench, "sharpness") == 0) return SHARPNESS;
	if (strcmp(ench, "smite") == 0) return SMITE;
	if (strcmp(ench, "bane_of_arthropods") == 0) return BANE_OF_ARTHROPODS;
	if (strcmp(ench, "knockback") == 0) return KNOCKBACK;
	if (strcmp(ench, "fire_aspect") == 0) return FIRE_ASPECT;
	if (strcmp(ench, "looting") == 0) return LOOTING;
	if (strstr(ench, "sweeping") != NULL) return SWEEPING_EDGE;

	if (strcmp(ench, "efficiency") == 0) return EFFICIENCY;
	if (strcmp(ench, "silk_touch") == 0) return SILK_TOUCH;
	if (strcmp(ench, "fortune") == 0) return FORTUNE;

	if (strcmp(ench, "power") == 0) return POWER;
	if (strcmp(ench, "flame") == 0) return FLAME;
	if (strcmp(ench, "infinity") == 0) return INFINITY_ENCHANTMENT;
	if (strcmp(ench, "punch") == 0) return PUNCH;

	if (strcmp(ench, "multishot") == 0) return MULTISHOT;
	if (strcmp(ench, "quick_charge") == 0) return QUICK_CHARGE;
	if (strcmp(ench, "piercing") == 0) return PIERCING;

	if (strcmp(ench, "impaling") == 0) return IMPALING;
	if (strcmp(ench, "loyalty") == 0) return LOYALTY;
	if (strcmp(ench, "riptide") == 0) return RIPTIDE;
	if (strcmp(ench, "channeling") == 0) return CHANNELING;

	if (strcmp(ench, "luck_of_the_sea") == 0) return LUCK_OF_THE_SEA;
	if (strcmp(ench, "lure") == 0) return LURE;

	if (strcmp(ench, "density") == 0) return DENSITY;
	if (strcmp(ench, "breach") == 0) return BREACH;
	if (strcmp(ench, "wind_burst") == 0) return WIND_BURST;

	if (strcmp(ench, "mending") == 0) return MENDING;
	if (strcmp(ench, "unbreaking") == 0) return UNBREAKING;
	if (strstr(ench, "binding") != NULL) return CURSE_OF_BINDING;
	if (strstr(ench, "vanishing") != NULL) return CURSE_OF_VANISHING;

	return NO_ENCHANTMENT;
}

// ----------------------------------------------
// loot function parsers

// OK
static int parse_set_count(LootFunction* loot_function, const cJSON* function_data)
{
	cJSON* count = cJSON_GetObjectItem(function_data, "count");

	if (cJSON_IsNumber(count))
	{
		// constant count
		create_set_count(loot_function, count->valueint, count->valueint);
		return 0;
	}
	else
	{
		// uniformly rolled count
		const int min = cJSON_GetObjectItem(count, "min")->valueint;
		const int max = cJSON_GetObjectItem(count, "max")->valueint;
		create_set_count(loot_function, min, max);
		return 0;
	}
}

// OK
static void parse_enchant_randomly(LootTableContext* ctx, LootFunction* loot_function, const cJSON* function_data, const char* item_name)
{
	const ItemType item_type = get_item_type(item_name);

	// if the "enchantments" field is present, we need to extract the enchantment name.
	// otherwise, we can simply create the enchant randomly function
	cJSON* defined_echants = cJSON_GetObjectItem(function_data, "enchantments");
	if (defined_echants == NULL)
	{
		// no-restriction enchant randomly
		create_enchant_randomly(loot_function, ctx->version, item_type, 1); // FIXME isTreasure is temporarily just set to true
		return;
	}

	// enchant randomly with a given list of enchantments
	const int ench_count = cJSON_GetArraySize(defined_echants);
	if (ench_count == 1)
	{
		// one-enchant variant
		const char* ench_name = cJSON_GetArrayItem(defined_echants, 0)->valuestring;
		const Enchantment ench = get_enchantment_from_name(ench_name);
		create_enchant_randomly_one_enchant(loot_function, ench);
	}
	else
	{
		// multi-enchant variant
		Enchantment* enchs = (Enchantment*)malloc(ench_count * sizeof(Enchantment));
		if (enchs == NULL) return; // FIXME add logging

		int i = 0;
		cJSON* element = NULL;
		cJSON_ArrayForEach(element, defined_echants)
		{
			const char* ench_name = cJSON_GetStringValue(element);
			const Enchantment ench = get_enchantment_from_name(ench_name);
			enchs[i] = ench;
			i++;
		}

		create_enchant_randomly_list(loot_function, enchs, ench_count);
		free(enchs);
	}
}

// OK
static void parse_enchant_with_levels(LootTableContext* ctx, LootFunction* loot_function, const cJSON* function_data, const char* item_name)
{
	const ItemType item_type = get_item_type(item_name);

	int min_level = 0;
	int max_level = 0;
	int is_treasure = 1;

	cJSON* levels = cJSON_GetObjectItem(function_data, "levels");
	if (cJSON_IsNumber(levels))
	{
		min_level = levels->valueint;
		max_level = levels->valueint;
	}
	else if (levels != NULL)
	{
		min_level = cJSON_GetObjectItem(levels, "min")->valueint;
		max_level = cJSON_GetObjectItem(levels, "max")->valueint;
	}

	cJSON* treasure = cJSON_GetObjectItem(function_data, "treasure");
	if (treasure != NULL)
		is_treasure = (int)cJSON_IsTrue(treasure);

	create_enchant_with_levels(
		loot_function, 
		ctx->version, 
		item_name, item_type, 
		min_level, max_level, 
		is_treasure
	);
}

// ----------------------------------------------


static void init_loot_table_items(const cJSON* loot_table, LootTableContext* ctx)
{
	// count how many item entries there are in the loot table
	int item_count = 0;

	const cJSON* pools = cJSON_GetObjectItem(loot_table, "pools");
	cJSON* pool = NULL;

	cJSON_ArrayForEach(pool, pools)
	{
		cJSON* entries = cJSON_GetObjectItem(pool, "entries");
		cJSON* entry = NULL;

		cJSON_ArrayForEach(entry, entries)
		{
			cJSON* name = cJSON_GetObjectItem(entry, "name");
			if (name == NULL) continue;
			item_count++;
		}
	}

	// allocate memory for item names
	ctx->item_count = item_count;
	ctx->item_names = (char**)malloc(item_count * sizeof(char*));
	if (ctx->item_names == NULL) return; // FIXME add logging

	// fill the item names array
	int ix = 0;
	cJSON_ArrayForEach(pool, pools)
	{
		cJSON* entries = cJSON_GetObjectItem(pool, "entries");
		cJSON* entry = NULL;

		cJSON_ArrayForEach(entry, entries)
		{
			cJSON* name = cJSON_GetObjectItem(entry, "name");
			if (name == NULL) continue;
			
			// copy the name to the LootContext structure
			const char* item_name = cJSON_GetStringValue(name);
			ctx->item_names[ix] = (char*)malloc(strlen(item_name) + 1);
			if (ctx->item_names[ix] == NULL || item_name == NULL) return; // FIXME add logging
			strcpy(ctx->item_names[ix], item_name);
			ix++;
			if (ix >= item_count) break;
		}
	}
}

// OK
static void init_rolls(const cJSON* pool_data, LootPool* pool)
{
	cJSON* rolls = cJSON_GetObjectItem(pool_data, "rolls");

	if (cJSON_IsNumber(rolls))
	{
		// constant roll
		pool->min_rolls = rolls->valueint;
		pool->max_rolls = rolls->valueint;
	}
	else
	{
		// uniform roll
		pool->min_rolls = cJSON_GetObjectItem(rolls, "min")->valueint;
		pool->max_rolls = cJSON_GetObjectItem(rolls, "max")->valueint;
	}
}

static void map_entry_to_item(const cJSON* entry_data, LootTableContext* ctx, int* item_id)
{
	char* name_field = extract_named_object(entry_data, "\"name\":"); // 1
	if (name_field == NULL) {
		*item_id = -1; // empty entry
		return; 
	}

	char* iname = remove_minecraft_prefix(name_field); // 2
	for (int i = 0; i < ctx->item_count; i++) {
		if (strcmp(ctx->item_names[i], iname) == 0) {
			*item_id = i;
			break;
		}
	}
	free(iname); // 1
	free(name_field); // 0
}

static void init_entry(const cJSON* entry_data, LootPool* pool, const int entry_id, LootTableContext* ctx)
{
	int functions = 0;

	char* functions_field = extract_named_object(entry_data, "\"functions\":"); // 1
	if (functions_field != NULL) {
		functions = count_unnamed(functions_field);
		free(functions_field); // 0
	}

	int w = extract_int(entry_data, "\"weight\":", 1);
	pool->total_weight += w;

	// initialize loot function fields

	pool->entry_functions_count[entry_id] = functions;
	int new_index = 0;
	if (entry_id > 0)
		new_index = pool->entry_functions_index[entry_id - 1] + pool->entry_functions_count[entry_id - 1];
	pool->entry_functions_index[entry_id] = new_index;

	// initialize entry to item mapping field

	pool->entry_to_item[entry_id] = -1; // assume it's an empty entry for now
	map_entry_to_item(entry_data, ctx, &(pool->entry_to_item[entry_id]));

	// loot function initialization will be done after all entries are processed
	// because we need to know the total number of functions to allocate the array
}

static void init_entry_functions(const cJSON* entry_data, LootPool* pool, const int entry_id, LootTableContext* ctx)
{
	char* functions_field = extract_named_object(entry_data, "\"functions\":"); // 1
	if (functions_field == NULL)
		return;
	
	const int entry_item = pool->entry_to_item[entry_id];
	const char* entry_item_name = entry_item == -1 ? NULL : ctx->item_names[entry_item];
	int findex = pool->entry_functions_index[entry_id];
	int fcount = pool->entry_functions_count[entry_id];

	for (int i = 0; i < fcount; i++) {
		int index_in_array = findex + i;
		LootFunction* loot_function = &(pool->loot_functions[index_in_array]);

		char* function_data = extract_unnamed_object(functions_field, i); // 2
		char* function_name = extract_named_object(function_data, "\"function\":"); // 3

		if (strcmp(function_name, "\"minecraft:set_count\"") == 0) {
			parse_set_count(loot_function, function_data);
		}
		else if (strcmp(function_name, "\"minecraft:enchant_with_levels\"") == 0) {
			parse_enchant_with_levels(ctx, loot_function, function_data, entry_item_name);
		}
		else if (strcmp(function_name, "\"minecraft:enchant_randomly\"") == 0) {
			parse_enchant_randomly(ctx, loot_function, function_data, entry_item_name);
		}
		else if (strcmp(function_name, "\"minecraft:set_damage\"") == 0) {
			// inline parsing here, it's a very simple function
			create_set_damage(loot_function);
		}
		else {
			create_no_op(loot_function);
		}

		free(function_data); // 2
		free(function_name); // 1
	}

	free(functions_field); // 0
	return;
}

static void precompute_loot_pool(LootPool* pool, const cJSON* entries_field)
{
	int index = 0;

	for (int i = 0; i < pool->entry_count; i++) {
		char* entry_data = extract_unnamed_object(entries_field, i); // 1
		int entry_weight = extract_int(entry_data, "\"weight\":", 1);

		for (int j = 0; j < entry_weight; j++) {
			pool->precomputed_loot[index] = i;
			index++;
		}

		free(entry_data); // 0
	}
}

// OK
static int init_loot_pool(const cJSON* pool_data, const int pool_id, LootTableContext* ctx)
{
	LootPool* pool = &(ctx->loot_pools[pool_id]);
	pool->total_weight = 0;
	init_rolls(pool_data, pool);

	// count entries
	cJSON* entries = cJSON_GetObjectItem(pool_data, "entries");
	pool->entry_count = cJSON_GetArraySize(entries);

	// allocate memory for entry data
	pool->entry_to_item = (int*)malloc(pool->entry_count * sizeof(int));
	pool->entry_functions_index = (int*)malloc(pool->entry_count * sizeof(int));
	pool->entry_functions_count = (int*)malloc(pool->entry_count * sizeof(int));
	if (pool->entry_to_item == NULL || pool->entry_functions_index == NULL || pool->entry_functions_count == NULL)
		return -1;

	// first pass: count total loot functions and create simple entry mappings
	for (int i = 0; i < pool->entry_count; i++) {
		cJSON* entry = cJSON_GetArrayItem(entries, i);
		init_entry(entry, pool, i, ctx);
	}

	// second pass: initialize loot functions
	int last_ix = pool->entry_count - 1;
	int function_count = pool->entry_functions_index[last_ix] + pool->entry_functions_count[last_ix];
	pool->loot_functions = (LootFunction*)malloc(function_count * sizeof(LootFunction));
	if (pool->loot_functions == NULL)
		return -1; // FIXME add logging

	for (int i = 0; i < pool->entry_count; i++) {
		cJSON* entry = cJSON_GetArrayItem(entries, i);
		init_entry_functions(entry, pool, i, ctx);
	}

	// final pass: precompute loot table
	pool->precomputed_loot = (int*)malloc(pool->total_weight * sizeof(int));
	if (pool->precomputed_loot == NULL)
		return -1; // FIXME add logging

	precompute_loot_pool(pool, entries);

	return 0;
}

// OK
static void free_loot_function(LootFunction* lf)
{
	if (lf->varparams_int != NULL)
		free(lf->varparams_int);

	if (lf->varparams_int_arr != NULL)
	{
		for (int i = 0; i < lf->varparams_int_arr_size; i++)
			free(lf->varparams_int_arr[i]);
		free(lf->varparams_int_arr);
		lf->varparams_int_arr_size = 0;
	}

	lf->params = NULL;
	lf->fun = NULL;
}

// OK
static void free_loot_pool(LootPool* pool)
{
	free(pool->precomputed_loot);

	free(pool->entry_to_item);
	
	int last = pool->entry_count - 1;
	int functionCount = pool->entry_functions_index[last] + pool->entry_functions_count[last];
	free(pool->entry_functions_index);
	free(pool->entry_functions_count);

	for (int i = 0; i < functionCount; i++)
		free_loot_function(&(pool->loot_functions[i]));

	free(pool->loot_functions);
}

// -------------------------------------------------------------------------------------

int init_loot_table(const char* filename, LootTableContext* context, const MCVersion version)
{
	context->version = version;

	// open the file
	FILE* fptr = fopen(filename, "r");
	if (fptr == NULL)
		return -1;
	fseek(fptr, 0, SEEK_END);
	size_t file_size = ftell(fptr);	// get the size of the file
	fseek(fptr, 0, SEEK_SET);		// go back to the beginning of the file

	// allocate memory for file contents
	char* file_content = (char*)malloc(file_size + 1); // 1
	if (file_content == NULL) {
		fclose(fptr);
		return -1;
	}

	// read the file content
	int read = (int)fread(file_content, 1, file_size, fptr);
	if (read != file_size) {
		fclose(fptr);
		free(file_content);
		return -1;
	}

	file_content[file_size] = '\0';
	fclose(fptr);

	cJSON* loot_table = cJSON_Parse(file_content);
	if (loot_table == NULL) {
		free(file_content); // 1
		return -1;
	}
	free(file_content); // 1

	// TODO

	cJSON_Delete(loot_table);
	return 0;
}

// OK
void free_loot_table(LootTableContext* context)
{
	// free item name arrays
	for (int i = 0; i < context->item_count; i++)
		free(context->item_names[i]);
	free(context->item_names);

	// free loot pools
	for (int i = 0; i < context->pool_count; i++)
		free_loot_pool(&(context->loot_pools[i]));
	free(context->loot_pools);
}

#include "mc_loot.h"
#include "loot_functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ------------------------------------------------------------------
// WARNING!
// this file contains awful, custom code for parsing json-defined
// Minecraft loot tables.
// ------------------------------------------------------------------


















#define ERR(msg) { fprintf(stderr, "ERROR in %s line %d: %s\n", __FILE__, __LINE__, msg); return -1; }
#define ENABLE_DEBUG_MESSAGES 0
#define DEBUG_MSG(...) { if (ENABLE_DEBUG_MESSAGES) printf(__VA_ARGS__); }

static char* get_no_whitespace_string(const char* str)
{
	int len = (int)strlen(str);
	char* no_whitespace = (char*)malloc(len + 1);

	int j = 0;
	for (int i = 0; i < len; i++)
	{
		if (!isspace(str[i]))
		{
			no_whitespace[j] = str[i];
			j++;
		}
	}
	no_whitespace[j] = '\0';
	return no_whitespace;
}

static char* substr(const char* str, int start, int end)
{
	if (start < 0) return NULL;
	char* ret_str = (char*)malloc((size_t)end - start + 2);
	if (ret_str == NULL) return NULL;
	strncpy(ret_str, str + start, (size_t)end - start + 1);
	ret_str[end - start + 1] = '\0';
	return ret_str;
}

static char* extract_unnamed_object(const char* jsonstr, int index) {
	// find the index-th unnamed object in the json string,
	// return it as a new string.

	int list_depth = 0;
	int obj_depth = 0;
	int len = (int)strlen(jsonstr);
	// DEBUG_MSG("extract_unnamed_object got string with len = %d\n", len);

	int indexBegin = 0;
	int indexEnd = 0;

	for (int i = 0; i < len; i++) {
		if (jsonstr[i] == '[') list_depth++;
		else if (jsonstr[i] == ']') list_depth--;
		if (jsonstr[i] == '{') obj_depth++;
		else if (jsonstr[i] == '}') obj_depth--;

		// if we are at the correct depth and we found the index-th object,
		// extract it and return it as a new string.

		if (jsonstr[i] == '{' && obj_depth == 1 && list_depth == 1) {
			if (index == 0) {
				indexBegin = i;
			}
			index--;
		}
		else if (jsonstr[i] == '}' && obj_depth == 0 && list_depth == 1) {
			if (index == -1) {
				indexEnd = i;
				break;
			}
		}
	}

	// DEBUG_MSG("indexBegin: %d,  indexEnd: %d\n", indexBegin, indexEnd);

	// extract the substring from indexBegin to indexEnd, or return null if the object was not found
	if (indexBegin == 0 && indexEnd == 0)
		return NULL;
	char* extracted = substr(jsonstr, indexBegin, indexEnd);
	return extracted;
}

static char* extract_named_object(const char* jsonstr, const char* key) {
	// find the object with the given name in the json string at the current depth,
	// return it as a new string.

	static const char startChars[] = { '{', '['};
	static const char endChars[] = { '}', ']' };

	// search for the key in the string
	char* keyPos = strstr(jsonstr, key);
	if (keyPos == NULL) return NULL;

	keyPos += strlen(key); // quote & colon are inside the key

	// extract the character that follows the key
	char objStart = *keyPos;
	char incDepth = 0;
	char decDepth = 0;

	// setup extraction of bracket-enclosed objects
	for (int i = 0; i < 2; i++) {
		if (objStart == startChars[i]) {
			incDepth = startChars[i];
			decDepth = endChars[i];
		}
	}

	int length = 0;
	int depth = 1;

	// find the number of characters in the object
	while (depth > 0) {
		length++;
		const char c = keyPos[length];
		if (c == incDepth) depth++;
		if (c == decDepth) depth--;

		// handle cases where it's just a key-value pair
		if (incDepth == 0 && c == ',') {
			length--;
			break;
		}
		if (incDepth == 0 && (c == '}' || c == ']')) {
			length--;
			break;
		}
	}
	//DEBUG_MSG("extracting substring of length: %d\n", length);

	// extract the substring
	char* extracted = substr(keyPos, 0, length);
	return extracted;
}

static int count_unnamed(const char* jsonarr)
{
	int depth = 0;
	int count = 0;
	int len = (int)strlen(jsonarr);

	for (int i = 0; i < len; i++) {
		if (jsonarr[i] == '[' || jsonarr[i] == '{') depth++;
		else if (jsonarr[i] == ']' || jsonarr[i] == '}') depth--;

		if (jsonarr[i] == '{' && depth == 2) 
			count++;
	}

	return count;
}

static char* remove_minecraft_prefix(const char* value)
{
	// remove quotes and "minecraft:" prefix
	char* name = substr(value, 1 + 10, (int)strlen(value) - 2);
	return name;
}

static int extract_int(const char* jsonstr, const char* key, const int or_else)
{
	char* value = extract_named_object(jsonstr, key);
	if (value == NULL) 
		return or_else;

	int result = atoi(value);
	free(value);
	return result;
}

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

static int parse_set_count(LootFunction* loot_function, const char* function_data)
{
	char* count_field = extract_named_object(function_data, "\"count\":"); // 1
	if (count_field == NULL)
		ERR("set_count function does not declare a count field");

	int min_i = extract_int(count_field, "\"min\":", -1);
	int max_i = extract_int(count_field, "\"max\":", -1);

	if (min_i == -1 || max_i == -1) {
		min_i = max_i = atoi(count_field);
	}

	free(count_field); // 0

	create_set_count(loot_function, min_i, max_i);
	return 0;
}

static void parse_enchant_randomly(LootTableContext* ctx, LootFunction* loot_function, const char* function_data, const char* item_name)
{
	// if the "enchantments" field is present, we need to extract the enchantment name.
	// otherwise, we can simply create the enchant randomly function

	ItemType item_type = get_item_type(item_name);
	char* defined_enchantment = extract_named_object(function_data, "\"enchantments\":"); // 1

	if (defined_enchantment == NULL)
	{
		create_enchant_randomly(loot_function, ctx->version, item_type, 1); // FIXME isTreasure is temporarily just set to true
		//DEBUG_MSG("Parsed enchant randomly for %s\n", item_name);
		return;
	}

	// extract enchantment index from enchantment name
	char* enchantment_name = substr(defined_enchantment, 1, (int)strlen(defined_enchantment) - 2); // 2
	free(defined_enchantment); // 1
	char* short_name = remove_minecraft_prefix(enchantment_name); // 2
	free(enchantment_name); // 1
	const Enchantment ench = get_enchantment_from_name(short_name);
	//DEBUG_MSG("Parsed one-enchant (%d, %s) enchant randomly for %s\n", ench, short_name, item_name);
	free(short_name); // 0

	create_enchant_randomly_one_enchant(loot_function, ench);
}

static void parse_enchant_with_levels(LootTableContext* ctx, LootFunction* loot_function, const char* function_data, const char* item_name)
{
	ItemType item_type = get_item_type(item_name);

	// need: min level, max level, isTreasure
	char* levels_object = extract_named_object(function_data, "\"levels\":"); // 1
	int min_level = extract_int(levels_object, "\"min\":", 0);
	int max_level = extract_int(levels_object, "\"max\":", 0);
	if (min_level == 0 && max_level == 0)
		min_level = max_level = atoi(levels_object); // levels = some constant
	free(levels_object); // 0

	char* is_treasure_field = extract_named_object(function_data, "\"treasure\":"); // 1
	int is_treasure = (is_treasure_field == NULL || strcmp(is_treasure_field, "true") == 0) ? 1 : 0;
	if (is_treasure_field != NULL) 
		free(is_treasure_field); // 0

	create_enchant_with_levels(loot_function, ctx->version, item_name, item_type, min_level, max_level, is_treasure);

	//DEBUG_MSG("Parsed enchant with levels for %s: %d - %d, treasure: %d\n", item_name, min_level, max_level, is_treasure);
}

// ----------------------------------------------


// private
static void init_loot_table_items(char* loot_table_string, LootTableContext* ctx)
{
	char* cursor = loot_table_string;

	// firstly, count items
	ctx->item_count = 0;
	while (1) {
		cursor = strstr(cursor, "\"name\":");
		if (cursor == NULL) break;
		ctx->item_count++;
		cursor++; // won't find the previous key now
	}

	//DEBUG_MSG("Found %d distinct items\n", ctx->item_count);

	// allocate memory for item names
	ctx->item_names = (char**)malloc(ctx->item_count * sizeof(char*));
	
	// fill the array
	cursor = loot_table_string;

	for (int i = 0; i < ctx->item_count; i++) {
		char* val = extract_named_object(cursor, "\"name\":");
		char* name = remove_minecraft_prefix(val);
		free(val);
		ctx->item_names[i] = name;
		//DEBUG_MSG("Item %d: %s\n", i, name);

		cursor = strstr(cursor, "\"name\":") + 8;
	}
}

// private
static int init_rolls(const char* pool_data, LootPool* pool)
{
	char* rolls_field = extract_named_object(pool_data, "\"rolls\":"); // 1
	if (rolls_field == NULL)
		return -1;

	int minrolls = extract_int(rolls_field, "\"min\":", -1);
	int maxrolls = extract_int(rolls_field, "\"max\":", -1);
	pool->min_rolls = minrolls;
	pool->max_rolls = maxrolls;
	pool->roll_count_function = roll_count_uniform;

	if (minrolls == -1 || maxrolls == -1) {
		// constant roll
		int rolls_i = atoi(rolls_field);
		pool->min_rolls = rolls_i;
		pool->max_rolls = rolls_i;
		pool->roll_count_function = roll_count_constant;
	}

	free(rolls_field); // 0
	return 0;
}

// private
static void map_entry_to_item(const char* entry_data, LootTableContext* ctx, int* item_id)
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

// private
static void init_entry(const char* entry_data, LootPool* pool, const int entry_id, LootTableContext* ctx)
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

// private
static void init_entry_functions(const char* entry_data, LootPool* pool, const int entry_id, LootTableContext* ctx)
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

// private
static void precompute_loot_pool(LootPool* pool, const char* entries_field)
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

// private
static int init_loot_pool(const char* pool_data, const int pool_id, LootTableContext* ctx)
{
	//DEBUG_MSG("Initializing loot pool %d\n", pool_id);

	LootPool* pool = &(ctx->loot_pools[pool_id]);
	pool->total_weight = 0;

	int ret = init_rolls(pool_data, pool);
	if (ret != 0)
		ERR("Loot pool does not declare a roll choice function");

	// count entries inside loot table

	char* entries_field = extract_named_object(pool_data, "\"entries\":"); // 1
	if (entries_field == NULL)
		ERR("Loot pool does not declare any entries");
	pool->entry_count = count_unnamed(entries_field);

	// create the entries
	pool->entry_to_item = (int*)malloc(pool->entry_count * sizeof(int));
	pool->entry_functions_index = (int*)malloc(pool->entry_count * sizeof(int));
	pool->entry_functions_count = (int*)malloc(pool->entry_count * sizeof(int));
	if (pool->entry_to_item == NULL || pool->entry_functions_index == NULL || pool->entry_functions_count == NULL)
		ERR("Could not allocate memory for entry fields");

	// first pass: count total loot functions and create simple entry mappings
	for (int i = 0; i < pool->entry_count; i++) {
		char* entry_data = extract_unnamed_object(entries_field, i); // 2
		//DEBUG_MSG("POOL %d:  ENTRY %d\n", pool_id, i);
		init_entry(entry_data, pool, i, ctx);
		free(entry_data); // 1
	}

	// second pass: initialize loot functions
	int last_ix = pool->entry_count - 1;
	int function_count = pool->entry_functions_index[last_ix] + pool->entry_functions_count[last_ix];
	pool->loot_functions = (LootFunction*)malloc(function_count * sizeof(LootFunction));
	if (pool->loot_functions == NULL)
		ERR("Could not allocate memory for entry fields");

	for (int i = 0; i < pool->entry_count; i++) {
		char* entry_data = extract_unnamed_object(entries_field, i); // 2
		init_entry_functions(entry_data, pool, i, ctx);
		free(entry_data); // 1
	}

	// final pass: precompute loot table
	pool->precomputed_loot = (int*)malloc(pool->total_weight * sizeof(int));
	if (pool->precomputed_loot == NULL) {
		//DEBUG_MSG("Total weight: %d\n", pool->total_weight);
		ERR("Could not allocate memory for precomputed loot table\n");
	}
	
	//DEBUG_MSG("Precomputing pool %d ->\n", pool_id);
	precompute_loot_pool(pool, entries_field);
	//DEBUG_MSG("-> done, total weight = %d\n\n", pool->total_weight);

	free(entries_field); // 0

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
		ERR("Could not open file");
	fseek(fptr, 0, SEEK_END);
	size_t file_size = ftell(fptr);	// get the size of the file
	fseek(fptr, 0, SEEK_SET);		// go back to the beginning of the file

	// allocate memory for file contents
	char* file_content = (char*)malloc(file_size + 1); // 1
	if (file_content == NULL) {
		fclose(fptr);
		ERR("Could not allocate memory for file content");
	}

	// read the file content
	int read = (int)fread(file_content, 1, file_size, fptr);
	if (read != file_size) {
		fclose(fptr);
		free(file_content);
		ERR("Error while reading file content");
	}

	file_content[file_size] = '\0';
	fclose(fptr);

	// TODO

	free(file_content); // 1
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

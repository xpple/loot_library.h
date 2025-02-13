#include "loot_table_parser.h"
#include "loot_functions.h"
#include "debug_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ------------------------------------------------------------------
// WARNING!
// this file contains awful, custom code for parsing json-defined
// Minecraft loot tables because i couldn't be bothered to use
// a proper, efficient json parser.
// I am not responsible for any mental trauma or damaged eyesight
// looking at the code below may cause. You have been warned.
// ------------------------------------------------------------------


















#define ERR(msg) { fprintf(stderr, "ERROR in %s line %d: %s\n", __FILE__, __LINE__, msg); return -1; }

static char* get_no_whitespace_string(const char* str)
{
	int len = strlen(str);
	char* no_whitespace = (char*)malloc(len + 1);
	if (no_whitespace == NULL)
		ERR("Could not allocate memory for no_whitespace string");

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
	int len = strlen(jsonstr);
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

	keyPos += strlen(key);
	keyPos += 2; // format is always "key":(value), need to skip quote & colon

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
	int len = strlen(jsonarr);

	for (int i = 0; i < len; i++) {
		if (jsonarr[i] == '[' || jsonarr[i] == '{') depth++;
		else if (jsonarr[i] == ']' || jsonarr[i] == '}') depth--;

		if (jsonarr[i] == '{' && depth == 2) 
			count++;
	}

	return count;
}

static char* get_item_name(const char* value)
{
	// remove quotes and "minecraft:" prefix
	char* name = substr(value, 1 + 10, strlen(value) - 2);
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


// ----------------------------------------------
// loot function parsers

static void parse_set_count(LootFunction* loot_function, const char* function_data)
{
	char* count_field = extract_named_object(function_data, "count"); // 1
	if (count_field == NULL)
		ERR("set_count function does not declare a count field");

	int min_i = extract_int(count_field, "min", -1);
	int max_i = extract_int(count_field, "max", -1);

	if (min_i == -1 || max_i == -1) {
		min_i = max_i = atoi(count_field);
	}

	free(count_field); // 0

	create_set_count(loot_function, min_i, max_i);
}


// ----------------------------------------------


// private
static void init_loot_table_items(const char* loot_table_string, LootTableContext* ctx)
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

	DEBUG_MSG("Found %d distinct items\n", ctx->item_count);

	// allocate memory for item names
	ctx->item_names = (char**)malloc(ctx->item_count * sizeof(char*));
	
	// fill the array
	cursor = loot_table_string;

	for (int i = 0; i < ctx->item_count; i++) {
		char* val = extract_named_object(cursor, "name");
		char* name = get_item_name(val);
		free(val);
		ctx->item_names[i] = name;
		DEBUG_MSG("Item %d: %s\n", i, name);

		cursor = strstr(cursor, "\"name\":") + 2;
	}
}

// private
static int init_rolls(const char* pool_data, LootPool* pool)
{
	char* rolls_field = extract_named_object(pool_data, "rolls"); // 1
	if (rolls_field == NULL)
		return -1;

	int minrolls = extract_int(rolls_field, "min", -1);
	int maxrolls = extract_int(rolls_field, "max", -1);
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
	char* name_field = extract_named_object(entry_data, "name"); // 1
	if (name_field == NULL) {
		*item_id = -1; // empty entry
		return; 
	}

	char* iname = get_item_name(name_field); // 2
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

	char* functions_field = extract_named_object(entry_data, "functions"); // 1
	if (functions_field != NULL) {
		functions = count_unnamed(functions_field);
		free(functions_field); // 0
	}

	int w = extract_int(entry_data, "weight", 1);
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
	char* functions_field = extract_named_object(entry_data, "functions"); // 1
	if (functions_field == NULL)
		return 0;
	
	int findex = pool->entry_functions_index[entry_id];
	int fcount = pool->entry_functions_count[entry_id];

	for (int i = 0; i < fcount; i++) {
		int index_in_array = findex + i;
		LootFunction* loot_function = &(pool->loot_functions[index_in_array]);

		char* function_data = extract_unnamed_object(functions_field, i); // 2
		char* function_name = extract_named_object(function_data, "function"); // 3

		if (strcmp(function_name, "\"minecraft:set_count\"") == 0) {
			parse_set_count(loot_function, function_data);
		}
		else if (strcmp(function_name, "\"minecraft:enchant_with_levels\"") == 0) {
			// TODO
			// parse_enchant_with_levels(loot_function, function_data);
			create_no_op(loot_function);
		}
		else if (strcmp(function_name, "\"minecraft:enchant_randomly\"") == 0) {
			// TODO
			// parse_enchant_randomly(loot_function, function_data);
			create_no_op(loot_function);
		}
		else if (strcmp(function_name, "\"minecraft:set_damage\"") == 0) {
			// inline parsing here, it's a very simple function
			create_set_damage(loot_function);
		}

		free(function_data); // 2
		free(function_name); // 1
	}

	free(functions_field); // 0
	return 0;
}

// private
static void precompute_loot_pool(LootPool* pool, const char* entries_field)
{
	int index = 0;

	for (int i = 0; i < pool->entry_count; i++) {
		char* entry_data = extract_unnamed_object(entries_field, i); // 1
		int entry_weight = extract_int(entry_data, "weight", 1);

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
	DEBUG_MSG("Initializing loot pool %d\n", pool_id);

	LootPool* pool = &(ctx->loot_pools[pool_id]);
	pool->total_weight = 0;

	int ret = init_rolls(pool_data, pool);
	if (ret != 0)
		ERR("Loot pool %d does not declare a roll choice function", pool_id);

	// count entries inside loot table

	char* entries_field = extract_named_object(pool_data, "entries"); // 1
	if (entries_field == NULL)
		ERR("Loot pool %d does not declare any entries", pool_id);
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
		DEBUG_MSG("POOL %d:  ENTRY %d\n", pool_id, i);
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
		DEBUG_MSG("Total weight: %d\n", pool->total_weight);
		ERR("Could not allocate memory for precomputed loot table\n");
	}
	
	DEBUG_MSG("Precomputing pool %d ->\n", pool_id);
	precompute_loot_pool(pool, entries_field);
	DEBUG_MSG("-> done, total weight = %d\n\n", pool->total_weight);

	free(entries_field); // 0

	return 0;
}

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

// private
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
// the public API

int init_loot_table(const char* filename, LootTableContext* context, const MCVersion version)
{
	DEBUG_MSG("Parsing loot table %s\n", filename);

	context->version = version;
	if (version == undefined)
		ERR("Can't parse loot table for undefined version");

	// open the file, read the content, close the file.

	FILE* fptr = fopen(filename, "r");
	if (fptr == NULL)
		ERR("Could not open file");

	fseek(fptr, 0, SEEK_END);
	size_t file_size = ftell(fptr);	// get the size of the file
	fseek(fptr, 0, SEEK_SET);		// go back to the beginning of the file

	char* file_content = (char*)malloc(file_size + 1); // 1
	if (file_content == NULL) {
		fclose(fptr);
		ERR("Could not allocate memory for file content");
	}

	// read the file content
	int read = fread(file_content, 1, file_size, fptr);
	if (read != file_size) {
		fclose(fptr);
		free(file_content);
		ERR("Error while reading file content");
	}

	file_content[file_size] = '\0';
	fclose(fptr);

	char* loot_table_string = get_no_whitespace_string(file_content); // 2
	free(file_content); // 1

	// ----------------------------------------------

	DEBUG_MSG("Initializing item name array ---\n");

	init_loot_table_items(loot_table_string, context);

	DEBUG_MSG("--- done!\n");

	// ----------------------------------------------

	DEBUG_MSG("Initializing loot pools...\n");

	char* pools_field = extract_named_object(loot_table_string, "pools"); // 2
	if (pools_field == NULL)
		ERR("Loot table does not declare any pools");

	int pool_count = count_unnamed(pools_field);
	context->pool_count = pool_count;
	context->loot_pools = (LootPool*)malloc(pool_count * sizeof(LootPool));

	DEBUG_MSG("Creating %d loot pools\n", context->pool_count);

	for (int pool_id = 0; pool_id < pool_count; pool_id++) {
		char* pool_data = extract_unnamed_object(pools_field, pool_id); // 3
		//DEBUG_MSG("%s\n\n", pools_field);

		int ret = init_loot_pool(pool_data, pool_id, context);
		if (ret != 0)
			ERR("Error while initializing loot pool #%d", pool_id);

		free(pool_data); // 2
	}

	DEBUG_MSG("All loot pools were parsed succesfully\n");

	free(pools_field); // 1
	free(loot_table_string); // 0
	return 0;
}

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

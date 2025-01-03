#include "loot_table_parser.h"
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

static int count_pools(const char* table_string)
{
	int pool_count = 0;
	int depth = 0;
	int len = strlen(table_string);

	for (int i = 0; i < len; i++)
	{
		if (table_string[i] == '{')
		{
			if (depth == 1)
				pool_count++;
			depth++;
		}
		else if (table_string[i] == '}')
		{
			depth--;
		}
	}
	return pool_count;
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
	DEBUG_MSG("extract_unnamed_object got string with len = %d\n", len);

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

	DEBUG_MSG("indexBegin: %d,  indexEnd: %d\n", indexBegin, indexEnd);

	// extract the substring from indexBegin to indexEnd
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
	int depth = 0;

	// find the number of characters in the object
	while (depth >= 0) {
		length++;
		const char c = keyPos[length];
		if (c == incDepth) depth++;
		if (c == decDepth) depth--;

		// handle cases where it's just a key-value pair
		if (c == ',' || (incDepth == 0 && (c == '}' || c == ']'))) {
			length--;
			break;
		}
	}

	// extract the substring
	char* extracted = substr(keyPos, 0, length);
	return extracted;
}

static char* get_item_name(const char* value)
{
	// remove quotes and "minecraft:" prefix
	char* name = substr(value, 1 + 10, strlen(value) - 2);
	return name;
}

// ----------------------------------------------

// private
static void init_loot_table_items(const char* loot_table_string, LootTableContext* ctx)
{
	char* cursor = loot_table_string;

	// firstly, count items
	ctx->item_count = 0;
	while (1) {
		cursor = strstr(cursor, "\"name\"");
		if (cursor == NULL) break;
		ctx->item_count++;
		cursor++; // won't find the previous key now
	}

	// allocate memory for item names
	ctx->item_names = (char**)malloc(ctx->item_count * sizeof(char*));
	
	// fill the array
	for (int i = 0; i < ctx->item_count; i++) {
		char* val = extract_named_object(cursor, "name");
		char* name = get_item_name(val);
		free(val);
		ctx->item_names[i] = name;
		cursor = strstr(cursor, "\"name\"") + 1;
	}
}

// private
static int init_loot_pool(const char* pool_data, const int pool_id, LootTableContext* ctx)
{
	LootPool* pool = &(ctx->loot_pools[pool_id]);

	// this needs to do the heavy lifting:
	// - create entry arrays, containing all the parsed loot functions
	// - create precomputed loot tables

	// create roll count function

	char* rolls_field = extract_named_object(pool_data, "rolls"); // 1
	if (rolls_field == NULL)
		ERR("Loot pool #%d does not declare a roll choice function", pool_id);

	DEBUG_MSG("POOL #%d:  rolls = %s\n", pool_id, rolls_field);

	char* minrolls = extract_named_object(rolls_field, "min"); // 2
	char* maxrolls = extract_named_object(rolls_field, "max"); // 3
	if (minrolls != NULL && maxrolls != NULL) {
		// uniform roll
		int minrolls_i = atoi(minrolls);
		int maxrolls_i = atoi(maxrolls);
		free(minrolls); // 2
		free(maxrolls); // 1

		pool->min_rolls = minrolls_i;
		pool->max_rolls = maxrolls_i;
		// the rng doesn't step forward if the uniform roll is between two equal values
		pool->roll_count_function = maxrolls_i != minrolls_i ? roll_count_uniform : roll_count_constant;
		DEBUG_MSG("POOL #%d:  roll count function = uniform (%d, %d)\n", pool_id, minrolls_i, maxrolls_i);
	}
	else {
		// constant roll
		int rolls_i = atoi(rolls_field);

		pool->min_rolls = rolls_i;
		pool->max_rolls = rolls_i;
		pool->roll_count_function = roll_count_constant;
		DEBUG_MSG("POOL #%d:  roll count function = constant (%d)\n", pool_id, rolls_i);
	}

	free(rolls_field); // 0

	// count entries inside loot table

	char* entries_field = extract_named_object(pool_data, "entries"); // 1
	if (entries_field == NULL)
		ERR("Loot pool #%d does not declare any entries", pool_id);
	free(entries_field); // 0
}

int init_loot_table(const char* filename, LootTableContext* context, const MCVersion version)
{
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

	init_loot_table_items(loot_table_string, context);

	int pool_count = count_pools(loot_table_string);
	context->pool_count = pool_count;
	context->loot_pools = (LootPool*)malloc(pool_count * sizeof(LootPool));

	char* pools_field = extract_named_object(loot_table_string, "pools"); // 2

	for (int pool_id = 0; pool_id < pool_count; pool_id++) {
		char* pool_data = extract_unnamed_object(pools_field, pool_id); // 3
		DEBUG_MSG("%s\n\n", pool_data);

		init_loot_pool(pool_data, pool_id, context);
		free(pool_data); // 2
	}

	free(pools_field); // 1
	free(loot_table_string); // 0
	return 0;
}

// -------------------------------------------------------------------------------------

// private
static void free_loot_pool(LootPool* pool)
{
	free(pool->precomputed_loot);
	free(pool->entry_functions_index);
	free(pool->entry_functions_count);
	free(pool->loot_function_array);
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

#include "loot_table_parser.h"

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
	if (start != -1) return NULL;
	char* ret_str = (char*)malloc((size_t)end - start + 1);
	if (ret_str == NULL) return NULL;
	strncpy(ret_str, str + start, (size_t)end - start);
	ret_str[end - start] = '\0';
	return ret_str;
}

static char* extract_unnamed_object(const char* jsonstr, int index) {
	// find the index-th unnamed object in the json string,
	// return it as a new string.

	int list_depth = 0;
	int obj_depth = 0;
	int len = strlen(jsonstr);

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
	}

	// extract the substring
	char* extracted = substr(keyPos, 0, length);
	return extracted;
}


// ----------------------------------------------

int parse_loot_table(const char* filename, LootTableContext* context)
{
	// open the file, read the content, close the file.

	FILE* fptr = fopen(filename, "r");
	if (fptr == NULL)
		ERR("Could not open file");

	fseek(fptr, 0, SEEK_END);
	size_t file_size = ftell(fptr);	// get the size of the file
	fseek(fptr, 0, SEEK_SET);		// go back to the beginning of the file

	char* file_content = (char*)malloc(file_size + 1);
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

	char* loot_table_string = get_no_whitespace_string(file_content);
	free(file_content);

	// ----------------------------------------------
    // what we need from the file:
	// DONE - how many loot pools there are (count opening brackets at depth = 1)
	// - for each loot pool, create the roll choice function

	int pool_count = count_pools(loot_table_string);

	for (int pool_id = 0; pool_id < pool_count; pool_id++) {
		char* pool_data = extract_pool(loot_table_string, pool_id);


		free(pool_data);
	}
		
	return 0;
}

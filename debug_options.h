#ifndef _DEBUG_OPTIONS_H
#define _DEBUG_OPTIONS_H

#include <stdio.h>

#define ENABLE_DEBUG_MESSAGES 1

#define DEBUG_MSG(...) { \
	if (ENABLE_DEBUG_MESSAGES) { \
		printf(__VA_ARGS__); \
	} \
}

#endif
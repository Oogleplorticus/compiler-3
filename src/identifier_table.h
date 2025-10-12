#pragma once

#include <stddef.h>

typedef struct {
	size_t ID;
	const char* name; //not null terminated
	size_t name_length;
} Identifier;

void resetIdentifierTable(); //call before other functions
void freeIdentifierTable();

//creates a copy, no need to keep identifier_name alive
Identifier getOrAddIdentifier(const char* identifier_name, size_t identifier_name_length);

Identifier getIdentifierFromID(size_t ID);

void printIdentifierTable();
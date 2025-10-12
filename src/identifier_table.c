#include "identifier_table.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Identifier* identifier_table = NULL;
static size_t identifier_table_length = 0;
static size_t identifier_table_capacity = 0;

static void expandTable() {
	identifier_table_capacity *= 2;
	Identifier* new_table = realloc(identifier_table, identifier_table_capacity);
	if (new_table == NULL) {
		printf("ERROR: Failed to reallocate memory for identifier table!\n");
		freeIdentifierTable();
		exit(1);
	}
	identifier_table = new_table;
}

void resetIdentifierTable() {
	//free if already exists
	if (identifier_table != NULL) {
		freeIdentifierTable();
	}

	//set capacity/length
	identifier_table_length = 0;
	identifier_table_capacity = 32; //32 seems like a good start right?

	//allocate initial space
	Identifier* new_table = malloc(identifier_table_capacity * sizeof(Identifier));
	if (new_table == NULL) {
		printf("ERROR: Failed to allocate memory for identifier table!\n");
		exit(1);
	}
	identifier_table = new_table;
}

void freeIdentifierTable() {
	for (size_t i = 0; i < identifier_table_length; ++i) {
		free((void*)identifier_table[i].name);
	}
	free(identifier_table);
	identifier_table_length = 0;
	identifier_table_capacity = 0;
}

Identifier getOrAddIdentifier(const char* identifier_name, size_t identifier_name_length) {
	for (size_t i = 0; i < identifier_table_length; ++i) {
		if (identifier_name_length != identifier_table[i].name_length) continue;

		if (memcmp(identifier_table[i].name, identifier_name, identifier_name_length) == 0) {
			return identifier_table[i];
		}
	}
	//if not in table add
	if (identifier_table_length >= identifier_table_capacity) expandTable(); //expand if needed
	identifier_table[identifier_table_length].ID = identifier_table_length;
	identifier_table[identifier_table_length].name_length = identifier_name_length;
	identifier_table[identifier_table_length].name = malloc(identifier_name_length * sizeof(char));
	if (identifier_table[identifier_table_length].name == NULL) {
		printf("ERROR: Failed to allocate memory for identifier name in identifier table!\n");
		exit(1);
	}
	memccpy((void*)identifier_table[identifier_table_length].name, identifier_name, sizeof(char), identifier_name_length);

	++identifier_table_length;
	return identifier_table[identifier_table_length - 1];
}

Identifier getIdentifierFromID(size_t ID) {
	if (ID >= identifier_table_length) {
		printf("ERROR: Attempted to access nonexistent identifier %zu in identifier table!\n", ID);
		exit(1);
	}
	return identifier_table[ID];
}

void printIdentifierTable() {
	for (size_t i = 0; i < identifier_table_length; ++i) {
		printf("%zu: ID %zu, %.*s\n", i, identifier_table[i].ID, (int)identifier_table[i].name_length, identifier_table[i].name);
	}
}
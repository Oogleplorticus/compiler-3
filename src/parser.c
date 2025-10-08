#include "parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "identifier_table.h"
#include "token.h"
#include "tokeniser.h"

//state variables
static size_t scope_depth = 0;

//parser tables
typedef struct {
	size_t ID; //same as index within the function table

	TokenType return_value_type; //should only be assigned the variable type token types or the none token type
	size_t return_value_width;

	size_t identifier_ID; 

	size_t codegen_ID;
} FunctionTableEntry;

typedef struct {
	size_t ID; //same as index within the variable table

	TokenType type; //should only be assigned the variable type token types
	size_t width;

	size_t* identifier_IDs; 
	size_t identifier_ID_count;

	size_t* enclosing_scope_IDs;
	size_t enclosing_scope_ID_count;

	size_t most_recent_codegen_ID;
} VariableTableEntry;

static FunctionTableEntry* function_table = NULL;
static size_t function_table_length = 0;
static size_t function_table_capacity = 0;

static VariableTableEntry* variable_table = NULL;
static size_t variable_table_length = 0;
static size_t variable_table_capacity = 0;

static void initialiseParsingTables() {
	//set capacities and lengths
	function_table_capacity = 16;
	variable_table_capacity = 32;

	function_table_length = 0;
	variable_table_length = 0;

	//allocate memory for tables
	function_table = malloc(function_table_capacity * sizeof(FunctionTableEntry));
	if (function_table == NULL) {
		printf("ERROR: Failed to allocate memory for function table!\n");
		exit(1);
	}
	variable_table = malloc(variable_table_capacity * sizeof(VariableTableEntry));
	if (variable_table == NULL) {
		printf("ERROR: Failed to allocate memory for variable table!\n");
		exit(1);
	}
}

static void freeParsingTables() {
	for (size_t i = 0; i < variable_table_length; ++i) {
		free(variable_table[i].identifier_IDs);
		free(variable_table[i].enclosing_scope_IDs);
	}

	free(function_table);
	free(variable_table);
}

FunctionTableEntry* createFunctionTableEntry() {
	if (function_table_length >= function_table_capacity) {
		function_table_capacity *= 2;
		FunctionTableEntry* new_table = realloc(function_table, function_table_capacity);
		if (new_table == NULL) {
			printf("ERROR: Failed to reallocate memory for function table!\n");
			exit(1);
		}
		function_table = new_table;
	}

	//initialise data
	FunctionTableEntry* entry = function_table + function_table_length;
	memset(entry, 0, sizeof(FunctionTableEntry));
	entry->ID = function_table_length;

	++function_table_length;
	return entry;
}

VariableTableEntry* createVariableTableEntry(size_t identifier_ID_count, size_t enclosing_scope_ID_count) {
	if (variable_table_length >= variable_table_capacity) {
		variable_table_capacity *= 2;
		VariableTableEntry* new_table = realloc(variable_table, variable_table_capacity);
		if (new_table == NULL) {
			printf("ERROR: Failed to allocate memory for variable table!\n");
			exit(1);
		}
		variable_table = new_table;
	}

	//initialise data
	VariableTableEntry* entry = variable_table + variable_table_length;
	memset(entry, 0, sizeof(VariableTableEntry));
	entry->ID = variable_table_length;

	entry->identifier_ID_count = identifier_ID_count;
	entry->enclosing_scope_ID_count = enclosing_scope_ID_count;
	//identifier_IDs and enclosing_scope_IDs are contiguous in memory
	entry->identifier_IDs = malloc((entry->identifier_ID_count + entry->enclosing_scope_ID_count) * sizeof(size_t));
	if (entry->identifier_IDs == NULL) {
		printf("ERROR: Failed to allocate memory for variable table entry data!\n");
		exit(1);
	}
	entry->enclosing_scope_IDs = entry->identifier_IDs + entry->identifier_ID_count;

	++variable_table_length;
	return entry;
}

FunctionTableEntry* findInFunctionTable(size_t identifier_ID) {
	for (size_t i = 0; i < function_table_length; ++i) {
		if (function_table[i].identifier_ID != identifier_ID) continue;

		return function_table + i;
	}

	return NULL;
}

VariableTableEntry* findInVariableTable(size_t* identifier_IDs, size_t identifier_ID_count, size_t* enclosing_scope_IDs, size_t enclosing_scope_ID_count) {
	for (size_t i = 0; i < variable_table_length; ++i) {
		if (variable_table[i].identifier_ID_count != identifier_ID_count) continue;
		if (variable_table[i].enclosing_scope_ID_count != enclosing_scope_ID_count) continue;
		for (size_t i = 0; i < identifier_ID_count; ++i) {
			if (identifier_IDs[i] != variable_table[i].identifier_IDs[i]) continue;
		}
		for (size_t i = 0; i < enclosing_scope_ID_count; ++i) {
			if (enclosing_scope_IDs[i] != variable_table[i].enclosing_scope_IDs[i]) continue;
		}

		return variable_table + i;
	}

	return NULL;
}

//parser logic
static void unexpectedToken(Token token) {
	printf("ERROR: Unexpected token ");
	printToken(token);
	printf("!\n");
	exit(1);
}

//starts on first token of expression
//ends on token proceding expression
static void parseExpression(TokenType previous_operator) {
	//TODO implement expression parsing
	//currently just skippin the expression
	while (currentToken().type != TOKEN_SEMICOLON && currentToken().type != TOKEN_COMMA) {
		incrementToken();
	}
}

//starts on variable identifier
//ends on first token of next statement
static void parseVariableDefinition() {
	//ensure correct tokens
	if (currentToken().type != TOKEN_IDENTIFIER) {
		unexpectedToken(currentToken());
	}
	if (nextToken().type != TOKEN_COLON) {
		unexpectedToken(nextToken());
	}

	incrementToken();
	incrementToken();

	//TODO handle type and tags
	//skip for now
	while (currentToken().type != TOKEN_SEMICOLON && currentToken().type != TOKEN_EQUAL) {
		incrementToken();
	}

	switch (currentToken().type) {
		case TOKEN_SEMICOLON: incrementToken(); return; //declaration only

		case TOKEN_EQUAL:
		incrementToken();
		parseExpression(TOKEN_EQUAL);
		incrementToken();
		return;

		default: unexpectedToken(currentToken());
	}
}

//starts on first parameter identifier or closing parenthesis
//ends on first token of next statement
static void parseFunctionDefinition(size_t identifier_ID) {
	if (scope_depth > 0) {
		printf("ERROR: attempted to define function in invalid scope!\n");
		unexpectedToken(currentToken());
	}

	//create function table entry
	FunctionTableEntry* function_table_entry = createFunctionTableEntry();
	function_table_entry->identifier_ID = identifier_ID;

	//check if main
	bool is_main = false;
	Identifier identifier = getIdentifierFromID(identifier_ID);
	if (identifier.name_length == sizeof("main") - sizeof(char)) {
		int comparison_result = memcmp(identifier.name, "main", identifier.name_length);
		if (comparison_result == 0) is_main = true;
	}

	//emit code
	if (is_main) {
		function_table_entry->codegen_ID = codegenCreateEntryFunction();
	} else {
		function_table_entry->codegen_ID = codegenCreateFunction(0);
	}

	//TODO handle parameters

	//ensure correct tokens
	if (currentToken().type != TOKEN_PARENTHESIS_RIGHT) {
		unexpectedToken(currentToken());
	}
	if (nextToken().type != TOKEN_COLON) {
		unexpectedToken(nextToken());
	}

	incrementToken();
	incrementToken();

	//TODO handle return value and tags

	//ensure correct token
	if (currentToken().type != TOKEN_BRACE_LEFT) {
		unexpectedToken(currentToken());
	}
	incrementToken();
	++scope_depth;
}

//starts on function identifier
//ends on first token of next statement
static void parseFunction() {
	//ensure correct token
	if (currentToken().type != TOKEN_IDENTIFIER) {
		unexpectedToken(currentToken());
	}

	size_t identifier_ID = currentToken().data.identifier_ID;

	incrementToken();
	incrementToken();

	//ensure no unexpected tokens
	switch (currentToken().type) {
		case TOKEN_IDENTIFIER:
		case TOKEN_PARENTHESIS_RIGHT:
		break;

		default: unexpectedToken(currentToken());
	}

	if (nextToken().type == TOKEN_COLON) {
		parseFunctionDefinition(identifier_ID);
		return;
	}

	//TODO handle function calls
}

//starts on first token of statement
//ends on first token of next statement
static void parseIdentifier() {
	switch (nextToken().type) {
		case TOKEN_COLON: parseVariableDefinition(); return;
		case TOKEN_PARENTHESIS_LEFT: parseFunction(); return;

		//expression
		case TOKEN_EQUAL:
		case TOKEN_PLUS_EQUAL:
		case TOKEN_MINUS_EQUAL:
		case TOKEN_STAR_EQUAL:
		case TOKEN_FORWARD_SLASH_EQUAL:
		case TOKEN_PERCENT_EQUAL:
		case TOKEN_AMPERSAND_EQUAL:
		case TOKEN_BAR_EQUAL:
		case TOKEN_CARET_EQUAL:
		case TOKEN_TILDE_TILDE:
		case TOKEN_LESS_LESS_EQUAL:
		case TOKEN_GREATER_GREATER_EQUAL:
		parseExpression(TOKEN_NONE);
		incrementToken();
		return;
		
		default: unexpectedToken(nextToken());
	}
}

//starts on the while keyword
//ends on first token of next statement
static void parseWhile() {
	//ensure correct token
	if (currentToken().type != TOKEN_WHILE) {
		unexpectedToken(currentToken());
	}
	incrementToken();

	parseExpression(TOKEN_NONE);

	incrementToken();
	++scope_depth;
}

//starts on first token of statement
//ends on first token of next statement
static void parseStatement() {
	switch (currentToken().type) {
		case TOKEN_IDENTIFIER: parseIdentifier(); return;

		case TOKEN_WHILE: parseWhile(); return;

		//close scope
		case TOKEN_BRACE_RIGHT:
		--scope_depth;
		incrementToken();
		return;

		default: unexpectedToken(currentToken());
	}
}

void parseTokens() {
	//setup
	initialiseParsingTables();

	scope_depth = 0;

	while(currentToken().type != TOKEN_EOF) {
		parseStatement();
	}

	//test for errors
	if (scope_depth > 0) {
		printf("ERROR: Scope depth did not return to 0, potential missing brace!\n");
		exit(1);
	}

	//free memory
	freeParsingTables();
}
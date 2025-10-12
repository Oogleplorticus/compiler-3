#include "parser.h"

#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>

#include "identifier_table.h"
#include "llvm_data.h"
#include "token.h"
#include "tokeniser.h"

//state variables
static size_t scope_depth = 0;
//if you have over 128 nested scopes you should delete your codebase
//this is also very lazy code though
//index 0 is always the function ID
#define SCOPE_STACK_LENGTH 128
#define FUNCTION_SCOPE_INDEX 0
static struct {
	size_t ID;
	TokenType block_type; //very lazy
	LLVMValueRef condition_result;
	LLVMBasicBlockRef condition_block;
	LLVMBasicBlockRef body_block;
} scope_stack[SCOPE_STACK_LENGTH];

//parser tables
typedef struct {
	size_t ID; //same as index within the function table

	size_t identifier_ID; 

	LLVMValueRef llvm_ID;
	LLVMBasicBlockRef entry_block;
} FunctionTableEntry;

typedef struct {
	size_t ID; //same as index within the variable table

	LLVMTypeRef llvm_type;

	size_t* identifier_IDs; 
	size_t identifier_ID_count;

	//first scope ID is the function ID, then the index of the scope ordered top to bottom
	size_t* enclosing_scope_IDs;
	size_t enclosing_scope_ID_count;

	LLVMValueRef llvm_stack_pointer;
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
		//enclosing scope ids doesnt need to be freed as it is contiguous with identifier ids
		free(variable_table[i].identifier_IDs); 
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
	entry->enclosing_scope_IDs = &entry->identifier_IDs[entry->identifier_ID_count];

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
	//search through table backwards so that variables in deeper scopes are prioritised
	//they will have been declared later
	for (size_t i = variable_table_length - 1; i < (size_t)-1; --i) {
		if (variable_table[i].identifier_ID_count != identifier_ID_count) continue;
		if (variable_table[i].enclosing_scope_ID_count > enclosing_scope_ID_count) continue;

		bool match = true;
		for (size_t identifier = 0; identifier < identifier_ID_count; ++identifier) {
			if (identifier_IDs[identifier] != variable_table[i].identifier_IDs[identifier]) match = false;
		}
		for (size_t scope = 0; scope < variable_table[i].enclosing_scope_ID_count; ++scope) {
			if (enclosing_scope_IDs[scope] != variable_table[i].enclosing_scope_IDs[scope]) match = false;
		}

		if (match) return variable_table + i;
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

//larger number = greater precedence
static size_t OperatorPrecedence(TokenType operator_type) {
	switch (operator_type) {
		case TOKEN_NONE: return 0;
	
		case TOKEN_EQUAL: return 1;
	
		case TOKEN_PLUS: return 10;
		case TOKEN_MINUS: return 10;
		case TOKEN_STAR: return 11;
		case TOKEN_FORWARD_SLASH: return 11;
		case TOKEN_PERCENT: return 11;
	
		case TOKEN_AMPERSAND: return 6;
		case TOKEN_BAR: return 4;
		case TOKEN_CARET: return 5;
		case TOKEN_TILDE: return 12;
		case TOKEN_LESS_LESS: return 9;
		case TOKEN_GREATER_GREATER: return 9;
	
		case TOKEN_PLUS_EQUAL: return 1;
		case TOKEN_MINUS_EQUAL: return 1;
		case TOKEN_STAR_EQUAL: return 1;
		case TOKEN_FORWARD_SLASH_EQUAL: return 1;
		case TOKEN_PERCENT_EQUAL: return 1;
	
		case TOKEN_AMPERSAND_EQUAL: return 1;
		case TOKEN_BAR_EQUAL: return 1;
		case TOKEN_CARET_EQUAL: return 1;
		case TOKEN_TILDE_TILDE: return 1;
		case TOKEN_LESS_LESS_EQUAL: return 1;
		case TOKEN_GREATER_GREATER_EQUAL: return 1;
	
		case TOKEN_EQUAL_EQUAL: return 7;
		case TOKEN_EXCLAMATION_EQUAL: return 7;
		case TOKEN_LESS: return 8;
		case TOKEN_GREATER: return 8;
		case TOKEN_LESS_EQUAL: return 8;
		case TOKEN_GREATER_EQUAL: return 8;

		default:
		printf("ERROR: Tried to get precedence of unsupported operator %d!\n", operator_type);
		exit(1);
	}
}

//currently assumes int
static LLVMValueRef emitBinaryOperation(TokenType operator_type, LLVMValueRef left, LLVMValueRef right) {
	switch (operator_type) {
		case TOKEN_PLUS:
		return LLVMBuildAdd(llvm_builder, left, right, "");
		case TOKEN_MINUS:
		return LLVMBuildSub(llvm_builder, left, right, "");
		case TOKEN_STAR:
		return LLVMBuildMul(llvm_builder, left, right, "");
		case TOKEN_FORWARD_SLASH:
		return LLVMBuildSDiv(llvm_builder, left, right, "");
		case TOKEN_PERCENT:
		return LLVMBuildSRem(llvm_builder, left, right, "");
	
		case TOKEN_AMPERSAND:
		return LLVMBuildAnd(llvm_builder, left, right, "");
		case TOKEN_BAR:
		return LLVMBuildOr(llvm_builder, left, right, "");
		case TOKEN_CARET:
		return LLVMBuildXor(llvm_builder, left, right, "");
		case TOKEN_LESS_LESS:
		return LLVMBuildShl(llvm_builder, left, right, "");
		case TOKEN_GREATER_GREATER:
		return LLVMBuildLShr(llvm_builder, left, right, "");
	
		case TOKEN_EQUAL_EQUAL:
		return LLVMBuildICmp(llvm_builder, LLVMIntEQ, left, right, "");
		case TOKEN_EXCLAMATION_EQUAL:
		return LLVMBuildICmp(llvm_builder, LLVMIntNE, left, right, "");
		case TOKEN_LESS:
		return LLVMBuildICmp(llvm_builder, LLVMIntSLT, left, right, "");
		case TOKEN_GREATER:
		return LLVMBuildICmp(llvm_builder, LLVMIntSGT, left, right, "");
		case TOKEN_LESS_EQUAL:
		return LLVMBuildICmp(llvm_builder, LLVMIntSLE, left, right, "");
		case TOKEN_GREATER_EQUAL:
		return LLVMBuildICmp(llvm_builder, LLVMIntSGE, left, right, "");

		default:
		printf("ERROR: Tried to emit unsupported binary operator %d!\n", operator_type);
		exit(1);
	}
}

static void emitAssignmentOperation(TokenType operator_type, VariableTableEntry* variable, LLVMValueRef to, LLVMValueRef from) {
	LLVMValueRef result;
	switch (operator_type) {
		case TOKEN_EQUAL:
		LLVMBuildStore(llvm_builder, from, variable->llvm_stack_pointer);
		return;
		
		case TOKEN_PLUS_EQUAL:
		result = LLVMBuildAdd(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		case TOKEN_MINUS_EQUAL:
		result = LLVMBuildSub(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		case TOKEN_STAR_EQUAL:
		result = LLVMBuildMul(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		case TOKEN_FORWARD_SLASH_EQUAL:
		result = LLVMBuildSDiv(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		case TOKEN_PERCENT_EQUAL:
		result = LLVMBuildSRem(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		
		case TOKEN_AMPERSAND_EQUAL:
		result = LLVMBuildAnd(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		case TOKEN_BAR_EQUAL:
		result = LLVMBuildOr(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		case TOKEN_CARET_EQUAL:
		result = LLVMBuildXor(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		case TOKEN_TILDE_TILDE:;
		LLVMValueRef llvm_negative_one = LLVMConstInt(LLVMIntTypeInContext(llvm_context, 64), -1, true);
		result = LLVMBuildXor(llvm_builder, to, llvm_negative_one, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		case TOKEN_LESS_LESS_EQUAL:
		result = LLVMBuildShl(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;
		case TOKEN_GREATER_GREATER_EQUAL:
		result = LLVMBuildLShr(llvm_builder, to, from, "");
		LLVMBuildStore(llvm_builder, result, variable->llvm_stack_pointer);
		return;

		default:
		printf("ERROR: Tried to emit unsupported assignment operator %d!\n", operator_type);
		exit(1);
	}
}

static VariableTableEntry* parseVariable() {
	VariableTableEntry* variable = NULL;

	//get enclosing scopes
	size_t enclosing_scope_IDs[SCOPE_STACK_LENGTH];
	for (size_t i = 0; i < scope_depth; ++i) {
		enclosing_scope_IDs[i] = scope_stack[i].ID;
	}

	//get identifier IDs
	size_t identifier_ID = currentToken().data.identifier_ID;
	incrementToken();

	//find
	variable = findInVariableTable(
		&identifier_ID,
		1,
		enclosing_scope_IDs,
		scope_depth
	);

	return variable;
}

//starts on the first token of the operand
//ends on the token after the operand
static LLVMValueRef getExpressionOperand(VariableTableEntry* variable) {
	LLVMValueRef value;

	//if variable
	if (variable != NULL) {
		value = LLVMBuildLoad2(llvm_builder, variable->llvm_type, variable->llvm_stack_pointer, "");
		return value;
	}

	//if not variable
	switch (currentToken().type) {
		case TOKEN_INTEGER_LITERAL:
		value = LLVMConstInt(LLVMIntTypeInContext(llvm_context, 64), currentToken().data.integer, false);
		incrementToken();
		return value;

		default: unexpectedToken(currentToken());
	}

	//should be unreachable
	return LLVMConstInt(LLVMIntTypeInContext(llvm_context, 32), 0, false);
}

//starts on first token of expression
//ends on token proceding expression
static LLVMValueRef parseExpression(TokenType previous_operator_type) {
	//get variable if variable
	VariableTableEntry* variable = NULL;
	if (currentToken().type == TOKEN_IDENTIFIER && nextToken().type != TOKEN_PARENTHESIS_LEFT) {
		variable = parseVariable();
		if (variable == NULL) {
			printf("ERROR: Failed to find variable preceeding ");
			printToken(currentToken());
			printf("\n");
			exit(1);
		}
	}

	LLVMValueRef left = getExpressionOperand(variable);

	while (currentToken().type != TOKEN_SEMICOLON && currentToken().type != TOKEN_BRACE_LEFT) {
		TokenType current_operator_type = currentToken().type;
		
		//if we drop down in operator precedence, return back up until operator precence equal/greater
		if (OperatorPrecedence(current_operator_type) < OperatorPrecedence(previous_operator_type)) {
			return left;
		}

		//continue parsing
		incrementToken();
		LLVMValueRef right = parseExpression(current_operator_type);

		//once returned emit code
		switch (current_operator_type) {
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
			if (variable == NULL) {
				printf("ERROR: Tried to assign to non-variable value\n");
				exit(1);
			}
			emitAssignmentOperation(current_operator_type, variable, left, right);
			break;
			
			case TOKEN_PLUS:
			case TOKEN_MINUS:
			case TOKEN_STAR:
			case TOKEN_FORWARD_SLASH:
			case TOKEN_PERCENT:
			case TOKEN_AMPERSAND:
			case TOKEN_BAR:
			case TOKEN_CARET:
			case TOKEN_LESS_LESS:
			case TOKEN_GREATER_GREATER:
			case TOKEN_EQUAL_EQUAL:
			case TOKEN_EXCLAMATION_EQUAL:
			case TOKEN_LESS:
			case TOKEN_GREATER:
			case TOKEN_LESS_EQUAL:
			case TOKEN_GREATER_EQUAL:
			left = emitBinaryOperation(current_operator_type, left, right);
			break;

			default:
			printf("ERROR: Tried to emit unsupported operator %d!\n", current_operator_type);
			exit(1);
		}
	}

	return left;
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

	//get identifier
	Identifier identifier = getIdentifierFromID(currentToken().data.identifier_ID);

	//increment to type and tags
	incrementToken();
	incrementToken();

	//TODO handle type and tags
	LLVMTypeRef llvm_type = LLVMIntTypeInContext(llvm_context, 64); //assume i64
	//skip for now
	while (currentToken().type != TOKEN_SEMICOLON && currentToken().type != TOKEN_EQUAL) {
		incrementToken();
	}

	//create variable table entry
	VariableTableEntry* variable = createVariableTableEntry(1, scope_depth);
	variable->identifier_IDs[0] = identifier.ID;
	variable->llvm_type = llvm_type;
	for (size_t i = 0; i < scope_depth; ++i) {
		variable->enclosing_scope_IDs[i] = scope_stack[i].ID;
	}

	//allocate stack memory
	char null_terminated_name[identifier.name_length + 1];
	null_terminated_name[identifier.name_length] = '\0';
	memccpy(null_terminated_name, identifier.name, sizeof(char), identifier.name_length);
	
	LLVMBasicBlockRef current_block = LLVMGetInsertBlock(llvm_builder);
	LLVMBasicBlockRef entry_block = findInFunctionTable(scope_stack[FUNCTION_SCOPE_INDEX].ID)->entry_block;
	LLVMValueRef first_instruction = LLVMGetFirstInstruction(entry_block);
	if (first_instruction != NULL) {
		LLVMPositionBuilderBefore(llvm_builder, first_instruction);
	} else {
		LLVMPositionBuilderAtEnd(llvm_builder, entry_block);
	}
	variable->llvm_stack_pointer = LLVMBuildAlloca(llvm_builder, llvm_type, null_terminated_name);
	LLVMPositionBuilderAtEnd(llvm_builder, current_block);

	switch (currentToken().type) {
		case TOKEN_SEMICOLON: incrementToken(); return; //declaration only

		case TOKEN_EQUAL:
		incrementToken();
		LLVMValueRef assignment_value = parseExpression(TOKEN_EQUAL);
		LLVMBuildStore(llvm_builder, assignment_value, variable->llvm_stack_pointer);
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
	scope_stack[FUNCTION_SCOPE_INDEX].ID = function_table_entry->ID;
	scope_stack[scope_depth + 1].ID = 0;

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

	//emit function
	LLVMTypeRef function_type = LLVMFunctionType(
		LLVMVoidTypeInContext(llvm_context),
		NULL,
		0,
		0
	);

	Identifier identifier = getIdentifierFromID(identifier_ID);
	char null_terminated_name[identifier.name_length + 1];
	null_terminated_name[identifier.name_length] = '\0';
	memccpy(null_terminated_name, identifier.name, sizeof(char), identifier.name_length);

	function_table_entry->llvm_ID = LLVMAddFunction(llvm_module, null_terminated_name, function_type);

	//emit entry block
	LLVMBasicBlockRef entry_block = LLVMAppendBasicBlockInContext(
		llvm_context,
		function_table_entry->llvm_ID,
		"entry"
	);
	function_table_entry->entry_block = entry_block;
	LLVMPositionBuilderAtEnd(llvm_builder, entry_block);

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
	
	//create block for condition
	LLVMBasicBlockRef condition_block = LLVMAppendBasicBlockInContext(
		llvm_context, 
		findInFunctionTable(scope_stack[FUNCTION_SCOPE_INDEX].ID)->llvm_ID, 
		"while_condition"
	);
	//create block for body
	LLVMBasicBlockRef body_block = LLVMAppendBasicBlockInContext(
		llvm_context, 
		findInFunctionTable(scope_stack[FUNCTION_SCOPE_INDEX].ID)->llvm_ID, 
		"while_body"
	);

	//emit jump to condition block
	LLVMBuildBr(llvm_builder, condition_block);

	//parse condition
	LLVMPositionBuilderAtEnd(llvm_builder, condition_block);
	LLVMValueRef condition_result = parseExpression(TOKEN_NONE);

	//add to scope stack
	scope_stack[scope_depth].block_type = TOKEN_WHILE;
	scope_stack[scope_depth].condition_result = condition_result;
	scope_stack[scope_depth].condition_block = condition_block;
	scope_stack[scope_depth].body_block = body_block;
	scope_stack[scope_depth + 1].ID = 0;
	++scope_stack[scope_depth].ID;

	//set builder up in body block
	LLVMPositionBuilderAtEnd(llvm_builder, body_block);

	incrementToken();
	++scope_depth;
}

//starts on the return keyword
static void parseReturn() {
	LLVMBuildRetVoid(llvm_builder);
	//assume void return
	incrementToken();
	incrementToken();
}

static void closeScope() {
	--scope_depth;
	if (scope_depth <= 0) return; //functions dont need closing

	//assume builder is at end of final block
	switch (scope_stack[scope_depth].block_type) {
		case TOKEN_WHILE:
		//end final while block with jump to condition
		LLVMBuildBr(llvm_builder, scope_stack[scope_depth].condition_block);

		//create proceeding block and setup branches for condition block
		LLVMBasicBlockRef next_block = LLVMAppendBasicBlockInContext(
			llvm_context,
			findInFunctionTable(scope_stack[FUNCTION_SCOPE_INDEX].ID)->llvm_ID,
			"after_while"
		);
		LLVMPositionBuilderAtEnd(llvm_builder, scope_stack[scope_depth].condition_block);
		LLVMBuildCondBr(
			llvm_builder,
			scope_stack[scope_depth].condition_result,
			scope_stack[scope_depth].body_block,
			next_block
		);
		LLVMPositionBuilderAtEnd(llvm_builder, next_block);
		return;
		
		default:
		printf("ERROR: Tried to close scope of unknown type %d!\n", scope_stack[scope_depth].block_type);
		unexpectedToken(currentToken());
	}
}

//starts on first token of statement
//ends on first token of next statement
static void parseStatement() {
	switch (currentToken().type) {
		case TOKEN_IDENTIFIER: parseIdentifier(); return;
		case TOKEN_WHILE: parseWhile(); return;
		case TOKEN_RETURN: parseReturn(); return;

		case TOKEN_BRACE_RIGHT:
		closeScope();
		incrementToken();
		return;

		default: unexpectedToken(currentToken());
	}
}

void parseTokens() {
	//setup
	initialiseParsingTables();

	scope_depth = 0;
	memset(scope_stack, 0, sizeof(scope_stack[0]) * 128);
	scope_stack[FUNCTION_SCOPE_INDEX].ID = (size_t)-1;

	while(currentToken().type != TOKEN_EOF) {
		parseStatement();
	}

	//test for errors
	if (scope_depth > 0) {
		printf("ERROR: Scope depth did not return to 0, potential missing brace!\n");
		exit(1);
	}
	
	LLVMVerifyModule(llvm_module, LLVMAbortProcessAction, NULL);

	//free memory
	freeParsingTables();
}
#include "parser_top_level.h"

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "compilation_unit.h"
#include "parser_utils.h"
#include "token.h"
#include "tokeniser.h"

//starts on fn keyword
static void parseFunctionDeclaration(CompilationUnit* compilation_unit) {
	if (currentToken().type == TOKEN_COMMA) incrementToken();

	ASSERT_CURRENT_TOKEN(TOKEN_FN);
	ASSERT_NEXT_TOKEN(TOKEN_IDENTIFIER);
	incrementToken();

	//create function and get identifier
	Function* function = compilationUnit_addFunction(compilation_unit);
	function->identifier_index = compilationUnit_getOrAddIdentifierIndex(compilation_unit, currentToken().data.identifier);

	ASSERT_NEXT_TOKEN(TOKEN_PARENTHESIS_LEFT);
	incrementToken();
	incrementToken();
	
	//handle parameters
	while (currentToken().type != TOKEN_PARENTHESIS_RIGHT) {
		if (currentToken().type == TOKEN_COMMA) incrementToken();

		ASSERT_CURRENT_TOKEN(TOKEN_IDENTIFIER);
		ASSERT_NEXT_TOKEN(TOKEN_COLON);

		//create parameter and assign identifier
		Variable* parameter = compilationUnit_addFunctionParameter(function);
		parameter->identifier_index = compilationUnit_getOrAddIdentifierIndex(compilation_unit, currentToken().data.identifier);
		
		incrementToken();
		incrementToken();

		//assign parameter type
		if (currentToken().type == TOKEN_IDENTIFIER) {
			//TODO support structs
			printf("ERROR: structs not yet supported!\n");
			UNEXPECTED_TOKEN(currentToken());
		} else {
			parameter->type = variableTypeFromToken(currentToken());
		}

		//TODO handle tags
		while (currentToken().type != TOKEN_COMMA && currentToken().type != TOKEN_PARENTHESIS_RIGHT) {
			incrementToken();
		}
	}
	incrementToken();

	//TODO handle tags

	//get return type
	if (currentToken().type == TOKEN_BRACE_LEFT) {
		//no return type
		function->return_type.kind = TYPE_VOID;
		function->return_type.data.width = 0;

	} else if (currentToken().type == TOKEN_MINUS_GREATER) {
		//explicit return type
		//TODO special handling for user defined types
		incrementToken();
		switch (currentToken().type) {
			case TOKEN_INTEGER_TYPE:   function->return_type.kind = TYPE_INT; break;
			case TOKEN_UNSIGNED_TYPE:  function->return_type.kind = TYPE_UNSIGNED; break;
			case TOKEN_FLOAT_TYPE:     function->return_type.kind = TYPE_FLOAT; break;
			case TOKEN_BOOL_TYPE:      function->return_type.kind = TYPE_BOOL; break;
			case TOKEN_CHARACTER_TYPE: function->return_type.kind = TYPE_CHAR; break;

			default: UNEXPECTED_TOKEN(currentToken());
		}
		function->return_type.data.width = currentToken().data.type_width;
		incrementToken();

	} else {
		UNEXPECTED_TOKEN(currentToken());
	}

	//skip function body
	ASSERT_CURRENT_TOKEN(TOKEN_BRACE_LEFT);
	skipScope();
	incrementToken();

	//create llvm function
	function->llvm_function_type = llvmFunctionTypeFromFunction(compilation_unit, function);
	function->llvm_function = LLVMAddFunction(
		compilation_unit->llvm_module,
		compilation_unit->identifiers[function->identifier_index],
		function->llvm_function_type
	);
}

static void parseNonStructs(CompilationUnit* compilation_unit) {
	switch (currentToken().type) {
		case TOKEN_FN:
		parseFunctionDeclaration(compilation_unit);
		return;

		case TOKEN_STRUCT:
		//skip declaration
		while (currentToken().type != TOKEN_BRACE_LEFT) {
			incrementToken();
		}
		//skip definition
		skipScope();
		incrementToken();
		return;

		default: UNEXPECTED_TOKEN(currentToken());
	}
}

//starts on struct keyword
static void parseStructDefinition(CompilationUnit* compilation_unit) {
	(void)compilation_unit;

	ASSERT_CURRENT_TOKEN(TOKEN_STRUCT);
	incrementToken();

	printf("ERROR: Attempted to parse currently unsupported struct!\n");
	exit(1);
}

static void parseStructs(CompilationUnit* compilation_unit) {
	switch (currentToken().type) {
		case TOKEN_STRUCT:
		parseStructDefinition(compilation_unit);
		return;

		case TOKEN_FN:
		//skip declaration
		while (currentToken().type != TOKEN_BRACE_LEFT) {
			incrementToken();
		}
		//skip definition
		skipScope();
		incrementToken();
		return;

		default: UNEXPECTED_TOKEN(currentToken());
	}
}

void parseTopLevel(CompilationUnit* compilation_unit) {
	tokeniserSetSource(compilation_unit->source_file);

	//parse structs first since they can be included as return types and static variable values
	while (currentToken().type != TOKEN_EOF) {
		parseStructs(compilation_unit);
	}

	tokeniserSetSource(compilation_unit->source_file); //reset for next run

	while (currentToken().type != TOKEN_EOF) {
		parseNonStructs(compilation_unit);
	}
}

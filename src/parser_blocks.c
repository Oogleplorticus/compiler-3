#include "parser_blocks.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "compilation_unit.h"
#include "parser_utils.h"
#include "token.h"
#include "tokeniser.h"

static void parseExpression(void) {
	
}

static void parseVariableDeclaration(void) {

}

//starts on fn keyword
static void parseFunctionBody(void) {
	ASSERT_CURRENT_TOKEN(TOKEN_FN);

	//skip declaration
	while (currentToken().type != TOKEN_BRACE_LEFT) {
		incrementToken();
	}
	incrementToken();

	//parse function body
	size_t scope_depth = 0;
	while (currentToken().type != TOKEN_EOF) {
		switch (currentToken().type) {
			case TOKEN_IDENTIFIER:
			if (nextToken().type == TOKEN_SEMICOLON) {
				parseVariableDeclaration();
			} else {
				parseExpression();
			}
			break;
			
			case TOKEN_BRACE_RIGHT:
			//go up a scope
			//if at scope depth 0 (end of function), return
			if (scope_depth <= 0) return;
			--scope_depth;
			break;

			default: UNEXPECTED_TOKEN(currentToken());
		}
	}
}

static void parseBlock(void) {
	switch (currentToken().type) {
		case TOKEN_FN:
		parseFunctionBody();
		return;

		case TOKEN_STRUCT:
		skipStruct();
		return;

		default: UNEXPECTED_TOKEN(currentToken());
	}
}

void parseBlocks(CompilationUnit* compilation_unit) {
	tokeniserSetSource(compilation_unit->source_file);

	while (currentToken().type != TOKEN_EOF) {
		parseBlock();
	}
}

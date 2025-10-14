#include "parser_top_level.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "compilation_unit.h"
#include "parser_utils.h"
#include "token.h"
#include "tokeniser.h"

//starts on opening brace
static void skipScope() {
	if (currentToken().type != TOKEN_BRACE_LEFT) {
		UNEXPECTED_TOKEN(currentToken());
	}
	incrementToken();

	size_t depth = 0;
	while (currentToken().type != TOKEN_EOF) {
		switch (currentToken().type) {
			case TOKEN_BRACE_LEFT:
			++depth;
			break;

			case TOKEN_BRACE_RIGHT:
			if (depth <= 0) return;
			--depth;
			break;

			default: break;
		}

		incrementToken();
	}

	printf("ERROR: Hit EOF while skipping over scope! Ensure all scopes are properly closed!\n");
	exit(1);
}

static void parseFunctionDeclaration(CompilationUnit* compilation_unit) {

}

static void parseStructDefinition(CompilationUnit* compilation_unit) {
	
}

static void parseTopLevelStatement(CompilationUnit* compilation_unit) {
	switch (currentToken().type) {
		case TOKEN_FN:
		parseFunctionDeclaration(compilation_unit);
		return;

		case TOKEN_STRUCT:
		parseStructDefinition(compilation_unit);
		return;

		default: UNEXPECTED_TOKEN(currentToken());
	}
}

void parseTopLevel(CompilationUnit* compilation_unit) {
	tokeniserSetSource(compilation_unit->source_file);

	while (currentToken().type != TOKEN_EOF) {
		parseTopLevelStatement(compilation_unit);
	}
}

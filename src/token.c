#include "token.h"

#include <stdio.h>
#include <string.h>

#include "identifier_table.h"

void printToken(Token token) {
	printf("{type: %i, index: %zu, length: %zu, line: %zu, column: %zu",
			token.type, token.offset_in_source, token.length_in_source, token.line_number, token.column_number);
	
	switch (token.type) {
		case TOKEN_INTEGER_LITERAL: printf(", data: %zu}", token.data.integer); break;
		case TOKEN_REAL_LITERAL: printf(", data: %f}", token.data.real); break;
		case TOKEN_CHARACTER_LITERAL: printf(", data: %c}", token.data.character); break;
		case TOKEN_STRING_LITERAL:
		printf(", data: ");
		for (size_t i = 0; i < token.data.string.length; ++i) {
			putchar(token.data.string.text[i]);
		}
		printf("}");
		break;

		case TOKEN_IDENTIFIER: printf(", identifier ID: %zu, identifier name: ", token.data.identifier_ID);
		Identifier identifier = getIdentifierFromID(token.data.identifier_ID);
		for (size_t i = 0; i < identifier.name_length; ++i) {
			putchar(identifier.name[i]);
		}
		printf("}");
		break;

		default: printf("}"); break;

		//need to add null termination for printf
		
	}
}
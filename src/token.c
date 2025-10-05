#include "token.h"

#include <stdio.h>
#include <string.h>

void printToken(Token token) {
	printf("{type: %i, index: %zu, length: %zu, line: %zu, column: %zu",
			token.type, token.offset_in_source, token.length_in_source, token.line_number, token.column_number);
	
	switch (token.type) {
		case TOKEN_INTEGER_LITERAL: printf(", data: %zu}", token.data.integer); break;
		case TOKEN_REAL_LITERAL: printf(", data: %f}", token.data.real); break;
		case TOKEN_CHARACTER_LITERAL: printf(", data: %c}", token.data.character); break;
		case TOKEN_IDENTIFIER: printf(", identifier ID: %zu}", token.data.identifier_ID); break;
		default: printf("}"); break;

		//need to add null termination for printf
		case TOKEN_STRING_LITERAL:;
		char buffer[token.data.string.length + 1];
		buffer[token.data.string.length] = '\0';
		memccpy(buffer, token.data.string.text, sizeof(char), token.data.string.length);
		printf(", data: %s}", buffer);
		break;
	}
}
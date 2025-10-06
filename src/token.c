#include "token.h"

#include <stdio.h>
#include <string.h>

#include "identifier_table.h"

static const char* tokenTypeToString(TokenType type) {
	switch (type) {
		case TOKEN_NONE: return "TOKEN_UNDETERMINED";
		case TOKEN_EOF: return "TOKEN_EOF";

		case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";

		case TOKEN_IF: return "TOKEN_IF";
		case TOKEN_ELSE: return "TOKEN_ELSE";
		case TOKEN_WHILE: return "TOKEN_WHILE";
		case TOKEN_FOR: return "TOKEN_FOR";
		case TOKEN_RETURN: return "TOKEN_RETURN";

		case TOKEN_INTEGER_TYPE: return "TOKEN_INTEGER_TYPE";
		case TOKEN_UNSIGNED_TYPE: return "TOKEN_UNSIGNED_TYPE";
		case TOKEN_FLOAT_TYPE: return "TOKEN_FLOAT_TYPE";
		case TOKEN_BOOL_TYPE: return "TOKEN_BOOL_TYPE";
		case TOKEN_CHARACTER_TYPE: return "TOKEN_CHARACTER_TYPE";

		case TOKEN_INTEGER_LITERAL: return "TOKEN_INTEGER_LITERAL";
		case TOKEN_REAL_LITERAL: return "TOKEN_REAL_LITERAL";
		case TOKEN_CHARACTER_LITERAL: return "TOKEN_CHARACTER_LITERAL";
		case TOKEN_STRING_LITERAL: return "TOKEN_STRING_LITERAL";

		case TOKEN_PARENTHESIS_LEFT: return "TOKEN_PARENTHESIS_LEFT";
		case TOKEN_PARENTHESIS_RIGHT: return "TOKEN_PARENTHESIS_RIGHT";
		case TOKEN_BRACKET_LEFT: return "TOKEN_BRACKET_LEFT";
		case TOKEN_BRACKET_RIGHT: return "TOKEN_BRACKET_RIGHT";
		case TOKEN_BRACE_LEFT: return "TOKEN_BRACE_LEFT";
		case TOKEN_BRACE_RIGHT: return "TOKEN_BRACE_RIGHT";
		case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
		case TOKEN_COLON: return "TOKEN_COLON";
		case TOKEN_COMMA: return "TOKEN_COMMA";
		case TOKEN_EQUAL: return "TOKEN_EQUAL";
		case TOKEN_PLUS: return "TOKEN_PLUS";
		case TOKEN_MINUS: return "TOKEN_MINUS";
		case TOKEN_STAR: return "TOKEN_STAR";
		case TOKEN_FORWARD_SLASH: return "TOKEN_FORWARD_SLASH";
		case TOKEN_PERCENT: return "TOKEN_PERCENT";
		case TOKEN_AMPERSAND: return "TOKEN_AMPERSAND";
		case TOKEN_BAR: return "TOKEN_BAR";
		case TOKEN_CARET: return "TOKEN_CARET";
		case TOKEN_TILDE: return "TOKEN_TILDE";
		case TOKEN_LESS_LESS: return "TOKEN_LESS_LESS";
		case TOKEN_GREATER_GREATER: return "TOKEN_GREATER_GREATER";
		case TOKEN_PLUS_EQUAL: return "TOKEN_PLUS_EQUAL";
		case TOKEN_MINUS_EQUAL: return "TOKEN_MINUS_EQUAL";
		case TOKEN_STAR_EQUAL: return "TOKEN_STAR_EQUAL";
		case TOKEN_FORWARD_SLASH_EQUAL: return "TOKEN_FORWARD_SLASH_EQUAL";
		case TOKEN_PERCENT_EQUAL: return "TOKEN_PERCENT_EQUAL";
		case TOKEN_AMPERSAND_EQUAL: return "TOKEN_AMPERSAND_EQUAL";
		case TOKEN_BAR_EQUAL: return "TOKEN_BAR_EQUAL";
		case TOKEN_CARET_EQUAL: return "TOKEN_CARET_EQUAL";
		case TOKEN_TILDE_TILDE: return "TOKEN_TILDE_TILDE";
		case TOKEN_LESS_LESS_EQUAL: return "TOKEN_LESS_LESS_EQUAL";
		case TOKEN_GREATER_GREATER_EQUAL: return "TOKEN_GREATER_GREATER_EQUAL";
		case TOKEN_EQUAL_EQUAL: return "TOKEN_EQUAL_EQUAL";
		case TOKEN_EXCLAMATION_EQUAL: return "TOKEN_EXCLAMATION_EQUAL";
		case TOKEN_LESS: return "TOKEN_LESS";
		case TOKEN_GREATER: return "TOKEN_GREATER";
		case TOKEN_LESS_EQUAL: return "TOKEN_LESS_EQUAL";
		case TOKEN_GREATER_EQUAL: return "TOKEN_GREATER_EQUAL";
		
		default: return "UNKNOWN_TOKEN";
	}
}

void printToken(Token token) {
	printf("{%s, index: %zu, length: %zu, line: %zu, column: %zu",
			tokenTypeToString(token.type), token.offset_in_source, token.length_in_source, token.line_number, token.column_number);
	
	switch (token.type) {
		case TOKEN_IDENTIFIER: printf(", identifier ID: %zu, identifier name: ", token.data.identifier_ID);
		Identifier identifier = getIdentifierFromID(token.data.identifier_ID);
		for (size_t i = 0; i < identifier.name_length; ++i) {
			putchar(identifier.name[i]);
		}
		printf("}");
		break;

		case TOKEN_INTEGER_TYPE:
		case TOKEN_UNSIGNED_TYPE:
		case TOKEN_FLOAT_TYPE:
		printf(", width: %zu}", token.data.type_width);
		break;

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

		default: printf("}"); break;

		//need to add null termination for printf
		
	}
}
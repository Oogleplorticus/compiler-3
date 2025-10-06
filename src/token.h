#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
	//misc
	TOKEN_NONE,
	TOKEN_EOF,

	TOKEN_IDENTIFIER,

	//keywords
	TOKEN_IF,
	TOKEN_ELSE,
	TOKEN_WHILE,
	TOKEN_FOR,
	TOKEN_RETURN,

	//types
	TOKEN_INTEGER_TYPE,
	TOKEN_UNSIGNED_TYPE,
	TOKEN_FLOAT_TYPE,
	TOKEN_BOOL_TYPE,
	TOKEN_CHARACTER_TYPE,

	//literals
	TOKEN_INTEGER_LITERAL,
	TOKEN_REAL_LITERAL,
	TOKEN_CHARACTER_LITERAL,
	TOKEN_STRING_LITERAL,

	//delimiters
	TOKEN_PARENTHESIS_LEFT,
	TOKEN_PARENTHESIS_RIGHT,
	TOKEN_BRACKET_LEFT,
	TOKEN_BRACKET_RIGHT,
	TOKEN_BRACE_LEFT,
	TOKEN_BRACE_RIGHT,
	TOKEN_SEMICOLON,
	TOKEN_COLON,
	TOKEN_COMMA,

	//operators
	//misc
	TOKEN_EQUAL,
	//arithmetic
	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_STAR,
	TOKEN_FORWARD_SLASH,
	TOKEN_PERCENT,
	//bitwise
	TOKEN_AMPERSAND,
	TOKEN_BAR,
	TOKEN_CARET,
	TOKEN_TILDE,
	TOKEN_LESS_LESS,
	TOKEN_GREATER_GREATER,
	//arithmetic assignment
	TOKEN_PLUS_EQUAL,
	TOKEN_MINUS_EQUAL,
	TOKEN_STAR_EQUAL,
	TOKEN_FORWARD_SLASH_EQUAL,
	TOKEN_PERCENT_EQUAL,
	//bitwise assignment
	TOKEN_AMPERSAND_EQUAL,
	TOKEN_BAR_EQUAL,
	TOKEN_CARET_EQUAL,
	TOKEN_TILDE_TILDE,
	TOKEN_LESS_LESS_EQUAL,
	TOKEN_GREATER_GREATER_EQUAL,
	//comparison
	TOKEN_EQUAL_EQUAL,
	TOKEN_EXCLAMATION_EQUAL,
	TOKEN_LESS,
	TOKEN_GREATER,
	TOKEN_LESS_EQUAL,
	TOKEN_GREATER_EQUAL,
} TokenType;

typedef struct {
	TokenType type;

	size_t offset_in_source;
	size_t length_in_source;
	size_t line_number;
	size_t column_number;

	union {
		int64_t integer;
		double real;
		char character;
		struct {char* text; size_t length;} string; //Not null-terminated. text points to a copy with escape characters handled
		size_t identifier_ID;
		size_t type_width; //in bits
	} data;
} Token;

void printToken(Token token);
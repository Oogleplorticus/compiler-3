#include "tokeniser.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "token.h"

static FILE* source;

static Token current_token;
static Token next_token;

static size_t line_number = 1;
static size_t column_number = 1;

void unexpectedCharacter(char c, size_t index, size_t line, size_t column) {
	printf("ERROR: Unexpected character '%c' at index: %zu, line: %zu, column: %zu!\n", c, index, line, column);
	exit(1);
}

void skipWhitespace() {
	char c = fgetc(source);
	while (isspace(c)) {
		if (c == '\n') {
			++line_number;
			column_number = 1;
		} else {
			++column_number;
		}
		c = fgetc(source);
	}
	fseek(source, -1, SEEK_CUR);
}

void getNumberLiteral(Token* token, char first_char) {
	char second_char = fgetc(source);

	int base = 10;
	//test for integer base
	if (first_char == '0' && !isdigit(second_char)) {
		switch (second_char) {
			case 'b': base = 2; break;
			case 'o': base = 8; break;
			case 'x': base = 16; break;

			default: unexpectedCharacter(second_char, ftell(source) - 1, line_number, column_number + 2);
		}
	}

	bool real = false;
	//skip to end of number
	char c = fgetc(source);
	while (isdigit(c) || c == '.') {
		if (c == '.') {
			real = true;
		}
		c = fgetc(source);
	}

	//test for error
	if (base != 10 && real) {
		printf("ERROR: Disallowed based real literal at index: %zu, line: %zu, column: %zu!\n", token->offset_in_source, line_number, column_number);
		exit(1);
	}

	//set token variables
	token->length_in_source = ftell(source) - token->offset_in_source;
	if (real) {
		token->type = TOKEN_REAL_LITERAL;
		//prepare string buffer
		char buffer[token->length_in_source + 1];
		buffer[token->length_in_source] = '\0';
		fseek(source, token->offset_in_source, SEEK_SET);
		fread(buffer, sizeof(char), token->length_in_source, source);
		//convert string buffer
		char* end_pointer;
		token->data.real = strtod(buffer, &end_pointer);
		if (end_pointer != NULL) {
			printf("ERROR: Failed to fully convert real literal at index: %zu, line: %zu, column: %zu!\n", token->offset_in_source, line_number, column_number);
			exit(1);
		}
	} else {
		token->type = TOKEN_INTEGER_LITERAL;
		//prepare string buffer
		//fancy handling as we want to ignore the base syntax
		size_t buffer_length = base == 10 ? token->length_in_source + 1 : token->length_in_source - 1;
		size_t copy_index = base == 10 ? token->offset_in_source : token->offset_in_source + 2;
		char buffer[buffer_length];
		buffer[buffer_length - 1] = '\0';
		fseek(source, copy_index, SEEK_SET);
		fread(buffer, sizeof(char), buffer_length - 1, source);
		//convert string buffer
		char* end_pointer;
		token->data.integer = strtol(buffer, &end_pointer, base);
		if (end_pointer != NULL) {
			printf("ERROR: Failed to fully convert integer literal at index: %zu, line: %zu, column: %zu!\n", token->offset_in_source, line_number, column_number);
			exit(1);
		}
	}
}

//passing n will return \n
char escapeCharacterToCharacter(char c) {
	switch (c) {
		case 'n': return '\n';
		case '\\': return '\\';

		default:
		printf("ERROR: Attempted to escape non escape character!");
		exit(1);
	}
}

void getCharacterLiteral(Token* token, char first_char) {
	token->type = TOKEN_CHARACTER_LITERAL;
	char second_char = fgetc(source);
	if (second_char != '\\') {
		//not an escape character
		token->length_in_source = 3;
		token->data.character = second_char;
		//ensure proper syntax
		char final_char = fgetc(source);
		if (final_char != '\'') unexpectedCharacter(final_char, ftell(source) - 1, line_number, column_number + 2);
	}
	//handle escape character
	token->length_in_source = 4;
	token->data.character = escapeCharacterToCharacter(fgetc(source));
	//ensure proper syntax
	char final_char = fgetc(source);
	if (final_char != '\'') unexpectedCharacter(final_char, ftell(source) - 1, line_number, column_number + 2);
}

void getStringLiteral(Token* token, char first_char) {
	token->type = TOKEN_STRING_LITERAL;

	//determine string data length
	size_t true_literal_length = 0;
	bool escaped = false;
	char c = fgetc(source);
	while (!feof(source) && !(c == '"' && !escaped)) {
		//escape character logic
		if (c == '\\' && !escaped) {
			escaped = true;
		} else {
			++true_literal_length;
			escaped = false;
		}

		c = fgetc(source);
	}

	//ensure proper syntax
	if (c != '"') {
		printf("ERROR: Could not process string literal at index: %zu, line: %zu, column: %zu!\n", token->offset_in_source, line_number, column_number);
		exit(1);
	}

	//allocate string memory
	char* string_ptr = malloc(true_literal_length);
	if (string_ptr == NULL) {
		printf("ERROR: Failed to allocate memory for string literal!\n");
		exit(1);
	}

	//set token variables
	token->length_in_source = ftell(source) - token->offset_in_source;
	token->data.string.length = true_literal_length;
	token->data.string.text = string_ptr;

	//insert string literal into allocated memory
	fseek(source, token->offset_in_source + 1, SEEK_SET);
	for (int i = 0; i < token->data.string.length; ++i) {
		char c = fgetc(source);
		if (c == '\\') {
			char escape = fgetc(source);
			token->data.string.text[i] = escapeCharacterToCharacter(escape);
		} else {
			token->data.string.text[i] = c;
		}
	}
}

Token getToken(size_t search_index) {
	fseek(source, search_index, SEEK_SET);
	skipWhitespace();

	//initialise token
	Token token;
	token.type = TOKEN_UNDETERMINED; //default, will be overwritten
	token.offset_in_source = ftell(source);
	token.length_in_source = 1; //default, will be overwritten
	token.line_number = line_number;
	token.column_number = column_number;

	char first_char = fgetc(source);
	
	//test eof
	fgetc(source);
	if (feof(source)) {
		token.type = TOKEN_EOF;
		token.length_in_source = 0;
		return token;
	}
	fseek(source, -1, SEEK_CUR);

	//test number literal
	if (isdigit(first_char)) {
		getNumberLiteral(&token, first_char);
		column_number += token.length_in_source;
		return token;
	}

	//test string literal
	if (first_char == '"') {
		getStringLiteral(&token, first_char);
		column_number += token.length_in_source;
		return token;
	}

	//test character literal
	if (first_char == '\'') {
		getCharacterLiteral(&token, first_char);
		column_number += token.length_in_source;
		return token;
	}

	//assume identifier or keyword

	return token;
}

void tokeniserSetSource(FILE* new_source) {
	source = new_source;

	//initialise tokens
	current_token = getToken(0);
	size_t next_token_search_index = current_token.offset_in_source + current_token.length_in_source;
	next_token = getToken(next_token_search_index);

	//reset line and column
	line_number = 1;
	column_number = 1;
}

Token currentToken() {
	return current_token;
}

Token nextToken() {
	return next_token;
}

void incrementToken() {
	//free string literal token data
	if (current_token.type == TOKEN_STRING_LITERAL) {
		free(current_token.data.string.text);
	}
	//update tokens
	current_token = next_token;
	size_t next_token_search_index = current_token.offset_in_source + current_token.length_in_source;
	next_token = getToken(next_token_search_index);
}
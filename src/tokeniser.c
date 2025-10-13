#include "tokeniser.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "identifier_table.h"
#include "token.h"

static FILE* source;

static Token current_token;
static Token next_token;

//not super accurate
static size_t line_number = 1;
static size_t column_number = 1;

typedef struct {
	const char* keyword;
	const size_t length;
	TokenType type;
} Keyword;
static const Keyword KEYWORD_TABLE[] = {
	{"fn", sizeof("fn") - sizeof(char), TOKEN_FN},
	{"if", sizeof("if") - sizeof(char), TOKEN_IF},
	{"else", sizeof("else") - sizeof(char), TOKEN_ELSE},
	{"while", sizeof("while") - sizeof(char), TOKEN_WHILE},
	{"for", sizeof("for") - sizeof(char), TOKEN_FOR},
	{"return", sizeof("return") - sizeof(char), TOKEN_RETURN},

	//not technically keywords but works best here since they have set sizes
	{"isize", sizeof("isize") - sizeof(char), TOKEN_INTEGER_TYPE},
	{"usize", sizeof("usize") - sizeof(char), TOKEN_UNSIGNED_TYPE},
	{"bool", sizeof("bool") - sizeof(char), TOKEN_BOOL_TYPE},
	{"char", sizeof("char") - sizeof(char), TOKEN_CHARACTER_TYPE},
};
static const size_t KEYWORD_TABLE_LENGTH = sizeof(KEYWORD_TABLE) / sizeof(KEYWORD_TABLE[0]);
//buffer should not be null terminated
static TokenType findInKeywordTable(const char* buffer, size_t buffer_length) {
	for (size_t i = 0; i < KEYWORD_TABLE_LENGTH; ++i) {
		if (KEYWORD_TABLE[i].length != buffer_length) continue;
		int comparison_result = memcmp(buffer, KEYWORD_TABLE[i].keyword, buffer_length);
		if (comparison_result == 0) return KEYWORD_TABLE[i].type;
	}
	return TOKEN_NONE;
}

//MUST be sorted from longest length to shortest length
//to prevent situations where << is identified as <
typedef struct {
	const char* punctuation;
	const size_t length;
	TokenType type;
} Punctuation;
static const Punctuation PUNCTUATION_TABLE[] = {
	//length 3
	{"<<=", sizeof("<<=") - sizeof(char), TOKEN_LESS_LESS_EQUAL},
	{">>=", sizeof(">>=") - sizeof(char), TOKEN_GREATER_GREATER_EQUAL},
	//length 2
	{"->", sizeof("->") - sizeof(char), TOKEN_MINUS_GREATER},
	{"<<", sizeof("<<") - sizeof(char), TOKEN_LESS_LESS},
	{">>", sizeof(">>") - sizeof(char), TOKEN_GREATER_GREATER},
	{"+=", sizeof("+=") - sizeof(char), TOKEN_PLUS_EQUAL},
	{"-=", sizeof("-=") - sizeof(char), TOKEN_MINUS_EQUAL},
	{"*=", sizeof("*=") - sizeof(char), TOKEN_STAR_EQUAL},
	{"/=", sizeof("/=") - sizeof(char), TOKEN_FORWARD_SLASH_EQUAL},
	{"%=", sizeof("%=") - sizeof(char), TOKEN_PERCENT_EQUAL},
	{"&=", sizeof("&=") - sizeof(char), TOKEN_AMPERSAND_EQUAL},
	{"|=", sizeof("|=") - sizeof(char), TOKEN_BAR_EQUAL},
	{"^=", sizeof("^=") - sizeof(char), TOKEN_CARET_EQUAL},
	{"~~", sizeof("~~") - sizeof(char), TOKEN_TILDE_TILDE},
	{"==", sizeof("==") - sizeof(char), TOKEN_EQUAL_EQUAL},
	{"!=", sizeof("!=") - sizeof(char), TOKEN_EXCLAMATION_EQUAL},
	{"<=", sizeof("<=") - sizeof(char), TOKEN_LESS_EQUAL},
	{">=", sizeof(">=") - sizeof(char), TOKEN_GREATER_EQUAL},
	//length 1
	{"(", sizeof("(") - sizeof(char), TOKEN_PARENTHESIS_LEFT},
	{")", sizeof(")") - sizeof(char), TOKEN_PARENTHESIS_RIGHT},
	{"[", sizeof("[") - sizeof(char), TOKEN_BRACKET_LEFT},
	{"]", sizeof("]") - sizeof(char), TOKEN_BRACKET_RIGHT},
	{"{", sizeof("{") - sizeof(char), TOKEN_BRACE_LEFT},
	{"}", sizeof("}") - sizeof(char), TOKEN_BRACE_RIGHT},
	{";", sizeof(";") - sizeof(char), TOKEN_SEMICOLON},
	{":", sizeof(":") - sizeof(char), TOKEN_COLON},
	{",", sizeof(",") - sizeof(char), TOKEN_COMMA},
	{"=", sizeof("=") - sizeof(char), TOKEN_EQUAL},
	{"+", sizeof("+") - sizeof(char), TOKEN_PLUS},
	{"-", sizeof("-") - sizeof(char), TOKEN_MINUS},
	{"*", sizeof("*") - sizeof(char), TOKEN_STAR},
	{"/", sizeof("/") - sizeof(char), TOKEN_FORWARD_SLASH},
	{"%", sizeof("%") - sizeof(char), TOKEN_PERCENT},
	{"&", sizeof("&") - sizeof(char), TOKEN_AMPERSAND},
	{"|", sizeof("|") - sizeof(char), TOKEN_BAR},
	{"^", sizeof("^") - sizeof(char), TOKEN_CARET},
	{"~", sizeof("~") - sizeof(char), TOKEN_TILDE},
	{"<", sizeof("<") - sizeof(char), TOKEN_LESS},
	{">", sizeof(">") - sizeof(char), TOKEN_GREATER},
};
static const size_t PUNCTUATION_TABLE_LENGTH = sizeof(PUNCTUATION_TABLE) / sizeof(PUNCTUATION_TABLE[0]);
//buffer should not be null terminated, always 3 characters long
static Punctuation findInPunctuationTable(const char* buffer) {
	for (size_t i = 0; i < PUNCTUATION_TABLE_LENGTH; ++i) {
		int comparison_result = memcmp(buffer, PUNCTUATION_TABLE[i].punctuation, PUNCTUATION_TABLE[i].length);
		if (comparison_result == 0) return PUNCTUATION_TABLE[i];
	}
	Punctuation not_found = {"", 0, TOKEN_NONE};
	return not_found;
}

static inline void unexpectedCharacter(char c, size_t index, size_t line, size_t column) {
	printf("ERROR: Unexpected character '%c' at index: %zu, line: %zu, column: %zu!\n", c, index, line, column);
	exit(1);
}

static void skipWhitespace() {
	char c = fgetc(source);
	while (isspace(c) && !feof(source)) {
		if (c == '\n') {
			++line_number;
			column_number = 1;
		} else {
			++column_number;
		}
		c = fgetc(source);
	}

	if (feof(source)) return;
	fseek(source, -1, SEEK_CUR);
}

static void getNumberLiteral(Token* token, char first_char) {
	int base = 10;

	//test for integer base
	char second_char = fgetc(source);
	if (first_char == '0' && !isdigit(second_char)) {
		switch (second_char) {
			case 'b': base = 2; break;
			case 'o': base = 8; break;
			case 'x': base = 16; break;

			default:
			fseek(source, -1, SEEK_CUR); // go back to start if no base
			break;
		}
		//ensure base has proceding digit
		char next_char = fgetc(source);
		if (base != 10 && !isdigit(next_char)) {
			unexpectedCharacter(next_char, ftell(source) - 1, line_number, column_number + 3);
		}
		fseek(source, -1, SEEK_CUR);
	} else {
		fseek(source, -1, SEEK_CUR); // go back to start if no base
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
	fseek(source, -1, SEEK_CUR);

	//test for error
	if (base != 10 && real) {
		printf("ERROR: Disallowed based real literal at index: %zu, line: %zu, column: %zu!\n",
			token->offset_in_source, line_number, column_number);
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
		char* end_ptr;
		token->data.real = strtod(buffer, &end_ptr);
		if (end_ptr != buffer + token->length_in_source) {
			printf("ERROR: Failed to fully convert real literal at index: %zu, line: %zu, column: %zu!\n",
				token->offset_in_source, line_number, column_number);
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
		char* end_ptr;
		token->data.integer = strtoll(buffer, &end_ptr, base);
		if (end_ptr != buffer + buffer_length - 1) {
			printf("ERROR: Failed to fully convert integer literal at index: %zu, line: %zu, column: %zu!\n",
				token->offset_in_source, line_number, column_number);
			exit(1);
		}
	}
}

//passing n will return \n
static char escapeCharacterToCharacter(char c) {
	switch (c) {
		case 'n': return '\n';
		case '\\': return '\\';

		default:
		printf("ERROR: Attempted to escape non escape character!");
		exit(1);
	}
}

static void getCharacterLiteral(Token* token) {
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

static void getStringLiteral(Token* token) {
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
		printf("ERROR: Could not process string literal at index: %zu, line: %zu, column: %zu!\n",
			token->offset_in_source, line_number, column_number);
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
	for (size_t i = 0; i < token->data.string.length; ++i) {
		char c = fgetc(source);
		if (c == '\\') {
			char escape = fgetc(source);
			token->data.string.text[i] = escapeCharacterToCharacter(escape);
		} else {
			token->data.string.text[i] = c;
		}
	}
}

//only changes token if a variable type identifier
static void getVariableSizeTypeIdentifier(Token* token, char first_char) {
	//test for correct character
	if (first_char != 'i' && first_char != 'u' && first_char != 'f') {
		return;
	}

	char c = fgetc(source);

	//ensure proceeding character is a number
	if (!isdigit(c)) {
		fseek(source, token->offset_in_source + 1, SEEK_SET);
		return;
	}

	//find end of numbers
	while (isdigit(c)) {
		c = fgetc(source);
	}

	//ensure not an identifier instead
	if (isalnum(c) || c == '_') {
		fseek(source, token->offset_in_source + 1, SEEK_SET);
		return;
	}

	//set token data
	switch (first_char) {
		case 'i': token->type = TOKEN_INTEGER_TYPE; break;
		case 'u': token->type = TOKEN_UNSIGNED_TYPE; break;
		case 'f': token->type = TOKEN_FLOAT_TYPE; break;

		default:
		printf("ERROR: Reached supposedly unreachable code in getVariableSizeTypeIdentifier() in tokeniser.c!\n");
		exit(1);
	}
	token->length_in_source = ftell(source) - token->offset_in_source - 1;

	char buffer[token->length_in_source];
	fseek(source, token->offset_in_source + 1, SEEK_SET);
	fread(buffer, sizeof(char), token->length_in_source - 1, source);
	buffer[token->length_in_source - 1] = '\0';

	char* end_ptr;
	size_t type_width = strtoll(buffer, &end_ptr, 10);
	if (end_ptr != buffer + token->length_in_source - 1) {
		printf("ERROR: Failed to fully convert type identifier bit width at index: %zu, line: %zu, column: %zu!\n",
			token->offset_in_source, line_number, column_number);
		exit(1);
	}
	token->data.type_width = type_width;
}

static void getIdentifierOrKeyword(Token* token) {
	//get length
	char c = fgetc(source);
	while (isalnum(c) || c == '_') {
		c = fgetc(source);
	}
	fseek(source, -1, SEEK_CUR);
	token->length_in_source = ftell(source) - token->offset_in_source;

	//create buffer of identifier/keyword text
	fseek(source, token->offset_in_source, SEEK_SET);
	char buffer[token->length_in_source];
	fread(buffer, sizeof(char), token->length_in_source, source);

	//test if a keyword
	token->type = findInKeywordTable(buffer, token->length_in_source);
	if (token->type != TOKEN_NONE) return; //if its a keyword we are done

	//we can now assume its an identifier
	token->type = TOKEN_IDENTIFIER;
	token->data.identifier_ID = getOrAddIdentifier(buffer, token->length_in_source).ID;
}

static Token getToken(size_t search_index) {
	fseek(source, search_index, SEEK_SET);
	skipWhitespace();

	//initialise token
	Token token;
	token.type = TOKEN_NONE; //default, will be overwritten
	token.offset_in_source = ftell(source);
	token.length_in_source = 1; //default, will be overwritten
	token.line_number = line_number;
	token.column_number = column_number;
	memset(&token.data, 0, sizeof(token.data)); //default, will be overwritten

	//test eof
	if (feof(source)) {
		token.type = TOKEN_EOF;
		token.length_in_source = 0;
		return token;
	}

	char first_char = fgetc(source);

	//test number literal
	if (isdigit(first_char)) {
		getNumberLiteral(&token, first_char);
		column_number += token.length_in_source;
		return token;
	}

	//test string literal
	if (first_char == '"') {
		getStringLiteral(&token);
		column_number += token.length_in_source;
		return token;
	}

	//test character literal
	if (first_char == '\'') {
		getCharacterLiteral(&token);
		column_number += token.length_in_source;
		return token;
	}

	//test variable size types
	getVariableSizeTypeIdentifier(&token, first_char);
	if (token.type != TOKEN_NONE) {
		column_number += token.length_in_source;
		return token;
	}

	//test identifier or keyword
	if (isalpha(first_char) || first_char == '_') {
		getIdentifierOrKeyword(&token);
		column_number += token.length_in_source;
		return token;
	}

	//test punctuation (operators and similar)
	char buffer[3];
	fseek(source, token.offset_in_source, SEEK_SET);
	fread(buffer, sizeof(buffer[0]), sizeof(buffer) / sizeof(buffer[0]), source);
	Punctuation punctuation = findInPunctuationTable(buffer);
	if (punctuation.type != TOKEN_NONE) {
		token.type = punctuation.type;
		token.length_in_source = punctuation.length;
		column_number += token.length_in_source;
		return token;
	}

	//return erroneous token
	return token;
}

void tokeniserSetSource(FILE* new_source) {
	source = new_source;

	//reset state
	line_number = 1;
	column_number = 1;

	resetIdentifierTable();

	//initialise tokens
	current_token = getToken(0);
	size_t next_token_search_index = current_token.offset_in_source + current_token.length_in_source;
	next_token = getToken(next_token_search_index);
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
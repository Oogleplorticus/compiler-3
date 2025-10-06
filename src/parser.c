#include "parser.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "token.h"
#include "tokeniser.h"

//state variables
static size_t scope_depth = 0;

static void unexpectedToken(Token token) {
	printf("ERROR: Unexpected token ");
	printToken(token);
	printf("!\n");
	exit(1);
}

//starts on first token of expression
//ends on token proceding expression
static void parseExpression(TokenType previous_operator) {
	//TODO implement expression parsing
	//currently just skippin the expression
	while (currentToken().type != TOKEN_SEMICOLON && currentToken().type != TOKEN_COMMA) {
		incrementToken();
	}
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

	incrementToken();
	incrementToken();

	//TODO handle type and tags
	//skip for now
	while (currentToken().type != TOKEN_SEMICOLON && currentToken().type != TOKEN_EQUAL) {
		incrementToken();
	}

	switch (currentToken().type) {
		case TOKEN_SEMICOLON: incrementToken(); return; //declaration only

		case TOKEN_EQUAL:
		incrementToken();
		parseExpression(TOKEN_EQUAL);
		incrementToken();
		return;

		default: unexpectedToken(currentToken());
	}
}

//starts on first parameter identifier or closing parenthesis
//ends on first token of next statement
static void parseFunctionDefinition() {
	if (scope_depth > 0) {
		printf("ERROR: attempted to define function outside of top scope!\n");
		unexpectedToken(currentToken());
	}

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
		parseFunctionDefinition();
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

	parseExpression(TOKEN_NONE);

	incrementToken();
	++scope_depth;
}

//starts on first token of statement
//ends on first token of next statement
static void parseStatement() {
	switch (currentToken().type) {
		case TOKEN_IDENTIFIER: parseIdentifier(); return;

		case TOKEN_WHILE: parseWhile(); return;

		//close scope
		case TOKEN_BRACE_RIGHT:
		--scope_depth;
		incrementToken();
		return;

		default: unexpectedToken(currentToken());
	}
}

void parseTokens() {
	//resetState
	scope_depth = 0;

	while(currentToken().type != TOKEN_EOF) {
		parseStatement();
	}

	//test for errors
	if (scope_depth > 0) {
		printf("ERROR: Scope depth did not return to 0, potential missing brace!\n");
		exit(1);
	}
}
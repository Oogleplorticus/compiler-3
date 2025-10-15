#include "parser_utils.h"

#include <stdio.h>
#include <stdlib.h>

#include "token.h"
#include "tokeniser.h"

//starts on opening brace, ends on closing brace
void skipScope(void) {
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

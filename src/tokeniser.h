#pragma once

#include <stddef.h>

#include "token.h"

//must be called before other functions, resets state
void tokeniserSetBuffer(char* buffer, size_t buffer_length);

Token currentToken();
Token nextToken();
void incrementToken();
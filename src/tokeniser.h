#pragma once

#include <stddef.h>
#include <stdio.h>

#include "token.h"

//must be called before other functions, resets state
void tokeniserSetSource(FILE* new_source);

Token currentToken();
Token nextToken();
void incrementToken();
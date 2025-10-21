#pragma once

#include "compilation_unit.h"
#include "token.h"

#define UNEXPECTED_TOKEN(TOKEN)                                                                         \
printf("ERROR: Unexpected token ");                                                                     \
printToken(TOKEN);                                                                                      \
printf("\nError called from:\n\tFile: %s\n\tFunction: %s\n\tLine: %d\n", __FILE__, __func__, __LINE__); \
exit(1)

#define ASSERT_CURRENT_TOKEN(TOKEN_TYPE) \
if (currentToken().type != TOKEN_TYPE) {UNEXPECTED_TOKEN(currentToken());}

#define ASSERT_NEXT_TOKEN(TOKEN_TYPE) \
if (nextToken().type != TOKEN_TYPE) {UNEXPECTED_TOKEN(nextToken());}

void skipScope(void);
void skipStruct(void);

VariableType variableTypeFromToken(Token token);

LLVMTypeRef llvmTypeFromVariableType(LLVMContextRef llvm_context, VariableType variable_type);
LLVMTypeRef llvmFunctionTypeFromFunction(LLVMContextRef llvm_context, Function* function);

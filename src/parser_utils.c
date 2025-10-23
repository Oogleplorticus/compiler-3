#include "parser_utils.h"

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "compilation_unit.h"
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

void skipStruct(void) {
	//skip declaration
	while (currentToken().type != TOKEN_BRACE_LEFT) {
		incrementToken();
	}
	//skip definition
	skipScope();
	incrementToken();
	return;
}

VariableType variableTypeFromToken(Token token) {
	VariableType variable_type;

	switch (token.type) {
		case TOKEN_INTEGER_TYPE:
		variable_type.kind = TYPE_INT;
		variable_type.data.width = token.data.type_width;
		break;

		case TOKEN_UNSIGNED_TYPE:
		variable_type.kind = TYPE_UNSIGNED;
		variable_type.data.width = token.data.type_width;
		break;

		case TOKEN_FLOAT_TYPE:
		variable_type.kind = TYPE_FLOAT;
		variable_type.data.width = token.data.type_width;
		//ensure valid bit width
		switch (variable_type.data.width) {
			case 16:
			case 32:
			case 64:
			case 128:
			break;

			default:
			printf("ERROR: Invalid floating point bit width of %zu! Valid widths are 16, 32, 64 or 128.\n", variable_type.data.width);
			UNEXPECTED_TOKEN(token);
		}
		break;

		case TOKEN_BOOL_TYPE:
		variable_type.kind = TYPE_BOOL;
		variable_type.data.width = 1;
		break;

		case TOKEN_CHARACTER_TYPE:
		variable_type.kind = TYPE_CHAR;
		variable_type.data.width = 32;
		break;
		
		default: UNEXPECTED_TOKEN(token);
	}

	return variable_type;
}

LLVMTypeRef llvmTypeFromVariableType(LLVMContextRef llvm_context, VariableType variable_type) {
	switch (variable_type.kind) {
		case TYPE_INT:
		case TYPE_UNSIGNED:
		return LLVMIntTypeInContext(llvm_context, variable_type.data.width);

		case TYPE_FLOAT:
		switch (variable_type.data.width) {
			case 16: return LLVMHalfTypeInContext(llvm_context);
			case 32: return LLVMFloatTypeInContext(llvm_context);
			case 64: return LLVMDoubleTypeInContext(llvm_context);
			case 128: return LLVMPPCFP128TypeInContext(llvm_context);

			default:
			printf("ERROR: Invalid floating point bit width of %zu! Valid widths are 16, 32, 64 or 128.\n", variable_type.data.width);
			exit(1);
		}

		case TYPE_CHAR:
		return LLVMInt32TypeInContext(llvm_context);

		case TYPE_BOOL:
		return LLVMInt1TypeInContext(llvm_context);

		case TYPE_VOID:
		return LLVMVoidTypeInContext(llvm_context);

		case TYPE_STRUCT:
		printf("ERROR: structs not yet supported!\n");
		exit(1);

		case TYPE_NONE:
		printf("ERROR: Attempted to get llvm type reference from invalid \"TYPE_NONE\"!\n");
		exit(1);
	}
}

LLVMTypeRef llvmFunctionTypeFromFunction(LLVMContextRef llvm_context, Function* function) {
	LLVMTypeRef parameters[function->parameter_count];
	for (size_t i = 0; i < function->parameter_count; ++i) {
		parameters[i] = llvmTypeFromVariableType(llvm_context, function->parameters[i].type);
	}

	return LLVMFunctionType(
		llvmTypeFromVariableType(llvm_context, function->returnType),
		parameters,
		function->parameter_count,
		false
	);
}

//larger number = greater precedence
size_t operatorPrecedence(TokenType operator_type) {
	switch (operator_type) {
		case TOKEN_NONE: return 0;
	
		case TOKEN_EQUAL: return 1;
	
		case TOKEN_PLUS: return 10;
		case TOKEN_MINUS: return 10;
		case TOKEN_STAR: return 11;
		case TOKEN_FORWARD_SLASH: return 11;
		case TOKEN_PERCENT: return 11;
	
		case TOKEN_AMPERSAND: return 6;
		case TOKEN_BAR: return 4;
		case TOKEN_CARET: return 5;
		case TOKEN_TILDE: return 12;
		case TOKEN_LESS_LESS: return 9;
		case TOKEN_GREATER_GREATER: return 9;
	
		case TOKEN_PLUS_EQUAL: return 1;
		case TOKEN_MINUS_EQUAL: return 1;
		case TOKEN_STAR_EQUAL: return 1;
		case TOKEN_FORWARD_SLASH_EQUAL: return 1;
		case TOKEN_PERCENT_EQUAL: return 1;
	
		case TOKEN_AMPERSAND_EQUAL: return 1;
		case TOKEN_BAR_EQUAL: return 1;
		case TOKEN_CARET_EQUAL: return 1;
		case TOKEN_TILDE_TILDE: return 1;
		case TOKEN_LESS_LESS_EQUAL: return 1;
		case TOKEN_GREATER_GREATER_EQUAL: return 1;
	
		case TOKEN_EQUAL_EQUAL: return 7;
		case TOKEN_EXCLAMATION_EQUAL: return 7;
		case TOKEN_LESS: return 8;
		case TOKEN_GREATER: return 8;
		case TOKEN_LESS_EQUAL: return 8;
		case TOKEN_GREATER_EQUAL: return 8;

		default:
		printf("ERROR: Tried to get precedence of unsupported operator %d!\n", operator_type);
		exit(1);
	}
}

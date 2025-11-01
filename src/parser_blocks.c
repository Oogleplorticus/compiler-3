#include "parser_blocks.h"

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compilation_unit.h"
#include "parser_utils.h"
#include "token.h"
#include "tokeniser.h"

//forward declarations
static void parseScope(CompilationUnit* compilation_unit, LLVMBuilderRef llvm_builder, Function* current_function, size_t scope_index);

typedef struct {
	enum {
		OPERAND_NULL,
		OPERAND_VARIABLE,
		OPERAND_CONSTANT,
		OPERAND_INTERMEDIATE,
	} operand_type;

	union {
		Variable* variable;
		//for constants and resulting intermediates
		struct {
			LLVMValueRef value;
			VariableType type;
		} llvm_value;
	} operand_value;
} ExpressionOperand;

static VariableType getOperandValueType(ExpressionOperand operand) {
	switch (operand.operand_type) {
		case OPERAND_NULL: return (VariableType){.kind=TYPE_NONE};

		case OPERAND_VARIABLE:
		return operand.operand_value.variable->type;

		case OPERAND_CONSTANT:
		case OPERAND_INTERMEDIATE:
		return operand.operand_value.llvm_value.type;
	}
}

static LLVMValueRef getOperandValue(CompilationUnit* compilation_unit, LLVMBuilderRef llvm_builder, ExpressionOperand operand) {
	switch (operand.operand_type) {
		case OPERAND_NULL: return NULL;
		
		case OPERAND_CONSTANT:
		case OPERAND_INTERMEDIATE:
		return  operand.operand_value.llvm_value.value;

		case OPERAND_VARIABLE:;
		size_t buffer_size = strlen(
			compilation_unit->identifiers[operand.operand_value.variable->identifier_index]
		) * sizeof(char) + sizeof("_ssa");
		char name[buffer_size];
		strcpy(name, compilation_unit->identifiers[operand.operand_value.variable->identifier_index]);
		strcat(name, "_ssa");

		return LLVMBuildLoad2(
			llvm_builder,
			operand.operand_value.variable->llvm_type,
			operand.operand_value.variable->llvm_stack_pointer,
			name
		);
	}
}

//starts on first token of operand
//ends on token following operand
static ExpressionOperand parseExpressionOperand(
	CompilationUnit* compilation_unit,
	Function* current_function,
	size_t current_scope_index,
	VariableType expected_type
) {
	ExpressionOperand expression_operand;
	memset(&expression_operand, 0, sizeof(expression_operand));

	switch (currentToken().type) {
		//variable or function call
		case TOKEN_IDENTIFIER:
		if (nextToken().type == TOKEN_PARENTHESIS_LEFT) {
			//TODO function calls
			printf("ERROR: Attempted to parse currently unsupported function call!\n");
			exit(1);
		}
		//is either variable, or struct member/function call
		//both of these require knowing the varaiable
		size_t variable_identifier_index = compilationUnit_getOrAddIdentifierIndex(compilation_unit, currentToken().data.identifier);
		Variable* variable = compilationUnit_findVariableFromScope(
			compilation_unit,
			current_function,
			current_scope_index,
			variable_identifier_index
		);
		if (variable == NULL) {
			printf("ERROR: Use of undeclared variable!\n");
			UNEXPECTED_TOKEN(currentToken());
		}
		//type checking
		if (variable->type.kind != expected_type.kind && expected_type.kind != TYPE_NONE) {
			printf("ERROR: Mismatched variable type!\n");
			UNEXPECTED_TOKEN(currentToken());
		}
		//if struct, check members
		if (variable->type.kind == TYPE_STRUCT) {
			//TODO support structs
			printf("ERROR: Attempted to parse currently unsupported struct!\n");
			exit(1);
		}

		//fill variable data
		expression_operand.operand_type = OPERAND_VARIABLE;
		expression_operand.operand_value.variable = variable;

		incrementToken();
		break;

		//literals
		case TOKEN_INTEGER_LITERAL:
		//ensure literal type matches expected type
		if (expected_type.kind != TYPE_INT && expected_type.kind != TYPE_UNSIGNED && expected_type.kind != TYPE_NONE) {
			printf("ERROR: Mismatched literal type!\n");
			UNEXPECTED_TOKEN(currentToken());
		}
		//default to 64 bit
		if (expected_type.kind == TYPE_NONE) {
			expected_type.kind = TYPE_INT;
			expected_type.data.width = 64;
		}
		//fill operand data
		expression_operand.operand_type = OPERAND_CONSTANT;
		expression_operand.operand_value.llvm_value.type = expected_type;
		expression_operand.operand_value.llvm_value.value = LLVMConstInt(
			llvmTypeFromVariableType(compilation_unit->llvm_context, expected_type),
			currentToken().data.integer,
			expected_type.kind != TYPE_UNSIGNED
		);
		incrementToken();
		break;
		
		case TOKEN_REAL_LITERAL:
		//ensure literal type matches expected type
		if (expected_type.kind != TYPE_FLOAT && expected_type.kind != TYPE_NONE) {
			printf("ERROR: Mismatched literal type!\n");
			UNEXPECTED_TOKEN(currentToken());
		}
		//default to 64 bit
		if (expected_type.kind == TYPE_NONE) {
			expected_type.kind = TYPE_FLOAT;
			expected_type.data.width = 64;
		}
		//fill operand data
		expression_operand.operand_type = OPERAND_CONSTANT;
		expression_operand.operand_value.llvm_value.type = expected_type;
		expression_operand.operand_value.llvm_value.value = LLVMConstReal(
			llvmTypeFromVariableType(compilation_unit->llvm_context, expected_type),
			currentToken().data.real
		);
		incrementToken();
		break;

		case TOKEN_CHARACTER_LITERAL:
		//ensure literal type matches expected type
		if (expected_type.kind != TYPE_CHAR && expected_type.kind != TYPE_NONE) {
			printf("ERROR: Mismatched literal type!\n");
			UNEXPECTED_TOKEN(currentToken());
		}
		//fill operand data
		expression_operand.operand_type = OPERAND_CONSTANT;
		expression_operand.operand_value.llvm_value.type = (VariableType){.kind=TYPE_CHAR, .data.width=32};
		expression_operand.operand_value.llvm_value.value = LLVMConstInt(
			LLVMInt32TypeInContext(compilation_unit->llvm_context),
			currentToken().data.character,
			false
		);
		incrementToken();
		break;

		case TOKEN_STRING_LITERAL:
		//ensure literal type matches expected type
		if (expected_type.kind != TYPE_STRUCT && expected_type.kind != TYPE_NONE) {
			printf("ERROR: Mismatched literal type!\n");
			UNEXPECTED_TOKEN(currentToken());
		}
		//fill operand data
		expression_operand.operand_type = OPERAND_CONSTANT;
		expression_operand.operand_value.llvm_value.type = (VariableType){.kind=TYPE_STRUCT};
		expression_operand.operand_value.llvm_value.value = LLVMConstStringInContext2(
			compilation_unit->llvm_context,
			currentToken().data.string.text,
			currentToken().data.string.length,
			true
		);
		incrementToken();
		break;

		//should only be used in the following block
		int bool_value = 0;
		case TOKEN_TRUE:
		bool_value = 1;
		case TOKEN_FALSE:
		//ensure literal type matches expected type
		if (expected_type.kind != TYPE_BOOL && expected_type.kind != TYPE_NONE) {
			printf("ERROR: Mismatched literal type!\n");
			UNEXPECTED_TOKEN(currentToken());
		}
		//fill operand data
		expression_operand.operand_type = OPERAND_CONSTANT;
		expression_operand.operand_value.llvm_value.type = (VariableType){.kind=TYPE_CHAR, .data.width=1};
		expression_operand.operand_value.llvm_value.value = LLVMConstInt(
			LLVMInt1TypeInContext(compilation_unit->llvm_context),
			bool_value,
			false
		);
		incrementToken();
		break;

		default: UNEXPECTED_TOKEN(currentToken());
	}

	return expression_operand;
}

static inline bool expressionTypesMismatched(ExpressionOperand left_operand, ExpressionOperand right_operand) {
	return getOperandValueType(left_operand).kind != getOperandValueType(right_operand).kind &&
		getOperandValueType(left_operand).kind != TYPE_NONE &&
		getOperandValueType(right_operand).kind != TYPE_NONE &&
		!(getOperandValueType(left_operand).kind == TYPE_INT && getOperandValueType(right_operand).kind == TYPE_UNSIGNED) &&
		!(getOperandValueType(left_operand).kind == TYPE_UNSIGNED && getOperandValueType(right_operand).kind == TYPE_INT);
}

//somewhat shabby code, not sure how to improve it though
static ExpressionOperand emitBinaryOperation(
	CompilationUnit* compilation_unit,
	LLVMBuilderRef llvm_builder,
	TokenType operator,
	ExpressionOperand left_operand,
	ExpressionOperand right_operand
) {
	//also used as a temporary in assignment operators
	ExpressionOperand operation_result;
	memset(&operation_result, 0, sizeof(operation_result));
	operation_result.operand_type = OPERAND_INTERMEDIATE;

	switch (operator) {
		case TOKEN_EQUAL:
		if (left_operand.operand_type != OPERAND_VARIABLE) {
			printf("ERROR: Attempted to assign to non-variable operand!\n");
			exit(1);
		}
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting assignment!\n");
			exit(1);
		}
		//get assignment value
		operation_result.operand_value.llvm_value.value = getOperandValue(compilation_unit, llvm_builder, right_operand);
		operation_result.operand_value.llvm_value.type = left_operand.operand_value.variable->type;
		LLVMBuildStore(
			llvm_builder,
			operation_result.operand_value.llvm_value.value,
			left_operand.operand_value.variable->llvm_stack_pointer
		);

		return operation_result;

		//arithmetic
		case TOKEN_PLUS:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting addition!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildAdd(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildFAdd(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit addition with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_MINUS:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting subtraction!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildSub(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildFSub(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit subtraction with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_STAR:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting multiplication!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildMul(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildFMul(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit multiplication with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_FORWARD_SLASH:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting division!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildSDiv(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildUDiv(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildFDiv(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit division with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_PERCENT:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting remainder!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildSRem(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildURem(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildFRem(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit remainder with unsupported type!\n");
			exit(1);
		}
		break;

		//bitwise
		case TOKEN_AMPERSAND:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting bitwise and!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildAnd(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit bitwise and with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_BAR:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting bitwise or!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildOr(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit bitwise or with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_CARET:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting bitwise xor!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildXor(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit bitwise xor with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_LESS_LESS:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting left shift!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildShl(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit left shift with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_GREATER_GREATER:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting left shift!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildAShr(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = getOperandValueType(left_operand);
			operation_result.operand_value.llvm_value.value = LLVMBuildLShr(
				llvm_builder,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit left shift with unsupported type!\n");
			exit(1);
		}
		break;

		//arithmetic assignment
		case TOKEN_PLUS_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_PLUS, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		case TOKEN_MINUS_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_MINUS, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		case TOKEN_STAR_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_STAR, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		case TOKEN_FORWARD_SLASH_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_FORWARD_SLASH, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		case TOKEN_PERCENT_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_PERCENT, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		//bitwise assignment
		case TOKEN_AMPERSAND_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_AMPERSAND, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		case TOKEN_BAR_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_BAR, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		case TOKEN_CARET_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_CARET, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		case TOKEN_LESS_LESS_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_LESS_LESS, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		case TOKEN_GREATER_GREATER_EQUAL:
		operation_result = emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_GREATER_GREATER, left_operand, right_operand);
		return emitBinaryOperation(compilation_unit, llvm_builder, TOKEN_EQUAL, left_operand, operation_result);

		//comparison
		case TOKEN_EQUAL_EQUAL:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting equal comparison!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			case TYPE_UNSIGNED:
			case TYPE_CHAR:
			case TYPE_BOOL:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntEQ,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildFCmp(
				llvm_builder,
				LLVMRealOEQ,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit equal comparison with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_EXCLAMATION_EQUAL:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting not-equal comparison!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			case TYPE_UNSIGNED:
			case TYPE_CHAR:
			case TYPE_BOOL:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntNE,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildFCmp(
				llvm_builder,
				LLVMRealONE,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit not-equal comparison with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_LESS:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting less-than comparison!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntSLT,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntULT,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildFCmp(
				llvm_builder,
				LLVMRealOLT,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit less-than comparison with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_GREATER:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting greater-than comparison!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntSGT,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntUGT,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildFCmp(
				llvm_builder,
				LLVMRealOGT,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit greater-than comparison with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_LESS_EQUAL:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting less-than-or-equal comparison!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntSLE,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntULE,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildFCmp(
				llvm_builder,
				LLVMRealOLE,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit less-than-or-equal comparison with unsupported type!\n");
			exit(1);
		}
		break;

		case TOKEN_GREATER_EQUAL:
		if (expressionTypesMismatched(left_operand, right_operand)) {
			printf("ERROR: Type mismatch when emitting greater-than-or-equal comparison!\n");
			exit(1);
		}
		switch (getOperandValueType(left_operand).kind) {
			case TYPE_INT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntSGE,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_UNSIGNED:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildICmp(
				llvm_builder,
				LLVMIntUGE,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			case TYPE_FLOAT:
			operation_result.operand_value.llvm_value.type = (VariableType){.kind=TYPE_BOOL, .data.width=1};
			operation_result.operand_value.llvm_value.value = LLVMBuildFCmp(
				llvm_builder,
				LLVMRealOGE,
				getOperandValue(compilation_unit, llvm_builder, left_operand),
				getOperandValue(compilation_unit, llvm_builder, right_operand),
				""
			);
			return operation_result;
			default:
			printf("ERROR: Attempted to emit greater-than-or-equal comparison with unsupported type!\n");
			exit(1);
		}
		break;

		default:
		printf("Attempted to emit unsupported binary operator: %s", tokenTypeToString(operator));
		exit(1);
	}


	return (ExpressionOperand){.operand_type=OPERAND_NULL};
}

//starts on first token of expression
//ends on expression terminator
static ExpressionOperand parseExpression(
	CompilationUnit* compilation_unit,
	LLVMBuilderRef llvm_builder,
	Function* current_function,
	size_t current_scope_index,
	TokenType expression_terminator,
	TokenType previous_operator,
	VariableType expected_type
) {
	ExpressionOperand left_operand = parseExpressionOperand(
		compilation_unit,
		current_function,
		current_scope_index,
		(VariableType){.kind=TYPE_NONE}
	);
	
	while (currentToken().type != expression_terminator) {
		if (currentToken().type == TOKEN_EOF) {UNEXPECTED_TOKEN(currentToken());}
		TokenType current_operator = currentToken().type;

		//if the operator precendence has dropped, return early
		//this results in all previous operators of greater precedence emitting code
		if (operatorPrecedence(current_operator) < operatorPrecedence(previous_operator)) {
			return left_operand;
		}

		//parse following operator
		incrementToken();
		ExpressionOperand right_operand = parseExpression(
			compilation_unit,
			llvm_builder,
			current_function,
			current_scope_index,
			expression_terminator,
			current_operator,
			(VariableType){.kind=TYPE_NONE}
		);

		//emit bytecode
		ExpressionOperand result = emitBinaryOperation(
			compilation_unit,
			llvm_builder,
			current_operator,
			left_operand,
			right_operand
		);

		//type checking
		if (!typesEquivalent(result.operand_value.llvm_value.type, expected_type, false) && expected_type.kind != TYPE_NONE) {
			printf("ERROR: Mismatched variable type!\n");
			UNEXPECTED_TOKEN(currentToken());
		}

		//setup for return/continued parsing
		left_operand = result;
	}

	return left_operand;
}

static void parseVariableDeclaration(
	CompilationUnit* compilation_unit,
	LLVMBuilderRef llvm_builder,
	Function* current_function,
	size_t current_scope_index
) {
	ASSERT_CURRENT_TOKEN(TOKEN_IDENTIFIER);
	ASSERT_NEXT_TOKEN(TOKEN_COLON);

	//create variable in compilation unit
	Scope* current_scope = current_function->scopes + current_scope_index;
	Variable* variable = compilationUnit_addScopeVariable(current_scope);
	variable->identifier_index = compilationUnit_getOrAddIdentifierIndex(compilation_unit, currentToken().data.identifier);

	//get type
	//assume type is first
	incrementToken();
	incrementToken();
	switch (currentToken().type) {
		case TOKEN_INTEGER_TYPE:
		variable->type.kind = TYPE_INT;
		variable->type.data.width = currentToken().data.type_width;
		break;
		case TOKEN_UNSIGNED_TYPE:
		variable->type.kind = TYPE_UNSIGNED;
		variable->type.data.width = currentToken().data.type_width;
		break;
		case TOKEN_FLOAT_TYPE:
		variable->type.kind = TYPE_FLOAT;
		variable->type.data.width = currentToken().data.type_width;
		break;
		case TOKEN_BOOL_TYPE:
		variable->type.kind = TYPE_BOOL;
		variable->type.data.width = 1;
		break;
		case TOKEN_CHARACTER_TYPE:
		variable->type.kind = TYPE_CHAR;
		variable->type.data.width = 32;
		break;

		//TODO handle structs

		default: UNEXPECTED_TOKEN(currentToken());
	}
	variable->llvm_type = llvmTypeFromVariableType(compilation_unit->llvm_context, variable->type);

	//emit stack allocation
	LLVMBuilderRef alloca_builder = LLVMCreateBuilderInContext(compilation_unit->llvm_context);
	LLVMValueRef first_instruction = LLVMGetFirstInstruction(current_function->llvm_entry_block);
	if (first_instruction != NULL) {
		LLVMPositionBuilderBefore(alloca_builder, LLVMGetFirstInstruction(current_function->llvm_entry_block));
	} else {
		LLVMPositionBuilderAtEnd(alloca_builder, current_function->llvm_entry_block);
	}
	variable->llvm_stack_pointer = LLVMBuildAlloca(
		alloca_builder,
		variable->llvm_type,
		compilation_unit->identifiers[variable->identifier_index]
	);

	//TODO handle tags

	//emit assignment if exists
	incrementToken();
	if (currentToken().type == TOKEN_EQUAL) {
		incrementToken();
		ExpressionOperand assignment_value = parseExpression(
			compilation_unit,
			llvm_builder,
			current_function,
			current_scope_index,
			TOKEN_SEMICOLON,
			TOKEN_EQUAL,
			variable->type
		);
		emitBinaryOperation(
			compilation_unit,
			llvm_builder,
			TOKEN_EQUAL,
			(ExpressionOperand){.operand_type=OPERAND_VARIABLE, .operand_value.variable=variable},
			assignment_value
		);
	}

	//finalise and return
	ASSERT_CURRENT_TOKEN(TOKEN_SEMICOLON);
}

//starts on while statement
//ends on end of while block
static void parseWhileStatement(
	CompilationUnit* compilation_unit,
	LLVMBuilderRef llvm_builder,
	Function* current_function,
	size_t current_scope_index
) {
	ASSERT_CURRENT_TOKEN(TOKEN_WHILE);
	incrementToken();

	//setup condition block
	LLVMBasicBlockRef condition_block = LLVMAppendBasicBlock(
		current_function->llvm_function,
		"while_loop_condition"
	);
	LLVMBuildBr(llvm_builder, condition_block);
	LLVMPositionBuilderAtEnd(llvm_builder, condition_block);

	//parse condition
	ExpressionOperand condition_result = parseExpression(
		compilation_unit,
		llvm_builder,
		current_function,
		current_scope_index,
		TOKEN_BRACE_LEFT,
		TOKEN_NONE,
		(VariableType){.kind=TYPE_BOOL, .data.width=1}
	);
	incrementToken();

	//setup first block of body
	LLVMBasicBlockRef body_start_block = LLVMAppendBasicBlock(
		current_function->llvm_function,
		"while_loop_body_start"
	);
	LLVMPositionBuilderAtEnd(llvm_builder, body_start_block);

	//create and parse new scope
	Scope* body_scope = compilationUnit_addFunctionScope(compilation_unit, current_function);
	body_scope->parent_scope_index = current_scope_index;
	size_t body_scope_index = body_scope - current_function->scopes;

	parseScope(compilation_unit, llvm_builder, current_function, body_scope_index);

	//create exit block
	LLVMBasicBlockRef exit_block = LLVMAppendBasicBlock(
		current_function->llvm_function,
		"while_loop_exit"
	);

	//finalise branches and position builder in exit block
	LLVMBuildBr(llvm_builder, condition_block);
	LLVMPositionBuilderAtEnd(llvm_builder, condition_block);
	LLVMBuildCondBr(
		llvm_builder,
		condition_result.operand_value.llvm_value.value,
		body_start_block,
		exit_block
	);
	LLVMPositionBuilderAtEnd(llvm_builder, exit_block);
}

//starts on fn keyword
static void parseFunctionBody(CompilationUnit* compilation_unit) {
	ASSERT_CURRENT_TOKEN(TOKEN_FN);
	ASSERT_NEXT_TOKEN(TOKEN_IDENTIFIER);

	//get function
	size_t function_identifier_index = compilationUnit_getOrAddIdentifierIndex(compilation_unit, nextToken().data.identifier);
	Function* function = NULL;
	for (size_t i = 0; i < compilation_unit->function_count; ++i) {
		if (function_identifier_index == compilation_unit->functions[i].identifier_index) {
			function = compilation_unit->functions + i;
		}
	}
	if (function == NULL) {
		printf("ERROR: Function declaration could not be found when parsing definition. This should be impossible!\n");
		exit(1);
	}

	//create initial llvm block
	function->llvm_entry_block = LLVMAppendBasicBlockInContext(
		compilation_unit->llvm_context,
		function->llvm_function,
		"entry"
	);

	//create llvm builder
	LLVMBuilderRef llvm_builder = LLVMCreateBuilderInContext(compilation_unit->llvm_context);
	LLVMPositionBuilderAtEnd(llvm_builder, function->llvm_entry_block);

	//setup parameter stack memory
	for (size_t i = 0; i < function->parameter_count; ++i) {
		function->parameters[i].llvm_stack_pointer = LLVMBuildAlloca(
			llvm_builder,
			llvmTypeFromVariableType(compilation_unit->llvm_context, function->parameters[i].type),
			compilation_unit->identifiers[function->parameters[i].identifier_index]
		);
		LLVMValueRef parameter_llvm_temporary = LLVMGetParam(function->llvm_function, i);
		LLVMBuildStore(llvm_builder, parameter_llvm_temporary, function->parameters[i].llvm_stack_pointer);
	}

	//create entry scope
	Scope* entry_scope = compilationUnit_addFunctionScope(compilation_unit, function);
	size_t entry_scope_index = entry_scope - function->scopes;

	//skip declaration
	while (currentToken().type != TOKEN_BRACE_LEFT) {
		incrementToken();
	}
	incrementToken();

	//parse function body
	parseScope(compilation_unit, llvm_builder, function, entry_scope_index);
	incrementToken();

	//implicit return for void functions and main
	char* function_identifier = compilation_unit->identifiers[function_identifier_index];
	bool llvm_block_terminated = LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(llvm_builder)) != NULL;

	if (!llvm_block_terminated) {
		if (strcmp(function_identifier, MAIN_FUNCTION_IDENTIFIER) == 0) {
			LLVMValueRef llvm_0_int = LLVMConstInt(LLVMInt32TypeInContext(compilation_unit->llvm_context), 0, false);
			LLVMBuildRet(llvm_builder, llvm_0_int);
		} else if (function->return_type.kind == TYPE_VOID) {
			LLVMBuildRetVoid(llvm_builder);
		} else {
			printf("ERROR: Non-void function \"%s\" does not return a value!\n", function_identifier);
			exit(1);
		}
	}
	
	LLVMDisposeBuilder(llvm_builder);
}

//return value is true if end of scope
static bool parseStatement(
	CompilationUnit* compilation_unit,
	LLVMBuilderRef llvm_builder, 
	Function* current_function,
	size_t scope_index
) {
	switch (currentToken().type) {
		case TOKEN_IDENTIFIER:
		if (nextToken().type == TOKEN_COLON) {
			parseVariableDeclaration(compilation_unit, llvm_builder, current_function, scope_index);
			incrementToken();
		} else {
			parseExpression(
				compilation_unit,
				llvm_builder,
				current_function,
				scope_index,
				TOKEN_SEMICOLON,
				TOKEN_NONE,
				(VariableType){.kind=TYPE_NONE, .data={NULL}}
			);
			incrementToken();
		}
		break;

		case TOKEN_WHILE:
		parseWhileStatement(compilation_unit, llvm_builder, current_function, scope_index);
		incrementToken();
		break;

		case TOKEN_BRACE_LEFT:;
		//go down a scope
		Scope* new_scope = compilationUnit_addFunctionScope(compilation_unit, current_function);
		new_scope->parent_scope_index = scope_index;
		size_t new_scope_index = new_scope - current_function->scopes;
		parseScope(compilation_unit, llvm_builder, current_function, new_scope_index);
		incrementToken();
		break;
		
		case TOKEN_BRACE_RIGHT:
		//signal to go up a scope
		return true;

		default: UNEXPECTED_TOKEN(currentToken());
	}

	return false;
}

//starts on token after opening brace
//ends at end of scope
static void parseScope(
	CompilationUnit* compilation_unit,
	LLVMBuilderRef llvm_builder,
	Function* current_function,
	size_t scope_index
) {
	while (currentToken().type != TOKEN_EOF) {
		if (parseStatement(compilation_unit, llvm_builder, current_function, scope_index)) return;
	}
}

static void parseFunctions(CompilationUnit* compilation_unit) {
	switch (currentToken().type) {
		case TOKEN_FN:
		parseFunctionBody(compilation_unit);
		return;

		case TOKEN_STRUCT:
		skipStruct();
		return;

		default: UNEXPECTED_TOKEN(currentToken());
	}
}

void parseBlocks(CompilationUnit* compilation_unit) {
	tokeniserSetSource(compilation_unit->source_file);

	while (currentToken().type != TOKEN_EOF) {
		parseFunctions(compilation_unit);
	}
}

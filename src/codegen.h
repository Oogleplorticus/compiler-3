/*
THIS IS ONLY AN INTERFACE
ALL FUNCTIONS ARE WRAPPERS FOR FUNCTION POINTERS TO ACTUAL IMPLEMENTATION
*/
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

//Only used to inform where a variable should be stored
typedef enum {
	TYPE_INTEGER,
	TYPE_UNSIGNED,
	TYPE_FLOAT,
	TYPE_BOOL,
	TYPE_CHAR,
} DataType;

typedef enum {
	INSTRUCTION_ADD,
} Instruction;

typedef union {
	size_t variable;
	size_t variable_as_pointer;
	size_t static_data;
	uint64_t immediate;
} Operand;
typedef enum {
	OPERAND_VARIABLE,
	OPERAND_VARIABLE_AS_POINTER,
	OPERAND_STATIC_DATA,
	OPERAND_IMMEDIATE,
} OperandType;

//file extension will be added by this function, do not add it preemptively
void codegenWriteToFile(char* file_path);

//returns the codegen function ID
size_t codegenCreateEntryFunction();
size_t codegenCreateFunction(size_t parameter_count);

//returns/uses the codegen variable ID
//an abstraction for register allocation and similar
size_t codegenAllocateVariable(DataType type, size_t type_width);
void codegenDeallocateVariable(size_t variable_ID);

//uses the codegen variable IDs
void codegenEmit3AddressInstruction(Instruction instruction,
									Operand result, OperandType result_type,
									Operand input0, OperandType input0_type,
									Operand input1, OperandType input1_type);

void codegenEmitMove(Operand destination, OperandType destination_type, Operand source, OperandType source_type);
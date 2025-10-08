#include "../codegen.h"

#include <stdbool.h>
#include <stddef.h>

//just to make code more readable
enum RegisterID {
	RAX = 0,
	RBX = 1,
	RCX = 2,
	RDX = 3,
	RSI = 4,
	RDI = 5,
	RBP = 6,
	RSP = 7,
	R8 = 8,
	R9 = 9,
	R10 = 10,
	R11 = 11,
	R12 = 12,
	R13 = 13,
	R14 = 14,
	R15 = 15,
	XMM0 = 16,
	XMM1 = 17,
	XMM2 = 18,
	XMM3 = 19,
	XMM4 = 20,
	XMM5 = 21,
	XMM6 = 22,
	XMM7 = 23,
	XMM8 = 24,
	XMM9 = 25,
	XMM10 = 26,
	XMM11 = 27,
	XMM12 = 28,
	XMM13 = 29,
	XMM14 = 30,
	XMM15 = 31,
};

typedef struct {
	struct {
		bool live;
		size_t owner;
	} register_states[32];
} FunctionData;

void codegenWriteToFile(char* file_path) {

}

size_t codegenCreateEntryFunction() {

}

size_t codegenCreateFunction(size_t parameter_count) {

}

size_t codegenAllocateVariable(DataType type, size_t type_width) {

}

void codegenDeallocateVariable(size_t variable_ID) {

}

void codegenEmit3AddressInstruction(Instruction instruction,
									Operand result, OperandType result_type,
									Operand input0, OperandType input0_type,
									Operand input1, OperandType input1_type) {

}

void codegenEmitMove(Operand destination, OperandType destination_type, Operand source, OperandType source_type) {

}
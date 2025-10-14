#include "llvm_data.h"

#include <llvm-c/Core.h>
#include <string.h>

LLVMContextRef llvm_context = NULL;
LLVMModuleRef llvm_module = NULL;
LLVMBuilderRef llvm_builder = NULL;

void setupLLVM(const char* module_ID) {
	llvm_context = LLVMContextCreate();
	llvm_module = LLVMModuleCreateWithNameInContext(module_ID, llvm_context);
	llvm_builder = LLVMCreateBuilderInContext(llvm_context);
}

void destroyLLVM(void) {
	LLVMDisposeBuilder(llvm_builder);
	LLVMDisposeModule(llvm_module);
	LLVMContextDispose(llvm_context);
}

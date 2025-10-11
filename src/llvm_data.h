#pragma once

#include <llvm-c/Core.h>

extern LLVMContextRef llvm_context;
extern LLVMModuleRef llvm_module;
extern LLVMBuilderRef llvm_builder;

void setupLLVM(const char* module_ID);
void destroyLLVM();
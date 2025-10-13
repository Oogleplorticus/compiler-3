#pragma once

#include <llvm-c/Types.h>
#include <stddef.h>
#include <stdio.h>

#include <llvm-c/Core.h>

typedef struct {
	char* source_path;
	FILE* source_file;

	LLVMContextRef llvm_context;
	LLVMModuleRef llvm_module;

	char** identifiers;
	size_t identifier_count;

	struct {

	}* structs;

	struct {

	}* functions;
} CompilationUnit;

//creation/destruction
CompilationUnit createCompilationUnit(const char* source_path, LLVMContextRef llvm_context);
void destroyCompilationUnit(CompilationUnit* compilation_unit);
#include "compilation_unit.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

CompilationUnit createCompilationUnit(const char* source_path, LLVMContextRef llvm_context) {
	//initialise compilation unit
	CompilationUnit compilation_unit;
	memset(&compilation_unit, 0, sizeof(compilation_unit));

	//copy source path
	const size_t source_path_size = (strlen(source_path) + 1) * sizeof(char);
	compilation_unit.source_path = malloc(source_path_size);
	if (compilation_unit.source_path == NULL) {
		printf("ERROR: Failed to allocate %zu bytes for compilation unit source path %s!\n", source_path_size, source_path);
		exit(1);
	}
	strcpy(compilation_unit.source_path, source_path);

	//open source file
	compilation_unit.source_file = fopen(source_path, "r");
	if (compilation_unit.source_file == NULL) {
		printf("ERROR: Failed to open source file: %s!\n", source_path);
		exit(1);
	}

	//setup llvm
	compilation_unit.llvm_context = llvm_context;
	compilation_unit.llvm_module = LLVMModuleCreateWithNameInContext(
		compilation_unit.source_path, 
		compilation_unit.llvm_context
	);
	if (compilation_unit.llvm_module == NULL) {
		printf("ERROR: Failed to create llvm module for compilation unit!\n");
		exit(1);
	}

	return compilation_unit;
}

void destroyCompilationUnit(CompilationUnit* compilation_unit) {
	if (compilation_unit == NULL) return;

	//free file data
	free(compilation_unit->source_path);
	compilation_unit->source_path = NULL;

	if (compilation_unit->source_file != NULL) {
		fclose(compilation_unit->source_file);
		compilation_unit->source_file = NULL;
	}

	//dispose of llvm module and NULL llvm context
	LLVMDisposeModule(compilation_unit->llvm_module);
	compilation_unit->llvm_module = NULL;
	compilation_unit->llvm_context = NULL;
}
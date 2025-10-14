#include "compilation_unit.h"

#include <llvm-c/Core.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_LIST_CAPACITY 16

/*

creation/destruction functions

*/

CompilationUnit compilationUnit_create(const char* source_path, LLVMContextRef llvm_context) {
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

	//initialise lists
	//identifiers
	compilation_unit.identifier_capacity = INITIAL_LIST_CAPACITY;
	size_t identifiers_size = sizeof(compilation_unit.identifiers[0]) * INITIAL_LIST_CAPACITY;
	compilation_unit.identifiers = malloc(identifiers_size);
	memset(compilation_unit.identifiers, 0, identifiers_size);
	//structs
	compilation_unit.struct_capacity = INITIAL_LIST_CAPACITY;
	size_t structs_size = sizeof(compilation_unit.structs[0]) * INITIAL_LIST_CAPACITY;
	compilation_unit.structs = malloc(structs_size);
	memset(compilation_unit.structs, 0, structs_size);
	//variables
	compilation_unit.variable_capacity = INITIAL_LIST_CAPACITY;
	size_t variables_size = sizeof(compilation_unit.variables[0]) * INITIAL_LIST_CAPACITY;
	compilation_unit.variables = malloc(variables_size);
	memset(compilation_unit.variables, 0, variables_size);
	//functions
	compilation_unit.function_capacity = INITIAL_LIST_CAPACITY;
	size_t functions_size = sizeof(compilation_unit.functions[0]) * INITIAL_LIST_CAPACITY;
	compilation_unit.functions = malloc(functions_size);
	memset(compilation_unit.functions, 0, functions_size);

	return compilation_unit;
}

void compilationUnit_destroy(CompilationUnit* compilation_unit) {
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

/*

member list modification functions

*/

char* compilationUnit_getOrAddIdentifier(CompilationUnit* compilation_unit, const char* identifier) {
	//check if already in identifiers list
	for (size_t i = 0; i < compilation_unit->identifier_count; ++i) {
		//if strings are the same, return string pointer in identifiers list
		int comparison_result = strcmp(identifier, compilation_unit->identifiers[i]);
		if (comparison_result == 0) return compilation_unit->identifiers[i];
	}

	//not found in identifiers list
	//add to identifiers list

	//if at capacity then double capacity
	if (compilation_unit->identifier_count >= compilation_unit->identifier_capacity) {
		//attempt to double size
		size_t new_size = compilation_unit->identifier_capacity * sizeof(compilation_unit->identifiers[0]) * 2;
		char** new_list = realloc(compilation_unit->identifiers, new_size);
		if (new_list == NULL) {
			printf("ERROR: Failed to double capacity of identifiers list!\n");
			exit(1);
		}
		//set list and capacity if successful
		compilation_unit->identifiers = new_list;
		compilation_unit->identifier_capacity *= 2;
	}

	//allocate memory and copy string data
	char** new_identifier = compilation_unit->identifiers + compilation_unit->identifier_count;
	size_t identifier_size = (strlen(identifier) + 1) * sizeof(char); //+1 for null character

	*new_identifier = malloc(identifier_size);
	strcpy(*new_identifier, identifier);

	++compilation_unit->identifier_count;
	return *new_identifier;
}

StructType* compilationUnit_addStructType(CompilationUnit* compilation_unit) {
	//if at capacity then double capacity
	if (compilation_unit->struct_count >= compilation_unit->struct_capacity) {
		//attempt to double size
		size_t new_size = compilation_unit->struct_capacity * sizeof(compilation_unit->structs[0]) * 2;
		StructType* new_list = realloc(compilation_unit->structs, new_size);
		if (new_list == NULL) {
			printf("ERROR: Failed to double capacity of identifiers list!\n");
			exit(1);
		}
		//set list and capacity if successful
		compilation_unit->structs = new_list;
		compilation_unit->struct_capacity *= 2;
	}

	//get new element and increment count
	StructType* new_struct = compilation_unit->structs + compilation_unit->struct_count;
	memset(new_struct, 0, sizeof(*new_struct));
	++compilation_unit->struct_count;

	return new_struct;
}

Variable* compilationUnit_addVariable(CompilationUnit* compilation_unit) {
	//if at capacity then double capacity
	if (compilation_unit->variable_count >= compilation_unit->variable_capacity) {
		//attempt to double size
		size_t new_size = compilation_unit->variable_capacity * sizeof(compilation_unit->variables[0]) * 2;
		Variable* new_list = realloc(compilation_unit->variables, new_size);
		if (new_list == NULL) {
			printf("ERROR: Failed to double capacity of identifiers list!\n");
			exit(1);
		}
		//set list and capacity if successful
		compilation_unit->variables = new_list;
		compilation_unit->variable_capacity *= 2;
	}

	//get new element and increment count
	Variable* new_variable = compilation_unit->variables + compilation_unit->variable_count;
	memset(new_variable, 0, sizeof(*new_variable));
	++compilation_unit->variable_count;

	return new_variable;
}

Function* compilationUnit_addFunction(CompilationUnit* compilation_unit) {
	//if at capacity then double capacity
	if (compilation_unit->function_count >= compilation_unit->function_capacity) {
		//attempt to double size
		size_t new_size = compilation_unit->function_capacity * sizeof(compilation_unit->functions[0]) * 2;
		Function* new_list = realloc(compilation_unit->functions, new_size);
		if (new_list == NULL) {
			printf("ERROR: Failed to double capacity of identifiers list!\n");
			exit(1);
		}
		//set list and capacity if successful
		compilation_unit->functions = new_list;
		compilation_unit->function_capacity *= 2;
	}

	//get new element and increment count
	Function* new_function = compilation_unit->functions + compilation_unit->function_count;
	memset(new_function, 0, sizeof(*new_function));
	++compilation_unit->function_count;

	return new_function;
}

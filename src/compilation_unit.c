#include "compilation_unit.h"

#include <llvm-c/Core.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_LIST_CAPACITY 1

/*

creation/destruction

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
	if (compilation_unit.identifiers == NULL) {
		printf("ERROR: Failed to allocate memory for compilation unit identifiers!\n");
	}
	memset(compilation_unit.identifiers, 0, identifiers_size);
	//structs
	compilation_unit.struct_capacity = INITIAL_LIST_CAPACITY;
	size_t structs_size = sizeof(compilation_unit.structs[0]) * INITIAL_LIST_CAPACITY;
	compilation_unit.structs = malloc(structs_size);
	if (compilation_unit.structs == NULL) {
		printf("ERROR: Failed to allocate memory for compilation unit structs!\n");
	}
	memset(compilation_unit.structs, 0, structs_size);
	//variables
	compilation_unit.global_variable_capacity = INITIAL_LIST_CAPACITY;
	size_t variables_size = sizeof(compilation_unit.global_variables[0]) * INITIAL_LIST_CAPACITY;
	compilation_unit.global_variables = malloc(variables_size);
	if (compilation_unit.global_variables == NULL) {
		printf("ERROR: Failed to allocate memory for compilation unit global variables!\n");
	}
	memset(compilation_unit.global_variables, 0, variables_size);
	//functions
	compilation_unit.function_capacity = INITIAL_LIST_CAPACITY;
	size_t functions_size = sizeof(compilation_unit.functions[0]) * INITIAL_LIST_CAPACITY;
	compilation_unit.functions = malloc(functions_size);
	if (compilation_unit.functions == NULL) {
		printf("ERROR: Failed to allocate memory for compilation unit functions!\n");
	}
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

	//TODO free everything else
}

/*

member list modification

*/

size_t compilationUnit_getOrAddIdentifierIndex(CompilationUnit* compilation_unit, const char* identifier) {
	//check if already in identifiers list
	for (size_t i = 0; i < compilation_unit->identifier_count; ++i) {
		//if strings are the same, return string pointer in identifiers list
		int comparison_result = strcmp(identifier, compilation_unit->identifiers[i]);
		if (comparison_result == 0) return i;
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
	return compilation_unit->identifier_count - 1;
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

	//initialise members
	new_struct->identifier_index = NULL_INDEX;

	return new_struct;
}

Variable* compilationUnit_addVariable(CompilationUnit* compilation_unit) {
	//if at capacity then double capacity
	if (compilation_unit->global_variable_count >= compilation_unit->global_variable_capacity) {
		//attempt to double size
		size_t new_size = compilation_unit->global_variable_capacity * sizeof(compilation_unit->global_variables[0]) * 2;
		Variable* new_list = realloc(compilation_unit->global_variables, new_size);
		if (new_list == NULL) {
			printf("ERROR: Failed to double capacity of identifiers list!\n");
			exit(1);
		}
		//set list and capacity if successful
		compilation_unit->global_variables = new_list;
		compilation_unit->global_variable_capacity *= 2;
	}

	//get new element and increment count
	Variable* new_variable = compilation_unit->global_variables + compilation_unit->global_variable_count;
	memset(new_variable, 0, sizeof(*new_variable));
	++compilation_unit->global_variable_count;

	//initialise members
	new_variable->identifier_index = NULL_INDEX;

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

	//allocate initial memory for scope array
	new_function->scope_capacity = INITIAL_LIST_CAPACITY;
	size_t scopes_size = sizeof(new_function->scopes[0]) * INITIAL_LIST_CAPACITY;
	new_function->scopes = malloc(scopes_size);
	if (new_function->scopes == NULL) {
		printf("ERROR: Failed to allocate memory for function scopes!\n");
	}
	memset(new_function->scopes, 0, scopes_size);

	//allocate initial memory for parameter array
	new_function->parameter_capacity = INITIAL_LIST_CAPACITY;
	size_t parameters_size = sizeof(new_function->parameters[0]) * INITIAL_LIST_CAPACITY;
	new_function->parameters = malloc(parameters_size);
	if (new_function->parameters == NULL) {
		printf("ERROR: Failed to allocate memory for function parameters!\n");
	}
	memset(new_function->parameters, 0, parameters_size);

	//initialise members
	new_function->identifier_index = NULL_INDEX;

	return new_function;
}

Variable* compilationUnit_addFunctionParameter(Function* function) {
	//if at capacity then double capacity
	if (function->parameter_count >= function->parameter_capacity) {
		//attempt to double size
		size_t new_size = function->parameter_capacity * sizeof(function->parameters[0]) * 2;
		Variable* new_list = realloc(function->parameters, new_size);
		if (new_list == NULL) {
			printf("ERROR: Failed to double capacity of identifiers list!\n");
			exit(1);
		}
		//set list and capacity if successful
		function->parameters = new_list;
		function->parameter_capacity *= 2;
	}

	//get new element and increment count
	Variable* new_parameter = function->parameters + function->parameter_count;
	memset(new_parameter, 0, sizeof(*new_parameter));
	++function->parameter_count;

	//initialise members
	new_parameter->identifier_index = NULL_INDEX;

	return new_parameter;
}

Scope* compilationUnit_addFunctionScope(CompilationUnit* compilation_unit, Function* function) {
	//if at capacity then double capacity
	if (function->scope_count >= function->scope_capacity) {
		//attempt to double size
		size_t new_size = function->scope_capacity * sizeof(function->scopes[0]) * 2;
		Scope* new_list = realloc(function->scopes, new_size);
		if (new_list == NULL) {
			printf("ERROR: Failed to double capacity of identifiers list!\n");
			exit(1);
		}
		//set list and capacity if successful
		function->scopes = new_list;
		function->scope_capacity *= 2;
	}

	//get new element and increment count
	Scope* new_scope = function->scopes + function->scope_count;
	memset(new_scope, 0, sizeof(*new_scope));
	++function->scope_count;

	//allocate initial memory for variable array
	new_scope->variable_capacity = INITIAL_LIST_CAPACITY;
	size_t variables_size = sizeof(new_scope->variables[0]) * INITIAL_LIST_CAPACITY;
	new_scope->variables = malloc(variables_size);
	if (new_scope->variables == NULL) {
		printf("ERROR: Failed to allocate memory for scope variables!\n");
	}
	memset(new_scope->variables, 0, variables_size);

	//initialise members
	new_scope->parent_scope_index = NULL_INDEX;
	new_scope->parent_function_index = function - compilation_unit->functions;

	return new_scope;
}

Variable* compilationUnit_addScopeVariable(Scope* scope) {
	//if at capacity then double capacity
	if (scope->variable_count >= scope->variable_capacity) {
		//attempt to double size
		size_t new_size = scope->variable_capacity * sizeof(scope->variables[0]) * 2;
		Variable* new_list = realloc(scope->variables, new_size);
		if (new_list == NULL) {
			printf("ERROR: Failed to double capacity of identifiers list!\n");
			exit(1);
		}
		//set list and capacity if successful
		scope->variables = new_list;
		scope->variable_capacity *= 2;
	}

	//get new element and increment count
	Variable* new_variable = scope->variables + scope->variable_count;
	memset(new_variable, 0, sizeof(*new_variable));
	++scope->variable_count;

	//initialise members
	new_variable->identifier_index = NULL_INDEX;

	return new_variable;
}

/*

member list lookup

*/

Variable* compilationUnit_findVariableFromScope(CompilationUnit* compilation_unit, Function* parent_function, size_t scope_index, size_t variable_identifier_index) {
	if (scope_index == NULL_INDEX) return NULL;

	//get helpful pointer
	Scope* scope = parent_function->scopes + scope_index;

	//check for variable in this scope
	for (size_t i = 0; i < scope->variable_count; ++i) {
		if (variable_identifier_index == scope->variables[i].identifier_index) {
			return scope->variables + i;
		}
	}

	//if no parent scope check function parameters and globals
	if (scope->parent_scope_index == NULL_INDEX) {
		//parameters
		for (size_t i = 0; i < parent_function->parameter_count; ++i) {
			if (variable_identifier_index == parent_function->parameters[i].identifier_index) {
				return parent_function->parameters + i;
			}
		}
		//globals
		for (size_t i = 0; i < compilation_unit->global_variable_count; ++i) {
			if (variable_identifier_index == compilation_unit->global_variables[i].identifier_index) {
				return compilation_unit->global_variables + i;
			}
		}

		//if not found by now, variable does not exist
		return NULL;
	}

	//check for variable in parent scope
	return compilationUnit_findVariableFromScope(compilation_unit, parent_function, scope->parent_scope_index, variable_identifier_index);
}

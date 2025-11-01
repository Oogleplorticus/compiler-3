#pragma once

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

//used as an equivalent of null for indexes
#define NULL_INDEX ((size_t)-1)

/*

Variable and struct type structs/enum

*/

typedef enum {
	TYPE_NONE, //special case

	TYPE_INT,
	TYPE_UNSIGNED,
	TYPE_FLOAT,
	TYPE_CHAR,
	TYPE_BOOL,
	TYPE_VOID,
	TYPE_STRUCT,
} TypeKind;

//forward declarations
typedef struct StructType StructType;
typedef struct Function Function;

typedef struct {
	TypeKind kind;
	union {
		StructType* struct_type;
		size_t width;
	} data;
} VariableType;

typedef struct {
	size_t identifier_index; //in compilation unit member "identifiers"
	VariableType type;
} StructMember;

struct StructType {
	size_t identifier_index; //in compilation unit member "identifiers"
	StructMember* members;
};

/*

Variable and function structs

*/

typedef struct {
	size_t identifier_index; //in compilation unit member "identifiers"
	VariableType type;

	//llvm data
	LLVMValueRef llvm_stack_pointer;
	LLVMTypeRef llvm_type;
} Variable;

typedef struct Scope Scope;
struct Scope {
	size_t parent_function_index; //in compilation unit member "functions", will be initialised by creation function
	size_t parent_scope_index; //in parent function member "scopes"
	Variable* variables;
	size_t variable_count;
	size_t variable_capacity;
};

struct Function {
	size_t identifier_index; //in compilation unit member "identifiers"
	Variable* parameters;
	size_t parameter_count;
	size_t parameter_capacity;
	VariableType return_type;

	Scope* scopes;
	size_t scope_count;
	size_t scope_capacity;

	//llvm data
	LLVMTypeRef llvm_function_type;
	LLVMValueRef llvm_function;
	LLVMBasicBlockRef llvm_entry_block;
};

/*

Compilation unit

*/

//memory allocated for compilation unit members must live until the entire compilation unit is destroyed
typedef struct {
	char* source_path;
	FILE* source_file;

	LLVMContextRef llvm_context;
	LLVMModuleRef llvm_module;
	
	//it is important that all identifier pointers refer to data in this member
	//this allows for fast identifier checking via pointer comparison
	char** identifiers;
	size_t identifier_count;
	size_t identifier_capacity;

	StructType* structs;
	size_t struct_count;
	size_t struct_capacity;

	Variable* global_variables;
	size_t global_variable_count;
	size_t global_variable_capacity;

	Function* functions;
	size_t function_count;
	size_t function_capacity;
} CompilationUnit;

/*

Functions

*/

//creation/destruction
CompilationUnit compilationUnit_create(const char* source_path, LLVMContextRef llvm_context);
void compilationUnit_destroy(CompilationUnit* compilation_unit);

//member list modification
size_t compilationUnit_getOrAddIdentifierIndex(CompilationUnit* compilation_unit, const char* identifier);
StructType* compilationUnit_addStructType(CompilationUnit* compilation_unit);
Variable* compilationUnit_addGlobalVariable(CompilationUnit* compilation_unit);

Function* compilationUnit_addFunction(CompilationUnit* compilation_unit);
Variable* compilationUnit_addFunctionParameter(Function* function);
Scope* compilationUnit_addFunctionScope(CompilationUnit* compilation_unit, Function* function);
Variable* compilationUnit_addScopeVariable(Scope* scope);

//member list lookup
Variable* compilationUnit_findVariableFromScope(CompilationUnit* compilation_unit, Function* parent_function, size_t scope_index, size_t variable_identifier_index);

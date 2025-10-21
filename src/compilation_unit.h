#pragma once

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/*

Variable and struct type structs/enum

*/

typedef enum {
	TYPE_INT,
	TYPE_UNSIGNED,
	TYPE_FLOAT,
	TYPE_CHAR,
	TYPE_BOOL,
	TYPE_VOID,
	TYPE_STRUCT,
} TypeKind;

typedef struct StructType StructType; //forward declaration

typedef struct {
	TypeKind kind;
	union {
		StructType* struct_type;
		size_t width;
	} data;
} VariableType;

typedef struct {
	char* identifier; //in compilation unit member "identifiers", do not allocate new memory
	VariableType type;
} StructMember;

struct StructType {
	char* identifier; //in compilation unit member "identifiers", do not allocate new memory
	StructMember* members;
};

/*

Variable and function structs

*/

typedef struct {
	char* identifier; //in compilation unit member "identifiers", do not allocate new memory
	VariableType type;
} Variable;

typedef struct Scope Scope;
struct Scope {
	Scope* parent_scope; //in parent function member "scopes", do not allocate new memory
	Variable* variables;
	size_t variable_count;
	size_t variable_capacity;
};

typedef struct {
	char* identifier; //in compilation unit member "identifiers", do not allocate new memory
	Variable* parameters;
	size_t parameter_count;
	size_t parameter_capacity;
	VariableType returnType;

	Scope* scopes;
	size_t scope_count;
	size_t scope_capacity;

	//llvm data
	LLVMTypeRef llvm_function_type;
	LLVMValueRef llvm_function;
} Function;

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
char* compilationUnit_getOrAddIdentifier(CompilationUnit* compilation_unit, const char* identifier);
StructType* compilationUnit_addStructType(CompilationUnit* compilation_unit);
Variable* compilationUnit_addGlobalVariable(CompilationUnit* compilation_unit);

Function* compilationUnit_addFunction(CompilationUnit* compilation_unit);
Variable* compilationUnit_addFunctionParameter(Function* function);
Scope* compilationUnit_addFunctionScope(Function* function);
Variable* compilationUnit_addScopeVariable(Scope* scope);

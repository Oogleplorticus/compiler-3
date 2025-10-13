#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <llvm-c/Core.h>

#include "compilation_unit.h"
#include "declaration_parser.h"
#include "definition_parser.h"

int main(int argc, char* argv[]) {
	//handle command line arguments
	if (argc != 2) {
		printf("ERROR: Incorrect argument count!\n");
		return 1;
	}
	char* source_path = argv[1];

	//setup LLVM context
	LLVMContextRef llvm_context = LLVMContextCreate();
	
	//setup compilation unit
	CompilationUnit compilation_unit = createCompilationUnit(source_path, llvm_context);
	LLVMSetTarget(compilation_unit.llvm_module, "x86_64-pc-linux-gnu"); //assume target

	//compile
	parseDeclarations(&compilation_unit);
	parseDefinitions(&compilation_unit);

	//output result
	char output_path[strlen(source_path) + sizeof(".ll")];
	strcpy(output_path, source_path);
	strcat(output_path, ".ll");

	char* ll_error_message;
	bool ll_failure = LLVMPrintModuleToFile(compilation_unit.llvm_module, output_path, &ll_error_message);
	if (ll_failure) {
		printf("ERROR: Failed to output llvm code: %s\n", ll_error_message);
    	LLVMDisposeMessage(ll_error_message);
	}

	//free resources
	destroyCompilationUnit(&compilation_unit);
	LLVMContextDispose(llvm_context);

	return 0;
}
#include <llvm-c/TargetMachine.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Transforms/PassBuilder.h>

#include "llvm_data.h"
#include "parser.h"
#include "tokeniser.h"

int main(int argc, char* argv[]) {
	//handle command line arguments
	if (argc != 2) {
		printf("ERROR: Incorrect argument count!\n");
		return 1;
	}
	char* source_path = argv[1];

	//open source file
	FILE* source_file;
	source_file = fopen(source_path, "r");
	if (source_file == NULL) {
		printf("ERROR: Failed to open source file: %s!\n", source_path);
		return 1;
	}

	//setup LLVM
	setupLLVM(source_path);
	LLVMSetTarget(llvm_module, "x86_64-pc-linux-gnu"); //assume target

	//give file to tokeniser and parse file
	tokeniserSetSource(source_file);
	parseTokens();

	//output result
	char output_path[strlen(source_path) + sizeof(".ll")];
	strcpy(output_path, source_path);
	strcat(output_path, ".ll");

	char* ll_error_message;
	bool ll_failure = LLVMPrintModuleToFile(llvm_module, output_path, &ll_error_message);
	if (ll_failure) {
		printf("ERROR: Failed to output llvm code: %s\n", ll_error_message);
    	LLVMDisposeMessage(ll_error_message);
	}

	//free resources
	fclose(source_file);
	destroyLLVM();

	return 0;
}
#include <llvm-c/Core.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

	//give file to tokeniser and parse file
	tokeniserSetSource(source_file);
	parseTokens();

	//output result
	char* ll_error_message;
	bool ll_success = LLVMPrintModuleToFile(llvm_module, strcat(source_path, ".ll"), &ll_error_message);
	if (!ll_success) {
		printf("ERROR: Failed to output llvm code: %s\n", ll_error_message);
    	LLVMDisposeMessage(ll_error_message);
	}

	//free resources
	fclose(source_file);
	destroyLLVM();

	return 0;
}
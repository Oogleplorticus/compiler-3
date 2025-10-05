#include <stdio.h>
#include <string.h>

#include "codegen.h"
#include "parser.h"
#include "tokeniser.h"

int main(int argc, char* argv[]) {
	//handle command line arguments
	if (argc != 2) {
		printf("ERROR: Incorrect argument count!\n");
		return 1;
	}
	char* source_path = argv[1];

	//get source file extension
	char* source_file_extension = strrchr(source_path, '.');
	if (source_file_extension == NULL) {
		printf("ERROR: Source file has no extension!\n");
		return 1;
	}

	//store copy of source path without extension
	size_t source_path_without_extension_length = source_file_extension - source_path + 1;
	char source_path_without_extension[source_path_without_extension_length];

	memccpy(source_path_without_extension, source_file_extension, sizeof(char), source_path_without_extension_length);
	source_path_without_extension[source_path_without_extension_length - 1] = '\0'; //null terminate

	//open source file
	FILE* source_file;
	source_file = fopen(source_path, "r");
	if (source_file == NULL) {
		printf("ERROR: Failed to open source file: %s!\n", source_path);
		return 1;
	}

	//give file to tokeniser and parse file
	tokeniserSetSource(source_file);
	parseTokens();

	//write to destination file
	codegenWriteToFile(source_path_without_extension);

	//close source file
	fclose(source_file);

	return 0;
}
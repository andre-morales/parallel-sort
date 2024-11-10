#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

typedef struct {
	int key;
	char data[96];
} Record;

int main(int argc, char* argv[]) {
	assert(sizeof(Record) == 100);

	char* inputFile = argv[1];
	char* resultFile = "result.out";

	struct stat fileInfo;

	// Open true input file
	int inFD = open(inputFile, O_RDONLY);
	if (inFD == -1) {
		printf("Could't open input file '%s'\n", inputFile);
		return -1;
	}
	fstat(inFD, &fileInfo);

	size_t inFileSize = fileInfo.st_size;
	Record* input = mmap(NULL, inFileSize, PROT_READ, MAP_PRIVATE, inFD, 0);

	// Open result output
	int resFD = open(resultFile, O_RDONLY);
	if (resFD == -1) {
		printf("Could't open result file '%s'\n", resultFile);
		return -1;
	}
	fstat(resFD, &fileInfo);

	size_t resFileSize = fileInfo.st_size;
	Record* result = mmap(NULL, resFileSize, PROT_READ, MAP_PRIVATE, resFD, 0);

	// 1: Check file size
	if (inFileSize != resFileSize) {
		printf("DIFF: Different file sizes.");
		return -1;
	}

	printf("File sizes ok!\n");

	size_t entryCount = inFileSize / sizeof(Record);

	if (argc > 2) {
		char* op = argv[2];
		if(strcmp(op, "list") == 0) {
			printf(":: INPUT\n");
			for (int i = 0; i < entryCount; i++) {
				printf("[%i:%i]\n", i, input[i].key);
			}

			printf("\n:: RESULT\n");
			for (int i = 0; i < entryCount; i++) {
				printf("[%i:%i]\n", i, result[i].key);
			}
		}
	}

	int lastKey = INT_MIN;
	for (int i = 0; i < entryCount; i++) {
		if (entryCount > 20 && i % (entryCount / 20) == 0) {
			printf("%lu%%\n", i * 100 / entryCount);
		}

		Record* entry = &result[i];
		//printf(":: Entry [%i:%i]\n", i, entry->key);

		// Ensure order of keys in the result
		if (entry->key < lastKey) {
			printf("Entry [%i, %i] has key smaller than last.\n", i, entry->key);
			return -1;
		}

		// Find original input entry with the result key
		Record* inEntry = NULL;
		for (int k = 0; k < entryCount; k++) {
			if (input[k].key == entry->key) {
				inEntry = &input[k];
				break;
			}
		}
		if (!inEntry) {
			printf("Entry [%i: %i] appears in the destination but not in the source file.\n", i, entry->key);
			return -1;
		}
		
		// Compare entry data
		if (memcmp(entry, inEntry, sizeof(Record)) != 0) {
			printf("Entry %i has mismatched data.\n", entry->key);
			return -1;
		}
		lastKey = entry->key;
	}

	printf("All tests passed.\n");
	return 0;
}
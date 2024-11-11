#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
	int key;
	char data[96];
} Record;

typedef struct {
	int fd;
	size_t fileSize;
	size_t numRecords;
	Record* data;
} File;

File openFile(const char*);

bool quietMode = false;

int main(int argc, char* argv[]) {
	assert(sizeof(Record) == 100);

	if (argc <= 1) {
		fprintf(stderr, "Incorrect usage. Use ./verify <input.dat>.\n");
		return -1;
	}

	File inputFile = openFile(argv[1]);
	File resultFile = openFile("result.out");

	size_t inFileSize = inputFile.fileSize;
	Record* input = inputFile.data;

	size_t resFileSize = resultFile.fileSize;
	Record* result = resultFile.data;

	// 1: Check file size
	if (inFileSize != resFileSize) {
		printf("DIFF: Different file sizes.\n");
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

		if (strcmp(op, "q") == 0) {
			quietMode = true;
		}
	}

	int lastKey = INT_MIN;
	bool printProgress = !quietMode && entryCount > 20;
	for (int i = 0; i < entryCount; i++) {
		if (printProgress && i % (entryCount / 20) == 0) {
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

File openFile(const char* fileName) {
	File file;
	
	file.fd = open(fileName, O_RDONLY);
	if (file.fd == -1) {
		printf("Could't open file '%s'\n", fileName);
		exit(-1);
	}

	struct stat fileInfo;
	fstat(file.fd, &fileInfo);
	file.fileSize = fileInfo.st_size;

	Record* data = mmap(NULL, file.fileSize, PROT_READ, MAP_PRIVATE, file.fd, 0);
	file.data = data;
	return file;
}
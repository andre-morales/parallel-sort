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
	bool checked;
	int key;
	char* data;
} CheckedRecord;

typedef struct {
	int fd;
	size_t fileSize;
	size_t numRecords;
	Record* data;
} File;

File openFile(const char*);
CheckedRecord* toCheckedRecords(Record*, size_t);

bool quietMode = false;

int main(int argc, char* argv[]) {
	if (argc <= 1) {
		fprintf(stderr, "Incorrect usage. Use ./verify <input.dat>.\n");
		return -1;
	}

	File inputFile = openFile(argv[1]);
	File resultFile = openFile("result.out");

	size_t inFileSize = inputFile.fileSize;
	size_t resFileSize = resultFile.fileSize;
	
	// 1: Check file size
	if (inFileSize != resFileSize) {
		printf("[!] ERROR: Different file sizes.\n");
		return -1;
	}

	size_t entryCount = inFileSize / sizeof(Record);
	CheckedRecord* input = toCheckedRecords(inputFile.data, entryCount);
	//Record* input = inputFile.data;
	Record* result = resultFile.data;

	if (argc > 2) {
		char* op = argv[2];
		if(strcmp(op, "list") == 0) {
			printf(":: INPUT\n");
			for (int i = 0; i < entryCount; i++) {
				printf("[%i:%8X %i]\n", i, input[i].key, input[i].key);
			}

			printf("\n:: RESULT\n");
			for (int i = 0; i < entryCount; i++) {
				printf("[%i:%8X %i]\n", i, result[i].key, result[i].key);
			}
		}

		if (strcmp(op, "q") == 0) {
			quietMode = true;
		}
	}

	int lastKey = INT_MIN;
	bool printProgress = !quietMode && entryCount > 20;
	for (int i = 0; i < entryCount; i++) {
		Record* entry = &result[i];

		if (printProgress && i % (entryCount / 20) == 0) {
			printf("%lu%%\n", i * 100 / entryCount);
		}

		// Ensure order of keys in the result
		if (entry->key < lastKey) {
			printf("Entry [%i, %i] has key smaller than last.\n", i, entry->key);
			return -1;
		}

		// Find original input entry with the result key
		CheckedRecord* inEntry = NULL;
		for (int k = 0; k < entryCount; k++) {
			if (input[k].checked) continue;

			if (input[k].key == entry->key) {
				inEntry = &input[k];
				break;
			}
		}

		if (!inEntry) {
			printf("Entry [%i: %i] appears in the destination but not in the source file.\n", i, entry->key);
			return -1;
		}

		// Mark this entry as being checked already.
		inEntry->checked = true;
		
		// Compare entry data
		if (memcmp(entry->data, inEntry->data, 96) != 0) {
			printf("[!] Entry [%i: %i] has mismatched data.\n", i, entry->key);
			return -1;
		}

		lastKey = entry->key;
	}

	printf("All tests passed.\n");
	return 0;
}

CheckedRecord* toCheckedRecords(Record* input, size_t count) {
	CheckedRecord* arr = malloc(sizeof(CheckedRecord) * count);
	for (int i = 0; i < count; i++) {
		arr[i].checked = false;
		arr[i].key = input[i].key;
		arr[i].data = input[i].data;
	}
	return arr;
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
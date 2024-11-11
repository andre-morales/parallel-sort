#include "psort.h"
#include "merge_sort.h"
#include "radix_sort.h"
#include "cond_lock.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
	Record* data;
	size_t fileSize;
	size_t entryCount;
} TInput;

typedef struct {
	Record* data;
	int file;
} TOutput;

typedef struct {
	int index;
	pthread_t thread;
	size_t entryCount;
	size_t offset;

	// Used in the merging step of the algorithm. Represents how many records this thread
	// has accumulated with others.
	size_t accumulatedSize;

	// If this thread has done it's individual sorting portion already. Only access this flag
	// with sortLock.
	bool sorted;
	ConditionLock sortLock;
} Thread;

void openInput(const char*);
void openOutput(const char*);
void closeOutput();
void* threadMain(void*);
void selectionSort(Key* data, size_t entryCount);
void spawnThreads(int);
void entriesToKeys(Record *entries, SortKey *keys, size_t entryCount);
void keysToEntries(SortKey *keys, Record *entries, size_t entryCount);
void mmapFailed();

struct {
	volatile bool createdThreads;
	int numThreads;
	Thread threads[64];
	TInput input;
	TOutput output;
	Key* bufferA;
	Key* bufferB;
	ConditionLock keyCoalesceLock;
	bool keyColaescingStarted;
} World;

int main(int argc, char* argv[]) {	
	if (argc <= 3) {
		fprintf(stderr, "Uso incorreto. Utilize com <entrada> <saída> <threads>\n");
		return -1;
	}
	
	char* inputFile = argv[1];
	char* outputFile = argv[2];
	char* strThreads = argv[3];

	char* endPtr = NULL;
	int threadCount = strtol(strThreads, &endPtr, 10);
	World.numThreads = threadCount;

	if (endPtr == strThreads || threadCount < 0) {
		fprintf(stderr, "Invalid thread count. N >= 0.");
		return -1;
	}

	// Open files
	openInput(inputFile);	
	openOutput(outputFile);

	// Allocate master memory buffer
	size_t bufferSize = World.input.entryCount * sizeof(Key);
	Key* masterBuffer = mmap(NULL, 2 * bufferSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (masterBuffer == MAP_FAILED) mmapFailed();
	
	World.bufferA = &masterBuffer[0];
	World.bufferB = &masterBuffer[World.input.entryCount];

	// Spawn N threads
	spawnThreads(threadCount);

	closeOutput();
	return 0; 
}

void* threadMain(void* threadInputArg) {
	Thread* params = (Thread*) threadInputArg;

	const size_t entryCount = params->entryCount;
	Record* const input = &World.input.data[params->offset];
	Record* const output = &World.output.data[params->offset];
	
	Key* bufferA = &World.bufferA[params->offset];
	Key* bufferB = &World.bufferB[params->offset];

	//printf("T%i Handling N=%lu [%lu, %lu]\n", params->index, params->entryCount, params->offset, params->offset + params->entryCount - 1);

	// Convert full records to sorting entries in buffer A
	entriesToKeys(input, bufferA, entryCount);

	// Perform radix sort on buffer A. The results remain in buffer A
	radixSort(bufferA, entryCount, bufferB);
	/*Key* tmp = bufferA;
	bufferA = bufferB;
	bufferB = tmp;*/

	// In the highly unlikely case this thread finishes sorting before the other threads are even
	// done being created, just spin wait for a few cycles.
	while(!World.createdThreads){}

	// Now, we will perform a parallel merge. This thread will wait for its brothers to finish
	// their sorting portion, then will join their work with ours.
	// For example, if we have 4 threads, here's what each thread will do:
	// 0: Will join the work by 1, then all the work of 2
	// 1: Will just quit.
	// 2: Will join the work by 3
	// 3: Will just quit.
	int thIndex = params->index;
	int numThreads = World.numThreads;

	// Current size will accumulate the size of the work joined from other threads
	size_t currentSize = entryCount;

	for (int level = 1; level < numThreads; level *= 2) {
		// Not a multiple of the current level, nothing to do anymore.
		if (thIndex % (level * 2) != 0) break;
		
		// If the target thead index doesn't exit, quit. We only quit here if the global number of
		// threads isn't a power of 2.
		int targetIndex = thIndex + level;
		if (targetIndex >= numThreads) break;

		Thread* target = &World.threads[targetIndex];
		//printf("%i: Joining with %i\n", index, targetIndex);
			
		// Wait for the target thread to finish it's sorting portion
		cl_lock(&target->sortLock);
		while (!target->sorted) {
			cl_wait(&target->sortLock);
		}
		cl_unlock(&target->sortLock);

		size_t targetOffset = target->offset;
		size_t targetSize = target->accumulatedSize;
		size_t finalSize = currentSize + targetSize;
		//printf("%i: Merging: %lu [%lu, %lu] and %lu [%lu, %lu]\n", index, currentSize, params->offset, params->offset + currentSize - 1, targetSize, targetOffset, targetOffset + targetSize - 1);
	
		// Merge my half and the joined half both in buffer A into buffer B
		mergeP(bufferA, finalSize, currentSize, bufferB);
		
		// Copy buffer B to buffer A
		memcpy(bufferA, bufferB, finalSize * sizeof(Key));	

		currentSize += targetSize;
	}

	// I am done joining the work of others. This is my final accumulated size. If any threads
	// use my work to join into theirs, this size will be used to account for all records.
	params->accumulatedSize = currentSize;

	// Notify other threads that this one has finished its sorting.
	cl_lock(&params->sortLock);
	params->sorted = true;
	cl_notify(&params->sortLock);
	cl_unlock(&params->sortLock);

	// If I am the first thread, no thread will wait for me. Also, I will always accumulate the
	// sorted arrays from everyone.
	// I am responsible for starting the final key coalescing phase.
	if (thIndex == 0) {
		cl_lock(&World.keyCoalesceLock);
		World.keyColaescingStarted = true;
		cl_notifyAll(&World.keyCoalesceLock);
		cl_unlock(&World.keyCoalesceLock);
	}

	// Wait for the key coalescing signal. When this signal arrives, it means the final merge step
	// has been finished, and the keys just need to be transformed back to records in the output
	// memory mapped file.
	cl_lock(&World.keyCoalesceLock);
	while (!World.keyColaescingStarted) {
		cl_wait(&World.keyCoalesceLock);
	}
	cl_unlock(&World.keyCoalesceLock);

	// Coalesce only my own original key range. The other threads will coalesce theirs as well
	// in parallel.
	keysToEntries(bufferA, output, entryCount);

	return NULL;
}

void spawnThreads(int threadCount) {
	cl_init(&World.keyCoalesceLock);
	World.keyColaescingStarted = false;

	size_t division = World.input.entryCount / threadCount;
	size_t remainder = World.input.entryCount % threadCount;

	size_t offset = 0;
	World.createdThreads = false;
	for (int i = 0; i < threadCount; i++) {
		size_t count = division;
		if (remainder != 0) {
			count++;
			remainder--;
		}

		Thread* t = &World.threads[i];
		t->index = i;
		t->entryCount = count;
		t->offset = offset;
		t->sorted = false;
		t->sortLock;
		cl_init(&t->sortLock);
		pthread_create(&World.threads[i].thread, NULL, threadMain, &World.threads[i]);

		offset += count;
	}

	World.createdThreads = true;
	
	// Wait for all N threads to finish execution.
	for (int i = 0; i < threadCount; i++) {
		pthread_join(World.threads[i].thread, NULL);
	}

	//pthread_join(World.threads[0].thread, NULL);
}

void entriesToKeys(Record* entries, SortKey* keys, size_t entryCount) {
	for (int i = 0; i < entryCount; i++) {
		keys[i].key = entries[i].key;
		keys[i].data = &entries[i].data;
	}
}

void keysToEntries(SortKey* keys, Record* entries, size_t entryCount) {
	for (int i = 0; i < entryCount; i++) {
		entries[i].key = keys[i].key;
		entries[i].data = *keys[i].data;		
	}
}

void openInput(const char* inputFile) {
	// Open input stream
	int fd = open(inputFile, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Erro ao abrir o arquivo '%s'\n", inputFile);
		exit(-1);
	}

	// Query file size
	struct stat fileInfo;
	if (fstat(fd, &fileInfo) == -1) {
		fprintf(stderr, "Erro ao ler tamanho de '%s'\n", inputFile);
		exit(-1);
	}

	size_t fileSize = fileInfo.st_size;
	Record* data = mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (data == MAP_FAILED) {
		fprintf(stderr, "Input mapping failed.\n");
		exit(-1);
	}

	World.input.data = data;
	World.input.fileSize = fileSize;
	World.input.entryCount = fileSize / sizeof(Record);
}

void openOutput(const char* outputFile) {
	// Open output stream
	int fd = open(outputFile, O_RDWR | O_CREAT | O_TRUNC, 0664);
	if (fd == -1) {
		fprintf(stderr, "Error opening output file '%s'\n", outputFile);
		exit(-1);
	}

	// Set output file to the same size as the input file.
	size_t fileSize = World.input.entryCount * sizeof(Record);
	if (ftruncate(fd, fileSize) == -1) {
		fprintf(stderr, "Output truncate failed.\n");
		exit(-1);
	}

	// Perform output memory mapping
	Record* data = mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		fprintf(stderr, "Output mapping failed.\n");
		exit(-1);
	}

	World.output.data = data;
	World.output.file = fd;
}

void closeOutput() {
	size_t fileSize = World.input.entryCount * sizeof(Record);

	if (msync(World.output.data, fileSize, MS_ASYNC) == -1) {
		fprintf(stderr, "Output sync failure.\n");
		exit(-1);
	}

	if (munmap(World.output.data, fileSize) != 0) {
		fprintf(stderr, "Output unmap failed.\n");
	}

	if (close(World.output.file) != 0) {
		fprintf(stderr, "Output close failed.\n");
	}	
}

void mmapFailed() {
	fprintf(stderr, "mmap() call failed. %i\n", errno);
	exit(-1);
}
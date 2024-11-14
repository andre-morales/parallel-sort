#include "psort.h"
#include "radix_sort.h"
#include "cond_lock.h"
#include "barrier.h"
#include "slow_barrier.h"
#include "allocator.h"
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

struct Thread_t;
typedef struct Thread_t Thread;

typedef struct {
	volatile bool createdThreads;
	int numThreads;
	Thread* threads;
	TInput input;
	TOutput output;
	Key* bufferA;
	Key* bufferB;

	// Radix sort state
	int rxTally[RADIX_COUNT];

	Barrier rxPassBarrier;
	SlowBarrier rxPrefixBarrier;
} World_t;

typedef struct Thread_t {
	int index;
	pthread_t thread;

	// How many records this thread will handle in sorting
	int recordCount;

	// Offset into the key array that this thread is responsible for sorting
	int offset;

	// In the distributed tally phase, how many digits will this thread handle
	int rxCountSize;

	// In the distributed tally phase, what index should this thread start counting from
	int rxCountOffset;

	// Personal count array of this thread
	int* radixCount;

	// Accumulated count array of this thread
	int* acculCount;
} Thread;

World_t World;

void openInput(const char*);
void openOutput(const char*);
void closeOutput();
void spawnThreads(int);
void* threadMain(void*);
void threadFullRadix(Thread*, Key*, Key*, size_t);
void radixPass(Thread*, Key*, Key*, int, int);
void radixParallelTally(Thread*);
void entriesToKeys(Record*, SortKey*, size_t);
void keysToEntries(SortKey*, Record*, size_t);

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

	// Spawn N threads
	spawnThreads(threadCount);

	closeOutput();
	return 0; 
}

void spawnThreads(int threadCount) {
	// Allocate major Key buffers
	mm_alloc(&World.bufferA, sizeof(Key[World.input.entryCount]));
	mm_alloc(&World.bufferB, sizeof(Key[World.input.entryCount]));

	// Allocate count and accumulated count block
	int* countArray;
	mm_alloc(&countArray, sizeof(int[RADIX_COUNT]) * threadCount * 2);

	// Allocate thread block
	mm_alloc(&World.threads, sizeof(Thread[threadCount]));
	mm_finish();

	// Initialize barriers
	barr_init(&World.rxPassBarrier, threadCount);
	slowbarr_init(&World.rxPrefixBarrier, threadCount);

	World.createdThreads = false;

	int division = World.input.entryCount / threadCount;
	int remainder = World.input.entryCount % threadCount;
	int offset = 0;

	for (int i = 0; i < threadCount; i++) {
		Thread* t = &World.threads[i];
		t->index = i;
		t->offset = offset;
		t->recordCount = division + (remainder-- > 0);
		t->radixCount = &countArray[(i * 2 + 0) * RADIX_COUNT];
		t->acculCount = &countArray[(i * 2 + 1) * RADIX_COUNT];

		// Every thread will be responsible for a portion of the global count array. This section
		// splits the count array fairly even if the number of threads isn't a multiple of the
		// count array size.
		{
			int div = RADIX_COUNT / World.numThreads;
			int rem = RADIX_COUNT % World.numThreads;
			bool compensate = i < rem;
			
			// Give an extra element to the first few threads
			t->rxCountSize = div + compensate;

			// Determine exactly where the count index should start for this thread
			t->rxCountOffset = i * div + ((compensate ? i : rem));
		}

		pthread_create(&World.threads[i].thread, NULL, threadMain, &World.threads[i]);

		offset += t->recordCount;
	}

	World.createdThreads = true;
	
	// Wait for all N threads to finish execution.
	for (int i = 0; i < threadCount; i++) {
		pthread_join(World.threads[i].thread, NULL);
	}
}

void* threadMain(void* threadInputArg) {
	Thread* thread = (Thread*) threadInputArg;
	const size_t entryCount = thread->recordCount;
	Record* const input = &World.input.data[thread->offset];
	Record* const output = &World.output.data[thread->offset];
	
	Key* bufferA = &World.bufferA[thread->offset];
	Key* bufferB = &World.bufferB[thread->offset];

	// Convert full records to sorting entries in buffer A
	entriesToKeys(input, bufferA, entryCount);

	threadFullRadix(thread, bufferA, bufferB, entryCount);

	// Coalesce only my own original key range. The other threads will coalesce theirs as well
	// in parallel.
	keysToEntries(bufferA, output, entryCount);
	return NULL;
}

void threadFullRadix(Thread* thread, Key* bufferA, Key* bufferB, size_t entryCount) {
	// Run the radix passes
	radixPass(thread, bufferA, World.bufferB, 0, entryCount);
	radixPass(thread, bufferB, World.bufferA, 1, entryCount);
}

void radixPass(Thread* thread, Key* source, Key* dest, int pass, int entryCount) {
	int* count = thread->radixCount;
	memset(count, 0, RADIX_COUNT * sizeof(int));
	
	// Perform the parallel counting portion
	radixCount(source, entryCount, pass * RADIX_BITS, count);

	// Perform the tallying in parallel
	radixParallelTally(thread);

	// Now the perform the final coalescing step in parallel
	radixCoalesceExt(source, entryCount, pass * RADIX_BITS, World.rxTally, thread->acculCount, dest);

	// Everyone must finish their coalescing to consider the pass finished.
	barr_wait(&World.rxPassBarrier);
}

void radixParallelTally(Thread* thread) {
	const int thIndex = thread->index;
	const int countSize = thread->rxCountSize;
	const int countOffset = thread->rxCountOffset;

	// Wait for all threads to finish counting in the earlier phase.
	barr_wait(&World.rxPassBarrier);

	// Zero-out the portion of the array i'm responsible for
	int* myDigits = &World.rxTally[countOffset];
	memset(myDigits, 0, sizeof(int[countSize]));

	for (int t = World.numThreads - 1; t >= 0; t--) {
		Thread* th = &World.threads[t];

		// Every thread gets an accumulated count corresponding to the current count array
		int* acculCount = &th->acculCount[countOffset];
		int* radixCount = &th->radixCount[countOffset];
		for (int i = 0; i < countSize; i++) {
			// The accumulated count for this digit is the count i've seen so far
			acculCount[i] = myDigits[i];

			// Add to my digit count the count of this array
			myDigits[i] += radixCount[i];
		}
	}

	// Only a single thread converts the count to a prefix array
	if(slowbarr_wait(&World.rxPrefixBarrier)) {
		radixCountToPrefix(World.rxTally);
		slowbarr_lower(&World.rxPrefixBarrier);
	}
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

	if (msync(World.output.data, fileSize, MS_SYNC) == -1) {
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

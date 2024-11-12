#include "psort.h"
#include "merge_sort.h"
#include "radix_sort.h"
#include "cond_lock.h"
#include "barrier.h"
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

	// If this thread has done it's counting part of the radix sort algorithm. 
	bool counted;
	ConditionLock countLock;

	int radixCount[RADIX_16];
	int acculCount[RADIX_16];
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
void preFault(void* buffer, int size);
void printKeys(const Key* arr, int n);
void parallelMerge(Thread* thread, Key* bufferA, Key* bufferB, size_t entryCount);

struct {
	volatile bool createdThreads;
	int numThreads;
	Thread threads[64];
	TInput input;
	TOutput output;
	Key* bufferA;
	Key* bufferB;

	bool keyColaescingStarted;
	ConditionLock keyCoalesceLock;

	// Radix sort state
	int radixTally[RADIX_16];

	int radixThreadsFinishedCoalescing;
	ConditionLock radixThreadsFinishedCoalescingLock;

	int radixTallyCompleted;
	ConditionLock radixTallyCompletedLock;

	Barrier radixCountedBarrier;
	Barrier radixPass2Barrier;
} World;

int main(int argc, char* argv[]) {	
	/*Record a = {0x00000001, {{'a'}}};
	Record b = {0x00000001, {{'b'}}};
	Record c = {0x00000002, {{'c'}}};
	Record d = {0x00000001, {{'d'}}};

	openInput("400b.dat");
	openOutput("cool.dat");
	World.output.data[0] = a;
	World.output.data[1] = b;
	World.output.data[2] = c;
	World.output.data[3] = d;
	closeOutput();*/
	//return 0;

	/*int countA[65536];
	int countB[65536];
	
	

	
		Key elementsA[] = {{4}, {9}, {1}, {0}};
		memset(countA, 0, sizeof(countA));
		radixCount(elementsA, 4, 0, countA);
		//radixCountToPrefix(count);
	
		Key elementsB[] = {{6}, {5}, {2}, {3}};
		memset(countB, 0, sizeof(countB));
		radixCount(elementsB, 4, 0, countB);
	

	int tally[65536];
	memset(tally, 0, sizeof(tally));
	radixTallyCount(tally, countA);
	radixTallyCount(tally, countB);

	radixCountToPrefix(tally);

	Key result[8];
	radixCoalesceExt(elementsB, 4, 0, countB, tally, result);
	radixCoalesceExt(elementsA, 4, 0, countA, tally, result);


	printKeys(result, 8);
	//radixCoalesce()
	return 0;*/

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

void radixPass(Thread* thread, Key* source, Key* dest, int pass, size_t entryCount) {
	int* count = thread->radixCount;
	memset(count, 0, RADIX_16 * sizeof(int));
	radixCount(source, entryCount, pass * 16, count);

	// Notify other threads that i'm done counting.
	/*cl_lock(&thread->countLock);
	thread->counted = true;
	cl_notify(&thread->countLock);
	cl_unlock(&thread->countLock);*/

	// In the highly unlikely case this thread finishes sorting before the other threads are even
	// done being created, just spin wait for a few cycles.
	while(!World.createdThreads){}

	barrierWait(&World.radixCountedBarrier);

	if (thread->index != 0) {
		// If i'm not the first thread, I will wait for the completion of the count tallying and the
		// generation of the global prefix array.
		cl_lock(&World.radixTallyCompletedLock);
		while (World.radixTallyCompleted != pass) {
			cl_wait(&World.radixTallyCompletedLock);
		}
		cl_unlock(&World.radixTallyCompletedLock);
	} else {
		// I'm the first thread. I am responsible for doing the tally of each count array.
		int* counts = World.radixTally;
		memset(counts, 0, RADIX_16 * sizeof(int));

		// Tally up the counts of all threads
		for (int t = World.numThreads - 1; t >= 0; t--) {
			Thread* th = &World.threads[t];
			memcpy(th->acculCount, counts, sizeof(int) * RADIX_16);	

			for (int i = 0; i < RADIX_16; i++) {
				counts[i] += th->radixCount[i];
			}
		}

		// Convert the tally arrays to prefixes
		radixCountToPrefix(World.radixTally);

		// Alert everyone that the tally process is finished
		cl_lock(&World.radixTallyCompletedLock);
		World.radixTallyCompleted++;
		cl_notifyAll(&World.radixTallyCompletedLock);
		cl_unlock(&World.radixTallyCompletedLock);		
	} 

	for (int i = 0; i < RADIX_16; i++) {
		count[i] = World.radixTally[i] - thread->acculCount[i];
	}
	
	// The tally has been finished. The global tally information can be used to coalesce the sorted portions now.
	radixCoalesce(source, entryCount, pass * 16, count, dest);
	//radixCoalesceExt(source, entryCount, pass * 16, World.radixTally, thread->acculCount, dest);
	barrierWait(&World.radixPass2Barrier);
}

void* threadMain(void* threadInputArg) {
	Thread* thread = (Thread*) threadInputArg;

	const size_t entryCount = thread->entryCount;
	Record* const input = &World.input.data[thread->offset];
	Record* const output = &World.output.data[thread->offset];
	
	Key* bufferA = &World.bufferA[thread->offset];
	Key* bufferB = &World.bufferB[thread->offset];

	// Convert full records to sorting entries in buffer A
	entriesToKeys(input, bufferA, entryCount);

	// Generate count arrays
	//int count[RADIX_16];
	//thread->radixCount = count;

	radixPass(thread, bufferA, World.bufferB, 0, entryCount);
	radixPass(thread, bufferB, World.bufferA, 1, entryCount);

	// I am done with the coalescing phase of the radix sort. I will now wait for the final record coalescing.
	/*cl_lock(&World.radixThreadsFinishedCoalescingLock);
	World.radixThreadsFinishedCoalescing++;

	if (World.radixThreadsFinishedCoalescing == World.numThreads) {
		cl_notifyAll(&World.radixThreadsFinishedCoalescingLock);
	} else {
		cl_wait(&World.radixThreadsFinishedCoalescingLock);
	}
	cl_unlock(&World.radixThreadsFinishedCoalescingLock);*/

	// Perform radix sort on buffer A. The results remain in buffer A
	//radixSort(bufferA, entryCount, bufferB);

	//parallelMerge(params, bufferA, bufferB, entryCount);

	// Wait for the key coalescing signal. When this signal arrives, it means the final merge step
	// has been finished, and the keys just need to be transformed back to records in the output
	// memory mapped file.
	/*cl_lock(&World.keyCoalesceLock);
	while (!World.keyColaescingStarted) {
		cl_wait(&World.keyCoalesceLock);
	}
	cl_unlock(&World.keyCoalesceLock);*/

	// Coalesce only my own original key range. The other threads will coalesce theirs as well
	// in parallel.
	keysToEntries(bufferA, output, entryCount);

	return NULL;
}

void parallelMerge(Thread* thread, Key* bufferA, Key* bufferB, size_t entryCount) {
	// Now, we will perform a parallel merge. This thread will wait for its brothers to finish
	// their sorting portion, then will join their work with ours.
	// For example, if we have 4 threads, here's what each thread will do:
	// 0: Will join the work by 1, then all the work of 2
	// 1: Will just quit.
	// 2: Will join the work by 3
	// 3: Will just quit.
	int thIndex = thread->index;
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
	thread->accumulatedSize = currentSize;

	// Notify other threads that this one has finished its sorting.
	cl_lock(&thread->sortLock);
	thread->sorted = true;
	cl_notify(&thread->sortLock);
	cl_unlock(&thread->sortLock);

	// If I am the first thread, no thread will wait for me. Also, I will always accumulate the
	// sorted arrays from everyone.
	// I am responsible for starting the final key coalescing phase.
	if (thIndex == 0) {
		cl_lock(&World.keyCoalesceLock);
		World.keyColaescingStarted = true;
		cl_notifyAll(&World.keyCoalesceLock);
		cl_unlock(&World.keyCoalesceLock);
	}
}

void spawnThreads(int threadCount) {
	cl_init(&World.keyCoalesceLock);
	cl_init(&World.radixTallyCompletedLock);
	cl_init(&World.radixThreadsFinishedCoalescingLock);
	barrierInit(&World.radixCountedBarrier, threadCount);
	barrierInit(&World.radixPass2Barrier, threadCount);
	World.keyColaescingStarted = false;
	World.radixTallyCompleted = -1;
	World.radixThreadsFinishedCoalescing = 0;

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
}

void preFault(void* buffer, int size) {
	char* buff = buffer;

	for (int i = 0; i < size; i += 4096) {
		(buff)[i] = 0;
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
		//memcpy(entries[i].data.data, keys[i].data, sizeof(RecordData));
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

void printKeys(const Key* arr, int n) {
	printf("[");
	for (int i = 0; i < n - 1; i++) {
		printf("%i, ", arr[i].key);
	}
	printf("%i]", arr[n - 1].key);
}
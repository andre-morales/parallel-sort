#define ENABLE_BIMERGE 0
#include "psort.h"
#include "merge_sort.h"
#include "radix_sort.h"
#include "cond_lock.h"
#include "barrier.h"
#include "slow_barrier.h"
#include "count_barrier.h"
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

#define ENABLEDX

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

	bool keyColaescingStarted;
	ConditionLock keyCoalesceLock;

	// Radix sort state
	int rxTally[RADIX_COUNT];

	int rxTallyCompleted;
	ConditionLock rxTallyCompletedLock;

	Barrier rxPassBarrier;
	Barrier rxDistCountedBarrier;
	SlowBarrier rxCountedBarrier;
} World_t;

typedef struct Thread_t {
	int index;
	pthread_t thread;
	size_t entryCount;
	size_t offset;

	#if ENABLE_BIMERGE
		// Used in the merging step of the algorithm. Represents how many records this thread
		// has accumulated with others.
		size_t accumulatedSize;
	
		// If this thread has done it's individual sorting portion already. Only access this flag
		// with sortLock.
		bool sorted;
		ConditionLock sortLock;
	#endif

	// In the distributed tally phase, how many digits will this thread handle
	int rxCountSize;

	// In the distributed tally phase, what index should this thread start counting from
	int rxCountOffset;

	#ifdef ENABLEDX
	int* radixCount;
	int* acculCount;
	#else
	int radixCount[RADIX_COUNT];
	int acculCount[RADIX_COUNT];
	#endif
} Thread;

World_t World;

void openInput(const char*);
void openOutput(const char*);
void closeOutput();
void* threadMain(void*);
void threadSortRadixAndMerge(Thread*, Key*, Key*, size_t);
void threadFullRadix(Thread*, Key*, Key*, size_t);

void selectionSort(Key* data, size_t entryCount);
void spawnThreads(int);
void entriesToKeys(Record *entries, SortKey *keys, size_t entryCount);
void keysToEntries(SortKey *keys, Record *entries, size_t entryCount);
void preFault(void* buffer, int size);
void printKeys(const Key* arr, int n);
void parallelMerge(Thread* thread, Key* bufferA, Key* bufferB, size_t entryCount);
static void radixPass(Thread* thread, Key* source, Key* dest, int pass, int entryCount);

int main(int argc, char* argv[]) {	
	/*Record a = {0x00000001, {{'b'}}};
	Record b = {0x00000001, {{'a'}}};
	Record c = {0x00000002, {{'c'}}};
	Record d = {0x00000001, {{'d'}}};

	openInput("400b.dat");
	openOutput("test/trouble.dat");
	World.output.data[0] = a;
	World.output.data[1] = b;
	World.output.data[2] = c;
	World.output.data[3] = d;
	closeOutput();
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

	// Spawn N threads
	spawnThreads(threadCount);

	closeOutput();
	return 0; 
}

void spawnThreads(int threadCount) {
	// Allocate Key[] buffers
	mm_alloc(&World.bufferA, sizeof(Key[World.input.entryCount]));
	mm_alloc(&World.bufferB, sizeof(Key[World.input.entryCount]));

	#ifdef ENABLEDX
	int* countArray;
	mm_alloc(&countArray, sizeof(int[RADIX_COUNT]) * threadCount * 2);
	#endif

	cl_init(&World.keyCoalesceLock);
	cl_init(&World.rxTallyCompletedLock);
	barr_init(&World.rxPassBarrier, threadCount);
	barr_init(&World.rxDistCountedBarrier, threadCount);
	slowbarr_init(&World.rxCountedBarrier, threadCount);
	World.keyColaescingStarted = false;
	World.rxTallyCompleted = -1;

	size_t division = World.input.entryCount / threadCount;
	size_t remainder = World.input.entryCount % threadCount;

	size_t offset = 0;
	
	World.createdThreads = false;
	mm_alloc(&World.threads, sizeof(Thread[threadCount]));

	mm_finish();
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
		#ifdef ENABLEDX
		t->radixCount = &countArray[(i * 2 + 0) * RADIX_COUNT];
		t->acculCount = &countArray[(i * 2 + 1) * RADIX_COUNT];
		#endif

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

		offset += count;
	}

	World.createdThreads = true;
	
	// Wait for all N threads to finish execution.
	for (int i = 0; i < threadCount; i++) {
		pthread_join(World.threads[i].thread, NULL);
	}
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

	//threadSortRadixAndMerge(thread, bufferA, bufferB, entryCount);
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
	//radixPass(thread, bufferA, World.bufferB, 2, entryCount);
	//radixPass(thread, bufferB, World.bufferA, 3, entryCount);
}

#if ENABLE_BIMERGE
void threadSortRadixAndMerge(Thread* thread, Key* bufferA, Key* bufferB, size_t entryCount) {
	// Perform radix sort on buffer A. The results remain in buffer A
	radixSort(bufferA, entryCount, bufferB);

	parallelMerge(thread, bufferA, bufferB, entryCount);

	// Wait for the key coalescing signal. When this signal arrives, it means the final merge step
	// has been finished, and the keys just need to be transformed back to records in the output
	// memory mapped file.
	cl_lock(&World.keyCoalesceLock);
	while (!World.keyColaescingStarted) {
		cl_wait(&World.keyCoalesceLock);
	}
	cl_unlock(&World.keyCoalesceLock);
}
#endif

void radixParallelTally(Thread* thread) {
	const int thIndex = thread->index;
	const int countSize = thread->rxCountSize;
	const int countOffset = thread->rxCountOffset;

	// Wait for all threads to finish counting in the earlier phase.
	barr_wait(&World.rxDistCountedBarrier);

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
	if(slowbarr_wait(&World.rxCountedBarrier)) {
		radixCountToPrefix(World.rxTally);
		slowbarr_lower(&World.rxCountedBarrier);
	}
}

static void radixSingleTally(Thread* thread) {
	// If I'm the last thread to arrive here, I won't wait, I will perform the tally for everyone.
	// Otherwise, I'm not the last thread to finish counting. I won't be the tally thread,
	// I will wait here until the tally thead notifies me.
	bool isTallyThread = slowbarr_wait(&World.rxCountedBarrier);

	if (isTallyThread) {
		int* counts = World.rxTally;
		memset(counts, 0, RADIX_COUNT * sizeof(int));

		// Tally up the counts of all threads
		for (int t = World.numThreads - 1; t >= 0; t--) {
			Thread* th = &World.threads[t];

			// Every thread gets an accumulated count.
			memcpy(th->acculCount, counts, sizeof(int) * RADIX_COUNT);	

			for (int i = 0; i < RADIX_COUNT; i++) {
				counts[i] += th->radixCount[i];
			}
		}

		// Convert the tally arrays to prefixes
		radixCountToPrefix(World.rxTally);

		// Lower the barrier for the other threads
		slowbarr_lower(&World.rxCountedBarrier);
	}
}

static INLINE void radixPass(Thread* thread, Key* source, Key* dest, int pass, int entryCount) {
	int* count = thread->radixCount;
	memset(count, 0, RADIX_COUNT * sizeof(int));
	
	// Perform the parallel counting portion
	radixCount(source, entryCount, pass * RADIX_BITS, count);

	// :: ---- DISTRIBUTED COUNTING -- SINGLE PREFIX THREAD
	radixParallelTally(thread);

	// :: ---- SINGLE TALLY THREAD
	//radixSingleTally(thread);

	// Now the perform the final coalescing step in parallel
	radixCoalesceExt(source, entryCount, pass * RADIX_BITS, World.rxTally, thread->acculCount, dest);

	// Everyone must finish their coalescing to consider the pass finished.
	barr_wait(&World.rxPassBarrier);
}

#if ENABLE_BIMERGE
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
#endif

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

/*void printKeys(const Key* arr, int n) {
	printf("[");
	for (int i = 0; i < n - 1; i++) {
		printf("%i, ", arr[i].key);
	}
	printf("%i]", arr[n - 1].key);
}*/
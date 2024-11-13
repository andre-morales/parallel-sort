#include "allocator.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    void** dest;
    size_t size;
} Allocation;

static int count = 0;
static size_t totalSize = 0;

static Allocation allocations[128];

void mm_alloc(void* pointer, size_t size) {
    allocations[count].dest = pointer;
    allocations[count].size = size;
    totalSize += size;
    count++;
}

void mm_finish() {
    //void* block = malloc(totalSize);
    void* block = mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED) {
        fprintf(stderr, "Main mmap() failed.\n");
        exit(-1);
    }

    for (int i = 0; i < count; i++) {
        *(allocations[i].dest) = block;
        block += allocations[i].size;
    }
}
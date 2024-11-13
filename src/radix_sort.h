#pragma once
#include "psort.h"
#include <stdint.h>

#pragma GCC optimize("-O2")

#define RADIX_8 256
#define RADIX_16 65536

#define RADIX_BITS 16
#define RADIX_COUNT RADIX_16

static INLINE void radixCount(Key* arr, int n, int shift, int* count) {
	// Count occurrences of each word value in the count array
	for (int i = 0; i < n; ++i) {
		int word = (arr[i].key >> shift) & (RADIX_COUNT - 1);
		count[word]++;
	}
}

static INLINE void radixCountToPrefix(int* count) {
	for (int i = 1; i < RADIX_COUNT; ++i) {
		count[i] += count[i - 1];
	}
}

void radixCoalesce(Key* arr, int n, int shift, int* prefix, Key* output);
void radixCountSort(Key* arr, int n, int shift, Key* output);
void radixSort(Key* arr, int n, Key* output);

static INLINE void radixCoalesceExt(const Key* restrict arr, int n, int shift, const int* prefix, int* restrict count, Key* restrict output) {
	for (int i = n - 1; i >= 0; --i) {
		int word = (arr[i].key >> shift) & (RADIX_COUNT - 1);
		int pos = prefix[word] - (count[word]++) - 1;
		output[pos] = arr[i];
	}
}

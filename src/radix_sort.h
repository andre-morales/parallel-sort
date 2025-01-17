#pragma once
#include "psort.h"
#include <stdint.h>

#define RADIX_BITS 16
#define RADIX_COUNT 65536

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

static INLINE void radixCoalesceExt(const Key* restrict arr, int n, int shift, const int* prefix, int* restrict count, Key* restrict output) {
	for (int i = n - 1; i >= 0; --i) {
		int word = (arr[i].key >> shift) & (RADIX_COUNT - 1);
		int pos = prefix[word] - (count[word]++) - 1;
		output[pos] = arr[i];
	}
}

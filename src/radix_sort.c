#include "radix_sort.h"
#include <string.h>

#pragma GCC push_options
#pragma GCC optimize("-O2")

void radixCount(Key* arr, int n, int shift, int* count) {
	// Count occurrences of each word value in the count array
	for (int i = 0; i < n; ++i) {
		int word = (arr[i].key >> shift) & (RADIX_COUNT - 1);
		count[word]++;
	}
}

void radixCountToPrefix(int* count) {
	for (int i = 1; i < RADIX_COUNT; ++i) {
		count[i] += count[i - 1];
	}
}

void radixCoalesce(Key* arr, int n, int shift, int* prefix, Key* output) {
	for (int i = n - 1; i >= 0; --i) {
		int word = (arr[i].key >> shift) & (RADIX_COUNT - 1);
		int pos = --prefix[word];
		output[pos] = arr[i];
	}
}

INLINE void radixCoalesceExt(const Key* restrict arr, int n, int shift, const int* prefix, int* restrict count, Key* restrict output) {
	for (int i = n - 1; i >= 0; --i) {
		int word = (arr[i].key >> shift) & (RADIX_COUNT - 1);
		int pos = prefix[word] - (count[word]++) - 1;
		output[pos] = arr[i];
	}
}

void radixCountSort(Key* arr, int n, int shift, Key* output) {
	int count[RADIX_COUNT];

	// Clear count array
	memset(count, 0, RADIX_COUNT * sizeof(int));

	// Count occurrences of each word value in the count array
	radixCount(arr, n, shift, count);

	// Compute prefix sums to determine output positions
	radixCountToPrefix(count);

	// Build the output array in stable manner
	radixCoalesce(arr, n, shift, count, output);
}

void radixSort(Key* data, int n, Key* buffer) {
	// We'll perform sorting passed from the LSB to the MSB.
	// The first pass will construct the array in the buffer, from then onwards, we alternate
	// between the buffer and the original array.
	// The final pass will have the sorted keys in the original array.
	
	/*countSort8(data,   n, 0,  buffer);
	countSort8(buffer, n, 8,  data);
	countSort8(data,   n, 16, buffer);
	countSort8(buffer, n, 24, data);*/
	
	radixCountSort(data,   n, 0, buffer);
	radixCountSort(buffer, n, 16, data);
}

#pragma GCC pop_options
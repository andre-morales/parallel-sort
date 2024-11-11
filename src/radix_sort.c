#include "radix_sort.h"
#include <string.h>

#define RADIX8 256
#define RADIX16 65536

static void countSort8(Key* arr, int n, int shift, Key* output) {
	int count[RADIX8];

	// Clear count array
	memset(count, 0, 256 * sizeof(int));

	// Count occurrences of each byte value
	for (int i = 0; i < n; ++i) {
		int byte = (arr[i].key >> shift) & (RADIX8 - 1); // Extract current byte
		count[byte]++;
	}

	// Compute prefix sums to determine output positions
	for (int i = 1; i < RADIX8; ++i) {
		count[i] += count[i - 1];
	}

	// Build the output array in stable manner
	for (int i = n - 1; i >= 0; --i) {
		int byte = (arr[i].key >> shift) & (RADIX8 - 1);
		output[--count[byte]] = arr[i];
	}
}

static void countSort16(Key* arr, int n, int shift, Key* output) {
	int count[RADIX16];

	// Clear count array
	memset(count, 0, RADIX16 * sizeof(int));

	// Count occurrences of each word value in the count array
	for (int i = 0; i < n; ++i) {
		int word = (arr[i].key >> shift) & (RADIX16 - 1);
		count[word]++;
	}

	// Compute prefix sums to determine output positions
	for (int i = 1; i < RADIX16; ++i) {
		count[i] += count[i - 1];
	}

	// Build the output array in stable manner
	for (int i = n - 1; i >= 0; --i) {
		int word = (arr[i].key >> shift) & (RADIX16 - 1);
		output[--count[word]] = arr[i];
	}
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
	
	countSort16(data,   n, 0, buffer);
	countSort16(buffer, n, 16, data);
}
#include "radix_sort.h"
#include <string.h>

#define RADIX8 256
#define RADIX16 65536

//#pragma GCC push_options
//#pragma GCC optimize("-O2")

void radixCount(Key* arr, int n, int shift, int* count) {
	// Count occurrences of each word value in the count array
	for (int i = 0; i < n; ++i) {
		int word = (arr[i].key >> shift) & (RADIX16 - 1);
		count[word]++;
	}
}

void radixTallyCount(int* countA, int* countB) {
	for (int i = 0; i < RADIX16; i++) {
		countA[i] += countB[i];
	}
}

void radixCountToPrefix(int* count) {
	for (int i = 1; i < RADIX16; ++i) {
		count[i] += count[i - 1];
	}
}

void radixPrefix(int* count, int* prefix) {
	prefix[0] = 0;
	for (int i = 1; i < RADIX16; ++i) {
		prefix[i] += prefix[i-1] + count[i - 1];
	}
}

void radixCoalesce(Key* arr, int n, int shift, int* prefix, Key* output) {
	for (int i = n - 1; i >= 0; --i) {
		int word = (arr[i].key >> shift) & (RADIX16 - 1);
		int pos = --prefix[word];
		output[pos] = arr[i];
	}
}

void radixCoalesceX(Key* arr, int n, int shift, int* prefix, Key* output) {
	for (int i = 0; i < n; i++) {
		int word = (arr[i].key >> shift) & (RADIX16 - 1);
		int pos = --prefix[word];
		output[pos] = arr[i];
	}
}


void radixCoalesceExt(const Key* arr, int n, int shift, int* count, const int* prefix, Key* output) {
	for (int i = 0; i < n; i++) {
		int word = (arr[i].key >> shift) & (RADIX16 - 1);
		int pos = prefix[word] - count[word];
		output[pos] = arr[i];
		--count[word];
	}
}

void radixCountSort(Key* arr, int n, int shift, Key* output) {
	int count[RADIX16];

	// Clear count array
	memset(count, 0, RADIX16 * sizeof(int));

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

	/*int count0[RADIX16];
	int count1[RADIX16];
	memset(count0, 0, RADIX16 * sizeof(int));
	memset(count1, 0, RADIX16 * sizeof(int));

	for (int i = 0; i < n; ++i) {
		int word0 = (data[i].key >> 0) & (RADIX16 - 1);
		int word1 = (data[i].key >> 16) & (RADIX16 - 1);
		count0[word0]++;
		count1[word1]++;
	}

	for (int i = 1; i < RADIX16; ++i) {
		count0[i] += count0[i - 1];
		count1[i] += count1[i - 1];
	}

	radixCoalesce(data,   n, 0, count0, buffer);
	radixCoalesce(buffer, n, 16, count1, data);*/

	/*for (int i = n - 1; i >= 0; --i) {
		int word = (arr[i].key >> 0) & (RADIX16 - 1);
		int pos = --count0[word];
		output[pos] = arr[i];
	}

	for (int i = n - 1; i >= 0; --i) {
		int word = (output[i].key >> 16) & (RADIX16 - 1);
		int pos = --count1[word];
		arr[pos] = output[i];
	}*/
}

//#pragma GCC pop_options
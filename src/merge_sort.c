#include "merge_sort.h"
#include <string.h>

void merge(Key* array, int size, Key* dest) {
	int iA = 0;
	int iB = 0;
	int p = size - size / 2;

	Key* A = array;
	Key* B = &array[p];

	for (int i = 0; i < size; i++) {
		int ak = A[iA].key;
		int bk = B[iB].key;
		if (ak < bk) {
			dest[i] = A[iA];
			iA++;

			// If the A array ran out, copy all from B and leave
			if (iA == p) {
				i++;
				while(i < size) {
					dest[i++] = B[iB++];
				}
				return;
			}
		} else {
			dest[i] = B[iB];
			iB++;

			// If the B array ran out, copy all from A and leave
			if (iB == size / 2) {
				i++;
				while(i < size) {
					dest[i++] = A[iA++];
				}
				return;
			}
		}
	}
}

void mergeP(Key* array, int size, int p, Key* dest) {
	int iA = 0;
	int iB = 0;

	Key* A = array;
	Key* B = &array[p];

	for (int i = 0; i < size; i++) {
		int ak = A[iA].key;
		int bk = B[iB].key;
		if (ak < bk) {
			dest[i] = A[iA];
			iA++;

			// If the A array ran out, copy all from B and leave
			if (iA == p) {
				i++;
				while(i < size) {
					dest[i++] = B[iB++];
				}
				return;
			}
		} else {
			dest[i] = B[iB];
			iB++;

			// If the B array ran out, copy all from A and leave
			if (iB == size - p) {
				i++;
				while(i < size) {
					dest[i++] = A[iA++];
				}
				return;
			}
		}
	}
}

void mergeSortAux(Key* array, int size, Key* buffer) {
	if (size == 1) {
		return;
	}

	int half = size / 2;
	int p = size - half;
	mergeSortAux(&array[0], p,    &buffer[0]);
	mergeSortAux(&array[p], half, &buffer[p]);

	merge(array, size, buffer);
	memcpy(array, buffer, size * sizeof(Key));
}

void mergeSort(Key* array, int size, Key* dest) {
	if (size == 1) {
		return;
	}

	int half = size / 2;
	int p = size - half;
	mergeSortAux(&array[0], p,    &dest[0]);
	mergeSortAux(&array[p], half, &dest[p]);

	merge(array, size, dest);
}
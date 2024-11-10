#include "psort.h"

void selectionSort(Key* data, size_t entryCount) {
	for (int i = 0; i < entryCount; i++) {
		Key* smallest = &data[i];
		for (int j = i + 1; j < entryCount; j++) {
			if (data[j].key < smallest->key) {
				smallest = &data[j];
			}
		}

		Key tmp = data[i];
		data[i] = *smallest;
		*smallest = tmp;
	}
}

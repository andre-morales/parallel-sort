#pragma once
#include "psort.h"

void merge(Key* array, int size, Key* dest);
void mergeP(Key* array, int size, int p, Key* dest);
void mergeSort(Key* array, int size, Key* dest);
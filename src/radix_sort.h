#pragma once
#include "psort.h"
#include <stdint.h>

#define RADIX_16 65536

void radixCount(Key* arr, int n, int shift, int* count);
void radixTallyCount(int* countA, int* countB);
void radixCountToPrefix(int* count);
void radixPrefix(int* count, int* prefix);
void radixCoalesce(Key* arr, int n, int shift, int* prefix, Key* output);
void radixCoalesceX(Key* arr, int n, int shift, int* prefix, Key* output);
void radixCoalesceExt(const Key* arr, int n, int shift, const int* prefix, int* count, Key* output);
void radixCountSort(Key* arr, int n, int shift, Key* output);
void radixSort(Key* arr, int n, Key* output);
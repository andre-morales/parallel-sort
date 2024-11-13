#pragma once
#include "psort.h"
#include <stdint.h>

#define RADIX_8 256
#define RADIX_16 65536

#define RADIX_BITS 16
#define RADIX_COUNT RADIX_16

void radixCount(Key* arr, int n, int shift, int* count);
void radixCountToPrefix(int* count);

void radixCoalesce(Key* arr, int n, int shift, int* prefix, Key* output);
void radixCoalesceExt(const Key* restrict arr, int n, int shift, const int* restrict prefix, int* count, Key* restrict output);

void radixCountSort(Key* arr, int n, int shift, Key* output);
void radixSort(Key* arr, int n, Key* output);
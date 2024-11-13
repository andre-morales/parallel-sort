#pragma once
#include <stddef.h>
#include <stdint.h>

#define INLINE inline __attribute__((always_inline)) 

typedef struct {
	char data[96];
} RecordData;

typedef struct {
	uint32_t key;
	RecordData data;
} Record;

typedef struct {
	uint32_t key;
	RecordData* data;
} SortKey;

typedef SortKey Key;
#pragma once
#include <stddef.h>
#include <stdint.h>

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
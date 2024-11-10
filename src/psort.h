#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
	char data[96];
} RecordData;

typedef struct {
	int64_t key;
	RecordData* data;
} SortKey;

typedef SortKey Key;
#pragma once
#include <stdlib.h>

void *tlsuv_malloc(size_t size);
void *tlsuv_realloc(void* ptr, size_t size);
void *tlsuv_calloc(size_t count, size_t size);
void tlsuv_free(void* ptr);

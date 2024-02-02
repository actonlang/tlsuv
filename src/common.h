#pragma once
#include <stdlib.h>

typedef void* (*tlsuv_malloc_func)(size_t size);
typedef void* (*tlsuv_malloc_func)(size_t size);
typedef void* (*tlsuv_realloc_func)(void* ptr, size_t size);
typedef void* (*tlsuv_calloc_func)(size_t count, size_t size);
typedef void (*tlsuv_free_func)(void* ptr);

int tlsuv_replace_allocator(tlsuv_malloc_func malloc_func,
                            tlsuv_realloc_func realloc_func,
                            tlsuv_calloc_func calloc_func,
                            tlsuv_free_func free_func);

void *tlsuv_malloc(size_t size);
void *tlsuv_realloc(void* ptr, size_t size);
void *tlsuv_calloc(size_t count, size_t size);
void tlsuv_free(void* ptr);

#include <uv.h>
#include "tlsuv/tlsuv.h"
#include "common.h"

typedef struct {
  tlsuv_malloc_func malloc;
  tlsuv_realloc_func realloc;
  tlsuv_calloc_func calloc;
  tlsuv_free_func free;
} tlsuv__allocator_t;

static tlsuv__allocator_t tlsuv__allocator = {
  malloc,
  realloc,
  calloc,
  free,
};

int tlsuv_replace_allocator(tlsuv_malloc_func malloc_func,
                            tlsuv_realloc_func realloc_func,
                            tlsuv_calloc_func calloc_func,
                            tlsuv_free_func free_func) {
  if (malloc_func == NULL || realloc_func == NULL ||
      calloc_func == NULL || free_func == NULL) {
    return UV_EINVAL;
  }

  tlsuv__allocator.malloc = malloc_func;
  tlsuv__allocator.realloc = realloc_func;
  tlsuv__allocator.calloc = calloc_func;
  tlsuv__allocator.free = free_func;

  return 0;
}

void* tlsuv_malloc(size_t size) {
    if (size > 0)
        return tlsuv__allocator.malloc(size);
    return NULL;
}

void* tlsuv_realloc(void* ptr, size_t size) {
  if (size > 0)
    return tlsuv__allocator.realloc(ptr, size);
  tlsuv_free(ptr);
  return NULL;
}


void* tlsuv_calloc(size_t count, size_t size) {
  return tlsuv__allocator.calloc(count, size);
}

void tlsuv_free(void* ptr) {
  int saved_errno;

  /* The system allocator the assumption that errno is not modified but custom
   * allocators may not be so careful.
   */
  saved_errno = errno;
  tlsuv__allocator.free(ptr);
  errno = saved_errno;
}

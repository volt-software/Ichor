#pragma once

#if defined(ICHOR_USE_SYSTEM_MIMALLOC) || defined(ICHOR_USE_MIMALLOC)
#include <mimalloc-new-delete.h>

#if (!defined(WIN32) && !defined(_WIN32) && !defined(__WIN32)) || defined(__CYGWIN__)
extern "C" {
    void *malloc(size_t size) {
        return mi_malloc(size);
    }

    void free(void *ptr) {
        mi_free(ptr);
    }

    void *realloc(void *ptr, size_t new_size) {
        return mi_realloc(ptr, new_size);
    }

    void *calloc(size_t num, size_t size) {
        return mi_calloc(num, size);
    }

    void *aligned_alloc(size_t alignment, size_t size) {
        return mi_aligned_alloc(alignment, size);
    }

    size_t malloc_usable_size(void *ptr) {
        return mi_malloc_usable_size(ptr);
    }

    void *memalign(size_t alignment, size_t size) {
        return mi_memalign(alignment, size);
    }

    int posix_memalign(void **memptr, size_t alignment, size_t size) {
        return mi_posix_memalign(memptr, alignment, size);
    }

    void *valloc(size_t size) {
        return mi_valloc(size);
    }

    void *pvalloc(size_t size) {
        return mi_pvalloc(size);
    }
}
#endif
#endif
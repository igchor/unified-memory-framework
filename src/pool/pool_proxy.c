/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <umf/memory_pool_ops.h>
#include <umf/pools/pool_proxy.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils_common.h"

static __TLS umf_result_t TLS_last_allocation_error;

struct proxy_memory_pool {
    umf_memory_provider_handle_t hProvider;
};

static umf_result_t
proxy_pool_initialize(umf_memory_provider_handle_t hProvider, void *params,
                      void **ppPool) {
    (void)params; // unused

    // TODO: use ba_alloc here
    struct proxy_memory_pool *pool = malloc(sizeof(struct proxy_memory_pool));
    if (!pool) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    pool->hProvider = hProvider;
    *ppPool = (void *)pool;

    return UMF_RESULT_SUCCESS;
}

static void proxy_pool_finalize(void *pool) {
    free(pool);
    fprintf(stderr, "proxy pool finalize\n");
}

static void *proxy_aligned_malloc_internal(void *pool, size_t size,
                                           size_t alignment) {
    assert(pool);

    void *ptr;
    struct proxy_memory_pool *hPool = (struct proxy_memory_pool *)pool;

    umf_result_t ret =
        umfMemoryProviderAlloc(hPool->hProvider, size + 8, alignment, &ptr);
    if (ret != UMF_RESULT_SUCCESS) {
        TLS_last_allocation_error = ret;
        return NULL;
    }

    if (ptr) {
        TLS_last_allocation_error = UMF_RESULT_SUCCESS;
        *((size_t *)ptr) = size;
        return (void *)((uintptr_t)ptr + 8);
    } else {
        return NULL;
    }
}

static void *proxy_aligned_malloc(void *pool, size_t size, size_t alignment) {
    return proxy_aligned_malloc_internal(pool, size, alignment);
}

static void *proxy_malloc(void *pool, size_t size) {
    assert(pool);

    return proxy_aligned_malloc(pool, size, 0);
}

static void *proxy_calloc(void *pool, size_t num, size_t size) {
    assert(pool);

    void *ptr = proxy_malloc(pool, num * size);
    if (ptr) {
        memset(ptr, 0, num * size);
    }
    return ptr;
}

static umf_result_t proxy_free(void *pool, void *ptr) {
    assert(pool);

    if (!ptr)
        return UMF_RESULT_SUCCESS;

    struct proxy_memory_pool *hPool = (struct proxy_memory_pool *)pool;
    return umfMemoryProviderFree(hPool->hProvider, ((char *)ptr - 8), 0);
}

static void *proxy_realloc(void *pool, void *ptr, size_t size) {
    assert(pool);

    if (size == 0) {
       proxy_free(pool, ptr);
    }

    if (ptr == NULL) {
        return proxy_malloc(pool, size);
    }

    fprintf(stderr, "proxy_realloc\n");

    size_t old_size = *((size_t *)(((uintptr_t)ptr) - 8));
    if (old_size >= size) {
        return ptr;
    }

    void *new_ptr = proxy_malloc(pool, size);
    memcpy(new_ptr, ptr, old_size);
    proxy_free(pool, ptr);
    return new_ptr;
}

static size_t proxy_malloc_usable_size(void *pool, void *ptr) {
    assert(pool);

    (void)pool;
    (void)ptr;

    TLS_last_allocation_error = UMF_RESULT_ERROR_NOT_SUPPORTED;
    return 0;
}

static umf_result_t proxy_get_last_allocation_error(void *pool) {
    (void)pool; // not used
    return TLS_last_allocation_error;
}

static umf_memory_pool_ops_t UMF_PROXY_POOL_OPS = {
    .version = UMF_VERSION_CURRENT,
    .initialize = proxy_pool_initialize,
    .finalize = proxy_pool_finalize,
    .malloc = proxy_malloc,
    .calloc = proxy_calloc,
    .realloc = proxy_realloc,
    .aligned_malloc = proxy_aligned_malloc,
    .malloc_usable_size = proxy_malloc_usable_size,
    .free = proxy_free,
    .get_last_allocation_error = proxy_get_last_allocation_error};

umf_memory_pool_ops_t *umfProxyPoolOps(void) { return &UMF_PROXY_POOL_OPS; }

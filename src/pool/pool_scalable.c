/*
 *
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "umf/pools/pool_scalable.h"
#include <umf/memory_pool.h>
#include <umf/memory_pool_ops.h>
#include <umf/memory_provider.h>

#include <tbb/scalable_allocator.h>

#include "base_alloc_global.h"
#include "utils_common.h"
#include "utils_concurrency.h"
#include "utils_sanitizers.h"

typedef void *(*raw_alloc_tbb_type)(intptr_t, size_t *);
typedef void (*raw_free_tbb_type)(intptr_t, void *, size_t);

static __TLS umf_result_t TLS_last_allocation_error;
static __TLS umf_result_t TLS_last_free_error;

typedef struct tbb_mem_pool_policy_t {
    raw_alloc_tbb_type pAlloc;
    raw_free_tbb_type pFree;
    size_t granularity;
    int version;
    unsigned fixed_pool : 1, keep_all_memory : 1, reserved : 30;
} tbb_mem_pool_policy_t;

typedef struct tbb_callbacks_t {
    void *(*pool_malloc)(void *, size_t);
    void *(*pool_realloc)(void *, void *, size_t);
    void *(*pool_aligned_malloc)(void *, size_t, size_t);
    bool (*pool_free)(void *, void *);
    int (*pool_create_v1)(intptr_t, const struct tbb_mem_pool_policy_t *,
                          void **);
    bool (*pool_destroy)(void *);
    void *(*pool_identify)(void *object);
    size_t (*pool_msize)(void *, void *);
    void *lib_handle;
} tbb_callbacks_t;

typedef struct tbb_memory_pool_t {
    umf_memory_provider_handle_t mem_provider;
    void *tbb_pool;
    tbb_callbacks_t tbb_callbacks;
} tbb_memory_pool_t;

static int init_tbb_callbacks(tbb_callbacks_t *tbb_callbacks) {
    assert(tbb_callbacks);

    const char so_name[] = "libtbbmalloc.so.2";
    tbb_callbacks->lib_handle = dlopen(so_name, RTLD_LAZY);
    if (!tbb_callbacks->lib_handle) {
        fprintf(stderr, "%s not found.\n", so_name);
        return -1;
    }

    *(void **)&tbb_callbacks->pool_malloc = dlsym(
        tbb_callbacks->lib_handle, "_ZN3rml11pool_mallocEPNS_10MemoryPoolEm");
    *(void **)&tbb_callbacks->pool_realloc =
        dlsym(tbb_callbacks->lib_handle,
              "_ZN3rml12pool_reallocEPNS_10MemoryPoolEPvm");
    *(void **)&tbb_callbacks->pool_aligned_malloc =
        dlsym(tbb_callbacks->lib_handle,
              "_ZN3rml19pool_aligned_mallocEPNS_10MemoryPoolEmm");
    *(void **)&tbb_callbacks->pool_free = dlsym(
        tbb_callbacks->lib_handle, "_ZN3rml9pool_freeEPNS_10MemoryPoolEPv");
    *(void **)&tbb_callbacks->pool_create_v1 = dlsym(
        tbb_callbacks->lib_handle,
        "_ZN3rml14pool_create_v1ElPKNS_13MemPoolPolicyEPPNS_10MemoryPoolE");
    *(void **)&tbb_callbacks->pool_destroy = dlsym(
        tbb_callbacks->lib_handle, "_ZN3rml12pool_destroyEPNS_10MemoryPoolE");
    *(void **)&tbb_callbacks->pool_identify =
        dlsym(tbb_callbacks->lib_handle, "_ZN3rml13pool_identifyEPv");
    *(void **)&tbb_callbacks->pool_msize = dlsym(
        tbb_callbacks->lib_handle, "_ZN3rml10pool_msizeEPNS_10MemoryPoolEPv");

    if (!tbb_callbacks->pool_malloc || !tbb_callbacks->pool_realloc ||
        !tbb_callbacks->pool_aligned_malloc || !tbb_callbacks->pool_free ||
        !tbb_callbacks->pool_create_v1 || !tbb_callbacks->pool_destroy ||
        !tbb_callbacks->pool_identify) {
        fprintf(stderr, "Could not find symbols in %s.\n", so_name);
        dlclose(tbb_callbacks->lib_handle);
        return -1;
    }

    return 0;
}

static void *tbb_raw_alloc_wrapper(intptr_t pool_id, size_t *raw_bytes) {
    void *resPtr;
    tbb_memory_pool_t *pool = (tbb_memory_pool_t *)pool_id;
    umf_result_t ret =
        umfMemoryProviderAlloc(pool->mem_provider, *raw_bytes, 0, &resPtr);
    if (ret != UMF_RESULT_SUCCESS) {
        TLS_last_allocation_error = UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        return NULL;
    }

    return resPtr;
}

static void tbb_raw_free_wrapper(intptr_t pool_id, void *ptr, size_t bytes) {
    tbb_memory_pool_t *pool = (tbb_memory_pool_t *)pool_id;
    umf_result_t ret = umfMemoryProviderFree(pool->mem_provider, ptr, bytes);
    if (ret != UMF_RESULT_SUCCESS) {
        TLS_last_free_error = ret;
        fprintf(
            stderr,
            "Memory provider failed to free memory, addr = %p, size = %zu\n",
            ptr, bytes);
    }
}

static umf_result_t tbb_pool_initialize(umf_memory_provider_handle_t provider,
                                        void *params, void **pool) {
    (void)params; // unused

    const size_t GRANULARITY = 2 * 1024 * 1024;
    tbb_mem_pool_policy_t policy = {.pAlloc = tbb_raw_alloc_wrapper,
                                    .pFree = tbb_raw_free_wrapper,
                                    .granularity = GRANULARITY,
                                    .version = 1,
                                    .fixed_pool = false,
                                    .keep_all_memory = false,
                                    .reserved = 0};

    tbb_memory_pool_t *pool_data =
        umf_ba_global_alloc(sizeof(tbb_memory_pool_t));
    if (!pool_data) {
        fprintf(stderr, "cannot allocate memory for metadata\n");
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    int ret = init_tbb_callbacks(&pool_data->tbb_callbacks);
    if (ret != 0) {
        fprintf(stderr, "loading TBB symbols failed\n");
        return UMF_RESULT_ERROR_UNKNOWN;
    }

    pool_data->mem_provider = provider;
    ret = pool_data->tbb_callbacks.pool_create_v1((intptr_t)pool_data, &policy,
                                                  &(pool_data->tbb_pool));
    if (ret != TBBMALLOC_OK) {
        return UMF_RESULT_ERROR_MEMORY_PROVIDER_SPECIFIC;
    }

    *pool = (void *)pool_data;

    return UMF_RESULT_SUCCESS;
}

static void tbb_pool_finalize(void *pool) {
    tbb_memory_pool_t *pool_data = (tbb_memory_pool_t *)pool;
    pool_data->tbb_callbacks.pool_destroy(pool_data->tbb_pool);
    dlclose(pool_data->tbb_callbacks.lib_handle);
    umf_ba_global_free(pool_data);
}

static void *tbb_malloc(void *pool, size_t size) {
    tbb_memory_pool_t *pool_data = (tbb_memory_pool_t *)pool;
    TLS_last_allocation_error = UMF_RESULT_SUCCESS;
    void *ptr = pool_data->tbb_callbacks.pool_malloc(pool_data->tbb_pool, size);
    if (ptr == NULL) {
        if (TLS_last_allocation_error == UMF_RESULT_SUCCESS) {
            TLS_last_allocation_error = UMF_RESULT_ERROR_UNKNOWN;
        }
        return NULL;
    }
    utils_annotate_acquire(pool);
    return ptr;
}

static void *tbb_calloc(void *pool, size_t num, size_t size) {
    assert(pool);
    size_t csize = num * size;
    void *ptr = tbb_malloc(pool, csize);
    if (ptr == NULL) {
        // TLS_last_allocation_error is set by tbb_malloc()
        return NULL;
    }

    memset(ptr, 0, csize);
    return ptr;
}

static void *tbb_realloc(void *pool, void *ptr, size_t size) {
    tbb_memory_pool_t *pool_data = (tbb_memory_pool_t *)pool;
    TLS_last_allocation_error = UMF_RESULT_SUCCESS;
    void *new_ptr =
        pool_data->tbb_callbacks.pool_realloc(pool_data->tbb_pool, ptr, size);
    if (new_ptr == NULL) {
        if (TLS_last_allocation_error == UMF_RESULT_SUCCESS) {
            TLS_last_allocation_error = UMF_RESULT_ERROR_UNKNOWN;
        }
        return NULL;
    }
    utils_annotate_acquire(pool);
    return new_ptr;
}

static void *tbb_aligned_malloc(void *pool, size_t size, size_t alignment) {
    tbb_memory_pool_t *pool_data = (tbb_memory_pool_t *)pool;
    TLS_last_allocation_error = UMF_RESULT_SUCCESS;
    void *ptr = pool_data->tbb_callbacks.pool_aligned_malloc(
        pool_data->tbb_pool, size, alignment);
    if (ptr == NULL) {
        if (TLS_last_allocation_error == UMF_RESULT_SUCCESS) {
            TLS_last_allocation_error = UMF_RESULT_ERROR_UNKNOWN;
        }
        return NULL;
    }
    utils_annotate_acquire(pool);
    return ptr;
}

static umf_result_t tbb_free(void *pool, void *ptr) {
    if (ptr == NULL) {
        return UMF_RESULT_SUCCESS;
    }

    TLS_last_free_error = UMF_RESULT_SUCCESS;

    // Establishes happens-before order with tbb_*alloc functions.
    // Makes sure that writes to the memory pointed to by 'ptr'
    // are not reported as data races whenever 'ptr' reused by
    // other threads.
    utils_annotate_release(pool);

    tbb_memory_pool_t *pool_data = (tbb_memory_pool_t *)pool;
    if (pool_data->tbb_callbacks.pool_free(pool_data->tbb_pool, ptr)) {
        return UMF_RESULT_SUCCESS;
    }

    if (TLS_last_free_error != UMF_RESULT_SUCCESS) {
        return TLS_last_free_error;
    }

    return UMF_RESULT_ERROR_UNKNOWN;
}

static size_t tbb_malloc_usable_size(void *pool, void *ptr) {
    tbb_memory_pool_t *pool_data = (tbb_memory_pool_t *)pool;
    return pool_data->tbb_callbacks.pool_msize(pool_data->tbb_pool, ptr);
}

static umf_result_t tbb_get_last_allocation_error(void *pool) {
    (void)pool; // not used
    return TLS_last_allocation_error;
}

static umf_memory_pool_ops_t UMF_SCALABLE_POOL_OPS = {
    .version = UMF_VERSION_CURRENT,
    .initialize = tbb_pool_initialize,
    .finalize = tbb_pool_finalize,
    .malloc = tbb_malloc,
    .calloc = tbb_calloc,
    .realloc = tbb_realloc,
    .aligned_malloc = tbb_aligned_malloc,
    .malloc_usable_size = tbb_malloc_usable_size,
    .free = tbb_free,
    .get_last_allocation_error = tbb_get_last_allocation_error};

umf_memory_pool_ops_t *umfScalablePoolOps(void) {
    return &UMF_SCALABLE_POOL_OPS;
}

/*
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/* A MT-safe base allocator */

#include "base_alloc_global.h"
#include "base_alloc_internal.h"
#include "utils_common.h"
#include "utils_math.h"

#include <umf/memory_pool_ops.h>
#include <umf/pools/pool_base.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: we can make this a param to the pool
// allocation classes need to be powers of 2
#define ALLOCATION_CLASSES                                                     \
    { 16, 32, 64, 128 }
#define NUM_ALLOCATION_CLASSES 4

struct base_memory_pool {
    umf_memory_provider_handle_t hProvider;
    size_t ac_sizes[NUM_ALLOCATION_CLASSES];
    umf_ba_pool_t *ac[NUM_ALLOCATION_CLASSES];
    size_t smallest_ac_size_log2;
};

// returns index of the allocation class for a give size
static int size_to_idx(struct base_memory_pool *pool, size_t size) {
    assert(size <= pool->ac_sizes[NUM_ALLOCATION_CLASSES - 1]);

    if (size <= pool->ac_sizes[0]) {
        return 0;
    }

    int isPowerOf2 = (0 == (size & (size - 1)));
    int index =
        (int)(log2Utils(size) + !isPowerOf2 - pool->smallest_ac_size_log2);

    assert(index >= 0);
    assert(index < NUM_ALLOCATION_CLASSES);

    return index;
}

static umf_result_t base_pool_initialize(umf_memory_provider_handle_t hProvider,
                                         void *params, void **ppPool) {
    (void)params; // unused

    struct base_memory_pool *pool =
        umf_ba_global_alloc(sizeof(struct base_memory_pool));
    if (!pool) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    *pool = (struct base_memory_pool){.ac_sizes = ALLOCATION_CLASSES};

    for (int i = 0; i < NUM_ALLOCATION_CLASSES; i++) {
        // allocation classes need to be powers of 2
        assert(0 == (pool->ac_sizes[i] & (pool->ac_sizes[i] - 1)));
        pool->ac[i] = umf_ba_create(pool->ac_sizes[i]);
        if (!pool->ac[i]) {
            fprintf(stderr,
                    "Cannot create base alloc allocation class for size: %zu\n",
                    pool->ac_sizes[i]);
        }
    }

    size_t smallestSize = pool->ac_sizes[0];
    pool->smallest_ac_size_log2 = log2Utils(smallestSize);

    pool->hProvider = hProvider;
    *ppPool = (void *)pool;

    return UMF_RESULT_SUCCESS;
}

static void base_pool_finalize(void *p) {
    struct base_memory_pool *pool = (struct base_memory_pool *)p;
    for (int i = 0; i < NUM_ALLOCATION_CLASSES; i++) {
        if (pool->ac[i]) {
            umf_ba_destroy(pool->ac[i]);
        }
    }
    umf_ba_global_free(pool, sizeof(struct base_memory_pool));
}

static void *base_aligned_malloc(void *p, size_t size, size_t alignment) {
    struct base_memory_pool *pool = (struct base_memory_pool *)p;
    assert(pool);

    if (alignment > 8) {
        return NULL;
    }

    if (size > pool->ac_sizes[NUM_ALLOCATION_CLASSES - 1] ||
        !pool->ac[size_to_idx(pool, size)]) {
        return ba_os_alloc(size);
    }

    return umf_ba_alloc(pool->ac[size_to_idx(pool, size)]);
}

static void *base_malloc(void *pool, size_t size) {
    assert(pool);
    return base_aligned_malloc(pool, size, 0);
}

static void *base_calloc(void *pool, size_t num, size_t size) {
    assert(pool);
    void *ptr = base_malloc(pool, num * size);

    if (ptr) {
        // TODO: memory might not be accessible on the host
        memset(ptr, 0, num * size);
    }

    return ptr;
}

static void *base_realloc(void *pool, void *ptr, size_t size) {
    assert(pool);

    (void)pool;
    (void)ptr;
    (void)size;

    // TODO: implement
    return NULL;
}

static umf_result_t base_free(void *p, void *ptr, size_t size) {
    if (!ptr) {
        return UMF_RESULT_SUCCESS;
    }

    struct base_memory_pool *pool = (struct base_memory_pool *)p;
    assert(pool);

    if (size == 0) {
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (size > pool->ac_sizes[NUM_ALLOCATION_CLASSES - 1] ||
        !pool->ac[size_to_idx(pool, size)]) {
        ba_os_free(ptr, size);
        return UMF_RESULT_SUCCESS;
    }

    umf_ba_free(pool->ac[size_to_idx(pool, size)], ptr);

    return UMF_RESULT_SUCCESS;
}

static size_t base_malloc_usable_size(void *pool, void *ptr) {
    (void)pool;
    (void)ptr;
    return 0;
}

static umf_result_t base_get_last_allocation_error(void *pool) {
    (void)pool;
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
}

static umf_memory_pool_ops_t UMF_BASE_POOL_OPS = {
    .version = UMF_VERSION_CURRENT,
    .initialize = base_pool_initialize,
    .finalize = base_pool_finalize,
    .malloc = base_malloc,
    .calloc = base_calloc,
    .realloc = base_realloc,
    .aligned_malloc = base_aligned_malloc,
    .malloc_usable_size = base_malloc_usable_size,
    .free = base_free,
    .get_last_allocation_error = base_get_last_allocation_error};

umf_memory_pool_ops_t *umfBasePoolOps(void) { return &UMF_BASE_POOL_OPS; }

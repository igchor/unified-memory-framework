/*
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/* A MT-safe base allocator */

#include <umf/pools/pool_base.h>

#include <assert.h>

// this is where base pool will allocate it's own metadata
char base_pool_metadata[128];

static int base_pool_init;
static umf_memory_pool_ops_t *ops;
static void *pool;

void umf_ba_create_global(void) {
    ops = umfBasePoolOps();

    // do not call umfPoolCreate to avoid inifite recursion
    ops->initialize(NULL, NULL, &pool);
}

void umf_ba_destroy_global(void) { ops->finalize(pool); }

void *umf_ba_global_alloc(size_t size) {
    if (!base_pool_init) {
        assert(size <= sizeof(base_pool_metadata));
        base_pool_init = 1;
        return &base_pool_metadata;
    }

    return ops->malloc(pool, size);
}

void umf_ba_global_free(void *ptr, size_t size) {
    if (ptr == &base_pool_metadata) {
        // nothing to do
        return;
    }

    umf_result_t ret = ops->free(pool, ptr, size);
    assert(ret == UMF_RESULT_SUCCESS);
}

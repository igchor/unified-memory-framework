// Copyright (C) 2023 Intel Corporation
// Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <assert.h>
#include <stdlib.h>

#include "pool_trace.h"
#include <umf/memory_pool_ops.h>

typedef struct umf_pool_trace_params_priv {
    umf_memory_pool_handle_t hUpstreamPool;
    void (*trace)(const char *);
} umf_pool_trace_params_priv_t;

typedef struct trace_pool {
    umf_pool_trace_params_priv_t params;
} trace_pool_t;

static umf_result_t traceInitialize(umf_memory_provider_handle_t provider,
                                    void *params, void **pool) {
    trace_pool_t *trace_pool = (trace_pool_t *)malloc(sizeof(trace_pool_t));
    if (NULL == trace_pool) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    umf_pool_trace_params_t *pub_params = params;
    trace_pool->params.hUpstreamPool = pub_params->hUpstreamPool;
    trace_pool->params.trace = pub_params->trace;

    (void)provider;
    assert(provider);

    *pool = trace_pool;
    return UMF_RESULT_SUCCESS;
}

static void traceFinalize(void *pool) { free(pool); }

static void *traceMalloc(void *pool, size_t size) {
    trace_pool_t *trace_pool = (trace_pool_t *)pool;

    trace_pool->params.trace("malloc");
    return umfPoolMalloc(trace_pool->params.hUpstreamPool, size);
}

static void *traceCalloc(void *pool, size_t num, size_t size) {
    trace_pool_t *trace_pool = (trace_pool_t *)pool;

    trace_pool->params.trace("calloc");
    return umfPoolCalloc(trace_pool->params.hUpstreamPool, num, size);
}

static void *traceRealloc(void *pool, void *ptr, size_t size) {
    trace_pool_t *trace_pool = (trace_pool_t *)pool;

    trace_pool->params.trace("realloc");
    return umfPoolRealloc(trace_pool->params.hUpstreamPool, ptr, size);
}

static void *traceAlignedMalloc(void *pool, size_t size, size_t alignment) {
    trace_pool_t *trace_pool = (trace_pool_t *)pool;

    trace_pool->params.trace("aligned_malloc");
    return umfPoolAlignedMalloc(trace_pool->params.hUpstreamPool, size,
                                alignment);
}

static size_t traceMallocUsableSize(void *pool, void *ptr) {
    trace_pool_t *trace_pool = (trace_pool_t *)pool;

    trace_pool->params.trace("malloc_usable_size");
    return umfPoolMallocUsableSize(trace_pool->params.hUpstreamPool, ptr);
}

static umf_result_t traceFree(void *pool, void *ptr, size_t size) {
    (void)size;
    trace_pool_t *trace_pool = (trace_pool_t *)pool;

    trace_pool->params.trace("free");
    return umfPoolFree(trace_pool->params.hUpstreamPool, ptr);
}

static umf_result_t traceGetLastStatus(void *pool) {
    trace_pool_t *trace_pool = (trace_pool_t *)pool;

    trace_pool->params.trace("get_last_native_error");
    return umfPoolGetLastAllocationError(trace_pool->params.hUpstreamPool);
}

umf_memory_pool_ops_t UMF_TRACE_POOL_OPS = {
    .version = UMF_VERSION_CURRENT,
    .initialize = traceInitialize,
    .finalize = traceFinalize,
    .malloc = traceMalloc,
    .realloc = traceRealloc,
    .calloc = traceCalloc,
    .aligned_malloc = traceAlignedMalloc,
    .malloc_usable_size = traceMallocUsableSize,
    .free = traceFree,
    .get_last_allocation_error = traceGetLastStatus,
};

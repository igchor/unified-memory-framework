/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <stdlib.h>

#include "../memory_targets/memory_target_numa.h"
#include "../memspace_internal.h"
#include "base_alloc_global.h"
#include "base_alloc_linear.h"
#include "memspace_numa.h"

enum umf_result_t
umfMemspaceCreateFromNumaArray(size_t *nodeIds, size_t numIds,
                               umf_memspace_handle_t *hMemspace) {
    if (!nodeIds || numIds == 0 || !hMemspace) {
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    enum umf_result_t ret = UMF_RESULT_SUCCESS;

    umf_ba_alloc_class_t *base_allocator =
        umfBaGetAllocClass(sizeof(struct umf_memspace_t));
    if (!base_allocator) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    umf_memspace_handle_t memspace =
        (struct umf_memspace_t *)umfBaAllocClassAllocate(base_allocator);
    if (!memspace) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    umf_ba_linear_pool_t *linear_allocator =
        umfBaLinearCreate(0 /* minimal pool size */);
    if (!linear_allocator) {
        ret = UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        goto err_umfBaLinearCreate;
    }

    memspace->base_allocator = base_allocator;
    memspace->linear_allocator = linear_allocator;

    memspace->size = numIds;
    memspace->nodes = (umf_memory_target_handle_t *)umfBaLinearAlloc(
        linear_allocator, numIds * sizeof(umf_memory_target_handle_t));
    if (!memspace->nodes) {
        ret = UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        goto err_nodes_alloc;
    }

    size_t nodeIdx;
    for (nodeIdx = 0; nodeIdx < numIds; nodeIdx++) {
        struct umf_numa_memory_target_config_t config = {nodeIds[nodeIdx]};
        ret = umfMemoryTargetCreate(&UMF_MEMORY_TARGET_NUMA_OPS, &config,
                                    &memspace->nodes[nodeIdx]);
        if (ret) {
            goto err_target_create;
        }
    }

    *hMemspace = memspace;

    return UMF_RESULT_SUCCESS;

err_target_create:
    for (size_t i = 0; i < nodeIdx; i++) {
        umfMemoryTargetDestroy(memspace->nodes[i]);
    }
err_nodes_alloc:
    umfBaLinearDestroy(linear_allocator);
err_umfBaLinearCreate:
    umfBaAllocClassFree(base_allocator, memspace);
    return ret;
}

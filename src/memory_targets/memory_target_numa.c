/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <assert.h>
#include <hwloc.h>
#include <stdlib.h>

#include <umf/pools/pool_disjoint.h>
#include <umf/providers/provider_os_memory.h>

#include "../memory_pool_internal.h"
#include "base_alloc.h"
#include "base_alloc_global.h"
#include "base_alloc_linear.h"
#include "memory_target_numa.h"

struct numa_memory_target_t {
    size_t id;
};

static umf_result_t numa_initialize(void *params, void **memTarget) {
    if (params == NULL || memTarget == NULL) {
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    struct umf_numa_memory_target_config_t *config =
        (struct umf_numa_memory_target_config_t *)params;

    struct numa_memory_target_t *numaTarget =
        umf_ba_global_alloc(sizeof(struct numa_memory_target_t));
    if (!numaTarget) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    numaTarget->id = config->id;
    *memTarget = numaTarget;
    return UMF_RESULT_SUCCESS;
}

static void numa_finalize(void *memTarget) {
    umf_ba_global_free(memTarget, sizeof(struct numa_memory_target_t));
}

// sets maxnode and allocates and initializes mask based on provided memory targets
static umf_result_t
numa_targets_create_nodemask(struct numa_memory_target_t **targets,
                             size_t numTargets, unsigned long **mask,
                             unsigned *maxnode,
                             umf_ba_linear_pool_t *linear_allocator) {
    assert(targets);
    assert(mask);
    assert(maxnode);

    hwloc_bitmap_t bitmap = hwloc_bitmap_alloc();
    if (!bitmap) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (size_t i = 0; i < numTargets; i++) {
        if (hwloc_bitmap_set(bitmap, targets[i]->id)) {
            hwloc_bitmap_free(bitmap);
            return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    int lastBit = hwloc_bitmap_last(bitmap);
    if (lastBit == -1) {
        // no node is set
        hwloc_bitmap_free(bitmap);
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *maxnode = lastBit + 1;

    // Do not use hwloc_bitmap_nr_ulongs due to:
    // https://github.com/open-mpi/hwloc/issues/429
    unsigned bits_per_long = sizeof(unsigned long) * 8;
    int nrUlongs = (lastBit + bits_per_long) / bits_per_long;

    unsigned long *nodemask = umf_ba_linear_alloc(
        linear_allocator, (sizeof(unsigned long) * nrUlongs));
    if (!nodemask) {
        hwloc_bitmap_free(bitmap);
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    int ret = hwloc_bitmap_to_ulongs(bitmap, nrUlongs, nodemask);
    hwloc_bitmap_free(bitmap);

    if (ret) {
        // nodemask is freed during destroying linear_allocator
        return UMF_RESULT_ERROR_UNKNOWN;
    }

    *mask = nodemask;

    return UMF_RESULT_SUCCESS;
}

static enum umf_result_t numa_memory_provider_create_from_memspace(
    umf_memspace_handle_t memspace, void **memTargets, size_t numTargets,
    umf_memspace_policy_handle_t policy,
    umf_memory_provider_handle_t *provider) {
    (void)memspace;
    // TODO: apply policy
    (void)policy;

    struct numa_memory_target_t **numaTargets =
        (struct numa_memory_target_t **)memTargets;

    unsigned long *nodemask;
    unsigned maxnode;

    umf_ba_linear_pool_t *linear_allocator =
        umf_ba_linear_create(0 /* minimal pool size */);
    if (!linear_allocator) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    umf_result_t ret = numa_targets_create_nodemask(
        numaTargets, numTargets, &nodemask, &maxnode, linear_allocator);
    if (ret != UMF_RESULT_SUCCESS) {
        return ret;
    }

    umf_os_memory_provider_params_t params = umfOsMemoryProviderParamsDefault();
    params.nodemask = nodemask;
    params.maxnode = maxnode;
    params.numa_mode = UMF_NUMA_MODE_BIND;
    params.numa_flags = UMF_NUMA_FLAGS_STRICT;

    umf_memory_provider_handle_t numaProvider = NULL;
    ret = umfMemoryProviderCreate(umfOsMemoryProviderOps(), &params,
                                  &numaProvider);
    umf_ba_linear_destroy(linear_allocator); // nodemask is freed here
    if (ret) {

        return ret;
    }

    *provider = numaProvider;

    return UMF_RESULT_SUCCESS;
}

static umf_result_t numa_pool_create_from_memspace(
    umf_memspace_handle_t memspace, void **memTargets, size_t numTargets,
    umf_memspace_policy_handle_t policy, umf_memory_pool_handle_t *pool) {
    (void)memspace;
    (void)memTargets;
    (void)numTargets;
    (void)policy;
    (void)pool;
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
}

struct umf_memory_target_ops_t UMF_MEMORY_TARGET_NUMA_OPS = {
    .version = UMF_VERSION_CURRENT,
    .initialize = numa_initialize,
    .finalize = numa_finalize,
    .pool_create_from_memspace = numa_pool_create_from_memspace,
    .memory_provider_create_from_memspace =
        numa_memory_provider_create_from_memspace};

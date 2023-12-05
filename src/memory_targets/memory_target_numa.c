/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <stdlib.h>

#include "memory_target_numa.h"
#include <umf/pools/pool_disjoint.h>
#include <umf/providers/provider_os_memory.h>

struct numa_memory_target_t {
    size_t id;
};

static enum umf_result_t numa_initialize(void *params, void **mem_target) {
    struct umf_numa_memory_target_config_t *config =
        (struct umf_numa_memory_target_config_t *)params;

    struct numa_memory_target_t *numa_target =
        malloc(sizeof(struct numa_memory_target_t));
    if (!numa_target) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    numa_target->id = config->id;
    *mem_target = numa_target;
    return UMF_RESULT_SUCCESS;
}

static void numa_finalize(void *mem_target) { free(mem_target); }

static const umf_os_memory_provider_params_t
    UMF_OS_MEMORY_PROVIDER_PARAMS_DEFAULT = {
        // Visibility & protection
        .protection = UMF_PROTECTION_READ | UMF_PROTECTION_WRITE,
        .visibility = UMF_VISIBILITY_PRIVATE,

        // NUMA config
        .nodemask = NULL,
        .maxnode = 0, // TODO: numa_max_node/GetNumaHighestNodeNumber
        .numa_mode = UMF_NUMA_MODE_DEFAULT,
        .numa_flags = UMF_NUMA_FLAGS_STRICT, // TODO: determine default behavior

        // Logging
        .traces = 0, // TODO: parse env variable for log level?
};

static enum umf_result_t numa_memory_provider_create_from_memspace(
    umf_memspace_handle_t memspace, void **mem_targets, size_t num_targets,
    umf_memspace_policy_handle_t policy,
    umf_memory_provider_handle_t *provider) {
    (void)memspace;
    // TODO: apply policy
    (void)policy;

    struct numa_memory_target_t **numa_targets =
        (struct numa_memory_target_t **)mem_targets;

    // Create node mask from IDs
    unsigned long nodemask = 0;
    unsigned long maxnode = 0;
    for (size_t i = 0; i < num_targets; i++) {
        nodemask |= (1UL << numa_targets[i]->id);
        maxnode = (numa_targets[i]->id > maxnode)
                      ? (unsigned long)numa_targets[i]->id
                      : maxnode;
    }

    umf_os_memory_provider_params_t params =
        UMF_OS_MEMORY_PROVIDER_PARAMS_DEFAULT;
    params.nodemask = &nodemask;

    umf_memory_provider_handle_t numa_provider = NULL;
    enum umf_result_t ret = umfMemoryProviderCreate(&UMF_OS_MEMORY_PROVIDER_OPS,
                                                    &params, &numa_provider);
    if (ret) {
        return ret;
    }

    *provider = numa_provider;

    return UMF_RESULT_SUCCESS;
}

static enum umf_result_t numa_pool_create_from_memspace(
    umf_memspace_handle_t memspace, void **mem_targets, size_t num_targets,
    umf_memspace_policy_handle_t policy, umf_memory_pool_handle_t *pool) {
    umf_memory_provider_handle_t numa_provider = NULL;
    enum umf_result_t ret = numa_memory_provider_create_from_memspace(
        memspace, mem_targets, num_targets, policy, &numa_provider);
    if (ret) {
        return ret;
    }

    struct umf_disjoint_pool_params params = umfDisjointPoolParamsDefault();
    umf_memory_pool_handle_t disjoint_pool = NULL;
    // TODO: determine which pool implementation to use
    ret = umfPoolCreateOwning(&UMF_DISJOINT_POOL_OPS, numa_provider, &params,
                              &disjoint_pool);
    if (ret) {
        umfMemoryProviderDestroy(numa_provider);
        return ret;
    }

    *pool = disjoint_pool;

    return UMF_RESULT_SUCCESS;
}

struct umf_memory_target_ops_t UMF_MEMORY_TARGET_NUMA_OPS = {
    .version = UMF_VERSION_CURRENT,
    .initialize = numa_initialize,
    .finalize = numa_finalize,
    .pool_create_from_memspace = numa_pool_create_from_memspace,
    .memory_provider_create_from_memspace =
        numa_memory_provider_create_from_memspace};

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

static enum umf_result_t numa_pool_create_from_memspace(
    umf_memspace_handle_t memspace, void **mem_targets,
    umf_memspace_policy_handle_t policy, umf_memory_pool_handle_t *pool) {
    // TODO: cast mem_targets to (struct numa_memory_target_t**), parse ids and initialize os provider
    (void)memspace;
    (void)mem_targets;
    (void)policy;
    (void)pool;
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
}

static enum umf_result_t numa_memory_provider_create_from_memspace(
    umf_memspace_handle_t memspace, void **mem_targets,
    umf_memspace_policy_handle_t policy,
    umf_memory_provider_handle_t *provider) {
    // TODO: cast mem_targets to (struct numa_memory_target_t**), parse ids and initialize os provider
    (void)memspace;
    (void)mem_targets;
    (void)policy;
    (void)provider;
    return UMF_RESULT_ERROR_NOT_SUPPORTED;
}

struct umf_memory_target_ops_t UMF_MEMORY_TARGET_NUMA_OPS = {
    .initialize = numa_initialize,
    .finalize = numa_finalize,
    .pool_create_from_memspace = numa_pool_create_from_memspace,
    .memory_provider_create_from_memspace =
        numa_memory_provider_create_from_memspace};

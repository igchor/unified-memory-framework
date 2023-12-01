/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_MEMORY_TARGET_OPS_H
#define UMF_MEMORY_TARGET_OPS_H 1

#include <stdbool.h>

#include <umf/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct umf_memory_target_t *umf_memory_target_handle_t;

struct umf_memory_target_ops_t {
    /// Version of the ops structure.
    /// Should be initialized using UMF_VERSION_CURRENT
    uint32_t version;

    enum umf_result_t (*initialize)(void *params, void **mem_target);
    void (*finalize)(void *mem_target);

    enum umf_result_t (*poolCreateFromMemspace)(
        umf_memspace_handle_t memspace, void **mem_targets,
        umf_memspace_policy_handle_t policy, umf_memory_pool_handle_t *pool);

    enum umf_result_t (*memoryProviderCreateFromMemspace)(
        umf_memspace_handle_t memspace, void **mem_targets,
        umf_memspace_policy_handle_t policy,
        umf_memory_provider_handle_t *provider);

    // TODO: query API (capacity, distance, etc.)
};

#ifdef __cplusplus
}
#endif

#endif /* #ifndef UMF_MEMORY_TARGET_OPS_H */
/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_MEMSPACE_H
#define UMF_MEMSPACE_H 1

#include <umf/base.h>
#include <umf/memory_pool.h>
#include <umf/memory_provider.h>
#include <umf/memory_target.h>
#include <umf/memspace_policy.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct umf_memspace_t *umf_memspace_handle_t;

enum umf_result_t umfMemspaceCreateFromMemoryTargets(
    const umf_memory_target_handle_t *memoryTargets, size_t numTargets,
    umf_memspace_handle_t *memspace);

umf_result_t umfPoolCreateFromMemspace(umf_memspace_handle_t memspace,
                                       umf_memspace_policy_handle_t policy,
                                       umf_memory_pool_handle_t *pool);
umf_result_t
umfMemoryProviderCreateFromMemspace(umf_memspace_handle_t memspace,
                                    umf_memspace_policy_handle_t policy,
                                    umf_memory_provider_handle_t *provider);

// TODO: iteration, filtering, sorting API
// can we just define memspace as an array of targets?

#ifdef __cplusplus
}
#endif

#endif /* UMF_MEMSPACE_H */
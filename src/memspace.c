/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <assert.h>
#include <stdlib.h>

#include <umf/memspace.h>

#include "memory_target.h"
#include "memory_target_ops.h"

struct umf_memspace_t {
    size_t size;

    // TODO: consider flexible array member (if we don't want to expose the
    // struct definition in the headers)
    umf_memory_target_handle_t *nodes;
};

#ifndef NDEBUG
static enum umf_result_t verifyMemTargetsTypes(umf_memspace_handle_t memspace) {
    if (memspace->size == 0) {
        return UMF_RESULT_ERROR_INVALID_ARGUMENT;
    }

    struct umf_memory_target_ops_t *ops = memspace->nodes[0]->ops;
    for (size_t i = 1; i < memspace->size; i++) {
        if (memspace->nodes[i]->ops != ops) {
            return UMF_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    return UMF_RESULT_SUCCESS;
}
#endif

static enum umf_result_t
memoryTargetHandlesToPriv(umf_memspace_handle_t memspace, void **privs) {
    privs = malloc(sizeof(void *) * memspace->size);
    if (privs == NULL) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (size_t i = 0; i < memspace->size; i++) {
        privs[i] = memspace->nodes[i]->priv;
    }

    return UMF_RESULT_SUCCESS;
}

enum umf_result_t umfPoolCreateFromMemspace(umf_memspace_handle_t memspace,
                                            umf_memspace_policy_handle_t policy,
                                            umf_memory_pool_handle_t *pool) {
    assert(verifyMemTargetsTypes(memspace) == UMF_RESULT_SUCCESS);

    void *privs = NULL;
    enum umf_result_t ret = memoryTargetHandlesToPriv(memspace, &privs);
    if (ret != UMF_RESULT_SUCCESS) {
        return ret;
    }

    ret = memspace->nodes[0]->ops->pool_create_from_memspace(memspace, privs,
                                                             policy, pool);
    free(privs);

    return ret;
}

enum umf_result_t
umfMemoryProviderCreateFromMemspace(umf_memspace_handle_t memspace,
                                    umf_memspace_policy_handle_t policy,
                                    umf_memory_provider_handle_t *provider) {
    assert(verifyMemTargetsTypes(memspace) == UMF_RESULT_SUCCESS);

    void *privs = NULL;
    enum umf_result_t ret = memoryTargetHandlesToPriv(memspace, &privs);
    if (ret != UMF_RESULT_SUCCESS) {
        return ret;
    }

    ret = memspace->nodes[0]->ops->memory_provider_create_from_memspace(
        memspace, privs, policy, provider);
    free(privs);

    return ret;
}

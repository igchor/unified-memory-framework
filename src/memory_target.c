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

#include "memory_target.h"
#include "memory_target_ops.h"

enum umf_result_t
umfMemoryTargetCreate(const struct umf_memory_target_ops_t *ops, void *params,
                      umf_memory_target_handle_t *memory_target) {
    umf_memory_target_handle_t target = (struct umf_memory_target_t *)malloc(
        sizeof(struct umf_memory_target_t));
    if (!target) {
        return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    assert(ops->version == UMF_VERSION_CURRENT);

    target->ops = ops;

    void *target_priv;
    enum umf_result_t ret = ops->initialize(params, &target_priv);
    if (ret != UMF_RESULT_SUCCESS) {
        free(target);
        return ret;
    }

    target->priv = target_priv;

    *memory_target = target;

    return UMF_RESULT_SUCCESS;
}

void umfMemoryTargetDestroy(umf_memory_target_handle_t memory_target) {
    memory_target->ops->finalize(memory_target->priv);
    free(memory_target);
}

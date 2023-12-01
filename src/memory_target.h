/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_MEMORY_TARGET_H
#define UMF_MEMORY_TARGET_H 1

#include <umf/base.h>

#ifdef __cplusplus
extern "C" {
#endif

struct umf_memory_target_ops_t;

struct umf_memory_target_t {
    struct umf_memory_target_ops_t *ops;
    void *priv;
};

typedef struct umf_memory_target_t *umf_memory_target_handle_t;

enum umf_result_t
umfMemoryTargetCreate(struct umf_memory_target_ops_t *ops, void *params,
                      umf_memory_target_handle_t *memory_target);

void umfMemoryTargetDestroy(umf_memory_target_handle_t memory_target);

#ifdef __cplusplus
}
#endif

#endif /* UMF_MEMORY_TARGET_H */

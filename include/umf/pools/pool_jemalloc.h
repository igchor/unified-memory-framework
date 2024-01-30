/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_JEMALLOC_MEMORY_POOL_H
#define UMF_JEMALLOC_MEMORY_POOL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <umf/memory_pool_ops.h>

UMF_EXPORT extern umf_memory_pool_ops_t UMF_JEMALLOC_POOL_OPS;

#ifdef __cplusplus
}
#endif

#endif /* UMF_JEMALLOC_MEMORY_POOL_H */

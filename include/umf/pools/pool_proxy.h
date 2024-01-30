/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_PROXY_MEMORY_POOL_H
#define UMF_PROXY_MEMORY_POOL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <umf/memory_pool.h>
#include <umf/memory_provider.h>

UMF_EXPORT umf_memory_pool_ops_t UMF_PROXY_POOL_OPS;

#ifdef __cplusplus
}
#endif

#endif /* UMF_PROXY_MEMORY_POOL_H */

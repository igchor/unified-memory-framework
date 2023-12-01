/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_MEMORY_TARGET_NUMA_H
#define UMF_MEMORY_TARGET_NUMA_H 1

#include <umf.h>
#include <umf/memory_target.h>
#include <umf/memory_target_ops.h>
#include <umf/memspace.h>

#ifdef __cplusplus
extern "C" {
#endif

struct umf_numa_memory_target_config_t {
    size_t id;
};

enum umf_memory_target_numa_type_t { UNKNOWN, DRAM, HBM };

extern struct umf_memory_target_ops_t UMF_MEMORY_TARGET_NUMA_OPS;
extern umf_memspace_handle_t UMF_MEMSPACE_HOST;

// TODO: should we have per-type queries or should this be generic (part of memory_target API)?
size_t umfMemoryTargetNumaGetId(umf_memory_target_handle_t memoryTarget);

enum umf_memory_target_numa_type_t
umfMemoryTargetNumaGetType(umf_memory_target_handle_t memoryTarget);

#ifdef __cplusplus
}
#endif

#endif /* UMF_MEMORY_TARGET_NUMA_H */
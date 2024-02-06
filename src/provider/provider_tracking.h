/*
 *
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_MEMORY_TRACKER_INTERNAL_H
#define UMF_MEMORY_TRACKER_INTERNAL_H 1

#include <assert.h>
#include <stdlib.h>

#include <umf/base.h>
#include <umf/memory_pool.h>
#include <umf/memory_provider.h>

#include "base_alloc.h"
#include "base_alloc_linear.h"
#include "critnib.h"
#include "utils_concurrency.h"

#ifdef __cplusplus
extern "C" {
#endif

struct umf_memory_tracker_t {
    umf_ba_linear_pool_t *pool_linear;
    umf_ba_alloc_class_t *tracker_allocator;
    critnib *map;
    os_mutex_t splitMergeMutex;
};

typedef struct umf_memory_tracker_t *umf_memory_tracker_handle_t;

extern umf_memory_tracker_handle_t TRACKER;

umf_memory_tracker_handle_t umfMemoryTrackerCreate(void);
void umfMemoryTrackerDestroy(umf_memory_tracker_handle_t handle);

umf_memory_pool_handle_t umfMemoryTrackerGetPool(const void *ptr);

// Creates a memory provider that tracks each allocation/deallocation through umf_memory_tracker_handle_t and
// forwards all requests to hUpstream memory Provider. hUpstream lifetime should be managed by the user of this function.
umf_result_t umfTrackingMemoryProviderCreate(
    umf_memory_provider_handle_t hUpstream, umf_memory_pool_handle_t hPool,
    umf_memory_provider_handle_t *hTrackingProvider);

void umfTrackingMemoryProviderGetUpstreamProvider(
    umf_memory_provider_handle_t hTrackingProvider,
    umf_memory_provider_handle_t *hUpstream);

#ifdef __cplusplus
}
#endif

#endif /* UMF_MEMORY_TRACKER_INTERNAL_H */

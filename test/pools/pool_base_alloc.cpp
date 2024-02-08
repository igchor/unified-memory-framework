// Copyright (C) 2023 Intel Corporation
// Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <unordered_map>

#include "umf/pools/pool_base.h"

#include "pool.hpp"
#include "poolFixtures.hpp"
#include "provider.hpp"

struct base_alloc_pool : public umf_test::pool_base_t {
    std::unordered_map<void *, size_t> sizes;
    std::mutex m;
    umf_memory_pool_handle_t pool;

    umf_result_t initialize(umf_memory_provider_handle_t) {
        auto ops = umfBasePoolOps();
        return umfPoolCreate(ops, nullptr, nullptr, 0, &pool);
    }

    void *malloc(size_t size) noexcept {
        auto *ptr = umfPoolMalloc(pool, size);
        std::unique_lock<std::mutex> l(m);
        sizes[ptr] = size;
        return ptr;
    }
    void *calloc(size_t, size_t) noexcept {
        umf::getPoolLastStatusRef<base_alloc_pool>() =
            UMF_RESULT_ERROR_NOT_SUPPORTED;
        return NULL;
    }
    void *realloc(void *, size_t) noexcept {
        umf::getPoolLastStatusRef<base_alloc_pool>() =
            UMF_RESULT_ERROR_NOT_SUPPORTED;
        return NULL;
    }
    void *aligned_malloc(size_t, size_t) noexcept {
        umf::getPoolLastStatusRef<base_alloc_pool>() =
            UMF_RESULT_ERROR_NOT_SUPPORTED;
        return NULL;
    }
    size_t malloc_usable_size(void *ptr) noexcept {
        std::unique_lock<std::mutex> l(m);
        return sizes[ptr];
    }
    umf_result_t free(void *ptr, size_t) noexcept {
        size_t size;
        {
            std::unique_lock<std::mutex> l(m);
            size = sizes[ptr];
        }

        return umfPoolFreeSized(pool, ptr, size);
    }
    umf_result_t get_last_allocation_error() {
        return umf::getPoolLastStatusRef<base_alloc_pool>();
    }
};

umf_memory_pool_ops_t BA_POOL_OPS = umf::poolMakeCOps<base_alloc_pool, void>();

INSTANTIATE_TEST_SUITE_P(baPool, umfPoolTest,
                         ::testing::Values(poolCreateExtParams{
                             &BA_POOL_OPS, nullptr,
                             &umf_test::BASE_PROVIDER_OPS, nullptr}));

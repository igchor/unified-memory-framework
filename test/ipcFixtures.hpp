// Copyright (C) 2024 Intel Corporation
// Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef UMF_TEST_IPC_FIXTURES_HPP
#define UMF_TEST_IPC_FIXTURES_HPP

#include "base.hpp"
#include "multithread_helpers.hpp"
#include "pool.hpp"
#include "test_helpers.h"

#include <umf/ipc.h>
#include <umf/memory_pool.h>
#include <umf/memory_provider.h>
#include <umf/pools/pool_proxy.h>

#include <cstring>
#include <numeric>
#include <tuple>

using testParams = std::tuple<umf_memory_pool_ops_t *, void *,
                              umf_memory_provider_ops_t *, void *>;

struct umfIpcTest : umf_test::test, ::testing::WithParamInterface<testParams> {
    umfIpcTest() : pool(nullptr, nullptr) {}
    void SetUp() override {
        test::SetUp();
        this->pool = makePool();
    }

    void TearDown() override { test::TearDown(); }

    umf::pool_unique_handle_t makePool() {
        // TODO: The function is similar to poolCreateExt function
        //       from memoryPool.hpp
        umf_memory_provider_handle_t hProvider;
        umf_memory_pool_handle_t hPool;
        auto [pool_ops, pool_params, provider_ops, provider_params] =
            this->GetParam();

        auto ret =
            umfMemoryProviderCreate(provider_ops, provider_params, &hProvider);
        EXPECT_EQ(ret, UMF_RESULT_SUCCESS);

        auto trace = [](void *trace_context, const char *name) {
            stats_type *stat = static_cast<stats_type *>(trace_context);
            if (std::strcmp(name, "get_ipc_handle") == 0) {
                ++stat->getCount;
            } else if (std::strcmp(name, "put_ipc_handle") == 0) {
                ++stat->putCount;
            } else if (std::strcmp(name, "open_ipc_handle") == 0) {
                ++stat->openCount;
            } else if (std::strcmp(name, "close_ipc_handle") == 0) {
                ++stat->closeCount;
            }
        };

        umf_memory_provider_handle_t hTraceProvider =
            traceProviderCreate(hProvider, true, (void *)&stat, trace);

        ret = umfPoolCreate(pool_ops, hTraceProvider, pool_params,
                            UMF_POOL_CREATE_FLAG_OWN_PROVIDER, &hPool);
        EXPECT_EQ(ret, UMF_RESULT_SUCCESS);

        return umf::pool_unique_handle_t(hPool, &umfPoolDestroy);
    }

    struct stats_type {
        std::atomic<size_t> getCount;
        std::atomic<size_t> putCount;
        std::atomic<size_t> openCount;
        std::atomic<size_t> closeCount;

        stats_type() : getCount(0), putCount(0), openCount(0), closeCount(0) {}
    };

    umf::pool_unique_handle_t pool;
    static constexpr int NTHREADS = 10;
    stats_type stat;
};

TEST_P(umfIpcTest, BasicFlow) {
    constexpr size_t SIZE = 100;
    int *ptr = (int *)umfPoolMalloc(pool.get(), SIZE * sizeof(int));
    EXPECT_NE(ptr, nullptr);

    std::iota(ptr, ptr + SIZE, 0);

    umf_ipc_handle_t ipcHandleFull = nullptr;
    size_t handleFullSize = 0;
    umf_result_t ret = umfGetIPCHandle(ptr, &ipcHandleFull, &handleFullSize);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
    umf_ipc_handle_t ipcHandleHalf = nullptr;
    size_t handleHalfSize = 0;
    ret = umfGetIPCHandle(ptr + SIZE / 2, &ipcHandleHalf, &handleHalfSize);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
    ASSERT_EQ(handleFullSize, handleHalfSize);

    void *fullArray = nullptr;
    ret = umfOpenIPCHandle(pool.get(), ipcHandleFull, &fullArray);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);

    void *halfArray = nullptr;
    ret = umfOpenIPCHandle(pool.get(), ipcHandleHalf, &halfArray);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);

    for (int i = 0; i < (int)SIZE; ++i) {
        ASSERT_EQ(reinterpret_cast<int *>(fullArray)[i], i);
    }
    // Close fullArray before reading halfArray
    ret = umfCloseIPCHandle(fullArray);
    EXPECT_EQ(ret, UMF_RESULT_SUCCESS);

    for (int i = 0; i < (int)SIZE / 2; ++i) {
        ASSERT_EQ(reinterpret_cast<int *>(halfArray)[i], i + SIZE / 2);
    }
    ret = umfCloseIPCHandle(halfArray);
    EXPECT_EQ(ret, UMF_RESULT_SUCCESS);

    ret = umfPutIPCHandle(ipcHandleFull);
    EXPECT_EQ(ret, UMF_RESULT_SUCCESS);

    ret = umfPutIPCHandle(ipcHandleHalf);
    EXPECT_EQ(ret, UMF_RESULT_SUCCESS);

    ret = umfPoolFree(pool.get(), ptr);
    EXPECT_EQ(ret, UMF_RESULT_SUCCESS);

    EXPECT_EQ(stat.getCount, 1);
    EXPECT_EQ(stat.putCount, stat.getCount);
    // TODO: enale check below once cache for open IPC handles is implemented
    // EXPECT_EQ(stat.openCount, 1);
    EXPECT_EQ(stat.closeCount, stat.openCount);
}

TEST_P(umfIpcTest, ConcurrentGetPutHandles) {
    std::vector<void *> ptrs;
    constexpr size_t ALLOC_SIZE = 100;
    constexpr size_t NUM_POINTERS = 100;
    for (size_t i = 0; i < NUM_POINTERS; ++i) {
        void *ptr = umfPoolMalloc(pool.get(), ALLOC_SIZE);
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    std::array<std::vector<umf_ipc_handle_t>, NTHREADS> ipcHandles;

    umf_test::syncthreads_barrier syncthreads(NTHREADS);

    auto getHandlesFn = [&ipcHandles, &ptrs, &syncthreads](size_t tid) {
        syncthreads();
        for (void *ptr : ptrs) {
            umf_ipc_handle_t ipcHandle;
            size_t handleSize;
            umf_result_t ret = umfGetIPCHandle(ptr, &ipcHandle, &handleSize);
            ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
            ipcHandles[tid].push_back(ipcHandle);
        }
    };

    umf_test::parallel_exec(NTHREADS, getHandlesFn);

    auto putHandlesFn = [&ipcHandles, &syncthreads](size_t tid) {
        syncthreads();
        for (umf_ipc_handle_t ipcHandle : ipcHandles[tid]) {
            umf_result_t ret = umfPutIPCHandle(ipcHandle);
            EXPECT_EQ(ret, UMF_RESULT_SUCCESS);
        }
    };

    umf_test::parallel_exec(NTHREADS, putHandlesFn);

    for (void *ptr : ptrs) {
        umf_result_t ret = umfPoolFree(pool.get(), ptr);
        EXPECT_EQ(ret, UMF_RESULT_SUCCESS);
    }

    EXPECT_GE(stat.getCount, NUM_POINTERS);
    EXPECT_LE(stat.getCount, NUM_POINTERS * NTHREADS);
    EXPECT_EQ(stat.putCount, stat.getCount);
}

TEST_P(umfIpcTest, ConcurrentOpenCloseHandles) {
    std::vector<void *> ptrs;
    constexpr size_t ALLOC_SIZE = 100;
    constexpr size_t NUM_POINTERS = 100;
    for (size_t i = 0; i < NUM_POINTERS; ++i) {
        void *ptr = umfPoolMalloc(pool.get(), ALLOC_SIZE);
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    std::array<umf_ipc_handle_t, NUM_POINTERS> ipcHandles;
    for (size_t i = 0; i < NUM_POINTERS; ++i) {
        umf_ipc_handle_t ipcHandle;
        size_t handleSize;
        umf_result_t ret = umfGetIPCHandle(ptrs[i], &ipcHandle, &handleSize);
        ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
        ipcHandles[i] = ipcHandle;
    }

    std::array<std::vector<void *>, NTHREADS> openedIpcHandles;

    umf_test::syncthreads_barrier syncthreads(NTHREADS);

    auto openHandlesFn = [this, &ipcHandles, &openedIpcHandles,
                          &syncthreads](size_t tid) {
        syncthreads();
        for (auto ipcHandle : ipcHandles) {
            void *ptr;
            umf_result_t ret = umfOpenIPCHandle(pool.get(), ipcHandle, &ptr);
            ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
            openedIpcHandles[tid].push_back(ptr);
        }
    };

    umf_test::parallel_exec(NTHREADS, openHandlesFn);

    auto closeHandlesFn = [&openedIpcHandles, &syncthreads](size_t tid) {
        syncthreads();
        for (void *ptr : openedIpcHandles[tid]) {
            umf_result_t ret = umfCloseIPCHandle(ptr);
            EXPECT_EQ(ret, UMF_RESULT_SUCCESS);
        }
    };

    umf_test::parallel_exec(NTHREADS, closeHandlesFn);

    for (auto ipcHandle : ipcHandles) {
        umf_result_t ret = umfPutIPCHandle(ipcHandle);
        EXPECT_EQ(ret, UMF_RESULT_SUCCESS);
    }

    for (void *ptr : ptrs) {
        umf_result_t ret = umfPoolFree(pool.get(), ptr);
        EXPECT_EQ(ret, UMF_RESULT_SUCCESS);
    }

    EXPECT_EQ(stat.openCount, stat.closeCount);
}

#endif /* UMF_TEST_IPC_FIXTURES_HPP */

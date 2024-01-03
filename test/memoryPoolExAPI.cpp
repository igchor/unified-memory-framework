// Copyright (C) 2023 Intel Corporation
// Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "base.hpp"
#include "common/provider_null.h"
#include "memory_pool_internal.h"
#include "pool.hpp"
#include "provider_trace.h"
#include "test_helpers.h"

#include <unordered_map>

struct umfPoolExTest : umf_test::test,
                       ::testing::WithParamInterface<umf_pool_create_flags_t> {
    void SetUp() override {
        test::SetUp();
        flags = this->GetParam();
    }
    umf_pool_create_flags_t flags;
};

template <typename T> umf_memory_pool_ops_t poolNoParamsMakeCOps() {
    umf_memory_pool_ops_t ops = umf::detail::poolOpsBase<T>();

    ops.initialize = [](umf_memory_provider_handle_t provider, void *params,
                        void **obj) {
        (void)params;
        try {
            *obj = new T;
        } catch (...) {
            return UMF_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }

        return umf::detail::initialize<T>(reinterpret_cast<T *>(*obj),
                                          std::make_tuple(provider));
    };

    return ops;
}

umf_memory_pool_ops_t PROXY_POOL_OPS =
    poolNoParamsMakeCOps<umf_test::proxy_pool>();

TEST_P(umfPoolExTest, poolCreateExSuccess) {
    umf_memory_provider_handle_t provider = nullptr;
    umf_result_t ret =
        umfMemoryProviderCreate(&UMF_NULL_PROVIDER_OPS, nullptr, &provider);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
    ASSERT_NE(provider, nullptr);

    umf_memory_pool_handle_t pool = nullptr;
    ret = umfPoolCreateEx(&PROXY_POOL_OPS, provider, nullptr, flags, &pool);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
    ASSERT_NE(pool, nullptr);

    umfPoolDestroy(pool);

    if (flags & UMF_POOL_CREATE_FLAG_OWN_PROVIDER) {
        umfMemoryProviderDestroy(provider);
    }
}

TEST_P(umfPoolExTest, poolCreateExNullOps) {
    umf_memory_provider_handle_t provider = nullptr;
    umf_result_t ret =
        umfMemoryProviderCreate(&UMF_NULL_PROVIDER_OPS, nullptr, &provider);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
    ASSERT_NE(provider, nullptr);

    umf_memory_pool_handle_t pool = nullptr;
    ret = umfPoolCreateEx(nullptr, provider, nullptr, flags, &pool);
    ASSERT_EQ(ret, UMF_RESULT_ERROR_INVALID_ARGUMENT);

    umfMemoryProviderDestroy(provider);
}

TEST_P(umfPoolExTest, poolCreateExNullPoolHandle) {
    umf_memory_provider_handle_t provider = nullptr;
    umf_result_t ret =
        umfMemoryProviderCreate(&UMF_NULL_PROVIDER_OPS, nullptr, &provider);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
    ASSERT_NE(provider, nullptr);

    ret = umfPoolCreateEx(&PROXY_POOL_OPS, provider, nullptr, flags, nullptr);
    ASSERT_EQ(ret, UMF_RESULT_ERROR_INVALID_ARGUMENT);

    umfMemoryProviderDestroy(provider);
}

TEST_P(umfPoolExTest, poolCreateExCountProviderCalls) {
    auto nullProvider = umf_test::wrapProviderUnique(nullProviderCreate());

    static std::unordered_map<std::string, size_t> providerCalls;
    providerCalls.clear();
    auto traceCb = [](const char *name) { providerCalls[name]++; };

    umf_provider_trace_params_t provider_params = {nullProvider.get(), traceCb};

    umf_memory_provider_handle_t provider = nullptr;
    umf_result_t ret = umfMemoryProviderCreate(&UMF_TRACE_PROVIDER_OPS,
                                               &provider_params, &provider);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
    ASSERT_NE(provider, nullptr);

    umf_memory_pool_handle_t pool = nullptr;
    ret = umfPoolCreateEx(&PROXY_POOL_OPS, provider, nullptr, flags, &pool);
    ASSERT_EQ(ret, UMF_RESULT_SUCCESS);
    ASSERT_NE(pool, nullptr);

    size_t provider_call_count = 0;

    umfPoolMalloc(pool, 0);
    ASSERT_EQ(providerCalls["alloc"], 1);
    ASSERT_EQ(providerCalls.size(), ++provider_call_count);

    umfPoolFree(pool, 0);
    ASSERT_EQ(providerCalls["free"], 1);
    ASSERT_EQ(providerCalls.size(), ++provider_call_count);

    umfPoolCalloc(pool, 0, 0);
    ASSERT_EQ(providerCalls["alloc"], 2);
    ASSERT_EQ(providerCalls.size(), provider_call_count);

    umfPoolAlignedMalloc(pool, 0, 0);
    ASSERT_EQ(providerCalls["alloc"], 3);
    ASSERT_EQ(providerCalls.size(), provider_call_count);

    umfPoolDestroy(pool);

    if (flags & UMF_POOL_CREATE_FLAG_OWN_PROVIDER) {
        umfMemoryProviderDestroy(provider);
    }
}

INSTANTIATE_TEST_SUITE_P(umfPoolEx, umfPoolExTest,
                         ::testing::Values(0,
                                           UMF_POOL_CREATE_FLAG_OWN_PROVIDER));

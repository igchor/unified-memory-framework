/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_HELPERS_H
#define UMF_HELPERS_H 1

#include <umf/memory_pool.h>
#include <umf/memory_pool_ops.h>
#include <umf/memory_provider.h>
#include <umf/memory_provider_ops.h>

#include <array>
#include <functional>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace umf {

using pool_unique_handle_t =
    std::unique_ptr<umf_memory_pool_t,
                    std::function<void(umf_memory_pool_handle_t)>>;
using provider_unique_handle_t =
    std::unique_ptr<umf_memory_provider_t,
                    std::function<void(umf_memory_provider_handle_t)>>;

#define UMF_ASSIGN_OP(ops, type, func, default_return)                         \
    ops.func = [](void *obj, auto... args) {                                   \
        return reinterpret_cast<type *>(obj)->func(args...);                   \
    }

#define UMF_ASSIGN_OP_NORETURN(ops, type, func)                                \
    ops.func = [](void *obj, auto... args) {                                   \
        return reinterpret_cast<type *>(obj)->func(args...);                   \
    }

namespace detail {
template <typename T, typename ArgsTuple>
umf_result_t initialize(T *obj, ArgsTuple &&args) {

    auto ret = std::apply(
        &T::initialize,
        std::tuple_cat(std::make_tuple(obj), std::forward<ArgsTuple>(args)));
    if (ret != UMF_RESULT_SUCCESS) {
        delete obj;
    }
    return ret;
}

template <typename T> umf_memory_pool_ops_t poolOpsBase() {
    umf_memory_pool_ops_t ops;
    ops.version = UMF_VERSION_CURRENT;
    ops.finalize = [](void *obj) { delete reinterpret_cast<T *>(obj); };
    UMF_ASSIGN_OP(ops, T, malloc, ((void *)nullptr));
    UMF_ASSIGN_OP(ops, T, calloc, ((void *)nullptr));
    UMF_ASSIGN_OP(ops, T, aligned_malloc, ((void *)nullptr));
    UMF_ASSIGN_OP(ops, T, realloc, ((void *)nullptr));
    UMF_ASSIGN_OP(ops, T, malloc_usable_size, ((size_t)0));
    UMF_ASSIGN_OP(ops, T, free, UMF_RESULT_SUCCESS);
    UMF_ASSIGN_OP(ops, T, get_last_allocation_error, UMF_RESULT_ERROR_UNKNOWN);
    return ops;
}

template <typename T> umf_memory_provider_ops_t providerOpsBase() {
    umf_memory_provider_ops_t ops;
    ops.version = UMF_VERSION_CURRENT;
    ops.finalize = [](void *obj) { delete reinterpret_cast<T *>(obj); };
    UMF_ASSIGN_OP(ops, T, alloc, UMF_RESULT_ERROR_UNKNOWN);
    UMF_ASSIGN_OP(ops, T, free, UMF_RESULT_ERROR_UNKNOWN);
    UMF_ASSIGN_OP_NORETURN(ops, T, get_last_native_error);
    UMF_ASSIGN_OP(ops, T, get_recommended_page_size, UMF_RESULT_ERROR_UNKNOWN);
    UMF_ASSIGN_OP(ops, T, get_min_page_size, UMF_RESULT_ERROR_UNKNOWN);
    UMF_ASSIGN_OP(ops, T, purge_lazy, UMF_RESULT_ERROR_UNKNOWN);
    UMF_ASSIGN_OP(ops, T, purge_force, UMF_RESULT_ERROR_UNKNOWN);
    UMF_ASSIGN_OP(ops, T, get_name, "");
    return ops;
}
} // namespace detail

// @brief creates UMF pool ops that can be exposed through
// C API. 'params' from ops.initialize will be casted to 'ParamType*'
// and passed to T::initialize() function.
template <typename T, typename ParamType> umf_memory_pool_ops_t poolMakeCOps() {
    umf_memory_pool_ops_t ops = detail::poolOpsBase<T>();

    ops.initialize = [](umf_memory_provider_handle_t provider, void *params,
                        void **obj) {
        *obj = new T;

        if constexpr (std::is_same_v<ParamType, void>) {
            return detail::initialize<T>(reinterpret_cast<T *>(*obj),
                                         std::make_tuple(provider));
        } else {
            return detail::initialize<T>(
                reinterpret_cast<T *>(*obj),
                std::make_tuple(provider,
                                reinterpret_cast<ParamType *>(params)));
        }
    };

    return ops;
}

// @brief creates UMF provider ops that can be exposed through
// C API. 'params' from ops.initialize will be casted to 'ParamType*'
// and passed to T::initialize() function.
template <typename T, typename ParamType>
umf_memory_provider_ops_t providerMakeCOps() {
    umf_memory_provider_ops_t ops = detail::providerOpsBase<T>();

    ops.initialize = [](void *params, void **obj) {
        *obj = new T;

        if constexpr (std::is_same_v<ParamType, void>) {
            return detail::initialize<T>(reinterpret_cast<T *>(*obj),
                                         std::make_tuple());
        } else {
            return detail::initialize<T>(
                reinterpret_cast<T *>(*obj),
                std::make_tuple(reinterpret_cast<ParamType *>(params)));
        }
    };

    return ops;
}

template <typename Type> umf_result_t &getPoolLastStatusRef() {
    static thread_local umf_result_t last_status = UMF_RESULT_SUCCESS;
    return last_status;
}

} // namespace umf

#endif /* UMF_HELPERS_H */

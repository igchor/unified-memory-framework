/*
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#include <cstdio>
#include <cstdlib>
#include <thread>

#include "base_alloc.h"

#include "base.hpp"

using umf_test::test;

TEST_F(test, multiThreadedAllocMemset) {
    static constexpr int NTHREADS = 10;
    static constexpr int ITERATIONS = 1000;
    static constexpr int ALLOCATION_SIZE = 16;

    auto pool = std::shared_ptr<umf_ba_pool_t>(umf_ba_create(ALLOCATION_SIZE),
                                               umf_ba_destroy);

    auto poolAlloc = [](int TID, umf_ba_pool_t *pool) {
        std::vector<std::shared_ptr<void>> ptrs;

        for (int i = 0; i < ITERATIONS; i++) {
            ptrs.emplace_back(umf_ba_alloc(pool), [pool = pool](void *ptr) {
                umf_ba_free(pool, ptr);
            });

            memset(ptrs.back().get(), (i + TID) & 0xFF, ALLOCATION_SIZE);
        }

        for (int i = 0; i < ITERATIONS; i++) {
            for (int k = 0; k < ALLOCATION_SIZE; k++) {
                if (*((unsigned char *)ptrs[i].get() + k) !=
                    ((i + TID) & 0xFF)) {
                    fprintf(
                        stderr,
                        "i = %i k = %i, *(ptrs[i] + k) = %i != ((i + TID) & "
                        "0xFF) = %i\n",
                        i, k, *((unsigned char *)ptrs[i].get() + k),
                        ((i + TID) & 0xFF));
                    abort();
                }
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NTHREADS; i++) {
        threads.emplace_back(poolAlloc, i, pool.get());
    }

    for (auto &thread : threads) {
        thread.join();
    }
}

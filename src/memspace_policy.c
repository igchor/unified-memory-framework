/*
 *
 * Copyright (C) 2024 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#include <umf/memspace_policy.h>

struct umf_memspace_policy_t {
    int dummy;
};

static struct umf_memspace_policy_t umfMemspacePolicyStrict = {0};
umf_memspace_policy_handle_t umfMemspacePolicyStrictGet(void) {
    return &umfMemspacePolicyStrict;
}

static struct umf_memspace_policy_t umfMemspacePolicyPreferred = {0};
umf_memspace_policy_handle_t umfMemspacePolicyPreferredGet(void) {
    return &umfMemspacePolicyPreferred;
}

/*
 *
 * Copyright (C) 2023 Intel Corporation
 *
 * Under the Apache License v2.0 with LLVM Exceptions. See LICENSE.TXT.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 */

#ifndef UMF_MEMSPACE_POLICY_H
#define UMF_MEMSPACE_POLICY_H 1

#include <umf/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct umf_memspace_policy_t *umf_memspace_policy_handle_t;

///
/// \brief Retrieves predefined 'strict' policy.
///        Memory is allocated from the first node in a memspace. If there is not enough
///        memory on the node, allocation will fail.
/// \return policy handle on success or NULL on failure.
///
umf_memspace_policy_handle_t umfMemspacePolicyStrictGet(void);

///
/// \brief Retrieves predefined 'preferred' policy.
///        Memory is allocated from the first node in a memspace. If there is not enough
///        memory on the node, allocation will be attempted on other nodes (in unspecified order).
/// \return policy handle on success or NULL on failure.
///
umf_memspace_policy_handle_t umfMemspacePolicyPreferredGet(void);

#ifdef __cplusplus
}
#endif

#endif /* UMF_MEMSPACE_POLICY_H */

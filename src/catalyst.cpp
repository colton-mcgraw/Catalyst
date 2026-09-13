/**
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief The monolithic library's translation unit.
 */

#include <catalyst/catalyst.hpp>

namespace catalyst
{

    const char *version() noexcept
    {
        return CATALYST_VERSION_STRING;
    }

} // namespace catalyst

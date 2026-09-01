/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Public device API; validates handles and forwards to the active backend.
 */

#include <catalyst/rendering/device.hpp>

#include "detail_backend.hpp"

namespace catalyst::rendering
{

    device create_device(const device_desc &desc)
    {
        return device{detail::create_device(desc)};
    }

    void destroy_device(device &d) noexcept
    {
        if (!d)
            return;
        detail::destroy_device(d.id());
        d = device{};
    }

    bool is_valid(const device &d) noexcept
    {
        return d && detail::is_device_valid(d.id());
    }

    device_info get_device_info(const device &d) noexcept
    {
        if (!d)
            return {};
        return detail::get_device_info(d.id());
    }

    void wait_idle(const device &d) noexcept
    {
        if (!d)
            return;
        detail::wait_idle(d.id());
    }

} // namespace catalyst::rendering

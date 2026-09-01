/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 *
 * @file
 * @brief Public buffer API; validates handles and arguments, then forwards to the active backend.
 */

#include <catalyst/rendering/buffer.hpp>

#include "detail_backend.hpp"

namespace catalyst::rendering
{

    buffer create_buffer(const device &dev, const buffer_desc &desc, std::span<const std::byte> initial_data)
    {
        if (!dev || desc.size_bytes == 0 || initial_data.size() > desc.size_bytes)
            return {};
        return buffer{detail::create_buffer(dev.id(), desc, initial_data)};
    }

    void destroy_buffer(buffer &b) noexcept
    {
        if (!b)
            return;
        detail::destroy_buffer(b.id());
        b = buffer{};
    }

    bool is_valid(const buffer &b) noexcept
    {
        return b && detail::is_buffer_valid(b.id());
    }

    buffer_desc get_buffer_desc(const buffer &b) noexcept
    {
        if (!b)
            return {};
        return detail::get_buffer_desc(b.id());
    }

    std::size_t buffer_size(const buffer &b) noexcept
    {
        return get_buffer_desc(b).size_bytes;
    }

    bool write_buffer(const buffer &b, std::size_t offset, std::span<const std::byte> data)
    {
        if (!b)
            return false;
        return detail::write_buffer(b.id(), offset, data);
    }

    bool read_buffer(const buffer &b, std::size_t offset, std::span<std::byte> out)
    {
        if (!b)
            return false;
        return detail::read_buffer(b.id(), offset, out);
    }

} // namespace catalyst::rendering

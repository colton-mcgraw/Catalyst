/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 * 
 * @file
 * @brief Main header for the Vulkan rendering backend implementation, which provides an interface for rendering graphics using the Vulkan API.
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace catalyst::rendering::detail::vulkan
{

	using native_handle = std::uint64_t;

	struct device
	{
		native_handle instance{};
		native_handle physical_device{};
		native_handle logical_device{};
		native_handle graphics_queue{};
		std::uint32_t graphics_queue_family{};
	};

	struct memory_allocation
	{
		native_handle memory{};
		std::size_t size{};
		std::uint32_t memory_type_index{};
	};

	struct buffer
	{
		native_handle handle{};
		memory_allocation allocation{};
		std::size_t size{};
	};

	struct swapchain
	{
		native_handle handle{};
		std::uint32_t image_count{};
		std::uint32_t width{};
		std::uint32_t height{};
	};

	void reset(device &value) noexcept;
	bool is_valid(const device &value) noexcept;

	void reset(memory_allocation &value) noexcept;
	bool is_valid(const memory_allocation &value) noexcept;

	void reset(buffer &value) noexcept;
	bool is_valid(const buffer &value) noexcept;

	void reset(swapchain &value) noexcept;
	bool is_valid(const swapchain &value) noexcept;

} // namespace catalyst::rendering::detail::vulkan


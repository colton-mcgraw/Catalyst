/**
 * SPDX-License-Identifier: CDDL-1.0
 * SPDX-FileCopyrightText: 2026-Current Catalyst
 * 
 * @file
 * @brief Implementation for the Vulkan memory management, which handles memory allocation and management for Vulkan resources.
 */

#include "vulkan.h"

namespace catalyst::rendering::detail::vulkan
{

	void reset(memory_allocation &value) noexcept
	{
		value = {};
	}

	bool is_valid(const memory_allocation &value) noexcept
	{
		return value.memory != 0 && value.size != 0;
	}

} // namespace catalyst::rendering::detail::vulkan


#include "vulkan.h"

namespace catalyst::rendering::detail::vulkan
{

	void reset(device &value) noexcept
	{
		value = {};
	}

	bool is_valid(const device &value) noexcept
	{
		return value.instance != 0 && value.physical_device != 0 && value.logical_device != 0 && value.graphics_queue != 0;
	}

} // namespace catalyst::rendering::detail::vulkan

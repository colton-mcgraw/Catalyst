#include "vulkan.h"

namespace catalyst::rendering::detail::vulkan
{

	void reset(swapchain &value) noexcept
	{
		value = {};
	}

	bool is_valid(const swapchain &value) noexcept
	{
		return value.handle != 0 && value.image_count != 0 && value.width != 0 && value.height != 0;
	}

} // namespace catalyst::rendering::detail::vulkan

#include "vulkan.h"

namespace catalyst::rendering::detail::vulkan
{

	void reset(buffer &value) noexcept
	{
		value = {};
	}

	bool is_valid(const buffer &value) noexcept
	{
		return value.handle != 0 && value.size != 0 && is_valid(value.allocation);
	}

} // namespace catalyst::rendering::detail::vulkan

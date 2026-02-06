#pragma once

#include <cstdint>

namespace catalyst::input {

	// Packed USB HID usage: (page << 16) | id
	// - Page: USB HID Usage Page (e.g. 0x07 = Keyboard/Keypad)
	// - Id:   Usage ID within that page
	using usb_hid = std::uint32_t;

	inline constexpr usb_hid usb_hid_unknown = 0u;

	inline constexpr std::uint16_t usb_hid_page_keyboard = 0x07u;

	[[nodiscard]] inline constexpr usb_hid make_usb_hid(std::uint16_t page, std::uint16_t id) noexcept
	{
		return (static_cast<usb_hid>(page) << 16) | static_cast<usb_hid>(id);
	}

	[[nodiscard]] inline constexpr std::uint16_t usb_hid_page(usb_hid usage) noexcept
	{
		return static_cast<std::uint16_t>((usage >> 16) & 0xFFFFu);
	}

	[[nodiscard]] inline constexpr std::uint16_t usb_hid_id(usb_hid usage) noexcept
	{
		return static_cast<std::uint16_t>(usage & 0xFFFFu);
	}

} // namespace catalyst::input


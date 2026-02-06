#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "catalyst/core/event.hpp"
#include "catalyst/input/usb.hpp"

namespace catalyst::input {

    // Unicode code point
    using character_code = char32_t;

    // Represents physical keys on a keyboard.
    // Values match USB HID Keyboard/Keypad usage IDs (usage page 0x07).
    // Text entry is delivered separately via character/text input events.
    enum class key_code : std::uint16_t {
        unknown = 0,
        a = 4,
        b = 5,
        c = 6,
        d = 7,
        e = 8,
        f = 9,
        g = 10,
        h = 11,
        i = 12,
        j = 13,
        k = 14,
        l = 15,
        m = 16,
        n = 17,
        o = 18,
        p = 19,
        q = 20,
        r = 21,
        s = 22,
        t = 23,
        u = 24,
        v = 25,
        w = 26,
        x = 27,
        y = 28,
        z = 29,
        digit_1 = 30,
        digit_2 = 31,
        digit_3 = 32,
        digit_4 = 33,
        digit_5 = 34,
        digit_6 = 35,
        digit_7 = 36,
        digit_8 = 37,
        digit_9 = 38,
        digit_0 = 39,
        enter = 40,
        escape = 41,
        backspace = 42,
        tab = 43,
        space = 44,
        minus = 45,
        equal = 46,
        left_bracket = 47,
        right_bracket = 48,
        backslash = 49,
        non_us_hash = 50,
        semicolon = 51,
        apostrophe = 52,
        grave_accent = 53,
        comma = 54,
        period = 55,
        slash = 56,
        caps_lock = 57,
        f1 = 58,
        f2 = 59,
        f3 = 60,
        f4 = 61,
        f5 = 62,
        f6 = 63,
        f7 = 64,
        f8 = 65,
        f9 = 66,
        f10 = 67,
        f11 = 68,
        f12 = 69,
        print_screen = 70,
        scroll_lock = 71,
        pause = 72,
        insert = 73,
        home = 74,
        page_up = 75,
        delete_key = 76,
        end = 77,
        page_down = 78,
        right_arrow = 79,
        left_arrow = 80,
        down_arrow = 81,
        up_arrow = 82,
        num_lock = 83,
        keypad_divide = 84,
        keypad_multiply = 85,
        keypad_minus = 86,
        keypad_plus = 87,
        keypad_enter = 88,
        keypad_1 = 89,
        keypad_2 = 90,
        keypad_3 = 91,
        keypad_4 = 92,
        keypad_5 = 93,
        keypad_6 = 94,
        keypad_7 = 95,
        keypad_8 = 96,
        keypad_9 = 97,
        keypad_0 = 98,
        keypad_period = 99,
        non_us_backslash = 100,
        application = 101,
        power = 102,
        keypad_equal = 103,
        f13 = 104,
        f14 = 105,
        f15 = 106,
        f16 = 107,
        f17 = 108,
        f18 = 109,
        f19 = 110,
        f20 = 111,
        f21 = 112,
        f22 = 113,
        f23 = 114,
        f24 = 115,
        execute = 116,
        help = 117,
        menu = 118,
        select = 119,
        stop = 120,
        again = 121,
        undo = 122,
        cut = 123,
        copy = 124,
        paste = 125,
        find = 126,
        mute = 127,
        volume_up = 128,
        volume_down = 129,
        keypad_comma = 133,
        keypad_equal_as400 = 134,
        international1 = 135,
        international2 = 136,
        international3 = 137,
        international4 = 138,
        international5 = 139,
        international6 = 140,
        international7 = 141,
        international8 = 142,
        international9 = 143,
        lang1 = 144,
        lang2 = 145,
        lang3 = 146,
        lang4 = 147,
        lang5 = 148,
        lang6 = 149,
        lang7 = 150,
        lang8 = 151,
        lang9 = 152
    };

    [[nodiscard]] inline constexpr usb_hid to_usb_hid(key_code code) noexcept
    {
        if (code == key_code::unknown) {
            return usb_hid_unknown;
        }

        return make_usb_hid(usb_hid_page_keyboard, static_cast<std::uint16_t>(code));
    }

    [[nodiscard]] inline constexpr key_code from_usb_hid(usb_hid usage) noexcept
    {
        if (usage == usb_hid_unknown) {
            return key_code::unknown;
        }

        if (usb_hid_page(usage) != usb_hid_page_keyboard) {
            return key_code::unknown;
        }

        const auto id = usb_hid_id(usage);
        if (id == 0u) {
            return key_code::unknown;
        }

        return static_cast<key_code>(id);
    }
    
    // Represents key actions (press, release, repeat).
    enum class key_action : std::uint8_t {
        press,
        release,
        repeat
    };

    // Represents modifier keys (shift, control, alt, super, etc.) as bitflags.
    enum class key_modifiers : std::uint8_t {
        none       = 0x00,
        shift      = 0x01,
        control    = 0x02,
        alt        = 0x04,
        super      = 0x08,
        caps_lock  = 0x10,
        num_lock   = 0x20
    };

    [[nodiscard]] inline constexpr key_modifiers operator|(key_modifiers a, key_modifiers b) noexcept
    {
        using u = std::underlying_type_t<key_modifiers>;
        return static_cast<key_modifiers>(static_cast<u>(a) | static_cast<u>(b));
    }

    [[nodiscard]] inline constexpr key_modifiers operator&(key_modifiers a, key_modifiers b) noexcept
    {
        using u = std::underlying_type_t<key_modifiers>;
        return static_cast<key_modifiers>(static_cast<u>(a) & static_cast<u>(b));
    }

    [[nodiscard]] inline constexpr key_modifiers operator^(key_modifiers a, key_modifiers b) noexcept
    {
        using u = std::underlying_type_t<key_modifiers>;
        return static_cast<key_modifiers>(static_cast<u>(a) ^ static_cast<u>(b));
    }

    [[nodiscard]] inline constexpr key_modifiers operator~(key_modifiers a) noexcept
    {
        using u = std::underlying_type_t<key_modifiers>;
        return static_cast<key_modifiers>(~static_cast<u>(a));
    }

    inline constexpr key_modifiers& operator|=(key_modifiers& a, key_modifiers b) noexcept
    {
        a = (a | b);
        return a;
    }

    inline constexpr key_modifiers& operator&=(key_modifiers& a, key_modifiers b) noexcept
    {
        a = (a & b);
        return a;
    }

    inline constexpr key_modifiers& operator^=(key_modifiers& a, key_modifiers b) noexcept
    {
        a = (a ^ b);
        return a;
    }

    [[nodiscard]] inline constexpr bool has_modifier(key_modifiers value, key_modifiers flag) noexcept
    {
        return (value & flag) != key_modifiers::none;
    }

    struct key_event : public core::event<key_event> {
        key_code code{};
        usb_hid native_code{ usb_hid_unknown };
        key_action action{ key_action::press };
        key_modifiers modifiers{ key_modifiers::none };
    };

    struct character_event : public core::event<character_event> {
        character_code character{};
        key_modifiers modifiers{ key_modifiers::none };
    };

    struct text_input_event : public core::event<text_input_event> {
        static constexpr std::size_t inline_capacity = 8;

        std::array<char32_t, inline_capacity> buffer{};
        std::uint8_t length = 0;

        // Preserved: view of the valid text codepoints.
        std::span<const char32_t> text{};

        text_input_event() noexcept : text(buffer.data(), 0) {}

        explicit text_input_event(std::span<const char32_t> input) noexcept : text(buffer.data(), 0)
        {
            assign(input);
        }

        text_input_event(const text_input_event &other) noexcept
            : buffer(other.buffer), length(other.length), text(buffer.data(), other.length)
        {
        }

        text_input_event &operator=(const text_input_event &other) noexcept
        {
            if (this == &other)
                return *this;
            buffer = other.buffer;
            length = other.length;
            text = {buffer.data(), other.length};
            return *this;
        }

        text_input_event(text_input_event &&other) noexcept
            : buffer(other.buffer), length(other.length), text(buffer.data(), other.length)
        {
        }

        text_input_event &operator=(text_input_event &&other) noexcept
        {
            buffer = other.buffer;
            length = other.length;
            text = {buffer.data(), other.length};
            return *this;
        }

        void assign(std::span<const char32_t> input) noexcept
        {
            const std::size_t n = std::min<std::size_t>(input.size(), buffer.size());
            length = static_cast<std::uint8_t>(n);
            std::copy_n(input.begin(), n, buffer.begin());
            text = {buffer.data(), n};
        }
    };

    struct edit_key_event : public core::event<edit_key_event> {
        key_code code{};
        key_modifiers modifiers{ key_modifiers::none };
    };

} // namespace catalyst::input